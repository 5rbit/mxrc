#pragma once

#include "core/sequence/interfaces/IAction.h"
#include "core/sequence/interfaces/IActionFactory.h"
#include "core/action/util/ExecutionContext.h" // Changed from core/sequence/core/ExecutionContext.h
#include <string>
#include <map>
#include <any>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

namespace mxrc::core::sequence::testing {

/**
 * @brief 성공하는 테스트용 동작
 */
class SuccessAction : public IAction {
public:
    explicit SuccessAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "SuccessAction"; }

    void execute(mxrc::core::action::ExecutionContext& context) override { // Changed ExecutionContext namespace
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 결과 저장
        context.setActionResult(id_, 42);

        progress_ = 1.0f;
        status_ = ActionStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock success action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 실패하는 테스트용 동작
 */
class FailureAction : public IAction {
public:
    explicit FailureAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "FailureAction"; }

    void execute(mxrc::core::action::ExecutionContext& context) override { // Changed ExecutionContext namespace
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;
        progress_ = 1.0f;
        status_ = ActionStatus::FAILED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock failure action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 결과를 저장하는 테스트용 동작
 */
class ResultStoringAction : public IAction {
public:
    explicit ResultStoringAction(const std::string& id, const std::any& result)
        : id_(id), result_(result), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "ResultStoringAction"; }

    void execute(mxrc::core::action::ExecutionContext& context) override { // Changed ExecutionContext namespace
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 지정된 결과 저장
        context.setActionResult(id_, result_);

        progress_ = 1.0f;
        status_ = ActionStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock result storing action"; }

private:
    std::string id_;
    std::any result_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 컨텍스트 변수를 읽고 수정하는 테스트용 동작
 */
class ContextModifyingAction : public IAction {
public:
    explicit ContextModifyingAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "ContextModifyingAction"; }

    void execute(mxrc::core::action::ExecutionContext& context) override { // Changed ExecutionContext namespace
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 컨텍스트 변수 읽기
        auto varValue = context.getVariable("input_var");
        if (varValue.has_value()) {
            try {
                int inputInt = std::any_cast<int>(varValue);
                // 결과에 입력값 두 배 저장
                context.setActionResult(id_, inputInt * 2);
            } catch (...) {
                context.setActionResult(id_, 0);
            }
        }

        progress_ = 1.0f;
        status_ = ActionStatus::COMPLETED;
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock context modifying action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 예외를 발생시키는 테스트용 동작
 */
class ExceptionThrowingAction : public IAction {
public:
    explicit ExceptionThrowingAction(const std::string& id)
        : id_(id), status_(ActionStatus::PENDING), progress_(0.0f) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "ExceptionThrowingAction"; }

    void execute(mxrc::core::action::ExecutionContext& context) override { // Changed ExecutionContext namespace
        status_ = ActionStatus::RUNNING;
        progress_ = 0.5f;

        // 예외 발생
        throw std::runtime_error("Mock action threw exception");
    }

    void cancel() override {
        if (status_ == ActionStatus::RUNNING) {
            status_ = ActionStatus::CANCELLED;
        }
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock exception throwing action"; }

private:
    std::string id_;
    ActionStatus status_;
    float progress_;
};

/**
 * @brief 취소 가능한 지연 동작
 */
class CancellableDelayAction : public IAction {
public:
    explicit CancellableDelayAction(const std::string& id, std::chrono::milliseconds duration)
        : id_(id), duration_(duration), status_(ActionStatus::PENDING), progress_(0.0f), cancel_requested_(false) {}

    std::string getId() const override { return id_; }
    std::string getType() const override { return "CancellableDelayAction"; }

    void execute(mxrc::core::action::ExecutionContext& context) override { // Changed ExecutionContext namespace
        status_ = ActionStatus::RUNNING;
        progress_ = 0.0f;

        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + duration_;
        
        while (std::chrono::steady_clock::now() < end_time) {
            if (cancel_requested_) {
                status_ = ActionStatus::CANCELLED;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 짧게 슬립하며 취소 요청 확인
            progress_ = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count()) / duration_.count();
            if (progress_ > 1.0f) progress_ = 1.0f;
        }

        if (!cancel_requested_) {
            status_ = ActionStatus::COMPLETED;
            progress_ = 1.0f;
            context.setActionResult(id_, "Delay completed");
        } else {
            status_ = ActionStatus::CANCELLED;
        }
    }

    void cancel() override {
        cancel_requested_ = true;
    }

    ActionStatus getStatus() const override { return status_; }
    float getProgress() const override { return progress_; }
    std::string getDescription() const override { return "Mock cancellable delay action"; }

private:
    std::string id_;
    std::chrono::milliseconds duration_;
    ActionStatus status_;
    float progress_;
    std::atomic<bool> cancel_requested_;
};

/**
 * @brief 테스트용 동작 팩토리
 */
class MockActionFactory : public IActionFactory {
public:
    std::shared_ptr<IAction> createAction(
        const std::string& type,
        const std::string& id,
        const std::map<std::string, std::string>& params) override {

        if (type == "success" || type.find("success") != std::string::npos) {
            return std::make_shared<SuccessAction>(id);
        } else if (type == "failure" || type.find("failure") != std::string::npos) {
            return std::make_shared<FailureAction>(id);
        } else if (type == "modify" || type.find("modify") != std::string::npos) {
            return std::make_shared<ContextModifyingAction>(id);
        } else if (type == "exception" || type.find("exception") != std::string::npos) {
            return std::make_shared<ExceptionThrowingAction>(id);
        } else if (type == "cancellable_delay") {
            auto it = params.find("duration_ms");
            std::chrono::milliseconds duration = std::chrono::milliseconds(0);
            if (it != params.end()) {
                try {
                    duration = std::chrono::milliseconds(std::stoi(it->second));
                } catch (...) {
                    // 기본값 사용
                }
            }
            return std::make_shared<CancellableDelayAction>(id, duration);
        }
        else {
            // 기본값: 성공 동작
            return std::make_shared<SuccessAction>(id);
        }
    }

    std::vector<std::string> getSupportedTypes() const override {
        return {"success", "failure", "modify", "exception", "cancellable_delay"};
    }
};

} // namespace mxrc::core::sequence::testing

