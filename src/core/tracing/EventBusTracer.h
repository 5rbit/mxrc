#pragma once

#include "TracerProvider.h"
#include "core/event/core/EventBus.h"
#include "core/event/interfaces/IEvent.h"
#include <memory>
#include <string>

namespace mxrc {
namespace tracing {

/**
 * @brief Event Bus Tracer
 *
 * Implements IEventObserver to automatically create spans for event publishing
 * and dispatching. Integrates distributed tracing with EventBus.
 *
 * Usage:
 *   auto tracer = getGlobalTracerProvider()->getTracer("mxrc-events");
 *   auto eventBusTracer = std::make_shared<EventBusTracer>(tracer);
 *   eventBus->registerObserver(eventBusTracer);
 */
class EventBusTracer : public mxrc::core::event::IEventObserver {
public:
    /**
     * @brief Constructor
     *
     * @param tracer Tracer instance to use for creating spans
     */
    explicit EventBusTracer(std::shared_ptr<ITracer> tracer);

    /**
     * @brief Destructor
     */
    ~EventBusTracer() override = default;

    /**
     * @brief Called before event is published
     *
     * Creates a root span for the event publish operation.
     * Injects trace context into event metadata for propagation.
     *
     * @param event Event being published
     */
    void onBeforePublish(const std::shared_ptr<mxrc::core::event::IEvent>& event) override;

    /**
     * @brief Called after event is published
     *
     * Ends the publish span and records success/failure status.
     *
     * @param event Event that was published
     * @param success Whether publish succeeded
     */
    void onAfterPublish(const std::shared_ptr<mxrc::core::event::IEvent>& event, bool success) override;

    /**
     * @brief Called before event is dispatched to subscribers
     *
     * Creates a child span for event dispatch operation.
     * Extracts trace context from event metadata for propagation.
     *
     * @param event Event being dispatched
     */
    void onBeforeDispatch(const std::shared_ptr<mxrc::core::event::IEvent>& event) override;

    /**
     * @brief Called after event is dispatched to subscribers
     *
     * Ends the dispatch span and records subscriber count.
     *
     * @param event Event that was dispatched
     * @param subscriber_count Number of subscribers that received the event
     */
    void onAfterDispatch(const std::shared_ptr<mxrc::core::event::IEvent>& event,
                        size_t subscriber_count) override;

    /**
     * @brief Enable/disable event tracing
     *
     * @param enabled True to enable, false to disable
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if event tracing is enabled
     *
     * @return true if enabled, false otherwise
     */
    bool isEnabled() const;

private:
    std::shared_ptr<ITracer> tracer_;
    bool enabled_;

    // Thread-local storage for publish/dispatch spans
    // Maps event ID to span
    static thread_local std::map<std::string, std::shared_ptr<ISpan>> publish_spans_;
    static thread_local std::map<std::string, std::shared_ptr<ISpan>> dispatch_spans_;

    /**
     * @brief Get event ID for span tracking
     *
     * @param event Event instance
     * @return std::string Event ID
     */
    std::string getEventId(const std::shared_ptr<mxrc::core::event::IEvent>& event) const;

    /**
     * @brief Inject trace context into event metadata
     *
     * @param event Event to inject into
     * @param context Trace context to inject
     */
    void injectTraceContext(const std::shared_ptr<mxrc::core::event::IEvent>& event,
                           const TraceContext& context);

    /**
     * @brief Extract trace context from event metadata
     *
     * @param event Event to extract from
     * @return TraceContext Extracted context (may be invalid)
     */
    TraceContext extractTraceContext(const std::shared_ptr<mxrc::core::event::IEvent>& event) const;
};

} // namespace tracing
} // namespace mxrc
