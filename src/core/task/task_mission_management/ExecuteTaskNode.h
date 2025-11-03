#ifndef EXECUTE_TASK_NODE_H
#define EXECUTE_TASK_NODE_H

#include <behaviortree_cpp/action_node.h>
#include "TaskFactory.h"
#include "TaskContext.h"
#include "TaskScheduler.h"
#include "AbstractTask.h"

namespace mxrc {
namespace task_mission {

// Custom Action Node to execute an AbstractTask
class ExecuteTaskNode : public BT::SyncActionNode
{
public:
    ExecuteTaskNode(const std::string& name, const BT::NodeConfig& config)
        : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<std::string>("task_id"),
                 BT::InputPort<int>("priority", 0, "Task priority"),
                 BT::InputPort<TaskContext>("context", "Task context"),
                 BT::OutputPort<std::string>("current_task_instance_id") }; // Output port to write current task ID
    }

    BT::NodeStatus tick() override;
};

} // namespace task_mission
} // namespace mxrc

#endif // EXECUTE_TASK_NODE_H
