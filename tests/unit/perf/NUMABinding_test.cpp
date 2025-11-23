#include <gtest/gtest.h>
#include "core/rt/perf/NUMABinding.h"
#include <fstream>

using namespace mxrc::rt::perf;

class NUMABindingTest : public ::testing::Test {
protected:
    void SetUp() override {
        binding_ = std::make_unique<NUMABinding>();
    }

    void TearDown() override {
        binding_.reset();
    }

    std::unique_ptr<NUMABinding> binding_;
};

TEST_F(NUMABindingTest, DefaultConfig) {
    NUMABindingConfig config;

    EXPECT_EQ(config.process_name, "");
    EXPECT_EQ(config.numa_node, 0);
    EXPECT_EQ(config.memory_policy, MemoryPolicy::LOCAL);
    EXPECT_TRUE(config.strict_binding);
    EXPECT_FALSE(config.migrate_pages);
}

TEST_F(NUMABindingTest, IsAvailable) {
    bool available = NUMABinding::isAvailable();

    std::cout << "NUMA available: " << (available ? "yes" : "no") << std::endl;

    if (available) {
        int num_nodes = NUMABinding::getNumNodes();
        std::cout << "Number of NUMA nodes: " << num_nodes << std::endl;
        EXPECT_GT(num_nodes, 0);
    } else {
        std::cout << "Note: NUMA not available on this system" << std::endl;
    }
}

TEST_F(NUMABindingTest, ApplyLocalPolicy) {
    if (!NUMABinding::isAvailable()) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    NUMABindingConfig config;
    config.process_name = "test_process";
    config.numa_node = 0;
    config.memory_policy = MemoryPolicy::LOCAL;
    config.strict_binding = false;  // Don't fail if binding doesn't work

    bool result = binding_->apply(config);

    if (result) {
        std::cout << "Successfully applied NUMA LOCAL policy" << std::endl;

        // Verify binding
        bool verified = binding_->verifyBinding(config);
        EXPECT_TRUE(verified);
    } else {
        std::cout << "Note: Could not apply NUMA binding (may need permissions)" << std::endl;
    }
}

TEST_F(NUMABindingTest, GetStats) {
    auto stats = binding_->getStats(0);  // Current process

    std::cout << "NUMA Stats:" << std::endl;
    std::cout << "  Total pages: " << stats.total_pages << std::endl;
    std::cout << "  Local pages: " << stats.local_pages << std::endl;
    std::cout << "  Remote pages: " << stats.remote_pages << std::endl;
    std::cout << "  Local access %: " << stats.local_access_percent << std::endl;

    // Stats should be valid
    EXPECT_GE(stats.total_pages, 0);
    EXPECT_GE(stats.local_access_percent, 0.0);
    EXPECT_LE(stats.local_access_percent, 100.0);
}

TEST_F(NUMABindingTest, NUMABindingGuard) {
    if (!NUMABinding::isAvailable()) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    {
        NUMABindingConfig config;
        config.numa_node = 0;
        config.memory_policy = MemoryPolicy::LOCAL;
        config.strict_binding = false;

        NUMABindingGuard guard(*binding_, config);

        // Inside guard scope - NUMA binding should be applied
        auto stats = binding_->getStats(0);
        std::cout << "Inside guard - local access: " << stats.local_access_percent << "%" << std::endl;
    }

    // After guard is destroyed - should be restored
    std::cout << "After guard destroyed" << std::endl;
}

TEST_F(NUMABindingTest, LoadConfigFromJSON) {
    // Create temporary JSON config file
    std::string config_path = "/tmp/test_numa_binding.json";
    std::ofstream config_file(config_path);
    config_file << R"({
        "process_name": "test_rt_process",
        "numa_node": 0,
        "memory_policy": "LOCAL",
        "strict_binding": true,
        "migrate_pages": false,
        "cpu_cores_hint": [0, 1]
    })";
    config_file.close();

    // Load configuration
    bool result = binding_->loadConfig(config_path);
    EXPECT_TRUE(result);

    // Clean up
    std::remove(config_path.c_str());
}

TEST_F(NUMABindingTest, InvalidNodeNumber) {
    if (!NUMABinding::isAvailable()) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    NUMABindingConfig config;
    config.numa_node = 9999;  // Invalid node number
    config.memory_policy = MemoryPolicy::LOCAL;
    config.strict_binding = false;

    // This should fail
    bool result = binding_->apply(config);
    EXPECT_FALSE(result);
}

TEST_F(NUMABindingTest, MemoryPolicyEnumValues) {
    EXPECT_EQ(std::string(memoryPolicyToString(MemoryPolicy::DEFAULT)), "DEFAULT");
    EXPECT_EQ(std::string(memoryPolicyToString(MemoryPolicy::BIND)), "BIND");
    EXPECT_EQ(std::string(memoryPolicyToString(MemoryPolicy::PREFERRED)), "PREFERRED");
    EXPECT_EQ(std::string(memoryPolicyToString(MemoryPolicy::INTERLEAVE)), "INTERLEAVE");
    EXPECT_EQ(std::string(memoryPolicyToString(MemoryPolicy::LOCAL)), "LOCAL");
}

TEST_F(NUMABindingTest, MultipleNodeSystem) {
    if (!NUMABinding::isAvailable()) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    int num_nodes = NUMABinding::getNumNodes();

    if (num_nodes > 1) {
        std::cout << "Multi-node system detected (" << num_nodes << " nodes)" << std::endl;

        // Test binding to different nodes
        for (int node = 0; node < std::min(num_nodes, 2); ++node) {
            NUMABindingConfig config;
            config.numa_node = node;
            config.memory_policy = MemoryPolicy::PREFERRED;
            config.strict_binding = false;

            bool result = binding_->apply(config);
            std::cout << "  Binding to node " << node << ": "
                     << (result ? "success" : "failed") << std::endl;
        }
    } else {
        std::cout << "Single-node system (UMA)" << std::endl;
    }
}
