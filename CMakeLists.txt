cmake_minimum_required(VERSION 3.16)
project(mxrc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(spdlog REQUIRED)

# Manually specify BehaviorTree.CPP
set(BEHAVIORTREE_CPP_INCLUDE_DIR /opt/homebrew/Cellar/behaviortree.cpp/4.8.2/include)
set(BEHAVIORTREE_CPP_LIB /opt/homebrew/Cellar/behaviortree.cpp/4.8.2/lib/libbehaviortree_cpp.dylib)

# GTest는 이름이 올바르지만, 가져온 대상 이름은 'GTest'가 아닌 'gtest'와 'gtest_main'을 사용합니다.
# 하지만 GTest::GTest, GTest::Main 형태도 널리 사용되므로 일단 유지합니다.
find_package(GTest REQUIRED) 

# Add executable
add_executable(mxrc src/main.cpp src/core/task/task_mission_management/TaskFactory.cpp src/core/task/task_mission_management/DriveToPositionTask.cpp src/core/task/task_mission_management/MissionManager.cpp)

# Link dependencies
target_link_libraries(mxrc PRIVATE 
    spdlog::spdlog 
    ${BEHAVIORTREE_CPP_LIB}
    GTest::GTest 
    GTest::Main
)

# Include directories
target_include_directories(mxrc PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${BEHAVIORTREE_CPP_INCLUDE_DIR}
)

# Enable testing
enable_testing()

# Add tests
add_executable(run_tests 
    tests/unit/datastore/DataStore_test.cpp
    tests/unit/task/MissionManager_test.cpp
    src/core/task/task_mission_management/MissionManager.cpp
    src/core/datastore/DataStore.cpp
    src/core/task/task_mission_management/TaskFactory.cpp
    src/core/task/task_mission_management/DriveToPositionTask.cpp
    tests/unit/task/TaskFactory_test.cpp
)
target_include_directories(run_tests PUBLIC 
    ${PROJECT_SOURCE_DIR}/src
    ${BEHAVIORTREE_CPP_INCLUDE_DIR}
)

# Link dependencies for tests
target_link_libraries(run_tests PRIVATE 
    GTest::GTest 
    GTest::Main
    ${BEHAVIORTREE_CPP_LIB}
)

include(GoogleTest)
gtest_discover_tests(run_tests)