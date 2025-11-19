#ifndef MAP_NOTIFIER_H
#define MAP_NOTIFIER_H

#include "DataStore.h"
#include <vector>
#include <memory>
#include <mutex>

/// @brief Observer 패턴의 Notifier 구현 (weak_ptr 기반 안전한 Observer 관리)
class MapNotifier : public Notifier {
public:
    /// @brief Destructor - 정리 보장
    ~MapNotifier() override {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.clear();
    }

    /// @brief Observer 구독 (weak_ptr로 내부 관리하여 dangling pointer 방지)
    void subscribe(std::shared_ptr<Observer> observer) override {
        if (!observer) {
            return;  // NULL observer 무시
        }
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_.push_back(observer);  // weak_ptr로 자동 변환
    }

    /// @brief Observer 구독 해제
    void unsubscribe(std::shared_ptr<Observer> observer) override {
        if (!observer) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);

        // weak_ptr 벡터에서 제거하기 위해 반복 처리
        for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
            if (auto obs = it->lock()) {
                if (obs == observer) {
                    it = subscribers_.erase(it);
                } else {
                    ++it;
                }
            } else {
                // 파괴된 observer는 자동 제거
                it = subscribers_.erase(it);
            }
        }
    }

    /// @brief 변경 알림 발행 (파괴된 Observer 자동 감지 및 정리)
    void notify(const SharedData& changed_data) override {
        std::lock_guard<std::mutex> lock(mutex_);

        // 파괴된 observer 자동 정리 및 호출
        for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
            if (auto observer = it->lock()) {
                // ✓ Observer가 유효함 - 안전하게 호출
                observer->onDataChanged(changed_data);
                ++it;
            } else {
                // ✓ Observer가 파괴됨 - 자동 제거
                it = subscribers_.erase(it);
            }
        }
    }

private:
    // ✓ weak_ptr 사용으로 dangling pointer 방지
    std::vector<std::weak_ptr<Observer>> subscribers_;
    std::mutex mutex_;
};

#endif // MAP_NOTIFIER_H
