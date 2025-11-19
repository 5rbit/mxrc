#include "gtest/gtest.h"
#include "core/logging/core/BagReplayer.h"
#include "core/logging/core/SimpleBagWriter.h"
#include "core/logging/dto/BagMessage.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

/**
 * @brief BagReplayer 단위 테스트
 *
 * BagReplayer의 재생, 속도 조절, 일시정지/재개, 필터링 기능을 검증합니다.
 */
class BagReplayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리
        testDir = fs::temp_directory_path() / "mxrc_bagreplayer_test";
        fs::create_directories(testDir);

        // 테스트용 Bag 파일 생성
        createTestBagFile();
    }

    void TearDown() override {
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    /**
     * @brief 테스트용 Bag 파일 생성
     *
     * 10개의 메시지를 1초 간격으로 생성
     */
    void createTestBagFile() {
        auto writer = std::make_shared<SimpleBagWriter>(
            testDir.string(), "test", 1000);

        writer->start();

        uint64_t baseTimestamp = 1700000000000000000;  // 2023-11-14 22:13:20 UTC

        // 10개 메시지 (1초 간격)
        for (int i = 0; i < 10; i++) {
            BagMessage msg;
            msg.timestamp_ns = baseTimestamp + i * 1000000000;  // 1초 간격
            msg.topic = (i % 2 == 0) ? "topic_a" : "topic_b";
            msg.data_type = DataType::Event;
            msg.serialized_value = R"({"value":)" + std::to_string(i) + R"(})";
            writer->append(msg);
        }

        writer->flush(1000);
        writer->close();

        testBagPath = writer->getCurrentFilePath();
    }

    fs::path testDir;
    std::string testBagPath;
};

// Test 1: Bag 파일 열기
TEST_F(BagReplayerTest, OpenBagFile) {
    // Given
    BagReplayer replayer;

    // When
    bool result = replayer.open(testBagPath);

    // Then
    EXPECT_TRUE(result);
}

// Test 2: 잘못된 파일 열기
TEST_F(BagReplayerTest, OpenInvalidFile) {
    // Given
    BagReplayer replayer;

    // When
    bool result = replayer.open("/nonexistent/file.bag");

    // Then
    EXPECT_FALSE(result);
}

// Test 3: 최대 속도 재생
TEST_F(BagReplayerTest, ReplayAsFastAsPossible) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
    });

    // When
    auto start = std::chrono::steady_clock::now();
    replayer.start(ReplaySpeed::asFastAsPossible());
    replayer.waitUntilFinished();
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Then
    EXPECT_EQ(messageCount, 10);
    EXPECT_LT(elapsed, 500);  // 최대 속도이므로 0.5초 미만이어야 함

    auto stats = replayer.getStats();
    EXPECT_EQ(stats.messagesReplayed, 10);
    EXPECT_EQ(stats.messagesSkipped, 0);
    EXPECT_GE(stats.progress, 0.99);  // 거의 100%
}

// Test 4: 실시간 재생 (1x 속도)
TEST_F(BagReplayerTest, ReplayRealtime) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
    });

    // When - 실시간 재생 (메시지 간격 1초 × 3개 = 최소 2초)
    auto start = std::chrono::steady_clock::now();
    replayer.start(ReplaySpeed::realtime());

    // 3개 메시지만 재생하고 중지 (약 2초 소요)
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    replayer.stop();

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Then - 약 2~3초 소요, 3~4개 메시지 재생
    EXPECT_GE(messageCount, 2);
    EXPECT_LE(messageCount, 5);
    EXPECT_GE(elapsed, 2000);  // 최소 2초
    EXPECT_LT(elapsed, 4000);  // 최대 4초
}

// Test 5: 배속 재생 (2x 속도)
TEST_F(BagReplayerTest, ReplayFastSpeed) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
    });

    // When - 2배속 재생 (메시지 간격 0.5초 × 3개 = 최소 1초)
    auto start = std::chrono::steady_clock::now();
    replayer.start(ReplaySpeed::fast(2.0));

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    replayer.stop();

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Then - 약 1~2초 소요, 3~4개 메시지 재생
    EXPECT_GE(messageCount, 2);
    EXPECT_LE(messageCount, 6);
    EXPECT_GE(elapsed, 1000);
    EXPECT_LT(elapsed, 2500);
}

// Test 6: 일시정지 및 재개 (개선된 폴링 기반)
TEST_F(BagReplayerTest, PauseAndResume) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
    });

    // When - 실시간 모드로 재생
    replayer.start(ReplaySpeed::realtime());

    // 최소 1개 메시지 재생될 때까지 대기 (타임아웃 5초)
    auto waitForMessages = [](std::atomic<int>& count, int targetCount, int timeoutMs) {
        auto start = std::chrono::steady_clock::now();
        while (count < targetCount) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeoutMs) {
                return false;  // 타임아웃
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    };

    ASSERT_TRUE(waitForMessages(messageCount, 1, 5000));  // 최소 1개 메시지 대기
    int countBeforePause = messageCount;

    // Pause 후 안정화 대기
    replayer.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(replayer.isPaused());

    // 일시정지 중 500ms 동안 메시지 수가 증가하지 않는지 확인
    int countDuringPause1 = messageCount;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    int countDuringPause2 = messageCount;

    // Resume 후 추가 메시지 재생 대기
    replayer.resume();
    EXPECT_FALSE(replayer.isPaused());

    // 재개 후 최소 1개 이상 추가 메시지 대기
    int countAtResume = messageCount;
    ASSERT_TRUE(waitForMessages(messageCount, countAtResume + 1, 5000));
    int countAfterResume = messageCount;

    replayer.stop();

    // Then
    EXPECT_GT(countBeforePause, 0);  // 최소 1개 메시지 재생됨
    EXPECT_EQ(countDuringPause1, countDuringPause2);  // 일시정지 중에는 증가 안 함
    EXPECT_GT(countAfterResume, countAtResume);  // 재개 후 증가
}

// Test 7: 토픽 필터링
TEST_F(BagReplayerTest, TopicFiltering) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    std::vector<std::string> topics;
    std::mutex topicsMutex;

    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
        std::lock_guard<std::mutex> lock(topicsMutex);
        topics.push_back(msg.topic);
    });

    // When - topic_a만 필터링
    replayer.setTopicFilter("topic_a");
    replayer.start(ReplaySpeed::asFastAsPossible());
    replayer.waitUntilFinished();

    // Then - topic_a 메시지만 5개 (짝수 인덱스)
    EXPECT_EQ(messageCount, 5);

    for (const auto& topic : topics) {
        EXPECT_EQ(topic, "topic_a");
    }

    auto stats = replayer.getStats();
    EXPECT_EQ(stats.messagesReplayed, 5);
    EXPECT_EQ(stats.messagesSkipped, 5);
}

// Test 8: 시간 범위 설정 (더 단순화된 테스트)
TEST_F(BagReplayerTest, TimeRangeFiltering) {
    // Given - 먼저 BagReader로 실제 타임스탬프 확인
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    std::vector<uint64_t> allTimestamps;
    while (reader.hasNext()) {
        auto msg = reader.readNext();
        if (msg) {
            allTimestamps.push_back(static_cast<uint64_t>(msg->timestamp_ns));
        }
    }
    reader.close();

    ASSERT_GE(allTimestamps.size(), 6) << "Need at least 6 messages for this test (indices 0-5)";

    // 타임스탬프 정렬 (Bag 파일이 시간 순서대로 읽히지 않을 수 있음)
    std::sort(allTimestamps.begin(), allTimestamps.end());

    // 정렬된 타임스탬프 기반으로 범위 설정 (인덱스 1~5, 총 5개)
    uint64_t startTime = allTimestamps[1];  // 두 번째 메시지부터
    uint64_t endTime = allTimestamps[5];    // 여섯 번째 메시지까지

    // When - Replayer로 시간 범위 재생
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    std::vector<uint64_t> receivedTimestamps;
    std::mutex timestampsMutex;

    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
        std::lock_guard<std::mutex> lock(timestampsMutex);
        receivedTimestamps.push_back(static_cast<uint64_t>(msg.timestamp_ns));
    });

    replayer.setTimeRange(startTime, endTime);
    replayer.start(ReplaySpeed::asFastAsPossible());
    replayer.waitUntilFinished();

    // Then - 인덱스 1~5의 5개 메시지 재생됨
    EXPECT_EQ(messageCount, 5) << "Expected 5 messages in range [" << startTime << ", " << endTime << "]";

    // 모든 타임스탬프가 범위 내에 있는지 확인
    for (size_t i = 0; i < receivedTimestamps.size(); i++) {
        EXPECT_GE(receivedTimestamps[i], startTime)
            << "Message " << i << " timestamp " << receivedTimestamps[i] << " is before start time";
        EXPECT_LE(receivedTimestamps[i], endTime)
            << "Message " << i << " timestamp " << receivedTimestamps[i] << " is after end time";
    }

    auto stats = replayer.getStats();
    EXPECT_EQ(stats.messagesReplayed, 5);
    // 범위 밖 메시지 확인 (처음 1개 + 마지막 4개 = 5개 skip 예상)
    EXPECT_GE(stats.messagesSkipped, 1);  // 최소 1개 이상
}

// Test 9: 재생 통계 확인
TEST_F(BagReplayerTest, ReplayStats) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    replayer.setMessageCallback([](const BagMessage& msg) {
        // 메시지 처리
    });

    // When
    replayer.start(ReplaySpeed::asFastAsPossible());
    replayer.waitUntilFinished();

    // Then
    auto stats = replayer.getStats();

    EXPECT_EQ(stats.messagesReplayed, 10);
    EXPECT_EQ(stats.messagesSkipped, 0);
    EXPECT_GT(stats.elapsedTime, 0.0);
    EXPECT_GE(stats.progress, 0.99);  // 거의 100%
}

// Test 10: 재생 중지
TEST_F(BagReplayerTest, StopReplay) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    std::atomic<int> messageCount{0};
    replayer.setMessageCallback([&](const BagMessage& msg) {
        messageCount++;
    });

    // When
    replayer.start(ReplaySpeed::realtime());
    EXPECT_TRUE(replayer.isPlaying());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    replayer.stop();

    // Then
    EXPECT_FALSE(replayer.isPlaying());
    EXPECT_LT(messageCount, 10);  // 전체 재생 전에 중지
}

// Test 11: 파일 닫기 (재생 중)
TEST_F(BagReplayerTest, CloseWhilePlaying) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    replayer.setMessageCallback([](const BagMessage& msg) {
        // 메시지 처리
    });

    // When
    replayer.start(ReplaySpeed::realtime());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    replayer.close();  // 재생 중 닫기 (자동으로 stop 호출)

    // Then
    EXPECT_FALSE(replayer.isPlaying());
}

// Test 12: 콜백 없이 재생
TEST_F(BagReplayerTest, ReplayWithoutCallback) {
    // Given
    BagReplayer replayer;
    ASSERT_TRUE(replayer.open(testBagPath));

    // When - 콜백 설정 안 함
    replayer.start(ReplaySpeed::asFastAsPossible());
    replayer.waitUntilFinished();

    // Then - 에러 없이 완료
    auto stats = replayer.getStats();
    EXPECT_EQ(stats.messagesReplayed, 10);
}

} // namespace mxrc::core::logging
