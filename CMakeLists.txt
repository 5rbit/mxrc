cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(TBB REQUIRED) 

# Add executable
add_executable(mxrc
    src/main.cpp
    src/core/action/core/ActionExecutor.cpp
    src/core/action/core/ActionFactory.cpp
    src/core/action/core/ActionRegistry.cpp
    src/core/action/impl/DelayAction.cpp
    src/core/action/impl/MoveAction.cpp
    src/core/sequence/core/SequenceRegistry.cpp
    src/core/sequence/core/SequenceEngine.cpp
    src/core/sequence/core/ConditionEvaluator.cpp
    src/core/sequence/core/RetryHandler.cpp
    src/core/task/core/TaskRegistry.cpp
    src/core/task/core/TaskExecutor.cpp
    src/core/task/core/PeriodicScheduler.cpp
    src/core/task/core/TriggerManager.cpp
    src/core/task/core/TaskMonitor.cpp
    src/core/event/core/EventBus.cpp
    src/core/event/adapters/DataStoreEventAdapter.cpp
    src/core/datastore/DataStore.cpp
)

# Link dependencies
target_link_libraries(mxrc PRIVATE 
    spdlog::spdlog 
    GTest::GTest 
    GTest::Main
    TBB::tbb
)

# Include directories
target_include_directories(mxrc PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/action
    ${PROJECT_SOURCE_DIR}/src/core/action/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/action/core
    ${PROJECT_SOURCE_DIR}/src/core/action/dto
    ${PROJECT_SOURCE_DIR}/src/core/action/impl
    ${PROJECT_SOURCE_DIR}/src/core/action/util
    ${PROJECT_SOURCE_DIR}/src/core/sequence
    ${PROJECT_SOURCE_DIR}/src/core/sequence/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/sequence/core
    ${PROJECT_SOURCE_DIR}/src/core/sequence/dto
    ${PROJECT_SOURCE_DIR}/src/core/sequence/integration
    ${PROJECT_SOURCE_DIR}/src/core/task
    ${PROJECT_SOURCE_DIR}/src/core/task/core
    ${PROJECT_SOURCE_DIR}/src/core/task/dto
    ${PROJECT_SOURCE_DIR}/src/core/task/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/event
    ${PROJECT_SOURCE_DIR}/src/core/event/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/event/core
    ${PROJECT_SOURCE_DIR}/src/core/event/dto
    ${PROJECT_SOURCE_DIR}/src/core/event/util
    ${PROJECT_SOURCE_DIR}/src/core/event/adapters
    ${PROJECT_SOURCE_DIR}/src/core/datastore
)

# Enable testing
enable_testing()

# Add tests
add_executable(run_tests
    tests/unit/task/TaskRegistry_test.cpp
    tests/unit/task/TaskCoreExecutor_test.cpp
    tests/unit/task/PeriodicScheduler_test.cpp
    tests/unit/task/TriggerManager_test.cpp
    tests/unit/task/TaskMonitor_test.cpp
    tests/unit/action/ActionExecutor_test.cpp
    tests/unit/action/ActionFactory_test.cpp
    tests/unit/action/ActionRegistry_test.cpp
    tests/integration/action_integration_test.cpp
    tests/unit/sequence/SequenceRegistry_test.cpp
    tests/unit/sequence/SequenceEngine_test.cpp
    tests/unit/event/LockFreeQueue_test.cpp
    tests/unit/event/MPSCLockFreeQueue_test.cpp
    tests/unit/event/SubscriptionManager_test.cpp
    tests/unit/event/EventBus_test.cpp
    tests/unit/event/DataStoreEventAdapter_test.cpp
    tests/integration/event/event_flow_test.cpp
    tests/integration/event/monitoring_extension_test.cpp
    tests/unit/datastore/tbb_integration_test.cpp
    examples/event_monitoring/ExecutionTimeCollector.cpp
    examples/event_monitoring/StateTransitionLogger.cpp
    src/core/task/core/TaskRegistry.cpp
    src/core/task/core/TaskExecutor.cpp
    src/core/task/core/PeriodicScheduler.cpp
    src/core/task/core/TriggerManager.cpp
    src/core/task/core/TaskMonitor.cpp
    src/core/action/core/ActionExecutor.cpp
    src/core/action/core/ActionFactory.cpp
    src/core/action/core/ActionRegistry.cpp
    src/core/action/impl/DelayAction.cpp
    src/core/action/impl/MoveAction.cpp
    src/core/sequence/core/SequenceRegistry.cpp
    src/core/sequence/core/SequenceEngine.cpp
    src/core/sequence/core/ConditionEvaluator.cpp
    src/core/sequence/core/RetryHandler.cpp
    src/core/event/core/EventBus.cpp
    src/core/event/adapters/DataStoreEventAdapter.cpp
    src/core/datastore/DataStore.cpp
)

target_include_directories(run_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/action
    ${PROJECT_SOURCE_DIR}/src/core/action/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/action/core
    ${PROJECT_SOURCE_DIR}/src/core/action/dto
    ${PROJECT_SOURCE_DIR}/src/core/action/impl
    ${PROJECT_SOURCE_DIR}/src/core/action/util
    ${PROJECT_SOURCE_DIR}/src/core/sequence
    ${PROJECT_SOURCE_DIR}/src/core/sequence/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/sequence/core
    ${PROJECT_SOURCE_DIR}/src/core/sequence/dto
    ${PROJECT_SOURCE_DIR}/src/core/sequence/integration
    ${PROJECT_SOURCE_DIR}/src/core/task
    ${PROJECT_SOURCE_DIR}/src/core/task/core
    ${PROJECT_SOURCE_DIR}/src/core/task/dto
    ${PROJECT_SOURCE_DIR}/src/core/task/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/event
    ${PROJECT_SOURCE_DIR}/src/core/event/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/event/core
    ${PROJECT_SOURCE_DIR}/src/core/event/dto
    ${PROJECT_SOURCE_DIR}/src/core/event/util
    ${PROJECT_SOURCE_DIR}/src/core/event/adapters
    ${PROJECT_SOURCE_DIR}/src/core/datastore
    ${PROJECT_SOURCE_DIR}/tests/unit/action
    ${PROJECT_SOURCE_DIR}/tests/unit/task
    ${PROJECT_SOURCE_DIR}/tests/unit/sequence
    ${PROJECT_SOURCE_DIR}/tests/unit/event
    ${PROJECT_SOURCE_DIR}/tests/unit/datastore
    ${PROJECT_SOURCE_DIR}/tests/integration
    ${PROJECT_SOURCE_DIR}/tests/integration/event
    ${PROJECT_SOURCE_DIR}/examples
    ${PROJECT_SOURCE_DIR}/examples/event_monitoring
)


# Link dependencies for tests
target_link_libraries(run_tests PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
)

include(GoogleTest)
gtest_discover_tests(run_tests)