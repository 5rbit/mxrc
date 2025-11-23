/**
 * @file event-priority.h
 * @brief EventPriority enum 및 PrioritizedEvent 구조체 정의 (문서화용 스니펫)
 *
 * Feature: 022-fix-architecture-issues
 * Phase: 1 (Design)
 * Date: 2025-01-22
 *
 * 본 파일은 실제 구현 코드가 아닌, 설계 문서화를 위한 헤더 스니펫입니다.
 * 실제 구현은 src/core/event/core/EventPriority.h에 위치합니다.
 */

#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace mxrc {
namespace event {

/**
 * @brief EventBus 이벤트의 3단계 우선순위
 *
 * EventBus에서 전송되는 이벤트의 처리 우선순위를 정의합니다.
 * 우선순위에 따라 큐 포화 시 버림(Drop) 정책이 달라집니다.
 *
 * 우선순위 순서:
 * 1. CRITICAL (최고 우선순위)
 * 2. NORMAL
 * 3. DEBUG (최저 우선순위)
 *
 * Drop Policy:
 * - CRITICAL: 절대 버리지 않음 (큐 가득 차도 삽입 시도)
 * - NORMAL: 큐 90% 이상 차면 버림
 * - DEBUG: 큐 80% 이상 차면 버림
 *
 * 사용 예시:
 * @code
 * // CRITICAL 이벤트 (RT 경로)
 * PrioritizedEvent error_event{
 *     .type = "sensor.fault",
 *     .priority = EventPriority::CRITICAL,
 *     .payload = error_code,
 *     .timestamp_ns = now()
 * };
 * eventBus->push(std::move(error_event));
 *
 * // DEBUG 이벤트 (Non-RT 경로)
 * PrioritizedEvent debug_event{
 *     .type = "metrics.update",
 *     .priority = EventPriority::DEBUG,
 *     .payload = metrics_data,
 *     .timestamp_ns = now()
 * };
 * eventBus->push(std::move(debug_event));  // 큐 80% 이상 차면 버려짐
 * @endcode
 */
enum class EventPriority : int {
    /**
     * @brief CRITICAL 우선순위 (최고)
     *
     * 오류, 상태 변경 등 즉시 처리가 필요한 이벤트.
     * 절대 버려지지 않으며, 큐가 가득 차도 삽입을 시도합니다.
     *
     * 사용 사례:
     * - 센서 고장 (sensor.fault)
     * - 비상 정지 (emergency.stop)
     * - 제어 모드 변경 (control.mode_changed)
     * - IPC 채널 장애 (ipc.failure)
     */
    CRITICAL = 0,

    /**
     * @brief NORMAL 우선순위 (중간)
     *
     * 일반적인 이벤트 (센서 데이터, 로그 등).
     * 큐 90% 이상 차면 버려질 수 있습니다.
     *
     * 사용 사례:
     * - 센서 데이터 업데이트 (sensor.data_updated)
     * - Task 진행률 업데이트 (task.progress_updated)
     * - 일반 로그 (log.info)
     */
    NORMAL = 1,

    /**
     * @brief DEBUG 우선순위 (최저)
     *
     * 디버그 로그, 메트릭 등.
     * 큐 80% 이상 차면 버려집니다.
     *
     * 사용 사례:
     * - 디버그 로그 (log.debug)
     * - 성능 메트릭 (metrics.performance)
     * - 통계 업데이트 (stats.updated)
     */
    DEBUG = 2
};

/**
 * @brief 우선순위 이벤트 엔티티
 *
 * EventBus에서 전송되는 이벤트를 표현하는 구조체입니다.
 * 우선순위, 타입, 페이로드, 타임스탬프를 포함합니다.
 *
 * 메모리 레이아웃:
 * - type: 64 bytes (고정 크기 문자열, stack 할당)
 * - priority: 4 bytes (enum)
 * - payload: 32 bytes (variant)
 * - timestamp_ns: 8 bytes
 * - Total: ~112 bytes (2 cache lines)
 *
 * 사용 예시 (RT 경로, move semantics):
 * @code
 * PrioritizedEvent event{
 *     .type = "sensor.fault",
 *     .priority = EventPriority::CRITICAL,
 *     .payload = 42,  // 에러 코드
 *     .timestamp_ns = getCurrentTimeNs()
 * };
 * bool pushed = eventBus->push(std::move(event));  // move semantics
 * @endcode
 */
struct PrioritizedEvent {
    /**
     * @brief 이벤트 타입 문자열
     *
     * 네임스페이스 형식 권장: "domain.event_name"
     * 예: "sensor.fault", "task.completed", "ipc.failure"
     *
     * @note 최대 길이: 64자 (스택 할당을 위해 제한)
     * @note std::string 대신 고정 크기 배열 사용 고려 (RT 안전)
     */
    std::string type;

    /**
     * @brief 이벤트 우선순위
     */
    EventPriority priority;

    /**
     * @brief 이벤트 페이로드 (다양한 타입 지원)
     *
     * std::variant를 사용하여 다양한 타입의 데이터를 저장합니다.
     * RT 경로에서는 단순 타입(int, double)만 사용 권장.
     *
     * 지원 타입:
     * - int: 에러 코드, 상태 값
     * - double: 센서 값, 메트릭
     * - std::string: 로그 메시지
     *
     * 사용 예시:
     * @code
     * PrioritizedEvent event;
     * event.payload = 42;  // int
     * event.payload = 3.14;  // double
     * event.payload = std::string("Error message");  // string
     *
     * // 페이로드 읽기
     * if (auto* val = std::get_if<int>(&event.payload)) {
     *     spdlog::info("Error code: {}", *val);
     * }
     * @endcode
     */
    std::variant<int, double, std::string> payload;

    /**
     * @brief 이벤트 생성 시각 (나노초)
     *
     * std::chrono::steady_clock을 사용한 monotonic time.
     * 이벤트 지연 시간 측정 및 디버깅에 사용됩니다.
     *
     * 사용 예시:
     * @code
     * uint64_t latency_ns = getCurrentTimeNs() - event.timestamp_ns;
     * if (latency_ns > 1'000'000) {  // 1ms 이상
     *     spdlog::warn("Event latency: {} μs", latency_ns / 1000);
     * }
     * @endcode
     */
    uint64_t timestamp_ns;

    /**
     * @brief 기본 생성자
     */
    PrioritizedEvent()
        : type(""), priority(EventPriority::NORMAL), payload(0), timestamp_ns(0) {}

    /**
     * @brief 필드 초기화 생성자
     *
     * @param t 이벤트 타입
     * @param p 우선순위
     * @param pl 페이로드
     * @param ts 타임스탬프
     */
    PrioritizedEvent(
        const std::string& t,
        EventPriority p,
        const std::variant<int, double, std::string>& pl,
        uint64_t ts
    ) : type(t), priority(p), payload(pl), timestamp_ns(ts) {}

    /**
     * @brief 이동 생성자 (RT 안전)
     *
     * EventBus push 시 move semantics 사용 (복사 방지).
     */
    PrioritizedEvent(PrioritizedEvent&&) noexcept = default;

    /**
     * @brief 이동 대입 연산자
     */
    PrioritizedEvent& operator=(PrioritizedEvent&&) noexcept = default;

    /**
     * @brief 복사 생성자 삭제
     *
     * RT 경로에서 복사 금지 (성능 이유).
     * move semantics만 허용.
     */
    PrioritizedEvent(const PrioritizedEvent&) = delete;

    /**
     * @brief 복사 대입 연산자 삭제
     */
    PrioritizedEvent& operator=(const PrioritizedEvent&) = delete;
};

/**
 * @brief 우선순위 문자열 변환 (디버깅용)
 *
 * @param priority EventPriority enum 값
 * @return 우선순위 문자열 ("CRITICAL", "NORMAL", "DEBUG")
 */
inline const char* priorityToString(EventPriority priority) {
    switch (priority) {
        case EventPriority::CRITICAL:
            return "CRITICAL";
        case EventPriority::NORMAL:
            return "NORMAL";
        case EventPriority::DEBUG:
            return "DEBUG";
        default:
            return "UNKNOWN";
    }
}

}  // namespace event
}  // namespace mxrc
