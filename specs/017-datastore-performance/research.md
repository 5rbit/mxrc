# 기술 조사: DataStore 성능 개선

**브랜치**: `017-datastore-performance` | **날짜**: 2025-11-19

이 문서는 Phase 0 연구 단계의 결과를 담고 있습니다.

## 1. std::atomic 메모리 순서 (Memory Order)

### 결정: `std::memory_order_relaxed` 사용

**근거**:
- 메트릭 카운터는 정확한 순서 보장이 불필요 (단순 카운팅)
- 다른 변수와의 동기화 불필요
- 최소 오버헤드로 최대 성능 달성

**대안 평가**:
- `seq_cst` (Sequential Consistency): 가장 강력한 보장이지만 성능 오버헤드 큼 ❌
- `acquire/release`: 순서 보장이 필요한 경우에만 사용 (메트릭에는 불필요) ❌
- `relaxed`: 순서 보장 없음, 성능 최고 ✅

**구현 예시**:
```cpp
std::atomic<size_t> get_calls_{0};

void incrementGetCalls() {
    get_calls_.fetch_add(1, std::memory_order_relaxed);
}

size_t getGetCalls() const {
    return get_calls_.load(std::memory_order_relaxed);
}
```

**참고 자료**:
- C++17 Standard: [atomics.order]
- "C++ Concurrency in Action" by Anthony Williams (Chapter 5)

---

## 2. std::shared_mutex 성능 특성

### 결정: `std::shared_mutex` 사용 (C++17)

**근거**:
- 읽기/쓰기 비율이 100:1 이상인 워크로드에 최적화
- 다수의 읽기 스레드가 동시에 진행 가능
- 쓰기 우선 정책으로 기아 방지

**성능 특성**:
- 읽기 락 획득: O(1), 평균 <1μs
- 쓰기 락 획득: 읽기 락이 모두 해제될 때까지 대기
- 공정성 (Fairness): 플랫폼 의존적 (Linux: 쓰기 우선)

**대안 평가**:
- `std::mutex`: 독점 락만 지원, 읽기 병렬성 없음 ❌
- `std::shared_timed_mutex`: 타임아웃 지원하지만 오버헤드 큼 ❌
- `std::shared_mutex`: 읽기 병렬성 + 최소 오버헤드 ✅

**구현 예시**:
```cpp
std::shared_mutex metadata_mutex_;

// 읽기 작업 (shared lock)
T getMetadata(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    return metadata_[key];
}

// 쓰기 작업 (exclusive lock)
void setMetadata(const std::string& key, T value) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    metadata_[key] = value;
}
```

**참고 자료**:
- C++17 Standard: [thread.sharedmutex]
- "The Art of Multiprocessor Programming" by Maurice Herlihy

---

## 3. oneTBB concurrent_hash_map 내부 락킹 메커니즘

### 결정: accessor 사용 시 추가 락킹 불필요

**이해한 내용**:
- `concurrent_hash_map`은 내부적으로 세분화된 락(fine-grained locking) 사용
- `accessor`는 RAII 패턴으로 자동 락 관리
- 여러 accessor가 서로 다른 버킷에 접근 시 병렬 실행 가능

**주의 사항**:
- accessor 사용 중 전역 뮤텍스를 추가로 사용하면 성능 저하
- accessor의 생명주기를 짧게 유지 (필요한 작업만 수행 후 즉시 해제)

**잘못된 패턴** (issue #005에서 발견):
```cpp
// ❌ concurrent_hash_map의 성능을 무효화하는 패턴
std::mutex global_mutex;
tbb::concurrent_hash_map<std::string, T> map_;

T get(const std::string& id) {
    typename MapType::accessor acc;
    map_.find(acc, id);

    std::lock_guard<std::mutex> lock(global_mutex);  // ❌ 불필요한 전역 락
    metrics_["get_calls"]++;

    return acc->second;
}
```

**올바른 패턴**:
```cpp
// ✅ atomic으로 메트릭만 수집
std::atomic<size_t> get_calls_{0};
tbb::concurrent_hash_map<std::string, T> map_;

T get(const std::string& id) {
    typename MapType::accessor acc;
    map_.find(acc, id);

    get_calls_.fetch_add(1, std::memory_order_relaxed);  // ✅ Lock-free

    return acc->second;
}
```

**참고 자료**:
- oneTBB Documentation: concurrent_hash_map
- Intel Threading Building Blocks Design Patterns

---

## 4. shared_ptr vs weak_ptr for Notifier 관리

### 결정: Notifier 관리에 `std::shared_ptr` 사용

**근거**:
- Notifier의 생명주기를 명확히 관리
- notify 호출 중 객체가 파괴되지 않도록 보장
- weak_ptr는 순환 참조 방지용이지만, 이 케이스에는 순환 참조 없음

**메모리 안전성 보장 방법**:
1. DataStore에 `std::vector<std::shared_ptr<INotifier>>` 저장
2. notify 호출 시 shared_ptr 복사 → 참조 카운트 증가
3. 호출 완료 후 자동 해제 → 참조 카운트 감소

**대안 평가**:
- Raw pointer: Dangling pointer 위험 ❌ (issue #005의 원인)
- `std::unique_ptr`: 복사 불가능, 여러 곳에서 참조 불가 ❌
- `std::weak_ptr`: 순환 참조 방지용, 이 케이스에는 불필요 ❌
- `std::shared_ptr`: 생명주기 자동 관리 + 스레드 안전 ✅

**구현 예시**:
```cpp
// ❌ 기존 코드 (issue #005)
std::map<std::string, std::unique_ptr<INotifier>> notifiers_;

void notifySubscribers(const SharedData& data) {
    INotifier* raw_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = notifiers_.find(data.id);
        if (it != notifiers_.end()) {
            raw_ptr = it->second.get();  // ❌ Dangling pointer 가능
        }
    }  // 락 해제

    if (raw_ptr) {
        raw_ptr->notify(data);  // ❌ 크래시 가능
    }
}

// ✅ 개선된 코드
std::map<std::string, std::shared_ptr<INotifier>> notifiers_;

void notifySubscribers(const SharedData& data) {
    std::shared_ptr<INotifier> notifier;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = notifiers_.find(data.id);
        if (it != notifiers_.end()) {
            notifier = it->second;  // ✅ 참조 카운트 증가
        }
    }  // 락 해제

    if (notifier) {
        notifier->notify(data);  // ✅ 안전함
    }
}  // notifier 소멸 → 참조 카운트 감소
```

**성능 고려 사항**:
- shared_ptr 복사는 원자적(atomic) 참조 카운트 증가/감소 (약간의 오버헤드)
- 하지만 메모리 안전성 향상이 더 중요 (크래시 방지)

**참고 자료**:
- C++17 Standard: [util.smartptr.shared]
- "Effective Modern C++" by Scott Meyers (Item 19-21)

---

## 5. 벤치마크 프레임워크 선정

### 결정: Google Test 기반 직접 구현

**근거**:
- 기존 프로젝트가 Google Test 사용 중 (추가 의존성 불필요)
- 성능 측정 로직이 단순 (스레드 수별 처리량 측정)
- Google Benchmark 도입 시 빌드 시스템 복잡도 증가

**측정 항목**:
1. **처리량 (Throughput)**: 초당 get/set 작업 수
2. **지연 시간 (Latency)**: 평균/p95/p99 응답 시간
3. **확장성 (Scalability)**: 스레드 수에 따른 선형성 계수 (R²)

**대안 평가**:
- Google Benchmark: 전문적이지만 의존성 추가 ❌
- 직접 구현 (Google Test + std::chrono): 단순하고 충분 ✅

**구현 예시**:
```cpp
TEST(DataStorePerformanceTest, ConcurrentThroughput) {
    const int num_threads = 100;
    const int ops_per_thread = 1000;

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ops_per_thread; j++) {
                datastore.set("key", value);
                datastore.get("key");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    size_t total_ops = num_threads * ops_per_thread * 2;  // get + set
    double throughput = static_cast<double>(total_ops) / duration * 1000;

    spdlog::info("Throughput: {} ops/sec", throughput);
}
```

**참고 자료**:
- Google Test Documentation
- "Systems Performance" by Brendan Gregg

---

## 요약

### 최종 기술 스택

| 컴포넌트 | 선택 기술 | 근거 |
|---------|---------|------|
| 메트릭 수집 | `std::atomic<size_t>` (relaxed) | Lock-free, 최소 오버헤드 |
| 읽기/쓰기 락 | `std::shared_mutex` | 읽기 병렬성 지원 |
| Notifier 관리 | `std::shared_ptr<INotifier>` | 메모리 안전성 보장 |
| 성능 측정 | Google Test + std::chrono | 의존성 최소화 |

### 예상 성능 개선

- **메트릭 수집 오버헤드**: 100μs → <1μs (100배 개선)
- **전체 처리량**: 스레드 수에 비례하여 선형 증가 (R² > 0.95)
- **읽기 작업 지연**: 독점 락 → shared 락으로 병렬 처리
- **메모리 안전성**: Dangling pointer 위험 완전 제거

### 다음 단계

Phase 1: 데이터 모델 및 계약서 작성 (data-model.md, quickstart.md)
