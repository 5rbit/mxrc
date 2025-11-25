// Hot Key Performance Integration Test
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Tests: T027 - RT Cycle performance verification

#include <gtest/gtest.h>
#include "DataStore.h"
#include "hotkey/HotKeyCache.h"
#include "ipc/DataStoreKeys.h"
#include <chrono>
#include <thread>
#include <array>
#include <numeric>
#include <algorithm>

using namespace std::chrono;
using namespace mxrc::ipc::DataStoreKeys;

/**
 * @brief Integration test for Hot Key performance in RT cycle context
 *
 * Validates:
 * 1. Hot Key cache integration with DataStore
 * 2. Performance in simulated RT cycle (1ms period)
 * 3. 64-axis motor data read/write performance
 * 4. Concurrent RT/Non-RT access patterns
 */
class HotKeyPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        datastore = DataStore::createForTest();

        // Give time for Hot Key initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown() override {
        datastore.reset();
    }

    std::shared_ptr<DataStore> datastore;

    // Helper: Measure operation latency
    template<typename Func>
    double measureLatencyNs(Func&& func, size_t iterations = 1000) {
        std::vector<double> latencies;
        latencies.reserve(iterations);

        for (size_t i = 0; i < iterations; ++i) {
            auto start = high_resolution_clock::now();
            func();
            auto end = high_resolution_clock::now();

            latencies.push_back(duration<double, std::nano>(end - start).count());
        }

        // Return median latency (more stable than mean)
        std::sort(latencies.begin(), latencies.end());
        return latencies[latencies.size() / 2];
    }
};

// ============================================================================
// Test 1: Basic Hot Key Read Performance
// ============================================================================

TEST_F(HotKeyPerformanceTest, HotKeyReadLatency) {
    // Set initial value
    datastore->set(ROBOT_POSITION, 123.456, DataType::RobotMode);

    // Measure read latency
    double median_ns = measureLatencyNs([this]() {
        double value = datastore->get<double>(ROBOT_POSITION);
        (void)value;  // Prevent optimization
    });

    std::cout << "Hot Key Read Latency (median): " << median_ns << " ns" << std::endl;

    // FR-006: Hot Key read target <60ns
    EXPECT_LT(median_ns, 60.0) << "Hot Key read latency exceeds 60ns target";
}

TEST_F(HotKeyPerformanceTest, HotKeyWriteLatency) {
    // Measure write latency
    double value = 0.0;
    double median_ns = measureLatencyNs([this, &value]() {
        datastore->set(ROBOT_POSITION, value, DataType::RobotMode);
        value += 0.001;
    });

    std::cout << "Hot Key Write Latency (median): " << median_ns << " ns" << std::endl;

    // FR-006: Hot Key write target <110ns
    EXPECT_LT(median_ns, 110.0) << "Hot Key write latency exceeds 110ns target";
}

// ============================================================================
// Test 2: 64-Axis Motor Data Performance
// ============================================================================

TEST_F(HotKeyPerformanceTest, MotorData64AxisRead) {
    // Initialize 64-axis motor data
    std::array<double, 64> positions;
    for (size_t i = 0; i < 64; ++i) {
        positions[i] = i * 0.1;
    }
    datastore->set(ETHERCAT_SENSOR_POSITION, positions, DataType::RobotMode);

    // Measure read latency
    double median_ns = measureLatencyNs([this]() {
        auto value = datastore->get<std::array<double, 64>>(ETHERCAT_SENSOR_POSITION);
        (void)value;
    });

    std::cout << "64-Axis Motor Read Latency (median): " << median_ns << " ns" << std::endl;

    // Should still be under 60ns despite larger data size
    EXPECT_LT(median_ns, 60.0) << "64-axis read latency exceeds 60ns target";
}

TEST_F(HotKeyPerformanceTest, MotorData64AxisWrite) {
    std::array<double, 64> positions;
    positions.fill(0.0);

    double median_ns = measureLatencyNs([this, &positions]() {
        datastore->set(ETHERCAT_SENSOR_POSITION, positions, DataType::RobotMode);
        positions[0] += 0.001;
    });

    std::cout << "64-Axis Motor Write Latency (median): " << median_ns << " ns" << std::endl;

    EXPECT_LT(median_ns, 110.0) << "64-axis write latency exceeds 110ns target";
}

// ============================================================================
// Test 3: Simulated RT Cycle (1ms period)
// ============================================================================

TEST_F(HotKeyPerformanceTest, RTCycleSimulation) {
    // Simulate RT cycle: read sensors → compute → write actuators
    // Target: 1ms cycle with Hot Key access overhead minimal

    std::array<double, 64> sensor_pos;
    std::array<double, 64> target_pos;
    sensor_pos.fill(0.0);
    target_pos.fill(0.0);

    datastore->set(ETHERCAT_SENSOR_POSITION, sensor_pos, DataType::RobotMode);
    datastore->set(ETHERCAT_TARGET_POSITION, target_pos, DataType::RobotMode);

    size_t cycles = 100;
    std::vector<double> cycle_times_us;
    cycle_times_us.reserve(cycles);

    for (size_t i = 0; i < cycles; ++i) {
        auto cycle_start = high_resolution_clock::now();

        // Read sensors (Hot Key)
        sensor_pos = datastore->get<std::array<double, 64>>(ETHERCAT_SENSOR_POSITION);

        // Simulate computation (simple PID-like calculation)
        for (size_t j = 0; j < 64; ++j) {
            target_pos[j] = sensor_pos[j] + 0.01;  // Simple increment
        }

        // Write targets (Hot Key)
        datastore->set(ETHERCAT_TARGET_POSITION, target_pos, DataType::RobotMode);

        auto cycle_end = high_resolution_clock::now();
        cycle_times_us.push_back(duration<double, std::micro>(cycle_end - cycle_start).count());

        // Simulate 1ms RT cycle period
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Calculate statistics
    double mean_us = std::accumulate(cycle_times_us.begin(), cycle_times_us.end(), 0.0) / cycles;
    double max_us = *std::max_element(cycle_times_us.begin(), cycle_times_us.end());

    std::cout << "RT Cycle Time (mean): " << mean_us << " µs" << std::endl;
    std::cout << "RT Cycle Time (max): " << max_us << " µs" << std::endl;

    // Hot Key overhead should be negligible (<1µs for read+write)
    EXPECT_LT(mean_us, 1.0) << "Hot Key overhead in RT cycle too high";
    EXPECT_LT(max_us, 2.0) << "Maximum Hot Key overhead exceeds 2µs";
}

// ============================================================================
// Test 4: Concurrent RT/Non-RT Access
// ============================================================================

TEST_F(HotKeyPerformanceTest, ConcurrentRTNonRTAccess) {
    std::array<double, 64> motor_pos;
    motor_pos.fill(0.0);
    datastore->set(ETHERCAT_SENSOR_POSITION, motor_pos, DataType::RobotMode);

    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> rt_read_count{0};
    std::atomic<size_t> nonrt_read_count{0};

    // Simulate RT thread (high frequency reads)
    std::thread rt_thread([this, &stop_flag, &rt_read_count]() {
        while (!stop_flag.load(std::memory_order_relaxed)) {
            auto value = datastore->get<std::array<double, 64>>(ETHERCAT_SENSOR_POSITION);
            rt_read_count.fetch_add(1, std::memory_order_relaxed);
            (void)value;
        }
    });

    // Simulate Non-RT thread (occasional writes)
    std::thread nonrt_thread([this, &stop_flag, &nonrt_read_count]() {
        std::array<double, 64> pos;
        pos.fill(1.0);

        while (!stop_flag.load(std::memory_order_relaxed)) {
            datastore->set(ETHERCAT_SENSOR_POSITION, pos, DataType::RobotMode);
            nonrt_read_count.fetch_add(1, std::memory_order_relaxed);

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop_flag.store(true, std::memory_order_relaxed);

    rt_thread.join();
    nonrt_thread.join();

    std::cout << "RT reads: " << rt_read_count.load() << std::endl;
    std::cout << "Non-RT writes: " << nonrt_read_count.load() << std::endl;

    // RT thread should achieve high read throughput
    EXPECT_GT(rt_read_count.load(), 100000) << "RT read throughput too low";
}

// ============================================================================
// Test 5: IO Module Data (64 digital/analog modules)
// ============================================================================

TEST_F(HotKeyPerformanceTest, IOModulePerformance) {
    std::array<uint64_t, 64> digital_input;
    std::array<uint64_t, 64> digital_output;
    digital_input.fill(0);
    digital_output.fill(0);

    // Measure digital I/O access
    double read_ns = measureLatencyNs([this, &digital_input]() {
        datastore->set(ETHERCAT_DIGITAL_INPUT, digital_input, DataType::RobotMode);
    });

    double write_ns = measureLatencyNs([this, &digital_output]() {
        datastore->set(ETHERCAT_DIGITAL_OUTPUT, digital_output, DataType::RobotMode);
    });

    std::cout << "Digital Input Write Latency: " << read_ns << " ns" << std::endl;
    std::cout << "Digital Output Write Latency: " << write_ns << " ns" << std::endl;

    EXPECT_LT(read_ns, 110.0);
    EXPECT_LT(write_ns, 110.0);
}

// ============================================================================
// Test 6: Multiple Hot Keys in Single RT Cycle
// ============================================================================

TEST_F(HotKeyPerformanceTest, MultipleHotKeysInCycle) {
    // Initialize all Hot Keys used in RT cycle
    datastore->set(ROBOT_POSITION, 0.0, DataType::RobotMode);
    datastore->set(ROBOT_VELOCITY, 0.0, DataType::RobotMode);
    datastore->set(ROBOT_ACCELERATION, 0.0, DataType::RobotMode);

    std::array<double, 64> motor_pos, motor_vel, motor_torque;
    motor_pos.fill(0.0);
    motor_vel.fill(0.0);
    motor_torque.fill(0.0);

    datastore->set(ETHERCAT_SENSOR_POSITION, motor_pos, DataType::RobotMode);
    datastore->set(ETHERCAT_SENSOR_VELOCITY, motor_vel, DataType::RobotMode);
    datastore->set(ETHERCAT_MOTOR_TORQUE, motor_torque, DataType::RobotMode);

    // Measure total time to access all Hot Keys
    auto start = high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        // Read all sensor Hot Keys
        auto robot_pos = datastore->get<double>(ROBOT_POSITION);
        auto robot_vel = datastore->get<double>(ROBOT_VELOCITY);
        auto robot_acc = datastore->get<double>(ROBOT_ACCELERATION);
        auto m_pos = datastore->get<std::array<double, 64>>(ETHERCAT_SENSOR_POSITION);
        auto m_vel = datastore->get<std::array<double, 64>>(ETHERCAT_SENSOR_VELOCITY);

        // Write actuator Hot Keys
        datastore->set(ETHERCAT_MOTOR_TORQUE, motor_torque, DataType::RobotMode);

        (void)robot_pos; (void)robot_vel; (void)robot_acc;
        (void)m_pos; (void)m_vel;
    }

    auto end = high_resolution_clock::now();
    double total_us = duration<double, std::micro>(end - start).count();
    double avg_us_per_cycle = total_us / 1000.0;

    std::cout << "Multi-Hot-Key Access Time (avg per cycle): " << avg_us_per_cycle << " µs" << std::endl;

    // Total overhead for 6 Hot Key accesses should be < 1µs
    EXPECT_LT(avg_us_per_cycle, 1.0) << "Multiple Hot Key access overhead too high";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
