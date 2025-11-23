#pragma once

#include <cstdint>
#include <string>

namespace mxrc {
namespace ethercat {

// 위치 센서 데이터 (엔코더)
struct PositionSensorData {
    int32_t position;       // 위치 값 (엔코더 카운트)
    int32_t velocity;       // 속도 (카운트/초)
    uint64_t timestamp;     // 읽은 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소

    PositionSensorData()
        : position(0), velocity(0), timestamp(0), valid(false), slave_id(0) {}
};

// 속도 센서 데이터 (타코미터)
struct VelocitySensorData {
    double velocity;        // 속도 (m/s 또는 rad/s)
    double acceleration;    // 가속도 (m/s² 또는 rad/s²)
    uint64_t timestamp;     // 읽은 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소
    std::string unit;       // 단위 ("m/s", "rad/s", "rpm" 등)

    VelocitySensorData()
        : velocity(0.0), acceleration(0.0), timestamp(0), valid(false), slave_id(0), unit("m/s") {}
};

// 토크/힘 센서 데이터 (6축)
struct TorqueSensorData {
    double force_x, force_y, force_z;       // 힘 X, Y, Z축 (N)
    double torque_x, torque_y, torque_z;    // 토크 X, Y, Z축 (Nm)
    uint64_t timestamp;     // 읽은 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소

    TorqueSensorData()
        : force_x(0.0), force_y(0.0), force_z(0.0)
        , torque_x(0.0), torque_y(0.0), torque_z(0.0)
        , timestamp(0), valid(false), slave_id(0) {}
};

// Digital Input 데이터
struct DigitalInputData {
    uint8_t channel;        // 채널 번호
    bool value;             // 디지털 값 (true = HIGH, false = LOW)
    uint64_t timestamp;     // 읽은 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소

    DigitalInputData()
        : channel(0), value(false), timestamp(0), valid(false), slave_id(0) {}
};

// Analog Input 데이터
struct AnalogInputData {
    uint8_t channel;        // 채널 번호
    double value;           // 아날로그 값 (단위에 따라)
    std::string unit;       // 단위 ("V", "mA", "C", "bar" 등)
    double min_value;       // 최소 유효 범위
    double max_value;       // 최대 유효 범위
    uint64_t timestamp;     // 읽은 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소

    AnalogInputData()
        : channel(0), value(0.0), unit("V"), min_value(-10.0), max_value(10.0)
        , timestamp(0), valid(false), slave_id(0) {}

    // 범위 내 값인지 확인
    bool isInRange() const {
        return value >= min_value && value <= max_value;
    }
};

// Digital Output 데이터
struct DigitalOutputData {
    uint8_t channel;        // 채널 번호
    bool value;             // 디지털 값 (true = HIGH, false = LOW)
    uint64_t timestamp;     // 쓴 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소

    DigitalOutputData()
        : channel(0), value(false), timestamp(0), valid(false), slave_id(0) {}
};

// Analog Output 데이터
struct AnalogOutputData {
    uint8_t channel;        // 채널 번호
    double value;           // 아날로그 값 (단위에 따라)
    std::string unit;       // 단위 ("V", "mA" 등)
    double min_value;       // 최소 출력 범위
    double max_value;       // 최대 출력 범위
    uint64_t timestamp;     // 쓴 시각 (nanoseconds)
    bool valid;             // 데이터 유효성
    uint16_t slave_id;      // EtherCAT slave 주소

    AnalogOutputData()
        : channel(0), value(0.0), unit("V"), min_value(-10.0), max_value(10.0)
        , timestamp(0), valid(false), slave_id(0) {}

    // 범위 내 값인지 확인
    bool isInRange() const {
        return value >= min_value && value <= max_value;
    }
};

} // namespace ethercat
} // namespace mxrc
