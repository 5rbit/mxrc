# Research: Docker 컨테이너화 모범 사례

**Feature**: 002-docker
**Date**: 2025-01-23
**Status**: Completed
**Phase**: 0 (Research)

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Docker, Multi-stage Build, Layer Caching 등)
- 조사 내용, 결정 사항, 대안 평가는 모두 한글로 작성합니다

---

## Overview

Docker 통합을 위한 기술 조사 및 설계 결정 사항을 문서화합니다. C++ 프로젝트의 특성과 실시간 성능 요구사항을 고려하여 최적의 Docker 구성을 선택합니다.

---

## Research Topics

### 1. Docker Base Image 선택

**Decision**: `ubuntu:24.04` 사용

**Rationale**:
- MXRC는 Ubuntu 24.04 LTS PREEMPT_RT 환경을 타겟으로 합니다
- 호스트와 Container의 OS 일치성을 통해 호환성 문제 최소화
- APT 패키지 관리자를 통한 의존성 설치 용이
- PREEMPT_RT 커널은 호스트에서 제공하며, Container는 커널을 공유합니다

**Alternatives Considered**:
- `alpine:latest`: 이미지 크기는 작지만, musl libc로 인한 호환성 문제 우려 (GCC, TBB 등)
- `debian:bookworm`: 안정성은 우수하지만, Ubuntu 24.04 타겟 환경과 차이 발생
- Custom Base Image: 불필요한 복잡도 증가, 표준 Image 사용이 유지보수에 유리

**Implementation Notes**:
```dockerfile
FROM ubuntu:24.04

# 타임존 설정 (interactive prompt 방지)
ENV TZ=Asia/Seoul
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
```

---

### 2. Multi-stage Build vs Single-stage Build

**Decision**: 초기 버전에서는 Single-stage Build 사용, 향후 Multi-stage로 전환 고려

**Rationale**:
- MXRC는 개발 환경 표준화가 주요 목표이며, 배포용 최적화는 우선순위가 낮습니다
- Single-stage Build가 디버깅 및 개발에 유리합니다 (모든 도구가 Container에 포함)
- Volume Mount를 통한 증분 빌드 지원으로 빌드 시간 단축 가능
- 향후 Production 배포 시 Multi-stage Build로 전환하여 이미지 크기 최적화 가능

**Alternatives Considered**:
- Multi-stage Build: 이미지 크기 최적화에 유리하나, 개발 환경에서는 오히려 복잡도 증가
- Buildkit Cache Mount: 의존성 캐싱에 유용하나, Docker Compose 통합 시 복잡도 증가

**Implementation Notes**:
현재는 모든 빌드 도구와 런타임을 하나의 Image에 포함합니다. 향후 Kubernetes 배포 시 Multi-stage로 전환:
```dockerfile
# 향후 Multi-stage 예시 (현재 미사용)
# Stage 1: Build
FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake ...
COPY . /workspace
RUN cmake -B build && make -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:24.04
COPY --from=builder /workspace/build/mxrc /app/mxrc
CMD ["/app/mxrc"]
```

---

### 3. Docker Compose Profile 설계

**Decision**: 4개 프로파일 제공 (dev, ci, rt-test, ethercat-dev)

**Rationale**:
- **dev**: 개발자가 가장 많이 사용하는 기본 프로파일, Volume Mount로 소스 코드 동기화
- **ci**: CI/CD Pipeline 최적화, 최소 의존성만 설치하여 빌드 시간 단축
- **rt-test**: 실시간 성능 테스트, PREEMPT_RT 커널 요구사항 명시
- **ethercat-dev**: EtherCAT 하드웨어 테스트, Privileged 모드로 하드웨어 접근

**Alternatives Considered**:
- 단일 프로파일: 모든 기능을 하나의 설정에 포함하면 복잡도 증가 및 빌드 시간 증가
- 8개 이상 프로파일: 과도한 세분화로 관리 복잡도 증가

**Implementation Notes**:
```yaml
# docker-compose.yml 예시
services:
  dev:
    profiles: ["dev"]
    build:
      context: .
      dockerfile: Dockerfile
    volumes:
      - ./src:/workspace/src
      - ./build:/workspace/build
    command: /bin/bash

  ci:
    profiles: ["ci"]
    build:
      context: .
      dockerfile: Dockerfile.ci
    command: ./docker/scripts/test.sh
```

---

### 4. Volume Mount 전략

**Decision**: 소스 코드, 빌드 출력, 로그, 테스트 결과를 각각 별도 Volume으로 관리

**Rationale**:
- 소스 코드는 호스트와 실시간 동기화 필요 (IDE에서 수정 즉시 반영)
- 빌드 출력은 호스트에서도 접근 가능해야 함 (디버깅, 실행)
- 로그 파일은 호스트에 영구 저장되어 Container 재시작 후에도 유지
- 테스트 결과는 CI/CD에서 아티팩트로 수집 가능

**Alternatives Considered**:
- Named Volume: 성능은 우수하나, 호스트에서 직접 접근 어려움
- 모든 파일을 하나의 Volume: 권한 충돌 및 관리 복잡도 증가

**Implementation Notes**:
```yaml
volumes:
  - ./src:/workspace/src:rw              # 소스 코드 (읽기/쓰기)
  - ./build:/workspace/build:rw          # 빌드 출력 (읽기/쓰기)
  - ./logs:/workspace/logs:rw            # 로그 파일 (읽기/쓰기)
  - ./test-results:/workspace/test-results:rw  # 테스트 결과 (읽기/쓰기)
```

**Performance Considerations**:
- macOS/Windows에서 Volume Mount 성능 저하 가능 (VirtioFS, gRPC-FUSE)
- Linux에서는 Native 성능 (Bind Mount)
- 대용량 파일 빌드 시 Named Volume + 결과물만 Copy 전략 고려

---

### 5. 의존성 설치 최적화

**Decision**: Layer Caching을 활용하여 의존성 변경 시에만 재설치

**Rationale**:
- APT 패키지는 자주 변경되지 않으므로 캐싱 효과가 큽니다
- Dockerfile에서 의존성 설치를 소스 코드 복사보다 먼저 수행
- Docker Layer Caching으로 재빌드 시간 대폭 단축

**Alternatives Considered**:
- 모든 명령을 하나의 RUN으로 통합: Layer 캐싱 불가능, 빌드 시간 증가
- 의존성을 Base Image에 미리 구워 두기: 의존성 변경 시 Base Image 재빌드 필요

**Implementation Notes**:
```dockerfile
# 의존성 목록을 별도 파일로 관리
COPY docker/dependencies.txt /tmp/
RUN apt-get update && \
    xargs -a /tmp/dependencies.txt apt-get install -y && \
    rm -rf /var/lib/apt/lists/*

# 소스 코드는 나중에 복사 (변경이 잦으므로)
COPY . /workspace
```

**Dependency List Example** (`docker/dependencies.txt`):
```
build-essential
cmake
libspdlog-dev
libgtest-dev
libtbb-dev
nlohmann-json3-dev
libyaml-cpp-dev
gdb
valgrind
```

---

### 6. EtherCAT Master 설치 전략

**Decision**: 선택적 설치, 별도 Dockerfile (Dockerfile.ethercat)로 관리

**Rationale**:
- EtherCAT Master는 Kernel Module이므로 특정 환경에서만 필요
- 기본 개발 환경(dev)에는 포함하지 않아 이미지 크기 및 빌드 시간 절약
- EtherCAT 하드웨어가 없는 환경에서는 Mock 기반 테스트 제공

**Alternatives Considered**:
- 모든 Image에 EtherCAT 포함: 불필요한 복잡도 및 이미지 크기 증가
- Runtime에 설치: Container 시작 시간 증가, 재현성 저하

**Implementation Notes**:
```dockerfile
# Dockerfile.ethercat
FROM ubuntu:24.04

# EtherCAT Master 빌드 및 설치
RUN apt-get update && apt-get install -y git build-essential autoconf libtool && \
    git clone https://gitlab.com/etherlab.org/ethercat.git /tmp/ethercat && \
    cd /tmp/ethercat && \
    ./bootstrap && \
    ./configure --prefix=/usr/local --disable-8139too --disable-eoe && \
    make && make install && \
    rm -rf /tmp/ethercat

# Privileged 모드로 실행 필요
# docker-compose.yml에서 privileged: true 설정
```

---

### 7. CI/CD 최적화 전략

**Decision**: 미리 빌드된 Docker Image를 GitHub Container Registry에 저장

**Rationale**:
- CI Pipeline에서 매번 의존성을 설치하면 5분 이상 소요
- 미리 빌드된 Image를 사용하면 빌드 시간 대폭 단축 (의존성 설치 스킵)
- 의존성 변경 시에만 Image 재빌드

**Alternatives Considered**:
- 매번 처음부터 빌드: 시간 낭비, CI 비용 증가
- Docker Hub Public Registry: Private Repository 제한 (무료 플랜)

**Implementation Notes**:
```yaml
# .github/workflows/docker-ci.yml
name: Docker CI

on:
  push:
    branches: [main, 002-docker]
  pull_request:

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Pull pre-built image
        run: docker pull ghcr.io/${{ github.repository }}/mxrc-ci:latest || true

      - name: Build with cache
        run: docker build --cache-from ghcr.io/${{ github.repository }}/mxrc-ci:latest \
                          -t mxrc-ci:latest \
                          -f Dockerfile.ci .

      - name: Run tests
        run: docker run --rm mxrc-ci:latest ./run_tests
```

---

### 8. 실시간 성능 테스트 환경

**Decision**: PREEMPT_RT 커널은 호스트에서 제공, Container는 `--cap-add=SYS_NICE` 권한으로 우선순위 조정

**Rationale**:
- Container는 호스트 커널을 공유하므로, PREEMPT_RT 커널이 호스트에 설치되어 있어야 합니다
- `SYS_NICE` Capability로 Real-time Priority 설정 가능
- `--cpu-rt-runtime` 옵션으로 CPU 시간 제한 설정

**Alternatives Considered**:
- Custom Kernel Container: 불가능 (Container는 커널을 공유)
- VM 사용: Docker의 경량성 이점 상실

**Implementation Notes**:
```yaml
# docker-compose.yml
services:
  rt-test:
    profiles: ["rt-test"]
    cap_add:
      - SYS_NICE
    cpu_rt_runtime: 950000  # 95% CPU time for RT tasks
    command: ./docker/scripts/rt-benchmark.sh
```

**검증 스크립트** (`docker/scripts/rt-benchmark.sh`):
```bash
#!/bin/bash
# PREEMPT_RT 커널 확인
uname -a | grep -q PREEMPT_RT || echo "WARNING: Not running on PREEMPT_RT kernel"

# 실시간 우선순위 설정 테스트
chrt -f 99 ./build/mxrc --benchmark
```

---

### 9. 디버깅 도구 통합

**Decision**: GDB, LLDB, Valgrind를 개발 환경(dev)에 포함

**Rationale**:
- Container 내부에서 디버깅 가능해야 개발 경험 향상
- Core Dump 생성 및 분석 지원
- AddressSanitizer와 Valgrind를 함께 사용하여 메모리 이슈 조기 발견

**Alternatives Considered**:
- Remote Debugging (호스트에서 디버거 실행): Container 네트워크 설정 복잡도 증가
- 디버깅 도구 미포함: 개발 효율성 저하

**Implementation Notes**:
```dockerfile
# 디버깅 도구 설치
RUN apt-get update && apt-get install -y \
    gdb \
    lldb \
    valgrind \
    strace
```

Container 실행 시 `--cap-add=SYS_PTRACE` 필요:
```yaml
services:
  dev:
    cap_add:
      - SYS_PTRACE  # GDB/LLDB에 필요
```

---

### 10. 빌드 캐시 전략

**Decision**: Build 디렉토리를 Volume으로 마운트하여 증분 빌드 지원

**Rationale**:
- CMake 및 Make의 증분 빌드 기능 활용
- 소스 코드 변경 시 변경된 파일만 재컴파일
- Docker Layer Caching과 별개로 빌드 시스템 자체의 캐싱 활용

**Alternatives Considered**:
- 매번 Clean Build: 빌드 시간 과다 (전체 빌드 10분)
- Named Volume for Build: 호스트에서 빌드 결과 접근 불가

**Implementation Notes**:
```yaml
volumes:
  - ./build:/workspace/build:rw  # 빌드 캐시 유지
```

증분 빌드 시간: 30초 이내 (목표 SC-004 충족)

---

## Summary of Decisions

| Topic | Decision | Key Benefit |
|-------|----------|-------------|
| Base Image | Ubuntu 24.04 | 호스트 환경 일치, 호환성 |
| Build Strategy | Single-stage (초기) | 개발 편의성, 디버깅 용이 |
| Compose Profiles | 4개 (dev, ci, rt-test, ethercat-dev) | 용도별 최적화 |
| Volume Strategy | 4개 별도 Volume | 권한 관리, 성능 최적화 |
| Dependency Install | Layer Caching | 재빌드 시간 단축 |
| EtherCAT | 선택적 설치 (별도 Dockerfile) | 이미지 크기 절약 |
| CI Optimization | Pre-built Image (GHCR) | CI 빌드 시간 단축 |
| RT Performance | Host PREEMPT_RT + SYS_NICE | 실시간 성능 유지 |
| Debugging | GDB/LLDB/Valgrind 포함 | 개발 효율성 향상 |
| Build Cache | Volume Mount | 증분 빌드 지원 |

---

## Next Steps

Phase 1로 진행:
1. **data-model.md**: Docker 구성 요소 엔터티 모델 정의
2. **quickstart.md**: Docker 빠른 시작 가이드 작성
3. **contracts/**: N/A (Docker는 API 계약 없음)

---

**Last Updated**: 2025-01-23
**Status**: Completed ✅
