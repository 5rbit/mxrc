#ifndef MXRC_CORE_LOGGING_DTO_ROTATIONPOLICY_H
#define MXRC_CORE_LOGGING_DTO_ROTATIONPOLICY_H

#include <cstdint>

namespace mxrc::core::logging {

/**
 * @brief 파일 순환 타입
 */
enum class RotationType {
    SIZE,    ///< 파일 크기 기반 순환
    TIME     ///< 시간 기반 순환 (예: 1시간마다)
};

/**
 * @brief Bag 파일 순환 정책
 *
 * Bag 파일의 크기나 기록 시간에 따라 자동으로 새 파일로 순환합니다.
 */
struct RotationPolicy {
    RotationType type = RotationType::SIZE;                  ///< 순환 조건 타입
    uint64_t maxSizeBytes = 1ULL * 1024 * 1024 * 1024;      ///< SIZE 타입: 최대 파일 크기 (기본 1GB)
    uint64_t maxDurationSeconds = 3600;                      ///< TIME 타입: 최대 기록 시간 (기본 1시간)

    /**
     * @brief 순환 조건 확인
     * @param currentSizeBytes 현재 파일 크기
     * @param elapsedSeconds 파일 기록 시작 후 경과 시간
     * @return 순환해야 하면 true
     */
    bool shouldRotate(uint64_t currentSizeBytes, uint64_t elapsedSeconds) const {
        if (type == RotationType::SIZE) {
            return currentSizeBytes >= maxSizeBytes;
        } else {
            return elapsedSeconds >= maxDurationSeconds;
        }
    }

    /**
     * @brief SIZE 기반 정책 생성
     * @param maxSizeMB 최대 크기 (MB)
     * @return RotationPolicy
     */
    static RotationPolicy createSizePolicy(uint64_t maxSizeMB) {
        RotationPolicy policy;
        policy.type = RotationType::SIZE;
        policy.maxSizeBytes = maxSizeMB * 1024 * 1024;
        return policy;
    }

    /**
     * @brief TIME 기반 정책 생성
     * @param maxDurationSeconds 최대 시간 (초)
     * @return RotationPolicy
     */
    static RotationPolicy createTimePolicy(uint64_t maxDurationSeconds) {
        RotationPolicy policy;
        policy.type = RotationType::TIME;
        policy.maxDurationSeconds = maxDurationSeconds;
        return policy;
    }
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_DTO_ROTATIONPOLICY_H
