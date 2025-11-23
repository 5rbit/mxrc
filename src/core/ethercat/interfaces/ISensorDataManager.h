#pragma once

#include "../dto/SensorData.h"

namespace mxrc {
namespace ethercat {

// 센서 데이터 관리 인터페이스
// 테스트 가능성을 위한 추상화
class ISensorDataManager {
public:
    virtual ~ISensorDataManager() = default;

    // 위치 센서 데이터 읽기
    virtual int readPositionSensor(uint16_t slave_id, PositionSensorData& out_data) = 0;

    // 속도 센서 데이터 읽기
    virtual int readVelocitySensor(uint16_t slave_id, VelocitySensorData& out_data) = 0;

    // 토크 센서 데이터 읽기
    virtual int readTorqueSensor(uint16_t slave_id, TorqueSensorData& out_data) = 0;

    // Digital Input 읽기
    virtual int readDigitalInput(uint16_t slave_id, uint8_t channel, DigitalInputData& out_data) = 0;

    // Analog Input 읽기
    virtual int readAnalogInput(uint16_t slave_id, uint8_t channel, AnalogInputData& out_data) = 0;

    // Digital Output 쓰기
    virtual int writeDigitalOutput(uint16_t slave_id, uint8_t channel, const DigitalOutputData& data) = 0;

    // Analog Output 쓰기
    virtual int writeAnalogOutput(uint16_t slave_id, uint8_t channel, const AnalogOutputData& data) = 0;
};

} // namespace ethercat
} // namespace mxrc
