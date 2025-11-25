#pragma once

#include <string>
#include <chrono>
#include <optional>
#include "../dto/AlarmDto.h"
#include "../dto/AlarmSeverity.h"

namespace mxrc::core::alarm {

/**
 * @brief Alarm 클래스
 *
 * 개별 alarm 인스턴스를 표현합니다.
 * AlarmDto의 래퍼 클래스로, 추가적인 비즈니스 로직을 제공합니다.
 *
 * Feature 016: Pallet Shuttle Control System
 */
class Alarm {
public:
    /**
     * @brief 생성자
     *
     * @param code Alarm 코드
     * @param name Alarm 이름
     * @param severity 심각도
     * @param source 발생 소스
     */
    Alarm(
        std::string code,
        std::string name,
        AlarmSeverity severity,
        std::string source);

    /**
     * @brief AlarmDto로 변환
     */
    AlarmDto toDto() const { return dto_; }

    /**
     * @brief Alarm ID 조회
     */
    const std::string& getId() const { return dto_.alarm_id; }

    /**
     * @brief Alarm 코드 조회
     */
    const std::string& getCode() const { return dto_.alarm_code; }

    /**
     * @brief 심각도 조회
     */
    AlarmSeverity getSeverity() const { return dto_.severity; }

    /**
     * @brief 상태 조회
     */
    AlarmState getState() const { return dto_.state; }

    /**
     * @brief 상세 메시지 설정
     */
    void setDetails(const std::string& details);

    /**
     * @brief Alarm 확인
     */
    void acknowledge(const std::string& acknowledged_by);

    /**
     * @brief Alarm 해결
     */
    void resolve();

    /**
     * @brief 재발 기록
     */
    void recordRecurrence();

    /**
     * @brief 재발 횟수 설정
     */
    void setRecurrenceCount(uint32_t count);

    /**
     * @brief 심각도 상향
     */
    void escalateSeverity(AlarmSeverity new_severity);

    /**
     * @brief 활성 상태 확인
     */
    bool isActive() const { return dto_.isActive(); }

    /**
     * @brief 경과 시간 (ms)
     */
    int64_t getElapsedMs() const { return dto_.getElapsedMs(); }

private:
    AlarmDto dto_;
};

} // namespace mxrc::core::alarm
