#include <gtest/gtest.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/loggers/bt_cout_logger.h>
#include "core/task/task_mission_management/MissionManager.h"
#include "core/task/task_mission_management/AbstractTask.h"
#include "core/task/task_mission_management/TaskFactory.h"
#include "core/task/task_mission_management/DriveToPositionTask.h" // For registering DriveToPosition

using namespace BT;
using namespace mxrc::task_mission;

// Define a simple custom action for testing purposes
class TestAction : public SyncActionNode
{
public:
    TestAction(const std::string& name, const NodeConfig& config)
        : SyncActionNode(name, config)
    {
        std::cout << "TestAction: " << name << " initialized." << std::endl;
    }

    static PortsList providedPorts()
    {
        return { InputPort<std::string>("message") };
    }

    NodeStatus tick() override
    {
        Optional<std::string> msg = getInput<std::string>("message");
        if (msg)
        {
            std::cout << "TestAction: " << name() << " received message: " << msg.value() << std::endl;
        }
        else
        {
            std::cout << "TestAction: " << name() << " ticked." << std::endl;
        }
        return NodeStatus::SUCCESS;
    }
};

// Test fixture for Behavior Tree
class BehaviorTreeTest : public ::testing::Test {
protected:
    BehaviorTreeFactory factory;

    void SetUp() override {
        // Register custom nodes
        factory.registerNodeType<TestAction>("TestAction");
        // Register tasks from TaskFactory
        TaskFactory::getInstance().registerAllTasks(factory); // Assuming TaskFactory has a method to register all its tasks
    }
};

TEST_F(BehaviorTreeTest, LoadAndExecuteSimpleTree) {
    // Define a simple XML for a Behavior Tree
    const std::string xml_tree = R"(
        <root BTCPP_format="4">
            <BehaviorTree>
                <Sequence name="root_sequence">
                    <TestAction name="action_1" message="Hello"/>
                    <TestAction name="action_2" message="World"/>
                </Sequence>
            </BehaviorTree>
        </root>
    )";

    // Create a tree from the XML
    auto tree = factory.createTreeFromText(xml_tree);

    // Execute the tree
    NodeStatus status = tree.tickWhileRunning();
    ASSERT_EQ(status, NodeStatus::SUCCESS);
}

TEST_F(BehaviorTreeTest, LoadAndExecuteMissionXML) {
    // This test assumes simple_mission.xml exists and is a valid BT XML
    // and that DriveToPositionTask is registered with the BTFactory via TaskFactory
    const std::string mission_xml_path = "/Users/tory/workspace/mxrc/missions/simple_mission.xml";

    // Create a tree from the XML file
    auto tree = factory.createTreeFromFile(mission_xml_path);

    // Execute the tree
    NodeStatus status = tree.tickWhileRunning();
    ASSERT_EQ(status, NodeStatus::SUCCESS);
}
