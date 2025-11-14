# 퀵스타트: 리팩터링된 TaskManager 사용하기

**날짜**: 2025-11-13
**사양서**: [spec.md](./spec.md)

## 개요

이 문서는 리팩터링된 `TaskManager` 아키텍처를 사용하여 새로운 Task를 정의하고, 등록하며, 실행하는 방법을 안내합니다.

## 1단계: 새로운 Task 클래스 정의

`ITask`를 상속받아 새로운 Task 클래스를 만듭니다. `Task` 추상 클래스를 상속하면 일부 기능(ID 생성, 기본 getter 등)을 재사용할 수 있습니다.

**`src/core/taskmanager/tasks/MyNewTask.h`**
```cpp
#pragma once

#include "core/taskmanager/Task.h"

namespace mxrc::core::taskmanager::tasks {

class MyNewTask : public Task {
public:
    MyNewTask();
    ~MyNewTask() override = default;

    void execute() override;
    void cancel() override;
};

} // namespace mxrc::core::taskmanager::tasks
```

**`src/core/taskmanager/tasks/MyNewTask.cpp`**
```cpp
#include "MyNewTask.h"
#include <iostream>
#include <thread>

namespace mxrc::core::taskmanager::tasks {

MyNewTask::MyNewTask() : Task("MyNewTask") {}

void MyNewTask::execute() {
    // 1. 상태를 RUNNING으로 변경
    status_ = TaskStatus::RUNNING;
    progress_ = 0.0f;
    std::cout << "MyNewTask [" << id_ << "] is starting." << std::endl;

    // 2. 실제 작업 수행
    for (int i = 0; i <= 100; ++i) {
        if (status_ == TaskStatus::CANCELLING) {
            status_ = TaskStatus::CANCELLED;
            std::cout << "MyNewTask [" << id_ << "] was cancelled." << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        progress_ = static_cast<float>(i) / 100.0f;
    }

    // 3. 상태를 COMPLETED로 변경
    progress_ = 1.0f;
    status_ = TaskStatus::COMPLETED;
    std::cout << "MyNewTask [" << id_ << "] has completed." << std::endl;
}

void MyNewTask::cancel() {
    // 취소 요청 플래그 설정
    if (status_ == TaskStatus::RUNNING || status_ == TaskStatus::PENDING) {
        status_ = TaskStatus::CANCELLING;
    }
}

} // namespace mxrc::core::taskmanager::tasks
```

## 2단계: Task Factory에 새로운 Task 등록

시스템 초기화 과정에서, 새로 정의한 Task를 `TaskDefinitionRegistry`에 등록합니다. 이는 일반적으로 `TaskManagerInit`와 같은 설정 클래스에서 수행됩니다.

**`src/core/taskmanager/TaskManagerInit.cpp` (수정)**
```cpp
#include "TaskManagerInit.h"
#include "core/taskmanager/TaskDefinitionRegistry.h"
#include "core/taskmanager/tasks/MyNewTask.h"
#include "core/taskmanager/tasks/DriveForwardTask.h"
// ... 다른 Task들

namespace mxrc::core::taskmanager {

void TaskManagerInit::registerAllTasks(TaskDefinitionRegistry& registry) {
    // 기존 Task 등록
    registry.registerDefinition("DriveForward", [] { return std::make_shared<tasks::DriveForwardTask>(); });
    // ...

    // 새로운 Task 등록
    registry.registerDefinition("MyNewTask", [] { return std::make_shared<tasks::MyNewTask>(); });
}

} // namespace mxrc::core::taskmanager
```

## 3단계: Command를 사용하여 Task 실행 요청

외부(예: `OperatorInterface`)에서는 `StartTaskCommand`를 생성하여 `ITaskManager`에 전달함으로써 Task 실행을 요청합니다.

**`src/core/taskmanager/operator_interface/OperatorInterface.cpp` (수정)**
```cpp
#include "OperatorInterface.h"
#include "core/taskmanager/commands/StartTaskCommand.h"
#include "core/taskmanager/commands/CancelTaskCommand.h"

namespace mxrc::core::taskmanager {

// ...

void OperatorInterface::startNewTask(const std::string& taskName) {
    if (taskManager_) {
        auto command = std::make_shared<StartTaskCommand>(taskName, /* task params */);
        taskManager_->executeCommand(command);
    }
}

void OperatorInterface::cancelRunningTask(const std::string& taskId) {
    if (taskManager_) {
        auto command = std::make_shared<CancelTaskCommand>(taskId);
        taskManager_->executeCommand(command);
    }
}

// ...

} // namespace mxrc::core::taskmanager
```

## 실행 흐름 요약

1.  **초기화**: `TaskManagerInit`가 `TaskDefinitionRegistry`에 `MyNewTask`를 등록합니다.
2.  **요청**: `OperatorInterface`가 `startNewTask("MyNewTask")`를 호출합니다.
3.  **Command 생성**: `StartTaskCommand` 객체가 생성됩니다.
4.  **Command 전달**: `OperatorInterface`가 `ITaskManager::executeCommand()`를 통해 Command를 `TaskManager`에 전달합니다.
5.  **처리 (TaskManager)**:
    a. `TaskManager`가 `StartTaskCommand`를 수신합니다.
    b. `TaskDefinitionRegistry::createTask("MyNewTask")`를 호출하여 `MyNewTask` 인스턴스를 생성합니다.
    c. 생성된 `ITask` 객체를 `TaskExecutor::submit()`에 전달합니다.
6.  **실행 (TaskExecutor)**:
    a. `TaskExecutor`가 스레드 풀의 워커 스레드에 `ITask::execute()` 메서드 실행을 할당합니다.
    b. `MyNewTask::execute()`가 호출되어 작업이 수행되고 내부적으로 상태가 변경됩니다.
