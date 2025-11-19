## 🟡 이슈 #005: DataStore 종합 성능 및 안정성 문제

**날짜**: 2025-11-19
**심각도**: High
**브랜치**: `refactor/datastore-improvements`
**상태**: 🔍 조사 중
**관련 커밋**: N/A

### 문제 증상

`DataStore` 모듈에 대한 코드 리뷰 및 부하 테스트 중 다음과 같은 성능 및 안정성 문제가 식별되었습니다.

1.  **성능 저하**: 높은 동시성 환경에서 `get`/`set` 메서드 처리량이 `std::mutex` 경합으로 인해 크게 저하됩니다.
2.  **간헐적 크래시**: 비동기 `notify` 로직에서 드물게 세그멘테이션 폴트(Segmentation Fault)가 발생할 가능성이 있습니다 (Dangling Pointer).
3.  **CPU 사용량 급증**: `cleanExpiredData` 호출 시 데이터 양에 비례하여 CPU 사용량이 급증하는 현상이 관찰됩니다.
4.  **유지보수성 및 확장성 문제**: `std::any`를 사용한 데이터 구조로 인해 신규 타입 추가 및 데이터 직렬화/역직렬화 구현이 어렵습니다.
5.  **읽기 병목**: 다수의 스레드가 데이터 정책(Policy) 등 메타데이터를 동시에 조회할 때 불필요한 대기 시간이 발생합니다.

### 근본 원인 분석

#### 1. 메트릭 수집 로직의 Global Mutex

**원인**: `get`/`set` 등 모든 데이터 접근 메서드에서 성능 메트릭을 업데이트하기 위해 단일 `std::mutex`를 사용합니다. 이는 데이터 자체의 접근 제어와 무관함에도 불구하고 모든 작업을 직렬화시켜 `concurrent_hash_map`의 장점을 무효화합니다.

**문제 코드 (`DataStore.cpp`)**:
```cpp
// 가상 코드
template<typename T>
T DataStore::get(const std::string& id) {
    // ... TBB 맵 접근 로직 (빠름) ...

    // ❌ 모든 스레드가 여기서 병목 현상을 겪음
    std::lock_guard<std::mutex> lock(performance_mutex_);
    performance_metrics_["get_calls"]++; 

    return result;
}
```

#### 2. Notifier의 부적절한 생명주기 관리

**원인**: `notifySubscribers` 함수가 `notifiers_` 맵에서 Notifier를 꺼낸 후, 락을 해제하고 notify를 호출합니다. 이 짧은 시간 동안 다른 스레드가 해당 Notifier를 `unsubscribe`하고 파괴하면, `notify()`가 호출되는 시점에는 이미 파괴된 메모리에 접근하는 Dangling Pointer 문제가 발생합니다.

**문제 코드 (`DataStore.cpp`)**:
```cpp
void DataStore::notifySubscribers(const SharedData& changed_data) {
    INotifier* notifier_raw = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = notifiers_.find(changed_data.id);
        if (it != notifiers_.end()) {
            notifier_raw = it->second.get(); // ❌ Raw Pointer 추출
        }
    } // 락 해제

    if (notifier_raw) {
        // ❌ 이 시점에 notifier_raw가 가리키는 객체는 이미 파괴되었을 수 있음
        notifier_raw->notify(changed_data);
    }
}
```

#### 3. 비효율적인 데이터 만료 알고리즘

**원인**: `cleanExpiredData` 함수가 만료 여부를 확인하기 위해 `data_map_`의 모든 원소를 순회하는 $O(N)$ 시간 복잡도를 가집니다. 데이터가 많아질수록 이 작업은 심각한 부하를 유발합니다.

#### 4. 타입 안전성이 낮은 `std::any` 사용

**원인**: 데이터 값을 `std::any`로 저장함으로써 컴파일 타임 타입 체크가 불가능해지며, `std::any_cast` 실패 시 런타임 예외가 발생할 위험이 있습니다. 또한, `std::any`는 표준적인 직렬화 방법이 없어 파일 저장/복원 기능 구현을 매우 어렵게 만듭니다.

---

### 해결 방안

#### 옵션 1: `std::atomic` 기반 메트릭 (성능 병목 제거)

**구현**: `std::mutex` 대신 `std::atomic<size_t>`를 사용하여 락 없이(lock-free) 카운터를 증가시킵니다.
```cpp
// DataStore.h
private:
    struct Metrics {
        std::atomic<size_t> set_calls{0};
        std::atomic<size_t> get_calls{0};
    } metrics_;

// DataStore.cpp
template<typename T>
T DataStore::get(const std::string& id) {
    // ...
    metrics_.get_calls.fetch_add(1, std::memory_order_relaxed); // ✅ Lock-free
    return result;
}
```

#### 옵션 2: `std::shared_ptr` 기반 Notifier 관리 (메모리 안전성 확보)

**구현**: `notifiers_` 맵이 `std::shared_ptr`를 소유하게 하고, `notifySubscribers`에서는 로컬 `shared_ptr`에 복사하여 생명주기를 보장합니다.
```cpp
// DataStore.h
std::map<std::string, std::shared_ptr<INotifier>> notifiers_;

// DataStore.cpp
void DataStore::notifySubscribers(const SharedData& changed_data) {
    std::shared_ptr<INotifier> notifier;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = notifiers_.find(changed_data.id);
        if (it != notifiers_.end()) {
            notifier = it->second; // ✅ RC 증가, 생명주기 연장
        }
    }

    if (notifier) {
        notifier->notify(changed_data); // ✅ 안전하게 호출
    }
}
```

#### 옵션 3: `std::variant`로 타입 안전성 강화

**구현**: `std::any`를 `std::variant`로 교체하여 지원 타입을 명시하고, `std::visit`으로 안전하고 효율적인 로직을 구현합니다.
```cpp
// types.h
using ValueType = std::variant<int, double, std::string, bool>;

// DataStore.h
struct SharedData {
    // ...
    ValueType value;
};
```

#### 옵션 4: `std::shared_mutex`로 읽기 성능 최적화

**구현**: 메타데이터 접근에 C++17의 `std::shared_mutex`를 사용하여 다중 읽기를 허용합니다.
```cpp
// DataStore.h
mutable std::shared_mutex meta_mutex_;

// DataStore.cpp
bool DataStore::hasAccess(const std::string& id, ...) const {
    std::shared_lock<std::shared_mutex> lock(meta_mutex_); // ✅ 읽기 락
    // ...
}
```

### 다음 단계

- [ ] **우선순위 1**: 옵션 1 적용 (`std::atomic`으로 메트릭 리팩토링)
- [ ] **우선순위 2**: 옵션 2 적용 (`shared_ptr`로 Notifier 생명주기 문제 해결)
- [ ] **우선순위 3**: 옵션 4 적용 (`shared_mutex`로 읽기 병목 완화)
- [ ] **장기 개선**: `std::any`를 `std::variant`로 점진적 전환 (옵션 3)
- [ ] **장기 개선**: 데이터 만료 로직을 Priority Queue 기반으로 개선

### 관련 이슈

- **이슈 #002**: `datastore-locking-bottleneck` (본 이슈가 해결)
- **이슈 #003**: `Mapnotifier-segmentation-fault` (본 이슈가 근본적인 해결책 제시)

### 참고 자료

- [std::atomic documentation](https://en.cppreference.com/w/cpp/atomic/atomic)
- [std::shared_mutex documentation](https://en.cppreference.com/w/cpp/thread/shared_mutex)
- [std::variant documentation](https://en.cppreference.com/w/cpp/utility/variant)