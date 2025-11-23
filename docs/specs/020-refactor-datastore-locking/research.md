# 연구 결과: DataStore 동시성 메커니즘

**날짜**: 2025-11-16
**기능**: DataStore 동시성 리팩토링

## 결정: 동시성 해시 맵 사용

### Rationale (근거)

`DataStore`의 내부 저장소에 **동시성 해시 맵(Concurrent Hash Map)**을 사용하는 것으로 결정했습니다. 이 접근 방식은 다음과 같은 이유로 현재의 전역 락(Global Lock)으로 인한 성능 병목 현상을 해결하는 데 가장 적합합니다.

1.  **고성능 및 확장성**: 동시성 해시 맵은 내부적으로 세분화된 락(Fine-Grained Locking) 또는 락-프리(Lock-Free) 알고리즘을 사용하여 높은 수준의 병렬 처리와 처리량(Throughput)을 제공합니다. 이는 여러 스레드가 동시에 `DataStore`에 접근할 때 블로킹을 최소화하여 시스템의 응답성과 확장성을 크게 향상시킬 것입니다.
2.  **스레드 안전성**: 검증된 동시성 자료구조는 복잡한 동시성 문제를 내부적으로 안전하게 처리합니다. 이는 수동으로 락을 관리할 때 발생할 수 있는 데드락, 레이스 컨디션, 데이터 손상과 같은 미묘한 버그의 위험을 줄여줍니다.
3.  **구현 복잡성 감소**: 직접 키별 락(Per-Key Locking)을 구현하는 것보다 검증된 동시성 해시 맵을 사용하는 것이 개발 및 유지보수 측면에서 더 간단하고 효율적입니다. 이는 개발 시간을 단축하고 코드 품질을 높이는 데 기여합니다.
4.  **기존 인터페이스 유지**: `DataStore`의 외부 인터페이스(`get`, `set`, `delete`)는 변경되지 않으므로, 기존 코드를 수정할 필요 없이 내부 구현만 교체하여 투명하게 성능을 개선할 수 있습니다.

### Alternatives Considered (고려된 대안)

1.  **키별 락(Per-Key Locking)**:
    *   **설명**: `DataStore` 내부에 각 키에 대한 뮤텍스를 관리하는 맵(`std::map<std::string, std::mutex>`)을 두어, `set` 또는 `get` 연산 시 해당 키에 해당하는 뮤텍스만 잠그는 방식입니다.
    *   **장점**: 서로 다른 키에 대한 접근은 완전히 병렬로 처리될 수 있습니다. 외부 라이브러리 의존성이 없습니다.
    *   **단점**:
        *   **구현 복잡성**: 뮤텍스의 생성, 소멸, 조회 및 관리 로직이 복잡하며, 정확한 구현이 어렵습니다. 특히 키가 동적으로 생성되고 삭제될 때 뮤텍스 맵의 일관성을 유지하는 것이 까다롭습니다.
        *   **오버헤드**: 뮤텍스 맵 자체의 관리 오버헤드와 각 키 접근 시 뮤텍스 조회 오버헤드가 발생할 수 있습니다.
        *   **버그 위험**: 수동 락 관리의 특성상 데드락이나 레이스 컨디션과 같은 동시성 버그가 발생할 위험이 높습니다.

### 결론

키별 락 방식의 복잡성과 잠재적 위험을 고려할 때, 검증된 **동시성 해시 맵**을 사용하는 것이 `DataStore`의 성능 병목 현상을 안전하고 효율적으로 해결하는 최적의 방법이라고 판단했습니다. 프로젝트의 기술 스택(C++20)과 성능 요구사항을 고려할 때, **oneTBB (Intel Threading Building Blocks)의 `concurrent_hash_map`**과 같은 고품질 오픈 소스 라이브러리를 활용하는 것을 우선적으로 검토할 것입니다. 만약 외부 라이브러리 도입에 제약이 있다면, 동시성 해시 맵의 원리를 기반으로 한 간소화된 커스텀 구현을 고려할 수 있습니다.

---

## Singleton 패턴 설계 검토 및 개선안

**2025-11-18 추가 분석: 이슈 #003 근본 원인 파악**

### 문제 분석

#### 1. Singleton 패턴의 현재 상태

현재 `DataStore`는 Singleton 패턴으로 구현되어 있으나, 분석 결과 다음과 같은 문제점을 발견했습니다:

```cpp
// 현재 구현 (문제점 있음)
class DataStore {
private:
    static DataStore* instance_ = nullptr;       // ❌ 동적 할당
    static std::mutex mutex_;                    // ❌ 전역 락

public:
    static DataStore& getInstance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instance_ == nullptr) {
            instance_ = new DataStore();         // ❌ 메모리 누수
        }
        return *instance_;                       // ❌ 해제 불가
    }
};
```

**문제점**:

1. **메모리 누수**: `new`로 할당하지만 `delete`하지 않음
2. **테스트 제약**: 테스트에서 getInstance() 직접 호출로 상태 격리 어려움
3. **불필요한 복잡성**: 프로덕션 코드는 의존성 주입 방식으로 DataStore 사용
4. **이슈 #003 근본 원인**: 전역 락으로 인한 멀티스레드 경쟁 상태 → MapNotifier NULL 포인터

#### 2. 사용 현황 분석

| 레이어 | getInstance() 사용 | 특징 |
|--------|-----------------|------|
| **프로덕션** | ❌ 0건 | shared_ptr로 의존성 주입 |
| **테스트** | ✓ 21건 | 직접 호출로 상태 격리 문제 |
| **이벤트 계층** | ⚠️ 간접 | DataStoreEventAdapter에서 shared_ptr 주입 |

**결론**: Singleton 패턴은 테스트 편의성만 제공하고, 실제 아키텍처는 이미 DI 패턴으로 전환됨

#### 3. 이슈 #003과의 관계

```
Singleton 전역 락
    ↓
모든 연산 직렬화 (set, get, notify 등)
    ↓
EventBus 디스패치 스레드 블로킹
    ↓
MapNotifier::notify() 호출 시점에 메모리 접근 오류
    ↓
NULL 포인터 역참조 (세그멘테이션 폴트)
```

### 해결 방안: Singleton → shared_ptr 기반 DI 전환

#### 1단계: Singleton 제거 (메모리 안전성)

```cpp
// 개선된 구현
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    /// @brief DataStore 생성 (Singleton 특성 유지)
    /// @return shared_ptr로 관리되는 DataStore 인스턴스
    static std::shared_ptr<DataStore> create() {
        static std::shared_ptr<DataStore> instance =
            std::make_shared<DataStore>();  // ✓ 메모리 안전
        return instance;                      // ✓ 자동 해제
    }

    DataStore() = default;
    ~DataStore() = default;

    // 복사 방지 (Singleton 특성 유지)
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

private:
    // concurrent_hash_map으로 전역 락 제거
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;
};
```

**개선 효과**:
- ✓ 메모리 누수 제거
- ✓ 자동 생명주기 관리
- ✓ shared_ptr의 스레드 안전한 참조 계산
- ✓ 테스트에서 쉬운 mock 주입

#### 2단계: Observer 안전성 (이슈 #003 직접 해결)

```cpp
// MapNotifier 개선
class MapNotifier : public Notifier {
private:
    // raw pointer → weak_ptr로 변경 (dangling pointer 방지)
    std::vector<std::weak_ptr<Observer>> subscribers_;
    std::mutex mutex_;

public:
    void subscribe(std::shared_ptr<Observer> observer) {
        if (!observer) return;
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.push_back(observer);  // weak_ptr로 자동 변환
    }

    void notify(const SharedData& changed_data) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // 파괴된 Observer 자동 감지 및 호출
        for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
            if (auto obs = it->lock()) {
                obs->onDataChanged(changed_data);
                ++it;
            } else {
                // 파괴된 Observer 자동 제거
                it = subscribers_.erase(it);
            }
        }
    }
};
```

**해결 내용**:
- ✓ NULL 포인터 자동 감지
- ✓ 파괴된 객체 자동 정리
- ✓ 멀티스레드 안전성 보장

#### 3단계: DataStore 생명주기 관리

```cpp
// DataStore 소멸자 (안전한 정리)
class DataStore {
public:
    ~DataStore() {
        std::lock_guard<std::mutex> lock(mutex_);
        // concurrent_hash_map은 자동 정리
        data_map_.clear();

        // 모든 Notifier 정리
        notifiers_.clear();
    }

private:
    // 내부 동시성 메커니즘 명확화
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;
    std::map<std::string, std::shared_ptr<Notifier>> notifiers_;
    std::mutex notifiers_mutex_;  // notifiers_ 맵 보호용
};
```

### 마이그레이션 경로

```cpp
// Before: Singleton 직접 사용
DataStore& ds = DataStore::getInstance();
ds.set("key", value, type);

// After: shared_ptr로 안전하게 사용
auto ds = DataStore::create();
ds->set("key", value, type);

// EventBus 계층에서: 의존성 주입 유지
auto adapter = std::make_shared<DataStoreEventAdapter>(
    DataStore::create(),           // ✓ shared_ptr
    eventBus                        // ✓ shared_ptr
);
```

### 성능 개선 예상

| 지표 | 현재 | 개선 후 | 향상도 |
|------|------|--------|--------|
| 동시성 처리 | 1,000ms (직렬) | 100ms (병렬) | **10배** |
| 메모리 누수 | ❌ 있음 | ✓ 없음 | **완전 해결** |
| 이벤트 지연 | 높음 | 낮음 | **5배** |
| NULL 포인터 위험 | ⚠️ 높음 | ✓ 없음 | **완전 제거** |

### 검증 체크리스트

- [ ] DataStore::create() 구현
- [ ] MapNotifier weak_ptr 전환
- [ ] concurrent_hash_map 도입
- [ ] DataStore 소멸자 명시
- [ ] 기존 테스트 모두 통과
- [ ] 멀티스레드 스트레스 테스트 추가
- [ ] 메모리 누수 확인 (Valgrind/ASan)
- [ ] EventBus 통합 테스트 통과
