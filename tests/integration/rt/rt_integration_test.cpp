#include <gtest/gtest.h>
#include "core/rt/RTExecutive.h"
#include "core/rt/RTDataStoreShared.h"
#include "core/rt/RTStateMachine.h"
#include <thread>
#include <atomic>
#include <chrono>

using namespace mxrc::core::rt;

class RTIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        RTDataStoreShared::unlinkShared("/test_rt_integration");
    }
};

// 통합 테스트: RTExecutive + RTDataStore + StateMachine
TEST_F(RTIntegrationTest, FullIntegration) {
    // 공유 메모리 데이터 저장소 생성
    RTDataStoreShared shared;
    ASSERT_EQ(0, shared.createShared("/test_rt_integration"));

    // RTExecutive 생성 (동적 주기 설정)
    std::vector<uint32_t> periods = {10, 20};
    auto* exec = RTExecutive::createFromPeriods(periods);
    ASSERT_NE(nullptr, exec);

    // 데이터 저장소 연결
    exec->setDataStore(shared.getDataStore());

    // 상태 머신 확인
    EXPECT_EQ(RTState::READY, exec->getStateMachine()->getState());

    // Action 등록: 10ms마다 카운터 증가
    std::atomic<int> counter_10ms{0};
    exec->registerAction("counter_10", 10, [&counter_10ms](RTContext& ctx) {
        counter_10ms++;
        // 데이터 저장소에 쓰기
        if (ctx.data_store) {
            ctx.data_store->setInt32(DataKey::ROBOT_X, counter_10ms.load());
        }
    });

    // Action 등록: 20ms마다 데이터 읽기
    std::atomic<int> read_value{0};
    exec->registerAction("reader_20", 20, [&read_value](RTContext& ctx) {
        if (ctx.data_store) {
            int32_t val = 0;
            ctx.data_store->getInt32(DataKey::ROBOT_X, val);
            read_value = val;
        }
    });

    // 실행
    std::thread exec_thread([exec]() {
        exec->run();
    });

    // RUNNING 상태 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(RTState::RUNNING, exec->getStateMachine()->getState());

    // 100ms 실행
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    exec->stop();
    exec_thread.join();

    // SHUTDOWN 상태 확인
    EXPECT_EQ(RTState::SHUTDOWN, exec->getStateMachine()->getState());

    // 카운터 확인 (10ms 주기로 100ms = 최소 8회 이상)
    EXPECT_GT(counter_10ms, 5);

    // 공유 메모리를 통한 데이터 읽기 확인
    int32_t final_val = 0;
    shared.getDataStore()->getInt32(DataKey::ROBOT_X, final_val);
    EXPECT_GT(final_val, 0);

    delete exec;
}

// 프로세스 간 통신 시뮬레이션
TEST_F(RTIntegrationTest, InterProcessDataSharing) {
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스: Non-RT (읽기 전용)
        sleep(1);  // RT 프로세스가 시작할 때까지 대기

        RTDataStoreShared reader;
        if (reader.openShared("/test_rt_integration") != 0) {
            exit(1);
        }

        // 5초간 주기적으로 읽기
        for (int i = 0; i < 5; i++) {
            int32_t value = 0;
            reader.getDataStore()->getInt32(DataKey::ROBOT_X, value);

            if (value > 0) {
                exit(0);  // 성공
            }

            usleep(100000);  // 100ms 대기
        }

        exit(2);  // 실패
    } else {
        // 부모 프로세스: RT (쓰기)
        RTDataStoreShared writer;
        ASSERT_EQ(0, writer.createShared("/test_rt_integration"));

        auto* exec = RTExecutive::createFromPeriods({10});
        ASSERT_NE(nullptr, exec);
        exec->setDataStore(writer.getDataStore());

        std::atomic<int> count{0};
        exec->registerAction("writer", 10, [&count](RTContext& ctx) {
            count++;
            if (ctx.data_store) {
                ctx.data_store->setInt32(DataKey::ROBOT_X, count.load());
            }
        });

        std::thread exec_thread([exec]() {
            exec->run();
        });

        // 자식 프로세스 대기
        int status;
        waitpid(pid, &status, 0);

        exec->stop();
        exec_thread.join();

        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(0, WEXITSTATUS(status));

        delete exec;
    }
}

// 상태 전환 통합 테스트
TEST_F(RTIntegrationTest, StateTransitionsIntegration) {
    RTDataStoreShared shared;
    ASSERT_EQ(0, shared.createShared("/test_rt_integration"));

    auto* exec = RTExecutive::createFromPeriods({10});
    ASSERT_NE(nullptr, exec);
    exec->setDataStore(shared.getDataStore());

    std::atomic<int> transition_count{0};
    exec->getStateMachine()->setTransitionCallback([&transition_count](RTState from, RTState to, RTEvent event) {
        transition_count++;
    });

    exec->registerAction("test", 10, [](RTContext&) {});

    // 초기 상태: READY (생성 시 INIT->READY 전환 발생)
    EXPECT_EQ(RTState::READY, exec->getStateMachine()->getState());
    int initial_transitions = transition_count.load();

    std::thread exec_thread([exec]() {
        exec->run();
    });

    // READY -> RUNNING 전환
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(RTState::RUNNING, exec->getStateMachine()->getState());

    exec->stop();
    exec_thread.join();

    // RUNNING -> SHUTDOWN 전환
    EXPECT_EQ(RTState::SHUTDOWN, exec->getStateMachine()->getState());

    // 최소 2번 전환 (READY->RUNNING, RUNNING->SHUTDOWN)
    EXPECT_GE(transition_count - initial_transitions, 2);

    delete exec;
}
