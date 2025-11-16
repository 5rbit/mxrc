// DataStoreEventAdapter.cpp - DataStore와 EventBus 연동 어댑터 구현
// Copyright (C) 2025 MXRC Project

#include "DataStoreEventAdapter.h"
#include "core/action/util/Logger.h"
#include "dto/SequenceEvents.h"
#include <sstream>
#include <iomanip>

namespace mxrc::core::event {

using namespace mxrc::core::action;

DataStoreEventAdapter::DataStoreEventAdapter(
    std::shared_ptr<DataStore> dataStore,
    std::shared_ptr<IEventBus> eventBus)
    : dataStore_(dataStore)
    , eventBus_(eventBus) {

    auto logger = Logger::get();
    logger->info("[DataStoreEventAdapter] Initialized");
}

DataStoreEventAdapter::~DataStoreEventAdapter() {
    auto logger = Logger::get();

    // 모든 구독 해제
    for (const auto& subId : subscriptionIds_) {
        eventBus_->unsubscribe(subId);
    }

    logger->info("[DataStoreEventAdapter] Destroyed, unsubscribed {} event listeners",
                 subscriptionIds_.size());
}

void DataStoreEventAdapter::onDataChanged(const SharedData& changed_data) {
    auto logger = Logger::get();

    logger->debug("[DataStoreEventAdapter] onDataChanged called for key: {}", changed_data.id);

    // 순환 업데이트 방지
    if (isCircularUpdate(changed_data.id)) {
        logger->debug("[DataStoreEventAdapter] Skipping circular update for key: {}",
                     changed_data.id);
        return;
    }

    // DataStore 변경을 EventBus로 발행
    std::string valueStr = valueToString(changed_data);
    std::string typeStr = dataTypeToString(changed_data.type);

    auto event = std::make_shared<DataStoreValueChangedEvent>(
        changed_data.id,
        "",  // oldValue는 DataStore에서 제공하지 않음
        valueStr,
        typeStr,
        "datastore"
    );

    bool published = eventBus_->publish(event);
    if (published) {
        logger->debug("[DataStoreEventAdapter] Published DATASTORE_VALUE_CHANGED for key: {}",
                     changed_data.id);
    } else {
        logger->warn("[DataStoreEventAdapter] Failed to publish event for key: {}",
                    changed_data.id);
    }
}

void DataStoreEventAdapter::subscribeToActionResults(const std::string& keyPrefix) {
    auto logger = Logger::get();

    // ACTION_COMPLETED 이벤트 구독
    auto subId = eventBus_->subscribe(
        Filters::byType(EventType::ACTION_COMPLETED),
        [this, keyPrefix, logger](std::shared_ptr<IEvent> event) {
            auto actionEvent = std::static_pointer_cast<ActionCompletedEvent>(event);

            std::string key = keyPrefix + actionEvent->actionId;

            // 순환 업데이트 방지
            markUpdating(key);

            try {
                // Action 결과를 DataStore에 저장
                std::ostringstream oss;
                oss << "completed:" << actionEvent->durationMs << "ms";

                dataStore_->set(key, oss.str(), DataType::TaskState);

                logger->debug("[DataStoreEventAdapter] Stored action result: {} = {}",
                             key, oss.str());
            } catch (const std::exception& e) {
                logger->error("[DataStoreEventAdapter] Failed to store action result: {}",
                             e.what());
            }

            unmarkUpdating(key);
        }
    );

    subscriptionIds_.push_back(subId);
    logger->info("[DataStoreEventAdapter] Subscribed to ACTION_COMPLETED events (prefix: {})",
                 keyPrefix);
}

void DataStoreEventAdapter::subscribeToSequenceResults(const std::string& keyPrefix) {
    auto logger = Logger::get();

    // SEQUENCE_COMPLETED 이벤트 구독
    auto subId = eventBus_->subscribe(
        Filters::byType(EventType::SEQUENCE_COMPLETED),
        [this, keyPrefix, logger](std::shared_ptr<IEvent> event) {
            auto seqEvent = std::static_pointer_cast<SequenceCompletedEvent>(event);

            std::string key = keyPrefix + seqEvent->sequenceId;

            // 순환 업데이트 방지
            markUpdating(key);

            try {
                // Sequence 결과를 DataStore에 저장
                std::ostringstream oss;
                oss << "completed:" << seqEvent->completedSteps << "/" << seqEvent->totalSteps
                    << " (" << seqEvent->durationMs << "ms)";

                dataStore_->set(key, oss.str(), DataType::TaskState);

                logger->debug("[DataStoreEventAdapter] Stored sequence result: {} = {}",
                             key, oss.str());
            } catch (const std::exception& e) {
                logger->error("[DataStoreEventAdapter] Failed to store sequence result: {}",
                             e.what());
            }

            unmarkUpdating(key);
        }
    );

    subscriptionIds_.push_back(subId);
    logger->info("[DataStoreEventAdapter] Subscribed to SEQUENCE_COMPLETED events (prefix: {})",
                 keyPrefix);
}

void DataStoreEventAdapter::startWatching(const std::string& keyPattern) {
    auto logger = Logger::get();

    // DataStore에 Observer 등록
    dataStore_->subscribe(keyPattern, this);

    logger->info("[DataStoreEventAdapter] Started watching DataStore key pattern: {}",
                 keyPattern);
}

void DataStoreEventAdapter::stopWatching(const std::string& keyPattern) {
    auto logger = Logger::get();

    // DataStore에서 Observer 해제
    dataStore_->unsubscribe(keyPattern, this);

    logger->info("[DataStoreEventAdapter] Stopped watching DataStore key pattern: {}",
                 keyPattern);
}

std::string DataStoreEventAdapter::valueToString(const SharedData& data) {
    try {
        // std::any를 문자열로 변환 시도
        if (data.value.type() == typeid(std::string)) {
            return std::any_cast<std::string>(data.value);
        } else if (data.value.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(data.value));
        } else if (data.value.type() == typeid(double)) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << std::any_cast<double>(data.value);
            return oss.str();
        } else if (data.value.type() == typeid(bool)) {
            return std::any_cast<bool>(data.value) ? "true" : "false";
        } else {
            return "<unknown type>";
        }
    } catch (const std::bad_any_cast& e) {
        return "<conversion error>";
    }
}

std::string DataStoreEventAdapter::dataTypeToString(DataType type) {
    switch (type) {
        case DataType::RobotMode: return "RobotMode";
        case DataType::InterfaceData: return "InterfaceData";
        case DataType::Config: return "Config";
        case DataType::Para: return "Para";
        case DataType::Alarm: return "Alarm";
        case DataType::Event: return "Event";
        case DataType::MissionState: return "MissionState";
        case DataType::TaskState: return "TaskState";
        default: return "Unknown";
    }
}

bool DataStoreEventAdapter::isCircularUpdate(const std::string& key) {
    std::lock_guard<std::mutex> lock(updateMutex_);
    return updatingKeys_.find(key) != updatingKeys_.end();
}

void DataStoreEventAdapter::markUpdating(const std::string& key) {
    std::lock_guard<std::mutex> lock(updateMutex_);
    updatingKeys_.insert(key);
}

void DataStoreEventAdapter::unmarkUpdating(const std::string& key) {
    std::lock_guard<std::mutex> lock(updateMutex_);
    updatingKeys_.erase(key);
}

} // namespace mxrc::core::event
