# Quickstart Guide: EtherCAT 센서/모터 데이터 수신 인프라

**Feature Branch**: `001-ethercat-integration`
**Created**: 2025-11-23
**Last Updated**: 2025-11-23

---

## 개요

본 가이드는 MXRC 프로젝트에 EtherCAT 기반 센서/모터 데이터 수신 기능을 빠르게 시작할 수 있도록 돕습니다.

---

## 사전 준비

### 시스템 요구사항

- **OS**: Ubuntu 24.04 LTS
- **Kernel**: Linux PREEMPT_RT 패치 적용 (권장)
- **Compiler**: GCC 11+ 또는 Clang 14+
- **CMake**: 3.16+
- **RAM**: 최소 4GB

### 필수 의존성 설치

```bash
# 기본 빌드 도구
sudo apt update
sudo apt install build-essential cmake git

# IgH EtherCAT Master 의존성
sudo apt install automake libtool linux-headers-$(uname -r)

# YAML 파싱 라이브러리
sudo apt install libyaml-cpp-dev

# 기존 MXRC 의존성
sudo apt install libspdlog-dev libgtest-dev libtbb-dev
```

### IgH EtherCAT Master 설치

```bash
# IgH EtherCAT Master 다운로드 및 빌드
cd /opt
sudo git clone https://gitlab.com/etherlab.org/ethercat.git
cd ethercat
sudo ./bootstrap
sudo ./configure --prefix=/usr/local --disable-8139too --disable-eoe
sudo make
sudo make install

# Kernel 모듈 로드
sudo modprobe ec_master
sudo modprobe ec_generic

# 영구 설정 (재부팅 후에도 유지)
echo "ec_master" | sudo tee -a /etc/modules
echo "ec_generic" | sudo tee -a /etc/modules

# EtherCAT master가 사용할 네트워크 인터페이스 지정
# /etc/ethercat.conf 파일 편집
sudo vi /etc/ethercat.conf
# MASTER0_DEVICE="eth0"  # 실제 EtherCAT 네트워크 인터페이스로 변경
```

---

## 프로젝트 빌드

### 1. 저장소 클론

```bash
cd ~/workspace
git clone https://github.com/your-org/mxrc.git
cd mxrc
git checkout 001-ethercat-integration
```

### 2. 빌드 디렉토리 생성

```bash
mkdir -p build
cd build
```

### 3. CMake 설정

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DENABLE_ETHERCAT=ON \
         -DETHERCAT_INCLUDE_DIR=/usr/local/include \
         -DETHERCAT_LIBRARY=/usr/local/lib/libethercat.so
```

### 4. 컴파일

```bash
make -j$(nproc)
```

### 5. 테스트 실행

```bash
# 단위 테스트
./run_tests --gtest_filter="EtherCAT*"

# 통합 테스트 (시뮬레이터 사용)
./run_tests --gtest_filter="EtherCATIntegration*"
```

---

## 설정 파일 작성

### YAML 설정 파일 구조

EtherCAT slave 설정은 YAML 파일로 관리됩니다. 기본 위치는 `config/ethercat/slaves.yaml`입니다.

#### 예제: `config/ethercat/slaves.yaml`

```yaml
# EtherCAT Master Configuration
master:
  index: 0                    # Master 번호 (단일 마스터는 0)
  cycle_time_ns: 10000000     # 10ms (10,000,000 ns)
  priority: 99                # RT priority (SCHED_FIFO)

# EtherCAT Slaves
slaves:
  - alias: 0
    position: 0
    vendor_id: 0x00000002     # Beckhoff
    product_code: 0x044c2c52  # EL5101 (Incremental encoder)
    device_name: "Joint1_Encoder"
    device_type: SENSOR
    pdo_mappings:
      - direction: INPUT
        index: 0x1A00
        subindex: 0x01
        data_type: INT32
        offset: 0
        description: "Position value"

  - alias: 1
    position: 1
    vendor_id: 0x000000ab     # Kollmorgen (예시)
    product_code: 0x12345678
    device_name: "Joint1_ServoDriver"
    device_type: MOTOR
    pdo_mappings:
      - direction: OUTPUT
        index: 0x1600
        subindex: 0x01
        data_type: INT32
        offset: 0
        description: "Target position"
      - direction: OUTPUT
        index: 0x1600
        subindex: 0x02
        data_type: UINT16
        offset: 4
        description: "Control word"

  - alias: 2
    position: 2
    vendor_id: 0x00000002     # Beckhoff
    product_code: 0x0c1e3052  # EL3104 (4-channel analog input)
    device_name: "Analog_Inputs"
    device_type: IO_MODULE
    pdo_mappings:
      - direction: INPUT
        index: 0x1A00
        subindex: 0x01
        data_type: INT16
        offset: 0
        description: "AI Channel 1"
      - direction: INPUT
        index: 0x1A01
        subindex: 0x01
        data_type: INT16
        offset: 2
        description: "AI Channel 2"
```

---

## 기본 사용법

### 1. EtherCAT Master 초기화

```cpp
#include "core/ethercat/core/EtherCATMaster.h"
#include "core/ethercat/util/YAMLConfigParser.h"

using namespace mxrc::core::ethercat;

// YAML 설정 로드
auto config = YAMLConfigParser::loadFromFile("config/ethercat/slaves.yaml");

// EtherCAT Master 생성 및 초기화
auto master = std::make_unique<EtherCATMaster>(config);
master->initialize();  // Slave 발견 및 상태 전환 (INIT → OP)
```

### 2. RT Executive에 등록

```cpp
#include "core/rt/RTExecutive.h"
#include "core/ethercat/adapters/RTEtherCATCycle.h"

// RT Executive 생성
auto rtExec = std::make_shared<RTExecutive>(
    std::chrono::milliseconds(10),  // minor cycle = 10ms
    std::chrono::milliseconds(100)  // major cycle = 100ms
);

// EtherCAT Cyclic Action 생성
auto ethercatCycle = std::make_shared<RTEtherCATCycle>(
    master.get(),
    rtDataStore  // RTDataStore 참조
);

// RT Executive에 등록
rtExec->registerCyclicAction(
    "ethercat_cycle",
    std::chrono::milliseconds(10),  // 10ms마다 실행
    [&ethercatCycle](RTContext& ctx) {
        ethercatCycle->execute(ctx);
    }
);

// RT Executive 시작
rtExec->start();
```

### 3. 센서 데이터 읽기

```cpp
#include "core/datastore/RTDataStore.h"
#include "core/ethercat/dto/SensorData.h"

using namespace mxrc::core::datastore;
using namespace mxrc::core::ethercat;

// RTDataStore에서 센서 데이터 읽기
auto versioned = rtDataStore->get<PositionSensorData>(
    DataKey::ETHERCAT_SENSOR_POSITION_0
);

if (versioned.isValid() && versioned.data.valid) {
    int32_t position = versioned.data.position;
    uint64_t timestamp = versioned.data.timestamp;

    spdlog::info("Joint 1 Position: {} counts at {} ns",
                 position, timestamp);
}
```

### 4. 모터 명령 전송

```cpp
#include "core/ethercat/dto/MotorCommand.h"

using namespace mxrc::core::ethercat;

// 모터 명령 생성
ServoDriverCommand cmd;
cmd.target_position = 1.5708;  // 90도 (radians)
cmd.control_mode = ControlMode::POSITION;
cmd.max_velocity = 2.0;         // 2 rad/s
cmd.max_torque = 10.0;          // 10 Nm
cmd.enable = true;
cmd.timestamp = getCurrentTimeNs();
cmd.slave_id = 1;

// RTDataStore에 명령 저장
rtDataStore->set(DataKey::ETHERCAT_MOTOR_CMD_0, cmd);

// 다음 RT cycle에서 EtherCAT으로 전송됨
```

---

## 시뮬레이션 환경

실제 EtherCAT 하드웨어 없이 테스트하려면 Virtual EtherCAT Master를 사용합니다.

### SOEM 기반 시뮬레이터 실행

```bash
# SOEM (Simple Open EtherCAT Master) 설치
cd /opt
sudo git clone https://github.com/OpenEtherCATsociety/SOEM.git
cd SOEM
mkdir build && cd build
cmake ..
make
sudo make install

# 시뮬레이터 모드로 빌드
cd ~/workspace/mxrc/build
cmake .. -DETHERCAT_SIMULATOR=ON
make -j$(nproc)

# 시뮬레이터 테스트 실행
./run_tests --gtest_filter="VirtualSlave*"
```

---

## 디버깅

### EtherCAT Master 상태 확인

```bash
# Master 상태 출력
sudo ethercat master

# Slave 목록 출력
sudo ethercat slaves

# PDO 매핑 확인
sudo ethercat pdos

# 실시간 데이터 모니터링
sudo ethercat data
```

### 로그 레벨 조정

```cpp
// spdlog 레벨 설정
spdlog::set_level(spdlog::level::debug);

// EtherCAT 관련 로그만 필터링
spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
```

### RT 성능 측정

```bash
# RT latency 측정 (cyclictest)
sudo apt install rt-tests
sudo cyclictest -p 99 -t 1 -n -m -i 10000

# EtherCAT cycle jitter 측정
sudo ethercat graph
```

---

## 트러블슈팅

### 문제: EtherCAT Master 초기화 실패

**증상**:
```
[ERROR] Failed to initialize EtherCAT master: Permission denied
```

**해결**:
```bash
# EtherCAT master 권한 확인
ls -l /dev/EtherCAT*

# 권한이 없으면 사용자를 ethercat 그룹에 추가
sudo usermod -a -G ethercat $USER
sudo reboot
```

### 문제: Slave 발견 안 됨

**증상**:
```
[WARNING] No slaves found on EtherCAT network
```

**해결**:
```bash
# 네트워크 인터페이스 확인
ip link show

# EtherCAT 케이블 연결 확인
sudo ethercat slaves
# Expected: "0:0  PREOP  +  Beckhoff EL5101"

# /etc/ethercat.conf에서 올바른 인터페이스 지정 확인
cat /etc/ethercat.conf | grep MASTER0_DEVICE
```

### 문제: RT Latency가 높음

**증상**:
```
[WARNING] RT cycle latency: 12.5ms (exceeds 10ms)
```

**해결**:
```bash
# CPU isolation 설정 (/etc/default/grub)
GRUB_CMDLINE_LINUX="isolcpus=1-3 nohz_full=1-3 rcu_nocbs=1-3"
sudo update-grub
sudo reboot

# RT priority 확인
chrt -p <RT_Executive_PID>
# Expected: policy: SCHED_FIFO, priority: 99
```

---

## 다음 단계

1. **Phase 1 완료 후**: `/speckit.tasks` 명령으로 세부 작업 목록 생성
2. **구현 시작**: `src/core/ethercat/` 디렉토리에 코드 작성
3. **테스트 작성**: TDD 방식으로 테스트 먼저 작성 후 구현
4. **문서 업데이트**: 구현 완료 후 `docs/architecture/` 아키텍처 문서 업데이트

---

## 참고 자료

- [IgH EtherCAT Master 공식 문서](https://etherlab.org/en/ethercat/)
- [EtherCAT Technology Group](https://www.ethercat.org/)
- [SOEM GitHub](https://github.com/OpenEtherCATsociety/SOEM)
- [PREEMPT_RT Kernel 패치](https://wiki.linuxfoundation.org/realtime/start)
- MXRC 내부 문서:
  - [RTExecutive 아키텍처](../../architecture/)
  - [RTDataStore 설계](../../architecture/)
  - [Constitution](../../../.specify/memory/constitution.md)
