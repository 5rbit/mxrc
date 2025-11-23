#pragma once

#include <cstdint>

namespace mxrc {
namespace ethercat {

// 통합 제어 모드 enum
enum class ControlMode : uint8_t {
    DISABLED = 0,   // 비활성화
    POSITION = 1,   // 위치 제어
    VELOCITY = 2,   // 속도 제어
    TORQUE = 3      // 토크 제어
};

// BLDC 모터 명령
struct BLDCMotorCommand {
    uint16_t slave_id;          // EtherCAT slave 주소
    double target_velocity;     // 목표 속도 (RPM)
    double target_torque;       // 목표 토크 (Nm)
    ControlMode control_mode;   // 제어 모드
    bool enable;                // 모터 활성화
    uint64_t timestamp;         // 명령 생성 시각 (nanoseconds)

    BLDCMotorCommand()
        : slave_id(0)
        , target_velocity(0.0)
        , target_torque(0.0)
        , control_mode(ControlMode::DISABLED)
        , enable(false)
        , timestamp(0) {}

    // 검증: 범위 체크 및 안전성 확인
    bool isValid() const {
        // enable=false면 항상 유효 (안전 상태)
        if (!enable) return true;

        // DISABLED 모드면 유효
        if (control_mode == ControlMode::DISABLED) return true;

        // 속도 모드: -10000 ~ 10000 RPM
        if (control_mode == ControlMode::VELOCITY) {
            return target_velocity >= -10000.0 && target_velocity <= 10000.0;
        }

        // 토크 모드: -100 ~ 100 Nm
        if (control_mode == ControlMode::TORQUE) {
            return target_torque >= -100.0 && target_torque <= 100.0;
        }

        // POSITION 모드는 BLDC에서 지원 안 함
        return false;
    }
};

// 서보 드라이버 명령
struct ServoDriverCommand {
    uint16_t slave_id;          // EtherCAT slave 주소
    double target_position;     // 목표 위치 (라디안 또는 미터)
    double target_velocity;     // 목표 속도 (rad/s 또는 m/s)
    double target_torque;       // 목표 토크 (Nm)
    ControlMode control_mode;   // 제어 모드
    double max_velocity;        // 최대 허용 속도 (안전 제한)
    double max_torque;          // 최대 허용 토크 (안전 제한)
    bool enable;                // 모터 활성화
    uint64_t timestamp;         // 명령 생성 시각 (nanoseconds)

    ServoDriverCommand()
        : slave_id(0)
        , target_position(0.0)
        , target_velocity(0.0)
        , target_torque(0.0)
        , control_mode(ControlMode::DISABLED)
        , max_velocity(10.0)
        , max_torque(100.0)
        , enable(false)
        , timestamp(0) {}

    // 검증: 범위 체크 및 안전성 확인
    bool isValid() const {
        // enable=false면 항상 유효 (안전 상태)
        if (!enable) return true;

        // DISABLED 모드면 유효
        if (control_mode == ControlMode::DISABLED) return true;

        // 위치 모드: -2π ~ 2π (회전) 또는 -10.0 ~ 10.0 (직선)
        if (control_mode == ControlMode::POSITION) {
            constexpr double TWO_PI = 6.28318530718;
            bool in_rotation_range = target_position >= -TWO_PI && target_position <= TWO_PI;
            bool in_linear_range = target_position >= -10.0 && target_position <= 10.0;
            bool velocity_valid = target_velocity >= 0.0 && target_velocity <= max_velocity;
            return (in_rotation_range || in_linear_range) && velocity_valid;
        }

        // 속도 모드: 0 ~ max_velocity
        if (control_mode == ControlMode::VELOCITY) {
            return target_velocity >= 0.0 && target_velocity <= max_velocity;
        }

        // 토크 모드: 0 ~ max_torque
        if (control_mode == ControlMode::TORQUE) {
            return target_torque >= 0.0 && target_torque <= max_torque;
        }

        return false;
    }
};

} // namespace ethercat
} // namespace mxrc
