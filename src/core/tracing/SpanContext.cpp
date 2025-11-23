#include "SpanContext.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>

namespace mxrc {
namespace tracing {

// Thread-local random number generator
thread_local std::mt19937_64 SpanContextUtils::rng_(std::random_device{}());

std::string SpanContextUtils::generateTraceId() {
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t high = dist(rng_);
    uint64_t low = dist(rng_);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << high
        << std::setw(16) << low;

    return oss.str();
}

std::string SpanContextUtils::generateSpanId() {
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t id = dist(rng_);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << id;

    return oss.str();
}

bool SpanContextUtils::isValidTraceId(const std::string& trace_id) {
    if (trace_id.length() != TRACE_ID_LENGTH) {
        return false;
    }

    // Check if all characters are hex
    if (!std::all_of(trace_id.begin(), trace_id.end(),
                     [](char c) { return std::isxdigit(c); })) {
        return false;
    }

    // Check if not all zeros
    if (std::all_of(trace_id.begin(), trace_id.end(),
                    [](char c) { return c == '0'; })) {
        return false;
    }

    return true;
}

bool SpanContextUtils::isValidSpanId(const std::string& span_id) {
    if (span_id.length() != SPAN_ID_LENGTH) {
        return false;
    }

    // Check if all characters are hex
    if (!std::all_of(span_id.begin(), span_id.end(),
                     [](char c) { return std::isxdigit(c); })) {
        return false;
    }

    // Check if not all zeros
    if (std::all_of(span_id.begin(), span_id.end(),
                    [](char c) { return c == '0'; })) {
        return false;
    }

    return true;
}

bool SpanContextUtils::isValidTraceFlags(uint8_t flags) {
    // Currently only bit 0 is defined (sampled)
    // All other bits should be 0
    return (flags & ~TRACE_FLAG_SAMPLED) == 0;
}

std::optional<TraceContext> SpanContextUtils::parseTraceparent(const std::string& traceparent) {
    // Format: "00-{trace_id}-{span_id}-{flags}"
    std::regex pattern(R"(^00-([0-9a-f]{32})-([0-9a-f]{16})-([0-9a-f]{2})$)");
    std::smatch matches;

    if (!std::regex_match(traceparent, matches, pattern)) {
        return std::nullopt;
    }

    std::string trace_id = matches[1].str();
    std::string span_id = matches[2].str();
    std::string flags_str = matches[3].str();

    if (!isValidTraceId(trace_id) || !isValidSpanId(span_id)) {
        return std::nullopt;
    }

    uint8_t flags = static_cast<uint8_t>(std::stoul(flags_str, nullptr, 16));

    TraceContext context;
    context.trace_id = trace_id;
    context.span_id = span_id;
    context.parent_span_id = "";
    context.trace_flags = flags;
    context.is_remote = true;

    return context;
}

std::string SpanContextUtils::formatTraceparent(const TraceContext& context) {
    std::ostringstream oss;
    oss << "00-"
        << context.trace_id << "-"
        << context.span_id << "-"
        << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<int>(context.trace_flags);

    return oss.str();
}

std::string SpanContextUtils::parseTracestate(const std::string& tracestate) {
    // Simple passthrough for now
    // In production, you might want to validate vendor-specific format
    return tracestate;
}

std::map<std::string, std::string> SpanContextUtils::parseBaggage(const std::string& baggage) {
    std::map<std::string, std::string> result;

    if (baggage.empty()) {
        return result;
    }

    // Format: "key1=value1,key2=value2"
    std::istringstream iss(baggage);
    std::string item;

    while (std::getline(iss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);

        auto eq_pos = item.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = item.substr(0, eq_pos);
            std::string value = item.substr(eq_pos + 1);

            result[key] = value;
        }
    }

    return result;
}

std::string SpanContextUtils::formatBaggage(const std::map<std::string, std::string>& baggage) {
    if (baggage.empty()) {
        return "";
    }

    std::ostringstream oss;
    bool first = true;

    for (const auto& [key, value] : baggage) {
        if (!first) {
            oss << ",";
        }
        oss << key << "=" << value;
        first = false;
    }

    return oss.str();
}

bool SpanContextUtils::isValidContext(const TraceContext& context) {
    return isValidTraceId(context.trace_id) &&
           isValidSpanId(context.span_id) &&
           isValidTraceFlags(context.trace_flags);
}

TraceContext SpanContextUtils::invalidContext() {
    TraceContext context;
    context.trace_id = std::string(TRACE_ID_LENGTH, '0');
    context.span_id = std::string(SPAN_ID_LENGTH, '0');
    context.parent_span_id = "";
    context.trace_flags = 0;
    context.is_remote = false;
    return context;
}

bool SpanContextUtils::isSampled(const TraceContext& context) {
    return (context.trace_flags & TRACE_FLAG_SAMPLED) != 0;
}

TraceContext extractTraceContext(const std::map<std::string, std::string>& carrier) {
    auto it = carrier.find(TRACEPARENT_HEADER);
    if (it == carrier.end()) {
        return SpanContextUtils::invalidContext();
    }

    auto context = SpanContextUtils::parseTraceparent(it->second);
    if (!context.has_value()) {
        return SpanContextUtils::invalidContext();
    }

    TraceContext result = context.value();

    // Parse tracestate
    auto ts_it = carrier.find(TRACESTATE_HEADER);
    if (ts_it != carrier.end()) {
        result.trace_state = SpanContextUtils::parseTracestate(ts_it->second);
    }

    // Parse baggage
    auto bag_it = carrier.find(BAGGAGE_HEADER);
    if (bag_it != carrier.end()) {
        result.baggage = SpanContextUtils::parseBaggage(bag_it->second);
    }

    return result;
}

void injectTraceContext(const TraceContext& context,
                       std::map<std::string, std::string>& carrier) {
    if (!SpanContextUtils::isValidContext(context)) {
        return;
    }

    carrier[TRACEPARENT_HEADER] = SpanContextUtils::formatTraceparent(context);

    if (!context.trace_state.empty()) {
        carrier[TRACESTATE_HEADER] = context.trace_state;
    }

    if (!context.baggage.empty()) {
        carrier[BAGGAGE_HEADER] = SpanContextUtils::formatBaggage(context.baggage);
    }
}

} // namespace tracing
} // namespace mxrc
