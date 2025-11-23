#pragma once

#include "../dto/MotorCommand.h"

namespace mxrc {
namespace ethercat {

// 모터 명령 관리 인터페이스
// 테스트 가능성을 위한 추상화
class IMotorCommandManager {
public:
    virtual ~IMotorCommandManager() = default;

    // BLDC 모터 명령 전송
    virtual int writeBLDCCommand(const BLDCMotorCommand& command) = 0;

    // 서보 드라이버 명령 전송
    virtual int writeServoCommand(const ServoDriverCommand& command) = 0;
};

} // namespace ethercat
} // namespace mxrc
