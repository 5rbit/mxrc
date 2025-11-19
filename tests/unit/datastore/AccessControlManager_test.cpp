#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <algorithm>
#include "managers/AccessControlManager.h"

namespace mxrc::core::datastore {

class AccessControlManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<AccessControlManager>();
    }

    void TearDown() override {
        manager_.reset();
    }

    std::unique_ptr<AccessControlManager> manager_;
};

// T001: 접근 정책 설정 테스트
TEST_F(AccessControlManagerTest, SetPolicy) {
    // Given: 키와 모듈 ID

    // When: 접근 허용 정책 설정
    manager_->setPolicy("key1", "module_A", true);

    // Then: 접근이 허용되어야 함
    EXPECT_TRUE(manager_->hasAccess("key1", "module_A"));
}

// T002: 접근 거부 정책 설정 테스트
TEST_F(AccessControlManagerTest, SetPolicy_Deny) {
    // Given: 키와 모듈 ID

    // When: 접근 거부 정책 설정
    manager_->setPolicy("key1", "module_B", false);

    // Then: 접근이 거부되어야 함
    EXPECT_FALSE(manager_->hasAccess("key1", "module_B"));
}

// T003: 기본 정책 테스트 (정책 없을 때 거부)
TEST_F(AccessControlManagerTest, DefaultPolicy_DenyAccess) {
    // Given: 정책이 설정되지 않은 상태

    // When: 접근 권한 확인
    // Then: 기본값으로 거부되어야 함
    EXPECT_FALSE(manager_->hasAccess("non_existent_key", "module_C"));
}

// T004: 다중 모듈 접근 정책 테스트
TEST_F(AccessControlManagerTest, MultipleModulePolicies) {
    // Given: 하나의 키에 여러 모듈 정책 설정
    manager_->setPolicy("shared_key", "module_A", true);
    manager_->setPolicy("shared_key", "module_B", false);
    manager_->setPolicy("shared_key", "module_C", true);

    // When: 각 모듈의 접근 권한 확인
    // Then: 각 모듈별로 다른 접근 권한을 가져야 함
    EXPECT_TRUE(manager_->hasAccess("shared_key", "module_A"));
    EXPECT_FALSE(manager_->hasAccess("shared_key", "module_B"));
    EXPECT_TRUE(manager_->hasAccess("shared_key", "module_C"));
}

// T005: 정책 덮어쓰기 테스트
TEST_F(AccessControlManagerTest, OverwritePolicy) {
    // Given: 초기 정책 설정 (거부)
    manager_->setPolicy("key1", "module_A", false);
    ASSERT_FALSE(manager_->hasAccess("key1", "module_A"));

    // When: 정책 덮어쓰기 (허용)
    manager_->setPolicy("key1", "module_A", true);

    // Then: 새로운 정책이 적용되어야 함
    EXPECT_TRUE(manager_->hasAccess("key1", "module_A"));
}

// T006: 키별 정책 제거 테스트
TEST_F(AccessControlManagerTest, RemovePolicy_ByKey) {
    // Given: 여러 모듈에 대한 정책 설정
    manager_->setPolicy("key1", "module_A", true);
    manager_->setPolicy("key1", "module_B", true);
    ASSERT_TRUE(manager_->hasAccess("key1", "module_A"));
    ASSERT_TRUE(manager_->hasAccess("key1", "module_B"));

    // When: 키별 정책 제거
    manager_->removePolicy("key1");

    // Then: 모든 모듈의 접근이 거부되어야 함
    EXPECT_FALSE(manager_->hasAccess("key1", "module_A"));
    EXPECT_FALSE(manager_->hasAccess("key1", "module_B"));
    EXPECT_FALSE(manager_->hasPolicy("key1"));
}

// T007: 키-모듈 조합 정책 제거 테스트
TEST_F(AccessControlManagerTest, RemovePolicy_ByKeyAndModule) {
    // Given: 하나의 키에 여러 모듈 정책 설정
    manager_->setPolicy("key1", "module_A", true);
    manager_->setPolicy("key1", "module_B", true);

    // When: 특정 키-모듈 조합 정책 제거
    manager_->removePolicy("key1", "module_A");

    // Then: 해당 모듈은 거부, 다른 모듈은 유지
    EXPECT_FALSE(manager_->hasAccess("key1", "module_A"));
    EXPECT_TRUE(manager_->hasAccess("key1", "module_B"));
    EXPECT_TRUE(manager_->hasPolicy("key1"));  // key1에 대한 정책은 여전히 존재
}

// T008: 존재하지 않는 정책 제거 테스트
TEST_F(AccessControlManagerTest, RemoveNonExistentPolicy) {
    // Given: 존재하지 않는 키

    // When: 존재하지 않는 키 제거 시도
    // Then: 예외가 발생하지 않아야 함
    EXPECT_NO_THROW(manager_->removePolicy("non_existent_key"));
    EXPECT_NO_THROW(manager_->removePolicy("non_existent_key", "module_X"));
}

// T009: 전체 정책 조회 테스트
TEST_F(AccessControlManagerTest, GetAllPolicies) {
    // Given: 여러 키와 모듈에 대한 정책 설정
    manager_->setPolicy("key1", "module_A", true);
    manager_->setPolicy("key1", "module_B", false);
    manager_->setPolicy("key2", "module_C", true);

    // When: 전체 정책 조회
    auto all_policies = manager_->getAllPolicies();

    // Then: 모든 정책이 반환되어야 함
    EXPECT_EQ(all_policies.size(), 2);  // key1, key2
    EXPECT_EQ(all_policies["key1"].size(), 2);  // module_A, module_B
    EXPECT_EQ(all_policies["key2"].size(), 1);  // module_C

    EXPECT_TRUE(all_policies["key1"]["module_A"]);
    EXPECT_FALSE(all_policies["key1"]["module_B"]);
    EXPECT_TRUE(all_policies["key2"]["module_C"]);
}

// T010: 정책 존재 여부 확인 테스트
TEST_F(AccessControlManagerTest, HasPolicy) {
    // Given: 정책 설정
    manager_->setPolicy("key1", "module_A", true);

    // When: 정책 존재 여부 확인
    // Then: 설정된 키는 true, 그렇지 않은 키는 false
    EXPECT_TRUE(manager_->hasPolicy("key1"));
    EXPECT_FALSE(manager_->hasPolicy("non_existent_key"));
}

// T011: 정책 개수 조회 테스트
TEST_F(AccessControlManagerTest, GetPolicyCount) {
    // Given: 초기 상태
    EXPECT_EQ(manager_->getPolicyCount(), 0);

    // When: 정책 추가
    manager_->setPolicy("key1", "module_A", true);
    manager_->setPolicy("key2", "module_B", true);
    manager_->setPolicy("key3", "module_C", false);

    // Then: 정책 개수가 증가해야 함
    EXPECT_EQ(manager_->getPolicyCount(), 3);

    // When: 정책 제거
    manager_->removePolicy("key1");

    // Then: 정책 개수가 감소해야 함
    EXPECT_EQ(manager_->getPolicyCount(), 2);
}

// T012: 스레드 안전성 테스트 - 동시 읽기
TEST_F(AccessControlManagerTest, ThreadSafety_ConcurrentRead) {
    // Given: 정책 설정
    manager_->setPolicy("shared_key", "module_A", true);
    manager_->setPolicy("shared_key", "module_B", false);

    // When: 10개의 스레드가 동시에 읽기
    constexpr int NUM_THREADS = 10;
    constexpr int READS_PER_THREAD = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, &success_count]() {
            for (int i = 0; i < READS_PER_THREAD; ++i) {
                if (manager_->hasAccess("shared_key", "module_A") &&
                    !manager_->hasAccess("shared_key", "module_B")) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Then: 모든 읽기가 성공해야 함
    EXPECT_EQ(success_count.load(), NUM_THREADS * READS_PER_THREAD);
}

// T013: 스레드 안전성 테스트 - 동시 쓰기
TEST_F(AccessControlManagerTest, ThreadSafety_ConcurrentWrite) {
    // Given: 10개의 스레드가 동시에 정책 설정
    constexpr int NUM_THREADS = 10;
    constexpr int POLICIES_PER_THREAD = 100;
    std::vector<std::thread> threads;

    // When: 동시에 정책 설정
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < POLICIES_PER_THREAD; ++i) {
                std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                std::string module = "module" + std::to_string(t);
                manager_->setPolicy(key, module, true);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Then: 모든 정책이 저장되어야 함 (1000개의 키)
    EXPECT_EQ(manager_->getPolicyCount(), NUM_THREADS * POLICIES_PER_THREAD);
}

// T014: 스레드 안전성 테스트 - 동시 읽기/쓰기
TEST_F(AccessControlManagerTest, ThreadSafety_ConcurrentReadWrite) {
    // Given: 초기 정책 설정
    for (int i = 0; i < 100; ++i) {
        manager_->setPolicy("key" + std::to_string(i), "module_A", true);
    }

    // When: 읽기 스레드와 쓰기 스레드 동시 실행
    constexpr int NUM_READERS = 5;
    constexpr int NUM_WRITERS = 5;
    std::vector<std::thread> threads;
    std::atomic<bool> all_reads_consistent{true};

    // 읽기 스레드
    for (int t = 0; t < NUM_READERS; ++t) {
        threads.emplace_back([this, &all_reads_consistent]() {
            for (int i = 0; i < 1000; ++i) {
                std::string key = "key" + std::to_string(i % 100);
                // shared_lock으로 보호된 읽기
                bool has_access = manager_->hasAccess(key, "module_A");
                // 일관성 확인 (읽기 중에는 값이 변하지 않아야 함)
                if (has_access != manager_->hasAccess(key, "module_A")) {
                    all_reads_consistent = false;
                }
            }
        });
    }

    // 쓰기 스레드
    for (int t = 0; t < NUM_WRITERS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 100; ++i) {
                std::string key = "new_key" + std::to_string(t * 100 + i);
                manager_->setPolicy(key, "module_B", false);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Then: 읽기 일관성 유지 및 쓰기 완료
    EXPECT_TRUE(all_reads_consistent);
    EXPECT_GE(manager_->getPolicyCount(), 100);  // 최소 초기 100개 + 새로 추가된 정책들
}

// T015: 빈 상태 테스트
TEST_F(AccessControlManagerTest, EmptyState) {
    // Given: 빈 상태

    // When: 전체 정책 조회
    auto all_policies = manager_->getAllPolicies();

    // Then: 빈 맵이 반환되어야 함
    EXPECT_TRUE(all_policies.empty());
    EXPECT_EQ(manager_->getPolicyCount(), 0);
    EXPECT_FALSE(manager_->hasPolicy("any_key"));
}

} // namespace mxrc::core::datastore
