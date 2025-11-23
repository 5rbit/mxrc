# Quickstart: Docker로 MXRC 시작하기

**Feature**: 002-docker
**Date**: 2025-01-23
**Status**: Completed
**Phase**: 1 (Design)

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Docker, Container, Volume 등)
- 사용 설명, 명령어 설명은 모두 한글로 작성합니다

---

## Overview

이 가이드는 Docker를 사용하여 MXRC 개발 환경을 10분 이내에 구축하고, 첫 번째 빌드와 테스트를 실행하는 방법을 설명합니다.

---

## Prerequisites

### 필수 요구사항

1. **Docker Engine 24.0+** 또는 **Docker Desktop** 설치
   ```bash
   # Docker 버전 확인
   docker --version  # 예: Docker version 24.0.7
   docker compose version  # 예: Docker Compose version v2.23.0
   ```

2. **시스템 요구사항**
   - OS: Ubuntu 24.04 LTS, macOS, 또는 Windows (WSL2)
   - RAM: 최소 8GB (권장 16GB)
   - 디스크: 최소 20GB 여유 공간

3. **Git**
   ```bash
   git --version  # 예: git version 2.43.0
   ```

### 선택적 요구사항

- **PREEMPT_RT 커널**: 실시간 성능 테스트를 위해 필요 (선택적)
- **EtherCAT 하드웨어**: EtherCAT 통합 테스트를 위해 필요 (선택적)

---

## Quick Start (5분)

### Step 1: Repository 클론

```bash
git clone https://github.com/your-org/mxrc.git
cd mxrc
```

### Step 2: 개발 환경 시작

```bash
# 개발 환경 Container 시작
docker compose up dev
```

**예상 출력**:
```
[+] Building 120.3s (15/15) FINISHED
[+] Running 1/1
 ✔ Container mxrc-dev-1  Started
Attaching to dev-1
dev-1  | MXRC Development Environment Ready
dev-1  | Workspace: /workspace
dev-1  | Run './build.sh' to start building
```

**첫 실행 시**: Docker Image 빌드에 약 5-10분 소요 (의존성 다운로드 포함)
**이후 실행**: 수초 이내 시작 (Image 캐시 활용)

### Step 3: Container 접속

```bash
# 실행 중인 Container에 접속
docker compose exec dev /bin/bash
```

**Container 내부 확인**:
```bash
# 현재 위치 확인
pwd  # /workspace

# 소스 코드 확인
ls src/

# 빌드 디렉토리 확인
ls build/
```

### Step 4: 빌드 및 테스트

```bash
# Container 내부에서 빌드 실행
cd /workspace
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 테스트 실행
cd /workspace
./run_tests
```

**예상 결과**:
```
[==========] Running 195 tests from 42 test suites.
[----------] Global test environment set-up.
...
[----------] Global test environment tear-down
[==========] 195 tests from 42 test suites ran. (1234 ms total)
[  PASSED  ] 195 tests.
```

**목표 시간**:
- 첫 번째 빌드: 10분 이내 ✅
- 이후 증분 빌드: 30초 이내 ✅

---

## Common Tasks

### 1. 코드 수정 및 재빌드

**호스트에서 코드 수정** (IDE 사용):
```bash
# 호스트에서 파일 수정
vim src/core/action/ActionExecutor.cpp
```

**Container에서 재빌드**:
```bash
# Container 내부에서
cd /workspace/build
make -j$(nproc)  # 변경된 파일만 재컴파일 (30초 이내)
```

Volume Mount 덕분에 호스트의 변경사항이 Container에 즉시 반영됩니다.

---

### 2. 특정 테스트만 실행

```bash
# Container 내부에서
./run_tests --gtest_filter=ActionExecutor*
./run_tests --gtest_filter=SequenceEngine*
```

---

### 3. 디버깅

**GDB 사용**:
```bash
# Container 내부에서
gdb ./build/mxrc
(gdb) break main
(gdb) run
(gdb) backtrace
```

**Valgrind로 메모리 검사**:
```bash
# Container 내부에서
valgrind --leak-check=full ./build/mxrc
```

**AddressSanitizer** (기본 활성화):
```bash
# Debug 빌드에는 AddressSanitizer가 기본 포함
./run_tests  # 메모리 이슈 자동 감지
```

---

### 4. Container 종료 및 재시작

**Container 종료**:
```bash
# 다른 터미널에서
docker compose down

# 또는 Container 내부에서 Ctrl+D
```

**Container 재시작**:
```bash
docker compose up dev
```

빌드 결과와 로그는 Volume에 저장되어 있어 Container 재시작 후에도 유지됩니다.

---

## Profiles

MXRC는 4가지 Docker Compose Profile을 제공합니다.

### 1. dev (개발 환경) - 기본

```bash
docker compose up dev
```

**특징**:
- 모든 Volume Mount (src, build, logs, test-results)
- 디버깅 도구 포함 (GDB, LLDB, Valgrind)
- Interactive Shell
- 용도: 로컬 개발, 디버깅, 수동 테스트

---

### 2. ci (CI/CD 환경)

```bash
docker compose --profile ci up ci
```

**특징**:
- 최소 Volume (test-results만)
- 빠른 빌드 (Pre-built Image 사용)
- 자동 테스트 실행
- 용도: GitHub Actions, GitLab CI

**GitHub Actions에서 사용**:
```yaml
# .github/workflows/docker-ci.yml
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: docker compose --profile ci up ci --exit-code-from ci
```

---

### 3. rt-test (실시간 성능 테스트)

```bash
# PREEMPT_RT 커널이 설치된 호스트에서
docker compose --profile rt-test up rt-test
```

**특징**:
- 실시간 우선순위 (SYS_NICE Capability)
- 성능 벤치마크 스크립트 자동 실행
- Task 실행 오버헤드 측정
- 용도: 실시간 성능 검증

**결과 확인**:
```bash
# 벤치마크 결과 확인
cat logs/rt-benchmark.log
```

**경고**: PREEMPT_RT 커널 없이 실행하면 경고 메시지 출력 후 일반 테스트만 실행

---

### 4. ethercat-dev (EtherCAT 통합)

```bash
# EtherCAT 하드웨어 연결 후
docker compose --profile ethercat-dev up ethercat-dev --privileged
```

**특징**:
- IgH EtherCAT Master 포함
- Privileged 모드 (하드웨어 접근)
- Mock 환경 지원 (하드웨어 없이 테스트)
- 용도: EtherCAT 센서/모터 통합 테스트

**Mock 환경으로 테스트**:
```bash
# Container 내부에서
ETHERCAT_MOCK=1 ./run_tests --gtest_filter=EtherCAT*
```

---

## Troubleshooting

### 문제 1: Docker가 설치되지 않음

**증상**:
```
bash: docker: command not found
```

**해결**:
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y docker.io docker-compose-plugin

# macOS (Homebrew)
brew install --cask docker

# 설치 확인
docker --version
```

---

### 문제 2: Volume Mount 권한 오류

**증상**:
```
Permission denied: /workspace/build
```

**해결**:
```bash
# 호스트에서 build 디렉토리 권한 수정
chmod -R 777 build/

# 또는 사용자 UID/GID를 Container와 맞춤
docker compose run --user $(id -u):$(id -g) dev
```

---

### 문제 3: Image 빌드 실패

**증상**:
```
ERROR: failed to solve: failed to fetch
```

**해결**:
```bash
# Docker 캐시 제거
docker builder prune

# 처음부터 재빌드
docker compose build --no-cache dev
```

---

### 문제 4: Container 메모리 부족

**증상**:
```
c++: fatal error: Killed signal terminated program cc1plus
```

**해결**:
```bash
# Docker Desktop 설정에서 메모리 증가 (최소 8GB)
# 또는 병렬 빌드 수 감소
make -j2  # 대신 make -j$(nproc)
```

---

### 문제 5: macOS/Windows에서 느린 빌드

**증상**: Volume Mount로 인한 파일 시스템 성능 저하

**해결**:
```yaml
# docker-compose.yml에서 :cached 옵션 추가
volumes:
  - ./src:/workspace/src:cached
  - ./build:/workspace/build:delegated
```

또는 Named Volume 사용:
```yaml
volumes:
  mxrc-build:

services:
  dev:
    volumes:
      - mxrc-build:/workspace/build  # Named Volume (빠름)
```

---

## Best Practices

### 1. 빌드 캐시 활용

**DO**:
```bash
# 증분 빌드로 시간 절약
docker compose exec dev make -j$(nproc)
```

**DON'T**:
```bash
# 매번 clean build는 시간 낭비
docker compose exec dev make clean && make
```

---

### 2. Volume 정리

**주기적으로 사용하지 않는 Volume 제거**:
```bash
# 사용하지 않는 Volume 확인
docker volume ls

# 사용하지 않는 Volume 제거
docker volume prune
```

---

### 3. Image 크기 관리

**사용하지 않는 Image 제거**:
```bash
# 모든 중지된 Container 제거
docker container prune

# 사용하지 않는 Image 제거
docker image prune -a
```

---

### 4. 로그 확인

**Container 로그 실시간 확인**:
```bash
docker compose logs -f dev
```

**특정 시점 로그 확인**:
```bash
docker compose logs --since 10m dev  # 최근 10분
```

---

## Performance Tips

### 1. 첫 번째 빌드 시간 단축

```bash
# Pre-built CI Image를 개발 환경 Base로 사용
docker pull ghcr.io/your-org/mxrc-ci:latest
docker compose build --build-arg BASE_IMAGE=ghcr.io/your-org/mxrc-ci:latest dev
```

---

### 2. 병렬 빌드 최적화

```bash
# CPU 코어 수에 맞게 병렬 빌드
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS
```

---

### 3. ccache 사용 (선택적)

```dockerfile
# Dockerfile에 ccache 추가
RUN apt-get install -y ccache && \
    ln -s /usr/bin/ccache /usr/local/bin/gcc && \
    ln -s /usr/bin/ccache /usr/local/bin/g++
```

```yaml
# docker-compose.yml에 ccache Volume 추가
volumes:
  - ccache:/root/.ccache
```

---

## Next Steps

1. **코드 작성**: 호스트 IDE에서 개발, Container에서 빌드/테스트
2. **PR 생성**: GitHub Actions가 자동으로 CI Profile로 테스트 실행
3. **문서 참조**:
   - [README.md](../../../README.md): 전체 프로젝트 개요
   - [CLAUDE.md](../../../dev/agent/CLAUDE.md): 개발 가이드
   - [docker-setup.md](../../../docs/onboarding/docker-setup.md): 상세 Docker 설정

---

## FAQ

### Q1: Docker 없이 개발할 수 있나요?

**A**: 네, 기존 방식대로 호스트에 직접 의존성을 설치하여 개발 가능합니다. Docker는 선택 사항이며, 환경 표준화를 위해 권장됩니다.

---

### Q2: Container 내부의 파일을 호스트에서 수정할 수 있나요?

**A**: 네, Volume Mount로 연결되어 있어 호스트 IDE에서 수정하면 Container에 즉시 반영됩니다.

---

### Q3: Container를 종료하면 빌드 결과가 사라지나요?

**A**: 아니요, 빌드 결과는 `./build` Volume에 저장되어 Container 재시작 후에도 유지됩니다.

---

### Q4: 여러 Profile을 동시에 실행할 수 있나요?

**A**: 네, 가능합니다.
```bash
docker compose --profile dev --profile rt-test up
```

---

### Q5: Image를 업데이트하려면?

**A**:
```bash
# 최신 Image Pull
docker pull ghcr.io/your-org/mxrc-dev:latest

# 또는 로컬에서 재빌드
docker compose build dev
```

---

**Last Updated**: 2025-01-23
**Status**: Completed ✅
