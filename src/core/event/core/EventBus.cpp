// EventBus.cpp - 중앙 이벤트 버스 구현
// Copyright (C) 2025 MXRC Project

#include "core/EventBus.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <thread>

namespace mxrc::core::event {

EventBus::EventBus(size_t queueCapacity)
    : eventQueue_(queueCapacity) {
    spdlog::info("EventBus created with queue capacity: {}", queueCapacity);
}

EventBus::~EventBus() {
    if (running_.load(std::memory_order_acquire)) {
        spdlog::warn("EventBus destroyed while still running, stopping...");
        stop();
    }
}

void EventBus::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_release)) {
        spdlog::warn("EventBus already running");
        return;
    }

    spdlog::info("Starting EventBus...");

    // 이벤트 처리 스레드 시작
    dispatchThread_ = std::thread([this]() {
        dispatchLoop();
    });

    spdlog::info("EventBus started successfully");
}

void EventBus::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_release)) {
        spdlog::warn("EventBus already stopped");
        return;
    }

    spdlog::info("Stopping EventBus...");

    // dispatch 스레드 종료 대기
    if (dispatchThread_.joinable()) {
        dispatchThread_.join();
    }

    spdlog::info("EventBus stopped. Stats - Published: {}, Processed: {}, Dropped: {}, FailedCallbacks: {}",
                stats_.publishedEvents.load(std::memory_order_relaxed),
                stats_.processedEvents.load(std::memory_order_relaxed),
                stats_.droppedEvents.load(std::memory_order_relaxed),
                stats_.failedCallbacks.load(std::memory_order_relaxed));
}

bool EventBus::isRunning() const {
    return running_.load(std::memory_order_acquire);
}

bool EventBus::publish(std::shared_ptr<IEvent> event) {
    if (!event) {
        spdlog::warn("Attempted to publish null event");
        return false;
    }

    // Production readiness: Notify observers before publish
    notifyBeforePublish(event);

    // Mutex로 보호하여 여러 생산자가 안전하게 publish 가능
    std::lock_guard<std::mutex> lock(publishMutex_);

    // 논블로킹 push 시도
    bool success = eventQueue_.tryPush(event);

    if (success) {
        stats_.publishedEvents.fetch_add(1, std::memory_order_relaxed);
    } else {
        stats_.droppedEvents.fetch_add(1, std::memory_order_relaxed);
        spdlog::warn("Event queue full, dropped event: {} ({})",
                    event->getTypeName(), event->getEventId());
    }

    // Production readiness: Notify observers after publish
    notifyAfterPublish(event, success);

    return success;
}

SubscriptionId EventBus::subscribe(EventFilter filter, EventCallback callback) {
    if (!callback) {
        spdlog::error("Attempted to subscribe with null callback");
        return "";
    }

    // filter가 null이면 모든 이벤트를 받는 필터로 대체
    if (!filter) {
        filter = [](const std::shared_ptr<IEvent>&) { return true; };
    }

    std::string subId = subscriptionManager_.addSubscription(filter, callback);
    stats_.activeSubscriptions.fetch_add(1, std::memory_order_relaxed);

    spdlog::debug("New subscription added: {}", subId);
    return subId;
}

bool EventBus::unsubscribe(const SubscriptionId& subscriptionId) {
    bool success = subscriptionManager_.removeSubscription(subscriptionId);

    if (success) {
        stats_.activeSubscriptions.fetch_sub(1, std::memory_order_relaxed);
        spdlog::debug("Subscription removed: {}", subscriptionId);
    } else {
        spdlog::warn("Failed to remove subscription (not found): {}", subscriptionId);
    }

    return success;
}

void EventBus::dispatchLoop() {
    spdlog::info("EventBus dispatch loop started");

    while (running_.load(std::memory_order_acquire)) {
        std::shared_ptr<IEvent> event;

        // 큐에서 이벤트 꺼내기
        if (eventQueue_.tryPop(event)) {
            dispatchToSubscribers(event);
        } else {
            // 큐가 비어 있으면 짧게 대기
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // 종료 시 남은 이벤트 모두 처리
    spdlog::info("Processing remaining events before shutdown...");
    std::shared_ptr<IEvent> event;
    while (eventQueue_.tryPop(event)) {
        dispatchToSubscribers(event);
    }

    spdlog::info("EventBus dispatch loop stopped");
}

void EventBus::dispatchToSubscribers(std::shared_ptr<IEvent> event) {
    if (!event) {
        return;
    }

    // Production readiness: Notify observers before dispatch
    notifyBeforeDispatch(event);

    auto subscriptions = subscriptionManager_.getAllSubscriptions();
    size_t subscriber_count = 0;

    for (const auto& sub : subscriptions) {
        try {
            // 필터 조건 확인
            if (sub.filter(event)) {
                sub.callback(event);
                stats_.processedEvents.fetch_add(1, std::memory_order_relaxed);
                subscriber_count++;
            }
        } catch (const std::exception& e) {
            stats_.failedCallbacks.fetch_add(1, std::memory_order_relaxed);
            spdlog::error("Subscriber exception for event {} ({}): {}",
                         event->getTypeName(), event->getEventId(), e.what());
        } catch (...) {
            stats_.failedCallbacks.fetch_add(1, std::memory_order_relaxed);
            spdlog::error("Unknown subscriber exception for event {} ({})",
                         event->getTypeName(), event->getEventId());
        }
    }

    // Production readiness: Notify observers after dispatch
    notifyAfterDispatch(event, subscriber_count);
}

// Production readiness: Observer pattern implementation
void EventBus::registerObserver(std::shared_ptr<IEventObserver> observer) {
    if (!observer) {
        spdlog::warn("Attempted to register null observer");
        return;
    }

    std::lock_guard<std::mutex> lock(observerMutex_);
    observers_.push_back(observer);
    spdlog::info("Event observer registered (total: {})", observers_.size());
}

void EventBus::unregisterObserver(std::shared_ptr<IEventObserver> observer) {
    if (!observer) {
        return;
    }

    std::lock_guard<std::mutex> lock(observerMutex_);
    auto it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end()) {
        observers_.erase(it);
        spdlog::info("Event observer unregistered (total: {})", observers_.size());
    }
}

void EventBus::notifyBeforePublish(const std::shared_ptr<IEvent>& event) {
    std::lock_guard<std::mutex> lock(observerMutex_);
    for (const auto& observer : observers_) {
        try {
            observer->onBeforePublish(event);
        } catch (const std::exception& e) {
            spdlog::error("Observer exception in onBeforePublish: {}", e.what());
        } catch (...) {
            spdlog::error("Unknown observer exception in onBeforePublish");
        }
    }
}

void EventBus::notifyAfterPublish(const std::shared_ptr<IEvent>& event, bool success) {
    std::lock_guard<std::mutex> lock(observerMutex_);
    for (const auto& observer : observers_) {
        try {
            observer->onAfterPublish(event, success);
        } catch (const std::exception& e) {
            spdlog::error("Observer exception in onAfterPublish: {}", e.what());
        } catch (...) {
            spdlog::error("Unknown observer exception in onAfterPublish");
        }
    }
}

void EventBus::notifyBeforeDispatch(const std::shared_ptr<IEvent>& event) {
    std::lock_guard<std::mutex> lock(observerMutex_);
    for (const auto& observer : observers_) {
        try {
            observer->onBeforeDispatch(event);
        } catch (const std::exception& e) {
            spdlog::error("Observer exception in onBeforeDispatch: {}", e.what());
        } catch (...) {
            spdlog::error("Unknown observer exception in onBeforeDispatch");
        }
    }
}

void EventBus::notifyAfterDispatch(const std::shared_ptr<IEvent>& event, size_t subscriber_count) {
    std::lock_guard<std::mutex> lock(observerMutex_);
    for (const auto& observer : observers_) {
        try {
            observer->onAfterDispatch(event, subscriber_count);
        } catch (const std::exception& e) {
            spdlog::error("Observer exception in onAfterDispatch: {}", e.what());
        } catch (...) {
            spdlog::error("Unknown observer exception in onAfterDispatch");
        }
    }
}

} // namespace mxrc::core::event
