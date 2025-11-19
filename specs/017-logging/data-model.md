# 데이터 모델: 강력한 로깅 시스템

**기능**: Bag Logging & Replay Infrastructure
**날짜**: 2025-11-19
**관련 문서**: `spec.md`, `research.md`

---

## 개요

이 문서는 Bag 로깅 시스템의 핵심 데이터 모델을 정의합니다. 모든 모델은 기능 사양서의 주요 엔티티를 바탕으로 설계되었으며, 연구 결과를 반영합니다.

---

## 1. BagMessage (메시지 구조체)

**목적**: Bag 파일에 저장되는 단일 메시지 단위

**필드**:

| 필드명 | 타입 | 필수 | 설명 | 검증 규칙 |
|--------|------|------|------|----------|
| `timestamp_ns` | `int64_t` | ✅ | 나노초 정밀도 Unix 타임스탬프 | > 0 |
| `topic` | `std::string` | ✅ | DataStore ID (예: "mission_state") | 비어있지 않음, 최대 256자 |
| `data_type` | `DataType` (enum) | ✅ | 데이터 유형 구분 | 유효한 DataType 값 |
| `serialized_value` | `std::string` | ✅ | JSON 직렬화된 값 | 유효한 JSON 문자열 |

**상태 전이**: 해당 없음 (불변 메시지)

**관계**:
- BagWriter → BagMessage (1:N, 쓰기)
- BagReader → BagMessage (1:N, 읽기)

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

struct BagMessage {
    int64_t timestamp_ns;
    std::string topic;
    DataType data_type;
    std::string serialized_value;

    // JSON 직렬화 (nlohmann::json 사용)
    nlohmann::json toJson() const {
        return {
            {"timestamp", timestamp_ns},
            {"topic", topic},
            {"type", dataTypeToString(data_type)},
            {"value", nlohmann::json::parse(serialized_value)}
        };
    }

    // JSON 역직렬화
    static BagMessage fromJson(const nlohmann::json& j) {
        BagMessage msg;
        msg.timestamp_ns = j["timestamp"];
        msg.topic = j["topic"];
        msg.data_type = stringToDataType(j["type"]);
        msg.serialized_value = j["value"].dump();
        return msg;
    }

    // 검증
    bool isValid() const {
        return timestamp_ns > 0 && !topic.empty() &&
               topic.size() <= 256 && !serialized_value.empty();
    }
};

} // namespace
```

---

## 2. LoggingStrategy (로깅 전략 열거형)

**목적**: DataType별 차별화된 로깅 전략 정의

**값**:

| 전략 | 설명 | 사용 사례 |
|------|------|---------|
| `NONE` | 로깅 안 함 | Config, Para (변경 빈도 매우 낮음) |
| `MEMORY_ONLY` | 순환 버퍼 사용 (메모리만) | InterfaceData (고빈도, 최근 N개만 필요) |
| `EVENT_DRIVEN` | 조건부 로깅 | Alarm (특정 severity 이상만) |
| `FULL_BAG` | 전체 로깅 | MissionState, TaskState (중요, 전체 이력 필요) |

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

enum class LoggingStrategy {
    NONE,           // 로깅 안 함
    MEMORY_ONLY,    // 순환 버퍼 사용 (디스크 쓰기 없음)
    EVENT_DRIVEN,   // 조건부 로깅 (predicate 함수 평가)
    FULL_BAG        // 모든 변경사항 Bag 파일에 기록
};

// 문자열 변환
inline std::string loggingStrategyToString(LoggingStrategy strategy) {
    switch (strategy) {
        case LoggingStrategy::NONE: return "NONE";
        case LoggingStrategy::MEMORY_ONLY: return "MEMORY_ONLY";
        case LoggingStrategy::EVENT_DRIVEN: return "EVENT_DRIVEN";
        case LoggingStrategy::FULL_BAG: return "FULL_BAG";
        default: return "UNKNOWN";
    }
}

inline LoggingStrategy stringToLoggingStrategy(const std::string& s) {
    if (s == "NONE") return LoggingStrategy::NONE;
    if (s == "MEMORY_ONLY") return LoggingStrategy::MEMORY_ONLY;
    if (s == "EVENT_DRIVEN") return LoggingStrategy::EVENT_DRIVEN;
    if (s == "FULL_BAG") return LoggingStrategy::FULL_BAG;
    throw std::invalid_argument("Unknown LoggingStrategy: " + s);
}

} // namespace
```

---

## 3. BagStats (로깅 통계)

**목적**: Bag Writer의 성능 및 상태 모니터링

**필드**:

| 필드명 | 타입 | 설명 |
|--------|------|------|
| `messagesWritten` | `std::atomic<uint64_t>` | 성공적으로 쓴 메시지 수 |
| `messagesDropped` | `std::atomic<uint64_t>` | 큐 가득 차서 드롭된 메시지 수 |
| `bytesWritten` | `std::atomic<uint64_t>` | 총 쓴 바이트 수 |
| `writeLatencyUs` | `std::atomic<uint64_t>` | 평균 쓰기 레이턴시 (마이크로초) |

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

struct BagStats {
    std::atomic<uint64_t> messagesWritten{0};
    std::atomic<uint64_t> messagesDropped{0};
    std::atomic<uint64_t> bytesWritten{0};
    std::atomic<uint64_t> writeLatencyUs{0};

    // 스냅샷 생성 (atomic 읽기)
    struct Snapshot {
        uint64_t messagesWritten;
        uint64_t messagesDropped;
        uint64_t bytesWritten;
        uint64_t writeLatencyUs;

        double getDropRate() const {
            uint64_t total = messagesWritten + messagesDropped;
            return total > 0 ? static_cast<double>(messagesDropped) / total : 0.0;
        }
    };

    Snapshot getSnapshot() const {
        return {
            messagesWritten.load(std::memory_order_relaxed),
            messagesDropped.load(std::memory_order_relaxed),
            bytesWritten.load(std::memory_order_relaxed),
            writeLatencyUs.load(std::memory_order_relaxed)
        };
    }
};

} // namespace
```

---

## 4. RotationPolicy (파일 순환 정책)

**목적**: Bag 파일 크기 제한 및 순환 규칙 정의

**필드**:

| 필드명 | 타입 | 기본값 | 설명 |
|--------|------|--------|------|
| `type` | `RotationType` (enum) | `SIZE` | 순환 조건 (SIZE 또는 TIME) |
| `maxSizeBytes` | `uint64_t` | 1GB | SIZE 타입 시 최대 파일 크기 |
| `maxDurationSeconds` | `uint64_t` | 3600 (1시간) | TIME 타입 시 최대 기록 시간 |

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

enum class RotationType {
    SIZE,    // 파일 크기 기반 순환
    TIME     // 시간 기반 순환 (예: 1시간마다)
};

struct RotationPolicy {
    RotationType type = RotationType::SIZE;
    uint64_t maxSizeBytes = 1ULL * 1024 * 1024 * 1024;  // 1GB
    uint64_t maxDurationSeconds = 3600;  // 1시간

    // 순환 조건 확인
    bool shouldRotate(uint64_t currentSizeBytes, uint64_t elapsedSeconds) const {
        if (type == RotationType::SIZE) {
            return currentSizeBytes >= maxSizeBytes;
        } else {
            return elapsedSeconds >= maxDurationSeconds;
        }
    }
};

} // namespace
```

---

## 5. RetentionPolicy (보존 정책)

**목적**: 오래된 Bag 파일 자동 삭제 규칙 정의

**필드**:

| 필드명 | 타입 | 기본값 | 설명 |
|--------|------|--------|------|
| `type` | `RetentionType` (enum) | `TIME` | 보존 조건 (TIME 또는 COUNT) |
| `maxAgeDays` | `uint32_t` | 7일 | TIME 타입 시 최대 보존 기간 |
| `maxFileCount` | `uint32_t` | 100 | COUNT 타입 시 최대 파일 수 |

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

enum class RetentionType {
    TIME,    // 시간 기반 보존 (N일 이상 경과 시 삭제)
    COUNT    // 개수 기반 보존 (N개 초과 시 가장 오래된 파일 삭제)
};

struct RetentionPolicy {
    RetentionType type = RetentionType::TIME;
    uint32_t maxAgeDays = 7;      // 7일
    uint32_t maxFileCount = 100;  // 100개

    // 삭제 조건 확인
    bool shouldDelete(const std::chrono::system_clock::time_point& fileTime,
                      size_t totalFileCount, size_t fileIndex) const {
        if (type == RetentionType::TIME) {
            auto now = std::chrono::system_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - fileTime).count();
            return age >= (maxAgeDays * 24);
        } else {
            // 파일은 오래된 순으로 정렬되어 있다고 가정
            return totalFileCount > maxFileCount && fileIndex < (totalFileCount - maxFileCount);
        }
    }
};

} // namespace
```

---

## 6. CircularBuffer<T> (순환 버퍼)

**목적**: MEMORY_ONLY 전략을 위한 고정 크기 순환 버퍼

**필드**:

| 필드명 | 타입 | 설명 |
|--------|------|------|
| `capacity_` | `size_t` | 최대 저장 가능 개수 |
| `buffer_` | `std::deque<T>` | 데이터 저장소 (FIFO) |
| `mutex_` | `std::mutex` | 스레드 안전성 보장 |

**주요 동작**:
- `push()`: 데이터 추가, 용량 초과 시 가장 오래된 데이터 자동 제거 (FIFO)
- `getLast(count)`: 최근 N개 데이터 조회
- `clear()`: 모든 데이터 제거

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

template<typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(size_t capacity = 1000)
        : capacity_(capacity) {}

    // 데이터 추가 (FIFO, 오버플로우 시 자동 제거)
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(item);
        if (buffer_.size() > capacity_) {
            buffer_.pop_front();  // 가장 오래된 항목 제거
        }
    }

    // 최근 N개 데이터 조회
    std::vector<T> getLast(size_t count) const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t start = buffer_.size() > count ? buffer_.size() - count : 0;
        return std::vector<T>(buffer_.begin() + start, buffer_.end());
    }

    // 전체 데이터 조회
    std::vector<T> getAll() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<T>(buffer_.begin(), buffer_.end());
    }

    // 버퍼 초기화
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.clear();
    }

    // 현재 크기
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

    // 용량
    size_t capacity() const {
        return capacity_;
    }

private:
    const size_t capacity_;
    std::deque<T> buffer_;
    mutable std::mutex mutex_;
};

} // namespace
```

---

## 7. IndexEntry (Bag 파일 인덱스 엔트리)

**목적**: Bag 파일 내 타임스탬프 기반 빠른 탐색을 위한 인덱스

**필드**:

| 필드명 | 타입 | 설명 |
|--------|------|------|
| `timestamp_ns` | `int64_t` | 나노초 정밀도 타임스탬프 |
| `file_offset` | `uint64_t` | 파일 내 바이트 오프셋 |

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

struct IndexEntry {
    int64_t timestamp_ns;    // 나노초 타임스탬프
    uint64_t file_offset;    // 파일 오프셋 (바이트)
} __attribute__((packed));   // 16 bytes, 캐시 친화적

static_assert(sizeof(IndexEntry) == 16, "IndexEntry must be 16 bytes");

} // namespace
```

---

## 8. BagFooter (Bag 파일 Footer)

**목적**: Bag 파일 메타데이터 및 인덱스 위치 정보

**필드**:

| 필드명 | 타입 | 크기 | 설명 |
|--------|------|------|------|
| `magic` | `char[8]` | 8B | "MXRC_BAG" 매직 넘버 (검증용) |
| `data_size` | `uint64_t` | 8B | 데이터 섹션 크기 (바이트) |
| `index_offset` | `uint64_t` | 8B | 인덱스 섹션 시작 오프셋 |
| `index_count` | `uint32_t` | 4B | 인덱스 엔트리 개수 |
| `checksum` | `uint32_t` | 4B | CRC32 체크섬 |
| `reserved` | `uint8_t[32]` | 32B | 향후 확장용 |

**전체 크기**: 64 bytes (파일 끝에 위치)

**C++ 정의**:
```cpp
namespace mxrc::core::logging {

struct BagFooter {
    char magic[8];           // "MXRC_BAG"
    uint64_t data_size;      // 데이터 섹션 크기
    uint64_t index_offset;   // 인덱스 섹션 오프셋
    uint32_t index_count;    // 인덱스 엔트리 수
    uint32_t checksum;       // CRC32 체크섬
    uint8_t reserved[32];    // 확장용

    bool isValid() const {
        return std::string(magic, 8) == "MXRC_BAG";
    }
} __attribute__((packed));

static_assert(sizeof(BagFooter) == 64, "BagFooter must be 64 bytes");

} // namespace
```

---

## 데이터 흐름 다이어그램

```
┌──────────────────┐
│  DataStore       │
│  (변경 발생)      │
└────────┬─────────┘
         │ notifySubscribers()
         ↓
┌──────────────────────────┐
│  DataStoreEventAdapter   │
│  (EventBus에 이벤트 발행) │
└────────┬─────────────────┘
         │ DataStoreValueChangedEvent
         ↓
┌──────────────────────────┐
│  EventBus                │
│  (비동기 디스패치)         │
└────────┬─────────────────┘
         │
         ↓
┌──────────────────────────┐
│  DataStoreBagLogger      │
│  (로깅 전략 적용)          │
└────────┬─────────────────┘
         │
         ├─ FULL_BAG ──────────> BagMessage ──> AsyncWriter ──> Bag File (.jsonl)
         │
         ├─ MEMORY_ONLY ──────> CircularBuffer (최근 1000개 메모리)
         │
         ├─ EVENT_DRIVEN ─────> Predicate 평가 ──> BagMessage (조건부)
         │
         └─ NONE ─────────────> (로깅 안 함)
```

---

## 검증 규칙 요약

| 엔티티 | 검증 규칙 |
|--------|---------|
| BagMessage | `timestamp_ns > 0`, `topic` 비어있지 않음, 최대 256자 |
| LoggingStrategy | 유효한 enum 값 (NONE, MEMORY_ONLY, EVENT_DRIVEN, FULL_BAG) |
| RotationPolicy | `maxSizeBytes > 0`, `maxDurationSeconds > 0` |
| RetentionPolicy | `maxAgeDays > 0`, `maxFileCount > 0` |
| IndexEntry | `timestamp_ns > 0`, `file_offset >= 128` (헤더 이후) |
| BagFooter | `magic == "MXRC_BAG"`, `index_count > 0` |

---

## 메모리 사용량 추정

| 구성 요소 | 개수 | 단위 크기 | 총 메모리 |
|----------|------|----------|----------|
| BagMessage (큐) | 10,000 | ~200B | ~2 MB |
| CircularBuffer (InterfaceData) | 1,000 | ~100B | ~100 KB |
| IndexEntry (메모리 로드) | 28,800 | 16B | ~460 KB |
| BagStats | 1 | ~32B | ~32 B |
| **총 메모리** | - | - | **~3 MB** |

---

## 다음 단계

✅ **완료**: data-model.md
**다음**: contracts/ API 계약 작성
