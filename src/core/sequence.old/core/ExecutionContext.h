#pragma once

#include <map>
#include <string>
#include <any>
#include <memory>

namespace mxrc::core::sequence {

/**
 * @brief 시퀀스 실행 중 동작들 간에 공유되는 실행 컨텍스트
 * 
 * 이전 동작의 결과를 다음 동작에서 참조할 수 있도록 제공합니다.
 */
class ExecutionContext {
public:
    ExecutionContext() = default;
    ~ExecutionContext() = default;
    
    /**
     * @brief 실행 결과 저장
     * @param actionId 동작 ID
     * @param result 동작 실행 결과
     */
    void setActionResult(const std::string& actionId, const std::any& result);
    
    /**
     * @brief 실행 결과 조회
     * @param actionId 동작 ID
     * @return 저장된 결과 (없으면 빈 any)
     */
    std::any getActionResult(const std::string& actionId) const;
    
    /**
     * @brief 특정 동작의 결과 존재 여부 확인
     * @param actionId 동작 ID
     * @return 결과 존재 여부
     */
    bool hasActionResult(const std::string& actionId) const;
    
    /**
     * @brief 모든 실행 결과 조회
     * @return 동작 ID와 결과의 맵
     */
    const std::map<std::string, std::any>& getAllResults() const;
    
    /**
     * @brief 실행 결과 초기화
     */
    void clear();
    
    /**
     * @brief 컨텍스트 변수 설정
     * @param key 변수 키
     * @param value 변수 값
     */
    void setVariable(const std::string& key, const std::any& value);
    
    /**
     * @brief 컨텍스트 변수 조회
     * @param key 변수 키
     * @return 변수 값 (없으면 빈 any)
     */
    std::any getVariable(const std::string& key) const;
    
    /**
     * @brief 시퀀스 실행 ID 설정
     * @param executionId 실행 ID
     */
    void setExecutionId(const std::string& executionId) { executionId_ = executionId; }
    
    /**
     * @brief 시퀀스 실행 ID 조회
     * @return 실행 ID
     */
    const std::string& getExecutionId() const { return executionId_; }

private:
    std::map<std::string, std::any> actionResults_;
    std::map<std::string, std::any> variables_;
    std::string executionId_;
};

} // namespace mxrc::core::sequence

