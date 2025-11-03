#include "ExecuteTaskNode.h"
#include <iostream>

namespace mxrc {
namespace task_mission {

BT::NodeStatus ExecuteTaskNode::tick() {
    // Get inputs
    BT::Optional<std::string> task_id = getInput<std::string>("task_id");
    BT::Optional<int> priority = getInput<int>("priority");
    // BT::Optional<TaskContext> context = getInput<TaskContext>("context"); // Not directly supported by BT.CPP ports

    if (!task_id) {
        throw BT::RuntimeError("Missing parameter [task_id] in ExecuteTaskNode");
    }

    // Write current task ID to output port
    setOutput("current_task_instance_id", task_id.value());

    // Create task using TaskFactory
    std::unique_ptr<AbstractTask> task = TaskFactory::getInstance().createTask(task_id.value());
    if (!task) {
        std::cerr << "Failed to create task: " << task_id.value() << std::endl;
        return BT::NodeStatus::FAILURE;
    }

    // Add task to TaskScheduler
    // MissionManager::getInstance().getTaskScheduler().addTask(std::move(task), priority.value_or(0));
    // For now, directly execute for simplicity, will integrate with TaskScheduler later
    TaskContext context; // Placeholder context
    task->initialize(context);
    bool success = task->execute(context);
    task->terminate(context);

    if (success) {
        return BT::NodeStatus::SUCCESS;
    } else {
        return BT::NodeStatus::FAILURE;
    }
}

} // namespace task_mission
} // namespace mxrc
