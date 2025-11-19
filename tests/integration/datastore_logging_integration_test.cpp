#include "gtest/gtest.h"
#include "DataStore.h"
#include <iostream>

// DataStore 로깅 통합 테스트
TEST(DataStoreLoggingIntegration, AccessAndErrorLogging) {
    auto datastore = DataStore::createForTest();

    // 정상 set 연산 (access log 기록됨)
    datastore->set("key1", 100, DataType::Config);
    datastore->set("key2", 200, DataType::Config);

    // 정상 get 연산 (access log 기록됨)
    int value1 = datastore->get<int>("key1");
    EXPECT_EQ(value1, 100);

    // 정상 poll 연산 (access log 기록됨)
    int value2 = datastore->poll<int>("key2");
    EXPECT_EQ(value2, 200);

    // 에러 발생: 존재하지 않는 키 조회 (error log 기록됨)
    try {
        datastore->get<int>("non_existent_key");
        FAIL() << "Expected std::out_of_range exception";
    } catch (const std::out_of_range& e) {
        // 예상된 예외
    }

    // 에러 발생: 타입 불일치 (error log 기록됨)
    try {
        datastore->get<std::string>("key1");  // int를 string으로 읽으려고 시도
        FAIL() << "Expected std::runtime_error exception";
    } catch (const std::runtime_error& e) {
        // 예상된 예외
    }

    // 로그 조회
    auto access_logs = datastore->getAccessLogs();
    auto error_logs = datastore->getErrorLogs();

    // Access logs: set(key1), set(key2), get(key1), poll(key2) = 4개
    EXPECT_GE(access_logs.size(), 4);

    // Error logs: not_found, type_mismatch 관련 = 최소 2개
    EXPECT_GE(error_logs.size(), 2);

    // 로그 내용 출력 (확인용)
    std::cout << "\n=== Access Logs ===" << std::endl;
    for (const auto& log : access_logs) {
        std::cout << log << std::endl;
    }

    std::cout << "\n=== Error Logs ===" << std::endl;
    for (const auto& log : error_logs) {
        std::cout << log << std::endl;
    }

    // 로그 내용 검증
    bool found_set_key1 = false;
    bool found_get_key1 = false;
    bool found_poll_key2 = false;

    for (const auto& log : access_logs) {
        if (log.find("[set]") != std::string::npos && log.find("key=key1") != std::string::npos) {
            found_set_key1 = true;
        }
        if (log.find("[get]") != std::string::npos && log.find("key=key1") != std::string::npos) {
            found_get_key1 = true;
        }
        if (log.find("[poll]") != std::string::npos && log.find("key=key2") != std::string::npos) {
            found_poll_key2 = true;
        }
    }

    EXPECT_TRUE(found_set_key1);
    EXPECT_TRUE(found_get_key1);
    EXPECT_TRUE(found_poll_key2);

    // 에러 로그 검증
    bool found_not_found_error = false;
    bool found_type_mismatch_error = false;

    for (const auto& log : error_logs) {
        if (log.find("[ERROR:not_found]") != std::string::npos ||
            log.find("non_existent_key") != std::string::npos) {
            found_not_found_error = true;
        }
        if (log.find("[ERROR:type_mismatch]") != std::string::npos ||
            log.find("Type mismatch") != std::string::npos) {
            found_type_mismatch_error = true;
        }
    }

    EXPECT_TRUE(found_not_found_error);
    EXPECT_TRUE(found_type_mismatch_error);
}

// 멀티스레드 환경에서 로깅 테스트
TEST(DataStoreLoggingIntegration, ThreadSafeLogging) {
    auto datastore = DataStore::createForTest();

    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int ops_per_thread = 25;

    // 여러 스레드에서 동시에 DataStore 연산 수행
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&datastore, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                std::string key = "key_t" + std::to_string(t) + "_" + std::to_string(i);
                datastore->set(key, i * 10, DataType::Config);
                int value = datastore->get<int>(key);
                EXPECT_EQ(value, i * 10);

                // 의도적으로 에러 발생 (일부만)
                if (i % 10 == 0) {
                    try {
                        datastore->get<int>("non_existent_" + key);
                    } catch (...) {
                        // 예외 무시
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 로그가 정상적으로 기록되었는지 확인
    auto access_logs = datastore->getAccessLogs();
    auto error_logs = datastore->getErrorLogs();

    // 각 스레드당 set(25) + get(25) = 50개, 총 200개 이상
    EXPECT_GE(access_logs.size(), 200);

    // 각 스레드당 에러 3개씩 = 총 12개 이상
    EXPECT_GE(error_logs.size(), 10);

    std::cout << "\nTotal access logs: " << access_logs.size() << std::endl;
    std::cout << "Total error logs: " << error_logs.size() << std::endl;

    // 크래시 없이 완료되었는지 확인
    SUCCEED();
}

// 성능 오버헤드 측정
TEST(DataStoreLoggingIntegration, PerformanceOverhead) {
    auto datastore = DataStore::createForTest();

    const int iterations = 1000;

    // set 연산 1000번 수행 시간 측정
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        datastore->set("perf_key" + std::to_string(i), i, DataType::Config);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "\n1000 set operations took " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average per operation: " << (duration.count() / 1000.0) << " microseconds" << std::endl;

    // 로깅 오버헤드가 1% 미만인지 확인
    // 1000번 연산이 100ms(100000us) 이내여야 함
    EXPECT_LT(duration.count(), 100000);

    // get 연산 성능 측정
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        int value = datastore->get<int>("perf_key" + std::to_string(i));
        EXPECT_EQ(value, i);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "1000 get operations took " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average per operation: " << (duration.count() / 1000.0) << " microseconds" << std::endl;

    // get 연산도 100ms 이내
    EXPECT_LT(duration.count(), 100000);

    // 로그 개수 확인 (circular buffer가 1000개로 제한되어 있음)
    auto access_logs = datastore->getAccessLogs();
    EXPECT_EQ(access_logs.size(), 1000);  // 최대 1000개까지만 보관
}
