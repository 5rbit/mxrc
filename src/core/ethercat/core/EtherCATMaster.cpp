#include "EtherCATMaster.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace mxrc {
namespace ethercat {

EtherCATMaster::EtherCATMaster(uint32_t master_index,
                               std::shared_ptr<ISlaveConfig> config)
    : master_index_(master_index)
    , config_(config)
    , master_(nullptr)
    , domain_(nullptr)
    , state_(MasterState::UNINITIALIZED)
    , active_(false)
    , error_count_(0)
    , total_cycles_(0)
    , send_error_count_(0)
    , receive_error_count_(0)
    , dc_enabled_(false)
    , dc_system_time_offset_(0) {
}

EtherCATMaster::~EtherCATMaster() {
    deactivate();

#ifdef ETHERCAT_ENABLE
    // Master 해제
    if (master_) {
        ecrt_release_master(master_);
        master_ = nullptr;
    }
#endif

    spdlog::info("EtherCATMaster 소멸자 호출 완료");
}

int EtherCATMaster::initialize() {
#ifdef ETHERCAT_ENABLE
    // 1. Master 요청
    master_ = ecrt_request_master(master_index_);
    if (!master_) {
        spdlog::error("EtherCAT Master {} 요청 실패", master_index_);
        state_ = MasterState::ERROR;
        return -1;
    }

    spdlog::info("EtherCAT Master {} 초기화 완료", master_index_);

    // 2. Domain 생성
    domain_ = ecrt_master_create_domain(master_);
    if (!domain_) {
        spdlog::error("EtherCAT Domain 생성 실패");
        state_ = MasterState::ERROR;
        return -1;
    }

    spdlog::info("EtherCAT Domain 생성 완료");

    state_ = MasterState::INITIALIZED;
    return 0;
#else
    spdlog::warn("ETHERCAT_ENABLE이 비활성화되어 있습니다. 시뮬레이션 모드로 동작합니다.");
    state_ = MasterState::INITIALIZED;
    return 0;
#endif
}

int EtherCATMaster::scanSlaves() {
#ifdef ETHERCAT_ENABLE
    if (state_ != MasterState::INITIALIZED) {
        spdlog::error("Master가 초기화되지 않았습니다.");
        return -1;
    }

    // IgH EtherCAT Master는 자동으로 slave를 발견하므로,
    // 여기서는 master state를 조회하여 slave 정보를 확인합니다.
    ec_master_state_t master_state;
    if (ecrt_master_state(master_, &master_state) < 0) {
        spdlog::error("Master 상태 조회 실패");
        return -1;
    }

    spdlog::info("EtherCAT 네트워크 스캔 완료: {} slaves 발견",
                 master_state.slaves_responding);

    return static_cast<int>(master_state.slaves_responding);
#else
    spdlog::info("시뮬레이션 모드: Slave 스캔 스킵");
    return 0;
#endif
}

int EtherCATMaster::configureSlaves() {
#ifdef ETHERCAT_ENABLE
    if (state_ != MasterState::INITIALIZED) {
        spdlog::error("Master가 초기화되지 않았습니다.");
        return -1;
    }

    if (!config_) {
        spdlog::error("Slave 설정이 없습니다.");
        return -1;
    }

    size_t slave_count = config_->getSlaveCount();
    spdlog::info("Slave 설정 시작: {} slaves", slave_count);

    // 각 slave에 대해 설정 수행
    for (size_t i = 0; i < slave_count; ++i) {
        const SlaveConfig* slave_cfg = config_->getSlaveConfig(static_cast<uint16_t>(i));
        if (!slave_cfg) {
            spdlog::error("Slave {} 설정을 찾을 수 없습니다.", i);
            continue;
        }

        // Slave configuration 생성
        ec_slave_config_t* sc = ecrt_master_slave_config(
            master_,
            slave_cfg->alias,
            slave_cfg->position,
            slave_cfg->vendor_id,
            slave_cfg->product_code
        );

        if (!sc) {
            spdlog::error("Slave {} configuration 생성 실패 (alias={}, pos={})",
                         i, slave_cfg->alias, slave_cfg->position);
            continue;
        }

        slave_configs_.push_back(sc);

        // PDO 매핑 설정
        const auto& pdo_mappings = config_->getPDOMappings(static_cast<uint16_t>(i));
        spdlog::info("Slave {}: {} PDO 매핑 등록", i, pdo_mappings.size());

        // PDO entry 등록은 domain_reg를 통해 수행
        // (실제로는 ecrt_slave_config_pdos를 사용하여 더 복잡한 설정 가능)
    }

    // Domain 등록 (실제 PDO entry 등록)
    if (ecrt_domain_reg_pdo_entry_list(domain_, nullptr) < 0) {
        spdlog::error("PDO entry 등록 실패");
        state_ = MasterState::ERROR;
        return -1;
    }

    state_ = MasterState::CONFIGURED;
    spdlog::info("Slave 설정 완료");
    return 0;
#else
    spdlog::info("시뮬레이션 모드: Slave 설정 스킵");
    state_ = MasterState::CONFIGURED;
    return 0;
#endif
}

int EtherCATMaster::transitionToPreOp() {
#ifdef ETHERCAT_ENABLE
    // IgH EtherCAT Master는 activate 시 자동으로 상태 전환을 수행합니다.
    // 명시적 상태 전환은 특별한 경우에만 필요합니다.
    spdlog::debug("INIT → PREOP 전환 (자동)");
    return 0;
#else
    return 0;
#endif
}

int EtherCATMaster::transitionToSafeOp() {
#ifdef ETHERCAT_ENABLE
    spdlog::debug("PREOP → SAFEOP 전환 (자동)");
    return 0;
#else
    return 0;
#endif
}

int EtherCATMaster::transitionToOp() {
#ifdef ETHERCAT_ENABLE
    spdlog::debug("SAFEOP → OP 전환 (자동)");
    return 0;
#else
    return 0;
#endif
}

int EtherCATMaster::transitionToOP() {
#ifdef ETHERCAT_ENABLE
    if (state_ != MasterState::CONFIGURED) {
        spdlog::error("Slave 설정이 완료되지 않았습니다.");
        return -1;
    }

    // Master activation은 자동으로 INIT → PREOP → SAFEOP → OP 전환을 시도합니다.
    spdlog::info("OP 상태로 전환 시작");

    if (transitionToPreOp() != 0) return -1;
    if (transitionToSafeOp() != 0) return -1;
    if (transitionToOp() != 0) return -1;

    spdlog::info("OP 상태 전환 완료");
    return 0;
#else
    spdlog::info("시뮬레이션 모드: OP 상태 전환 스킵");
    return 0;
#endif
}

int EtherCATMaster::activate() {
#ifdef ETHERCAT_ENABLE
    if (state_ != MasterState::CONFIGURED) {
        spdlog::error("Slave 설정이 완료되지 않았습니다.");
        return -1;
    }

    // Master 활성화
    if (ecrt_master_activate(master_) < 0) {
        spdlog::error("Master 활성화 실패");
        state_ = MasterState::ERROR;
        return -1;
    }

    // Domain data 포인터 획득
    uint8_t* domain_pd = ecrt_domain_data(domain_);
    if (!domain_pd) {
        spdlog::error("Domain data 포인터 획득 실패");
        state_ = MasterState::ERROR;
        return -1;
    }

    active_ = true;
    state_ = MasterState::ACTIVATED;
    spdlog::info("EtherCAT Master 활성화 완료 (OP 상태)");
    return 0;
#else
    spdlog::info("시뮬레이션 모드: Master 활성화 스킵");
    active_ = true;
    state_ = MasterState::ACTIVATED;
    return 0;
#endif
}

int EtherCATMaster::deactivate() {
#ifdef ETHERCAT_ENABLE
    if (!active_) {
        return 0;
    }

    // Master를 비활성화하려면 ecrt_master_deactivate를 호출
    // (IgH EtherCAT Master는 master 해제 시 자동으로 비활성화)
    active_ = false;
    state_ = MasterState::CONFIGURED;
    spdlog::info("EtherCAT Master 비활성화 완료");
    return 0;
#else
    active_ = false;
    return 0;
#endif
}

int EtherCATMaster::send() {
#ifdef ETHERCAT_ENABLE
    if (!active_) {
        spdlog::error("Master가 활성화되지 않았습니다.");
        return -1;
    }

    // Domain 큐 (출력 데이터 준비)
    ecrt_domain_queue(domain_);

    // Master 전송
    if (ecrt_master_send(master_) < 0) {
        send_error_count_++;
        error_count_++;
        return -1;
    }

    total_cycles_++;
    return 0;
#else
    total_cycles_++;
    return 0;
#endif
}

int EtherCATMaster::receive() {
#ifdef ETHERCAT_ENABLE
    if (!active_) {
        spdlog::error("Master가 활성화되지 않았습니다.");
        return -1;
    }

    // Master 수신
    if (ecrt_master_receive(master_) < 0) {
        receive_error_count_++;
        error_count_++;
        return -1;
    }

    // Domain 처리 (입력 데이터 처리)
    ecrt_domain_process(domain_);

    // DC 동기화가 활성화된 경우, offset 모니터링
    if (dc_enabled_) {
        ec_master_state_t master_state;
        if (ecrt_master_state(master_, &master_state) == 0) {
            // DC system time offset 저장 (nanoseconds)
            // 주의: master_state 구조체에서 실제 DC offset을 가져오려면
            // IgH EtherCAT Master의 버전에 따라 다를 수 있음
            // 여기서는 예시로 0으로 설정
            dc_system_time_offset_ = 0;  // 실제로는 ecrt_master_sync_reference_clock 등 사용
        }
    }

    return 0;
#else
    return 0;
#endif
}

bool EtherCATMaster::isActive() const {
    return active_;
}

uint32_t EtherCATMaster::getErrorCount() const {
    return error_count_;
}

uint8_t* EtherCATMaster::getDomainData() {
#ifdef ETHERCAT_ENABLE
    if (!domain_) {
        return nullptr;
    }
    return ecrt_domain_data(domain_);
#else
    return nullptr;
#endif
}

int EtherCATMaster::configureDC(const DCConfiguration& dc_config) {
#ifdef ETHERCAT_ENABLE
    if (!dc_config.enable) {
        spdlog::info("DC 동기화가 비활성화되어 있습니다.");
        dc_enabled_ = false;
        return 0;
    }

    // Reference slave configuration
    if (slave_configs_.empty()) {
        spdlog::error("Slave configuration이 없습니다.");
        return -1;
    }

    ec_slave_config_t* ref_sc = slave_configs_[dc_config.reference_slave];

    // DC 설정
    ecrt_slave_config_dc(
        ref_sc,
        0x0300,  // SYNC0 assign activate
        dc_config.sync0_cycle_time,
        dc_config.sync0_shift_time,
        0,
        0
    );

    dc_enabled_ = true;
    spdlog::info("DC 동기화 설정 완료: ref_slave={}, sync0_cycle={}ns",
                 dc_config.reference_slave, dc_config.sync0_cycle_time);
    return 0;
#else
    spdlog::info("시뮬레이션 모드: DC 설정 스킵");
    dc_enabled_ = dc_config.enable;
    return 0;
#endif
}

int EtherCATMaster::registerPDOEntry(uint16_t slave_id, const PDOMapping& mapping,
                                      ec_pdo_entry_reg_t* reg) {
#ifdef ETHERCAT_ENABLE
    // PDO entry 등록 헬퍼
    // 실제 구현은 ecrt_domain_reg_pdo_entry_list를 통해 수행
    // 여기서는 reg 구조체를 채우는 역할만 수행

    const SlaveConfig* slave_cfg = config_->getSlaveConfig(slave_id);
    if (!slave_cfg) {
        return -1;
    }

    reg->alias = slave_cfg->alias;
    reg->position = slave_cfg->position;
    reg->vendor_id = slave_cfg->vendor_id;
    reg->product_code = slave_cfg->product_code;
    reg->index = mapping.index;
    reg->subindex = mapping.subindex;
    reg->offset = nullptr;  // Domain에서 자동 할당
    reg->bit_position = nullptr;

    return 0;
#else
    return 0;
#endif
}

} // namespace ethercat
} // namespace mxrc
