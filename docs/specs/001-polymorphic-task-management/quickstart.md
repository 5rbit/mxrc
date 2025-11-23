# 빠른 시작 (Quickstart): 다형적 Task 관리 모듈

이 문서는 다형적 Task 관리 모듈을 빠르게 이해하고 사용하는 데 필요한 핵심 정보를 제공합니다.

## 1. 모듈 개요

다형적 Task 관리 모듈은 로봇 시스템 내에서 다양한 Task(작업)를 종류에 상관없이 정의, 등록, 실행 및 모니터링하는 기능을 제공합니다. 새로운 Task 유형을 쉽게 정의하고 추가할 수 있도록 설계되었으며, SOLID 원칙을 준수하여 다른 모듈과의 낮은 결합도를 유지하고 독립적인 실행이 가능하도록 구현됩니다.

## 2. 주요 인터페이스

다형적 Task 관리 모듈은 주로 다음 세 가지 인터페이스를 통해 외부와 상호작용합니다.

### 2.1. `ITaskManager`

`ITaskManager`는 Task 유형의 등록, Task 인스턴스 생성, Task 실행 요청 및 상태 모니터링 등 Task의 핵심 관리 기능을 제공하는 인터페이스입니다.

**주요 기능:**

-   `registerTaskType(taskType, taskFactory)`: 새로운 Task 유형을 시스템에 등록하고, 해당 유형의 Task 인스턴스를 생성할 팩토리 함수를 연결합니다.
-   `createTaskInstance(taskType, taskName, initialParameters)`: 등록된 Task 유형을 기반으로 새로운 Task 인스턴스를 생성합니다.
-   `getTaskStatus(taskId)`: 특정 Task 인스턴스의 현재 상태를 조회합니다.
-   `startTaskExecution(taskId, runtimeParameters)`: 특정 Task 인스턴스의 실행을 시작합니다.
-   `updateTaskStatus(taskId, status)`: 특정 Task 인스턴스의 상태를 업데이트합니다.
-   `updateTaskProgress(taskId, progress)`: 특정 Task 인스턴스의 진행률을 업데이트합니다.

### 2.2. `IOperatorInterface`

`IOperatorInterface`는 외부 시스템(예: 오퍼레이터 콘솔, 상위 제어 시스템)이 Task 관리 모듈과 상호작용하기 위한 인터페이스입니다. `ITaskManager`의 기능을 추상화하여 사용자 친화적인 방식으로 Task를 제어할 수 있도록 합니다.

**주요 기능:**

-   `defineNewTaskType(taskType, description, requiredParametersSchema)`: 새로운 Task 유형을 시스템에 등록합니다.
-   `createAndRegisterTask(taskType, taskName, initialParameters)`: 특정 유형의 Task 인스턴스를 생성하고 등록합니다.
-   `getTaskDetails(taskId)`: 특정 Task 인스턴스의 상세 정보를 조회합니다.
-   `startTask(taskId, runtimeParameters)`: Task 인스턴스의 실행을 시작합니다.
-   `getAvailableTaskTypes()`: 현재 시스템에 등록된 모든 Task 유형 목록을 조회합니다.
-   `getAllTaskInstances()`: 현재 시스템에 등록된 모든 Task 인스턴스 목록을 조회합니다.

### 2.3. `ITask`

`ITask`는 모든 다형적 Task 구현체가 따라야 하는 기본 인터페이스입니다. 각 Task 유형은 이 인터페이스를 구현하여 고유한 실행 로직을 제공합니다.

**주요 기능:**

-   `getId()`, `getName()`, `getType()`, `getParameters()`, `getStatus()`, `getProgress()`, `getCreatedAt()`, `getUpdatedAt()`: Task의 속성을 조회합니다.
-   `setStatus(status)`, `setProgress(progress)`, `setParameters(parameters)`: Task의 속성을 설정합니다.
-   `execute(runtimeParameters)`: Task의 실제 실행 로직을 포함합니다.

## 3. Task 정의 및 실행 흐름

1.  **Task 유형 정의 및 등록**: `IOperatorInterface::defineNewTaskType` 또는 `ITaskManager::registerTaskType`을 사용하여 새로운 Task 유형을 시스템에 등록합니다. 이때, 해당 유형의 Task 인스턴스를 생성할 팩토리 함수를 함께 제공합니다.
2.  **Task 인스턴스 생성**: `IOperatorInterface::createAndRegisterTask` 또는 `ITaskManager::createTaskInstance`를 사용하여 등록된 Task 유형을 기반으로 특정 Task 인스턴스를 생성합니다.
3.  **Task 인스턴스 조회**: `IOperatorInterface::getAllTaskInstances` 또는 `ITaskManager::getTaskStatus`를 통해 등록된 Task 인스턴스 목록을 확인하거나 특정 Task의 상세 정보를 조회합니다.
4.  **Task 실행 요청**: `IOperatorInterface::startTask` 또는 `ITaskManager::startTaskExecution`을 호출하여 특정 Task 인스턴스의 실행을 요청합니다. 이때, Task 실행에 필요한 런타임 파라미터를 전달할 수 있습니다.
5.  **Task 상태 모니터링**: `IOperatorInterface::getTaskDetails` 또는 `ITaskManager::getTaskStatus`를 통해 실행 중인 Task 인스턴스의 현재 상태(진행 중, 완료, 실패 등)와 진행률을 실시간으로 확인합니다.
6.  **Task 상태/진행률 업데이트**: `ITaskManager::updateTaskStatus` 및 `ITaskManager::updateTaskProgress`를 사용하여 Task 인스턴스의 상태 및 진행률을 업데이트합니다.

## 4. SOLID 원칙 준수

다형적 Task 관리 모듈은 SOLID 원칙을 엄격히 준수하여 설계됩니다. 특히, 개방-폐쇄 원칙(OCP)을 통해 새로운 Task 유형이 추가되더라도 기존 Task 관리 모듈의 핵심 코드를 수정할 필요 없이 확장할 수 있도록 유연성을 확보합니다. 인터페이스 분리 원칙(ISP)을 통해 `ITaskManager`, `IOperatorInterface`, `ITask`와 같이 클라이언트가 사용하지 않는 메서드에 의존하지 않도록 합니다.

## 5. 외부 시스템 통합

외부 저장소 모듈 및 외부 Task 실행기와는 전용 어댑터/플러그인 계층을 통해 통합됩니다. 이는 모듈 간의 낮은 결합도를 유지하고 확장성을 높이는 데 기여합니다.
