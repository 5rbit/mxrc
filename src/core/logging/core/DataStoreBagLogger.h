#ifndef MXRC_CORE_LOGGING_CORE_DATASTOREBAGLOGGER_H
#define MXRC_CORE_LOGGING_CORE_DATASTOREBAGLOGGER_H

#include "interfaces/IBagWriter.h"
#include "core/event/interfaces/IEventBus.h"
#include "core/event/dto/DataStoreEvents.h"
#include "dto/BagMessage.h"
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

namespace mxrc::core::logging {

/**
 * @brief DataStore 이벤트를 Bag 파일로 기록하는 로거
 *
 * EventBus를 구독하여 DataStoreValueChangedEvent를 수신하고,
 * IBagWriter를 사용하여 Bag 파일에 비동기로 기록합니다.
 *
 * 주요 기능:
 * - EventBus 구독 및 이벤트 수신
 * - DataStore 변경사항을 BagMessage로 변환
 * - IBagWriter를 통한 비동기 쓰기
 * - 통계 수집 (기록/드롭 카운트)
 * - 안전한 시작/종료 제어
 */
class DataStoreBagLogger {
public:
    /**
     * @brief DataStoreBagLogger 생성자
     *
     * @param eventBus EventBus 인스턴스 (이벤트 구독용)
     * @param bagWriter Bag Writer 인스턴스 (파일 쓰기용)
     * @throws std::invalid_argument eventBus 또는 bagWriter가 nullptr인 경우
     */
    explicit DataStoreBagLogger(
        std::shared_ptr<event::IEventBus> eventBus,
        std::shared_ptr<IBagWriter> bagWriter);

    ~DataStoreBagLogger();

    /**
     * @brief 로거 시작
     *
     * EventBus 구독을 등록하고 Bag Writer를 시작합니다.
     * @return 성공하면 true, 이미 실행 중이면 false
     */
    bool start();

    /**
     * @brief 로거 정지
     *
     * EventBus 구독을 해제하고 Bag Writer를 안전하게 종료합니다.
     * 큐에 남은 모든 메시지를 flush한 후 종료합니다.
     */
    void stop();

    /**
     * @brief 실행 상태 확인
     * @return 실행 중이면 true, 정지 상태이면 false
     */
    bool isRunning() const;

    /**
     * @brief 누적 통계 조회
     * @return BagWriterStats 구조체 (messagesWritten, messagesDropped 등)
     */
    BagWriterStats getStats() const;

    /**
     * @brief 현재 Bag 파일 경로 조회
     * @return 현재 활성화된 Bag 파일 경로
     */
    std::string getCurrentFilePath() const;

    /**
     * @brief Rotation 정책 설정
     * @param policy 순환 정책 (SIZE 또는 TIME 기반)
     */
    void setRotationPolicy(const RotationPolicy& policy);

    /**
     * @brief Retention 정책 설정
     * @param policy 보존 정책 (COUNT 또는 TIME 기반)
     */
    void setRetentionPolicy(const RetentionPolicy& policy);

    /**
     * @brief 수동 flush 실행
     *
     * 큐에 남은 모든 메시지를 강제로 디스크에 기록합니다.
     * @param timeoutMs 타임아웃 (밀리초)
     * @return 성공하면 true, 타임아웃 발생 시 false
     */
    bool flush(uint32_t timeoutMs = 5000);

private:
    /**
     * @brief EventBus 콜백 - DataStoreValueChangedEvent 처리
     *
     * @param event DataStore 변경 이벤트
     */
    void onDataStoreEvent(std::shared_ptr<event::IEvent> event);

    /**
     * @brief DataStoreValueChangedEvent를 BagMessage로 변환
     *
     * @param event DataStore 변경 이벤트
     * @return BagMessage 인스턴스
     */
    BagMessage convertToBagMessage(
        const std::shared_ptr<event::DataStoreValueChangedEvent>& event);

    std::shared_ptr<event::IEventBus> eventBus_;    ///< EventBus 인스턴스
    std::shared_ptr<IBagWriter> bagWriter_;         ///< Bag Writer 인스턴스
    event::SubscriptionId subscriptionId_;          ///< 구독 ID (해제 시 사용)
    std::atomic<bool> isRunning_;                   ///< 실행 상태
    std::atomic<uint64_t> eventsReceived_;          ///< 수신한 이벤트 수
    std::atomic<uint64_t> eventsDropped_;           ///< 드롭된 이벤트 수

    mutable std::mutex mutex_;                      ///< 동기화 뮤텍스
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_CORE_DATASTOREBAGLOGGER_H
