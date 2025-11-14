#ifndef MXRC_CORE_SEQUENCE_ISEQUENCE_ENGINE_H
#define MXRC_CORE_SEQUENCE_ISEQUENCE_ENGINE_H

#include "core/sequence/dto/SequenceStatus.h"
#include "core/sequence/dto/SequenceDefinition.h"
#include "core/action/util/ExecutionContext.h"
#include <memory>
#include <string>

namespace mxrc::core::sequence {

/**
 * @brief Sequence 실행 결과
 */
struct SequenceResult {
    std::string sequenceId;
    SequenceStatus status;
    float progress{0.0f};                          // 진행률 (0.0 ~ 1.0)
    std::string errorMessage;
    int completedSteps{0};                         // 완료된 스텝 수
    int totalSteps{0};                             // 전체 스텝 수
    std::chrono::milliseconds executionTime{0};    // 실행 시간

    bool isSuccessful() const {
        return status == SequenceStatus::COMPLETED;
    }

    bool isFailed() const {
        return status == SequenceStatus::FAILED ||
               status == SequenceStatus::CANCELLED ||
               status == SequenceStatus::TIMEOUT;
    }
};

/**
 * @brief Sequence 엔진 인터페이스
 *
 * Sequence를 실행하고 관리하는 엔진의 인터페이스입니다.
 */
class ISequenceEngine {
public:
    virtual ~ISequenceEngine() = default;

    /**
     * @brief Sequence 실행
     *
     * @param definition Sequence 정의
     * @param context 실행 컨텍스트
     * @return Sequence 실행 결과
     */
    virtual SequenceResult execute(
        const SequenceDefinition& definition,
        mxrc::core::action::ExecutionContext& context) = 0;

    /**
     * @brief Sequence 취소
     *
     * @param sequenceId Sequence ID
     */
    virtual void cancel(const std::string& sequenceId) = 0;

    /**
     * @brief Sequence 일시정지
     *
     * @param sequenceId Sequence ID
     */
    virtual void pause(const std::string& sequenceId) = 0;

    /**
     * @brief Sequence 재개
     *
     * @param sequenceId Sequence ID
     */
    virtual void resume(const std::string& sequenceId) = 0;

    /**
     * @brief Sequence 상태 조회
     *
     * @param sequenceId Sequence ID
     * @return Sequence 상태
     */
    virtual SequenceStatus getStatus(const std::string& sequenceId) const = 0;

    /**
     * @brief Sequence 진행률 조회
     *
     * @param sequenceId Sequence ID
     * @return 진행률 (0.0 ~ 1.0)
     */
    virtual float getProgress(const std::string& sequenceId) const = 0;
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_ISEQUENCE_ENGINE_H
