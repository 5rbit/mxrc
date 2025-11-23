# Technical Research: EtherCAT 센서/모터 데이터 수신 인프라

**Feature Branch**: `001-ethercat-integration`
**Created**: 2025-11-23
**Status**: Research Complete
**Last Updated**: 2025-11-23

---

## 개요

본 문서는 MXRC 프로젝트에 EtherCAT 기반 센서/모터 데이터 수신 인프라를 구현하기 위한 기술 조사 결과입니다. Linux PREEMPT_RT 환경에서 실시간 제어를 위한 최적의 기술 스택을 선정하고, MXRC의 기존 아키텍처(RT/Non-RT 분리, DataStore, EventBus)와의 통합 방안을 제시합니다.

---

## 1. EtherCAT Master 라이브러리 선택

### Decision: IgH EtherCAT Master (EtherLab)

IgH EtherCAT Master를 주 구현체로 선택하고, 개발/테스트 단계에서는 SOEM을 병행 지원합니다.

### Rationale

**IgH EtherCAT Master 선택 이유**:

1. **Real-Time 성능**:
   - Kernel-space 구현으로 낮은 latency와 높은 determinism 제공
   - PREEMPT_RT 커널과의 완벽한 호환성
   - 프로덕션 환경에서 검증된 안정성

2. **하드웨어 타이밍 요구사항 충족**:
   - MXRC의 1ms minor cycle 요구사항을 만족 (sensor_read, motor_control actions)
   - RT Executive와의 긴밀한 통합 가능 (RTExecutive::registerAction)

3. **DC 동기화 지원**:
   - Distributed Clock 동기화로 ±1μs 이내 정밀도 달성
   - 멀티 축 모터 제어 시 jitter 최소화 (User Story 4 요구사항)

4. **프로덕션 실적**:
   - 산업용 로봇, CNC, 반도체 장비 등에서 광범위하게 사용됨
   - 24시간 연속 운영 환경에서 검증됨

### Alternatives Considered

#### SOEM (Simple Open EtherCAT Master)

**장점**:
- User-space 구현으로 개발/디버깅이 용이
- 크로스 플랫폼 지원 (Windows, Linux, macOS)
- GPLv2 예외 조항으로 상용 제품 통합 시 소스 공개 불필요
- 간단한 API로 초기 프로토타입 제작에 유리

**단점**:
- User-space 동작으로 인한 추가 latency (context switching 오버헤드)
- 1ms cycle time 환경에서 jitter가 IgH 대비 높음 (실시간성 저하)
- Kernel-space 대비 성능 예측성이 낮음

**결정**: 개발 초기 단계 및 시뮬레이션 환경에서 SOEM을 지원하되, 프로덕션 배포는 IgH로 진행

#### acontis EC-Master (Commercial)

**장점**:
- 상업적 지원 및 기술 컨설팅 제공
- 풍부한 문서 및 예제 코드
- GUI 기반 진단 도구 제공

**단점**:
- 고가의 라이선스 비용 (SDK + runtime license)
- GPL 라이선스 기반 오픈소스 프로젝트와 충돌 가능성
- Vendor lock-in 리스크

**결정**: 현 시점에서는 오픈소스 솔루션으로 충분하며, 향후 상용 지원 필요 시 재검토

### Integration Notes

#### MXRC 아키텍처 통합

1. **RT Executive 통합**:
   ```cpp
   // RTExecutive에 EtherCAT cyclic action 등록
   rt_executive->registerAction("ethercat_cycle", 1ms,
       [&](RTContext& ctx) {
           // EtherCAT PDO 읽기/쓰기
           ecrt_master_receive(master);
           ecrt_domain_process(domain);

           // 센서 데이터 → RTDataStore
           updateRTDataStore(ctx.data_store);

           // 모터 명령 ← RTDataStore
           updateMotorCommands(ctx.data_store);

           ecrt_domain_queue(domain);
           ecrt_master_send(master);
       });
   ```

2. **DataStore 통합**:
   - EtherCAT PDO 데이터를 `RTDataStore`의 고정 키로 매핑
   - Versioned data로 저장하여 Non-RT 시스템이 안전하게 접근
   - 기존 `DataKey` enum 확장:
     ```cpp
     enum class DataKey : uint16_t {
         // 기존 키들...
         ROBOT_X = 0,

         // EtherCAT 센서 키들
         ETHERCAT_ENCODER_POS_1 = 100,
         ETHERCAT_FORCE_SENSOR_FX = 120,
         ETHERCAT_TEMP_SENSOR_1 = 140,

         // EtherCAT 모터 명령 키들
         ETHERCAT_MOTOR_CMD_VEL_1 = 200,
         ETHERCAT_MOTOR_CMD_TORQUE_1 = 220,

         MAX_KEYS = 512
     };
     ```

3. **에러 처리**:
   - EtherCAT 통신 에러 시 RTStateMachine을 통해 SAFE_MODE 전환
   - EventBus를 통해 Non-RT 시스템에 에러 이벤트 발행
   ```cpp
   if (ecrt_master_state(master, &master_state) < 0 ||
       master_state.link_up == 0) {
       // EventBus에 에러 이벤트 발행
       event_bus->publish(Event{
           .type = EventType::ERROR,
           .source = "ethercat",
           .message = "Link down detected"
       });

       // RTStateMachine을 SAFE_MODE로 전환
       state_machine->transitionTo(RTState::SAFE_MODE);
   }
   ```

#### 시스템 구성

1. **초기화 시퀀스** (RT Executive 시작 전):
   ```
   1. EtherCAT master 초기화 (ecrt_request_master)
   2. Domain 생성 및 PDO 매핑
   3. Slave configuration (YAML 파일 기반)
   4. Master activation (ecrt_master_activate)
   5. INIT → PREOP → SAFEOP → OP 상태 전환
   6. DC 동기화 설정 (선택사항, P3 priority)
   7. RT Executive 시작
   ```

2. **Cyclic 데이터 교환** (1ms minor cycle):
   ```
   Cycle Start (RTExecutive::run loop)
     ↓
   ecrt_master_receive() - 네트워크로부터 PDO 수신
     ↓
   ecrt_domain_process() - PDO 데이터 처리
     ↓
   센서 데이터 읽기 → RTDataStore 업데이트
     ↓
   제어 알고리즘 실행 (다른 RT action)
     ↓
   RTDataStore로부터 모터 명령 읽기 → PDO 설정
     ↓
   ecrt_domain_queue() - PDO 데이터 준비
     ↓
   ecrt_master_send() - 네트워크로 PDO 전송
     ↓
   Cycle End (wait until next cycle)
   ```

#### 커널 모듈 설치

```bash
# IgH EtherCAT Master 설치
cd /usr/src
hg clone http://hg.code.sf.net/p/etherlabmaster/code etherlab
cd etherlab
./bootstrap
./configure --enable-cycles --enable-hrtimer --disable-8139too
make
sudo make install

# 커널 모듈 로드
sudo modprobe ec_master
sudo modprobe ec_generic

# 네트워크 인터페이스 할당 (예: eth1을 EtherCAT 전용으로)
echo "MASTER0_DEVICE=$(ethtool -i eth1 | grep bus-info | awk '{print $2}')" | \
    sudo tee -a /etc/sysconfig/ethercat
sudo systemctl enable ethercat
sudo systemctl start ethercat
```

#### 라이선스 고려사항

- **IgH EtherCAT Master**: GPLv2 라이선스
- **영향**: 커널 모듈을 수정하면 소스 공개 의무 발생
- **MXRC 통합**:
  - User-space 애플리케이션(mxrc-rt)에서 IgH 라이브러리를 사용하는 것은 문제 없음
  - 커널 모듈 자체를 수정하지 않는 한 MXRC 코드는 GPL 영향 받지 않음
  - 상용 제품 배포 시 IgH 커널 모듈은 바이너리로 배포 가능 (GPL 준수 하에)

---

## 2. YAML 파싱 라이브러리

### Decision: yaml-cpp

yaml-cpp를 EtherCAT slave configuration 파싱에 사용합니다.

### Rationale

1. **성숙도 및 안정성**:
   - C++11/14/17/20 지원으로 MXRC의 C++20 표준과 호환
   - GitHub star 4.8k+, 10년 이상의 프로덕션 검증
   - 활발한 유지보수 (2024년까지 정기 릴리즈)

2. **API 직관성**:
   ```cpp
   // yaml-cpp 사용 예제
   YAML::Node config = YAML::LoadFile("ethercat_slaves.yaml");
   for (const auto& slave : config["slaves"]) {
       uint16_t alias = slave["alias"].as<uint16_t>();
       uint16_t position = slave["position"].as<uint16_t>();
       uint32_t vendor_id = slave["vendor_id"].as<uint32_t>();
       uint32_t product_code = slave["product_code"].as<uint32_t>();

       // PDO 매핑 파싱
       for (const auto& pdo : slave["pdos"]) {
           // ...
       }
   }
   ```

3. **nlohmann::json 호환성**:
   - MXRC에서 이미 사용 중인 `nlohmann::json`과 유사한 API 디자인
   - 기존 JSON 설정 파일(rt_schedule.json 등)과 일관된 파싱 패턴
   - 필요 시 YAML ↔ JSON 변환 가능

4. **성능 및 메모리**:
   - 초기화 시 한 번만 파싱하므로 성능이 RT 경로에 영향 없음
   - 메모리 사용량: 중소형 설정 파일(< 100KB)에서 수 MB 이내
   - MXRC의 비 RT 초기화 단계에서 수행되므로 메모리 제약 없음

### Alternatives Considered

#### RapidYAML

**장점**:
- C++ 성능에 최적화된 파서 (yaml-cpp 대비 5-10배 빠름)
- 메모리 할당 최소화
- Header-only 옵션 제공

**단점**:
- 비교적 신생 라이브러리 (성숙도 낮음)
- 커뮤니티 및 문서가 yaml-cpp 대비 부족
- API가 저수준이며 사용하기 복잡함

**결정**: 초기화 시에만 사용하므로 성능보다 안정성과 사용 편의성 우선

#### libYAML (C library)

**장점**:
- 가장 빠른 성능 (C 구현)
- 표준 YAML 1.1 스펙 완전 준수

**단점**:
- C API로 C++ 프로젝트에서 사용 불편
- 메모리 관리 수동 처리 필요
- 타입 안전성 부족

**결정**: C++ 중심 프로젝트에 부적합

#### JSON 사용 (YAML 대신)

**장점**:
- 이미 nlohmann::json 라이브러리 사용 중
- 추가 의존성 없음

**단점**:
- JSON은 주석 미지원 (설정 파일 문서화 어려움)
- 복잡한 계층 구조 표현 시 가독성 저하
- EtherCAT 설정 파일의 표준이 YAML 기반 (ESI 파일 변환 시)

**결정**: YAML의 가독성과 주석 지원이 복잡한 EtherCAT 설정에 유리

### Integration Notes

#### 설정 파일 구조

`config/ethercat_slaves.yaml`:
```yaml
# EtherCAT Network Configuration
network:
  cycle_time_us: 1000  # 1ms cycle time
  master_index: 0

# Slave Device Configuration
slaves:
  # Slave 1: Position Encoder
  - alias: 0
    position: 0
    vendor_id: 0x00000002
    product_code: 0x044c2c52
    name: "encoder_joint_1"
    type: "encoder"

    # PDO Mapping (Input)
    rx_pdos:
      - index: 0x1A00
        entries:
          - index: 0x6064  # Position actual value
            subindex: 0x00
            bit_length: 32
            datatype: "int32"
            datastore_key: "ETHERCAT_ENCODER_POS_1"

          - index: 0x606C  # Velocity actual value
            subindex: 0x00
            bit_length: 32
            datatype: "int32"
            datastore_key: "ETHERCAT_ENCODER_VEL_1"

    # PDO Mapping (Output)
    tx_pdos:
      - index: 0x1600
        entries:
          - index: 0x607A  # Target position
            subindex: 0x00
            bit_length: 32
            datatype: "int32"
            datastore_key: "ETHERCAT_MOTOR_CMD_POS_1"

    # CoE Configuration (SDO)
    sdo_config:
      - index: 0x1C12  # RxPDO assign
        subindex: 0x00
        value: 0x0001
      - index: 0x1C13  # TxPDO assign
        subindex: 0x00
        value: 0x0001

  # Slave 2: Force/Torque Sensor
  - alias: 0
    position: 1
    vendor_id: 0x00000002
    product_code: 0x12345678
    name: "force_sensor_1"
    type: "force_sensor"

    rx_pdos:
      - index: 0x1A01
        entries:
          - index: 0x2001  # Force X
            subindex: 0x01
            bit_length: 32
            datatype: "float"
            datastore_key: "ETHERCAT_FORCE_SENSOR_FX"
          - index: 0x2001  # Force Y
            subindex: 0x02
            bit_length: 32
            datatype: "float"
            datastore_key: "ETHERCAT_FORCE_SENSOR_FY"
          - index: 0x2001  # Force Z
            subindex: 0x03
            bit_length: 32
            datatype: "float"
            datastore_key: "ETHERCAT_FORCE_SENSOR_FZ"

# Distributed Clock Configuration (P3 priority)
distributed_clock:
  enabled: true
  sync0_cycle_time_ns: 1000000  # 1ms
  sync0_shift_ns: 0
  reference_clock: 0  # 첫 번째 DC-capable slave
```

#### 파싱 코드 통합

```cpp
// src/core/ethercat/EtherCATConfig.h
#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

namespace mxrc::core::ethercat {

struct PDOEntry {
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    std::string datatype;  // "int32", "float", etc.
    std::string datastore_key;  // RTDataStore key name
};

struct PDO {
    uint16_t index;
    std::vector<PDOEntry> entries;
};

struct SDOConfig {
    uint16_t index;
    uint8_t subindex;
    uint32_t value;
};

struct SlaveConfig {
    uint16_t alias;
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    std::string name;
    std::string type;

    std::vector<PDO> rx_pdos;  // Input PDOs (Slave → Master)
    std::vector<PDO> tx_pdos;  // Output PDOs (Master → Slave)
    std::vector<SDOConfig> sdo_config;
};

struct NetworkConfig {
    uint32_t cycle_time_us;
    uint32_t master_index;
};

struct DCConfig {
    bool enabled;
    uint32_t sync0_cycle_time_ns;
    int32_t sync0_shift_ns;
    uint16_t reference_clock;
};

struct EtherCATConfig {
    NetworkConfig network;
    std::vector<SlaveConfig> slaves;
    DCConfig distributed_clock;

    // YAML 파일로부터 로드
    static EtherCATConfig loadFromFile(const std::string& filepath);
};

} // namespace mxrc::core::ethercat
```

```cpp
// src/core/ethercat/EtherCATConfig.cpp
#include "EtherCATConfig.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::ethercat {

EtherCATConfig EtherCATConfig::loadFromFile(const std::string& filepath) {
    EtherCATConfig config;

    try {
        YAML::Node root = YAML::LoadFile(filepath);

        // Network config
        auto network = root["network"];
        config.network.cycle_time_us = network["cycle_time_us"].as<uint32_t>();
        config.network.master_index = network["master_index"].as<uint32_t>();

        // Slaves
        for (const auto& slave_node : root["slaves"]) {
            SlaveConfig slave;
            slave.alias = slave_node["alias"].as<uint16_t>();
            slave.position = slave_node["position"].as<uint16_t>();
            slave.vendor_id = slave_node["vendor_id"].as<uint32_t>();
            slave.product_code = slave_node["product_code"].as<uint32_t>();
            slave.name = slave_node["name"].as<std::string>();
            slave.type = slave_node["type"].as<std::string>();

            // RxPDOs (Input)
            if (slave_node["rx_pdos"]) {
                for (const auto& pdo_node : slave_node["rx_pdos"]) {
                    PDO pdo;
                    pdo.index = pdo_node["index"].as<uint16_t>();

                    for (const auto& entry_node : pdo_node["entries"]) {
                        PDOEntry entry;
                        entry.index = entry_node["index"].as<uint16_t>();
                        entry.subindex = entry_node["subindex"].as<uint8_t>();
                        entry.bit_length = entry_node["bit_length"].as<uint8_t>();
                        entry.datatype = entry_node["datatype"].as<std::string>();
                        entry.datastore_key = entry_node["datastore_key"].as<std::string>();
                        pdo.entries.push_back(entry);
                    }

                    slave.rx_pdos.push_back(pdo);
                }
            }

            // TxPDOs (Output) - 동일한 방식
            // SDO Config - 동일한 방식

            config.slaves.push_back(slave);
        }

        // Distributed Clock (optional)
        if (root["distributed_clock"]) {
            auto dc = root["distributed_clock"];
            config.distributed_clock.enabled = dc["enabled"].as<bool>();
            config.distributed_clock.sync0_cycle_time_ns = dc["sync0_cycle_time_ns"].as<uint32_t>();
            config.distributed_clock.sync0_shift_ns = dc["sync0_shift_ns"].as<int32_t>();
            config.distributed_clock.reference_clock = dc["reference_clock"].as<uint16_t>();
        }

        spdlog::info("Loaded EtherCAT config: {} slaves", config.slaves.size());

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML parsing error: {}", e.what());
        throw;
    }

    return config;
}

} // namespace mxrc::core::ethercat
```

#### CMakeLists.txt 추가

```cmake
# YAML-cpp dependency
find_package(yaml-cpp REQUIRED)

# EtherCAT 관련 소스 추가
add_executable(rt
    # ... 기존 소스들 ...
    src/core/ethercat/EtherCATConfig.cpp
    src/core/ethercat/EtherCATMaster.cpp
)

target_link_libraries(rt PRIVATE
    # ... 기존 라이브러리들 ...
    yaml-cpp
    ethercat  # IgH EtherCAT master library
)
```

#### 설치

```bash
# Ubuntu/Debian
sudo apt install libyaml-cpp-dev

# 또는 소스 빌드
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build && cd build
cmake -DYAML_BUILD_SHARED_LIBS=ON ..
make
sudo make install
```

---

## 3. EtherCAT PDO 매핑 방법

### Decision: 정적 PDO 매핑 (Static Mapping) 우선, 동적 매핑은 선택적 확장

초기 구현은 정적 PDO 매핑으로 진행하고, 향후 필요 시 동적 매핑 지원을 추가합니다.

### Rationale

1. **실시간 시스템에서의 예측 가능성**:
   - 정적 매핑은 컴파일/초기화 시점에 메모리 레이아웃 확정
   - RT cycle 중 PDO 구조 변경 없음 → 일정한 latency 보장
   - WCET (Worst Case Execution Time) 계산 용이

2. **MXRC 요구사항 적합성**:
   - 대부분의 로봇 애플리케이션은 고정된 센서/모터 구성
   - 런타임 중 센서/모터 추가/제거는 드묾 (설정 변경은 재시작으로 처리)
   - 1ms cycle time 환경에서 복잡도 최소화 필요

3. **안정성 및 검증**:
   - 정적 매핑은 검증된 best practice (산업용 EtherCAT 애플리케이션 표준)
   - 초기화 시 한 번의 설정으로 안정적 동작
   - 디버깅 및 문제 해결 용이

4. **성능**:
   - 정적 매핑과 동적 매핑의 RT cycle 성능 차이 없음 (초기화 후)
   - 하지만 정적 매핑이 코드 복잡도 낮음 → 유지보수성 향상

### Alternatives Considered

#### 동적 PDO 매핑 (Dynamic/Flexible Mapping)

**장점**:
- 런타임 전(Pre-Operational 상태)에 PDO 구성 변경 가능
- 다양한 센서/모터 조합을 하나의 바이너리로 지원
- 설정 파일만 수정하면 재컴파일 불필요

**단점**:
- 초기화 복잡도 증가 (CoE SDO를 통한 PDO 재매핑)
- 일부 EtherCAT master가 동적 매핑 미지원
- YAML 설정 파일 오류 시 런타임 실패 위험

**결정**: P2 우선순위로 미루고, 초기 MVP는 정적 매핑으로 구현

#### Best Practices for Real-Time Systems

1. **Pre-Operational 단계에서 모든 설정 완료**:
   - PDO 매핑, SDO 설정은 `ecrt_master_activate()` 전에 완료
   - OP 상태 진입 후에는 PDO 구조 변경 불가

2. **3-Buffer Mode 사용**:
   - EtherCAT의 3-buffer 메커니즘으로 데이터 일관성 보장
   - PDI (Process Data Interface)와 EtherCAT 간 동시 읽기/쓰기 안전

3. **PDO 크기 최적화**:
   - 필요한 데이터만 매핑 (불필요한 PDO entry 제거)
   - 네트워크 대역폭 및 cycle time 단축
   - Padding 최소화 (32-bit 정렬)

4. **타입 안전성**:
   - PDO entry의 데이터 타입을 YAML 설정에 명시
   - 컴파일 시점에 타입 체크 수행
   ```cpp
   template<typename T>
   void updatePDOEntry(const PDOEntry& entry, T value) {
       static_assert(sizeof(T) * 8 == entry.bit_length,
                     "PDO entry size mismatch");
       // ...
   }
   ```

### Integration Notes

#### 정적 PDO 매핑 구현 예제

```cpp
// src/core/ethercat/EtherCATMaster.cpp

namespace mxrc::core::ethercat {

class EtherCATMaster {
public:
    int initialize(const EtherCATConfig& config) {
        // 1. Master 요청
        master_ = ecrt_request_master(config.network.master_index);
        if (!master_) {
            spdlog::error("Failed to request EtherCAT master");
            return -1;
        }

        // 2. Domain 생성
        domain_ = ecrt_master_create_domain(master_);
        if (!domain_) {
            spdlog::error("Failed to create domain");
            return -1;
        }

        // 3. Slave 설정 및 PDO 매핑
        for (const auto& slave_config : config.slaves) {
            configureSlave(slave_config);
        }

        // 4. Domain 레지스터 설정
        if (ecrt_domain_reg_pdo_entry_list(domain_, domain_regs_) < 0) {
            spdlog::error("Failed to register PDO entries");
            return -1;
        }

        // 5. Master 활성화
        if (ecrt_master_activate(master_) < 0) {
            spdlog::error("Failed to activate master");
            return -1;
        }

        // 6. Domain 데이터 포인터 획득
        process_data_ = ecrt_domain_data(domain_);
        if (!process_data_) {
            spdlog::error("Failed to get domain data pointer");
            return -1;
        }

        // 7. Distributed Clock 설정 (선택사항)
        if (config.distributed_clock.enabled) {
            configureDC(config.distributed_clock);
        }

        return 0;
    }

private:
    void configureSlave(const SlaveConfig& config) {
        // Slave configuration
        ec_slave_config_t* sc = ecrt_master_slave_config(
            master_,
            config.alias,
            config.position,
            config.vendor_id,
            config.product_code
        );

        if (!sc) {
            spdlog::error("Failed to get slave configuration for {}", config.name);
            return;
        }

        // RxPDO 설정 (Input: Slave → Master)
        for (const auto& pdo : config.rx_pdos) {
            for (const auto& entry : pdo.entries) {
                // Domain registry에 추가
                ec_pdo_entry_reg_t reg = {
                    config.alias,
                    config.position,
                    config.vendor_id,
                    config.product_code,
                    entry.index,
                    entry.subindex,
                    &pdo_offsets_[entry.datastore_key]  // offset 저장
                };
                domain_regs_.push_back(reg);

                // DataStore 키 매핑 저장
                pdo_mappings_[entry.datastore_key] = {
                    .offset = 0,  // 나중에 설정됨
                    .bit_length = entry.bit_length,
                    .datatype = entry.datatype
                };
            }
        }

        // TxPDO 설정 (Output: Master → Slave)
        // 동일한 방식

        // SDO 설정
        for (const auto& sdo : config.sdo_config) {
            uint8_t data[4];
            EC_WRITE_U32(data, sdo.value);
            ecrt_slave_config_sdo(sc, sdo.index, sdo.subindex,
                                  data, sizeof(data));
        }

        spdlog::info("Configured slave: {}", config.name);
    }

    ec_master_t* master_;
    ec_domain_t* domain_;
    uint8_t* process_data_;

    std::vector<ec_pdo_entry_reg_t> domain_regs_;
    std::map<std::string, unsigned int> pdo_offsets_;

    struct PDOMapping {
        unsigned int offset;
        uint8_t bit_length;
        std::string datatype;
    };
    std::map<std::string, PDOMapping> pdo_mappings_;
};

} // namespace mxrc::core::ethercat
```

#### Cyclic 데이터 읽기/쓰기

```cpp
void EtherCATMaster::cyclicTask(RTDataStore* rt_datastore) {
    // 1. PDO 수신
    ecrt_master_receive(master_);
    ecrt_domain_process(domain_);

    // 2. 센서 데이터 읽기 → RTDataStore
    for (const auto& [key, mapping] : pdo_mappings_) {
        if (mapping.datatype == "int32") {
            int32_t value = EC_READ_S32(process_data_ + mapping.offset);
            DataKey data_key = stringToDataKey(key);  // "ETHERCAT_ENCODER_POS_1" → enum
            rt_datastore->setInt32(data_key, value);
        }
        else if (mapping.datatype == "float") {
            float value = EC_READ_REAL(process_data_ + mapping.offset);
            DataKey data_key = stringToDataKey(key);
            rt_datastore->setFloat(data_key, value);
        }
        // 다른 타입들...
    }

    // 3. RTDataStore → 모터 명령 쓰기
    for (const auto& [key, mapping] : tx_pdo_mappings_) {
        if (mapping.datatype == "int32") {
            DataKey data_key = stringToDataKey(key);
            int32_t cmd_value;
            if (rt_datastore->getInt32(data_key, cmd_value) == 0) {
                EC_WRITE_S32(process_data_ + mapping.offset, cmd_value);
            }
        }
        // 다른 타입들...
    }

    // 4. PDO 전송
    ecrt_domain_queue(domain_);
    ecrt_master_send(master_);
}
```

#### 동적 매핑 지원 (향후 확장)

동적 매핑 구현 시 추가 고려사항:
1. **Pre-Operational 상태에서만 변경**:
   ```cpp
   // OP 상태 → Pre-OP 전환
   ecrt_master_deactivate(master_);

   // PDO 재매핑 (CoE SDO 사용)
   ecrt_slave_config_sdo8(sc, 0x1C12, 0x00, 0);  // RxPDO assign clear
   ecrt_slave_config_sdo16(sc, 0x1C12, 0x01, new_pdo_index);
   ecrt_slave_config_sdo8(sc, 0x1C12, 0x00, 1);  // RxPDO assign count

   // Master 재활성화
   ecrt_master_activate(master_);
   ```

2. **재초기화 오버헤드**:
   - Master deactivate/activate는 수백 ms 소요
   - RT 제어 루프 중단 필요 → SAFE_MODE 전환 후 수행

3. **YAML 설정 validation**:
   - 동적 매핑 전 설정 파일 검증 필수
   - PDO 크기 제한, 디바이스 지원 여부 확인

---

## 4. Distributed Clock (DC) 동기화

### Decision: DC 동기화를 P3 우선순위로 구현 (선택적 활성화)

기본 통신은 Free-Run 모드로 동작하며, 정밀 제어가 필요한 경우 DC를 활성화합니다.

### Rationale

1. **성능 vs 복잡도 트레이드오프**:
   - Free-Run 모드: 구현 간단, 대부분의 로봇 제어에 충분 (±100μs jitter 허용)
   - DC 모드: 나노초 단위 동기화, 멀티 축 정밀 제어 시 필수 (±1μs 이내)

2. **MXRC 요구사항 분석**:
   - User Story 1-2 (P1): 센서 읽기/모터 명령 전송 → DC 없이 가능
   - User Story 4 (P3): 다중 슬레이브 동기화 → DC 필요

3. **단계적 구현**:
   - Phase 1 (P1): Free-Run 모드로 기본 통신 검증
   - Phase 2 (P2): 초기화 및 상태 관리 완성
   - Phase 3 (P3): DC 동기화 추가, 성능 벤치마크

4. **실제 운영 경험**:
   - 많은 산업용 로봇이 Free-Run 모드로 운영됨 (jitter < 100μs면 충분)
   - DC는 CNC, 반도체 장비 등 초정밀 제어에만 필수

### DC 동기화 메커니즘

#### 원리

1. **Reference Clock 선택**:
   - 네트워크의 첫 번째 DC-capable slave를 reference로 사용
   - 모든 slave의 local clock을 reference에 동기화

2. **SYNC0 신호**:
   - Reference clock이 주기적으로 SYNC0 이벤트 생성 (예: 1ms 주기)
   - 모든 slave가 SYNC0 신호에 맞춰 application logic 실행
   - Frame delay나 jitter와 무관하게 동기화된 실행 보장

3. **Clock Offset 보정**:
   - Master가 주기적으로 slave의 clock offset 측정
   - Offset이 임계값 초과 시 자동 보정 (drift compensation)
   - 보정 주기: 수십 ms ~ 수백 ms

4. **MainDevice Synchronization (DCM)**:
   - Master 자신도 reference clock에 동기화 (Bus Shift 또는 Master Shift)
   - RT Executive의 cycle timer를 EtherCAT DC에 정렬

#### 성능 지표 (검색 결과 기반)

- **Jitter**: ±12ns ~ ±20ns (SYNC signal)
- **Clock Accuracy**: < 1μs
- **전체 시스템 Jitter**: < 100ns (DC 모드)
- **Free-Run 대비**: 10~100배 개선

### Integration Notes

#### DC 설정 코드

```cpp
void EtherCATMaster::configureDC(const DCConfig& dc_config) {
    if (!dc_config.enabled) {
        return;
    }

    spdlog::info("Configuring Distributed Clock...");

    // Reference clock slave 선택
    ec_slave_config_t* ref_slave = ecrt_master_slave_config(
        master_,
        0,  // alias
        dc_config.reference_clock,  // position (보통 0)
        0, 0  // vendor_id, product_code (any)
    );

    if (!ref_slave) {
        spdlog::error("Failed to get reference clock slave config");
        return;
    }

    // SYNC0 활성화
    // sync0_cycle_time: SYNC0 주기 (ns)
    // sync0_shift: SYNC0 이벤트 시작 오프셋 (ns)
    ecrt_slave_config_dc(
        ref_slave,
        0x0300,  // AssignActivate (SYNC0 enable)
        dc_config.sync0_cycle_time_ns,
        dc_config.sync0_shift_ns,
        0,  // SYNC1 cycle time (unused)
        0   // SYNC1 shift (unused)
    );

    spdlog::info("DC configured: SYNC0 cycle={}ns, shift={}ns",
                 dc_config.sync0_cycle_time_ns,
                 dc_config.sync0_shift_ns);
}
```

#### DCM (Master Synchronization)

```cpp
void EtherCATMaster::synchronizeMaster() {
    // Master의 application time을 DC reference clock에 동기화

    uint32_t ref_time;
    ecrt_master_reference_clock_time(master_, &ref_time);

    // Application time을 DC time으로 조정
    uint32_t app_time = /* RT Executive의 cycle time */;
    int32_t time_diff = ref_time - app_time;

    // Drift가 임계값 초과 시 보정
    if (abs(time_diff) > DC_SYNC_THRESHOLD_NS) {
        // Bus Shift 방식: DC reference clock offset 조정
        ecrt_master_sync_reference_clock_to(master_, app_time);

        spdlog::debug("DC sync: drift={}ns, corrected", time_diff);
    }
}
```

#### RT Executive와의 통합

```cpp
// RTExecutive에 DC 동기화 action 추가
rt_executive->registerAction("dc_sync", 100ms,  // 100ms마다 동기화
    [&](RTContext& ctx) {
        ethercat_master->synchronizeMaster();
    });
```

#### Jitter 최소화 기법

1. **CPU Isolation**:
   ```bash
   # Kernel boot parameter에 RT core 격리
   isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3

   # RT Executive를 격리된 core에 할당
   taskset -c 2 ./mxrc-rt
   ```

2. **High Resolution Timer 사용**:
   ```cpp
   // RTExecutive에서 HRTIMER 사용
   struct timespec next_period;
   clock_gettime(CLOCK_MONOTONIC, &next_period);
   next_period.tv_nsec += cycle_time_ns;

   // 절대 시간 기준 대기 (drift 누적 방지)
   clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
   ```

3. **SCHED_FIFO 우선순위**:
   ```cpp
   // RT Executive를 SCHED_FIFO 최고 우선순위로 실행
   struct sched_param param;
   param.sched_priority = 99;
   sched_setscheduler(0, SCHED_FIFO, &param);
   ```

4. **Page Locking**:
   ```cpp
   // 메모리 페이지 lock (page fault 방지)
   mlockall(MCL_CURRENT | MCL_FUTURE);
   ```

#### Clock Offset 모니터링

```cpp
void EtherCATMaster::monitorDCStatus() {
    ec_master_state_t master_state;
    ecrt_master_state(master_, &master_state);

    // 각 slave의 DC offset 확인
    for (size_t i = 0; i < num_slaves_; ++i) {
        ec_slave_config_state_t slave_state;
        ecrt_slave_config_state(slave_configs_[i], &slave_state);

        // System time difference (ns)
        int32_t system_time_diff = /* slave DC offset */;

        if (abs(system_time_diff) > 1000) {  // > 1μs
            spdlog::warn("Slave {} DC offset: {}ns", i, system_time_diff);
        }
    }
}
```

#### 성능 벤치마크

DC 활성화 전후 비교:
```cpp
// tests/integration/ethercat/dc_benchmark.cpp

TEST(EtherCATDC, JitterMeasurement) {
    // Free-Run 모드 jitter 측정
    auto jitter_freerun = measureCycleJitter(1000 /* cycles */);
    EXPECT_LT(jitter_freerun.max, 100'000);  // < 100μs

    // DC 모드 jitter 측정
    ethercat_master->enableDC(true);
    auto jitter_dc = measureCycleJitter(1000);
    EXPECT_LT(jitter_dc.max, 1'000);  // < 1μs

    spdlog::info("Jitter - Free-Run: {}ns, DC: {}ns",
                 jitter_freerun.max, jitter_dc.max);
}
```

---

## 5. DataStore 통합

### Decision: RTDataStore를 EtherCAT PDO 데이터의 primary storage로 사용

EtherCAT PDO 데이터를 RTDataStore의 고정 키에 매핑하고, Non-RT 시스템은 SharedMemory를 통해 접근합니다.

### Rationale

1. **기존 아키텍처 재사용**:
   - MXRC는 이미 RT/Non-RT 데이터 공유를 위한 `RTDataStore`와 `SharedMemory` 구조를 보유
   - EtherCAT 데이터도 동일한 메커니즘으로 처리 → 일관성 유지

2. **Versioned Data 패턴 활용**:
   - `VersionedData<T>`로 센서 데이터 저장 → 버전 번호로 freshness 확인
   - Non-RT 시스템이 오래된 데이터 사용 방지
   ```cpp
   auto encoder_data = rt_datastore->getVersioned<int32_t>(DataKey::ETHERCAT_ENCODER_POS_1);
   if (encoder_data.isModified() && encoder_data.isFresh(100ms)) {
       // 최근 100ms 이내 데이터만 사용
   }
   ```

3. **Lock-Free 읽기/쓰기**:
   - RTDataStore는 atomic 연산 기반 → RT cycle에 안전
   - EtherCAT PDO 업데이트가 RT deadline에 영향 없음

4. **타입 안전성**:
   - DataKey enum으로 컴파일 타임 타입 체크
   - std::any 대신 고정 타입 사용 → 오버헤드 최소화

### RT/Non-RT 데이터 공유 메커니즘

#### 아키텍처 다이어그램

```
┌─────────────────────────────────────────────────────────────┐
│                       RT Process (mxrc-rt)                  │
│                                                             │
│  ┌────────────────┐       ┌──────────────────┐            │
│  │ EtherCAT Master│ ────> │   RTDataStore    │            │
│  │   (IgH/SOEM)   │       │  (Lock-Free)     │            │
│  └────────────────┘       └──────────────────┘            │
│         ▲                          │                       │
│         │                          │ Periodic sync         │
│         │ 1ms cycle                │ (10ms)                │
│         │                          ▼                       │
│  ┌────────────────┐       ┌──────────────────┐            │
│  │  RT Executive  │       │ RTDataStoreShared│            │
│  │  (SCHED_FIFO)  │       │  (SharedMemory)  │            │
│  └────────────────┘       └──────────────────┘            │
│                                    │                       │
└────────────────────────────────────┼───────────────────────┘
                                     │ POSIX Shared Memory
┌────────────────────────────────────┼───────────────────────┐
│                  Non-RT Process (mxrc-nonrt)               │
│                                    │                       │
│  ┌─────────────────┐       ┌──────┴──────────┐            │
│  │ Task Management │ <──── │   DataStore     │            │
│  │    Module       │       │ (Non-RT, TBB)   │            │
│  └─────────────────┘       └─────────────────┘            │
│                                                             │
│  ┌─────────────────┐       ┌──────────────────┐            │
│  │  Monitoring/    │ <──── │    EventBus      │            │
│  │   Logging       │       │  (Priority Queue)│            │
│  └─────────────────┘       └──────────────────┘            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 데이터 흐름

1. **RT → Non-RT (센서 데이터)**:
   ```
   EtherCAT PDO → RTDataStore (1ms)
        ↓
   RTDataStoreShared::syncToSharedMemory (10ms)
        ↓
   SharedMemory (POSIX shm)
        ↓
   Non-RT DataStore::poll() → Task/Monitoring
   ```

2. **Non-RT → RT (제어 파라미터)**:
   ```
   Non-RT DataStore::set() (사용자 명령)
        ↓
   SharedMemory (POSIX shm)
        ↓
   RTDataStoreShared::syncFromSharedMemory (100ms)
        ↓
   RTDataStore → EtherCAT PDO (1ms)
   ```

### Integration Notes

#### RTDataStore 키 확장

```cpp
// src/core/rt/RTDataStore.h

enum class DataKey : uint16_t {
    // 기존 키들
    ROBOT_X = 0,
    ROBOT_Y = 1,
    ROBOT_Z = 2,
    ROBOT_SPEED = 3,
    ROBOT_STATUS = 4,

    // ===== EtherCAT 센서 키 (100~199) =====

    // 엔코더 위치 (int32, counts)
    ETHERCAT_ENCODER_POS_1 = 100,
    ETHERCAT_ENCODER_POS_2 = 101,
    ETHERCAT_ENCODER_POS_3 = 102,
    ETHERCAT_ENCODER_POS_4 = 103,
    ETHERCAT_ENCODER_POS_5 = 104,
    ETHERCAT_ENCODER_POS_6 = 105,

    // 엔코더 속도 (int32, counts/s)
    ETHERCAT_ENCODER_VEL_1 = 110,
    ETHERCAT_ENCODER_VEL_2 = 111,
    ETHERCAT_ENCODER_VEL_3 = 112,
    ETHERCAT_ENCODER_VEL_4 = 113,
    ETHERCAT_ENCODER_VEL_5 = 114,
    ETHERCAT_ENCODER_VEL_6 = 115,

    // 힘/토크 센서 (float, N or Nm)
    ETHERCAT_FORCE_SENSOR_FX = 120,
    ETHERCAT_FORCE_SENSOR_FY = 121,
    ETHERCAT_FORCE_SENSOR_FZ = 122,
    ETHERCAT_FORCE_SENSOR_TX = 123,
    ETHERCAT_FORCE_SENSOR_TY = 124,
    ETHERCAT_FORCE_SENSOR_TZ = 125,

    // 온도 센서 (float, °C)
    ETHERCAT_TEMP_SENSOR_1 = 140,
    ETHERCAT_TEMP_SENSOR_2 = 141,

    // Digital Input (uint64, bit flags)
    ETHERCAT_DI_MODULE_1 = 150,

    // Analog Input (float, raw voltage or scaled)
    ETHERCAT_AI_CHANNEL_1 = 160,
    ETHERCAT_AI_CHANNEL_2 = 161,

    // ===== EtherCAT 모터 명령 키 (200~299) =====

    // 위치 명령 (int32, counts)
    ETHERCAT_MOTOR_CMD_POS_1 = 200,
    ETHERCAT_MOTOR_CMD_POS_2 = 201,
    ETHERCAT_MOTOR_CMD_POS_3 = 202,
    ETHERCAT_MOTOR_CMD_POS_4 = 203,
    ETHERCAT_MOTOR_CMD_POS_5 = 204,
    ETHERCAT_MOTOR_CMD_POS_6 = 205,

    // 속도 명령 (int32, rpm or counts/s)
    ETHERCAT_MOTOR_CMD_VEL_1 = 210,
    ETHERCAT_MOTOR_CMD_VEL_2 = 211,
    ETHERCAT_MOTOR_CMD_VEL_3 = 212,
    ETHERCAT_MOTOR_CMD_VEL_4 = 213,
    ETHERCAT_MOTOR_CMD_VEL_5 = 214,
    ETHERCAT_MOTOR_CMD_VEL_6 = 215,

    // 토크 명령 (float, Nm or mNm)
    ETHERCAT_MOTOR_CMD_TORQUE_1 = 220,
    ETHERCAT_MOTOR_CMD_TORQUE_2 = 221,
    ETHERCAT_MOTOR_CMD_TORQUE_3 = 222,
    ETHERCAT_MOTOR_CMD_TORQUE_4 = 223,
    ETHERCAT_MOTOR_CMD_TORQUE_5 = 224,
    ETHERCAT_MOTOR_CMD_TORQUE_6 = 225,

    // 제어 워드 (uint64, bit flags)
    ETHERCAT_MOTOR_CTRL_WORD_1 = 240,
    ETHERCAT_MOTOR_CTRL_WORD_2 = 241,

    // Digital Output (uint64, bit flags)
    ETHERCAT_DO_MODULE_1 = 250,

    // ===== EtherCAT 상태 키 (300~319) =====

    // 모터 상태 워드 (uint64)
    ETHERCAT_MOTOR_STATUS_1 = 300,
    ETHERCAT_MOTOR_STATUS_2 = 301,

    // 에러 코드 (uint64)
    ETHERCAT_ERROR_CODE = 310,

    // 통신 통계 (uint64)
    ETHERCAT_FRAME_COUNT = 315,
    ETHERCAT_ERROR_COUNT = 316,

    MAX_KEYS = 512
};
```

#### EtherCAT → RTDataStore 업데이트

```cpp
// src/core/ethercat/EtherCATMaster.cpp

void EtherCATMaster::updateRTDataStore(RTDataStore* rt_datastore) {
    // PDO 데이터를 RTDataStore에 복사

    for (const auto& [key, mapping] : pdo_mappings_) {
        DataKey data_key = stringToDataKey(key);

        if (mapping.datatype == "int32") {
            int32_t value = EC_READ_S32(process_data_ + mapping.offset);
            rt_datastore->setInt32(data_key, value);
        }
        else if (mapping.datatype == "float") {
            float value = EC_READ_REAL(process_data_ + mapping.offset);
            rt_datastore->setFloat(data_key, value);
        }
        else if (mapping.datatype == "uint64") {
            uint64_t value = EC_READ_U64(process_data_ + mapping.offset);
            rt_datastore->setUint64(data_key, value);
        }
        // 다른 타입들...
    }
}
```

#### Versioned Data 저장 패턴

```cpp
// Non-RT에서 센서 데이터 읽기 (SharedMemory 통해)

class SensorMonitor {
public:
    void checkEncoderPosition() {
        // DataStore::getVersioned는 내부적으로 SharedMemory에서 읽음
        auto pos_data = datastore_->getVersioned<int32_t>("encoder_pos_1");

        // 버전 번호로 freshness 확인
        if (pos_data.getVersion() > last_version_) {
            // 새 데이터 도착
            int32_t position = pos_data.value;
            uint64_t timestamp_ns = pos_data.getTimestampNs();

            // 100ms 이내 데이터인지 확인
            uint64_t now_ns = getCurrentTimeNs();
            if (now_ns - timestamp_ns < 100'000'000) {  // 100ms
                processEncoderData(position);
                last_version_ = pos_data.getVersion();
            } else {
                spdlog::warn("Encoder data stale: age={}ms",
                             (now_ns - timestamp_ns) / 1'000'000);
            }
        }
    }

private:
    std::shared_ptr<DataStore> datastore_;
    uint64_t last_version_ = 0;
};
```

#### RT-Safe 데이터 접근 패턴

```cpp
// RT cycle에서의 안전한 데이터 접근

void RTExecutive::controlLoop(RTContext& ctx) {
    // 1. 센서 데이터 읽기 (EtherCAT → RTDataStore, 이미 완료됨)

    // 2. RTDataStore에서 데이터 가져오기 (lock-free)
    int32_t encoder_pos;
    if (ctx.data_store->getInt32(DataKey::ETHERCAT_ENCODER_POS_1, encoder_pos) != 0) {
        // 에러 처리: 데이터 없음
        spdlog::error("Failed to get encoder position");
        state_machine_->transitionTo(RTState::SAFE_MODE);
        return;
    }

    // 3. 데이터 신선도 확인
    if (!ctx.data_store->isFresh(DataKey::ETHERCAT_ENCODER_POS_1, 10'000'000)) {  // 10ms
        // 경고: 오래된 데이터 (통신 지연 가능성)
        spdlog::warn("Encoder data is stale");
    }

    // 4. 제어 알고리즘 실행
    int32_t target_velocity = computeControlOutput(encoder_pos);

    // 5. 모터 명령 설정 (RTDataStore → EtherCAT)
    ctx.data_store->setInt32(DataKey::ETHERCAT_MOTOR_CMD_VEL_1, target_velocity);
}
```

#### Shared Memory 동기화 (10ms 주기)

```cpp
// src/core/rt/RTDataStoreShared.cpp

void RTDataStoreShared::syncToSharedMemory(RTDataStore* rt_datastore) {
    // RT → Non-RT 데이터 동기화

    // EtherCAT 센서 데이터만 선택적으로 복사 (전체 복사는 비효율적)
    for (uint16_t key = 100; key < 200; ++key) {  // ETHERCAT_ENCODER_POS_1 ~ 센서 범위
        DataKey data_key = static_cast<DataKey>(key);

        // 타입에 따라 복사
        DataType type = rt_datastore->getType(data_key);
        if (type == DataType::INT32) {
            int32_t value;
            if (rt_datastore->getInt32(data_key, value) == 0) {
                writeToSharedMemory(key, value);
            }
        }
        // 다른 타입들...
    }
}

void RTDataStoreShared::syncFromSharedMemory(RTDataStore* rt_datastore) {
    // Non-RT → RT 데이터 동기화 (제어 파라미터)

    // EtherCAT 모터 명령 범위만 읽기
    for (uint16_t key = 200; key < 300; ++key) {
        DataKey data_key = static_cast<DataKey>(key);

        // SharedMemory에서 읽기
        int32_t cmd_value;
        if (readFromSharedMemory(key, cmd_value)) {
            rt_datastore->setInt32(data_key, cmd_value);
        }
    }
}
```

#### 성능 최적화

1. **Selective Sync** (선택적 동기화):
   - 모든 키를 복사하지 않고, EtherCAT 관련 키만 동기화
   - 범위 기반 iteration (100~199, 200~299)
   - 불필요한 메모리 복사 최소화

2. **Batch Update** (배치 업데이트):
   - 10ms마다 한 번에 모든 센서 데이터 동기화
   - 개별 업데이트 대비 IPC 오버헤드 감소

3. **Zero-Copy Access** (가능한 경우):
   - SharedMemory 포인터 직접 접근 (타입 체크 필수)
   ```cpp
   struct EtherCATSensorData {
       int32_t encoder_pos[6];
       int32_t encoder_vel[6];
       float force_sensor[6];
       uint64_t timestamp_ns;
       std::atomic<uint64_t> version;
   };

   // SharedMemory에 구조체 직접 매핑 (alignment 주의)
   EtherCATSensorData* sensor_data =
       reinterpret_cast<EtherCATSensorData*>(shared_memory_ptr_);
   ```

---

## 구현 우선순위 및 마일스톤

### Phase 1: 기본 EtherCAT 통신 (P1, 2주)

**목표**: 센서 읽기 및 모터 명령 전송의 기본 통신 검증

**작업**:
1. IgH EtherCAT Master 커널 모듈 설치 및 설정
2. SOEM 사용자 공간 라이브러리 통합 (개발/테스트용)
3. yaml-cpp 통합 및 설정 파일 파서 구현
4. 정적 PDO 매핑 구현
5. RTDataStore 키 확장 (DataKey enum)
6. EtherCAT cyclic action을 RT Executive에 등록
7. 단일 슬레이브 테스트 (엔코더 또는 모터 드라이버)

**검증**:
- 1ms cycle time 내 PDO 읽기/쓰기 완료
- RTDataStore에 센서 데이터 정상 저장
- Non-RT에서 SharedMemory를 통해 데이터 접근 확인

### Phase 2: 초기화 및 상태 관리 (P2, 1.5주)

**목표**: 시스템 시작부터 EtherCAT 통신 활성화까지 자동화

**작업**:
1. EtherCAT Master 초기화 시퀀스 구현
2. Slave 상태 전환 (INIT → PREOP → SAFEOP → OP)
3. 에러 처리 및 RTStateMachine 통합
4. EventBus를 통한 에러 이벤트 발행
5. Watchdog timeout 감지 및 SAFE_MODE 전환
6. 멀티 슬레이브 테스트 (3개 이상)

**검증**:
- 부팅 후 5초 이내 EtherCAT 통신 활성화
- 통신 장애 시 SAFE_MODE 전환 확인
- 에러 이벤트가 Non-RT 시스템에 전달됨

### Phase 3: DC 동기화 및 최적화 (P3, 1주)

**목표**: 정밀 제어를 위한 Distributed Clock 활성화

**작업**:
1. DC 설정 코드 구현 (SYNC0 활성화)
2. DCM (Master Synchronization) 구현
3. Clock offset 모니터링 및 drift compensation
4. Jitter 벤치마크 (Free-Run vs DC 모드)
5. CPU isolation 및 SCHED_FIFO 최적화
6. 성능 문서화

**검증**:
- DC 모드에서 jitter < 1μs
- Clock offset ±1μs 이내 수렴
- 24시간 연속 운영 안정성 테스트

### Phase 4: 문서화 및 통합 테스트 (0.5주)

**작업**:
1. 사용자 매뉴얼 작성 (설정 파일 작성 가이드)
2. 통합 테스트 시나리오 실행
3. CI/CD 파이프라인 통합
4. 성능 리포트 작성

---

## 리스크 및 완화 전략

### Risk 1: IgH EtherCAT Master 설치 복잡도

**영향**: 개발 환경 구축 지연, 신규 개발자 온보딩 어려움

**완화**:
- Docker 이미지 제공 (PREEMPT_RT 커널 + IgH 사전 설치)
- 설치 스크립트 자동화 (`scripts/install_ethercat.sh`)
- SOEM 병행 지원으로 로컬 개발 시 대안 제공

### Risk 2: EtherCAT 하드웨어 의존성

**영향**: 실제 장비 없이 테스트 불가

**완화**:
- EtherCAT 시뮬레이터 사용 (Virtual EtherCAT Master)
- Mock PDO 데이터 생성 테스트
- CI/CD에서 SOEM 기반 단위 테스트 실행

### Risk 3: Real-Time 성능 미달

**영향**: 1ms cycle time 요구사항 미충족

**완화**:
- 초기 프로파일링으로 병목 지점 식별
- PREEMPT_RT 커널 튜닝 (isolcpus, CPU affinity)
- WCET 분석 및 코드 최적화
- 필요 시 cycle time 조정 (2ms로 완화)

### Risk 4: DC 동기화 불안정

**영향**: Jitter 증가, 멀티 축 동기화 실패

**완화**:
- Phase 3를 선택사항으로 유지 (Free-Run 모드로도 대부분 동작)
- DC 활성화 전 충분한 Free-Run 모드 검증
- Vendor 문서 및 커뮤니티 best practice 참조

### Risk 5: 라이선스 이슈

**영향**: GPLv2로 인한 상용 제품 배포 제약

**완화**:
- IgH 커널 모듈 수정 없이 사용 (바이너리 배포 가능)
- 필요 시 SOEM 라이선스 예외 조항 활용
- 법무팀 검토 후 최종 결정

---

## 참고 자료

### 공식 문서
- [EtherCAT Technology Group](https://www.ethercat.org/)
- [IgH EtherCAT Master Documentation](https://etherlab.org/en/ethercat/)
- [SOEM GitHub Repository](https://github.com/OpenEtherCATsociety/SOEM)
- [yaml-cpp GitHub](https://github.com/jbeder/yaml-cpp)

### 기술 논문
- "EtherCAT: The Ethernet Fieldbus" - Beckhoff Automation
- "Real-Time Ethernet for Industrial Automation" - IEEE Industrial Electronics Magazine

### 커뮤니티
- [LinuxCNC EtherCAT Forum](https://forum.linuxcnc.org/ethercat)
- [Open EtherCAT Society](https://openethercatsociety.github.io/)

### MXRC 내부 문서
- `/home/tory/workspace/mxrc/mxrc/docs/specs/021-rt-nonrt-architecture/spec.md`
- `/home/tory/workspace/mxrc/mxrc/docs/specs/022-fix-architecture-issues/plan.md`
- `/home/tory/workspace/mxrc/mxrc/config/rt_schedule.json`

---

## 결론

본 기술 조사를 통해 MXRC 프로젝트에 EtherCAT 통합을 위한 최적의 기술 스택을 선정했습니다:

1. **EtherCAT Master**: IgH EtherCAT Master (프로덕션) + SOEM (개발/테스트)
2. **YAML 파서**: yaml-cpp
3. **PDO 매핑**: 정적 매핑 우선, 동적 매핑은 선택적 확장
4. **DC 동기화**: P3 우선순위, Free-Run 모드 후 추가
5. **DataStore 통합**: RTDataStore + SharedMemory 기반 RT/Non-RT 공유

모든 선택은 MXRC의 기존 아키텍처(RT Executive, RTDataStore, EventBus)와의 호환성을 최우선으로 고려했으며, 1ms cycle time 및 24시간 연속 운영 요구사항을 충족할 수 있도록 설계되었습니다.

다음 단계는 `/speckit.plan` 명령을 통해 상세 구현 계획을 작성하고, `/speckit.tasks` 명령으로 실행 가능한 작업 목록을 생성하는 것입니다.
