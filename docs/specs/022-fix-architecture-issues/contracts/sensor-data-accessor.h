/**
 * @file sensor-data-accessor.h
 * @brief ISensorDataAccessor 인터페이스 정의 (문서화용 스니펫)
 *
 * Feature: 022-fix-architecture-issues
 * Phase: 1 (Design)
 * Date: 2025-01-22
 *
 * 본 파일은 실제 구현 코드가 아닌, 설계 문서화를 위한 헤더 스니펫입니다.
 * 실제 구현은 src/core/datastore/interfaces/ISensorDataAccessor.h에 위치합니다.
 */

#pragma once

#include "data-accessor-interface.h"
#include "versioned-data.h"

namespace mxrc {
namespace datastore {

/**
 * @brief 센서 도메인(sensor.*)에 대한 타입 안전 접근 인터페이스
 *
 * DataStore의 sensor.* 키에 대한 읽기/쓰기 작업을 제공합니다.
 * 다른 도메인(robot_state.*, task_status.* 등)에 대한 접근은 차단됩니다.
 *
 * 접근 가능 키:
 * - sensor.temperature (온도 센서, °C)
 * - sensor.pressure (압력 센서, Pa)
 * - sensor.humidity (습도 센서, %)
 * - sensor.vibration (진동 센서, mm/s²)
 * - sensor.current (전류 센서, A)
 *
 * 접근 권한:
 * - 읽기: RT 경로 (Control Loop에서 피드백)
 * - 쓰기: Non-RT 경로 (센서 드라이버만)
 *
 * 사용 예시 (RT 경로):
 * @code
 * void ControlLoop::readSensorFeedback() {
 *     auto temp = sensorAccessor->getTemperature();
 *     double temp_value = temp.value;  // 최신 값 바로 사용
 *     // ... 제어 로직 ...
 * }
 * @endcode
 *
 * 사용 예시 (Non-RT 경로):
 * @code
 * void SensorDriver::updateTemperature(double temp_celsius) {
 *     sensorAccessor->setTemperature(temp_celsius);
 *     // DataStore 내부: version++, timestamp_ns = now()
 * }
 * @endcode
 *
 * @see IDataAccessor
 * @see VersionedData
 */
class ISensorDataAccessor : public IDataAccessor {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~ISensorDataAccessor() = default;

    /**
     * @brief 도메인 이름 반환
     * @return "sensor"
     */
    std::string getDomain() const override { return "sensor"; }

    // ==================== 읽기 메서드 (const) ====================

    /**
     * @brief 온도 센서 값 읽기
     *
     * DataStore의 sensor.temperature 키를 읽어 VersionedData로 반환합니다.
     *
     * @return VersionedData<double> (값, 버전, 타임스탬프)
     *
     * @note RT 안전: 인라인 함수, lock-free 읽기
     * @note 지연 시간: < 60ns (목표)
     *
     * 사용 예시:
     * @code
     * auto temp = accessor->getTemperature();
     * spdlog::info("Temperature: {} °C (version {})", temp.value, temp.version);
     * @endcode
     */
    virtual VersionedData<double> getTemperature() const = 0;

    /**
     * @brief 압력 센서 값 읽기
     *
     * @return VersionedData<double> (압력, Pa)
     */
    virtual VersionedData<double> getPressure() const = 0;

    /**
     * @brief 습도 센서 값 읽기
     *
     * @return VersionedData<double> (습도, %)
     */
    virtual VersionedData<double> getHumidity() const = 0;

    /**
     * @brief 진동 센서 값 읽기
     *
     * @return VersionedData<double> (진동, mm/s²)
     */
    virtual VersionedData<double> getVibration() const = 0;

    /**
     * @brief 전류 센서 값 읽기
     *
     * @return VersionedData<double> (전류, A)
     */
    virtual VersionedData<double> getCurrent() const = 0;

    // ==================== 쓰기 메서드 (Non-RT 전용) ====================

    /**
     * @brief 온도 센서 값 쓰기
     *
     * DataStore의 sensor.temperature 키를 업데이트합니다.
     * 버전과 타임스탬프는 자동으로 증가/갱신됩니다.
     *
     * @param value 온도 값 (°C)
     *
     * @warning Non-RT 경로에서만 호출 가능 (RT 경로에서 호출 금지)
     * @warning 센서 드라이버만 호출해야 함 (다른 모듈 금지)
     *
     * 사용 예시:
     * @code
     * void SensorDriver::updateTemperature(double temp_celsius) {
     *     if (temp_celsius < -50.0 || temp_celsius > 150.0) {
     *         spdlog::error("Temperature out of range: {}", temp_celsius);
     *         return;
     *     }
     *     accessor->setTemperature(temp_celsius);
     * }
     * @endcode
     */
    virtual void setTemperature(double value) = 0;

    /**
     * @brief 압력 센서 값 쓰기
     *
     * @param value 압력 값 (Pa)
     */
    virtual void setPressure(double value) = 0;

    /**
     * @brief 습도 센서 값 쓰기
     *
     * @param value 습도 값 (%)
     */
    virtual void setHumidity(double value) = 0;

    /**
     * @brief 진동 센서 값 쓰기
     *
     * @param value 진동 값 (mm/s²)
     */
    virtual void setVibration(double value) = 0;

    /**
     * @brief 전류 센서 값 쓰기
     *
     * @param value 전류 값 (A)
     */
    virtual void setCurrent(double value) = 0;

protected:
    /**
     * @brief 기본 생성자 (protected)
     */
    ISensorDataAccessor() = default;
};

}  // namespace datastore
}  // namespace mxrc
