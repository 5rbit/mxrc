#pragma once

#include <cstdint>
#include <memory>

#ifdef ETHERCAT_ENABLE
#include <ecrt.h>
#else
typedef struct ec_domain ec_domain_t;
#endif

namespace mxrc {
namespace ethercat {

// EtherCAT Domain RAII wrapper
// PDO 데이터 메모리를 관리하고, process/queue 작업 수행
class EtherCATDomain {
public:
    // 생성자
    // domain: IgH EtherCAT domain 핸들
    explicit EtherCATDomain(ec_domain_t* domain);

    ~EtherCATDomain();

    // 복사/이동 금지 (RAII)
    EtherCATDomain(const EtherCATDomain&) = delete;
    EtherCATDomain& operator=(const EtherCATDomain&) = delete;
    EtherCATDomain(EtherCATDomain&&) = delete;
    EtherCATDomain& operator=(EtherCATDomain&&) = delete;

    // PDO domain data 포인터 조회
    uint8_t* getData();

    // Domain process (입력 데이터 처리)
    void process();

    // Domain queue (출력 데이터 준비)
    void queue();

    // Domain 상태 조회
    int getState();

private:
    ec_domain_t* domain_;
    uint8_t* domain_data_;
};

} // namespace ethercat
} // namespace mxrc
