#include "commands/PauseTaskCommand.h"
#include "../interfaces/ITaskManager.h"
#include "../interfaces/ITask.h"
#include "../TaskManager.h"
#include "../TaskExecutor.h"
#include <iostream>

namespace mxrc::core::taskmanager {

PauseTaskCommand::PauseTaskCommand(ITaskManager& taskManager, std::string taskId)
    : taskManager_(taskManager), taskId_(std::move(taskId)) {}

void PauseTaskCommand::execute() {
    std::cout << "PauseTaskCommand: Pausing task " << taskId_ << std::endl;

    // TaskManager를 TaskManager 타입으로 캐스팅하여 getExecutor() 접근
    auto* taskMgr = dynamic_cast<TaskManager*>(&taskManager_);
    if (!taskMgr) {
        std::cerr << "PauseTaskCommand: Failed to cast to TaskManager!" << std::endl;
        return;
    }

    auto executor = taskMgr->getExecutor();
    if (!executor) {
        std::cerr << "PauseTaskCommand: Executor is null!" << std::endl;
        return;
    }

    // Executor에서 Task 가져와서 pause 호출
    auto task = executor->getTask(taskId_);
    if (task) {
        task->pause();
    } else {
        std::cerr << "PauseTaskCommand: Task " << taskId_ << " not found in executor" << std::endl;
    }
}

} // namespace mxrc::core::taskmanager
