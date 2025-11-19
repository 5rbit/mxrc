#include "gtest/gtest.h"
#include "util/Serializer.h"
#include <nlohmann/json.hpp>

namespace mxrc::core::logging {

class SerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 데이터 준비
    }
};

// Test 1: RobotMode 직렬화 (정수)
TEST_F(SerializerTest, SerializeRobotModeInteger) {
    // Given
    int mode = 1;  // 예: AUTO 모드
    std::any value = mode;

    // When
    auto json = Serializer::serialize(value, DataType::RobotMode);

    // Then
    EXPECT_TRUE(json.contains("mode"));
    EXPECT_EQ(json["mode"], 1);
}

// Test 2: RobotMode 직렬화 (문자열)
TEST_F(SerializerTest, SerializeRobotModeString) {
    // Given
    std::string mode = "MANUAL";
    std::any value = mode;

    // When
    auto json = Serializer::serialize(value, DataType::RobotMode);

    // Then
    EXPECT_TRUE(json.contains("mode"));
    EXPECT_EQ(json["mode"], "MANUAL");
}

// Test 3: InterfaceData 직렬화 (센서 데이터)
TEST_F(SerializerTest, SerializeInterfaceDataDouble) {
    // Given
    double sensorValue = 25.6;  // 예: 온도 센서
    std::any value = sensorValue;

    // When
    auto json = Serializer::serialize(value, DataType::InterfaceData);

    // Then
    EXPECT_TRUE(json.contains("value"));
    EXPECT_DOUBLE_EQ(json["value"], 25.6);
}

// Test 4: Config 직렬화 (JSON 문자열)
TEST_F(SerializerTest, SerializeConfigJsonString) {
    // Given
    std::string configJson = R"({"max_speed":100,"timeout":5000})";
    std::any value = configJson;

    // When
    auto json = Serializer::serialize(value, DataType::Config);

    // Then
    EXPECT_TRUE(json.contains("max_speed"));
    EXPECT_EQ(json["max_speed"], 100);
    EXPECT_EQ(json["timeout"], 5000);
}

// Test 5: Para 직렬화 (다양한 타입)
TEST_F(SerializerTest, SerializeParaVariousTypes) {
    // String parameter
    {
        std::any value = std::string("param_value");
        auto json = Serializer::serialize(value, DataType::Para);
        EXPECT_TRUE(json.contains("parameter"));
        EXPECT_EQ(json["parameter"], "param_value");
    }

    // Double parameter
    {
        std::any value = 3.14;
        auto json = Serializer::serialize(value, DataType::Para);
        EXPECT_TRUE(json.contains("parameter"));
        EXPECT_DOUBLE_EQ(json["parameter"], 3.14);
    }

    // Integer parameter
    {
        std::any value = 42;
        auto json = Serializer::serialize(value, DataType::Para);
        EXPECT_TRUE(json.contains("parameter"));
        EXPECT_EQ(json["parameter"], 42);
    }
}

// Test 6: Alarm 직렬화
TEST_F(SerializerTest, SerializeAlarm) {
    // Given
    std::string alarmJson = R"({"severity":"HIGH","message":"Temperature critical"})";
    std::any value = alarmJson;

    // When
    auto json = Serializer::serialize(value, DataType::Alarm);

    // Then
    EXPECT_TRUE(json.contains("severity"));
    EXPECT_EQ(json["severity"], "HIGH");
    EXPECT_EQ(json["message"], "Temperature critical");
}

// Test 7: MissionState 직렬화
TEST_F(SerializerTest, SerializeMissionState) {
    // Given
    std::string stateJson = R"({"state":"RUNNING","progress":0.75})";
    std::any value = stateJson;

    // When
    auto json = Serializer::serialize(value, DataType::MissionState);

    // Then
    EXPECT_TRUE(json.contains("state"));
    EXPECT_EQ(json["state"], "RUNNING");
    EXPECT_DOUBLE_EQ(json["progress"], 0.75);
}

// Test 8: TaskState 직렬화
TEST_F(SerializerTest, SerializeTaskState) {
    // Given
    std::string taskJson = R"({"task_id":"task_1","status":"EXECUTING"})";
    std::any value = taskJson;

    // When
    auto json = Serializer::serialize(value, DataType::TaskState);

    // Then
    EXPECT_TRUE(json.contains("task_id"));
    EXPECT_EQ(json["task_id"], "task_1");
    EXPECT_EQ(json["status"], "EXECUTING");
}

// Test 9: 역직렬화 테스트 - RobotMode
TEST_F(SerializerTest, DeserializeRobotMode) {
    // Given
    nlohmann::json json = {{"mode", 2}};

    // When
    auto value = Serializer::deserialize(json, DataType::RobotMode);

    // Then
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(std::any_cast<int>(value), 2);
}

// Test 10: 역직렬화 테스트 - MissionState
TEST_F(SerializerTest, DeserializeMissionState) {
    // Given
    nlohmann::json json = {{"state", "PAUSED"}, {"reason", "user_request"}};

    // When
    auto value = Serializer::deserialize(json, DataType::MissionState);

    // Then
    EXPECT_TRUE(value.has_value());
    std::string restored = std::any_cast<std::string>(value);
    auto restoredJson = nlohmann::json::parse(restored);
    EXPECT_EQ(restoredJson["state"], "PAUSED");
    EXPECT_EQ(restoredJson["reason"], "user_request");
}

// Test 11: 왕복 변환 테스트 (serialize → deserialize)
TEST_F(SerializerTest, RoundTripConversion) {
    // Given
    std::string original = R"({"config_key":"config_value","enabled":true})";
    std::any value = original;

    // When
    auto json = Serializer::serialize(value, DataType::Config);
    auto restored = Serializer::deserialize(json, DataType::Config);

    // Then
    std::string restoredStr = std::any_cast<std::string>(restored);
    auto restoredJson = nlohmann::json::parse(restoredStr);
    auto originalJson = nlohmann::json::parse(original);
    EXPECT_EQ(restoredJson, originalJson);
}

// Test 12: 타입 불일치 에러 처리
TEST_F(SerializerTest, TypeMismatchHandling) {
    // Given - RobotMode에 bool 타입 전달
    std::any value = true;  // RobotMode는 int 또는 string 기대

    // When
    auto json = Serializer::serialize(value, DataType::RobotMode);

    // Then - 에러 객체 반환
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "type_mismatch");
}

// Test 13: 알 수 없는 DataType 처리
TEST_F(SerializerTest, UnknownDataTypeHandling) {
    // Given
    std::any value = std::string("test");

    // When - 잘못된 enum 값 사용
    auto json = Serializer::serialize(value, static_cast<DataType>(999));

    // Then
    EXPECT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"], "unknown_type");
}

// Test 14: Event 직렬화/역직렬화
TEST_F(SerializerTest, SerializeDeserializeEvent) {
    // Given
    std::string eventJson = R"({"event_type":"collision","timestamp":1700000000})";
    std::any value = eventJson;

    // When
    auto json = Serializer::serialize(value, DataType::Event);
    auto restored = Serializer::deserialize(json, DataType::Event);

    // Then
    std::string restoredStr = std::any_cast<std::string>(restored);
    auto restoredJson = nlohmann::json::parse(restoredStr);
    EXPECT_EQ(restoredJson["event_type"], "collision");
    EXPECT_EQ(restoredJson["timestamp"], 1700000000);
}

// Test 15: 빈 값 처리
TEST_F(SerializerTest, EmptyValueHandling) {
    // Given - 빈 문자열
    std::any value = std::string("");

    // When
    auto json = Serializer::serialize(value, DataType::Config);

    // Then - 빈 문자열도 직렬화 가능
    EXPECT_TRUE(json.contains("value"));
    EXPECT_EQ(json["value"], "");
}

} // namespace mxrc::core::logging
