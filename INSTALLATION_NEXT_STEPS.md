# MXRC 프로젝트 - 다음 설치 단계

**작성 일자**: 2025-11-23
**현재 상태**: Feature 019 Phase 4 완료, 의존성 설치 대기
**브랜치**: 001-ethercat-integration

---

## 📋 현재 상황

- ✅ Phase 1-3: 완료
- ✅ Phase 4 (Hot Key): 코드 구현 완료 (테스트 대기)
- ✅ Phase 5 (EventBus Priority): 완료 (73 tests 통과)
- ✅ 의존성 설치 스크립트 작성 완료
- ✅ 의존성 문서화 완료
- ⏳ **의존성 설치 필요** ← 다음 단계

---

## 🚀 다음 단계 (순서대로)

### Step 1: 필수 의존성 설치 (5-10분)

**실행 명령**:
```bash
cd /home/tory/workspace/mxrc/mxrc
sudo ./scripts/install-dependencies.sh --required
```

**설치 내용**:
- build-essential, g++, cmake, git
- libspdlog-dev (로깅)
- libgtest-dev (테스트)
- libtbb-dev (concurrent_hash_map)
- nlohmann-json3-dev (JSON)
- libyaml-cpp-dev (YAML, IPC 스키마용)
- libsystemd-dev (systemd 통합)
- Python 패키지: jinja2, pyyaml

**예상 결과**:
```
✓ Build tools installed
✓ C++ libraries installed
✓ Python packages installed
✓ All REQUIRED dependencies installed successfully!
```

---

### Step 2: CMake 재구성 (1분)

**실행 명령**:
```bash
cd build
cmake ..
```

**확인 사항**:
```
✓ All REQUIRED dependencies found
  - Threads: OK
  - spdlog: OK
  - GTest: OK
  - TBB: OK (concurrent containers)
  - nlohmann_json: OK
  - systemd: systemd
✓ yaml-cpp: FOUND
```

**경고 메시지 (정상)**:
```
✗ Folly: NOT FOUND - Hot Key optimization DISABLED
✗ Google Benchmark: NOT FOUND
```
→ 이것은 선택적 패키지이므로 지금은 무시해도 됩니다.

---

### Step 3: 프로젝트 빌드 (2-3분)

**실행 명령**:
```bash
make -j$(nproc)
```

**예상 결과**:
- 모든 소스 파일 컴파일
- IPC 스키마 자동 생성 (DataStoreKeys.h, EventBusEvents.h)
- `run_tests` 실행 파일 생성

**에러 발생 시**:
```bash
# CMake 캐시 정리 후 재시도
rm -rf build/*
cd build && cmake .. && make -j$(nproc)
```

---

### Step 4: 테스트 실행 (1-2분)

**실행 명령**:
```bash
./run_tests
```

**예상 결과**:
```
[==========] Running 1057 tests from 100 test suites.
[==========] 1057 tests from 100 test suites ran.
[  PASSED  ] 1057 tests.
```

**테스트 범위**:
- Phase 3: IPC 스키마 (19 keys, 12 events)
- Phase 5: EventBus 우선순위 (73 tests)
- Phase 6: EtherCAT 인터페이스
- Phase 7: Monitoring
- Phase 8: HA (High Availability)
- 기존 모든 테스트

---

### Step 5 (선택): Hot Key 최적화 활성화 (30-40분)

Hot Key 최적화를 활성화하려면 Folly와 Google Benchmark를 설치해야 합니다.

**실행 명령**:
```bash
sudo ./scripts/install-dependencies.sh --optional
```

**설치 내용**:
- Boost (lock-free structures)
- NUMA (성능 최적화)
- Google Benchmark (소스 빌드, 5-10분)
- Folly (소스 빌드, 20-30분)
- prometheus-cpp (메트릭)
- CivetWeb (HTTP 서버)

**Hot Key 활성화 절차** (Folly 설치 후):
1. `src/core/datastore/DataStore.h` 편집:
   ```cpp
   // 주석 제거:
   #include "hotkey/HotKeyCache.h"
   #include "hotkey/HotKeyConfig.h"

   // 멤버 변수 주석 제거:
   std::unique_ptr<mxrc::core::datastore::HotKeyCache> hot_key_cache_;
   std::unique_ptr<mxrc::core::datastore::HotKeyConfig> hot_key_config_;
   ```

2. `src/core/datastore/DataStore.cpp` 편집:
   ```cpp
   // 생성자에서 주석 제거:
   hot_key_cache_(std::make_unique<mxrc::core::datastore::HotKeyCache>(32)),
   hot_key_config_(std::make_unique<mxrc::core::datastore::HotKeyConfig>())

   // set/get 함수에서 Hot Key fast path 주석 제거
   ```

3. CMakeLists.txt에 Hot Key 소스 추가:
   ```cmake
   # Hot Key 소스 파일
   src/core/datastore/hotkey/HotKeyCache.cpp
   src/core/datastore/hotkey/HotKeyConfig.cpp

   # Hot Key 테스트 파일
   tests/unit/datastore/HotKeyCache_test.cpp
   tests/benchmark/hotkey_benchmark.cpp
   tests/integration/hotkey_performance_test.cpp
   ```

4. 재빌드 및 테스트:
   ```bash
   cd build
   cmake ..
   make -j$(nproc)
   ./run_tests --gtest_filter="HotKey*"
   ```

**Hot Key 성능 목표**:
- 읽기 지연: <60ns
- 쓰기 지연: <110ns
- 최대 32개 Hot Keys
- 값 크기: ≤512 bytes
- 총 메모리: <10MB

---

## 📖 참고 문서

### 빠른 참조
- **README_DEPENDENCIES.md**: 이 파일의 간단 버전
- **scripts/install-dependencies.sh --help**: 설치 옵션 도움말

### 상세 문서
- **docs/DEPENDENCIES.md**: 전체 의존성 목록 및 문제 해결
- **docs/specs/019-architecture-improvements/folly-benchmark-installation.md**: Folly 설치 상세 가이드
- **docs/specs/019-architecture-improvements/completion-summary.md**: Feature 019 완료 요약

### CMake 참조
- **CMakeLists.txt**: 현재 빌드 설정
- **CMakeLists.txt.new**: 개선된 의존성 표시 예제

---

## ❓ 문제 해결

### 문제 1: yaml-cpp not found
```bash
sudo apt-get install -y libyaml-cpp-dev
cd build && cmake ..
```

### 문제 2: 빌드 에러 발생
```bash
# CMake 캐시 정리
rm -rf build/*
cd build
cmake ..
make -j$(nproc)
```

### 문제 3: Python 패키지 설치 에러
```bash
# Ubuntu 24.04에서 --break-system-packages 필요
pip3 install --break-system-packages jinja2 pyyaml
```

### 문제 4: Folly 빌드 실패
```bash
# 의존성 재설치
sudo apt-get install -y \
    libevent-dev libdouble-conversion-dev libgoogle-glog-dev \
    libgflags-dev libiberty-dev liblz4-dev liblzma-dev \
    libsnappy-dev zlib1g-dev binutils-dev libjemalloc-dev \
    libssl-dev libunwind-dev libfmt-dev libsodium-dev

# Folly 재빌드
cd /tmp/folly/_build
make clean && cmake .. && make -j$(nproc) && sudo make install && sudo ldconfig
```

---

## 🎯 요약: 지금 할 일

```bash
# 1. 필수 의존성 설치
cd /home/tory/workspace/mxrc/mxrc
sudo ./scripts/install-dependencies.sh --required

# 2. CMake 재구성
cd build
cmake ..

# 3. 빌드
make -j$(nproc)

# 4. 테스트
./run_tests

# 5. (선택) Hot Key 최적화
# sudo ./scripts/install-dependencies.sh --optional
# (그 다음 DataStore.h/cpp 주석 제거 및 재빌드)
```

**예상 소요 시간**: 10-15분 (필수만) 또는 50-60분 (전체)

---

**참고**: 이 문서는 현재 세션의 작업 내용을 정리한 것입니다. 설치 완료 후 삭제하셔도 됩니다.
