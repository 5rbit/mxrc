#ifndef MXRC_CORE_ACTION_EXECUTION_CONTEXT_H
#define MXRC_CORE_ACTION_EXECUTION_CONTEXT_H

#include <string>
#include <map>
#include <any>
#include <optional>
#include <mutex>

namespace mxrc::core::action {

/**
 * @brief Action 실행 컨텍스트
 *
 * Action들 간의 상태 공유를 위한 변수 및 결과 저장소입니다.
 * 여러 Action이 동일한 컨텍스트를 공유하여 데이터를 주고받을 수 있습니다.
 *
 * 스레드 안전성: 내부적으로 뮤텍스를 사용하여 스레드 안전성을 보장합니다.
 */
class ExecutionContext {
public:
    ExecutionContext() = default;
    ~ExecutionContext() = default;

    // 복사 및 이동 금지 (mutex는 복사/이동 불가능)
    ExecutionContext(const ExecutionContext&) = delete;
    ExecutionContext& operator=(const ExecutionContext&) = delete;
    ExecutionContext(ExecutionContext&&) = delete;
    ExecutionContext& operator=(ExecutionContext&&) = delete;

    /**
     * @brief 변수 설정
     *
     * @param key 변수 이름
     * @param value 변수 값
     */
    void setVariable(const std::string& key, const std::any& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        variables_[key] = value;
    }

    /**
     * @brief 변수 조회
     *
     * @param key 변수 이름
     * @return 변수 값 (존재하지 않으면 empty optional)
     */
    std::optional<std::any> getVariable(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = variables_.find(key);
        if (it != variables_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 변수 존재 여부 확인
     *
     * @param key 변수 이름
     * @return 변수가 존재하면 true
     */
    bool hasVariable(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return variables_.find(key) != variables_.end();
    }

    /**
     * @brief Action 결과 저장
     *
     * @param actionId Action ID
     * @param result 결과 데이터
     */
    void setActionResult(const std::string& actionId, const std::any& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        actionResults_[actionId] = result;
    }

    /**
     * @brief Action 결과 조회
     *
     * @param actionId Action ID
     * @return Action 결과 (존재하지 않으면 empty optional)
     */
    std::optional<std::any> getActionResult(const std::string& actionId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actionResults_.find(actionId);
        if (it != actionResults_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 모든 변수 삭제
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        variables_.clear();
        actionResults_.clear();
    }

private:
    mutable std::mutex mutex_;                          // 스레드 안전성을 위한 뮤텍스
    std::map<std::string, std::any> variables_;         // 공유 변수
    std::map<std::string, std::any> actionResults_;     // Action 실행 결과
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_EXECUTION_CONTEXT_H
