# Data Model: EtherCAT 센서/모터 데이터 수신 인프라

**Feature Branch**: `001-ethercat-integration`
**Created**: 2025-11-23
**Status**: Design
**Last Updated**: 2025-11-23

---

## 개요

본 문서는 EtherCAT 센서/모터 데이터 수신 인프라의 데이터 모델을 정의합니다. 모든 엔터티는 타입 안전성과 실시간 성능을 고려하여 설계되었습니다.

---

## 핵심 엔터티

### 1. Sensor Data (센서 데이터)

#### 1.1 PositionSensorData (위치 센서)

**설명**: 엔코더로부터 읽은 위치 데이터

**필드**:
- `position`: int32_t - 엔코더 카운트 값
- `velocity`: int32_t - 속도 (카운트/초)
- `timestamp`: uint64_t - 읽은 시각 (nanoseconds, CLOCK_MONOTONIC)
- `valid`: bool - 데이터 유효성 플래그
- `slave_id`: uint16_t - EtherCAT slave 주소

**검증 규칙**:
- `position`: INT32_MIN ~ INT32_MAX
- `timestamp`: 단조 증가 (monotonic increasing)
- `valid`: false인 경우 데이터 사용 금지

**상태 전환**:
```
INVALID (valid=false) ──read_pdo──> VALID (valid=true)
VALID ──error_detected──> INVALID
```

#### 1.2 VelocitySensorData (속도 센서)

**설명**: 타코미터 또는 계산된 속도 데이터

**필드**:
- `velocity`: double - 속도 (m/s 또는 rad/s)
- `timestamp`: uint64_t
- `valid`: bool
- `slave_id`: uint16_t

#### 1.3 TorqueSensorData (토크/힘 센서)

**설명**: 힘/토크 센서로부터 읽은 데이터

**필드**:
- `force_x`: double - X축 힘 (N)
- `force_y`: double - Y축 힘 (N)
- `force_z`: double - Z축 힘 (N)
- `torque_x`: double - X축 토크 (Nm)
- `torque_y`: double - Y축 토크 (Nm)
- `torque_z`: double - Z축 토크 (Nm)
- `timestamp`: uint64_t
- `valid`: bool
- `slave_id`: uint16_t

**검증 규칙**:
- 모든 force/torque 값: -10000.0 ~ 10000.0 (센서 범위)
- Out-of-range 시 `valid = false`

#### 1.4 DigitalInputData (범용 디지털 입력)

**설명**: 범용 DI 모듈로부터 읽은 boolean 값

**필드**:
- `channel`: uint8_t - 채널 번호 (0~31)
- `value`: bool - HIGH(true) / LOW(false)
- `timestamp`: uint64_t
- `slave_id`: uint16_t

#### 1.5 AnalogInputData (범용 아날로그 입력)

**설명**: 범용 AI 모듈로부터 읽은 실수 값

**필드**:
- `channel`: uint8_t - 채널 번호 (0~15)
- `value`: double - 측정값 (센서 타입에 따라 온도, 압력 등)
- `unit`: std::string - 단위 ("°C", "Pa", "V" 등)
- `min_value`: double - 최소 범위
- `max_value`: double - 최대 범위
- `timestamp`: uint64_t
- `valid`: bool
- `slave_id`: uint16_t

**검증 규칙**:
- `value`: min_value <= value <= max_value
- Out-of-range 시 `valid = false` 및 경고 로그

---

### 2. Motor Command (모터 명령)

#### 2.1 BLDCMotorCommand (BLDC 모터)

**설명**: BLDC 모터 드라이버에 전송할 명령

**필드**:
- `target_velocity`: double - 목표 속도 (RPM)
- `target_torque`: double - 목표 토크 (Nm)
- `control_mode`: enum ControlMode { VELOCITY, TORQUE, DISABLED }
- `enable`: bool - 모터 활성화 플래그
- `timestamp`: uint64_t
- `slave_id`: uint16_t

**검증 규칙**:
- `target_velocity`: -10000.0 ~ 10000.0 RPM
- `target_torque`: -100.0 ~ 100.0 Nm
- `enable=false` 시 모터 비활성화 및 안전 상태

**상태 전환**:
```
DISABLED ──enable=true──> VELOCITY_MODE / TORQUE_MODE
VELOCITY_MODE ──enable=false──> DISABLED
TORQUE_MODE ──enable=false──> DISABLED
```

#### 2.2 ServoDriverCommand (서보 드라이버)

**설명**: 서보 드라이버에 전송할 명령 (위치/속도/토크 모드)

**필드**:
- `target_position`: double - 목표 위치 (라디안 또는 미터)
- `target_velocity`: double - 목표 속도 (rad/s 또는 m/s)
- `target_torque`: double - 목표 토크 (Nm)
- `control_mode`: enum ControlMode { POSITION, VELOCITY, TORQUE, DISABLED }
- `max_velocity`: double - 최대 속도 제한
- `max_torque`: double - 최대 토크 제한
- `enable`: bool
- `timestamp`: uint64_t
- `slave_id`: uint16_t

**검증 규칙**:
- `target_position`: -2π ~ 2π (회전각) 또는 -10.0 ~ 10.0 m (직선)
- `target_velocity`: 0 ~ max_velocity
- `target_torque`: 0 ~ max_torque

---

### 3. EtherCAT Configuration (설정)

#### 3.1 SlaveConfig (슬레이브 설정)

**설명**: EtherCAT slave 장비 구성 정보

**필드**:
- `alias`: uint16_t - Slave alias (YAML에서 지정)
- `position`: uint16_t - Bus position (0부터 시작)
- `vendor_id`: uint32_t - Vendor ID (예: Beckhoff = 0x00000002)
- `product_code`: uint32_t - Product code (예: EL3104 = 0x0c1e3052)
- `device_name`: std::string - 장치 이름 (사람이 읽을 수 있는 이름)
- `device_type`: enum DeviceType { SENSOR, MOTOR, IO_MODULE }
- `pdo_mappings`: std::vector<PDOMapping> - PDO 매핑 목록

#### 3.2 PDOMapping (PDO 매핑)

**설명**: Process Data Object 매핑 정의

**필드**:
- `direction`: enum Direction { INPUT, OUTPUT } - PDO 방향
- `index`: uint16_t - PDO index (CoE object dictionary)
- `subindex`: uint8_t - PDO subindex
- `bit_length`: uint8_t - 데이터 길이 (비트)
- `data_type`: enum DataType { INT8, UINT8, INT16, UINT16, INT32, UINT32, FLOAT, DOUBLE }
- `offset`: size_t - PDO domain 내 offset (bytes)

**검증 규칙**:
- `index`: 0x1600 ~ 0x1FFF (RxPDO), 0x1A00 ~ 0x1FFF (TxPDO)
- `bit_length`: 1, 8, 16, 32, 64
- `offset`: 4-byte aligned (32-bit boundary)

#### 3.3 DCConfiguration (Distributed Clock 설정)

**설명**: DC 동기화 설정 (P3 우선순위)

**필드**:
- `enable`: bool - DC 활성화 여부
- `reference_slave`: uint16_t - 기준 clock slave (보통 0)
- `sync0_cycle_time`: uint32_t - SYNC0 주기 (nanoseconds)
- `sync0_shift_time`: int32_t - SYNC0 shift (nanoseconds)
- `sync1_cycle_time`: uint32_t - SYNC1 주기 (optional)

---

### 4. RTDataStore 통합

#### 4.1 DataKey 확장

**설명**: RTDataStore의 DataKey enum에 EtherCAT 관련 키 추가

**키 범위 할당**:
```cpp
enum class DataKey : uint16_t {
    // 기존 키 (0~99)
    TASK_STATUS = 0,
    SEQUENCE_STATE = 1,
    // ...

    // EtherCAT 센서 데이터 (100~199)
    ETHERCAT_SENSOR_POSITION_0 = 100,  // Slave 0 위치 센서
    ETHERCAT_SENSOR_VELOCITY_0 = 101,  // Slave 0 속도 센서
    ETHERCAT_SENSOR_TORQUE_0 = 102,    // Slave 0 토크 센서
    ETHERCAT_SENSOR_DI_0 = 110,        // Slave 0 디지털 입력
    ETHERCAT_SENSOR_AI_0 = 120,        // Slave 0 아날로그 입력
    // ... (최대 100개 센서)

    // EtherCAT 모터 명령 (200~299)
    ETHERCAT_MOTOR_CMD_0 = 200,  // Slave 0 모터 명령
    ETHERCAT_MOTOR_CMD_1 = 201,
    // ... (최대 100개 모터)

    // EtherCAT 상태 (300~319)
    ETHERCAT_MASTER_STATE = 300,
    ETHERCAT_LINK_UP = 301,
    ETHERCAT_ERROR_COUNT = 302,
};
```

#### 4.2 VersionedData 저장

**패턴**:
```cpp
// 센서 데이터 읽기 후 DataStore에 저장
PositionSensorData pos = readPositionSensor(slave_0);
rtDataStore->set(DataKey::ETHERCAT_SENSOR_POSITION_0, pos);

// Non-RT에서 읽기
auto versioned = rtDataStore->get<PositionSensorData>(
    DataKey::ETHERCAT_SENSOR_POSITION_0);
if (versioned.isValid() && versioned.data.valid) {
    // 유효한 데이터 사용
}
```

---

## 데이터 플로우

### RT Cycle (10ms)

```
1. EtherCAT Master Receive
   ecrt_master_receive(master) → PDO domain 업데이트

2. Sensor Data 읽기
   domain_data → PositionSensorData, VelocitySensorData, ...
   → RTDataStore::set()

3. 제어 알고리즘 실행
   RTDataStore::get() → 센서 데이터
   → 제어 계산
   → 모터 명령 생성

4. Motor Command 쓰기
   RTDataStore::get<MotorCommand>()
   → domain_data 업데이트

5. EtherCAT Master Send
   ecrt_domain_queue(domain)
   ecrt_master_send(master)
```

### Non-RT 동기화 (100ms)

```
1. RTDataStore → SharedMemory
   RTDataStoreShared::syncToSharedMemory()
   - ETHERCAT_SENSOR_* 키만 선택적 동기화

2. Non-RT 프로세스
   SharedMemory::read() → 센서 데이터
   → 모니터링, 로깅, 시각화

3. Non-RT → RT 명령
   Non-RT에서 모터 명령 생성
   → SharedMemory::write()
   → RTDataStoreShared::syncFromSharedMemory()
```

---

## 관계 다이어그램

```
┌─────────────────────┐
│  EtherCAT Master    │
│  (IgH kernel module)│
└──────────┬──────────┘
           │ PDO exchange (10ms)
           ▼
┌─────────────────────────────────────┐
│  EtherCAT Slaves                    │
│  ┌─────────┐ ┌─────────┐ ┌────────┐│
│  │ Sensor  │ │ Motor   │ │ I/O    ││
│  │ (TxPDO) │ │ (RxPDO) │ │ Module ││
│  └─────────┘ └─────────┘ └────────┘│
└─────────────────────────────────────┘
           │
           ▼
┌─────────────────────────────────────┐
│  SensorDataManager                  │
│  - readPositionSensor()             │
│  - readVelocitySensor()             │
│  - readTorqueSensor()               │
└──────────┬──────────────────────────┘
           │
           ▼
┌─────────────────────────────────────┐
│  RTDataStore                        │
│  (VersionedData<SensorData>)        │
└──────────┬──────────────────────────┘
           │
           ├─> RT Control Algorithm
           │
           ▼
┌─────────────────────────────────────┐
│  MotorCommandManager                │
│  - writeBLDCCommand()               │
│  - writeServoCommand()              │
└──────────┬──────────────────────────┘
           │
           ▼
┌─────────────────────────────────────┐
│  EtherCAT Master                    │
│  (Motor RxPDO transmission)         │
└─────────────────────────────────────┘
```

---

## 타입 안전성

모든 데이터 구조는 다음을 보장합니다:

1. **Compile-time 타입 체크**:
   - `RTDataStore::set<T>()`, `get<T>()` 사용
   - Template 기반 타입 안전성

2. **Runtime 검증**:
   - `valid` 플래그로 데이터 유효성 확인
   - Range check (min/max 범위 검증)

3. **Zero-copy 최적화**:
   - RTDataStore는 shared_ptr 사용하지 않음 (RT 안전)
   - Pre-allocated buffers로 메모리 할당 없음

---

## 메모리 레이아웃

EtherCAT PDO domain은 contiguous memory로 할당됩니다:

```
PDO Domain Memory Layout (예시)
┌──────────────────────────────────────┐
│ Slave 0 Input PDO (센서)             │
│ ┌────────┬────────┬────────┐        │
│ │position│velocity│ torque │ 12B    │
│ └────────┴────────┴────────┘        │
├──────────────────────────────────────┤
│ Slave 1 Output PDO (모터)            │
│ ┌────────┬────────┬────────┐        │
│ │cmd_pos │cmd_vel │cmd_trq │ 12B    │
│ └────────┴────────┴────────┘        │
├──────────────────────────────────────┤
│ Slave 2 I/O Module                   │
│ ┌────────┬────────┐                 │
│ │DI (4B) │AI (8B) │ 12B             │
│ └────────┴────────┘                 │
└──────────────────────────────────────┘
Total: 36 bytes (aligned to 4-byte boundary)
```

**메모리 정렬**:
- 모든 필드는 4-byte aligned (32-bit boundary)
- Padding 최소화로 캐시 효율성 향상

---

## 참고 자료

- [IgH EtherCAT Master Documentation](https://etherlab.org/en/ethercat/)
- [EtherCAT Technology Group: PDO Mapping](https://www.ethercat.org/en/technology.html)
- MXRC RTDataStore 설계: `src/core/datastore/`
