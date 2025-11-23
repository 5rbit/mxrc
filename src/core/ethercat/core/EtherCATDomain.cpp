#include "EtherCATDomain.h"
#include <spdlog/spdlog.h>

namespace mxrc {
namespace ethercat {

EtherCATDomain::EtherCATDomain(ec_domain_t* domain)
    : domain_(domain)
    , domain_data_(nullptr) {
#ifdef ETHERCAT_ENABLE
    if (domain_) {
        domain_data_ = ecrt_domain_data(domain_);
    }
#endif
}

EtherCATDomain::~EtherCATDomain() {
    // Domain은 Master에 의해 관리되므로 명시적 해제 불필요
    spdlog::debug("EtherCATDomain 소멸자 호출");
}

uint8_t* EtherCATDomain::getData() {
    return domain_data_;
}

void EtherCATDomain::process() {
#ifdef ETHERCAT_ENABLE
    if (domain_) {
        ecrt_domain_process(domain_);
    }
#endif
}

void EtherCATDomain::queue() {
#ifdef ETHERCAT_ENABLE
    if (domain_) {
        ecrt_domain_queue(domain_);
    }
#endif
}

int EtherCATDomain::getState() {
#ifdef ETHERCAT_ENABLE
    if (!domain_) {
        return -1;
    }

    ec_domain_state_t state;
    ecrt_domain_state(domain_, &state);

    // working_counter 확인
    return (state.working_counter > 0) ? 0 : -1;
#else
    return 0;
#endif
}

} // namespace ethercat
} // namespace mxrc
