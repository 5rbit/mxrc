# 빠른 시작 (Quickstart): Task 관리 모듈

이 문서는 Task 관리 모듈을 빠르게 이해하고 사용하는 데 필요한 핵심 정보를 제공합니다.

## 1. 모듈 개요

Task 관리 모듈은 로봇 시스템 내에서 다양한 Task(작업)를 정의, 등록, 실행 및 모니터링하는 기능을 제공합니다. Task의 종류에 관계없이 일관된 인터페이스를 통해 Task를 관리할 수 있도록 설계되었습니다. SOLID 원칙을 준수하여 다른 모듈과의 결합도를 낮추고 독립적인 실행이 가능하도록 구현됩니다.

## 2. 주요 인터페이스

Task 관리 모듈은 주로 다음 두 가지 인터페이스를 통해 외부와 상호작용합니다.

### 2.1. `ITaskManager`

`ITaskManager`는 Task 정의의 등록, 조회, Task 실행 요청 및 상태 모니터링 등 Task의 핵심 관리 기능을 제공하는 인터페이스입니다. 이 인터페이스는 Task 관리 모듈의 내부 구현과 외부 사용자를 분리하여 모듈의 독립성을 보장합니다.

**주요 기능:**

- `registerTaskDefinition(taskName, taskType, defaultParameters)`: 새로운 Task 정의를 시스템에 등록합니다.
- `getAllTaskDefinitions()`: 등록된 모든 Task 정의 목록을 반환합니다.
- `getTaskDefinitionById(taskId)`: 특정 Task 정의의 상세 정보를 조회합니다.
- `requestTaskExecution(taskId, runtimeParameters)`: 특정 Task의 실행을 요청합니다.
- `getTaskExecutionStatus(executionId)`: 실행 중인 Task의 현재 상태를 모니터링합니다.

### 2.2. `IOperatorInterface`

`IOperatorInterface`는 외부 시스템(예: 오퍼레이터 콘솔, 상위 제어 시스템)이 Task 관리 모듈과 상호작용하기 위한 인터페이스입니다. `ITaskManager`의 기능을 추상화하여 사용자 친화적인 방식으로 Task를 제어할 수 있도록 합니다.

**주요 기능:**

- `defineNewTask(taskName, taskType, defaultParameters)`: 새로운 Task 정의를 생성하고 등록합니다.
- `getAvailableTasks()`: 현재 시스템에 등록된 Task 정의 목록을 가져옵니다.
- `getTaskDetails(taskId)`: 특정 Task의 상세 정보를 조회합니다.
- `startTaskExecution(taskId, runtimeParameters)`: Task 실행을 시작합니다.
- `monitorTaskStatus(executionId)`: 실행 중인 Task의 상태를 확인합니다.

## 3. Task 정의 및 실행 흐름

1.  **Task 정의**: `IOperatorInterface::defineNewTask` 또는 `ITaskManager::registerTaskDefinition`을 사용하여 Task의 이름, 타입, 기본 파라미터 등을 정의하고 시스템에 등록합니다.
2.  **Task 조회**: `IOperatorInterface::getAvailableTasks` 또는 `ITaskManager::getAllTaskDefinitions`를 통해 등록된 Task 목록을 확인합니다.
3.  **Task 실행 요청**: `IOperatorInterface::startTaskExecution` 또는 `ITaskManager::requestTaskExecution`을 호출하여 특정 Task의 실행을 요청합니다. 이때, Task 실행에 필요한 런타임 파라미터를 전달할 수 있습니다.
4.  **Task 상태 모니터링**: `IOperatorInterface::monitorTaskStatus` 또는 `ITaskManager::getTaskExecutionStatus`를 통해 실행 중인 Task의 현재 상태(진행 중, 완료, 실패 등)와 진행률을 실시간으로 확인합니다.
5.  **오류 처리**: Task 실행 중 오류 발생 시, 시스템은 정의된 복구 절차(예: 이전 상태로 롤백, 대체 Task 실행)를 자동으로 수행합니다.

## 4. 데이터 모델

주요 엔티티는 `Task`와 `TaskManager`입니다. `Task`는 고유 ID, 이름, 타입, 파라미터, 상태, 진행률 등의 속성을 가지며, `TaskManager`는 이러한 `Task`들을 관리합니다. 자세한 내용은 `data-model.md` 파일을 참조하십시오.

## 5. SOLID 원칙 준수

Task 관리 모듈은 SOLID 원칙을 엄격히 준수하여 설계됩니다. 특히, 단일 책임 원칙(SRP)과 개방-폐쇄 원칙(OCP)을 통해 Task의 종류가 추가되거나 변경되어도 기존 코드를 수정하지 않고 확장할 수 있도록 유연성을 확보합니다. 인터페이스 분리 원칙(ISP)을 통해 `ITaskManager`와 `IOperatorInterface`와 같이 클라이언트가 사용하지 않는 메서드에 의존하지 않도록 합니다.
