#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <any>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <stdexcept>
#include <tbb/concurrent_hash_map.h>

#include "managers/ExpirationManager.h"
#include "managers/AccessControlManager.h"
#include "managers/MetricsCollector.h"
#include "managers/LogManager.h"
#include "core/VersionedData.h"

class Observer;

/// @brief 로봇 데이터 타입 분류
enum class DataType {
    RobotMode,
    InterfaceData,
    Config,
    Para,
    Alarm,
    Event,
    MissionState,
    TaskState,
};

/// @brief 공유 데이터 구조체
struct SharedData {
    std::string id;
    DataType type;
    std::any value;
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::chrono::time_point<std::chrono::system_clock> expiration_time;
};

/// @brief 만료 정책 타입
enum class ExpirationPolicyType {
    TTL,
    LRU,
    None,
};

/// @brief 데이터 만료 정책
struct DataExpirationPolicy {
    ExpirationPolicyType policy_type;
    std::chrono::milliseconds duration;
};

/// @brief Observer 패턴의 Notifier 인터페이스
class Notifier {
public:
    virtual ~Notifier() = default;
    virtual void subscribe(std::shared_ptr<Observer> observer) = 0;
    virtual void unsubscribe(std::shared_ptr<Observer> observer) = 0;
    virtual void notify(const SharedData& changed_data) = 0;
};

/// @brief Observer 패턴의 Observer 인터페이스
class Observer {
public:
    virtual ~Observer() = default;
    virtual void onDataChanged(const SharedData& changed_data) = 0;
};

/**
 * @brief 스레드 안전한 중앙 데이터 저장소 (Facade 패턴)
 *
 * 로봇 시스템의 모든 모듈 간 데이터를 공유하는 중앙 저장소입니다.
 * Facade 패턴을 사용하여 ExpirationManager, AccessControlManager, MetricsCollector에 위임합니다.
 *
 * 주요 기능:
 * - 스레드 안전한 데이터 저장/조회 (concurrent_hash_map)
 * - Observer 패턴을 통한 데이터 변경 알림
 * - 데이터 만료 정책 관리 (TTL)
 * - 접근 제어 (모듈별 권한 관리)
 * - 성능 메트릭 수집 (lock-free atomic)
 */
class DataStore : public std::enable_shared_from_this<DataStore> {
public:
    /// @brief Singleton 인스턴스 생성
    static std::shared_ptr<DataStore> create();

    /// @brief 테스트용 독립 인스턴스 생성 (테스트 격리)
    static std::shared_ptr<DataStore> createForTest();

    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    /// @brief 데이터 저장 (타입 안전, 만료 정책 지원)
    template<typename T>
    void set(const std::string& id, const T& data, DataType type,
             const DataExpirationPolicy& policy = {ExpirationPolicyType::None, std::chrono::milliseconds(0)});

    /// @brief 데이터 조회 (타입 안전)
    template<typename T>
    T get(const std::string& id);

    /// @brief 데이터 폴링 (인터페이스 모듈 데이터용)
    template<typename T>
    T poll(const std::string& id);

    /// @brief 버전이 있는 데이터 조회 (Feature 022: P2 Accessor Pattern)
    /// @note RT-safe: lock-free read with atomic version check
    template<typename T>
    mxrc::core::datastore::VersionedData<T> getVersioned(const std::string& id);

    /// @brief 버전이 있는 데이터 저장 (Feature 022: P2 Accessor Pattern)
    /// @note RT-safe: atomic version increment
    template<typename T>
    void setVersioned(const std::string& id, const T& value, DataType type = DataType::RobotMode);

    /// @brief 데이터 변경 알림 구독
    void subscribe(const std::string& id, std::shared_ptr<Observer> observer);

    /// @brief 구독 해제
    void unsubscribe(const std::string& id, std::shared_ptr<Observer> observer);

    /// @brief 만료 정책 적용
    void applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy);

    /// @brief 만료 정책 제거
    void removeExpirationPolicy(const std::string& id);

    /// @brief 만료된 데이터 정리 (주기적 호출)
    void cleanExpiredData();

    /// @brief 성능 메트릭 조회
    std::map<std::string, double> getPerformanceMetrics() const;

    /// @brief 접근 로그 조회 (향후 Logger 통합 예정)
    std::vector<std::string> getAccessLogs() const;

    /// @brief 에러 로그 조회 (향후 Logger 통합 예정)
    std::vector<std::string> getErrorLogs() const;

    /// @brief 현재 저장된 데이터 개수
    size_t getCurrentDataCount() const;

    /// @brief 메모리 사용량 추정
    size_t getCurrentMemoryUsage() const;

    /// @brief 데이터 저장 (Placeholder)
    void saveState(const std::string& filepath);

    /// @brief 데이터 로드 (Placeholder)
    void loadState(const std::string& filepath);

    /// @brief 접근 제어 정책 설정
    void setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access);

    /// @brief 접근 권한 확인
    bool hasAccess(const std::string& id, const std::string& module_id) const;

    DataStore();
    ~DataStore() = default;

private:
    /// @brief 스레드 안전 데이터 저장소 (concurrent_hash_map)
    tbb::concurrent_hash_map<std::string, SharedData> data_map_;

    /// @brief 버전 정보 저장 (Feature 022: P2 Accessor Pattern)
    /// @note Key: data id, Value: atomic version counter
    tbb::concurrent_hash_map<std::string, std::atomic<uint64_t>> version_map_;

    /// @brief Observer 패턴 Notifier 관리
    std::map<std::string, std::shared_ptr<Notifier>> notifiers_;

    /// @brief Facade 패턴: Manager 객체들 (RAII)
    std::unique_ptr<mxrc::core::datastore::ExpirationManager> expiration_manager_;
    std::unique_ptr<mxrc::core::datastore::AccessControlManager> access_control_manager_;
    std::unique_ptr<mxrc::core::datastore::MetricsCollector> metrics_collector_;
    std::unique_ptr<mxrc::core::datastore::LogManager> log_manager_;

    /// @brief 내부 헬퍼: Observer 알림 발행
    void notifySubscribers(const SharedData& changed_data);

    /// @brief Notifier 보호용 뮤텍스 (data_map_은 내부 락 사용)
    mutable std::mutex mutex_;
};

// ============================================================================
// Template 메서드 구현
// ============================================================================

template<typename T>
void DataStore::set(const std::string& id, const T& data, DataType type,
                    const DataExpirationPolicy& policy) {
    try {
        SharedData new_data;
        new_data.id = id;
        new_data.type = type;
        new_data.value = data;
        new_data.timestamp = std::chrono::system_clock::now();
        new_data.expiration_time = (policy.policy_type == ExpirationPolicyType::TTL) ?
                                    new_data.timestamp + policy.duration :
                                    std::chrono::time_point<std::chrono::system_clock>();

        // concurrent_hash_map accessor로 스레드 안전 접근
        {
            typename tbb::concurrent_hash_map<std::string, SharedData>::accessor acc;

            if (data_map_.find(acc, id)) {
                // 타입 일치 검사 (DataType + std::any 타입)
                if (acc->second.type != type) {
                    log_manager_->logError("type_mismatch", "Data type mismatch for existing ID: " + id);
                    throw std::runtime_error("Data type mismatch for existing ID: " + id);
                }
                if (acc->second.value.type() != typeid(T)) {
                    std::string error_msg = "Value type mismatch for existing ID: " + id +
                                           " (expected: " + std::string(acc->second.value.type().name()) +
                                           ", got: " + std::string(typeid(T).name()) + ")";
                    log_manager_->logError("type_mismatch", error_msg);
                    throw std::runtime_error(error_msg);
                }
            } else {
                data_map_.insert(acc, id);
            }

            acc->second = new_data;
        }

        metrics_collector_->incrementSet();
        log_manager_->logAccess("set", id);

        // 만료 정책 적용
        if (policy.policy_type == ExpirationPolicyType::TTL) {
            auto expiration_time = new_data.timestamp + policy.duration;
            expiration_manager_->applyPolicy(id, expiration_time);
        } else if (policy.policy_type == ExpirationPolicyType::LRU) {
            // LRU 정책: duration을 capacity로 해석
            size_t capacity = static_cast<size_t>(policy.duration.count());
            expiration_manager_->applyLRUPolicy(id, capacity);
        }

        notifySubscribers(new_data);
    } catch (const std::exception& e) {
        log_manager_->logError("set_failed", e.what(), "id=" + id);
        throw;
    }
}

template<typename T>
T DataStore::get(const std::string& id) {
    try {
        T result;

        // concurrent_hash_map const_accessor로 읽기 전용 접근
        {
            typename tbb::concurrent_hash_map<std::string, SharedData>::const_accessor acc;

            if (!data_map_.find(acc, id)) {
                log_manager_->logError("not_found", "Data not found for ID: " + id);
                throw std::out_of_range("Data not found for ID: " + id);
            }

            const SharedData& stored_data = acc->second;

            if (stored_data.value.type() != typeid(T)) {
                log_manager_->logError("type_mismatch", "Type mismatch for ID: " + id);
                throw std::runtime_error("Type mismatch for ID: " + id);
            }

            result = std::any_cast<T>(stored_data.value);
        }

        metrics_collector_->incrementGet();
        log_manager_->logAccess("get", id);

        // LRU 접근 기록
        expiration_manager_->recordAccess(id);

        return result;
    } catch (const std::exception& e) {
        log_manager_->logError("get_failed", e.what(), "id=" + id);
        throw;
    }
}

template<typename T>
T DataStore::poll(const std::string& id) {
    try {
        T result;

        // concurrent_hash_map const_accessor로 읽기 전용 접근
        {
            typename tbb::concurrent_hash_map<std::string, SharedData>::const_accessor acc;

            if (!data_map_.find(acc, id)) {
                log_manager_->logError("not_found", "Data not found for ID: " + id);
                throw std::out_of_range("Data not found for ID: " + id);
            }

            const SharedData& stored_data = acc->second;

            if (stored_data.value.type() != typeid(T)) {
                log_manager_->logError("type_mismatch", "Type mismatch for ID: " + id);
                throw std::runtime_error("Type mismatch for ID: " + id);
            }

            result = std::any_cast<T>(stored_data.value);
        }

        metrics_collector_->incrementPoll();
        log_manager_->logAccess("poll", id);

        // LRU 접근 기록
        expiration_manager_->recordAccess(id);

        return result;
    } catch (const std::exception& e) {
        log_manager_->logError("poll_failed", e.what(), "id=" + id);
        throw;
    }
}

#endif // DATASTORE_H