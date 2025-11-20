#include "gtest/gtest.h"
#include "core/rt/RTDataStore.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

namespace mxrc {
namespace core {
namespace rt {

class RTDataStoreConcurrencyTest : public ::testing::Test {
protected:
    RTDataStore data_store;
    static constexpr int TEST_DURATION_MS = 200;
    static constexpr int NUM_READERS = 4;
};

// 이 테스트는 현재의 잘못된 구현에서 실패해야 합니다.
// 경합 조건을 안정적으로 재현하기는 어렵지만,
// 이 테스트는 잠재적인 문제를 드러낼 가능성이 있습니다.
TEST_F(RTDataStoreConcurrencyTest, ShouldFailWithTornReads) {
    std::atomic<bool> stop_flag(false);
    std::atomic<int> writer_value(0);
    std::atomic<int> errors(0);

    const DataKey test_key = DataKey::ROBOT_SPEED;

    // Writer thread
    std::thread writer([&]() {
        int val = 0;
        while (!stop_flag) {
            data_store.setInt32(test_key, val++);
            writer_value.store(val, std::memory_order_relaxed);
            // 작은 지연을 주어 reader가 끼어들 기회를 줌
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&]() {
            int last_read_value = -1;
            while (!stop_flag) {
                int current_value;
                if (data_store.getInt32(test_key, current_value) == 0) {
                    // 데이터가 단조적으로 증가해야 함
                    if (current_value < last_read_value) {
                        errors++;
                        // 상세 정보 로깅
                        GTEST_LOG_(ERROR) << "Torn read detected! "
                                          << "Last: " << last_read_value
                                          << ", Current: " << current_value;
                    }
                    last_read_value = current_value;
                }
                 // 작은 지연
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(TEST_DURATION_MS));
    stop_flag = true;

    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }

    // 현재 구현에서는 이 테스트가 통과할 수 있지만(경쟁 조건이 발생하지 않으면),
    // seqlock이 없으면 실패할 가능성이 높습니다.
    // 수정 후에는 이 assert가 항상 통과해야 합니다.
    EXPECT_EQ(errors.load(), 0) << "Torn reads were detected during the test.";
}

} // namespace rt
} // namespace core
} // namespace mxrc
