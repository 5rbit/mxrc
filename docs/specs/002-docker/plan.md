# Implementation Plan: Docker 컨테이너화 및 개발 환경 표준화

**Branch**: `002-docker` | **Date**: 2025-01-23 | **Spec**: [spec.md](./spec.md)
**Status**: Planning
**Progress**: Phase 0 (Research) → Phase 1 (Design) → Phase 2 (Tasks)
**Last Updated**: 2025-01-23
**Input**: Feature specification from `docs/specs/002-docker/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Docker, Container, Image, Volume, CMake 등)
- 일반 설명, 구현 계획, 설계 결정은 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "Docker Compose에서 여러 프로파일을 관리합니다"
- ❌ 나쁜 예: "Manage multiple profiles in Docker Compose"

---

## Summary

MXRC 프로젝트에 Docker 컨테이너화를 도입하여 개발 환경 표준화, CI/CD 통합, 실시간 성능 테스트 환경을 구축합니다. Ubuntu 24.04 LTS 기반 Docker Image를 제공하고, Docker Compose를 통해 개발(dev), CI(ci), 실시간 테스트(rt-test), EtherCAT(ethercat-dev) 등 다양한 프로파일을 지원합니다. Volume Mount를 통해 호스트와 Container 간 소스 코드 동기화를 제공하며, 새로운 개발자가 10분 이내에 빌드 환경을 구축할 수 있도록 합니다.

## Technical Context

**Language/Version**: Docker 24.0+, Docker Compose V2, Bash scripting
**Primary Dependencies**:
- Ubuntu 24.04 LTS (Base Image)
- C++20 빌드 환경 (GCC 11+, CMake 3.16+)
- 의존성 패키지 (spdlog, GTest, TBB, nlohmann_json, yaml-cpp)
- 선택적 의존성 (IgH EtherCAT Master 1.5.2+)

**Storage**:
- Docker Volume (빌드 캐시, 로그 파일)
- Volume Mount (소스 코드 동기화)

**Testing**:
- Container 내부에서 기존 테스트 프레임워크 활용 (GoogleTest)
- Dockerfile 검증 (hadolint)
- Docker Compose 검증 (docker-compose config)

**Target Platform**:
- Linux (Ubuntu 24.04 LTS, PREEMPT_RT 커널 지원)
- Docker Engine 또는 Docker Desktop

**Project Type**: Infrastructure/DevOps (기존 C++ 프로젝트에 Docker 통합)

**Performance Goals**:
- 첫 번째 빌드: 10분 이내 (의존성 다운로드 포함)
- 증분 빌드: 30초 이내
- CI 빌드: 5분 이내 (전체 빌드 + 195 테스트)
- Docker Image 크기: 2GB 이하

**Constraints**:
- 195개 기존 테스트 100% 통과 필수
- 실시간 성능 유지 (Task 실행 오버헤드 < 1ms)
- Volume Mount 성능 최적화 (개발 경험 저하 방지)
- 호스트 머신 최소 요구사항: 8GB RAM, 20GB 디스크

**Scale/Scope**:
- 4개 Docker Compose 프로파일 (dev, ci, rt-test, ethercat-dev)
- 2개 빌드 타겟 (Debug with AddressSanitizer, Release)
- 지원 플랫폼: Linux (Ubuntu 24.04 LTS)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### ✅ Principle I: 계층적 아키텍처 원칙
**Status**: PASS (N/A)
**Rationale**: Docker는 인프라 계층이므로 기존 Action/Sequence/Task 계층 구조에 영향을 주지 않습니다. Container 내부에서 동일한 계층 구조가 유지됩니다.

### ✅ Principle II: 인터페이스 기반 설계
**Status**: PASS (N/A)
**Rationale**: Docker는 런타임 환경 제공이며, 기존 인터페이스 설계를 변경하지 않습니다.

### ✅ Principle III: RAII 원칙
**Status**: PASS
**Rationale**: Container 내부에서 기존 RAII 원칙이 그대로 적용됩니다. Docker는 이를 방해하지 않습니다.

### ✅ Principle IV: 메모리 안전성
**Status**: PASS
**Rationale**: AddressSanitizer가 활성화된 Debug 빌드를 별도 Docker Target으로 제공합니다 (FR-010). Container 내부에서 메모리 안전성 검증이 계속 수행됩니다.

### ✅ Principle V: 테스트 주도 개발
**Status**: PASS
**Rationale**: Container 내부에서 기존 195개 테스트가 모두 실행됩니다 (SC-003). Docker는 테스트 실행 환경만 제공합니다.

### ✅ Principle VI: 실시간 성능
**Status**: PASS (조건부)
**Rationale**: PREEMPT_RT 커널 환경을 위한 별도 프로파일(rt-test)을 제공합니다. Container는 호스트 커널을 공유하므로, 호스트에 PREEMPT_RT 커널이 설치되어 있으면 실시간 성능이 유지됩니다.

### ✅ Principle VII: 문서화 및 한글 사용
**Status**: PASS
**Rationale**: 모든 문서(spec.md, plan.md, README 업데이트)가 한글로 작성되며, 기술 용어만 영어로 표기합니다.

### 🟢 Overall Gate Status: PASS
모든 Constitution 원칙을 준수하며, Docker 도입이 기존 아키텍처 및 개발 원칙에 부정적 영향을 주지 않습니다.

## Project Structure

### Documentation (this feature)

```text
docs/specs/002-docker/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output: Docker 모범 사례 조사
├── data-model.md        # Phase 1 output: Docker 구성 엔터티 모델
├── quickstart.md        # Phase 1 output: Docker 빠른 시작 가이드
├── contracts/           # Phase 1 output: (N/A - Docker는 API 계약 없음)
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
# Docker 관련 파일 (프로젝트 루트)
mxrc/
├── Dockerfile                    # 기본 개발 환경 Image
├── Dockerfile.ci                 # CI/CD 최적화 Image
├── Dockerfile.rt-test            # 실시간 테스트 환경 Image
├── Dockerfile.ethercat           # EtherCAT 통합 Image
├── docker-compose.yml            # Docker Compose 설정
├── .dockerignore                 # Docker 빌드 제외 파일
├── docker/                       # Docker 관련 스크립트 및 설정
│   ├── scripts/
│   │   ├── entrypoint.sh        # Container 시작 스크립트
│   │   ├── build.sh             # 빌드 헬퍼 스크립트
│   │   └── test.sh              # 테스트 헬퍼 스크립트
│   └── config/
│       ├── dev.env              # 개발 환경 변수
│       ├── ci.env               # CI 환경 변수
│       └── rt-test.env          # 실시간 테스트 환경 변수
├── .github/
│   └── workflows/
│       └── docker-ci.yml        # GitHub Actions CI/CD Workflow
│
# 기존 프로젝트 구조 (변경 없음)
├── src/core/
│   ├── action/
│   ├── sequence/
│   ├── task/
│   ├── event/
│   └── datastore/
├── tests/
│   ├── unit/
│   └── integration/
├── CMakeLists.txt
├── README.md                     # Docker 사용법 추가
└── docs/
    └── onboarding/
        └── docker-setup.md       # Docker 온보딩 가이드 (신규)
```

**Structure Decision**:
Docker 관련 파일은 프로젝트 루트에 배치하여 표준 Docker 프로젝트 구조를 따릅니다. 여러 Dockerfile을 목적별로 분리하여 이미지 크기 최적화 및 빌드 시간 단축을 지원합니다. docker/ 디렉토리에 스크립트와 설정을 모아 관리의 일관성을 유지합니다. 기존 소스 코드 구조는 변경하지 않으며, Docker는 투명한 개발 환경만 제공합니다.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

해당 없음 - 모든 Constitution 원칙을 준수합니다.
