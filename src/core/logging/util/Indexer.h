#ifndef MXRC_CORE_LOGGING_UTIL_INDEXER_H
#define MXRC_CORE_LOGGING_UTIL_INDEXER_H

#include "dto/IndexEntry.h"
#include "dto/BagFooter.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 인덱싱 유틸리티
 *
 * Bag 파일의 인덱스 블록을 생성, 저장, 로드하고
 * 타임스탬프 기반 이진 탐색을 제공합니다.
 *
 * **파일 구조**:
 * ```
 * [Messages...] [Index Block] [Footer (64 bytes)]
 * ```
 *
 * **사용 예시**:
 * ```cpp
 * Indexer indexer;
 *
 * // 1. 인덱스 생성
 * indexer.addEntry(1700000000000000000, 0);
 * indexer.addEntry(1700000001000000000, 1024);
 *
 * // 2. 파일에 저장
 * indexer.writeToFile(ofs, dataSize);
 *
 * // 3. 파일에서 로드
 * Indexer loaded;
 * BagFooter footer = loaded.readFromFile("file.bag");
 *
 * // 4. 타임스탬프 탐색
 * auto entry = loaded.findByTimestamp(1700000000500000000);
 * ```
 */
class Indexer {
public:
    /**
     * @brief 기본 생성자
     */
    Indexer() = default;

    /**
     * @brief 인덱스 엔트리 추가
     *
     * @param timestamp_ns 타임스탬프 (나노초)
     * @param file_offset 파일 오프셋 (바이트)
     */
    void addEntry(uint64_t timestamp_ns, uint64_t file_offset);

    /**
     * @brief 인덱스 블록을 파일에 쓰기
     *
     * 파일 포맷:
     * - Index Block: [IndexEntry...] (entries_.size() * 16 bytes)
     * - Footer: BagFooter (64 bytes)
     *
     * @param ofs 출력 파일 스트림 (append 모드)
     * @param dataSize 메시지 데이터 영역 크기 (바이트)
     * @return 쓰기 성공 여부
     */
    bool writeToFile(std::ofstream& ofs, uint64_t dataSize);

    /**
     * @brief 파일에서 인덱스 블록 및 푸터 읽기
     *
     * 파일 끝에서 64바이트를 읽어 Footer를 파싱하고,
     * Footer의 index_offset을 사용하여 인덱스 블록을 로드합니다.
     *
     * @param filepath Bag 파일 경로
     * @return BagFooter (isValid() == false면 실패)
     */
    BagFooter readFromFile(const std::string& filepath);

    /**
     * @brief 타임스탬프로 인덱스 엔트리 찾기 (이진 탐색)
     *
     * 주어진 타임스탬프보다 작거나 같은 가장 최근 엔트리를 반환합니다.
     * (lower_bound 방식)
     *
     * @param timestamp_ns 찾을 타임스탬프 (나노초)
     * @param found 찾았는지 여부 (출력 파라미터)
     * @return 찾은 IndexEntry (found == false면 유효하지 않음)
     */
    IndexEntry findByTimestamp(uint64_t timestamp_ns, bool& found) const;

    /**
     * @brief 인덱스 엔트리 개수 반환
     *
     * @return 엔트리 개수
     */
    size_t size() const { return entries_.size(); }

    /**
     * @brief 인덱스가 비어있는지 확인
     *
     * @return true if 엔트리가 없음
     */
    bool empty() const { return entries_.empty(); }

    /**
     * @brief 인덱스 초기화
     */
    void clear() { entries_.clear(); }

    /**
     * @brief 모든 엔트리 반환 (읽기 전용)
     *
     * @return 인덱스 엔트리 벡터
     */
    const std::vector<IndexEntry>& getEntries() const { return entries_; }

    /**
     * @brief CRC32 체크섬 계산
     *
     * 데이터 + 인덱스 블록의 체크섬을 계산합니다.
     *
     * @param filepath Bag 파일 경로
     * @param dataSize 메시지 데이터 크기
     * @param indexSize 인덱스 블록 크기
     * @return CRC32 체크섬
     */
    static uint32_t calculateChecksum(const std::string& filepath,
                                       uint64_t dataSize,
                                       uint64_t indexSize);

private:
    std::vector<IndexEntry> entries_;  ///< 인덱스 엔트리 목록

    /**
     * @brief 데이터의 CRC32 계산 (내부 헬퍼)
     *
     * @param data 데이터 포인터
     * @param length 데이터 길이
     * @return CRC32 값
     */
    static uint32_t crc32(const char* data, size_t length);
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_UTIL_INDEXER_H
