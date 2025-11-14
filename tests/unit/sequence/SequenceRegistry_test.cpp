#include <gtest/gtest.h>
#include "core/sequence/core/SequenceRegistry.h"

using namespace mxrc::core::sequence;

/**
 * @brief SequenceRegistry 단위 테스트
 */
class SequenceRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry = std::make_unique<SequenceRegistry>();
    }

    std::unique_ptr<SequenceRegistry> registry;
};

// Sequence 정의 등록 및 조회
TEST_F(SequenceRegistryTest, RegisterAndRetrieveDefinition) {
    SequenceDefinition def("seq1", "Test Sequence");
    def.setDescription("Test sequence for unit testing");
    
    ActionStep step1("action1", "Delay");
    step1.addParameter("delay_ms", "100");
    def.addStep(step1);

    registry->registerDefinition(def);

    auto retrieved = registry->getDefinition("seq1");

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, "seq1");
    EXPECT_EQ(retrieved->name, "Test Sequence");
    EXPECT_EQ(retrieved->description, "Test sequence for unit testing");
    EXPECT_EQ(retrieved->steps.size(), 1);
    EXPECT_EQ(retrieved->steps[0].actionId, "action1");
}

// Sequence 정의 존재 여부 확인
TEST_F(SequenceRegistryTest, CheckDefinitionExists) {
    SequenceDefinition def("seq1", "Test Sequence");
    registry->registerDefinition(def);

    EXPECT_TRUE(registry->hasDefinition("seq1"));
    EXPECT_FALSE(registry->hasDefinition("seq2"));
}

// 모든 정의 ID 조회
TEST_F(SequenceRegistryTest, GetAllDefinitionIds) {
    SequenceDefinition def1("seq1", "Sequence 1");
    SequenceDefinition def2("seq2", "Sequence 2");

    registry->registerDefinition(def1);
    registry->registerDefinition(def2);

    auto ids = registry->getAllDefinitionIds();

    EXPECT_EQ(ids.size(), 2);
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "seq1") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "seq2") != ids.end());
}

// 정의 제거
TEST_F(SequenceRegistryTest, RemoveDefinition) {
    SequenceDefinition def("seq1", "Test Sequence");
    registry->registerDefinition(def);

    EXPECT_TRUE(registry->hasDefinition("seq1"));
    
    bool removed = registry->removeDefinition("seq1");
    
    EXPECT_TRUE(removed);
    EXPECT_FALSE(registry->hasDefinition("seq1"));
}

// 존재하지 않는 정의 제거 시도
TEST_F(SequenceRegistryTest, RemoveNonExistentDefinition) {
    bool removed = registry->removeDefinition("nonexistent");
    EXPECT_FALSE(removed);
}

// 정의 덮어쓰기
TEST_F(SequenceRegistryTest, OverwriteDefinition) {
    SequenceDefinition def1("seq1", "Original");
    def1.setDescription("Original description");
    registry->registerDefinition(def1);

    SequenceDefinition def2("seq1", "Updated");
    def2.setDescription("Updated description");
    registry->registerDefinition(def2);

    auto retrieved = registry->getDefinition("seq1");

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Updated");
    EXPECT_EQ(retrieved->description, "Updated description");
}

// 모든 정의 삭제
TEST_F(SequenceRegistryTest, ClearAllDefinitions) {
    SequenceDefinition def1("seq1", "Sequence 1");
    SequenceDefinition def2("seq2", "Sequence 2");

    registry->registerDefinition(def1);
    registry->registerDefinition(def2);

    EXPECT_EQ(registry->getAllDefinitionIds().size(), 2);

    registry->clear();

    EXPECT_EQ(registry->getAllDefinitionIds().size(), 0);
    EXPECT_FALSE(registry->hasDefinition("seq1"));
    EXPECT_FALSE(registry->hasDefinition("seq2"));
}
