#pragma once

#include "../interfaces/IMotorCommandManager.h"
#include "../interfaces/IEtherCATMaster.h"
#include "../interfaces/ISlaveConfig.h"
#include <memory>

namespace mxrc {
namespace ethercat {

// 모터 명령 관리자 구현
// BLDC/Servo 모터 명령을 EtherCAT PDO로 변환하여 전송
class MotorCommandManager : public IMotorCommandManager {
public:
    MotorCommandManager(std::shared_ptr<IEtherCATMaster> master,
                        std::shared_ptr<ISlaveConfig> config);

    ~MotorCommandManager() override = default;

    // IMotorCommandManager 인터페이스 구현
    int writeBLDCCommand(const BLDCMotorCommand& command) override;
    int writeServoCommand(const ServoDriverCommand& command) override;

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

    // 헬퍼: Control Word 작성
    int writeControlWord(uint16_t slave_id, uint16_t control_word);

    // 헬퍼: PDO 매핑 찾기
    int findPDOOffset(uint16_t slave_id, uint16_t index, uint8_t subindex,
                       uint32_t& out_offset, PDODataType& out_type);
};

} // namespace ethercat
} // namespace mxrc
