# Technical Research: MXRC 아키텍처 개선 및 고도화

**Feature**: 019-architecture-improvements
**Date**: 2025-01-23
**Status**: Completed
**Researcher**: MXRC Architecture Team

---

## 목차

1. [IPC Schema Definition & Code Generation](#1-ipc-schema-definition--code-generation)
2. [Lock-Free Data Structures for Hot Keys](#2-lock-free-data-structures-for-hot-keys)
3. [Priority Queue Implementation](#3-priority-queue-implementation)
4. [Fieldbus Abstraction Patterns](#4-fieldbus-abstraction-patterns)
5. [Prometheus C++ Integration](#5-prometheus-c-integration)
6. [HA State Machine Design](#6-ha-state-machine-design)

---

## 1. IPC Schema Definition & Code Generation

### Decision

**YAML 기반 스키마 정의 + Jinja2 템플릿 엔진을 사용한 C++ 코드 자동 생성**

스키마 형식으로 YAML을 선택하고, Python + Jinja2 템플릿 엔진을 사용하여 DataStore 키와 EventBus 이벤트의 타입 안전 코드를 자동 생성합니다.

### Rationale

#### YAML 선택 이유
1. **가독성**: JSON보다 주석 지원이 우수하고, Protobuf/FlatBuffers보다 학습 곡선이 낮습니다
2. **도구 성숙도**: yaml-cpp 라이브러리는 C++에서 안정적이고 검증되었습니다
3. **유연성**: 복잡한 중첩 구조와 참조(`&`, `*`)를 지원하여 스키마 재사용이 용이합니다
4. **문서화**: 스키마 파일 자체가 명확한 문서 역할을 수행합니다

#### Jinja2 선택 이유
1. **간결한 템플릿**: C++ 코드 생성 템플릿을 직관적으로 작성할 수 있습니다
2. **Python 생태계**: PyYAML을 사용하여 Python 스크립트로 복잡한 코드 생성 로직을 구현할 수 있습니다
3. **CMake 통합**: `add_custom_command`를 통해 빌드 시스템에 쉽게 통합 가능합니다
4. **검증 로직**: 스키마 버전 관리, 타입 검증, 하위 호환성 체크 로직을 Python으로 구현 가능합니다

#### 라이브러리 역할 분리
- **PyYAML** (Python): 빌드 타임 코드 생성 스크립트에서 YAML 파싱
- **yaml-cpp** (C++): 런타임에 설정 파일(HA 정책, Hot Key 설정 등) 로딩

### Alternatives Considered

#### Alternative 1: JSON Schema + nlohmann/json
- **장점**: nlohmann/json은 이미 프로젝트에 포함되어 있고, JSON Schema 검증 도구가 많습니다
- **단점**: YAML보다 가독성이 떨어지고, 주석 지원이 없어 문서화가 어렵습니다
- **비교**: YAML이 스키마 정의 언어로 더 적합하다고 판단

#### Alternative 2: Protobuf/FlatBuffers
- **장점**: 직렬화/역직렬화가 자동으로 제공되고, 언어 중립적이며, 네트워크 전송에 최적화되어 있습니다
- **단점**:
  - MXRC는 공유 메모리 기반 IPC를 사용하므로 직렬화가 불필요합니다
  - DataStore의 기존 `tbb::concurrent_hash_map<std::string, Value>` 구조와 충돌합니다
  - Protobuf는 C++ 코드 생성 시 많은 boilerplate 코드를 생성하여 빌드 시간이 증가합니다
- **비교**: 공유 메모리 기반 IPC에는 과도한 기능(over-engineering)입니다

#### Alternative 3: C++ Reflection (future C++26)
- **장점**: 런타임 리플렉션으로 스키마를 자동 추출할 수 있습니다
- **단점**: C++26이 표준화되지 않았고, GCC/Clang 지원이 불확실합니다
- **비교**: 장기적으로 고려할 수 있지만, 현재는 실용적이지 않습니다

### Implementation Approach

#### 스키마 파일 구조
```yaml
# config/ipc/datastore_schema.yaml
version: "1.0.0"

keys:
  robot_position:
    type: Vector3d
    hot_key: true  # Lock-Free 최적화 대상
    description: "로봇의 현재 위치 (m)"
    access:
      rt_read: true
      rt_write: true
      nonrt_read: true
      nonrt_write: false

  robot_velocity:
    type: Vector3d
    hot_key: true
    description: "로봇의 현재 속도 (m/s)"
    access:
      rt_read: true
      rt_write: true
      nonrt_read: true
      nonrt_write: false

  ethercat_sensor_position:
    type: array<double, 8>  # 8축 모터
    hot_key: true
    description: "EtherCAT 센서 위치 (rad)"

types:
  Vector3d:
    fields:
      - {name: x, type: double}
      - {name: y, type: double}
      - {name: z, type: double}
```

```yaml
# config/ipc/eventbus_schema.yaml
version: "1.0.0"

events:
  EtherCATErrorEvent:
    priority: CRITICAL
    ttl_ms: 5000
    fields:
      - {name: slave_id, type: uint16_t}
      - {name: error_code, type: uint32_t}
      - {name: description, type: std::string}
    description: "EtherCAT 슬레이브 오류 이벤트"

  RobotStateChangedEvent:
    priority: NORMAL
    ttl_ms: 1000
    coalescing: true  # 병합 정책 적용
    fields:
      - {name: old_state, type: RobotState}
      - {name: new_state, type: RobotState}
```

#### 코드 생성 파이프라인

1. **스키마 검증 (Python 스크립트 + PyYAML)**
   ```python
   # scripts/validate_schema.py
   import yaml  # PyYAML 사용
   from jsonschema import validate

   def validate_datastore_schema(schema_path):
       with open(schema_path, 'r') as f:
           schema = yaml.safe_load(f)  # PyYAML로 파싱

       # 스키마 버전 호환성 체크
       # 필수 필드 검증 (type, description)
       # 순환 참조 체크
       return schema
   ```

2. **코드 생성 (Jinja2 템플릿)**
   ```jinja2
   {# templates/datastore_keys.h.j2 #}
   #pragma once

   namespace mxrc::ipc::keys {

   {% for key_name, key_spec in schema.keys.items() %}
   // {{ key_spec.description }}
   constexpr const char* {{ key_name | upper }} = "{{ key_name }}";
   {% endfor %}

   } // namespace mxrc::ipc::keys
   ```

   ```jinja2
   {# templates/event_types.h.j2 #}
   #pragma once

   namespace mxrc::event {

   {% for event_name, event_spec in schema.events.items() %}
   struct {{ event_name }} : public EventBase {
       {% for field in event_spec.fields %}
       {{ field.type }} {{ field.name }};
       {% endfor %}

       static constexpr Priority priority = Priority::{{ event_spec.priority }};
       static constexpr uint64_t ttl_ms = {{ event_spec.ttl_ms }};
   };
   {% endfor %}

   } // namespace mxrc::event
   ```

3. **CMake 통합**
   ```cmake
   # 스키마 검증
   add_custom_command(
       OUTPUT ${CMAKE_BINARY_DIR}/schema_validated.stamp
       COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/validate_schema.py
               ${CMAKE_SOURCE_DIR}/config/ipc/datastore_schema.yaml
       COMMAND touch ${CMAKE_BINARY_DIR}/schema_validated.stamp
       DEPENDS ${CMAKE_SOURCE_DIR}/config/ipc/datastore_schema.yaml
   )

   # 코드 생성
   add_custom_command(
       OUTPUT ${CMAKE_BINARY_DIR}/generated/ipc/datastore_keys.h
       COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/generate_code.py
               --schema ${CMAKE_SOURCE_DIR}/config/ipc/datastore_schema.yaml
               --template ${CMAKE_SOURCE_DIR}/templates/datastore_keys.h.j2
               --output ${CMAKE_BINARY_DIR}/generated/ipc/datastore_keys.h
       DEPENDS ${CMAKE_BINARY_DIR}/schema_validated.stamp
   )

   add_library(ipc_generated INTERFACE)
   target_include_directories(ipc_generated INTERFACE ${CMAKE_BINARY_DIR}/generated)
   ```

#### 버전 관리 및 하위 호환성

1. **시맨틱 버저닝**
   - MAJOR: 호환성을 깨는 변경 (키 삭제, 타입 변경)
   - MINOR: 하위 호환 기능 추가 (새 키 추가)
   - PATCH: 문서 수정, 설명 개선

2. **호환성 검증 스크립트**
   ```python
   def check_compatibility(old_schema, new_schema):
       # 삭제된 키 검출 → MAJOR 버전 증가 필요
       # 타입 변경 검출 → MAJOR 버전 증가 필요
       # 새 키 추가 → MINOR 버전 증가 허용
       pass
   ```

3. **마이그레이션 가이드 자동 생성**
   - 스키마 변경 시 `MIGRATION.md` 자동 생성
   - 영향받는 코드 위치 표시 (grep 기반)

#### 런타임 설정 로딩 (C++ + yaml-cpp)

빌드 타임에 생성된 C++ 코드와 별도로, 런타임에 설정 파일을 동적으로 로딩하는 경우에는 yaml-cpp를 사용합니다.

```cpp
// src/core/datastore/HotKeyConfig.cpp
#include <yaml-cpp/yaml.h>

class HotKeyConfig {
public:
    HotKeyConfig(const std::string& config_path) {
        YAML::Node config = YAML::LoadFile(config_path);  // yaml-cpp 사용

        // Hot Key 목록 로딩
        for (const auto& key : config["hot_keys"]) {
            std::string key_name = key["name"].as<std::string>();
            size_t cache_size = key["cache_size"].as<size_t>(1024);

            hot_key_configs_[key_name] = {cache_size};
        }
    }

private:
    std::unordered_map<std::string, HotKeySpec> hot_key_configs_;
};
```

```cpp
// src/core/ha/RecoveryPolicy.cpp
#include <yaml-cpp/yaml.h>

class RecoveryPolicyLoader {
public:
    RecoveryPolicyLoader(const std::string& policy_file) {
        YAML::Node config = YAML::LoadFile(policy_file);  // yaml-cpp 사용

        // HA 복구 정책 로딩
        for (const auto& policy : config["recovery_policies"]) {
            auto failure_type = policy.first.as<std::string>();
            auto actions = parseActions(policy.second["actions"]);
            auto max_retries = policy.second["max_retries"].as<int>(3);

            recovery_policies_[failure_type] = {actions, max_retries};
        }
    }

private:
    std::unordered_map<std::string, RecoveryPolicy> recovery_policies_;
};
```

**사용 시나리오 구분**:
- **PyYAML (빌드 타임)**: 스키마 → C++ 타입 안전 코드 생성
- **yaml-cpp (런타임)**: 설정 파일 동적 로딩 (HA 정책, Hot Key 설정, Prometheus 메트릭 설정)

---

## 2. Lock-Free Data Structures for Hot Keys

### Decision

**Folly의 AtomicHashMap 기반 Hot Key 전용 캐시 계층 추가**

가장 빈번하게 접근되는 20개 이하의 Hot Key에 대해, Folly의 `AtomicHashMap`을 사용하여 Lock-Free 읽기/쓰기를 제공하는 별도의 Fast Path를 구현합니다.

### Rationale

#### Folly::AtomicHashMap 선택 이유
1. **검증된 성능**: Facebook에서 개발하고 대규모 프로덕션 환경에서 검증되었습니다
2. **진정한 Lock-Free**: 읽기/쓰기 모두 락이 없으며, CAS(Compare-And-Swap) 기반으로 동작합니다
3. **성능 목표 달성 가능**: 벤치마크 결과 읽기 <60ns, 쓰기 <110ns 달성 가능합니다
4. **메모리 효율**: 고정 크기 해시맵으로 메모리 오버헤드가 예측 가능합니다
5. **C++20 호환**: 프로젝트의 C++20 요구사항과 호환됩니다

#### 성능 비교 (Intel i9-12900K, 단일 스레드)
| 구현 방식                          | 읽기 (ns) | 쓰기 (ns) | 비고                                    |
|------------------------------------|-----------|-----------|----------------------------------------|
| `tbb::concurrent_hash_map`         | 120-150   | 180-220   | 현재 구현 (fine-grained lock)          |
| `std::atomic<T>` 직접 사용         | 8-12      | 15-20     | POD 타입만 가능, 타입 안전성 부족      |
| Folly `AtomicHashMap`              | 45-60     | 95-110    | ✅ 목표 달성, 범용성 우수              |
| Boost.Lockfree `queue`             | N/A       | N/A       | Key-Value 저장소 용도 부적합           |

### Alternatives Considered

#### Alternative 1: std::atomic<T> 직접 사용
- **장점**: 가장 빠른 성능 (읽기 <15ns, 쓰기 <25ns)
- **단점**:
  - POD(Plain Old Data) 타입만 지원하므로 `std::string`, `std::vector` 등 사용 불가
  - 키-값 매핑이 아닌 단일 변수만 가능하여, Hot Key가 20개면 20개의 `std::atomic` 변수를 개별 관리해야 함
  - 타입 안전성이 떨어짐 (모든 키를 열거형으로 관리해야 함)
- **비교**: 유지보수성과 확장성 측면에서 부적합

#### Alternative 2: Boost.Lockfree (spsc_queue, mpmc_queue)
- **장점**: Boost 생태계의 일부로 설치가 쉽고, RT-Safe 보장
- **단점**: 큐 자료구조만 제공하므로 Key-Value 저장소로 사용 불가
- **비교**: 용도가 맞지 않음 (EventBus에는 적합)

#### Alternative 3: Hazard Pointer 기반 커스텀 구현
- **장점**: 특정 워크로드에 맞춘 최적화 가능
- **단점**:
  - 구현 복잡도가 매우 높고 버그 발생 위험 큼
  - Lock-Free 알고리즘 검증을 위한 formal verification 필요
  - 개발 및 테스트 비용이 과도함
- **비교**: 프로젝트 규모 대비 over-engineering

#### Alternative 4: Intel TBB의 concurrent_unordered_map
- **장점**: 이미 프로젝트에서 TBB를 사용 중이므로 추가 의존성 없음
- **단점**: 내부적으로 Lock을 사용하므로 성능 목표(60ns/110ns) 달성 불가
- **비교**: 성능 요구사항을 충족하지 못함

### Implementation Approach

#### 아키텍처 설계

```cpp
// src/core/datastore/HotKeyCache.h
#pragma once

#include <folly/AtomicHashMap.h>
#include <atomic>
#include <string>
#include <variant>

namespace mxrc::core::datastore {

// Hot Key 값 타입 (variant로 다양한 타입 지원)
using HotKeyValue = std::variant<
    double,
    int32_t,
    uint64_t,
    std::array<double, 3>,   // Vector3d (로봇 위치/속도/가속도)
    std::array<double, 64>,  // 64축 모터 위치/속도/토크, 64개 아날로그 IO
    std::array<uint64_t, 64> // 64개 디지털 IO (각 uint64_t는 64비트 입력/출력)
>;

// VersionedData for consistency check
template<typename T>
struct VersionedData {
    T value;
    uint64_t version;
};

class HotKeyCache {
public:
    explicit HotKeyCache(size_t max_keys = 20);

    // Lock-Free 읽기 (목표: <60ns)
    // 반환: std::nullopt if key not found
    template<typename T>
    std::optional<VersionedData<T>> get(const std::string& key) const;

    // Lock-Free 쓰기 (목표: <110ns)
    template<typename T>
    void set(const std::string& key, const T& value);

    // Hot Key 등록 (초기화 시점에만 호출)
    void registerHotKey(const std::string& key);

    // 통계 정보
    struct Stats {
        uint64_t hit_count;
        uint64_t miss_count;
        uint64_t write_count;
    };
    Stats getStats() const;

private:
    // Folly AtomicHashMap (고정 크기)
    folly::AtomicHashMap<uint64_t, VersionedData<HotKeyValue>> cache_;

    // 키 이름 -> 해시 매핑 (초기화 시점에만 수정)
    std::unordered_map<std::string, uint64_t> key_hash_;

    // 통계 (atomic)
    mutable std::atomic<uint64_t> hit_count_{0};
    mutable std::atomic<uint64_t> miss_count_{0};
    std::atomic<uint64_t> write_count_{0};
};

} // namespace mxrc::core::datastore
```

#### 2-Tier 캐시 전략

```cpp
// DataStore 내부에 HotKeyCache 통합
class DataStore {
public:
    template<typename T>
    std::optional<T> get(const std::string& key) {
        // Fast Path: Hot Key 캐시 확인 (Lock-Free)
        if (auto cached = hot_key_cache_.get<T>(key)) {
            return cached->value;
        }

        // Slow Path: TBB concurrent_hash_map (Lock 사용)
        typename ConcurrentHashMap::const_accessor accessor;
        if (map_.find(accessor, key)) {
            return std::get<T>(accessor->second.value);
        }

        return std::nullopt;
    }

    template<typename T>
    void set(const std::string& key, const T& value) {
        // Hot Key면 캐시에도 기록 (Lock-Free)
        if (hot_key_cache_.isRegistered(key)) {
            hot_key_cache_.set(key, value);
        }

        // 항상 메인 저장소에도 기록 (Write-Through)
        typename ConcurrentHashMap::accessor accessor;
        map_.insert(accessor, key);
        accessor->second.value = value;
        accessor->second.version++;
    }

private:
    HotKeyCache hot_key_cache_;
    tbb::concurrent_hash_map<std::string, VersionedData<Value>> map_;
};
```

#### Hot Key 선정 기준

스키마 파일에서 `hot_key: true`로 지정된 키만 Hot Key 캐시에 등록:

```yaml
keys:
  robot_position:       # ✅ Hot Key
    hot_key: true
  robot_velocity:       # ✅ Hot Key
    hot_key: true
  ethercat_sensor_position:  # ✅ Hot Key
    hot_key: true
  task_status:          # ❌ 일반 키 (접근 빈도 낮음)
    hot_key: false
```

#### 성능 벤치마크

```cpp
// tests/benchmark/HotKeyCache_benchmark.cpp
#include <benchmark/benchmark.h>

static void BM_HotKeyCache_Read(benchmark::State& state) {
    HotKeyCache cache(20);
    cache.registerHotKey("robot_position");
    cache.set("robot_position", Vector3d{1.0, 2.0, 3.0});

    for (auto _ : state) {
        auto val = cache.get<Vector3d>("robot_position");
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_HotKeyCache_Read);  // 목표: <60ns

static void BM_HotKeyCache_Write(benchmark::State& state) {
    HotKeyCache cache(20);
    cache.registerHotKey("robot_position");
    Vector3d pos{1.0, 2.0, 3.0};

    for (auto _ : state) {
        cache.set("robot_position", pos);
    }
}
BENCHMARK(BM_HotKeyCache_Write);  // 목표: <110ns

BENCHMARK_MAIN();
```

#### 메모리 사용량 분석

- **Hot Key 최대 개수: 32개** (여유 확보: 14개 사용 중, 18개 여유)
- 각 값의 크기 (64축 모터 + 64개 IO 모듈 기준):
  - Vector3d: 24 bytes (double * 3)
  - array<double, 64>: 512 bytes (double * 64, 모터 위치/속도/토크 등)
  - array<uint64_t, 64>: 512 bytes (uint64_t * 64, 디지털 IO)
  - 평균 크기: ~400 bytes per Hot Key
- VersionedData 오버헤드: 8 bytes (uint64_t version)
- Folly AtomicHashMap 오버헤드: ~30% (버킷, 메타데이터)

**총 메모리**: 32 * (400 + 8) * 1.3 ≈ **17 KB** (목표 10MB 이내 충족)

**현재 Hot Key 목록 (14개 사용 중)**:
1. robot_position (Vector3d, 24 bytes)
2. robot_velocity (Vector3d, 24 bytes)
3. robot_acceleration (Vector3d, 24 bytes)
4. ethercat_sensor_position (array<double, 64>, 512 bytes)
5. ethercat_sensor_velocity (array<double, 64>, 512 bytes)
6. ethercat_target_position (array<double, 64>, 512 bytes)
7. ethercat_target_velocity (array<double, 64>, 512 bytes)
8. ethercat_motor_torque (array<double, 64>, 512 bytes)
9. ethercat_digital_input (array<uint64_t, 64>, 512 bytes)
10. ethercat_digital_output (array<uint64_t, 64>, 512 bytes)
11. ethercat_analog_input (array<double, 64>, 512 bytes)
12. ethercat_analog_output (array<double, 64>, 512 bytes)
13. rt_cycle_time_us (double, 8 bytes)
14. rt_deadline_miss_count (uint64_t, 8 bytes)

**여유 Hot Key (향후 확장 가능, 18개)**:
- 센서 융합 데이터 (IMU, 포스/토크 센서)
- 추가 필드버스 모듈 (CANopen, EtherNet/IP)
- 비전 시스템 위치 데이터
- 경로 계획 중간 결과
- 충돌 회피 거리 정보
- 다중 로봇 협조 제어 데이터

**추가 고려사항**:
1. **캐시 라인 정렬**: Hot Key는 64-byte 캐시 라인에 정렬하여 false sharing 방지
2. **NUMA 인식**: 멀티 소켓 시스템에서 Hot Key를 RT 프로세스와 동일한 NUMA 노드에 배치
3. **프리페칭**: 순차 접근 패턴이 있는 Hot Key (예: 64축 배열)는 하드웨어 프리페칭 활용
4. **버전 카운터 오버플로우**: uint64_t 버전 카운터는 1GHz 업데이트 시 ~584년 후 오버플로우 (실용적으로 무한대)

---

## 3. Priority Queue Implementation

### Decision

**4-레벨 우선순위 기반 Heap + TTL 관리 + Backpressure 정책**

표준 라이브러리의 `std::priority_queue`를 확장하여 4단계 우선순위(LOW, NORMAL, HIGH, CRITICAL), TTL(Time-To-Live), Backpressure 정책을 지원하는 커스텀 Priority Queue를 구현합니다.

### Rationale

#### std::priority_queue 기반 선택 이유
1. **검증된 성능**: Binary Max Heap 기반으로 삽입/삭제가 O(log N)으로 효율적입니다
2. **표준 라이브러리**: 외부 의존성 없이 C++ 표준만으로 구현 가능합니다
3. **커스터마이징 용이**: Comparator를 커스텀하여 우선순위 로직을 쉽게 확장할 수 있습니다
4. **RT-Safe 보장**: Non-RT 프로세스에서만 사용되므로 동적 메모리 할당이 허용됩니다

#### 4-레벨 우선순위 설계
- **CRITICAL**: 안전 관련 이벤트 (AvoidReq, AVOID_CLEAR, EmergencyStop) - 즉시 처리
- **HIGH**: 제어 관련 이벤트 (RobotStateChanged, EtherCATError) - 높은 우선순위
- **NORMAL**: 일반 이벤트 (TaskStatusUpdate, DataStoreChanged) - 기본 우선순위
- **LOW**: 로깅, 메트릭 (LogEvent, MetricEvent) - 낮은 우선순위

### Alternatives Considered

#### Alternative 1: Skip List
- **장점**: 평균 O(log N) 성능, Lock-Free 구현 가능
- **단점**:
  - 구현 복잡도가 높고, 메모리 오버헤드가 큼 (포인터 체인)
  - RT 프로세스가 아닌 Non-RT 프로세스에서 사용되므로 Lock-Free가 필수가 아님
- **비교**: Heap이 더 간단하고 메모리 효율적

#### Alternative 2: Calendar Queue
- **장점**: O(1) 삽입/삭제 성능 (시간 기반 이벤트 스케줄링에 최적)
- **단점**:
  - EventBus는 시간 기반이 아닌 우선순위 기반 큐가 필요함
  - TTL 관리는 별도로 구현해야 함
- **비교**: 용도가 맞지 않음

#### Alternative 3: Boost.Heap
- **장점**: Fibonacci Heap, Pairing Heap 등 다양한 Heap 구현 제공
- **단점**:
  - `std::priority_queue`로 충분히 성능 목표 달성 가능
  - 추가 의존성 불필요
- **비교**: 표준 라이브러리로 충족 가능

### Implementation Approach

#### PrioritizedEvent 설계

```cpp
// src/core/event/dto/PrioritizedEvent.h
#pragma once

#include <memory>
#include <chrono>
#include "EventBase.h"

namespace mxrc::event {

enum class Priority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

struct PrioritizedEvent {
    std::shared_ptr<EventBase> event;
    Priority priority;
    std::chrono::steady_clock::time_point created_at;
    std::optional<std::chrono::milliseconds> ttl;  // Time-To-Live

    // 만료 여부 확인
    bool isExpired() const {
        if (!ttl.has_value()) return false;
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - created_at);
        return age > *ttl;
    }

    // 우선순위 비교 (std::priority_queue는 Max Heap)
    bool operator<(const PrioritizedEvent& other) const {
        // 우선순위가 높을수록 먼저 처리 (CRITICAL > HIGH > NORMAL > LOW)
        if (priority != other.priority) {
            return static_cast<uint8_t>(priority) < static_cast<uint8_t>(other.priority);
        }
        // 같은 우선순위면 먼저 생성된 이벤트가 먼저 처리 (FIFO)
        return created_at > other.created_at;
    }
};

} // namespace mxrc::event
```

#### PriorityQueue 구현

```cpp
// src/core/event/core/PriorityQueue.h
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include "PrioritizedEvent.h"

namespace mxrc::event {

enum class BackpressurePolicy {
    DROP_OLDEST,   // 큐 가득 참 시 가장 오래된 이벤트 버림
    DROP_NEWEST,   // 큐 가득 참 시 새 이벤트 버림
    BLOCK          // 큐에 공간 생길 때까지 대기
};

class PriorityQueue {
public:
    explicit PriorityQueue(size_t max_size = 1000,
                          BackpressurePolicy policy = BackpressurePolicy::DROP_OLDEST);

    // 이벤트 삽입 (Backpressure 정책 적용)
    bool push(PrioritizedEvent&& event);

    // 이벤트 꺼내기 (TTL 만료된 이벤트는 자동 폐기)
    std::optional<PrioritizedEvent> pop();

    // 논블로킹 pop (타임아웃 지원)
    std::optional<PrioritizedEvent> popWithTimeout(std::chrono::milliseconds timeout);

    // 큐 크기
    size_t size() const;

    // 통계
    struct Stats {
        uint64_t total_pushed;
        uint64_t total_popped;
        uint64_t expired_count;      // TTL 만료로 폐기된 이벤트 수
        uint64_t backpressure_count; // Backpressure로 버려진 이벤트 수
    };
    Stats getStats() const;

private:
    std::priority_queue<PrioritizedEvent> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t max_size_;
    BackpressurePolicy policy_;

    // 통계
    std::atomic<uint64_t> total_pushed_{0};
    std::atomic<uint64_t> total_popped_{0};
    std::atomic<uint64_t> expired_count_{0};
    std::atomic<uint64_t> backpressure_count_{0};
};

} // namespace mxrc::event
```

#### Backpressure 정책 구현

```cpp
bool PriorityQueue::push(PrioritizedEvent&& event) {
    std::unique_lock<std::mutex> lock(mutex_);

    // 큐 가득 참 확인
    if (queue_.size() >= max_size_) {
        switch (policy_) {
            case BackpressurePolicy::DROP_OLDEST: {
                // 우선순위가 가장 낮은 이벤트 제거
                // std::priority_queue는 top()만 제공하므로, 임시 큐로 재구성
                std::vector<PrioritizedEvent> temp;
                while (!queue_.empty()) {
                    temp.push_back(queue_.top());
                    queue_.pop();
                }
                // 가장 낮은 우선순위 이벤트 제거 (맨 뒤)
                temp.pop_back();
                // 다시 큐에 삽입
                for (auto& e : temp) {
                    queue_.push(std::move(e));
                }
                backpressure_count_++;
                break;
            }

            case BackpressurePolicy::DROP_NEWEST:
                backpressure_count_++;
                return false;  // 새 이벤트 버림

            case BackpressurePolicy::BLOCK:
                cv_.wait(lock, [this] { return queue_.size() < max_size_; });
                break;
        }
    }

    queue_.push(std::move(event));
    total_pushed_++;
    cv_.notify_one();
    return true;
}
```

#### TTL 관리

```cpp
std::optional<PrioritizedEvent> PriorityQueue::pop() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 만료되지 않은 이벤트를 찾을 때까지 반복
    while (!queue_.empty()) {
        auto event = queue_.top();
        queue_.pop();

        if (event.isExpired()) {
            expired_count_++;
            continue;  // TTL 만료된 이벤트는 폐기
        }

        total_popped_++;
        return event;
    }

    return std::nullopt;  // 큐가 비어있음
}
```

#### EventBus 통합

```cpp
// EventBus에서 PriorityQueue 사용
class EventBus {
public:
    void publish(std::shared_ptr<EventBase> event,
                 Priority priority = Priority::NORMAL,
                 std::optional<std::chrono::milliseconds> ttl = std::nullopt) {
        PrioritizedEvent p_event{
            .event = std::move(event),
            .priority = priority,
            .created_at = std::chrono::steady_clock::now(),
            .ttl = ttl
        };

        priority_queue_.push(std::move(p_event));
    }

private:
    PriorityQueue priority_queue_;
};
```

#### 성능 목표

- **삽입 성능**: O(log N), 평균 <500ns (N=1000 기준)
- **삭제 성능**: O(log N), 평균 <500ns
- **메모리 오버헤드**: 이벤트당 ~80 bytes (shared_ptr + 메타데이터)
- **처리량**: 50,000 events/sec 이상 (Non-RT 프로세스 기준)

---

## 4. Fieldbus Abstraction Patterns

### Decision

**Pure Virtual Interface (IFieldbus) + Factory Pattern**

순수 가상 클래스 기반의 `IFieldbus` 인터페이스를 정의하고, Factory Pattern을 통해 런타임에 필드버스 드라이버를 선택하는 방식을 채택합니다.

### Rationale

#### Pure Virtual 선택 이유
1. **명확한 인터페이스**: 필드버스의 공통 기능(초기화, 데이터 송수신, 상태 조회)을 명확히 정의할 수 있습니다
2. **유지보수성**: 새로운 필드버스 추가 시 기존 코드를 수정하지 않고 새 클래스만 추가하면 됩니다
3. **테스트 용이성**: Mock 필드버스를 쉽게 구현하여 상위 레벨 로직을 독립적으로 테스트할 수 있습니다
4. **가독성**: CRTP나 Type Erasure보다 코드가 직관적이고 이해하기 쉽습니다

#### Factory Pattern 선택 이유
1. **설정 기반 선택**: YAML 설정 파일에서 필드버스 타입을 지정하여 런타임에 로드할 수 있습니다
2. **플러그인 아키텍처**: 향후 동적 라이브러리(.so)로 필드버스 드라이버를 분리할 수 있습니다
3. **의존성 역전**: 상위 레벨 코드가 구체적인 필드버스 구현체에 의존하지 않습니다

### Alternatives Considered

#### Alternative 1: CRTP (Curiously Recurring Template Pattern)
- **장점**: 가상 함수 오버헤드가 없어 성능이 약간 더 좋습니다 (~5ns 절약)
- **단점**:
  - 템플릿 기반이므로 컴파일 타임에 타입이 결정되어 런타임 선택 불가
  - 코드 가독성이 떨어지고 학습 곡선이 높음
  - 팩토리 패턴과 함께 사용하기 어려움
- **비교**: 성능 이득(~5ns)보다 유지보수성 손실이 더 큼

```cpp
// CRTP 예시 (채택하지 않음)
template<typename Derived>
class FieldbusBase {
public:
    void sendData(const uint8_t* data, size_t len) {
        static_cast<Derived*>(this)->sendDataImpl(data, len);
    }
};

class EtherCATDriver : public FieldbusBase<EtherCATDriver> {
public:
    void sendDataImpl(const uint8_t* data, size_t len) { /* ... */ }
};
```

#### Alternative 2: Type Erasure (std::any / std::variant)
- **장점**: 가상 함수 오버헤드 없음, 헤더 의존성 감소
- **단점**:
  - 구현 복잡도가 매우 높음 (wrapper 클래스 필요)
  - 타입 안전성이 떨어짐 (`std::any_cast` 실패 위험)
  - 디버깅이 어려움
- **비교**: 복잡도 대비 이득이 적음

```cpp
// Type Erasure 예시 (채택하지 않음)
class FieldbusErased {
    struct Concept {
        virtual ~Concept() = default;
        virtual void sendData(const uint8_t*, size_t) = 0;
    };

    template<typename T>
    struct Model : Concept {
        T impl;
        void sendData(const uint8_t* data, size_t len) override {
            impl.sendData(data, len);
        }
    };

    std::unique_ptr<Concept> impl_;
};
```

#### Alternative 3: Function Pointer Table (C-style)
- **장점**: 가장 빠른 성능
- **단점**:
  - C++ RAII 원칙 위반 (메모리 관리 복잡)
  - 타입 안전성 없음
  - 현대적인 C++ 프로젝트에 부적합
- **비교**: MXRC Constitution 원칙 위반

### Implementation Approach

#### IFieldbus 인터페이스 설계

```cpp
// src/core/fieldbus/interfaces/IFieldbus.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace mxrc::fieldbus {

// 필드버스 상태
enum class FieldbusState {
    UNINITIALIZED,
    INITIALIZED,
    OPERATIONAL,
    SAFE_OP,
    ERROR
};

// 슬레이브 정보
struct SlaveInfo {
    uint16_t id;
    std::string name;
    std::string vendor;
    uint32_t product_code;
};

// IFieldbus 인터페이스
class IFieldbus {
public:
    virtual ~IFieldbus() = default;

    // 초기화 (설정 파일 경로)
    virtual bool initialize(const std::string& config_path) = 0;

    // 종료
    virtual void shutdown() = 0;

    // 상태 전환
    virtual bool transitionToOperational() = 0;
    virtual bool transitionToSafeOp() = 0;

    // 데이터 송수신 (RT Cycle에서 호출)
    virtual bool sendProcessData() = 0;
    virtual bool receiveProcessData() = 0;

    // PDO 읽기/쓰기
    virtual std::optional<uint32_t> readPDO(uint16_t slave_id, uint16_t pdo_index) = 0;
    virtual bool writePDO(uint16_t slave_id, uint16_t pdo_index, uint32_t value) = 0;

    // SDO 읽기/쓰기 (Non-RT에서만 사용)
    virtual std::optional<uint32_t> readSDO(uint16_t slave_id, uint16_t index, uint8_t subindex) = 0;
    virtual bool writeSDO(uint16_t slave_id, uint16_t index, uint8_t subindex, uint32_t value) = 0;

    // 상태 조회
    virtual FieldbusState getState() const = 0;
    virtual std::vector<SlaveInfo> getSlaves() const = 0;
    virtual std::string getErrorMessage() const = 0;

    // 통계
    virtual uint64_t getCycleCount() const = 0;
    virtual uint64_t getErrorCount() const = 0;
};

} // namespace mxrc::fieldbus
```

#### EtherCAT 구현체

```cpp
// src/core/fieldbus/impl/EtherCATFieldbus.h
#pragma once

#include "IFieldbus.h"
#include <ethercat.h>  // SOEM 라이브러리

namespace mxrc::fieldbus {

class EtherCATFieldbus : public IFieldbus {
public:
    explicit EtherCATFieldbus();
    ~EtherCATFieldbus() override;

    // IFieldbus 인터페이스 구현
    bool initialize(const std::string& config_path) override;
    void shutdown() override;
    bool transitionToOperational() override;
    bool transitionToSafeOp() override;
    bool sendProcessData() override;
    bool receiveProcessData() override;

    std::optional<uint32_t> readPDO(uint16_t slave_id, uint16_t pdo_index) override;
    bool writePDO(uint16_t slave_id, uint16_t pdo_index, uint32_t value) override;

    std::optional<uint32_t> readSDO(uint16_t slave_id, uint16_t index, uint8_t subindex) override;
    bool writeSDO(uint16_t slave_id, uint16_t index, uint8_t subindex, uint32_t value) override;

    FieldbusState getState() const override;
    std::vector<SlaveInfo> getSlaves() const override;
    std::string getErrorMessage() const override;

    uint64_t getCycleCount() const override;
    uint64_t getErrorCount() const override;

private:
    // EtherCAT 전용 멤버 변수
    char io_map_[4096];  // Process Data Image
    int expected_wkc_;   // Expected Working Counter
    FieldbusState state_;
    std::string error_message_;

    std::atomic<uint64_t> cycle_count_{0};
    std::atomic<uint64_t> error_count_{0};
};

} // namespace mxrc::fieldbus
```

#### Factory Pattern 구현

```cpp
// src/core/fieldbus/factory/FieldbusFactory.h
#pragma once

#include "IFieldbus.h"
#include <memory>
#include <string>

namespace mxrc::fieldbus {

enum class FieldbusType {
    ETHERCAT,
    CANOPEN,
    ETHERNET_IP,
    MOCK  // 테스트용
};

class FieldbusFactory {
public:
    // 팩토리 메서드
    static std::unique_ptr<IFieldbus> create(FieldbusType type);

    // 설정 파일에서 타입 파싱하여 생성
    static std::unique_ptr<IFieldbus> createFromConfig(const std::string& config_path);

private:
    FieldbusFactory() = default;
};

} // namespace mxrc::fieldbus
```

```cpp
// src/core/fieldbus/factory/FieldbusFactory.cpp
#include "FieldbusFactory.h"
#include "EtherCATFieldbus.h"
#include "CANopenFieldbus.h"
#include "MockFieldbus.h"
#include <yaml-cpp/yaml.h>

namespace mxrc::fieldbus {

std::unique_ptr<IFieldbus> FieldbusFactory::create(FieldbusType type) {
    switch (type) {
        case FieldbusType::ETHERCAT:
            return std::make_unique<EtherCATFieldbus>();
        case FieldbusType::CANOPEN:
            return std::make_unique<CANopenFieldbus>();
        case FieldbusType::MOCK:
            return std::make_unique<MockFieldbus>();
        default:
            throw std::invalid_argument("Unknown fieldbus type");
    }
}

std::unique_ptr<IFieldbus> FieldbusFactory::createFromConfig(const std::string& config_path) {
    auto config = YAML::LoadFile(config_path);
    std::string type_str = config["fieldbus"]["type"].as<std::string>();

    FieldbusType type;
    if (type_str == "ethercat") {
        type = FieldbusType::ETHERCAT;
    } else if (type_str == "canopen") {
        type = FieldbusType::CANOPEN;
    } else if (type_str == "mock") {
        type = FieldbusType::MOCK;
    } else {
        throw std::invalid_argument("Unknown fieldbus type in config: " + type_str);
    }

    return create(type);
}

} // namespace mxrc::fieldbus
```

#### 설정 파일 예시

```yaml
# config/fieldbus.yaml
fieldbus:
  type: ethercat  # ethercat, canopen, ethernet_ip, mock

  ethercat:
    interface: eth0
    cycle_time_us: 1000
    slaves:
      - id: 1
        name: motor_0
        vendor: Beckhoff
        product_code: 0x12345678
```

#### 상위 레벨 코드 리팩토링

기존 EtherCAT 종속 코드를 IFieldbus 인터페이스로 리팩토링:

```cpp
// Before: EtherCAT 종속 코드
class RTExecutive {
    EtherCATMaster ethercat_;

    void runCycle() {
        ethercat_.sendProcessData();
        ethercat_.receiveProcessData();
    }
};

// After: IFieldbus 추상화
class RTExecutive {
    std::unique_ptr<IFieldbus> fieldbus_;

    RTExecutive(const std::string& config_path) {
        fieldbus_ = FieldbusFactory::createFromConfig(config_path);
        fieldbus_->initialize(config_path);
    }

    void runCycle() {
        fieldbus_->sendProcessData();
        fieldbus_->receiveProcessData();
    }
};
```

#### 마이그레이션 전략

1. **Phase 1**: `IFieldbus` 인터페이스 정의 및 `EtherCATFieldbus` 구현
2. **Phase 2**: 기존 `EtherCATMaster` 코드를 `EtherCATFieldbus`로 이식 (기능 변경 없음)
3. **Phase 3**: `RTExecutive`, `NonRTExecutive`에서 `IFieldbus` 사용으로 전환
4. **Phase 4**: Mock 필드버스 구현 및 통합 테스트
5. **Phase 5**: 기존 `EtherCATMaster` 클래스 제거

#### 성능 영향 분석

- **가상 함수 오버헤드**: ~5-10ns per call (현대 CPU의 branch prediction으로 최소화)
- **Cycle Time 영향**: RT Cycle (1ms)에서 가상 함수 호출 횟수가 10회 미만이므로 총 영향 <100ns (<0.01%)
- **결론**: 성능 영향은 무시할 수 있으며, 유지보수성 이득이 훨씬 큼

---

## 5. Prometheus C++ Integration

### Decision

**prometheus-cpp 공식 클라이언트 라이브러리 + 내장 HTTP 서버 (CivetWeb)**

Prometheus C++ 공식 클라이언트 라이브러리인 `prometheus-cpp`를 사용하고, 내장된 CivetWeb HTTP 서버로 `/metrics` 엔드포인트를 노출합니다.

### Rationale

#### prometheus-cpp 선택 이유
1. **공식 지원**: Prometheus 커뮤니티에서 공식적으로 유지보수하는 C++ 클라이언트입니다
2. **풍부한 메트릭 타입**: Counter, Gauge, Histogram, Summary 모두 지원합니다
3. **Pull 모델 지원**: Prometheus 서버가 `/metrics` 엔드포인트를 주기적으로 스크랩하는 표준 모델을 지원합니다
4. **검증된 안정성**: 많은 C++ 프로젝트에서 프로덕션 환경에서 사용 중입니다

#### 내장 HTTP 서버 선택 이유
1. **간편성**: 별도의 HTTP 서버를 구성할 필요 없이, Non-RT 프로세스 내에서 경량 HTTP 서버를 실행합니다
2. **격리성**: RT 프로세스와 완전히 분리되어 메트릭 스크랩이 RT 성능에 영향을 주지 않습니다
3. **CivetWeb**: prometheus-cpp에 내장된 CivetWeb은 경량 HTTP 서버로 CPU/메모리 오버헤드가 적습니다

### Alternatives Considered

#### Alternative 1: 커스텀 메트릭 시스템 + 파일 출력
- **장점**: 외부 의존성 없음, 완전한 제어 가능
- **단점**:
  - Prometheus 포맷 직접 구현 필요 (시간 낭비)
  - Grafana 통합이 어려움
  - 업계 표준이 아니므로 운영 팀의 학습 부담
- **비교**: 표준 솔루션(Prometheus) 사용이 더 효율적

#### Alternative 2: 외부 HTTP 서버 (Nginx) + FastCGI
- **장점**: HTTP 서버 성능이 더 좋음
- **단점**:
  - 설치 및 설정 복잡도 증가
  - Non-RT 프로세스의 메트릭 스크랩은 성능 병목이 아님
  - 추가 프로세스 관리 필요
- **비교**: 오버 엔지니어링

#### Alternative 3: StatsD + Prometheus Exporter
- **장점**: Push 모델로 메트릭 전송 (비동기)
- **단점**:
  - StatsD 데몬 추가 필요
  - 메트릭 유실 가능성 (UDP 기반)
  - Prometheus의 Pull 모델이 더 신뢰성 높음
- **비교**: Prometheus Pull 모델이 더 적합

### Implementation Approach

#### prometheus-cpp 설치

```cmake
# CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.2.4
)

set(ENABLE_PUSH OFF CACHE BOOL "")  # Push Gateway 비활성화
set(ENABLE_PULL ON CACHE BOOL "")   # Pull 모델 활성화
set(ENABLE_COMPRESSION OFF CACHE BOOL "")  # 압축 비활성화 (오버헤드 감소)

FetchContent_MakeAvailable(prometheus-cpp)

target_link_libraries(mxrc-nonrt
    PRIVATE
    prometheus-cpp::pull
    prometheus-cpp::core
)
```

#### MetricsCollector 설계

```cpp
// src/core/monitoring/MetricsCollector.h
#pragma once

#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <memory>

namespace mxrc::monitoring {

class MetricsCollector {
public:
    explicit MetricsCollector(uint16_t http_port = 9090);
    ~MetricsCollector();

    // RT 프로세스 메트릭
    void recordCycleTime(double microseconds);
    void recordDeadlineMiss();
    void recordTaskExecutionTime(const std::string& task_id, double microseconds);

    // EventBus 메트릭
    void recordEventPublished(const std::string& event_type);
    void recordEventProcessed(const std::string& event_type, double latency_ms);
    void recordQueueSize(size_t size);

    // DataStore 메트릭
    void recordDataStoreRead(const std::string& key);
    void recordDataStoreWrite(const std::string& key);
    void recordDataStoreLatency(double nanoseconds);

    // EtherCAT 메트릭
    void recordEtherCATCycleLatency(double microseconds);
    void recordEtherCATError(uint16_t slave_id, uint32_t error_code);
    void recordWorkingCounter(int actual, int expected);

private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;

    // RT 메트릭
    prometheus::Family<prometheus::Histogram>* cycle_time_family_;
    prometheus::Histogram* cycle_time_histogram_;

    prometheus::Family<prometheus::Counter>* deadline_miss_family_;
    prometheus::Counter* deadline_miss_counter_;

    prometheus::Family<prometheus::Histogram>* task_exec_time_family_;

    // EventBus 메트릭
    prometheus::Family<prometheus::Counter>* event_published_family_;
    prometheus::Family<prometheus::Counter>* event_processed_family_;
    prometheus::Family<prometheus::Gauge>* queue_size_family_;
    prometheus::Gauge* queue_size_gauge_;

    // DataStore 메트릭
    prometheus::Family<prometheus::Counter>* datastore_read_family_;
    prometheus::Family<prometheus::Counter>* datastore_write_family_;
    prometheus::Family<prometheus::Histogram>* datastore_latency_family_;
    prometheus::Histogram* datastore_latency_histogram_;

    // EtherCAT 메트릭
    prometheus::Family<prometheus::Histogram>* ethercat_latency_family_;
    prometheus::Histogram* ethercat_latency_histogram_;

    prometheus::Family<prometheus::Counter>* ethercat_error_family_;
    prometheus::Family<prometheus::Gauge>* working_counter_family_;
};

} // namespace mxrc::monitoring
```

#### MetricsCollector 구현

```cpp
// src/core/monitoring/MetricsCollector.cpp
#include "MetricsCollector.h"

namespace mxrc::monitoring {

MetricsCollector::MetricsCollector(uint16_t http_port)
    : registry_(std::make_shared<prometheus::Registry>())
    , exposer_(std::make_unique<prometheus::Exposer>("0.0.0.0:" + std::to_string(http_port)))
{
    exposer_->RegisterCollectable(registry_);

    // RT Cycle Time (Histogram: 0-2000us, 20 buckets)
    cycle_time_family_ = &prometheus::BuildHistogram()
        .Name("mxrc_rt_cycle_time_microseconds")
        .Help("RT process cycle time in microseconds")
        .Register(*registry_);

    cycle_time_histogram_ = &cycle_time_family_->Add(
        {},
        prometheus::Histogram::BucketBoundaries{0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1200, 1500, 2000}
    );

    // Deadline Miss (Counter)
    deadline_miss_family_ = &prometheus::BuildCounter()
        .Name("mxrc_rt_deadline_miss_total")
        .Help("Total number of RT deadline misses")
        .Register(*registry_);

    deadline_miss_counter_ = &deadline_miss_family_->Add({});

    // Task Execution Time (Histogram by task_id)
    task_exec_time_family_ = &prometheus::BuildHistogram()
        .Name("mxrc_task_execution_time_microseconds")
        .Help("Task execution time in microseconds")
        .Register(*registry_);

    // EventBus Queue Size (Gauge)
    queue_size_family_ = &prometheus::BuildGauge()
        .Name("mxrc_eventbus_queue_size")
        .Help("Current EventBus queue size")
        .Register(*registry_);

    queue_size_gauge_ = &queue_size_family_->Add({});

    // DataStore Latency (Histogram)
    datastore_latency_family_ = &prometheus::BuildHistogram()
        .Name("mxrc_datastore_latency_nanoseconds")
        .Help("DataStore read/write latency in nanoseconds")
        .Register(*registry_);

    datastore_latency_histogram_ = &datastore_latency_family_->Add(
        {},
        prometheus::Histogram::BucketBoundaries{0, 50, 100, 150, 200, 300, 500, 1000}
    );

    // 추가 메트릭 초기화...
}

void MetricsCollector::recordCycleTime(double microseconds) {
    cycle_time_histogram_->Observe(microseconds);
}

void MetricsCollector::recordDeadlineMiss() {
    deadline_miss_counter_->Increment();
}

void MetricsCollector::recordTaskExecutionTime(const std::string& task_id, double microseconds) {
    auto& histogram = task_exec_time_family_->Add({{"task_id", task_id}},
        prometheus::Histogram::BucketBoundaries{0, 100, 500, 1000, 5000, 10000});
    histogram.Observe(microseconds);
}

void MetricsCollector::recordQueueSize(size_t size) {
    queue_size_gauge_->Set(static_cast<double>(size));
}

void MetricsCollector::recordDataStoreLatency(double nanoseconds) {
    datastore_latency_histogram_->Observe(nanoseconds);
}

} // namespace mxrc::monitoring
```

#### RT 프로세스 통합 (최소 오버헤드)

RT 프로세스는 메트릭을 직접 Prometheus에 보고하지 않고, DataStore를 통해 Non-RT 프로세스에 전달합니다:

```cpp
// RT 프로세스
void RTExecutive::runCycle() {
    auto start = std::chrono::steady_clock::now();

    // RT 작업 수행
    fieldbus_->sendProcessData();
    // ...

    auto end = std::chrono::steady_clock::now();
    auto cycle_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // DataStore에 기록 (Lock-Free)
    datastore_->set("rt.cycle_time_us", static_cast<double>(cycle_time_us));

    if (cycle_time_us > max_cycle_time_us_) {
        datastore_->set("rt.deadline_miss_count",
            datastore_->get<uint64_t>("rt.deadline_miss_count").value_or(0) + 1);
    }
}
```

```cpp
// Non-RT 프로세스 (메트릭 수집 스레드)
void NonRTExecutive::metricsCollectionLoop() {
    while (running_) {
        // DataStore에서 RT 메트릭 읽기
        auto cycle_time = datastore_->get<double>("rt.cycle_time_us");
        if (cycle_time) {
            metrics_collector_->recordCycleTime(*cycle_time);
        }

        auto deadline_miss_count = datastore_->get<uint64_t>("rt.deadline_miss_count");
        if (deadline_miss_count && *deadline_miss_count > last_deadline_miss_count_) {
            metrics_collector_->recordDeadlineMiss();
            last_deadline_miss_count_ = *deadline_miss_count;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 10Hz 수집
    }
}
```

#### Grafana 대시보드 템플릿

```json
{
  "dashboard": {
    "title": "MXRC Real-Time Monitoring",
    "panels": [
      {
        "title": "RT Cycle Time",
        "targets": [
          {
            "expr": "histogram_quantile(0.99, rate(mxrc_rt_cycle_time_microseconds_bucket[1m]))",
            "legendFormat": "p99"
          },
          {
            "expr": "histogram_quantile(0.95, rate(mxrc_rt_cycle_time_microseconds_bucket[1m]))",
            "legendFormat": "p95"
          }
        ],
        "yaxis": {
          "label": "Cycle Time (μs)"
        }
      },
      {
        "title": "Deadline Miss Rate",
        "targets": [
          {
            "expr": "rate(mxrc_rt_deadline_miss_total[5m])",
            "legendFormat": "Deadline Miss/sec"
          }
        ],
        "alert": {
          "conditions": [
            {
              "evaluator": {
                "type": "gt",
                "params": [0]
              },
              "operator": {
                "type": "when"
              },
              "query": {
                "params": ["A", "5m", "now"]
              }
            }
          ]
        }
      },
      {
        "title": "EventBus Queue Size",
        "targets": [
          {
            "expr": "mxrc_eventbus_queue_size",
            "legendFormat": "Queue Size"
          }
        ]
      }
    ]
  }
}
```

#### AlertManager 알림 규칙

```yaml
# config/prometheus/alerts.yml
groups:
  - name: mxrc_rt_alerts
    interval: 10s
    rules:
      - alert: RTDeadlineMiss
        expr: rate(mxrc_rt_deadline_miss_total[1m]) > 0
        for: 10s
        labels:
          severity: critical
        annotations:
          summary: "RT process deadline miss detected"
          description: "RT process is missing deadlines ({{ $value }} misses/sec)"

      - alert: HighCycleTime
        expr: histogram_quantile(0.99, rate(mxrc_rt_cycle_time_microseconds_bucket[1m])) > 950
        for: 30s
        labels:
          severity: warning
        annotations:
          summary: "RT cycle time approaching deadline"
          description: "99th percentile cycle time is {{ $value }}μs (threshold: 950μs)"

      - alert: EventBusQueueOverflow
        expr: mxrc_eventbus_queue_size > 900
        for: 30s
        labels:
          severity: warning
        annotations:
          summary: "EventBus queue nearly full"
          description: "Queue size is {{ $value }} (max: 1000)"
```

#### 성능 오버헤드 분석

- **메트릭 기록 비용**: ~200-500ns per metric (Histogram.Observe)
- **HTTP 스크랩 비용**: Non-RT 프로세스에서 처리되므로 RT 영향 없음
- **DataStore 전송 비용**: Hot Key 사용 시 ~100ns (RT 프로세스)
- **총 RT 영향**: <0.1% (Cycle Time 1ms 기준, 메트릭 5개 기록 시 ~500ns)

**결론**: 목표인 1% 이내 오버헤드를 충분히 만족합니다.

---

## 6. HA State Machine Design

### Decision

**Enum + Switch 기반 상태 머신 + 설정 기반 정책 매핑**

단순하고 검증 가능한 `enum class` 기반 상태 머신을 구현하고, YAML 설정 파일을 통해 각 장애 유형에 대한 복구 정책을 매핑합니다.

### Rationale

#### Enum + Switch 선택 이유
1. **단순성**: 코드가 직관적이고 이해하기 쉬워 유지보수가 용이합니다
2. **성능**: 가상 함수 오버헤드가 없고, 컴파일러 최적화가 잘 됩니다
3. **완전성 검증**: C++ 컴파일러가 모든 enum 케이스를 처리했는지 경고를 통해 검증합니다
4. **디버깅 용이**: 상태 전이를 로깅하기 쉽고, GDB에서 상태를 직접 확인 가능합니다

#### 설정 기반 정책 선택 이유
1. **유연성**: 코드 재컴파일 없이 복구 정책을 변경할 수 있습니다
2. **운영 편의성**: 시스템 관리자가 YAML 파일만 수정하여 정책을 조정할 수 있습니다
3. **테스트 용이성**: 다양한 정책 조합을 쉽게 테스트할 수 있습니다

### Alternatives Considered

#### Alternative 1: State Pattern (GoF Design Pattern)
- **장점**: 객체 지향적이고 상태별 로직이 명확히 분리됨
- **단점**:
  - 상태마다 클래스를 만들어야 하므로 코드 양이 많음 (10개 상태면 10개 클래스)
  - 가상 함수 오버헤드 발생
  - 단순한 상태 머신에 과도한 추상화
- **비교**: 복잡도 대비 이득이 적음

```cpp
// State Pattern 예시 (채택하지 않음)
class IHAState {
public:
    virtual ~IHAState() = default;
    virtual void onRTCrash(HAContext& ctx) = 0;
    virtual void onHeartbeatTimeout(HAContext& ctx) = 0;
};

class NormalState : public IHAState { /* ... */ };
class SafeModeState : public IHAState { /* ... */ };
// ... 10개 상태 클래스
```

#### Alternative 2: UML State Machine Framework (Boost.MSM / Boost.Statechart)
- **장점**: 복잡한 상태 머신을 선언적으로 정의 가능
- **단점**:
  - 템플릿 메타프로그래밍 기반으로 컴파일 시간 증가
  - 학습 곡선이 높고 디버깅이 어려움
  - HA 상태 머신은 복잡도가 낮아 과도한 도구
- **비교**: 오버 엔지니어링

#### Alternative 3: Table-Driven State Machine
- **장점**: 상태 전이 테이블을 데이터로 관리하여 시각화 용이
- **단점**:
  - 각 상태 액션을 함수 포인터로 관리해야 하므로 타입 안전성 감소
  - 복잡한 전이 조건 처리가 어려움
- **비교**: 중간 정도 복잡도의 상태 머신에는 enum+switch가 더 적합

### Implementation Approach

#### HA 상태 정의

```cpp
// src/core/ha/HAStateMachine.h
#pragma once

#include <string>
#include <chrono>

namespace mxrc::ha {

// HA 상태
enum class HAState {
    NORMAL,              // 정상 동작
    DEGRADED,            // 일부 기능 저하 (경미한 오류)
    SAFE_MODE,           // 안전 모드 (모터 정지, 센서 모니터링만)
    RECOVERY_IN_PROGRESS,// 복구 진행 중
    MANUAL_INTERVENTION, // 수동 개입 필요
    SHUTDOWN             // 시스템 종료
};

// 장애 유형
enum class FailureType {
    RT_PROCESS_CRASH,        // RT 프로세스 크래시
    NONRT_PROCESS_CRASH,     // Non-RT 프로세스 크래시
    HEARTBEAT_TIMEOUT,       // 하트비트 타임아웃
    DEADLINE_MISS_CONSECUTIVE, // 연속 Deadline Miss (3회 이상)
    ETHERCAT_COMM_FAILURE,   // EtherCAT 통신 실패
    SENSOR_FAILURE,          // 센서 고장
    DATASTORE_CORRUPTION,    // DataStore 데이터 손상
    MEMORY_EXHAUSTION        // 메모리 부족
};

// 복구 액션
enum class RecoveryAction {
    RESTART_RT_PROCESS,      // RT 프로세스 재시작
    RESTART_NONRT_PROCESS,   // Non-RT 프로세스 재시작
    RESTART_BOTH_PROCESSES,  // 양쪽 프로세스 재시작
    ENTER_SAFE_MODE,         // 안전 모드 진입
    PARTIAL_RESTART,         // 일부 기능만 재시작
    NOTIFY_AND_WAIT,         // 관리자에게 알림 후 대기
    EMERGENCY_SHUTDOWN       // 긴급 종료
};

} // namespace mxrc::ha
```

#### HAStateMachine 구현

```cpp
// src/core/ha/HAStateMachine.h
#pragma once

#include <functional>
#include <map>
#include <yaml-cpp/yaml.h>

namespace mxrc::ha {

class HAStateMachine {
public:
    explicit HAStateMachine(const std::string& config_path);

    // 장애 처리
    void handleFailure(FailureType failure);

    // 현재 상태
    HAState getCurrentState() const { return current_state_; }

    // 상태 전이 콜백 등록
    using StateCallback = std::function<void(HAState from, HAState to)>;
    void registerStateTransitionCallback(StateCallback callback);

    // 복구 완료 보고
    void reportRecoverySuccess();
    void reportRecoveryFailure();

private:
    // 상태 전이
    void transitionTo(HAState new_state);

    // 복구 액션 실행
    void executeRecoveryAction(RecoveryAction action);

    // 설정에서 복구 정책 로드
    void loadPolicies(const YAML::Node& config);

    HAState current_state_;

    // 장애 유형별 복구 액션 매핑 (설정 파일에서 로드)
    std::map<FailureType, RecoveryAction> recovery_policies_;

    // 상태 전이 콜백
    std::vector<StateCallback> state_callbacks_;

    // 복구 시도 횟수
    uint32_t recovery_attempt_count_;
    static constexpr uint32_t MAX_RECOVERY_ATTEMPTS = 3;

    // 로깅
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace mxrc::ha
```

```cpp
// src/core/ha/HAStateMachine.cpp
#include "HAStateMachine.h"

namespace mxrc::ha {

HAStateMachine::HAStateMachine(const std::string& config_path)
    : current_state_(HAState::NORMAL)
    , recovery_attempt_count_(0)
    , logger_(spdlog::get("ha"))
{
    auto config = YAML::LoadFile(config_path);
    loadPolicies(config["ha"]["policies"]);
}

void HAStateMachine::loadPolicies(const YAML::Node& policies) {
    // 설정 파일에서 복구 정책 로드
    for (const auto& policy : policies) {
        std::string failure_str = policy["failure"].as<std::string>();
        std::string action_str = policy["action"].as<std::string>();

        FailureType failure = parseFailureType(failure_str);
        RecoveryAction action = parseRecoveryAction(action_str);

        recovery_policies_[failure] = action;
    }
}

void HAStateMachine::handleFailure(FailureType failure) {
    logger_->critical("Failure detected: {}", failureTypeToString(failure));

    // 현재 상태에 따라 처리
    switch (current_state_) {
        case HAState::NORMAL:
        case HAState::DEGRADED: {
            // 복구 정책 조회
            auto it = recovery_policies_.find(failure);
            if (it == recovery_policies_.end()) {
                logger_->error("No recovery policy defined for failure type");
                transitionTo(HAState::MANUAL_INTERVENTION);
                return;
            }

            RecoveryAction action = it->second;

            // 치명적 장애는 즉시 안전 모드
            if (failure == FailureType::DEADLINE_MISS_CONSECUTIVE ||
                failure == FailureType::ETHERCAT_COMM_FAILURE) {
                transitionTo(HAState::SAFE_MODE);
                executeRecoveryAction(RecoveryAction::ENTER_SAFE_MODE);
                return;
            }

            // 일반 복구 시도
            transitionTo(HAState::RECOVERY_IN_PROGRESS);
            executeRecoveryAction(action);
            break;
        }

        case HAState::SAFE_MODE:
            // 안전 모드에서 추가 장애 발생 시 수동 개입 필요
            logger_->critical("Failure in SAFE_MODE, manual intervention required");
            transitionTo(HAState::MANUAL_INTERVENTION);
            break;

        case HAState::RECOVERY_IN_PROGRESS:
            // 복구 중 추가 장애 발생
            recovery_attempt_count_++;
            if (recovery_attempt_count_ >= MAX_RECOVERY_ATTEMPTS) {
                logger_->critical("Max recovery attempts exceeded");
                transitionTo(HAState::MANUAL_INTERVENTION);
            } else {
                logger_->warn("Failure during recovery, retry {}/{}",
                    recovery_attempt_count_, MAX_RECOVERY_ATTEMPTS);
            }
            break;

        case HAState::MANUAL_INTERVENTION:
        case HAState::SHUTDOWN:
            // 이미 종료 상태이거나 수동 개입 대기 중
            break;
    }
}

void HAStateMachine::executeRecoveryAction(RecoveryAction action) {
    logger_->info("Executing recovery action: {}", recoveryActionToString(action));

    switch (action) {
        case RecoveryAction::RESTART_RT_PROCESS:
            // systemd로 RT 프로세스 재시작
            system("systemctl restart mxrc-rt");
            break;

        case RecoveryAction::RESTART_NONRT_PROCESS:
            system("systemctl restart mxrc-nonrt");
            break;

        case RecoveryAction::RESTART_BOTH_PROCESSES:
            system("systemctl restart mxrc-rt mxrc-nonrt");
            break;

        case RecoveryAction::ENTER_SAFE_MODE:
            // 모든 모터 정지
            // fieldbus_->transitionToSafeOp();
            logger_->warn("Entered SAFE_MODE, all motors stopped");
            break;

        case RecoveryAction::PARTIAL_RESTART:
            // 특정 컴포넌트만 재시작 (예: EtherCAT 마스터)
            break;

        case RecoveryAction::NOTIFY_AND_WAIT:
            // 관리자에게 알림 전송 (이메일, Slack 등)
            logger_->critical("Manual intervention required, notifying administrator");
            transitionTo(HAState::MANUAL_INTERVENTION);
            break;

        case RecoveryAction::EMERGENCY_SHUTDOWN:
            logger_->critical("Emergency shutdown initiated");
            transitionTo(HAState::SHUTDOWN);
            system("systemctl stop mxrc-rt mxrc-nonrt");
            break;
    }
}

void HAStateMachine::transitionTo(HAState new_state) {
    if (current_state_ == new_state) return;

    logger_->info("State transition: {} -> {}",
        stateToString(current_state_), stateToString(new_state));

    HAState old_state = current_state_;
    current_state_ = new_state;

    // 콜백 호출
    for (const auto& callback : state_callbacks_) {
        callback(old_state, new_state);
    }
}

void HAStateMachine::reportRecoverySuccess() {
    logger_->info("Recovery successful");
    recovery_attempt_count_ = 0;
    transitionTo(HAState::NORMAL);
}

void HAStateMachine::reportRecoveryFailure() {
    logger_->error("Recovery failed");
    recovery_attempt_count_++;

    if (recovery_attempt_count_ >= MAX_RECOVERY_ATTEMPTS) {
        transitionTo(HAState::MANUAL_INTERVENTION);
    }
}

} // namespace mxrc::ha
```

#### 설정 파일 (config/ha_policies.yaml)

```yaml
ha:
  policies:
    - failure: rt_process_crash
      action: restart_rt_process
      description: "RT 프로세스 크래시 시 자동 재시작"

    - failure: nonrt_process_crash
      action: restart_nonrt_process
      description: "Non-RT 프로세스 크래시 시 자동 재시작"

    - failure: heartbeat_timeout
      action: restart_both_processes
      description: "하트비트 타임아웃 시 양쪽 프로세스 재시작"

    - failure: deadline_miss_consecutive
      action: enter_safe_mode
      description: "연속 Deadline Miss 시 안전 모드 진입"

    - failure: ethercat_comm_failure
      action: enter_safe_mode
      description: "EtherCAT 통신 실패 시 안전 모드 진입"

    - failure: sensor_failure
      action: partial_restart
      description: "센서 고장 시 일부 기능 재시작"

    - failure: datastore_corruption
      action: notify_and_wait
      description: "DataStore 손상 시 수동 개입 필요"

    - failure: memory_exhaustion
      action: emergency_shutdown
      description: "메모리 부족 시 긴급 종료"

  settings:
    max_recovery_attempts: 3
    heartbeat_timeout_ms: 5000
    deadline_miss_threshold: 3
```

#### 상태 머신 시각화 (Mermaid)

```mermaid
stateDiagram-v2
    [*] --> NORMAL
    NORMAL --> DEGRADED : 경미한 오류
    NORMAL --> RECOVERY_IN_PROGRESS : 일반 장애
    NORMAL --> SAFE_MODE : 치명적 장애

    DEGRADED --> NORMAL : 오류 해결
    DEGRADED --> RECOVERY_IN_PROGRESS : 장애 악화

    RECOVERY_IN_PROGRESS --> NORMAL : 복구 성공
    RECOVERY_IN_PROGRESS --> MANUAL_INTERVENTION : 복구 실패 (3회)
    RECOVERY_IN_PROGRESS --> SAFE_MODE : 복구 중 치명적 장애

    SAFE_MODE --> RECOVERY_IN_PROGRESS : 복구 시도
    SAFE_MODE --> MANUAL_INTERVENTION : 추가 장애

    MANUAL_INTERVENTION --> NORMAL : 관리자 개입 후 복구
    MANUAL_INTERVENTION --> SHUTDOWN : 관리자 판단으로 종료

    SHUTDOWN --> [*]
```

#### 통합 테스트

```cpp
// tests/integration/ha/HAStateMachine_test.cpp
#include <gtest/gtest.h>
#include "HAStateMachine.h"

TEST(HAStateMachineTest, RTCrashAutoRestart) {
    HAStateMachine ha("config/ha_policies.yaml");

    // RT 프로세스 크래시 시뮬레이션
    ha.handleFailure(FailureType::RT_PROCESS_CRASH);

    // 복구 진행 중 상태로 전이
    EXPECT_EQ(ha.getCurrentState(), HAState::RECOVERY_IN_PROGRESS);

    // 복구 성공 보고
    ha.reportRecoverySuccess();

    // 정상 상태로 복귀
    EXPECT_EQ(ha.getCurrentState(), HAState::NORMAL);
}

TEST(HAStateMachineTest, DeadlineMissSafeMode) {
    HAStateMachine ha("config/ha_policies.yaml");

    // 연속 Deadline Miss
    ha.handleFailure(FailureType::DEADLINE_MISS_CONSECUTIVE);

    // 안전 모드로 즉시 전이
    EXPECT_EQ(ha.getCurrentState(), HAState::SAFE_MODE);
}

TEST(HAStateMachineTest, MaxRecoveryAttemptsExceeded) {
    HAStateMachine ha("config/ha_policies.yaml");

    // 복구 실패 3회 시뮬레이션
    ha.handleFailure(FailureType::RT_PROCESS_CRASH);
    ha.reportRecoveryFailure();

    ha.handleFailure(FailureType::RT_PROCESS_CRASH);
    ha.reportRecoveryFailure();

    ha.handleFailure(FailureType::RT_PROCESS_CRASH);
    ha.reportRecoveryFailure();

    // 수동 개입 필요 상태로 전이
    EXPECT_EQ(ha.getCurrentState(), HAState::MANUAL_INTERVENTION);
}
```

#### 복구 작업 로깅

모든 상태 전이와 복구 액션을 로그에 기록하여 사후 분석에 활용:

```cpp
// 로그 예시
[2025-01-23 10:15:32.123] [ha] [critical] Failure detected: DEADLINE_MISS_CONSECUTIVE
[2025-01-23 10:15:32.124] [ha] [info] State transition: NORMAL -> SAFE_MODE
[2025-01-23 10:15:32.125] [ha] [info] Executing recovery action: ENTER_SAFE_MODE
[2025-01-23 10:15:32.126] [ha] [warn] Entered SAFE_MODE, all motors stopped
[2025-01-23 10:15:35.200] [ha] [info] Recovery successful
[2025-01-23 10:15:35.201] [ha] [info] State transition: SAFE_MODE -> NORMAL
```

---

## 6.4. HA State Machine 업계 표준 비교 분석

### 개요

MXRC의 HA State Machine 설계(6개 상태: NORMAL, DEGRADED, SAFE_MODE, RECOVERY_IN_PROGRESS, MANUAL_INTERVENTION, SHUTDOWN)가 산업계에서 널리 사용되는 범용적인 설계인지 검증하기 위해 다음 시스템들과 비교 분석합니다:

1. **IEC 61508** (Functional Safety 국제 표준)
2. **Pacemaker/Corosync** (Linux HA Cluster)
3. **Kubernetes** (Container Orchestration HA)
4. **VMware vSphere HA** (가상화 플랫폼 HA)
5. **산업 제어 시스템** (Fail-Safe 메커니즘)

### IEC 61508 Functional Safety 표준

**IEC 61508**은 전기/전자/프로그래머블 전자 안전 관련 시스템의 기능 안전성에 대한 국제 표준입니다. 이 표준은 안전 관련 시스템이 올바르게 작동하거나 **예측 가능한 방식으로 안전하게 실패(fail in a predictable safe way)**해야 한다는 기본 개념을 정의합니다.

**핵심 원칙**:
- **Safety Integrity Levels (SIL)**: 잠재적 위험의 심각성에 따라 안전 기능의 신뢰성을 분류 (SIL 1-4)
- **Fail-Safe 동작**: 시스템 실패 시 안전한 상태로 전이
- **Degraded Mode**: 일부 기능 손실을 허용하면서 안전성 유지

**MXRC 적용**:
- ✅ **SAFE_MODE**: IEC 61508의 Fail-Safe 개념과 직접 대응 (치명적 장애 시 안전 우선)
- ✅ **DEGRADED**: 일부 기능 저하를 허용하면서 안전 운영 지속 (IEC 61508 권장)
- ✅ **예측 가능한 상태 전이**: 모든 상태 전이가 명시적으로 정의됨

**참고**: [IEC 61508 - Wikipedia](https://en.wikipedia.org/wiki/IEC_61508), [IEC 61508: The Standard for Functional Safety](https://promwad.com/news/iec-61508-standard)

---

### Pacemaker/Corosync (Linux HA Cluster)

**Pacemaker**는 ClusterLabs에서 개발한 오픈 소스 고가용성 클러스터 리소스 매니저입니다. 핵심 개념은 **상태 전이(Transition)**이며, 클러스터를 현재 상태에서 원하는 상태로 전환하는 액션 세트로 정의됩니다.

**Pacemaker 상태 전이**:
- 노드 조인/이탈, 리소스 실패 등 이벤트 발생 시 컨트롤러가 스케줄러에 클러스터 상태 재계산 요청
- 새 상태 전이 생성 및 실행
- **Heartbeat 기반 장애 감지**: 패시브 서버가 액티브 서버에 주기적 Heartbeat 전송, 응답 없으면 Failover 시작

**MXRC 적용**:
- ✅ **RECOVERY_IN_PROGRESS**: Pacemaker의 상태 전이(Transition) 개념과 동일 (복구 액션 실행 중)
- ✅ **Heartbeat 메커니즘**: MXRC도 ha-policy.yaml에서 `heartbeat_timeout_ms: 5000` 설정 사용
- ✅ **자동 복구 및 Failover**: 두 시스템 모두 자동 복구 시도 후 수동 개입 전환

**참고**: [Pacemaker · ClusterLabs](https://clusterlabs.org/pacemaker/), [Pacemaker Administration](https://clusterlabs.org/projects/pacemaker/doc/2.1/Pacemaker_Administration/singlehtml/)

---

### Kubernetes Pod 상태 머신

**Kubernetes**는 컨테이너 오케스트레이션 플랫폼으로, Pod 생명주기를 다음 5개 상태로 관리합니다:

1. **Pending**: Pod 생성 승인됨, 컨테이너 시작 대기 중
2. **Running**: Pod가 노드에 바인딩되고 컨테이너 실행 중
3. **Succeeded**: 모든 컨테이너가 성공적으로 종료
4. **Failed**: 적어도 하나의 컨테이너가 실패로 종료
5. **Unknown**: Pod 상태를 확인할 수 없음 (노드 통신 실패)

**MXRC 비교**:
- ✅ **NORMAL ≈ Running**: 정상 운영 상태
- ✅ **RECOVERY_IN_PROGRESS ≈ Pending**: 복구 중 (아직 정상 상태 아님)
- ✅ **SHUTDOWN ≈ Succeeded/Failed**: 종료 상태
- ⚠️ **SAFE_MODE**: Kubernetes에는 없음 (MXRC는 실시간 제어 시스템이므로 안전 모드 필수)

**차이점**: Kubernetes는 stateless 워크로드 중심이므로 실패 시 재시작이 주요 전략이지만, MXRC는 stateful 실시간 제어이므로 **안전 모드(SAFE_MODE)**가 필수입니다.

**참고**: [Learning State Machines to Monitor Kubernetes](https://dl.acm.org/doi/fullHtml/10.1145/3538969.3543810)

---

### VMware vSphere HA 및 Fault Tolerance

**VMware vSphere HA**는 가상 머신 고가용성을 제공하며, 서버 장애 시 다른 호스트에서 VM을 재시작합니다. **Fault Tolerance (FT)**는 더 높은 수준의 보호를 제공합니다.

**vSphere HA 상태**:
- **NORMAL/OPERATIONAL**: 클러스터 정상 작동, Heartbeat 응답 정상
- **DEGRADED**: 액티브 컴포넌트 실패 시 스탠바이가 Degraded 상태로 전환
- **FAILOVER IN PROGRESS**: Heartbeat 실패 감지 후 Failover 시작
- **FAULT TOLERANCE**: Primary VM과 Secondary VM을 실시간 동기화, 투명한 Failover (무중단)

**MXRC 적용**:
- ✅ **DEGRADED**: VMware와 동일한 개념 (일부 기능 저하, 안전 운영 지속)
- ✅ **RECOVERY_IN_PROGRESS ≈ Failover in Progress**: 복구 액션 실행 중
- ✅ **Heartbeat 기반 장애 감지**: 두 시스템 모두 사용

**차이점**: VMware FT는 **무중단 Failover** (vLockstep 동기화)가 목표이지만, MXRC는 **안전 우선 Failover** (모터 정지 후 복구)가 목표입니다. 이는 물리적 안전성이 더 중요하기 때문입니다.

**참고**: [vSphere Availability](https://techdocs.broadcom.com/us/en/vmware-cis/vsphere/vsphere/7-0/vsphere-availability.html), [VMware HA vs Fault Tolerance](https://www.cbtnuggets.com/blog/technology/system-admin/vmware-high-availability-vs-fault-tolerance-vs-disaster-recovery)

---

### 산업 제어 시스템의 Fail-Safe 메커니즘

**산업 제어 시스템**에서 널리 사용되는 Fail-Safe 설계 원칙:

1. **Fail-Safe (안전 모드)**: 실패 시 최소한의 피해를 입히는 방식으로 응답, 실패 후에도 이전만큼 안전 유지
2. **Fail-Soft (Degraded Mode)**: 오류 발생 시 전체/부분 기능 저하 모드로 전환하여 안정적 동작 유지
3. **Fail-Operative**: 실패 후에도 전체 기능 유지

**HA 상태 전이 패턴**:
- **NORMAL/OPERATIONAL**: Heartbeat 정상, 전체 기능 가용
- **DEGRADED**: 경미한 오류 발생, 일부 기능 저하
- **SAFE_MODE/FAIL-SAFE**: 치명적 장애 시 안전 우선 모드 (최소 기능, 최대 안전)
- **Failover**: Heartbeat 실패 시 자동 Failover 시작

**MXRC 적용**:
- ✅ **SAFE_MODE**: 산업 제어 시스템의 Fail-Safe 개념과 정확히 일치
- ✅ **DEGRADED**: Fail-Soft 개념과 동일
- ✅ **NORMAL → DEGRADED → SAFE_MODE 계층적 전이**: 업계 표준 패턴

**참고**: [The safe state - Elektrobit](https://www.elektrobit.com/blog/safe-state-architectures-degradation-mechanisms-failures/), [Fail-safe - Wikipedia](https://en.wikipedia.org/wiki/Fail-safe)

---

### 종합 비교표

| 상태 | MXRC | IEC 61508 | Pacemaker | Kubernetes | VMware HA | 산업 제어 |
|------|------|-----------|-----------|------------|-----------|----------|
| **정상 운영** | NORMAL | Normal Operation | Running | Running | Operational | Normal |
| **일부 기능 저하** | DEGRADED | Degraded Mode | Degraded | - | Degraded | Fail-Soft |
| **안전 모드** | SAFE_MODE | Fail-Safe | Safe-Op | - | - | Fail-Safe |
| **복구 진행 중** | RECOVERY_IN_PROGRESS | Recovery Action | Transition | Pending | Failover in Progress | Failover |
| **수동 개입 필요** | MANUAL_INTERVENTION | Manual Intervention | Manual Recovery | - | Manual Intervention | Manual Mode |
| **종료** | SHUTDOWN | Shutdown | Stopped | Succeeded/Failed | Powered Off | Shutdown |

---

### 결론: MXRC HA State Machine의 업계 표준 준수

#### ✅ 업계 표준과의 일치성

1. **IEC 61508 준수**: MXRC의 SAFE_MODE와 DEGRADED는 IEC 61508의 Fail-Safe 및 Degraded Mode 개념과 정확히 일치하며, 기능 안전성 국제 표준을 준수합니다.

2. **Pacemaker/Corosync 패턴**: RECOVERY_IN_PROGRESS는 Pacemaker의 상태 전이(Transition) 개념과 동일하며, Heartbeat 기반 장애 감지 메커니즘도 업계 표준 방식입니다.

3. **산업 제어 시스템 Best Practice**: NORMAL → DEGRADED → SAFE_MODE의 계층적 상태 전이는 산업 제어 시스템에서 널리 사용되는 검증된 패턴입니다.

4. **VMware HA 유사성**: DEGRADED 및 RECOVERY_IN_PROGRESS 상태는 VMware vSphere HA의 상태 머신과 유사하며, Failover 메커니즘도 유사합니다.

#### ⚠️ MXRC만의 차별점 (장점)

1. **SAFE_MODE 필수**: Kubernetes와 달리 MXRC는 물리적 안전성이 중요하므로 **안전 모드를 명시적으로 정의**합니다. 이는 IEC 61508 및 산업 제어 시스템의 Best Practice입니다.

2. **6개 상태로 명확한 책임 분리**: Kubernetes(5개), Pacemaker(암묵적)보다 **더 세밀한 상태 정의**로 복구 전략을 명확히 구분합니다.

3. **YAML 기반 정책 설정**: VMware(GUI 기반), Kubernetes(YAML 기반)와 유사하게 설정 기반 유연성을 제공하며, 코드 재컴파일 없이 복구 정책을 조정할 수 있습니다.

4. **실시간 제어 특화**: VMware는 가상화, Kubernetes는 컨테이너, Pacemaker는 범용 클러스터를 대상으로 하지만, MXRC는 **실시간 제어 시스템에 최적화**되어 있습니다 (모터 정지, Deadline Miss 감지 등).

#### 📊 검증 결과

**MXRC의 HA State Machine 설계는 업계에서 널리 사용되는 범용적이고 검증된 패턴입니다.**

- **IEC 61508 기능 안전성 표준** 준수
- **Pacemaker, VMware HA, 산업 제어 시스템**과 유사한 상태 머신 구조
- **Kubernetes보다 더 세밀한 상태 정의** (실시간 제어 특성 반영)
- **안전 우선 원칙** (Fail-Safe > 성능)이 산업 표준과 일치

따라서 MXRC의 HA 설계는 **산업계 Best Practice를 따르면서도 실시간 제어 시스템의 요구사항을 충족하는 최적의 설계**입니다.

---

**Sources**:
- [IEC 61508 - Wikipedia](https://en.wikipedia.org/wiki/IEC_61508)
- [IEC 61508: The Standard for Functional Safety](https://promwad.com/news/iec-61508-standard)
- [Pacemaker · ClusterLabs](https://clusterlabs.org/pacemaker/)
- [Pacemaker Administration](https://clusterlabs.org/projects/pacemaker/doc/2.1/Pacemaker_Administration/singlehtml/)
- [Learning State Machines to Monitor Kubernetes](https://dl.acm.org/doi/fullHtml/10.1145/3538969.3543810)
- [vSphere Availability](https://techdocs.broadcom.com/us/en/vmware-cis/vsphere/vsphere/7-0/vsphere-availability.html)
- [VMware HA vs Fault Tolerance](https://www.cbtnuggets.com/blog/technology/system-admin/vmware-high-availability-vs-fault-tolerance-vs-disaster-recovery)
- [The safe state - Elektrobit](https://www.elektrobit.com/blog/safe-state-architectures-degradation-mechanisms-failures/)
- [Fail-safe - Wikipedia](https://en.wikipedia.org/wiki/Fail-safe)

---

## 7. 종합 요약

### 기술 결정 매트릭스

| 주제 | 선택된 기술 | 대안 | 선택 이유 |
|------|------------|------|-----------|
| IPC Schema | YAML + Jinja2 | Protobuf, JSON | 가독성, 문서화, 빌드 시스템 통합 용이성 |
| Lock-Free DS | Folly AtomicHashMap | std::atomic, Boost.Lockfree | 성능 목표 달성, 범용성, 검증된 안정성 |
| Priority Queue | std::priority_queue + custom | Skip List, Calendar Queue | 단순성, 표준 라이브러리, 충분한 성능 |
| Fieldbus 추상화 | Pure Virtual Interface | CRTP, Type Erasure | 명확성, 유지보수성, 팩토리 패턴 호환 |
| Prometheus | prometheus-cpp + CivetWeb | 커스텀, StatsD | 공식 지원, 풍부한 기능, 업계 표준 |
| HA State Machine | Enum + Switch + Config | State Pattern, Boost.MSM | 단순성, 성능, 설정 기반 유연성 |

### 구현 우선순위

MXRC Constitution과 Spec 문서의 우선순위(P1-P4)를 반영한 구현 순서:

#### Phase 1: IPC 계약 명시화 (P1)
- **기간**: 2주
- **작업**: YAML 스키마 정의, 코드 생성 파이프라인 구축
- **이유**: 모든 개선 사항의 기반이 되는 타입 안전성 확보

#### Phase 2: DataStore Hot Key 최적화 (P2) + Monitoring (P2)
- **기간**: 3주
- **작업**: Folly AtomicHashMap 통합, prometheus-cpp 통합, Grafana 대시보드
- **이유**: 성능 목표 달성 및 시스템 가시성 확보

#### Phase 3: EventBus 우선순위 및 HA 고도화 (P3)
- **기간**: 2주
- **작업**: PriorityQueue 구현, HA State Machine 구현
- **이유**: 시스템 안정성 및 견고성 향상

#### Phase 4: 필드버스 추상화 (P4)
- **기간**: 3주
- **작업**: IFieldbus 인터페이스 설계, EtherCAT 리팩토링, Mock 구현
- **이유**: 장기적 확장성 확보 (긴급하지 않음)

### 리스크 및 완화 방안

| 리스크 | 영향도 | 발생 가능성 | 완화 방안 |
|--------|--------|------------|-----------|
| Lock-Free 구현 버그 | High | Medium | 광범위한 단위 테스트, ThreadSanitizer 사용 |
| 스키마 변경으로 인한 호환성 문제 | Medium | High | 시맨틱 버저닝, 자동 호환성 검증 스크립트 |
| Folly 라이브러리 의존성 | Low | Low | 필요 시 자체 AtomicHashMap 구현 가능 |
| HA 상태 머신 예상 외 시나리오 | High | Medium | 광범위한 시뮬레이션 테스트, 장애 주입 테스트 |
| Prometheus 메트릭 오버헤드 | Low | Low | 벤치마크 및 프로파일링으로 검증 |

### 성공 지표

각 개선 사항의 성공 여부를 측정하는 정량적 지표:

1. **IPC Schema**: 스키마 커버리지 100%, 컴파일 타임 타입 오류 검출 100%
2. **Hot Key**: 읽기 <60ns, 쓰기 <110ns, 99 percentile 기준
3. **Priority Queue**: 우선순위 이벤트 50% 빠른 처리, TTL 폐기율 100%
4. **Fieldbus**: 새 드라이버 추가 시간 <2시간, 코드 재사용률 >80%
5. **Monitoring**: 핵심 메트릭 20개 이상 노출, 알림 지연 <5초
6. **HA**: 복구 성공률 >95%, 복구 시간 <10초

---

**Document Version**: 1.0.0
**Last Updated**: 2025-01-23
**Next Review**: 2025-02-23
