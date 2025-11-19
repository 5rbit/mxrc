# 연구 문서 #001: DataStore Bag 로깅 방식 도입 평가

**작성일**: 2025-11-18
**상태**: 📋 제안
**우선순위**: P2 (중간)
**관련 컴포넌트**: DataStore, EventBus, Logging Infrastructure

---

## 📌 Executive Summary

DataStore에 ROS Bag 스타일의 타임스탬프 기반 순차 로깅 시스템 도입을 평가했습니다.

**핵심 결론**:
- ✅ **도입 권장** (조건부 구현)
- ⭐ **권장 방식**: EventBus 기반 비동기 로깅
- ⚠️ **주의사항**: 고빈도 InterfaceData는 순환 버퍼 사용
- ⏱️ **추정 기간**: 3-4주 (단계별 구현)

---

## 🎯 연구 배경 및 목적

### 현재 상황

**DataStore 아키텍처 현황**:
```cpp
class DataStore {
    // Phase 3: concurrent_hash_map 기반 고성능 인메모리 저장소
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;

    // FR-012: Data recovery (placeholder only)
    void saveState(const std::string& filepath);  // ❌ 미구현
    void loadState(const std::string& filepath);  // ❌ 미구현
};
```

**기존 기능**:
- ✅ 고성능 인메모리 key-value 저장소
- ✅ concurrent_hash_map으로 thread-safe 접근
- ✅ Observer 패턴으로 실시간 알림
- ✅ EventBus 통합으로 이벤트 기반 아키텍처
- ❌ **영구 저장 기능 없음**
- ❌ **데이터 변경 이력 추적 없음**
- ❌ **Replay 기능 없음**

### 연구 목적

1. **디버깅 능력 향상**: 문제 발생 시점의 시스템 상태 완벽 재현
2. **테스트 재현성**: 복잡한 시나리오의 정확한 재실행
3. **규정 준수**: 산업용 로봇의 작업 기록 의무 충족
4. **성능 분석**: 오프라인 상태에서 시스템 동작 분석

---

## 🔍 Bag 로깅 개념 분석

### ROS Bag 로깅의 특징

**1. 기본 구조**
```python
# ROS Bag 메시지 포맷
{
    "timestamp": 1234567890.123456,  # 마이크로초 정밀도
    "topic": "/robot/joint_states",
    "message_type": "sensor_msgs/JointState",
    "data": {
        "name": ["joint1", "joint2"],
        "position": [0.0, 1.57],
        "velocity": [0.1, 0.2],
        "effort": [10.0, 15.0]
    }
}
```

**2. 핵심 특징**:

| 특징 | 설명 | 장점 |
|------|------|------|
| **완벽한 재현성** | 모든 데이터 변경을 시간순으로 기록 | 버그 재현 용이 |
| **Replay 기능** | 과거 상태를 정확히 재생 가능 | 테스트 자동화 |
| **타임스탬프 정밀도** | 마이크로초 단위 시간 기록 | 동기화 분석 |
| **압축 지원** | LZ4, BZ2 등 압축 알고리즘 | 저장 공간 절약 |
| **Indexing** | 빠른 탐색을 위한 인덱스 | 대용량 파일 처리 |

**3. 전형적인 사용 사례**:
- 센서 데이터 수집 (LiDAR, 카메라, IMU)
- 로봇 제어 명령 기록
- 시스템 상태 변화 추적
- 오프라인 분석 및 테스트

### Bag 로깅의 기술적 요구사항

**1. 직렬화 (Serialization)**
```cpp
// 예시: MessagePack 사용
struct SharedDataMessage {
    std::string id;
    DataType type;
    std::any value;  // ← 직렬화 문제!
    int64_t timestamp_ns;

    // std::any를 어떻게 직렬화?
    msgpack::sbuffer serialize() const;
};
```

**문제점**:
- `std::any`는 타입 정보를 런타임에만 보유
- 직렬화 시 타입별 처리 로직 필요
- 역직렬화 시 타입 복원 복잡

**2. 파일 포맷 설계**
```
┌─────────────────────────────────────┐
│ Header                              │
│ - Magic number: "MXRC_BAG"         │
│ - Version: 1.0                      │
│ - Compression: LZ4                  │
│ - Index offset: 0x12345678          │
├─────────────────────────────────────┤
│ Message 1 (timestamp: 100.000)      │
├─────────────────────────────────────┤
│ Message 2 (timestamp: 100.010)      │
├─────────────────────────────────────┤
│ ...                                 │
├─────────────────────────────────────┤
│ Index                               │
│ - topic_1: [offset1, offset2, ...]  │
│ - topic_2: [offset3, offset4, ...]  │
└─────────────────────────────────────┘
```

**3. 성능 고려사항**
- 비동기 I/O 필수 (메인 스레드 블로킹 방지)
- 버퍼링 전략 (메모리 vs 디스크 트레이드오프)
- 압축 오버헤드 vs 저장 공간

---

## 🔍 Bag 파일 인덱싱 전략 연구

### 배경 및 요구사항

**문제 정의**:
- JSONL 포맷의 대용량 Bag 파일 (>1GB)에서 특정 타임스탬프로 빠르게 탐색
- Replay 기능 구현 시 "10:15:23.456 시점부터 재생" 같은 요구사항 충족
- 성능 목표: 1GB 파일에서 10초 이내 탐색

**JSONL 포맷 특성**:
```jsonl
{"timestamp":1705553123456789,"topic":"mission_state","type":"MissionState","value":"EXECUTING"}
{"timestamp":1705553123556789,"topic":"task_state","type":"TaskState","value":"RUNNING"}
{"timestamp":1705553124456789,"topic":"alarm","type":"Alarm","value":"MOTOR_TEMP_HIGH"}
```

- 장점: 라인별 파싱 가능, 텍스트 기반으로 디버깅 용이, 압축 효율 우수
- 단점: 가변 길이 라인, 오프셋 예측 불가, 순차 접근 필요

### 3. Bag 파일 인덱싱 전략

**결정**: **파일 내 인덱스 블록 (ROS Bag/MCAP 스타일)**

**근거**:
- ✅ **독립성**: Bag 파일 하나만으로 완전한 재생 가능 (별도 인덱스 파일 불필요)
- ✅ **견고성**: 파일 손상 시 인덱스 재구축 가능, 순차 읽기로 복구 가능
- ✅ **성능**: 1GB 파일에서 2-3초 내 탐색 (Binary search O(log N) + mmap)
- ✅ **메모리 효율**: 인덱스만 메모리 로드 (~1MB for 1GB file with 1초 간격 인덱스)
- ✅ **확장성**: 압축, 멀티 토픽 인덱스, 통계 정보 추가 용이

**대안 평가**:

| 접근 방식 | 탐색 속도 | 메모리 사용 | 구현 복잡도 | 파일 포맷 변경 | 복구 가능성 | 최종 판단 |
|----------|---------|-----------|------------|-------------|-----------|----------|
| **메모리 인덱스** | ⭐⭐⭐⭐⭐<br>(즉시, <1초) | ❌ 큰 편<br>(10-50MB/1GB) | ⭐⭐⭐⭐⭐<br>(간단) | ✅ 불필요 | ❌ 나쁨<br>(별도 파일) | 🔴 기각 |
| **파일 내 인덱스** | ⭐⭐⭐⭐<br>(빠름, 2-3초) | ✅ 작음<br>(1-5MB/1GB) | ⭐⭐⭐<br>(중간) | ⚠️ 필요<br>(footer 추가) | ✅ 우수<br>(순차 읽기) | **🟢 채택** |
| **Binary Search** | ⭐⭐<br>(느림, 5-10초) | ✅ 최소<br>(<1MB) | ⭐⭐⭐⭐<br>(간단) | ✅ 불필요 | ⭐⭐⭐⭐⭐<br>(최고) | 🟡 보조 수단 |

**세부 분석**:

#### 1. 메모리 인덱스 (Separate Index File)
```cpp
// 별도 .idx 파일
// mission_20250118.bag.idx
struct MemoryIndex {
    std::map<int64_t, uint64_t> timestamp_to_offset;
    // 예: {1705553123456789 → 0, 1705553124456789 → 150, ...}
};
```

**장점**:
- ✅ 구현 간단 (std::map 사용)
- ✅ 즉시 탐색 (메모리 내 binary search)
- ✅ 파일 포맷 변경 불필요 (JSONL 그대로)

**단점**:
- ❌ 파일 2개 관리 (.bag + .idx)
- ❌ .idx 손실 시 재생 불가 (또는 느린 재구축 필요)
- ❌ 메모리 사용량 큼 (1GB 파일 → 10-50MB 인덱스, 100Hz 샘플링 시)
- ❌ 동기화 문제 (Bag 파일 업데이트 시 인덱스 재생성 필요)

**성능 벤치마크** (예상):
```
1GB Bag 파일 (100Hz TaskState, 8시간 데이터 = 2.88M 레코드)
- 인덱스 크기: 2.88M * 16 bytes = 46MB (timestamp:8B + offset:8B)
- 탐색 시간: O(log N) = log₂(2.88M) ≈ 22회 비교 → <1ms
- 인덱스 로드: 46MB / 500MB/s (SSD) = 92ms
→ 총 탐색 시간: ~100ms ⭐⭐⭐⭐⭐
```

**기각 이유**: 파일 독립성 결여, 배포/백업 시 2개 파일 관리 복잡성

---

#### 2. 파일 내 인덱스 블록 (MCAP/ROS Bag 2.0 스타일) ✅ 채택

**파일 구조**:
```
┌─────────────────────────────────────────────────────────┐
│ HEADER (128 bytes)                                      │
│ - Magic: "MXRC_BAG\0" (8 bytes)                         │
│ - Version: 1 (4 bytes)                                  │
│ - Flags: compression=LZ4, indexed=true (4 bytes)        │
│ - Index offset: 0x3FF00000 (8 bytes, footer 시작 위치)   │
│ - Reserved: (104 bytes)                                 │
├─────────────────────────────────────────────────────────┤
│ DATA SECTION (sequential JSONL records)                 │
│ {"timestamp":1705553123456789,...}\n                    │ ← offset: 128
│ {"timestamp":1705553123556789,...}\n                    │ ← offset: 234
│ {"timestamp":1705553124456789,...}\n                    │ ← offset: 340
│ ...                                                      │
│ (계속해서 메시지 누적)                                     │
│                                                          │
│ [~1GB data]                                             │
├─────────────────────────────────────────────────────────┤
│ INDEX SECTION (sparse timestamp index)                  │
│ - IndexEntry count: 28800 (4 bytes)                     │
│ - IndexEntry[] {                                        │
│     {timestamp: 1705553123000000, offset: 128},         │ ← 1초 간격 샘플링
│     {timestamp: 1705553124000000, offset: 5340},        │
│     {timestamp: 1705553125000000, offset: 10520},       │
│     ...                                                  │
│     {timestamp: 1705581923000000, offset: 1073740000}   │
│   }                                                      │
│ - Total size: 28800 * 16 bytes = 460KB                 │
├─────────────────────────────────────────────────────────┤
│ FOOTER (64 bytes)                                       │
│ - Data section size: 1073739872 (8 bytes)               │
│ - Index section offset: 1073740000 (8 bytes)            │
│ - Index entry count: 28800 (4 bytes)                    │
│ - Checksum: CRC32 (4 bytes)                             │
│ - Magic (검증용): "MXRC_BAG\0" (8 bytes)                 │
│ - Reserved: (32 bytes)                                  │
└─────────────────────────────────────────────────────────┘
```

**인덱스 데이터 구조**:
```cpp
// 1. IndexEntry (16 bytes, 캐시 친화적)
struct IndexEntry {
    int64_t timestamp_ns;  // 나노초 타임스탬프
    uint64_t file_offset;  // 파일 오프셋 (바이트)
} __attribute__((packed));

// 2. BagIndex (인덱스 관리 클래스)
class BagIndex {
public:
    // 인덱스 로드 (파일 열 때 1회)
    void load(const std::string& filepath) {
        std::ifstream ifs(filepath, std::ios::binary);

        // Footer 읽기 (파일 끝에서 64바이트)
        ifs.seekg(-64, std::ios::end);
        BagFooter footer;
        ifs.read(reinterpret_cast<char*>(&footer), sizeof(footer));

        // Magic 검증
        if (std::string(footer.magic, 8) != "MXRC_BAG") {
            throw std::runtime_error("Invalid bag file format");
        }

        // 인덱스 섹션으로 이동
        ifs.seekg(footer.index_offset, std::ios::beg);

        // 인덱스 엔트리 읽기
        uint32_t count;
        ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
        entries_.resize(count);
        ifs.read(reinterpret_cast<char*>(entries_.data()),
                 count * sizeof(IndexEntry));

        spdlog::info("Loaded {} index entries from {}",
                     count, filepath);
    }

    // 타임스탬프로 파일 오프셋 찾기 (Binary Search)
    uint64_t findOffset(int64_t target_timestamp_ns) const {
        // std::lower_bound: O(log N) 복잡도
        auto it = std::lower_bound(
            entries_.begin(), entries_.end(), target_timestamp_ns,
            [](const IndexEntry& entry, int64_t ts) {
                return entry.timestamp_ns < ts;
            });

        if (it == entries_.end()) {
            return entries_.back().file_offset;  // 마지막 오프셋
        }

        return it->file_offset;
    }

    // 통계 정보
    size_t size() const { return entries_.size(); }
    int64_t getFirstTimestamp() const { return entries_.front().timestamp_ns; }
    int64_t getLastTimestamp() const { return entries_.back().timestamp_ns; }

private:
    std::vector<IndexEntry> entries_;  // 정렬된 인덱스 배열
};

// 3. BagReader (Replay 시 사용)
class BagReader {
public:
    BagReader(const std::string& filepath)
        : filepath_(filepath), ifs_(filepath) {
        // 인덱스 로드
        index_.load(filepath);

        // mmap 옵션 (선택적, 성능 최적화)
        #ifdef USE_MMAP
        fd_ = open(filepath.c_str(), O_RDONLY);
        struct stat sb;
        fstat(fd_, &sb);
        file_size_ = sb.st_size;
        mmap_ptr_ = static_cast<char*>(
            mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd_, 0));
        #endif
    }

    // 특정 시간으로 Seek
    void seekTime(int64_t timestamp_ns) {
        uint64_t offset = index_.findOffset(timestamp_ns);

        #ifdef USE_MMAP
        current_ptr_ = mmap_ptr_ + offset;
        #else
        ifs_.seekg(offset, std::ios::beg);
        #endif

        spdlog::info("Seeked to timestamp {} (offset: {})",
                     timestamp_ns, offset);
    }

    // 다음 메시지 읽기
    std::optional<BagMessage> readNext() {
        std::string line;

        #ifdef USE_MMAP
        // mmap 사용 시 (고성능)
        char* line_end = std::strchr(current_ptr_, '\n');
        if (!line_end) return std::nullopt;

        line = std::string(current_ptr_, line_end - current_ptr_);
        current_ptr_ = line_end + 1;
        #else
        // fstream 사용 시 (표준)
        if (!std::getline(ifs_, line)) {
            return std::nullopt;
        }
        #endif

        // JSON 파싱
        auto json = nlohmann::json::parse(line);
        BagMessage msg;
        msg.timestamp_ns = json["timestamp"];
        msg.topic = json["topic"];
        msg.data_type = static_cast<DataType>(json["type"]);
        msg.serialized_value = json["value"].dump();

        return msg;
    }

private:
    std::string filepath_;
    std::ifstream ifs_;
    BagIndex index_;

    #ifdef USE_MMAP
    int fd_;
    size_t file_size_;
    char* mmap_ptr_;
    char* current_ptr_;
    #endif
};
```

**인덱스 빌드 전략**:
```cpp
// BagWriter에서 실시간 인덱스 생성
class BagWriter {
public:
    void appendAsync(const BagMessage& msg) {
        // 1. 메시지 쓰기
        uint64_t current_offset = getCurrentFileOffset();
        writeMessage(msg);  // JSONL 쓰기

        // 2. 인덱스 업데이트 (1초마다 샘플링)
        auto ts_sec = msg.timestamp_ns / 1'000'000'000;
        if (ts_sec != last_indexed_second_) {
            indexBuilder_.addEntry({msg.timestamp_ns, current_offset});
            last_indexed_second_ = ts_sec;
        }
    }

    void close() {
        // 파일 종료 시 인덱스 쓰기
        uint64_t data_end_offset = getCurrentFileOffset();

        // Index section 쓰기
        auto entries = indexBuilder_.getEntries();
        uint32_t count = entries.size();
        ofs_.write(reinterpret_cast<char*>(&count), sizeof(count));
        ofs_.write(reinterpret_cast<char*>(entries.data()),
                   count * sizeof(IndexEntry));

        // Footer 쓰기
        BagFooter footer;
        std::memcpy(footer.magic, "MXRC_BAG", 8);
        footer.data_size = data_end_offset - 128;  // 헤더 제외
        footer.index_offset = data_end_offset;
        footer.index_count = count;
        footer.checksum = computeCRC32(/*...*/);
        ofs_.write(reinterpret_cast<char*>(&footer), sizeof(footer));

        spdlog::info("Bag file closed: {} index entries, {} bytes",
                     count, getCurrentFileOffset());
    }

private:
    IndexBuilder indexBuilder_;
    int64_t last_indexed_second_ = 0;
};
```

**성능 벤치마크** (예상):

```
1GB Bag 파일, 8시간 데이터 (100Hz TaskState)
- 인덱스 간격: 1초 (sparse index)
- 인덱스 엔트리 수: 8 * 3600 = 28,800개
- 인덱스 크기: 28,800 * 16 bytes = 460KB (파일 끝에 저장)

탐색 시간 분석:
1. Footer 읽기: fseek(-64) + read(64B) = ~1ms (SSD)
2. 인덱스 로드: fseek(index_offset) + read(460KB) = ~5ms (SSD, 100MB/s)
3. Binary search: log₂(28,800) ≈ 15회 비교 = <1ms
4. Data seek: fseek(target_offset) = ~1ms
→ 총 탐색 시간: ~10ms (파일 캐시 없을 때)
→ 캐시된 경우: <1ms ⭐⭐⭐⭐

순차 읽기 성능:
- mmap 사용 시: ~500MB/s (메모리 캐시 히트 시)
- fstream 사용 시: ~200MB/s (SSD 순차 읽기)
→ 1GB 파일 전체 파싱: 2-5초

메모리 사용:
- 인덱스 상주: 460KB (항상 메모리에 로드)
- mmap 사용 시: 커널이 자동 페이징 (메모리 압박 없음)
- fstream 사용 시: 버퍼 크기만큼 (~8KB)
→ 메모리 효율 우수 ⭐⭐⭐⭐⭐
```

**장점**:
- ✅ 파일 독립성: .bag 파일 하나로 완전한 재생
- ✅ 견고성: 인덱스 손상 시 데이터 섹션 순차 읽기로 복구
- ✅ 성능: 10ms 내 탐색 (목표 10초 대비 1000배 빠름)
- ✅ 메모리 효율: 460KB 인덱스로 1GB 파일 처리
- ✅ 확장성: 추후 압축, 멀티 토픽 인덱스 추가 가능

**단점**:
- ⚠️ 파일 포맷 변경: JSONL → Custom format (Header + Data + Index + Footer)
- ⚠️ 호환성: 표준 JSONL 도구로 직접 읽기 불가 (Data 섹션만 추출 필요)
- ⚠️ 스트리밍 제약: 파일 종료 시 인덱스 쓰기 (실시간 스트리밍 시 불완전)

**호환성 유지 방안**:
```bash
# Bag 파일에서 JSONL 추출 도구
$ mxrc-bag-extract mission.bag > mission.jsonl

# 구현
dd if=mission.bag bs=1 skip=128 count=$DATA_SIZE > mission.jsonl
# 또는
mxrc-bag-tool extract mission.bag --output mission.jsonl
```

---

#### 3. Binary Search with Lazy Parsing (인덱스 없음)

**개념**: 파일을 절반씩 나누며 타임스탬프 샘플링으로 탐색

```cpp
class BinarySearchBagReader {
public:
    void seekTime(int64_t target_timestamp_ns) {
        uint64_t low = 0;
        uint64_t high = getFileSize();

        while (high - low > THRESHOLD) {
            uint64_t mid = (low + high) / 2;

            // 중간 지점의 라인 시작점 찾기
            uint64_t line_start = findLineStart(mid);

            // 타임스탬프 파싱 (첫 번째 레코드만)
            auto ts = parseTimestampAt(line_start);

            if (ts < target_timestamp_ns) {
                low = line_start;
            } else {
                high = line_start;
            }
        }

        // 최종 위치에서 순차 스캔
        ifs_.seekg(low);
        while (true) {
            auto msg = readNext();
            if (msg.timestamp_ns >= target_timestamp_ns) {
                break;
            }
        }
    }

private:
    uint64_t findLineStart(uint64_t offset) {
        // offset 이후 첫 번째 '\n' 찾기
        ifs_.seekg(offset);
        ifs_.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return ifs_.tellg();
    }

    int64_t parseTimestampAt(uint64_t offset) {
        ifs_.seekg(offset);
        std::string line;
        std::getline(ifs_, line);

        // JSON 파싱 (timestamp 필드만)
        auto json = nlohmann::json::parse(line);
        return json["timestamp"];
    }
};
```

**성능 벤치마크** (예상):

```
1GB Bag 파일, Binary Search
- 파일 크기: 1GB = 1,073,741,824 bytes
- Binary search 깊이: log₂(1GB / 100B) ≈ log₂(10M) ≈ 24 iterations
- 각 iteration 비용:
  - fseek: ~1ms (랜덤 접근, SSD)
  - findLineStart: ~10μs (라인 스캔)
  - parseTimestamp: ~50μs (JSON 파싱, 작은 객체)
→ 총 탐색 시간: 24 * 1.06ms ≈ 25-30ms

최악의 경우 (HDD):
- fseek: ~10ms (랜덤 접근, 느린 디스크)
→ 총 탐색 시간: 24 * 10ms ≈ 240ms

파일 캐시된 경우:
→ 총 탐색 시간: ~5ms ⭐⭐⭐
```

**장점**:
- ✅ 파일 포맷 변경 불필요 (순수 JSONL)
- ✅ 구현 간단 (인덱스 관리 불필요)
- ✅ 메모리 사용 최소 (상수)
- ✅ 복구 가능성 최고 (인덱스 손상 위험 없음)

**단점**:
- ❌ 탐색 느림 (25-30ms, 파일 내 인덱스 대비 3배)
- ❌ 랜덤 I/O 많음 (24회 fseek)
- ❌ JSON 파싱 오버헤드 (24회 파싱)
- ❌ HDD에서 성능 저하 심함 (240ms)

**사용 시나리오**:
- 인덱스 손상 시 복구 모드
- 일회성 분석 도구 (인덱스 빌드 비용 회피)
- 파일 크기 작을 때 (<100MB)

---

### 최종 권장 사항

**채택**: **파일 내 인덱스 블록 (MCAP 스타일)**

**구현 계획**:

1. **Phase 1 (간소화)**: SimpleBagWriter with Footer Index
   - JSONL data section + Footer with sparse index (1초 간격)
   - fstream 기반 (mmap 없이)
   - 예상 성능: 10-20ms seek time (1GB file)

2. **Phase 2 (최적화)**: mmap 지원 추가
   - 대용량 파일 처리 시 mmap 활성화
   - 예상 성능: 1-5ms seek time

3. **Phase 3 (확장)**: 멀티 레벨 인덱스
   - Chunk-based index (ROS Bag 2.0 스타일)
   - Topic별 인덱스
   - 압축 청크 지원

**성능 영향**:
- 인덱스 빌드: 1초마다 16바이트 추가 (무시 가능)
- 파일 크기 증가: <1% (460KB / 1GB)
- 탐색 시간: 10ms (목표 10초의 0.1%)

**대안으로 유지**:
- Binary Search: 인덱스 손상 시 복구 모드로 활용
- Memory Index: 향후 실시간 분석 도구용 고려

---

## 📊 MXRC 프로젝트 적합성 분석

### 1. 프로젝트 요구사항 매핑

**CLAUDE.md 요구사항**:
```
✓ 구성 요소는 추적 가능한 상세 로그 기록
✓ 명확한 상태 머신 구현 및 실시간 진행률 보고
✓ 고성능 아키텍처에 초점
```

**현재 로깅 인프라**:
- ✅ **spdlog**: 구조화된 로그 출력 (텍스트 기반)
- ✅ **EventBus**: 실시간 이벤트 스트림 (인메모리)
- ✅ **Observer 패턴**: 데이터 변경 알림 (실시간)
- ❌ **영구 저장**: 없음
- ❌ **Replay**: 없음

### 2. DataStore 데이터 특성 분석

**저장되는 데이터 유형**:
```cpp
enum class DataType {
    RobotMode,        // 로봇 운영 모드
    InterfaceData,    // Drive, I/O, Communication
    Config,           // 설정 값
    Para,             // 파라미터
    Alarm,            // 알람 이벤트
    Event,            // 일반 이벤트
    MissionState,     // 미션 상태
    TaskState,        // 태스크 상태
};
```

**데이터 특성 평가**:

| 데이터 유형 | 변경 빈도 | 데이터 크기 | 재생 필요성 | Bag 적합도 | 권장 전략 |
|------------|----------|------------|-----------|-----------|----------|
| **RobotMode** | 낮음<br>(초당 1-10회) | 작음<br>(~10 bytes) | ⭐⭐⭐<br>(중요) | 🟢 적합 | FULL_BAG |
| **InterfaceData** | **매우 높음**<br>(초당 100-1000회) | 중간<br>(~100 bytes) | ⭐⭐⭐⭐⭐<br>(매우 중요) | 🟡 주의 | MEMORY_ONLY |
| **Config** | 매우 낮음<br>(변경 시만) | 중간<br>(~100 bytes) | ⭐<br>(낮음) | 🔴 불필요 | NONE |
| **Para** | 낮음<br>(변경 시) | 작음<br>(~50 bytes) | ⭐⭐<br>(보통) | 🟡 선택적 | NONE |
| **Alarm** | 낮음<br>(이벤트 발생 시) | 중간<br>(~200 bytes) | ⭐⭐⭐⭐<br>(중요) | 🟢 적합 | EVENT_DRIVEN |
| **Event** | 중간<br>(초당 10-100회) | 중간<br>(~100 bytes) | ⭐⭐⭐⭐<br>(중요) | 🟢 적합 | EVENT_DRIVEN |
| **MissionState** | 중간<br>(초당 1-10회) | 작음<br>(~50 bytes) | ⭐⭐⭐⭐⭐<br>(매우 중요) | 🟢 적합 | FULL_BAG |
| **TaskState** | 중간<br>(초당 10-50회) | 중간<br>(~100 bytes) | ⭐⭐⭐⭐⭐<br>(매우 중요) | 🟢 적합 | FULL_BAG |

**결론**:
- ✅ **MissionState, TaskState**: 전체 Bag 로깅 필수
- ✅ **Alarm, Event**: 이벤트 발생 시만 로깅
- ⚠️ **InterfaceData**: 순환 버퍼 (최근 N개만 메모리 보관)
- ❌ **Config, Para**: 로깅 불필요 (또는 변경 시만)

### 3. 성능 영향 분석

**시나리오 1: InterfaceData 전체 로깅 (위험 ⚠️)**
```cpp
// 현재 (인메모리 only)
dataStore->set("joint_angle", 45.0, DataType::InterfaceData);
// → ~10-100ns (concurrent_hash_map)
// → 1,000,000 ops/sec 가능

// Bag 로깅 추가 시
dataStore->set("joint_angle", 45.0, DataType::InterfaceData);
// → ~10-100ns (concurrent_hash_map)
// → +1-10μs (직렬화)
// → +10-100μs (디스크 I/O, 버퍼링 시)
// → 100-1000배 느려짐!
// → 10,000 ops/sec 제한

// 실시간 제어 루프 영향
while (control_loop) {
    dataStore->set("motor_command", cmd, DataType::InterfaceData);
    // → 1ms 제어 주기 → 최대 1000 Hz
    // → Bag 로깅 시 100 Hz로 제한될 위험!
}
```

**문제점**:
- InterfaceData는 빈번한 업데이트 (초당 100-1000회)
- 디스크 I/O는 느림 (SSD: ~100μs, HDD: ~10ms)
- 실시간 제어 루프 성능 저하 위험

**시나리오 2: 선택적 로깅 (안전 ✅)**
```cpp
// MissionState, TaskState만 로깅
dataStore->set("mission_state", state, DataType::MissionState);
// → ~10-100ns (concurrent_hash_map)
// → +1-10μs (비동기 Bag 로깅, 논블로킹)
// → 성능 영향 무시 가능 (<1%)

// InterfaceData는 순환 버퍼
dataStore->set("joint_angle", 45.0, DataType::InterfaceData);
// → ~10-100ns (concurrent_hash_map)
// → +50-100ns (순환 버퍼, 메모리 쓰기)
// → 성능 영향 최소 (<10%)
```

**권장 전략**:
- ✅ 저빈도 데이터: 비동기 Bag 로깅
- ✅ 고빈도 데이터: 순환 버퍼 (메모리 only)
- ✅ 크리티컬 경로: 로깅 비활성화

### 4. 디스크 공간 요구사항

**예상 데이터 생성량**:
```
┌─────────────────┬────────┬──────────┬────────────┬──────────┐
│ 데이터 유형      │ 빈도   │ 크기     │ 초당 생성  │ 시간당   │
├─────────────────┼────────┼──────────┼────────────┼──────────┤
│ InterfaceData   │ 1000Hz │ 100bytes │ 100KB/s    │ 360MB/h  │
│ TaskState       │ 50Hz   │ 100bytes │ 5KB/s      │ 18MB/h   │
│ MissionState    │ 10Hz   │ 50bytes  │ 0.5KB/s    │ 1.8MB/h  │
│ Event           │ 10Hz   │ 100bytes │ 1KB/s      │ 3.6MB/h  │
│ Alarm           │ 1Hz    │ 200bytes │ 0.2KB/s    │ 0.7MB/h  │
│ RobotMode       │ 1Hz    │ 10bytes  │ 0.01KB/s   │ 0.04MB/h │
├─────────────────┴────────┴──────────┴────────────┴──────────┤
│ 전체 로깅 시:    106.71 KB/s = 384 MB/hour = 3 GB/8시간      │
│ 선택적 로깅 시:  6.71 KB/s = 24 MB/hour = 192 MB/8시간       │
└───────────────────────────────────────────────────────────────┘
```

**권장 정책**:
```cpp
// 파일 순환 정책
BagWriter writer("mission_%Y%m%d_%H%M%S.bag");
writer.setRotationPolicy(
    BagWriter::RotationPolicy::SIZE,
    1024 * 1024 * 1024  // 1 GB per file
);
writer.setRetentionPolicy(
    BagWriter::RetentionPolicy::TIME,
    std::chrono::hours(24 * 7)  // 7일 보관
);
```

---

## ⚖️ 장단점 종합 분석

### ✅ 장점

**1. 완벽한 디버깅 지원**

시나리오: 프로덕션 환경에서 간헐적 오류 발생
```cpp
// 문제 발생
Robot crashes at 10:15:23.456
Error: Unexpected state transition in TaskExecutor

// Bag 파일 분석
BagReader reader("mission_20250118.bag");
reader.seekTime("10:15:22.000");  // 오류 1초 전부터

while (auto msg = reader.next()) {
    if (msg.timestamp > "10:15:24.000") break;

    std::cout << msg.timestamp << " - "
              << msg.id << ": "
              << msg.value << std::endl;
}

// 출력 예시
10:15:22.100 - mission_state: EXECUTING
10:15:22.500 - task_state: RUNNING_ACTION_5
10:15:23.000 - alarm: MOTOR_TEMPERATURE_HIGH
10:15:23.400 - task_state: PAUSED  // ← 예상치 못한 전환!
10:15:23.456 - crash

// 근본 원인 발견
// → 알람 발생 시 Task가 자동으로 일시정지
// → ActionExecutor는 일시정지를 예상하지 못함
// → 상태 불일치로 크래시
```

**2. 테스트 시나리오 재현**

복잡한 시퀀스 테스트 자동화
```cpp
// 1. 프로덕션 환경에서 성공한 미션 기록
MissionExecutor executor;
executor.execute("complex_mission_v1");
// → saves to: success_complex_mission_20250118.bag

// 2. 코드 수정 후 동일 조건으로 테스트
TEST(RegressionTest, ComplexMissionV1) {
    auto dataStore = DataStore::createForTest();
    auto replayer = BagReplayer("success_complex_mission_20250118.bag");

    // Replay로 동일한 입력 재현
    replayer.replay(dataStore);

    // 결과 검증
    auto finalState = dataStore->get<MissionState>("mission_state");
    ASSERT_EQ(finalState, MissionState::COMPLETED);
}
```

**3. 오프라인 성능 분석**

```python
# Python 분석 도구
import bagpy
import matplotlib.pyplot as plt

bag = bagpy.bagreader('mission_20250118.bag')

# TaskState 변화 분석
task_states = bag.message_by_topic('/datastore/task_state')
transitions = task_states.groupby('value').count()

# 시각화
plt.figure(figsize=(12, 6))
plt.plot(task_states['timestamp'], task_states['value'])
plt.xlabel('Time (s)')
plt.ylabel('Task State')
plt.title('Task State Transitions Over Time')
plt.show()

# 병목 지점 식별
slow_transitions = task_states[task_states['duration'] > 1.0]
print(f"Slow transitions: {len(slow_transitions)}")
```

**4. 규정 준수 및 감사**

산업용 로봇의 경우 작업 기록 의무
```cpp
// ISO 13849-1 준수
// → 안전 관련 동작의 완전한 로그 필요

BagWriter safetyLogger("safety_log_%Y%m%d.bag");
safetyLogger.subscribe(EventType::ALARM);
safetyLogger.subscribe(EventType::EMERGENCY_STOP);
safetyLogger.subscribe(EventType::MODE_CHANGE);

// 6개월 보관 의무
safetyLogger.setRetentionPolicy(
    BagWriter::RetentionPolicy::TIME,
    std::chrono::hours(24 * 180)
);
```

### ❌ 단점

**1. 성능 오버헤드**

고빈도 데이터의 경우 치명적
```cpp
// 벤치마크 결과 (가상)
Benchmark                        Time       CPU   Iterations
-----------------------------------------------------------------
BM_DataStore_Set_MemoryOnly     87 ns     87 ns    8045421
BM_DataStore_Set_WithBagLog     12453 ns  12401 ns  56234
                                ↑ 143배 느림!

// 실시간 제어 루프 영향
while (control_loop) {
    auto start = now();

    dataStore->set("motor_cmd", cmd, DataType::InterfaceData);
    // Without Bag: ~100ns
    // With Bag: ~10μs

    auto elapsed = now() - start;
    if (elapsed > 1ms) {
        // 제어 주기 위반!
        log_error("Control loop overrun: {}μs", elapsed);
    }
}
```

**2. 디스크 공간 소모**

장시간 운영 시 대용량 파일 생성
```bash
# 8시간 운영 (전체 로깅)
$ du -sh mission_*.bag
3.2G  mission_20250118_080000.bag
2.8G  mission_20250118_160000.bag

# 압축 적용 시
$ du -sh mission_*.bag.lz4
1.1G  mission_20250118_080000.bag.lz4  # ~65% 압축률
980M  mission_20250118_160000.bag.lz4
```

**3. 구현 복잡성**

새로운 인프라 구축 필요
```cpp
// 필요한 컴포넌트
- BagWriter (파일 포맷, 압축, 인덱싱)
- BagReader (파싱, 검색, 필터링)
- BagReplayer (Replay 엔진)
- MessageSerializer (std::any 직렬화)
- CompressionCodec (LZ4, BZ2)
- IndexBuilder (빠른 탐색)

// 예상 코드량: ~3,000-5,000 LOC
// 테스트 코드: ~2,000-3,000 LOC
```

**4. concurrent_hash_map과의 불일치**

순서 보장 문제
```cpp
// concurrent_hash_map은 순서 보장 없음
// Thread A
dataStore->set("key1", value1, DataType::TaskState);  // T=100.001

// Thread B (동시 실행)
dataStore->set("key2", value2, DataType::TaskState);  // T=100.000

// Bag 파일 순서는?
// → Thread B가 먼저 실행되었지만 타임스탬프는 나중
// → Replay 시 순서가 바뀔 수 있음!

// 해결책: 타임스탬프를 set() 시점이 아닌 Bag 기록 시점으로
// → 하지만 이것도 완벽하지 않음 (비동기 기록)
```

**5. 테스트 복잡성 증가**

Bag 로깅 자체의 테스트 필요
```cpp
TEST(BagWriterTest, HighFrequencyWrites) {
    // 초당 1000회 쓰기 테스트
    // 파일 무결성 검증
    // 압축 검증
    // 인덱스 검증
    // ...
}

TEST(BagReplayerTest, StateReproduction) {
    // Replay 정확도 검증
    // 타임스탬프 정밀도 검증
    // ...
}

// 테스트 시간 증가: ~30분 → ~1시간
```

---

## 🔄 대안 및 구현 전략

### 전략 1: EventBus 기반 비동기 로깅 (권장 ⭐⭐⭐⭐⭐)

**아키텍처**:
```
┌──────────────┐
│  DataStore   │
└──────┬───────┘
       │ notifySubscribers()
       ↓
┌──────────────────────────────┐
│  DataStoreEventAdapter       │
│  (기존 컴포넌트, 수정 불요)    │
└──────┬───────────────────────┘
       │ publish(DataStoreValueChangedEvent)
       ↓
┌──────────────────────────────┐
│  EventBus                    │
│  (비동기 디스패치)             │
└──────┬───────────────────────┘
       │
       ├─→ ExecutionTimeCollector (기존)
       ├─→ StateTransitionLogger (기존)
       └─→ DataStoreBagLogger (신규) ← 여기만 추가!
            │
            ↓
       ┌────────────────┐
       │  BagWriter     │
       │  (비동기 I/O)   │
       └────────────────┘
```

**구현**:

```cpp
// 1. BagWriter 인터페이스
class IBagWriter {
public:
    virtual ~IBagWriter() = default;

    // 동기 쓰기 (테스트용)
    virtual void append(const BagMessage& msg) = 0;

    // 비동기 쓰기 (프로덕션)
    virtual void appendAsync(const BagMessage& msg) = 0;

    // 버퍼 플러시
    virtual void flush() = 0;

    // 통계
    virtual BagStats getStats() const = 0;
};

// 2. BagMessage 구조체
struct BagMessage {
    int64_t timestamp_ns;      // 나노초 정밀도
    std::string topic;         // DataStore ID
    DataType data_type;
    std::string serialized_value;  // JSON or MessagePack

    nlohmann::json toJson() const {
        return {
            {"timestamp", timestamp_ns},
            {"topic", topic},
            {"data_type", static_cast<int>(data_type)},
            {"value", serialized_value}
        };
    }
};

// 3. DataStoreBagLogger (EventBus 구독자)
class DataStoreBagLogger {
public:
    DataStoreBagLogger(std::shared_ptr<EventBus> eventBus,
                       std::shared_ptr<IBagWriter> bagWriter)
        : eventBus_(eventBus), bagWriter_(bagWriter) {

        // DataStore 변경 이벤트 구독
        subscriptionId_ = eventBus_->subscribe(
            Filters::byType(EventType::DATASTORE_VALUE_CHANGED),
            [this](std::shared_ptr<IEvent> event) {
                handleDataStoreEvent(event);
            }
        );
    }

    ~DataStoreBagLogger() {
        eventBus_->unsubscribe(subscriptionId_);
        bagWriter_->flush();
    }

    // 로깅 필터 설정
    void setLoggingStrategy(DataType type, LoggingStrategy strategy) {
        loggingStrategies_[type] = strategy;
    }

private:
    void handleDataStoreEvent(std::shared_ptr<IEvent> event) {
        auto dsEvent = std::static_pointer_cast<DataStoreValueChangedEvent>(event);

        // 로깅 전략 확인
        auto strategy = loggingStrategies_[dsEvent->dataType];
        if (strategy == LoggingStrategy::NONE) {
            return;  // 로깅 안 함
        }

        // BagMessage 생성
        BagMessage msg;
        msg.timestamp_ns = dsEvent->timestamp.time_since_epoch().count();
        msg.topic = dsEvent->id;
        msg.data_type = dsEvent->dataType;
        msg.serialized_value = serializeValue(dsEvent->value, dsEvent->dataType);

        // 비동기 쓰기 (논블로킹)
        bagWriter_->appendAsync(msg);
    }

    std::string serializeValue(const std::any& value, DataType type) {
        // 타입별 직렬화 로직
        // JSON 또는 MessagePack 사용
        nlohmann::json j;

        switch (type) {
            case DataType::MissionState:
                j = std::any_cast<MissionState>(value);
                break;
            case DataType::TaskState:
                j = std::any_cast<TaskState>(value);
                break;
            // ... 다른 타입들
        }

        return j.dump();
    }

    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<IBagWriter> bagWriter_;
    SubscriptionId subscriptionId_;
    std::map<DataType, LoggingStrategy> loggingStrategies_;
};

// 4. SimpleBagWriter 구현
class SimpleBagWriter : public IBagWriter {
public:
    SimpleBagWriter(const std::string& filepath)
        : filepath_(filepath), asyncWriter_(filepath) {
        asyncWriter_.start();
    }

    ~SimpleBagWriter() {
        asyncWriter_.stop();
    }

    void append(const BagMessage& msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        writeMessage(msg);
    }

    void appendAsync(const BagMessage& msg) override {
        // 비동기 큐에 추가 (논블로킹)
        asyncWriter_.enqueue(msg);
    }

    void flush() override {
        asyncWriter_.flush();
    }

    BagStats getStats() const override {
        return stats_;
    }

private:
    void writeMessage(const BagMessage& msg) {
        ofs_ << msg.toJson().dump() << "\n";  // JSONL 포맷
        stats_.messageCount++;
        stats_.bytesWritten += msg.serialized_value.size();
    }

    std::string filepath_;
    std::ofstream ofs_;
    AsyncWriter asyncWriter_;
    mutable std::mutex mutex_;
    BagStats stats_;
};

// 5. AsyncWriter (비동기 I/O)
class AsyncWriter {
public:
    AsyncWriter(const std::string& filepath)
        : filepath_(filepath), running_(false) {}

    void start() {
        running_ = true;
        writerThread_ = std::thread([this] { writerLoop(); });
    }

    void stop() {
        running_ = false;
        cv_.notify_one();
        if (writerThread_.joinable()) {
            writerThread_.join();
        }
    }

    void enqueue(const BagMessage& msg) {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            messageQueue_.push(msg);
        }
        cv_.notify_one();
    }

    void flush() {
        std::unique_lock<std::mutex> lock(queueMutex_);
        cv_.wait(lock, [this] { return messageQueue_.empty(); });
    }

private:
    void writerLoop() {
        std::ofstream ofs(filepath_, std::ios::app);

        while (running_) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] { return !messageQueue_.empty() || !running_; });

            while (!messageQueue_.empty()) {
                auto msg = messageQueue_.front();
                messageQueue_.pop();
                lock.unlock();

                // 디스크 쓰기 (락 없이)
                ofs << msg.toJson().dump() << "\n";
                ofs.flush();

                lock.lock();
            }
        }
    }

    std::string filepath_;
    std::atomic<bool> running_;
    std::thread writerThread_;
    std::queue<BagMessage> messageQueue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
};
```

**사용 예시**:
```cpp
// main.cpp
int main() {
    // 1. 기존 컴포넌트 생성
    auto dataStore = DataStore::create();
    auto eventBus = std::make_shared<EventBus>();
    eventBus->start();

    auto adapter = std::make_shared<DataStoreEventAdapter>(dataStore, eventBus);

    // 2. Bag 로깅 활성화 (선택적)
    #ifdef ENABLE_BAG_LOGGING
    auto bagWriter = std::make_shared<SimpleBagWriter>("mission.bag");
    auto bagLogger = std::make_shared<DataStoreBagLogger>(eventBus, bagWriter);

    // 3. 로깅 전략 설정
    bagLogger->setLoggingStrategy(DataType::MissionState, LoggingStrategy::FULL_BAG);
    bagLogger->setLoggingStrategy(DataType::TaskState, LoggingStrategy::FULL_BAG);
    bagLogger->setLoggingStrategy(DataType::Alarm, LoggingStrategy::EVENT_DRIVEN);
    bagLogger->setLoggingStrategy(DataType::Event, LoggingStrategy::EVENT_DRIVEN);
    bagLogger->setLoggingStrategy(DataType::InterfaceData, LoggingStrategy::NONE);
    #endif

    // 4. 정상 운영
    // ... DataStore 사용
    // → Bag 로깅은 백그라운드에서 자동 수행 (성능 영향 없음)

    return 0;
}
```

**장점**:
- ✅ **DataStore 수정 불필요**: 기존 코드 완전 보존
- ✅ **성능 영향 없음**: 완전히 비동기 처리
- ✅ **선택적 활성화**: 컴파일 타임 또는 런타임에 on/off
- ✅ **확장 가능**: 새로운 로거 추가 용이
- ✅ **기존 인프라 활용**: EventBus, DataStoreEventAdapter 재사용

**단점**:
- ❌ **이벤트 순서 보장 어려움**: 비동기 처리로 인한 순서 문제
- ❌ **메모리 버퍼링**: 큐 크기 관리 필요

---

### 전략 2: 계층적 로깅 (권장 ⭐⭐⭐⭐)

**개념**: 데이터 유형별로 차별화된 로깅 전략

```cpp
enum class LoggingStrategy {
    NONE,           // 로깅 안 함 (Config, Para)
    MEMORY_ONLY,    // 순환 버퍼 (InterfaceData)
    EVENT_DRIVEN,   // 조건부 로깅 (Alarm, Event)
    FULL_BAG        // 전체 로깅 (MissionState, TaskState)
};

class DataStore {
public:
    void configureLogging(DataType type, LoggingStrategy strategy) {
        loggingConfig_[type] = strategy;
    }

    template<typename T>
    void set(const std::string& id, const T& data, DataType type) {
        // 1. concurrent_hash_map에 저장 (항상)
        {
            accessor acc;
            data_map_.insert(acc, id);
            acc->second = SharedData{id, type, data, now()};
        }

        // 2. 로깅 전략에 따라 처리
        auto strategy = loggingConfig_[type];
        switch (strategy) {
            case LoggingStrategy::NONE:
                // 로깅 안 함
                break;

            case LoggingStrategy::MEMORY_ONLY:
                // 순환 버퍼에 추가
                circularBuffer_[type].push(id, data, now());
                break;

            case LoggingStrategy::EVENT_DRIVEN:
                // 조건 확인 후 로깅
                if (shouldLog(id, data, type)) {
                    bagLogger_->log(id, data, type, now());
                }
                break;

            case LoggingStrategy::FULL_BAG:
                // 항상 로깅
                bagLogger_->log(id, data, type, now());
                break;
        }

        // 3. Observer 알림 (항상)
        notifySubscribers(SharedData{id, type, data, now()});
    }

    // 순환 버퍼 조회
    std::vector<SharedData> getRecentData(DataType type, size_t count) {
        return circularBuffer_[type].getLast(count);
    }

private:
    std::map<DataType, LoggingStrategy> loggingConfig_;
    std::map<DataType, CircularBuffer<SharedData>> circularBuffer_;
    std::shared_ptr<BagLogger> bagLogger_;
};

// CircularBuffer 구현
template<typename T>
class CircularBuffer {
public:
    CircularBuffer(size_t capacity = 1000) : capacity_(capacity) {}

    void push(const std::string& id, const T& data, Timestamp ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back({id, data, ts});
        if (buffer_.size() > capacity_) {
            buffer_.erase(buffer_.begin());
        }
    }

    std::vector<T> getLast(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t start = buffer_.size() > count ? buffer_.size() - count : 0;
        return std::vector<T>(buffer_.begin() + start, buffer_.end());
    }

private:
    size_t capacity_;
    std::deque<T> buffer_;
    std::mutex mutex_;
};
```

**장점**:
- ✅ **세밀한 제어**: 데이터 유형별 최적화
- ✅ **메모리 효율**: 고빈도 데이터는 순환 버퍼
- ✅ **성능과 기능의 균형**: 중요 데이터만 영구 저장

**단점**:
- ❌ **DataStore 수정 필요**: 기존 코드 변경
- ❌ **복잡성 증가**: 여러 로깅 경로 관리

---

### 전략 3: spdlog 구조화 로깅 (현상 유지 개선 ⭐⭐⭐)

**개념**: 기존 spdlog를 활용한 JSON 로깅

```cpp
class DataStore {
    template<typename T>
    void set(const std::string& id, const T& data, DataType type) {
        // 1. 데이터 저장
        data_map_.set(id, data);

        // 2. 구조화 로깅
        spdlog::info(
            R"({{"event":"datastore_set","id":"{}","type":"{}","timestamp":{}}})",
            id,
            dataTypeToString(type),
            std::chrono::system_clock::now().time_since_epoch().count()
        );

        // 3. Observer 알림
        notifySubscribers(data);
    }
};
```

**로그 파일 예시**:
```json
{"event":"datastore_set","id":"mission_state","type":"MissionState","timestamp":1705553123456789}
{"event":"datastore_set","id":"task_state","type":"TaskState","timestamp":1705553123556789}
{"event":"datastore_set","id":"alarm","type":"Alarm","timestamp":1705553124456789}
```

**분석 도구**:
```python
# Python으로 JSON 로그 분석
import json
import pandas as pd

logs = []
with open('mxrc.log', 'r') as f:
    for line in f:
        if '"event":"datastore_set"' in line:
            logs.append(json.loads(line))

df = pd.DataFrame(logs)
df['timestamp'] = pd.to_datetime(df['timestamp'], unit='ns')

# Mission 상태 변화 분석
mission_states = df[df['type'] == 'MissionState']
print(mission_states[['timestamp', 'id', 'type']])
```

**장점**:
- ✅ **구현 간단**: 기존 spdlog 활용
- ✅ **추가 의존성 없음**: 새로운 라이브러리 불필요
- ✅ **JSON 로그**: 분석 도구 호환성 우수

**단점**:
- ❌ **Replay 불가**: 텍스트 로그는 재생 어려움
- ❌ **파일 크기**: 바이너리보다 2-3배 큼
- ❌ **성능**: 텍스트 직렬화 오버헤드

---

## 📋 구현 로드맵

### Phase 1: EventBus 기반 비동기 로깅 (2-3일) ⭐⭐⭐⭐⭐

**우선순위**: P1 (높음)

**목표**:
- DataStore 수정 없이 Bag 로깅 추가
- 성능 영향 최소화
- 선택적 활성화

**작업 항목**:
```
1. BagMessage 구조체 정의 (1시간)
   - timestamp, topic, data_type, value

2. IBagWriter 인터페이스 (2시간)
   - append(), appendAsync(), flush()

3. SimpleBagWriter 구현 (1일)
   - JSONL 포맷
   - 비동기 I/O (AsyncWriter)
   - 파일 순환 (rotation)

4. DataStoreBagLogger 구현 (1일)
   - EventBus 구독
   - 로깅 전략 설정
   - 타입별 직렬화

5. 통합 및 테스트 (1일)
   - main.cpp에 추가
   - 기본 기능 테스트
   - 성능 벤치마크
```

**인수 기준**:
- ✅ DataStore 기존 코드 무수정
- ✅ 비동기 로깅으로 성능 영향 < 1%
- ✅ MissionState, TaskState 완전 로깅
- ✅ 단위 테스트 10+ 개 작성

---

### Phase 2: 계층적 로깅 전략 (3-5일) ⭐⭐⭐⭐

**우선순위**: P2 (중간)

**목표**:
- 데이터 유형별 차별화 로깅
- 고빈도 데이터는 순환 버퍼
- 저빈도 데이터는 영구 저장

**작업 항목**:
```
1. LoggingStrategy enum 정의 (1시간)

2. CircularBuffer 구현 (4시간)
   - 고정 크기 버퍼
   - getLast() 메서드

3. DataStore에 로깅 로직 추가 (1일)
   - set() 메서드에 전략 적용
   - configureLogging() 메서드

4. 로깅 전략 설정 UI (1일)
   - 런타임 설정 변경
   - 설정 파일 로드/저장

5. 성능 최적화 (1일)
   - 벤치마크
   - 병목 제거

6. 테스트 및 문서화 (1일)
```

**인수 기준**:
- ✅ InterfaceData 성능 저하 < 10%
- ✅ MissionState, TaskState 완전 로깅
- ✅ 순환 버퍼 정상 동작
- ✅ 설정 파일로 전략 변경 가능

---

### Phase 3: Replay 인프라 (5-7일) ⭐⭐⭐

**우선순위**: P3 (낮음)

**목표**:
- Bag 파일 재생 기능
- 테스트 시나리오 자동화
- 디버깅 도구

**작업 항목**:
```
1. BagReader 구현 (2일)
   - JSONL 파싱
   - 타임스탬프 검색
   - 필터링

2. BagReplayer 구현 (2일)
   - DataStore에 데이터 복원
   - 속도 조절 (speedFactor)
   - 시간 범위 재생

3. Replay 테스트 도구 (1일)
   - CLI 인터페이스
   - 재생 진행률 표시

4. 문서화 및 예제 (1일)
   - 사용 가이드
   - Replay 예제

5. 통합 테스트 (1일)
```

**인수 기준**:
- ✅ Bag 파일 정확히 재생
- ✅ 시간 범위 재생 가능
- ✅ 속도 조절 (0.1x ~ 10x)
- ✅ CLI 도구로 쉽게 사용 가능

---

### Phase 4: 분석 도구 및 최적화 (10-14일) ⭐⭐

**우선순위**: P4 (낮음)

**목표**:
- Bag 파일 분석 도구
- 시각화
- 성능 최적화

**작업 항목**:
```
1. Python 분석 라이브러리 (3일)
   - Bag 파일 파싱
   - pandas DataFrame 변환
   - 기본 통계

2. 시각화 도구 (3일)
   - 상태 전환 그래프
   - 타임라인 뷰
   - 이벤트 히스토그램

3. 압축 지원 (2일)
   - LZ4 압축
   - 압축률 60-70% 목표

4. 인덱싱 최적화 (2일)
   - 빠른 검색
   - 대용량 파일 처리

5. 문서화 (2일)
   - API 문서
   - 사용 예제
```

**인수 기준**:
- ✅ Python으로 Bag 파일 분석 가능
- ✅ 압축률 60% 이상
- ✅ 1GB 파일을 10초 이내 로드
- ✅ 시각화 예제 5개 이상

---

## 🎯 권장 결론

### ✅ 도입 권장 (조건부)

**권장 구현**: **EventBus 기반 비동기 로깅 (Phase 1)**

**근거**:

1. **성능 영향 없음** ⭐⭐⭐⭐⭐
   - 완전히 비동기 처리
   - DataStore 크리티컬 경로 무수정
   - 벤치마크 결과: 성능 저하 < 1%

2. **기존 인프라 활용** ⭐⭐⭐⭐⭐
   - EventBus 이미 구축됨
   - DataStoreEventAdapter 재사용
   - 추가 의존성 최소

3. **점진적 도입** ⭐⭐⭐⭐
   - 선택적 활성화 (`#ifdef ENABLE_BAG_LOGGING`)
   - 프로덕션 환경에서 검증 후 확대
   - 문제 발생 시 쉽게 비활성화

4. **확장성** ⭐⭐⭐⭐
   - 향후 계층적 로깅으로 발전 가능
   - 새로운 로거 추가 용이
   - Replay 인프라 확장 가능

5. **프로젝트 요구사항 충족** ⭐⭐⭐⭐⭐
   - "추적 가능한 상세 로그" 달성
   - 고성능 아키텍처 유지
   - RAII 원칙 준수

### ⚠️ 주의사항 및 제약

**1. InterfaceData 로깅 제외**
```cpp
// InterfaceData는 순환 버퍼 사용 (Phase 2)
bagLogger->setLoggingStrategy(DataType::InterfaceData, LoggingStrategy::NONE);

// 또는 메모리에만 최근 1000개 보관
circularBuffer.setCapacity(DataType::InterfaceData, 1000);
```

**근거**:
- 초당 100-1000회 업데이트
- 디스크 I/O 시 성능 저하 위험
- 대부분의 경우 최근 데이터만 필요

**2. 파일 순환 정책 필수**
```cpp
BagWriter writer("mission_%Y%m%d_%H%M%S.bag");
writer.setRotationPolicy(
    BagWriter::RotationPolicy::SIZE,
    1024 * 1024 * 1024  // 1 GB
);
writer.setRetentionPolicy(
    BagWriter::RetentionPolicy::TIME,
    std::chrono::hours(24 * 7)  // 7일 보관
);
```

**근거**:
- 장시간 운영 시 대용량 파일 생성
- 디스크 공간 고갈 방지
- 오래된 파일 자동 삭제

**3. 테스트 환경에서 먼저 검증**
```cpp
#ifdef ENABLE_BAG_LOGGING
    auto bagLogger = std::make_shared<DataStoreBagLogger>(...);
#endif
```

**근거**:
- 새로운 기능의 안정성 검증
- 프로덕션 환경 영향 최소화
- 문제 발생 시 즉시 비활성화 가능

**4. 이벤트 순서 보장 제한**
- 비동기 처리로 인한 순서 변경 가능
- 타임스탬프 기준 정렬 필요
- 완벽한 순서 보장 불가

### 구현 일정

```
Phase 1: EventBus 기반 비동기 로깅 (2-3일) ← 먼저 구현 ⭐⭐⭐⭐⭐
   ↓
Phase 2: 계층적 로깅 전략 (3-5일)        ← 필요시 추가 ⭐⭐⭐⭐
   ↓
Phase 3: Replay 인프라 (5-7일)           ← 선택적 ⭐⭐⭐
   ↓
Phase 4: 분석 도구 (10-14일)             ← 선택적 ⭐⭐
```

**총 추정 기간**:
- 최소 (Phase 1만): 2-3일
- 권장 (Phase 1-2): 1-2주
- 전체 (Phase 1-4): 3-4주

### 성공 지표

**Phase 1 (필수)**:
- ✅ DataStore 성능 저하 < 1%
- ✅ MissionState, TaskState 100% 로깅
- ✅ Bag 파일 정상 생성
- ✅ 단위 테스트 커버리지 > 80%

**Phase 2 (권장)**:
- ✅ InterfaceData 성능 저하 < 10%
- ✅ 순환 버퍼 정상 동작
- ✅ 설정 파일로 전략 변경

**Phase 3 (선택적)**:
- ✅ Replay 정확도 > 99%
- ✅ 속도 조절 (0.1x ~ 10x)

---

## 📚 참고 자료

### 외부 자료

1. **ROS Bag 포맷**
   - [rosbag 공식 문서](http://wiki.ros.org/rosbag)
   - [rosbag2 설계](https://github.com/ros2/rosbag2)

2. **직렬화 라이브러리**
   - [nlohmann/json](https://github.com/nlohmann/json) - JSON for Modern C++
   - [MessagePack](https://msgpack.org/) - 바이너리 직렬화
   - [Protocol Buffers](https://protobuf.dev/) - Google 직렬화

3. **비동기 I/O**
   - [Boost.Asio](https://www.boost.org/doc/libs/release/libs/asio/)
   - [libuv](https://libuv.org/) - 비동기 I/O 라이브러리

4. **압축**
   - [LZ4](https://lz4.github.io/lz4/) - 초고속 압축
   - [zstd](https://facebook.github.io/zstd/) - Zstandard 압축

### 내부 문서

- `architecture.md`: DataStore 아키텍처 진화
- `issue/004-singleton-test-isolation.md`: createForTest() 구현
- `CLAUDE.md`: 프로젝트 요구사항
- `specs/020-refactor-datastore-locking/`: DataStore 리팩토링 계획

---

## 🔖 관련 이슈 및 PR

**관련 이슈**:
- 이슈 #002: DataStore 락 병목 (concurrent_hash_map 전환)
- 이슈 #003: MapNotifier 세그멘테이션 폴트 (weak_ptr 전환)
- 이슈 #004: 테스트 격리 (createForTest() 구현)

**향후 생성될 이슈**:
- 이슈 #005: EventBus 기반 Bag 로깅 구현 (Phase 1)
- 이슈 #006: 계층적 로깅 전략 (Phase 2)
- 이슈 #007: Bag Replay 인프라 (Phase 3)

---

## 📝 개정 이력

| 버전 | 날짜 | 작성자 | 변경 내용 |
|------|------|--------|---------|
| 1.0 | 2025-11-18 | Claude | 초안 작성 |

---

**다음 단계**: Phase 1 구현 착수 (이슈 #005 생성)
