#include "AccessControlManager.h"

namespace mxrc::core::datastore {

void AccessControlManager::setPolicy(const std::string& key, const std::string& module_id, bool can_access) {
    // unique_lock for write (독점 접근)
    std::unique_lock<std::shared_mutex> lock(mutex_);
    access_policies_[key][module_id] = can_access;
}

bool AccessControlManager::hasAccess(const std::string& key, const std::string& module_id) const {
    // shared_lock for read (읽기 병렬성)
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it_key = access_policies_.find(key);
    if (it_key != access_policies_.end()) {
        auto it_module = it_key->second.find(module_id);
        if (it_module != it_key->second.end()) {
            return it_module->second;
        }
    }

    // 기본값: 접근 거부
    return false;
}

void AccessControlManager::removePolicy(const std::string& key) {
    // unique_lock for write (독점 접근)
    std::unique_lock<std::shared_mutex> lock(mutex_);
    access_policies_.erase(key);
}

void AccessControlManager::removePolicy(const std::string& key, const std::string& module_id) {
    // unique_lock for write (독점 접근)
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it_key = access_policies_.find(key);
    if (it_key != access_policies_.end()) {
        it_key->second.erase(module_id);

        // 해당 키에 모듈이 하나도 없으면 키 자체 제거
        if (it_key->second.empty()) {
            access_policies_.erase(it_key);
        }
    }
}

std::map<std::string, std::map<std::string, bool>> AccessControlManager::getAllPolicies() const {
    // shared_lock for read (읽기 병렬성)
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return access_policies_;  // 복사 반환
}

bool AccessControlManager::hasPolicy(const std::string& key) const {
    // shared_lock for read (읽기 병렬성)
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return access_policies_.find(key) != access_policies_.end();
}

size_t AccessControlManager::getPolicyCount() const {
    // shared_lock for read (읽기 병렬성)
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return access_policies_.size();
}

} // namespace mxrc::core::datastore
