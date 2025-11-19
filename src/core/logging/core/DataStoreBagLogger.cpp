#include "core/DataStoreBagLogger.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <stdexcept>

namespace mxrc::core::logging {

DataStoreBagLogger::DataStoreBagLogger(
    std::shared_ptr<event::IEventBus> eventBus,
    std::shared_ptr<IBagWriter> bagWriter)
    : eventBus_(eventBus),
      bagWriter_(bagWriter),
      isRunning_(false),
      eventsReceived_(0),
      eventsDropped_(0) {

    if (!eventBus_) {
        throw std::invalid_argument("EventBus cannot be nullptr");
    }
    if (!bagWriter_) {
        throw std::invalid_argument("BagWriter cannot be nullptr");
    }

    spdlog::info("DataStoreBagLogger created");
}

DataStoreBagLogger::~DataStoreBagLogger() {
    stop();
    spdlog::info("DataStoreBagLogger destroyed");
}

bool DataStoreBagLogger::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isRunning_) {
        spdlog::warn("DataStoreBagLogger already running");
        return false;
    }

    // 1. Bag Writer 시작
    bagWriter_->start();

    // 2. EventBus 구독 등록 (DATASTORE_VALUE_CHANGED 이벤트만)
    subscriptionId_ = eventBus_->subscribe(
        [](const std::shared_ptr<event::IEvent>& event) {
            return event->getType() == event::EventType::DATASTORE_VALUE_CHANGED;
        },
        [this](std::shared_ptr<event::IEvent> event) {
            onDataStoreEvent(event);
        }
    );

    isRunning_ = true;
    spdlog::info("DataStoreBagLogger started, subscription ID: {}", subscriptionId_);
    return true;
}

void DataStoreBagLogger::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRunning_) {
        return;
    }

    // 1. EventBus 구독 해제
    if (!subscriptionId_.empty()) {
        eventBus_->unsubscribe(subscriptionId_);
        subscriptionId_.clear();
    }

    // 2. 남은 메시지 flush
    bagWriter_->flush(5000);

    // 3. Bag Writer 정지
    bagWriter_->stop();

    isRunning_ = false;
    spdlog::info("DataStoreBagLogger stopped, events received: {}, dropped: {}",
                 eventsReceived_.load(), eventsDropped_.load());
}

bool DataStoreBagLogger::isRunning() const {
    return isRunning_.load();
}

BagWriterStats DataStoreBagLogger::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto stats = bagWriter_->getStats();

    // eventsDropped는 BagWriter의 messagesDropped와 별도로 추적
    // (EventBus에서 수신했지만 BagWriter로 전달 실패한 경우)
    stats.messagesDropped += eventsDropped_.load();

    return stats;
}

std::string DataStoreBagLogger::getCurrentFilePath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bagWriter_->getCurrentFilePath();
}

void DataStoreBagLogger::setRotationPolicy(const RotationPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    bagWriter_->setRotationPolicy(policy);
}

void DataStoreBagLogger::setRetentionPolicy(const RetentionPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    bagWriter_->setRetentionPolicy(policy);
}

bool DataStoreBagLogger::flush(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isRunning_) {
        return false;
    }

    return bagWriter_->flush(timeoutMs);
}

void DataStoreBagLogger::onDataStoreEvent(std::shared_ptr<event::IEvent> event) {
    eventsReceived_++;

    // DataStoreValueChangedEvent로 캐스팅
    auto dsEvent = std::dynamic_pointer_cast<event::DataStoreValueChangedEvent>(event);
    if (!dsEvent) {
        spdlog::warn("Failed to cast event to DataStoreValueChangedEvent");
        eventsDropped_++;
        return;
    }

    // BagMessage로 변환
    BagMessage bagMsg = convertToBagMessage(dsEvent);

    // 유효성 검증
    if (!bagMsg.isValid()) {
        spdlog::warn("Invalid BagMessage generated from event: topic={}", bagMsg.topic);
        eventsDropped_++;
        return;
    }

    // 비동기 쓰기 (실패 시 드롭 카운트 증가)
    if (!bagWriter_->appendAsync(bagMsg)) {
        spdlog::warn("Failed to append BagMessage to writer: topic={}", bagMsg.topic);
        eventsDropped_++;
    }
}

BagMessage DataStoreBagLogger::convertToBagMessage(
    const std::shared_ptr<event::DataStoreValueChangedEvent>& event) {

    BagMessage msg;

    // 1. 타임스탬프: 이벤트 시각을 나노초로 변환
    auto epoch = event->getTimestamp().time_since_epoch();
    msg.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch).count();

    // 2. Topic: DataStore 키 사용
    msg.topic = event->key;

    // 3. DataType: valueType 문자열을 DataType enum으로 변환
    // (기본적으로는 문자열 비교, 향후 확장 가능)
    if (event->valueType == "MissionState") {
        msg.data_type = DataType::MissionState;
    } else if (event->valueType == "TaskState") {
        msg.data_type = DataType::TaskState;
    } else if (event->valueType == "Alarm") {
        msg.data_type = DataType::Alarm;
    } else if (event->valueType == "Event") {
        msg.data_type = DataType::Event;
    } else if (event->valueType == "InterfaceData") {
        msg.data_type = DataType::InterfaceData;
    } else {
        // 알 수 없는 타입은 Event로 처리
        msg.data_type = DataType::Event;
        spdlog::debug("Unknown valueType '{}', treating as Event", event->valueType);
    }

    // 4. Serialized Value: newValue를 그대로 사용
    // (DataStore에서 이미 JSON 문자열로 제공된다고 가정)
    msg.serialized_value = event->newValue;

    return msg;
}

} // namespace mxrc::core::logging
