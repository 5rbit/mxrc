#include "core/tracing/RTCycleTracer.h"
#include "core/tracing/TracerProvider.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

using namespace mxrc::tracing;

class RTCycleTracerTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::err);
        provider_ = getGlobalTracerProvider();
        tracer_ = provider_->getTracer("test-rt");
        rt_tracer_ = std::make_shared<RTCycleTracer>(tracer_, 1.0);  // 100% sampling for tests
    }

    void TearDown() override {
        spdlog::set_level(spdlog::level::info);
    }

    std::shared_ptr<ITracerProvider> provider_;
    std::shared_ptr<ITracer> tracer_;
    std::shared_ptr<RTCycleTracer> rt_tracer_;
};

TEST_F(RTCycleTracerTest, Creation) {
    ASSERT_NE(rt_tracer_, nullptr);
    EXPECT_TRUE(rt_tracer_->isEnabled());
    EXPECT_DOUBLE_EQ(rt_tracer_->getSamplingRate(), 1.0);
}

TEST_F(RTCycleTracerTest, EnableDisable) {
    EXPECT_TRUE(rt_tracer_->isEnabled());

    rt_tracer_->setEnabled(false);
    EXPECT_FALSE(rt_tracer_->isEnabled());

    rt_tracer_->setEnabled(true);
    EXPECT_TRUE(rt_tracer_->isEnabled());
}

TEST_F(RTCycleTracerTest, SamplingRate) {
    rt_tracer_->setSamplingRate(0.5);
    EXPECT_DOUBLE_EQ(rt_tracer_->getSamplingRate(), 0.5);

    // Invalid sampling rates
    rt_tracer_->setSamplingRate(-0.1);
    EXPECT_DOUBLE_EQ(rt_tracer_->getSamplingRate(), 0.5);  // Should not change

    rt_tracer_->setSamplingRate(1.5);
    EXPECT_DOUBLE_EQ(rt_tracer_->getSamplingRate(), 0.5);  // Should not change
}

TEST_F(RTCycleTracerTest, CycleLifecycle) {
    rt_tracer_->startCycle(1);
    rt_tracer_->endCycle(true);
}

TEST_F(RTCycleTracerTest, CycleWithFailure) {
    rt_tracer_->startCycle(2);
    rt_tracer_->endCycle(false);
}

TEST_F(RTCycleTracerTest, RecordAction) {
    rt_tracer_->startCycle(3);
    rt_tracer_->recordAction("action1", 100);
    rt_tracer_->recordAction("action2", 200);
    rt_tracer_->endCycle(true);
}

TEST_F(RTCycleTracerTest, RecordTiming) {
    rt_tracer_->startCycle(4);

    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    rt_tracer_->recordTiming(now_us, now_us + 100, now_us + 10000);
    rt_tracer_->endCycle(true);
}

TEST_F(RTCycleTracerTest, HighJitterDetection) {
    rt_tracer_->startCycle(5);

    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // High jitter (>1ms)
    rt_tracer_->recordTiming(now_us, now_us + 2000, now_us + 10000);
    rt_tracer_->endCycle(true);
}

TEST_F(RTCycleTracerTest, LowSlackDetection) {
    rt_tracer_->startCycle(6);

    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Low slack (<1ms)
    rt_tracer_->recordTiming(now_us, now_us + 100, now_us + 500);
    rt_tracer_->endCycle(true);
}

TEST_F(RTCycleTracerTest, RTCycleGuard) {
    {
        RTCycleGuard guard(rt_tracer_, 7);
        guard.recordAction("guarded_action", 150);
    }
    // Cycle should auto-end when guard goes out of scope
}

TEST_F(RTCycleTracerTest, MultipleCycles) {
    for (uint64_t i = 0; i < 10; ++i) {
        rt_tracer_->startCycle(i);
        rt_tracer_->recordAction("action", 100);
        rt_tracer_->endCycle(true);
    }
}

TEST_F(RTCycleTracerTest, DisabledTracer) {
    rt_tracer_->setEnabled(false);

    rt_tracer_->startCycle(100);
    rt_tracer_->recordAction("action", 100);
    rt_tracer_->endCycle(true);
}

TEST_F(RTCycleTracerTest, ZeroSampling) {
    rt_tracer_->setSamplingRate(0.0);

    for (int i = 0; i < 100; ++i) {
        rt_tracer_->startCycle(i);
        rt_tracer_->endCycle(true);
    }

    // With 0% sampling, no cycles should be sampled
    // (We can't directly test this without accessing internals,
    // but we ensure it doesn't crash)
}

TEST_F(RTCycleTracerTest, GetStats) {
    for (int i = 0; i < 10; ++i) {
        rt_tracer_->startCycle(i);
        rt_tracer_->endCycle(true);
    }

    std::string stats = rt_tracer_->getStats();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("Total cycles"), std::string::npos);
}

TEST_F(RTCycleTracerTest, ConcurrentCycles) {
    // Test thread-local storage
    const int num_threads = 5;
    const int cycles_per_thread = 20;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < cycles_per_thread; ++i) {
                uint64_t cycle_num = t * cycles_per_thread + i;
                rt_tracer_->startCycle(cycle_num);
                rt_tracer_->recordAction("thread_action", 50);
                rt_tracer_->endCycle(true);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}
