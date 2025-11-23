// Hot Key Performance Benchmark
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Tests: T025-T026 - Read <60ns, Write <110ns

#include <benchmark/benchmark.h>
#include "hotkey/HotKeyCache.h"
#include <array>
#include <random>

using namespace mxrc::core::datastore;

// ============================================================================
// Benchmark Fixtures
// ============================================================================

class HotKeyCacheBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) {
        cache = std::make_unique<HotKeyCache>(32);

        // Register Hot Keys for benchmarking
        cache->registerHotKey("robot_position_x");
        cache->registerHotKey("robot_velocity_x");
        cache->registerHotKey("motor_positions");
        cache->registerHotKey("motor_velocities");
        cache->registerHotKey("io_digital_input");

        // Pre-populate with initial values
        cache->set("robot_position_x", 0.0);
        cache->set("robot_velocity_x", 0.0);

        std::array<double, 64> motor_pos;
        motor_pos.fill(0.0);
        cache->set("motor_positions", motor_pos);

        std::array<double, 64> motor_vel;
        motor_vel.fill(0.0);
        cache->set("motor_velocities", motor_vel);

        std::array<uint64_t, 64> io_input;
        io_input.fill(0);
        cache->set("io_digital_input", io_input);
    }

    void TearDown(const ::benchmark::State& state) {
        cache.reset();
    }

    std::unique_ptr<HotKeyCache> cache;
};

// ============================================================================
// T025: Read Benchmarks (Target: <60ns)
// ============================================================================

BENCHMARK_F(HotKeyCacheBenchmark, ReadDouble)(benchmark::State& state) {
    for (auto _ : state) {
        auto value = cache->get<double>("robot_position_x");
        benchmark::DoNotOptimize(value);
    }

    state.SetItemsProcessed(state.iterations());

    // Report average latency
    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["target_ns"] = 60.0;
    state.counters["passed"] = (avg_ns < 60.0) ? 1.0 : 0.0;
}

BENCHMARK_F(HotKeyCacheBenchmark, ReadArray64)(benchmark::State& state) {
    for (auto _ : state) {
        auto value = cache->get<std::array<double, 64>>("motor_positions");
        benchmark::DoNotOptimize(value);
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["target_ns"] = 60.0;
    state.counters["passed"] = (avg_ns < 60.0) ? 1.0 : 0.0;
}

BENCHMARK_F(HotKeyCacheBenchmark, ReadArrayUint64)(benchmark::State& state) {
    for (auto _ : state) {
        auto value = cache->get<std::array<uint64_t, 64>>("io_digital_input");
        benchmark::DoNotOptimize(value);
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["target_ns"] = 60.0;
    state.counters["passed"] = (avg_ns < 60.0) ? 1.0 : 0.0;
}

// ============================================================================
// T026: Write Benchmarks (Target: <110ns)
// ============================================================================

BENCHMARK_F(HotKeyCacheBenchmark, WriteDouble)(benchmark::State& state) {
    double value = 123.456;

    for (auto _ : state) {
        cache->set("robot_position_x", value);
        value += 0.001;  // Vary value to prevent optimization
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["target_ns"] = 110.0;
    state.counters["passed"] = (avg_ns < 110.0) ? 1.0 : 0.0;
}

BENCHMARK_F(HotKeyCacheBenchmark, WriteArray64)(benchmark::State& state) {
    std::array<double, 64> motor_pos;
    motor_pos.fill(0.0);

    for (auto _ : state) {
        cache->set("motor_positions", motor_pos);
        motor_pos[0] += 0.001;  // Vary to prevent optimization
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["target_ns"] = 110.0;
    state.counters["passed"] = (avg_ns < 110.0) ? 1.0 : 0.0;
}

BENCHMARK_F(HotKeyCacheBenchmark, WriteArrayUint64)(benchmark::State& state) {
    std::array<uint64_t, 64> io_input;
    io_input.fill(0);

    for (auto _ : state) {
        cache->set("io_digital_input", io_input);
        io_input[0]++;
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["target_ns"] = 110.0;
    state.counters["passed"] = (avg_ns < 110.0) ? 1.0 : 0.0;
}

// ============================================================================
// Mixed Read/Write (90% read, 10% write - typical RT workload)
// ============================================================================

BENCHMARK_F(HotKeyCacheBenchmark, MixedReadWrite_90_10)(benchmark::State& state) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 9);

    double write_value = 0.0;

    for (auto _ : state) {
        if (dist(rng) < 9) {
            // 90% reads
            auto value = cache->get<double>("robot_position_x");
            benchmark::DoNotOptimize(value);
        } else {
            // 10% writes
            cache->set("robot_position_x", write_value);
            write_value += 0.001;
        }
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
}

// ============================================================================
// Concurrent Access Benchmark
// ============================================================================

BENCHMARK_F(HotKeyCacheBenchmark, ConcurrentReads)(benchmark::State& state) {
    if (state.thread_index() == 0) {
        // Setup already done in SetUp()
    }

    for (auto _ : state) {
        auto value = cache->get<double>("robot_position_x");
        benchmark::DoNotOptimize(value);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(HotKeyCacheBenchmark, ConcurrentReads)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8);

// ============================================================================
// Throughput Benchmarks
// ============================================================================

BENCHMARK_F(HotKeyCacheBenchmark, ReadThroughput)(benchmark::State& state) {
    size_t operations = 0;

    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            auto value = cache->get<double>("robot_position_x");
            benchmark::DoNotOptimize(value);
            operations++;
        }
    }

    state.SetItemsProcessed(operations);
    state.counters["ops_per_sec"] = benchmark::Counter(
        operations, benchmark::Counter::kIsRate);
}

BENCHMARK_F(HotKeyCacheBenchmark, WriteThroughput)(benchmark::State& state) {
    size_t operations = 0;
    double value = 0.0;

    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            cache->set("robot_position_x", value);
            value += 0.001;
            operations++;
        }
    }

    state.SetItemsProcessed(operations);
    state.counters["ops_per_sec"] = benchmark::Counter(
        operations, benchmark::Counter::kIsRate);
}

// ============================================================================
// Cache Line Alignment Benchmark (measure false sharing impact)
// ============================================================================

BENCHMARK_F(HotKeyCacheBenchmark, MultipleKeysRead)(benchmark::State& state) {
    // Read from multiple keys to test cache line alignment
    for (auto _ : state) {
        auto pos = cache->get<double>("robot_position_x");
        auto vel = cache->get<double>("robot_velocity_x");
        benchmark::DoNotOptimize(pos);
        benchmark::DoNotOptimize(vel);
    }

    state.SetItemsProcessed(state.iterations() * 2);

    double avg_ns_per_op = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / (state.iterations() * 2)
        : 0.0;

    state.counters["avg_latency_ns_per_op"] = avg_ns_per_op;
}

// ============================================================================
// Comparison: Hot Key vs Backing Store (tbb::concurrent_hash_map)
// ============================================================================

class BackingStoreBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) {
        backing_store = std::make_unique<tbb::concurrent_hash_map<std::string, double>>();

        typename tbb::concurrent_hash_map<std::string, double>::accessor acc;
        backing_store->insert(acc, "robot_position_x");
        acc->second = 0.0;
    }

    void TearDown(const ::benchmark::State& state) {
        backing_store.reset();
    }

    std::unique_ptr<tbb::concurrent_hash_map<std::string, double>> backing_store;
};

BENCHMARK_F(BackingStoreBenchmark, ReadConcurrentHashMap)(benchmark::State& state) {
    for (auto _ : state) {
        typename tbb::concurrent_hash_map<std::string, double>::const_accessor acc;
        if (backing_store->find(acc, "robot_position_x")) {
            double value = acc->second;
            benchmark::DoNotOptimize(value);
        }
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["speedup_vs_target_60ns"] = 60.0 / avg_ns;
}

BENCHMARK_F(BackingStoreBenchmark, WriteConcurrentHashMap)(benchmark::State& state) {
    double value = 0.0;

    for (auto _ : state) {
        typename tbb::concurrent_hash_map<std::string, double>::accessor acc;
        if (backing_store->find(acc, "robot_position_x")) {
            acc->second = value;
            value += 0.001;
        }
    }

    state.SetItemsProcessed(state.iterations());

    double avg_ns = state.iterations() > 0
        ? (state.elapsed_time().count() * 1e9) / state.iterations()
        : 0.0;

    state.counters["avg_latency_ns"] = avg_ns;
    state.counters["speedup_vs_target_110ns"] = 110.0 / avg_ns;
}

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
