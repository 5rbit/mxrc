#include "core/tracing/TracerProvider.h"
#include "core/tracing/SpanContext.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace mxrc::tracing;

class TracerProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set log level to error to reduce test noise
        spdlog::set_level(spdlog::level::err);
    }

    void TearDown() override {
        // Reset log level
        spdlog::set_level(spdlog::level::info);
    }
};

TEST_F(TracerProviderTest, GetGlobalTracerProvider) {
    auto provider = getGlobalTracerProvider();
    ASSERT_NE(provider, nullptr);
}

TEST_F(TracerProviderTest, GetTracer) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");
    ASSERT_NE(tracer, nullptr);
}

TEST_F(TracerProviderTest, GetSameTracerTwice) {
    auto provider = getGlobalTracerProvider();
    auto tracer1 = provider->getTracer("test-tracer");
    auto tracer2 = provider->getTracer("test-tracer");

    // Should return the same tracer instance
    EXPECT_EQ(tracer1, tracer2);
}

TEST_F(TracerProviderTest, StartRootSpan) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto span = tracer->startSpan("test-operation");
    ASSERT_NE(span, nullptr);
    EXPECT_TRUE(span->isRecording());

    auto context = span->getContext();
    EXPECT_TRUE(SpanContextUtils::isValidTraceId(context.trace_id));
    EXPECT_TRUE(SpanContextUtils::isValidSpanId(context.span_id));
    EXPECT_EQ(context.parent_span_id, "");  // Root span has no parent
    EXPECT_FALSE(context.is_remote);
}

TEST_F(TracerProviderTest, StartChildSpan) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto parent_span = tracer->startSpan("parent-operation");
    auto parent_context = parent_span->getContext();

    auto child_span = tracer->startSpan("child-operation", parent_context);
    auto child_context = child_span->getContext();

    // Child should have same trace_id as parent
    EXPECT_EQ(child_context.trace_id, parent_context.trace_id);

    // Child should have different span_id
    EXPECT_NE(child_context.span_id, parent_context.span_id);

    // Child's parent_span_id should be parent's span_id
    EXPECT_EQ(child_context.parent_span_id, parent_context.span_id);
}

TEST_F(TracerProviderTest, SpanAttributes) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    std::map<std::string, std::string> attributes;
    attributes["key1"] = "value1";
    attributes["key2"] = "value2";

    auto span = tracer->startSpan("test-operation", attributes);
    ASSERT_NE(span, nullptr);

    // Add more attributes after span creation
    span->setAttribute("key3", "value3");
}

TEST_F(TracerProviderTest, SpanEvents) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto span = tracer->startSpan("test-operation");

    std::map<std::string, std::string> event_attrs;
    event_attrs["event_key"] = "event_value";

    span->addEvent("test-event", event_attrs);
    span->addEvent("another-event");
}

TEST_F(TracerProviderTest, SpanStatus) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto span = tracer->startSpan("test-operation");

    // Set status to OK
    span->setStatus(SpanStatus::OK);

    // Set status to ERROR
    span->setStatus(SpanStatus::ERROR, "Something went wrong");
}

TEST_F(TracerProviderTest, SpanLifecycle) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto span = tracer->startSpan("test-operation");
    EXPECT_TRUE(span->isRecording());

    span->end();
    EXPECT_FALSE(span->isRecording());

    // Calling end() again should be safe
    span->end();
    EXPECT_FALSE(span->isRecording());
}

TEST_F(TracerProviderTest, SpanGuard) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto span = tracer->startSpan("test-operation");
    bool was_recording = false;
    bool is_recording_after = false;

    {
        SpanGuard guard(span);
        was_recording = guard->isRecording();
    }

    // Span should be ended after guard goes out of scope
    is_recording_after = span->isRecording();

    EXPECT_TRUE(was_recording);
    EXPECT_FALSE(is_recording_after);
}

TEST_F(TracerProviderTest, ContextPropagation) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    auto span = tracer->startSpan("test-operation");
    auto context = span->getContext();

    // Inject context into carrier
    std::map<std::string, std::string> carrier;
    tracer->injectContext(context, carrier);

    EXPECT_FALSE(carrier.empty());
    EXPECT_NE(carrier.find("traceparent"), carrier.end());

    // Extract context from carrier
    auto extracted_context = tracer->extractContext(carrier);

    EXPECT_EQ(extracted_context.trace_id, context.trace_id);
    EXPECT_EQ(extracted_context.span_id, context.span_id);
    EXPECT_TRUE(extracted_context.is_remote);
}

TEST_F(TracerProviderTest, CurrentSpan) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    // Initially no current span
    EXPECT_EQ(tracer->getCurrentSpan(), nullptr);

    auto span = tracer->startSpan("test-operation");
    tracer->setCurrentSpan(span);

    auto current = tracer->getCurrentSpan();
    EXPECT_EQ(current, span);

    // Clear current span
    tracer->setCurrentSpan(nullptr);
    EXPECT_EQ(tracer->getCurrentSpan(), nullptr);
}

TEST_F(TracerProviderTest, ForceFlush) {
    auto provider = getGlobalTracerProvider();
    bool result = provider->forceFlush(1000);
    EXPECT_TRUE(result);
}

TEST_F(TracerProviderTest, Shutdown) {
    // Create a new provider for this test
    auto provider = getGlobalTracerProvider();

    // Shutdown should not throw
    EXPECT_NO_THROW(provider->shutdown());
}

TEST_F(TracerProviderTest, MultithreadedSpanCreation) {
    auto provider = getGlobalTracerProvider();
    auto tracer = provider->getTracer("test-tracer");

    const int num_threads = 10;
    const int spans_per_thread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < spans_per_thread; ++j) {
                try {
                    auto span = tracer->startSpan("thread-" + std::to_string(i) +
                                                 "-span-" + std::to_string(j));
                    if (span && span->isRecording()) {
                        span->end();
                        success_count++;
                    }
                } catch (...) {
                    // Ignore errors
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_threads * spans_per_thread);
}
