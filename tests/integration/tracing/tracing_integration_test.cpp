#include "core/tracing/TracerProvider.h"
#include "core/tracing/SpanContext.h"
#include "core/tracing/EventBusTracer.h"
#include "core/tracing/RTCycleTracer.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

using namespace mxrc::tracing;

class TracingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::info);
    }

    void TearDown() override {}
};

TEST_F(TracingIntegrationTest, EndToEndTracing) {
    // Get global tracer provider
    auto provider = getGlobalTracerProvider();
    ASSERT_NE(provider, nullptr);

    // Get tracer
    auto tracer = provider->getTracer("integration-test");
    ASSERT_NE(tracer, nullptr);

    // Create root span
    auto root_span = tracer->startSpan("integration-test-root");
    ASSERT_NE(root_span, nullptr);

    // Add attributes
    root_span->setAttribute("test.type", "integration");
    root_span->setAttribute("test.name", "end-to-end");

    // Create child span
    auto child_span = tracer->startSpan("child-operation",
                                       root_span->getContext());
    ASSERT_NE(child_span, nullptr);

    // Verify parent-child relationship
    EXPECT_EQ(child_span->getContext().trace_id,
              root_span->getContext().trace_id);
    EXPECT_EQ(child_span->getContext().parent_span_id,
              root_span->getContext().span_id);

    // Add event to child span
    child_span->addEvent("child-event", {{"event.key", "event.value"}});

    // End spans
    child_span->setStatus(SpanStatus::OK);
    child_span->end();

    root_span->setStatus(SpanStatus::OK);
    root_span->end();

    // Force flush
    EXPECT_TRUE(provider->forceFlush(5000));
}

TEST_F(TracingIntegrationTest, DistributedTracing) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("distributed-test");

    // Service A: Create span and inject context
    auto service_a_span = tracer->startSpan("service-a-operation");
    auto context = service_a_span->getContext();

    std::map<std::string, std::string> carrier;
    tracer->injectContext(context, carrier);

    service_a_span->end();

    // Simulate network transfer (carrier contains W3C headers)
    EXPECT_FALSE(carrier.empty());

    // Service B: Extract context and create child span
    auto extracted_context = tracer->extractContext(carrier);
    EXPECT_TRUE(SpanContextUtils::isValidContext(extracted_context));

    auto service_b_span = tracer->startSpan("service-b-operation",
                                           extracted_context);

    // Verify distributed trace
    EXPECT_EQ(service_b_span->getContext().trace_id, context.trace_id);
    EXPECT_TRUE(service_b_span->getContext().is_remote);

    service_b_span->end();
}

TEST_F(TracingIntegrationTest, RTCycleTracingIntegration) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("rt-test");

    auto rt_tracer = std::make_shared<RTCycleTracer>(tracer, 1.0);

    // Simulate RT cycles
    for (uint64_t cycle = 0; cycle < 5; ++cycle) {
        RTCycleGuard guard(rt_tracer, cycle);

        // Simulate actions
        guard.recordAction("read_sensors", 50);
        guard.recordAction("compute_control", 100);
        guard.recordAction("write_actuators", 30);

        // Simulate timing
        auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        rt_tracer->recordTiming(now_us, now_us + 10, now_us + 1000);
    }

    // Check stats
    std::string stats = rt_tracer->getStats();
    EXPECT_FALSE(stats.empty());
}

TEST_F(TracingIntegrationTest, MultithreadedTracing) {
    auto provider = getGlobalTracerProvider();

    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            auto tracer = provider->getTracer("thread-" + std::to_string(i));

            for (int j = 0; j < 10; ++j) {
                auto span = tracer->startSpan("operation-" + std::to_string(j));
                span->setAttribute("thread.id", std::to_string(i));
                span->setAttribute("iteration", std::to_string(j));

                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                span->setStatus(SpanStatus::OK);
                span->end();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(provider->forceFlush(10000));
}

TEST_F(TracingIntegrationTest, NestedSpans) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("nested-test");

    // Create nested span hierarchy
    auto level1_span = tracer->startSpan("level1");
    auto level1_context = level1_span->getContext();

    auto level2_span = tracer->startSpan("level2", level1_context);
    auto level2_context = level2_span->getContext();

    auto level3_span = tracer->startSpan("level3", level2_context);
    auto level3_context = level3_span->getContext();

    // Verify hierarchy
    EXPECT_EQ(level2_context.trace_id, level1_context.trace_id);
    EXPECT_EQ(level3_context.trace_id, level1_context.trace_id);

    EXPECT_EQ(level2_context.parent_span_id, level1_context.span_id);
    EXPECT_EQ(level3_context.parent_span_id, level2_context.span_id);

    // End in reverse order
    level3_span->end();
    level2_span->end();
    level1_span->end();
}

TEST_F(TracingIntegrationTest, SpanWithEvents) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("events-test");

    auto span = tracer->startSpan("operation-with-events");

    // Add multiple events
    span->addEvent("started");
    span->addEvent("processing", {{"items", "10"}});
    span->addEvent("checkpoint", {{"progress", "50%"}});
    span->addEvent("completed", {{"total_time_ms", "123"}});

    span->setStatus(SpanStatus::OK);
    span->end();
}

TEST_F(TracingIntegrationTest, SpanWithError) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("error-test");

    auto span = tracer->startSpan("failing-operation");

    try {
        // Simulate error
        throw std::runtime_error("Simulated error");
    } catch (const std::exception& e) {
        span->addEvent("exception", {{"error.message", e.what()}});
        span->setStatus(SpanStatus::ERROR, e.what());
    }

    span->end();
}

TEST_F(TracingIntegrationTest, SamplingBehavior) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("sampling-test");

    // Create RT tracer with 50% sampling
    auto rt_tracer = std::make_shared<RTCycleTracer>(tracer, 0.5);

    // Run many cycles
    const int num_cycles = 1000;
    for (int i = 0; i < num_cycles; ++i) {
        rt_tracer->startCycle(i);
        rt_tracer->endCycle(true);
    }

    // Check that some cycles were sampled
    std::string stats = rt_tracer->getStats();
    EXPECT_NE(stats.find("Sampled cycles:"), std::string::npos);
}

TEST_F(TracingIntegrationTest, ProviderShutdown) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("shutdown-test");

    // Create and end some spans
    for (int i = 0; i < 5; ++i) {
        auto span = tracer->startSpan("span-" + std::to_string(i));
        span->end();
    }

    // Force flush before shutdown
    EXPECT_TRUE(provider->forceFlush(5000));

    // Shutdown should not throw
    EXPECT_NO_THROW(provider->shutdown());
}
