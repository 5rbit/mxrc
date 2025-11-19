#include "ExpirationManager.h"
#include <algorithm>

namespace mxrc::core::datastore {

void ExpirationManager::applyPolicy(const std::string& key, const TimePoint& expiration_time) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 기존 정책이 있으면 먼저 제거 (내부 헬퍼 사용 불가 - 이미 락 획득)
    auto it = key_to_expiration_.find(key);
    if (it != key_to_expiration_.end()) {
        // 기존 만료 시간의 set에서 키 제거
        TimePoint old_expiration = it->second;
        auto map_it = expiration_map_.find(old_expiration);
        if (map_it != expiration_map_.end()) {
            map_it->second.erase(key);
            // set이 비어있으면 map에서도 제거
            if (map_it->second.empty()) {
                expiration_map_.erase(map_it);
            }
        }
    }

    // 새로운 정책 적용
    expiration_map_[expiration_time].insert(key);
    key_to_expiration_[key] = expiration_time;
}

void ExpirationManager::removePolicy(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 키가 존재하는지 확인
    auto it = key_to_expiration_.find(key);
    if (it == key_to_expiration_.end()) {
        // 존재하지 않는 키 - 무시 (예외 발생 안 함)
        return;
    }

    // expiration_map_에서 제거
    TimePoint expiration_time = it->second;
    auto map_it = expiration_map_.find(expiration_time);
    if (map_it != expiration_map_.end()) {
        map_it->second.erase(key);
        // set이 비어있으면 map에서도 제거
        if (map_it->second.empty()) {
            expiration_map_.erase(map_it);
        }
    }

    // key_to_expiration_에서 제거
    key_to_expiration_.erase(it);
}

std::vector<std::string> ExpirationManager::getExpiredKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> expired_keys;
    auto now = std::chrono::system_clock::now();

    // O(log N) 시간 복잡도로 만료된 키 검색
    // upper_bound: 현재 시간 이후의 첫 번째 요소를 찾음
    // 즉, begin()부터 upper_bound(now)까지가 만료된 항목들
    auto it = expiration_map_.begin();
    auto end_it = expiration_map_.upper_bound(now);

    // O(K) 시간 복잡도로 만료된 키 수집 (K = 만료된 키 개수)
    for (; it != end_it; ++it) {
        // 해당 시간에 만료되는 모든 키 추가
        for (const auto& key : it->second) {
            expired_keys.push_back(key);
        }
    }

    return expired_keys;
}

bool ExpirationManager::hasPolicy(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return key_to_expiration_.find(key) != key_to_expiration_.end();
}

ExpirationManager::TimePoint ExpirationManager::getExpirationTime(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = key_to_expiration_.find(key);
    if (it == key_to_expiration_.end()) {
        throw std::out_of_range("Key not found in ExpirationManager: " + key);
    }

    return it->second;
}

size_t ExpirationManager::getPolicyCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return key_to_expiration_.size();
}

// ============================================================================
// LRU (Least Recently Used) 정책 메서드 구현
// ============================================================================

void ExpirationManager::applyLRUPolicy(const std::string& key, size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 이미 LRU 추적 중이면 무시
    if (lru_map_.find(key) != lru_map_.end()) {
        return;
    }

    // 용량 설정 (0이면 기본값 사용)
    if (capacity > 0) {
        max_lru_capacity_ = capacity;
    }

    // MRU 위치(front)에 키 추가
    lru_list_.push_front(key);
    lru_map_[key] = lru_list_.begin();
}

void ExpirationManager::recordAccess(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // LRU 추적 중이 아니면 무시
    auto it = lru_map_.find(key);
    if (it == lru_map_.end()) {
        return;
    }

    // 리스트에서 해당 키를 MRU 위치(front)로 이동
    // splice: O(1) 시간 복잡도로 노드 이동
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    // iterator는 splice 후에도 유효하므로 업데이트 불필요
}

std::vector<std::string> ExpirationManager::getExpiredKeysLRU() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> expired_keys;

    // 용량 초과 확인
    if (lru_list_.size() <= max_lru_capacity_) {
        return expired_keys; // 용량 이내면 빈 벡터 반환
    }

    // LRU부터 제거 (back에서 pop)
    size_t to_remove = lru_list_.size() - max_lru_capacity_;
    for (size_t i = 0; i < to_remove; ++i) {
        const std::string& lru_key = lru_list_.back();
        expired_keys.push_back(lru_key);

        // LRU 추적에서 제거
        lru_map_.erase(lru_key);
        lru_list_.pop_back();
    }

    return expired_keys;
}

void ExpirationManager::removeLRUPolicy(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // LRU 추적 중인지 확인
    auto it = lru_map_.find(key);
    if (it == lru_map_.end()) {
        return; // 존재하지 않으면 무시
    }

    // 리스트에서 제거
    lru_list_.erase(it->second);

    // 맵에서 제거
    lru_map_.erase(it);
}

bool ExpirationManager::hasLRUPolicy(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lru_map_.find(key) != lru_map_.end();
}

size_t ExpirationManager::getLRUCapacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_lru_capacity_;
}

size_t ExpirationManager::getLRUSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lru_list_.size();
}

} // namespace mxrc::core::datastore
