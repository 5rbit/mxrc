// TaskRegistry 클래스 테스트.
// Task 정의 등록, 조회, 제거 등의 기능 검증.

#include "gtest/gtest.h"
#include "core/task/core/TaskRegistry.h"
#include "core/task/dto/TaskDefinition.h"
#include <thread>
#include <vector>

namespace mxrc::core::task {

/**
 * @brief TaskRegistry 기본 기능 테스트
 */
class TaskRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<TaskRegistry>();
    }

    void TearDown() override {
        registry->clear();
    }

    std::unique_ptr<TaskRegistry> registry;
};

/**
 * @brief Task 정의 등록 및 조회 테스트
 */
TEST_F(TaskRegistryTest, RegisterAndGetDefinition) {
    TaskDefinition def("task1", "Test Task 1");
    def.setWork("action1");

    registry->registerDefinition(def);

    auto retrieved = registry->getDefinition("task1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, "task1");
    EXPECT_EQ(retrieved->name, "Test Task 1");
    EXPECT_EQ(retrieved->workId, "action1");
}

/**
 * @brief 존재하지 않는 Task 정의 조회 테스트
 */
TEST_F(TaskRegistryTest, GetNonExistentDefinition) {
    auto retrieved = registry->getDefinition("non_existent");
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * @brief Task 정의 존재 확인 테스트
 */
TEST_F(TaskRegistryTest, HasDefinition) {
    TaskDefinition def("task1");
    def.setWork("action1");

    EXPECT_FALSE(registry->hasDefinition("task1"));

    registry->registerDefinition(def);

    EXPECT_TRUE(registry->hasDefinition("task1"));
    EXPECT_FALSE(registry->hasDefinition("task2"));
}

/**
 * @brief 모든 Task ID 조회 테스트
 */
TEST_F(TaskRegistryTest, GetAllDefinitionIds) {
    EXPECT_TRUE(registry->getAllDefinitionIds().empty());

    TaskDefinition def1("task1");
    def1.setWork("action1");
    TaskDefinition def2("task2");
    def2.setWork("action2");
    TaskDefinition def3("task3");
    def3.setWork("action3");

    registry->registerDefinition(def1);
    registry->registerDefinition(def2);
    registry->registerDefinition(def3);

    auto ids = registry->getAllDefinitionIds();
    EXPECT_EQ(ids.size(), 3);
    EXPECT_NE(std::find(ids.begin(), ids.end(), "task1"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "task2"), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), "task3"), ids.end());
}

/**
 * @brief Task 정의 제거 테스트
 */
TEST_F(TaskRegistryTest, RemoveDefinition) {
    TaskDefinition def("task1");
    def.setWork("action1");

    registry->registerDefinition(def);
    EXPECT_TRUE(registry->hasDefinition("task1"));

    bool removed = registry->removeDefinition("task1");
    EXPECT_TRUE(removed);
    EXPECT_FALSE(registry->hasDefinition("task1"));

    // 이미 제거된 정의 다시 제거 시도
    bool removedAgain = registry->removeDefinition("task1");
    EXPECT_FALSE(removedAgain);
}

/**
 * @brief 모든 Task 정의 제거 테스트
 */
TEST_F(TaskRegistryTest, ClearAllDefinitions) {
    TaskDefinition def1("task1");
    def1.setWork("action1");
    TaskDefinition def2("task2");
    def2.setWork("action2");

    registry->registerDefinition(def1);
    registry->registerDefinition(def2);

    EXPECT_EQ(registry->getAllDefinitionIds().size(), 2);

    registry->clear();

    EXPECT_TRUE(registry->getAllDefinitionIds().empty());
    EXPECT_FALSE(registry->hasDefinition("task1"));
    EXPECT_FALSE(registry->hasDefinition("task2"));
}

/**
 * @brief 같은 ID로 중복 등록 시 덮어쓰기 테스트
 */
TEST_F(TaskRegistryTest, OverwriteDefinition) {
    TaskDefinition def1("task1", "Original Name");
    def1.setWork("action1");

    registry->registerDefinition(def1);

    auto retrieved1 = registry->getDefinition("task1");
    EXPECT_EQ(retrieved1->name, "Original Name");

    TaskDefinition def2("task1", "Updated Name");
    def2.setWork("action2");

    registry->registerDefinition(def2);

    auto retrieved2 = registry->getDefinition("task1");
    EXPECT_EQ(retrieved2->name, "Updated Name");
    EXPECT_EQ(retrieved2->workId, "action2");
}

/**
 * @brief 여러 실행 모드 Task 정의 테스트
 */
TEST_F(TaskRegistryTest, DifferentExecutionModes) {
    // ONCE 모드
    TaskDefinition def1("task1");
    def1.setWork("action1").setOnceMode();
    registry->registerDefinition(def1);

    // PERIODIC 모드
    TaskDefinition def2("task2");
    def2.setWork("action2").setPeriodicMode(std::chrono::milliseconds(100));
    registry->registerDefinition(def2);

    // TRIGGERED 모드
    TaskDefinition def3("task3");
    def3.setWork("action3").setTriggeredMode("condition == true");
    registry->registerDefinition(def3);

    auto retrieved1 = registry->getDefinition("task1");
    auto retrieved2 = registry->getDefinition("task2");
    auto retrieved3 = registry->getDefinition("task3");

    EXPECT_EQ(retrieved1->executionMode, TaskExecutionMode::ONCE);
    EXPECT_EQ(retrieved2->executionMode, TaskExecutionMode::PERIODIC);
    EXPECT_EQ(retrieved3->executionMode, TaskExecutionMode::TRIGGERED);

    EXPECT_TRUE(retrieved2->periodicInterval.has_value());
    EXPECT_EQ(retrieved2->periodicInterval.value(), std::chrono::milliseconds(100));

    EXPECT_TRUE(retrieved3->triggerCondition.has_value());
    EXPECT_EQ(retrieved3->triggerCondition.value(), "condition == true");
}

/**
 * @brief Action과 Sequence 작업 타입 테스트
 */
TEST_F(TaskRegistryTest, DifferentWorkTypes) {
    // ACTION 타입
    TaskDefinition def1("task1");
    def1.setWork("action1");
    registry->registerDefinition(def1);

    // SEQUENCE 타입
    TaskDefinition def2("task2");
    def2.setWorkSequence("sequence1");
    registry->registerDefinition(def2);

    auto retrieved1 = registry->getDefinition("task1");
    auto retrieved2 = registry->getDefinition("task2");

    EXPECT_EQ(retrieved1->workType, TaskWorkType::ACTION);
    EXPECT_EQ(retrieved1->workId, "action1");

    EXPECT_EQ(retrieved2->workType, TaskWorkType::SEQUENCE);
    EXPECT_EQ(retrieved2->workId, "sequence1");
}

/**
 * @brief 멀티스레드 환경에서의 동시 등록 테스트
 */
TEST_F(TaskRegistryTest, ConcurrentRegistration) {
    const int NUM_THREADS = 10;
    const int TASKS_PER_THREAD = 10;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < TASKS_PER_THREAD; ++i) {
                std::string id = "task_" + std::to_string(t) + "_" + std::to_string(i);
                TaskDefinition def(id);
                def.setWork("action_" + id);
                registry->registerDefinition(def);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto ids = registry->getAllDefinitionIds();
    EXPECT_EQ(ids.size(), NUM_THREADS * TASKS_PER_THREAD);
}

/**
 * @brief 멀티스레드 환경에서의 동시 조회 테스트
 */
TEST_F(TaskRegistryTest, ConcurrentRetrieval) {
    // 먼저 정의 등록
    for (int i = 0; i < 100; ++i) {
        std::string id = "task_" + std::to_string(i);
        TaskDefinition def(id);
        def.setWork("action_" + id);
        registry->registerDefinition(def);
    }

    const int NUM_THREADS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, &successCount]() {
            for (int i = 0; i < 100; ++i) {
                std::string id = "task_" + std::to_string(i);
                auto def = registry->getDefinition(id);
                if (def != nullptr && def->id == id) {
                    successCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), NUM_THREADS * 100);
}

/**
 * @brief 멀티스레드 환경에서의 등록/조회/삭제 혼합 테스트
 */
TEST_F(TaskRegistryTest, ConcurrentMixedOperations) {
    const int NUM_THREADS = 5;
    std::vector<std::thread> threads;

    // 등록 스레드
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 20; ++i) {
                std::string id = "task_reg_" + std::to_string(t) + "_" + std::to_string(i);
                TaskDefinition def(id);
                def.setWork("action_" + id);
                registry->registerDefinition(def);
            }
        });
    }

    // 조회 스레드
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 20; ++i) {
                std::string id = "task_reg_" + std::to_string(t) + "_" + std::to_string(i);
                auto def = registry->getDefinition(id);
                // 정의가 존재할 수도 있고 아직 등록되지 않았을 수도 있음
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 최종적으로 모든 정의가 등록되었는지 확인
    auto ids = registry->getAllDefinitionIds();
    EXPECT_EQ(ids.size(), NUM_THREADS * 20);
}

} // namespace mxrc::core::task
