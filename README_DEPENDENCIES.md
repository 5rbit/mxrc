# MXRC 프로젝트 의존성 설치 가이드

## 🚀 빠른 시작

```bash
# 1. 전체 의존성 자동 설치 (권장)
sudo ./scripts/install-dependencies.sh --all

# 2. CMake 재구성
cd build
cmake ..

# 3. 빌드
make -j$(nproc)

# 4. 테스트
./run_tests
```

## 📦 의존성 패키지 목록

### ✅ 필수 (REQUIRED)

프로젝트 빌드에 **반드시** 필요한 패키지:

```bash
# 자동 설치
sudo ./scripts/install-dependencies.sh --required

# 수동 설치
sudo apt-get install -y \
    build-essential g++ cmake git pkg-config \
    python3 python3-pip \
    libspdlog-dev libgtest-dev libtbb-dev \
    nlohmann-json3-dev libyaml-cpp-dev libsystemd-dev

pip3 install --break-system-packages jinja2 pyyaml
```

### ⚠️ 선택 (OPTIONAL)

특정 기능 활성화를 위한 패키지:

#### Hot Key 최적화 (Feature 019 Phase 4)
```bash
# Folly + Google Benchmark
# 설치 시간: 30-40분
sudo ./scripts/install-dependencies.sh --optional
```

또는 [상세 가이드](docs/specs/019-architecture-improvements/folly-benchmark-installation.md) 참조

#### 성능 최적화
```bash
sudo apt-get install -y libboost-all-dev libnuma-dev
```

## 📊 설치 확인

```bash
cd build
cmake ..
```

**성공 메시지 예시**:
```
========================================
Checking REQUIRED dependencies...
========================================
✓ All REQUIRED dependencies found
  - Threads: OK
  - spdlog: OK
  - GTest: OK
  - TBB: OK (concurrent containers)
  - nlohmann_json: OK
  - systemd: systemd

========================================
Feature 019: Hot Key Optimization
========================================
✓ Folly: FOUND - Hot Key optimization ENABLED
✓ Google Benchmark: FOUND

========================================
Dependency Check Summary
========================================
Required:  ✓ ALL FOUND
IPC Schema: ✓ ENABLED
Hot Key:    ✓ ENABLED
Monitoring: ✓ ENABLED
```

**경고 메시지** (선택 패키지 미설치):
```
✗ Folly: NOT FOUND - Hot Key optimization DISABLED
   See: docs/specs/019-architecture-improvements/folly-benchmark-installation.md
```

## 🔧 설치 스크립트 옵션

```bash
# 도움말
sudo ./scripts/install-dependencies.sh --help

# 필수 의존성만
sudo ./scripts/install-dependencies.sh --required

# 선택 의존성만 (이미 필수 설치 완료 시)
sudo ./scripts/install-dependencies.sh --optional

# 전체 설치
sudo ./scripts/install-dependencies.sh --all
```

## 📖 상세 문서

- **전체 의존성 목록**: [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md)
- **Folly 설치 가이드**: [docs/specs/019-architecture-improvements/folly-benchmark-installation.md](docs/specs/019-architecture-improvements/folly-benchmark-installation.md)
- **CMakeLists.txt**: 의존성 검사 로직 확인

## ❓ 문제 해결

### yaml-cpp not found
```bash
sudo apt-get install -y libyaml-cpp-dev
```

### Folly 빌드 실패
```bash
# 의존성 재설치
sudo apt-get install -y \
    libevent-dev libdouble-conversion-dev libgoogle-glog-dev \
    libgflags-dev libiberty-dev liblz4-dev liblzma-dev \
    libsnappy-dev zlib1g-dev binutils-dev libjemalloc-dev \
    libssl-dev libunwind-dev libfmt-dev libsodium-dev

# 재빌드
cd /tmp/folly/_build
make clean && cmake .. && make -j$(nproc) && sudo make install && sudo ldconfig
```

### Python 패키지 오류
```bash
# Ubuntu 24.04
pip3 install --break-system-packages jinja2 pyyaml
```

## 🎯 기능별 의존성 매핑

| 기능 | 필요 패키지 | 상태 |
|------|------------|------|
| 기본 빌드 | spdlog, GTest, TBB, nlohmann_json, systemd | ✅ 필수 |
| IPC 스키마 | yaml-cpp, Python (jinja2, pyyaml) | ✅ 필수 |
| Hot Key 최적화 | Folly, Google Benchmark | ⚠️ 선택 |
| 성능 최적화 | Boost, NUMA | ⚠️ 선택 |
| Monitoring | prometheus-cpp, CivetWeb | ⚠️ 선택 |
| EtherCAT | IgH EtherCAT Master | ⚠️ 선택 |

## 💡 팁

1. **최소 설치**: 빠르게 시작하려면 필수 의존성만 설치
   ```bash
   sudo ./scripts/install-dependencies.sh --required
   ```

2. **Hot Key 최적화**: 나중에 Folly 설치 가능
   ```bash
   # Folly 설치 후
   sudo ./scripts/install-dependencies.sh --optional
   # DataStore.h/cpp 주석 제거
   # 재빌드
   ```

3. **CMake 캐시 정리**: 의존성 설치 후 빌드가 안 되면
   ```bash
   rm -rf build/*
   cd build && cmake ..
   ```

---

**참고**: Ubuntu 24.04 LTS 기준으로 작성되었습니다.
