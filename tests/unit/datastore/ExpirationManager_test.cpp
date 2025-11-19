#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include "managers/ExpirationManager.h"

using namespace std::chrono_literals;

namespace mxrc::core::datastore {

class ExpirationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<ExpirationManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<ExpirationManager> manager_;
};

// T011: 만료 정책 적용 테스트
TEST_F(ExpirationManagerTest, ApplyExpirationPolicy) {
    // Given: 만료 시간 100ms 설정
    auto expiration_time = std::chrono::system_clock::now() + 100ms;

    // When: 만료 정책 적용
    manager_->applyPolicy("key1", expiration_time);

    // Then: 정책이 저장되어야 함
    EXPECT_TRUE(manager_->hasPolicy("key1"));
    EXPECT_EQ(manager_->getExpirationTime("key1"), expiration_time);
}

// T012: 만료 정책 제거 테스트
TEST_F(ExpirationManagerTest, RemoveExpirationPolicy) {
    // Given: 만료 정책 적용
    auto expiration_time = std::chrono::system_clock::now() + 100ms;
    manager_->applyPolicy("key1", expiration_time);
    ASSERT_TRUE(manager_->hasPolicy("key1"));

    // When: 만료 정책 제거
    manager_->removePolicy("key1");

    // Then: 정책이 제거되어야 함
    EXPECT_FALSE(manager_->hasPolicy("key1"));
}

// T013: 만료된 키 수집 테스트 (기본 케이스)
TEST_F(ExpirationManagerTest, GetExpiredKeys_BasicCase) {
    // Given: 과거 시간으로 만료 정책 설정
    auto past_time = std::chrono::system_clock::now() - 100ms;
    auto future_time = std::chrono::system_clock::now() + 1000ms;

    manager_->applyPolicy("expired_key", past_time);
    manager_->applyPolicy("valid_key", future_time);

    // When: 만료된 키 수집
    auto expired_keys = manager_->getExpiredKeys();

    // Then: 만료된 키만 반환되어야 함
    ASSERT_EQ(expired_keys.size(), 1);
    EXPECT_EQ(expired_keys[0], "expired_key");
}

// T014: 만료된 키 수집 테스트 (다중 키)
TEST_F(ExpirationManagerTest, GetExpiredKeys_MultipleKeys) {
    // Given: 여러 키에 만료 정책 설정
    auto now = std::chrono::system_clock::now();
    manager_->applyPolicy("expired1", now - 200ms);
    manager_->applyPolicy("expired2", now - 100ms);
    manager_->applyPolicy("expired3", now - 50ms);
    manager_->applyPolicy("valid1", now + 100ms);
    manager_->applyPolicy("valid2", now + 200ms);

    // When: 만료된 키 수집
    auto expired_keys = manager_->getExpiredKeys();

    // Then: 3개의 만료된 키가 반환되어야 함
    EXPECT_EQ(expired_keys.size(), 3);
    EXPECT_TRUE(std::find(expired_keys.begin(), expired_keys.end(), "expired1") != expired_keys.end());
    EXPECT_TRUE(std::find(expired_keys.begin(), expired_keys.end(), "expired2") != expired_keys.end());
    EXPECT_TRUE(std::find(expired_keys.begin(), expired_keys.end(), "expired3") != expired_keys.end());
}

// T015: 만료 정책 덮어쓰기 테스트
TEST_F(ExpirationManagerTest, OverwriteExpirationPolicy) {
    // Given: 초기 만료 정책 설정
    auto initial_time = std::chrono::system_clock::now() + 100ms;
    manager_->applyPolicy("key1", initial_time);

    // When: 새로운 만료 시간으로 덮어쓰기
    auto new_time = std::chrono::system_clock::now() + 500ms;
    manager_->applyPolicy("key1", new_time);

    // Then: 새로운 만료 시간이 적용되어야 함
    EXPECT_EQ(manager_->getExpirationTime("key1"), new_time);
}

// T016: 빈 상태에서 만료 키 수집 테스트
TEST_F(ExpirationManagerTest, GetExpiredKeys_EmptyState) {
    // Given: 만료 정책이 없는 상태

    // When: 만료된 키 수집
    auto expired_keys = manager_->getExpiredKeys();

    // Then: 빈 벡터가 반환되어야 함
    EXPECT_TRUE(expired_keys.empty());
}

// T017: 존재하지 않는 키 제거 테스트
TEST_F(ExpirationManagerTest, RemoveNonExistentPolicy) {
    // Given: 존재하지 않는 키

    // When: 존재하지 않는 키 제거 시도
    // Then: 예외가 발생하지 않아야 함
    EXPECT_NO_THROW(manager_->removePolicy("non_existent_key"));
}

// T018: 만료 시간 조회 (존재하지 않는 키)
TEST_F(ExpirationManagerTest, GetExpirationTime_NonExistentKey) {
    // Given: 존재하지 않는 키

    // When: 만료 시간 조회
    // Then: 예외가 발생해야 함
    EXPECT_THROW(manager_->getExpirationTime("non_existent_key"), std::out_of_range);
}

// T019: 스레드 안전성 테스트 (동시 정책 적용)
TEST_F(ExpirationManagerTest, ThreadSafety_ConcurrentApplyPolicy) {
    // Given: 10개의 스레드가 동시에 정책 적용
    constexpr int NUM_THREADS = 10;
    constexpr int POLICIES_PER_THREAD = 100;
    std::vector<std::thread> threads;

    // When: 동시에 정책 적용
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            auto future_time = std::chrono::system_clock::now() + 1000ms;
            for (int i = 0; i < POLICIES_PER_THREAD; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                manager_->applyPolicy(key, future_time);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Then: 모든 정책이 저장되어야 함 (1000개)
    // Note: getExpiredKeys()는 만료된 키만 반환하므로 별도의 getPolicyCount() 메서드 필요
    // 여기서는 간접적으로 확인
    EXPECT_EQ(manager_->getPolicyCount(), NUM_THREADS * POLICIES_PER_THREAD);
}

// T020: 성능 테스트 - O(log N) 특성 검증 (1,000개 데이터)
TEST_F(ExpirationManagerTest, Performance_LogNCharacteristic_1000Keys) {
    // Given: 1,000개의 만료 정책 설정 (50%는 만료됨)
    auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key" + std::to_string(i);
        if (i < 500) {
            // 만료된 키
            manager_->applyPolicy(key, now - 100ms);
        } else {
            // 유효한 키
            manager_->applyPolicy(key, now + 1000ms);
        }
    }

    // When: 만료된 키 수집 (시간 측정)
    auto start = std::chrono::high_resolution_clock::now();
    auto expired_keys = manager_->getExpiredKeys();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Then:
    // 1. 500개의 만료된 키가 반환되어야 함
    EXPECT_EQ(expired_keys.size(), 500);

    // 2. 1ms (1000 microseconds) 이내에 완료되어야 함 (Success Criteria SC-005)
    EXPECT_LT(duration.count(), 1000) << "Duration: " << duration.count() << " microseconds";

    // 추가 정보 출력
    std::cout << "Performance: " << expired_keys.size() << " expired keys collected in "
              << duration.count() << " microseconds" << std::endl;
}

// T021: 성능 벤치마크 - 100개 데이터 (<0.1ms)
TEST_F(ExpirationManagerTest, PerformanceBenchmark_100Keys) {
    // Given: 100개의 만료된 키
    auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 100; ++i) {
        manager_->applyPolicy("key" + std::to_string(i), now - 10ms);
    }

    // When: 만료된 키 수집
    auto start = std::chrono::high_resolution_clock::now();
    auto expired_keys = manager_->getExpiredKeys();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Then: 100 microseconds (0.1ms) 이내
    EXPECT_LT(duration.count(), 100) << "Duration: " << duration.count() << " microseconds";
    EXPECT_EQ(expired_keys.size(), 100);
}

// T022: 성능 벤치마크 - 10,000개 데이터 (<10ms)
TEST_F(ExpirationManagerTest, PerformanceBenchmark_10000Keys) {
    // Given: 10,000개의 만료 정책 (50% 만료)
    auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 10000; ++i) {
        std::string key = "key" + std::to_string(i);
        if (i < 5000) {
            manager_->applyPolicy(key, now - 100ms);
        } else {
            manager_->applyPolicy(key, now + 1000ms);
        }
    }

    // When: 만료된 키 수집
    auto start = std::chrono::high_resolution_clock::now();
    auto expired_keys = manager_->getExpiredKeys();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Then: 10ms (10000 microseconds) 이내
    EXPECT_LT(duration.count(), 10000) << "Duration: " << duration.count() << " microseconds";
    EXPECT_EQ(expired_keys.size(), 5000);

    std::cout << "Performance (10K): " << expired_keys.size() << " expired keys collected in "
              << duration.count() << " microseconds" << std::endl;
}

// ============================================================================
// LRU (Least Recently Used) 정책 테스트
// ============================================================================

// T023: LRU 정책 적용 테스트
TEST_F(ExpirationManagerTest, ApplyLRUPolicy) {
    // Given: 5개 용량의 LRU 정책
    size_t capacity = 5;

    // When: LRU 정책 적용
    manager_->applyLRUPolicy("key1", capacity);

    // Then: LRU 추적이 시작되어야 함
    EXPECT_TRUE(manager_->hasLRUPolicy("key1"));
    EXPECT_EQ(manager_->getLRUCapacity(), capacity);
    EXPECT_EQ(manager_->getLRUSize(), 1);
}

// T024: LRU 제거 테스트 (용량=5, 10개 아이템 추가)
TEST_F(ExpirationManagerTest, LRUEviction_Capacity5_Add10Items) {
    // Given: 5개 용량의 LRU 정책으로 10개 키 추가
    size_t capacity = 5;
    for (int i = 0; i < 10; ++i) {
        manager_->applyLRUPolicy("key" + std::to_string(i), capacity);
    }

    // When: LRU 만료 키 조회
    auto expired_keys = manager_->getExpiredKeysLRU();

    // Then: 5개의 가장 오래된 키가 제거되어야 함
    EXPECT_EQ(expired_keys.size(), 5);
    EXPECT_EQ(manager_->getLRUSize(), 5);

    // 제거된 키는 key0 ~ key4 (FIFO 순서)
    for (int i = 0; i < 5; ++i) {
        std::string expected_key = "key" + std::to_string(i);
        EXPECT_TRUE(std::find(expired_keys.begin(), expired_keys.end(), expected_key) != expired_keys.end());
        EXPECT_FALSE(manager_->hasLRUPolicy(expected_key));
    }

    // 남은 키는 key5 ~ key9
    for (int i = 5; i < 10; ++i) {
        EXPECT_TRUE(manager_->hasLRUPolicy("key" + std::to_string(i)));
    }
}

// T025: LRU 접근 패턴 테스트 (중간 항목 접근)
TEST_F(ExpirationManagerTest, LRUAccessPattern_MiddleItemAccess) {
    // Given: 5개 용량, 5개 키 추가
    size_t capacity = 5;
    for (int i = 0; i < 5; ++i) {
        manager_->applyLRUPolicy("key" + std::to_string(i), capacity);
    }

    // When: key2 접근 후 새로운 key5 추가
    manager_->recordAccess("key2");  // key2를 MRU로 이동
    manager_->applyLRUPolicy("key5", capacity);

    // Then: key0가 제거되어야 함 (key2는 최근 접근으로 보호)
    auto expired_keys = manager_->getExpiredKeysLRU();
    EXPECT_EQ(expired_keys.size(), 1);
    EXPECT_EQ(expired_keys[0], "key0");  // LRU는 key0

    // key2는 여전히 추적 중
    EXPECT_TRUE(manager_->hasLRUPolicy("key2"));
}

// T026: TTL과 LRU 혼합 정책 테스트
TEST_F(ExpirationManagerTest, MixedPolicyTTLandLRU) {
    // Given: TTL 정책과 LRU 정책 혼합
    auto now = std::chrono::system_clock::now();

    // TTL 정책 (key1, key2 - 만료됨)
    manager_->applyPolicy("key1", now - 100ms);
    manager_->applyPolicy("key2", now - 50ms);

    // LRU 정책 (key3, key4, key5 - 용량 2)
    manager_->applyLRUPolicy("key3", 2);
    manager_->applyLRUPolicy("key4", 2);
    manager_->applyLRUPolicy("key5", 2);

    // When: TTL 만료 키와 LRU 만료 키 각각 조회
    auto expired_ttl = manager_->getExpiredKeys();
    auto expired_lru = manager_->getExpiredKeysLRU();

    // Then:
    // 1. TTL 만료: key1, key2
    EXPECT_EQ(expired_ttl.size(), 2);
    EXPECT_TRUE(std::find(expired_ttl.begin(), expired_ttl.end(), "key1") != expired_ttl.end());
    EXPECT_TRUE(std::find(expired_ttl.begin(), expired_ttl.end(), "key2") != expired_ttl.end());

    // 2. LRU 만료: key3 (가장 오래된 키)
    EXPECT_EQ(expired_lru.size(), 1);
    EXPECT_EQ(expired_lru[0], "key3");

    // 3. 남은 LRU 키: key4, key5
    EXPECT_TRUE(manager_->hasLRUPolicy("key4"));
    EXPECT_TRUE(manager_->hasLRUPolicy("key5"));
}

// T027: LRU 스레드 안전성 테스트
TEST_F(ExpirationManagerTest, ThreadSafety_ConcurrentLRUOperations) {
    // Given: 10개의 스레드가 동시에 LRU 정책 적용 및 접근
    constexpr int NUM_THREADS = 10;
    constexpr int OPERATIONS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    // When: 동시에 LRU 정책 적용 및 접근
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                manager_->applyLRUPolicy(key, 1000);
                manager_->recordAccess(key);  // 접근 기록
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Then: 모든 키가 LRU 추적 중이어야 함 (1000개)
    EXPECT_EQ(manager_->getLRUSize(), NUM_THREADS * OPERATIONS_PER_THREAD);
}

// T028: LRU recordAccess 성능 테스트 (O(1) 검증)
TEST_F(ExpirationManagerTest, Performance_RecordAccessO1) {
    // Given: 1,000개의 LRU 키
    size_t capacity = 1000;
    for (int i = 0; i < 1000; ++i) {
        manager_->applyLRUPolicy("key" + std::to_string(i), capacity);
    }

    // When: 중간 키(key500) 접근 시간 측정
    auto start = std::chrono::high_resolution_clock::now();
    manager_->recordAccess("key500");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Then: O(1) 시간 복잡도 검증 (1 microsecond = 1000 nanoseconds)
    // 실제로는 수십~수백 nanoseconds 내에 완료되어야 함
    EXPECT_LT(duration.count(), 10000) << "Duration: " << duration.count() << " nanoseconds";

    std::cout << "recordAccess performance: " << duration.count() << " nanoseconds" << std::endl;
}

// T029: LRU getExpiredKeysLRU 성능 테스트 (O(K) 검증)
TEST_F(ExpirationManagerTest, Performance_GetExpiredKeysLRU_OK) {
    // Given: 100개 키, 용량 50 (50개 제거 예정)
    size_t capacity = 50;
    for (int i = 0; i < 100; ++i) {
        manager_->applyLRUPolicy("key" + std::to_string(i), capacity);
    }

    // When: LRU 만료 키 조회 (시간 측정)
    auto start = std::chrono::high_resolution_clock::now();
    auto expired_keys = manager_->getExpiredKeysLRU();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Then:
    // 1. 50개 제거
    EXPECT_EQ(expired_keys.size(), 50);

    // 2. O(K) 성능: K=50이므로 100 microseconds 이내
    EXPECT_LT(duration.count(), 100) << "Duration: " << duration.count() << " microseconds";

    std::cout << "getExpiredKeysLRU performance: " << expired_keys.size()
              << " keys removed in " << duration.count() << " microseconds" << std::endl;
}

// T030: LRU 정책 제거 테스트
TEST_F(ExpirationManagerTest, RemoveLRUPolicy) {
    // Given: LRU 정책 적용
    manager_->applyLRUPolicy("key1", 10);
    ASSERT_TRUE(manager_->hasLRUPolicy("key1"));

    // When: LRU 정책 제거
    manager_->removeLRUPolicy("key1");

    // Then: LRU 추적이 중단되어야 함
    EXPECT_FALSE(manager_->hasLRUPolicy("key1"));
    EXPECT_EQ(manager_->getLRUSize(), 0);
}

// T031: 존재하지 않는 키에 대한 LRU 접근 기록 (무시)
TEST_F(ExpirationManagerTest, RecordAccess_NonExistentKey) {
    // Given: LRU 추적 중이 아닌 키

    // When: 존재하지 않는 키 접근 시도
    // Then: 예외가 발생하지 않아야 함 (무시됨)
    EXPECT_NO_THROW(manager_->recordAccess("non_existent_key"));
}

// T032: 중복 LRU 정책 적용 (무시)
TEST_F(ExpirationManagerTest, ApplyLRUPolicy_Duplicate) {
    // Given: LRU 정책 적용
    manager_->applyLRUPolicy("key1", 10);
    ASSERT_EQ(manager_->getLRUSize(), 1);

    // When: 동일 키에 다시 LRU 정책 적용
    manager_->applyLRUPolicy("key1", 10);

    // Then: 크기가 증가하지 않아야 함 (무시됨)
    EXPECT_EQ(manager_->getLRUSize(), 1);
}

// T033: LRU 용량 이내일 때 제거 없음
TEST_F(ExpirationManagerTest, LRUEviction_WithinCapacity) {
    // Given: 10개 용량, 5개 키 추가
    size_t capacity = 10;
    for (int i = 0; i < 5; ++i) {
        manager_->applyLRUPolicy("key" + std::to_string(i), capacity);
    }

    // When: LRU 만료 키 조회
    auto expired_keys = manager_->getExpiredKeysLRU();

    // Then: 제거할 키가 없어야 함 (용량 이내)
    EXPECT_TRUE(expired_keys.empty());
    EXPECT_EQ(manager_->getLRUSize(), 5);
}

} // namespace mxrc::core::datastore
