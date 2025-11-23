// BagReplayer.h
// Bag 파일 재생 엔진 (FR-014 ~ FR-016, FR-025)
//
// 이 클래스는 Bag 파일에 기록된 메시지를 타임스탬프 순서대로
// DataStore에 재생하여, 과거 실행 상태를 재현합니다.

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <cstdint>
#include "BagReader.h"
#include "../data-model.md"  // BagMessage

namespace mxrc::core::logging {

// Forward declaration (DataStore는 다른 모듈)
class DataStore;

/// @brief Replay 상태 열거형
enum class ReplayState {
    IDLE,       ///< 대기 중
    RUNNING,    ///< 재생 중
    PAUSED,     ///< 일시정지
    COMPLETED,  ///< 재생 완료
    FAILED      ///< 재생 실패
};

/// @brief Bag 파일 재생 엔진
///
/// 이 클래스는 다음 기능 요구사항을 충족합니다:
/// - FR-014: DataStore 상태 재현 (타임스탬프 순서 보장)
/// - FR-015: 재생 속도 배율 조정 (0.1x ~ 10x)
/// - FR-016: 시간 범위 필터링
/// - FR-025: Replay 불일치 감지 및 로그 기록
class BagReplayer {
public:
    /// @brief 생성자
    ///
    /// @param reader Bag 파일 읽기 객체
    BagReplayer(std::shared_ptr<BagReader> reader);

    /// @brief 소멸자
    ///
    /// 재생 중인 스레드를 안전하게 종료합니다.
    ~BagReplayer();

    /// @brief Bag 파일 재생 시작 (FR-014)
    ///
    /// BagReader에서 메시지를 순차적으로 읽어서 DataStore에 쓰기합니다.
    /// 타임스탬프 간격에 따라 sleep하여 실시간 재생을 시뮬레이션합니다.
    ///
    /// @param dataStore 재생 대상 DataStore
    /// @throws std::runtime_error DataStore가 nullptr인 경우
    /// @note 비동기로 실행되며, 백그라운드 스레드에서 재생됩니다.
    void replay(std::shared_ptr<DataStore> dataStore);

    /// @brief 재생 일시정지
    ///
    /// 현재 메시지 재생 후 일시정지합니다.
    void pause();

    /// @brief 재생 재개
    ///
    /// 일시정지된 지점부터 재생을 계속합니다.
    void resume();

    /// @brief 재생 중지
    ///
    /// 재생을 즉시 중단하고 스레드를 종료합니다.
    void stop();

    /// @brief 재생 속도 배율 설정 (FR-015)
    ///
    /// 재생 속도를 배율로 조정합니다.
    ///
    /// @param factor 속도 배율 (0.1 ~ 10.0)
    /// @throws std::invalid_argument 범위를 벗어난 경우
    /// @example
    ///   replayer->setSpeedFactor(2.0);  // 2배속 재생
    ///   replayer->setSpeedFactor(0.5);  // 0.5배속 재생 (슬로우 모션)
    void setSpeedFactor(double factor);

    /// @brief 재생 시간 범위 설정 (FR-016)
    ///
    /// 지정된 시간 범위 내의 메시지만 재생합니다.
    ///
    /// @param start_ns 시작 타임스탬프 (나노초)
    /// @param end_ns 종료 타임스탬프 (나노초)
    /// @throws std::invalid_argument start_ns >= end_ns인 경우
    void setTimeRange(int64_t start_ns, int64_t end_ns);

    /// @brief 재생 진행률 조회 (0.0 ~ 1.0)
    ///
    /// @return 진행률 (0.0 = 시작, 1.0 = 완료)
    double getProgress() const;

    /// @brief 현재 재생 상태 조회
    ///
    /// @return ReplayState (IDLE, RUNNING, PAUSED, COMPLETED, FAILED)
    ReplayState getState() const;

    /// @brief 재생 불일치 감지 콜백 등록 (FR-025)
    ///
    /// Replay 중 DataStore 값과 예상 값이 다를 경우 호출되는 콜백을 등록합니다.
    ///
    /// @param callback 콜백 함수 (topic, expected_value, actual_value)
    /// @example
    ///   replayer->onMismatch([](const std::string& topic,
    ///                           const std::string& expected,
    ///                           const std::string& actual) {
    ///       spdlog::error("Replay mismatch: {} expected={} actual={}",
    ///                     topic, expected, actual);
    ///   });
    void onMismatch(std::function<void(const std::string&,
                                        const std::string&,
                                        const std::string&)> callback);

    /// @brief 재생 완료 콜백 등록
    ///
    /// 재생이 완료되면 호출되는 콜백을 등록합니다.
    ///
    /// @param callback 콜백 함수 (success, messagesReplayed)
    void onComplete(std::function<void(bool, uint64_t)> callback);

    /// @brief 재생된 메시지 개수 조회
    ///
    /// @return 재생된 메시지 개수
    uint64_t getMessagesReplayed() const;

    /// @brief 불일치 개수 조회 (FR-025)
    ///
    /// @return 불일치 감지 횟수
    uint64_t getMismatchCount() const;

private:
    std::shared_ptr<BagReader> reader_;
    std::shared_ptr<DataStore> dataStore_;

    std::atomic<ReplayState> state_{ReplayState::IDLE};
    std::atomic<double> speedFactor_{1.0};
    std::atomic<uint64_t> messagesReplayed_{0};
    std::atomic<uint64_t> mismatchCount_{0};

    int64_t startTime_ns_ = 0;
    int64_t endTime_ns_ = INT64_MAX;
    int64_t totalMessages_ = 0;

    std::function<void(const std::string&, const std::string&, const std::string&)> mismatchCallback_;
    std::function<void(bool, uint64_t)> completeCallback_;

    std::thread replayThread_;
    std::atomic<bool> stopRequested_{false};

    /// @brief 재생 루프 (백그라운드 스레드)
    void replayLoop();

    /// @brief 메시지를 DataStore에 쓰기
    void applyMessage(const BagMessage& msg);

    /// @brief DataStore 값 검증 (FR-025)
    bool verifyValue(const std::string& topic, const std::string& expected);

    /// @brief 타임스탬프 간격만큼 sleep
    void sleepForTimestamp(int64_t prev_ns, int64_t current_ns);
};

} // namespace mxrc::core::logging
