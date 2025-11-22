#include <gtest/gtest.h>
#include "core/rt/ipc/SharedMemory.h"
#include "core/rt/ipc/SharedMemoryData.h"
#include "core/nonrt/NonRTExecutive.h"
#include "core/event/core/EventBus.h"
#include "core/datastore/DataStore.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <atomic>

using namespace mxrc::core;

/**
 * @brief Integration test for Feature 022 P1: systemd startup order
 *
 * Tests:
 * 1. RT process creates shared memory FIRST
 * 2. Non-RT process connects AFTER RT is ready
 * 3. Startup order is deterministic (no race condition)
 *
 * Simulates systemd startup sequence:
 * - RT starts (creates shared memory)
 * - RT signals READY (in real system, sd_notify)
 * - Non-RT starts (connects with retry logic)
 * - Non-RT succeeds on first attempt
 */
class StartupOrderTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::debug);
        shm_name_ = "/mxrc_test_startup_order";
    }

    void TearDown() override {
        // Clean up shared memory
        rt::ipc::SharedMemoryRegion cleanup_region;
        cleanup_region.unlink(shm_name_);
    }

    std::string shm_name_;
};

// Test 1: RT creates shared memory first
TEST_F(StartupOrderTest, RT_CreatesSharedMemory_BeforeNonRT) {
    // Simulate RT process startup
    rt::ipc::SharedMemoryRegion rt_shm;
    ASSERT_EQ(rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData)), 0)
        << "RT should successfully create shared memory";

    auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
    ASSERT_NE(shm_data, nullptr) << "RT shared memory pointer should be valid";

    // Initialize shared memory
    new (shm_data) rt::ipc::SharedMemoryData();

    // Verify RT can write to shared memory
    shm_data->rt_heartbeat_ns.store(12345, std::memory_order_release);
    EXPECT_EQ(shm_data->rt_heartbeat_ns.load(std::memory_order_acquire), 12345u);

    spdlog::info("RT shared memory created and initialized");

    // Simulate Non-RT process connecting AFTER RT is ready
    rt::ipc::SharedMemoryRegion nonrt_shm;
    ASSERT_EQ(nonrt_shm.open(shm_name_), 0)
        << "Non-RT should successfully connect to existing shared memory";

    auto* nonrt_shm_data = static_cast<rt::ipc::SharedMemoryData*>(nonrt_shm.getPtr());
    ASSERT_NE(nonrt_shm_data, nullptr) << "Non-RT shared memory pointer should be valid";

    // Verify Non-RT can read RT's data
    EXPECT_EQ(nonrt_shm_data->rt_heartbeat_ns.load(std::memory_order_acquire), 12345u)
        << "Non-RT should see RT's heartbeat";

    spdlog::info("Non-RT successfully connected to RT shared memory");
}

// Test 2: Non-RT fails if RT hasn't started yet
TEST_F(StartupOrderTest, NonRT_FailsImmediately_IfRT_NotStarted) {
    // Simulate Non-RT trying to connect WITHOUT RT running
    rt::ipc::SharedMemoryRegion nonrt_shm;

    // This should fail because RT hasn't created shared memory yet
    ASSERT_NE(nonrt_shm.open(shm_name_), 0)
        << "Non-RT should fail to connect when RT hasn't started";

    spdlog::info("Non-RT correctly failed to connect (RT not started)");
}

// Test 3: Non-RT succeeds with retry logic after RT starts
TEST_F(StartupOrderTest, NonRT_Succeeds_WithRetryLogic_AfterRT_Starts) {
    std::atomic<bool> rt_ready{false};
    std::atomic<bool> nonrt_connected{false};

    // Simulate RT process starting with delay
    std::thread rt_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Delay 500ms

        rt::ipc::SharedMemoryRegion rt_shm;
        ASSERT_EQ(rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData)), 0);

        auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
        new (shm_data) rt::ipc::SharedMemoryData();

        rt_ready = true;
        spdlog::info("RT shared memory created (after 500ms delay)");

        // Keep shared memory alive
        std::this_thread::sleep_for(std::chrono::seconds(2));
    });

    // Simulate Non-RT process with retry logic
    std::thread nonrt_thread([&]() {
        rt::ipc::SharedMemoryRegion nonrt_shm;

        const int MAX_RETRIES = 50;
        const int RETRY_INTERVAL_MS = 100;

        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
            if (nonrt_shm.open(shm_name_) == 0) {
                nonrt_connected = true;
                spdlog::info("Non-RT connected on attempt {}", attempt + 1);
                break;
            }

            if (attempt < MAX_RETRIES - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
            }
        }

        EXPECT_TRUE(nonrt_connected.load()) << "Non-RT should eventually connect";
    });

    rt_thread.join();
    nonrt_thread.join();

    EXPECT_TRUE(rt_ready.load()) << "RT should have started";
    EXPECT_TRUE(nonrt_connected.load()) << "Non-RT should have connected";

    spdlog::info("Startup order test passed: RT started, Non-RT connected with retry");
}

// Test 4: Non-RT timeout after max retries (RT never starts)
TEST_F(StartupOrderTest, NonRT_TimesOut_IfRT_NeverStarts) {
    rt::ipc::SharedMemoryRegion nonrt_shm;

    const int MAX_RETRIES = 10;  // Reduced for faster test
    const int RETRY_INTERVAL_MS = 50;

    auto start_time = std::chrono::steady_clock::now();

    int attempts = 0;
    for (attempts = 0; attempts < MAX_RETRIES; ++attempts) {
        if (nonrt_shm.open(shm_name_) == 0) {
            FAIL() << "Non-RT should NOT connect (RT never started)";
        }

        if (attempts < MAX_RETRIES - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    EXPECT_EQ(attempts, MAX_RETRIES) << "Non-RT should have exhausted all retries";
    EXPECT_GE(elapsed_ms, (MAX_RETRIES - 1) * RETRY_INTERVAL_MS)
        << "Retry duration should be at least " << (MAX_RETRIES - 1) * RETRY_INTERVAL_MS << "ms";

    spdlog::info("Non-RT correctly timed out after {} attempts ({}ms)", attempts, elapsed_ms);
}

// Test 5: NonRTExecutive integration with retry logic
TEST_F(StartupOrderTest, NonRTExecutive_Integration_WithRetryLogic) {
    std::atomic<bool> rt_started{false};

    // Start RT process in background
    std::thread rt_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // Small delay

        rt::ipc::SharedMemoryRegion rt_shm;
        ASSERT_EQ(rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData)), 0);

        auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
        new (shm_data) rt::ipc::SharedMemoryData();

        rt_started = true;
        spdlog::info("RT process started");

        // Keep alive
        std::this_thread::sleep_for(std::chrono::seconds(2));
    });

    // Wait a bit to ensure RT hasn't started yet
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(rt_started.load()) << "RT should not have started yet";

    // Create NonRTExecutive (will use retry logic in init())
    auto datastore = std::make_shared<DataStore>();
    auto event_bus = std::make_shared<event::EventBus>();
    nonrt::NonRTExecutive executive(shm_name_, datastore, event_bus);

    // init() should succeed after RT starts (within 200ms + retries)
    ASSERT_EQ(executive.init(), 0) << "NonRTExecutive should connect after RT starts";

    spdlog::info("NonRTExecutive successfully initialized with retry logic");

    rt_thread.join();

    EXPECT_TRUE(rt_started.load()) << "RT should have started";
}

// Main is provided by the run_tests executable
