## 🔴 이슈 #002: DataStore 전역 락으로 인한 심각한 성능 병목

**날짜**: 2025-11-16
**심각도**: Critical
**브랜치**: `feature/020-*` (추정)
**사양서**: `/spec/020-*` (추가예정)
**상태**: ✅ 해결됨 (Resolved)

### 문제 증상

- `DataStoreEventAdapter`의 동시성 테스트 (`ConcurrentEventBusPublish`, `ConcurrentDataStoreUpdates`) 실행 시 타임아웃 발생.
- 여러 스레드가 동시에 `DataStore`에 데이터를 쓸 때 시스템 응답성이 현저히 저하됨.
- 이벤트 버스가 많은 수의 데이터 저장 이벤트를 처리할 때 전체 이벤트 처리가 지연됨.

### 근본 원인 분석

**원인**: `DataStore` 모듈이 모든 `set` 연산에 단일 전역 `static` 뮤텍스를 사용합니다.

1.  **설계 문제: 과도한 범위의 락 (Coarse-Grained Locking)**
    - `src/core/datastore/DataStore.h`에 정의된 `DataStore::set` 메서드는 연산을 시작할 때 `DataStore::mutex_`를 잠급니다.
    - 이 뮤텍스는 `DataStore`의 모든 인스턴스와 모든 키(key)에 대해 공유되는 단일 락입니다.
    - 따라서 서로 다른 키에 대한 동시 쓰기 작업조차도 직렬로 실행되어야만 합니다.

2.  **병목 현상 발생**
    - 동시성 테스트에서 여러 스레드가 `dataStore->set()`을 호출하면, 이들은 전역 락을 획득하기 위해 경쟁합니다.
    - 결과적으로 병렬 실행의 이점을 전혀 얻지 못하고, 모든 쓰기 작업이 순차적으로 처리됩니다. 이는 테스트의 타임아웃 임계값을 초과하는 원인이 됩니다.

3.  **아키텍처 전체로의 문제 확산**
    - `EventBus`는 단일 디스패치 스레드(`dispatchThread_`)를 사용하여 이벤트를 처리합니다.
    - `DataStoreEventAdapter`와 같은 이벤트 핸들러가 블로킹되는 `DataStore::set`을 호출하면, `EventBus`의 전체 디스패치 루프가 해당 작업이 끝날 때까지 멈춥니다.
    - 이는 `DataStore`의 병목 현상이 이벤트 처리 시스템 전체의 안정성을 저해하는 단일 실패 지점(Single Point of Failure)이 되게 만듭니다.

### 해결 방법 (권장)

**핵심 전략**: `DataStore`의 락 메커니즘을 전역 락에서 세분화된 락(Fine-Grained Locking)으로 리팩토링해야 합니다.

#### 제안 1: 키 기반 락 (Per-Key Locking)

- `DataStore` 내부에 `std::map<std::string, std::mutex>`와 같은 구조를 두어 각 키별로 뮤텍스를 관리합니다.
- `set` 또는 `get` 연산 시, 대상 키에 해당하는 뮤텍스만 잠급니다.
- **장점**: 서로 다른 키에 대한 접근은 완전히 병렬로 처리됩니다. 구현이 비교적 직관적입니다.
- **단점**: 키가 동적으로 생성되고 삭제될 때 뮤텍스 맵을 관리하는 오버헤드가 발생할 수 있습니다.

#### 제안 2: 동시성 해시 맵 사용 (Concurrent Hash Map)

- `DataStore`의 내부 저장소를 `tbb::concurrent_hash_map` (Intel TBB) 또는 유사한 라이브러리의 동시성 맵으로 교체합니다.
- 이 자료구조들은 내부적으로 세분화된 락을 사용하여 높은 수준의 동시성을 제공합니다.
- **장점**: 스레드 안전성이 자료구조 수준에서 보장되므로 코드가 단순해집니다. 일반적으로 성능이 매우 우수합니다.
- **단점**: 외부 라이브러리(TBB 등)에 대한 의존성이 추가됩니다.

### 검증 전략

- 수정된 `DataStore`를 사용하여 `DataStoreEventAdapter_test.cpp`의 모든 동시성 테스트를 실행하고 타임아웃 없이 통과하는지 확인합니다.
- 쓰기 작업이 많은 시나리오에 대한 새로운 스트레스 테스트를 추가하여 시스템의 전반적인 처리량과 응답성을 측정합니다.

### 교훈 및 권장사항

1.  **락의 범위를 최소화하라**: 공유 데이터에 대한 락은 가능한 한 짧은 시간 동안, 그리고 최소한의 범위에만 적용해야 합니다. 전역 락은 확장성을 심각하게 저해하므로 가급적 피해야 합니다.
2.  **동시성 자료구조를 활용하라**: 직접 락을 관리하는 것보다 검증된 동시성 컨테이너를 사용하는 것이 더 안전하고 효율적일 수 있습니다.
3.  **블로킹 연산을 경계하라**: 이벤트 핸들러나 콜백과 같이 비동기적으로 실행되는 코드 내에서 장시간 블로킹되는 작업을 수행하면 시스템 전체의 반응성에 영향을 미칠 수 있습니다.

### 관련 파일

**문제의 근원**:
- `src/core/datastore/DataStore.h`
- `src/core/datastore/DataStore.cpp`

**영향받는 모듈**:
- `src/core/event/adapters/DataStoreEventAdapter.cpp`
- `src/core/event/core/EventBus.h`
- `src/core/task/**` (DataStore를 사용하는 모든 태스크)

**실패하는 테스트**:
- `tests/unit/event/DataStoreEventAdapter_test.cpp`
