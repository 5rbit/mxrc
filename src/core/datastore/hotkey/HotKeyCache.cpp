// HotKeyCache.cpp - High-performance cache implementation
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Copyright (C) 2025 MXRC Project

#include "HotKeyCache.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::datastore {

HotKeyCache::HotKeyCache(size_t capacity)
    : capacity_(capacity),
      cache_(std::make_unique<folly::AtomicHashMap<std::string, std::any>>(capacity * 2))
{
    spdlog::info("[HotKeyCache] Initialized with capacity: {}", capacity);
}

bool HotKeyCache::registerHotKey(const std::string& key) {
    size_t current_count = hot_key_count_.load(std::memory_order_relaxed);

    if (current_count >= capacity_) {
        spdlog::error("[HotKeyCache] Registration failed: capacity exceeded ({}/{})",
                      current_count, capacity_);
        return false;
    }

    // Pre-allocate slot with std::any containing empty VersionedValue<std::any>
    // Actual type will be set during first set() call
    try {
        auto result = cache_->insert(key, std::any{});

        if (result.second) {
            // Successfully inserted
            hot_key_count_.fetch_add(1, std::memory_order_relaxed);
            spdlog::debug("[HotKeyCache] Registered Hot Key: '{}' ({}/{})",
                         key, hot_key_count_.load(), capacity_);
            return true;
        } else {
            // Key already exists
            spdlog::warn("[HotKeyCache] Hot Key already registered: '{}'", key);
            return true;  // Not an error
        }
    } catch (const std::exception& e) {
        spdlog::error("[HotKeyCache] Registration failed for '{}': {}", key, e.what());
        return false;
    }
}

bool HotKeyCache::isHotKey(const std::string& key) const {
    return cache_->find(key) != cache_->end();
}

size_t HotKeyCache::getHotKeyCount() const {
    return hot_key_count_.load(std::memory_order_relaxed);
}

std::map<std::string, uint64_t> HotKeyCache::getMetrics() const {
    return {
        {"read_count", read_count_.load(std::memory_order_relaxed)},
        {"write_count", write_count_.load(std::memory_order_relaxed)},
        {"retry_count", retry_count_.load(std::memory_order_relaxed)},
        {"hot_key_count", hot_key_count_.load(std::memory_order_relaxed)},
        {"capacity", capacity_}
    };
}

}  // namespace mxrc::core::datastore
