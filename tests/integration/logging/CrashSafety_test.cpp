#include "gtest/gtest.h"
#include "core/logging/Log.h"
#include "core/logging/SignalHandler.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace mxrc::core::logging {

class CrashSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 로그 디렉토리 생성
        std::filesystem::create_directories("logs");

        // 기존 로그 파일 삭제
        if (std::filesystem::exists("logs/mxrc.log")) {
            std::filesystem::remove("logs/mxrc.log");
        }
    }

    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 로그 파일의 특정 패턴 발생 횟수 계산
    int count_log_occurrences(const std::string& pattern) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        std::ifstream log_file("logs/mxrc.log");
        if (!log_file.is_open()) {
            return 0;
        }

        int count = 0;
        std::string line;
        while (std::getline(log_file, line)) {
            if (line.find(pattern) != std::string::npos) {
                count++;
            }
        }
        return count;
    }
};

// T042: 크래시 3초 이내 로그 보존율 99% 테스트
TEST_F(CrashSafetyTest, NinetyNinePercentPreservation) {
    // Given
    const int total_messages = 100;
    const int messages_before_crash = 99;  // 99%

    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스
        initialize_async_logger();
        register_signal_handlers();

        // 100개 메시지 중 99개 전송 후 크래시
        for (int i = 0; i < messages_before_crash; i++) {
            spdlog::info("Message {}", i);

            // 3초 동안 균등하게 분포 (30ms 간격)
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        // 마지막 CRITICAL 메시지 (즉시 플러시)
        spdlog::critical("About to crash - last message");

        // 크래시 유도
        std::raise(SIGSEGV);

        std::exit(1);

    } else if (pid > 0) {
        // 부모 프로세스
        int status;
        waitpid(pid, &status, 0);

        // Then
        EXPECT_TRUE(WIFSIGNALED(status));

        // "Message" 패턴 발생 횟수 확인
        int preserved_messages = count_log_occurrences("Message");

        std::cout << "Preserved messages: " << preserved_messages << " / " << messages_before_crash << std::endl;

        // 99% 이상 보존 (99개 중 최소 98개)
        double preservation_rate = (preserved_messages * 100.0) / messages_before_crash;
        std::cout << "Preservation rate: " << preservation_rate << "%" << std::endl;

        EXPECT_GE(preserved_messages, 98);  // 99% 이상
        EXPECT_GE(preservation_rate, 99.0);

        // CRITICAL 메시지는 반드시 보존
        EXPECT_TRUE(count_log_occurrences("About to crash - last message") > 0);

    } else {
        FAIL() << "fork() failed";
    }
}

// 짧은 시간 (100ms) 내 크래시 시 로그 보존 테스트
TEST_F(CrashSafetyTest, ShortTimeCrash) {
    // Given
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스
        initialize_async_logger();
        register_signal_handlers();

        // 100ms 동안 10개 메시지 전송
        for (int i = 0; i < 10; i++) {
            spdlog::info("Fast message {}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        spdlog::critical("Fast crash");
        std::raise(SIGSEGV);

        std::exit(1);

    } else if (pid > 0) {
        // 부모 프로세스
        int status;
        waitpid(pid, &status, 0);

        // Then
        EXPECT_TRUE(WIFSIGNALED(status));

        // 최소 9개 이상 보존 (90%)
        int preserved = count_log_occurrences("Fast message");
        std::cout << "Short time preserved: " << preserved << " / 10" << std::endl;
        EXPECT_GE(preserved, 9);

        // CRITICAL은 항상 보존
        EXPECT_TRUE(count_log_occurrences("Fast crash") > 0);

    } else {
        FAIL() << "fork() failed";
    }
}

// 주기적 flush에 의한 로그 보존 테스트
TEST_F(CrashSafetyTest, PeriodicFlushPreservation) {
    // Given
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스
        initialize_async_logger();
        register_signal_handlers();

        // 3.5초 동안 로그 작성 (주기적 flush 1회 발생)
        for (int i = 0; i < 35; i++) {
            spdlog::info("Periodic test {}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 크래시
        std::raise(SIGSEGV);

        std::exit(1);

    } else if (pid > 0) {
        // 부모 프로세스
        int status;
        waitpid(pid, &status, 0);

        // Then
        EXPECT_TRUE(WIFSIGNALED(status));

        // 주기적 flush로 인해 거의 100% 보존
        int preserved = count_log_occurrences("Periodic test");
        std::cout << "Periodic flush preserved: " << preserved << " / 35" << std::endl;

        // 최소 34개 이상 보존 (97%)
        EXPECT_GE(preserved, 34);

    } else {
        FAIL() << "fork() failed";
    }
}

}  // namespace mxrc::core::logging
