#pragma once

#include "TracerProvider.h"
#include <memory>
#include <string>
#include <chrono>
#include <atomic>

namespace mxrc {
namespace tracing {

/**
 * @brief RT Cycle Tracer
 *
 * Provides low-overhead tracing for real-time cycles.
 * Designed to minimize performance impact (<5% overhead target).
 *
 * Usage:
 *   auto tracer = getGlobalTracerProvider()->getTracer("mxrc-rt");
 *   auto rtTracer = std::make_shared<RTCycleTracer>(tracer);
 *
 *   // At start of RT cycle:
 *   rtTracer->startCycle(cycle_number);
 *
 *   // Record actions within cycle:
 *   rtTracer->recordAction("action_name", duration_us);
 *
 *   // At end of RT cycle:
 *   rtTracer->endCycle(success);
 *
 * Implementation notes:
 * - Uses RAII pattern for automatic span lifecycle
 * - Minimizes allocations in RT path
 * - Thread-local storage for zero-contention
 * - Sampling support to reduce overhead
 */
class RTCycleTracer {
public:
    /**
     * @brief Constructor
     *
     * @param tracer Tracer instance to use for creating spans
     * @param sampling_rate Sampling rate (0.0-1.0, default: 0.1 = 10%)
     */
    explicit RTCycleTracer(std::shared_ptr<ITracer> tracer, double sampling_rate = 0.1);

    /**
     * @brief Destructor
     */
    ~RTCycleTracer() = default;

    /**
     * @brief Start RT cycle tracing
     *
     * Creates a span for the current RT cycle.
     * Uses sampling to reduce overhead.
     *
     * @param cycle_number RT cycle number
     */
    void startCycle(uint64_t cycle_number);

    /**
     * @brief End RT cycle tracing
     *
     * Ends the current cycle span.
     *
     * @param success Whether cycle completed successfully
     */
    void endCycle(bool success = true);

    /**
     * @brief Record action execution within cycle
     *
     * Adds an event to the current cycle span.
     * Low-overhead operation.
     *
     * @param action_name Name of action executed
     * @param duration_us Action duration in microseconds
     */
    void recordAction(const std::string& action_name, uint64_t duration_us);

    /**
     * @brief Record cycle timing metrics
     *
     * Adds timing attributes to the current cycle span.
     *
     * @param schedule_time_us Scheduled start time (microseconds since epoch)
     * @param actual_time_us Actual start time (microseconds since epoch)
     * @param deadline_us Deadline (microseconds since epoch)
     */
    void recordTiming(uint64_t schedule_time_us, uint64_t actual_time_us, uint64_t deadline_us);

    /**
     * @brief Enable/disable RT cycle tracing
     *
     * @param enabled True to enable, false to disable
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if RT cycle tracing is enabled
     *
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const;

    /**
     * @brief Set sampling rate
     *
     * @param rate Sampling rate (0.0-1.0)
     */
    void setSamplingRate(double rate);

    /**
     * @brief Get sampling rate
     *
     * @return double Current sampling rate
     */
    double getSamplingRate() const;

    /**
     * @brief Get statistics
     *
     * @return std::string Statistics string
     */
    std::string getStats() const;

private:
    std::shared_ptr<ITracer> tracer_;
    std::atomic<bool> enabled_;
    std::atomic<double> sampling_rate_;

    // Statistics
    std::atomic<uint64_t> total_cycles_;
    std::atomic<uint64_t> sampled_cycles_;

    // Thread-local current cycle span
    static thread_local std::shared_ptr<ISpan> current_cycle_span_;
    static thread_local uint64_t current_cycle_number_;
    static thread_local std::chrono::steady_clock::time_point cycle_start_time_;

    /**
     * @brief Check if current cycle should be sampled
     *
     * @return true if should sample, false otherwise
     */
    bool shouldSample();
};

/**
 * @brief RAII guard for RT cycle tracing
 *
 * Automatically starts cycle on construction and ends on destruction.
 *
 * Usage:
 *   {
 *       RTCycleGuard guard(rtTracer, cycle_number);
 *       // ... RT cycle work ...
 *   } // Cycle automatically ends here
 */
class RTCycleGuard {
public:
    RTCycleGuard(std::shared_ptr<RTCycleTracer> tracer, uint64_t cycle_number)
        : tracer_(std::move(tracer))
        , cycle_number_(cycle_number) {
        if (tracer_) {
            tracer_->startCycle(cycle_number_);
        }
    }

    ~RTCycleGuard() {
        if (tracer_) {
            tracer_->endCycle();
        }
    }

    // Non-copyable, movable
    RTCycleGuard(const RTCycleGuard&) = delete;
    RTCycleGuard& operator=(const RTCycleGuard&) = delete;
    RTCycleGuard(RTCycleGuard&&) = default;
    RTCycleGuard& operator=(RTCycleGuard&&) = default;

    /**
     * @brief Record action within guarded cycle
     *
     * @param action_name Action name
     * @param duration_us Action duration in microseconds
     */
    void recordAction(const std::string& action_name, uint64_t duration_us) {
        if (tracer_) {
            tracer_->recordAction(action_name, duration_us);
        }
    }

private:
    std::shared_ptr<RTCycleTracer> tracer_;
    uint64_t cycle_number_;
};

} // namespace tracing
} // namespace mxrc
