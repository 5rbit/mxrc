#include "BehaviorArbiter.h"
#include "core/datastore/DataStore.h"
#include "core/alarm/dto/AlarmSeverity.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::control {

BehaviorArbiter::BehaviorArbiter(
    std::shared_ptr<alarm::IAlarmManager> alarm_manager,
    std::shared_ptr<DataStore> data_store)
    : alarm_manager_(std::move(alarm_manager)),
      data_store_(std::move(data_store))
{
    spdlog::info("[BehaviorArbiter] Initialized with DataStore: {}",
                 data_store_ ? "yes" : "no");
}

bool BehaviorArbiter::requestBehavior(const BehaviorRequest& request) {
    stats_.total_requests++;

    // 큐에 추가
    if (!pending_behaviors_.push(request)) {
        spdlog::error("[BehaviorArbiter] Failed to enqueue behavior: {}",
            request.behavior_id);
        return false;
    }

    spdlog::debug("[BehaviorArbiter] Behavior requested: {} (priority: {})",
        request.behavior_id, static_cast<int>(request.priority));

    return true;
}

void BehaviorArbiter::tick() {
    // 1. Critical Alarm 체크
    checkCriticalAlarms();

    // 2. Warning Alarm 체크
    checkWarningAlarms();

    // 3. 일시 정지 상태면 처리 안 함
    if (paused_.load()) {
        return;
    }

    // 4. Timeout된 behavior 제거
    removeTimedOutBehaviors();

    // 5. 현재 실행 중인 task 확인
    if (current_behavior_) {
        auto& current_task = current_behavior_->task;
        auto status = current_task->getStatus();

        // Task 완료 또는 실패 시 다음 behavior 선택
        if (status == task::TaskStatus::COMPLETED ||
            status == task::TaskStatus::FAILED ||
            status == task::TaskStatus::CANCELLED) {

            spdlog::info("[BehaviorArbiter] Current task finished: {} (status: {})",
                current_behavior_->behavior_id, static_cast<int>(status));

            current_behavior_.reset();

            // Warning alarm으로 인한 MAINT 모드 전환 대기 중이면 지금 전환
            if (pending_safe_mode_.load()) {
                spdlog::warn("[BehaviorArbiter] Transitioning to MAINT mode after task completion (Warning Alarm)");
                transitionTo(ControlMode::MAINT);
                pending_safe_mode_ = false;
                return;
            }
        } else {
            // 아직 실행 중이면 선점 가능한지 확인
            auto next_behavior = selectNextBehavior();
            if (next_behavior && shouldPreempt(*next_behavior)) {
                handlePreemption(*next_behavior);
                return;
            }

            // 선점 불가능하면 현재 task 계속 실행
            return;
        }
    }

    // 6. 다음 behavior 선택 및 시작
    auto next_behavior = selectNextBehavior();
    if (next_behavior) {
        startTask(*next_behavior);
    }
}

ControlMode BehaviorArbiter::getCurrentMode() const {
    return current_mode_.load();
}

std::string BehaviorArbiter::getCurrentTaskId() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (current_behavior_) {
        return current_behavior_->task->getId();
    }

    return "";
}

size_t BehaviorArbiter::getPendingBehaviorCount() const {
    return pending_behaviors_.size();
}

bool BehaviorArbiter::transitionTo(ControlMode newMode) {
    ControlMode current = current_mode_.load();

    // 전이 유효성 검증
    if (!isValidModeTransition(current, newMode)) {
        spdlog::warn("[BehaviorArbiter] Invalid mode transition: {} -> {}",
            toString(current), toString(newMode));
        return false;
    }

    // FAULT 모드 진입 시 모든 task 중단
    if (newMode == ControlMode::FAULT) {
        if (current_behavior_) {
            stopCurrentTask("cancel");
        }
        clearPendingBehaviors();
    }

    current_mode_.store(newMode);
    stats_.mode_transitions++;

    // DataStore에 모드 변경 기록
    if (data_store_) {
        try {
            // 현재 모드 저장
            std::string mode_key = "control/current_mode";
            data_store_->set(mode_key, static_cast<int>(newMode), DataType::RobotMode);

            // 모드 전환 이력 저장
            std::string history_key = "control/mode_transition_history";
            std::string transition = toString(current) + " -> " + toString(newMode);
            data_store_->set(history_key, transition, DataType::Event);

            // 모드별 카운터 업데이트
            std::string counter_key = "control/mode_transitions_count";
            data_store_->set(counter_key, static_cast<int>(stats_.mode_transitions), DataType::RobotMode);

            spdlog::debug("[BehaviorArbiter] Stored mode transition to DataStore");
        } catch (const std::exception& e) {
            spdlog::error("[BehaviorArbiter] Failed to store mode transition to DataStore: {}", e.what());
        }
    }

    spdlog::info("[BehaviorArbiter] Mode transition: {} -> {}",
        toString(current), toString(newMode));

    return true;
}

void BehaviorArbiter::clearPendingBehaviors() {
    pending_behaviors_.clear();

    std::lock_guard<std::mutex> lock(mutex_);
    suspended_behaviors_.clear();

    spdlog::info("[BehaviorArbiter] Cleared all pending behaviors");
}

bool BehaviorArbiter::cancelBehavior(const std::string& behavior_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 현재 실행 중인 behavior인지 확인
    if (current_behavior_ && current_behavior_->behavior_id == behavior_id) {
        if (!current_behavior_->cancellable) {
            spdlog::warn("[BehaviorArbiter] Cannot cancel non-cancellable behavior: {}",
                behavior_id);
            return false;
        }

        stopCurrentTask("cancel");
        current_behavior_.reset();

        spdlog::info("[BehaviorArbiter] Cancelled current behavior: {}", behavior_id);
        return true;
    }

    // 일시 중지된 behavior인지 확인
    auto it = suspended_behaviors_.find(behavior_id);
    if (it != suspended_behaviors_.end()) {
        suspended_behaviors_.erase(it);
        spdlog::info("[BehaviorArbiter] Cancelled suspended behavior: {}", behavior_id);
        return true;
    }

    spdlog::warn("[BehaviorArbiter] Behavior not found: {}", behavior_id);
    return false;
}

bool BehaviorArbiter::pause() {
    if (paused_.load()) {
        return false;  // 이미 일시 정지됨
    }

    paused_.store(true);

    // 현재 실행 중인 task 일시 정지
    if (current_behavior_) {
        stopCurrentTask("pause");
    }

    spdlog::info("[BehaviorArbiter] Paused");
    return true;
}

bool BehaviorArbiter::resume() {
    if (!paused_.load()) {
        return false;  // 이미 실행 중
    }

    paused_.store(false);

    // 일시 중지된 task 재개
    if (current_behavior_) {
        current_behavior_->task->resume();
        spdlog::info("[BehaviorArbiter] Resumed task: {}",
            current_behavior_->behavior_id);
    }

    spdlog::info("[BehaviorArbiter] Resumed");
    return true;
}

std::optional<BehaviorRequest> BehaviorArbiter::selectNextBehavior() {
    return pending_behaviors_.pop();
}

bool BehaviorArbiter::shouldPreempt(const BehaviorRequest& new_behavior) {
    if (!current_behavior_) {
        return false;  // 실행 중인 behavior 없음
    }

    // 우선순위 비교
    if (new_behavior.priority < current_behavior_->priority) {
        // 새 behavior가 더 높은 우선순위
        return canPreempt(new_behavior.priority);
    }

    return false;
}

void BehaviorArbiter::handlePreemption(const BehaviorRequest& new_behavior) {
    if (!current_behavior_) {
        return;
    }

    spdlog::warn("[BehaviorArbiter] Preemption: {} (P{}) → {} (P{})",
        current_behavior_->behavior_id, static_cast<int>(current_behavior_->priority),
        new_behavior.behavior_id, static_cast<int>(new_behavior.priority));

    stats_.preemptions++;

    // EMERGENCY_STOP은 즉시 중단
    if (new_behavior.priority == Priority::EMERGENCY_STOP) {
        stopCurrentTask("cancel");
        current_behavior_.reset();
        transitionTo(ControlMode::FAULT);
    }
    // SAFETY_ISSUE는 일시 중지
    else if (new_behavior.priority == Priority::SAFETY_ISSUE) {
        stopCurrentTask("pause");

        // 일시 중지된 behavior 저장
        std::lock_guard<std::mutex> lock(mutex_);
        suspended_behaviors_[current_behavior_->behavior_id] = *current_behavior_;

        current_behavior_.reset();
    }
    // URGENT_TASK는 NORMAL_TASK/MAINTENANCE만 선점
    else if (new_behavior.priority == Priority::URGENT_TASK) {
        if (current_behavior_->priority >= Priority::NORMAL_TASK) {
            stopCurrentTask("pause");

            std::lock_guard<std::mutex> lock(mutex_);
            suspended_behaviors_[current_behavior_->behavior_id] = *current_behavior_;

            current_behavior_.reset();
        }
    }

    // 새로운 behavior 시작
    startTask(new_behavior);
}

void BehaviorArbiter::stopCurrentTask(const std::string& method) {
    if (!current_behavior_) {
        return;
    }

    auto& task = current_behavior_->task;

    if (method == "cancel") {
        task->stop();
        spdlog::info("[BehaviorArbiter] Cancelled task: {}",
            current_behavior_->behavior_id);
    } else if (method == "pause") {
        task->pause();
        spdlog::info("[BehaviorArbiter] Paused task: {}",
            current_behavior_->behavior_id);
    }
}

void BehaviorArbiter::startTask(const BehaviorRequest& behavior) {
    current_behavior_ = behavior;

    auto& task = current_behavior_->task;
    task->start();

    spdlog::info("[BehaviorArbiter] Started task: {} (priority: {})",
        behavior.behavior_id, static_cast<int>(behavior.priority));
}

bool BehaviorArbiter::isValidModeTransition(ControlMode from, ControlMode to) {
    // ControlMode.h의 isValidTransition() 함수 사용
    return control::isValidTransition(from, to);
}

void BehaviorArbiter::checkCriticalAlarms() {
    if (alarm_manager_->hasCriticalAlarm()) {
        ControlMode current = current_mode_.load();

        if (current != ControlMode::FAULT) {
            spdlog::error("[BehaviorArbiter] Critical alarm detected! Transitioning to FAULT");
            transitionTo(ControlMode::FAULT);
        }
    }
}

void BehaviorArbiter::checkWarningAlarms() {
    // Warning Alarm이 활성화되어 있는지 확인
    auto warning_alarms = alarm_manager_->getActiveAlarmsBySeverity(
        alarm::AlarmSeverity::WARNING);

    if (!warning_alarms.empty()) {
        ControlMode current = current_mode_.load();

        // MAINT 모드가 아니고, 아직 대기 플래그가 없으면 설정
        if (current != ControlMode::MAINT &&
            current != ControlMode::FAULT &&
            !pending_safe_mode_.load()) {

            if (current_behavior_) {
                // 현재 작업이 진행 중이면 완료 후 MAINT 전환 예약
                pending_safe_mode_ = true;
                spdlog::warn("[BehaviorArbiter] Warning alarm detected! Will transition to MAINT mode after current task completes");
            } else {
                // 현재 작업이 없으면 즉시 MAINT 전환
                spdlog::warn("[BehaviorArbiter] Warning alarm detected! Transitioning to MAINT mode immediately");
                transitionTo(ControlMode::MAINT);
            }
        }
    }
}

void BehaviorArbiter::removeTimedOutBehaviors() {
    // TODO: Timeout 체크 구현 (Feature 016 후속 작업)
    // BehaviorRequest의 timeout 필드 확인하여 만료된 요청 제거
}

} // namespace mxrc::core::control
