// state_checkpoint_test.cpp
// Copyright (C) 2025 MXRC Project
//
// T066: StateCheckpoint unit test (serialization/deserialization)

#include "core/ha/StateCheckpoint.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

using namespace mxrc::ha;

class StateCheckpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/mxrc_checkpoint_test";
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    std::filesystem::path test_dir_;
};

// T066.1: Factory creation
TEST_F(StateCheckpointTest, FactoryCreation) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);
    EXPECT_NE(manager, nullptr);
}

// T066.2: Create checkpoint
TEST_F(StateCheckpointTest, CreateCheckpoint) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    auto checkpoint = manager->createCheckpoint();

    EXPECT_FALSE(checkpoint.checkpoint_id.empty());
    EXPECT_EQ(checkpoint.process_name, "test_process");
    EXPECT_FALSE(checkpoint.is_complete);
    EXPECT_EQ(checkpoint.checkpoint_size_bytes, 0);
}

// T066.3: Save and load checkpoint
TEST_F(StateCheckpointTest, SaveAndLoadCheckpoint) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    // Create and save checkpoint
    auto checkpoint = manager->createCheckpoint();
    checkpoint.rt_state = {{"task_id", "task_001"}, {"status", "running"}};
    checkpoint.datastore_snapshot = {{"key1", "value1"}, {"key2", 42}};
    checkpoint.eventbus_queue_snapshot = {"event_001", "event_002", "event_003"};
    checkpoint.is_complete = true;

    // Calculate size
    nlohmann::json j;
    j["checkpoint_id"] = checkpoint.checkpoint_id;
    j["process_name"] = checkpoint.process_name;
    j["rt_state"] = checkpoint.rt_state;
    j["datastore_snapshot"] = checkpoint.datastore_snapshot;
    j["eventbus_queue_snapshot"] = checkpoint.eventbus_queue_snapshot;
    j["is_complete"] = checkpoint.is_complete;
    checkpoint.checkpoint_size_bytes = j.dump().size();

    EXPECT_TRUE(manager->saveCheckpoint(checkpoint));

    // Load checkpoint
    auto loaded = manager->loadCheckpoint(checkpoint.checkpoint_id);

    EXPECT_EQ(loaded.checkpoint_id, checkpoint.checkpoint_id);
    EXPECT_EQ(loaded.process_name, "test_process");
    EXPECT_EQ(loaded.rt_state["task_id"], "task_001");
    EXPECT_EQ(loaded.rt_state["status"], "running");
    EXPECT_EQ(loaded.datastore_snapshot["key1"], "value1");
    EXPECT_EQ(loaded.datastore_snapshot["key2"], 42);
    EXPECT_EQ(loaded.eventbus_queue_snapshot.size(), 3);
    EXPECT_EQ(loaded.eventbus_queue_snapshot[0], "event_001");
    EXPECT_TRUE(loaded.is_complete);
}

// T066.4: Load non-existent checkpoint
TEST_F(StateCheckpointTest, LoadNonExistentCheckpoint) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    EXPECT_THROW(manager->loadCheckpoint("non_existent_id"), std::runtime_error);
}

// T066.5: Delete checkpoint
TEST_F(StateCheckpointTest, DeleteCheckpoint) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    auto checkpoint = manager->createCheckpoint();
    checkpoint.is_complete = true;
    checkpoint.checkpoint_size_bytes = 100;
    manager->saveCheckpoint(checkpoint);

    // Verify exists
    auto checkpoints = manager->listCheckpoints();
    EXPECT_EQ(checkpoints.size(), 1);

    // Delete
    EXPECT_TRUE(manager->deleteCheckpoint(checkpoint.checkpoint_id));

    // Verify deleted
    checkpoints = manager->listCheckpoints();
    EXPECT_EQ(checkpoints.size(), 0);
}

// T066.6: Delete non-existent checkpoint
TEST_F(StateCheckpointTest, DeleteNonExistentCheckpoint) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    EXPECT_FALSE(manager->deleteCheckpoint("non_existent_id"));
}

// T066.7: List checkpoints (sorted by timestamp)
TEST_F(StateCheckpointTest, ListCheckpoints) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    // Initially empty
    auto checkpoints = manager->listCheckpoints();
    EXPECT_EQ(checkpoints.size(), 0);

    // Create 3 checkpoints
    std::vector<std::string> ids;
    for (int i = 0; i < 3; i++) {
        auto checkpoint = manager->createCheckpoint();
        checkpoint.is_complete = true;
        checkpoint.checkpoint_size_bytes = 100;
        manager->saveCheckpoint(checkpoint);
        ids.push_back(checkpoint.checkpoint_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // List should have 3 checkpoints in order
    checkpoints = manager->listCheckpoints();
    EXPECT_EQ(checkpoints.size(), 3);

    // Should be sorted by timestamp (oldest first)
    EXPECT_EQ(checkpoints[0], ids[0]);
    EXPECT_EQ(checkpoints[1], ids[1]);
    EXPECT_EQ(checkpoints[2], ids[2]);
}

// T066.8: Max checkpoints cleanup
TEST_F(StateCheckpointTest, MaxCheckpointsCleanup) {
    auto manager = createStateCheckpointManager("test_process", test_dir_, 3);

    // Create 5 checkpoints
    std::vector<std::string> ids;
    for (int i = 0; i < 5; i++) {
        auto checkpoint = manager->createCheckpoint();
        checkpoint.is_complete = true;
        checkpoint.checkpoint_size_bytes = 100;
        manager->saveCheckpoint(checkpoint);
        ids.push_back(checkpoint.checkpoint_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Should only keep last 3
    auto checkpoints = manager->listCheckpoints();
    EXPECT_EQ(checkpoints.size(), 3);

    // Should have deleted oldest 2
    EXPECT_EQ(checkpoints[0], ids[2]);
    EXPECT_EQ(checkpoints[1], ids[3]);
    EXPECT_EQ(checkpoints[2], ids[4]);
}

// T066.9: Cleanup expired checkpoints
TEST_F(StateCheckpointTest, CleanupExpiredCheckpoints) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    // Create checkpoint with old timestamp
    auto old_checkpoint = manager->createCheckpoint();
    old_checkpoint.timestamp = std::chrono::system_clock::now() - std::chrono::hours(48);
    old_checkpoint.is_complete = true;
    old_checkpoint.checkpoint_size_bytes = 100;
    manager->saveCheckpoint(old_checkpoint);

    // Create recent checkpoint
    auto recent_checkpoint = manager->createCheckpoint();
    recent_checkpoint.is_complete = true;
    recent_checkpoint.checkpoint_size_bytes = 100;
    manager->saveCheckpoint(recent_checkpoint);

    // Cleanup with 24-hour retention
    size_t deleted = manager->cleanupExpiredCheckpoints(24);

    EXPECT_EQ(deleted, 1);

    // Only recent checkpoint should remain
    auto checkpoints = manager->listCheckpoints();
    EXPECT_EQ(checkpoints.size(), 1);
    EXPECT_EQ(checkpoints[0], recent_checkpoint.checkpoint_id);
}

// T066.10: Verify checkpoint integrity
TEST_F(StateCheckpointTest, VerifyCheckpointIntegrity) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    auto checkpoint = manager->createCheckpoint();
    checkpoint.rt_state = {{"status", "running"}};
    checkpoint.is_complete = true;
    checkpoint.checkpoint_size_bytes = 200;
    manager->saveCheckpoint(checkpoint);

    EXPECT_TRUE(manager->verifyCheckpoint(checkpoint.checkpoint_id));
}

// T066.11: Verify non-existent checkpoint
TEST_F(StateCheckpointTest, VerifyNonExistentCheckpoint) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    EXPECT_FALSE(manager->verifyCheckpoint("non_existent_id"));
}

// T066.12: Checkpoint state enum to string
TEST_F(StateCheckpointTest, CheckpointStateToString) {
    EXPECT_EQ(checkpointStateToString(CheckpointState::CREATING), "CREATING");
    EXPECT_EQ(checkpointStateToString(CheckpointState::COMPLETE), "COMPLETE");
    EXPECT_EQ(checkpointStateToString(CheckpointState::LOADED), "LOADED");
    EXPECT_EQ(checkpointStateToString(CheckpointState::EXPIRED), "EXPIRED");
}

// T066.13: JSON serialization format
TEST_F(StateCheckpointTest, JSONSerializationFormat) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    auto checkpoint = manager->createCheckpoint();
    checkpoint.rt_state = {{"task_id", "task_001"}};
    checkpoint.datastore_snapshot = {{"key1", "value1"}};
    checkpoint.eventbus_queue_snapshot = {"event_001"};
    checkpoint.is_complete = true;
    checkpoint.checkpoint_size_bytes = 100;
    manager->saveCheckpoint(checkpoint);

    // Read JSON file directly
    std::ifstream file(checkpoint.file_path);
    nlohmann::json j;
    file >> j;

    EXPECT_TRUE(j.contains("checkpoint_id"));
    EXPECT_TRUE(j.contains("process_name"));
    EXPECT_TRUE(j.contains("timestamp"));
    EXPECT_TRUE(j.contains("rt_state"));
    EXPECT_TRUE(j.contains("datastore_snapshot"));
    EXPECT_TRUE(j.contains("eventbus_queue_snapshot"));
    EXPECT_TRUE(j.contains("checkpoint_size_bytes"));
    EXPECT_TRUE(j.contains("is_complete"));

    EXPECT_EQ(j["checkpoint_id"], checkpoint.checkpoint_id);
    EXPECT_EQ(j["process_name"], "test_process");
    EXPECT_TRUE(j["is_complete"]);
}

// T066.14: Empty RT state
TEST_F(StateCheckpointTest, EmptyRTState) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    auto checkpoint = manager->createCheckpoint();
    checkpoint.rt_state = nlohmann::json::object();
    checkpoint.is_complete = true;
    checkpoint.checkpoint_size_bytes = 50;
    manager->saveCheckpoint(checkpoint);

    auto loaded = manager->loadCheckpoint(checkpoint.checkpoint_id);
    EXPECT_TRUE(loaded.rt_state.is_object());
    EXPECT_TRUE(loaded.rt_state.empty());
}

// T066.15: Complex nested RT state
TEST_F(StateCheckpointTest, ComplexNestedRTState) {
    auto manager = createStateCheckpointManager("test_process", test_dir_);

    auto checkpoint = manager->createCheckpoint();
    checkpoint.rt_state = {
        {"task", {
            {"id", "task_001"},
            {"sequence", {
                {"actions", {
                    {{"type", "move"}, {"position", {{"x", 10}, {"y", 20}}}},
                    {{"type", "grasp"}, {"force", 50}}
                }}
            }}
        }},
        {"timestamp", 1234567890}
    };
    checkpoint.is_complete = true;
    checkpoint.checkpoint_size_bytes = 300;
    manager->saveCheckpoint(checkpoint);

    auto loaded = manager->loadCheckpoint(checkpoint.checkpoint_id);
    EXPECT_EQ(loaded.rt_state["task"]["id"], "task_001");
    EXPECT_EQ(loaded.rt_state["task"]["sequence"]["actions"][0]["type"], "move");
    EXPECT_EQ(loaded.rt_state["task"]["sequence"]["actions"][0]["position"]["x"], 10);
    EXPECT_EQ(loaded.rt_state["task"]["sequence"]["actions"][1]["force"], 50);
    EXPECT_EQ(loaded.rt_state["timestamp"], 1234567890);
}
