#ifndef MXRC_CORE_LOGGING_DTO_RETENTIONPOLICY_H
#define MXRC_CORE_LOGGING_DTO_RETENTIONPOLICY_H

#include <cstdint>
#include <chrono>

namespace mxrc::core::logging {

/**
 * @brief 파일 보존 타입
 */
enum class RetentionType {
    TIME,    ///< 시간 기반 보존 (N일 이상 경과 시 삭제)
    COUNT    ///< 개수 기반 보존 (N개 초과 시 가장 오래된 파일 삭제)
};

/**
 * @brief Bag 파일 보존 정책
 *
 * 오래된 Bag 파일을 자동으로 삭제하여 디스크 공간을 관리합니다.
 */
struct RetentionPolicy {
    RetentionType type = RetentionType::TIME;    ///< 보존 조건 타입
    uint32_t maxAgeDays = 7;                     ///< TIME 타입: 최대 보존 기간 (기본 7일)
    uint32_t maxFileCount = 100;                 ///< COUNT 타입: 최대 파일 수 (기본 100개)

    /**
     * @brief 삭제 조건 확인
     * @param fileTime 파일 생성/수정 시간
     * @param totalFileCount 총 파일 개수
     * @param fileIndex 파일 인덱스 (0부터, 오래된 순)
     * @return 삭제해야 하면 true
     */
    bool shouldDelete(const std::chrono::system_clock::time_point& fileTime,
                      size_t totalFileCount, size_t fileIndex) const {
        if (type == RetentionType::TIME) {
            auto now = std::chrono::system_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - fileTime).count();
            return age >= (maxAgeDays * 24);
        } else {
            // 파일은 오래된 순으로 정렬되어 있다고 가정
            return totalFileCount > maxFileCount && fileIndex < (totalFileCount - maxFileCount);
        }
    }

    /**
     * @brief TIME 기반 정책 생성
     * @param days 최대 보존 기간 (일)
     * @return RetentionPolicy
     */
    static RetentionPolicy createTimePolicy(uint32_t days) {
        RetentionPolicy policy;
        policy.type = RetentionType::TIME;
        policy.maxAgeDays = days;
        return policy;
    }

    /**
     * @brief COUNT 기반 정책 생성
     * @param count 최대 파일 개수
     * @return RetentionPolicy
     */
    static RetentionPolicy createCountPolicy(uint32_t count) {
        RetentionPolicy policy;
        policy.type = RetentionType::COUNT;
        policy.maxFileCount = count;
        return policy;
    }
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_DTO_RETENTIONPOLICY_H
