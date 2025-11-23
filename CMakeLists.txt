cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add cmake modules directory
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Optional: backward-cpp for stack traces
option(USE_BACKWARD "Enable backward-cpp for stack traces" OFF)

# ============================================================================
# REQUIRED Dependencies
# ============================================================================
# These packages are REQUIRED for building the project.
# Install: sudo ./scripts/install-dependencies.sh --required
# See: README_DEPENDENCIES.md

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Checking REQUIRED dependencies...")
message(STATUS "========================================")

find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(TBB REQUIRED)  # concurrent_hash_map
find_package(nlohmann_json 3.11.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

message(STATUS "✓ All REQUIRED dependencies found")
message(STATUS "  - Threads: OK")
message(STATUS "  - spdlog: OK")
message(STATUS "  - GTest: OK")
message(STATUS "  - TBB: OK (concurrent containers)")
message(STATUS "  - nlohmann_json: OK")
message(STATUS "  - systemd: ${SYSTEMD_LIBRARIES}")

# ============================================================================
# OPTIONAL Dependencies (Feature 019: IPC Schema)
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Feature 019: IPC Schema")
message(STATUS "========================================")

# yaml-cpp (REQUIRED for IPC code generation)
find_package(yaml-cpp QUIET)
if(yaml-cpp_FOUND)
    message(STATUS "✓ yaml-cpp: FOUND")
else()
    message(WARNING "✗ yaml-cpp: NOT FOUND")
    message(WARNING "   IPC schema code generation will FAIL!")
    message(WARNING "   Install: sudo apt install libyaml-cpp-dev")
endif()

# ============================================================================
# OPTIONAL Dependencies (Feature 019: Hot Key Optimization)
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Feature 019: Hot Key Optimization")
message(STATUS "========================================")

# Folly (AtomicHashMap for lock-free cache)
find_package(folly QUIET)
if(folly_FOUND)
    message(STATUS "✓ Folly: FOUND - Hot Key optimization ENABLED")
else()
    message(WARNING "✗ Folly: NOT FOUND - Hot Key optimization DISABLED")
    message(WARNING "   See: docs/specs/019-architecture-improvements/folly-benchmark-installation.md")
    message(WARNING "   Or:  README_DEPENDENCIES.md")
endif()

# Google Benchmark (performance testing)
find_package(benchmark QUIET)
if(benchmark_FOUND)
    message(STATUS "✓ Google Benchmark: FOUND")
else()
    message(WARNING "✗ Google Benchmark: NOT FOUND")
    message(WARNING "   Hot Key benchmarks will not run")
    message(WARNING "   Install: Build from source (see README_DEPENDENCIES.md)")
endif()

# ============================================================================
# OPTIONAL Dependencies (Performance Optimization)
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Performance Optimization")
message(STATUS "========================================")

# Boost (lock-free data structures)
find_package(Boost 1.65 QUIET)
if(Boost_FOUND)
    message(STATUS "✓ Boost: FOUND at ${Boost_INCLUDE_DIRS}")
else()
    message(WARNING "✗ Boost: NOT FOUND")
    message(WARNING "   Install: sudo apt install libboost-all-dev")
endif()

# NUMA (Non-Uniform Memory Access)
find_library(NUMA_LIBRARY NAMES numa)
find_path(NUMA_INCLUDE_DIR NAMES numa.h)
if(NUMA_LIBRARY AND NUMA_INCLUDE_DIR)
    set(NUMA_FOUND TRUE)
    message(STATUS "✓ NUMA: FOUND at ${NUMA_LIBRARY}")
else()
    set(NUMA_FOUND FALSE)
    message(WARNING "✗ NUMA: NOT FOUND")
    message(WARNING "   Install: sudo apt install libnuma-dev")
endif()

# ============================================================================
# OPTIONAL Dependencies (Feature 019: Monitoring)
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Feature 019: Monitoring")
message(STATUS "========================================")

# prometheus-cpp (Metrics collection)
find_package(prometheus-cpp QUIET)
if(prometheus-cpp_FOUND)
    message(STATUS "✓ prometheus-cpp: FOUND")
else()
    message(WARNING "✗ prometheus-cpp: NOT FOUND")
    message(WARNING "   Install: Build from source (see README_DEPENDENCIES.md)")
endif()

# CivetWeb (HTTP server for metrics endpoint)
# Note: Ubuntu's civetweb package has broken CMake config, checking manually
find_library(CIVETWEB_LIBRARY NAMES civetweb)
find_path(CIVETWEB_INCLUDE_DIR NAMES civetweb.h)
if(CIVETWEB_LIBRARY AND CIVETWEB_INCLUDE_DIR)
    set(civetweb_FOUND TRUE)
    message(STATUS "✓ CivetWeb: FOUND at ${CIVETWEB_LIBRARY}")
else()
    set(civetweb_FOUND FALSE)
    message(WARNING "✗ CivetWeb: NOT FOUND")
    message(WARNING "   Install: sudo apt install libcivetweb-dev")
endif()

# ============================================================================
# OPTIONAL Dependencies (Feature 001: EtherCAT)
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Feature 001: EtherCAT Integration")
message(STATUS "========================================")

# IgH EtherCAT Master
find_package(EtherCAT QUIET)
if(EtherCAT_FOUND)
    message(STATUS "✓ IgH EtherCAT Master: FOUND at ${EtherCAT_LIBRARIES}")
else()
    message(WARNING "✗ IgH EtherCAT Master: NOT FOUND")
    message(WARNING "   EtherCAT features will be DISABLED")
    message(WARNING "   Install: https://gitlab.com/etherlab.org/ethercat")
endif()

# ============================================================================
# OPTIONAL Dependencies (Tracing)
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Distributed Tracing")
message(STATUS "========================================")

# OpenTelemetry
find_package(opentelemetry-cpp QUIET)
if(opentelemetry-cpp_FOUND)
    message(STATUS "✓ OpenTelemetry: FOUND")
else()
    message(STATUS "ℹ OpenTelemetry: NOT FOUND (optional)")
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Dependency Check Summary")
message(STATUS "========================================")
message(STATUS "Required:  ✓ ALL FOUND")
if(yaml-cpp_FOUND)
    message(STATUS "IPC Schema: ✓ ENABLED")
else()
    message(STATUS "IPC Schema: ✗ DISABLED (yaml-cpp missing)")
endif()
if(folly_FOUND)
    message(STATUS "Hot Key:    ✓ ENABLED")
else()
    message(STATUS "Hot Key:    ✗ DISABLED (Folly missing)")
endif()
if(prometheus-cpp_FOUND AND civetweb_FOUND)
    message(STATUS "Monitoring: ✓ ENABLED")
else()
    message(STATUS "Monitoring: ⚠ PARTIAL (some libs missing)")
endif()
if(EtherCAT_FOUND)
    message(STATUS "EtherCAT:   ✓ ENABLED")
else()
    message(STATUS "EtherCAT:   ✗ DISABLED")
endif()
message(STATUS "========================================")
message(STATUS "")

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
    src/core/event/core/PriorityQueue.cpp
    src/core/event/adapters/DataStoreEventAdapter.cpp
    src/core/event/adapters/ThrottlingPolicy.cpp
    src/core/event/adapters/CoalescingPolicy.cpp
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
    src/core/config/ConfigLoader.cpp
    # Production readiness: Performance optimization
    src/core/rt/perf/CPUAffinityManager.cpp
    src/core/rt/perf/NUMABinding.cpp
    src/core/rt/perf/PerfMonitor.cpp
    # Production readiness: Monitoring
    src/core/rt/RTMetrics.cpp
    src/core/monitoring/MetricsCollector.cpp
    src/core/monitoring/MetricsServer.cpp
    src/core/monitoring/StructuredLogger.cpp
    # Production readiness: Tracing
    src/core/tracing/SpanContext.cpp
    src/core/tracing/TracerProvider.cpp
    src/core/tracing/EventBusTracer.cpp
    src/core/tracing/RTCycleTracer.cpp
)

# Link dependencies
target_link_libraries(mxrc PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIBRARIES}
)
if(Boost_FOUND)
    target_link_libraries(mxrc PRIVATE Boost::boost)
endif()

# Link NUMA if found
if(NUMA_FOUND)
    target_link_libraries(mxrc PRIVATE ${NUMA_LIBRARY})
    target_include_directories(mxrc PRIVATE ${NUMA_INCLUDE_DIR})
endif()

# Enable Address Sanitizer (always on for memory safety)
target_compile_options(mxrc PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(mxrc PRIVATE -fsanitize=address)

# Include directories
target_include_directories(mxrc PUBLIC
    ${PROJECT_SOURCE_DIR}/include
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
    ${PROJECT_SOURCE_DIR}/src/core/datastore/hotkey
    ${PROJECT_SOURCE_DIR}/src/core/logging
    ${PROJECT_SOURCE_DIR}/src/core/logging/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/logging/core
    ${PROJECT_SOURCE_DIR}/src/core/logging/dto
    ${PROJECT_SOURCE_DIR}/src/core/logging/util
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
    ${PROJECT_SOURCE_DIR}/src/core/rt/ipc
    ${PROJECT_SOURCE_DIR}/src/core/ha
    ${PROJECT_SOURCE_DIR}/src/core/monitoring
    ${PROJECT_SOURCE_DIR}/src/core/tracing
    ${PROJECT_SOURCE_DIR}/src/core/systemd
    ${PROJECT_SOURCE_DIR}/src/core/systemd/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/systemd/impl
    ${PROJECT_SOURCE_DIR}/src/core/systemd/util
    ${PROJECT_SOURCE_DIR}/src/core/systemd/dto
    ${SYSTEMD_INCLUDE_DIRS}
)

# ===========================
# RT Schedule Tools
# ===========================

# Create generated directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/ipc)  # Feature 019: IPC Schema

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
    spdlog::spdlog
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
    spdlog::spdlog
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
add_dependencies(mxrc generate_ipc_schema)

# ============================================================================
# Feature 019: IPC Schema Code Generation
# ============================================================================

# Generate IPC Schema C++ headers from YAML
add_custom_command(
    OUTPUT
        ${CMAKE_BINARY_DIR}/generated/ipc/DataStoreKeys.h
        ${CMAKE_BINARY_DIR}/generated/ipc/EventBusEvents.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/codegen/validate_schema.py
            ${CMAKE_SOURCE_DIR}/config/ipc/ipc-schema.yaml
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/codegen/generate_ipc_schema.py
            ${CMAKE_SOURCE_DIR}/config/ipc/ipc-schema.yaml
            ${CMAKE_BINARY_DIR}/generated/ipc
    DEPENDS
        ${CMAKE_SOURCE_DIR}/scripts/codegen/validate_schema.py
        ${CMAKE_SOURCE_DIR}/scripts/codegen/generate_ipc_schema.py
        ${CMAKE_SOURCE_DIR}/scripts/codegen/templates/datastore_keys.h.j2
        ${CMAKE_SOURCE_DIR}/scripts/codegen/templates/eventbus_events.h.j2
        ${CMAKE_SOURCE_DIR}/config/ipc/ipc-schema.yaml
    COMMENT "Generating IPC schema C++ headers from YAML..."
)

# Create custom target for IPC code generation
add_custom_target(generate_ipc_schema
    DEPENDS
        ${CMAKE_BINARY_DIR}/generated/ipc/DataStoreKeys.h
        ${CMAKE_BINARY_DIR}/generated/ipc/EventBusEvents.h
)

# Ensure mxrc depends on IPC schema generation
add_dependencies(mxrc generate_ipc_schema)

# Add generated directory to .gitignore
file(APPEND ${PROJECT_SOURCE_DIR}/.gitignore "\nbuild/generated/\n")

# ===========================
# Non-RT Executive
# ===========================

# Non-RT executable
add_executable(nonrt
    src/nonrt_main.cpp
    src/core/nonrt/NonRTExecutive.cpp
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
    src/core/event/core/PriorityQueue.cpp
    src/core/event/adapters/DataStoreEventAdapter.cpp
    src/core/datastore/DataStore.cpp
    # DataStore manager classes
    src/core/datastore/managers/ExpirationManager.cpp
    src/core/datastore/managers/AccessControlManager.cpp
    src/core/datastore/managers/MetricsCollector.cpp
    src/core/datastore/managers/LogManager.cpp
    # HA classes (Feature 019)
    src/core/ha/HAStateMachine.cpp
    src/core/ha/RecoveryPolicy.cpp
    # RT IPC
    src/core/rt/ipc/SharedMemory.cpp
    src/core/rt/util/TimeUtils.cpp
    # Config loader
    src/core/config/ConfigLoader.cpp
)

# Link dependencies for nonrt
target_link_libraries(nonrt PRIVATE
    spdlog::spdlog
    TBB::tbb
    nlohmann_json::nlohmann_json
    yaml-cpp
    Threads::Threads
    ${SYSTEMD_LIBRARIES}
)
if(Boost_FOUND)
    target_link_libraries(nonrt PRIVATE Boost::boost)
endif()

# Enable Address Sanitizer
target_compile_options(nonrt PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(nonrt PRIVATE -fsanitize=address)

# Include directories for nonrt
target_include_directories(nonrt PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/generated
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
    ${PROJECT_SOURCE_DIR}/src/core/nonrt
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
    ${PROJECT_SOURCE_DIR}/src/core/rt/ipc
    ${PROJECT_SOURCE_DIR}/src/core/ha
    ${PROJECT_SOURCE_DIR}/src/core/systemd
    ${PROJECT_SOURCE_DIR}/src/core/systemd/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/systemd/impl
    ${PROJECT_SOURCE_DIR}/src/core/systemd/util
    ${PROJECT_SOURCE_DIR}/src/core/systemd/dto
    ${SYSTEMD_INCLUDE_DIRS}
)

# ===========================
# RT Executive
# ===========================

# RT executable
add_executable(rt
    src/rt_main.cpp
    src/core/rt/RTExecutive.cpp
    src/core/rt/RTStateMachine.cpp
    src/core/rt/RTDataStore.cpp
    src/core/rt/RTDataStoreShared.cpp
    src/core/rt/ipc/SharedMemory.cpp
    src/core/rt/util/TimeUtils.cpp
    src/core/rt/util/ScheduleCalculator.cpp
    src/core/event/core/EventBus.cpp
    src/core/event/core/PriorityQueue.cpp
    src/core/config/ConfigLoader.cpp
    # Production readiness: Performance optimization
    src/core/rt/perf/CPUAffinityManager.cpp
    src/core/rt/perf/NUMABinding.cpp
    src/core/rt/perf/PerfMonitor.cpp
    # Production readiness: Monitoring
    src/core/rt/RTMetrics.cpp
    src/core/monitoring/MetricsCollector.cpp
    src/core/monitoring/MetricsServer.cpp
    src/core/monitoring/StructuredLogger.cpp
    # Production readiness: High Availability
    src/core/ha/HealthCheckServer.cpp
    src/core/ha/ProcessMonitor.cpp
    src/core/ha/StateCheckpoint.cpp
    src/core/ha/FailoverManager.cpp
    # Production readiness: Tracing
    src/core/tracing/SpanContext.cpp
    src/core/tracing/TracerProvider.cpp
    src/core/tracing/EventBusTracer.cpp
    src/core/tracing/RTCycleTracer.cpp
)

# Link dependencies for rt
target_link_libraries(rt PRIVATE
    spdlog::spdlog
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIBRARIES}
)
if(Boost_FOUND)
    target_link_libraries(rt PRIVATE Boost::boost)
endif()

# Link EtherCAT if found
if(EtherCAT_FOUND)
    target_link_libraries(rt PRIVATE EtherCAT::EtherCAT)
endif()

# Link yaml-cpp if found
if(yaml-cpp_FOUND)
    target_link_libraries(rt PRIVATE yaml-cpp)
endif()

# Link NUMA if found
if(NUMA_FOUND)
    target_link_libraries(rt PRIVATE ${NUMA_LIBRARY})
    target_include_directories(rt PRIVATE ${NUMA_INCLUDE_DIR})
endif()

# Enable Address Sanitizer
target_compile_options(rt PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(rt PRIVATE -fsanitize=address)

# Include directories for rt
target_include_directories(rt PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/rt/util
    ${PROJECT_SOURCE_DIR}/src/core/rt/ipc
    ${PROJECT_SOURCE_DIR}/src/core/event
    ${PROJECT_SOURCE_DIR}/src/core/event/core
    ${PROJECT_SOURCE_DIR}/src/core/event/dto
    ${PROJECT_SOURCE_DIR}/src/core/event/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/event/util
    ${PROJECT_SOURCE_DIR}/src/core/ha
    ${PROJECT_SOURCE_DIR}/src/core/monitoring
    ${PROJECT_SOURCE_DIR}/src/core/tracing
    ${PROJECT_SOURCE_DIR}/src/core/systemd
    ${PROJECT_SOURCE_DIR}/src/core/systemd/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/systemd/impl
    ${PROJECT_SOURCE_DIR}/src/core/systemd/util
    ${PROJECT_SOURCE_DIR}/src/core/systemd/dto
    ${PROJECT_SOURCE_DIR}/src/core/ethercat
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/core
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/dto
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/util
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/adapters
    ${SYSTEMD_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/generated
)

# Add EtherCAT include directories if found
if(EtherCAT_FOUND)
    target_include_directories(rt PRIVATE ${EtherCAT_INCLUDE_DIRS})
endif()

# Ensure rt depends on schedule generation
add_dependencies(rt generate_schedule)

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
    tests/unit/event/PrioritizedEvent_test.cpp
    tests/unit/event/PriorityQueue_test.cpp
    tests/unit/event/ThrottlingPolicy_test.cpp
    tests/unit/event/CoalescingPolicy_test.cpp
    tests/integration/event/event_flow_test.cpp
    tests/integration/event/priority_integration_test.cpp
    tests/integration/event/event_storm_test.cpp
    tests/integration/event/monitoring_extension_test.cpp
    tests/integration/datastore_logging_integration_test.cpp
    tests/unit/datastore/DataStore_test.cpp
    tests/unit/datastore/tbb_integration_test.cpp
    tests/unit/datastore/ExpirationManager_test.cpp
    tests/unit/datastore/MetricsCollector_test.cpp
    tests/unit/datastore/AccessControlManager_test.cpp
    tests/unit/datastore/LogManager_test.cpp
    tests/unit/datastore/VersionedData_test.cpp
    tests/unit/datastore/accessor_benchmark.cpp
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
    tests/unit/rt/RTDataStore_concurrency_test.cpp
    tests/unit/rt/SharedMemory_test.cpp
    tests/unit/rt/RTExecutive_test.cpp
    tests/unit/rt/RTStateMachine_test.cpp
    tests/integration/rt/rt_integration_test.cpp
    tests/core/rt/RTExecutiveEventBusTest.cpp
    # Fieldbus tests (Feature 019)
    tests/unit/fieldbus/FieldbusFactory_test.cpp
    tests/unit/monitoring/MetricsCollector_test.cpp
    tests/unit/monitoring/MetricsServer_test.cpp
    tests/unit/monitoring/RTMetrics_test.cpp
    tests/unit/monitoring/structured_logger_test.cpp
    tests/integration/monitoring/monitoring_integration_test.cpp
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
    src/core/event/core/PriorityQueue.cpp
    src/core/event/adapters/DataStoreEventAdapter.cpp
    src/core/event/adapters/ThrottlingPolicy.cpp
    src/core/event/adapters/CoalescingPolicy.cpp
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
    # Non-RT Executive (for integration tests)
    src/core/nonrt/NonRTExecutive.cpp
    # HA classes (Feature 019 - required by NonRTExecutive)
    src/core/ha/HAStateMachine.cpp
    src/core/ha/RecoveryPolicy.cpp
    # Fieldbus drivers (Feature 019 - for FieldbusFactory test)
    src/core/fieldbus/factory/FieldbusFactory.cpp
    src/core/fieldbus/drivers/MockDriver.cpp
    src/core/fieldbus/drivers/EtherCATDriver.cpp
    # Monitoring
    src/core/monitoring/MetricsCollector.cpp
    src/core/monitoring/MetricsServer.cpp
    src/core/monitoring/StructuredLogger.cpp
    src/core/rt/RTMetrics.cpp
    # Production readiness: Performance tests
    tests/unit/perf/CPUAffinityManager_test.cpp
    tests/unit/perf/NUMABinding_test.cpp
    tests/unit/perf/PerfMonitor_test.cpp
    # Production readiness: Integration tests
    tests/integration/perf/cpu_isolation_test.cpp
    tests/integration/perf/numa_optimization_test.cpp
    src/core/rt/perf/CPUAffinityManager.cpp
    src/core/rt/perf/NUMABinding.cpp
    src/core/rt/perf/PerfMonitor.cpp
    src/core/config/ConfigLoader.cpp
    # Production readiness: High Availability
    src/core/ha/HealthCheckServer.cpp
    src/core/ha/StateCheckpoint.cpp
    src/core/ha/FailoverManager.cpp
    src/core/ha/ProcessMonitor.cpp
    # Production readiness: High Availability tests
    tests/core/ha/process_monitor_test.cpp
    tests/core/ha/failover_manager_test.cpp
    tests/core/ha/state_checkpoint_test.cpp
    # Production readiness: Tracing
    src/core/tracing/SpanContext.cpp
    src/core/tracing/TracerProvider.cpp
    src/core/tracing/EventBusTracer.cpp
    src/core/tracing/RTCycleTracer.cpp
    # Production readiness: Tracing tests
    tests/unit/tracing/TracerProvider_test.cpp
    tests/unit/tracing/SpanContext_test.cpp
    tests/unit/tracing/EventBusTracer_test.cpp
    tests/unit/tracing/RTCycleTracer_test.cpp
    tests/integration/tracing/tracing_integration_test.cpp
    # systemd unit tests
    tests/unit/systemd/systemd_util_test.cpp
    tests/unit/systemd/watchdog_notifier_test.cpp
    tests/unit/systemd/watchdog_timer_test.cpp
    # systemd integration tests (Feature 022: P1 Startup Order)
    tests/integration/systemd/startup_order_test.cpp
    tests/integration/systemd/retry_logic_test.cpp
    # systemd integration tests (Phase 5: Resource Control)
    tests/integration/systemd/cpu_quota_test.cpp
    tests/integration/systemd/memory_limit_test.cpp
    tests/integration/systemd/io_weight_test.cpp
    # systemd integration tests (Phase 6: Service Dependencies)
    tests/integration/systemd/service_order_test.cpp
    tests/integration/systemd/dependency_chain_test.cpp
    tests/integration/systemd/target_test.cpp
    # systemd integration tests (Phase 7: Prometheus Metrics)
    tests/integration/systemd/metrics_collection_test.cpp
    tests/integration/systemd/prometheus_format_test.cpp
    tests/integration/systemd/metrics_endpoint_test.cpp
    # systemd integration tests (Phase 8: journald Integration)
    tests/integration/systemd/journald_logging_test.cpp
    tests/integration/systemd/structured_logging_test.cpp
    tests/integration/systemd/log_query_test.cpp
    # systemd integration tests (Phase 9: Security Hardening)
    tests/integration/systemd/security_hardening_test.cpp
    tests/integration/systemd/capability_test.cpp
    tests/integration/systemd/filesystem_restriction_test.cpp
    # Phase 10: Boot Optimization (User Story 8)
    tests/integration/systemd/boot_time_test.cpp
    src/core/systemd/util/SystemdUtil.cpp
    src/core/systemd/util/WatchdogTimer.cpp
    src/core/systemd/impl/SdNotifyWatchdog.cpp
    src/systemd/SystemdMetricsCollector.cpp
    src/systemd/JournaldLogger.cpp
    # EtherCAT (Feature 001)
    tests/unit/ethercat/YAMLConfigParser_test.cpp
    tests/unit/ethercat/SensorDataManager_test.cpp
    tests/unit/ethercat/MotorCommandManager_test.cpp
    tests/integration/ethercat/RTEtherCATCycle_test.cpp
    src/core/ethercat/util/YAMLConfigParser.cpp
    src/core/ethercat/util/EtherCATLogger.cpp
    src/core/ethercat/impl/SensorDataManager.cpp
    src/core/ethercat/impl/MotorCommandManager.cpp
    src/core/ethercat/core/EtherCATMaster.cpp
    src/core/ethercat/core/EtherCATDomain.cpp
    src/core/ethercat/adapters/RTEtherCATCycle.cpp
)

target_include_directories(run_tests PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
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
    ${PROJECT_SOURCE_DIR}/src/core/datastore/hotkey
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
    ${PROJECT_SOURCE_DIR}/tests/core/rt
    ${PROJECT_SOURCE_DIR}/src/core/monitoring
    ${PROJECT_SOURCE_DIR}/tests/unit/monitoring
    ${PROJECT_SOURCE_DIR}/tests/integration/monitoring
    ${PROJECT_SOURCE_DIR}/src/core/ha
    ${PROJECT_SOURCE_DIR}/src/core/tracing
    ${PROJECT_SOURCE_DIR}/tests/unit/tracing
    ${PROJECT_SOURCE_DIR}/tests/integration/tracing
    ${PROJECT_SOURCE_DIR}/src/core/systemd
    ${PROJECT_SOURCE_DIR}/src/core/systemd/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/systemd/impl
    ${PROJECT_SOURCE_DIR}/src/core/systemd/util
    ${PROJECT_SOURCE_DIR}/src/core/systemd/dto
    ${PROJECT_SOURCE_DIR}/tests/unit/systemd
    ${PROJECT_SOURCE_DIR}/tests/integration/systemd
    ${PROJECT_SOURCE_DIR}/include/mxrc/systemd
    ${PROJECT_SOURCE_DIR}/src/core/ethercat
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/core
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/dto
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/util
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/impl
    ${PROJECT_SOURCE_DIR}/src/core/ethercat/adapters
    ${PROJECT_SOURCE_DIR}/tests/unit/ethercat
    ${PROJECT_SOURCE_DIR}/tests/integration/ethercat
    ${PROJECT_SOURCE_DIR}/tests/mocks
    ${SYSTEMD_INCLUDE_DIRS}
)

# Add EtherCAT include directories if found
if(EtherCAT_FOUND)
    target_include_directories(run_tests PRIVATE ${EtherCAT_INCLUDE_DIRS})
endif()


# Link dependencies for tests
target_link_libraries(run_tests PRIVATE
    spdlog::spdlog
    GTest::GTest
    GTest::Main
    TBB::tbb
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIBRARIES}
)
if(Boost_FOUND)
    target_link_libraries(run_tests PRIVATE Boost::boost)
endif()

# Link EtherCAT for tests if found
if(EtherCAT_FOUND)
    target_link_libraries(run_tests PRIVATE EtherCAT::EtherCAT)
endif()

# Link yaml-cpp for tests if found
if(yaml-cpp_FOUND)
    target_link_libraries(run_tests PRIVATE yaml-cpp)
endif()

# Link Folly for tests if found (Feature 019: Hot Key Optimization)
if(folly_FOUND)
    target_link_libraries(run_tests PRIVATE Folly::folly)
endif()

# Link Google Benchmark for tests if found (Feature 019)
if(benchmark_FOUND)
    target_link_libraries(run_tests PRIVATE benchmark::benchmark)
endif()

# Link NUMA if found
if(NUMA_FOUND)
    target_link_libraries(run_tests PRIVATE ${NUMA_LIBRARY})
    target_include_directories(run_tests PRIVATE ${NUMA_INCLUDE_DIR})
endif()

# Enable Address Sanitizer (always on for memory safety)
target_compile_options(run_tests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_libraries(run_tests PRIVATE -fsanitize=address)

# Link backward library if enabled
if(USE_BACKWARD)
    target_link_libraries(run_tests PRIVATE backward)
endif()

include(GoogleTest)
gtest_discover_tests(run_tests)

# Ensure run_tests depends on code generation
add_dependencies(run_tests generate_ipc_schema generate_schedule)

add_custom_command(
    TARGET run_tests
    POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/count_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Counting all tests..."
)