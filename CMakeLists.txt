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
    src/core/taskmanager/operator_interface/OperatorInterface.cpp 
    src/core/taskmanager/factory/TaskFactory.cpp 
    src/core/taskmanager/commands/StartTaskCommand.cpp 
    src/core/taskmanager/TaskManagerInit.cpp
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
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/factory
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/commands
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/tasks
)

# Enable testing
enable_testing()

# Add tests
add_executable(run_tests 
    tests/unit/task/TaskManager_test.cpp
    tests/unit/task/OperatorInterface_test.cpp
    tests/unit/task/TaskFactory_test.cpp
    src/core/taskmanager/Task.cpp 
    src/core/taskmanager/TaskManager.cpp 
    src/core/taskmanager/operator_interface/OperatorInterface.cpp 
    src/core/taskmanager/factory/TaskFactory.cpp 
    src/core/taskmanager/commands/StartTaskCommand.cpp 
    src/core/taskmanager/TaskManagerInit.cpp
)

target_include_directories(run_tests PUBLIC 
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/interfaces
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/operator_interface
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/factory
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/commands
    ${PROJECT_SOURCE_DIR}/src/core/taskmanager/tasks
)


# Link dependencies for tests
target_link_libraries(run_tests PRIVATE 
    GTest::GTest 
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(run_tests)