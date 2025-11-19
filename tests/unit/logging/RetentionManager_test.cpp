#include "gtest/gtest.h"
#include "util/RetentionManager.h"
#include "dto/RetentionPolicy.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

class RetentionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "mxrc_retention_test";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    void createTestBagFile(const std::string& filename, size_t sizeKB = 1) {
        auto filepath = testDir / filename;
        std::ofstream ofs(filepath);
        std::string content(sizeKB * 1024, 'A');
        ofs << content;
        ofs.close();
    }

    void setFileTime(const std::string& filename, int daysAgo) {
        auto filepath = testDir / filename;
        auto now = std::chrono::system_clock::now();
        auto targetTime = now - std::chrono::hours(daysAgo * 24);

        fs::last_write_time(filepath,
            fs::file_time_type::clock::now() -
            (std::chrono::system_clock::now() - targetTime));
    }

    fs::path testDir;
};

// Test 1: TIME 기반 보존 정책 - 7일 보존
TEST_F(RetentionManagerTest, TimeBasedRetention7Days) {
    // Given - TIME 정책 (7일)
    RetentionPolicy policy = RetentionPolicy::createTimePolicy(7);
    RetentionManager manager(testDir.string(), policy);

    // 5일 전, 10일 전 파일 생성
    createTestBagFile("recent.bag");
    createTestBagFile("old.bag");

    setFileTime("recent.bag", 5);  // 5일 전
    setFileTime("old.bag", 10);    // 10일 전

    // When
    size_t deleted = manager.deleteOldFiles();

    // Then - 10일 전 파일만 삭제되어야 함
    EXPECT_EQ(deleted, 1);
    EXPECT_TRUE(fs::exists(testDir / "recent.bag"));
    EXPECT_FALSE(fs::exists(testDir / "old.bag"));
}

// Test 2: COUNT 기반 보존 정책 - 최대 3개
TEST_F(RetentionManagerTest, CountBasedRetentionMax3) {
    // Given - COUNT 정책 (최대 3개)
    RetentionPolicy policy = RetentionPolicy::createCountPolicy(3);
    RetentionManager manager(testDir.string(), policy);

    // 5개 파일 생성 (시간 차이)
    for (int i = 0; i < 5; i++) {
        createTestBagFile("file" + std::to_string(i) + ".bag");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // When
    size_t deleted = manager.deleteOldFiles();

    // Then - 가장 오래된 2개 파일이 삭제되어야 함
    EXPECT_EQ(deleted, 2);

    auto remaining = manager.listBagFiles();
    EXPECT_EQ(remaining.size(), 3);
}

// Test 3: 긴급 삭제 (디스크 공간 부족)
TEST_F(RetentionManagerTest, EmergencyDeleteOldest) {
    // Given
    RetentionManager manager(testDir.string());

    // 10개 파일 생성 (각 100KB)
    for (int i = 0; i < 10; i++) {
        createTestBagFile("bag" + std::to_string(i) + ".bag", 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // When - 500KB 공간 필요 (약 5개 파일 크기)
    size_t deleted = manager.emergencyDeleteOldest(500 * 1024);

    // Then - 최소 5개 파일이 삭제되어야 함
    EXPECT_GE(deleted, 5);

    auto remaining = manager.listBagFiles();
    EXPECT_LE(remaining.size(), 5);
}

// Test 4: Bag 파일 목록 조회
TEST_F(RetentionManagerTest, ListBagFiles) {
    // Given
    RetentionManager manager(testDir.string());

    // Bag 파일과 다른 파일 생성
    createTestBagFile("file1.bag");
    createTestBagFile("file2.bag");

    // .txt 파일 (무시되어야 함)
    auto txtFile = testDir / "other.txt";
    std::ofstream ofs(txtFile);
    ofs << "not a bag file";
    ofs.close();

    // When
    auto bagFiles = manager.listBagFiles();

    // Then - .bag 파일만 조회
    EXPECT_EQ(bagFiles.size(), 2);
}

// Test 5: 총 Bag 파일 크기 조회
TEST_F(RetentionManagerTest, GetTotalSize) {
    // Given
    RetentionManager manager(testDir.string());

    // 각 100KB 파일 3개
    createTestBagFile("file1.bag", 100);
    createTestBagFile("file2.bag", 100);
    createTestBagFile("file3.bag", 100);

    // When
    uint64_t totalSize = manager.getTotalSize();

    // Then - 약 300KB
    EXPECT_GE(totalSize, 300 * 1024);
    EXPECT_LE(totalSize, 310 * 1024);  // 약간의 여유
}

// Test 6: 디스크 공간 확보
TEST_F(RetentionManagerTest, EnsureDiskSpace) {
    // Given
    RetentionManager manager(testDir.string());

    // 작은 파일들 생성
    for (int i = 0; i < 5; i++) {
        createTestBagFile("small" + std::to_string(i) + ".bag", 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // When - 작은 공간 요구 (항상 성공해야 함)
    bool ensured = manager.ensureDiskSpace(1024);  // 1KB

    // Then
    EXPECT_TRUE(ensured);
}

// Test 7: 빈 디렉토리 처리
TEST_F(RetentionManagerTest, EmptyDirectory) {
    // Given - 빈 디렉토리
    RetentionManager manager(testDir.string());

    // When
    size_t deleted = manager.deleteOldFiles();
    auto files = manager.listBagFiles();
    uint64_t totalSize = manager.getTotalSize();

    // Then
    EXPECT_EQ(deleted, 0);
    EXPECT_EQ(files.size(), 0);
    EXPECT_EQ(totalSize, 0);
}

// Test 8: 정책 변경
TEST_F(RetentionManagerTest, PolicyChange) {
    // Given
    RetentionPolicy policy1 = RetentionPolicy::createCountPolicy(5);
    RetentionManager manager(testDir.string(), policy1);

    // 10개 파일 생성
    for (int i = 0; i < 10; i++) {
        createTestBagFile("file" + std::to_string(i) + ".bag");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // When - COUNT 5 정책
    size_t deleted1 = manager.deleteOldFiles();
    EXPECT_EQ(deleted1, 5);

    // 정책 변경 - COUNT 3
    RetentionPolicy policy2 = RetentionPolicy::createCountPolicy(3);
    manager.setPolicy(policy2);

    // When - COUNT 3 정책
    size_t deleted2 = manager.deleteOldFiles();

    // Then - 추가로 2개 삭제
    EXPECT_EQ(deleted2, 2);

    auto remaining = manager.listBagFiles();
    EXPECT_EQ(remaining.size(), 3);
}

} // namespace mxrc::core::logging
