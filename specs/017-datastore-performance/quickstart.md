# 빠른 시작: DataStore 성능 개선 구현 가이드

**브랜치**: `017-datastore-performance` | **날짜**: 2025-11-19

이 문서는 개발자가 DataStore 성능 개선을 구현하기 위한 실용적인 가이드입니다.

## 📋 전제 조건

- C++17 이상 컴파일러 (GCC 7.0+, Clang 5.0+)
- oneTBB (Intel Threading Building Blocks)
- Google Test
- 기존 DataStore 코드 이해

## 🚀 구현 순서

### 1단계: Lock-Free 메트릭 수집

**목표**: 전역 뮤텍스를 atomic 변수로 대체

#### 1.1 헤더 파일 수정 (DataStore.h)

```cpp
// ❌ 기존 코드 (제거)
private:
    std::mutex performance_mutex_;
    std::map<std::string, size_t> performance_metrics_;

// ✅ 새 코드 (추가)
private:
    struct PerformanceMetrics {
        std::atomic<size_t> get_calls{0};
        std::atomic<size_t> set_calls{0};
        std::atomic<size_t> remove_calls{0};
        std::atomic<size_t> has_calls{0};
    };

    PerformanceMetrics metrics_;
```

#### 1.2 구현 파일 수정 (DataStore.cpp)

```cpp
// get() 메서드 예시
template<typename T>
T DataStore::get(const std::string& id) {
    typename MapType::accessor acc;
    if (!data_map_.find(acc, id)) {
        throw std::runtime_error("Key not found");
    }

    // ❌ 기존 코드 (제거)
    // std::lock_guard<std::mutex> lock(performance_mutex_);
    // performance_metrics_["get_calls"]++;

    // ✅ 새 코드 (추가)
    metrics_.get_calls.fetch_add(1, std::memory_order_relaxed);

    return std::any_cast<T>(acc->second.value);
}

// getMetrics() 메서드 수정
std::map<std::string, size_t> DataStore::getPerformanceMetrics() const {
    return {
        {"get_calls", metrics_.get_calls.load(std::memory_order_relaxed)},
        {"set_calls", metrics_.set_calls.load(std::memory_order_relaxed)},
        {"remove_calls", metrics_.remove_calls.load(std::memory_order_relaxed)},
        {"has_calls", metrics_.has_calls.load(std::memory_order_relaxed)}
    };
}
```

#### 1.3 테스트 작성

```cpp
// tests/unit/datastore/DataStorePerformance_test.cpp

TEST(DataStorePerformanceTest, LockFreeMetrics) {
    auto datastore = DataStore::create();
    const int num_threads = 100;
    const int ops_per_thread = 1000;

    // 다중 스레드로 get() 호출
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ops_per_thread; j++) {
                try {
                    datastore->get<int>("test_key");
                } catch (...) {
                    // 키 없음 무시
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 메트릭 확인
    auto metrics = datastore->getPerformanceMetrics();
    EXPECT_EQ(metrics["get_calls"], num_threads * ops_per_thread);
}
```

---

### 2단계: 안전한 Notifier 생명주기 관리

**목표**: Raw pointer를 shared_ptr로 전환하여 dangling pointer 방지

#### 2.1 헤더 파일 수정 (DataStore.h)

```cpp
// ❌ 기존 코드 (제거)
private:
    std::map<std::string, std::unique_ptr<INotifier>> notifiers_;

// ✅ 새 코드 (추가)
private:
    std::map<std::string, std::shared_ptr<INotifier>> notifiers_;
    std::mutex notifiers_mutex_;
```

#### 2.2 구현 파일 수정 (DataStore.cpp)

```cpp
// subscribe() 메서드 수정
void DataStore::subscribe(const std::string& id,
                           std::shared_ptr<INotifier> notifier) {
    std::lock_guard<std::mutex> lock(notifiers_mutex_);
    notifiers_[id] = notifier;  // ✅ shared_ptr 저장
}

// unsubscribe() 메서드 수정
void DataStore::unsubscribe(const std::string& id) {
    std::lock_guard<std::mutex> lock(notifiers_mutex_);
    notifiers_.erase(id);
}

// notifySubscribers() 메서드 수정
void DataStore::notifySubscribers(const SharedData& data) {
    // ❌ 기존 코드 (제거)
    // INotifier* raw_ptr = nullptr;
    // {
    //     std::lock_guard<std::mutex> lock(mutex_);
    //     raw_ptr = notifiers_[data.id].get();
    // }
    // raw_ptr->notify(data);  // 크래시 가능

    // ✅ 새 코드 (추가)
    std::shared_ptr<INotifier> notifier;
    {
        std::lock_guard<std::mutex> lock(notifiers_mutex_);
        auto it = notifiers_.find(data.id);
        if (it != notifiers_.end()) {
            notifier = it->second;  // 참조 카운트 증가
        }
    }  // 락 해제

    if (notifier) {
        try {
            notifier->notify(data);  // 안전함
        } catch (const std::exception& e) {
            spdlog::error("Notifier exception for {}: {}", data.id, e.what());
            // 다른 구독자에게 영향 없음
        }
    }
}
```

#### 2.3 테스트 작성

```cpp
// tests/integration/datastore/DataStoreConcurrency_test.cpp

TEST(DataStoreConcurrencyTest, SafeNotifierLifecycle) {
    auto datastore = DataStore::create();

    std::atomic<int> notify_count{0};
    auto notifier = std::make_shared<TestNotifier>([&](const SharedData& data) {
        notify_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    datastore->subscribe("test_id", notifier);

    // 알림 스레드
    std::thread notify_thread([&]() {
        for (int i = 0; i < 100; i++) {
            SharedData data;
            data.id = "test_id";
            datastore->notifySubscribers(data);
        }
    });

    // 구독 해제 스레드 (경쟁 상태 유도)
    std::thread unsub_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        datastore->unsubscribe("test_id");
    });

    notify_thread.join();
    unsub_thread.join();

    // 크래시 없이 완료되어야 함
    EXPECT_GT(notify_count, 0);
}
```

---

### 3단계: 최적화된 읽기 접근

**목표**: shared_mutex를 사용하여 읽기 병렬성 지원

#### 3.1 헤더 파일 수정 (DataStore.h)

```cpp
// ❌ 기존 코드 (제거)
private:
    std::mutex metadata_mutex_;

// ✅ 새 코드 (추가)
private:
    mutable std::shared_mutex metadata_mutex_;
```

#### 3.2 구현 파일 수정 (DataStore.cpp)

```cpp
// 읽기 전용 메서드 (예: hasAccess)
bool DataStore::hasAccess(const std::string& id,
                           const std::string& user) const {
    // ❌ 기존 코드 (제거)
    // std::lock_guard<std::mutex> lock(metadata_mutex_);

    // ✅ 새 코드 (추가)
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);

    auto it = access_policies_.find(id);
    if (it == access_policies_.end()) {
        return false;
    }

    return checkAccess(it->second, user);
}

// 쓰기 메서드 (예: updatePolicy)
void DataStore::updateAccessPolicy(const std::string& id,
                                    const AccessPolicy& policy) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    access_policies_[id] = policy;
}
```

#### 3.3 테스트 작성

```cpp
TEST(DataStorePerformanceTest, SharedMutexReadParallelism) {
    auto datastore = DataStore::create();

    // 정책 설정
    AccessPolicy policy;
    policy.is_public = true;
    datastore->updateAccessPolicy("test_id", policy);

    const int num_readers = 100;
    std::atomic<int> read_count{0};

    auto start = std::chrono::steady_clock::now();

    // 다수의 읽기 스레드
    std::vector<std::thread> threads;
    for (int i = 0; i < num_readers; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; j++) {
                if (datastore->hasAccess("test_id", "user")) {
                    read_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // 읽기 병렬성으로 빠르게 완료
    EXPECT_LT(duration, 500);  // 0.5초 미만
    EXPECT_EQ(read_count, num_readers * 1000);
}
```

---

## 🧪 테스트 전략

### 1. 단위 테스트 (Unit Tests)

```bash
# 개별 메서드 테스트
cd build
./run_tests --gtest_filter=DataStore*
```

**검증 항목**:
- atomic 연산 정확성
- shared_ptr 참조 카운트
- shared_mutex 락 획득/해제

### 2. 통합 테스트 (Integration Tests)

```bash
# 동시성 스트레스 테스트
./run_tests --gtest_filter=DataStoreConcurrency*
```

**검증 항목**:
- 100개 스레드 동시 접근
- 구독/해제 경쟁 상태
- 메모리 안전성

### 3. 성능 벤치마크 (Performance Benchmark)

```bash
# 성능 측정
./run_tests --gtest_filter=DataStorePerformance*
```

**측정 항목**:
- 처리량 (ops/sec)
- 지연 시간 (p95, p99)
- 확장성 (R² > 0.95)

### 4. 메모리 검증 (Memory Validation)

```bash
# Valgrind 메모리 누수 검사
valgrind --leak-check=full --show-leak-kinds=all ./run_tests --gtest_filter=DataStore*

# AddressSanitizer (빌드 시 -fsanitize=address)
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
./run_tests
```

---

## 📊 성능 측정 방법

### 처리량 측정

```cpp
auto start = std::chrono::steady_clock::now();

// 작업 수행 (예: 100,000 get/set)
for (int i = 0; i < 100000; i++) {
    datastore->get<int>("key");
}

auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end - start).count();

double throughput = 100000.0 / duration * 1000;  // ops/sec
spdlog::info("Throughput: {} ops/sec", throughput);
```

### 확장성 측정

```cpp
std::vector<double> throughputs;
for (int num_threads : {1, 10, 50, 100}) {
    auto throughput = measureThroughput(num_threads);
    throughputs.push_back(throughput);
}

// 선형 회귀로 R² 계산
double r_squared = calculateRSquared(throughputs);
EXPECT_GT(r_squared, 0.95);  // 95% 이상 선형성
```

---

## 🐛 디버깅 팁

### 1. atomic 연산 디버깅

```cpp
// 메모리 순서 확인
auto value = metrics_.get_calls.load(std::memory_order_relaxed);
spdlog::debug("Current get_calls: {}", value);
```

### 2. shared_ptr 참조 카운트 확인

```cpp
{
    std::lock_guard<std::mutex> lock(notifiers_mutex_);
    auto it = notifiers_.find(id);
    if (it != notifiers_.end()) {
        long refcount = it->second.use_count();
        spdlog::debug("Notifier {} refcount: {}", id, refcount);
    }
}
```

### 3. 데드락 탐지

```bash
# gdb로 스레드 상태 확인
gdb ./run_tests
(gdb) run --gtest_filter=DataStore*
^C  # 멈췄을 때
(gdb) info threads
(gdb) thread apply all bt  # 모든 스레드 백트레이스
```

---

## ✅ 체크리스트

구현 전:
- [ ] issue #005 읽고 문제 이해
- [ ] research.md와 data-model.md 검토
- [ ] 기존 DataStore 코드 파악

구현 중:
- [ ] atomic 메트릭 구현 및 테스트
- [ ] shared_ptr Notifier 구현 및 테스트
- [ ] shared_mutex 메타데이터 구현 및 테스트
- [ ] 모든 단위 테스트 통과

구현 후:
- [ ] 통합 테스트 통과 (동시성 스트레스)
- [ ] 성능 벤치마크 목표 달성 (80% 개선)
- [ ] Valgrind 메모리 누수 없음
- [ ] 기존 테스트 100% 통과 (하위 호환성)
- [ ] 코드 리뷰 (CLAUDE.md 규칙 준수)

---

## 📚 참고 자료

- [research.md](research.md) - 기술 조사 결과
- [data-model.md](data-model.md) - 데이터 모델 상세
- [spec.md](spec.md) - 기능 사양서
- [issue/005](../../issue/005-datastore-lock-free-metrics.md) - 원인 분석

## 🤝 도움 받기

- 질문: CLAUDE.md의 코드 작성 가이드 참조
- 버그 리포트: issue/ 디렉토리에 새 이슈 생성
- 성능 이슈: spdlog 디버그 로그 활성화하여 병목 지점 식별
