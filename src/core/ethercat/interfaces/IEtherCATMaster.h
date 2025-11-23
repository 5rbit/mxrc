#pragma once

#include <cstdint>

namespace mxrc {
namespace ethercat {

// EtherCAT Master 인터페이스
// 테스트 가능성을 위한 추상화 (의존성 주입)
class IEtherCATMaster {
public:
    virtual ~IEtherCATMaster() = default;

    // Master 초기화
    virtual int initialize() = 0;

    // Master 활성화 (OP 상태로 전환)
    virtual int activate() = 0;

    // Master 비활성화
    virtual int deactivate() = 0;

    // PDO 데이터 송신 (Master → Slave)
    virtual int send() = 0;

    // PDO 데이터 수신 (Slave → Master)
    virtual int receive() = 0;

    // Master 상태 확인
    virtual bool isActive() const = 0;

    // 에러 카운트 조회
    virtual uint32_t getErrorCount() const = 0;
};

} // namespace ethercat
} // namespace mxrc
