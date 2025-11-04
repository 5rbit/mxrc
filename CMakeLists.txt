cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(spdlog REQUIRED)

# 1. 패키지 이름을 'BehaviorTreeCPP'로 수정합니다.
#    (Homebrew 패키지 이름 'behaviortree.cpp'와 연관될 가능성이 높음)
find_package(BehaviorTreeCPP QUIET)

if(EXISTS ${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP)
    message(STATUS "Using vendored BehaviorTree.CPP from ${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP")
    add_subdirectory(${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP)
else()
    find_package(BehaviorTreeCPP REQUIRED)
endif()

find_package(GTest REQUIRED) 

# Add executable
add_executable(mxrc src/main.cpp src/core/task/TaskFactory.cpp src/core/task/tasks/DriveToPosition.cpp src/core/task/MissionManager.cpp src/core/task/ExecuteTaskNode.cpp)

# Link dependencies
target_link_libraries(mxrc PRIVATE 
    spdlog::spdlog 
    BT::behaviortree_cpp 
    GTest::GTest 
    GTest::Main
)

# Include directories
target_include_directories(mxrc PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/task/contracts
)

# If using vendored BehaviorTree.CPP, add its include directory to mxrc
if(EXISTS ${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP)
    target_include_directories(mxrc PUBLIC ${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP/include)
endif()

# Enable testing
enable_testing()

# Add tests
add_executable(run_tests 
    tests/unit/datastore/DataStore_test.cpp
    tests/unit/task/MissionManager_test.cpp
    src/core/task/MissionManager.cpp
    src/core/datastore/DataStore.cpp
    src/core/task/TaskFactory.cpp
    src/core/task/tasks/DriveToPosition.cpp
    tests/unit/task/TaskFactory_test.cpp
    src/core/task/ExecuteTaskNode.cpp
)

target_include_directories(run_tests PUBLIC 
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/core/task/contracts
)

# If using vendored BehaviorTree.CPP, add its include directory to run_tests
if(EXISTS ${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP)
    target_include_directories(run_tests PUBLIC ${PROJECT_SOURCE_DIR}/vendor/BehaviorTree.CPP/include)
endif()

# Link dependencies for tests
target_link_libraries(run_tests PRIVATE 
    GTest::GTest 
    GTest::Main
    BT::behaviortree_cpp
)

include(GoogleTest)
gtest_discover_tests(run_tests)