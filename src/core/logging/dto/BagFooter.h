#ifndef MXRC_CORE_LOGGING_DTO_BAGFOOTER_H
#define MXRC_CORE_LOGGING_DTO_BAGFOOTER_H

#include <cstdint>
#include <cstring>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 푸터 (파일 끝에 위치)
 *
 * Bag 파일의 메타데이터와 인덱스 위치 정보를 저장합니다.
 * 파일 끝에서부터 64바이트를 읽어 푸터를 파싱합니다.
 *
 * **메모리 레이아웃**: 64 bytes (packed)
 * - magic: 8 bytes (ASCII "MXRCBAG\0")
 * - version: 4 bytes (uint32_t)
 * - data_size: 8 bytes (uint64_t) - 메시지 데이터 영역 크기
 * - index_offset: 8 bytes (uint64_t) - 인덱스 블록 시작 위치
 * - index_count: 8 bytes (uint64_t) - 인덱스 엔트리 개수
 * - checksum: 4 bytes (uint32_t) - CRC32 체크섬
 * - reserved: 24 bytes - 향후 확장용
 *
 * **파일 구조**:
 * ```
 * [Messages...] [Index Block...] [Footer (64 bytes)]
 * ```
 */
struct BagFooter {
    /// @brief 매직 넘버 (파일 타입 식별)
    char magic[8];

    /// @brief Bag 포맷 버전 (현재: 1)
    uint32_t version;

    /// @brief 메시지 데이터 영역 크기 (바이트)
    uint64_t data_size;

    /// @brief 인덱스 블록 시작 오프셋 (바이트)
    uint64_t index_offset;

    /// @brief 인덱스 엔트리 개수
    uint64_t index_count;

    /// @brief 데이터 + 인덱스 영역의 CRC32 체크섬
    uint32_t checksum;

    /// @brief 향후 확장용 예약 영역
    char reserved[24];

    /**
     * @brief 기본 생성자 (초기화)
     */
    BagFooter() {
        std::memset(this, 0, sizeof(BagFooter));
        setMagic();
        version = 1;
    }

    /**
     * @brief 매직 넘버 설정
     */
    void setMagic() {
        std::strncpy(magic, "MXRCBAG", 8);
        magic[7] = '\0';
    }

    /**
     * @brief 매직 넘버 검증
     *
     * @return true if 유효한 Bag 파일
     */
    bool isValid() const {
        return std::strncmp(magic, "MXRCBAG", 7) == 0;
    }

    /**
     * @brief 버전 호환성 확인
     *
     * @return true if 지원되는 버전 (현재: version == 1)
     */
    bool isSupportedVersion() const {
        return version == 1;
    }

    /**
     * @brief 체크섬 설정
     *
     * @param crc32 CRC32 체크섬 값
     */
    void setChecksum(uint32_t crc32) {
        checksum = crc32;
    }

    /**
     * @brief 인덱스 정보 설정
     *
     * @param offset 인덱스 블록 시작 위치
     * @param count 인덱스 엔트리 개수
     */
    void setIndexInfo(uint64_t offset, uint64_t count) {
        index_offset = offset;
        index_count = count;
    }

    /**
     * @brief 데이터 크기 설정
     *
     * @param size 메시지 데이터 영역 크기
     */
    void setDataSize(uint64_t size) {
        data_size = size;
    }

    /**
     * @brief 크기 검증
     *
     * @return 구조체 크기가 64바이트인지 확인
     */
    static constexpr bool validateSize() {
        return sizeof(BagFooter) == 64;
    }
} __attribute__((packed));  // 패딩 제거 (정확히 64 bytes)

// 컴파일 타임 크기 검증
static_assert(sizeof(BagFooter) == 64,
              "BagFooter must be exactly 64 bytes");

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_DTO_BAGFOOTER_H
