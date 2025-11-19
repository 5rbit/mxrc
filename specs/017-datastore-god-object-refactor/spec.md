# 기능 사양서: DataStore God Object 리팩토링

**기능 브랜치**: `017-datastore-god-object-refactor`
**생성일**: 2025-11-19
**상태**: 초안
**우선순위**: High (ExpirationManager), Medium (기타 관리자)
**입력**: issue/007-datastore-god-object-refactor.md

## 사용자 시나리오 및 테스트 *(필수)*

### 사용자 스토리 1 - ExpirationManager 분리 및 성능 개선 (우선순위: P0 - High)

**As a** 시스템 아키텍트,
**I want** DataStore의 데이터 만료 로직을 ExpirationManager로 분리하고,
**So that** O(N) → O(log N) 성능 개선과 단일 책임 원칙을 준수할 수 있습니다.

**이 우선순위인 이유**:
- 현재 `cleanExpiredData()`는 전체 데이터를 순회하는 O(N) 알고리즘으로, 데이터가 많아질수록 심각한 CPU 스파이크를 유발합니다.
- DataStore의 핵심 책임(키-값 저장)과 무관한 로직이 혼재되어 있어 유지보수성이 낮습니다.

**독립 테스트**:
- `ExpirationManager_test.cpp`에서 만료 정책 적용, 만료 데이터 정리, 성능 벤치마크를 독립적으로 테스트합니다.
- DataStore 통합 테스트에서 ExpirationManager 위임이 올바르게 작동하는지 검증합니다.

**인수 시나리오**:

1. **주어진 상황** ExpirationManager가 1,000개의 만료 정책을 관리하고 있을 때,
   **언제** `cleanExpiredData()`를 호출하면,
   **그러면** O(log N) 시간 복잡도로 만료된 데이터만 효율적으로 제거해야 합니다.

2. **주어진 상황** DataStore에 만료 정책이 설정된 데이터가 저장될 때,
   **언제** 만료 시간이 도래하면,
   **그러면** ExpirationManager가 자동으로 해당 데이터를 정리해야 합니다.

---

### 사용자 스토리 2 - AccessControlManager 분리 (우선순위: P1 - Medium)

**As a** 시스템 아키텍트,
**I want** DataStore의 접근 제어 로직을 AccessControlManager로 분리하고,
**So that** 접근 정책 관리가 독립적으로 테스트 및 확장 가능해집니다.

**이 우선순위인 이유**:
- 접근 제어는 중요한 기능이지만, ExpirationManager만큼 긴급한 성능 문제는 없습니다.
- 단일 책임 원칙 준수를 위해 분리가 필요하지만, 단계적 리팩토링이 가능합니다.

**독립 테스트**:
- `AccessControlManager_test.cpp`에서 접근 정책 설정, 검증, 동시성 테스트를 수행합니다.

**인수 시나리오**:

1. **주어진 상황** AccessControlManager에 읽기 전용 정책이 설정된 키가 있을 때,
   **언제** 해당 키에 쓰기를 시도하면,
   **그러면** 접근 거부 예외를 발생시켜야 합니다.

2. **주어진 상황** 여러 스레드가 동시에 접근 정책을 조회할 때,
   **언제** 정책 조회가 발생하면,
   **그러면** 스레드 안전하게 정책을 반환해야 합니다.

---

### 사용자 스토리 3 - MetricsCollector 분리 (우선순위: P1 - Medium)

**As a** 시스템 아키텍트,
**I want** DataStore의 메트릭 수집 로직을 MetricsCollector로 분리하고,
**So that** 성능 모니터링 로직이 독립적으로 관리되고 확장 가능해집니다.

**이 우선순위인 이유**:
- 메트릭 수집은 부가적인 기능으로, 핵심 비즈니스 로직과 분리되어야 합니다.
- 향후 다양한 메트릭 추가 시 MetricsCollector만 수정하면 되므로 확장성이 향상됩니다.

**독립 테스트**:
- `MetricsCollector_test.cpp`에서 카운터 증가, 메트릭 조회, 동시성 테스트를 수행합니다.

**인수 시나리오**:

1. **주어진 상황** MetricsCollector가 get/set/delete 카운터를 관리하고 있을 때,
   **언제** DataStore 작업이 발생하면,
   **그러면** 해당 카운터가 원자적으로 증가해야 합니다.

2. **주어진 상황** 여러 스레드가 동시에 메트릭을 업데이트할 때,
   **언제** 메트릭 조회를 요청하면,
   **그러면** 정확한 누적 값을 반환해야 합니다.

---

### 사용자 스토리 4 - std::any → std::variant 마이그레이션 (우선순위: P2 - Low)

**As a** 시스템 개발자,
**I want** DataStore의 값 저장을 std::any에서 std::variant로 변경하고,
**So that** 컴파일 타임 타입 안전성이 보장되고 직렬화가 간결해집니다.

**이 우선순위인 이유**:
- 타입 안전성은 중요하지만, 기존 std::any로도 런타임 안정성은 확보되어 있습니다.
- 직렬화 기능 구현 시 std::variant가 유리하므로, 장기적으로 진행합니다.

**독립 테스트**:
- `DataStore_test.cpp`에서 다양한 타입(int, string, double 등)의 저장 및 조회를 테스트합니다.
- std::bad_any_cast 대신 컴파일 타임 오류가 발생하는지 확인합니다.

**인수 시나리오**:

1. **주어진 상황** DataStore에 허용된 타입(int, double, string 등)의 값을 저장할 때,
   **언제** 값을 조회하면,
   **그러면** std::visit를 통해 타입 안전하게 접근할 수 있어야 합니다.

2. **주어진 상황** DataStore에 허용되지 않은 타입을 저장하려고 할 때,
   **언제** 컴파일하면,
   **그러면** 컴파일 타임 오류가 발생해야 합니다.

---

### 엣지 케이스

- **ExpirationManager**: 만료 시간이 과거인 데이터를 저장하면 즉시 제거되는가?
- **AccessControlManager**: 동일한 키에 여러 접근 정책이 중복 설정되면 어떻게 처리하는가?
- **MetricsCollector**: 카운터가 오버플로우되면 어떻게 처리하는가? (std::atomic 사용 시 wrapping 동작)
- **std::variant**: 직렬화되지 않은 타입(예: 사용자 정의 클래스)을 저장하려고 하면?

## 요구사항 *(필수)*

### 기능적 요구사항

- **FR-001**: `ExpirationManager`는 만료 정책 적용(`applyExpirationPolicy`), 제거(`removeExpirationPolicy`), 만료 데이터 정리(`cleanExpiredData`)를 담당해야 합니다.
- **FR-002**: `ExpirationManager`는 O(log N) 시간 복잡도로 만료 데이터를 정리해야 합니다. (std::map<timestamp, key> 활용)
- **FR-003**: `AccessControlManager`는 접근 정책 설정, 검증, 조회를 담당하며, 스레드 안전해야 합니다.
- **FR-004**: `MetricsCollector`는 get/set/delete 카운터를 원자적으로 관리하며, 메트릭 조회 기능을 제공해야 합니다.
- **FR-005**: `DataStore`는 ExpirationManager, AccessControlManager, MetricsCollector에게 관련 작업을 위임하는 퍼사드 역할을 수행해야 합니다.
- **FR-006**: 리팩토링된 DataStore는 기존 public 인터페이스(`get`, `set`, `delete`, `applyExpirationPolicy` 등)를 유지해야 합니다.
- **FR-007**: std::variant 마이그레이션 시, 최소한 int, double, string, bool 타입을 지원해야 합니다.

### 비기능적 요구사항

- **NFR-001**: ExpirationManager의 `cleanExpiredData()`는 1,000개 데이터 기준 1ms 이내에 완료되어야 합니다.
- **NFR-002**: 리팩토링 후 기존 DataStore 테스트가 100% 통과해야 합니다.
- **NFR-003**: 메모리 사용량은 기존 대비 10% 이상 증가해서는 안 됩니다.
- **NFR-004**: 코드 복잡도는 감소해야 하며, DataStore.cpp 파일 크기는 150줄 이하로 축소되어야 합니다.

### 주요 엔티티 *(기능에 데이터가 포함된 경우 포함)*

- **DataStore**: 키-값 저장소의 퍼사드 역할. 관리자들에게 작업을 위임합니다.
- **ExpirationManager**: 만료 정책 관리 및 만료 데이터 정리를 담당합니다.
  - `expiration_map_`: std::map<timestamp, std::set<key>> - 만료 시간 순서로 키를 정렬
  - `key_to_expiration_`: std::unordered_map<key, timestamp> - 키별 만료 시간 조회
- **AccessControlManager**: 접근 제어 정책 관리를 담당합니다.
  - `access_policies_`: std::map<key, AccessPolicy> - 키별 접근 정책
  - `mutex_`: std::shared_mutex - 읽기/쓰기 동시성 제어
- **MetricsCollector**: 성능 메트릭 수집 및 조회를 담당합니다.
  - `get_count_`, `set_count_`, `delete_count_`: std::atomic<uint64_t> - 작업 카운터
  - `memory_usage_`: std::atomic<size_t> - 메모리 사용량 추정치

## 성공 기준 *(필수)*

### 측정 가능한 결과

- **SC-001**: `ExpirationManager_test.cpp`의 모든 테스트가 통과해야 합니다. (최소 10개 테스트)
- **SC-002**: `AccessControlManager_test.cpp`의 모든 테스트가 통과해야 합니다. (최소 8개 테스트)
- **SC-003**: `MetricsCollector_test.cpp`의 모든 테스트가 통과해야 합니다. (최소 6개 테스트)
- **SC-004**: 기존 `DataStore_test.cpp`의 모든 테스트가 리팩토링 후에도 100% 통과해야 합니다.
- **SC-005**: ExpirationManager의 `cleanExpiredData()` 성능 벤치마크가 O(log N) 특성을 보여야 합니다.
  - 100개 데이터: <0.1ms
  - 1,000개 데이터: <1ms
  - 10,000개 데이터: <10ms
- **SC-006**: DataStore.cpp 파일 크기가 150줄 이하로 축소되어야 합니다.
- **SC-007**: AddressSanitizer 메모리 테스트에서 누수가 발생하지 않아야 합니다.

## 설계 제약사항 및 가정 *(선택 사항)*

### 제약사항

- C++20 표준을 사용합니다.
- 기존 DataStore의 public API는 변경하지 않습니다. (하위 호환성 보장)
- TBB concurrent_hash_map을 계속 사용합니다. (020-refactor-datastore-locking과 호환)

### 가정

- ExpirationManager는 주기적으로 호출되는 `cleanExpiredData()`를 통해 만료 데이터를 정리합니다. (백그라운드 스레드는 현재 범위 외)
- std::variant 마이그레이션은 별도의 Phase로 분리 가능하며, 초기 구현에서는 선택적으로 진행합니다.
- 접근 제어 정책은 현재 READ_ONLY/WRITE_ONLY/READ_WRITE 3가지 타입만 지원합니다.

## 범위 밖 사항 *(선택 사항)*

- 백그라운드 스레드를 통한 자동 만료 데이터 정리 (향후 구현)
- 데이터 지속성 기능 (`saveState`, `loadState`) 구현 (별도 이슈로 분리)
- 분산 데이터 저장소 지원
- 트랜잭션 기능

## 참고 자료 *(선택 사항)*

- [issue/007-datastore-god-object-refactor.md](../../issue/007-datastore-god-object-refactor.md)
- [specs/020-refactor-datastore-locking/spec.md](../020-refactor-datastore-locking/spec.md) - 동시성 리팩토링 참조
- [CLAUDE.md](../../CLAUDE.md) - 설계 원칙 (RAII, 단일 책임 원칙)
- C++20 std::variant 문서
- God Object Anti-pattern: https://en.wikipedia.org/wiki/God_object

## 변경 이력

| 날짜 | 작성자 | 변경 내용 |
|------|--------|---------|
| 2025-11-19 | Claude Code | 초안 작성 |
