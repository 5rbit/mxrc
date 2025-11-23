# MXRC Project - Dependencies Installation Guide

**버전**: 1.0
**대상 OS**: Ubuntu 24.04 LTS
**최종 수정**: 2025-11-23

## 목차
- [빠른 시작](#빠른-시작)
- [필수 의존성](#필수-의존성-required)
- [선택적 의존성](#선택적-의존성-optional)
- [수동 설치](#수동-설치)
- [설치 확인](#설치-확인)
- [문제 해결](#문제-해결)

---

## 빠른 시작

### 자동 설치 (권장)

```bash
# 모든 의존성 설치 (필수 + 선택)
sudo ./scripts/install-dependencies.sh --all

# 필수 의존성만 설치
sudo ./scripts/install-dependencies.sh --required

# 선택적 의존성만 설치 (이미 필수 의존성이 설치된 경우)
sudo ./scripts/install-dependencies.sh --optional
```

**예상 소요 시간**:
- 필수 의존성: 5-10분
- 선택적 의존성: 30-40분 (Folly 빌드 시간 포함)

---

## 필수 의존성 (REQUIRED)

다음 패키지들은 프로젝트 빌드에 **반드시 필요**합니다.

### 빌드 도구

| 패키지 | 용도 | 설치 명령 |
|--------|------|-----------|
| build-essential | GCC/G++ 컴파일러 | `apt install build-essential` |
| cmake | 빌드 시스템 | `apt install cmake` |
| git | 버전 관리 | `apt install git` |
| pkg-config | 라이브러리 검색 | `apt install pkg-config` |
| python3 | 코드 생성 스크립트 | `apt install python3 python3-pip` |

### C++ 라이브러리

| 패키지 | 버전 | 용도 | CMake 패키지명 |
|--------|------|------|----------------|
| spdlog | latest | 고성능 로깅 | `spdlog` |
| Google Test | latest | 단위 테스트 프레임워크 | `GTest` |
| TBB | latest | Thread Building Blocks (concurrent_hash_map) | `TBB` |
| nlohmann-json | 3.11.0+ | JSON 파싱 | `nlohmann_json` |
| yaml-cpp | latest | YAML 파싱 (IPC 스키마) | `yaml-cpp` |
| systemd | latest | systemd 통합 | `libsystemd` |

### Python 패키지

| 패키지 | 용도 |
|--------|------|
| jinja2 | 코드 생성 템플릿 엔진 |
| pyyaml | YAML 파싱 (스키마 검증) |

### 설치 명령

```bash
# APT 패키지
sudo apt-get update
sudo apt-get install -y \
    build-essential g++ cmake git pkg-config python3 python3-pip \
    libspdlog-dev libgtest-dev libtbb-dev nlohmann-json3-dev \
    libyaml-cpp-dev libsystemd-dev

# Python 패키지
pip3 install --break-system-packages jinja2 pyyaml
```

---

## 선택적 의존성 (OPTIONAL)

다음 패키지들은 특정 기능을 활성화하기 위해 필요합니다.

### 성능 최적화

#### Boost (lock-free data structures)
- **용도**: EventBus, RT 데이터 구조
- **CMake**: `find_package(Boost 1.65)`
- **상태**: ⚠️ 선택 (성능 향상)

```bash
sudo apt-get install -y libboost-all-dev
```

#### NUMA (Non-Uniform Memory Access)
- **용도**: CPU 친화도, 메모리 최적화
- **CMake**: `find_library(NUMA_LIBRARY numa)`
- **상태**: ⚠️ 선택 (성능 향상)

```bash
sudo apt-get install -y libnuma-dev
```

---

### Hot Key 최적화 (Feature 019 Phase 4)

#### Google Benchmark
- **용도**: 성능 벤치마크
- **CMake**: `find_package(benchmark)`
- **상태**: ⚠️ 선택 (Hot Key 벤치마크)
- **설치**: 소스에서 빌드 (Ubuntu 24.04에 없음)

```bash
cd /tmp
git clone https://github.com/google/benchmark.git
cd benchmark
cmake -E make_directory "build"
cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
cmake --build "build" --config Release
sudo cmake --build "build" --config Release --target install
```

#### Folly
- **용도**: AtomicHashMap (Hot Key 캐시)
- **CMake**: `find_package(folly)`
- **상태**: ⚠️ 선택 (Hot Key 최적화)
- **설치**: 소스에서 빌드 (Ubuntu 24.04에 없음)

**의존성 설치**:
```bash
sudo apt-get install -y \
    libevent-dev libdouble-conversion-dev libgoogle-glog-dev \
    libgflags-dev libiberty-dev liblz4-dev liblzma-dev \
    libsnappy-dev zlib1g-dev binutils-dev libjemalloc-dev \
    libssl-dev libunwind-dev libfmt-dev libsodium-dev
```

**Folly 빌드** (10-20분 소요):
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

---

### Monitoring (Feature 019 Phase 7)

#### prometheus-cpp
- **용도**: Prometheus 메트릭 수집
- **CMake**: `find_package(prometheus-cpp)`
- **상태**: ⚠️ 선택 (메트릭 수집)
- **설치**: 소스에서 빌드

```bash
cd /tmp
git clone --recursive https://github.com/jupp0r/prometheus-cpp.git
cd prometheus-cpp
mkdir _build && cd _build
cmake .. -DBUILD_SHARED_LIBS=ON
make -j$(nproc)
sudo make install
sudo ldconfig
```

#### CivetWeb
- **용도**: HTTP 서버 (메트릭 엔드포인트)
- **CMake**: `find_package(civetweb)`
- **상태**: ⚠️ 선택 (HTTP API)

```bash
sudo apt-get install -y libcivetweb-dev
```

---

### EtherCAT (Feature 001)

#### IgH EtherCAT Master
- **용도**: EtherCAT 통신
- **CMake**: `find_package(EtherCAT)`
- **상태**: ⚠️ 선택 (EtherCAT 필드버스)
- **설치**: [공식 사이트](https://gitlab.com/etherlab.org/ethercat)에서 수동 빌드

```bash
# 별도 설치 가이드 필요
# https://gitlab.com/etherlab.org/ethercat
```

---

## 설치 확인

### CMake 구성 확인

```bash
cd build
cmake ..
```

**성공 메시지 예시**:
```
-- Found spdlog
-- Found GTest
-- Found TBB
-- Found nlohmann_json
-- systemd found: systemd
-- yaml-cpp found
-- Boost found: /usr/include
-- NUMA library found: /usr/lib/x86_64-linux-gnu/libnuma.so
-- Google Benchmark found
-- Folly found
```

**경고 메시지** (선택적 패키지):
```
CMake Warning: Boost not found. Install with: sudo apt install libboost-dev
CMake Warning: Folly not found. Install with: [설치 가이드]
```

### 빌드 테스트

```bash
# 전체 빌드
make -j$(nproc)

# 테스트 실행
./run_tests

# 특정 기능 테스트
./run_tests --gtest_filter="HotKey*"       # Hot Key (Folly 필요)
./run_tests --gtest_filter="*Priority*"   # EventBus 우선순위
./run_tests --gtest_filter="*Monitoring*" # Monitoring
```

---

## 수동 설치

### 최소 설치 (필수 의존성만)

프로젝트를 빌드하고 기본 테스트를 실행하려면:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    libspdlog-dev libgtest-dev libtbb-dev \
    nlohmann-json3-dev libyaml-cpp-dev libsystemd-dev

pip3 install --break-system-packages jinja2 pyyaml
```

### 권장 설치 (성능 최적화 포함)

```bash
# 필수 + Boost + NUMA
sudo apt-get install -y \
    build-essential cmake git \
    libspdlog-dev libgtest-dev libtbb-dev \
    nlohmann-json3-dev libyaml-cpp-dev libsystemd-dev \
    libboost-all-dev libnuma-dev

pip3 install --break-system-packages jinja2 pyyaml
```

### 전체 설치 (모든 기능 활성화)

자동 설치 스크립트 사용:
```bash
sudo ./scripts/install-dependencies.sh --all
```

---

## 문제 해결

### CMake가 패키지를 찾지 못함

**증상**: `CMake Error: Could not find package ...`

**해결**:
```bash
# 패키지 캐시 업데이트
sudo apt-get update

# pkg-config 경로 확인
pkg-config --list-all | grep <패키지명>

# CMake 캐시 삭제 후 재시도
rm -rf build/*
cd build && cmake ..
```

### Folly 설치 실패

**증상**: 컴파일 에러 또는 의존성 누락

**해결**:
1. 의존성 재설치
   ```bash
   sudo apt-get install -y \
       libevent-dev libdouble-conversion-dev libgoogle-glog-dev \
       libgflags-dev libiberty-dev liblz4-dev liblzma-dev \
       libsnappy-dev zlib1g-dev binutils-dev libjemalloc-dev \
       libssl-dev libunwind-dev libfmt-dev libsodium-dev
   ```

2. 빌드 디렉토리 정리
   ```bash
   cd /tmp/folly/_build
   make clean
   cmake ..
   make -j$(nproc)
   sudo make install
   sudo ldconfig
   ```

### Python 패키지 설치 오류

**증상**: `error: externally-managed-environment`

**해결** (Ubuntu 24.04):
```bash
# --break-system-packages 플래그 사용
pip3 install --break-system-packages jinja2 pyyaml

# 또는 venv 사용
python3 -m venv .venv
source .venv/bin/activate
pip install jinja2 pyyaml
```

### 라이브러리 링크 오류

**증상**: 빌드는 되지만 실행 시 `.so` 파일을 찾지 못함

**해결**:
```bash
# 라이브러리 캐시 업데이트
sudo ldconfig

# LD_LIBRARY_PATH 확인
echo $LD_LIBRARY_PATH

# 필요시 추가
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

---

## 의존성 트리

```
MXRC Project
├── 필수 (REQUIRED)
│   ├── build-essential (컴파일러)
│   ├── cmake (빌드 시스템)
│   ├── spdlog (로깅)
│   ├── GTest (테스트)
│   ├── TBB (concurrent_hash_map)
│   ├── nlohmann_json (JSON)
│   ├── yaml-cpp (YAML)
│   ├── systemd (systemd 통합)
│   └── Python (jinja2, pyyaml)
│
├── 성능 최적화 (OPTIONAL)
│   ├── Boost (lock-free structures)
│   └── NUMA (메모리 최적화)
│
├── Hot Key 최적화 (OPTIONAL - Feature 019)
│   ├── Folly (AtomicHashMap)
│   │   └── 의존성: libevent, glog, gflags, lz4, ...
│   └── Google Benchmark (벤치마크)
│
├── Monitoring (OPTIONAL - Feature 019)
│   ├── prometheus-cpp (메트릭)
│   └── CivetWeb (HTTP 서버)
│
└── EtherCAT (OPTIONAL - Feature 001)
    └── IgH EtherCAT Master
```

---

## 패키지 상태 요약

| 패키지 | 필수/선택 | Ubuntu 24.04 | 설치 방법 | 기능 |
|--------|----------|--------------|-----------|------|
| spdlog | ✅ 필수 | ✅ 있음 | apt | 로깅 |
| GTest | ✅ 필수 | ✅ 있음 | apt | 테스트 |
| TBB | ✅ 필수 | ✅ 있음 | apt | concurrent_hash_map |
| nlohmann_json | ✅ 필수 | ✅ 있음 | apt | JSON |
| yaml-cpp | ✅ 필수 | ✅ 있음 | apt | YAML |
| systemd | ✅ 필수 | ✅ 있음 | apt | systemd |
| Boost | ⚠️ 선택 | ✅ 있음 | apt | 성능 최적화 |
| NUMA | ⚠️ 선택 | ✅ 있음 | apt | 성능 최적화 |
| Google Benchmark | ⚠️ 선택 | ❌ 없음 | 소스 | Hot Key 벤치마크 |
| Folly | ⚠️ 선택 | ❌ 없음 | 소스 | Hot Key 캐시 |
| prometheus-cpp | ⚠️ 선택 | ❌ 없음 | 소스 | 메트릭 |
| CivetWeb | ⚠️ 선택 | ⚠️ 불확실 | apt/소스 | HTTP 서버 |
| EtherCAT Master | ⚠️ 선택 | ❌ 없음 | 소스 | EtherCAT |

---

## 참고 자료

- [CMakeLists.txt](../CMakeLists.txt): 전체 의존성 목록
- [Folly 설치 가이드](specs/019-architecture-improvements/folly-benchmark-installation.md)
- [Feature 019 완료 요약](specs/019-architecture-improvements/completion-summary.md)

---

**작성**: MXRC Team
**검토**: Claude Code
**라이선스**: MIT
