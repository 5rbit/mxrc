#include <gtest/gtest.h>
#include "core/rt/RTExecutive.h"
#include "core/rt/RTDataStore.h"
#include "core/rt/RTStateMachine.h"
#include "core/rt/ipc/SharedMemoryData.h"
#include "core/rt/util/ScheduleCalculator.h"
#include "core/rt/util/TimeUtils.h"
#include <thread>
#include <atomic>

using namespace mxrc::core::rt;

class RTExecutiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 간단한 주기 사용
    }
};

// 기본 생성자
TEST_F(RTExecutiveTest, BasicConstruction) {
    RTExecutive exec(10, 100);
    EXPECT_EQ(10, exec.getMinorCycleMs());
    EXPECT_EQ(100, exec.getMajorCycleMs());
    EXPECT_EQ(10, exec.getNumSlots());
}

// createFromPeriods - 단일 주기
TEST_F(RTExecutiveTest, CreateFromSinglePeriod) {
    std::vector<uint32_t> periods = {20};
    auto exec = RTExecutive::createFromPeriods(periods);

    ASSERT_NE(nullptr, exec);
    EXPECT_EQ(20, exec->getMinorCycleMs());
    EXPECT_EQ(20, exec->getMajorCycleMs());
    EXPECT_EQ(1, exec->getNumSlots());
}

// createFromPeriods - 여러 주기
TEST_F(RTExecutiveTest, CreateFromMultiplePeriods) {
    std::vector<uint32_t> periods = {10, 20, 50};
    auto exec = RTExecutive::createFromPeriods(periods);

    ASSERT_NE(nullptr, exec);
    EXPECT_EQ(10, exec->getMinorCycleMs());  // GCD(10, 20, 50) = 10
    EXPECT_EQ(100, exec->getMajorCycleMs()); // LCM(10, 20, 50) = 100
    EXPECT_EQ(10, exec->getNumSlots());
}

// createFromPeriods - 복잡한 주기
TEST_F(RTExecutiveTest, CreateFromComplexPeriods) {
    std::vector<uint32_t> periods = {12, 18, 24};
    auto exec = RTExecutive::createFromPeriods(periods);

    ASSERT_NE(nullptr, exec);
    EXPECT_EQ(6, exec->getMinorCycleMs());   // GCD(12, 18, 24) = 6
    EXPECT_EQ(72, exec->getMajorCycleMs());  // LCM(12, 18, 24) = 72
    EXPECT_EQ(12, exec->getNumSlots());
}

// createFromPeriods - 빈 배열
TEST_F(RTExecutiveTest, CreateFromEmptyPeriods) {
    std::vector<uint32_t> periods;
    auto exec = RTExecutive::createFromPeriods(periods);

    EXPECT_EQ(nullptr, exec);
}

// createFromPeriods - 0 주기
TEST_F(RTExecutiveTest, CreateFromZeroPeriod) {
    std::vector<uint32_t> periods = {0, 10};
    auto exec = RTExecutive::createFromPeriods(periods);

    EXPECT_EQ(nullptr, exec);
}

// createFromPeriods - 최대 제한 초과
TEST_F(RTExecutiveTest, CreateFromExcessivePeriods) {
    // LCM이 MAX_MAJOR_CYCLE_MS(1000)을 초과하는 경우
    std::vector<uint32_t> periods = {7, 11, 13, 17};  // LCM = 17017
    auto exec = RTExecutive::createFromPeriods(periods);

    EXPECT_EQ(nullptr, exec);
}

// Action 등록
TEST_F(RTExecutiveTest, RegisterAction) {
    RTExecutive exec(10, 100);

    int call_count = 0;
    auto callback = [&call_count](RTContext& ctx) {
        call_count++;
    };

    EXPECT_EQ(0, exec.registerAction("test_action", 20, callback));
}

// Action 등록 - 잘못된 주기
TEST_F(RTExecutiveTest, RegisterActionInvalidPeriod) {
    RTExecutive exec(10, 100);

    auto callback = [](RTContext& ctx) {};

    // 15ms는 minor_cycle(10ms)의 배수가 아님
    EXPECT_EQ(-1, exec.registerAction("test_action", 15, callback));
}

// RTDataStore 설정
TEST_F(RTExecutiveTest, SetDataStore) {
    RTExecutive exec(10, 100);
    RTDataStore store;

    exec.setDataStore(&store);
    // 설정되었는지 확인 (실행시 context를 통해 접근 가능)
}

// 짧은 실행 테스트
TEST_F(RTExecutiveTest, ShortRun) {
    RTExecutive exec(10, 50);  // 10ms minor, 50ms major

    std::atomic<int> call_count{0};
    auto callback = [&call_count](RTContext& ctx) {
        call_count++;
    };

    exec.registerAction("test", 10, callback);

    // 별도 스레드에서 실행
    std::thread exec_thread([&exec]() {
        exec.run();
    });

    // 100ms 대기 후 중지
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    exec.stop();
    exec_thread.join();

    // 최소 몇 번은 호출되어야 함 (100ms / 10ms = 10회 정도)
    EXPECT_GT(call_count, 5);
}

// Context 정보 확인
TEST_F(RTExecutiveTest, ContextInfo) {
    RTExecutive exec(10, 50);
    RTDataStore store;
    exec.setDataStore(&store);

    std::atomic<bool> context_valid{false};
    auto callback = [&context_valid, &store](RTContext& ctx) {
        if (ctx.data_store == &store &&
            ctx.current_slot < 5 &&
            ctx.timestamp_ns > 0) {
            context_valid = true;
        }
    };

    exec.registerAction("test", 10, callback);

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    exec.stop();
    exec_thread.join();

    EXPECT_TRUE(context_valid);
}

// 여러 주기의 Action 등록
TEST_F(RTExecutiveTest, MultiplePeriodicActions) {
    auto exec = RTExecutive::createFromPeriods({10, 20, 50});
    ASSERT_NE(nullptr, exec);

    std::atomic<int> count_10ms{0};
    std::atomic<int> count_20ms{0};
    std::atomic<int> count_50ms{0};

    exec->registerAction("10ms", 10, [&count_10ms](RTContext&) { count_10ms++; });
    exec->registerAction("20ms", 20, [&count_20ms](RTContext&) { count_20ms++; });
    exec->registerAction("50ms", 50, [&count_50ms](RTContext&) { count_50ms++; });

    std::thread exec_thread([&exec]() {
        exec->run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    exec->stop();
    exec_thread.join();

    // 비율 확인: 10ms가 가장 많이, 50ms가 가장 적게 호출
    EXPECT_GT(count_10ms, count_20ms);
    EXPECT_GT(count_20ms, count_50ms);
}

// ==========================================
// Guard Condition Tests (TASK-023)
// ==========================================

// Guard condition - RUNNING 상태에서만 실행
TEST_F(RTExecutiveTest, GuardConditionRunningOnly) {
    RTExecutive exec(10, 50);

    std::atomic<int> call_count{0};
    auto callback = [&call_count](RTContext& ctx) {
        call_count++;
    };

    // Guard: RUNNING 상태에서만 실행
    auto guard = [](const RTStateMachine& sm) {
        return sm.getState() == RTState::RUNNING;
    };

    exec.registerAction("guarded_action", 10, callback, guard);

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    exec.stop();
    exec_thread.join();

    // RUNNING 상태에서 실행되었으므로 호출되어야 함
    EXPECT_GT(call_count, 0);
}

// Guard condition - 특정 상태에서 차단
TEST_F(RTExecutiveTest, GuardConditionBlocked) {
    RTExecutive exec(10, 50);

    std::atomic<int> call_count{0};
    auto callback = [&call_count](RTContext& ctx) {
        call_count++;
    };

    // Guard: PAUSED 상태에서만 실행 (절대 실행되지 않음)
    auto guard = [](const RTStateMachine& sm) {
        return sm.getState() == RTState::PAUSED;
    };

    exec.registerAction("blocked_action", 10, callback, guard);

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    exec.stop();
    exec_thread.join();

    // PAUSED 상태가 아니므로 호출되지 않아야 함
    EXPECT_EQ(0, call_count);
}

// Guard condition - nullptr (항상 실행)
TEST_F(RTExecutiveTest, GuardConditionNull) {
    RTExecutive exec(10, 50);

    std::atomic<int> call_count{0};
    auto callback = [&call_count](RTContext& ctx) {
        call_count++;
    };

    // Guard가 nullptr이면 항상 실행
    exec.registerAction("always_action", 10, callback, nullptr);

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    exec.stop();
    exec_thread.join();

    EXPECT_GT(call_count, 0);
}

// ==========================================
// Heartbeat & SAFE_MODE Tests (TASK-024)
// ==========================================

// Heartbeat 정상 동작
TEST_F(RTExecutiveTest, HeartbeatNormal) {
    RTExecutive exec(10, 50);

    // Shared memory 생성
    ipc::SharedMemoryData shm_data{};
    shm_data.rt_heartbeat_ns.store(0);
    shm_data.nonrt_heartbeat_ns.store(util::getMonotonicTimeNs());

    exec.setSharedMemory(&shm_data);
    exec.enableHeartbeatMonitoring(true);

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    // Non-RT heartbeat 갱신 시뮬레이션
    std::thread heartbeat_thread([&shm_data]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            shm_data.nonrt_heartbeat_ns.store(util::getMonotonicTimeNs());
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    exec.stop();
    exec_thread.join();
    heartbeat_thread.join();

    // SAFE_MODE로 전환되지 않아야 함
    EXPECT_EQ(RTState::SHUTDOWN, exec.getStateMachine()->getState());
}

// Heartbeat timeout → SAFE_MODE 진입
TEST_F(RTExecutiveTest, HeartbeatTimeoutSafeMode) {
    RTExecutive exec(10, 50);

    // Shared memory 생성
    ipc::SharedMemoryData shm_data{};
    shm_data.rt_heartbeat_ns.store(0);
    // Non-RT heartbeat를 오래된 시간으로 설정 (timeout 유발)
    shm_data.nonrt_heartbeat_ns.store(util::getMonotonicTimeNs() - 2'000'000'000ULL); // 2초 전

    exec.setSharedMemory(&shm_data);
    exec.enableHeartbeatMonitoring(true);

    std::atomic<bool> entered_safe_mode{false};

    // State transition callback으로 SAFE_MODE 진입 감지
    exec.getStateMachine()->setTransitionCallback(
        [&entered_safe_mode](RTState from, RTState to, RTEvent event) {
            if (to == RTState::SAFE_MODE && event == RTEvent::SAFE_MODE_ENTER) {
                entered_safe_mode = true;
            }
        }
    );

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    // 충분한 시간 대기 (heartbeat check 발생)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    exec.stop();
    exec_thread.join();

    // SAFE_MODE로 진입했어야 함
    EXPECT_TRUE(entered_safe_mode);
}

// Heartbeat 복구 → SAFE_MODE 탈출
TEST_F(RTExecutiveTest, HeartbeatRecoverySafeModeExit) {
    RTExecutive exec(10, 50);

    // Shared memory 생성
    ipc::SharedMemoryData shm_data{};
    shm_data.rt_heartbeat_ns.store(0);
    // 처음에는 timeout
    shm_data.nonrt_heartbeat_ns.store(util::getMonotonicTimeNs() - 2'000'000'000ULL);

    exec.setSharedMemory(&shm_data);
    exec.enableHeartbeatMonitoring(true);

    std::atomic<bool> exited_safe_mode{false};

    exec.getStateMachine()->setTransitionCallback(
        [&exited_safe_mode](RTState from, RTState to, RTEvent event) {
            if (from == RTState::SAFE_MODE && event == RTEvent::SAFE_MODE_EXIT) {
                exited_safe_mode = true;
            }
        }
    );

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    // SAFE_MODE 진입 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Heartbeat 복구
    shm_data.nonrt_heartbeat_ns.store(util::getMonotonicTimeNs());

    // 복구 확인 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    exec.stop();
    exec_thread.join();

    // SAFE_MODE에서 탈출했어야 함
    EXPECT_TRUE(exited_safe_mode);
}

// Heartbeat monitoring 비활성화 시 SAFE_MODE 진입하지 않음
TEST_F(RTExecutiveTest, HeartbeatMonitoringDisabled) {
    RTExecutive exec(10, 50);

    ipc::SharedMemoryData shm_data{};
    shm_data.rt_heartbeat_ns.store(0);
    shm_data.nonrt_heartbeat_ns.store(util::getMonotonicTimeNs() - 2'000'000'000ULL);

    exec.setSharedMemory(&shm_data);
    exec.enableHeartbeatMonitoring(false);  // 비활성화

    std::atomic<bool> entered_safe_mode{false};

    exec.getStateMachine()->setTransitionCallback(
        [&entered_safe_mode](RTState from, RTState to, RTEvent event) {
            if (to == RTState::SAFE_MODE) {
                entered_safe_mode = true;
            }
        }
    );

    std::thread exec_thread([&exec]() {
        exec.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    exec.stop();
    exec_thread.join();

    // Monitoring이 비활성화되어 SAFE_MODE로 진입하지 않아야 함
    EXPECT_FALSE(entered_safe_mode);
}
