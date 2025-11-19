#include "gtest/gtest.h"
#include "dto/BagMessage.h"
#include <nlohmann/json.hpp>

namespace mxrc::core::logging {

class BagMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 기본 BagMessage 설정
        validMessage.timestamp_ns = 1700000000000000000;
        validMessage.topic = "mission_state";
        validMessage.data_type = DataType::MissionState;
        validMessage.serialized_value = R"({"state":"RUNNING"})";
    }

    BagMessage validMessage;
};

// Test 1: toJson 직렬화 테스트
TEST_F(BagMessageTest, ToJsonSerialization) {
    // When
    auto json = validMessage.toJson();

    // Then
    EXPECT_EQ(json["timestamp"], 1700000000000000000);
    EXPECT_EQ(json["topic"], "mission_state");
    EXPECT_EQ(json["type"], "MissionState");
    EXPECT_TRUE(json["value"].is_object());
    EXPECT_EQ(json["value"]["state"], "RUNNING");
}

// Test 2: fromJson 역직렬화 테스트
TEST_F(BagMessageTest, FromJsonDeserialization) {
    // Given
    nlohmann::json json = {
        {"timestamp", 1700000000100000000},
        {"topic", "task_state"},
        {"type", "TaskState"},
        {"value", {{"task_id", "task_1"}, {"status", "EXECUTING"}}}
    };

    // When
    auto msg = BagMessage::fromJson(json);

    // Then
    EXPECT_EQ(msg.timestamp_ns, 1700000000100000000);
    EXPECT_EQ(msg.topic, "task_state");
    EXPECT_EQ(msg.data_type, DataType::TaskState);

    auto value = nlohmann::json::parse(msg.serialized_value);
    EXPECT_EQ(value["task_id"], "task_1");
    EXPECT_EQ(value["status"], "EXECUTING");
}

// Test 3: toJson/fromJson 왕복 변환 테스트
TEST_F(BagMessageTest, RoundTripConversion) {
    // When
    auto json = validMessage.toJson();
    auto restored = BagMessage::fromJson(json);

    // Then
    EXPECT_EQ(restored.timestamp_ns, validMessage.timestamp_ns);
    EXPECT_EQ(restored.topic, validMessage.topic);
    EXPECT_EQ(restored.data_type, validMessage.data_type);
    EXPECT_EQ(restored.serialized_value, validMessage.serialized_value);
}

// Test 4: JSONL 라인 직렬화/역직렬화 테스트
TEST_F(BagMessageTest, JsonLineConversion) {
    // When
    std::string line = validMessage.toJsonLine();

    // Then - 개행 문자 확인
    EXPECT_TRUE(line.back() == '\n');

    // When - 역직렬화
    auto restored = BagMessage::fromJsonLine(line);

    // Then
    EXPECT_EQ(restored.timestamp_ns, validMessage.timestamp_ns);
    EXPECT_EQ(restored.topic, validMessage.topic);
    EXPECT_EQ(restored.data_type, validMessage.data_type);
}

// Test 5: isValid() 검증 테스트
TEST_F(BagMessageTest, ValidationRules) {
    // Valid message
    EXPECT_TRUE(validMessage.isValid());

    // Invalid: timestamp <= 0
    BagMessage msg1 = validMessage;
    msg1.timestamp_ns = 0;
    EXPECT_FALSE(msg1.isValid());

    msg1.timestamp_ns = -1;
    EXPECT_FALSE(msg1.isValid());

    // Invalid: empty topic
    BagMessage msg2 = validMessage;
    msg2.topic = "";
    EXPECT_FALSE(msg2.isValid());

    // Invalid: topic too long (> 256 chars)
    BagMessage msg3 = validMessage;
    msg3.topic = std::string(257, 'a');
    EXPECT_FALSE(msg3.isValid());

    // Invalid: empty serialized_value
    BagMessage msg4 = validMessage;
    msg4.serialized_value = "";
    EXPECT_FALSE(msg4.isValid());

    // Valid: topic exactly 256 chars
    BagMessage msg5 = validMessage;
    msg5.topic = std::string(256, 'a');
    EXPECT_TRUE(msg5.isValid());
}

// Test 6: 모든 DataType 직렬화 테스트
TEST_F(BagMessageTest, AllDataTypeSerialization) {
    std::vector<std::pair<DataType, std::string>> types = {
        {DataType::RobotMode, "RobotMode"},
        {DataType::InterfaceData, "InterfaceData"},
        {DataType::Config, "Config"},
        {DataType::Para, "Para"},
        {DataType::Alarm, "Alarm"},
        {DataType::Event, "Event"},
        {DataType::MissionState, "MissionState"},
        {DataType::TaskState, "TaskState"}
    };

    for (const auto& [type, typeStr] : types) {
        // Given
        BagMessage msg;
        msg.timestamp_ns = 1700000000000000000;
        msg.topic = "test_" + typeStr;
        msg.data_type = type;
        msg.serialized_value = R"({"test":"value"})";

        // When
        auto json = msg.toJson();
        auto restored = BagMessage::fromJson(json);

        // Then
        EXPECT_EQ(json["type"], typeStr);
        EXPECT_EQ(restored.data_type, type);
    }
}

// Test 7: JSON 파싱 오류 처리 테스트
TEST_F(BagMessageTest, InvalidJsonHandling) {
    // Invalid JSON in serialized_value
    BagMessage msg = validMessage;
    msg.serialized_value = "not a json";

    // toJson()은 예외 발생 (nlohmann::json::parse 실패)
    EXPECT_THROW(msg.toJson(), nlohmann::json::exception);

    // Invalid JSONL line
    std::string invalidLine = "{invalid json\n";
    EXPECT_THROW(BagMessage::fromJsonLine(invalidLine), nlohmann::json::exception);
}

} // namespace mxrc::core::logging
