# Data Model: Docker 구성 요소

**Feature**: 002-docker
**Date**: 2025-01-23
**Status**: Completed
**Phase**: 1 (Design)

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Docker Image, Container, Volume, Profile 등)
- 엔터티 설명, 관계, 속성은 모두 한글로 작성합니다

---

## Overview

Docker 컨테이너화에 필요한 구성 요소와 그 관계를 정의합니다. 이 문서는 물리적인 데이터베이스 모델이 아닌, Docker 인프라의 논리적 구성 요소를 설명합니다.

---

## Entity Definitions

### 1. Docker Image

**설명**: MXRC 실행에 필요한 모든 의존성과 도구가 포함된 불변(immutable) 컨테이너 이미지

**속성**:
- `name`: Image 이름 (예: `mxrc-dev`, `mxrc-ci`)
- `tag`: 버전 태그 (예: `latest`, `v1.0.0`)
- `base_image`: 기반 이미지 (예: `ubuntu:24.04`)
- `size`: 이미지 크기 (목표: < 2GB)
- `build_time`: 빌드 소요 시간
- `dockerfile`: 빌드에 사용되는 Dockerfile 경로

**관계**:
- **builds** → Container (1:N) - 하나의 Image로 여러 Container 생성 가능
- **stored in** → Registry (N:1) - 여러 Image가 하나의 Registry에 저장

**Validation Rules**:
- 이미지 크기는 2GB 이하여야 함 (SC-005)
- Ubuntu 24.04 LTS를 Base Image로 사용해야 함 (FR-001)
- 모든 필수 의존성이 포함되어야 함 (FR-002)

**상태**:
- `building`: 빌드 중
- `built`: 빌드 완료
- `pushed`: Registry에 푸시 완료
- `failed`: 빌드 실패

---

### 2. Container

**설명**: Docker Image로부터 생성된 실행 가능한 인스턴스, 격리된 런타임 환경 제공

**속성**:
- `name`: Container 이름 (예: `mxrc-dev-1`)
- `image`: 기반 Image 참조
- `status`: 실행 상태 (running, stopped, exited)
- `profile`: 사용 중인 Compose Profile (dev, ci, rt-test, ethercat-dev)
- `created_at`: 생성 시각
- `cpu_limit`: CPU 제한 (선택적)
- `memory_limit`: 메모리 제한 (선택적)
- `capabilities`: 추가 권한 (예: SYS_NICE, SYS_PTRACE)

**관계**:
- **created from** → Image (N:1) - 여러 Container가 하나의 Image에서 생성
- **mounts** → Volume (N:M) - 하나의 Container가 여러 Volume을 마운트
- **uses** → Profile (N:1) - 하나의 Container는 하나의 Profile 사용

**Validation Rules**:
- Container 내부에서 195개 테스트가 모두 통과해야 함 (SC-003)
- 실시간 테스트 Container는 SYS_NICE Capability 필요 (rt-test profile)
- EtherCAT Container는 privileged 모드 필요 (ethercat-dev profile)

**상태 전이**:
```
[created] → [running] → [stopped] → [removed]
    ↓           ↓
  [failed]   [paused]
```

---

### 3. Volume

**설명**: 호스트와 Container 간 데이터를 공유하거나 Container 간 데이터를 공유하는 저장소

**속성**:
- `name`: Volume 이름 (예: `mxrc-build-cache`)
- `type`: Volume 유형 (bind, named, tmpfs)
- `source`: 호스트 경로 (bind mount의 경우)
- `target`: Container 내부 경로
- `mode`: 읽기/쓰기 모드 (ro, rw)
- `size`: 사용 중인 디스크 크기

**관계**:
- **mounted by** → Container (M:N) - 여러 Volume이 여러 Container에 마운트

**Volume Types**:

1. **소스 코드 Volume**
   - Source: `./src`
   - Target: `/workspace/src`
   - Mode: `rw`
   - 용도: 호스트 IDE에서 수정한 코드를 Container에서 즉시 빌드

2. **빌드 출력 Volume**
   - Source: `./build`
   - Target: `/workspace/build`
   - Mode: `rw`
   - 용도: 빌드 결과를 호스트에서 실행/디버깅, 증분 빌드 캐시

3. **로그 Volume**
   - Source: `./logs`
   - Target: `/workspace/logs`
   - Mode: `rw`
   - 용도: Container 재시작 후에도 로그 보존 (FR-012)

4. **테스트 결과 Volume**
   - Source: `./test-results`
   - Target: `/workspace/test-results`
   - Mode: `rw`
   - 용도: CI/CD에서 테스트 아티팩트 수집

**Validation Rules**:
- 로그 파일은 Volume을 통해 호스트에 영구 저장되어야 함 (FR-012)
- Volume Mount로 인한 권한 문제 발생 시 명확한 에러 메시지 제공 (Edge Case)

---

### 4. Compose Profile

**설명**: 사용 목적에 따라 다른 Container 설정을 제공하는 Docker Compose 프로파일

**속성**:
- `name`: 프로파일 이름 (dev, ci, rt-test, ethercat-dev)
- `description`: 프로파일 설명
- `dockerfile`: 사용하는 Dockerfile 경로
- `volumes`: 마운트할 Volume 목록
- `environment`: 환경 변수
- `capabilities`: 추가 권한 목록
- `command`: 기본 실행 명령

**관계**:
- **defines** → Container Config (1:1) - 하나의 Profile은 하나의 Container 설정 정의

**Profile Types**:

1. **dev (개발 환경)**
   - Dockerfile: `Dockerfile`
   - Volumes: 모든 Volume (src, build, logs, test-results)
   - Capabilities: `SYS_PTRACE` (디버깅)
   - Command: `/bin/bash` (interactive shell)
   - 용도: 로컬 개발, 디버깅, 테스트

2. **ci (CI/CD 환경)**
   - Dockerfile: `Dockerfile.ci`
   - Volumes: 최소 (test-results만)
   - Capabilities: 없음
   - Command: `./docker/scripts/test.sh`
   - 용도: GitHub Actions 자동 빌드/테스트

3. **rt-test (실시간 테스트)**
   - Dockerfile: `Dockerfile.rt-test`
   - Volumes: build, logs, test-results
   - Capabilities: `SYS_NICE` (실시간 우선순위)
   - Command: `./docker/scripts/rt-benchmark.sh`
   - 용도: PREEMPT_RT 커널에서 성능 벤치마크

4. **ethercat-dev (EtherCAT 개발)**
   - Dockerfile: `Dockerfile.ethercat`
   - Volumes: src, build, logs
   - Privileged: `true` (하드웨어 접근)
   - Command: `/bin/bash`
   - 용도: EtherCAT 하드웨어 통합 테스트

**Validation Rules**:
- 개발 프로파일은 `docker compose up dev` 명령으로 시작 가능해야 함 (FR-003)
- 각 프로파일은 독립적으로 실행 가능해야 함

---

### 5. Dependency Package

**설명**: Container 내부에 설치되는 시스템 패키지 및 라이브러리

**속성**:
- `name`: 패키지 이름 (예: `libspdlog-dev`)
- `version`: 버전 (예: `1.12.0`)
- `manager`: 패키지 관리자 (apt, git)
- `required_by`: 필수/선택적 구분
- `install_time`: 설치 소요 시간

**관계**:
- **installed in** → Image (N:M) - 여러 패키지가 여러 Image에 설치

**Package Categories**:

1. **필수 빌드 도구**
   - `build-essential`: GCC, G++, Make
   - `cmake`: 빌드 시스템
   - `git`: 버전 관리

2. **필수 라이브러리**
   - `libspdlog-dev`: 비동기 로깅
   - `libgtest-dev`: 테스트 프레임워크
   - `libtbb-dev`: 병렬 처리
   - `nlohmann-json3-dev`: JSON 처리
   - `libyaml-cpp-dev`: YAML 설정 (EtherCAT용)

3. **디버깅 도구** (dev 프로파일)
   - `gdb`: GNU 디버거
   - `lldb`: LLVM 디버거
   - `valgrind`: 메모리 검사

4. **선택적 의존성** (ethercat-dev 프로파일)
   - IgH EtherCAT Master (소스 빌드)

**Validation Rules**:
- 모든 필수 의존성이 Image에 포함되어야 함 (FR-002)
- EtherCAT Master 없이도 빌드 가능해야 함 (FR-009)

---

### 6. Build Artifact

**설명**: Docker Container 내부에서 빌드된 결과물

**속성**:
- `name`: 아티팩트 이름 (예: `mxrc`, `libmxrc.so`)
- `path`: Container 내부 경로
- `type`: 타입 (executable, library, test)
- `build_config`: 빌드 설정 (Debug, Release)
- `size`: 파일 크기
- `build_time`: 빌드 소요 시간

**관계**:
- **produced by** → Container (N:1) - 여러 Artifact가 하나의 Container에서 생성
- **stored in** → Volume (N:1) - Artifact는 build Volume에 저장

**Build Configurations**:

1. **Debug (AddressSanitizer 포함)**
   - CMake Flags: `-DCMAKE_BUILD_TYPE=Debug`
   - Sanitizer: `-fsanitize=address`
   - 용도: 개발, 테스트, 메모리 검증

2. **Release (최적화)**
   - CMake Flags: `-DCMAKE_BUILD_TYPE=Release`
   - 최적화: `-O3`
   - 용도: 성능 벤치마크, 배포

**Validation Rules**:
- 빌드 결과물은 호스트의 `build/` 디렉토리에 저장되어야 함 (FR-005)
- 증분 빌드는 30초 이내에 완료되어야 함 (SC-004)

---

### 7. Environment Configuration

**설명**: Container 실행 시 주입되는 환경 변수 및 설정

**속성**:
- `profile`: 적용 대상 Profile
- `key`: 환경 변수 이름
- `value`: 환경 변수 값
- `secret`: 민감 정보 여부
- `source`: 값의 출처 (file, inline, secret)

**관계**:
- **applied to** → Container (N:M) - 여러 환경 변수가 여러 Container에 적용

**Configuration Sets**:

1. **개발 환경 (dev.env)**
   ```bash
   CMAKE_BUILD_TYPE=Debug
   SPDLOG_LEVEL=debug
   ASAN_OPTIONS=detect_leaks=1:halt_on_error=0
   ```

2. **CI 환경 (ci.env)**
   ```bash
   CMAKE_BUILD_TYPE=Debug
   SPDLOG_LEVEL=info
   GTEST_OUTPUT=xml:test-results/
   ```

3. **실시간 테스트 (rt-test.env)**
   ```bash
   CMAKE_BUILD_TYPE=Release
   RT_PRIORITY=99
   BENCHMARK_ITERATIONS=1000
   ```

**Validation Rules**:
- 민감 정보는 환경 변수로만 전달 (Dockerfile에 하드코딩 금지)
- 환경 변수 검증 실패 시 명확한 에러 메시지 제공

---

## Entity Relationships

```
┌─────────────┐      builds      ┌─────────────┐
│   Image     │─────────────────>│  Container  │
└─────────────┘         1:N      └─────────────┘
      │                                  │
      │ stored in                   mounts│ N:M
      │ N:1                               │
      ↓                                   ↓
┌─────────────┐                  ┌─────────────┐
│  Registry   │                  │   Volume    │
└─────────────┘                  └─────────────┘

┌─────────────┐      uses        ┌─────────────┐
│  Container  │─────────────────>│   Profile   │
└─────────────┘         N:1      └─────────────┘

┌─────────────┐   installed in   ┌─────────────┐
│  Dependency │<─────────────────│    Image    │
└─────────────┘         N:M      └─────────────┘

┌─────────────┐   produced by    ┌─────────────┐
│   Artifact  │<─────────────────│  Container  │
└─────────────┘         N:1      └─────────────┘

┌─────────────┐   applied to     ┌─────────────┐
│Environment  │─────────────────>│  Container  │
└─────────────┘         N:M      └─────────────┘
```

---

## Lifecycle Flows

### 1. 개발 환경 시작 Flow

```
User: docker compose up dev
    ↓
[Profile: dev] 선택
    ↓
[Image: mxrc-dev] 확인/빌드
    ↓
[Dependency] 설치 (캐시 활용)
    ↓
[Container] 생성 및 시작
    ↓
[Volume] 마운트 (src, build, logs, test-results)
    ↓
[Environment] 주입 (dev.env)
    ↓
[Container] Ready (bash shell 실행)
```

### 2. CI/CD 빌드 Flow

```
GitHub Actions Trigger
    ↓
[Profile: ci] 선택
    ↓
[Image: mxrc-ci] Pull (Pre-built from GHCR)
    ↓
[Container] 생성 및 시작
    ↓
[Volume] 마운트 (test-results만)
    ↓
[Environment] 주입 (ci.env)
    ↓
[Artifact] 빌드 (./docker/scripts/test.sh)
    ↓
[Tests] 실행 (195 tests)
    ↓
[Results] 수집 (test-results/ → GitHub Artifacts)
    ↓
[Container] 종료 및 삭제
```

### 3. 증분 빌드 Flow

```
User: 소스 코드 수정 (호스트 IDE)
    ↓
[Volume: src] 변경 감지
    ↓
[Container] 내부에서 make 실행
    ↓
[CMake] 변경된 파일만 감지
    ↓
[Artifact] 증분 컴파일 (30초 이내)
    ↓
[Volume: build] 업데이트
    ↓
[Host] 빌드 결과 즉시 사용 가능
```

---

## Data Constraints

### Size Constraints
- Docker Image: < 2GB (SC-005)
- Container Memory: 최소 8GB RAM 호스트 권장
- Volume Disk: 최소 20GB 여유 공간 권장

### Performance Constraints
- 첫 번째 빌드: < 10분 (SC-001)
- 증분 빌드: < 30초 (SC-004)
- CI 빌드: < 5분 (SC-002)
- 테스트 실행: 195 tests 100% 통과 (SC-003)

### Security Constraints
- Container는 기본적으로 unprivileged 모드 실행
- EtherCAT Profile만 privileged 모드 허용
- 디버깅 Profile만 SYS_PTRACE Capability 허용
- 실시간 Profile만 SYS_NICE Capability 허용

---

## Next Steps

Phase 1 계속:
- **quickstart.md**: Docker 빠른 시작 가이드 작성

---

**Last Updated**: 2025-01-23
**Status**: Completed ✅
