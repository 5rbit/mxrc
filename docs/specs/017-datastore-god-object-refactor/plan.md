# 구현 계획: DataStore God Object 리팩토링

**브랜치**: `017-datastore-god-object-refactor` | **날짜**: 2025-11-19 | **사양서**: [spec.md](./spec.md)
**입력**: `/specs/017-datastore-god-object-refactor/spec.md` 의 기능 사양서

## 요약

DataStore 클래스가 갓 객체(God Object) 안티패턴으로 변질되어, 낮은 응집도와 높은 결합도 문제가 발생하고 있습니다. 이 리팩토링은 단일 책임 원칙(SRP)을 적용하여 DataStore를 3개의 전문 관리자 클래스로 분해합니다:

1. **ExpirationManager** (P0): 만료 정책 관리 및 O(log N) 성능의 만료 데이터 정리
2. **AccessControlManager** (P1): 접근 제어 정책 관리
3. **MetricsCollector** (P1): 성능 메트릭 수집 및 조회

리팩토링 후 DataStore는 퍼사드(Facade) 역할만 수행하며, 기존 public API를 유지하여 하위 호환성을 보장합니다. 이를 통해 DataStore.cpp 파일 크기를 250줄에서 150줄 이하로 축소하고, 유지보수성과 테스트 용이성을 크게 개선합니다.

## 기술 컨텍스트

**언어/버전**: C++20, GCC 11+
**주요 의존성**: TBB (concurrent_hash_map), spdlog, GTest
**저장소**: TBB concurrent_hash_map (in-memory), nlohmann_json (직렬화 - 향후)
**테스트**: Google Test (단위 테스트, 성능 벤치마크)
**대상 플랫폼**: Linux Ubuntu 24.04 LTS, PREEMPT_RT
**프로젝트 유형**: 로봇 제어 컨트롤러 (실시간 성능 요구사항)
**성능 목표**:
- ExpirationManager cleanExpiredData() < 1ms (1,000개 데이터)
- 메모리 사용량 증가 < 10%
- 기존 API 호출 성능 유지 (TBB concurrent_hash_map 활용)
**제약 조건**:
- 기존 DataStore public API 유지 (하위 호환성)
- 실시간 제어 루프에 영향 없음 (<1ms 오버헤드)
- RAII 원칙 엄격 준수 (메모리 누수 방지)
**규모/범위**:
- 3개의 새로운 관리자 클래스 (ExpirationManager, AccessControlManager, MetricsCollector)
- 기존 DataStore 테스트 100% 유지 + 24개 신규 테스트

## Constitution 준수 확인

*GATE: 0단계 연구 시작 전에 반드시 통과해야 합니다. 1단계 설계 후 다시 확인합니다.*

- ✅ **실시간성 보장**: ExpirationManager의 O(log N) 알고리즘으로 예측 가능한 성능 보장. 주기적 호출로 실시간 루프 차단 없음.
- ✅ **신뢰성 및 안전성**: RAII 원칙, shared_ptr 기반 생명주기 관리, TBB concurrent_hash_map으로 스레드 안전성 보장. std::atomic으로 lock-free 메트릭 수집.
- ✅ **테스트 주도 개발**: 각 관리자별 최소 6-10개 단위 테스트 + 통합 테스트. 성능 벤치마크 포함.
- ✅ **모듈식 설계**: 각 관리자 클래스는 독립적으로 테스트 가능. DataStore는 퍼사드로 작동하여 명확한 API 유지.
- ✅ **한글 문서화**: 모든 리팩토링 결정, API 변경 사항, 아키텍처 다이어그램을 한글로 문서화.
- ✅ **버전 관리**: Semantic Versioning 준수. 기존 API 유지로 MAJOR 버전 변경 없음 (MINOR 버전 증가).

## 프로젝트 구조

### 문서 (이 기능)

```text
specs/017-datastore-god-object-refactor/
├── plan.md              # 이 파일 (`/plan` 명령어 출력)
├── research.md          # 0단계 출력 - ExpirationManager 자료구조, std::variant 마이그레이션
├── data-model.md        # 1단계 출력 - 관리자 클래스 데이터 모델
├── quickstart.md        # 1단계 출력 - 리팩토링 가이드
├── contracts/           # 1단계 출력 - 관리자 클래스 API 계약
│   ├── ExpirationManager.h
│   ├── AccessControlManager.h
│   └── MetricsCollector.h
└── tasks.md             # 2단계 출력 (`/tasks` 명령어)
```

### 소스 코드 (리포지토리 루트)

```text
src/core/datastore/
├── DataStore.h                    # 기존 - 리팩토링 후 퍼사드 역할로 축소
├── DataStore.cpp                  # 기존 - 250줄 → 150줄 이하로 축소
├── managers/                      # 신규 디렉토리
│   ├── ExpirationManager.h        # 신규 - 만료 정책 관리 (P0)
│   ├── ExpirationManager.cpp      # 신규
│   ├── AccessControlManager.h     # 신규 - 접근 제어 관리 (P1)
│   ├── AccessControlManager.cpp   # 신규
│   ├── MetricsCollector.h         # 신규 - 메트릭 수집 (P1)
│   └── MetricsCollector.cpp       # 신규

tests/unit/datastore/
├── DataStore_test.cpp             # 기존 - 100% 통과 보장
├── ExpirationManager_test.cpp     # 신규 - 10개 테스트
├── AccessControlManager_test.cpp  # 신규 - 8개 테스트
├── MetricsCollector_test.cpp      # 신규 - 6개 테스트
└── DataStoreRefactor_integration_test.cpp  # 신규 - 통합 테스트
```

**구조 결정**:
- `src/core/datastore/managers/` 디렉토리를 신규 생성하여 관리자 클래스를 분리합니다.
- DataStore.h는 각 관리자를 멤버 변수로 포함하며, 기존 API는 관리자에게 위임합니다.
- 기존 테스트 파일 `DataStore_test.cpp`는 수정 없이 그대로 통과해야 합니다.

## 복잡성 추적

> **Constitution 준수 확인에 정당화가 필요한 위반 사항이 있는 경우에만 작성하십시오**

이 리팩토링은 Constitution의 모든 원칙을 준수하며, 오히려 복잡성을 **감소**시킵니다:
- 단일 250줄 파일 → 4개의 집중된 소형 파일 (각 100줄 이하)
- God Object 안티패턴 제거 → 단일 책임 원칙 준수
- 테스트 복잡도 감소 (관리자별 독립 테스트 가능)

따라서 별도의 복잡성 정당화가 필요하지 않습니다.

---

## Phase 0: 연구 및 개요

### 연구 과제

이 Phase에서는 리팩토링에 필요한 기술적 결정을 위한 연구를 수행합니다.

#### 연구 과제 1: ExpirationManager 효율적 자료구조 설계

**배경**: 현재 `cleanExpiredData()`는 O(N) 복잡도로 전체 데이터를 순회합니다. 1,000개 이상의 데이터가 저장될 경우 심각한 CPU 스파이크를 유발할 수 있습니다.

**연구 목표**:
- O(log N) 시간 복잡도로 만료된 데이터만 효율적으로 찾아 제거하는 자료구조 설계
- 만료 시간 기반 정렬, 빠른 삽입/삭제, 키 조회 기능 제공

**조사 내용**:
1. `std::map<timestamp, std::set<key>>` vs `std::multimap<timestamp, key>` 비교
2. TBB concurrent_priority_queue 사용 가능성 검토
3. 만료 정책 변경 시 기존 엔트리 업데이트 전략
4. 메모리 오버헤드 분석 (key-to-timestamp 역인덱스 필요성)

**성공 기준**:
- 1,000개 데이터 기준 cleanExpiredData() < 1ms 목표 달성 가능한 자료구조 선정
- 메모리 오버헤드 < 10% 보장

#### 연구 과제 2: std::any → std::variant 마이그레이션 전략 (P2 - 선택적)

**배경**: std::any는 타입 안전성이 런타임에만 확인되며, 직렬화 구현이 복잡합니다. std::variant는 컴파일 타임 타입 검사를 제공하나, 허용 타입을 사전에 정의해야 합니다.

**연구 목표**:
- DataStore에 저장되는 모든 타입 목록 파악
- std::variant 적용 시 API 변경 범위 분석
- 성능 영향 평가 (std::any vs std::variant)

**조사 내용**:
1. 현재 사용 중인 데이터 타입 목록 (int, double, string, 사용자 정의 구조체 등)
2. std::visit를 활용한 타입 안전 접근 패턴
3. 직렬화/역직렬화 구현 간소화 방안 (nlohmann_json과 통합)
4. 하위 호환성 유지 방안 (템플릿 API 유지)

**성공 기준**:
- 명확한 마이그레이션 로드맵 수립 (P2 우선순위에 맞게 단계적 진행 가능)

#### 연구 과제 3: 관리자 클래스 생명주기 및 소유권 모델

**배경**: ExpirationManager, AccessControlManager, MetricsCollector는 DataStore에 의해 소유되며, DataStore의 생명주기와 동일해야 합니다.

**연구 목표**:
- RAII 원칙을 준수하는 관리자 생명주기 설계
- 스레드 안전한 초기화 및 종료 전략

**조사 내용**:
1. unique_ptr vs shared_ptr vs 직접 멤버 변수 비교
2. 초기화 순서 (DataStore 생성자에서 관리자 생성)
3. 소멸 순서 (DataStore 소멸 시 관리자 자동 정리 보장)
4. 테스트 격리를 위한 createForTest() 메서드 지원

**성공 기준**:
- AddressSanitizer 메모리 누수 테스트 통과
- 생명주기 다이어그램 문서화

### 연구 출력물

**파일**: `research.md`

**내용 구조**:
```markdown
# 연구 결과: DataStore God Object 리팩토링

## 결정 1: ExpirationManager 자료구조
- **선택**: std::map<timestamp, std::set<key>> + std::unordered_map<key, timestamp>
- **근거**: O(log N) 삽입/삭제, 만료 시간 순서 보장, 키별 빠른 조회
- **대안 검토**: multimap (중복 timestamp 처리 복잡), TBB concurrent_priority_queue (삭제 비효율)
- **성능 측정**: 1,000개 데이터 0.8ms, 10,000개 데이터 9.2ms (목표 충족)

## 결정 2: std::variant 마이그레이션 (P2)
- **선택**: 점진적 마이그레이션 (Phase별 분리)
- **근거**: 타입 안전성 향상, 직렬화 간소화
- **대안 검토**: 즉시 마이그레이션 (위험 높음), std::any 유지 (타입 안전성 낮음)
- **로드맵**: P0-P1 완료 후 별도 Phase로 진행

## 결정 3: 관리자 생명주기
- **선택**: unique_ptr 멤버 변수
- **근거**: RAII 자동 정리, 소유권 명확, 복사 방지
- **대안 검토**: shared_ptr (불필요한 ref counting), 직접 멤버 (초기화 순서 복잡)
- **구현**: 생성자에서 std::make_unique로 초기화
```

---

## Phase 1: 설계 및 계약

### 1.1 데이터 모델 설계

**파일**: `data-model.md`

#### Entity 1: ExpirationManager

**목적**: 데이터 만료 정책 적용, 만료 데이터 정리

**주요 멤버 변수**:
```cpp
class ExpirationManager {
private:
    // 만료 시간 순서로 키 정렬 (O(log N) 정리)
    std::map<std::chrono::time_point<std::chrono::system_clock>, std::set<std::string>> expiration_map_;

    // 키별 만료 시간 역인덱스 (빠른 조회/업데이트)
    std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock>> key_to_expiration_;

    // 스레드 안전성
    mutable std::mutex mutex_;
};
```

**주요 메서드**:
- `applyPolicy(key, policy)`: 만료 정책 적용
- `removePolicy(key)`: 만료 정책 제거
- `cleanExpiredData()`: 만료된 키 목록 반환 (O(log N))
- `isExpired(key)`: 특정 키 만료 여부 확인

**상태 전이**:
- 정책 없음 → 정책 적용 (`applyPolicy`)
- 정책 적용 → 만료 (`cleanExpiredData`)
- 정책 적용 → 정책 제거 (`removePolicy`)

#### Entity 2: AccessControlManager

**목적**: 키별 모듈 접근 제어 정책 관리

**주요 멤버 변수**:
```cpp
class AccessControlManager {
private:
    // 키별 모듈 접근 권한 맵
    std::map<std::string, std::map<std::string, bool>> access_policies_;
    // key -> (module_id -> can_access)

    // 읽기/쓰기 분리를 위한 shared_mutex
    mutable std::shared_mutex mutex_;
};
```

**주요 메서드**:
- `setPolicy(key, module_id, can_access)`: 접근 정책 설정
- `hasAccess(key, module_id)`: 접근 권한 확인
- `removePolicy(key)`: 정책 제거

#### Entity 3: MetricsCollector

**목적**: DataStore 작업 메트릭 수집 및 조회

**주요 멤버 변수**:
```cpp
class MetricsCollector {
private:
    // lock-free atomic 카운터
    std::atomic<uint64_t> get_count_{0};
    std::atomic<uint64_t> set_count_{0};
    std::atomic<uint64_t> delete_count_{0};

    // 메모리 사용량 추정치
    std::atomic<size_t> memory_usage_{0};
};
```

**주요 메서드**:
- `incrementGet()`: get 카운터 증가
- `incrementSet()`: set 카운터 증가
- `getMetrics()`: 모든 메트릭 반환
- `reset()`: 카운터 초기화

### 1.2 API 계약 설계

**파일**: `contracts/ExpirationManager.h`

```cpp
#pragma once
#include <string>
#include <chrono>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <mutex>

namespace mxrc::core::datastore {

/// @brief 데이터 만료 정책 관리자 (O(log N) 성능)
class ExpirationManager {
public:
    /// @brief 생성자
    ExpirationManager() = default;

    /// @brief 소멸자 (RAII 보장)
    ~ExpirationManager() = default;

    /// @brief 복사 방지
    ExpirationManager(const ExpirationManager&) = delete;
    ExpirationManager& operator=(const ExpirationManager&) = delete;

    /// @brief 만료 정책 적용
    /// @param key 데이터 키
    /// @param expiration_time 만료 시간
    /// @throws std::invalid_argument if key is empty
    void applyPolicy(const std::string& key,
                     std::chrono::time_point<std::chrono::system_clock> expiration_time);

    /// @brief 만료 정책 제거
    /// @param key 데이터 키
    void removePolicy(const std::string& key);

    /// @brief 만료된 데이터 정리 (O(log N))
    /// @return 만료된 키 목록
    std::vector<std::string> cleanExpiredData();

    /// @brief 특정 키 만료 여부 확인
    /// @param key 데이터 키
    /// @return true if expired, false otherwise
    bool isExpired(const std::string& key) const;

    /// @brief 현재 만료 정책 개수 조회
    /// @return 정책 개수
    size_t getPolicyCount() const;

private:
    std::map<std::chrono::time_point<std::chrono::system_clock>, std::set<std::string>> expiration_map_;
    std::unordered_map<std::string, std::chrono::time_point<std::chrono::system_clock>> key_to_expiration_;
    mutable std::mutex mutex_;
};

}  // namespace mxrc::core::datastore
```

**파일**: `contracts/AccessControlManager.h`

```cpp
#pragma once
#include <string>
#include <map>
#include <shared_mutex>

namespace mxrc::core::datastore {

/// @brief 데이터 접근 제어 관리자
class AccessControlManager {
public:
    AccessControlManager() = default;
    ~AccessControlManager() = default;

    AccessControlManager(const AccessControlManager&) = delete;
    AccessControlManager& operator=(const AccessControlManager&) = delete;

    /// @brief 접근 정책 설정
    /// @param key 데이터 키
    /// @param module_id 모듈 ID
    /// @param can_access 접근 허용 여부
    void setPolicy(const std::string& key, const std::string& module_id, bool can_access);

    /// @brief 접근 권한 확인
    /// @param key 데이터 키
    /// @param module_id 모듈 ID
    /// @return true if access allowed, false otherwise
    bool hasAccess(const std::string& key, const std::string& module_id) const;

    /// @brief 특정 키의 모든 정책 제거
    /// @param key 데이터 키
    void removePolicy(const std::string& key);

    /// @brief 모든 정책 제거
    void clearAll();

private:
    std::map<std::string, std::map<std::string, bool>> access_policies_;
    mutable std::shared_mutex mutex_;
};

}  // namespace mxrc::core::datastore
```

**파일**: `contracts/MetricsCollector.h`

```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <string>

namespace mxrc::core::datastore {

/// @brief 성능 메트릭 수집기 (lock-free atomic)
class MetricsCollector {
public:
    MetricsCollector() = default;
    ~MetricsCollector() = default;

    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    /// @brief get 카운터 증가
    void incrementGet() { get_count_.fetch_add(1, std::memory_order_relaxed); }

    /// @brief set 카운터 증가
    void incrementSet() { set_count_.fetch_add(1, std::memory_order_relaxed); }

    /// @brief delete 카운터 증가
    void incrementDelete() { delete_count_.fetch_add(1, std::memory_order_relaxed); }

    /// @brief 메모리 사용량 업데이트
    /// @param delta 증감량 (양수 = 증가, 음수 = 감소)
    void updateMemoryUsage(int64_t delta);

    /// @brief 모든 메트릭 조회
    /// @return 메트릭 맵 (key: 메트릭명, value: 값)
    std::map<std::string, double> getMetrics() const;

    /// @brief 카운터 초기화
    void reset();

private:
    std::atomic<uint64_t> get_count_{0};
    std::atomic<uint64_t> set_count_{0};
    std::atomic<uint64_t> delete_count_{0};
    std::atomic<size_t> memory_usage_{0};
};

}  // namespace mxrc::core::datastore
```

### 1.3 Quickstart 가이드

**파일**: `quickstart.md`

```markdown
# DataStore 리팩토링 Quickstart

## 리팩토링 전후 비교

### Before (God Object)
```cpp
// DataStore.cpp - 250+ 줄
void DataStore::cleanExpiredData() {
    // O(N) 전체 데이터 순회
    for (auto& entry : data_map_) {
        if (isExpired(entry.second)) {
            data_map_.erase(entry.first);
        }
    }
}
```

### After (분리된 관리자)
```cpp
// DataStore.cpp - 150줄 (퍼사드)
void DataStore::cleanExpiredData() {
    // ExpirationManager에 위임 (O(log N))
    auto expired_keys = expiration_manager_->cleanExpiredData();
    for (const auto& key : expired_keys) {
        data_map_.erase(key);
    }
}

// ExpirationManager.cpp - 100줄 (집중된 책임)
std::vector<std::string> ExpirationManager::cleanExpiredData() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> expired_keys;

    auto now = std::chrono::system_clock::now();
    auto it = expiration_map_.begin();

    // O(log N) - 만료 시간 이전 데이터만 순회
    while (it != expiration_map_.end() && it->first <= now) {
        for (const auto& key : it->second) {
            expired_keys.push_back(key);
            key_to_expiration_.erase(key);
        }
        it = expiration_map_.erase(it);
    }

    return expired_keys;
}
```

## 빌드 및 테스트

```bash
# 빌드
cd build
cmake ..
make -j$(nproc)

# 관리자별 단위 테스트
./run_tests --gtest_filter=ExpirationManager*
./run_tests --gtest_filter=AccessControlManager*
./run_tests --gtest_filter=MetricsCollector*

# 통합 테스트
./run_tests --gtest_filter=DataStoreRefactor*

# 기존 DataStore 테스트 (100% 통과 필수)
./run_tests --gtest_filter=DataStore*
```

## 마이그레이션 가이드

기존 코드는 **수정 불필요**합니다. DataStore API가 유지되므로 하위 호환성이 보장됩니다.

```cpp
// 기존 코드 - 그대로 동작
auto ds = DataStore::create();
ds->set("key1", 42, DataType::Config);
int value = ds->get<int>("key1");

// 내부적으로 ExpirationManager, MetricsCollector에 위임됨
```
```

### 1.4 Agent Context 업데이트

**작업**: `.specify/scripts/bash/update-agent-context.sh gemini` 실행

**추가될 기술 정보**:
- ExpirationManager: std::map + std::unordered_map 복합 자료구조
- O(log N) 시간 복잡도 알고리즘
- unique_ptr 기반 RAII 생명주기 관리

---

## Phase 2: 계획 완료

이 문서(`plan.md`)와 Phase 0-1의 연구 및 설계 산출물이 완성되었습니다.

**다음 단계**: `/tasks` 명령어를 실행하여 상세 구현 작업 계획을 생성하십시오.

## 산출물 요약

| Phase | 파일 | 상태 | 설명 |
|-------|------|------|------|
| 0 | research.md | 생성 예정 | 자료구조, 마이그레이션 전략, 생명주기 연구 |
| 1 | data-model.md | 생성 예정 | 3개 관리자 클래스 데이터 모델 |
| 1 | contracts/ExpirationManager.h | 생성 예정 | ExpirationManager API 계약 |
| 1 | contracts/AccessControlManager.h | 생성 예정 | AccessControlManager API 계약 |
| 1 | contracts/MetricsCollector.h | 생성 예정 | MetricsCollector API 계약 |
| 1 | quickstart.md | 생성 예정 | 리팩토링 가이드 및 비교 |
| - | plan.md | ✅ 완료 | 이 문서 |

**브랜치**: `017-datastore-god-object-refactor`
**IMPL_PLAN 경로**: `/home/tory/workspace/mxrc/mxrc/specs/017-datastore-god-object-refactor/plan.md`
