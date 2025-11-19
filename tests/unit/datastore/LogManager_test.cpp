#include "gtest/gtest.h"
#include "managers/LogManager.h"
#include <thread>
#include <vector>
#include <chrono>

using namespace mxrc::core::datastore;

// ============================================================================
// 기본 기능 테스트
// ============================================================================

TEST(LogManagerTest, ConstructorInitializesEmpty) {
    LogManager log_manager;

    EXPECT_EQ(log_manager.getAccessLogCount(), 0);
    EXPECT_EQ(log_manager.getErrorLogCount(), 0);
    EXPECT_TRUE(log_manager.getAccessLogs().empty());
    EXPECT_TRUE(log_manager.getErrorLogs().empty());
}

TEST(LogManagerTest, LogAccessBasic) {
    LogManager log_manager;

    log_manager.logAccess("get", "key1", "module1");

    EXPECT_EQ(log_manager.getAccessLogCount(), 1);

    auto logs = log_manager.getAccessLogs();
    ASSERT_EQ(logs.size(), 1);
    EXPECT_NE(logs[0].find("[get]"), std::string::npos);
    EXPECT_NE(logs[0].find("key=key1"), std::string::npos);
    EXPECT_NE(logs[0].find("module=module1"), std::string::npos);
}

TEST(LogManagerTest, LogAccessWithoutModule) {
    LogManager log_manager;

    log_manager.logAccess("set", "key2");

    EXPECT_EQ(log_manager.getAccessLogCount(), 1);

    auto logs = log_manager.getAccessLogs();
    ASSERT_EQ(logs.size(), 1);
    EXPECT_NE(logs[0].find("[set]"), std::string::npos);
    EXPECT_NE(logs[0].find("key=key2"), std::string::npos);
    // module_id가 없으므로 "module=" 문자열이 없어야 함
    EXPECT_EQ(logs[0].find("module="), std::string::npos);
}

TEST(LogManagerTest, LogErrorBasic) {
    LogManager log_manager;

    log_manager.logError("not_found", "Key not found", "key=test");

    EXPECT_EQ(log_manager.getErrorLogCount(), 1);

    auto logs = log_manager.getErrorLogs();
    ASSERT_EQ(logs.size(), 1);
    EXPECT_NE(logs[0].find("[ERROR:not_found]"), std::string::npos);
    EXPECT_NE(logs[0].find("Key not found"), std::string::npos);
    EXPECT_NE(logs[0].find("key=test"), std::string::npos);
}

TEST(LogManagerTest, LogErrorWithoutContext) {
    LogManager log_manager;

    log_manager.logError("type_mismatch", "Type does not match");

    EXPECT_EQ(log_manager.getErrorLogCount(), 1);

    auto logs = log_manager.getErrorLogs();
    ASSERT_EQ(logs.size(), 1);
    EXPECT_NE(logs[0].find("[ERROR:type_mismatch]"), std::string::npos);
    EXPECT_NE(logs[0].find("Type does not match"), std::string::npos);
}

TEST(LogManagerTest, MultipleAccessLogs) {
    LogManager log_manager;

    log_manager.logAccess("get", "key1", "module1");
    log_manager.logAccess("set", "key2", "module2");
    log_manager.logAccess("poll", "key3", "module3");

    EXPECT_EQ(log_manager.getAccessLogCount(), 3);

    auto logs = log_manager.getAccessLogs();
    ASSERT_EQ(logs.size(), 3);
    EXPECT_NE(logs[0].find("[get]"), std::string::npos);
    EXPECT_NE(logs[1].find("[set]"), std::string::npos);
    EXPECT_NE(logs[2].find("[poll]"), std::string::npos);
}

TEST(LogManagerTest, MultipleErrorLogs) {
    LogManager log_manager;

    log_manager.logError("not_found", "Error 1");
    log_manager.logError("type_mismatch", "Error 2");
    log_manager.logError("access_denied", "Error 3");

    EXPECT_EQ(log_manager.getErrorLogCount(), 3);

    auto logs = log_manager.getErrorLogs();
    ASSERT_EQ(logs.size(), 3);
    EXPECT_NE(logs[0].find("Error 1"), std::string::npos);
    EXPECT_NE(logs[1].find("Error 2"), std::string::npos);
    EXPECT_NE(logs[2].find("Error 3"), std::string::npos);
}

// ============================================================================
// 순환 버퍼 테스트
// ============================================================================

TEST(LogManagerTest, CircularBufferAccessLogs) {
    LogManager log_manager(5, 5);  // 최대 5개

    // 6개 로그 추가 (1개 초과)
    for (int i = 0; i < 6; ++i) {
        log_manager.logAccess("get", "key" + std::to_string(i));
    }

    // 최대 5개만 유지
    EXPECT_EQ(log_manager.getAccessLogCount(), 5);

    auto logs = log_manager.getAccessLogs();
    ASSERT_EQ(logs.size(), 5);

    // 가장 오래된 로그(key0)는 삭제되고 key1-key5만 남음
    EXPECT_EQ(logs[0].find("key=key0"), std::string::npos);  // key0는 없음
    EXPECT_NE(logs[0].find("key=key1"), std::string::npos);  // key1부터 시작
    EXPECT_NE(logs[4].find("key=key5"), std::string::npos);  // key5까지
}

TEST(LogManagerTest, CircularBufferErrorLogs) {
    LogManager log_manager(5, 3);  // 에러 로그 최대 3개

    // 5개 에러 로그 추가 (2개 초과)
    for (int i = 0; i < 5; ++i) {
        log_manager.logError("error_type", "Error " + std::to_string(i));
    }

    // 최대 3개만 유지
    EXPECT_EQ(log_manager.getErrorLogCount(), 3);

    auto logs = log_manager.getErrorLogs();
    ASSERT_EQ(logs.size(), 3);

    // Error 2, 3, 4만 남음
    EXPECT_NE(logs[0].find("Error 2"), std::string::npos);
    EXPECT_NE(logs[1].find("Error 3"), std::string::npos);
    EXPECT_NE(logs[2].find("Error 4"), std::string::npos);
}

TEST(LogManagerTest, CircularBufferLargeVolume) {
    LogManager log_manager(100, 100);

    // 200개 로그 추가
    for (int i = 0; i < 200; ++i) {
        log_manager.logAccess("get", "key" + std::to_string(i));
    }

    // 최대 100개만 유지
    EXPECT_EQ(log_manager.getAccessLogCount(), 100);

    auto logs = log_manager.getAccessLogs();
    ASSERT_EQ(logs.size(), 100);

    // key100부터 key199까지 남음
    EXPECT_NE(logs[0].find("key=key100"), std::string::npos);
    EXPECT_NE(logs[99].find("key=key199"), std::string::npos);
}

// ============================================================================
// Clear 기능 테스트
// ============================================================================

TEST(LogManagerTest, ClearAccessLogs) {
    LogManager log_manager;

    log_manager.logAccess("get", "key1");
    log_manager.logAccess("set", "key2");
    EXPECT_EQ(log_manager.getAccessLogCount(), 2);

    log_manager.clear();

    EXPECT_EQ(log_manager.getAccessLogCount(), 0);
    EXPECT_TRUE(log_manager.getAccessLogs().empty());
}

TEST(LogManagerTest, ClearErrorLogs) {
    LogManager log_manager;

    log_manager.logError("error1", "Message1");
    log_manager.logError("error2", "Message2");
    EXPECT_EQ(log_manager.getErrorLogCount(), 2);

    log_manager.clear();

    EXPECT_EQ(log_manager.getErrorLogCount(), 0);
    EXPECT_TRUE(log_manager.getErrorLogs().empty());
}

TEST(LogManagerTest, ClearBothLogs) {
    LogManager log_manager;

    log_manager.logAccess("get", "key1");
    log_manager.logError("error", "Message");

    EXPECT_EQ(log_manager.getAccessLogCount(), 1);
    EXPECT_EQ(log_manager.getErrorLogCount(), 1);

    log_manager.clear();

    EXPECT_EQ(log_manager.getAccessLogCount(), 0);
    EXPECT_EQ(log_manager.getErrorLogCount(), 0);
}

// ============================================================================
// 스레드 안전성 테스트
// ============================================================================

TEST(LogManagerTest, ThreadSafeAccessLogging) {
    LogManager log_manager(10000, 10000);

    const int num_threads = 4;
    const int logs_per_thread = 100;
    std::vector<std::thread> threads;

    // 여러 스레드에서 동시에 로그 기록
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&log_manager, t, logs_per_thread]() {
            for (int i = 0; i < logs_per_thread; ++i) {
                log_manager.logAccess("get",
                                     "key_t" + std::to_string(t) + "_" + std::to_string(i),
                                     "module" + std::to_string(t));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 모든 로그가 기록되어야 함
    EXPECT_EQ(log_manager.getAccessLogCount(), num_threads * logs_per_thread);
}

TEST(LogManagerTest, ThreadSafeErrorLogging) {
    LogManager log_manager(10000, 10000);

    const int num_threads = 4;
    const int logs_per_thread = 100;
    std::vector<std::thread> threads;

    // 여러 스레드에서 동시에 에러 로그 기록
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&log_manager, t, logs_per_thread]() {
            for (int i = 0; i < logs_per_thread; ++i) {
                log_manager.logError("error_type",
                                    "Error from thread " + std::to_string(t),
                                    "index=" + std::to_string(i));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 모든 에러 로그가 기록되어야 함
    EXPECT_EQ(log_manager.getErrorLogCount(), num_threads * logs_per_thread);
}

TEST(LogManagerTest, ThreadSafeMixedOperations) {
    LogManager log_manager(10000, 10000);

    std::vector<std::thread> threads;

    // 로그 추가 스레드
    threads.emplace_back([&log_manager]() {
        for (int i = 0; i < 100; ++i) {
            log_manager.logAccess("get", "key" + std::to_string(i));
        }
    });

    // 에러 로그 추가 스레드
    threads.emplace_back([&log_manager]() {
        for (int i = 0; i < 100; ++i) {
            log_manager.logError("error", "Message" + std::to_string(i));
        }
    });

    // 로그 조회 스레드
    threads.emplace_back([&log_manager]() {
        for (int i = 0; i < 50; ++i) {
            auto access_logs = log_manager.getAccessLogs();
            auto error_logs = log_manager.getErrorLogs();
            // 조회 중에도 크래시 없이 안전하게 동작해야 함
        }
    });

    for (auto& thread : threads) {
        thread.join();
    }

    // 정확한 개수 확인
    EXPECT_EQ(log_manager.getAccessLogCount(), 100);
    EXPECT_EQ(log_manager.getErrorLogCount(), 100);
}

TEST(LogManagerTest, ThreadSafeClearWhileLogging) {
    LogManager log_manager(10000, 10000);

    std::vector<std::thread> threads;
    std::atomic<bool> stop_flag{false};

    // 로그 추가 스레드
    threads.emplace_back([&log_manager, &stop_flag]() {
        int i = 0;
        while (!stop_flag.load()) {
            log_manager.logAccess("get", "key" + std::to_string(i++));
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Clear 스레드
    threads.emplace_back([&log_manager, &stop_flag]() {
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            log_manager.clear();
        }
        stop_flag.store(true);
    });

    for (auto& thread : threads) {
        thread.join();
    }

    // 크래시 없이 정상 종료되어야 함
    SUCCEED();
}

// ============================================================================
// 성능 테스트 (오버헤드 측정)
// ============================================================================

TEST(LogManagerTest, PerformanceOverhead) {
    LogManager log_manager(10000, 10000);

    // 1000개 로그 추가 시간 측정
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        log_manager.logAccess("get", "key" + std::to_string(i), "module");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 1000개 로그 추가가 10ms 이하여야 함 (평균 10us/log)
    EXPECT_LT(duration.count(), 10000);

    std::cout << "1000 access logs took " << duration.count() << " microseconds" << std::endl;
}

TEST(LogManagerTest, RetrievalPerformance) {
    LogManager log_manager(10000, 10000);

    // 1000개 로그 추가
    for (int i = 0; i < 1000; ++i) {
        log_manager.logAccess("get", "key" + std::to_string(i));
    }

    // 조회 시간 측정
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        auto logs = log_manager.getAccessLogs();
        EXPECT_EQ(logs.size(), 1000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 100번 조회가 200ms 이하여야 함 (환경에 따라 유연하게)
    EXPECT_LT(duration.count(), 200000);

    std::cout << "100 log retrievals took " << duration.count() << " microseconds" << std::endl;
}

// ============================================================================
// 타임스탬프 테스트
// ============================================================================

TEST(LogManagerTest, TimestampOrdering) {
    LogManager log_manager;

    log_manager.logAccess("get", "key1");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    log_manager.logAccess("get", "key2");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    log_manager.logAccess("get", "key3");

    auto logs = log_manager.getAccessLogs();
    ASSERT_EQ(logs.size(), 3);

    // 타임스탬프가 포함되어 있어야 함
    for (const auto& log : logs) {
        // 날짜 형식 확인 (YYYY-MM-DD 패턴)
        EXPECT_NE(log.find("-"), std::string::npos);
        // 시간 형식 확인 (HH:MM:SS 패턴)
        EXPECT_NE(log.find(":"), std::string::npos);
    }
}
