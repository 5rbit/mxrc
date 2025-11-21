#include "TracerProvider.h"
#include "SpanContext.h"
#include <spdlog/spdlog.h>
#include <mutex>
#include <vector>
#include <atomic>

namespace mxrc {
namespace tracing {

/**
 * @brief Span implementation
 *
 * Lightweight span implementation that stores span data in memory.
 */
class Span : public ISpan {
public:
    Span(const std::string& operation_name,
         const TraceContext& context,
         std::chrono::steady_clock::time_point start_time)
        : operation_name_(operation_name)
        , context_(context)
        , start_time_(start_time)
        , is_recording_(true)
        , status_(SpanStatus::UNSET) {
    }

    ~Span() override {
        if (is_recording_) {
            end();
        }
    }

    void end() override {
        if (!is_recording_) {
            return;
        }

        end_time_ = std::chrono::steady_clock::now();
        is_recording_ = false;

        // Calculate duration
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time_ - start_time_).count();

        // Log span completion (console exporter)
        spdlog::debug("[Trace] Span ended: {} (trace_id={}, span_id={}, duration_us={})",
                     operation_name_,
                     context_.trace_id,
                     context_.span_id,
                     duration_us);

        // If status is ERROR, log as warning
        if (status_ == SpanStatus::ERROR) {
            spdlog::warn("[Trace] Span failed: {} - {} (trace_id={}, span_id={})",
                        operation_name_,
                        status_message_,
                        context_.trace_id,
                        context_.span_id);
        }
    }

    void setStatus(SpanStatus status, const std::string& message = "") override {
        status_ = status;
        status_message_ = message;
    }

    void setAttribute(const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        attributes_[key] = value;
    }

    void addEvent(const std::string& name,
                 const std::map<std::string, std::string>& attributes = {}) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto timestamp = std::chrono::steady_clock::now();
        auto offset_us = std::chrono::duration_cast<std::chrono::microseconds>(
            timestamp - start_time_).count();

        spdlog::debug("[Trace] Span event: {} in {} (offset_us={}, trace_id={}, span_id={})",
                     name,
                     operation_name_,
                     offset_us,
                     context_.trace_id,
                     context_.span_id);
    }

    TraceContext getContext() const override {
        return context_;
    }

    bool isRecording() const override {
        return is_recording_;
    }

private:
    std::string operation_name_;
    TraceContext context_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
    std::atomic<bool> is_recording_;
    SpanStatus status_;
    std::string status_message_;
    std::map<std::string, std::string> attributes_;
    std::mutex mutex_;
};

/**
 * @brief Tracer implementation
 *
 * Lightweight tracer that manages span lifecycle and context propagation.
 */
class Tracer : public ITracer {
public:
    explicit Tracer(const std::string& name) : name_(name) {}

    std::shared_ptr<ISpan> startSpan(
        const std::string& operation_name,
        const std::map<std::string, std::string>& attributes = {}) override {

        TraceContext context;

        // Check for current span in thread-local storage
        auto current_span = getCurrentSpan();
        if (current_span) {
            // Create child span
            auto parent_context = current_span->getContext();
            context.trace_id = parent_context.trace_id;
            context.span_id = SpanContextUtils::generateSpanId();
            context.parent_span_id = parent_context.span_id;
            context.trace_flags = parent_context.trace_flags;
            context.trace_state = parent_context.trace_state;
            context.baggage = parent_context.baggage;
            context.is_remote = false;
        } else {
            // Create root span
            context.trace_id = SpanContextUtils::generateTraceId();
            context.span_id = SpanContextUtils::generateSpanId();
            context.parent_span_id = "";
            context.trace_flags = TRACE_FLAG_SAMPLED;  // Always sample for now
            context.is_remote = false;
        }

        auto start_time = std::chrono::steady_clock::now();
        auto span = std::make_shared<Span>(operation_name, context, start_time);

        // Add attributes
        for (const auto& [key, value] : attributes) {
            span->setAttribute(key, value);
        }

        spdlog::debug("[Trace] Span started: {} (trace_id={}, span_id={}, parent_span_id={})",
                     operation_name,
                     context.trace_id,
                     context.span_id,
                     context.parent_span_id);

        return span;
    }

    std::shared_ptr<ISpan> startSpan(
        const std::string& operation_name,
        const TraceContext& parent_context,
        const std::map<std::string, std::string>& attributes = {}) override {

        TraceContext context;
        context.trace_id = parent_context.trace_id;
        context.span_id = SpanContextUtils::generateSpanId();
        context.parent_span_id = parent_context.span_id;
        context.trace_flags = parent_context.trace_flags;
        context.trace_state = parent_context.trace_state;
        context.baggage = parent_context.baggage;
        context.is_remote = parent_context.is_remote;

        auto start_time = std::chrono::steady_clock::now();
        auto span = std::make_shared<Span>(operation_name, context, start_time);

        // Add attributes
        for (const auto& [key, value] : attributes) {
            span->setAttribute(key, value);
        }

        spdlog::debug("[Trace] Span started (explicit parent): {} (trace_id={}, span_id={}, parent_span_id={})",
                     operation_name,
                     context.trace_id,
                     context.span_id,
                     context.parent_span_id);

        return span;
    }

    std::shared_ptr<ISpan> getCurrentSpan() const override {
        return current_span_;
    }

    void setCurrentSpan(std::shared_ptr<ISpan> span) override {
        current_span_ = span;
    }

    TraceContext extractContext(const std::map<std::string, std::string>& carrier) const override {
        return extractTraceContext(carrier);
    }

    void injectContext(const TraceContext& context,
                      std::map<std::string, std::string>& carrier) const override {
        injectTraceContext(context, carrier);
    }

private:
    std::string name_;
    static thread_local std::shared_ptr<ISpan> current_span_;
};

// Define thread-local storage
thread_local std::shared_ptr<ISpan> Tracer::current_span_ = nullptr;

/**
 * @brief TracerProvider implementation
 *
 * Manages tracer instances and lifecycle.
 */
class TracerProvider : public ITracerProvider {
public:
    TracerProvider() = default;

    std::shared_ptr<ITracer> getTracer(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tracers_.find(name);
        if (it != tracers_.end()) {
            return it->second;
        }

        auto tracer = std::make_shared<Tracer>(name);
        tracers_[name] = tracer;

        spdlog::debug("[Trace] Created tracer: {}", name);

        return tracer;
    }

    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);

        spdlog::info("[Trace] Shutting down TracerProvider with {} tracers", tracers_.size());
        tracers_.clear();
    }

    bool forceFlush(uint32_t timeout_ms = 5000) override {
        // In a real implementation, this would flush to exporters
        // For lightweight impl, just log
        spdlog::debug("[Trace] Force flush called (timeout_ms={})", timeout_ms);
        return true;
    }

private:
    std::map<std::string, std::shared_ptr<ITracer>> tracers_;
    std::mutex mutex_;
};

// Global tracer provider instance
static std::shared_ptr<ITracerProvider> global_tracer_provider_ = nullptr;
static std::mutex global_mutex_;

std::shared_ptr<ITracerProvider> getGlobalTracerProvider() {
    std::lock_guard<std::mutex> lock(global_mutex_);

    if (!global_tracer_provider_) {
        global_tracer_provider_ = std::make_shared<TracerProvider>();
        spdlog::info("[Trace] Initialized global TracerProvider");
    }

    return global_tracer_provider_;
}

void setGlobalTracerProvider(std::shared_ptr<ITracerProvider> provider) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    global_tracer_provider_ = provider;
}

} // namespace tracing
} // namespace mxrc
