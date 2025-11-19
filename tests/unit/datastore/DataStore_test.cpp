#include "gtest/gtest.h"
#include "DataStore.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>

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

// 테스트: DataStore 인스턴스가 항상 동일한 싱글톤 인스턴스를 반환하는지 확인
TEST(DataStoreTest, GetInstanceReturnsSameInstance) {
    auto instance1 = DataStore::create();
    auto instance2 = DataStore::create();
    ASSERT_EQ(instance1.get(), instance2.get());
}

// 테스트: createForTest()는 매번 다른 인스턴스를 반환하는지 확인
TEST(DataStoreTest, CreateForTestReturnsUniqueInstances) {
    auto instance1 = DataStore::createForTest();
    auto instance2 = DataStore::createForTest();
    ASSERT_NE(instance1.get(), instance2.get());
}

// 테스트: 기본 데이터 유형(int)을 설정하고 올바르게 가져오는지 확인
TEST(DataStoreTest, SetAndGetBasicType) {
    auto ds = DataStore::createForTest();
    std::string id = "test_int";
    int value = 123;
    ds->set(id, value, DataType::Para);
    ASSERT_EQ(ds->get<int>(id), value);
}

// 테스트: 복합 데이터 유형(TestData struct)을 설정하고 올바르게 가져오는지 확인
TEST(DataStoreTest, SetAndGetComplexType) {
    auto ds = DataStore::createForTest();
    std::string id = "test_data";
    TestData data = {42, "hello"};
    ds->set(id, data, DataType::Config);
    ASSERT_EQ(ds->get<TestData>(id), data);
}

// 테스트: 존재하지 않는 데이터를 가져오려고 할 때 예외가 발생하는지 확인
TEST(DataStoreTest, GetNonExistentDataThrowsException) {
    auto ds = DataStore::createForTest();
    std::string id = "non_existent";
    ASSERT_THROW(ds->get<int>(id), std::out_of_range);
}

// 테스트: 잘못된 데이터 유형으로 데이터를 가져오려고 할 때 예외가 발생하는지 확인
TEST(DataStoreTest, GetWithWrongTypeThrowsException) {
    auto ds = DataStore::createForTest();
    std::string id = "test_float";
    float value = 3.14f;
    ds->set(id, value, DataType::Para);
    ASSERT_THROW(ds->get<int>(id), std::runtime_error);
}

// 테스트: 이미 존재하는 데이터의 유형과 다른 유형으로 설정하려고 할 때 예외가 발생하는지 확인
TEST(DataStoreTest, SetWithDifferentTypeThrowsException) {
    auto ds = DataStore::createForTest();
    std::string id = "test_type_change";
    int value1 = 10;
    ds->set(id, value1, DataType::Para);
    float value2 = 20.0f;
    ASSERT_THROW(ds->set(id, value2, DataType::Config), std::runtime_error); // Should throw due to type mismatch
}

// 테스트: 기본 데이터 유형(int)을 폴링하고 올바르게 가져오는지 확인
TEST(DataStoreTest, PollBasicType) {
    auto ds = DataStore::createForTest();
    std::string id = "poll_int";
    int value = 456;
    ds->set(id, value, DataType::InterfaceData);
    ASSERT_EQ(ds->poll<int>(id), value);
}

// 테스트: 존재하지 않는 데이터를 폴링하려고 할 때 예외가 발생하는지 확인
TEST(DataStoreTest, PollNonExistentDataThrowsException) {
    auto ds = DataStore::createForTest();
    std::string id = "non_existent_poll";
    ASSERT_THROW(ds->poll<int>(id), std::out_of_range);
}

// 테스트: 잘못된 데이터 유형으로 데이터를 폴링하려고 할 때 예외가 발생하는지 확인
TEST(DataStoreTest, PollWithWrongTypeThrowsException) {
    auto ds = DataStore::createForTest();
    std::string id = "poll_float";
    float value = 6.28f;
    ds->set(id, value, DataType::InterfaceData);
    ASSERT_THROW(ds->poll<int>(id), std::runtime_error);
}

// 테스트: Observer를 구독하고 데이터 변경 시 알림을 받는지 확인
TEST(DataStoreTest, SubscribeAndNotify) {
    auto ds = DataStore::createForTest();
    auto observer = std::make_shared<MockObserver>();
    std::string id = "alarm_event";
    ds->subscribe(id, observer);

    int alarm_code = 101;
    ds->set(id, alarm_code, DataType::Alarm);

    ASSERT_EQ(observer->call_count(), 1);
    ASSERT_EQ(observer->last_changed_data().id, id);
    ASSERT_EQ(std::any_cast<int>(observer->last_changed_data().value), alarm_code);
}

// 테스트: Observer 구독 해지 후 알림이 중지되는지 확인
TEST(DataStoreTest, UnsubscribeStopsNotifications) {
    auto ds = DataStore::createForTest();
    auto observer = std::make_shared<MockObserver>();
    std::string id = "unsubscribe_test";
    ds->subscribe(id, observer);

    ds->set(id, 1, DataType::Event);
    ASSERT_EQ(observer->call_count(), 1);

    ds->unsubscribe(id, observer);
    ds->set(id, 2, DataType::Event);
    ASSERT_EQ(observer->call_count(), 1); // Should not be called again
}

// 테스트: 여러 Observer가 데이터 변경 시 모두 알림을 받는지 확인
TEST(DataStoreTest, MultipleSubscribers) {
    auto ds = DataStore::createForTest();
    auto obs1 = std::make_shared<MockObserver>();
    auto obs2 = std::make_shared<MockObserver>();
    std::string id = "multi_sub";

    ds->subscribe(id, obs1);
    ds->subscribe(id, obs2);

    ds->set(id, 100, DataType::Alarm);

    ASSERT_EQ(obs1->call_count(), 1);
    ASSERT_EQ(obs2->call_count(), 1);
}

// 테스트: 다중 스레드 환경에서 set/get 작업의 스레드 안전성 확인
TEST(DataStoreTest, ThreadSafetySetGet) {
    auto ds = DataStore::createForTest();
    std::string id_prefix = "thread_test_";
    const int num_threads = 10;
    const int num_iterations = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_iterations; ++j) {
                std::string id = id_prefix + std::to_string(i * num_iterations + j);
                int value = i * num_iterations + j;
                ds->set(id, value, DataType::Para);
                ASSERT_EQ(ds->get<int>(id), value);
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
            ASSERT_EQ(ds->get<int>(id), value);
        }
    }
}

// 테스트: set, get, poll 작업 후 성능 메트릭이 올바르게 업데이트되는지 확인
TEST(DataStoreTest, PerformanceMetricsUpdate) {
    auto ds = DataStore::createForTest();
    // 이전 메트릭을 지우고 깨끗한 테스트를 위해 (공개 clear 메서드 없음)
    // ds->getPerformanceMetrics().clear(); 

    ds->set("metric_test_set", 1, DataType::Para);
    ds->get<int>("metric_test_set");
    ds->poll<int>("metric_test_set");

    auto metrics = ds->getPerformanceMetrics();
    ASSERT_GT(metrics["set_calls"], 0);
    ASSERT_GT(metrics["get_calls"], 0);
    ASSERT_GT(metrics["poll_calls"], 0);
}

// 테스트: TTL(Time To Live) 정책에 따라 데이터가 만료되고 제거되는지 확인
TEST(DataStoreTest, DataExpirationPolicyTTL) {
    auto ds = DataStore::createForTest();
    std::string id = "ttl_data";
    DataExpirationPolicy policy = {ExpirationPolicyType::TTL, std::chrono::milliseconds(100)};
    ds->set(id, 100, DataType::Para, policy);
    ASSERT_EQ(ds->get<int>(id), 100);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ds->cleanExpiredData();
    ASSERT_THROW(ds->get<int>(id), std::out_of_range);
}

// 테스트: 만료 정책이 없는 데이터가 만료되지 않고 유지되는지 확인
TEST(DataStoreTest, DataExpirationPolicyNoExpiration) {
    auto ds = DataStore::createForTest();
    std::string id = "no_expire_data";
    ds->set(id, 200, DataType::Para);
    ASSERT_EQ(ds->get<int>(id), 200);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ds->cleanExpiredData();
    ASSERT_EQ(ds->get<int>(id), 200); // Should still exist
}

// 테스트: 현재 DataStore에 저장된 데이터 항목의 수가 올바르게 반환되는지 확인
TEST(DataStoreTest, GetCurrentDataCount) {
    auto ds = DataStore::createForTest();
    // 이전 테스트로 인한 항목 수를 고려하거나, 테스트 시작 시 DataStore를 초기화해야 함
    size_t initial_count = ds->getCurrentDataCount();
    ds->set("count_test_1", 1, DataType::Para);
    ds->set("count_test_2", 2, DataType::Para);
    ASSERT_EQ(ds->getCurrentDataCount(), initial_count + 2);
}

// 테스트: 기본 타입(int, double, string, bool)의 저장 및 로드
TEST(DataStoreTest, SaveAndLoadBasicTypes) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_datastore_basic.json";

    // 다양한 기본 타입 저장
    ds->set("int_value", 42, DataType::Para);
    ds->set("double_value", 3.14159, DataType::Para);
    ds->set("string_value", std::string("hello world"), DataType::Config);
    ds->set("bool_value", true, DataType::Event);
    ds->set("long_value", 9223372036854775807L, DataType::Para);

    // 상태 저장
    ASSERT_NO_THROW(ds->saveState(filepath));

    // 새로운 인스턴스 생성 후 로드
    auto ds2 = DataStore::createForTest();
    ASSERT_NO_THROW(ds2->loadState(filepath));

    // 데이터 검증
    EXPECT_EQ(ds2->get<int>("int_value"), 42);
    EXPECT_DOUBLE_EQ(ds2->get<double>("double_value"), 3.14159);
    EXPECT_EQ(ds2->get<std::string>("string_value"), "hello world");
    EXPECT_EQ(ds2->get<bool>("bool_value"), true);
    EXPECT_EQ(ds2->get<long>("long_value"), 9223372036854775807L);

    // 테스트 파일 정리
    std::remove(filepath.c_str());
}

// 테스트: 다양한 DataType으로 저장 및 로드
TEST(DataStoreTest, SaveAndLoadDifferentDataTypes) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_datastore_types.json";

    // 각 DataType별로 데이터 저장
    ds->set("robot_mode", 1, DataType::RobotMode);
    ds->set("interface_data", 2, DataType::InterfaceData);
    ds->set("config_data", 3, DataType::Config);
    ds->set("para_data", 4, DataType::Para);
    ds->set("alarm_data", 5, DataType::Alarm);
    ds->set("event_data", 6, DataType::Event);
    ds->set("mission_state", 7, DataType::MissionState);
    ds->set("task_state", 8, DataType::TaskState);

    ASSERT_NO_THROW(ds->saveState(filepath));

    auto ds2 = DataStore::createForTest();
    ASSERT_NO_THROW(ds2->loadState(filepath));

    // 모든 데이터 검증
    EXPECT_EQ(ds2->get<int>("robot_mode"), 1);
    EXPECT_EQ(ds2->get<int>("interface_data"), 2);
    EXPECT_EQ(ds2->get<int>("config_data"), 3);
    EXPECT_EQ(ds2->get<int>("para_data"), 4);
    EXPECT_EQ(ds2->get<int>("alarm_data"), 5);
    EXPECT_EQ(ds2->get<int>("event_data"), 6);
    EXPECT_EQ(ds2->get<int>("mission_state"), 7);
    EXPECT_EQ(ds2->get<int>("task_state"), 8);

    std::remove(filepath.c_str());
}

// 테스트: Round-trip 일관성 (저장 → 로드 → 검증)
TEST(DataStoreTest, RoundTripConsistency) {
    auto ds1 = DataStore::createForTest();
    std::string filepath = "test_roundtrip.json";

    // 복잡한 데이터 세트 생성
    ds1->set("temperature", 25.5, DataType::Para);
    ds1->set("position_x", 100, DataType::Config);
    ds1->set("robot_name", std::string("Robot-A"), DataType::Config);
    ds1->set("is_active", true, DataType::Event);
    ds1->set("counter", 12345L, DataType::Para);

    // 첫 번째 저장
    ds1->saveState(filepath);

    // 로드 후 다시 저장
    auto ds2 = DataStore::createForTest();
    ds2->loadState(filepath);
    std::string filepath2 = "test_roundtrip2.json";
    ds2->saveState(filepath2);

    // 세 번째 인스턴스로 로드
    auto ds3 = DataStore::createForTest();
    ds3->loadState(filepath2);

    // 데이터 일관성 검증
    EXPECT_DOUBLE_EQ(ds3->get<double>("temperature"), 25.5);
    EXPECT_EQ(ds3->get<int>("position_x"), 100);
    EXPECT_EQ(ds3->get<std::string>("robot_name"), "Robot-A");
    EXPECT_EQ(ds3->get<bool>("is_active"), true);
    EXPECT_EQ(ds3->get<long>("counter"), 12345L);

    std::remove(filepath.c_str());
    std::remove(filepath2.c_str());
}

// 테스트: 빈 DataStore 저장 및 로드
TEST(DataStoreTest, SaveAndLoadEmptyDataStore) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_empty.json";

    // 빈 상태 저장
    ASSERT_NO_THROW(ds->saveState(filepath));

    // 빈 상태 로드
    auto ds2 = DataStore::createForTest();
    ASSERT_NO_THROW(ds2->loadState(filepath));

    // 데이터가 없는지 확인
    EXPECT_EQ(ds2->getCurrentDataCount(), 0);

    std::remove(filepath.c_str());
}

// 테스트: 파일 I/O 에러 처리 (쓰기 실패)
TEST(DataStoreTest, SaveStateInvalidPath) {
    auto ds = DataStore::createForTest();
    ds->set("test_data", 123, DataType::Para);

    // 존재하지 않는 디렉토리 경로
    std::string invalid_path = "/nonexistent_directory/test_state.json";
    ASSERT_THROW(ds->saveState(invalid_path), std::runtime_error);
}

// 테스트: 파일 I/O 에러 처리 (읽기 실패)
TEST(DataStoreTest, LoadStateFileNotFound) {
    auto ds = DataStore::createForTest();
    std::string nonexistent_file = "nonexistent_file.json";

    ASSERT_THROW(ds->loadState(nonexistent_file), std::runtime_error);
}

// 테스트: 손상된 JSON 파일 처리
TEST(DataStoreTest, LoadStateCorruptedJSON) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_corrupted.json";

    // 손상된 JSON 파일 생성
    std::ofstream ofs(filepath);
    ofs << "{ this is not valid json }";
    ofs.close();

    ASSERT_THROW(ds->loadState(filepath), std::runtime_error);

    std::remove(filepath.c_str());
}

// 테스트: 잘못된 버전 처리
TEST(DataStoreTest, LoadStateInvalidVersion) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_invalid_version.json";

    // 잘못된 버전의 JSON 파일 생성
    std::ofstream ofs(filepath);
    ofs << R"({"version": 999, "data": []})";
    ofs.close();

    ASSERT_THROW(ds->loadState(filepath), std::runtime_error);

    std::remove(filepath.c_str());
}

// 테스트: 버전 필드 누락 처리
TEST(DataStoreTest, LoadStateMissingVersion) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_missing_version.json";

    // 버전 필드가 없는 JSON 파일 생성
    std::ofstream ofs(filepath);
    ofs << R"({"data": []})";
    ofs.close();

    ASSERT_THROW(ds->loadState(filepath), std::runtime_error);

    std::remove(filepath.c_str());
}

// 테스트: 데이터 배열 누락 처리
TEST(DataStoreTest, LoadStateMissingData) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_missing_data.json";

    // 데이터 배열이 없는 JSON 파일 생성
    std::ofstream ofs(filepath);
    ofs << R"({"version": 1})";
    ofs.close();

    ASSERT_THROW(ds->loadState(filepath), std::runtime_error);

    std::remove(filepath.c_str());
}

// 테스트: 타입 불일치 처리 (필드 누락 시 graceful skip)
TEST(DataStoreTest, LoadStateIncompleteData) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_incomplete.json";

    // 불완전한 데이터 항목이 있는 JSON 파일 생성
    std::ofstream ofs(filepath);
    ofs << R"({
        "version": 1,
        "data": [
            {"id": "complete", "type": 0, "value_type": "int", "value": 42},
            {"id": "missing_value", "type": 0, "value_type": "int"},
            {"id": "missing_type"},
            {"id": "valid_again", "type": 1, "value_type": "string", "value": "test"}
        ]
    })";
    ofs.close();

    // 불완전한 항목은 건너뛰고 유효한 항목만 로드
    ASSERT_NO_THROW(ds->loadState(filepath));

    // 유효한 데이터만 존재하는지 확인
    EXPECT_EQ(ds->get<int>("complete"), 42);
    EXPECT_EQ(ds->get<std::string>("valid_again"), "test");

    // 불완전한 항목은 존재하지 않아야 함
    EXPECT_THROW(ds->get<int>("missing_value"), std::out_of_range);
    EXPECT_THROW(ds->get<int>("missing_type"), std::out_of_range);

    std::remove(filepath.c_str());
}

// 테스트: loadState 시 기존 데이터 삭제 확인
TEST(DataStoreTest, LoadStateClearsExistingData) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_clear.json";

    // 초기 데이터 설정
    ds->set("old_data_1", 100, DataType::Para);
    ds->set("old_data_2", 200, DataType::Para);

    // 새로운 데이터만 포함된 파일 생성
    std::ofstream ofs(filepath);
    ofs << R"({
        "version": 1,
        "data": [
            {"id": "new_data", "type": 0, "value_type": "int", "value": 999}
        ]
    })";
    ofs.close();

    // 로드하면 기존 데이터는 사라져야 함
    ds->loadState(filepath);

    EXPECT_EQ(ds->get<int>("new_data"), 999);
    EXPECT_THROW(ds->get<int>("old_data_1"), std::out_of_range);
    EXPECT_THROW(ds->get<int>("old_data_2"), std::out_of_range);

    std::remove(filepath.c_str());
}

// 테스트: float 타입 저장 및 로드
TEST(DataStoreTest, SaveAndLoadFloatType) {
    auto ds = DataStore::createForTest();
    std::string filepath = "test_float.json";

    ds->set("float_val", 2.718f, DataType::Para);
    ds->saveState(filepath);

    auto ds2 = DataStore::createForTest();
    ds2->loadState(filepath);

    EXPECT_FLOAT_EQ(ds2->get<float>("float_val"), 2.718f);

    std::remove(filepath.c_str());
}

// 테스트: DataStore 접근 제어 정책이 올바르게 작동하는지 확인
TEST(DataStoreTest, AccessControl) {
    auto ds = DataStore::createForTest();
    std::string data_id = "sensitive_data";
    std::string module_a = "ModuleA";
    std::string module_b = "ModuleB";

    ds->set(data_id, 999, DataType::Config);

    // 기본적으로 접근 권한 없음
    ASSERT_FALSE(ds->hasAccess(data_id, module_a));

    // ModuleA에 접근 권한 부여
    ds->setAccessPolicy(data_id, module_a, true);
    ASSERT_TRUE(ds->hasAccess(data_id, module_a));
    ASSERT_FALSE(ds->hasAccess(data_id, module_b)); // ModuleB는 여전히 접근 권한 없음

    // ModuleA의 접근 권한 회수
    ds->setAccessPolicy(data_id, module_a, false);
    ASSERT_FALSE(ds->hasAccess(data_id, module_a));
}