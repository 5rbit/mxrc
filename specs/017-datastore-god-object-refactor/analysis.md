# DataStore 현재 상태 분석

**분석 일시**: 2025-11-19
**목적**: God Object 리팩토링을 위한 기반 분석

## 1. 파일 크기 측정 (T006)

- **DataStore.cpp**: 259 lines (목표: <150 lines)
- **DataStore.h**: 310 lines
- **총 코드량**: 569 lines
- **리팩토링 목표**: DataStore.cpp를 150줄 이하로 축소 (42% 감소 필요)

## 2. DataStore.h 책임 분석 (T005)

### 현재 DataStore가 수행하는 책임 목록:

#### 책임 1: 데이터 저장 및 조회 (핵심 책임 - 유지)
```cpp
// Line 90-100: 템플릿 메서드
template<typename T> void set(...)
template<typename T> T get(...)
template<typename T> T poll(...)

// Line 145: concurrent_hash_map 기반 저장소
tbb::concurrent_hash_map<std::string, SharedData> data_map_;
```
**파일**: DataStore.h:90-100, 145, 182-309
**유지 여부**: ✅ DataStore의 핵심 책임 (유지)

#### 책임 2: 데이터 만료 정책 관리 → ExpirationManager로 분리 (P0)
```cpp
// Line 112-115: Expiration 인터페이스
void applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy);
void removeExpirationPolicy(const std::string& id);
void cleanExpiredData();

// Line 149: 만료 정책 저장
std::map<std::string, DataExpirationPolicy> expiration_policies_;
```
**파일**:
- DataStore.h:112-115, 149
- DataStore.cpp:137-163 (27 lines)

**문제점**:
- `cleanExpiredData()`: O(N) 전체 순회 알고리즘 (Line 147-163)
- 1,000개 데이터 기준 성능 저하 발생

**이동 계획**:
- ExpirationManager 클래스로 분리
- std::map<timestamp, set<key>> + std::unordered_map<key, timestamp> 구조
- O(log N) 성능 개선

#### 책임 3: 접근 제어 정책 관리 → AccessControlManager로 분리 (P1)
```cpp
// Line 137-138: Access Control 인터페이스
void setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access);
bool hasAccess(const std::string& id, const std::string& module_id) const;

// Line 150: 접근 정책 저장
std::map<std::string, std::map<std::string, bool>> access_policies_;

// Line 177: shared_mutex for read/write concurrency
mutable std::shared_mutex access_policies_mutex_;
```
**파일**:
- DataStore.h:137-138, 150, 177
- DataStore.cpp:242-259 (18 lines)

**구현 특징**:
- shared_lock (읽기) / unique_lock (쓰기) 패턴 사용
- 읽기 중심 워크로드에 최적화

**이동 계획**:
- AccessControlManager 클래스로 분리
- 기존 shared_mutex 패턴 유지

#### 책임 4: 성능 메트릭 수집 → MetricsCollector로 분리 (P1)
```cpp
// Line 120-122: Metrics 인터페이스
std::map<std::string, double> getPerformanceMetrics() const;
std::vector<std::string> getAccessLogs() const;
std::vector<std::string> getErrorLogs() const;

// Line 156-162: atomic 카운터 구조체
struct PerformanceMetrics {
    std::atomic<size_t> get_calls{0};
    std::atomic<size_t> set_calls{0};
    std::atomic<size_t> poll_calls{0};
    std::atomic<size_t> remove_calls{0};
    std::atomic<size_t> has_calls{0};
};

// Line 165: atomic 메트릭 멤버
PerformanceMetrics metrics_;

// Line 168-170: 기존 메트릭 (향후 제거)
std::map<std::string, double> performance_metrics_;
std::vector<std::string> access_logs_;
std::vector<std::string> error_logs_;
```
**파일**:
- DataStore.h:120-122, 156-170
- DataStore.cpp:75-77, 166-191, 217, 260, 264-265, 299, 303-304 (~30 lines)

**구현 특징**:
- std::atomic으로 lock-free 카운터 구현
- 기존 mutex 기반 메트릭과 중복 (하위 호환성)

**이동 계획**:
- MetricsCollector 클래스로 분리
- 기존 atomic 카운터 유지
- 레거시 메트릭 제거

#### 책임 5: Observer 패턴 구현 (핵심 책임 - 유지)
```cpp
// Line 102-110: Observer 인터페이스
void subscribe(const std::string& id, std::shared_ptr<Observer> observer);
void unsubscribe(const std::string& id, std::shared_ptr<Observer> observer);

// Line 148: Notifier 저장
std::map<std::string, std::shared_ptr<Notifier>> notifiers_;

// Line 153: 내부 헬퍼
void notifySubscribers(const SharedData& changed_data);
```
**파일**:
- DataStore.h:102-110, 148, 153
- DataStore.cpp:11-71 (MapNotifier 클래스), 91-134 (43 lines)

**구현 특징**:
- weak_ptr 기반 안전한 Observer 관리
- dangling pointer 방지 메커니즘

**유지 여부**: ✅ DataStore의 핵심 기능 (유지)

#### 책임 6: 기타 기능 (범위 외 또는 placeholder)
```cpp
// Line 129-134: Scalability & Recovery
size_t getCurrentDataCount() const;
size_t getCurrentMemoryUsage() const;
void saveState(const std::string& filepath);
void loadState(const std::string& filepath);
```
**파일**:
- DataStore.h:129-134
- DataStore.cpp:194-239 (46 lines)

**상태**: placeholder 구현 (범위 외)

## 3. 만료 정책 코드 분석 (T007)

### 현재 구현 (DataStore.cpp:137-163)

```cpp
void DataStore::applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    expiration_policies_[id] = policy;
}

void DataStore::removeExpirationPolicy(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    expiration_policies_.erase(id);
}

void DataStore::cleanExpiredData() {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> expired_keys;

    // ❌ O(N) 전체 순회 - 성능 병목
    for (auto it = data_map_.begin(); it != data_map_.end(); ++it) {
        if (it->second.expiration_time != std::chrono::time_point<std::chrono::system_clock>() &&
            it->second.expiration_time <= now) {
            expired_keys.push_back(it->first);
        }
    }

    // 2단계: 만료된 키 삭제
    for (const auto& key : expired_keys) {
        data_map_.erase(key);
    }
}
```

### 성능 문제
- **시간 복잡도**: O(N) - 전체 데이터 순회
- **메모리**: 만료된 키 벡터 임시 할당
- **성능 목표 미달**: 1,000개 기준 1ms 이내 목표 (현재 측정 필요)

### ExpirationManager 설계 방향

**자료구조**:
```cpp
class ExpirationManager {
private:
    // O(log N) 검색을 위한 시간 기반 정렬
    std::map<std::chrono::time_point<std::chrono::system_clock>,
             std::set<std::string>> expiration_map_;

    // O(1) 키별 만료 시간 조회
    std::unordered_map<std::string,
                       std::chrono::time_point<std::chrono::system_clock>> key_to_expiration_;

    mutable std::mutex mutex_;
};
```

**개선 효과**:
- `cleanExpiredData()`: O(N) → O(log N + K), K = 만료된 키 개수
- `applyExpirationPolicy()`: O(1) → O(log N) (map insert)
- `removeExpirationPolicy()`: O(1) → O(log N)

## 4. 접근 제어 코드 분석 (T008)

### 현재 구현 (DataStore.cpp:242-259)

```cpp
void DataStore::setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access) {
    // ✅ unique_lock for write
    std::unique_lock<std::shared_mutex> lock(access_policies_mutex_);
    access_policies_[id][module_id] = can_access;
}

bool DataStore::hasAccess(const std::string& id, const std::string& module_id) const {
    // ✅ shared_lock for read (읽기 병렬성)
    std::shared_lock<std::shared_mutex> lock(access_policies_mutex_);
    auto it_id = access_policies_.find(id);
    if (it_id != access_policies_.end()) {
        auto it_module = it_id->second.find(module_id);
        if (it_module != it_id->second.end()) {
            return it_module->second;
        }
    }
    return false;  // 기본값: 접근 거부
}
```

### 장점
- **읽기/쓰기 분리**: shared_lock (읽기) / unique_lock (쓰기)
- **읽기 병렬성**: 여러 스레드가 동시에 접근 정책 조회 가능
- **스레드 안전**: std::shared_mutex로 보장

### AccessControlManager 설계 방향

**자료구조 유지**:
```cpp
class AccessControlManager {
private:
    std::map<std::string, std::map<std::string, bool>> access_policies_;
    mutable std::shared_mutex mutex_;
};
```

**인터페이스**:
- `setPolicy(id, module_id, can_access)` - unique_lock
- `hasAccess(id, module_id)` - shared_lock
- `removePolicy(id)` - unique_lock
- `getAllPolicies()` - shared_lock

## 5. 메트릭 코드 분석 (T009)

### 현재 구현

**atomic 카운터** (DataStore.h:156-165):
```cpp
struct PerformanceMetrics {
    std::atomic<size_t> get_calls{0};     // ✅ lock-free
    std::atomic<size_t> set_calls{0};
    std::atomic<size_t> poll_calls{0};
    std::atomic<size_t> remove_calls{0};
    std::atomic<size_t> has_calls{0};
};
PerformanceMetrics metrics_;
```

**사용 위치** (DataStore.h):
- Line 217: `metrics_.set_calls.fetch_add(1, std::memory_order_relaxed)`
- Line 260: `metrics_.get_calls.fetch_add(1, std::memory_order_relaxed)`
- Line 299: `metrics_.poll_calls.fetch_add(1, std::memory_order_relaxed)`

**조회 메서드** (DataStore.cpp:166-181):
```cpp
std::map<std::string, double> DataStore::getPerformanceMetrics() const {
    std::map<std::string, double> result;
    // ✅ lock-free atomic load
    result["get_calls"] = static_cast<double>(metrics_.get_calls.load(std::memory_order_relaxed));
    result["set_calls"] = static_cast<double>(metrics_.set_calls.load(std::memory_order_relaxed));
    result["poll_calls"] = static_cast<double>(metrics_.poll_calls.load(std::memory_order_relaxed));

    // ❌ 레거시 메트릭 (제거 예정)
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [key, value] : performance_metrics_) {
        result[key] = value;
    }
    return result;
}
```

### 문제점
- **중복 구조**: atomic 카운터 + 레거시 map 메트릭
- **일관성 부족**: 일부는 atomic, 일부는 mutex 보호
- **DataStore 오염**: 메트릭 로직이 핵심 로직과 혼재

### MetricsCollector 설계 방향

**자료구조**:
```cpp
class MetricsCollector {
private:
    std::atomic<uint64_t> get_count_{0};
    std::atomic<uint64_t> set_count_{0};
    std::atomic<uint64_t> delete_count_{0};
    std::atomic<size_t> memory_usage_{0};
};
```

**인터페이스**:
- `incrementGet()` - lock-free
- `incrementSet()` - lock-free
- `incrementDelete()` - lock-free
- `updateMemoryUsage(delta)` - lock-free
- `getMetrics()` - lock-free load

**제거할 레거시 코드**:
- `performance_metrics_` (map)
- `access_logs_` (vector)
- `error_logs_` (vector)

## 6. 리팩토링 영향 범위

### 삭제할 코드 (약 100 lines)

**DataStore.h**:
- Line 112-115: Expiration 인터페이스 (4 lines)
- Line 137-138: Access Control 인터페이스 (2 lines)
- Line 120-122: Metrics 인터페이스 (3 lines)
- Line 149-150: 정책 저장소 (2 lines)
- Line 156-170: PerformanceMetrics 구조체 + 레거시 (15 lines)
- Line 177: access_policies_mutex_ (1 line)

**DataStore.cpp**:
- Line 75-77: 레거시 메트릭 초기화 (3 lines)
- Line 137-163: Expiration 구현 (27 lines)
- Line 166-191: Metrics 구현 (26 lines)
- Line 217, 260, 264-265, 299, 303-304: 메트릭 업데이트 (8 lines)
- Line 242-259: Access Control 구현 (18 lines)

**총 삭제**: ~109 lines → **목표 150 lines 달성 가능**

### 추가할 코드

**새로운 파일**:
- `ExpirationManager.h` (~60 lines)
- `ExpirationManager.cpp` (~80 lines)
- `AccessControlManager.h` (~40 lines)
- `AccessControlManager.cpp` (~50 lines)
- `MetricsCollector.h` (~40 lines)
- `MetricsCollector.cpp` (~30 lines)

**DataStore 수정**:
- `unique_ptr` 멤버 추가 (3 lines)
- 위임 메서드 추가 (~20 lines)

## 7. 마이그레이션 전략

### Phase 순서 (의존성 기반)

1. **Phase 3 (P0)**: ExpirationManager
   - 성능 크리티컬한 문제 해결
   - 다른 Manager와 독립적

2. **Phase 4 (P1)**: AccessControlManager
   - ExpirationManager와 독립적
   - 병렬 개발 가능

3. **Phase 5 (P1)**: MetricsCollector
   - 모든 Manager의 메트릭 수집
   - ExpirationManager, AccessControlManager 완료 후

4. **Phase 6**: DataStore Facade 리팩토링
   - 모든 Manager 완료 후
   - 통합 테스트

### 하위 호환성 보장

**public 인터페이스 유지**:
```cpp
// 기존 API 그대로 유지
void set(...);
T get(...);
void applyExpirationPolicy(...);
void setAccessPolicy(...);
std::map<std::string, double> getPerformanceMetrics() const;
```

**내부 위임으로 변경**:
```cpp
void DataStore::applyExpirationPolicy(...) {
    expiration_manager_->applyPolicy(...);  // 위임
}
```

## 8. 성공 기준 체크리스트

- [ ] **SC-001**: ExpirationManager_test.cpp 10+ tests 통과
- [ ] **SC-002**: AccessControlManager_test.cpp 8+ tests 통과
- [ ] **SC-003**: MetricsCollector_test.cpp 6+ tests 통과
- [ ] **SC-004**: 기존 DataStore_test.cpp 100% 통과 (56/60 현재)
- [ ] **SC-005**: cleanExpiredData() 성능 벤치마크 O(log N) 특성
  - 100개: <0.1ms
  - 1,000개: <1ms
  - 10,000개: <10ms
- [ ] **SC-006**: DataStore.cpp < 150 lines (현재 259 → 42% 감소)
- [ ] **SC-007**: AddressSanitizer 메모리 누수 없음

## 9. 다음 단계 (Phase 3 시작)

**즉시 실행 가능한 작업**:
1. ✅ T004: DataStore 테스트 실행 (56/60 통과)
2. ✅ T005: 책임 문서화 (본 문서)
3. ✅ T006: 줄 수 측정 (259 lines)
4. ✅ T007: Expiration 코드 분석
5. ✅ T008: Access Control 코드 분석
6. ✅ T009: Metrics 코드 분석

**Phase 3 시작 준비 완료**:
- [P] T010: ExpirationManager_test.cpp 생성
- [P] T011-T020: TDD 테스트 작성 (10 tests)
- T021-T031: ExpirationManager 구현

---

**분석 완료**: Phase 2 (T004-T009) ✅
**다음**: Phase 3 ExpirationManager TDD 시작
