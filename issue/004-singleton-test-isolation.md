## 🟡 이슈 #004: Singleton DataStore의 테스트 간 상태 공유 문제

**날짜**: 2025-11-18
**심각도**: Medium
**브랜치**: `020-refactor-datastore-locking`
**상태**: 🔍 조사 중
**관련 커밋**: 3eb74fa (Phase 3: concurrent_hash_map 전환)

### 문제 증상

- 전체 테스트 스위트 실행 시 pthread mutex 관련 크래시 발생
- 개별 테스트는 모두 성공하지만, 연속 실행 시 실패
- 특정 테스트 순서에 따라 간헐적으로 발생

```bash
Fatal glibc error: pthread_mutex_lock.c:450 (__pthread_mutex_lock_full):
assertion failed: e != ESRCH || !robust

terminate called after throwing an instance of 'std::system_error'
  what():  Resource deadlock avoided
```

**재현 조건**:
```bash
# 성공 (개별 테스트)
./run_tests --gtest_filter="DataStore*"           # 27/28 통과
./run_tests --gtest_filter="ActionExecutorTest*"  # 모두 통과

# 실패 (전체 테스트)
./run_tests  # pthread mutex 오류로 크래시
```

### 근본 원인 분석

#### 1. Singleton 패턴의 테스트 격리 문제

**현재 DataStore 구조**:
```cpp
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    static std::shared_ptr<DataStore> create() {
        // C++11 thread-safe static initialization
        static std::shared_ptr<DataStore> instance =
            std::make_shared<DataStore>();
        return instance;  // ❌ 모든 테스트가 같은 인스턴스 공유
    }
};
```

**문제점**:
- 모든 테스트가 동일한 DataStore 인스턴스 사용
- 테스트 A가 설정한 데이터를 테스트 B가 참조
- notifiers_, expiration_policies_, access_policies_ 등이 테스트 간 누적
- 이전 테스트의 Observer가 파괴된 후에도 notifiers_에 등록된 상태

#### 2. Orphaned Mutex 문제

**발생 시나리오**:
1. 테스트 A가 ActionExecutor를 생성하고 비동기 작업 시작
2. 비동기 작업이 DataStore에 접근하여 concurrent_hash_map의 내부 mutex 잠금
3. 테스트 A 종료 → ActionExecutor 소멸 → 작업 스레드 강제 종료
4. **mutex를 잠근 채로 스레드 종료** → orphaned mutex
5. 테스트 B가 DataStore 접근 시 orphaned mutex 감지 → ESRCH 오류

**pthread 오류 코드**:
- `ESRCH` (No such process): mutex를 소유한 스레드가 더 이상 존재하지 않음
- `robust mutex`: 소유 스레드가 종료되면 다음 잠금 시도에서 오류 반환

#### 3. concurrent_hash_map의 내부 구조

**TBB concurrent_hash_map 특성**:
- 세분화된 락 (bucket-level locking)
- 각 accessor가 내부적으로 mutex 보유
- accessor 소멸 시 자동 unlock (RAII)
- **주의**: 스레드가 accessor를 잡은 채로 종료되면 문제 발생

### 영향 받는 코드

#### DataStore.h
```cpp
template<typename T>
void DataStore::set(const std::string& id, const T& data, ...) {
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::accessor acc;
        if (data_map_.find(acc, id)) { /* ... */ }
        else {
            data_map_.insert(acc, id);
        }
        acc->second = new_data;
        // accessor는 스코프를 벗어나면 자동 해제 (RAII)
    }  // ✅ 정상 경로에서는 여기서 unlock

    // ❌ 하지만 스레드가 강제 종료되면 unlock 안 됨
    notifySubscribers(new_data);
}
```

#### ActionExecutor 소멸자 (Phase 2-B에서 수정됨)
```cpp
~ActionExecutor() {
    // 실행 중인 액션 취소 및 스레드 정리
    std::vector<std::unique_ptr<std::thread>> threadsToJoin;
    {
        std::lock_guard<std::mutex> lock(actionsMutex_);
        for (auto& [id, state] : runningActions_) {
            if (state.timeoutThread && state.timeoutThread->joinable()) {
                threadsToJoin.push_back(std::move(state.timeoutThread));
            }
        }
        runningActions_.clear();
    }

    for (auto& thread : threadsToJoin) {
        if (thread && thread->joinable()) {
            thread->join();  // ❌ 스레드가 DataStore mutex를 잡은 채로 종료 가능
        }
    }
}
```

### 해결 방안

#### 옵션 1: 테스트별 DataStore 인스턴스 생성 (권장)

**장점**:
- 완벽한 테스트 격리
- 상태 공유 문제 완전 제거
- 예측 가능한 테스트 동작

**단점**:
- Singleton 패턴 포기
- 프로덕션 코드와 테스트 코드 동작 차이

**구현 예시**:
```cpp
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    // 프로덕션: Singleton
    static std::shared_ptr<DataStore> create() {
        static std::shared_ptr<DataStore> instance =
            std::make_shared<DataStore>();
        return instance;
    }

    // 테스트: 독립 인스턴스 생성
    static std::shared_ptr<DataStore> createForTest() {
        return std::make_shared<DataStore>();  // ✅ 매번 새로운 인스턴스
    }
};

// 테스트 픽스처
class DataStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        dataStore_ = DataStore::createForTest();  // ✅ 테스트마다 독립
    }

    void TearDown() override {
        dataStore_.reset();  // ✅ 명시적 정리
    }

    std::shared_ptr<DataStore> dataStore_;
};
```

#### 옵션 2: DataStore 상태 초기화 메서드

**장점**:
- Singleton 패턴 유지
- 프로덕션 코드와 동일한 인스턴스 사용

**단점**:
- 완벽한 격리 보장 어려움
- concurrent_hash_map의 내부 상태는 초기화 불가
- orphaned mutex 문제는 여전히 발생 가능

**구현 예시**:
```cpp
class DataStore {
public:
    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 주의: concurrent_hash_map의 clear()는 모든 accessor 해제 필요
        data_map_.clear();
        notifiers_.clear();
        expiration_policies_.clear();
        access_policies_.clear();
        performance_metrics_.clear();
        access_logs_.clear();
        error_logs_.clear();
    }
};

// 테스트 픽스처
class DataStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        DataStore::create()->resetForTest();  // ⚠️ 불완전한 격리
    }
};
```

#### 옵션 3: 스레드 안전한 종료 보장

**장점**:
- 근본 원인 해결
- Singleton 유지
- orphaned mutex 방지

**단점**:
- 복잡한 구현
- ActionExecutor, SequenceEngine, TaskExecutor 모두 수정 필요

**구현 예시**:
```cpp
class ActionExecutor {
    ~ActionExecutor() {
        // 1. 실행 중인 작업에 취소 플래그 설정
        {
            std::lock_guard<std::mutex> lock(actionsMutex_);
            for (auto& [id, state] : runningActions_) {
                state.cancelRequested = true;
            }
        }

        // 2. 모든 스레드가 안전하게 종료될 때까지 대기
        for (auto& thread : threadsToJoin) {
            if (thread && thread->joinable()) {
                // ✅ 스레드가 mutex를 해제할 때까지 기다림
                thread->join();
            }
        }

        // 3. 이제 안전하게 정리
        runningActions_.clear();
    }
};

// Action 실행 람다 수정
auto future = std::async([weak_self, action, ctx]() {
    auto self = weak_self.lock();
    if (!self) return;

    // ✅ 주기적으로 취소 플래그 확인
    while (!self->isCancelRequested(action->getId())) {
        // 작업 수행
        if (작업_완료) break;
    }

    // ✅ DataStore 접근은 RAII accessor 사용
    // accessor 소멸 전에 스레드 종료 보장
});
```

### 추가 조사 필요

1. **TBB 버전 확인**
   - oneTBB vs 구버전 TBB
   - robust mutex 지원 여부

2. **테스트 순서 의존성**
   ```bash
   # 특정 테스트만 실행하여 문제 테스트 식별
   ./run_tests --gtest_filter="ActionExecutorTest.DestructorCancelsRunningActions"
   ./run_tests --gtest_filter="DataStoreEventAdapterTest*:ActionExecutorTest*"
   ```

3. **메모리 안전성 검증**
   ```bash
   # AddressSanitizer로 실행
   ASAN_OPTIONS=detect_leaks=1 ./run_tests

   # ThreadSanitizer로 실행
   TSAN_OPTIONS=second_deadlock_stack=1 ./run_tests
   ```

### 임시 회피 방법

1. **테스트 격리 실행**
   ```bash
   # 각 테스트 스위트를 독립 실행
   ./run_tests --gtest_filter="DataStore*"
   ./run_tests --gtest_filter="ActionExecutor*"
   ./run_tests --gtest_filter="SequenceEngine*"
   ```

2. **문제 테스트 건너뛰기**
   ```bash
   # 소멸자 관련 테스트 제외
   ./run_tests --gtest_filter="-*Destructor*"
   ```

### 다음 단계

- [ ] **우선순위 1**: 옵션 1 구현 (createForTest() 메서드)
- [ ] **우선순위 2**: 전체 테스트 스위트에서 DataStore::createForTest() 사용
- [ ] **우선순위 3**: AddressSanitizer/ThreadSanitizer로 근본 원인 확인
- [ ] **장기 개선**: 스레드 안전한 종료 보장 (옵션 3)

### 관련 이슈

- **이슈 #003**: MapNotifier 세그멘테이션 폴트 (weak_ptr 전환으로 해결)
- **Phase 2-B**: ActionExecutor 소멸자 mutex 데드락 (RAII 패턴으로 해결)
- **Phase 3**: concurrent_hash_map 전환 (orphaned mutex 문제 발견)

### 참고 자료

- [TBB concurrent_hash_map 문서](https://spec.oneapi.io/versions/latest/elements/oneTBB/source/containers/concurrent_hash_map_cls.html)
- [pthread robust mutex](https://man7.org/linux/man-pages/man3/pthread_mutexattr_setrobust.3.html)
- [RAII and Thread Safety](https://en.cppreference.com/w/cpp/language/raii)
