# TaskManager 모듈 단계별 개선 플랜

**날짜**: 2025-11-14
**브랜치**: `016-refactor-taskmanager`
**현재 상태**: Phase 6 T030 완료 (메인 빌드 성공, 테스트 미완료)

## 📊 현재 상태 분석

### ✅ 완료된 작업
- [x] TaskDefinitionRegistry 생성 (US1)
- [x] TaskExecutor 생성 (US1)
- [x] TaskManager 책임 분리 (US1)
- [x] 모든 인터페이스에 네임스페이스 추가
- [x] CancelTaskCommand, PauseTaskCommand 추가 (US4)
- [x] TaskFactory 제거
- [x] 메인 실행파일 빌드 성공

### ⚠️ 미완료/문제 사항
- [ ] Task 클래스 미구현 메서드 (`pause()`, `getType()`, `getParameters()`)
- [ ] TaskDefinitionRegistry 팩토리 시그니처 불일치
- [ ] TaskManager의 이중 저장소 (`tasks_` 벡터)
- [ ] StartTaskCommand 실제 구현 누락 (placeholder)
- [ ] TaskManagerInit 주석 처리 (Task 등록 로직)
- [ ] TaskStatus에 PAUSED 상태 누락
- [ ] 테스트 코드 오류 (TaskStatus::CANCELLING, 팩토리 시그니처)

---

## 🎯 Phase 1: 핵심 아키텍처 수정 (P0 - 필수)

**목표**: 아키텍처 불일치 해결 및 빌드 안정화

### Step 1.1: TaskDefinitionRegistry 팩토리 시그니처 수정
**파일**: `src/core/taskmanager/TaskDefinitionRegistry.h/cpp`

**Before**:
```cpp
using TaskFactoryFunc = std::function<std::shared_ptr<ITask>()>;
```

**After**:
```cpp
using TaskFactoryFunc = std::function<std::shared_ptr<ITask>(
    const std::string& id,
    const std::string& type,
    const std::map<std::string, std::string>& params
)>;

void registerDefinition(const std::string& taskName, TaskFactoryFunc factory);

std::shared_ptr<ITask> createTask(
    const std::string& taskName,
    const std::string& id,
    const std::string& type,
    const std::map<std::string, std::string>& params
);
```

**의존성**:
- Task 생성자도 이에 맞게 수정 필요
- 모든 concrete task 클래스 생성자 수정

---

### Step 1.2: TaskStatus enum에 PAUSED 추가
**파일**: `src/core/taskmanager/TaskDto.h`

**수정**:
```cpp
enum class TaskStatus {
    PENDING,
    RUNNING,
    PAUSED,      // 추가
    COMPLETED,
    FAILED,
    CANCELLED
};

// Helper 함수도 업데이트
inline std::string taskStatusToString(TaskStatus status) {
    // ... PAUSED case 추가
}
```

---

### Step 1.3: Task 추상 클래스 완성
**파일**: `src/core/taskmanager/Task.h/cpp`

**추가할 멤버 변수**:
```cpp
protected:
    std::string id_;
    std::string name_;
    std::string type_;      // 추가
    TaskStatus status_;
    float progress_;
    std::map<std::string, std::string> parameters_;  // 추가
```

**구현할 메서드**:
```cpp
// Task.h
void pause() override;  // = 0 제거
std::string getType() const override;  // 추가
std::map<std::string, std::string> getParameters() const override;  // 추가

// Task.cpp
void Task::pause() {
    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::PAUSED;
    }
}

std::string Task::getType() const {
    return type_;
}

std::map<std::string, std::string> Task::getParameters() const {
    return parameters_;
}
```

**생성자 수정**:
```cpp
Task::Task(std::string id, std::string name, std::string type,
           std::map<std::string, std::string> parameters)
    : id_(std::move(id)),
      name_(std::move(name)),
      type_(std::move(type)),
      status_(TaskStatus::PENDING),
      progress_(0.0f),
      parameters_(std::move(parameters)) {}
```

---

### Step 1.4: Concrete Task 클래스 업데이트
**파일**: `src/core/taskmanager/tasks/*.h/cpp`

**각 Task 클래스 (DriveForwardTask, DummyTask, etc.)**:

**헤더 수정**:
```cpp
class DriveForwardTask : public Task {
public:
    DriveForwardTask(const std::string& id, const std::string& type,
                     const std::map<std::string, std::string>& params);

    void execute() override;
    void cancel() override;
    // pause(), getType(), getParameters()는 Task 기본 구현 사용
};
```

**구현 파일 생성/수정**:
```cpp
DriveForwardTask::DriveForwardTask(
    const std::string& id,
    const std::string& type,
    const std::map<std::string, std::string>& params)
    : Task(id, "DriveForwardTask", type, params) {}

void DriveForwardTask::execute() {
    status_ = TaskStatus::RUNNING;

    // 실제 작업 수행
    float distance = std::stof(parameters_["distance"]);
    float speed = std::stof(parameters_["speed"]);

    // 시뮬레이션
    for (int i = 0; i <= 100; i += 10) {
        if (status_ == TaskStatus::CANCELLED || status_ == TaskStatus::PAUSED) {
            break;
        }
        progress_ = i / 100.0f;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (status_ == TaskStatus::RUNNING) {
        status_ = TaskStatus::COMPLETED;
        progress_ = 1.0f;
    }
}

void DriveForwardTask::cancel() {
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PAUSED) {
        status_ = TaskStatus::CANCELLED;
    }
}
```

**모든 Task에 적용**:
- [ ] DriveForwardTask
- [ ] DummyTask
- [ ] LiftPalletTask
- [ ] InspectAreaTask
- [ ] FailureTypeTask

---

## 🎯 Phase 2: TaskManager 단순화 (P1 - 중요)

**목표**: TaskManager의 책임을 Registry와 Executor에 완전히 위임

### Step 2.1: TaskManager에서 tasks_ 제거
**파일**: `src/core/taskmanager/TaskManager.h/cpp`

**Before**:
```cpp
private:
    std::shared_ptr<TaskDefinitionRegistry> registry_;
    std::shared_ptr<TaskExecutor> executor_;
    std::vector<TaskDto> tasks_;  // 제거!
```

**After**:
```cpp
private:
    std::shared_ptr<TaskDefinitionRegistry> registry_;
    std::shared_ptr<TaskExecutor> executor_;
    // tasks_ 제거
```

---

### Step 2.2: TaskManager 메서드 리팩터링
**파일**: `src/core/taskmanager/TaskManager.cpp`

**registerTaskDefinition 수정**:
```cpp
std::string TaskManager::registerTaskDefinition(
    const std::string& taskName,
    const std::string& taskType,
    const std::map<std::string, std::string>& defaultParameters) {

    // 단순히 Registry에 위임
    // Registry가 Task 메타데이터를 관리
    // TaskManager는 ID 생성만 담당

    static int taskIdCounter = 0;
    std::string taskId = "task_" + std::to_string(++taskIdCounter);

    // 나중에 Task를 생성할 때 사용할 정보를 저장
    // (Registry에 별도 저장 로직 필요)

    return taskId;
}
```

**getAllTaskDefinitions 수정**:
```cpp
std::vector<TaskDto> TaskManager::getAllTaskDefinitions() const {
    // Registry에서 가져오기
    return registry_->getAllDefinitions();
}
```

---

### Step 2.3: TaskManager에 getter 추가
**파일**: `src/core/taskmanager/TaskManager.h`

```cpp
public:
    // ... 기존 메서드들 ...

    // Registry와 Executor 접근을 위한 getter
    std::shared_ptr<TaskDefinitionRegistry> getRegistry() const { return registry_; }
    std::shared_ptr<TaskExecutor> getExecutor() const { return executor_; }
```

---

## 🎯 Phase 3: StartTaskCommand 실제 구현 (P1 - 중요)

**목표**: Task 생성 및 실행 로직 완성

### Step 3.1: StartTaskCommand 구현
**파일**: `src/core/taskmanager/commands/StartTaskCommand.cpp`

```cpp
void StartTaskCommand::execute() {
    // 1. TaskManager에서 Task 정의 가져오기
    auto taskDto = taskManager_.getTaskDefinitionById(taskId_);
    if (!taskDto) {
        throw std::runtime_error("Task definition not found: " + taskId_);
    }

    // 2. Runtime parameters와 default parameters 병합
    std::map<std::string, std::string> mergedParams = taskDto->parameters;
    for (const auto& [key, value] : runtimeParameters_) {
        mergedParams[key] = value;  // runtime이 우선
    }

    // 3. Registry를 통해 실제 ITask 객체 생성
    auto task = taskManager_.getRegistry()->createTask(
        taskDto->type,      // task type name (e.g., "DriveForward")
        taskDto->id,        // task instance id
        taskDto->type,      // task type again
        mergedParams        // merged parameters
    );

    if (!task) {
        throw std::runtime_error("Failed to create task: " + taskDto->type);
    }

    // 4. Executor에 제출
    taskManager_.getExecutor()->submit(task);

    std::cout << "StartTaskCommand: Task " << taskId_
              << " created and submitted." << std::endl;
}
```

---

## 🎯 Phase 4: TaskDefinitionRegistry 구조 개선 (P1)

**목표**: Task 메타데이터 관리 기능 추가

### Step 4.1: TaskDefinitionRegistry에 메타데이터 저장 추가
**파일**: `src/core/taskmanager/TaskDefinitionRegistry.h`

```cpp
class TaskDefinitionRegistry {
public:
    using TaskFactoryFunc = std::function<std::shared_ptr<ITask>(
        const std::string& id,
        const std::string& type,
        const std::map<std::string, std::string>& params
    )>;

    struct TaskDefinition {
        std::string typeName;  // e.g., "DriveForward"
        TaskFactoryFunc factory;
        std::map<std::string, std::string> defaultParams;
    };

    void registerDefinition(const std::string& taskTypeName,
                           TaskFactoryFunc factory,
                           const std::map<std::string, std::string>& defaultParams = {});

    std::shared_ptr<ITask> createTask(
        const std::string& taskTypeName,
        const std::string& id,
        const std::string& type,
        const std::map<std::string, std::string>& params);

    std::vector<TaskDto> getAllDefinitions() const;  // 추가
    const TaskDefinition* getDefinition(const std::string& typeName) const;  // 추가

private:
    std::unordered_map<std::string, TaskDefinition> definitions_;
};
```

---

## 🎯 Phase 5: TaskManagerInit 완성 (P1)

**목표**: 모든 Task 타입 등록

### Step 5.1: initializeTaskManagerModule 구현
**파일**: `src/core/taskmanager/TaskManagerInit.cpp`

```cpp
namespace mxrc::core::taskmanager {

void initializeTaskManagerModule(TaskDefinitionRegistry& registry) {
    // DriveForwardTask 등록
    registry.registerDefinition(
        "DriveForward",
        [](const std::string& id, const std::string& type,
           const std::map<std::string, std::string>& params) {
            return std::make_shared<tasks::DriveForwardTask>(id, type, params);
        },
        {{"speed", "1.0"}, {"distance", "10.0"}}  // default params
    );

    // DummyTask 등록
    registry.registerDefinition(
        "DummyTask",
        [](const std::string& id, const std::string& type,
           const std::map<std::string, std::string>& params) {
            return std::make_shared<tasks::DummyTask>(id, type, params);
        },
        {{"message", "Hello"}}
    );

    // LiftPalletTask 등록
    registry.registerDefinition(
        "LiftPallet",
        [](const std::string& id, const std::string& type,
           const std::map<std::string, std::string>& params) {
            return std::make_shared<tasks::LiftPalletTask>(id, type, params);
        },
        {{"height", "0.5"}, {"payload", "pallet"}}
    );

    // InspectAreaTask 등록
    registry.registerDefinition(
        "InspectArea",
        [](const std::string& id, const std::string& type,
           const std::map<std::string, std::string>& params) {
            return std::make_shared<tasks::InspectAreaTask>(id, type, params);
        },
        {{"area", "zone1"}, {"camera", "front"}}
    );

    // FailureTypeTask 등록
    registry.registerDefinition(
        "FailureType",
        [](const std::string& id, const std::string& type,
           const std::map<std::string, std::string>& params) {
            return std::make_shared<tasks::FailureTypeTask>(id, type, params);
        }
    );

    std::cout << "TaskDefinitionRegistry initialized with all task types." << std::endl;
}

} // namespace mxrc::core::taskmanager
```

**헤더 수정**:
```cpp
void initializeTaskManagerModule(TaskDefinitionRegistry& registry);
```

**main.cpp 수정**:
```cpp
auto registry = std::make_shared<TaskDefinitionRegistry>();
initializeTaskManagerModule(*registry);  // 초기화
auto executor = std::make_shared<TaskExecutor>();
auto taskManager = std::make_shared<TaskManager>(registry, executor);
```

---

## 🎯 Phase 6: 테스트 코드 수정 (P2)

**목표**: 모든 테스트가 통과하도록 수정

### Step 6.1: TaskManager_test.cpp 수정
**파일**: `tests/unit/task/TaskManager_test.cpp`

**수정 사항**:
1. `TaskStatus::CANCELLING` → `TaskStatus::CANCELLED`
2. 팩토리 함수 시그니처 수정 (3개 매개변수)
3. Mock 클래스에 누락된 메서드 구현
4. Registry 초기화 로직 추가

### Step 6.2: OperatorInterface_test.cpp 수정
**파일**: `tests/unit/task/OperatorInterface_test.cpp`

**수정 사항**:
1. Command 생성 시 ITaskManager& 전달
2. Registry 초기화 로직 추가

### Step 6.3: TaskDefinitionRegistry_test.cpp 작성
**파일**: `tests/unit/task/TaskDefinitionRegistry_test.cpp`

**테스트 케이스**:
- Task 등록 성공
- 중복 등록 실패
- Task 생성 성공
- 존재하지 않는 타입 생성 실패

### Step 6.4: TaskExecutor_test.cpp 작성
**파일**: `tests/unit/task/TaskExecutor_test.cpp`

**테스트 케이스**:
- Task 제출 및 실행
- Task 취소
- 여러 Task 동시 실행
- 완료된 Task 조회

---

## 📅 실행 일정 및 체크리스트

### Week 1: Phase 1 - 핵심 아키텍처 수정
- [ ] Step 1.1: TaskDefinitionRegistry 팩토리 시그니처 수정
- [ ] Step 1.2: TaskStatus에 PAUSED 추가
- [ ] Step 1.3: Task 추상 클래스 완성
- [ ] Step 1.4: Concrete Task 클래스 업데이트
  - [ ] DriveForwardTask
  - [ ] DummyTask
  - [ ] LiftPalletTask
  - [ ] InspectAreaTask
  - [ ] FailureTypeTask
- [ ] 빌드 테스트

### Week 2: Phase 2-3 - TaskManager 단순화 및 Command 구현
- [ ] Step 2.1: TaskManager tasks_ 제거
- [ ] Step 2.2: TaskManager 메서드 리팩터링
- [ ] Step 2.3: TaskManager getter 추가
- [ ] Step 3.1: StartTaskCommand 실제 구현
- [ ] 빌드 및 실행 테스트

### Week 3: Phase 4-5 - Registry 개선 및 초기화
- [ ] Step 4.1: TaskDefinitionRegistry 메타데이터 저장
- [ ] Step 5.1: TaskManagerInit 완성
- [ ] main.cpp 업데이트
- [ ] 통합 테스트

### Week 4: Phase 6 - 테스트 코드 수정
- [ ] Step 6.1: TaskManager_test.cpp 수정
- [ ] Step 6.2: OperatorInterface_test.cpp 수정
- [ ] Step 6.3: TaskDefinitionRegistry_test.cpp 작성
- [ ] Step 6.4: TaskExecutor_test.cpp 작성
- [ ] 전체 테스트 실행 및 검증

---

## 🎯 성공 기준

### 빌드 및 실행
- [x] 메인 실행파일 빌드 성공
- [ ] 테스트 실행파일 빌드 성공
- [ ] 모든 테스트 통과
- [ ] main.cpp 실행 시 모든 시나리오 동작

### 아키텍처 품질
- [ ] TaskManager LOC 30% 감소 (SC-001)
- [ ] 새 Task 추가 시 TaskManager 수정 불필요 (SC-002)
- [ ] ITaskManager에서 updateTaskStatus 제거 (SC-003)
- [ ] 모든 Task 조작이 Command 패턴으로 수행 (SC-004)

### 코드 품질
- [ ] Doxygen 주석 완료 (T031)
- [ ] README.md 업데이트 (T032)
- [ ] 정적 분석 통과 (T033)

---

## 📝 참고 문서

- [spec.md](./spec.md) - 기능 사양서
- [plan.md](./plan.md) - 구현 계획
- [tasks.md](./tasks.md) - 작업 목록
- [data-model.md](./data-model.md) - 데이터 모델

---

## 🔄 진행 상황 추적

**현재 Phase**: Phase 1 준비
**다음 작업**: Step 1.1 (TaskDefinitionRegistry 팩토리 시그니처 수정)
**예상 완료일**: 4주 후

---

**작성자**: Claude Code
**최종 업데이트**: 2025-11-14
