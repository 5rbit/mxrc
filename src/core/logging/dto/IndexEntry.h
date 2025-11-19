#ifndef MXRC_CORE_LOGGING_DTO_INDEXENTRY_H
#define MXRC_CORE_LOGGING_DTO_INDEXENTRY_H

#include <cstdint>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 인덱스 엔트리
 *
 * 각 BagMessage의 위치와 타임스탬프를 기록하여
 * 빠른 시간 기반 탐색을 가능하게 합니다.
 *
 * **메모리 레이아웃**: 16 bytes (packed)
 * - timestamp_ns: 8 bytes (uint64_t)
 * - file_offset: 8 bytes (uint64_t)
 *
 * **사용 예시**:
 * ```cpp
 * IndexEntry entry;
 * entry.timestamp_ns = 1700000000000000000;  // 2023-11-14 22:13:20 UTC
 * entry.file_offset = 1024;  // 파일 내 1KB 위치
 * ```
 */
struct IndexEntry {
    /// @brief 메시지 타임스탬프 (나노초 단위, epoch 기준)
    uint64_t timestamp_ns;

    /// @brief 파일 내 바이트 오프셋 (메시지 시작 위치)
    uint64_t file_offset;

    /**
     * @brief 기본 생성자
     */
    IndexEntry() : timestamp_ns(0), file_offset(0) {}

    /**
     * @brief 파라미터 생성자
     *
     * @param ts 타임스탬프 (나노초)
     * @param offset 파일 오프셋 (바이트)
     */
    IndexEntry(uint64_t ts, uint64_t offset)
        : timestamp_ns(ts), file_offset(offset) {}

    /**
     * @brief 타임스탬프 기준 비교 연산자 (<)
     *
     * 이진 탐색에서 사용됩니다.
     *
     * @param other 비교 대상
     * @return true if this->timestamp_ns < other.timestamp_ns
     */
    bool operator<(const IndexEntry& other) const {
        return timestamp_ns < other.timestamp_ns;
    }

    /**
     * @brief 동등 비교 연산자 (==)
     *
     * @param other 비교 대상
     * @return true if 모든 필드가 동일
     */
    bool operator==(const IndexEntry& other) const {
        return timestamp_ns == other.timestamp_ns &&
               file_offset == other.file_offset;
    }

    /**
     * @brief 크기 검증
     *
     * @return 구조체 크기가 16바이트인지 확인
     */
    static constexpr bool validateSize() {
        return sizeof(IndexEntry) == 16;
    }
} __attribute__((packed));  // 패딩 제거 (정확히 16 bytes)

// 컴파일 타임 크기 검증
static_assert(sizeof(IndexEntry) == 16,
              "IndexEntry must be exactly 16 bytes");

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_DTO_INDEXENTRY_H
