#include <gtest/gtest.h>
#include "core/rt/perf/CPUAffinityManager.h"
#include <thread>
#include <fstream>

using namespace mxrc::rt::perf;

class CPUAffinityManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<CPUAffinityManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<CPUAffinityManager> manager_;
};

TEST_F(CPUAffinityManagerTest, DefaultConfig) {
    CPUAffinityConfig config;

    EXPECT_EQ(config.process_name, "");
    EXPECT_EQ(config.isolation_mode, IsolationMode::NONE);
    EXPECT_TRUE(config.is_exclusive);
    EXPECT_EQ(config.priority, 80);
    EXPECT_EQ(config.policy, SchedPolicy::FIFO);
}

TEST_F(CPUAffinityManagerTest, GetCurrentAffinity) {
    auto cores = manager_->getCurrentAffinity();

    // Should have at least one CPU core assigned
    EXPECT_GT(cores.size(), 0);

    std::cout << "Current CPU affinity: ";
    for (int core : cores) {
        std::cout << core << " ";
    }
    std::cout << std::endl;
}

TEST_F(CPUAffinityManagerTest, SetCPUAffinityToCore0) {
    CPUAffinityConfig config;
    config.process_name = "test_process";
    config.cpu_cores = {0};  // Pin to core 0
    config.policy = SchedPolicy::OTHER;  // Use SCHED_OTHER to avoid needing root
    config.priority = 0;

    // Apply configuration
    bool result = manager_->apply(config);

    // On systems without RT capabilities, this might fail
    // but we should still be able to set affinity
    if (!result) {
        std::cout << "Note: Could not set RT priority (may need CAP_SYS_NICE)" << std::endl;
    }

    // Check affinity was set
    auto current_cores = manager_->getCurrentAffinity();

    // We should be pinned to core 0 only
    EXPECT_EQ(current_cores.size(), 1);
    if (current_cores.size() > 0) {
        EXPECT_EQ(current_cores[0], 0);
    }
}

TEST_F(CPUAffinityManagerTest, SetCPUAffinityMultipleCores) {
    CPUAffinityConfig config;
    config.process_name = "test_process";
    config.cpu_cores = {0, 1};  // Pin to cores 0 and 1
    config.policy = SchedPolicy::OTHER;
    config.priority = 0;

    bool result = manager_->apply(config);

    if (!result) {
        std::cout << "Note: Could not apply full configuration" << std::endl;
    }

    auto current_cores = manager_->getCurrentAffinity();

    // Should be pinned to cores 0 and 1
    EXPECT_GE(current_cores.size(), 1);
    EXPECT_LE(current_cores.size(), 2);
}

TEST_F(CPUAffinityManagerTest, CPUAffinityGuard) {
    // Save original affinity
    auto original_cores = manager_->getCurrentAffinity();

    {
        // Apply temporary affinity with guard
        CPUAffinityConfig config;
        config.cpu_cores = {0};
        config.policy = SchedPolicy::OTHER;
        config.priority = 0;

        CPUAffinityGuard guard(*manager_, config);

        // Inside guard scope - should be pinned to core 0
        auto temp_cores = manager_->getCurrentAffinity();
        EXPECT_EQ(temp_cores.size(), 1);
        if (temp_cores.size() > 0) {
            EXPECT_EQ(temp_cores[0], 0);
        }
    }

    // After guard is destroyed - should be restored
    auto restored_cores = manager_->getCurrentAffinity();
    EXPECT_EQ(restored_cores.size(), original_cores.size());
}

TEST_F(CPUAffinityManagerTest, LoadConfigFromJSON) {
    // Create temporary JSON config file
    std::string config_path = "/tmp/test_cpu_affinity.json";
    std::ofstream config_file(config_path);
    config_file << R"({
        "process_name": "test_rt_process",
        "thread_name": "main",
        "cpu_cores": [0, 1],
        "isolation_mode": "HYBRID",
        "is_exclusive": true,
        "priority": 90,
        "policy": "SCHED_FIFO"
    })";
    config_file.close();

    // Load configuration
    bool result = manager_->loadConfig(config_path);
    EXPECT_TRUE(result);

    // Clean up
    std::remove(config_path.c_str());
}

TEST_F(CPUAffinityManagerTest, InvalidCoreNumber) {
    CPUAffinityConfig config;
    config.cpu_cores = {9999};  // Invalid core number
    config.policy = SchedPolicy::OTHER;
    config.priority = 0;

    // This should fail
    bool result = manager_->apply(config);
    EXPECT_FALSE(result);
}

TEST_F(CPUAffinityManagerTest, EmptyCoreList) {
    CPUAffinityConfig config;
    config.cpu_cores = {};  // Empty core list
    config.policy = SchedPolicy::OTHER;
    config.priority = 0;

    // This should fail
    bool result = manager_->apply(config);
    EXPECT_FALSE(result);
}

TEST_F(CPUAffinityManagerTest, SchedulingPolicyEnumValues) {
    EXPECT_EQ(std::string(schedPolicyToString(SchedPolicy::OTHER)), "SCHED_OTHER");
    EXPECT_EQ(std::string(schedPolicyToString(SchedPolicy::FIFO)), "SCHED_FIFO");
    EXPECT_EQ(std::string(schedPolicyToString(SchedPolicy::RR)), "SCHED_RR");
    EXPECT_EQ(std::string(schedPolicyToString(SchedPolicy::DEADLINE)), "SCHED_DEADLINE");
}

TEST_F(CPUAffinityManagerTest, IsolationModeEnumValues) {
    EXPECT_EQ(std::string(isolationModeToString(IsolationMode::NONE)), "NONE");
    EXPECT_EQ(std::string(isolationModeToString(IsolationMode::ISOLCPUS)), "ISOLCPUS");
    EXPECT_EQ(std::string(isolationModeToString(IsolationMode::CGROUPS)), "CGROUPS");
    EXPECT_EQ(std::string(isolationModeToString(IsolationMode::HYBRID)), "HYBRID");
}
