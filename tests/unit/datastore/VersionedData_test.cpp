#include <gtest/gtest.h>
#include "core/datastore/core/VersionedData.h"
#include <thread>
#include <vector>

using namespace mxrc::core::datastore;

// Test fixture for VersionedData
class VersionedDataTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Basic Construction and Initialization Tests
// ============================================================================

TEST_F(VersionedDataTest, DefaultConstructor_InitializesToZero) {
    VersionedData<int> vdata;

    EXPECT_EQ(vdata.value, 0);
    EXPECT_EQ(vdata.getVersion(), 0u);
    EXPECT_EQ(vdata.getTimestampNs(), 0u);
    EXPECT_FALSE(vdata.isModified());
}

TEST_F(VersionedDataTest, ExplicitConstructor_SetsValueAndVersion) {
    VersionedData<int> vdata(42);

    EXPECT_EQ(vdata.value, 42);
    EXPECT_EQ(vdata.getVersion(), 1u);
    EXPECT_GT(vdata.getTimestampNs(), 0u);  // Timestamp should be set
    EXPECT_TRUE(vdata.isModified());
}

TEST_F(VersionedDataTest, CopyConstructor_CopiesAllFields) {
    VersionedData<int> original(100);
    VersionedData<int> copy(original);

    EXPECT_EQ(copy.value, original.value);
    EXPECT_EQ(copy.getVersion(), original.getVersion());
    EXPECT_EQ(copy.getTimestampNs(), original.getTimestampNs());
}

TEST_F(VersionedDataTest, AssignmentOperator_CopiesAllFields) {
    VersionedData<int> original(200);
    VersionedData<int> assigned;

    assigned = original;

    EXPECT_EQ(assigned.value, original.value);
    EXPECT_EQ(assigned.getVersion(), original.getVersion());
    EXPECT_EQ(assigned.getTimestampNs(), original.getTimestampNs());
}

// ============================================================================
// Update and Version Increment Tests
// ============================================================================

TEST_F(VersionedDataTest, Update_IncrementsVersion) {
    VersionedData<int> vdata;

    EXPECT_EQ(vdata.getVersion(), 0u);

    vdata.update(10);
    EXPECT_EQ(vdata.value, 10);
    EXPECT_EQ(vdata.getVersion(), 1u);

    vdata.update(20);
    EXPECT_EQ(vdata.value, 20);
    EXPECT_EQ(vdata.getVersion(), 2u);

    vdata.update(30);
    EXPECT_EQ(vdata.value, 30);
    EXPECT_EQ(vdata.getVersion(), 3u);
}

TEST_F(VersionedDataTest, Update_UpdatesTimestamp) {
    VersionedData<int> vdata;
    vdata.update(1);

    uint64_t ts1 = vdata.getTimestampNs();
    EXPECT_GT(ts1, 0u);

    // Sleep to ensure timestamp changes
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    vdata.update(2);
    uint64_t ts2 = vdata.getTimestampNs();

    EXPECT_GT(ts2, ts1);  // Timestamp should increase
}

TEST_F(VersionedDataTest, MultipleUpdates_MonotonicVersionIncrease) {
    VersionedData<int> vdata;

    for (int i = 1; i <= 100; ++i) {
        vdata.update(i);
        EXPECT_EQ(vdata.getVersion(), static_cast<uint64_t>(i));
        EXPECT_EQ(vdata.value, i);
    }
}

// ============================================================================
// Consistency Check Tests
// ============================================================================

TEST_F(VersionedDataTest, IsConsistentWith_SameVersion_ReturnsTrue) {
    VersionedData<int> vdata1(42);
    VersionedData<int> vdata2(vdata1);  // Copy constructor

    EXPECT_TRUE(vdata1.isConsistentWith(vdata2));
    EXPECT_TRUE(vdata2.isConsistentWith(vdata1));
}

TEST_F(VersionedDataTest, IsConsistentWith_DifferentVersion_ReturnsFalse) {
    VersionedData<int> vdata1(42);
    VersionedData<int> vdata2(vdata1);

    vdata2.update(100);  // Increment version

    EXPECT_FALSE(vdata1.isConsistentWith(vdata2));
    EXPECT_FALSE(vdata2.isConsistentWith(vdata1));
}

TEST_F(VersionedDataTest, IsNewerThan_GreaterVersion_ReturnsTrue) {
    VersionedData<int> old_vdata;
    old_vdata.update(1);  // version = 1

    VersionedData<int> new_vdata;
    new_vdata.update(1);  // version = 1
    new_vdata.update(2);  // version = 2

    EXPECT_TRUE(new_vdata.isNewerThan(old_vdata));
    EXPECT_FALSE(old_vdata.isNewerThan(new_vdata));
}

TEST_F(VersionedDataTest, IsNewerThan_SameVersion_ReturnsFalse) {
    VersionedData<int> vdata1;
    vdata1.update(1);  // version = 1

    VersionedData<int> vdata2;
    vdata2.update(1);  // version = 1

    EXPECT_FALSE(vdata1.isNewerThan(vdata2));
    EXPECT_FALSE(vdata2.isNewerThan(vdata1));
}

// ============================================================================
// Optimistic Read Pattern Tests
// ============================================================================

TEST_F(VersionedDataTest, TryOptimisticRead_NoContention_Succeeds) {
    VersionedData<int> vdata(42);
    int snapshot = 0;

    bool success = tryOptimisticRead(vdata, snapshot);

    EXPECT_TRUE(success);
    EXPECT_EQ(snapshot, 42);
}

TEST_F(VersionedDataTest, TryOptimisticRead_ConcurrentUpdate_MayFail) {
    VersionedData<int> vdata(100);
    std::atomic<bool> writer_started{false};
    std::atomic<bool> reader_done{false};
    int successful_reads = 0;
    int failed_reads = 0;

    // Writer thread: continuously update
    std::thread writer([&]() {
        writer_started = true;
        for (int i = 0; i < 1000; ++i) {
            vdata.update(i);
        }
    });

    // Reader thread: optimistic reads
    std::thread reader([&]() {
        while (!writer_started.load()) {
            std::this_thread::yield();
        }

        for (int i = 0; i < 1000; ++i) {
            int snapshot;
            if (tryOptimisticRead(vdata, snapshot)) {
                successful_reads++;
            } else {
                failed_reads++;
            }
        }
        reader_done = true;
    });

    writer.join();
    reader.join();

    // At least some reads should succeed
    EXPECT_GT(successful_reads, 0);
    std::cout << "Optimistic reads: " << successful_reads
              << " successful, " << failed_reads << " failed" << std::endl;
}

// ============================================================================
// Thread Safety Tests (Atomic Version)
// ============================================================================

TEST_F(VersionedDataTest, ConcurrentUpdates_VersionMonotonicallyIncreases) {
    VersionedData<int> vdata;
    const int num_threads = 4;
    const int updates_per_thread = 250;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < updates_per_thread; ++i) {
                vdata.update(i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Total updates = num_threads * updates_per_thread
    EXPECT_EQ(vdata.getVersion(), static_cast<uint64_t>(num_threads * updates_per_thread));
}

TEST_F(VersionedDataTest, ConcurrentReads_NoDataRaces) {
    VersionedData<int> vdata(42);
    const int num_readers = 8;
    std::atomic<int> read_count{0};
    std::vector<std::thread> threads;

    // Writer thread
    threads.emplace_back([&]() {
        for (int i = 0; i < 1000; ++i) {
            vdata.update(i);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Reader threads
    for (int t = 0; t < num_readers; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 1000; ++i) {
                uint64_t ver = vdata.getVersion();
                int val = vdata.value;
                uint64_t ts = vdata.getTimestampNs();

                // Just read, no assertions (testing for data races)
                (void)ver; (void)val; (void)ts;
                read_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(read_count, num_readers * 1000);
}

// ============================================================================
// Custom Type Tests
// ============================================================================

struct SensorData {
    double temperature;
    double pressure;
    uint32_t sensor_id;

    bool operator==(const SensorData& other) const {
        return temperature == other.temperature &&
               pressure == other.pressure &&
               sensor_id == other.sensor_id;
    }
};

TEST_F(VersionedDataTest, CustomType_SensorData_Works) {
    SensorData sensor{25.5, 101.3, 42};
    VersionedData<SensorData> vdata(sensor);

    EXPECT_EQ(vdata.value.temperature, 25.5);
    EXPECT_EQ(vdata.value.pressure, 101.3);
    EXPECT_EQ(vdata.value.sensor_id, 42u);
    EXPECT_EQ(vdata.getVersion(), 1u);
}

TEST_F(VersionedDataTest, CustomType_Update_Works) {
    VersionedData<SensorData> vdata;

    SensorData reading1{20.0, 100.0, 1};
    vdata.update(reading1);
    EXPECT_EQ(vdata.value, reading1);
    EXPECT_EQ(vdata.getVersion(), 1u);

    SensorData reading2{25.0, 102.0, 1};
    vdata.update(reading2);
    EXPECT_EQ(vdata.value, reading2);
    EXPECT_EQ(vdata.getVersion(), 2u);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(VersionedDataTest, IsModified_DefaultConstructed_ReturnsFalse) {
    VersionedData<int> vdata;
    EXPECT_FALSE(vdata.isModified());
}

TEST_F(VersionedDataTest, IsModified_AfterUpdate_ReturnsTrue) {
    VersionedData<int> vdata;
    vdata.update(1);
    EXPECT_TRUE(vdata.isModified());
}

TEST_F(VersionedDataTest, IsModified_ExplicitConstructor_ReturnsTrue) {
    VersionedData<int> vdata(42);
    EXPECT_TRUE(vdata.isModified());
}

TEST_F(VersionedDataTest, SelfAssignment_NoOp) {
    VersionedData<int> vdata(42);
    uint64_t original_version = vdata.getVersion();

    vdata = vdata;  // Self-assignment

    EXPECT_EQ(vdata.value, 42);
    EXPECT_EQ(vdata.getVersion(), original_version);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
