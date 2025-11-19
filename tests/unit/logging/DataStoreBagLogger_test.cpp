#include "gtest/gtest.h"
#include "core/DataStoreBagLogger.h"
#include "core/SimpleBagWriter.h"
#include "core/event/core/EventBus.h"
#include "core/event/dto/DataStoreEvents.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

class DataStoreBagLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리
        testDir = fs::temp_directory_path() / "mxrc_baglogger_test";
        fs::create_directories(testDir);

        // EventBus 생성 및 시작
        eventBus = std::make_shared<event::EventBus>(10000);
        eventBus->start();

        // SimpleBagWriter 생성
        bagWriter = std::make_shared<SimpleBagWriter>(testDir.string(), "test", 1000);
    }

    void TearDown() override {
        // 정리
        if (eventBus) {
            eventBus->stop();
        }

        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    std::shared_ptr<event::DataStoreValueChangedEvent> createTestEvent(
        const std::string& key,
        const std::string& oldValue,
        const std::string& newValue,
        const std::string& valueType) {

        return std::make_shared<event::DataStoreValueChangedEvent>(
            key, oldValue, newValue, valueType, "test"
        );
    }

    int countBagFiles() {
        int count = 0;
        for (const auto& entry : fs::directory_iterator(testDir)) {
            if (entry.path().extension() == ".bag") {
                count++;
            }
        }
        return count;
    }

    fs::path testDir;
    std::shared_ptr<event::EventBus> eventBus;
    std::shared_ptr<SimpleBagWriter> bagWriter;
};

// Test 1: DataStoreBagLogger 생성 및 시작
TEST_F(DataStoreBagLoggerTest, BasicCreationAndStart) {
    // Given
    DataStoreBagLogger logger(eventBus, bagWriter);

    // When
    bool started = logger.start();

    // Then
    EXPECT_TRUE(started);
    EXPECT_TRUE(logger.isRunning());
    EXPECT_FALSE(logger.getCurrentFilePath().empty());

    logger.stop();
    EXPECT_FALSE(logger.isRunning());
}

// Test 2: nullptr 인자 거부
TEST_F(DataStoreBagLoggerTest, RejectNullptrArguments) {
    // Given / When / Then
    EXPECT_THROW(DataStoreBagLogger(nullptr, bagWriter), std::invalid_argument);
    EXPECT_THROW(DataStoreBagLogger(eventBus, nullptr), std::invalid_argument);
}

// Test 3: 이벤트 수신 및 Bag 기록
TEST_F(DataStoreBagLoggerTest, ReceiveEventAndWriteToBag) {
    // Given
    DataStoreBagLogger logger(eventBus, bagWriter);
    logger.start();

    // When - 이벤트 10개 발행
    for (int i = 0; i < 10; i++) {
        auto event = createTestEvent(
            "mission_state",
            R"({"state":"IDLE"})",
            R"({"state":"RUNNING"})",
            "MissionState"
        );
        eventBus->publish(event);
    }

    // EventBus 처리 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // flush 대기
    EXPECT_TRUE(logger.flush(5000));

    // Then
    auto stats = logger.getStats();
    EXPECT_EQ(stats.messagesWritten, 10);
    EXPECT_EQ(stats.messagesDropped, 0);

    logger.stop();

    // 파일 검증
    std::string filepath = logger.getCurrentFilePath();
    EXPECT_TRUE(fs::exists(filepath));

    std::ifstream ifs(filepath);
    std::string line;
    int lineCount = 0;
    while (std::getline(ifs, line)) {
        lineCount++;
    }
    EXPECT_EQ(lineCount, 10);
}

// Test 4: 다양한 DataType 처리
TEST_F(DataStoreBagLoggerTest, HandleVariousDataTypes) {
    // Given
    DataStoreBagLogger logger(eventBus, bagWriter);
    logger.start();

    // When - 다양한 타입의 이벤트 발행 (valid JSON)
    auto event1 = createTestEvent("mission", R"({"state":"old"})", R"({"state":"new"})", "MissionState");
    auto event2 = createTestEvent("task", R"({"status":"old"})", R"({"status":"new"})", "TaskState");
    auto event3 = createTestEvent("alarm", R"({"level":"old"})", R"({"level":"new"})", "Alarm");
    auto event4 = createTestEvent("event", R"({"type":"old"})", R"({"type":"new"})", "Event");
    auto event5 = createTestEvent("interface", R"({"data":"old"})", R"({"data":"new"})", "InterfaceData");
    auto event6 = createTestEvent("unknown", R"({"value":"old"})", R"({"value":"new"})", "UnknownType");

    eventBus->publish(event1);
    eventBus->publish(event2);
    eventBus->publish(event3);
    eventBus->publish(event4);
    eventBus->publish(event5);
    eventBus->publish(event6);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(logger.flush(5000));

    // Then
    auto stats = logger.getStats();
    EXPECT_EQ(stats.messagesWritten, 6);

    logger.stop();
}

// Test 5: 통계 수집
TEST_F(DataStoreBagLoggerTest, StatisticsTracking) {
    // Given
    DataStoreBagLogger logger(eventBus, bagWriter);
    logger.start();

    // When - 이벤트 20개 발행
    for (int i = 0; i < 20; i++) {
        auto event = createTestEvent(
            "test_key",
            R"({"value":0})",
            R"({"value":1})",
            "Event"
        );
        eventBus->publish(event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(logger.flush(5000));

    // Then
    auto stats = logger.getStats();
    EXPECT_EQ(stats.messagesWritten, 20);
    EXPECT_EQ(stats.messagesDropped, 0);
    EXPECT_GT(stats.bytesWritten, 0);

    logger.stop();
}

// Test 6: Rotation 정책 동작
TEST_F(DataStoreBagLoggerTest, RotationPolicyWorks) {
    // Given - 작은 파일 크기로 순환 유도 (20KB)
    DataStoreBagLogger logger(eventBus, bagWriter);
    logger.setRotationPolicy(RotationPolicy::createSizePolicy(0.02));  // 20KB
    logger.start();

    // When - 메시지 500개 발행 (~54KB → 2번 순환 예상)
    for (int i = 0; i < 500; i++) {
        auto event = createTestEvent(
            "rotation_test",
            R"({"value":0})",
            R"({"value":1})",
            "MissionState"
        );
        eventBus->publish(event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(logger.flush(5000));

    // Then - 파일이 순환되었는지 확인
    auto stats = logger.getStats();
    int fileCount = countBagFiles();

    spdlog::info("Rotation 테스트: 순환 {}회, 파일 {}개", stats.rotationCount, fileCount);

    EXPECT_GT(stats.rotationCount, 0);
    EXPECT_GT(fileCount, 1);
    EXPECT_EQ(stats.messagesWritten, 500);

    logger.stop();
}

// Test 7: 소멸자 안전성
TEST_F(DataStoreBagLoggerTest, DestructorSafety) {
    // Given
    std::string filepath;
    {
        DataStoreBagLogger logger(eventBus, bagWriter);
        logger.start();

        // When - 이벤트 추가 후 즉시 소멸
        for (int i = 0; i < 50; i++) {
            auto event = createTestEvent(
                "destructor_test",
                R"({"value":0})",
                R"({"value":1})",
                "Event"
            );
            eventBus->publish(event);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        filepath = logger.getCurrentFilePath();

        // 소멸자 호출 (자동)
    }

    // Then - 파일이 안전하게 닫혀야 함
    EXPECT_TRUE(fs::exists(filepath));

    std::ifstream ifs(filepath);
    std::string line;
    int lineCount = 0;
    while (std::getline(ifs, line)) {
        lineCount++;
    }
    EXPECT_GT(lineCount, 0);  // 일부 메시지는 쓰여져야 함
}

// Test 8: 이중 시작 방지
TEST_F(DataStoreBagLoggerTest, PreventDoubleStart) {
    // Given
    DataStoreBagLogger logger(eventBus, bagWriter);
    logger.start();

    // When
    bool secondStart = logger.start();

    // Then
    EXPECT_FALSE(secondStart);
    EXPECT_TRUE(logger.isRunning());

    logger.stop();
}

} // namespace mxrc::core::logging
