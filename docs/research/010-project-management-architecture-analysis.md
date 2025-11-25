# Research 010: 프로젝트 관리 및 아키텍처 분석

**작성일**: 2025-11-25
**작성자**: Claude (AI Assistant)
**카테고리**: Architecture, Project Management
**관련 Feature**: 016, 019, 022

---

## 1. 개요

본 문서는 MXRC 프로젝트의 현재 구조를 분석하고, Core 모듈 기반으로 Robot을 구현하는 과정에서 발생하는 프로젝트 관리 문제점을 식별하여 개선 방안을 제시합니다.

## 2. 현재 프로젝트 구조 분석

### 2.1 전체 아키텍처 구성

```
mxrc/
├── src/
│   ├── core/              # 핵심 프레임워크 (19개 모듈)
│   │   ├── action/        # 액션 관리
│   │   ├── sequence/      # 시퀀스 조합
│   │   ├── task/          # 태스크 실행
│   │   ├── event/         # 이벤트 버스
│   │   ├── datastore/     # 공유 메모리 스토어
│   │   ├── rt/            # 실시간 executive
│   │   ├── nonrt/         # 비실시간 executive
│   │   ├── alarm/         # 알람 시스템
│   │   ├── control/       # 행동 중재 시스템
│   │   ├── monitoring/    # 메트릭 수집
│   │   ├── tracing/       # 분산 추적
│   │   ├── systemd/       # 시스템 통합
│   │   ├── ethercat/      # 필드버스
│   │   └── ...
│   │
│   ├── robot/             # 로봇별 구현 (1개 로봇)
│   │   └── pallet_shuttle/
│   │       ├── actions/   # 로봇 특화 액션
│   │       ├── sequences/ # 로봇 특화 시퀀스
│   │       ├── tasks/     # 로봇 특화 태스크
│   │       ├── control/   # 제어 로직
│   │       ├── state/     # 상태 관리
│   │       └── config/    # 설정
│   │
│   ├── main.cpp           # 통합 실행 파일 (legacy)
│   ├── rt_main.cpp        # RT 프로세스
│   └── nonrt_main.cpp     # Non-RT 프로세스
│
├── systemd/               # 서비스 관리 (5개 서비스)
│   ├── mxrc-rt.service
│   ├── mxrc-nonrt.service
│   ├── mxrc-webapi.service
│   ├── mxrc-metrics.service
│   ├── mxrc-monitor.service
│   └── mxrc.target
│
├── webapi/                # Web API 서버 (Node.js/TypeScript)
│   ├── src/
│   │   ├── server.ts
│   │   ├── routes/
│   │   ├── ipc/           # Core와의 IPC 통신
│   │   ├── websocket/
│   │   └── middleware/
│   ├── package.json
│   └── tsconfig.json
│
├── config/                # 설정 파일
│   ├── rt_schedule.json
│   └── ipc/
│
├── docs/
│   ├── specs/             # 39개 Feature 스펙
│   ├── research/          # 9개 리서치 문서
│   └── issue/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   └── benchmark/
│
└── CMakeLists.txt         # 통합 빌드 (단일 파일)
```

### 2.2 빌드 시스템

**현재 상태**: 모놀리식 CMakeLists.txt (1100+ 라인)

- **RT 실행 파일** (`rt`): 실시간 제어 프로세스
- **Non-RT 실행 파일** (`nonrt`): 비실시간 관리 프로세스
- **통합 실행 파일** (`mxrc`): 레거시 단일 프로세스
- **테스트 실행 파일** (`run_tests`): 전체 테스트 스위트
- **도구**: `schedule_generator`, `validate_schedule`

**Include 경로**: 50+ 개의 개별 디렉토리를 모두 나열

### 2.3 프로세스 실행 구조

```
systemd (init system)
├── mxrc-rt.service         # RT 프로세스 (SCHED_FIFO, CPU 2-3)
├── mxrc-nonrt.service      # Non-RT 프로세스 (CPU 0-1)
├── mxrc-webapi.service     # Node.js Web API 서버
├── mxrc-metrics.service    # Prometheus 메트릭 수집
└── mxrc-monitor.service    # HA 모니터링
```

**의존성 체인**:
- `mxrc-rt.service` → Before → `mxrc-nonrt.service`
- `mxrc-webapi.service` → After → `mxrc-rt`, `mxrc-nonrt`

## 3. 문제점 식별

### 3.1 빌드 시스템 관리 어려움

#### 문제 1: 모놀리식 CMakeLists.txt

**증상**:
- 1100+ 라인의 단일 파일에서 모든 타겟 관리
- Core/Robot 구분 없이 모든 소스 파일을 한 곳에 나열
- Include 경로가 50+ 개로 중복과 순환 의존성 가능성

**영향**:
```cmake
# 현재: 모든 것이 한 파일에
add_executable(mxrc
    src/main.cpp
    src/core/action/core/ActionExecutor.cpp
    src/core/sequence/core/SequenceEngine.cpp
    # ... 200+ 파일 나열 ...
    src/robot/pallet_shuttle/actions/MoveToPositionAction.cpp
    # ...
)

# Include 경로도 모두 나열
target_include_directories(mxrc PUBLIC
    ${PROJECT_SOURCE_DIR}/src/core/action
    ${PROJECT_SOURCE_DIR}/src/core/action/interfaces
    # ... 50+ 경로 ...
)
```

**결과**:
- 새로운 로봇 추가 시 CMakeLists.txt를 직접 수정해야 함
- 빌드 의존성 파악 어려움
- 증분 빌드 최적화 불가능

#### 문제 2: 모듈 간 경계 불명확

**증상**:
- Core의 어떤 부분을 Robot이 사용할 수 있는지 명시적이지 않음
- `#include` 경로가 모두 노출되어 있어 잘못된 의존성 생성 가능

**예시**:
```cpp
// Robot 코드에서 Core 내부 구현을 직접 접근 가능
#include "core/datastore/managers/ExpirationManager.h"  // ❌ 내부 구현
#include "core/control/interfaces/IBehaviorArbiter.h"   // ✅ 공개 인터페이스
```

### 3.2 systemd 서비스 관리 복잡성

#### 문제 3: 서비스 간 의존성 불명확

**증상**:
```systemd
# mxrc-rt.service
After=network.target
Before=mxrc-nonrt.service
Wants=mxrc-nonrt.service

# mxrc-webapi.service
After=network.target mxrc-rt.service mxrc-nonrt.service
Requires=mxrc-rt.service
```

**문제**:
- 서비스 간 의존성이 분산되어 있어 전체 시작 순서 파악 어려움
- 어떤 서비스가 필수이고 선택인지 불명확
- 실패 시 복구 전략이 서비스마다 다름

#### 문제 4: 리소스 제어 정책 분산

각 서비스 파일에 리소스 설정이 개별적으로 정의:

```systemd
# mxrc-rt.service
CPUQuota=200%
MemoryMax=2G
CPUAffinity=2 3

# mxrc-webapi.service
CPUQuota=150%
MemoryMax=512M
CPUAffinity=0 1
```

**문제**:
- 전체 시스템 리소스 예산 파악 어려움
- CPU/메모리 경합 상황 예측 불가
- 리소스 정책 변경 시 여러 파일 수정 필요

### 3.3 WebAPI 프로젝트 독립성 문제

#### 문제 5: 멀티 언어 프로젝트 관리

**현황**:
- Core: C++ (CMake 빌드)
- WebAPI: TypeScript/Node.js (npm 빌드)

**증상**:
- 빌드 프로세스가 완전히 분리됨
- WebAPI의 IPC 스키마가 Core와 동기화되지 않을 위험
- 통합 테스트 실행 어려움

**예시**:
```yaml
# config/ipc/ipc-schema.yaml (Core가 생성)
datastore:
  keys:
    - robot.position.x

# webapi/src/ipc/types.ts (수동 동기화 필요)
interface DataStoreKeys {
  'robot.position.x': number;  // ⚠️ 수동으로 맞춰야 함
}
```

#### 문제 6: WebAPI의 Core 의존성

```javascript
// webapi/src/ipc/client.ts
const ipcSocket = '/var/run/mxrc/ipc.sock';  // Core가 생성하는 소켓
const schemaPath = '/opt/mxrc/config/ipc/ipc-schema.yaml';  // Core 스키마
```

**문제**:
- WebAPI가 실행되려면 Core가 먼저 실행되어야 함
- 하지만 이 의존성이 프로젝트 레벨에서는 표현되지 않음
- 개발 시 Mock 없이 독립 실행 불가

### 3.4 프로젝트 문서 관리 어려움

#### 문제 7: 스펙 문서 과다

```
docs/specs/
├── 001-datastore-webui-api/
├── 001-ethercat-integration/
├── 001-polymorphic-task-management/
├── 001-production-readiness/
├── 001-project-overview/
├── 001-rt-stability/
├── 001-task-management/      # ⚠️ 001이 7개
├── ...
└── 022-fix-architecture-issues/
```

**문제**:
- Feature 넘버링이 중복 (001이 7개)
- 어떤 Feature가 구현되었고 어떤 것이 계획인지 불명확
- 의존 관계 추적 어려움

## 4. 개선 방안

### 4.1 빌드 시스템 모듈화

#### 방안 1: CMake 서브디렉토리 구조화

**목표**: Core를 라이브러리로, Robot을 별도 타겟으로 분리

```cmake
# 개선 후 구조
mxrc/
├── CMakeLists.txt                 # 루트 (최소화)
├── src/
│   ├── core/
│   │   ├── CMakeLists.txt         # Core 라이브러리 정의
│   │   └── ...
│   ├── robot/
│   │   └── pallet_shuttle/
│   │       ├── CMakeLists.txt     # Robot 타겟 정의
│   │       └── ...
│   └── executables/
│       ├── rt/
│       │   └── CMakeLists.txt     # RT 실행 파일
│       └── nonrt/
│           └── CMakeLists.txt     # Non-RT 실행 파일
└── tests/
    └── CMakeLists.txt             # 테스트 타겟
```

**루트 CMakeLists.txt** (간결화):
```cmake
cmake_minimum_required(VERSION 3.16)
project(mxrc)

# 의존성 찾기
find_package(spdlog REQUIRED)
# ...

# 서브디렉토리 추가
add_subdirectory(src/core)
add_subdirectory(src/robot/pallet_shuttle)
add_subdirectory(src/executables)
add_subdirectory(tests)
```

**src/core/CMakeLists.txt**:
```cmake
# Core 라이브러리 정의
add_library(mxrc_core STATIC
    action/core/ActionExecutor.cpp
    sequence/core/SequenceEngine.cpp
    # ... Core만의 소스
)

# 공개 헤더 지정
target_include_directories(mxrc_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../  # src/
        ${CMAKE_CURRENT_SOURCE_DIR}/action/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/control/interfaces
        # 공개 인터페이스만
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/action/core
        ${CMAKE_CURRENT_SOURCE_DIR}/datastore/managers
        # 내부 구현은 PRIVATE
)

target_link_libraries(mxrc_core
    PUBLIC spdlog::spdlog TBB::tbb
    PRIVATE nlohmann_json::nlohmann_json
)
```

**src/robot/pallet_shuttle/CMakeLists.txt**:
```cmake
add_library(mxrc_pallet_shuttle STATIC
    actions/MoveToPositionAction.cpp
    sequences/PalletTransportSequence.cpp
    tasks/PalletTransportTask.cpp
)

# Core 라이브러리에만 의존
target_link_libraries(mxrc_pallet_shuttle
    PUBLIC mxrc_core
)
```

**장점**:
- Core의 공개/비공개 인터페이스 명확히 구분
- 로봇별 독립적인 빌드 가능
- 증분 빌드 속도 향상
- 새 로봇 추가 시 `add_subdirectory(src/robot/new_robot)` 한 줄만 추가

### 4.2 systemd 서비스 중앙 관리

#### 방안 2: systemd 설정 템플릿화

**목표**: 서비스 설정을 중앙에서 생성하고 관리

```
systemd/
├── templates/              # Jinja2 템플릿
│   ├── mxrc-rt.service.j2
│   ├── mxrc-nonrt.service.j2
│   └── mxrc-webapi.service.j2
│
├── config/
│   └── services.yaml       # 중앙 설정
│
└── generate.py             # 생성 스크립트
```

**services.yaml** (중앙 설정):
```yaml
# 전역 설정
global:
  user: mxrc
  group: mxrc
  base_dir: /opt/mxrc

# CPU 할당 정책
cpu_allocation:
  rt_cores: [2, 3]          # RT 전용
  nonrt_cores: [0, 1]       # Non-RT 전용
  total_quota: 400%         # 전체 CPU 예산

# 메모리 할당 정책
memory_allocation:
  total_budget: 4G
  rt_max: 2G
  nonrt_max: 1G
  webapi_max: 512M

# 서비스 정의
services:
  mxrc-rt:
    type: realtime
    exec: /usr/local/bin/mxrc-rt
    dependencies:
      before: [mxrc-nonrt]
    scheduling:
      policy: SCHED_FIFO
      priority: 80
    resources:
      cpu_quota: 200%
      cpu_affinity: ${cpu_allocation.rt_cores}
      memory_max: ${memory_allocation.rt_max}

  mxrc-nonrt:
    type: standard
    exec: /usr/local/bin/mxrc-nonrt
    dependencies:
      after: [mxrc-rt]
    resources:
      cpu_quota: 100%
      cpu_affinity: ${cpu_allocation.nonrt_cores}
      memory_max: ${memory_allocation.nonrt_max}

  mxrc-webapi:
    type: standard
    exec: /usr/bin/node /opt/mxrc/webapi/dist/server.js
    working_dir: /opt/mxrc/webapi
    dependencies:
      after: [mxrc-rt, mxrc-nonrt]
      requires: [mxrc-rt]
    resources:
      cpu_quota: 150%
      cpu_affinity: ${cpu_allocation.nonrt_cores}
      memory_max: ${memory_allocation.webapi_max}
```

**generate.py** (자동 생성):
```python
#!/usr/bin/env python3
import yaml
import jinja2

# 설정 로드
with open('config/services.yaml') as f:
    config = yaml.safe_load(f)

# 검증: CPU/메모리 예산 초과 체크
total_cpu = sum(svc['resources']['cpu_quota'] for svc in config['services'].values())
if total_cpu > config['cpu_allocation']['total_quota']:
    raise ValueError(f"CPU quota exceeded: {total_cpu}% > {config['cpu_allocation']['total_quota']}%")

# 서비스 파일 생성
env = jinja2.Environment(loader=jinja2.FileSystemLoader('templates'))
for name, svc_config in config['services'].items():
    template = env.get_template(f'{name}.service.j2')
    output = template.render(config=config, service=svc_config)
    with open(f'{name}.service', 'w') as f:
        f.write(output)
```

**장점**:
- 리소스 예산을 한눈에 파악 가능
- 정책 변경 시 YAML 파일 하나만 수정
- CPU/메모리 오버커밋 자동 검증
- 서비스 간 의존성 명확히 문서화

### 4.3 WebAPI 통합 관리

#### 방안 3: IPC 스키마 자동 동기화

**목표**: C++ Core와 TypeScript WebAPI 간 타입 안전성 확보

```
config/ipc/
├── ipc-schema.yaml          # 단일 소스 of truth
│
scripts/codegen/
├── generate_ipc_schema.py   # C++ 헤더 생성 (기존)
└── generate_ts_types.py     # TypeScript 타입 생성 (신규)
```

**generate_ts_types.py**:
```python
#!/usr/bin/env python3
import yaml
from jinja2 import Template

# ipc-schema.yaml 읽기
with open('config/ipc/ipc-schema.yaml') as f:
    schema = yaml.safe_load(f)

# TypeScript 타입 생성
ts_template = Template('''
// Auto-generated from ipc-schema.yaml - DO NOT EDIT

export interface DataStoreKeys {
{% for key in schema.datastore.keys %}
  '{{ key.name }}': {{ key.type }};
{% endfor %}
}

export interface EventBusEvents {
{% for event in schema.eventbus.events %}
  '{{ event.name }}': {{ event.payload_type }};
{% endfor %}
}
''')

output = ts_template.render(schema=schema)
with open('webapi/src/ipc/generated/types.ts', 'w') as f:
    f.write(output)
```

**CMakeLists.txt 통합**:
```cmake
# C++ 헤더와 TypeScript 타입을 함께 생성
add_custom_command(
    OUTPUT
        ${CMAKE_BINARY_DIR}/generated/ipc/DataStoreKeys.h
        ${PROJECT_SOURCE_DIR}/webapi/src/ipc/generated/types.ts
    COMMAND ${Python3_EXECUTABLE} scripts/codegen/generate_ipc_schema.py
    COMMAND ${Python3_EXECUTABLE} scripts/codegen/generate_ts_types.py
    DEPENDS config/ipc/ipc-schema.yaml
)
```

**장점**:
- Core와 WebAPI의 타입이 항상 동기화됨
- 스키마 변경 시 자동으로 양쪽 코드 생성
- 컴파일 타임에 타입 불일치 감지

#### 방안 4: WebAPI Mock 모드 개선

**목표**: Core 없이 WebAPI 독립 개발 가능

**webapi/tests/mocks/core-mock.ts**:
```typescript
// Core IPC를 모방하는 Mock 서버
import { MockIPCServer } from './ipc-mock';
import { DataStoreKeys } from '../src/ipc/generated/types';

const mockServer = new MockIPCServer();

// 초기 상태 설정
mockServer.datastore.set('robot.position.x', 0.0);
mockServer.datastore.set('robot.position.y', 0.0);

// 이벤트 시뮬레이션
setInterval(() => {
  mockServer.eventbus.emit('robot.state.changed', {
    state: 'RUNNING',
    timestamp: Date.now()
  });
}, 1000);

mockServer.listen('/tmp/mxrc-mock.sock');
```

**package.json**:
```json
{
  "scripts": {
    "dev:standalone": "ts-node tests/mocks/core-mock.ts & npm run dev",
    "dev:integrated": "IPC_SOCKET_PATH=/var/run/mxrc/ipc.sock npm run dev"
  }
}
```

### 4.4 문서 및 프로젝트 관리

#### 방안 5: Feature 관리 시스템

**목표**: Feature 상태 추적 및 의존성 관리

```
docs/
├── features/
│   ├── index.yaml          # Feature 레지스트리
│   └── status.md           # 자동 생성되는 상태 보드
│
└── specs/
    ├── F001-task-management/
    ├── F016-pallet-shuttle/
    └── F022-architecture-fix/
```

**features/index.yaml**:
```yaml
features:
  F001:
    name: "Task Management"
    status: completed
    version: 1.0.0

  F016:
    name: "Pallet Shuttle Control"
    status: in_progress
    version: 0.8.0
    depends_on: [F001, F015, F022]
    blocking: []

  F022:
    name: "Architecture Issues Fix"
    status: in_progress
    version: 0.9.0
    depends_on: [F001, F015]
    blocking: [F016]  # F016이 이것에 의존
    priority: critical

  F019:
    name: "Architecture Improvements"
    status: completed
    version: 1.0.0
```

**scripts/feature-status.py**:
```python
#!/usr/bin/env python3
import yaml

with open('docs/features/index.yaml') as f:
    features = yaml.safe_load(f)

# 의존성 그래프 검증
for fid, feature in features['features'].items():
    for dep in feature.get('depends_on', []):
        if features['features'][dep]['status'] != 'completed':
            print(f"⚠️  {fid} depends on incomplete {dep}")

# 상태 보드 생성
md = "# Feature Status Board\n\n"
md += "| ID | Name | Status | Depends On | Blocking |\n"
md += "|----|------|--------|------------|----------|\n"
for fid, feature in features['features'].items():
    deps = ', '.join(feature.get('depends_on', []))
    blocking = ', '.join(feature.get('blocking', []))
    md += f"| {fid} | {feature['name']} | {feature['status']} | {deps} | {blocking} |\n"

with open('docs/features/status.md', 'w') as f:
    f.write(md)
```

**CI/CD 통합**:
```yaml
# .github/workflows/feature-check.yml
name: Feature Dependency Check

on: [pull_request]

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Validate feature dependencies
        run: python3 scripts/feature-status.py
```

## 5. 마이그레이션 전략

### 5.1 단계별 적용 계획

#### Phase 1: 빌드 시스템 모듈화 (우선순위: 높음)

**목표**: CMake 구조 개선으로 빌드 관리 용이성 확보

**작업 항목**:
1. `src/core/CMakeLists.txt` 생성 및 `mxrc_core` 라이브러리 정의
2. `src/robot/pallet_shuttle/CMakeLists.txt` 생성
3. 루트 `CMakeLists.txt` 리팩토링
4. 빌드 검증 및 테스트

**예상 소요**: 2-3일
**위험도**: 중간 (빌드 시스템 변경은 전체 프로젝트에 영향)

**검증 기준**:
```bash
# 모든 기존 타겟이 정상 빌드되어야 함
cmake -B build
cmake --build build --target mxrc_core
cmake --build build --target mxrc_pallet_shuttle
cmake --build build --target rt
cmake --build build --target nonrt
cmake --build build --target run_tests
./build/run_tests  # 모든 테스트 통과
```

#### Phase 2: systemd 설정 중앙화 (우선순위: 중간)

**목표**: 서비스 관리 단순화 및 리소스 정책 명확화

**작업 항목**:
1. `systemd/config/services.yaml` 작성
2. `systemd/generate.py` 스크립트 구현
3. Jinja2 템플릿 작성
4. 기존 서비스 파일과 생성 파일 비교 검증

**예상 소요**: 1-2일
**위험도**: 낮음 (systemd 파일만 변경, 런타임 영향 없음)

#### Phase 3: IPC 스키마 자동 동기화 (우선순위: 높음)

**목표**: Core와 WebAPI 타입 안전성 확보

**작업 항목**:
1. `scripts/codegen/generate_ts_types.py` 작성
2. CMake에 TypeScript 생성 단계 추가
3. WebAPI에서 생성된 타입 사용하도록 리팩토링
4. 통합 테스트 작성

**예상 소요**: 2일
**위험도**: 중간 (WebAPI 코드 변경 필요)

#### Phase 4: Feature 관리 시스템 (우선순위: 낮음)

**목표**: 장기 프로젝트 관리 개선

**작업 항목**:
1. `docs/features/index.yaml` 작성
2. Feature 상태 스크립트 작성
3. CI 통합

**예상 소요**: 1일
**위험도**: 낮음 (문서 변경만)

### 5.2 롤백 계획

각 Phase마다 롤백 가능하도록 설계:

```bash
# Phase 1 롤백
git checkout main -- CMakeLists.txt src/*/CMakeLists.txt

# Phase 2 롤백
# 기존 .service 파일 유지하면 됨 (생성 스크립트 사용 안 함)

# Phase 3 롤백
# WebAPI에서 수동 타입 정의로 되돌림
```

## 6. 예상 효과

### 6.1 정량적 개선

| 항목 | 현재 | 개선 후 | 개선율 |
|------|------|---------|--------|
| CMakeLists.txt 라인 수 | 1100+ | ~400 | -63% |
| Include 경로 중복 | 높음 | 없음 | -100% |
| 증분 빌드 시간 (Core 변경 시) | ~45초 | ~15초 | -66% |
| 새 로봇 추가 시 변경 파일 수 | 3+ | 1 | -66% |
| WebAPI 타입 불일치 버그 | 가능 | 불가능 | -100% |

### 6.2 정성적 개선

1. **개발자 경험 향상**
   - 빌드 구조 이해 시간 단축
   - 새 기능 추가 시 변경 범위 명확

2. **유지보수성 향상**
   - 모듈 간 의존성 명확
   - 리소스 정책 중앙 관리

3. **확장성 향상**
   - 새 로봇 추가 용이
   - 멀티 언어 프로젝트 통합 관리

4. **안정성 향상**
   - 타입 안전성 보장
   - 빌드 타임 에러 감지

## 7. 결론

현재 MXRC 프로젝트는 **기술적으로는 매우 우수한 아키텍처**를 가지고 있으나, **프로젝트 관리 측면에서는 개선의 여지**가 있습니다.

### 7.1 핵심 문제

1. **모놀리식 빌드 시스템**: 1100+ 라인의 CMakeLists.txt로 관리 어려움
2. **분산된 systemd 설정**: 서비스별로 리소스 정책이 흩어져 있음
3. **멀티 언어 프로젝트 동기화**: C++와 TypeScript 간 타입 불일치 가능성
4. **문서 관리 부재**: Feature 상태 추적 시스템 없음

### 7.2 제안 솔루션

1. **CMake 서브디렉토리 구조화**: Core를 라이브러리로, Robot을 독립 타겟으로
2. **systemd 설정 템플릿화**: YAML 기반 중앙 관리 및 자동 생성
3. **IPC 스키마 자동 동기화**: 단일 YAML에서 C++/TypeScript 타입 생성
4. **Feature 관리 시스템**: 의존성 추적 및 상태 보드

### 7.3 다음 단계

**즉시 적용 권장** (Feature 016 구현 전):
- Phase 1: 빌드 시스템 모듈화
- Phase 3: IPC 스키마 자동 동기화

**점진적 적용 가능**:
- Phase 2: systemd 설정 중앙화
- Phase 4: Feature 관리 시스템

이러한 개선을 통해 MXRC 프로젝트는 기술적 우수성뿐만 아니라 **관리 용이성과 확장성**까지 갖춘 성숙한 로봇 제어 플랫폼으로 발전할 수 있을 것입니다.

---

## 부록 A: 참고 자료

- [docs/issue/009-architecture-analysis.md](../issue/009-architecture-analysis.md): 아키텍처 분석 보고서
- [docs/specs/022-fix-architecture-issues/](../specs/022-fix-architecture-issues/): 아키텍처 이슈 수정
- [docs/specs/016-pallet-shuttle-control/](../specs/016-pallet-shuttle-control/): 팔렛 셔틀 제어 기능

## 부록 B: 용어 정의

- **모놀리식 CMakeLists**: 모든 타겟과 설정이 하나의 CMake 파일에 정의된 구조
- **IPC 스키마**: Inter-Process Communication에서 사용하는 데이터 구조 정의
- **systemd 타겟**: 여러 서비스를 그룹화하는 systemd 단위
- **Feature 의존성**: 기능 간의 선후 관계 및 요구 사항
