# Feature Specification: Docker 컨테이너화 및 개발 환경 표준화

**Feature Branch**: `002-docker`
**Created**: 2025-01-23
**Status**: In Progress
**Progress**: 3/5 (Spec → Plan → Tasks → Implementation → Completed)
**Last Updated**: 2025-01-23
**Input**: User description: "docker의 도입"

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Docker, Container, Image, Volume 등)
- 일반 설명, 요구사항, 시나리오는 모두 한글로 작성합니다

**예시**:
- ✅ 좋은 예: "개발자는 Docker Container를 사용하여 환경을 구성할 수 있어야 합니다"
- ❌ 나쁜 예: "Developer can set up environment using Docker containers"

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 - 로컬 개발 환경 표준화 (Priority: P1)

개발자가 MXRC 프로젝트를 처음 시작할 때, 복잡한 의존성 설치 없이 Docker Container만으로 즉시 개발 환경을 구성하고 빌드/테스트를 실행할 수 있어야 합니다.

**Why this priority**: 새로운 개발자의 온보딩 시간을 단축하고, 환경 차이로 인한 빌드 실패를 방지하는 것이 가장 중요한 가치입니다. 모든 개발자가 동일한 환경에서 작업할 수 있어야 협업이 원활해집니다.

**Independent Test**: 새로운 개발자가 Git Clone 후 `docker compose up dev` 명령 하나로 빌드 및 테스트를 실행하고, 195개의 모든 테스트가 통과하는지 확인합니다.

**Acceptance Scenarios**:

1. **Given** 개발자가 Ubuntu 24.04 LTS 호스트 머신에 Docker만 설치된 상태에서, **When** Git Repository를 클론하고 `docker compose up dev` 명령을 실행하면, **Then** C++20, CMake, spdlog, GTest, TBB, nlohmann_json 등 모든 의존성이 자동 설치되고 MXRC가 빌드됩니다

2. **Given** Docker Container가 실행 중일 때, **When** `docker compose exec dev ./run_tests` 명령을 실행하면, **Then** 195개의 모든 테스트가 통과하고 결과가 표시됩니다

3. **Given** 개발자가 소스 코드를 수정했을 때, **When** Container 내부에서 `make -j$(nproc)` 명령을 실행하면, **Then** 변경사항이 즉시 반영되어 재빌드됩니다

4. **Given** 개발자가 IDE에서 코드를 수정할 때, **When** Volume Mount를 통해 호스트와 Container가 연결되어 있으면, **Then** 호스트의 변경사항이 Container에서 즉시 반영됩니다

---

### User Story 2 - CI/CD Pipeline 통합 (Priority: P2)

GitHub Actions 또는 GitLab CI에서 Docker Image를 사용하여 자동으로 빌드/테스트를 실행하고, 테스트 실패 시 즉시 피드백을 받을 수 있어야 합니다.

**Why this priority**: 코드 품질을 자동으로 검증하고, PR 단계에서 문제를 조기 발견하는 것이 두 번째로 중요합니다. 표준화된 Docker Image를 사용하면 CI 환경과 로컬 환경의 차이를 제거할 수 있습니다.

**Independent Test**: GitHub에 PR을 생성했을 때, GitHub Actions가 Docker Container에서 빌드 및 테스트를 자동 실행하고, 결과가 PR 페이지에 표시되는지 확인합니다.

**Acceptance Scenarios**:

1. **Given** 개발자가 GitHub에 PR을 생성했을 때, **When** GitHub Actions Workflow가 트리거되면, **Then** Docker Image를 사용하여 자동으로 빌드 및 테스트가 실행됩니다

2. **Given** CI Pipeline에서 테스트가 실행 중일 때, **When** 테스트가 실패하면, **Then** 실패한 테스트 목록과 로그가 PR 코멘트로 자동 추가됩니다

3. **Given** CI Pipeline에서 빌드가 성공했을 때, **When** AddressSanitizer 검증도 통과하면, **Then** 메모리 안전성이 검증되었다는 뱃지가 표시됩니다

---

### User Story 3 - 실시간 환경 테스트 (Priority: P3)

PREEMPT_RT 커널이 적용된 Docker Container에서 실시간 성능을 테스트하고, Task 실행 오버헤드가 1ms 이하임을 검증할 수 있어야 합니다.

**Why this priority**: 실제 배포 환경과 유사한 조건에서 성능을 검증하는 것이 세 번째 우선순위입니다. 하지만 실시간 커널은 개발 단계에서는 필수가 아니므로 선택적 기능으로 제공합니다.

**Independent Test**: PREEMPT_RT 커널이 적용된 호스트에서 `docker compose up rt-test` 명령을 실행하고, 성능 벤치마크 결과가 목표치를 충족하는지 확인합니다.

**Acceptance Scenarios**:

1. **Given** PREEMPT_RT 커널이 설치된 Ubuntu 24.04 LTS 호스트에서, **When** `docker compose up rt-test` 명령을 실행하면, **Then** 실시간 우선순위가 적용된 Container에서 성능 테스트가 실행됩니다

2. **Given** 실시간 성능 테스트가 실행 중일 때, **When** Task 실행 오버헤드를 측정하면, **Then** 99% 이상의 경우에서 1ms 이하의 지연시간이 측정됩니다

3. **Given** 비실시간 환경에서, **When** 실시간 테스트를 실행하면, **Then** 경고 메시지와 함께 테스트가 스킵되고 일반 테스트만 실행됩니다

---

### User Story 4 - EtherCAT 통합 테스트 환경 (Priority: P4)

EtherCAT Master가 설치된 Docker Container에서 Feature 001의 EtherCAT 센서/모터 데이터 수신 기능을 테스트할 수 있어야 합니다.

**Why this priority**: EtherCAT은 선택적 기능이므로, Docker에서도 선택적으로 활성화할 수 있어야 합니다. 하드웨어 의존성이 있으므로 Mock 기반 테스트도 제공합니다.

**Independent Test**: `docker compose up ethercat-dev` 명령으로 EtherCAT Master가 포함된 Container를 시작하고, EtherCAT 관련 테스트가 통과하는지 확인합니다.

**Acceptance Scenarios**:

1. **Given** EtherCAT 하드웨어가 연결되지 않은 환경에서, **When** `docker compose up dev` 명령을 실행하면, **Then** EtherCAT 없이도 정상적으로 빌드되고 나머지 테스트가 실행됩니다

2. **Given** EtherCAT 하드웨어가 연결된 환경에서, **When** `docker compose up ethercat-dev --privileged` 명령을 실행하면, **Then** IgH EtherCAT Master가 설치되고 하드웨어와 통신할 수 있습니다

3. **Given** EtherCAT Mock 환경에서, **When** Mock 데이터를 사용하여 테스트를 실행하면, **Then** 하드웨어 없이도 EtherCAT 데이터 수신 로직을 검증할 수 있습니다

---

### Edge Cases

- Docker가 설치되지 않은 환경에서 사용자가 `docker compose up` 명령을 실행하면 어떤 안내 메시지가 표시되는가?
- Container 내부에서 메모리가 부족한 경우 빌드 실패 시 명확한 에러 메시지가 표시되는가?
- Volume Mount 권한 문제로 파일 생성/수정이 실패하면 어떻게 처리되는가?
- macOS 또는 Windows 호스트에서 실행할 때 파일 시스템 성능 저하가 개발 경험에 영향을 미치는가?
- Container가 실행 중일 때 호스트에서 의존성 패키지가 업데이트되면 Container를 재빌드해야 하는가?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: 시스템은 Ubuntu 24.04 LTS 기반 Docker Image를 제공해야 합니다
- **FR-002**: 시스템은 C++20, GCC 11+, CMake 3.16+, spdlog, GTest, TBB, nlohmann_json 의존성이 사전 설치된 Image를 제공해야 합니다
- **FR-003**: 개발자는 `docker compose up dev` 명령 하나로 개발 환경을 시작할 수 있어야 합니다
- **FR-004**: 개발자는 Volume Mount를 통해 호스트의 소스 코드를 Container와 동기화할 수 있어야 합니다
- **FR-005**: 시스템은 Container 내부에서 빌드된 바이너리가 호스트의 `build/` 디렉토리에 저장되어야 합니다
- **FR-006**: 개발자는 Container 내부에서 `./run_tests` 명령으로 모든 테스트를 실행할 수 있어야 합니다
- **FR-007**: 시스템은 Docker Compose에서 개발(dev), CI(ci), 실시간 테스트(rt-test), EtherCAT(ethercat-dev) 등 다양한 프로파일을 제공해야 합니다
- **FR-008**: CI/CD Pipeline은 미리 빌드된 Docker Image를 사용하여 빌드 시간을 단축해야 합니다
- **FR-009**: 시스템은 EtherCAT Master 설치 여부를 자동 감지하고, 없을 경우 경고와 함께 해당 기능을 비활성화해야 합니다
- **FR-010**: 시스템은 AddressSanitizer가 활성화된 Debug 빌드와 최적화된 Release 빌드를 각각 별도의 Docker Target으로 제공해야 합니다
- **FR-011**: 개발자는 Container 내부에서 GDB 또는 LLDB를 사용하여 디버깅할 수 있어야 합니다
- **FR-012**: 시스템은 로그 파일(`logs/mxrc.log`)이 Volume을 통해 호스트에 영구 저장되어야 합니다

### Key Entities *(include if feature involves data)*

- **Docker Image**: MXRC 실행에 필요한 모든 의존성이 포함된 컨테이너 이미지
  - Base Image: `ubuntu:24.04`
  - 빌드 도구: GCC 11+, CMake, Make
  - 의존성: spdlog, GTest, TBB, nlohmann_json, yaml-cpp
  - 선택적 의존성: IgH EtherCAT Master

- **Docker Compose Profile**: 사용 목적에 따라 다른 설정을 제공하는 프로파일
  - `dev`: 개발 환경 (Volume Mount, 디버깅 도구 포함)
  - `ci`: CI/CD 환경 (최소 설정, 빠른 빌드)
  - `rt-test`: 실시간 성능 테스트 (PREEMPT_RT 커널 필요)
  - `ethercat-dev`: EtherCAT 개발/테스트 (Privileged 모드, 하드웨어 접근)

- **Volume**: 호스트와 Container 간 데이터 공유
  - 소스 코드: `./src:/workspace/src` (읽기/쓰기)
  - 빌드 출력: `./build:/workspace/build` (읽기/쓰기)
  - 로그 파일: `./logs:/workspace/logs` (읽기/쓰기)
  - 테스트 결과: `./test-results:/workspace/test-results` (읽기/쓰기)

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 새로운 개발자가 프로젝트를 클론한 후 10분 이내에 첫 번째 빌드를 완료할 수 있습니다
- **SC-002**: Docker 기반 CI Pipeline에서 전체 빌드 및 테스트 실행 시간이 5분 이내입니다
- **SC-003**: 195개의 모든 기존 테스트가 Docker Container에서 100% 통과합니다
- **SC-004**: 개발자가 소스 코드를 수정한 후 증분 빌드가 30초 이내에 완료됩니다
- **SC-005**: Docker Image 크기가 2GB 이하로 유지됩니다 (의존성 모두 포함)
- **SC-006**: 실시간 테스트 환경에서 Task 실행 오버헤드가 1ms 이하로 측정됩니다
- **SC-007**: 새로운 개발자의 환경 설정 관련 질문이 80% 이상 감소합니다

## Assumptions

- Docker Desktop 또는 Docker Engine이 호스트에 이미 설치되어 있습니다
- 개발자는 기본적인 Docker 명령어(`docker`, `docker compose`)를 사용할 수 있습니다
- 호스트 머신에 최소 8GB RAM과 20GB 디스크 공간이 있습니다
- EtherCAT 하드웨어 테스트는 선택적이며, 대부분의 개발은 Mock 환경에서 수행됩니다
- PREEMPT_RT 커널은 실제 배포 환경에서만 필수이며, 개발 환경에서는 선택적입니다
- CI/CD Pipeline은 GitHub Actions를 기본으로 하지만, GitLab CI 등 다른 플랫폼도 지원 가능합니다

## Dependencies

- Docker Engine 24.0+ 또는 Docker Desktop
- Docker Compose V2 (docker-compose-plugin)
- Ubuntu 24.04 LTS 기반 Base Image
- 기존 MXRC 빌드 시스템 (CMake, GCC)

## Scope

### In Scope

- Ubuntu 24.04 LTS 기반 Docker Image 생성
- 개발 환경용 Docker Compose 설정
- CI/CD용 최적화된 Docker Image
- Volume Mount를 통한 소스 코드 동기화
- 테스트 실행 환경 (모든 의존성 포함)
- EtherCAT 지원 (선택적)
- 실시간 성능 테스트 프로파일 (선택적)
- Docker 관련 문서 작성 (README 업데이트, 온보딩 가이드)

### Out of Scope

- Production 배포용 Kubernetes 설정 (향후 별도 Feature로 진행)
- Multi-stage Build 최적화 (초기 버전에서는 단일 Stage로 시작)
- Docker Registry 운영 및 Image 배포 전략
- Windows 또는 macOS 네이티브 빌드 지원
- Docker Swarm 또는 클러스터 설정
- Container Orchestration 자동화
