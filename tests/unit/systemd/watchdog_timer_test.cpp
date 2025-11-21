#include <gtest/gtest.h>
#include "util/WatchdogTimer.h"
#include "interfaces/IWatchdogNotifier.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace mxrc::systemd;

// 테스트용 Mock Watchdog Notifier
class MockWatchdogNotifier : public interfaces::IWatchdogNotifier {
private:
    std::atomic<int> watchdogCount_{0};
    std::atomic<int> readyCount_{0};

public:
    bool sendWatchdog() override {
        watchdogCount_++;
        return true;
    }

    bool sendReady() override {
        readyCount_++;
        return true;
    }

    bool sendStatus(const std::string& status) override {
        return true;
    }

    int getWatchdogCount() const {
        return watchdogCount_.load();
    }

    int getReadyCount() const {
        return readyCount_.load();
    }

    void reset() {
        watchdogCount_ = 0;
        readyCount_ = 0;
    }
};

// Watchdog 타이머 단위 테스트
class WatchdogTimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockNotifier = std::make_shared<MockWatchdogNotifier>();
    }

    void TearDown() override {
        mockNotifier.reset();
    }

    std::shared_ptr<MockWatchdogNotifier> mockNotifier;
};

// WatchdogTimer 생성 테스트
TEST_F(WatchdogTimerTest, CreateTimer) {
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    SUCCEED();
}

// WatchdogTimer 시작 테스트
TEST_F(WatchdogTimerTest, StartTimer) {
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    bool started = timer.start();
    EXPECT_TRUE(started);

    // 정리
    timer.stop();
}

// WatchdogTimer 정지 테스트
TEST_F(WatchdogTimerTest, StopTimer) {
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool stopped = timer.stop();
    EXPECT_TRUE(stopped);
}

// 주기적 Watchdog 알림 테스트
TEST_F(WatchdogTimerTest, PeriodicNotifications) {
    mockNotifier->reset();

    // 100ms 주기로 설정
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    timer.start();

    // 350ms 대기 (약 3번 알림)
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    timer.stop();

    int count = mockNotifier->getWatchdogCount();
    std::cout << "Watchdog notifications sent: " << count << std::endl;

    // 최소 2번, 최대 4번 알림 (타이밍 여유 고려)
    EXPECT_GE(count, 2);
    EXPECT_LE(count, 5);
}

// 타이머 간격 정확도 테스트
TEST_F(WatchdogTimerTest, TimerAccuracy) {
    mockNotifier->reset();

    // 50ms 주기
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(50));

    timer.start();

    // 정확히 1초 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    timer.stop();

    int count = mockNotifier->getWatchdogCount();
    std::cout << "Notifications in 1 second: " << count << std::endl;

    // 1초 / 50ms = 20번 (±3회 오차 허용)
    EXPECT_GE(count, 17);
    EXPECT_LE(count, 23);
}

// 중복 시작 방지 테스트
TEST_F(WatchdogTimerTest, PreventDoubleStart) {
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    bool firstStart = timer.start();
    EXPECT_TRUE(firstStart);

    bool secondStart = timer.start();
    EXPECT_FALSE(secondStart) << "Timer should not start twice";

    timer.stop();
}

// 시작하지 않고 정지 테스트
TEST_F(WatchdogTimerTest, StopWithoutStart) {
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    bool stopped = timer.stop();
    EXPECT_FALSE(stopped) << "Should not stop a timer that wasn't started";
}

// 빠른 시작/정지 반복 테스트
TEST_F(WatchdogTimerTest, RapidStartStop) {
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(100));

    for (int i = 0; i < 10; i++) {
        timer.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        timer.stop();
    }

    SUCCEED();
}

// 매우 짧은 간격 테스트
TEST_F(WatchdogTimerTest, VeryShortInterval) {
    mockNotifier->reset();

    // 10ms 주기 (매우 짧음)
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(10));

    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    timer.stop();

    int count = mockNotifier->getWatchdogCount();
    std::cout << "Notifications with 10ms interval: " << count << std::endl;

    // 100ms / 10ms = 10번 (±3회 오차 허용)
    EXPECT_GE(count, 7);
    EXPECT_LE(count, 13);
}

// 소멸자에서 자동 정지 테스트
TEST_F(WatchdogTimerTest, AutoStopOnDestroy) {
    mockNotifier->reset();

    {
        util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(50));
        timer.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // timer 소멸 시 자동으로 stop() 호출
    }

    // 소멸 후 알림이 더 이상 발생하지 않아야 함
    int countBefore = mockNotifier->getWatchdogCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int countAfter = mockNotifier->getWatchdogCount();

    EXPECT_EQ(countBefore, countAfter) << "No notifications after timer destruction";
}

// 긴 간격 테스트
TEST_F(WatchdogTimerTest, LongInterval) {
    mockNotifier->reset();

    // 500ms 주기 (긴 간격)
    util::WatchdogTimer timer(mockNotifier, std::chrono::milliseconds(500));

    timer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    timer.stop();

    int count = mockNotifier->getWatchdogCount();
    std::cout << "Notifications with 500ms interval: " << count << std::endl;

    // 1200ms / 500ms = 2~3번
    EXPECT_GE(count, 2);
    EXPECT_LE(count, 3);
}
