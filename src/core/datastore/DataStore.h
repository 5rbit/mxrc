#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <any>
#include <chrono>
#include <functional>  // For std::function
#include <mutex>
#include <atomic>      // Phase 2: lock-free metrics
#include <shared_mutex> // Phase 5: 읽기 병렬성
#include <stdexcept>
#include <tbb/concurrent_hash_map.h>

// Forward declarations
class Observer;

// Enum for data types
enum class DataType {
    RobotMode,
    InterfaceData, // Generic for Drive, I/O, Communication
    Config,
    Para,
    Alarm,
    Event,
    MissionState,
    TaskState,
    // Add more as needed
};

// Struct to hold shared data
struct SharedData {
    std::string id;
    DataType type;
    std::any value; // Use std::any for flexible data types
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::chrono::time_point<std::chrono::system_clock> expiration_time; // Optional
};

// Interface for data expiration policies
enum class ExpirationPolicyType {
    TTL, // Time To Live
    LRU, // Least Recently Used (not fully implemented in this interface)
    None,
};

struct DataExpirationPolicy {
    ExpirationPolicyType policy_type;
    std::chrono::milliseconds duration; // For TTL
};

// Notifier interface for Observer pattern
class Notifier {
public:
    virtual ~Notifier() = default;
    /// @brief Observer 구독 (shared_ptr 기반으로 안전한 생명주기 관리)
    virtual void subscribe(std::shared_ptr<Observer> observer) = 0;
    /// @brief Observer 구독 해제
    virtual void unsubscribe(std::shared_ptr<Observer> observer) = 0;
    /// @brief 변경 알림 발행
    virtual void notify(const SharedData& changed_data) = 0;
};

// Observer interface
class Observer {
public:
    virtual ~Observer() = default;
    virtual void onDataChanged(const SharedData& changed_data) = 0;
};

// DataStore class (shared_ptr 기반 생성 방식)
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    /// @brief DataStore 생성 (Singleton 특성 유지)
    /// @return shared_ptr로 관리되는 DataStore 인스턴스
    static std::shared_ptr<DataStore> create();

    /// @brief 테스트용 독립 인스턴스 생성
    /// @note 테스트 간 격리를 위해 매번 새로운 인스턴스 반환
    /// @return 독립적인 DataStore 인스턴스
    static std::shared_ptr<DataStore> createForTest();

    // Delete copy constructor and assignment operator
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    // FR-006: Set/Get interfaces for data manipulation
    template<typename T>
    void set(const std::string& id, const T& data, DataType type,
             const DataExpirationPolicy& policy = {ExpirationPolicyType::None, std::chrono::milliseconds(0)});

    template<typename T>
    T get(const std::string& id);

    // FR-002: Polling interface for specific data types (e.g., Interface Module data)
    template<typename T>
    T poll(const std::string& id);

    // FR-003: Notifier (Observer pattern) for Alarm and Event data
    /// @brief DataStore 값 변경 알림을 구독 (weak_ptr로 안전하게 관리됨)
    /// @param id 감시할 데이터 키
    /// @param observer 구독자 (shared_ptr로 전달되어 weak_ptr로 내부 관리)
    void subscribe(const std::string& id, std::shared_ptr<Observer> observer);

    /// @brief DataStore 값 변경 알림 구독 해제
    /// @param id 데이터 키
    /// @param observer 제거할 구독자
    void unsubscribe(const std::string& id, std::shared_ptr<Observer> observer);

    // FR-007: Data expiration policy management
    void applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy);
    void removeExpirationPolicy(const std::string& id);
    void cleanExpiredData(); // To be called periodically

    // FR-008: Observability - Metrics and Logs
    // These would typically be integrated with a separate logging/metrics module
    // For simplicity, we'll just define interfaces here.
    std::map<std::string, double> getPerformanceMetrics() const;
    std::vector<std::string> getAccessLogs() const;
    std::vector<std::string> getErrorLogs() const;

    // FR-009: Error handling - exceptions
    // get() and poll() methods will throw if data not found or type mismatch
    // set() will throw if data type is inconsistent with existing data (if strict type checking is desired)

    // FR-011: Scalability monitoring
    size_t getCurrentDataCount() const;
    size_t getCurrentMemoryUsage() const; // FR-013: For SharedData memory usage

    // FR-012: Data recovery
    void saveState(const std::string& filepath);
    void loadState(const std::string& filepath);

    // FR-014: Security - Access Control
    void setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access);
    bool hasAccess(const std::string& id, const std::string& module_id) const;

    DataStore(); // Constructor
    ~DataStore() = default;

private:
    // FR-004, FR-005: concurrent_hash_map for high-performance thread-safe data access
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;

    // Phase 4: Notifier 저장 (shared_ptr로 안전한 생명주기 관리)
    std::map<std::string, std::shared_ptr<Notifier>> notifiers_;
    std::map<std::string, DataExpirationPolicy> expiration_policies_;
    std::map<std::string, std::map<std::string, bool>> access_policies_; // id -> module_id -> can_access

    // Internal helper for notification
    void notifySubscribers(const SharedData& changed_data);

    // Phase 2: PerformanceMetrics 구조체 정의 (lock-free atomic 카운터)
    struct PerformanceMetrics {
        std::atomic<size_t> get_calls{0};        // get() 호출 횟수
        std::atomic<size_t> set_calls{0};        // set() 호출 횟수
        std::atomic<size_t> poll_calls{0};       // poll() 호출 횟수
        std::atomic<size_t> remove_calls{0};     // remove() 호출 횟수 (향후)
        std::atomic<size_t> has_calls{0};        // has() 호출 횟수 (향후)
    };

    // Phase 2: atomic 메트릭 멤버 변수
    PerformanceMetrics metrics_;

    // For observability (기존 - 향후 제거 예정)
    std::map<std::string, double> performance_metrics_;
    std::vector<std::string> access_logs_;
    std::vector<std::string> error_logs_;

    // Mutex for non-critical members
    // data_map_ uses concurrent_hash_map's internal fine-grained locking
    mutable std::mutex mutex_;  // notifiers, expiration_policies 보호

    // Phase 5: shared_mutex for read-heavy metadata access
    mutable std::shared_mutex access_policies_mutex_;
};

// Template method implementations (typically in a .tpp or .h for header-only)
template<typename T>
void DataStore::set(const std::string& id, const T& data, DataType type,
                    const DataExpirationPolicy& policy) {
    // FR-014: Access control check
    // For simplicity, assuming current_module_id is available in context or passed.
    // if (!hasAccess(id, "current_module_id")) { throw std::runtime_error("Access denied for ID: " + id); }

    SharedData new_data;
    new_data.id = id;
    new_data.type = type;
    new_data.value = data;
    new_data.timestamp = std::chrono::system_clock::now();
    new_data.expiration_time = (policy.policy_type == ExpirationPolicyType::TTL) ?
                                new_data.timestamp + policy.duration :
                                std::chrono::time_point<std::chrono::system_clock>();

    // FR-004, FR-005: concurrent_hash_map accessor로 스레드 안전 접근
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::accessor acc;

        // 기존 데이터가 있으면 타입 일치 확인
        if (data_map_.find(acc, id)) {
            if (acc->second.type != type) {
                throw std::runtime_error("Data type mismatch for existing ID: " + id);
            }
        } else {
            // 새로운 키라면 insert
            data_map_.insert(acc, id);
        }

        // 데이터 업데이트
        acc->second = new_data;
        // accessor는 스코프를 벗어나면 자동 해제 (RAII)
    }

    // FR-008: Update performance metrics (Phase 3: lock-free atomic)
    metrics_.set_calls.fetch_add(1, std::memory_order_relaxed);

    // 다른 멤버들은 mutex로 보호 (비-크리티컬 경로)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // FR-007: Applying expiration policy
        expiration_policies_[id] = policy;
        // 기존 코드 (하위 호환성 유지 - 향후 제거)
        performance_metrics_["set_calls"]++;
    }

    // FR-003: Notifying subscribers (락 없이 실행)
    notifySubscribers(new_data);
}

template<typename T>
T DataStore::get(const std::string& id) {
    // FR-014: Access control check
    // if (!hasAccess(id, "current_module_id")) { throw std::runtime_error("Access denied for ID: " + id); }

    T result;

    // FR-004, FR-005: concurrent_hash_map const_accessor로 읽기 전용 접근
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::const_accessor acc;

        // FR-009: Error handling - throw if not found
        if (!data_map_.find(acc, id)) {
            throw std::out_of_range("Data not found for ID: " + id);
        }

        const SharedData& stored_data = acc->second;

        // FR-009: Error handling - type checking and casting
        if (stored_data.value.type() != typeid(T)) {
            throw std::runtime_error("Type mismatch for ID: " + id);
        }

        result = std::any_cast<T>(stored_data.value);
        // accessor는 스코프를 벗어나면 자동 해제 (RAII)
    }

    // FR-008: Update performance metrics (Phase 3: lock-free atomic)
    metrics_.get_calls.fetch_add(1, std::memory_order_relaxed);

    // 기존 코드 (하위 호환성 유지 - 향후 제거)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        performance_metrics_["get_calls"]++;
    }

    return result;
}

template<typename T>
T DataStore::poll(const std::string& id) {
    // FR-014: Access control check
    // if (!hasAccess(id, "current_module_id")) { throw std::runtime_error("Access denied for ID: " + id); }

    T result;

    // FR-004, FR-005: concurrent_hash_map const_accessor로 읽기 전용 접근
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::const_accessor acc;

        // FR-009: Error handling - throw if not found
        if (!data_map_.find(acc, id)) {
            throw std::out_of_range("Data not found for ID: " + id);
        }

        const SharedData& stored_data = acc->second;

        // FR-009: Error handling - type checking and casting
        if (stored_data.value.type() != typeid(T)) {
            throw std::runtime_error("Type mismatch for ID: " + id);
        }

        result = std::any_cast<T>(stored_data.value);
        // accessor는 스코프를 벗어나면 자동 해제 (RAII)
    }

    // FR-008: Update performance metrics (Phase 3: lock-free atomic)
    metrics_.poll_calls.fetch_add(1, std::memory_order_relaxed);

    // 기존 코드 (하위 호환성 유지 - 향후 제거)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        performance_metrics_["poll_calls"]++;
    }

    return result;
}

#endif // DATASTORE_H