#include "gtest/gtest.h"
#include "core/datastore/DataStore.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Define a simple test struct
struct TestData {
    int value;
    std::string name;
    bool operator==(const TestData& other) const {
        return value == other.value && name == other.name;
    }
};

// Mock Observer for testing
class MockObserver : public Observer {
public:
    MockObserver() : call_count_(0) {}
    void onDataChanged(const SharedData& changed_data) override {
        call_count_++;
        last_changed_data_ = changed_data;
    }
    int call_count() const { return call_count_; }
    SharedData last_changed_data() const { return last_changed_data_; }

private:
    int call_count_;
    SharedData last_changed_data_;
};

class DataStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset DataStore instance for each test to ensure a clean state
        DataStore::getInstance().resetPerformanceMetrics();
    }
};

// 테스트: DataStore 인스턴스가 항상 동일한 싱글톤 인스턴스를 반환하는지 확인
TEST_F(DataStoreTest, GetInstanceReturnsSameInstance) {
    DataStore& instance1 = DataStore::getInstance();
    DataStore& instance2 = DataStore::getInstance();
    ASSERT_EQ(&instance1, &instance2);
}

// 테스트: 기본 데이터 유형(int)을 설정하고 올바르게 가져오는지 확인
TEST_F(DataStoreTest, SetAndGetBasicType) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "test_int";
    int value = 123;
    ds.set(id, value, DataType::Para);
    ASSERT_EQ(ds.get<int>(id), value);
}

// 테스트: 복합 데이터 유형(TestData struct)을 설정하고 올바르게 가져오는지 확인
TEST_F(DataStoreTest, SetAndGetComplexType) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "test_data";
    TestData data = {42, "hello"};
    ds.set(id, data, DataType::Config);
    ASSERT_EQ(ds.get<TestData>(id), data);
}

// 테스트: 존재하지 않는 데이터를 가져오려고 할 때 예외가 발생하는지 확인
TEST_F(DataStoreTest, GetNonExistentDataThrowsException) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "non_existent";
    ASSERT_THROW(ds.get<int>(id), std::out_of_range);
}

// 테스트: 잘못된 데이터 유형으로 데이터를 가져오려고 할 때 예외가 발생하는지 확인
TEST_F(DataStoreTest, GetWithWrongTypeThrowsException) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "test_float";
    float value = 3.14f;
    ds.set(id, value, DataType::Para);
    ASSERT_THROW(ds.get<int>(id), std::bad_any_cast);
}

// 테스트: 이미 존재하는 데이터의 유형과 다른 유형으로 설정하려고 할 때 예외가 발생하는지 확인
TEST_F(DataStoreTest, SetWithDifferentTypeThrowsException) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "test_type_change";
    int value1 = 10;
    ds.set(id, value1, DataType::Para);
    float value2 = 20.0f;
    ASSERT_THROW(ds.set(id, value2, DataType::Config), std::runtime_error); // Should throw due to type mismatch
}

// 테스트: 기본 데이터 유형(int)을 폴링하고 올바르게 가져오는지 확인
TEST_F(DataStoreTest, PollBasicType) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "poll_int";
    int value = 456;
    ds.set(id, value, DataType::InterfaceData);
    ASSERT_EQ(ds.poll<int>(id), value);
}

// 테스트: 존재하지 않는 데이터를 폴링하려고 할 때 예외가 발생하는지 확인
TEST_F(DataStoreTest, PollNonExistentDataThrowsException) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "non_existent_poll";
    ASSERT_THROW(ds.poll<int>(id), std::out_of_range);
}

// 테스트: 잘못된 데이터 유형으로 데이터를 폴링하려고 할 때 예외가 발생하는지 확인
TEST_F(DataStoreTest, PollWithWrongTypeThrowsException) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "poll_float";
    float value = 6.28f;
    ds.set(id, value, DataType::InterfaceData);
    ASSERT_THROW(ds.poll<int>(id), std::bad_any_cast);
}

// 테스트: Observer를 구독하고 데이터 변경 시 알림을 받는지 확인
TEST_F(DataStoreTest, SubscribeAndNotify) {
    DataStore& ds = DataStore::getInstance();
    MockObserver observer;
    std::string id = "alarm_event";
    ds.subscribe(id, &observer);

    int alarm_code = 101;
    ds.set(id, alarm_code, DataType::Alarm);

    ASSERT_EQ(observer.call_count(), 1);
    ASSERT_EQ(observer.last_changed_data().id, id);
    ASSERT_EQ(std::any_cast<int>(observer.last_changed_data().value), alarm_code);
}

// 테스트: Observer 구독 해지 후 알림이 중지되는지 확인
TEST_F(DataStoreTest, UnsubscribeStopsNotifications) {
    DataStore& ds = DataStore::getInstance();
    MockObserver observer;
    std::string id = "unsubscribe_test";
    ds.subscribe(id, &observer);

    ds.set(id, 1, DataType::Event);
    ASSERT_EQ(observer.call_count(), 1);

    ds.unsubscribe(id, &observer);
    ds.set(id, 2, DataType::Event);
    ASSERT_EQ(observer.call_count(), 1); // Should not be called again
}

// 테스트: 여러 Observer가 데이터 변경 시 모두 알림을 받는지 확인
TEST_F(DataStoreTest, MultipleSubscribers) {
    DataStore& ds = DataStore::getInstance();
    MockObserver obs1, obs2;
    std::string id = "multi_sub";

    ds.subscribe(id, &obs1);
    ds.subscribe(id, &obs2);

    ds.set(id, 100, DataType::Alarm);

    ASSERT_EQ(obs1.call_count(), 1);
    ASSERT_EQ(obs2.call_count(), 1);
}

// 테스트: 다중 스레드 환경에서 set/get 작업의 스레드 안전성 확인
TEST_F(DataStoreTest, ThreadSafetySetGet) {
    DataStore& ds = DataStore::getInstance();
    std::string id_prefix = "thread_test_";
    const int num_threads = 10;
    const int num_iterations = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_iterations; ++j) {
                std::string id = id_prefix + std::to_string(i * num_iterations + j);
                int value = i * num_iterations + j;
                ds.set(id, value, DataType::Para);
                ASSERT_EQ(ds.get<int>(id), value);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 모든 데이터가 존재하고 올바른지 확인
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < num_iterations; ++j) {
            std::string id = id_prefix + std::to_string(i * num_iterations + j);
            int value = i * num_iterations + j;
            ASSERT_EQ(ds.get<int>(id), value);
        }
    }
}

// 테스트: set, get, poll 작업 후 성능 메트릭이 올바르게 업데이트되는지 확인
TEST_F(DataStoreTest, PerformanceMetricsUpdate) {
    DataStore& ds = DataStore::getInstance();
    
    // Capture initial metrics
    auto initial_metrics = ds.getPerformanceMetrics();
    double initial_set_calls = initial_metrics["set_calls"];
    double initial_get_calls = initial_metrics["get_calls"];
    double initial_poll_calls = initial_metrics["poll_calls"];

    ds.set("metric_test_set", 1, DataType::Para);
    ds.get<int>("metric_test_set");
    ds.poll<int>("metric_test_set");

    auto final_metrics = ds.getPerformanceMetrics();
    ASSERT_EQ(final_metrics["set_calls"], initial_set_calls + 1);
    ASSERT_EQ(final_metrics["get_calls"], initial_get_calls + 1);
    ASSERT_EQ(final_metrics["poll_calls"], initial_poll_calls + 1);
}

// 테스트: TTL(Time To Live) 정책에 따라 데이터가 만료되고 제거되는지 확인
TEST_F(DataStoreTest, DataExpirationPolicyTTL) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "ttl_data";
    DataExpirationPolicy policy = {ExpirationPolicyType::TTL, std::chrono::milliseconds(100)};
    ds.set(id, 100, DataType::Para, policy);
    ASSERT_EQ(ds.get<int>(id), 100);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ds.cleanExpiredData();
    ASSERT_THROW(ds.get<int>(id), std::out_of_range);
}

// 테스트: 만료 정책이 없는 데이터가 만료되지 않고 유지되는지 확인
TEST_F(DataStoreTest, DataExpirationPolicyNoExpiration) {
    DataStore& ds = DataStore::getInstance();
    std::string id = "no_expire_data";
    ds.set(id, 200, DataType::Para);
    ASSERT_EQ(ds.get<int>(id), 200);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ds.cleanExpiredData();
    ASSERT_EQ(ds.get<int>(id), 200); // Should still exist
}

// 테스트: 현재 DataStore에 저장된 데이터 항목의 수가 올바르게 반환되는지 확인
TEST_F(DataStoreTest, GetCurrentDataCount) {
    DataStore& ds = DataStore::getInstance();
    // 이전 테스트로 인한 항목 수를 고려하거나, 테스트 시작 시 DataStore를 초기화해야 함
    size_t initial_count = ds.getCurrentDataCount();
    ds.set("count_test_1", 1, DataType::Para);
    ds.set("count_test_2", 2, DataType::Para);
    ASSERT_EQ(ds.getCurrentDataCount(), initial_count + 2);
}

// 테스트: DataStore 상태를 파일에 저장하고 로드하는 기능 확인 (플레이스홀더 구현)
TEST_F(DataStoreTest, SaveAndLoadState) {
    DataStore& ds = DataStore::getInstance();
    std::string filepath = "test_datastore_state.txt";
    ds.set("state_data_1", 10, DataType::Para);
    ds.saveState(filepath);

    // 현재는 loadState가 예외를 발생시키지 않고 파일이 존재하는지 확인하는 수준
    ASSERT_NO_THROW(ds.loadState(filepath));

    // 테스트 파일 정리
    std::remove(filepath.c_str());
}

// 테스트: DataStore 접근 제어 정책이 올바르게 작동하는지 확인
TEST_F(DataStoreTest, AccessControl) {
    DataStore& ds = DataStore::getInstance();
    std::string data_id = "sensitive_data";
    std::string module_a = "ModuleA";
    std::string module_b = "ModuleB";

    ds.set(data_id, 999, DataType::Config);

    // 기본적으로 접근 권한 없음
    ASSERT_FALSE(ds.hasAccess(data_id, module_a));

    // ModuleA에 접근 권한 부여
    ds.setAccessPolicy(data_id, module_a, true);
    ASSERT_TRUE(ds.hasAccess(data_id, module_a));
    ASSERT_FALSE(ds.hasAccess(data_id, module_b)); // ModuleB는 여전히 접근 권한 없음

    // ModuleA의 접근 권한 회수
    ds.setAccessPolicy(data_id, module_a, false);
    ASSERT_FALSE(ds.hasAccess(data_id, module_a));
}