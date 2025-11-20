#include <gtest/gtest.h>
#include "core/rt/RTDataStore.h"
#include "core/rt/util/TimeUtils.h"
#include <thread>
#include <chrono>

using namespace mxrc::core::rt;

class RTDataStoreTest : public ::testing::Test {
protected:
    RTDataStore store_;
};

// 기본 구조 테스트
TEST_F(RTDataStoreTest, BasicConstruction) {
    EXPECT_EQ(0, store_.getSeq(DataKey::ROBOT_X));
}

// INT32 set/get
TEST_F(RTDataStoreTest, SetGetInt32) {
    EXPECT_EQ(0, store_.setInt32(DataKey::ROBOT_X, 100));

    int32_t value = 0;
    EXPECT_EQ(0, store_.getInt32(DataKey::ROBOT_X, value));
    EXPECT_EQ(100, value);
}

// FLOAT set/get
TEST_F(RTDataStoreTest, SetGetFloat) {
    EXPECT_EQ(0, store_.setFloat(DataKey::ROBOT_SPEED, 3.14f));

    float value = 0.0f;
    EXPECT_EQ(0, store_.getFloat(DataKey::ROBOT_SPEED, value));
    EXPECT_FLOAT_EQ(3.14f, value);
}

// DOUBLE set/get
TEST_F(RTDataStoreTest, SetGetDouble) {
    EXPECT_EQ(0, store_.setDouble(DataKey::ROBOT_Y, 2.718281828));

    double value = 0.0;
    EXPECT_EQ(0, store_.getDouble(DataKey::ROBOT_Y, value));
    EXPECT_DOUBLE_EQ(2.718281828, value);
}

// UINT64 set/get
TEST_F(RTDataStoreTest, SetGetUint64) {
    EXPECT_EQ(0, store_.setUint64(DataKey::ROBOT_STATUS, 0xDEADBEEF12345678ULL));

    uint64_t value = 0;
    EXPECT_EQ(0, store_.getUint64(DataKey::ROBOT_STATUS, value));
    EXPECT_EQ(0xDEADBEEF12345678ULL, value);
}

// STRING set/get
TEST_F(RTDataStoreTest, SetGetString) {
    const char* test_str = "HelloRT";
    EXPECT_EQ(0, store_.setString(DataKey::ROBOT_Z, test_str, strlen(test_str)));

    char buffer[32] = {0};
    EXPECT_EQ(0, store_.getString(DataKey::ROBOT_Z, buffer, sizeof(buffer)));
    EXPECT_STREQ("HelloRT", buffer);
}

// 긴 문자열 잘림 테스트
TEST_F(RTDataStoreTest, StringTruncation) {
    // 32바이트를 초과하는 문자열
    const char* long_str = "ThisIsAVeryLongStringThatExceeds32BytesLimit";
    EXPECT_EQ(0, store_.setString(DataKey::ROBOT_Z, long_str, strlen(long_str)));

    char buffer[32] = {0};
    EXPECT_EQ(0, store_.getString(DataKey::ROBOT_Z, buffer, sizeof(buffer)));

    // 31바이트까지만 복사되어야 함
    EXPECT_EQ(31, strlen(buffer));
}

// 타입 불일치 에러
TEST_F(RTDataStoreTest, TypeMismatch) {
    store_.setInt32(DataKey::ROBOT_X, 42);

    float value = 0.0f;
    // INT32로 저장된 값을 FLOAT로 읽으면 실패
    EXPECT_EQ(-1, store_.getFloat(DataKey::ROBOT_X, value));
}

// 유효하지 않은 키
TEST_F(RTDataStoreTest, InvalidKey) {
    DataKey invalid_key = static_cast<DataKey>(600);  // MAX_KEYS(512) 초과

    EXPECT_EQ(-1, store_.setInt32(invalid_key, 100));

    int32_t value = 0;
    EXPECT_EQ(-1, store_.getInt32(invalid_key, value));
}

// Sequence number 증가
TEST_F(RTDataStoreTest, SequenceIncrement) {
    EXPECT_EQ(0, store_.getSeq(DataKey::ROBOT_X));

    store_.setInt32(DataKey::ROBOT_X, 10);
    EXPECT_EQ(1, store_.getSeq(DataKey::ROBOT_X));

    store_.setInt32(DataKey::ROBOT_X, 20);
    EXPECT_EQ(2, store_.getSeq(DataKey::ROBOT_X));

    store_.setInt32(DataKey::ROBOT_X, 30);
    EXPECT_EQ(3, store_.getSeq(DataKey::ROBOT_X));
}

// Atomic sequence increment
TEST_F(RTDataStoreTest, AtomicSequenceIncrement) {
    uint64_t seq1 = store_.incrementSeq(DataKey::ROBOT_X);
    uint64_t seq2 = store_.incrementSeq(DataKey::ROBOT_X);
    uint64_t seq3 = store_.incrementSeq(DataKey::ROBOT_X);

    EXPECT_EQ(0, seq1);
    EXPECT_EQ(1, seq2);
    EXPECT_EQ(2, seq3);
}

// Timestamp 테스트
TEST_F(RTDataStoreTest, Timestamp) {
    uint64_t before = util::getMonotonicTimeNs();
    store_.setInt32(DataKey::ROBOT_X, 100);
    uint64_t after = util::getMonotonicTimeNs();

    uint64_t timestamp = store_.getTimestamp(DataKey::ROBOT_X);
    EXPECT_GE(timestamp, before);
    EXPECT_LE(timestamp, after);
}

// isFresh() - 신선한 데이터
TEST_F(RTDataStoreTest, IsFreshTrue) {
    store_.setInt32(DataKey::ROBOT_X, 100);

    // 1초 이내 데이터는 신선함
    EXPECT_TRUE(store_.isFresh(DataKey::ROBOT_X, 1'000'000'000ULL));
}

// isFresh() - 오래된 데이터
TEST_F(RTDataStoreTest, IsFreshFalse) {
    store_.setInt32(DataKey::ROBOT_X, 100);

    // 10ms 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 5ms 이내 데이터만 허용하면 오래된 것으로 판단
    EXPECT_FALSE(store_.isFresh(DataKey::ROBOT_X, 5'000'000ULL));
}

// isFresh() - 데이터 없음
TEST_F(RTDataStoreTest, IsFreshNoData) {
    // 아무 데이터도 set하지 않은 키
    EXPECT_FALSE(store_.isFresh(DataKey::ROBOT_Y, 1'000'000'000ULL));
}

// 멀티스레드 동시 쓰기 (기본 테스트)
TEST_F(RTDataStoreTest, ConcurrentWrites) {
    constexpr int NUM_THREADS = 4;
    constexpr int WRITES_PER_THREAD = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < WRITES_PER_THREAD; ++j) {
                store_.setInt32(DataKey::ROBOT_X, i * 1000 + j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 최소한 NUM_THREADS * WRITES_PER_THREAD 만큼 sequence가 증가해야 함
    EXPECT_GE(store_.getSeq(DataKey::ROBOT_X), NUM_THREADS * WRITES_PER_THREAD);
}
