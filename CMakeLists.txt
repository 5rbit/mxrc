cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED) 

# Add executable
add_executable(mxrc
    src/main.cpp
    src/core/taskmanager/Task.cpp
    src/core/taskmanager/TaskManager.cpp
    src/core/taskmanager/TaskDefinitionRegistry.cpp
    src/core/taskmanager/TaskExecutor.cpp
    src/core/taskmanager/operator_interface/OperatorInterface.cpp
    src/core/taskmanager/commands/StartTaskCommand.cpp
    src/core/taskmanager/commands/CancelTaskCommand.cpp
    src/core/taskmanager/commands/PauseTaskCommand.cpp
    src/core/taskmanager/TaskManagerInit.cpp
    src/core/sequence/core/ExecutionContext.cpp
    src/core/sequence/core/SequenceRegistry.cpp
    src/core/sequence/core/RetryHandler.cpp
    src/core/sequence/core/ActionExecutor.cpp
    src/core/sequence/core/ConditionEvaluator.cpp
    src/core/sequence/core/ExecutionMonitor.cpp
    src/core/sequence/core/SequenceEngine.cpp
)

# Link dependencies
target_link_libraries(mxrc PRIVATE 
    spdlog::spdlog 
    GTest::GTest 
    GTest::Main
)

# Include directories
target_include_directories(mxrc PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/operator_interface
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/commands
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/tasks
    ${PROJECT_SOURCE_DIR}/src/core/sequence
    ${PROJECT_SOURCE_DIR}/src/core/sequence/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/sequence/core
    ${PROJECT_SOURCE_DIR}/src/core/sequence/dto
    ${PROJECT_SOURCE_DIR}/src/core/sequence/integration
)

# Enable testing
enable_testing()

# Add tests
add_executable(run_tests
    tests/unit/task/TaskManager_test.cpp
    tests/unit/task/OperatorInterface_test.cpp
    tests/unit/task/TaskDefinitionRegistry_test.cpp
    tests/unit/task/TaskExecutor_test.cpp
    tests/unit/sequence/ExecutionContext_test.cpp
    tests/unit/sequence/SequenceEngine_test.cpp
    tests/unit/sequence/ActionExecutor_test.cpp
    tests/unit/sequence/SequenceTemplate_test.cpp
    tests/unit/sequence/ExecutionMonitoring_test.cpp
    tests/unit/sequence/EdgeCases_test.cpp
    tests/integration/sequence/SequenceIntegration_test.cpp
    src/core/taskmanager/Task.cpp
    src/core/taskmanager/TaskManager.cpp
    src/core/taskmanager/TaskDefinitionRegistry.cpp
    src/core/taskmanager/TaskExecutor.cpp
    src/core/taskmanager/operator_interface/OperatorInterface.cpp
    src/core/taskmanager/commands/StartTaskCommand.cpp
    src/core/taskmanager/commands/CancelTaskCommand.cpp
    src/core/taskmanager/commands/PauseTaskCommand.cpp
    src/core/taskmanager/TaskManagerInit.cpp
    src/core/sequence/core/ExecutionContext.cpp
    src/core/sequence/core/SequenceRegistry.cpp
    src/core/sequence/core/RetryHandler.cpp
    src/core/sequence/core/ActionExecutor.cpp
    src/core/sequence/core/ConditionEvaluator.cpp
    src/core/sequence/core/ExecutionMonitor.cpp
    src/core/sequence/core/SequenceEngine.cpp
)

target_include_directories(run_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/operator_interface
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/commands
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/tasks
    ${PROJECT_SOURCE_DIR}/src/core/sequence
    ${PROJECT_SOURCE_DIR}/src/core/sequence/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/sequence/core
    ${PROJECT_SOURCE_DIR}/src/core/sequence/dto
    ${PROJECT_SOURCE_DIR}/src/core/sequence/integration
    ${PROJECT_SOURCE_DIR}/tests/unit/sequence
    ${PROJECT_SOURCE_DIR}/tests/integration/sequence
)


# Link dependencies for tests
target_link_libraries(run_tests PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(run_tests)