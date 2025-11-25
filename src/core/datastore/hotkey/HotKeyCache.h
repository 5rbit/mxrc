// HotKeyCache.h - High-performance cache for frequently accessed data
// Feature 019: Architecture Improvements - US2 Hot Key Optimization
// Copyright (C) 2025 MXRC Project

#ifndef MXRC_CORE_DATASTORE_HOTKEY_HOTKEYCACHE_H
#define MXRC_CORE_DATASTORE_HOTKEY_HOTKEYCACHE_H

#include <string>
#include <any>
#include <optional>
#include <atomic>
#include <memory>
#include <chrono>
#include <folly/AtomicHashMap.h>

namespace mxrc::core::datastore {

/**
 * @brief Versioned data wrapper for lock-free read consistency
 *
 * Uses version counter to detect concurrent writes during reads.
 * Based on seqlock pattern for RT-safe reading.
 */
template<typename T>
struct VersionedValue {
    std::atomic<uint64_t> version{0};  ///< Odd during write, even when stable
    T value;                           ///< Actual data

    VersionedValue() = default;
    explicit VersionedValue(const T& val) : value(val) {}
};

/**
 * @brief High-performance cache for Hot Keys using Folly AtomicHashMap
 *
 * Performance targets (from research.md):
 * - Read: <60ns average (lock-free with version check)
 * - Write: <110ns average (atomic version increment)
 * - Capacity: 32 Hot Keys maximum (FR-006)
 * - Value size: ≤512 bytes (64-axis motor data)
 *
 * Design:
 * - Folly AtomicHashMap for lock-free operations
 * - Version counter for read consistency (seqlock pattern)
 * - Cache line alignment to prevent false sharing
 * - NUMA-aware allocation (future enhancement)
 *
 * Thread-safety:
 * - Lock-free reads (may retry on concurrent write)
 * - Atomic writes with version increment
 * - Safe for RT and Non-RT concurrent access
 */
class HotKeyCache {
public:
    /**
     * @brief Constructor
     *
     * @param capacity Maximum number of Hot Keys (default: 32)
     */
    explicit HotKeyCache(size_t capacity = 32);

    ~HotKeyCache() = default;

    // Non-copyable, non-movable (contains std::atomic)
    HotKeyCache(const HotKeyCache&) = delete;
    HotKeyCache& operator=(const HotKeyCache&) = delete;
    HotKeyCache(HotKeyCache&&) = delete;
    HotKeyCache& operator=(HotKeyCache&&) = delete;

    /**
     * @brief Register a key as Hot Key
     *
     * Pre-allocates slot in AtomicHashMap to avoid allocation during RT operations.
     * Must be called during initialization, not in RT context.
     *
     * @param key DataStore key name
     * @return true if registration succeeded, false if capacity exceeded
     */
    bool registerHotKey(const std::string& key);

    /**
     * @brief Write data to Hot Key cache (RT-safe)
     *
     * Performance: <110ns average
     * - Atomic version increment (odd)
     * - Memory barrier
     * - Value write
     * - Atomic version increment (even)
     *
     * @tparam T Value type
     * @param key DataStore key name
     * @param value Data to cache
     * @return true if write succeeded, false if key not registered
     */
    template<typename T>
    bool set(const std::string& key, const T& value);

    /**
     * @brief Read data from Hot Key cache (RT-safe, lock-free)
     *
     * Performance: <60ns average
     * Uses seqlock pattern:
     * 1. Read version (must be even)
     * 2. Read value
     * 3. Read version again (must match)
     * 4. Retry if version changed during read
     *
     * @tparam T Value type
     * @param key DataStore key name
     * @return Value if found and consistent, std::nullopt otherwise
     */
    template<typename T>
    std::optional<T> get(const std::string& key);

    /**
     * @brief Check if key is registered as Hot Key
     *
     * @param key DataStore key name
     * @return true if key is registered
     */
    bool isHotKey(const std::string& key) const;

    /**
     * @brief Get current number of registered Hot Keys
     *
     * @return Number of Hot Keys
     */
    size_t getHotKeyCount() const;

    /**
     * @brief Get maximum capacity
     *
     * @return Maximum Hot Key capacity
     */
    size_t getCapacity() const { return capacity_; }

    /**
     * @brief Get performance metrics
     *
     * @return Map of metric name to value (read_count, write_count, retry_count)
     */
    std::map<std::string, uint64_t> getMetrics() const;

private:
    /// Maximum Hot Key capacity (FR-006: 32 keys)
    const size_t capacity_;

    /// Number of registered Hot Keys
    std::atomic<size_t> hot_key_count_{0};

    /// Folly AtomicHashMap for lock-free operations
    /// Key: DataStore key name
    /// Value: std::any containing VersionedValue<T>
    std::unique_ptr<folly::AtomicHashMap<std::string, std::any>> cache_;

    /// Performance metrics (atomic for thread-safety)
    std::atomic<uint64_t> read_count_{0};
    std::atomic<uint64_t> write_count_{0};
    std::atomic<uint64_t> retry_count_{0};  ///< Number of read retries due to version mismatch

    /// Maximum retry attempts for read consistency
    static constexpr size_t MAX_READ_RETRIES = 10;
};

// ============================================================================
// Template Method Implementations
// ============================================================================

template<typename T>
bool HotKeyCache::set(const std::string& key, const T& value) {
    write_count_.fetch_add(1, std::memory_order_relaxed);

    auto iter = cache_->find(key);
    if (iter == cache_->end()) {
        return false;  // Key not registered as Hot Key
    }

    try {
        // Get VersionedValue<T> from std::any
        auto& versioned_any = iter->second;
        auto* versioned = std::any_cast<VersionedValue<T>>(&versioned_any);

        if (!versioned) {
            // Type mismatch - key was registered with different type
            return false;
        }

        // Seqlock write pattern:
        // 1. Increment version to odd (write in progress)
        uint64_t old_version = versioned->version.fetch_add(1, std::memory_order_acquire);

        // 2. Write value
        versioned->value = value;

        // 3. Memory barrier + increment to even (write complete)
        std::atomic_thread_fence(std::memory_order_release);
        versioned->version.fetch_add(1, std::memory_order_release);

        return true;
    } catch (const std::bad_any_cast&) {
        return false;
    }
}

template<typename T>
std::optional<T> HotKeyCache::get(const std::string& key) {
    read_count_.fetch_add(1, std::memory_order_relaxed);

    auto iter = cache_->find(key);
    if (iter == cache_->end()) {
        return std::nullopt;  // Key not found
    }

    try {
        // Get VersionedValue<T> from std::any
        const auto& versioned_any = iter->second;
        const auto* versioned = std::any_cast<VersionedValue<T>>(&versioned_any);

        if (!versioned) {
            return std::nullopt;  // Type mismatch
        }

        // Seqlock read pattern with retry
        for (size_t retry = 0; retry < MAX_READ_RETRIES; ++retry) {
            // 1. Read version (must be even for stable data)
            uint64_t version_before = versioned->version.load(std::memory_order_acquire);

            if (version_before % 2 != 0) {
                // Writer is active, retry
                retry_count_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            // 2. Read value
            T value_copy = versioned->value;

            // 3. Memory barrier
            std::atomic_thread_fence(std::memory_order_acquire);

            // 4. Check version again
            uint64_t version_after = versioned->version.load(std::memory_order_acquire);

            if (version_before == version_after) {
                // Consistent read
                return value_copy;
            }

            // Version changed during read, retry
            retry_count_.fetch_add(1, std::memory_order_relaxed);
        }

        // Max retries exceeded
        return std::nullopt;

    } catch (const std::bad_any_cast&) {
        return std::nullopt;
    }
}

}  // namespace mxrc::core::datastore

#endif  // MXRC_CORE_DATASTORE_HOTKEY_HOTKEYCACHE_H
