#include <gtest/gtest.h>
#include "core/rt/ipc/SharedMemory.h"
#include "core/rt/RTDataStoreShared.h"
#include "core/rt/RTDataStore.h"
#include <sys/wait.h>
#include <unistd.h>

using namespace mxrc::core::rt;
using namespace mxrc::core::rt::ipc;

class SharedMemoryTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Cleanup shared memory
        SharedMemoryRegion::unlink("/test_shm");
    }
};

// 기본 생성/열기/닫기
TEST_F(SharedMemoryTest, BasicCreateOpenClose) {
    SharedMemoryRegion shm1;
    EXPECT_EQ(0, shm1.create("/test_shm", 4096));
    EXPECT_TRUE(shm1.isValid());
    EXPECT_EQ(4096, shm1.getSize());
    EXPECT_NE(nullptr, shm1.getPtr());

    // 다른 프로세스가 열기
    SharedMemoryRegion shm2;
    EXPECT_EQ(0, shm2.open("/test_shm"));
    EXPECT_TRUE(shm2.isValid());
    EXPECT_EQ(4096, shm2.getSize());

    shm1.close();
    shm2.close();
}

// 데이터 쓰기/읽기
TEST_F(SharedMemoryTest, WriteRead) {
    SharedMemoryRegion shm1;
    ASSERT_EQ(0, shm1.create("/test_shm", 4096));

    // 데이터 쓰기
    int* data = static_cast<int*>(shm1.getPtr());
    *data = 0xDEADBEEF;

    // 다른 SharedMemoryRegion 인스턴스로 읽기
    SharedMemoryRegion shm2;
    ASSERT_EQ(0, shm2.open("/test_shm"));

    int* read_data = static_cast<int*>(shm2.getPtr());
    EXPECT_EQ(0xDEADBEEF, *read_data);

    shm1.close();
    shm2.close();
}

// RTDataStoreShared 생성 및 열기
TEST_F(SharedMemoryTest, RTDataStoreSharedCreateOpen) {
    RTDataStoreShared shared1;
    EXPECT_EQ(0, shared1.createShared("/test_shm"));
    EXPECT_TRUE(shared1.isValid());
    EXPECT_NE(nullptr, shared1.getDataStore());

    // 데이터 쓰기
    EXPECT_EQ(0, shared1.getDataStore()->setInt32(DataKey::ROBOT_X, 42));

    // 다른 인스턴스로 읽기
    RTDataStoreShared shared2;
    EXPECT_EQ(0, shared2.openShared("/test_shm"));
    EXPECT_TRUE(shared2.isValid());

    int32_t value = 0;
    EXPECT_EQ(0, shared2.getDataStore()->getInt32(DataKey::ROBOT_X, value));
    EXPECT_EQ(42, value);
}

// 프로세스 간 통신 시뮬레이션
TEST_F(SharedMemoryTest, InterProcessCommunication) {
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스 (Reader)
        sleep(1);  // 부모가 쓸 때까지 대기

        RTDataStoreShared reader;
        if (reader.openShared("/test_shm") != 0) {
            exit(1);
        }

        int32_t x = 0, y = 0;
        reader.getDataStore()->getInt32(DataKey::ROBOT_X, x);
        reader.getDataStore()->getInt32(DataKey::ROBOT_Y, y);

        if (x == 100 && y == 200) {
            exit(0);  // 성공
        } else {
            exit(2);  // 실패
        }
    } else {
        // 부모 프로세스 (Writer)
        RTDataStoreShared writer;
        ASSERT_EQ(0, writer.createShared("/test_shm"));

        writer.getDataStore()->setInt32(DataKey::ROBOT_X, 100);
        writer.getDataStore()->setInt32(DataKey::ROBOT_Y, 200);

        // 자식 프로세스 대기
        int status;
        waitpid(pid, &status, 0);

        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(0, WEXITSTATUS(status));
    }
}

// Sequence number 동기화
TEST_F(SharedMemoryTest, SequenceNumberSync) {
    RTDataStoreShared shared1;
    ASSERT_EQ(0, shared1.createShared("/test_shm"));

    // Sequence 증가 (Seqlock: set할 때마다 +2)
    shared1.getDataStore()->setInt32(DataKey::ROBOT_X, 10);
    EXPECT_EQ(2, shared1.getDataStore()->getSeq(DataKey::ROBOT_X));

    shared1.getDataStore()->setInt32(DataKey::ROBOT_X, 20);
    EXPECT_EQ(4, shared1.getDataStore()->getSeq(DataKey::ROBOT_X));

    // 다른 인스턴스에서 확인
    RTDataStoreShared shared2;
    ASSERT_EQ(0, shared2.openShared("/test_shm"));

    EXPECT_EQ(4, shared2.getDataStore()->getSeq(DataKey::ROBOT_X));

    // shared2에서 증가
    shared2.getDataStore()->setInt32(DataKey::ROBOT_X, 30);
    EXPECT_EQ(6, shared2.getDataStore()->getSeq(DataKey::ROBOT_X));

    // shared1에서 확인
    EXPECT_EQ(6, shared1.getDataStore()->getSeq(DataKey::ROBOT_X));
}

// Timestamp 동기화
TEST_F(SharedMemoryTest, TimestampSync) {
    RTDataStoreShared shared1;
    ASSERT_EQ(0, shared1.createShared("/test_shm"));

    shared1.getDataStore()->setInt32(DataKey::ROBOT_X, 100);
    uint64_t ts1 = shared1.getDataStore()->getTimestamp(DataKey::ROBOT_X);

    RTDataStoreShared shared2;
    ASSERT_EQ(0, shared2.openShared("/test_shm"));

    uint64_t ts2 = shared2.getDataStore()->getTimestamp(DataKey::ROBOT_X);
    EXPECT_EQ(ts1, ts2);
}

// 여러 타입 데이터 공유
TEST_F(SharedMemoryTest, MultipleDataTypes) {
    RTDataStoreShared writer;
    ASSERT_EQ(0, writer.createShared("/test_shm"));

    writer.getDataStore()->setInt32(DataKey::ROBOT_X, 42);
    writer.getDataStore()->setFloat(DataKey::ROBOT_SPEED, 3.14f);
    writer.getDataStore()->setDouble(DataKey::ROBOT_Y, 2.718);
    writer.getDataStore()->setUint64(DataKey::ROBOT_STATUS, 0xABCDEF);
    writer.getDataStore()->setString(DataKey::ROBOT_Z, "test", 4);

    RTDataStoreShared reader;
    ASSERT_EQ(0, reader.openShared("/test_shm"));

    int32_t i32_val;
    float f32_val;
    double f64_val;
    uint64_t u64_val;
    char str_val[32];

    EXPECT_EQ(0, reader.getDataStore()->getInt32(DataKey::ROBOT_X, i32_val));
    EXPECT_EQ(42, i32_val);

    EXPECT_EQ(0, reader.getDataStore()->getFloat(DataKey::ROBOT_SPEED, f32_val));
    EXPECT_FLOAT_EQ(3.14f, f32_val);

    EXPECT_EQ(0, reader.getDataStore()->getDouble(DataKey::ROBOT_Y, f64_val));
    EXPECT_DOUBLE_EQ(2.718, f64_val);

    EXPECT_EQ(0, reader.getDataStore()->getUint64(DataKey::ROBOT_STATUS, u64_val));
    EXPECT_EQ(0xABCDEF, u64_val);

    EXPECT_EQ(0, reader.getDataStore()->getString(DataKey::ROBOT_Z, str_val, sizeof(str_val)));
    EXPECT_STREQ("test", str_val);
}
