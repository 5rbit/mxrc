# 데이터 모델: DataStore 내부 구조

**브랜치**: `017-datastore-performance` | **날짜**: 2025-11-19

이 문서는 DataStore 클래스의 내부 데이터 모델을 정의합니다.

## 개요

DataStore는 내부 구현만 변경하며, **공개 API는 변경되지 않습니다**. 따라서 외부에서 보이는 데이터 모델은 기존과 동일합니다.

## 엔티티 (Entities)

### 1. PerformanceMetrics (성능 메트릭)

**목적**: 시스템 운영 상태를 추적하는 카운터 집합

**속성**:
```cpp
struct PerformanceMetrics {
    std::atomic<size_t> get_calls{0};        // get() 호출 횟수
    std::atomic<size_t> set_calls{0};        // set() 호출 횟수
    std::atomic<size_t> remove_calls{0};     // remove() 호출 횟수
    std::atomic<size_t> has_calls{0};        // has() 호출 횟수
};
```

**변경 전** (issue #005의 문제):
```cpp
// ❌ 전역 뮤텍스로 보호되는 std::map
std::mutex performance_mutex_;
std::map<std::string, size_t> performance_metrics_;
```

**변경 후**:
```cpp
// ✅ Lock-free atomic 카운터
PerformanceMetrics metrics_;
```

**관계**:
- DataStore 인스턴스마다 하나씩 존재
- 읽기: `load(std::memory_order_relaxed)`
- 쓰기: `fetch_add(1, std::memory_order_relaxed)`

**검증 규칙**:
- 오버플로우 시 wrap-around 허용 (UINT64_MAX → 0)
- 차분값 계산 시 오버플로우 고려

---

### 2. Notifier (알림 구독자)

**목적**: 데이터 변경 시 알림을 받을 구독자

**속성**:
```cpp
class INotifier {
public:
    virtual ~INotifier() = default;
    virtual void notify(const SharedData& data) = 0;
};

// DataStore 내부 저장 타입
std::map<std::string, std::shared_ptr<INotifier>> notifiers_;
std::mutex notifiers_mutex_;  // notifiers_ 맵 보호용
```

**변경 전** (issue #005의 문제):
```cpp
// ❌ Dangling pointer 위험
std::map<std::string, std::unique_ptr<INotifier>> notifiers_;

void notifySubscribers(const SharedData& data) {
    INotifier* raw_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        raw_ptr = notifiers_[data.id].get();  // ❌ Raw pointer 추출
    }  // 락 해제

    raw_ptr->notify(data);  // ❌ 크래시 가능
}
```

**변경 후**:
```cpp
// ✅ 안전한 생명주기 관리
std::map<std::string, std::shared_ptr<INotifier>> notifiers_;

void notifySubscribers(const SharedData& data) {
    std::shared_ptr<INotifier> notifier;
    {
        std::lock_guard<std::mutex> lock(notifiers_mutex_);
        auto it = notifiers_.find(data.id);
        if (it != notifiers_.end()) {
            notifier = it->second;  // ✅ 참조 카운트 증가
        }
    }  // 락 해제

    if (notifier) {
        try {
            notifier->notify(data);  // ✅ 안전함
        } catch (const std::exception& e) {
            spdlog::error("Notifier exception: {}", e.what());
        }
    }
}
```

**관계**:
- DataStore와 N:1 관계 (하나의 DataStore에 여러 Notifier 등록)
- `subscribe()`: Notifier 등록
- `unsubscribe()`: Notifier 제거

**상태 전이**:
```
[등록 대기] --subscribe()--> [활성] --unsubscribe()--> [제거됨]
                                 |
                                 v
                            [알림 처리 중]
```

**검증 규칙**:
- notify 호출 중 unsubscribe 시도 → notify 완료 후 제거
- notify 콜백에서 예외 발생 → 로깅 후 다음 구독자 계속 처리
- 동시 subscribe/unsubscribe → 마지막 작업이 승리 (last-write-wins)

---

### 3. SharedMetadata (공유 메타데이터)

**목적**: 다중 스레드가 자주 읽는 정보 (데이터 정책, 접근 권한 등)

**속성**:
```cpp
// 예시: 데이터 접근 정책
struct AccessPolicy {
    std::string owner;
    std::vector<std::string> allowed_readers;
    bool is_public;
};

std::map<std::string, AccessPolicy> access_policies_;
mutable std::shared_mutex policies_mutex_;  // shared_mutex로 변경
```

**변경 전**:
```cpp
// ❌ 독점 락으로 읽기도 블로킹
std::mutex policies_mutex_;

bool hasAccess(const std::string& id, const std::string& user) const {
    std::lock_guard<std::mutex> lock(policies_mutex_);  // ❌ 독점 락
    return checkAccess(access_policies_[id], user);
}
```

**변경 후**:
```cpp
// ✅ 읽기 병렬성 지원
mutable std::shared_mutex policies_mutex_;

bool hasAccess(const std::string& id, const std::string& user) const {
    std::shared_lock<std::shared_mutex> lock(policies_mutex_);  // ✅ Shared 락
    return checkAccess(access_policies_.at(id), user);
}

void updatePolicy(const std::string& id, const AccessPolicy& policy) {
    std::unique_lock<std::shared_mutex> lock(policies_mutex_);  // ✅ Exclusive 락
    access_policies_[id] = policy;
}
```

**관계**:
- DataStore 인스턴스마다 하나씩 존재
- 읽기 우선 접근: 다수의 읽기 스레드 동시 진행
- 쓰기 우선 정책: 읽기 요청이 계속되어도 쓰기 기아 방지

**검증 규칙**:
- 읽기 락 획득 시간 <1μs
- 쓰기 락 대기 시 읽기 락 모두 해제될 때까지 대기

---

## 데이터 흐름 (Data Flow)

### 1. get() 호출 흐름

```
사용자 요청 get(id)
    ↓
concurrent_hash_map accessor 획득 (세분화된 락)
    ↓
데이터 읽기
    ↓
metrics_.get_calls.fetch_add(1, relaxed)  // Lock-free
    ↓
accessor 해제 (RAII)
    ↓
결과 반환
```

**성능 특성**:
- 락 경합 최소화 (concurrent_hash_map의 내부 락만 사용)
- 메트릭 수집 오버헤드 <1μs

### 2. set() 호출 흐름

```
사용자 요청 set(id, value)
    ↓
concurrent_hash_map accessor 획득 (쓰기 락)
    ↓
데이터 쓰기
    ↓
metrics_.set_calls.fetch_add(1, relaxed)  // Lock-free
    ↓
accessor 해제
    ↓
notifySubscribers(data)  // 비동기 알림
    ↓
결과 반환
```

### 3. notifySubscribers() 흐름

```
데이터 변경 발생
    ↓
notifiers_mutex_ 획득 (짧은 시간)
    ↓
shared_ptr 복사 → 참조 카운트 증가
    ↓
notifiers_mutex_ 해제
    ↓
notifier->notify(data)  // 락 없이 호출
    ↓
참조 카운트 감소 (자동)
```

**안전성 보장**:
- notify 호출 중 객체가 파괴되지 않음 (shared_ptr)
- 콜백 예외가 다른 구독자에게 영향 주지 않음 (try-catch)

---

## 메모리 레이아웃

### DataStore 클래스 멤버 변수 (변경 후)

```cpp
class DataStore {
private:
    // 기존 데이터 저장소 (변경 없음)
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;

    // ✅ 변경: atomic 메트릭
    PerformanceMetrics metrics_;

    // ✅ 변경: shared_ptr Notifier
    std::map<std::string, std::shared_ptr<INotifier>> notifiers_;
    std::mutex notifiers_mutex_;

    // ✅ 변경: shared_mutex 메타데이터
    std::map<std::string, AccessPolicy> access_policies_;
    mutable std::shared_mutex policies_mutex_;
};
```

**메모리 사용량 변화**:
- atomic<size_t> × 4: 32 bytes (기존 map<string, size_t> 대비 감소)
- shared_ptr 오버헤드: 참조 카운트 블록 (16 bytes per Notifier)
- shared_mutex: 약 40 bytes (기존 mutex 32 bytes 대비 소폭 증가)

**전체 메모리 증가**: 약 100 bytes 미만 (무시 가능)

---

## 스레드 안전성

### 락 계층 구조 (Lock Hierarchy)

```
Level 1: concurrent_hash_map 내부 락 (세분화된 버킷 락)
    ↓
Level 2: notifiers_mutex_ (Notifier 등록/해제)
    ↓
Level 3: policies_mutex_ (메타데이터 읽기/쓰기)
```

**데드락 방지**:
- 각 락은 독립적 (중첩 사용 없음)
- RAII 패턴으로 자동 해제
- lock_guard, shared_lock, unique_lock 사용

---

## 성능 특성

### 시간 복잡도

| 연산 | 기존 (with global mutex) | 변경 후 (lock-free + shared_mutex) |
|------|-------------------------|-----------------------------------|
| get() | O(1) + mutex contention | O(1) + no contention |
| set() | O(1) + mutex contention | O(1) + no contention |
| getMetrics() | O(1) + mutex contention | O(1) atomic load |
| hasAccess() (read) | O(1) + exclusive lock | O(1) + shared lock |
| updatePolicy() (write) | O(1) + exclusive lock | O(1) + exclusive lock |

### 예상 성능 개선

- **100 스레드 환경**: 평균 처리 시간 80% 단축
- **메트릭 수집 오버헤드**: 100μs → <1μs
- **읽기 병렬성**: 100개 스레드 동시 진행 가능

---

## 다음 단계

Phase 1 계속: quickstart.md 작성 (개발자 가이드)
