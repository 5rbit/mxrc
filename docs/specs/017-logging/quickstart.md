# Quickstart Guide: MXRC Bag Logging & Replay

## 개요

이 가이드는 MXRC 로깅 시스템을 사용하여 로봇 실행 데이터를 기록하고, 재생하여 회귀 테스트를 수행하는 방법을 안내합니다.

**핵심 기능:**
- 📝 **P1**: EventBus 기반 비동기 Bag 파일 로깅
- 🔄 **P2**: Bag 파일 읽기 및 DataStore 상태 재현 (Replay)
- ⚙️ **P2**: 선택적 로깅 전략 (NONE, MEMORY_ONLY, EVENT_DRIVEN, FULL_BAG)

---

## 1. 기본 사용법: Bag 파일 기록

### 1.1 필요한 헤더 포함

```cpp
#include "core/logging/BagLogger.h"
#include "core/logging/contracts/IBagWriter.h"
#include "core/event/EventBus.h"
#include "core/datastore/DataStore.h"
```

### 1.2 BagLogger 초기화

```cpp
using namespace mxrc::core::logging;

// EventBus 생성 (이미 존재하면 재사용)
auto eventBus = std::make_shared<mxrc::core::event::EventBus>(10000);
eventBus->start();

// BagLogger 생성 (Singleton 대신 static factory 사용)
auto bagLogger = BagLogger::create(eventBus);

// Bag 파일 경로 설정 (타임스탬프 자동 추가)
std::string logPath = "logs/mission_2025-11-19_14-30-00.bag";
bagLogger->open(logPath);
```

### 1.3 DataStore에서 자동 로깅 설정

```cpp
auto dataStore = DataStore::create();

// DataStoreEventAdapter로 DataStore 변경 → EventBus 연동
auto adapter = std::make_shared<DataStoreEventAdapter>(dataStore, eventBus);

// BagLogger가 EventBus 구독하여 자동 기록 시작
// (DataStore.set() 호출 시 자동으로 Bag에 기록됨)
```

### 1.4 로봇 실행 중 데이터 기록

```cpp
// DataStore에 값 쓰기 → 자동으로 EventBus → BagLogger로 전달
dataStore->set("MissionState.status", "RUNNING");
dataStore->set("TaskState.position", Position{x: 10.5, y: 20.3, z: 0.0});

// 8시간 운영 시 모든 변경사항 자동 기록 (FR-001, SC-002)
```

### 1.5 정상 종료 시 파일 닫기

```cpp
bagLogger->flush();  // 버퍼링된 데이터 모두 기록
bagLogger->close();  // 인덱스 + footer 작성 후 파일 닫기

eventBus->stop();
```

**결과:**
- `logs/mission_2025-11-19_14-30-00.bag` 파일 생성
- JSONL 포맷으로 타임스탬프, topic, 값 기록
- 파일 끝에 인덱스 블록 + footer 추가

---

## 2. Bag 파일 읽기 및 탐색

### 2.1 BagReader로 파일 열기

```cpp
#include "core/logging/contracts/BagReader.h"

BagReader reader;
reader.open("logs/mission_2025-11-19_14-30-00.bag");

// 파일 정보 확인
auto [start_ns, end_ns] = reader.getTimeRange();
uint32_t totalMessages = reader.getMessageCount();

spdlog::info("Bag file: {} messages, duration: {} seconds",
             totalMessages, (end_ns - start_ns) / 1e9);
```

### 2.2 타임스탬프 기반 탐색 (FR-013)

```cpp
// 14:35:00으로 이동 (1GB 파일에서 ~10ms)
int64_t target_ns = 1700400900000000000;  // 2025-11-19 14:35:00
reader.seekTime(target_ns);

// 해당 시간 이후 메시지 읽기
while (auto msg = reader.next()) {
    spdlog::info("Topic: {}, Value: {}", msg->topic, msg->serialized_value);
}
```

### 2.3 시간 범위 필터링 (FR-016)

```cpp
// 14:35:00 ~ 14:36:00 범위의 모든 메시지 추출
int64_t start_ns = 1700400900000000000;
int64_t end_ns   = 1700400960000000000;

auto messages = reader.getMessagesInRange(start_ns, end_ns);
spdlog::info("Found {} messages in range", messages.size());
```

---

## 3. Bag 파일 재생 (Replay)

### 3.1 BagReplayer로 회귀 테스트

```cpp
#include "core/logging/contracts/BagReplayer.h"

// BagReader 생성
auto reader = std::make_shared<BagReader>();
reader->open("logs/mission_2025-11-19_14-30-00.bag");

// BagReplayer 생성
BagReplayer replayer(reader);

// DataStore 생성 (테스트용 격리 인스턴스)
auto testDataStore = DataStore::createForTest();

// 재생 속도 2배속 설정 (FR-015)
replayer.setSpeedFactor(2.0);

// 불일치 감지 콜백 등록 (FR-025)
replayer.onMismatch([](const std::string& topic,
                        const std::string& expected,
                        const std::string& actual) {
    spdlog::error("Replay mismatch: {} expected={} actual={}",
                  topic, expected, actual);
});

// 재생 시작 (비동기)
replayer.replay(testDataStore);

// 진행률 모니터링
while (replayer.getState() == ReplayState::RUNNING) {
    spdlog::info("Replay progress: {:.1f}%", replayer.getProgress() * 100);
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// 결과 확인
spdlog::info("Replay completed: {} messages, {} mismatches",
             replayer.getMessagesReplayed(),
             replayer.getMismatchCount());
```

**결과:**
- DataStore 상태가 과거 실행 시점으로 재현됨 (FR-014)
- 재생 정확도 99% 이상 (SC-005)
- 불일치 발생 시 로그 기록 (FR-025)

---

## 4. 선택적 로깅 전략 (P2)

### 4.1 LoggingStrategy 설정

```cpp
// 전략 1: 전체 로깅 (기본값)
bagLogger->setStrategy("MissionState.*", LoggingStrategy::FULL_BAG);

// 전략 2: 순환 버퍼 (고빈도 데이터)
// 메모리에만 최근 1000개 보관, 디스크에 기록 안 함
bagLogger->setStrategy("SensorData.lidar", LoggingStrategy::MEMORY_ONLY);
bagLogger->setCircularBufferSize("SensorData.lidar", 1000);

// 전략 3: 이벤트 기반 로깅 (조건부)
// 오류 발생 시에만 기록
bagLogger->setStrategy("ErrorLog.*", LoggingStrategy::EVENT_DRIVEN);
bagLogger->setEventCondition("ErrorLog.*", [](const auto& msg) {
    return msg.data_type == DataType::STRING &&
           msg.serialized_value.find("ERROR") != std::string::npos;
});

// 전략 4: 로깅 비활성화
bagLogger->setStrategy("DebugInfo.*", LoggingStrategy::NONE);
```

### 4.2 순환 버퍼 데이터 조회

```cpp
// 메모리에 보관된 최근 100개 메시지 조회
auto recent = bagLogger->getRecentMessages("SensorData.lidar", 100);

for (const auto& msg : recent) {
    spdlog::info("[{}] {}: {}", msg.timestamp_ns, msg.topic, msg.serialized_value);
}
```

**효과:**
- 디스크 사용량 90% 감소 (384MB/hour → 24MB/hour, SC-003)
- 고빈도 센서 데이터는 메모리에만 보관
- 중요한 상태 변경만 Bag 파일에 기록

---

## 5. 파일 회전 및 보관 정책

### 5.1 파일 크기 기반 회전 (FR-004)

```cpp
// 1GB마다 자동 파일 회전
RotationPolicy sizePolicy;
sizePolicy.type = RotationType::SIZE;
sizePolicy.maxSizeMB = 1024;  // 1GB

bagLogger->setRotationPolicy(sizePolicy);
```

**결과:**
```
logs/mission_2025-11-19_14-30-00.bag  (1GB)
logs/mission_2025-11-19_14-45-00.bag  (1GB)
logs/mission_2025-11-19_15-00-00.bag  (500MB)
```

### 5.2 시간 기반 보관 정책 (FR-005)

```cpp
// 7일 이상 파일 자동 삭제
RetentionPolicy timePolicy;
timePolicy.type = RetentionType::TIME;
timePolicy.maxAgeDays = 7;

bagLogger->setRetentionPolicy(timePolicy);
```

**효과:**
- 디스크 공간 자동 관리
- 8일째 되는 파일은 자동 삭제 (SC-006)

---

## 6. 성능 모니터링

### 6.1 BagStats 조회 (FR-008)

```cpp
auto stats = bagLogger->getStats();

spdlog::info("Bag Statistics:");
spdlog::info("  Messages written: {}", stats.messagesWritten.load());
spdlog::info("  Messages dropped: {}", stats.messagesDropped.load());
spdlog::info("  Bytes written:    {} MB", stats.bytesWritten.load() / 1024 / 1024);
spdlog::info("  Write latency:    {} μs", stats.writeLatencyUs.load());
```

**성능 목표 (SC-001, SC-009):**
- DataStore 성능 저하 < 1% (87ns → 88ns)
- 쓰기 지연 < 1ms
- 메모리 사용량 < 10MB

---

## 7. 에러 처리

### 7.1 디스크 공간 부족 (FR-006, FR-023)

```cpp
// BagLogger는 디스크 공간 부족 시 자동으로:
// 1. 가장 오래된 Bag 파일 삭제
// 2. 그래도 부족하면 messagesDropped 증가
// 3. 통계에 기록

auto stats = bagLogger->getStats();
if (stats.messagesDropped.load() > 0) {
    spdlog::warn("Dropped {} messages due to disk space", stats.messagesDropped.load());
}
```

### 7.2 Bag 파일 손상 복구 (FR-024)

```cpp
BagReader reader;
reader.setRecoveryMode(true);  // 복구 모드 활성화 (기본값)
reader.open("logs/corrupted.bag");

// 손상된 JSONL 라인은 자동 스킵
while (auto msg = reader.next()) {
    // 읽을 수 있는 메시지만 처리
}
```

---

## 8. 회귀 테스트 자동화 예제

```cpp
#include "gtest/gtest.h"

TEST(RegressionTest, ReplayMission2025_11_19) {
    // Given: 과거 성공한 미션의 Bag 파일
    auto reader = std::make_shared<BagReader>();
    reader->open("logs/successful_mission.bag");

    BagReplayer replayer(reader);
    auto testDataStore = DataStore::createForTest();

    // When: Replay 실행
    std::atomic<uint64_t> mismatchCount{0};
    replayer.onMismatch([&](auto, auto, auto) { mismatchCount++; });
    replayer.replay(testDataStore);

    // Wait for completion
    while (replayer.getState() == ReplayState::RUNNING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Then: 불일치 없이 완료되어야 함
    EXPECT_EQ(mismatchCount.load(), 0);
    EXPECT_EQ(replayer.getState(), ReplayState::COMPLETED);
}
```

**결과:**
- 회귀 테스트 재현 정확도 99% (SC-005)
- 자동화된 검증으로 코드 변경 영향 확인

---

## 9. 의존성 및 빌드

### 9.1 CMakeLists.txt 설정

```cmake
# logging 모듈 추가
add_library(mxrc_logging
    src/core/logging/BagLogger.cpp
    src/core/logging/BagWriter.cpp
    src/core/logging/BagReader.cpp
    src/core/logging/BagReplayer.cpp
)

target_link_libraries(mxrc_logging
    PUBLIC
        mxrc_event        # EventBus
        mxrc_datastore    # DataStore
        nlohmann_json     # JSON 직렬화
        spdlog            # 로깅
)
```

### 9.2 빌드 명령어

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 테스트 실행
./run_tests --gtest_filter=BagLogger*
```

---

## 10. 참고 자료

- **전체 사양**: [specs/017-logging/spec.md](./spec.md)
- **데이터 모델**: [specs/017-logging/data-model.md](./data-model.md)
- **API 계약**:
  - [contracts/IBagWriter.h](./contracts/IBagWriter.h)
  - [contracts/BagReader.h](./contracts/BagReader.h)
  - [contracts/BagReplayer.h](./contracts/BagReplayer.h)
- **연구 문서**: [specs/017-logging/research.md](./research.md)
- **구현 계획**: [specs/017-logging/plan.md](./plan.md)

---

## 11. FAQ

### Q1: Bag 파일은 어디에 저장되나요?
**A**: 기본적으로 `logs/` 디렉토리에 타임스탬프 기반 파일명으로 저장됩니다.
예: `logs/mission_2025-11-19_14-30-00.bag`

### Q2: 실시간 성능에 영향을 주나요?
**A**: 아니오. EventBus 기반 비동기 로깅으로 DataStore 성능 저하 < 1% (SC-001)

### Q3: Bag 파일이 손상되면 어떻게 되나요?
**A**: BagReader의 복구 모드가 손상된 라인을 스킵하고, 읽을 수 있는 메시지만 처리합니다 (FR-024)

### Q4: Python으로 Bag 파일을 분석할 수 있나요?
**A**: 네, JSONL 포맷이므로 Python에서 쉽게 읽을 수 있습니다:
```python
import json
with open('mission.bag', 'r') as f:
    for line in f:
        msg = json.loads(line)
        print(msg['timestamp_ns'], msg['topic'], msg['serialized_value'])
```

### Q5: 디스크 공간이 부족하면 어떻게 되나요?
**A**: 자동으로 오래된 파일을 삭제하고, 그래도 부족하면 새 메시지를 드롭합니다 (FR-006, FR-023)

---

**다음 단계**: [구현 계획(plan.md)](./plan.md)을 참조하여 Phase 2-4 구현을 진행하세요.
