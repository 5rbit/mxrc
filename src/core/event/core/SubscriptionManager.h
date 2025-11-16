// SubscriptionManager.h - 구독 관리
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_EVENT_CORE_SUBSCRIPTIONMANAGER_H
#define MXRC_CORE_EVENT_CORE_SUBSCRIPTIONMANAGER_H

#include "interfaces/IEvent.h"
#include "dto/EventType.h"
#include "util/EventFilter.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <random>
#include <sstream>

namespace mxrc::core::event {

/**
 * @brief 구독 정보 구조체
 */
struct Subscription {
    std::string id;           ///< 구독 ID
    EventFilter filter;       ///< 이벤트 필터 함수
    EventCallback callback;   ///< 콜백 함수

    Subscription(std::string id, EventFilter filter, EventCallback callback)
        : id(std::move(id)), filter(std::move(filter)), callback(std::move(callback)) {}
};

/**
 * @brief 구독 관리 클래스
 *
 * EventBus의 구독자를 등록, 조회, 삭제하는 기능을 제공합니다.
 * 스레드 안전하게 구현되어 여러 스레드에서 동시에 접근 가능합니다.
 */
class SubscriptionManager {
private:
    mutable std::mutex mutex_;                          ///< 스레드 안전성을 위한 mutex
    std::unordered_map<std::string, Subscription> subscriptions_; ///< 구독 ID → 구독 정보

    /**
     * @brief 고유한 구독 ID 생성
     *
     * @return 생성된 구독 ID
     */
    static std::string generateSubscriptionId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::atomic<uint64_t> counter{0};

        std::stringstream ss;
        ss << "sub_" << counter.fetch_add(1, std::memory_order_relaxed) << "_";

        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }

        return ss.str();
    }

public:
    SubscriptionManager() = default;
    ~SubscriptionManager() = default;

    // 복사/이동 방지
    SubscriptionManager(const SubscriptionManager&) = delete;
    SubscriptionManager& operator=(const SubscriptionManager&) = delete;

    /**
     * @brief 구독 추가
     *
     * @param filter 이벤트 필터 함수
     * @param callback 콜백 함수
     * @return 생성된 구독 ID
     */
    std::string addSubscription(EventFilter filter, EventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string id = generateSubscriptionId();
        subscriptions_.emplace(id, Subscription(id, std::move(filter), std::move(callback)));
        return id;
    }

    /**
     * @brief 구독 제거
     *
     * @param subscriptionId 제거할 구독 ID
     * @return true이면 성공, false이면 해당 ID가 없음
     */
    bool removeSubscription(const std::string& subscriptionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        return subscriptions_.erase(subscriptionId) > 0;
    }

    /**
     * @brief 모든 구독자 조회
     *
     * 이벤트를 처리할 때 사용됩니다.
     * 스레드 안전성을 위해 복사본을 반환합니다.
     *
     * @return 모든 구독자의 복사본
     */
    std::vector<Subscription> getAllSubscriptions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Subscription> result;
        result.reserve(subscriptions_.size());

        for (const auto& [id, sub] : subscriptions_) {
            result.push_back(sub);
        }

        return result;
    }

    /**
     * @brief 현재 구독자 수 반환
     *
     * @return 구독자 수
     */
    size_t getSubscriptionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return subscriptions_.size();
    }

    /**
     * @brief 모든 구독 제거
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        subscriptions_.clear();
    }
};

} // namespace mxrc::core::event

#endif // MXRC_CORE_EVENT_CORE_SUBSCRIPTIONMANAGER_H
