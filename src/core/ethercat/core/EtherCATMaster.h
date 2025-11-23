#pragma once

#include "../interfaces/IEtherCATMaster.h"
#include "../interfaces/ISlaveConfig.h"
#include "../dto/SlaveConfig.h"
#include "../dto/PDOMapping.h"
#include "../dto/DCConfiguration.h"
#include <memory>
#include <vector>
#include <cstdint>

// IgH EtherCAT Master forward declarations
// 실제 환경에서만 ecrt.h를 include
#ifdef ETHERCAT_ENABLE
#include <ecrt.h>
#else
// 시뮬레이션 환경을 위한 타입 정의
typedef struct ec_master ec_master_t;
typedef struct ec_slave_config ec_slave_config_t;
typedef struct ec_domain ec_domain_t;
typedef struct ec_pdo_entry_reg ec_pdo_entry_reg_t;
#endif

namespace mxrc {
namespace ethercat {

// EtherCAT Master 상태
enum class MasterState {
    UNINITIALIZED,  // 초기화 전
    INITIALIZED,    // Master 초기화 완료
    CONFIGURED,     // Slave 설정 완료
    ACTIVATED,      // Master 활성화 완료 (OP 상태)
    ERROR           // 에러 상태
};

// IgH EtherCAT Master RAII wrapper
// 프로덕션 환경에서 실제 EtherCAT 하드웨어 제어
class EtherCATMaster : public IEtherCATMaster {
public:
    // 생성자
    // master_index: EtherCAT master 인덱스 (기본값 0)
    // config: Slave 설정 (YAML에서 로드)
    explicit EtherCATMaster(uint32_t master_index = 0,
                           std::shared_ptr<ISlaveConfig> config = nullptr);

    ~EtherCATMaster() override;

    // IEtherCATMaster 인터페이스 구현
    int initialize() override;
    int activate() override;
    int deactivate() override;
    int send() override;
    int receive() override;
    bool isActive() const override;
    uint32_t getErrorCount() const override;

    // User Story 3 추가 기능

    // 네트워크 스캔 및 슬레이브 발견
    // 반환: 발견된 slave 개수, 실패 시 -1
    int scanSlaves();

    // Slave 설정 (PDO 매핑)
    // YAML 설정 기반으로 각 slave의 PDO 매핑 수행
    // 반환: 성공 0, 실패 -1
    int configureSlaves();

    // OP 상태로 전환 (INIT → PREOP → SAFEOP → OP)
    // 반환: 성공 0, 실패 -1
    int transitionToOP();

    // DC (Distributed Clock) 설정
    // dc_config: DC 설정 (YAML에서 로드)
    // 반환: 성공 0, 실패 -1
    int configureDC(const DCConfiguration& dc_config);

    // 현재 Master 상태 조회
    MasterState getState() const { return state_; }

    // PDO domain 포인터 조회 (SensorDataManager, MotorCommandManager용)
    uint8_t* getDomainData();

    // 통계 조회
    uint64_t getTotalCycles() const { return total_cycles_; }
    uint64_t getSendErrorCount() const { return send_error_count_; }
    uint64_t getReceiveErrorCount() const { return receive_error_count_; }

    // DC (Distributed Clock) 통계 조회
    int32_t getDCSystemTimeOffset() const { return dc_system_time_offset_; }
    bool isDCEnabled() const { return dc_enabled_; }

private:
    // 설정
    uint32_t master_index_;
    std::shared_ptr<ISlaveConfig> config_;

    // EtherCAT Master 핸들 (IgH)
    ec_master_t* master_;
    ec_domain_t* domain_;

    // Slave 설정 핸들들
    std::vector<ec_slave_config_t*> slave_configs_;

    // 상태
    MasterState state_;
    bool active_;

    // 통계
    uint32_t error_count_;
    uint64_t total_cycles_;
    uint64_t send_error_count_;
    uint64_t receive_error_count_;

    // DC 통계
    bool dc_enabled_;
    int32_t dc_system_time_offset_;  // DC system time offset (nanoseconds)

    // 헬퍼: 상태 전환 (INIT → PREOP)
    int transitionToPreOp();

    // 헬퍼: 상태 전환 (PREOP → SAFEOP)
    int transitionToSafeOp();

    // 헬퍼: 상태 전환 (SAFEOP → OP)
    int transitionToOp();

    // 헬퍼: PDO entry 등록
    int registerPDOEntry(uint16_t slave_id, const PDOMapping& mapping,
                         ec_pdo_entry_reg_t* reg);
};

} // namespace ethercat
} // namespace mxrc
