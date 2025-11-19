#include "gtest/gtest.h"
#include "core/logging/core/BagReader.h"
#include "core/logging/core/SimpleBagWriter.h"
#include "core/logging/dto/BagMessage.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

/**
 * @brief BagReader 단위 테스트
 *
 * BagReader의 파일 읽기, 탐색, 필터링 기능을 검증합니다.
 */
class BagReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리
        testDir = fs::temp_directory_path() / "mxrc_bagreader_test";
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
     * 3개의 토픽, 총 10개의 메시지를 포함하는 Bag 파일 생성
     */
    void createTestBagFile() {
        auto writer = std::make_shared<SimpleBagWriter>(
            testDir.string(), "test", 1000);

        writer->start();  // SimpleBagWriter 시작

        uint64_t baseTimestamp = 1700000000000000000;  // 2023-11-14 22:13:20 UTC

        // mission_state: 3 messages
        for (int i = 0; i < 3; i++) {
            BagMessage msg;
            msg.timestamp_ns = baseTimestamp + i * 1000000000;  // 1초 간격
            msg.topic = "mission_state";
            msg.data_type = DataType::MissionState;
            msg.serialized_value = R"({"state":")" + std::to_string(i) + R"("})";
            writer->append(msg);
        }

        // robot_position: 4 messages
        for (int i = 0; i < 4; i++) {
            BagMessage msg;
            msg.timestamp_ns = baseTimestamp + 3000000000 + i * 500000000;  // 0.5초 간격
            msg.topic = "robot_position";
            msg.data_type = DataType::Event;
            msg.serialized_value = R"({"x":)" + std::to_string(i * 10) + R"(,"y":)" + std::to_string(i * 20) + R"(})";
            writer->append(msg);
        }

        // task_status: 3 messages
        for (int i = 0; i < 3; i++) {
            BagMessage msg;
            msg.timestamp_ns = baseTimestamp + 5000000000 + i * 1000000000;  // 1초 간격
            msg.topic = "task_status";
            msg.data_type = DataType::TaskState;
            msg.serialized_value = R"({"status":")" + std::to_string(i) + R"("})";
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
TEST_F(BagReaderTest, OpenBagFile) {
    // Given
    BagReader reader;

    // When
    bool result = reader.open(testBagPath);

    // Then
    EXPECT_TRUE(result);
    EXPECT_TRUE(reader.isOpen());
    EXPECT_EQ(reader.getMessageCount(), 10);  // 총 10개 메시지
    EXPECT_GT(reader.getStartTimestamp(), 0);
    EXPECT_GT(reader.getEndTimestamp(), 0);
    EXPECT_EQ(reader.getFilePath(), testBagPath);
}

// Test 2: 잘못된 파일 열기 시도
TEST_F(BagReaderTest, OpenInvalidFile) {
    // Given
    BagReader reader;

    // When
    bool result = reader.open("/nonexistent/file.bag");

    // Then
    EXPECT_FALSE(result);
    EXPECT_FALSE(reader.isOpen());
}

// Test 3: 순차적 메시지 읽기
TEST_F(BagReaderTest, SequentialRead) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // When - 모든 메시지 읽기
    std::vector<BagMessage> messages;
    while (reader.hasNext()) {
        auto msg = reader.readNext();
        if (msg) {
            messages.push_back(*msg);
        }
    }

    // Then
    EXPECT_EQ(messages.size(), 10);

    // 타임스탬프가 증가하는지 확인
    for (size_t i = 1; i < messages.size(); i++) {
        EXPECT_GE(messages[i].timestamp_ns, messages[i-1].timestamp_ns);
    }
}

// Test 4: 타임스탬프 기반 탐색
TEST_F(BagReaderTest, SeekToTimestamp) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // 첫 번째 메시지 읽기 (타임스탬프 확인)
    auto firstMsg = reader.readNext();
    ASSERT_TRUE(firstMsg.has_value());
    uint64_t firstTimestamp = firstMsg->timestamp_ns;

    // When - 특정 타임스탬프로 이동 (3번째 메시지)
    uint64_t targetTimestamp = firstTimestamp + 3000000000;  // +3초
    bool seekResult = reader.seekToTimestamp(targetTimestamp);

    // Then
    EXPECT_TRUE(seekResult);

    auto msg = reader.readNext();
    ASSERT_TRUE(msg.has_value());
    EXPECT_GE(msg->timestamp_ns, targetTimestamp);
}

// Test 5: 파일 시작 위치로 이동
TEST_F(BagReaderTest, SeekToStart) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // 몇 개 메시지 읽기
    reader.readNext();
    reader.readNext();
    reader.readNext();

    // When - 시작 위치로 이동
    reader.seekToStart();

    // Then
    auto msg = reader.readNext();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->topic, "mission_state");  // 첫 번째 메시지는 mission_state
}

// Test 6: 토픽 필터링
TEST_F(BagReaderTest, TopicFiltering) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // When - robot_position 토픽만 필터링
    reader.setTopicFilter("robot_position");

    std::vector<BagMessage> messages;
    while (reader.hasNext()) {
        auto msg = reader.readNext();
        if (msg) {
            messages.push_back(*msg);
        }
    }

    // Then
    EXPECT_EQ(messages.size(), 4);  // robot_position 메시지만 4개

    for (const auto& msg : messages) {
        EXPECT_EQ(msg.topic, "robot_position");
    }
}

// Test 7: 토픽 필터 제거
TEST_F(BagReaderTest, ClearTopicFilter) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));
    reader.setTopicFilter("mission_state");

    // When - 필터 제거
    reader.clearTopicFilter();

    std::vector<std::string> topics;
    while (reader.hasNext()) {
        auto msg = reader.readNext();
        if (msg) {
            topics.push_back(msg->topic);
        }
    }

    // Then - 모든 토픽이 포함되어야 함
    EXPECT_GT(topics.size(), 3);  // 최소 3개 이상 토픽

    // mission_state, robot_position, task_status가 모두 포함되어야 함
    bool hasMission = false;
    bool hasRobot = false;
    bool hasTask = false;

    for (const auto& topic : topics) {
        if (topic == "mission_state") hasMission = true;
        if (topic == "robot_position") hasRobot = true;
        if (topic == "task_status") hasTask = true;
    }

    EXPECT_TRUE(hasMission);
    EXPECT_TRUE(hasRobot);
    EXPECT_TRUE(hasTask);
}

// Test 8: 메타데이터 조회
TEST_F(BagReaderTest, GetMetadata) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // When
    BagFooter footer = reader.getFooter();
    uint64_t startTs = reader.getStartTimestamp();
    uint64_t endTs = reader.getEndTimestamp();
    size_t count = reader.getMessageCount();

    // Then
    EXPECT_TRUE(footer.isValid());
    EXPECT_TRUE(footer.isSupportedVersion());
    EXPECT_GT(startTs, 0);
    EXPECT_GT(endTs, 0);
    EXPECT_GT(endTs, startTs);  // 종료 타임스탬프가 시작보다 커야 함
    EXPECT_EQ(count, 10);
}

// Test 9: 파일 닫기
TEST_F(BagReaderTest, CloseFile) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));
    ASSERT_TRUE(reader.isOpen());

    // When
    reader.close();

    // Then
    EXPECT_FALSE(reader.isOpen());
    EXPECT_FALSE(reader.hasNext());
}

// Test 10: 재사용 (닫고 다시 열기)
TEST_F(BagReaderTest, ReuseReader) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // 몇 개 메시지 읽기
    reader.readNext();
    reader.readNext();

    // When - 닫고 다시 열기
    reader.close();
    bool reopened = reader.open(testBagPath);

    // Then
    EXPECT_TRUE(reopened);
    EXPECT_TRUE(reader.isOpen());

    // 다시 처음부터 읽기 가능
    auto msg = reader.readNext();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->topic, "mission_state");  // 첫 번째 메시지
}

// Test 11: hasNext() 경계 조건
TEST_F(BagReaderTest, HasNextBoundary) {
    // Given
    BagReader reader;
    ASSERT_TRUE(reader.open(testBagPath));

    // When - 모든 메시지 읽기
    int count = 0;
    while (reader.hasNext()) {
        auto msg = reader.readNext();
        if (msg) {
            count++;
        }
    }

    // Then
    EXPECT_EQ(count, 10);
    EXPECT_FALSE(reader.hasNext());  // 모두 읽은 후 false
}

// Test 12: 빈 Bag 파일 (메시지 0개)
TEST_F(BagReaderTest, EmptyBagFile) {
    // Given - 빈 Bag 파일 생성
    auto writer = std::make_shared<SimpleBagWriter>(
        testDir.string(), "empty", 1000);
    writer->start();  // SimpleBagWriter 시작
    writer->flush(1000);
    writer->close();
    std::string emptyBagPath = writer->getCurrentFilePath();

    // When
    BagReader reader;
    bool opened = reader.open(emptyBagPath);

    // Then
    EXPECT_TRUE(opened);
    EXPECT_EQ(reader.getMessageCount(), 0);
    EXPECT_FALSE(reader.hasNext());
}

} // namespace mxrc::core::logging
