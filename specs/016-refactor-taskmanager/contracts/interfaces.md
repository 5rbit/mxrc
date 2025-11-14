# C++ 인터페이스 계약: TaskManager 리팩터링

**날짜**: 2025-11-13
**사양서**: [spec.md](./spec.md)

## 개요

이 문서는 `TaskManager` 모듈 리팩터링에 따른 C++ 헤더 파일의 인터페이스 계약을 정의합니다. 이는 외부 모듈과의 상호작용 및 모듈 내부의 핵심 컴포넌트 간의 상호작용을 명확히 하기 위함입니다.

---

## 1. `ITaskManager.h` (수정)

`ITaskManager`는 이제 Command 객체를 통해 모든 Task 관련 작업을 처리하는 단일 진입점을 제공합니다. 상태를 직접 업데이트하는 메서드들은 제거됩니다.

```cpp
#pragma once

#include <memory>

// Forward declarations
class ICommand;

namespace mxrc {
namespace core {
namespace taskmanager {

/**
 * @interface ITaskManager
 * @brief Task 실행 및 관리를 위한 최상위 인터페이스.
 * 모든 Task 관련 요청은 Command Pattern을 통해 이 인터페이스로 전달됩니다.
 */
class ITaskManager {
public:
    virtual ~ITaskManager() = default;

    /**
     * @brief Command를 실행하여 Task를 제어합니다.
     * @param command 실행할 Command 객체 (예: StartTaskCommand, CancelTaskCommand).
     */
    virtual void executeCommand(std::shared_ptr<ICommand> command) = 0;

    // 참고: getTaskById, getAllTasks와 같은 조회용 메서드는 필요에 따라 추가될 수 있으나,
    // 상태 변경 관련 메서드(updateStatus, updateProgress 등)는 포함되지 않습니다.
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
```

---

## 2. `ITask.h` (수정)

`ITask`는 이제 데이터와 행위를 모두 포함하는 인터페이스입니다. Task의 상태는 내부적으로 관리됩니다.

```cpp
#pragma once

#include <string>
#include "TaskDto.h" // TaskStatus, TaskDto 등 포함

namespace mxrc {
namespace core {
namespace taskmanager {

/**
 * @interface ITask
 * @brief 실행 가능한 Task의 기본 인터페이스. 데이터와 행위를 모두 포함합니다.
 */
class ITask {
public:
    virtual ~ITask() = default;

    /**
     * @brief Task의 주 실행 로직.
     * 이 메서드 내에서 Task의 상태가 RUNNING, COMPLETED, FAILED 등으로 변경됩니다.
     */
    virtual void execute() = 0;

    /**
     * @brief Task 실행을 취소합니다.
     */
    virtual void cancel() = 0;

    /**
     * @brief 현재 Task의 상태를 반환합니다.
     * @return TaskStatus 현재 상태.
     */
    virtual TaskStatus getStatus() const = 0;

    /**
     * @brief 현재 Task의 진행률을 반환합니다.
     * @return float 진행률 (0.0 ~ 1.0).
     */
    virtual float getProgress() const = 0;

    /**
     * @brief Task의 고유 ID를 반환합니다.
     * @return const std::string& 고유 ID.
     */
    virtual const std::string& getId() const = 0;

    /**
     * @brief Task의 정보를 담은 DTO를 생성하여 반환합니다.
     * @return TaskDto Task 데이터 전송 객체.
     */
    virtual TaskDto toDto() const = 0;
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
```

---

## 3. `ICommand.h` (기존/유지)

모든 Command 객체가 구현해야 하는 공통 인터페이스입니다.

```cpp
#pragma once

namespace mxrc {
namespace core {
namespace taskmanager {

/**
 * @interface ICommand
 * @brief 실행 가능한 모든 Command의 기본 인터페이스.
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Command를 실행합니다.
     */
    virtual void execute() = 0;
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
```

---

## 4. 새로운 주요 클래스 인터페이스 (개념)

아래는 새로 추가될 `TaskDefinitionRegistry`와 `TaskExecutor`의 개념적 인터페이스입니다. 이들은 `taskmanager` 모듈 내부적으로 사용될 수 있습니다.

### `TaskDefinitionRegistry.h` (신규)

```cpp
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declarations
class ITask;

namespace mxrc {
namespace core {
namespace taskmanager {

class TaskDefinitionRegistry {
public:
    using TaskFactoryFunc = std::function<std::shared_ptr<ITask>()>;

    void registerDefinition(const std::string& taskName, TaskFactoryFunc factory);
    std::shared_ptr<ITask> createTask(const std::string& taskName);

private:
    std::unordered_map<std::string, TaskFactoryFunc> definitions_;
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
```

### `TaskExecutor.h` (신규)

```cpp
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

// Forward declarations
class ITask;

namespace mxrc {
namespace core {
namespace taskmanager {

class TaskExecutor {
public:
    TaskExecutor();
    ~TaskExecutor();

    void submit(std::shared_ptr<ITask> task);
    void cancel(const std::string& taskId);

private:
    // 스레드 풀, 실행 중인 Task 목록 등 내부 관리용 멤버
    void workerLoop();
    
    std::vector<std::thread> threadPool_;
    // ... 기타 동기화 및 Task 관리용 멤버
};

} // namespace taskmanager
} // namespace core
} // namespace mxrc
```
