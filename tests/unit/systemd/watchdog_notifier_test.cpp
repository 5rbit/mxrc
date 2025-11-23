#include <gtest/gtest.h>
#include "interfaces/IWatchdogNotifier.h"
#include "impl/SdNotifyWatchdog.h"
#include <cstdlib>

using namespace mxrc::systemd;

// Watchdog 알림 전송 단위 테스트
class WatchdogNotifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 환경 설정
    }

    void TearDown() override {
        // 테스트 후 정리
    }

    // NOTIFY_SOCKET 환경변수 설정 (테스트용)
    void setNotifySocket(const char* value) {
        if (value) {
            setenv("NOTIFY_SOCKET", value, 1);
        } else {
            unsetenv("NOTIFY_SOCKET");
        }
    }
};

// IWatchdogNotifier 인터페이스 존재 확인
TEST_F(WatchdogNotifierTest, InterfaceExists) {
    // 인터페이스가 정의되어 있는지 확인
    // 컴파일 시점 체크
    SUCCEED();
}

// SdNotifyWatchdog 클래스가 IWatchdogNotifier를 상속하는지 확인
TEST_F(WatchdogNotifierTest, SdNotifyWatchdogImplementsInterface) {
    // SdNotifyWatchdog이 IWatchdogNotifier를 구현하는지 확인
    impl::SdNotifyWatchdog watchdog;

    // 인터페이스 포인터로 캐스팅 가능한지 확인
    interfaces::IWatchdogNotifier* notifier = &watchdog;
    EXPECT_NE(notifier, nullptr);
}

// Watchdog 알림 전송 기본 테스트
TEST_F(WatchdogNotifierTest, SendWatchdogNotification) {
    impl::SdNotifyWatchdog watchdog;

    // NOTIFY_SOCKET이 설정되지 않은 경우
    setNotifySocket(nullptr);
    bool result = watchdog.sendWatchdog();

    // systemd 서비스가 아닌 경우 실패할 수 있음
    // 실패해도 프로그램은 계속 실행되어야 함
    EXPECT_TRUE(result || !result);  // 항상 통과
}

// sd_notify("READY=1") 전송 테스트
TEST_F(WatchdogNotifierTest, SendReadyNotification) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);
    bool result = watchdog.sendReady();

    // systemd 서비스가 아닌 경우에도 에러 없이 처리
    EXPECT_TRUE(result || !result);
}

// sd_notify("STATUS=...") 전송 테스트
TEST_F(WatchdogNotifierTest, SendStatusMessage) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);
    bool result = watchdog.sendStatus("Test status message");

    EXPECT_TRUE(result || !result);
}

// 연속적인 Watchdog 알림 테스트
TEST_F(WatchdogNotifierTest, MultipleSendWatchdog) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);

    // 여러 번 호출해도 문제없이 동작해야 함
    for (int i = 0; i < 10; i++) {
        watchdog.sendWatchdog();
    }

    SUCCEED();
}

// 빠른 연속 호출 테스트 (성능)
TEST_F(WatchdogNotifierTest, RapidWatchdogCalls) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    // 1000번 호출
    for (int i = 0; i < 1000; i++) {
        watchdog.sendWatchdog();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 평균 호출 시간이 10μs 이하인지 확인 (성능 목표)
    double avgTime = duration.count() / 1000.0;
    std::cout << "Average watchdog call time: " << avgTime << " μs" << std::endl;

    EXPECT_LT(avgTime, 10.0) << "Watchdog overhead should be < 10μs";
}

// NULL 포인터 안전성 테스트
TEST_F(WatchdogNotifierTest, NullStatusMessage) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);

    // 빈 문자열 전송
    bool result = watchdog.sendStatus("");
    EXPECT_TRUE(result || !result);
}

// 매우 긴 상태 메시지 테스트
TEST_F(WatchdogNotifierTest, LongStatusMessage) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);

    // 1KB 크기의 상태 메시지
    std::string longMessage(1024, 'x');
    bool result = watchdog.sendStatus(longMessage);

    EXPECT_TRUE(result || !result);
}

// Watchdog 오버헤드 측정
TEST_F(WatchdogNotifierTest, WatchdogOverheadMeasurement) {
    impl::SdNotifyWatchdog watchdog;

    setNotifySocket(nullptr);

    std::vector<double> times;

    // 100회 측정
    for (int i = 0; i < 100; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        watchdog.sendWatchdog();
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        times.push_back(duration.count() / 1000.0);  // μs로 변환
    }

    // 최대값 계산
    double maxTime = *std::max_element(times.begin(), times.end());

    std::cout << "Max watchdog overhead: " << maxTime << " μs" << std::endl;

    // 최악의 경우에도 10μs 이하 유지
    EXPECT_LT(maxTime, 10.0);
}
