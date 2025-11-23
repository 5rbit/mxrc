#pragma once

#include "TracerProvider.h"
#include <string>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace mxrc {
namespace tracing {

/**
 * @brief W3C Trace Context constants
 */
constexpr uint8_t TRACE_FLAG_SAMPLED = 0x01;
constexpr size_t TRACE_ID_LENGTH = 32;  // 16 bytes = 32 hex chars
constexpr size_t SPAN_ID_LENGTH = 16;   // 8 bytes = 16 hex chars

/**
 * @brief W3C Trace Context header names
 */
constexpr const char* TRACEPARENT_HEADER = "traceparent";
constexpr const char* TRACESTATE_HEADER = "tracestate";
constexpr const char* BAGGAGE_HEADER = "baggage";

/**
 * @brief Span Context Utilities
 *
 * Provides utilities for W3C Trace Context standard compliance.
 */
class SpanContextUtils {
public:
    /**
     * @brief Generate random trace ID (32-char hex string)
     *
     * @return std::string Generated trace ID
     */
    static std::string generateTraceId();

    /**
     * @brief Generate random span ID (16-char hex string)
     *
     * @return std::string Generated span ID
     */
    static std::string generateSpanId();

    /**
     * @brief Validate trace ID format
     *
     * Must be 32-char hex string, not all zeros.
     *
     * @param trace_id Trace ID to validate
     * @return true if valid, false otherwise
     */
    static bool isValidTraceId(const std::string& trace_id);

    /**
     * @brief Validate span ID format
     *
     * Must be 16-char hex string, not all zeros.
     *
     * @param span_id Span ID to validate
     * @return true if valid, false otherwise
     */
    static bool isValidSpanId(const std::string& span_id);

    /**
     * @brief Validate trace flags
     *
     * Only bit 0 (sampled) is defined by W3C spec.
     *
     * @param flags Trace flags byte
     * @return true if valid, false otherwise
     */
    static bool isValidTraceFlags(uint8_t flags);

    /**
     * @brief Parse W3C traceparent header
     *
     * Format: "00-{trace_id}-{span_id}-{flags}"
     *
     * @param traceparent Traceparent header value
     * @return std::optional<TraceContext> Parsed context or nullopt if invalid
     */
    static std::optional<TraceContext> parseTraceparent(const std::string& traceparent);

    /**
     * @brief Format trace context as W3C traceparent header
     *
     * @param context Trace context to format
     * @return std::string Traceparent header value
     */
    static std::string formatTraceparent(const TraceContext& context);

    /**
     * @brief Parse W3C tracestate header
     *
     * Format: "vendor1=value1,vendor2=value2"
     *
     * @param tracestate Tracestate header value
     * @return std::string Parsed tracestate
     */
    static std::string parseTracestate(const std::string& tracestate);

    /**
     * @brief Parse W3C baggage header
     *
     * Format: "key1=value1,key2=value2"
     *
     * @param baggage Baggage header value
     * @return std::map<std::string, std::string> Parsed baggage map
     */
    static std::map<std::string, std::string> parseBaggage(const std::string& baggage);

    /**
     * @brief Format baggage as W3C baggage header
     *
     * @param baggage Baggage map
     * @return std::string Baggage header value
     */
    static std::string formatBaggage(const std::map<std::string, std::string>& baggage);

    /**
     * @brief Check if trace context is valid
     *
     * @param context Trace context to validate
     * @return true if valid, false otherwise
     */
    static bool isValidContext(const TraceContext& context);

    /**
     * @brief Create invalid/empty trace context
     *
     * @return TraceContext Invalid context
     */
    static TraceContext invalidContext();

    /**
     * @brief Check if context is sampled
     *
     * @param context Trace context
     * @return true if sampled, false otherwise
     */
    static bool isSampled(const TraceContext& context);

private:
    static thread_local std::mt19937_64 rng_;
};

/**
 * @brief Extract trace context from carrier
 *
 * Helper function for context propagation.
 *
 * @param carrier Map containing W3C headers
 * @return TraceContext Extracted context (may be invalid)
 */
TraceContext extractTraceContext(const std::map<std::string, std::string>& carrier);

/**
 * @brief Inject trace context into carrier
 *
 * Helper function for context propagation.
 *
 * @param context Trace context to inject
 * @param carrier Output map to inject headers into
 */
void injectTraceContext(const TraceContext& context,
                       std::map<std::string, std::string>& carrier);

} // namespace tracing
} // namespace mxrc
