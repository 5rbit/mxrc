#include "EventBusTracer.h"
#include "SpanContext.h"
#include <spdlog/spdlog.h>

namespace mxrc {
namespace tracing {

// Thread-local span storage
thread_local std::map<std::string, std::shared_ptr<ISpan>> EventBusTracer::publish_spans_;
thread_local std::map<std::string, std::shared_ptr<ISpan>> EventBusTracer::dispatch_spans_;

EventBusTracer::EventBusTracer(std::shared_ptr<ITracer> tracer)
    : tracer_(tracer)
    , enabled_(true) {
}

void EventBusTracer::onBeforePublish(const std::shared_ptr<mxrc::core::event::IEvent>& event) {
    if (!enabled_ || !event) {
        return;
    }

    try {
        std::string event_id = getEventId(event);

        // Create root span for event publish
        std::map<std::string, std::string> attributes;
        attributes["event.id"] = event_id;
        attributes["event.type"] = event->getTypeName();
        attributes["event.target_id"] = event->getTargetId();
        attributes["mxrc.component"] = "eventbus";
        attributes["mxrc.operation"] = "publish";

        auto span = tracer_->startSpan("EventBus.publish", attributes);

        // Store span for later retrieval in onAfterPublish
        publish_spans_[event_id] = span;

        // Note: Since IEvent doesn't have metadata storage, we store trace context
        // in thread-local storage. In a production system, you would extend IEvent
        // with a metadata map or use a separate correlation mechanism.

    } catch (const std::exception& e) {
        spdlog::error("[EventBusTracer] Error in onBeforePublish: {}", e.what());
    }
}

void EventBusTracer::onAfterPublish(const std::shared_ptr<mxrc::core::event::IEvent>& event,
                                   bool success) {
    if (!enabled_ || !event) {
        return;
    }

    try {
        std::string event_id = getEventId(event);

        auto it = publish_spans_.find(event_id);
        if (it == publish_spans_.end()) {
            return;
        }

        auto span = it->second;

        // Set span status
        if (success) {
            span->setStatus(SpanStatus::OK);
        } else {
            span->setStatus(SpanStatus::ERROR, "Event publish failed");
        }

        // Add completion event
        span->addEvent("publish.completed", {
            {"success", success ? "true" : "false"}
        });

        // End the span
        span->end();

        // Remove from tracking
        publish_spans_.erase(it);

    } catch (const std::exception& e) {
        spdlog::error("[EventBusTracer] Error in onAfterPublish: {}", e.what());
    }
}

void EventBusTracer::onBeforeDispatch(const std::shared_ptr<mxrc::core::event::IEvent>& event) {
    if (!enabled_ || !event) {
        return;
    }

    try {
        std::string event_id = getEventId(event);

        // Check if we have a parent span from publish
        std::shared_ptr<ISpan> parent_span = nullptr;
        auto publish_it = publish_spans_.find(event_id);
        if (publish_it != publish_spans_.end()) {
            parent_span = publish_it->second;
        }

        // Create span for event dispatch
        std::map<std::string, std::string> attributes;
        attributes["event.id"] = event_id;
        attributes["event.type"] = event->getTypeName();
        attributes["event.target_id"] = event->getTargetId();
        attributes["mxrc.component"] = "eventbus";
        attributes["mxrc.operation"] = "dispatch";

        std::shared_ptr<ISpan> span;
        if (parent_span) {
            // Create child span from parent context
            span = tracer_->startSpan("EventBus.dispatch",
                                     parent_span->getContext(),
                                     attributes);
        } else {
            // Create independent span (no parent context available)
            span = tracer_->startSpan("EventBus.dispatch", attributes);
        }

        // Store span for later retrieval
        dispatch_spans_[event_id] = span;

    } catch (const std::exception& e) {
        spdlog::error("[EventBusTracer] Error in onBeforeDispatch: {}", e.what());
    }
}

void EventBusTracer::onAfterDispatch(const std::shared_ptr<mxrc::core::event::IEvent>& event,
                                    size_t subscriber_count) {
    if (!enabled_ || !event) {
        return;
    }

    try {
        std::string event_id = getEventId(event);

        auto it = dispatch_spans_.find(event_id);
        if (it == dispatch_spans_.end()) {
            return;
        }

        auto span = it->second;

        // Add subscriber count attribute
        span->setAttribute("event.subscriber_count", std::to_string(subscriber_count));

        // Add completion event
        span->addEvent("dispatch.completed", {
            {"subscriber_count", std::to_string(subscriber_count)}
        });

        // Set status
        span->setStatus(SpanStatus::OK);

        // End the span
        span->end();

        // Remove from tracking
        dispatch_spans_.erase(it);

    } catch (const std::exception& e) {
        spdlog::error("[EventBusTracer] Error in onAfterDispatch: {}", e.what());
    }
}

void EventBusTracer::setEnabled(bool enabled) {
    enabled_ = enabled;
}

bool EventBusTracer::isEnabled() const {
    return enabled_;
}

std::string EventBusTracer::getEventId(const std::shared_ptr<mxrc::core::event::IEvent>& event) const {
    return event->getEventId();
}

void EventBusTracer::injectTraceContext(const std::shared_ptr<mxrc::core::event::IEvent>& event,
                                       const TraceContext& context) {
    // Note: IEvent doesn't have metadata storage.
    // In a production system, you would extend IEvent with a metadata map.
    // For now, we rely on thread-local span correlation via event ID.
}

TraceContext EventBusTracer::extractTraceContext(
    const std::shared_ptr<mxrc::core::event::IEvent>& event) const {
    // Note: IEvent doesn't have metadata storage.
    // Return invalid context for now.
    return SpanContextUtils::invalidContext();
}

} // namespace tracing
} // namespace mxrc
