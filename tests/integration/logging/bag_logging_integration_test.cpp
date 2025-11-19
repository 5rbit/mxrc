#include "gtest/gtest.h"
#include "core/datastore/DataStore.h"
#include "core/event/core/EventBus.h"
#include "core/event/adapters/DataStoreEventAdapter.h"
#include "core/logging/core/DataStoreBagLogger.h"
#include "core/logging/core/SimpleBagWriter.h"
#include "core/logging/dto/BagMessage.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace mxrc::integration::logging {

using namespace mxrc::core::event;
// DataStore와 DataType은 글로벌 네임스페이스에 정의됨
using mxrc::core::logging::DataStoreBagLogger;
using mxrc::core::logging::SimpleBagWriter;
using mxrc::core::logging::BagMessage;
using mxrc::core::logging::RotationPolicy;
using mxrc::core::logging::RetentionPolicy;

/**
 * @brief DataStore → EventBus → DataStoreBagLogger → Bag 파일 통합 테스트
 *
 * User Story 1의 핵심 시나리오:
 * "DataStore의 주요 데이터 타입을 변경하면서 Bag 파일이 정상적으로 생성되고,
 *  모든 변경 사항이 타임스탬프와 함께 기록되는지 검증"
 */
class BagLoggingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리
        testDir = fs::temp_directory_path() / "mxrc_integration_test";
        fs::create_directories(testDir);

        // 1. DataStore 생성
        dataStore = DataStore::create();

        // 2. EventBus 생성 및 시작
        eventBus = std::make_shared<EventBus>(10000);
        eventBus->start();

        // 3. SimpleBagWriter 생성
        bagWriter = std::make_shared<SimpleBagWriter>(testDir.string(), "integration", 1000);

        // 4. DataStoreBagLogger 생성 및 시작
        bagLogger = std::make_shared<DataStoreBagLogger>(eventBus, bagWriter);
        bagLogger->start();

        // 5. DataStoreEventAdapter 생성 (DataStore → EventBus 연결)
        adapter = std::make_shared<DataStoreEventAdapter>(dataStore, eventBus);

        // 시스템 안정화 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        // 역순으로 정리
        if (bagLogger) {
            bagLogger->stop();
        }

        adapter.reset();  // DataStoreEventAdapter는 소멸자에서 자동 정리

        if (eventBus) {
            eventBus->stop();
        }

        // 디렉토리 정리
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
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

    std::vector<BagMessage> readBagFile(const std::string& filepath) {
        std::vector<BagMessage> messages;
        std::ifstream ifs(filepath);
        std::string line;

        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            try {
                messages.push_back(BagMessage::fromJsonLine(line));
            } catch (const std::exception& e) {
                spdlog::error("Failed to parse line: {}", e.what());
            }
        }

        return messages;
    }

    fs::path testDir;
    std::shared_ptr<DataStore> dataStore;
    std::shared_ptr<EventBus> eventBus;
    std::shared_ptr<DataStoreEventAdapter> adapter;
    std::shared_ptr<SimpleBagWriter> bagWriter;
    std::shared_ptr<DataStoreBagLogger> bagLogger;
};

// Test 1: DataStore 변경 시 Bag 파일에 자동 기록
TEST_F(BagLoggingIntegrationTest, DataStoreChangesAreLoggedToBag) {
    // Given - DataStore에 데이터 설정
    dataStore->set("mission_state", std::string(R"({"state":"IDLE"})"), DataType::MissionState);
    dataStore->set("task_status", std::string(R"({"status":"PENDING"})"), DataType::TaskState);
    dataStore->set("robot_position", std::string(R"({"x":0.0,"y":0.0})"), DataType::Event);

    // When - DataStore 값 변경
    dataStore->set("mission_state", std::string(R"({"state":"RUNNING"})"), DataType::MissionState);
    dataStore->set("task_status", std::string(R"({"status":"ACTIVE"})"), DataType::TaskState);
    dataStore->set("robot_position", std::string(R"({"x":10.5,"y":20.3})"), DataType::Event);

    // EventBus 처리 및 Bag 쓰기 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(bagLogger->flush(5000));

    // Then - Bag 파일 생성 확인
    EXPECT_EQ(countBagFiles(), 1);

    auto filepath = bagLogger->getCurrentFilePath();
    EXPECT_TRUE(fs::exists(filepath));

    // Bag 파일 내용 검증
    auto messages = readBagFile(filepath);
    EXPECT_EQ(messages.size(), 6);  // 3 set + 3 update = 6 messages

    // 첫 번째 메시지 검증 (mission_state)
    EXPECT_EQ(messages[0].topic, "mission_state");
    EXPECT_GT(messages[0].timestamp_ns, 0);

    // 타임스탬프 순서 확인
    for (size_t i = 1; i < messages.size(); i++) {
        EXPECT_GE(messages[i].timestamp_ns, messages[i-1].timestamp_ns);
    }
}

// Test 2: 파일 순환 동작 확인
TEST_F(BagLoggingIntegrationTest, FileRotationWorks) {
    // Given - 작은 파일 크기로 순환 유도 (30KB)
    bagLogger->setRotationPolicy(RotationPolicy::createSizePolicy(0.03));

    // When - 많은 데이터 쓰기 (500 updates)
    for (int i = 0; i < 500; i++) {
        std::string value = R"({"iteration":)" + std::to_string(i) + R"(,"data":"test_data"})";
        dataStore->set("test_key", value, DataType::Event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(bagLogger->flush(5000));

    // Then - 여러 Bag 파일 생성 확인
    int fileCount = countBagFiles();
    auto stats = bagLogger->getStats();

    spdlog::info("파일 순환 테스트: {} 파일 생성, {} 순환", fileCount, stats.rotationCount);

    EXPECT_GT(fileCount, 1);  // 최소 2개 파일
    EXPECT_GT(stats.rotationCount, 0);  // 최소 1번 순환
    EXPECT_EQ(stats.messagesWritten, 500);
}

// Test 3: 보존 정책 동작 확인
TEST_F(BagLoggingIntegrationTest, RetentionPolicyWorks) {
    // Given - 최대 2개 파일만 유지
    bagLogger->setRetentionPolicy(RetentionPolicy::createCountPolicy(2));
    bagLogger->setRotationPolicy(RotationPolicy::createSizePolicy(0.02));  // 20KB

    // When - 많은 데이터 쓰기 (1000 updates → 5번 순환 예상)
    for (int i = 0; i < 1000; i++) {
        std::string value = R"({"iteration":)" + std::to_string(i) + R"(,"data":"test_data_for_retention"})";
        dataStore->set("retention_test", value, DataType::Event);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_TRUE(bagLogger->flush(5000));

    // Then - 최대 2개 파일만 존재 (현재 활성 파일 + 보존된 1개)
    int fileCount = countBagFiles();
    auto stats = bagLogger->getStats();

    spdlog::info("보존 정책 테스트: {} 파일 존재, {} 순환", fileCount, stats.rotationCount);

    EXPECT_LE(fileCount, 3);  // 현재 활성 + 최대 2개
    EXPECT_GT(stats.rotationCount, 2);  // 여러 번 순환
    EXPECT_EQ(stats.messagesWritten, 1000);
}

} // namespace mxrc::integration::logging
