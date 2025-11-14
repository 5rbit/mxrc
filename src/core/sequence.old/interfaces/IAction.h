#pragma once

#include <string>
#include <memory>
#include "../dto/ActionStatus.h"

namespace mxrc::core::sequence {

class ExecutionContext;

/**
 * @brief 시퀀스에서 실행되는 개별 동작의 인터페이스
 * 
 * 모든 동작은 이 인터페이스를 구현해야 합니다.
 * TaskManager의 ITask와 유사하지만, 시퀀스 시스템에 특화된 설계입니다.
 */
class IAction {
public:
    virtual ~IAction() = default;
    
    /**
     * @brief 동작 ID 반환
     * @return 동작 고유 식별자
     */
    virtual std::string getId() const = 0;
    
    /**
     * @brief 동작 타입 반환
     * @return 동작의 타입 (예: "Move", "Grip")
     */
    virtual std::string getType() const = 0;
    
    /**
     * @brief 동작 실행
     * @param context 실행 컨텍스트 (이전 결과 참조 가능)
     * @throws std::exception 실행 중 오류 발생 시
     */
    virtual void execute(ExecutionContext& context) = 0;
    
    /**
     * @brief 동작 취소
     * 실행 중인 동작을 즉시 중단합니다.
     */
    virtual void cancel() = 0;
    
    /**
     * @brief 현재 상태 반환
     * @return 동작의 현재 상태
     */
    virtual ActionStatus getStatus() const = 0;
    
    /**
     * @brief 진행률 반환
     * @return 진행률 (0.0 ~ 1.0)
     */
    virtual float getProgress() const = 0;
    
    /**
     * @brief 동작 설명 반환 (선택사항)
     * @return 동작 설명
     */
    virtual std::string getDescription() const { return getType(); }
};

} // namespace mxrc::core::sequence

