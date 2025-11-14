# 기능 사양서: TaskManager 모듈 리팩터링

**기능 브랜치**: `016-refactor-taskmanager`
**생성일**: 2025-11-13
**상태**: 초안
**입력**: 사용자 설명: "아래의 제안 내용으로 리펙터링 진행하고싶어 아키텍처 분석 현재 taskmanager 폴더는 다음과 같은 구조와 특징을 가지고 있습니다. 1. 역할 기반 디렉토리 구조: * commands: StartTaskCommand와 같이 Command Pattern을 사용하여 Task 실행 로직을 캡슐화합니다. * factory: TaskFactory를 통해 Task 타입에 따른 객체 생성을 담당합니다. (Factory Pattern) * interfaces: ITask, ITaskManager 등 시스템의 핵심 인터페이스를 정의하여 의존성 역전 원칙(DIP)을 적용하려는 시도가 보입니다. * operator_interface: OperatorInterface는 ITaskManager를 사용하여 외부(UI 또는 다른 시스템)에 기능을 제공하는 Facade 역할을 합니다. * tasks: DriveForwardTask, LiftPalletTask 등 ITask의 구체적인 구현체들이 위치합니다. 2. 주요 클래스 및 역할: * TaskManager: Task 정의 등록, 실행 요청, 상태 관리 등 Task와 관련된 핵심 비즈니스 로직을 처리합니다. ITaskManager 인터페이스를 구현합니다. * Task: Task의 데이터 모델(ID, 이름, 상태 등)을 나타내는 클래스입니다. * TaskDto: 데이터 전송 객체(DTO)로, 시스템 경계를 넘어 Task 정보를 전달하는 데 사용됩니다. * ITask: 실행 가능한 Task의 공통 인터페이스. execute, cancel 등의 메서드를 정의합니다. * TaskFactory: ITask 구현체를 동적으로 생성하는 싱글톤 팩토리입니다. * StartTaskCommand: TaskManager에게 Task 실행을 요청하는 Command 객체입니다. * OperatorInterface: ITaskManager에 대한 인터페이스를 제공하여 시스템의 다른 부분과의 결합도를 낮춥니다. 3. 설계 패턴: * Factory Pattern: TaskFactory에서 새로운 Task 유형을 쉽게 추가할 수 있도록 합니다. * Command Pattern: StartTaskCommand를 통해 요청을 객체로 캡슐화하여, 요청을 큐에 저장하거나 로깅하는 등의 추가적인 확장이 용이합니다. * Facade Pattern: OperatorInterface가 복잡한 TaskManager의 기능을 단순화된 인터페이스로 제공합니다. * Dependency Injection: OperatorInterface가 생성자에서 ITaskManager의 shared_ptr를 주입받아, TaskManager의 구체적인 구현으로부터 분리됩니다. 개선 제안 1. `Task`와 `ITask`의 역할 분리 명확화: * 현재 Task 클래스는 Task의 상태와 데이터를 관리하는 데이터 컨테이너 역할에 가깝고, ITask는 실제 실행 로직을 정의하는 인터페이스입니다. 하지만 TaskManager가 Task와 ITask 두 가지를 모두 관리(tasks_ 와 activeExecutableTasks_)하고 있어 복잡성이 증가합니다. * 개선 방안: Task 클래스가 ITask를 멤버로 소유하거나, Task 클래스 자체가 ITask 인터페이스를 상속받아 데이터와 행위를 함께 갖도록 구조를 변경하는 것을 고려할 수 있습니다. 후자의 경우, Task가 데이터인 동시에 실행 가능한 객체가 되어 관리가 더 용이해집니다. 2. `TaskManager`의 책임 과다 (God Object) 문제: * TaskManager가 Task 정의 관리, 실행 관리, 상태 업데이트, Command 실행 등 너무 많은 책임을 가지고 있습니다. * 개선 방안: * Task Definition Registry: Task 정의(메타데이터)를 관리하는 별도의 클래스(TaskDefinitionRegistry 등)를 분리합니다. * Task Executor/Scheduler: 실제 ITask의 실행 생명주기(실행, 중지, 취소)를 관리하는 TaskExecutor 또는 TaskScheduler를 분리합니다. TaskManager는 이 Executor에게 Task 실행을 위임합니다. 이렇게 하면 스레드 풀링, 비동기 실행 관리 등을 TaskExecutor에 집중시킬 수 있습니다. 3. 상태 관리 로직 개선: * 현재 updateTaskStatus와 updateTaskProgress가 ITaskManager 인터페이스에 직접 노출되어 있어, 외부에서 Task의 상태를 직접 변경할 수 있습니다. 이는 Task의 상태 무결성을 해칠 수 있습니다. * 개선 방안: Task의 상태 변경은 해당 Task(ITask 구현체) 스스로가 내부 로직에 따라 수행해야 합니다. ITask에 getStatus()와 같은 메서드를 추가하고, TaskManager나 TaskExecutor는 이 상태를 조회하여 DTO를 업데이트하는 역할을 해야 합니다. 외부에서는 상태 변경을 직접 호출하는 것이 아니라 cancel()과 같은 행위를 요청해야 합니다. 4. `interfaces`와 `tasks` 폴더의 의존성 관계: * ITask 인터페이스는 interfaces에 있지만, 구체적인 Task들은 tasks 폴더에 있습니다. 이는 좋은 구조입니다. * 하지만 TaskManager.h가 ITask.h, ICommand.h 등 여러 인터페이스와 TaskFactory.h까지 직접 포함하고 있어 결합도가 높습니다. * 개선 방안: Forward Declaration을 사용하여 헤더 파일에서의 #include를 최소화하고, 실제 구현이 필요한 .cpp 파일에서만 헤더를 포함하도록 변경하여 컴파일 의존성을 낮출 수 있습니다. 5. Command Pattern 활용 확대: * StartTaskCommand 외에 CancelTaskCommand, PauseTaskCommand 등 Task와 관련된 다른操作도 Command Pattern으로 구현하면, Undo/Redo 기능이나 트랜잭션 로그 구현이 용이해집니다. 요약 현재 아키텍처는 주요 디자인 패턴을 적절히 활용하여 확장성을 고려한 좋은 시도를 보여줍니다. 하지만 TaskManager의 책임이 과도하게 집중되어 있어 향후 유지보수가 어려워질 수 있습니다. 핵심 개선점은 `TaskManager`의 책임을 분리하는 것입니다. Task 정의 관리, Task 실행 및 생명주기 관리로 역할을 나누어 별도의 클래스로 만들면 각 클래스의 책임이 명확해지고 시스템의 유연성과 확장성이 크게 향상될 것입니다. 또한, Task의 상태는 외부에서 직접 변경하는 것이 아니라 Task 스스로가 관리하도록 하여 캡슐화를 강화하는 것이 좋습니다."

## 사용자 시나리오 및 테스트 *(필수)*

### 사용자 스토리 1 - `TaskManager` 책임 분리 (우선순위: P1)

`TaskManager`의 과도한 책임을 분리하여 유지보수성을 향상시킵니다. `TaskManager`는 Task 실행 요청의 진입점 역할만 하고, 실제 로직은 다른 클래스에 위임합니다.

**이 우선순위인 이유**: `TaskManager`는 현재 God Object에 가까워 수정이 어렵고 테스트가 복잡합니다. 책임을 분리하면 각 클래스가 단일 책임을 갖게 되어 코드 이해와 유지보수가 용이해집니다.

**독립 테스트**: 분리된 `TaskDefinitionRegistry`와 `TaskExecutor` 클래스는 각각 독립적으로 단위 테스트를 할 수 있습니다.

**인수 시나리오**:

1.  **주어진 상황** 개발자가 새로운 Task 유형을 시스템에 추가해야 할 때, **언제** `TaskDefinitionRegistry`에 새 Task 정의를 등록하면, **그러면** `TaskManager` 코드 변경 없이 새 Task를 생성하고 실행할 수 있어야 합니다.
2.  **주어진 상황** Task 실행 로직을 변경해야 할 때, **언제** `TaskExecutor`의 코드만 수정하면, **그러면** `TaskManager`나 `TaskDefinitionRegistry`에 영향을 주지 않고 변경이 완료되어야 합니다.

---

### 사용자 스토리 2 - `Task`와 `ITask` 역할 명확화 (우선순위: P2)

`Task` 클래스가 데이터와 행위를 모두 갖도록 `ITask` 인터페이스를 직접 구현하게 하여, 데이터와 실행 로직을 통합합니다.

**이 우선순위인 이유**: 현재 `Task`와 `ITask`가 분리되어 있어 `TaskManager`가 두 개의 다른 목록(`tasks_`, `activeExecutableTasks_`)을 관리해야 하는 등 불필요한 복잡성이 발생합니다. 역할을 통합하면 모델이 단순해지고 관리가 용이해집니다.

**독립 테스트**: `Task` 객체 자체가 `execute`, `cancel` 등의 메서드를 가지므로, `Task` 객체만으로 실행 관련 단위 테스트가 가능합니다.

**인수 시나리오**:

1.  **주어진 상황** `TaskManager`가 Task를 관리할 때, **언제** 단일 목록(예: `std::vector<std::shared_ptr<ITask>>`)만을 사용하여 모든 Task를 관리하면, **그러면** Task의 데이터와 실행 상태를 한 번에 조회하고 제어할 수 있어야 합니다.

---

### 사용자 스토리 3 - 상태 관리 로직 개선 (우선순위: P3)

Task의 상태를 외부에서 직접 변경할 수 없도록 캡슐화를 강화합니다.

**이 우선순위인 이유**: 외부에서 Task의 상태를 임의로 변경할 수 있으면 상태 무결성이 깨지기 쉽습니다. Task가 자신의 상태를 스스로 관리하게 함으로써 예측 가능하고 안정적인 상태 전이를 보장할 수 있습니다.

**독립 테스트**: Task의 `execute` 또는 `cancel` 메서드를 호출했을 때, 내부 상태가 예상대로 변경되는지 단위 테스트를 통해 검증할 수 있습니다.

**인수 시나리오**:

1.  **주어진 상황** Task가 실행 중일 때, **언제** 외부 코드에서 `updateTaskStatus`와 같은 메서드를 직접 호출하려고 시도하면, **그러면** 컴파일 에러가 발생하거나 해당 기능이 제공되지 않아 상태를 변경할 수 없어야 합니다.
2.  **주어진 상황** 실행 중인 Task가 작업을 완료했을 때, **언제** Task 내부 로직에 의해 상태가 `COMPLETED`로 자동 변경되면, **그러면** `TaskManager`는 해당 Task의 상태를 조회하여 변경 사실을 인지할 수 있어야 합니다.

---

### 사용자 스토리 4 - Command Pattern 활용 확대 (우선순위: P4)

`StartTaskCommand` 외에 `CancelTaskCommand`, `PauseTaskCommand` 등 다양한 Task 제어 명령을 Command Pattern으로 구현합니다.

**이 우선순위인 이유**: Command Pattern을 확대 적용하면 Task에 대한 모든 조작을 객체로 캡슐화할 수 있습니다. 이는 향후 Undo/Redo, 로깅, 트랜잭션과 같은 고급 기능을 쉽게 추가할 수 있는 기반이 됩니다.

**독립 테스트**: `CancelTaskCommand` 객체를 생성하고 `execute` 메서드를 호출했을 때, 대상 Task가 실제로 취소되는지 단위 테스트로 검증할 수 있습니다.

**인수 시나리오**:

1.  **주어진 상황** 사용자가 실행 중인 Task를 취소하기를 원할 때, **언제** `CancelTaskCommand`를 생성하여 실행하면, **그러면** 해당 Task의 `cancel` 메서드가 호출되고 Task 상태가 `CANCELLED`로 변경되어야 합니다.

---

### 엣지 케이스

- 존재하지 않는 Task ID로 Task 실행 또는 취소를 요청하면 어떻게 됩니까? (오류 처리)
- 이미 완료되거나 취소된 Task에 대해 다시 취소 명령을 보내면 어떻게 됩니까? (상태 전이 규칙)
- Task 실행 중 시스템이 예기치 않게 종료되면 어떻게 됩니까? (상태 복구)

## 요구사항 *(필수)*

### 기능적 요구사항

- **FR-001**: `TaskManager`의 책임은 `TaskDefinitionRegistry`와 `TaskExecutor`로 분리되어야 합니다.
- **FR-002**: `TaskDefinitionRegistry`는 Task 정의(메타데이터)를 등록하고 관리해야 합니다.
- **FR-003**: `TaskExecutor`는 Task의 실행 생명주기(실행, 중지, 취소)를 관리해야 합니다.
- **FR-004**: `Task` 클래스는 데이터와 행위를 함께 갖도록 `ITask` 인터페이스를 구현해야 합니다.
- **FR-005**: Task의 상태 변경은 외부에서 직접 호출할 수 없어야 하며, Task 스스로 내부 로직에 따라 상태를 변경해야 합니다.
- **FR-006**: `CancelTaskCommand`, `PauseTaskCommand` 등 새로운 Command가 구현되어야 합니다.
- **FR-007**: 헤더 파일의 의존성을 줄이기 위해 Forward Declaration을 사용해야 합니다.

### 주요 엔티티

- **TaskDefinitionRegistry**: Task의 메타데이터(예: 이름, 유형, 생성 함수)를 관리하는 클래스.
- **TaskExecutor**: `ITask` 객체의 실행, 취소, 일시 중지 등 생명주기를 관리하는 클래스. 스레드 풀을 사용하여 비동기 실행을 처리할 수 있습니다.
- **ITask**: 데이터와 행위를 모두 포함하는 실행 가능한 Task의 인터페이스. 기존 `Task`와 `ITask`의 역할이 통합됩니다.
- **Command (CancelTask, PauseTask)**: `ICommand` 인터페이스를 구현하는 구체적인 명령 클래스들.

## 성공 기준 *(필수)*

### 측정 가능한 결과

- **SC-001**: `TaskManager` 클래스의 코드 라인 수가 리팩터링 이전 대비 30% 이상 감소합니다.
- **SC-002**: 새로운 Task 유형을 시스템에 추가할 때, `TaskManager` 클래스의 코드를 수정할 필요가 없습니다.
- **SC-003**: Task의 상태(예: `TaskStatus`)를 변경하는 public 메서드(`updateTaskStatus` 등)가 `ITaskManager` 인터페이스에서 제거되어 외부에서 직접 상태를 수정할 수 없습니다.
- **SC-004**: Task 취소 및 일시 중지와 같은 조작이 `CancelTaskCommand`, `PauseTaskCommand`와 같은 Command 객체를 통해 수행되는 것을 테스트 코드로 증명할 수 있습니다.
- **SC-005**: 전체 프로젝트의 컴파일 시간이 리팩터링 이전 대비 10% 이상 감소합니다.
