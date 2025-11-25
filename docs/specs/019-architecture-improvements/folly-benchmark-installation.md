# Folly & Google Benchmark Installation Guide

**Feature 019: Architecture Improvements - Phase 4 Hot Key Optimization**

Phase 4의 Hot Key 최적화 기능을 활성화하려면 Folly와 Google Benchmark 라이브러리가 필요합니다.

## 필요한 라이브러리

- **Folly**: Facebook의 고성능 C++ 라이브러리 (AtomicHashMap 사용)
- **Google Benchmark**: 성능 벤치마크 프레임워크

## Ubuntu 24.04 설치 방법

### 자동 설치 스크립트

```bash
# 스크립트 다운로드 및 실행
sudo bash /tmp/install_folly_benchmark.sh

# 설치 후 라이브러리 경로 업데이트
sudo ldconfig
```

### 수동 설치 (단계별)

#### 1. Google Benchmark 설치

```bash
cd /tmp
git clone https://github.com/google/benchmark.git
cd benchmark
cmake -E make_directory "build"
cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
cmake --build "build" --config Release
sudo cmake --build "build" --config Release --target install
```

#### 2. Folly 의존성 설치

```bash
sudo apt-get update
sudo apt-get install -y \
    g++ cmake libboost-all-dev libevent-dev \
    libdouble-conversion-dev libgoogle-glog-dev \
    libgflags-dev libiberty-dev liblz4-dev liblzma-dev \
    libsnappy-dev make zlib1g-dev binutils-dev \
    libjemalloc-dev libssl-dev pkg-config libunwind-dev \
    libfmt-dev libsodium-dev
```

#### 3. Folly 설치

```bash
cd /tmp
git clone https://github.com/facebook/folly.git
cd folly
mkdir _build && cd _build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

## 설치 확인

```bash
# CMake가 Folly를 찾는지 확인
cd /home/tory/workspace/mxrc/mxrc/build
cmake ..

# 다음 메시지가 나오지 않으면 성공:
# CMake Warning: Folly not found
```

## Hot Key 기능 활성화

설치 완료 후 다음 파일들의 주석을 제거하여 Hot Key 기능을 활성화합니다:

### 1. DataStore.h 수정

**파일**: `src/core/datastore/DataStore.h`

```cpp
// Before (주석 처리됨):
// Feature 019: Hot Key Optimization (disabled - requires Folly)
// TODO: Enable when Folly is installed
// #include "hotkey/HotKeyCache.h"
// #include "hotkey/HotKeyConfig.h"

// After (활성화):
// Feature 019: Hot Key Optimization
#include "hotkey/HotKeyCache.h"
#include "hotkey/HotKeyConfig.h"
```

멤버 변수도 주석 제거:
```cpp
// Before:
// std::unique_ptr<mxrc::core::datastore::HotKeyCache> hot_key_cache_;
// std::unique_ptr<mxrc::core::datastore::HotKeyConfig> hot_key_config_;

// After:
std::unique_ptr<mxrc::core::datastore::HotKeyCache> hot_key_cache_;
std::unique_ptr<mxrc::core::datastore::HotKeyConfig> hot_key_config_;
```

Template 메서드의 Hot Key fast path도 주석 제거 (line 211-218, 276-285)

### 2. DataStore.cpp 수정

**파일**: `src/core/datastore/DataStore.cpp`

생성자 초기화 리스트 주석 제거 (line 17-19):
```cpp
// Before:
// hot_key_cache_(std::make_unique<mxrc::core::datastore::HotKeyCache>(32)),
// hot_key_config_(std::make_unique<mxrc::core::datastore::HotKeyConfig>())

// After:
hot_key_cache_(std::make_unique<mxrc::core::datastore::HotKeyCache>(32)),
hot_key_config_(std::make_unique<mxrc::core::datastore::HotKeyConfig>())
```

Hot Key 초기화 코드 주석 제거 (line 21-37)

### 3. CMakeLists.txt 수정

Hot Key 소스 파일 추가 (mxrc 타겟, line ~127):
```cmake
# DataStore manager classes
src/core/datastore/managers/ExpirationManager.cpp
src/core/datastore/managers/AccessControlManager.cpp
src/core/datastore/managers/MetricsCollector.cpp
src/core/datastore/managers/LogManager.cpp
# Feature 019: Hot Key Optimization
src/core/datastore/hotkey/HotKeyCache.cpp
src/core/datastore/hotkey/HotKeyConfig.cpp
```

Hot Key 테스트 파일 추가 (run_tests 타겟, line ~565):
```cmake
tests/unit/datastore/VersionedData_test.cpp
tests/unit/datastore/accessor_benchmark.cpp
# Feature 019: Hot Key Optimization tests
tests/unit/datastore/HotKeyCache_test.cpp
tests/benchmark/hotkey_benchmark.cpp
tests/integration/hotkey_performance_test.cpp
```

run_tests 타겟에도 Hot Key 소스 추가 (line ~623)

### 4. 재빌드 및 테스트

```bash
cd build
cmake ..
make run_tests -j8
./run_tests --gtest_filter="HotKey*"
```

## Hot Key 테스트 항목

활성화 후 실행 가능한 테스트:

### 단위 테스트
- `tests/unit/datastore/HotKeyCache_test.cpp`: HotKeyCache 기능 검증

### 벤치마크
- `tests/benchmark/hotkey_benchmark.cpp`: 읽기/쓰기 성능 측정
  - 목표: 읽기 <60ns, 쓰기 <110ns

### 통합 테스트
- `tests/integration/hotkey_performance_test.cpp`: RT 사이클 시뮬레이션

## 성능 목표 (FR-006)

Hot Key Cache는 다음 성능 목표를 달성해야 합니다:

- **읽기 성능**: <60ns (평균)
- **쓰기 성능**: <110ns (평균)
- **최대 Hot Key 수**: 32개
- **최대 값 크기**: 512 bytes (64축 모터 데이터)
- **총 메모리 사용**: <10MB

## 문제 해결

### Folly를 찾을 수 없음

```bash
# pkg-config 경로 확인
pkg-config --modversion folly

# 없으면 LD_LIBRARY_PATH 설정
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
sudo ldconfig
```

### 컴파일 에러: folly/AtomicHashMap.h not found

```bash
# Folly 설치 확인
ls /usr/local/include/folly/

# 없으면 재설치
cd /tmp/folly/_build
sudo make install
```

## 참고 자료

- [Folly GitHub](https://github.com/facebook/folly)
- [Google Benchmark GitHub](https://github.com/google/benchmark)
- [Feature 019 연구 문서](./research.md)
- [Phase 4 태스크](./tasks.md#phase-4-us2---hot-key-최적화-p2)
