#ifndef MXRC_CORE_LOGGING_CORE_BAGREADER_H
#define MXRC_CORE_LOGGING_CORE_BAGREADER_H

#include "dto/BagMessage.h"
#include "dto/BagFooter.h"
#include "dto/IndexEntry.h"
#include "util/Indexer.h"
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <memory>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 읽기 클래스
 *
 * Bag 파일로부터 메시지를 읽고, 타임스탬프 기반으로 탐색합니다.
 *
 * **주요 기능**:
 * - Bag 파일 열기 및 검증
 * - 순차적 메시지 읽기 (Iterator 패턴)
 * - 타임스탬프 기반 탐색 (seek)
 * - 토픽 필터링
 * - 메타데이터 조회
 *
 * **사용 예시**:
 * ```cpp
 * BagReader reader;
 * if (!reader.open("/data/bag_20231114_150000.bag")) {
 *     spdlog::error("Failed to open bag file");
 *     return;
 * }
 *
 * // 1. 순차 읽기
 * while (reader.hasNext()) {
 *     auto msg = reader.readNext();
 *     if (msg) {
 *         spdlog::info("Topic: {}, Timestamp: {}", msg->topic, msg->timestamp_ns);
 *     }
 * }
 *
 * // 2. 타임스탬프 기반 탐색
 * reader.seekToTimestamp(1700000000000000000);  // 특정 시간으로 이동
 * auto msg = reader.readNext();
 *
 * // 3. 토픽 필터링
 * reader.setTopicFilter("robot_position");
 * while (reader.hasNext()) {
 *     auto msg = reader.readNext();  // robot_position 토픽만 반환
 * }
 * ```
 *
 * **Thread-Safety**: NOT thread-safe (단일 스레드 사용 권장)
 */
class BagReader {
public:
    /**
     * @brief 기본 생성자
     */
    BagReader();

    /**
     * @brief 소멸자 (파일 자동 닫기)
     */
    ~BagReader();

    /**
     * @brief Bag 파일 열기
     *
     * 파일을 열고 Footer 및 인덱스를 로드합니다.
     *
     * @param filepath Bag 파일 경로
     * @return true if 성공
     */
    bool open(const std::string& filepath);

    /**
     * @brief 파일 닫기
     */
    void close();

    /**
     * @brief 파일이 열려있는지 확인
     *
     * @return true if 열려있음
     */
    bool isOpen() const { return ifs_.is_open(); }

    /**
     * @brief 다음 메시지가 있는지 확인
     *
     * @return true if 읽을 메시지가 남아있음
     */
    bool hasNext() const;

    /**
     * @brief 다음 메시지 읽기
     *
     * 현재 파일 위치에서 다음 메시지를 읽습니다.
     * 토픽 필터가 설정된 경우, 필터링된 메시지만 반환합니다.
     *
     * @return BagMessage (실패 시 std::nullopt)
     */
    std::optional<BagMessage> readNext();

    /**
     * @brief 타임스탬프로 탐색
     *
     * 지정된 타임스탬프 이상의 첫 번째 메시지 위치로 이동합니다.
     *
     * @param timestamp_ns 나노초 단위 타임스탬프
     * @return true if 성공
     */
    bool seekToTimestamp(uint64_t timestamp_ns);

    /**
     * @brief 파일 시작 위치로 이동
     */
    void seekToStart();

    /**
     * @brief 토픽 필터 설정
     *
     * 설정된 토픽만 읽습니다. 빈 문자열이면 필터링 비활성화.
     *
     * @param topic 토픽 이름
     */
    void setTopicFilter(const std::string& topic);

    /**
     * @brief 토픽 필터 제거
     */
    void clearTopicFilter();

    /**
     * @brief 현재 Bag 파일의 메타데이터 조회
     *
     * @return BagFooter
     */
    BagFooter getFooter() const { return footer_; }

    /**
     * @brief 전체 메시지 개수
     *
     * @return 인덱스에 기록된 메시지 개수
     */
    size_t getMessageCount() const;

    /**
     * @brief 시작 타임스탬프 조회
     *
     * @return 첫 번째 메시지의 타임스탬프 (나노초)
     */
    uint64_t getStartTimestamp() const;

    /**
     * @brief 종료 타임스탬프 조회
     *
     * @return 마지막 메시지의 타임스탬프 (나노초)
     */
    uint64_t getEndTimestamp() const;

    /**
     * @brief 파일 경로 조회
     *
     * @return 현재 열린 파일 경로
     */
    std::string getFilePath() const { return filepath_; }

private:
    /**
     * @brief JSONL 한 줄 읽기
     *
     * @return JSONL 라인 (실패 시 std::nullopt)
     */
    std::optional<std::string> readLine();

    /**
     * @brief 현재 위치가 데이터 영역 내인지 확인
     *
     * @return true if 데이터 영역 내
     */
    bool isInDataArea() const;

    std::string filepath_;          ///< 현재 파일 경로
    std::ifstream ifs_;             ///< 파일 입력 스트림
    BagFooter footer_;              ///< Bag 파일 Footer
    Indexer indexer_;               ///< 인덱스 관리자
    std::string topicFilter_;       ///< 토픽 필터 (빈 문자열이면 비활성화)
    uint64_t currentPosition_;      ///< 현재 파일 읽기 위치
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_CORE_BAGREADER_H
