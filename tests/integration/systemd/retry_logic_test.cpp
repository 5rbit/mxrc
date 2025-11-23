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
 * @brief Integration test for Feature 022 P1: Non-RT retry logic
 *
 * Tests:
 * 1. Non-RT retries every 100ms for up to 5 seconds
 * 2. Non-RT succeeds on first successful connection
 * 3. Non-RT fails gracefully after max retries
 * 4. Retry timing is accurate (fixed 100ms interval)
 */
class RetryLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::debug);
        shm_name_ = "/mxrc_test_retry_logic";
    }

    void TearDown() override {
        // Clean up shared memory
        rt::ipc::SharedMemoryRegion cleanup_region;
        cleanup_region.unlink(shm_name_);
    }

    std::string shm_name_;
};

// Test 1: Retry succeeds on attempt 1 (RT already running)
TEST_F(RetryLogicTest, Retry_SucceedsOnAttempt1_WhenRT_AlreadyRunning) {
    // RT already running
    rt::ipc::SharedMemoryRegion rt_shm;
    ASSERT_EQ(rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData)), 0);
    auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
    new (shm_data) rt::ipc::SharedMemoryData();

    // Non-RT should connect immediately
    auto datastore = std::make_shared<DataStore>();
    auto event_bus = std::make_shared<event::EventBus>();
    nonrt::NonRTExecutive executive(shm_name_, datastore, event_bus);

    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(executive.init(), 0) << "Should succeed on first attempt";
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // Should be very fast (< 50ms)
    EXPECT_LT(elapsed, 50) << "Connection should be immediate (attempt 1)";
    spdlog::info("Connected on attempt 1 in {}ms", elapsed);
}

// Test 2: Retry succeeds on attempt 5 (RT starts after 400ms)
TEST_F(RetryLogicTest, Retry_SucceedsOnAttempt5_WhenRT_StartsAfter400ms) {
    std::atomic<bool> rt_started{false};

    // RT starts after 400ms delay
    std::thread rt_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));

        rt::ipc::SharedMemoryRegion rt_shm;
        rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData));
        auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
        new (shm_data) rt::ipc::SharedMemoryData();

        rt_started = true;
        spdlog::info("RT started after 400ms");

        std::this_thread::sleep_for(std::chrono::seconds(2));
    });

    // Non-RT starts immediately (will retry)
    auto datastore = std::make_shared<DataStore>();
    auto event_bus = std::make_shared<event::EventBus>();
    nonrt::NonRTExecutive executive(shm_name_, datastore, event_bus);

    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(executive.init(), 0) << "Should succeed after retries";
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    rt_thread.join();

    // Should connect around attempt 5 (400-500ms)
    EXPECT_GE(elapsed, 400) << "Should wait at least 400ms";
    EXPECT_LT(elapsed, 600) << "Should connect within 600ms";

    spdlog::info("Connected after {}ms (expected ~400-500ms for attempt 5)", elapsed);
}

// Test 3: Retry fails after max retries (RT never starts)
TEST_F(RetryLogicTest, Retry_FailsAfterMaxRetries_WhenRT_NeverStarts) {
    // RT never starts
    auto datastore = std::make_shared<DataStore>();
    auto event_bus = std::make_shared<event::EventBus>();
    nonrt::NonRTExecutive executive(shm_name_, datastore, event_bus);

    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(executive.init(), -1) << "Should fail after max retries";
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // Should take ~5 seconds (50 retries × 100ms)
    EXPECT_GE(elapsed, 4900) << "Should wait at least 4.9 seconds";
    EXPECT_LT(elapsed, 5200) << "Should timeout within 5.2 seconds";

    spdlog::info("Timed out after {}ms (expected ~5000ms)", elapsed);
}

// Test 4: Retry timing accuracy (100ms fixed interval)
TEST_F(RetryLogicTest, Retry_TimingAccuracy_100ms_FixedInterval) {
    std::vector<uint64_t> retry_timestamps;
    std::atomic<int> retry_count{0};

    // RT starts after 1 second
    std::thread rt_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        rt::ipc::SharedMemoryRegion rt_shm;
        rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData));
        auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
        new (shm_data) rt::ipc::SharedMemoryData();

        spdlog::info("RT started after 1 second");
        std::this_thread::sleep_for(std::chrono::seconds(2));
    });

    // Non-RT with manual retry logic to measure intervals
    rt::ipc::SharedMemoryRegion nonrt_shm;
    const int MAX_RETRIES = 50;
    const int RETRY_INTERVAL_MS = 100;

    auto start = std::chrono::steady_clock::now();
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        retry_timestamps.push_back(elapsed);

        if (nonrt_shm.open(shm_name_) == 0) {
            spdlog::info("Connected on attempt {} after {}ms", attempt + 1, elapsed);
            break;
        }

        if (attempt < MAX_RETRIES - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_INTERVAL_MS));
        }
        retry_count++;
    }

    rt_thread.join();

    // Verify retry intervals
    ASSERT_GE(retry_timestamps.size(), 10u) << "Should have at least 10 retries";

    for (size_t i = 1; i < retry_timestamps.size(); ++i) {
        uint64_t interval = retry_timestamps[i] - retry_timestamps[i - 1];
        // Allow ±10ms tolerance for scheduling jitter
        EXPECT_GE(interval, 90u) << "Interval " << i << " too short";
        EXPECT_LE(interval, 110u) << "Interval " << i << " too long";
    }

    spdlog::info("Retry timing accuracy verified: {} intervals measured", retry_timestamps.size() - 1);
}

// Test 5: Concurrent startup (multiple Non-RT processes)
TEST_F(RetryLogicTest, Concurrent_NonRT_Processes_CanConnect) {
    // RT starts after 300ms
    std::thread rt_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        rt::ipc::SharedMemoryRegion rt_shm;
        rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData));
        auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
        new (shm_data) rt::ipc::SharedMemoryData();

        spdlog::info("RT started");
        std::this_thread::sleep_for(std::chrono::seconds(3));
    });

    // 3 Non-RT processes start concurrently
    const int NUM_NONRT = 3;
    std::vector<std::thread> nonrt_threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < NUM_NONRT; ++i) {
        nonrt_threads.emplace_back([&, i]() {
            auto datastore = std::make_shared<DataStore>();
            auto event_bus = std::make_shared<event::EventBus>();
            nonrt::NonRTExecutive executive(shm_name_, datastore, event_bus);

            if (executive.init() == 0) {
                success_count++;
                spdlog::info("Non-RT process {} connected successfully", i);
            }
        });
    }

    for (auto& t : nonrt_threads) {
        t.join();
    }

    rt_thread.join();

    EXPECT_EQ(success_count, NUM_NONRT) << "All Non-RT processes should connect";
    spdlog::info("{}/{} Non-RT processes connected successfully", success_count.load(), NUM_NONRT);
}

// Test 6: Retry after RT crash and restart
TEST_F(RetryLogicTest, Retry_SucceedsAfterRT_CrashAndRestart) {
    // RT starts, then crashes, then restarts
    std::atomic<int> rt_lifecycle{0};  // 0=not started, 1=running, 2=crashed, 3=restarted

    std::thread rt_thread([&]() {
        // Initial RT startup
        {
            rt::ipc::SharedMemoryRegion rt_shm;
            rt_shm.create(shm_name_, sizeof(rt::ipc::SharedMemoryData));
            auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm.getPtr());
            new (shm_data) rt::ipc::SharedMemoryData();

            rt_lifecycle = 1;
            spdlog::info("RT started (lifecycle=1)");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // RT crashes (shared memory destroyed)
            rt_lifecycle = 2;
            spdlog::info("RT crashed (lifecycle=2)");
        }  // rt_shm destructor unlinks shared memory

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // RT restarts
        {
            rt::ipc::SharedMemoryRegion rt_shm_restart;
            rt_shm_restart.create(shm_name_, sizeof(rt::ipc::SharedMemoryData));
            auto* shm_data = static_cast<rt::ipc::SharedMemoryData*>(rt_shm_restart.getPtr());
            new (shm_data) rt::ipc::SharedMemoryData();

            rt_lifecycle = 3;
            spdlog::info("RT restarted (lifecycle=3)");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    // Wait for RT to crash
    while (rt_lifecycle.load() < 2) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Non-RT starts AFTER RT crash (will retry until RT restarts)
    auto datastore = std::make_shared<DataStore>();
    auto event_bus = std::make_shared<event::EventBus>();
    nonrt::NonRTExecutive executive(shm_name_, datastore, event_bus);

    ASSERT_EQ(executive.init(), 0) << "Should connect after RT restarts";

    rt_thread.join();

    EXPECT_EQ(rt_lifecycle, 3) << "RT should have restarted";
    spdlog::info("Non-RT successfully connected after RT crash and restart");
}

// Main is provided by the run_tests executable
