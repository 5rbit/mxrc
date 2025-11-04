# 데이터 모델: Task & Mission Management (Task 및 임무 관리) 고도화

**기능 브랜치**: `004-task-mission-management`
**생성일**: 2025-11-03
**사양서**: [specs/004-task-mission-management/spec.md](specs/004-task-mission-management/spec.md)
**계획**: [specs/004-task-mission-management/plan.md](specs/004-task-mission-management/plan.md)

## 1. Mission 정의 (Behavior Tree 기반)

Mission은 Behavior Tree 구조로 정의되며, JSON 또는 YAML 형식의 설정 파일로 관리됩니다. 각 노드는 Task 또는 제어 흐름 로직을 나타냅니다.

### MissionDefinition
- **id**: `string` (Mission 고유 식별자)
- **name**: `string` (Mission 이름)
- **version**: `string` (Semantic Versioning 준수)
- **description**: `string` (Mission 설명)
- **behavior_tree**: `object` (Behavior Tree 구조를 나타내는 JSON/YAML 객체)
  - **root**: `string` (루트 노드의 ID)
  - **nodes**: `array<object>` (Behavior Tree 노드 목록)
    - **id**: `string` (노드 고유 식별자)
    - **type**: `enum` (Sequence, Selector, Parallel, Action, Condition 등)
    - **task_id**: `string` (Action 노드인 경우, 실행할 Task의 ID)
    - **children**: `array<string>` (자식 노드의 ID 목록)
    - **parameters**: `object` (노드 실행에 필요한 파라미터)
    - **failure_strategy**: `enum` (ABORT_MISSION, RETRY_TRANSIENT, SKIP_TASK, CUSTOM_HANDLER) (선택 사항, Task 레벨의 FailureStrategy를 오버라이드)

## 2. Task

로봇이 수행하는 가장 작은 작업 단위입니다. `AbstractTask` 인터페이스를 상속하며, 자체 상태와 오류 처리 로직을 가집니다.

### Task (런타임 인스턴스)
- **id**: `string` (Task 정의 고유 식별자)
- **instance_id**: `string` (Task 런타임 인스턴스 고유 식별자)
- **name**: `string` (Task 이름)
- **current_state**: `enum` (PENDING, RUNNING, PAUSED, COMPLETED, FAILED, CANCELLED)
- **context**: `TaskContext` (Task 실행에 필요한 입력/출력 파라미터)
- **priority**: `integer` (Task 스케줄링 우선순위)
- **dependencies**: `array<string>` (의존하는 다른 Task 인스턴스 ID 목록)
- **resource_requirements**: `array<string>` (필요한 리소스 목록)
- **failure_strategy**: `enum` (ABORT_MISSION, RETRY_TRANSIENT, SKIP_TASK, CUSTOM_HANDLER)
- **start_time**: `timestamp`
- **end_time**: `timestamp` (완료 또는 실패 시)
- **error_info**: `object` (오류 코드, 설명, 관련 Task ID)

### TaskContext
- **parameters**: `map<string, any>` (다양한 데이터 타입을 저장하는 키-값 쌍)

## 3. MissionState (런타임 인스턴스)

실행 중인 Mission의 현재 상태를 나타냅니다.

### MissionState
- **mission_id**: `string` (실행 중인 Mission의 정의 ID)
- **instance_id**: `string` (Mission 런타임 인스턴스 고유 식별자)
- **current_status**: `enum` (RUNNING, PAUSED, COMPLETED, FAILED, CANCELLED)
- **current_task_instance_id**: `string` (현재 실행 중인 Task 인스턴스 ID)
- **progress**: `float` (0.0 ~ 1.0, Mission 전체 진행률)
- **estimated_completion_time**: `timestamp`
- **start_time**: `timestamp`
- **end_time**: `timestamp` (완료 또는 실패 시)
- **active_task_instances**: `array<string>` (현재 활성화된 Task 인스턴스 ID 목록)

## 4. TaskStateHistory

각 Task의 상태 변화 이력을 기록합니다.

### TaskStateHistory Entry
- **task_instance_id**: `string`
- **timestamp**: `timestamp`
- **old_state**: `enum` (PENDING, RUNNING, PAUSED, COMPLETED, FAILED, CANCELLED)
- **new_state**: `enum` (PENDING, RUNNING, PAUSED, COMPLETED, FAILED, CANCELLED)
- **reason**: `string` (상태 변화 이유, 예: "Task completed successfully", "Resource unavailable")
- **error_info**: `object` (오류 코드, 설명, 관련 Task ID) (오류로 인한 상태 변화 시)

## 6. DataStore 저장 항목

`DataStore`는 다음 항목들을 영속적으로 저장하고 관리합니다.

- **Mission 정의**: `MissionDefinition` 객체
- **Mission 런타임 상태**: `MissionState` 객체 (주기적으로 또는 주요 상태 변화 시 저장)
- **Task 런타임 상태**: `Task` 객체 (주기적으로 또는 주요 상태 변화 시 저장)
- **Task 상태 변화 이력**: `TaskStateHistory` Entry
