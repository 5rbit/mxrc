#include "core/tracing/EventBusTracer.h"
#include "core/tracing/TracerProvider.h"
#include "core/event/interfaces/IEvent.h"
#include "core/event/dto/EventType.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace mxrc::tracing;
using namespace mxrc::core::event;

// Mock event for testing
class MockEvent : public IEvent {
public:
    explicit MockEvent(const std::string& id) : id_(id) {}

    std::string getEventId() const override { return id_; }
    EventType getType() const override { return EventType::ACTION_STARTED; }
    std::chrono::system_clock::time_point getTimestamp() const override {
        return std::chrono::system_clock::now();
    }
    std::string getTargetId() const override { return "target-123"; }
    std::string getTypeName() const override { return "ACTION_STARTED"; }

private:
    std::string id_;
};

class EventBusTracerTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::err);
        provider_ = getGlobalTracerProvider();
        tracer_ = provider_->getTracer("test-eventbus");
        eventbus_tracer_ = std::make_shared<EventBusTracer>(tracer_);
    }

    void TearDown() override {
        spdlog::set_level(spdlog::level::info);
    }

    std::shared_ptr<ITracerProvider> provider_;
    std::shared_ptr<ITracer> tracer_;
    std::shared_ptr<EventBusTracer> eventbus_tracer_;
};

TEST_F(EventBusTracerTest, Creation) {
    ASSERT_NE(eventbus_tracer_, nullptr);
    EXPECT_TRUE(eventbus_tracer_->isEnabled());
}

TEST_F(EventBusTracerTest, EnableDisable) {
    EXPECT_TRUE(eventbus_tracer_->isEnabled());

    eventbus_tracer_->setEnabled(false);
    EXPECT_FALSE(eventbus_tracer_->isEnabled());

    eventbus_tracer_->setEnabled(true);
    EXPECT_TRUE(eventbus_tracer_->isEnabled());
}

TEST_F(EventBusTracerTest, PublishLifecycle) {
    auto event = std::make_shared<MockEvent>("event-123");

    // Before publish
    EXPECT_NO_THROW(eventbus_tracer_->onBeforePublish(event));

    // After publish (success)
    EXPECT_NO_THROW(eventbus_tracer_->onAfterPublish(event, true));
}

TEST_F(EventBusTracerTest, PublishFailure) {
    auto event = std::make_shared<MockEvent>("event-456");

    eventbus_tracer_->onBeforePublish(event);
    eventbus_tracer_->onAfterPublish(event, false);  // Publish failed
}

TEST_F(EventBusTracerTest, DispatchLifecycle) {
    auto event = std::make_shared<MockEvent>("event-789");

    // Before dispatch
    EXPECT_NO_THROW(eventbus_tracer_->onBeforeDispatch(event));

    // After dispatch
    EXPECT_NO_THROW(eventbus_tracer_->onAfterDispatch(event, 5));
}

TEST_F(EventBusTracerTest, CompleteEventFlow) {
    auto event = std::make_shared<MockEvent>("event-complete");

    // Full lifecycle: publish -> dispatch
    eventbus_tracer_->onBeforePublish(event);
    eventbus_tracer_->onAfterPublish(event, true);

    eventbus_tracer_->onBeforeDispatch(event);
    eventbus_tracer_->onAfterDispatch(event, 3);
}

TEST_F(EventBusTracerTest, NullEvent) {
    std::shared_ptr<IEvent> null_event = nullptr;

    // Should not crash with null event
    EXPECT_NO_THROW(eventbus_tracer_->onBeforePublish(null_event));
    EXPECT_NO_THROW(eventbus_tracer_->onAfterPublish(null_event, true));
    EXPECT_NO_THROW(eventbus_tracer_->onBeforeDispatch(null_event));
    EXPECT_NO_THROW(eventbus_tracer_->onAfterDispatch(null_event, 0));
}

TEST_F(EventBusTracerTest, DisabledTracer) {
    eventbus_tracer_->setEnabled(false);

    auto event = std::make_shared<MockEvent>("event-disabled");

    // Should not create spans when disabled
    EXPECT_NO_THROW(eventbus_tracer_->onBeforePublish(event));
    EXPECT_NO_THROW(eventbus_tracer_->onAfterPublish(event, true));
}

TEST_F(EventBusTracerTest, MultipleEvents) {
    for (int i = 0; i < 10; ++i) {
        auto event = std::make_shared<MockEvent>("event-" + std::to_string(i));

        eventbus_tracer_->onBeforePublish(event);
        eventbus_tracer_->onAfterPublish(event, true);

        eventbus_tracer_->onBeforeDispatch(event);
        eventbus_tracer_->onAfterDispatch(event, i);
    }
}
