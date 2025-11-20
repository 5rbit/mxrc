cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: backward-cpp for stack traces
option(USE_BACKWARD "Enable backward-cpp for stack traces" OFF)

# Find dependencies
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(TBB REQUIRED)
find_package(nlohmann_json 3.11.0 REQUIRED)

# backward-cpp (header-only, optional)
if(USE_BACKWARD)
    add_library(backward INTERFACE)
    target_include_directories(backward INTERFACE ${PROJECT_SOURCE_DIR}/src/core/logging)
    target_compile_definitions(backward INTERFACE USE_BACKWARD_CPP)
    message(STATUS "backward-cpp enabled for stack traces")
endif() 

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
    # DataStore manager classes
    src/core/datastore/managers/ExpirationManager.cpp
    src/core/datastore/managers/AccessControlManager.cpp
    src/core/datastore/managers/MetricsCollector.cpp
    src/core/datastore/managers/LogManager.cpp
    src/core/logging/util/Serializer.cpp
    src/core/logging/util/FileUtils.cpp
    src/core/logging/core/AsyncWriter.cpp
    src/core/logging/util/RetentionManager.cpp
    # RT Executive
    src/core/rt/RTExecutive.cpp
    src/core/rt/RTStateMachine.cpp
    src/core/rt/RTDataStore.cpp
    src/core/rt/RTDataStoreShared.cpp
    src/core/rt/ipc/SharedMemory.cpp
    src/core/rt/util/TimeUtils.cpp
    src/core/rt/util/ScheduleCalculator.cpp
)

# Link dependencies
target_link_libraries(mxrc PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
    nlohmann_json::nlohmann_json
)

# Enable Address Sanitizer (always on for memory safety)
target_compile_options(mxrc PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(mxrc PRIVATE -fsanitize=address)

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
    ${PROJECT_SOURCE_DIR}/src/core/datastore/managers
    ${PROJECT_SOURCE_DIR}/src/core/logging
    ${PROJECT_SOURCE_DIR}/src/core/logging/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/logging/core
    ${PROJECT_SOURCE_DIR}/src/core/logging/dto
    ${PROJECT_SOURCE_DIR}/src/core/logging/util
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
    ${PROJECT_SOURCE_DIR}/src/core/rt/ipc
)

# ===========================
# RT Schedule Tools
# ===========================

# Create generated directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

# Build schedule_generator tool
add_executable(schedule_generator
    tools/schedule_generator.cpp
    src/core/rt/util/ScheduleCalculator.cpp
)
target_include_directories(schedule_generator PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
)
target_link_libraries(schedule_generator PRIVATE
    nlohmann_json::nlohmann_json
)

# Build validate_schedule tool
add_executable(validate_schedule
    tools/validate_schedule.cpp
    src/core/rt/util/ScheduleCalculator.cpp
)
target_include_directories(validate_schedule PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
)
target_link_libraries(validate_schedule PRIVATE
    nlohmann_json::nlohmann_json
)

# Generate RTSchedule.h from config
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
    COMMAND validate_schedule ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
    COMMAND schedule_generator
            ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
            ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
    DEPENDS
        schedule_generator
        validate_schedule
        ${CMAKE_SOURCE_DIR}/config/rt_schedule.json
    COMMENT "Generating RT schedule from config..."
)

# Create custom target for schedule generation
add_custom_target(generate_schedule
    DEPENDS ${CMAKE_BINARY_DIR}/generated/RTSchedule.h
)

# Add generated directory to include path
target_include_directories(mxrc PUBLIC
    ${CMAKE_BINARY_DIR}/generated
)

# Ensure mxrc depends on schedule generation
add_dependencies(mxrc generate_schedule)

# Add generated directory to .gitignore
file(APPEND ${PROJECT_SOURCE_DIR}/.gitignore "\nbuild/generated/\n")

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
    tests/integration/datastore_logging_integration_test.cpp
    tests/unit/datastore/DataStore_test.cpp
    tests/unit/datastore/tbb_integration_test.cpp
    tests/unit/datastore/ExpirationManager_test.cpp
    tests/unit/datastore/MetricsCollector_test.cpp
    tests/unit/datastore/AccessControlManager_test.cpp
    tests/unit/datastore/LogManager_test.cpp
    tests/unit/logging/BagMessage_test.cpp
    tests/unit/logging/Serializer_test.cpp
    tests/unit/logging/FileUtils_test.cpp
    tests/unit/logging/AsyncWriter_test.cpp
    tests/unit/logging/RetentionManager_test.cpp
    tests/unit/logging/SimpleBagWriter_test.cpp
    tests/unit/logging/DataStoreBagLogger_test.cpp
    tests/unit/logging/Indexer_test.cpp
    tests/unit/logging/BagReader_test.cpp
    tests/unit/logging/BagReplayer_test.cpp
    tests/unit/logging/AsyncLogger_test.cpp
    tests/unit/logging/LogPerformance_test.cpp
    tests/unit/logging/SignalHandler_test.cpp
    tests/integration/logging/bag_logging_integration_test.cpp
    tests/integration/logging/CrashSafety_test.cpp
    tests/benchmark/logging/datastore_logging_benchmark.cpp
    tests/unit/rt/RTDataStore_test.cpp
    tests/unit/rt/SharedMemory_test.cpp
    tests/unit/rt/RTExecutive_test.cpp
    tests/unit/rt/RTStateMachine_test.cpp
    tests/integration/rt/rt_integration_test.cpp
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
    src/core/datastore/managers/ExpirationManager.cpp
    src/core/datastore/managers/MetricsCollector.cpp
    src/core/datastore/managers/AccessControlManager.cpp
    src/core/datastore/managers/LogManager.cpp
    src/core/logging/util/Serializer.cpp
    src/core/logging/util/FileUtils.cpp
    src/core/logging/core/AsyncWriter.cpp
    src/core/logging/util/RetentionManager.cpp
    src/core/logging/util/Indexer.cpp
    src/core/logging/core/SimpleBagWriter.cpp
    src/core/logging/core/DataStoreBagLogger.cpp
    src/core/logging/core/BagReader.cpp
    src/core/logging/core/BagReplayer.cpp
    # RT Executive
    src/core/rt/RTExecutive.cpp
    src/core/rt/RTStateMachine.cpp
    src/core/rt/RTDataStore.cpp
    src/core/rt/RTDataStoreShared.cpp
    src/core/rt/ipc/SharedMemory.cpp
    src/core/rt/util/TimeUtils.cpp
    src/core/rt/util/ScheduleCalculator.cpp
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
    ${PROJECT_SOURCE_DIR}/tests/benchmark/logging
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
    ${PROJECT_SOURCE_DIR}/src/core/rt/ipc
    ${PROJECT_SOURCE_DIR}/tests/unit/rt
    ${PROJECT_SOURCE_DIR}/tests/integration/rt
)


# Link dependencies for tests
target_link_libraries(run_tests PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
    nlohmann_json::nlohmann_json
)

# Enable Address Sanitizer (always on for memory safety)
target_compile_options(run_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(run_tests PRIVATE -fsanitize=address)

# Link backward library if enabled
if(USE_BACKWARD)
    target_link_libraries(run_tests PRIVATE backward)
endif()

include(GoogleTest)
gtest_discover_tests(run_tests)