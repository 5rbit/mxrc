// HotKeyCache Unit Test
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Tests: T024 - Correctness verification

#include <gtest/gtest.h>
#include "hotkey/HotKeyCache.h"
#include <thread>
#include <vector>
#include <array>

using namespace mxrc::core::datastore;

/**
 * @brief Unit test suite for HotKeyCache
 *
 * Tests:
 * 1. Basic read/write operations
 * 2. Type safety
 * 3. Capacity limits
 * 4. Concurrent access
 * 5. Version consistency (seqlock)
 */
class HotKeyCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<HotKeyCache>(32);
    }

    void TearDown() override {
        cache.reset();
    }

    std::unique_ptr<HotKeyCache> cache;
};

// ============================================================================
// Test 1: Registration
// ============================================================================

TEST_F(HotKeyCacheTest, RegisterHotKey) {
    EXPECT_TRUE(cache->registerHotKey("test_key_1"));
    EXPECT_TRUE(cache->isHotKey("test_key_1"));
    EXPECT_FALSE(cache->isHotKey("non_existent_key"));
    EXPECT_EQ(cache->getHotKeyCount(), 1);
}

TEST_F(HotKeyCacheTest, RegisterDuplicateKey) {
    EXPECT_TRUE(cache->registerHotKey("test_key"));
    EXPECT_TRUE(cache->registerHotKey("test_key"));  // Should not fail
    EXPECT_EQ(cache->getHotKeyCount(), 1);  // Count should not increase
}

TEST_F(HotKeyCacheTest, CapacityLimit) {
    // Register 32 keys (maximum capacity)
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_TRUE(cache->registerHotKey("key_" + std::to_string(i)));
    }
    EXPECT_EQ(cache->getHotKeyCount(), 32);

    // 33rd key should fail
    EXPECT_FALSE(cache->registerHotKey("key_overflow"));
    EXPECT_EQ(cache->getHotKeyCount(), 32);
}

// ============================================================================
// Test 2: Basic Read/Write
// ============================================================================

TEST_F(HotKeyCacheTest, SetAndGetDouble) {
    cache->registerHotKey("robot_position_x");

    EXPECT_TRUE(cache->set("robot_position_x", 123.456));

    auto value = cache->get<double>("robot_position_x");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(value.value(), 123.456);
}

TEST_F(HotKeyCacheTest, SetAndGetInt) {
    cache->registerHotKey("cycle_count");

    EXPECT_TRUE(cache->set("cycle_count", 42));

    auto value = cache->get<int>("cycle_count");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), 42);
}

TEST_F(HotKeyCacheTest, SetAndGetArray) {
    cache->registerHotKey("motor_positions");

    std::array<double, 64> positions;
    positions.fill(3.14159);

    EXPECT_TRUE(cache->set("motor_positions", positions));

    auto value = cache->get<std::array<double, 64>>("motor_positions");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(value.value()[0], 3.14159);
    EXPECT_DOUBLE_EQ(value.value()[63], 3.14159);
}

TEST_F(HotKeyCacheTest, GetUnregisteredKey) {
    auto value = cache->get<double>("unregistered_key");
    EXPECT_FALSE(value.has_value());
}

TEST_F(HotKeyCacheTest, SetUnregisteredKey) {
    EXPECT_FALSE(cache->set("unregistered_key", 100.0));
}

// ============================================================================
// Test 3: Type Safety
// ============================================================================

TEST_F(HotKeyCacheTest, TypeMismatch) {
    cache->registerHotKey("typed_key");

    // Set as double
    EXPECT_TRUE(cache->set("typed_key", 123.456));

    // Try to get as int - should fail (type mismatch)
    auto int_value = cache->get<int>("typed_key");
    EXPECT_FALSE(int_value.has_value());

    // Get as correct type should succeed
    auto double_value = cache->get<double>("typed_key");
    ASSERT_TRUE(double_value.has_value());
    EXPECT_DOUBLE_EQ(double_value.value(), 123.456);
}

// ============================================================================
// Test 4: Multiple Writes
// ============================================================================

TEST_F(HotKeyCacheTest, MultipleWrites) {
    cache->registerHotKey("counter");

    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(cache->set("counter", i));

        auto value = cache->get<int>("counter");
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }
}

// ============================================================================
// Test 5: Concurrent Read/Write
// ============================================================================

TEST_F(HotKeyCacheTest, ConcurrentReads) {
    cache->registerHotKey("shared_value");
    cache->set("shared_value", 42.0);

    std::vector<std::thread> threads;
    std::atomic<size_t> success_count{0};

    // 10 concurrent readers
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 1000; ++j) {
                auto value = cache->get<double>("shared_value");
                if (value.has_value()) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All reads should succeed
    EXPECT_EQ(success_count.load(), 10000);
}

TEST_F(HotKeyCacheTest, ConcurrentReadWrite) {
    cache->registerHotKey("counter");
    cache->set("counter", 0);

    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> read_success{0};

    // Writer thread
    std::thread writer([this, &stop_flag]() {
        int counter = 0;
        while (!stop_flag.load(std::memory_order_relaxed)) {
            cache->set("counter", counter);
            counter++;
            std::this_thread::yield();
        }
    });

    // Multiple reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([this, &stop_flag, &read_success]() {
            while (!stop_flag.load(std::memory_order_relaxed)) {
                auto value = cache->get<int>("counter");
                if (value.has_value()) {
                    read_success.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::yield();
            }
        });
    }

    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flag.store(true, std::memory_order_relaxed);

    writer.join();
    for (auto& r : readers) {
        r.join();
    }

    // Most reads should succeed (some may retry due to concurrent writes)
    EXPECT_GT(read_success.load(), 0);

    auto metrics = cache->getMetrics();
    EXPECT_GT(metrics["read_count"], 0);
    EXPECT_GT(metrics["write_count"], 0);
}

// ============================================================================
// Test 6: Performance Metrics
// ============================================================================

TEST_F(HotKeyCacheTest, MetricsTracking) {
    cache->registerHotKey("test_key");

    auto metrics_before = cache->getMetrics();
    uint64_t read_before = metrics_before["read_count"];
    uint64_t write_before = metrics_before["write_count"];

    // Perform operations
    cache->set("test_key", 100);
    cache->get<int>("test_key");
    cache->get<int>("test_key");

    auto metrics_after = cache->getMetrics();
    EXPECT_EQ(metrics_after["write_count"], write_before + 1);
    EXPECT_EQ(metrics_after["read_count"], read_before + 2);
}

// ============================================================================
// Test 7: Large Data (64-axis motor data)
// ============================================================================

TEST_F(HotKeyCacheTest, LargeArrayData) {
    cache->registerHotKey("motor_64_positions");
    cache->registerHotKey("motor_64_velocities");
    cache->registerHotKey("motor_64_torques");

    // Simulate 64-axis motor data
    std::array<double, 64> positions;
    std::array<double, 64> velocities;
    std::array<double, 64> torques;

    for (size_t i = 0; i < 64; ++i) {
        positions[i] = i * 0.1;
        velocities[i] = i * 0.01;
        torques[i] = i * 0.001;
    }

    EXPECT_TRUE(cache->set("motor_64_positions", positions));
    EXPECT_TRUE(cache->set("motor_64_velocities", velocities));
    EXPECT_TRUE(cache->set("motor_64_torques", torques));

    // Verify data integrity
    auto pos_result = cache->get<std::array<double, 64>>("motor_64_positions");
    ASSERT_TRUE(pos_result.has_value());
    for (size_t i = 0; i < 64; ++i) {
        EXPECT_DOUBLE_EQ(pos_result.value()[i], i * 0.1);
    }
}

// ============================================================================
// Test 8: Retry Mechanism (Version Consistency)
// ============================================================================

TEST_F(HotKeyCacheTest, RetryOnConcurrentWrite) {
    cache->registerHotKey("contested_key");

    std::atomic<bool> writer_active{true};
    std::atomic<size_t> successful_reads{0};

    // Continuous writer (creates contention)
    std::thread writer([this, &writer_active]() {
        int value = 0;
        while (writer_active.load(std::memory_order_relaxed)) {
            cache->set("contested_key", value++);
        }
    });

    // Reader with potential retries
    std::thread reader([this, &writer_active, &successful_reads]() {
        while (writer_active.load(std::memory_order_relaxed)) {
            auto result = cache->get<int>("contested_key");
            if (result.has_value()) {
                successful_reads.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    writer_active.store(false, std::memory_order_relaxed);

    writer.join();
    reader.join();

    // Should have some successful reads despite contention
    EXPECT_GT(successful_reads.load(), 0);

    // Check retry count
    auto metrics = cache->getMetrics();
    // retry_count may be > 0 if there was contention
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
