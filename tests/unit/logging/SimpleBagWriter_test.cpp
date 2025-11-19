#include "gtest/gtest.h"
#include "core/SimpleBagWriter.h"
#include "dto/BagMessage.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

class SimpleBagWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리
        testDir = fs::temp_directory_path() / "mxrc_simplebagwriter_test";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        // 정리
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    BagMessage createTestMessage(int64_t timestamp, const std::string& topic) {
        BagMessage msg;
        msg.timestamp_ns = timestamp;
        msg.topic = topic;
        msg.data_type = DataType::MissionState;
        msg.serialized_value = R"({"state":"RUNNING"})";
        return msg;
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
};

// Test 1: SimpleBagWriter 생성 및 시작
TEST_F(SimpleBagWriterTest, BasicCreationAndStart) {
    // Given
    SimpleBagWriter writer(testDir.string(), "test", 1000);

    // When
    writer.start();

    // Then
    EXPECT_TRUE(writer.isOpen());
    EXPECT_FALSE(writer.getCurrentFilePath().empty());
    EXPECT_TRUE(fs::exists(writer.getCurrentFilePath()));

    writer.stop();
}

// Test 2: 비동기 메시지 쓰기
TEST_F(SimpleBagWriterTest, AsyncMessageWriting) {
    // Given
    SimpleBagWriter writer(testDir.string(), "test", 1000);
    writer.start();

    // When - 10개 메시지 추가
    for (int i = 0; i < 10; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "test_topic");
        EXPECT_TRUE(writer.appendAsync(msg));
    }

    // flush 대기
    EXPECT_TRUE(writer.flush(5000));

    // Then
    auto stats = writer.getStats();
    EXPECT_EQ(stats.messagesWritten, 10);
    EXPECT_EQ(stats.messagesDropped, 0);
    EXPECT_GT(stats.bytesWritten, 0);

    writer.stop();

    // 파일 검증
    std::string filepath = writer.getCurrentFilePath();
    EXPECT_TRUE(fs::exists(filepath));

    std::ifstream ifs(filepath);
    std::string line;
    int lineCount = 0;
    while (std::getline(ifs, line)) {
        lineCount++;
    }
    EXPECT_EQ(lineCount, 10);
}

// Test 3: 동기 메시지 쓰기
TEST_F(SimpleBagWriterTest, SyncMessageWriting) {
    // Given
    SimpleBagWriter writer(testDir.string(), "test", 1000);
    writer.start();

    // When - 5개 메시지 동기 쓰기
    for (int i = 0; i < 5; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "sync_topic");
        EXPECT_TRUE(writer.append(msg));
    }

    // Then
    auto stats = writer.getStats();
    EXPECT_EQ(stats.messagesWritten, 5);

    writer.stop();
}

// Test 4: SIZE 기반 파일 순환
TEST_F(SimpleBagWriterTest, SizeBasedRotation) {
    // Given - 작은 파일 크기로 순환 유도 (50KB)
    SimpleBagWriter writer(testDir.string(), "test", 10000);
    writer.setRotationPolicy(RotationPolicy::createSizePolicy(0.05));  // 50KB = 0.05MB

    writer.start();

    auto startTime = std::chrono::steady_clock::now();

    // When - 메시지 쓰기 (각 메시지 ~108 bytes, 1000개 = ~108KB → 2번 순환 예상)
    const int messageCount = 1000;
    for (int i = 0; i < messageCount; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "rotation_test");
        writer.appendAsync(msg);
    }

    // flush 대기
    auto flushStart = std::chrono::steady_clock::now();
    EXPECT_TRUE(writer.flush(5000));  // 5초 타임아웃
    auto flushEnd = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto endTime = std::chrono::steady_clock::now();

    // 시간 측정
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    auto flushMs = std::chrono::duration_cast<std::chrono::milliseconds>(flushEnd - flushStart).count();

    // Then - 파일이 순환되었는지 확인
    auto stats = writer.getStats();
    int fileCount = countBagFiles();

    spdlog::info("SIZE 기반 순환 테스트: {} 메시지, 총 {}ms, flush {}ms, 순환 {}회, 파일 {}개",
                 messageCount, totalMs, flushMs, stats.rotationCount, fileCount);

    EXPECT_GT(stats.rotationCount, 0);  // 최소 1번 순환
    EXPECT_GT(fileCount, 1);            // 여러 파일 생성
    EXPECT_EQ(stats.messagesWritten, messageCount);

    writer.stop();
}

// Test 5: TIME 기반 파일 순환
TEST_F(SimpleBagWriterTest, TimeBasedRotation) {
    // Given - 1초마다 순환
    SimpleBagWriter writer(testDir.string(), "test", 10000);
    writer.setRotationPolicy(RotationPolicy::createTimePolicy(1));  // 1초

    writer.start();

    auto startTime = std::chrono::steady_clock::now();

    // When - 첫 번째 메시지 배치 쓰기 (500개)
    for (int i = 0; i < 500; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "time_test");
        writer.appendAsync(msg);
    }

    writer.flush(5000);

    // 2초 대기 (순환 유발)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 두 번째 메시지 배치 쓰기 (500개, 순환 발생)
    for (int i = 500; i < 1000; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "time_test");
        writer.appendAsync(msg);
    }

    writer.flush(5000);

    auto endTime = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Then
    auto stats = writer.getStats();
    int fileCount = countBagFiles();

    spdlog::info("TIME 기반 순환 테스트: 총 {}ms, 순환 {}회, 파일 {}개",
                 totalMs, stats.rotationCount, fileCount);

    EXPECT_GT(stats.rotationCount, 0);
    EXPECT_GT(fileCount, 1);

    writer.stop();
}

// Test 6: COUNT 기반 보존 정책
TEST_F(SimpleBagWriterTest, CountBasedRetention) {
    // Given - 최대 3개 파일만 유지
    SimpleBagWriter writer(testDir.string(), "test", 10000);
    writer.setRetentionPolicy(RetentionPolicy::createCountPolicy(3));
    writer.setRotationPolicy(RotationPolicy::createSizePolicy(0.02));  // 20KB (더 빠른 순환)

    writer.start();

    auto startTime = std::chrono::steady_clock::now();

    // When - 메시지 쓰기 (2000개 = ~216KB → 10번 순환 예상)
    const int messageCount = 2000;
    for (int i = 0; i < messageCount; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "retention_test");
        writer.appendAsync(msg);
    }

    auto flushStart = std::chrono::steady_clock::now();
    EXPECT_TRUE(writer.flush(10000));
    auto flushEnd = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto endTime = std::chrono::steady_clock::now();

    // 시간 측정
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    auto flushMs = std::chrono::duration_cast<std::chrono::milliseconds>(flushEnd - flushStart).count();

    // Then - 최대 3개 파일만 존재
    auto stats = writer.getStats();
    int fileCount = countBagFiles();

    spdlog::info("COUNT 기반 보존 테스트: {} 메시지, 총 {}ms, flush {}ms, 순환 {}회, 파일 {}개",
                 messageCount, totalMs, flushMs, stats.rotationCount, fileCount);

    EXPECT_LE(fileCount, 4);  // 현재 활성 파일 + 최대 3개
    EXPECT_EQ(stats.messagesWritten, messageCount);

    writer.stop();
}

// Test 7: 통계 조회
TEST_F(SimpleBagWriterTest, StatisticsTracking) {
    // Given
    SimpleBagWriter writer(testDir.string(), "test", 100);
    writer.start();

    // When
    for (int i = 0; i < 20; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "stats_test");
        writer.appendAsync(msg);
    }

    writer.flush(5000);

    // Then
    auto stats = writer.getStats();
    EXPECT_EQ(stats.messagesWritten, 20);
    EXPECT_GT(stats.bytesWritten, 100);  // 최소 100 바이트
    EXPECT_FALSE(stats.currentFilePath.empty());
    EXPECT_GT(stats.currentFileSize, 0);

    writer.stop();
}

// Test 8: 소멸자 안전성
TEST_F(SimpleBagWriterTest, DestructorSafety) {
    // Given
    std::string filepath;
    {
        SimpleBagWriter writer(testDir.string(), "test", 1000);
        writer.start();

        // When - 메시지 추가 후 즉시 소멸
        for (int i = 0; i < 50; i++) {
            auto msg = createTestMessage(1700000000000000000 + i, "destructor_test");
            writer.appendAsync(msg);
        }

        filepath = writer.getCurrentFilePath();

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

} // namespace mxrc::core::logging
