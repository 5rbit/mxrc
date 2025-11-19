// DataStoreEvents.h - DataStore 관련 이벤트 정의
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_DTO_DATASTOREEVENTS_H
#define MXRC_CORE_EVENT_DTO_DATASTOREEVENTS_H

#include "EventBase.h"
#include <string>
#include <chrono>

namespace mxrc::core::event {

/**
 * @brief DataStore 값 변경 이벤트
 *
 * DataStore의 값이 변경되었을 때 발행되는 이벤트입니다.
 * 양방향 연동을 위해 oldValue와 newValue를 모두 포함합니다.
 */
class DataStoreValueChangedEvent : public EventBase {
public:
    std::string key;           ///< 변경된 데이터의 키
    std::string oldValue;      ///< 이전 값 (문자열로 변환)
    std::string newValue;      ///< 새로운 값 (문자열로 변환)
    std::string valueType;     ///< 값의 타입 정보 (예: "int", "string", "double")
    std::string source;        ///< 변경 소스 (예: "user", "system", "event")

    /**
     * @brief 생성자
     *
     * @param key 변경된 데이터 키
     * @param oldValue 이전 값
     * @param newValue 새로운 값
     * @param valueType 값의 타입
     * @param source 변경 소스
     * @param timestamp 이벤트 발생 시각
     */
    DataStoreValueChangedEvent(
        const std::string& key,
        const std::string& oldValue,
        const std::string& newValue,
        const std::string& valueType,
        const std::string& source = "unknown",
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::DATASTORE_VALUE_CHANGED, key, timestamp),
          key(key),
          oldValue(oldValue),
          newValue(newValue),
          valueType(valueType),
          source(source) {}
};

/**
 * @brief DataStore 값 삭제 이벤트
 *
 * DataStore에서 값이 삭제되었을 때 발행되는 이벤트입니다.
 */
class DataStoreValueRemovedEvent : public EventBase {
public:
    std::string key;           ///< 삭제된 데이터의 키
    std::string oldValue;      ///< 삭제된 값
    std::string valueType;     ///< 값의 타입 정보

    /**
     * @brief 생성자
     *
     * @param key 삭제된 데이터 키
     * @param oldValue 삭제된 값
     * @param valueType 값의 타입
     * @param timestamp 이벤트 발생 시각
     */
    DataStoreValueRemovedEvent(
        const std::string& key,
        const std::string& oldValue,
        const std::string& valueType,
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : EventBase(EventType::DATASTORE_VALUE_REMOVED, key, timestamp),
          key(key),
          oldValue(oldValue),
          valueType(valueType) {}
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_DTO_DATASTOREEVENTS_H
