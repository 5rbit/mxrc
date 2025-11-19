#include "DataStore.h"
#include "MapNotifier.h"
#include <mutex>
#include <stdexcept>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>

DataStore::DataStore()
    : expiration_manager_(std::make_unique<mxrc::core::datastore::ExpirationManager>()),
      access_control_manager_(std::make_unique<mxrc::core::datastore::AccessControlManager>()),
      metrics_collector_(std::make_unique<mxrc::core::datastore::MetricsCollector>()),
      log_manager_(std::make_unique<mxrc::core::datastore::LogManager>()) {}

std::shared_ptr<DataStore> DataStore::create() {
    static std::shared_ptr<DataStore> instance = std::make_shared<DataStore>();
    return instance;
}

std::shared_ptr<DataStore> DataStore::createForTest() {
    return std::make_shared<DataStore>();
}

void DataStore::subscribe(const std::string& id, std::shared_ptr<Observer> observer) {
    if (!observer) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (notifiers_.find(id) == notifiers_.end()) {
        notifiers_[id] = std::make_shared<MapNotifier>();
    }
    notifiers_[id]->subscribe(observer);
}

void DataStore::unsubscribe(const std::string& id, std::shared_ptr<Observer> observer) {
    if (!observer) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (notifiers_.find(id) != notifiers_.end()) {
        notifiers_[id]->unsubscribe(observer);
    }
}

void DataStore::notifySubscribers(const SharedData& changed_data) {
    // shared_ptr 복사로 안전한 생명주기 관리
    std::shared_ptr<Notifier> notifier;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = notifiers_.find(changed_data.id);
        if (it != notifiers_.end() && it->second) {
            notifier = it->second;
        }
    }

    if (notifier) {
        try {
            notifier->notify(changed_data);
        } catch (const std::exception& e) {
            // Notifier 콜백 예외 격리 및 에러 로그 기록
            log_manager_->logError("callback_exception", e.what(), "id=" + changed_data.id);
        }
    }
}

void DataStore::applyExpirationPolicy(const std::string& id, const DataExpirationPolicy& policy) {
    auto expiration_time = std::chrono::system_clock::now() + policy.duration;
    expiration_manager_->applyPolicy(id, expiration_time);
}

void DataStore::removeExpirationPolicy(const std::string& id) {
    expiration_manager_->removePolicy(id);
}

void DataStore::cleanExpiredData() {
    // TTL 만료 키 수집 및 제거
    auto expired_keys_ttl = expiration_manager_->getExpiredKeys();

    for (const auto& key : expired_keys_ttl) {
        data_map_.erase(key);
        expiration_manager_->removePolicy(key);
        expiration_manager_->removeLRUPolicy(key);  // LRU 추적도 제거
    }

    // LRU 용량 초과 키 수집 및 제거
    auto expired_keys_lru = expiration_manager_->getExpiredKeysLRU();

    for (const auto& key : expired_keys_lru) {
        data_map_.erase(key);
        expiration_manager_->removePolicy(key);  // TTL 정책도 제거
        // LRU는 이미 getExpiredKeysLRU()에서 제거됨
    }
}

std::map<std::string, double> DataStore::getPerformanceMetrics() const {
    return metrics_collector_->getMetrics();
}

std::vector<std::string> DataStore::getAccessLogs() const {
    return log_manager_->getAccessLogs();
}

std::vector<std::string> DataStore::getErrorLogs() const {
    return log_manager_->getErrorLogs();
}

size_t DataStore::getCurrentDataCount() const {
    return data_map_.size();
}

size_t DataStore::getCurrentMemoryUsage() const {
    // 기본 추정: 항목 수 * SharedData 크기
    return data_map_.size() * sizeof(SharedData);
}

void DataStore::saveState(const std::string& filepath) {
    using json = nlohmann::json;

    // JSON 객체 생성
    json state;
    state["version"] = 1;
    state["data"] = json::array();

    // data_map_의 모든 항목을 순회하여 직렬화
    {
        typename tbb::concurrent_hash_map<std::string, SharedData>::const_accessor acc;

        for (auto it = data_map_.begin(); it != data_map_.end(); ++it) {
            if (data_map_.find(acc, it->first)) {
                const SharedData& data = acc->second;

                json item;
                item["id"] = data.id;
                item["type"] = static_cast<int>(data.type);

                // std::any 타입별 직렬화
                try {
                    if (data.value.type() == typeid(int)) {
                        item["value_type"] = "int";
                        item["value"] = std::any_cast<int>(data.value);
                    } else if (data.value.type() == typeid(double)) {
                        item["value_type"] = "double";
                        item["value"] = std::any_cast<double>(data.value);
                    } else if (data.value.type() == typeid(float)) {
                        item["value_type"] = "float";
                        item["value"] = std::any_cast<float>(data.value);
                    } else if (data.value.type() == typeid(std::string)) {
                        item["value_type"] = "string";
                        item["value"] = std::any_cast<std::string>(data.value);
                    } else if (data.value.type() == typeid(bool)) {
                        item["value_type"] = "bool";
                        item["value"] = std::any_cast<bool>(data.value);
                    } else if (data.value.type() == typeid(long)) {
                        item["value_type"] = "long";
                        item["value"] = std::any_cast<long>(data.value);
                    } else {
                        // 지원하지 않는 타입은 건너뜀
                        continue;
                    }

                    state["data"].push_back(item);
                } catch (const std::bad_any_cast&) {
                    // any_cast 실패 시 건너뜀
                    continue;
                }
            }
        }
    }

    // 임시 파일에 먼저 쓰기 (원자적 쓰기)
    std::string temp_filepath = filepath + ".tmp";

    try {
        std::ofstream ofs(temp_filepath);
        if (!ofs.is_open()) {
            throw std::runtime_error("Failed to open temporary file for saving state: " + temp_filepath);
        }

        ofs << state.dump(2);  // 들여쓰기 2칸
        ofs.flush();

        if (!ofs.good()) {
            throw std::runtime_error("Failed to write state to temporary file: " + temp_filepath);
        }

        ofs.close();

        // 임시 파일을 실제 파일로 이름 변경 (원자적 연산)
        std::filesystem::rename(temp_filepath, filepath);

    } catch (const std::exception& e) {
        // 임시 파일 정리
        std::filesystem::remove(temp_filepath);
        throw std::runtime_error("Failed to save DataStore state: " + std::string(e.what()));
    }
}

void DataStore::loadState(const std::string& filepath) {
    using json = nlohmann::json;

    // 파일 읽기
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file for loading state: " + filepath);
    }

    json state;
    try {
        ifs >> state;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON file: " + std::string(e.what()));
    }

    // 버전 확인
    if (!state.contains("version") || !state["version"].is_number_integer()) {
        throw std::runtime_error("Invalid or missing version in state file");
    }

    int version = state["version"];
    if (version != 1) {
        throw std::runtime_error("Unsupported state file version: " + std::to_string(version));
    }

    // 데이터 배열 확인
    if (!state.contains("data") || !state["data"].is_array()) {
        throw std::runtime_error("Invalid or missing data array in state file");
    }

    // 기존 데이터 모두 삭제
    data_map_.clear();

    // 데이터 역직렬화
    for (const auto& item : state["data"]) {
        if (!item.contains("id") || !item.contains("type") ||
            !item.contains("value_type") || !item.contains("value")) {
            // 필수 필드 누락 시 건너뜀
            continue;
        }

        std::string id = item["id"];
        DataType type = static_cast<DataType>(item["type"].get<int>());
        std::string value_type = item["value_type"];

        SharedData new_data;
        new_data.id = id;
        new_data.type = type;
        new_data.timestamp = std::chrono::system_clock::now();
        new_data.expiration_time = std::chrono::time_point<std::chrono::system_clock>();

        try {
            // 타입별 역직렬화
            if (value_type == "int") {
                new_data.value = item["value"].get<int>();
            } else if (value_type == "double") {
                new_data.value = item["value"].get<double>();
            } else if (value_type == "float") {
                new_data.value = item["value"].get<float>();
            } else if (value_type == "string") {
                new_data.value = item["value"].get<std::string>();
            } else if (value_type == "bool") {
                new_data.value = item["value"].get<bool>();
            } else if (value_type == "long") {
                new_data.value = item["value"].get<long>();
            } else {
                // 지원하지 않는 타입은 건너뜀
                continue;
            }

            // concurrent_hash_map에 삽입
            typename tbb::concurrent_hash_map<std::string, SharedData>::accessor acc;
            data_map_.insert(acc, id);
            acc->second = new_data;

        } catch (const std::exception& e) {
            // 역직렬화 실패 시 건너뜀
            continue;
        }
    }
}

void DataStore::setAccessPolicy(const std::string& id, const std::string& module_id, bool can_access) {
    access_control_manager_->setPolicy(id, module_id, can_access);
}

bool DataStore::hasAccess(const std::string& id, const std::string& module_id) const {
    return access_control_manager_->hasAccess(id, module_id);
}
