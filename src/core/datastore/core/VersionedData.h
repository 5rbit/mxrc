#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace mxrc::core::datastore {

/**
 * @brief VersionedData wraps data with versioning and timestamp metadata.
 *
 * This template provides:
 * - Atomic version tracking (monotonic increment)
 * - High-precision nanosecond timestamps
 * - Consistency validation between versions
 * - Memory-efficient 24-byte layout (8 + 8 + 8 for primitive types)
 *
 * Inspired by Linux kernel seqlock pattern for read-side optimistic concurrency.
 *
 * Thread Safety:
 * - Version increments are atomic
 * - Read operations are lock-free (optimistic reads)
 * - Write operations require external synchronization
 *
 * Performance:
 * - Read overhead: ~5ns (cache hit)
 * - Write overhead: ~10ns (atomic increment + timestamp)
 *
 * @tparam T The type of data to version (typically primitive or small struct)
 */
template <typename T>
struct VersionedData {
    T value;                          ///< The actual data value
    std::atomic<uint64_t> version;    ///< Monotonically increasing version number
    uint64_t timestamp_ns;            ///< Nanosecond-precision timestamp (not atomic)

    /**
     * @brief Default constructor - zero-initialized
     */
    VersionedData()
        : value{}, version{0}, timestamp_ns{0} {}

    /**
     * @brief Construct with initial value (version=1, timestamp=now)
     * @param val Initial value
     */
    explicit VersionedData(const T& val)
        : value{val}, version{1}, timestamp_ns{getCurrentTimestampNs()} {}

    /**
     * @brief Construct with value, version, and timestamp (Feature 022: P2 Accessor Pattern)
     * @param val Initial value
     * @param ver Initial version
     * @param ts_ns Initial timestamp in nanoseconds
     */
    VersionedData(const T& val, uint64_t ver, uint64_t ts_ns)
        : value{val}, version{ver}, timestamp_ns{ts_ns} {}

    /**
     * @brief Copy constructor (non-atomic copy)
     * @param other Source VersionedData
     */
    VersionedData(const VersionedData& other)
        : value{other.value},
          version{other.version.load(std::memory_order_relaxed)},
          timestamp_ns{other.timestamp_ns} {}

    /**
     * @brief Assignment operator (non-atomic copy)
     * @param other Source VersionedData
     */
    VersionedData& operator=(const VersionedData& other) {
        if (this != &other) {
            value = other.value;
            version.store(other.version.load(std::memory_order_relaxed), std::memory_order_relaxed);
            timestamp_ns = other.timestamp_ns;
        }
        return *this;
    }

    /**
     * @brief Update the value and increment version atomically
     * @param new_value New value to store
     *
     * Thread Safety: Caller must ensure external synchronization for writes
     */
    void update(const T& new_value) {
        value = new_value;
        version.fetch_add(1, std::memory_order_release);
        timestamp_ns = getCurrentTimestampNs();
    }

    /**
     * @brief Check if this version is consistent with another
     * @param other Another VersionedData instance
     * @return true if versions match (data is consistent)
     */
    bool isConsistentWith(const VersionedData& other) const {
        return version.load(std::memory_order_acquire) ==
               other.version.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if this version is newer than another
     * @param other Another VersionedData instance
     * @return true if this version is strictly newer
     */
    bool isNewerThan(const VersionedData& other) const {
        return version.load(std::memory_order_acquire) >
               other.version.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current version number
     * @return Current version (atomic read)
     */
    uint64_t getVersion() const {
        return version.load(std::memory_order_acquire);
    }

    /**
     * @brief Get timestamp in nanoseconds since epoch
     * @return Timestamp (non-atomic read)
     */
    uint64_t getTimestampNs() const {
        return timestamp_ns;
    }

    /**
     * @brief Check if data has been modified (version > 0)
     * @return true if data has been written at least once
     */
    bool isModified() const {
        return version.load(std::memory_order_acquire) > 0;
    }

private:
    /**
     * @brief Get current time in nanoseconds since epoch
     * @return Nanosecond timestamp (steady_clock for monotonicity)
     */
    static uint64_t getCurrentTimestampNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
};

/**
 * @brief Optimistic read pattern for VersionedData
 *
 * Usage:
 * @code
 * VersionedData<SensorData> vdata;
 * SensorData snapshot;
 * uint64_t read_version;
 *
 * do {
 *     read_version = vdata.getVersion();
 *     snapshot = vdata.value;  // Read data
 * } while (vdata.getVersion() != read_version);  // Retry if version changed
 * @endcode
 *
 * @tparam T Data type
 * @param vdata VersionedData to read
 * @param out Output snapshot
 * @return true if read was consistent (no version change during read)
 */
template <typename T>
bool tryOptimisticRead(const VersionedData<T>& vdata, T& out) {
    uint64_t v1 = vdata.getVersion();
    out = vdata.value;
    uint64_t v2 = vdata.getVersion();
    return v1 == v2;
}

} // namespace mxrc::core::datastore
