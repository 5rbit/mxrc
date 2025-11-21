#include "core/tracing/SpanContext.h"
#include "core/tracing/TracerProvider.h"
#include <gtest/gtest.h>

using namespace mxrc::tracing;

class SpanContextTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SpanContextTest, GenerateTraceId) {
    auto trace_id = SpanContextUtils::generateTraceId();

    EXPECT_EQ(trace_id.length(), TRACE_ID_LENGTH);
    EXPECT_TRUE(SpanContextUtils::isValidTraceId(trace_id));
}

TEST_F(SpanContextTest, GenerateSpanId) {
    auto span_id = SpanContextUtils::generateSpanId();

    EXPECT_EQ(span_id.length(), SPAN_ID_LENGTH);
    EXPECT_TRUE(SpanContextUtils::isValidSpanId(span_id));
}

TEST_F(SpanContextTest, GenerateUniqueIds) {
    auto trace_id1 = SpanContextUtils::generateTraceId();
    auto trace_id2 = SpanContextUtils::generateTraceId();

    EXPECT_NE(trace_id1, trace_id2);

    auto span_id1 = SpanContextUtils::generateSpanId();
    auto span_id2 = SpanContextUtils::generateSpanId();

    EXPECT_NE(span_id1, span_id2);
}

TEST_F(SpanContextTest, ValidateTraceId) {
    // Valid trace ID
    EXPECT_TRUE(SpanContextUtils::isValidTraceId("0123456789abcdef0123456789abcdef"));

    // Invalid: wrong length
    EXPECT_FALSE(SpanContextUtils::isValidTraceId("0123456789abcdef"));

    // Invalid: all zeros
    EXPECT_FALSE(SpanContextUtils::isValidTraceId("00000000000000000000000000000000"));

    // Invalid: non-hex characters
    EXPECT_FALSE(SpanContextUtils::isValidTraceId("0123456789abcdeg0123456789abcdef"));
}

TEST_F(SpanContextTest, ValidateSpanId) {
    // Valid span ID
    EXPECT_TRUE(SpanContextUtils::isValidSpanId("0123456789abcdef"));

    // Invalid: wrong length
    EXPECT_FALSE(SpanContextUtils::isValidSpanId("0123456789abcdef0123456789abcdef"));

    // Invalid: all zeros
    EXPECT_FALSE(SpanContextUtils::isValidSpanId("0000000000000000"));

    // Invalid: non-hex characters
    EXPECT_FALSE(SpanContextUtils::isValidSpanId("0123456789abcdeg"));
}

TEST_F(SpanContextTest, ValidateTraceFlags) {
    // Valid flags
    EXPECT_TRUE(SpanContextUtils::isValidTraceFlags(0x00));
    EXPECT_TRUE(SpanContextUtils::isValidTraceFlags(0x01));

    // Invalid: other bits set
    EXPECT_FALSE(SpanContextUtils::isValidTraceFlags(0x02));
    EXPECT_FALSE(SpanContextUtils::isValidTraceFlags(0xFF));
}

TEST_F(SpanContextTest, ParseTraceparent) {
    std::string traceparent = "00-0123456789abcdef0123456789abcdef-0123456789abcdef-01";

    auto context = SpanContextUtils::parseTraceparent(traceparent);

    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(context->trace_id, "0123456789abcdef0123456789abcdef");
    EXPECT_EQ(context->span_id, "0123456789abcdef");
    EXPECT_EQ(context->trace_flags, 0x01);
    EXPECT_TRUE(context->is_remote);
}

TEST_F(SpanContextTest, ParseInvalidTraceparent) {
    // Wrong version
    auto result1 = SpanContextUtils::parseTraceparent(
        "99-0123456789abcdef0123456789abcdef-0123456789abcdef-01");
    EXPECT_FALSE(result1.has_value());

    // Wrong format
    auto result2 = SpanContextUtils::parseTraceparent("invalid");
    EXPECT_FALSE(result2.has_value());

    // All zeros trace_id
    auto result3 = SpanContextUtils::parseTraceparent(
        "00-00000000000000000000000000000000-0123456789abcdef-01");
    EXPECT_FALSE(result3.has_value());
}

TEST_F(SpanContextTest, FormatTraceparent) {
    TraceContext context;
    context.trace_id = "0123456789abcdef0123456789abcdef";
    context.span_id = "0123456789abcdef";
    context.trace_flags = 0x01;

    std::string traceparent = SpanContextUtils::formatTraceparent(context);

    EXPECT_EQ(traceparent, "00-0123456789abcdef0123456789abcdef-0123456789abcdef-01");
}

TEST_F(SpanContextTest, ParseBaggage) {
    std::string baggage = "key1=value1,key2=value2,key3=value3";

    auto result = SpanContextUtils::parseBaggage(baggage);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["key1"], "value1");
    EXPECT_EQ(result["key2"], "value2");
    EXPECT_EQ(result["key3"], "value3");
}

TEST_F(SpanContextTest, ParseEmptyBaggage) {
    auto result = SpanContextUtils::parseBaggage("");
    EXPECT_TRUE(result.empty());
}

TEST_F(SpanContextTest, FormatBaggage) {
    std::map<std::string, std::string> baggage;
    baggage["key1"] = "value1";
    baggage["key2"] = "value2";

    std::string result = SpanContextUtils::formatBaggage(baggage);

    // Note: map iteration order is sorted by key
    EXPECT_EQ(result, "key1=value1,key2=value2");
}

TEST_F(SpanContextTest, FormatEmptyBaggage) {
    std::map<std::string, std::string> baggage;
    std::string result = SpanContextUtils::formatBaggage(baggage);
    EXPECT_EQ(result, "");
}

TEST_F(SpanContextTest, ValidateContext) {
    TraceContext valid_context;
    valid_context.trace_id = "0123456789abcdef0123456789abcdef";
    valid_context.span_id = "0123456789abcdef";
    valid_context.trace_flags = 0x01;

    EXPECT_TRUE(SpanContextUtils::isValidContext(valid_context));

    TraceContext invalid_context;
    invalid_context.trace_id = "invalid";
    invalid_context.span_id = "0123456789abcdef";
    invalid_context.trace_flags = 0x01;

    EXPECT_FALSE(SpanContextUtils::isValidContext(invalid_context));
}

TEST_F(SpanContextTest, InvalidContext) {
    auto context = SpanContextUtils::invalidContext();

    EXPECT_FALSE(SpanContextUtils::isValidContext(context));
    EXPECT_EQ(context.trace_id, std::string(TRACE_ID_LENGTH, '0'));
    EXPECT_EQ(context.span_id, std::string(SPAN_ID_LENGTH, '0'));
}

TEST_F(SpanContextTest, IsSampled) {
    TraceContext sampled_context;
    sampled_context.trace_flags = TRACE_FLAG_SAMPLED;
    EXPECT_TRUE(SpanContextUtils::isSampled(sampled_context));

    TraceContext unsampled_context;
    unsampled_context.trace_flags = 0x00;
    EXPECT_FALSE(SpanContextUtils::isSampled(unsampled_context));
}

TEST_F(SpanContextTest, ExtractTraceContext) {
    std::map<std::string, std::string> carrier;
    carrier["traceparent"] = "00-0123456789abcdef0123456789abcdef-0123456789abcdef-01";
    carrier["tracestate"] = "vendor=value";
    carrier["baggage"] = "key1=value1";

    auto context = extractTraceContext(carrier);

    EXPECT_EQ(context.trace_id, "0123456789abcdef0123456789abcdef");
    EXPECT_EQ(context.span_id, "0123456789abcdef");
    EXPECT_EQ(context.trace_flags, 0x01);
    EXPECT_EQ(context.trace_state, "vendor=value");
    EXPECT_EQ(context.baggage.size(), 1);
    EXPECT_EQ(context.baggage["key1"], "value1");
    EXPECT_TRUE(context.is_remote);
}

TEST_F(SpanContextTest, ExtractInvalidTraceContext) {
    std::map<std::string, std::string> carrier;
    carrier["traceparent"] = "invalid";

    auto context = extractTraceContext(carrier);

    EXPECT_FALSE(SpanContextUtils::isValidContext(context));
}

TEST_F(SpanContextTest, InjectTraceContext) {
    TraceContext context;
    context.trace_id = "0123456789abcdef0123456789abcdef";
    context.span_id = "0123456789abcdef";
    context.trace_flags = 0x01;
    context.trace_state = "vendor=value";
    context.baggage["key1"] = "value1";

    std::map<std::string, std::string> carrier;
    injectTraceContext(context, carrier);

    EXPECT_EQ(carrier["traceparent"],
              "00-0123456789abcdef0123456789abcdef-0123456789abcdef-01");
    EXPECT_EQ(carrier["tracestate"], "vendor=value");
    EXPECT_EQ(carrier["baggage"], "key1=value1");
}

TEST_F(SpanContextTest, InjectInvalidTraceContext) {
    TraceContext context = SpanContextUtils::invalidContext();

    std::map<std::string, std::string> carrier;
    injectTraceContext(context, carrier);

    // Should not inject invalid context
    EXPECT_TRUE(carrier.empty());
}

TEST_F(SpanContextTest, RoundTripContextPropagation) {
    // Create original context
    TraceContext original;
    original.trace_id = SpanContextUtils::generateTraceId();
    original.span_id = SpanContextUtils::generateSpanId();
    original.trace_flags = TRACE_FLAG_SAMPLED;
    original.trace_state = "vendor=value";
    original.baggage["key1"] = "value1";

    // Inject into carrier
    std::map<std::string, std::string> carrier;
    injectTraceContext(original, carrier);

    // Extract from carrier
    auto extracted = extractTraceContext(carrier);

    // Verify round-trip
    EXPECT_EQ(extracted.trace_id, original.trace_id);
    EXPECT_EQ(extracted.span_id, original.span_id);
    EXPECT_EQ(extracted.trace_flags, original.trace_flags);
    EXPECT_EQ(extracted.trace_state, original.trace_state);
    EXPECT_EQ(extracted.baggage, original.baggage);
}
