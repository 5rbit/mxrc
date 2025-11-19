cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(TBB REQUIRED)
find_package(nlohmann_json 3.11.0 REQUIRED) 

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
    src/core/logging/util/Serializer.cpp
    src/core/logging/util/FileUtils.cpp
    src/core/logging/core/AsyncWriter.cpp
    src/core/logging/util/RetentionManager.cpp
)

# Link dependencies
target_link_libraries(mxrc PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
    nlohmann_json::nlohmann_json
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
    ${PROJECT_SOURCE_DIR}/src/core/logging
    ${PROJECT_SOURCE_DIR}/src/core/logging/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/logging/core
    ${PROJECT_SOURCE_DIR}/src/core/logging/dto
    ${PROJECT_SOURCE_DIR}/src/core/logging/util
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
    tests/unit/datastore/DataStore_test.cpp
    tests/unit/datastore/tbb_integration_test.cpp
    tests/unit/logging/BagMessage_test.cpp
    tests/unit/logging/Serializer_test.cpp
    tests/unit/logging/FileUtils_test.cpp
    tests/unit/logging/AsyncWriter_test.cpp
    tests/unit/logging/RetentionManager_test.cpp
    tests/unit/logging/SimpleBagWriter_test.cpp
    tests/unit/logging/DataStoreBagLogger_test.cpp
    tests/integration/logging/bag_logging_integration_test.cpp
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
    src/core/logging/util/Serializer.cpp
    src/core/logging/util/FileUtils.cpp
    src/core/logging/core/AsyncWriter.cpp
    src/core/logging/util/RetentionManager.cpp
    src/core/logging/core/SimpleBagWriter.cpp
    src/core/logging/core/DataStoreBagLogger.cpp
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
    ${PROJECT_SOURCE_DIR}/src/core/logging
    ${PROJECT_SOURCE_DIR}/src/core/logging/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/logging/core
    ${PROJECT_SOURCE_DIR}/src/core/logging/dto
    ${PROJECT_SOURCE_DIR}/src/core/logging/util
    ${PROJECT_SOURCE_DIR}/tests/unit/logging
    ${PROJECT_SOURCE_DIR}/tests/integration/logging
)


# Link dependencies for tests
target_link_libraries(run_tests PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
    nlohmann_json::nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(run_tests)