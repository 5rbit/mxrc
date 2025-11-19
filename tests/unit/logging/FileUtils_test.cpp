#include "gtest/gtest.h"
#include "util/FileUtils.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

class FileUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리 생성
        testDir = fs::temp_directory_path() / "mxrc_fileutils_test";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        // 테스트 후 정리
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
};

// Test 1: 파일 존재 여부 확인
TEST_F(FileUtilsTest, FileExists) {
    // Given - 파일 생성
    auto testFile = testDir / "test.txt";
    std::ofstream ofs(testFile);
    ofs << "test content";
    ofs.close();

    // When & Then
    EXPECT_TRUE(FileUtils::fileExists(testFile.string()));
    EXPECT_FALSE(FileUtils::fileExists((testDir / "nonexistent.txt").string()));
}

// Test 2: 디렉토리 존재 여부 및 생성
TEST_F(FileUtilsTest, DirectoryCreation) {
    // Given
    auto newDir = testDir / "subdir" / "nested";

    // When
    bool created = FileUtils::createDirectories(newDir.string());

    // Then
    EXPECT_TRUE(created);
    EXPECT_TRUE(FileUtils::directoryExists(newDir.string()));
}

// Test 3: 파일 크기 조회
TEST_F(FileUtilsTest, GetFileSize) {
    // Given
    auto testFile = testDir / "sizefile.txt";
    std::ofstream ofs(testFile);
    std::string content(1024, 'A');  // 1KB 파일
    ofs << content;
    ofs.close();

    // When
    uint64_t size = FileUtils::getFileSize(testFile.string());

    // Then
    EXPECT_EQ(size, 1024);
}

// Test 4: 사용 가능한 디스크 공간 확인
TEST_F(FileUtilsTest, GetAvailableSpace) {
    // When
    uint64_t space = FileUtils::getAvailableSpace(testDir.string());

    // Then - 최소한 1MB 이상은 있어야 함
    EXPECT_GT(space, 1024 * 1024);
}

// Test 5: 디스크 공간 부족 시뮬레이션
TEST_F(FileUtilsTest, DiskSpaceInsufficient) {
    // Given
    uint64_t availableSpace = FileUtils::getAvailableSpace(testDir.string());

    // When & Then
    // 사용 가능한 공간보다 많이 요구하면 true
    EXPECT_TRUE(FileUtils::isDiskSpaceInsufficient(testDir.string(), availableSpace + 1));

    // 사용 가능한 공간보다 적게 요구하면 false
    EXPECT_FALSE(FileUtils::isDiskSpaceInsufficient(testDir.string(), 1024));
}

// Test 6: 파일 목록 조회
TEST_F(FileUtilsTest, ListFiles) {
    // Given - 여러 파일 생성
    std::vector<std::string> filenames = {"file1.bag", "file2.bag", "file3.txt"};
    for (const auto& name : filenames) {
        std::ofstream ofs(testDir / name);
        ofs << "content";
        ofs.close();
        // 파일 생성 시간 차이를 위해 약간 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // When - .bag 파일만 조회
    auto bagFiles = FileUtils::listFiles(testDir.string(), "*.bag");

    // Then
    EXPECT_EQ(bagFiles.size(), 2);

    // 모든 파일 조회
    auto allFiles = FileUtils::listFiles(testDir.string(), "*");
    EXPECT_EQ(allFiles.size(), 3);
}

// Test 7: 파일 삭제
TEST_F(FileUtilsTest, DeleteFile) {
    // Given
    auto testFile = testDir / "deleteme.txt";
    std::ofstream ofs(testFile);
    ofs << "delete this";
    ofs.close();

    EXPECT_TRUE(FileUtils::fileExists(testFile.string()));

    // When
    bool deleted = FileUtils::deleteFile(testFile.string());

    // Then
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(FileUtils::fileExists(testFile.string()));
}

// Test 8: 파일 최종 수정 시간 조회
TEST_F(FileUtilsTest, GetLastModifiedTime) {
    // Given
    auto testFile = testDir / "timefile.txt";
    std::ofstream ofs(testFile);
    ofs << "timestamp test";
    ofs.close();

    // When
    uint64_t timestamp = FileUtils::getLastModifiedTime(testFile.string());

    // Then - 현재 시간과 가까워야 함 (10초 이내)
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    EXPECT_LT(std::abs(static_cast<int64_t>(timestamp - now)), 10);
}

// Test 9: 타임스탬프 파일명 생성
TEST_F(FileUtilsTest, GenerateTimestampedFilename) {
    // When
    std::string filename = FileUtils::generateTimestampedFilename("mission", ".bag");

    // Then - 형식 확인: mission_YYYY-MM-DD_HH-MM-SS.bag
    EXPECT_TRUE(filename.find("mission_") == 0);
    EXPECT_TRUE(filename.find(".bag") != std::string::npos);
    EXPECT_GT(filename.length(), 20);  // 최소 길이 확인
}

// Test 10: 존재하지 않는 파일 크기 조회 시 예외
TEST_F(FileUtilsTest, GetFileSizeThrowsOnNonexistent) {
    // Given
    auto nonexistent = testDir / "nonexistent.txt";

    // When & Then
    EXPECT_THROW(FileUtils::getFileSize(nonexistent.string()), fs::filesystem_error);
}

// Test 11: 파일 목록 정렬 확인 (오래된 파일이 먼저)
TEST_F(FileUtilsTest, ListFilesSortedByTime) {
    // Given - 시간 차이를 두고 파일 생성
    auto file1 = testDir / "old.txt";
    auto file2 = testDir / "new.txt";

    std::ofstream ofs1(file1);
    ofs1 << "old";
    ofs1.close();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::ofstream ofs2(file2);
    ofs2 << "new";
    ofs2.close();

    // When
    auto files = FileUtils::listFiles(testDir.string(), "*");

    // Then - 첫 번째 파일이 더 오래된 파일이어야 함
    ASSERT_EQ(files.size(), 2);
    EXPECT_TRUE(files[0].find("old.txt") != std::string::npos);
    EXPECT_TRUE(files[1].find("new.txt") != std::string::npos);
}

// Test 12: 빈 디렉토리 파일 목록 조회
TEST_F(FileUtilsTest, ListFilesEmptyDirectory) {
    // Given - 빈 디렉토리
    auto emptyDir = testDir / "empty";
    fs::create_directories(emptyDir);

    // When
    auto files = FileUtils::listFiles(emptyDir.string(), "*");

    // Then
    EXPECT_TRUE(files.empty());
}

// Test 13: 존재하지 않는 디렉토리 파일 목록 조회
TEST_F(FileUtilsTest, ListFilesNonexistentDirectory) {
    // Given
    auto nonexistentDir = testDir / "nonexistent";

    // When
    auto files = FileUtils::listFiles(nonexistentDir.string(), "*");

    // Then - 경고 로그 출력 및 빈 벡터 반환
    EXPECT_TRUE(files.empty());
}

} // namespace mxrc::core::logging
