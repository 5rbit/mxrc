#ifndef MXRC_CORE_LOGGING_CORE_BAGREPLAYER_H
#define MXRC_CORE_LOGGING_CORE_BAGREPLAYER_H

#include "BagReader.h"
#include "dto/BagMessage.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 재생 속도 설정
 */
struct ReplaySpeed {
    double multiplier;  ///< 재생 속도 배율 (1.0 = 실시간, 2.0 = 2배속)

    static ReplaySpeed realtime() { return {1.0}; }
    static ReplaySpeed fast(double multiplier) { return {multiplier}; }
    static ReplaySpeed asFastAsPossible() { return {0.0}; }  // 0.0 = 최대 속도

    bool isRealtime() const { return multiplier > 0.0; }
};

/**
 * @brief Bag 파일 재생 통계
 */
struct ReplayStats {
    uint64_t messagesReplayed = 0;  ///< 재생된 메시지 수
    uint64_t messagesSkipped = 0;   ///< 건너뛴 메시지 수
    double elapsedTime = 0.0;       ///< 경과 시간 (초)
    double progress = 0.0;          ///< 진행률 (0.0 ~ 1.0)
};

/**
 * @brief Bag 파일 재생기
 *
 * Bag 파일을 읽어서 타임스탬프에 따라 메시지를 재생합니다.
 * 실시간 속도 또는 사용자 지정 속도로 재생할 수 있습니다.
 *
 * **주요 기능**:
 * - 실시간 재생 (1x 속도)
 * - 배속 재생 (2x, 5x 등)
 * - 최대 속도 재생 (타임스탬프 무시)
 * - 특정 시간 구간 재생
 * - 토픽 필터링
 * - 일시정지/재개/중지
 * - 진행률 추적
 *
 * **사용 예시**:
 * ```cpp
 * // 1. 실시간 재생
 * BagReplayer replayer;
 * replayer.open("/data/bag_20231114.bag");
 * replayer.setMessageCallback([](const BagMessage& msg) {
 *     std::cout << "Topic: " << msg.topic << std::endl;
 * });
 * replayer.start(ReplaySpeed::realtime());
 * replayer.waitUntilFinished();
 *
 * // 2. 2배속 재생
 * replayer.start(ReplaySpeed::fast(2.0));
 *
 * // 3. 최대 속도 재생
 * replayer.start(ReplaySpeed::asFastAsPossible());
 *
 * // 4. 특정 구간 재생
 * replayer.setTimeRange(startTime, endTime);
 * replayer.start(ReplaySpeed::realtime());
 * ```
 *
 * **Thread-Safety**: 재생 제어 메서드는 thread-safe
 */
class BagReplayer {
public:
    using MessageCallback = std::function<void(const BagMessage&)>;

    /**
     * @brief 기본 생성자
     */
    BagReplayer();

    /**
     * @brief 소멸자 (자동 정지)
     */
    ~BagReplayer();

    /**
     * @brief Bag 파일 열기
     *
     * @param filepath Bag 파일 경로
     * @return true if 성공
     */
    bool open(const std::string& filepath);

    /**
     * @brief 파일 닫기 (재생 중이면 자동 정지)
     */
    void close();

    /**
     * @brief 재생 시작
     *
     * @param speed 재생 속도 설정
     * @return true if 성공
     */
    bool start(const ReplaySpeed& speed = ReplaySpeed::realtime());

    /**
     * @brief 재생 일시정지
     */
    void pause();

    /**
     * @brief 재생 재개
     */
    void resume();

    /**
     * @brief 재생 중지
     */
    void stop();

    /**
     * @brief 재생 완료까지 대기
     */
    void waitUntilFinished();

    /**
     * @brief 메시지 콜백 설정
     *
     * @param callback 메시지 수신 시 호출될 함수
     */
    void setMessageCallback(MessageCallback callback);

    /**
     * @brief 토픽 필터 설정
     *
     * @param topic 재생할 토픽 (빈 문자열이면 모든 토픽)
     */
    void setTopicFilter(const std::string& topic);

    /**
     * @brief 시간 범위 설정
     *
     * @param startTime 시작 타임스탬프 (나노초)
     * @param endTime 종료 타임스탬프 (나노초)
     */
    void setTimeRange(uint64_t startTime, uint64_t endTime);

    /**
     * @brief 재생 통계 조회
     *
     * @return 현재 재생 통계
     */
    ReplayStats getStats() const;

    /**
     * @brief 재생 중인지 확인
     *
     * @return true if 재생 중
     */
    bool isPlaying() const;

    /**
     * @brief 일시정지 상태 확인
     *
     * @return true if 일시정지
     */
    bool isPaused() const;

private:
    /**
     * @brief 재생 스레드 (메인 루프)
     */
    void replayThread();

    /**
     * @brief 다음 메시지까지 대기 (타임스탬프 기반)
     *
     * @param currentMsg 현재 메시지
     * @param nextMsg 다음 메시지
     */
    void waitForNextMessage(const BagMessage& currentMsg, const BagMessage& nextMsg);

    /**
     * @brief 메시지가 시간 범위 내인지 확인
     *
     * @param msg 메시지
     * @return true if 범위 내
     */
    bool isInTimeRange(const BagMessage& msg) const;

    std::unique_ptr<BagReader> reader_;         ///< Bag 파일 리더
    MessageCallback messageCallback_;           ///< 메시지 콜백
    ReplaySpeed speed_;                         ///< 재생 속도
    std::string topicFilter_;                   ///< 토픽 필터

    uint64_t startTime_ = 0;                    ///< 시작 타임스탬프 (0 = 처음부터)
    uint64_t endTime_ = UINT64_MAX;             ///< 종료 타임스탬프 (MAX = 끝까지)

    std::unique_ptr<std::thread> replayThread_; ///< 재생 스레드
    std::atomic<bool> isPlaying_{false};        ///< 재생 상태
    std::atomic<bool> isPaused_{false};         ///< 일시정지 상태
    std::atomic<bool> shouldStop_{false};       ///< 중지 요청

    ReplayStats stats_;                         ///< 재생 통계
    mutable std::mutex statsMutex_;             ///< 통계 접근 동기화
    mutable std::mutex controlMutex_;           ///< 재생 제어 동기화

    std::chrono::steady_clock::time_point replayStartTime_;  ///< 재생 시작 시각
    uint64_t firstMessageTime_ = 0;             ///< 첫 메시지 타임스탬프
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_CORE_BAGREPLAYER_H
