# 빠른 시작 가이드: Task & Mission Management (Task 및 임무 관리) 고도화

**기능 브랜치**: `004-task-mission-management`
**생성일**: 2025-11-03
**사양서**: [specs/004-task-mission-management/spec.md](specs/004-task-mission-management/spec.md)
**계획**: [specs/004-task-mission-management/plan.md](specs/004-task-mission-management/plan.md)

이 가이드는 Task 및 Mission Management 시스템을 빠르게 시작하고 사용하는 방법을 설명합니다.

## 1. Mission 정의 파일 생성

Mission은 Behavior Tree 구조를 따르는 JSON 또는 YAML 파일로 정의됩니다. 다음은 간단한 Mission 정의 예시입니다.

**`my_first_mission.json` 예시:**

```json
{
  "id": "my_first_mission",
  "name": "My First Robot Mission",
  "version": "1.0.0",
  "description": "로봇이 특정 위치로 이동 후 팔레트를 들어 올리는 미션",
  "behavior_tree": {
    "root": "sequence_main",
    "nodes": [
      {
        "id": "sequence_main",
        "type": "Sequence",
        "children": ["action_drive_to_pos", "action_lift_pallet"]
      },
      {
        "id": "action_drive_to_pos",
        "type": "Action",
        "task_id": "DriveToPosition",
        "parameters": {
          "target_x": 1.0,
          "target_y": 2.0,
          "speed": 0.5
        },
        "failure_strategy": "RETRY_TRANSIENT"
      },
      {
        "id": "action_lift_pallet",
        "type": "Action",
        "task_id": "LiftPallet",
        "parameters": {
          "pallet_id": "PALLET_001"
        },
        "failure_strategy": "ABORT_MISSION"
      }
    ]
  }
}
```

- `id`: Mission의 고유 식별자입니다.
- `behavior_tree.root`: Behavior Tree의 시작 노드 ID입니다.
- `behavior_tree.nodes`: Behavior Tree를 구성하는 노드들의 배열입니다.
  - `type`: 노드의 종류 (예: `Sequence`, `Selector`, `Parallel`, `Action`, `Condition`).
  - `task_id`: `Action` 노드인 경우, 실행할 Task의 ID입니다. `TaskFactory`에 등록된 Task여야 합니다.
  - `parameters`: Task 실행에 필요한 입력 파라미터입니다.
  - `failure_strategy`: 해당 Task 실패 시 Mission의 동작을 정의합니다.

## 2. 커스텀 Task 생성

새로운 Task를 생성하려면 `AbstractTask` 인터페이스를 상속받아 `initialize`, `execute`, `terminate`, `getTaskId` 메서드를 구현해야 합니다.

**`MyCustomTask.h` 예시:**

```cpp
#ifndef MY_CUSTOM_TASK_H
#define MY_CUSTOM_TASK_H

#include "../../src/core/task/AbstractTask.h"
#include "../../src/core/task/TaskContext.h"
#include <iostream>

namespace mxrc {
namespace task {

class MyCustomTask : public AbstractTask {
public:
    bool initialize(TaskContext& context) override {
        std::cout << "MyCustomTask initialized." << std::endl;
        // 필요한 초기화 로직
        return true;
    }

    bool execute(TaskContext& context) override {
        std::cout << "MyCustomTask executing." << std::endl;
        // Task의 핵심 로직
        // 예: context.setParameter<bool>("result", true);
        return true;
    }

    void terminate(TaskContext& context) override {
        std::cout << "MyCustomTask terminated." << std::endl;
        // 필요한 정리 로직
    }

    std::string getTaskId() const override {
        return "MyCustomTask";
    }
};

} // namespace task
} // namespace mxrc

#endif // MY_CUSTOM_TASK_H
```

**`MyCustomTask.cpp` 예시 (TaskFactory에 등록):**

```cpp
#include "MyCustomTask.h"
#include "../../src/core/task/TaskFactory.h"

namespace mxrc {
namespace task {

// TaskFactory에 MyCustomTask 등록
static bool registered = TaskFactory().registerTask("MyCustomTask", []() {
    return std::make_unique<MyCustomTask>();
});

} // namespace task
} // namespace mxrc
```

## 3. Mission 실행

`MissionManager`를 사용하여 Mission 정의 파일을 로드하고 실행합니다.

```cpp
#include "../../src/core/task/MissionManager.h" // MissionManager 구현체 포함
#include "../../src/core/task/TaskContext.h"
#include <iostream>

int main() {
    mxrc::task::MissionManager missionManager; // 실제 구현체 사용
    mxrc::task::TaskContext initialContext;

    if (missionManager.loadMissionDefinition("my_first_mission.json")) {
        std::string missionInstanceId = missionManager.startMission("my_first_mission", initialContext);
        if (!missionInstanceId.empty()) {
            std::cout << "Mission started with instance ID: " << missionInstanceId << std::endl;

            // Mission 상태 모니터링 (예시)
            while (true) {
                mxrc::task::MissionState state = missionManager.getMissionState(missionInstanceId);
                std::cout << "Mission Status: " << static_cast<int>(state.current_status) << ", Progress: " << state.progress << std::endl;
                if (state.current_status == mxrc::task::MissionState::Status::COMPLETED ||
                    state.current_status == mxrc::task::MissionState::Status::FAILED ||
                    state.current_status == mxrc::task::MissionState::Status::CANCELLED) {
                    break;
                }
                // 실제 시스템에서는 비동기적으로 상태를 폴링하거나 이벤트 기반으로 처리
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            std::cout << "Failed to start mission." << std::endl;
        }
    } else {
        std::cout << "Failed to load mission definition." << std::endl;
    }

    return 0;
}
```

## 4. Mission 및 Task 모니터링/제어

`OperatorInterface`를 통해 Mission 및 Task의 상태를 모니터링하고 제어 명령을 내릴 수 있습니다.

```cpp
#include "../../src/core/task/OperatorInterface.h" // OperatorInterface 구현체 포함
#include <iostream>

// ... (Mission 실행 코드 이후)

    mxrc::task::OperatorInterface opInterface; // 실제 구현체 사용

    // Mission 일시 중지
    if (opInterface.requestPauseMission(missionInstanceId)) {
        std::cout << "Mission paused." << std::endl;
    }

    // Mission 재개
    if (opInterface.requestResumeMission(missionInstanceId)) {
        std::cout << "Mission resumed." << std::endl;
    }

    // 특정 Task 건너뛰기
    // if (opInterface.requestSkipCurrentTask(missionInstanceId)) {
    //     std::cout << "Current task skipped." << std::endl;
    // }

    // Task 상태 이력 조회
    // std::vector<mxrc::task::TaskStateHistory> history = opInterface.getTaskHistory("some_task_instance_id");
    // for (const auto& entry : history) {
    //     std::cout << "Task State Change: " << static_cast<int>(entry.old_state) << " -> " << static_cast<int>(entry.new_state) << std::endl;
    // }

// ...
```

이 가이드를 통해 Task 및 Mission Management 시스템의 기본적인 사용법을 이해하고, 자신만의 Task와 Mission을 정의하여 로봇을 제어할 수 있습니다.