#include "gtest/gtest.h"
#include "core/logging/Log.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace mxrc::core::logging {

class AsyncLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 로그 디렉토리가 없으면 생성
        std::filesystem::create_directories("logs");

        // 기존 로그 파일 삭제 (테스트 격리)
        if (std::filesystem::exists("logs/mxrc.log")) {
            std::filesystem::remove("logs/mxrc.log");
        }
    }

    void TearDown() override {
        // 로거 정리
        shutdown_logger();
        spdlog::drop_all();

        // 짧은 대기 (파일 핸들 완전히 닫힐 때까지)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 로그 파일에서 특정 문자열 검색
    bool log_file_contains(const std::string& pattern) {
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

// T016: 초기화 테스트
TEST_F(AsyncLoggerTest, Initialization) {
    // Given & When
    ASSERT_NO_THROW(initialize_async_logger());

    // Then
    auto logger = spdlog::default_logger();
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "mxrc_logger");
}

// T017: 기본 로깅 테스트 (info, debug, warn, error)
TEST_F(AsyncLoggerTest, BasicLogging) {
    // Given
    initialize_async_logger();

    // When
    spdlog::info("Test info message");
    spdlog::debug("Test debug message");
    spdlog::warn("Test warning message");
    spdlog::error("Test error message");

    // 비동기 로거이므로 파일에 쓰일 시간 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    spdlog::default_logger()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then
    EXPECT_TRUE(log_file_contains("Test info message"));
    EXPECT_TRUE(log_file_contains("Test debug message"));
    EXPECT_TRUE(log_file_contains("Test warning message"));
    EXPECT_TRUE(log_file_contains("Test error message"));
}

// T039, T040: CRITICAL flush 테스트 (100ms 이내 파일 기록)
TEST_F(AsyncLoggerTest, CriticalFlush) {
    // Given
    initialize_async_logger();

    // When
    auto start = std::chrono::high_resolution_clock::now();
    spdlog::critical("Critical message");

    // CRITICAL 레벨은 즉시 플러시되므로 짧은 대기 후 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // Then
    EXPECT_LT(duration_ms, 100);  // 100ms 이내 처리
    EXPECT_TRUE(log_file_contains("Critical message"));
}

// 멀티스레드 로깅 테스트
TEST_F(AsyncLoggerTest, MultithreadedLogging) {
    // Given
    initialize_async_logger();

    // When
    const int num_threads = 4;
    const int messages_per_thread = 100;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; j++) {
                spdlog::info("Thread {} message {}", i, j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 모든 메시지가 큐에 쌓일 시간 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    spdlog::default_logger()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then - 로그 파일이 생성되었는지만 확인
    EXPECT_TRUE(std::filesystem::exists("logs/mxrc.log"));

    // 파일 크기가 0보다 큰지 확인
    auto file_size = std::filesystem::file_size("logs/mxrc.log");
    EXPECT_GT(file_size, 0);
}

// 주기적 flush 테스트 (3초 간격)
TEST_F(AsyncLoggerTest, PeriodicFlush) {
    // Given
    initialize_async_logger();

    // When
    spdlog::info("Before flush");

    // 3초 대기하여 주기적 flush 발생 확인
    std::this_thread::sleep_for(std::chrono::milliseconds(3500));

    // Then
    EXPECT_TRUE(log_file_contains("Before flush"));
}

// shutdown_logger() 테스트
TEST_F(AsyncLoggerTest, Shutdown) {
    // Given
    initialize_async_logger();
    spdlog::info("Message before shutdown");

    // When
    ASSERT_NO_THROW(shutdown_logger());

    // 짧은 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then
    EXPECT_TRUE(log_file_contains("Message before shutdown"));
}

}  // namespace mxrc::core::logging
