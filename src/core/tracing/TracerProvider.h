#pragma once

#include <string>
#include <memory>
#include <map>
#include <chrono>

namespace mxrc {
namespace tracing {

/**
 * @brief Span status enumeration
 */
enum class SpanStatus {
    UNSET,          // 상태 미설정 (기본값)
    OK,             // 성공
    ERROR           // 오류
};

/**
 * @brief Trace context for W3C Trace Context propagation
 *
 * Follows W3C Trace Context standard for distributed tracing.
 */
struct TraceContext {
    std::string trace_id;        // 16-byte hex string (32 chars)
    std::string span_id;         // 8-byte hex string (16 chars)
    std::string parent_span_id;  // Empty for root span
    uint8_t trace_flags;         // Bit 0: sampled
    std::string trace_state;     // Vendor-specific state
    std::map<std::string, std::string> baggage;  // Custom context propagation
    bool is_remote;              // Propagated from remote process
};

/**
 * @brief Span represents a unit of work in distributed tracing
 */
class ISpan {
public:
    virtual ~ISpan() = default;

    /**
     * @brief End the span
     *
     * Records end time and finalizes span.
     * Should be called automatically by RAII Span Guard.
     */
    virtual void end() = 0;

    /**
     * @brief Set span status
     *
     * @param status SpanStatus (OK or ERROR)
     * @param message Optional status message (for errors)
     */
    virtual void setStatus(SpanStatus status, const std::string& message = "") = 0;

    /**
     * @brief Add attribute to span
     *
     * @param key Attribute key
     * @param value Attribute value
     */
    virtual void setAttribute(const std::string& key, const std::string& value) = 0;

    /**
     * @brief Add event to span
     *
     * Events are timestamped log entries within a span.
     *
     * @param name Event name
     * @param attributes Optional event attributes
     */
    virtual void addEvent(const std::string& name,
                         const std::map<std::string, std::string>& attributes = {}) = 0;

    /**
     * @brief Get span context
     *
     * @return TraceContext Current span's trace context
     */
    virtual TraceContext getContext() const = 0;

    /**
     * @brief Check if span is recording
     *
     * @return true if span is actively recording, false otherwise
     */
    virtual bool isRecording() const = 0;
};

/**
 * @brief RAII Span Guard for automatic span lifecycle management
 *
 * Follows MXRC Constitution principle: RAII (Resource Acquisition Is Initialization).
 *
 * Usage:
 *   {
 *       SpanGuard guard(tracer->startSpan("operation_name"));
 *       // ... do work ...
 *   } // Span automatically ends here
 */
class SpanGuard {
public:
    explicit SpanGuard(std::shared_ptr<ISpan> span) : span_(std::move(span)) {}

    ~SpanGuard() {
        if (span_ && span_->isRecording()) {
            span_->end();
        }
    }

    // Non-copyable, movable
    SpanGuard(const SpanGuard&) = delete;
    SpanGuard& operator=(const SpanGuard&) = delete;
    SpanGuard(SpanGuard&&) = default;
    SpanGuard& operator=(SpanGuard&&) = default;

    ISpan* operator->() { return span_.get(); }
    const ISpan* operator->() const { return span_.get(); }

private:
    std::shared_ptr<ISpan> span_;
};

/**
 * @brief Tracer interface for creating spans
 *
 * Follows MXRC Constitution principle: Interface-based design (I-prefix).
 */
class ITracer {
public:
    virtual ~ITracer() = default;

    /**
     * @brief Start a new span
     *
     * Creates a new span with the given operation name.
     * If a parent context is active (thread-local), creates a child span.
     *
     * @param operation_name Name of the operation being traced
     * @param attributes Optional span attributes
     * @return std::shared_ptr<ISpan> New span instance
     */
    virtual std::shared_ptr<ISpan> startSpan(
        const std::string& operation_name,
        const std::map<std::string, std::string>& attributes = {}) = 0;

    /**
     * @brief Start a span with explicit parent context
     *
     * @param operation_name Name of the operation
     * @param parent_context Parent trace context
     * @param attributes Optional span attributes
     * @return std::shared_ptr<ISpan> New span instance
     */
    virtual std::shared_ptr<ISpan> startSpan(
        const std::string& operation_name,
        const TraceContext& parent_context,
        const std::map<std::string, std::string>& attributes = {}) = 0;

    /**
     * @brief Get current active span from thread-local storage
     *
     * @return std::shared_ptr<ISpan> Active span or nullptr
     */
    virtual std::shared_ptr<ISpan> getCurrentSpan() const = 0;

    /**
     * @brief Set current span in thread-local storage
     *
     * @param span Span to set as current
     */
    virtual void setCurrentSpan(std::shared_ptr<ISpan> span) = 0;

    /**
     * @brief Extract trace context from carrier (for propagation)
     *
     * Used for receiving trace context from remote processes.
     *
     * @param carrier Map containing W3C Trace Context headers
     * @return TraceContext Extracted trace context
     */
    virtual TraceContext extractContext(const std::map<std::string, std::string>& carrier) const = 0;

    /**
     * @brief Inject trace context into carrier (for propagation)
     *
     * Used for sending trace context to remote processes.
     *
     * @param context Trace context to inject
     * @param carrier Output map to inject headers into
     */
    virtual void injectContext(const TraceContext& context,
                              std::map<std::string, std::string>& carrier) const = 0;
};

/**
 * @brief Tracer Provider interface for tracer lifecycle management
 */
class ITracerProvider {
public:
    virtual ~ITracerProvider() = default;

    /**
     * @brief Get tracer instance
     *
     * @param name Tracer name (e.g., "mxrc-rt", "mxrc-nonrt")
     * @return std::shared_ptr<ITracer> Tracer instance
     */
    virtual std::shared_ptr<ITracer> getTracer(const std::string& name) = 0;

    /**
     * @brief Shutdown tracer provider
     *
     * Flushes all pending spans and releases resources.
     * Should be called on application shutdown.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Force flush all pending spans
     *
     * Blocks until all spans are exported or timeout occurs.
     *
     * @param timeout_ms Timeout in milliseconds
     * @return true if flush succeeded, false if timeout
     */
    virtual bool forceFlush(uint32_t timeout_ms = 5000) = 0;
};

/**
 * @brief Convert SpanStatus to string
 */
inline std::string spanStatusToString(SpanStatus status) {
    switch (status) {
        case SpanStatus::UNSET: return "UNSET";
        case SpanStatus::OK: return "OK";
        case SpanStatus::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace tracing
} // namespace mxrc
