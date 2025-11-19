// EventFilter.h - 이벤트 필터 유틸리티
// Copyright (C) 2025 MXRC Project
// 이벤트 필터링을 위한 타입 정의 및 헬퍼 함수

#ifndef MXRC_CORE_EVENT_UTIL_EVENTFILTER_H
#define MXRC_CORE_EVENT_UTIL_EVENTFILTER_H

#include "interfaces/IEvent.h"
#include "dto/EventType.h"
#include <memory>
#include <functional>

namespace mxrc::core::event {

/**
 * @brief 이벤트 필터 함수 타입 (재정의)
 *
 * EventFilter와 EventCallback은 IEventBus.h에 정의되어 있으나,
 * util 모듈에서도 사용하기 위해 여기서 재정의합니다.
 */
using EventFilter = std::function<bool(const std::shared_ptr<IEvent>&)>;
using EventCallback = std::function<void(std::shared_ptr<IEvent>)>;

/**
 * @brief 이벤트 필터 생성 헬퍼 함수들
 */
namespace Filters {

/**
 * @brief 특정 이벤트 타입만 허용하는 필터 생성
 *
 * @param type 허용할 이벤트 타입
 * @return 해당 타입의 이벤트만 true를 반환하는 필터
 */
inline EventFilter byType(EventType type) {
    return [type](const std::shared_ptr<IEvent>& event) {
        return event && event->getType() == type;
    };
}

/**
 * @brief 특정 타겟 ID를 가진 이벤트만 허용하는 필터 생성
 *
 * @param targetId 허용할 타겟 ID
 * @return 해당 타겟 ID의 이벤트만 true를 반환하는 필터
 */
inline EventFilter byTargetId(const std::string& targetId) {
    return [targetId](const std::shared_ptr<IEvent>& event) {
        return event && event->getTargetId() == targetId;
    };
}

/**
 * @brief 타입과 타겟 ID를 모두 만족하는 이벤트만 허용하는 필터 생성
 *
 * @param type 허용할 이벤트 타입
 * @param targetId 허용할 타겟 ID
 * @return 두 조건을 모두 만족하는 이벤트만 true를 반환하는 필터
 */
inline EventFilter byTypeAndTarget(EventType type, const std::string& targetId) {
    return [type, targetId](const std::shared_ptr<IEvent>& event) {
        return event && event->getType() == type && event->getTargetId() == targetId;
    };
}

/**
 * @brief 모든 이벤트를 허용하는 필터 생성
 *
 * @return 항상 true를 반환하는 필터
 */
inline EventFilter all() {
    return [](const std::shared_ptr<IEvent>& event) {
        return event != nullptr;
    };
}

/**
 * @brief 두 필터를 AND 조건으로 결합
 *
 * @param filter1 첫 번째 필터
 * @param filter2 두 번째 필터
 * @return 두 필터를 모두 만족하는 경우에만 true를 반환하는 필터
 */
inline EventFilter andFilter(EventFilter filter1, EventFilter filter2) {
    return [filter1, filter2](const std::shared_ptr<IEvent>& event) {
        return filter1(event) && filter2(event);
    };
}

/**
 * @brief 두 필터를 OR 조건으로 결합
 *
 * @param filter1 첫 번째 필터
 * @param filter2 두 번째 필터
 * @return 두 필터 중 하나라도 만족하면 true를 반환하는 필터
 */
inline EventFilter orFilter(EventFilter filter1, EventFilter filter2) {
    return [filter1, filter2](const std::shared_ptr<IEvent>& event) {
        return filter1(event) || filter2(event);
    };
}

} // namespace Filters

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_UTIL_EVENTFILTER_H
