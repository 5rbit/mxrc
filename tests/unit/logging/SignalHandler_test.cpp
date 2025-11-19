#include "gtest/gtest.h"
#include "core/logging/Log.h"
#include "core/logging/SignalHandler.h"
#include <spdlog/spdlog.h>
#include <csignal>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace mxrc::core::logging {

class SignalHandlerTest : public ::testing::Test {
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
        // 정리
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 로그 파일에서 특정 문자열 검색
    bool log_file_contains(const std::string& pattern) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::ifstream log_file("logs/mxrc.log");
        if (!log_file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(log_file, line)) {
            if (line.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

// T032: register_signal_handlers() 기본 동작 테스트
TEST_F(SignalHandlerTest, RegisterHandlers) {
    // Given
    initialize_async_logger();

    // When
    ASSERT_NO_THROW(register_signal_handlers());

    // Then
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    shutdown_logger();
    spdlog::drop_all();

    // 로그에 핸들러 등록 메시지 확인
    EXPECT_TRUE(log_file_contains("Signal handlers registered"));
}

// T038: SIGSEGV 로그 보존 테스트 (fork + 로그 확인)
TEST_F(SignalHandlerTest, SIGSEGVPreservesLogs) {
    // Given - 자식 프로세스에서 크래시 유도
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스
        initialize_async_logger();
        register_signal_handlers();

        // 크래시 직전 로그
        spdlog::critical("About to crash via SIGSEGV");

        // 의도적인 segmentation fault
        std::raise(SIGSEGV);

        // 여기는 실행되지 않음
        std::exit(1);

    } else if (pid > 0) {
        // 부모 프로세스 - 자식 종료 대기
        int status;
        waitpid(pid, &status, 0);

        // Then
        // 자식 프로세스가 시그널로 종료되었는지 확인
        EXPECT_TRUE(WIFSIGNALED(status));

        // 로그 파일에 크래시 직전 메시지 확인
        EXPECT_TRUE(log_file_contains("About to crash via SIGSEGV"));
        EXPECT_TRUE(log_file_contains("Signal 11 received"));  // SIGSEGV = 11

    } else {
        // fork 실패
        FAIL() << "fork() failed";
    }
}

// SIGABRT 로그 보존 테스트
TEST_F(SignalHandlerTest, SIGABRTPreservesLogs) {
    // Given
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스
        initialize_async_logger();
        register_signal_handlers();

        spdlog::critical("About to crash via SIGABRT");
        std::raise(SIGABRT);

        std::exit(1);

    } else if (pid > 0) {
        // 부모 프로세스
        int status;
        waitpid(pid, &status, 0);

        // Then
        EXPECT_TRUE(WIFSIGNALED(status));
        EXPECT_TRUE(log_file_contains("About to crash via SIGABRT"));
        EXPECT_TRUE(log_file_contains("Signal 6 received"));  // SIGABRT = 6

    } else {
        FAIL() << "fork() failed";
    }
}

// SIGTERM 핸들러 테스트
TEST_F(SignalHandlerTest, SIGTERMHandling) {
    // Given
    pid_t pid = fork();

    if (pid == 0) {
        // 자식 프로세스
        initialize_async_logger();
        register_signal_handlers();

        spdlog::info("Ready for SIGTERM");

        // SIGTERM 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::exit(0);

    } else if (pid > 0) {
        // 부모 프로세스
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 자식에게 SIGTERM 전송
        kill(pid, SIGTERM);

        int status;
        waitpid(pid, &status, 0);

        // Then
        EXPECT_TRUE(WIFSIGNALED(status));
        EXPECT_TRUE(log_file_contains("Signal 15 received"));  // SIGTERM = 15

    } else {
        FAIL() << "fork() failed";
    }
}

}  // namespace mxrc::core::logging
