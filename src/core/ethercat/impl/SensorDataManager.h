#pragma once

#include "../interfaces/ISensorDataManager.h"
#include "../interfaces/IEtherCATMaster.h"
#include "../interfaces/ISlaveConfig.h"
#include <memory>

namespace mxrc {
namespace ethercat {

// 센서 데이터 읽기 관리자
// PDO domain에서 센서 데이터를 추출하여 구조화된 형태로 변환
class SensorDataManager : public ISensorDataManager {
public:
    // 생성자: EtherCAT Master와 Slave 설정 의존성 주입
    SensorDataManager(
        std::shared_ptr<IEtherCATMaster> master,
        std::shared_ptr<ISlaveConfig> config);

    ~SensorDataManager() override = default;

    // ISensorDataManager 인터페이스 구현
    int readPositionSensor(uint16_t slave_id, PositionSensorData& data) override;
    int readVelocitySensor(uint16_t slave_id, VelocitySensorData& data) override;
    int readTorqueSensor(uint16_t slave_id, TorqueSensorData& data) override;
    int readDigitalInput(uint16_t slave_id, uint8_t channel, DigitalInputData& data) override;
    int readAnalogInput(uint16_t slave_id, uint8_t channel, AnalogInputData& data) override;
    int writeDigitalOutput(uint16_t slave_id, uint8_t channel, const DigitalOutputData& data) override;
    int writeAnalogOutput(uint16_t slave_id, uint8_t channel, const AnalogOutputData& data) override;

    // PDO domain 포인터 설정 (테스트용)
    void setDomainPtr(uint8_t* domain_ptr) {
        domain_ptr_ = domain_ptr;
    }

private:
    // 의존성
    std::shared_ptr<IEtherCATMaster> master_;
    std::shared_ptr<ISlaveConfig> config_;

    // PDO domain 포인터
    uint8_t* domain_ptr_;

    // 헬퍼: PDO 매핑에서 오프셋 찾기
    int findPDOOffset(uint16_t slave_id, uint16_t index, uint8_t subindex, uint32_t& out_offset);
};

} // namespace ethercat
} // namespace mxrc
