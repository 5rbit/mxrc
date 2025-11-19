#include "gtest/gtest.h"
#include "util/Indexer.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace mxrc::core::logging {

class IndexerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "mxrc_indexer_test";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
};

// Test 1: 인덱스 엔트리 추가 및 조회
TEST_F(IndexerTest, AddAndRetrieveEntries) {
    // Given
    Indexer indexer;

    // When - 엔트리 추가
    indexer.addEntry(1000, 0);
    indexer.addEntry(2000, 128);
    indexer.addEntry(3000, 256);

    // Then
    EXPECT_EQ(indexer.size(), 3);
    EXPECT_FALSE(indexer.empty());

    auto entries = indexer.getEntries();
    EXPECT_EQ(entries[0].timestamp_ns, 1000);
    EXPECT_EQ(entries[0].file_offset, 0);
    EXPECT_EQ(entries[1].timestamp_ns, 2000);
    EXPECT_EQ(entries[1].file_offset, 128);
}

// Test 2: 파일 쓰기 및 읽기
TEST_F(IndexerTest, WriteAndReadFromFile) {
    // Given - 인덱스 생성
    Indexer writer;
    writer.addEntry(1700000000000000000, 0);
    writer.addEntry(1700000001000000000, 512);
    writer.addEntry(1700000002000000000, 1024);

    std::string filepath = (testDir / "test.bag").string();

    // When - 파일에 쓰기
    {
        std::ofstream ofs(filepath, std::ios::binary);
        ASSERT_TRUE(ofs.is_open());

        // 가상의 데이터 영역 (100 bytes)
        std::vector<char> dummyData(100, 'X');
        ofs.write(dummyData.data(), dummyData.size());

        uint64_t dataSize = ofs.tellp();
        EXPECT_TRUE(writer.writeToFile(ofs, dataSize));
    }

    // Then - 파일에서 읽기
    Indexer reader;
    BagFooter footer = reader.readFromFile(filepath);

    EXPECT_TRUE(footer.isValid());
    EXPECT_TRUE(footer.isSupportedVersion());
    EXPECT_EQ(footer.data_size, 100);
    EXPECT_EQ(footer.index_count, 3);

    EXPECT_EQ(reader.size(), 3);
    auto entries = reader.getEntries();
    EXPECT_EQ(entries[0].timestamp_ns, 1700000000000000000);
    EXPECT_EQ(entries[1].timestamp_ns, 1700000001000000000);
    EXPECT_EQ(entries[2].timestamp_ns, 1700000002000000000);
}

// Test 3: 이진 탐색 - 정확한 타임스탬프
TEST_F(IndexerTest, BinarySearchExactMatch) {
    // Given
    Indexer indexer;
    indexer.addEntry(1000, 0);
    indexer.addEntry(2000, 128);
    indexer.addEntry(3000, 256);
    indexer.addEntry(4000, 384);

    // When - 정확히 일치하는 타임스탬프 검색
    bool found = false;
    IndexEntry entry = indexer.findByTimestamp(2000, found);

    // Then
    EXPECT_TRUE(found);
    EXPECT_EQ(entry.timestamp_ns, 2000);
    EXPECT_EQ(entry.file_offset, 128);
}

// Test 4: 이진 탐색 - 중간 타임스탬프 (가장 가까운 과거)
TEST_F(IndexerTest, BinarySearchClosestPast) {
    // Given
    Indexer indexer;
    indexer.addEntry(1000, 0);
    indexer.addEntry(2000, 128);
    indexer.addEntry(3000, 256);
    indexer.addEntry(4000, 384);

    // When - 2500 (2000과 3000 사이) 검색
    bool found = false;
    IndexEntry entry = indexer.findByTimestamp(2500, found);

    // Then - 2000 엔트리 반환 (가장 가까운 과거)
    EXPECT_TRUE(found);
    EXPECT_EQ(entry.timestamp_ns, 2000);
    EXPECT_EQ(entry.file_offset, 128);
}

// Test 5: 이진 탐색 - 범위 밖 (이전)
TEST_F(IndexerTest, BinarySearchBeforeRange) {
    // Given
    Indexer indexer;
    indexer.addEntry(1000, 0);
    indexer.addEntry(2000, 128);
    indexer.addEntry(3000, 256);

    // When - 500 (가장 작은 타임스탬프보다 작음)
    bool found = false;
    IndexEntry entry = indexer.findByTimestamp(500, found);

    // Then - 첫 번째 엔트리 반환
    EXPECT_TRUE(found);
    EXPECT_EQ(entry.timestamp_ns, 1000);
    EXPECT_EQ(entry.file_offset, 0);
}

// Test 6: 이진 탐색 - 범위 밖 (이후)
TEST_F(IndexerTest, BinarySearchAfterRange) {
    // Given
    Indexer indexer;
    indexer.addEntry(1000, 0);
    indexer.addEntry(2000, 128);
    indexer.addEntry(3000, 256);

    // When - 5000 (가장 큰 타임스탬프보다 큼)
    bool found = false;
    IndexEntry entry = indexer.findByTimestamp(5000, found);

    // Then - 마지막 엔트리 반환
    EXPECT_TRUE(found);
    EXPECT_EQ(entry.timestamp_ns, 3000);
    EXPECT_EQ(entry.file_offset, 256);
}

// Test 7: 빈 인덱스 탐색
TEST_F(IndexerTest, BinarySearchEmptyIndex) {
    // Given
    Indexer indexer;

    // When
    bool found = false;
    IndexEntry entry = indexer.findByTimestamp(1000, found);

    // Then
    EXPECT_FALSE(found);
}

// Test 8: CRC32 체크섬 계산
TEST_F(IndexerTest, CRC32Checksum) {
    // Given - 테스트 파일 생성
    std::string filepath = (testDir / "checksum_test.bag").string();
    {
        std::ofstream ofs(filepath, std::ios::binary);
        std::string testData = "Hello, MXRC Bag!";
        ofs.write(testData.c_str(), testData.size());
    }

    // When - CRC32 계산
    uint32_t crc = Indexer::calculateChecksum(filepath, 16, 0);

    // Then - CRC32 값이 0이 아니어야 함
    EXPECT_NE(crc, 0);

    // 동일한 데이터는 동일한 CRC32 생성
    uint32_t crc2 = Indexer::calculateChecksum(filepath, 16, 0);
    EXPECT_EQ(crc, crc2);
}

// Test 9: 잘못된 파일 읽기
TEST_F(IndexerTest, ReadInvalidFile) {
    // Given
    Indexer indexer;

    // When - 존재하지 않는 파일 읽기
    BagFooter footer = indexer.readFromFile("/nonexistent/file.bag");

    // Then
    EXPECT_FALSE(footer.isValid());
    EXPECT_EQ(indexer.size(), 0);
}

// Test 10: Clear 기능
TEST_F(IndexerTest, ClearEntries) {
    // Given
    Indexer indexer;
    indexer.addEntry(1000, 0);
    indexer.addEntry(2000, 128);

    EXPECT_EQ(indexer.size(), 2);

    // When
    indexer.clear();

    // Then
    EXPECT_EQ(indexer.size(), 0);
    EXPECT_TRUE(indexer.empty());
}

} // namespace mxrc::core::logging
