#include "gtest/gtest.h"
#include "core/AsyncWriter.h"
#include "dto/BagMessage.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

class AsyncWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리
        testDir = fs::temp_directory_path() / "mxrc_asyncwriter_test";
        fs::create_directories(testDir);
        testFile = (testDir / "test.bag").string();
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

    fs::path testDir;
    std::string testFile;
};

// Test 1: AsyncWriter 기본 생성 및 시작
TEST_F(AsyncWriterTest, BasicCreationAndStart) {
    // Given
    AsyncWriter writer(testFile, 1000);

    // When
    writer.start();

    // Then
    EXPECT_TRUE(writer.isOpen());
    EXPECT_EQ(writer.queueSize(), 0);
    EXPECT_EQ(writer.getDroppedCount(), 0);
    EXPECT_EQ(writer.getWrittenCount(), 0);

    writer.stop();
}

// Test 2: 비동기 메시지 쓰기
TEST_F(AsyncWriterTest, AsyncMessageWriting) {
    // Given
    AsyncWriter writer(testFile, 1000);
    writer.start();

    // When - 10개 메시지 추가
    for (int i = 0; i < 10; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "test_topic");
        EXPECT_TRUE(writer.tryPush(msg));
    }

    // flush 대기
    EXPECT_TRUE(writer.flush(5000));

    // Then
    EXPECT_EQ(writer.getWrittenCount(), 10);
    EXPECT_EQ(writer.getDroppedCount(), 0);
    EXPECT_GT(writer.getBytesWritten(), 0);

    writer.stop();

    // 파일 검증
    EXPECT_TRUE(fs::exists(testFile));
    std::ifstream ifs(testFile);
    std::string line;
    int lineCount = 0;
    while (std::getline(ifs, line)) {
        lineCount++;
    }
    EXPECT_EQ(lineCount, 10);
}

// Test 3: 큐 오버플로우 드롭 정책
TEST_F(AsyncWriterTest, QueueOverflowDropPolicy) {
    // Given - 작은 큐 용량 (10개)
    AsyncWriter writer(testFile, 10);
    writer.start();

    // When - 큐 용량을 초과하는 메시지 추가 (100개)
    int successCount = 0;
    for (int i = 0; i < 100; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "overflow_test");
        if (writer.tryPush(msg)) {
            successCount++;
        }
        // 빠르게 추가하여 큐 오버플로우 유발
    }

    // Then - 일부 메시지는 드롭되어야 함
    EXPECT_LT(successCount, 100);
    EXPECT_GT(writer.getDroppedCount(), 0);

    writer.flush(5000);
    writer.stop();

    spdlog::info("Success: {}, Dropped: {}, Written: {}",
                 successCount, writer.getDroppedCount(), writer.getWrittenCount());
}

// Test 4: flush 타임아웃 테스트
TEST_F(AsyncWriterTest, FlushTimeout) {
    // Given
    AsyncWriter writer(testFile, 10000);
    writer.start();

    // When - 많은 메시지 추가
    for (int i = 0; i < 100; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "timeout_test");
        writer.tryPush(msg);
    }

    // Then - 매우 짧은 타임아웃으로 flush (실패할 수 있음)
    bool flushed = writer.flush(10);  // 10ms 타임아웃

    // 정상적인 flush로 완료
    EXPECT_TRUE(writer.flush(5000));

    writer.stop();
}

// Test 5: 동시성 테스트 (멀티스레드)
TEST_F(AsyncWriterTest, ConcurrentWriting) {
    // Given
    AsyncWriter writer(testFile, 10000);
    writer.start();

    // When - 여러 스레드에서 동시에 메시지 추가
    const int threadCount = 4;
    const int messagesPerThread = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([&writer, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; i++) {
                auto msg = BagMessage();
                msg.timestamp_ns = 1700000000000000000 + t * 1000 + i;
                msg.topic = "thread_" + std::to_string(t);
                msg.data_type = DataType::TaskState;
                msg.serialized_value = R"({"task_id":"task_1"})";
                writer.tryPush(msg);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // flush 대기
    EXPECT_TRUE(writer.flush(5000));

    // Then
    EXPECT_EQ(writer.getWrittenCount(), threadCount * messagesPerThread);

    writer.stop();
}

// Test 6: 소멸자 안전성 (큐에 남은 메시지 처리)
TEST_F(AsyncWriterTest, DestructorSafety) {
    // Given
    {
        AsyncWriter writer(testFile, 1000);
        writer.start();

        // When - 메시지 추가 후 즉시 소멸 (큐에 남은 메시지 있을 수 있음)
        for (int i = 0; i < 50; i++) {
            auto msg = createTestMessage(1700000000000000000 + i, "destructor_test");
            writer.tryPush(msg);
        }

        // 소멸자 호출 (자동)
    }

    // Then - 파일이 생성되어야 함
    EXPECT_TRUE(fs::exists(testFile));

    // 파일 내용 확인
    std::ifstream ifs(testFile);
    std::string line;
    int lineCount = 0;
    while (std::getline(ifs, line)) {
        lineCount++;
    }
    EXPECT_GT(lineCount, 0);  // 일부 메시지는 쓰여져야 함
}

// Test 7: 파일 열기 실패 처리
TEST_F(AsyncWriterTest, FileOpenFailure) {
    // Given - 존재하지 않는 디렉토리
    std::string invalidPath = "/nonexistent_dir/test.bag";

    // When & Then - 예외 발생
    AsyncWriter writer(invalidPath, 1000);
    EXPECT_THROW(writer.start(), std::runtime_error);
}

// Test 8: 통계 조회
TEST_F(AsyncWriterTest, StatisticsTracking) {
    // Given
    AsyncWriter writer(testFile, 100);
    writer.start();

    // When
    for (int i = 0; i < 10; i++) {
        auto msg = createTestMessage(1700000000000000000 + i, "stats_test");
        writer.tryPush(msg);
    }

    writer.flush(5000);

    // Then
    EXPECT_EQ(writer.getWrittenCount(), 10);
    EXPECT_GT(writer.getBytesWritten(), 100);  // 최소 100 바이트 이상
    EXPECT_EQ(writer.getDroppedCount(), 0);

    writer.stop();
}

} // namespace mxrc::core::logging
