#include "commands/CancelTaskCommand.h"
#include "../interfaces/ITaskManager.h"
#include "../interfaces/ITask.h"
#include "../TaskManager.h"
#include "../TaskExecutor.h"
#include <iostream>

namespace mxrc::core::taskmanager {

CancelTaskCommand::CancelTaskCommand(ITaskManager& taskManager, std::string taskId)
    : taskManager_(taskManager), taskId_(std::move(taskId)) {}

void CancelTaskCommand::execute() {
    std::cout << "CancelTaskCommand: Cancelling task " << taskId_ << std::endl;

    // TaskManager를 TaskManager 타입으로 캐스팅하여 getExecutor() 접근
    auto* taskMgr = dynamic_cast<TaskManager*>(&taskManager_);
    if (!taskMgr) {
        std::cerr << "CancelTaskCommand: Failed to cast to TaskManager!" << std::endl;
        return;
    }

    auto executor = taskMgr->getExecutor();
    if (!executor) {
        std::cerr << "CancelTaskCommand: Executor is null!" << std::endl;
        return;
    }

    // Executor에서 Task 가져와서 cancel 호출
    auto task = executor->getTask(taskId_);
    if (task) {
        task->cancel();
    } else {
        std::cerr << "CancelTaskCommand: Task " << taskId_ << " not found in executor" << std::endl;
    }
}

} // namespace mxrc::core::taskmanager
