#include "RTCycleTracer.h"
#include "SpanContext.h"
#include <spdlog/spdlog.h>
#include <random>
#include <sstream>

namespace mxrc {
namespace tracing {

// Thread-local storage
thread_local std::shared_ptr<ISpan> RTCycleTracer::current_cycle_span_ = nullptr;
thread_local uint64_t RTCycleTracer::current_cycle_number_ = 0;
thread_local std::chrono::steady_clock::time_point RTCycleTracer::cycle_start_time_;

RTCycleTracer::RTCycleTracer(std::shared_ptr<ITracer> tracer, double sampling_rate)
    : tracer_(tracer)
    , enabled_(true)
    , sampling_rate_(sampling_rate)
    , total_cycles_(0)
    , sampled_cycles_(0) {

    if (sampling_rate < 0.0 || sampling_rate > 1.0) {
        spdlog::warn("[RTCycleTracer] Invalid sampling rate {}, using 0.1", sampling_rate);
        sampling_rate_ = 0.1;
    }
}

void RTCycleTracer::startCycle(uint64_t cycle_number) {
    if (!enabled_) {
        return;
    }

    total_cycles_++;

    // Check if we should sample this cycle
    if (!shouldSample()) {
        current_cycle_span_ = nullptr;
        return;
    }

    sampled_cycles_++;
    current_cycle_number_ = cycle_number;
    cycle_start_time_ = std::chrono::steady_clock::now();

    try {
        // Create span for RT cycle
        std::map<std::string, std::string> attributes;
        attributes["mxrc.component"] = "rt_executive";
        attributes["mxrc.cycle_number"] = std::to_string(cycle_number);
        attributes["mxrc.operation"] = "rt_cycle";

        current_cycle_span_ = tracer_->startSpan("RT.cycle", attributes);

    } catch (const std::exception& e) {
        spdlog::error("[RTCycleTracer] Error in startCycle: {}", e.what());
        current_cycle_span_ = nullptr;
    }
}

void RTCycleTracer::endCycle(bool success) {
    if (!enabled_ || !current_cycle_span_) {
        return;
    }

    try {
        auto end_time = std::chrono::steady_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - cycle_start_time_).count();

        // Add cycle duration attribute
        current_cycle_span_->setAttribute("mxrc.cycle_duration_us",
                                         std::to_string(duration_us));

        // Set span status
        if (success) {
            current_cycle_span_->setStatus(SpanStatus::OK);
        } else {
            current_cycle_span_->setStatus(SpanStatus::ERROR, "RT cycle failed");
        }

        // End the span
        current_cycle_span_->end();
        current_cycle_span_ = nullptr;

    } catch (const std::exception& e) {
        spdlog::error("[RTCycleTracer] Error in endCycle: {}", e.what());
    }
}

void RTCycleTracer::recordAction(const std::string& action_name, uint64_t duration_us) {
    if (!enabled_ || !current_cycle_span_) {
        return;
    }

    try {
        // Add event to current cycle span
        // This is a low-overhead operation
        std::map<std::string, std::string> event_attrs;
        event_attrs["action.name"] = action_name;
        event_attrs["action.duration_us"] = std::to_string(duration_us);

        current_cycle_span_->addEvent("action.executed", event_attrs);

    } catch (const std::exception& e) {
        // Don't log errors in RT path to avoid overhead
        // Just silently fail
    }
}

void RTCycleTracer::recordTiming(uint64_t schedule_time_us,
                                uint64_t actual_time_us,
                                uint64_t deadline_us) {
    if (!enabled_ || !current_cycle_span_) {
        return;
    }

    try {
        // Calculate jitter and slack
        int64_t jitter_us = static_cast<int64_t>(actual_time_us) -
                           static_cast<int64_t>(schedule_time_us);
        int64_t slack_us = static_cast<int64_t>(deadline_us) -
                          static_cast<int64_t>(actual_time_us);

        // Add timing attributes
        current_cycle_span_->setAttribute("mxrc.schedule_time_us",
                                         std::to_string(schedule_time_us));
        current_cycle_span_->setAttribute("mxrc.actual_time_us",
                                         std::to_string(actual_time_us));
        current_cycle_span_->setAttribute("mxrc.deadline_us",
                                         std::to_string(deadline_us));
        current_cycle_span_->setAttribute("mxrc.jitter_us",
                                         std::to_string(jitter_us));
        current_cycle_span_->setAttribute("mxrc.slack_us",
                                         std::to_string(slack_us));

        // If jitter is high, add event
        if (std::abs(jitter_us) > 1000) {  // > 1ms jitter
            std::map<std::string, std::string> event_attrs;
            event_attrs["jitter_us"] = std::to_string(jitter_us);
            current_cycle_span_->addEvent("high_jitter_detected", event_attrs);
        }

        // If slack is low, add event
        if (slack_us < 1000) {  // < 1ms slack
            std::map<std::string, std::string> event_attrs;
            event_attrs["slack_us"] = std::to_string(slack_us);
            current_cycle_span_->addEvent("low_slack_detected", event_attrs);
        }

    } catch (const std::exception& e) {
        // Don't log errors in RT path
    }
}

void RTCycleTracer::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool RTCycleTracer::isEnabled() const {
    return enabled_;
}

void RTCycleTracer::setSamplingRate(double rate) {
    if (rate < 0.0 || rate > 1.0) {
        spdlog::warn("[RTCycleTracer] Invalid sampling rate {}, ignoring", rate);
        return;
    }
    sampling_rate_ = rate;
}

double RTCycleTracer::getSamplingRate() const {
    return sampling_rate_;
}

std::string RTCycleTracer::getStats() const {
    std::ostringstream oss;
    oss << "RTCycleTracer Stats:\n"
        << "  Total cycles: " << total_cycles_.load() << "\n"
        << "  Sampled cycles: " << sampled_cycles_.load() << "\n"
        << "  Sampling rate: " << (sampling_rate_.load() * 100.0) << "%\n"
        << "  Actual sample rate: ";

    if (total_cycles_ > 0) {
        double actual_rate = static_cast<double>(sampled_cycles_) /
                           static_cast<double>(total_cycles_);
        oss << (actual_rate * 100.0) << "%";
    } else {
        oss << "N/A";
    }

    return oss.str();
}

bool RTCycleTracer::shouldSample() {
    if (sampling_rate_ >= 1.0) {
        return true;  // Always sample
    }

    if (sampling_rate_ <= 0.0) {
        return false;  // Never sample
    }

    // Use thread-local random number generator
    static thread_local std::mt19937 rng(std::random_device{}());
    static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);

    return dist(rng) < sampling_rate_;
}

} // namespace tracing
} // namespace mxrc
