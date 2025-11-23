# Tasks: Docker 컨테이너화 및 개발 환경 표준화

**Input**: Design documents from `/docs/specs/002-docker/`
**Status**: In Progress
**Progress**: 0/42 tasks completed
**Last Updated**: 2025-01-23
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 작업 설명은 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Dockerfile, Container, Volume, Profile 등)
- 파일 경로와 코드는 원래대로 표기합니다

**예시**:
- ✅ 좋은 예: "개발 환경 Dockerfile 작성 in Dockerfile"
- ❌ 나쁜 예: "Create development Dockerfile in Dockerfile"

---

**Tests**: 이 Feature는 인프라 구성이므로, 기존 195개 테스트가 Container에서 통과하는지 검증하는 방식으로 테스트합니다.

**Organization**: Tasks는 User Story별로 그룹화되어 각 Story를 독립적으로 구현 및 테스트할 수 있습니다.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 병렬 실행 가능 (다른 파일, 의존성 없음)
- **[Story]**: User Story 번호 (US1, US2, US3, US4)
- 파일 경로를 명시적으로 포함

## Path Conventions

Docker 관련 파일은 프로젝트 루트에 배치:
- **Dockerfile**: 프로젝트 루트
- **docker-compose.yml**: 프로젝트 루트
- **docker/**: 스크립트 및 설정 디렉토리
- **.github/workflows/**: CI/CD Workflow

---

## Phase 1: Setup (공유 인프라)

**Purpose**: 프로젝트 초기화 및 기본 Docker 구조

- [ ] T001 프로젝트 루트에 docker/ 디렉토리 구조 생성 (docker/scripts/, docker/config/)
- [ ] T002 [P] .dockerignore 파일 작성 (build/, logs/, .git/ 등 제외)
- [ ] T003 [P] docker/dependencies.txt 파일 작성 (APT 패키지 목록)

---

## Phase 2: Foundational (핵심 선행 작업)

**Purpose**: 모든 User Story가 의존하는 핵심 Dockerfile 및 기본 설정

**⚠️ CRITICAL**: 이 Phase가 완료되어야 모든 User Story 작업 시작 가능

- [ ] T004 기본 개발 환경 Dockerfile 작성 (Ubuntu 24.04, 필수 의존성 설치) in Dockerfile
- [ ] T005 [P] docker/scripts/entrypoint.sh 작성 (Container 시작 스크립트)
- [ ] T006 [P] docker/scripts/build.sh 작성 (빌드 헬퍼 스크립트)
- [ ] T007 [P] docker/scripts/test.sh 작성 (테스트 헬퍼 스크립트)
- [ ] T008 기본 docker-compose.yml 작성 (dev profile만 포함) in docker-compose.yml
- [ ] T009 Dockerfile에서 Layer Caching 최적화 (의존성 설치를 소스 코드 복사보다 먼저)

**Checkpoint**: 기본 개발 환경 준비 완료 - User Story 구현 시작 가능

---

## Phase 3: User Story 1 - 로컬 개발 환경 표준화 (Priority: P1) 🎯 MVP

**Goal**: 개발자가 `docker compose up dev` 명령으로 10분 이내에 빌드 환경 구축

**Independent Test**: Git Clone 후 `docker compose up dev` 실행 → Container 내부에서 `./run_tests` 실행 → 195 tests 통과

### Implementation for User Story 1

- [ ] T010 [US1] docker-compose.yml에 dev profile Volume Mount 설정 추가 (./src, ./build, ./logs, ./test-results)
- [ ] T011 [US1] docker-compose.yml에 dev profile Capability 추가 (SYS_PTRACE for debugging)
- [ ] T012 [P] [US1] docker/config/dev.env 파일 작성 (개발 환경 변수: DEBUG=1, ASAN_OPTIONS 등)
- [ ] T013 [US1] Dockerfile에 디버깅 도구 설치 추가 (GDB, LLDB, Valgrind)
- [ ] T014 [US1] docker/scripts/entrypoint.sh에서 Volume 권한 검증 및 에러 메시지 추가
- [ ] T015 [US1] README.md에 Docker 사용법 섹션 추가 (Quick Start, Volume Mount 설명)
- [ ] T016 [US1] Container 내부에서 기존 195 tests 실행 및 검증 (SC-003)
- [ ] T017 [US1] 첫 번째 빌드 시간 측정 및 10분 이내 검증 (SC-001)
- [ ] T018 [US1] 증분 빌드 시간 측정 및 30초 이내 검증 (SC-004)

**Checkpoint**: 개발 환경 완성 - 개발자가 Docker로 즉시 시작 가능

---

## Phase 4: User Story 2 - CI/CD Pipeline 통합 (Priority: P2)

**Goal**: GitHub Actions에서 Docker Image로 자동 빌드/테스트 실행 (5분 이내)

**Independent Test**: GitHub PR 생성 → GitHub Actions 트리거 → 빌드/테스트 자동 실행 → 결과 PR 페이지 표시

### Implementation for User Story 2

- [ ] T019 [P] [US2] CI 최적화 Dockerfile 작성 (최소 의존성, Layer Caching) in Dockerfile.ci
- [ ] T020 [P] [US2] docker-compose.yml에 ci profile 추가 (test-results Volume만 마운트)
- [ ] T021 [P] [US2] docker/config/ci.env 파일 작성 (CI 환경 변수: GTEST_OUTPUT=xml 등)
- [ ] T022 [US2] docker/scripts/test.sh에 CI용 테스트 결과 수집 로직 추가
- [ ] T023 [US2] GitHub Actions Workflow 작성 in .github/workflows/docker-ci.yml
- [ ] T024 [US2] Workflow에서 Pre-built Image Pull 설정 (GHCR에서)
- [ ] T025 [US2] Workflow에서 빌드 캐시 활용 설정 (--cache-from)
- [ ] T026 [US2] Workflow에서 테스트 실패 시 PR 코멘트 추가 로직
- [ ] T027 [US2] Workflow에서 AddressSanitizer 검증 통과 시 뱃지 추가
- [ ] T028 [US2] CI 빌드 시간 측정 및 5분 이내 검증 (SC-002)
- [ ] T029 [US2] Docker Image 크기 측정 및 2GB 이하 검증 (SC-005)

**Checkpoint**: CI/CD 파이프라인 완성 - PR 자동 테스트 가능

---

## Phase 5: User Story 3 - 실시간 환경 테스트 (Priority: P3)

**Goal**: PREEMPT_RT 커널 환경에서 실시간 성능 테스트 (Task 오버헤드 < 1ms)

**Independent Test**: PREEMPT_RT 호스트에서 `docker compose up rt-test` → 벤치마크 실행 → 1ms 이하 검증

### Implementation for User Story 3

- [ ] T030 [P] [US3] 실시간 테스트 Dockerfile 작성 (Release 빌드) in Dockerfile.rt-test
- [ ] T031 [P] [US3] docker-compose.yml에 rt-test profile 추가 (SYS_NICE Capability, CPU 제한)
- [ ] T032 [P] [US3] docker/config/rt-test.env 파일 작성 (RT 환경 변수: RT_PRIORITY=99)
- [ ] T033 [US3] docker/scripts/rt-benchmark.sh 작성 (PREEMPT_RT 커널 확인 및 벤치마크)
- [ ] T034 [US3] rt-benchmark.sh에 PREEMPT_RT 미설치 시 경고 메시지 및 일반 테스트 실행
- [ ] T035 [US3] docker-compose.yml의 rt-test에 cpu_rt_runtime 설정 추가
- [ ] T036 [US3] 실시간 성능 벤치마크 실행 및 1ms 이하 검증 (SC-006)

**Checkpoint**: 실시간 테스트 환경 완성 - RT 성능 검증 가능

---

## Phase 6: User Story 4 - EtherCAT 통합 테스트 환경 (Priority: P4)

**Goal**: EtherCAT Master가 포함된 Container로 EtherCAT 테스트 가능

**Independent Test**: `docker compose up ethercat-dev` → EtherCAT 테스트 실행 → Mock 환경에서 통과

### Implementation for User Story 4

- [ ] T037 [P] [US4] EtherCAT Dockerfile 작성 (IgH EtherCAT Master 빌드) in Dockerfile.ethercat
- [ ] T038 [P] [US4] docker-compose.yml에 ethercat-dev profile 추가 (privileged: true)
- [ ] T039 [US4] Dockerfile.ethercat에서 EtherCAT Master 소스 빌드 (autoconf, configure, make)
- [ ] T040 [US4] docker/scripts/test.sh에 EtherCAT Mock 환경 지원 추가 (ETHERCAT_MOCK=1)
- [ ] T041 [US4] EtherCAT 없는 환경에서 경고 메시지 및 기능 비활성화 검증 (FR-009)

**Checkpoint**: EtherCAT 테스트 환경 완성 - 선택적 기능 테스트 가능

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: 모든 User Story에 영향을 주는 개선 사항

- [ ] T042 [P] docs/onboarding/docker-setup.md 작성 (상세 Docker 온보딩 가이드)
- [ ] T043 [P] README.md Docker 섹션 확장 (4가지 Profile 설명, Troubleshooting)
- [ ] T044 Dockerfile 검증 (hadolint 실행 및 Best Practices 확인)
- [ ] T045 docker-compose.yml 검증 (docker compose config 실행)
- [ ] T046 quickstart.md 시나리오 검증 (각 Profile별 5분 테스트)
- [ ] T047 모든 Profile에서 195 tests 100% 통과 검증

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: 즉시 시작 가능 - 의존성 없음
- **Foundational (Phase 2)**: Setup 완료 후 - **모든 User Story를 블록**
- **User Stories (Phase 3-6)**: Foundational 완료 후 시작 가능
  - User Story 간 의존성 없음 - 병렬 작업 가능
  - 또는 우선순위 순서로 순차 진행 (P1 → P2 → P3 → P4)
- **Polish (Phase 7)**: 원하는 User Story 완료 후

### User Story Dependencies

- **User Story 1 (P1)**: Foundational 후 시작 - 다른 Story 의존성 없음
- **User Story 2 (P2)**: Foundational 후 시작 - US1과 독립적 (동일 Dockerfile 사용)
- **User Story 3 (P3)**: Foundational 후 시작 - US1/US2와 독립적 (별도 Dockerfile)
- **User Story 4 (P4)**: Foundational 후 시작 - US1/US2/US3과 독립적 (별도 Dockerfile)

### Within Each User Story

- 환경 변수 파일 → Dockerfile 수정 → docker-compose.yml 설정
- 스크립트 작성 → Dockerfile에 통합
- Profile 구성 → 검증 및 테스트

### Parallel Opportunities

- **Setup**: T002, T003 병렬 실행 가능
- **Foundational**: T005, T006, T007 병렬 실행 가능
- **User Story 1**: T012 단독 병렬 실행
- **User Story 2**: T019, T020, T021 병렬 실행 가능
- **User Story 3**: T030, T031, T032 병렬 실행 가능
- **User Story 4**: T037, T038 병렬 실행 가능
- **Polish**: T042, T043 병렬 실행 가능
- **User Stories 자체**: US1, US2, US3, US4 모두 병렬 진행 가능 (팀 인력 충분 시)

---

## Parallel Example: User Story 2

```bash
# CI 관련 파일 병렬 작성:
Task: "CI 최적화 Dockerfile 작성 in Dockerfile.ci"
Task: "docker-compose.yml에 ci profile 추가"
Task: "docker/config/ci.env 파일 작성"
```

---

## Implementation Strategy

### MVP First (User Story 1만)

1. Phase 1: Setup 완료
2. Phase 2: Foundational 완료 (CRITICAL - 모든 Story 블록)
3. Phase 3: User Story 1 완료
4. **STOP and VALIDATE**: US1 독립 테스트 (`docker compose up dev` → `./run_tests`)
5. 배포/데모 준비 완료

### Incremental Delivery

1. Setup + Foundational → 기반 준비 완료
2. User Story 1 추가 → 독립 테스트 → 배포/데모 (MVP!)
3. User Story 2 추가 → 독립 테스트 → 배포/데모 (CI/CD 통합)
4. User Story 3 추가 → 독립 테스트 → 배포/데모 (RT 성능 검증)
5. User Story 4 추가 → 독립 테스트 → 배포/데모 (EtherCAT 지원)
6. 각 Story가 이전 Story를 깨지 않고 가치 추가

### Parallel Team Strategy

여러 개발자가 있을 경우:

1. 팀이 Setup + Foundational 함께 완료
2. Foundational 완료 후:
   - Developer A: User Story 1 (로컬 개발 환경)
   - Developer B: User Story 2 (CI/CD)
   - Developer C: User Story 3 (실시간 테스트)
   - Developer D: User Story 4 (EtherCAT)
3. 각 Story가 독립적으로 완료 및 통합

---

## Success Validation

각 User Story 완료 후 다음 검증 필수:

### User Story 1 Validation
- [ ] `docker compose up dev` 명령이 10분 이내에 완료됩니다
- [ ] Container 내부에서 `./run_tests` 실행 시 195 tests 100% 통과합니다
- [ ] 호스트에서 소스 코드 수정 후 Container에서 30초 이내 증분 빌드 가능합니다
- [ ] Volume Mount로 build/ 디렉토리가 호스트에 공유됩니다

### User Story 2 Validation
- [ ] GitHub PR 생성 시 자동으로 빌드/테스트가 실행됩니다
- [ ] CI 빌드가 5분 이내에 완료됩니다
- [ ] 테스트 실패 시 PR에 코멘트가 자동 추가됩니다
- [ ] Docker Image 크기가 2GB 이하입니다

### User Story 3 Validation
- [ ] PREEMPT_RT 호스트에서 `docker compose up rt-test` 실행 가능합니다
- [ ] 실시간 벤치마크에서 99% 이상이 1ms 이하 오버헤드를 측정합니다
- [ ] 비RT 환경에서 경고 메시지 출력 후 일반 테스트 실행됩니다

### User Story 4 Validation
- [ ] `docker compose up ethercat-dev --privileged` 실행 가능합니다
- [ ] EtherCAT 없는 환경에서 경고 메시지 출력됩니다
- [ ] ETHERCAT_MOCK=1로 Mock 환경 테스트 가능합니다

---

## Notes

- **[P] tasks**: 다른 파일, 의존성 없음 - 병렬 실행 가능
- **[Story] label**: 각 Task가 어느 User Story에 속하는지 표시
- 각 User Story는 독립적으로 완료 및 테스트 가능해야 합니다
- 각 Task 또는 논리적 그룹 완료 후 commit 권장
- 각 Checkpoint에서 Story 독립 검증 수행
- 피해야 할 것: 모호한 Task, 동일 파일 충돌, Story 독립성을 깨는 의존성

---

**Last Updated**: 2025-01-23
**Total Tasks**: 47
**MVP Tasks** (US1 only): 18 tasks
**Parallel Opportunities**: 12 tasks marked [P]
