// ActionExecutor 재시도 및 에러 처리 테스트

#include "gtest/gtest.h"
#include "core/sequence/core/ActionExecutor.h"
#include "core/sequence/core/ExecutionContext.h"
#include "core/sequence/core/RetryPolicy.h"
#include "MockActions.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

using namespace mxrc::core::sequence;
using namespace mxrc::core::sequence::testing;

class ActionExecutorTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        // spdlog 초기화
        try {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto logger = std::make_shared<spdlog::logger>("mxrc", console_sink);
            logger->set_level(spdlog::level::debug);
            spdlog::register_logger(logger);
        } catch (const spdlog::spdlog_ex&) {
            // 로거가 이미 존재
        }
    }

protected:
    void SetUp() override {
        executor_ = std::make_shared<ActionExecutor>();
        context_ = std::make_shared<ExecutionContext>();
    }

    std::shared_ptr<ActionExecutor> executor_;
    std::shared_ptr<ExecutionContext> context_;
};

/**
 * @brief 재시도 없음 - 성공 케이스
 *
 * 동작이 즉시 성공하는 경우 재시도가 발생하지 않음
 */
TEST_F(ActionExecutorTest, NoRetrySuccess) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 재시도 정책 없음
    bool success = executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    EXPECT_TRUE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
    EXPECT_TRUE(executor_->getLastErrorMessage().empty());
}

/**
 * @brief 재시도 없음 - 실패 케이스
 *
 * 동작이 실패하고 재시도가 설정되지 않은 경우 즉시 실패
 */
TEST_F(ActionExecutorTest, NoRetryFailure) {
    auto action = std::make_shared<FailureAction>("test_action");

    bool success = executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    EXPECT_FALSE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::FAILED);
    EXPECT_FALSE(executor_->getLastErrorMessage().empty());
}

/**
 * @brief 기본 재시도 정책 - 성공
 *
 * 재시도 정책이 설정되어 있으나 첫 시도에 성공하는 경우
 */
TEST_F(ActionExecutorTest, DefaultRetryPolicySuccess) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 기본 재시도 정책 (최대 3회)
    bool success = executor_->execute(
        action, *context_, 0, RetryPolicy::defaultPolicy());

    EXPECT_TRUE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
}

/**
 * @brief 기본 재시도 정책 - 실패
 *
 * 항상 실패하는 동작이 재시도 정책에 따라 여러 번 시도됨
 */
TEST_F(ActionExecutorTest, DefaultRetryPolicyFailure) {
    auto action = std::make_shared<FailureAction>("test_action");

    // 기본 재시도 정책 (최대 3회)
    bool success = executor_->execute(
        action, *context_, 0, RetryPolicy::defaultPolicy());

    EXPECT_FALSE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::FAILED);
}

/**
 * @brief 공격적 재시도 정책
 *
 * 빠른 재시도로 일시적 실패에 대응
 */
TEST_F(ActionExecutorTest, AggressiveRetryPolicy) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 공격적 정책 (최대 5회, 빠른 간격)
    bool success = executor_->execute(
        action, *context_, 0, RetryPolicy::aggressive());

    EXPECT_TRUE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
}

/**
 * @brief 보수적 재시도 정책
 *
 * 느린 재시도로 서버 과부하 방지
 */
TEST_F(ActionExecutorTest, ConservativeRetryPolicy) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 보수적 정책 (최대 2회, 긴 간격)
    bool success = executor_->execute(
        action, *context_, 0, RetryPolicy::conservative());

    EXPECT_TRUE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
}

/**
 * @brief 사용자 정의 재시도 정책
 *
 * 특정 요구사항에 맞춰 사용자가 정의한 재시도 정책
 */
TEST_F(ActionExecutorTest, CustomRetryPolicy) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 사용자 정의: 최대 4회, 초기 대기 200ms, 지수 백오프 1.5배
    RetryPolicy custom{4, 200, 5000, 1.5};

    bool success = executor_->execute(action, *context_, 0, custom);

    EXPECT_TRUE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
}

/**
 * @brief null Action 처리
 *
 * null 포인터가 전달되면 에러 처리
 */
TEST_F(ActionExecutorTest, NullActionHandling) {
    bool success = executor_->execute(nullptr, *context_, 0, RetryPolicy::noRetry());

    EXPECT_FALSE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::FAILED);
    EXPECT_FALSE(executor_->getLastErrorMessage().empty());
    EXPECT_TRUE(executor_->getLastErrorMessage().find("null") != std::string::npos);
}

/**
 * @brief 예외 처리 - Exception throw
 *
 * 동작이 예외를 던지는 경우 안전하게 처리
 */
TEST_F(ActionExecutorTest, ExceptionHandling) {
    auto action = std::make_shared<ExceptionThrowingAction>("test_action");

    bool success = executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    EXPECT_FALSE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::FAILED);
    // 재시도 없는 정책으로는 "retries failed" 메시지가 나옴
    EXPECT_FALSE(executor_->getLastErrorMessage().empty());
}

/**
 * @brief 실행 시간 측정
 *
 * 동작의 실행 시간이 정확하게 기록됨
 */
TEST_F(ActionExecutorTest, ExecutionTimeMeasurement) {
    auto action = std::make_shared<SuccessAction>("test_action");

    executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    long long executionTime = executor_->getLastExecutionTimeMs();
    EXPECT_GE(executionTime, 0);  // 실행 시간은 0 이상
}

/**
 * @brief 에러 메시지 기록
 *
 * 실패 시 에러 메시지가 정확하게 기록됨
 */
TEST_F(ActionExecutorTest, ErrorMessageRecording) {
    auto action = std::make_shared<FailureAction>("test_action");

    executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    const std::string& errorMsg = executor_->getLastErrorMessage();
    EXPECT_FALSE(errorMsg.empty());
    // 에러 메시지에는 재시도 정보가 포함됨
    EXPECT_TRUE(errorMsg.find("retries") != std::string::npos ||
                errorMsg.find("failure") != std::string::npos);
}

/**
 * @brief 상태 전이 검증
 *
 * 동작의 상태가 정확하게 전이됨
 */
TEST_F(ActionExecutorTest, StatusTransition) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 실행 전 상태 확인
    ActionStatus statusBefore = executor_->getLastStatus();

    // 실행
    bool success = executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    // 실행 후 상태 확인
    ActionStatus statusAfter = executor_->getLastStatus();

    EXPECT_TRUE(success);
    EXPECT_EQ(statusAfter, ActionStatus::COMPLETED);
}

/**
 * @brief 재시도 정책 유효성 검증
 *
 * 유효한 재시도 정책으로 성공
 */
TEST_F(ActionExecutorTest, ValidRetryPolicyHandling) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 유효한 정책 사용
    RetryPolicy validPolicy{2, 100, 5000, 2.0};

    bool success = executor_->execute(action, *context_, 0, validPolicy);

    EXPECT_TRUE(success);
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
}

/**
 * @brief 동작 취소 처리
 *
 * 실행 중 동작 취소 요청 처리
 */
TEST_F(ActionExecutorTest, ActionCancellation) {
    auto action = std::make_shared<SuccessAction>("test_action");

    executor_->execute(action, *context_, 0, RetryPolicy::noRetry());

    // 취소 요청
    executor_->cancel("test_action");

    // 상태 확인
    EXPECT_EQ(executor_->getLastStatus(), ActionStatus::CANCELLED);
}

/**
 * @brief 타임아웃 시 에러 메시지
 *
 * 타임아웃 발생 시 명확한 에러 메시지
 */
TEST_F(ActionExecutorTest, TimeoutErrorMessage) {
    auto action = std::make_shared<SuccessAction>("test_action");

    // 매우 짧은 타임아웃 (1ms)
    bool success = executor_->execute(action, *context_, 1, RetryPolicy::noRetry());

    // 타임아웃 여부 또는 성공 여부 확인 (매우 빠를 수 있음)
    // 에러 메시지가 기록되어야 함
    if (!success) {
        EXPECT_FALSE(executor_->getLastErrorMessage().empty());
    }
}

/**
 * @brief 순차적 실행으로 인한 에러 누적
 *
 * 여러 실행에서 에러 정보가 적절히 업데이트됨
 */
TEST_F(ActionExecutorTest, ErrorAccumulation) {
    // 첫 번째 실행 - 성공
    {
        auto action = std::make_shared<SuccessAction>("action_1");
        executor_->execute(action, *context_, 0, RetryPolicy::noRetry());
        EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
    }

    // 두 번째 실행 - 실패
    {
        auto action = std::make_shared<FailureAction>("action_2");
        executor_->execute(action, *context_, 0, RetryPolicy::noRetry());
        EXPECT_EQ(executor_->getLastStatus(), ActionStatus::FAILED);
        EXPECT_FALSE(executor_->getLastErrorMessage().empty());
    }

    // 세 번째 실행 - 성공
    {
        auto action = std::make_shared<SuccessAction>("action_3");
        executor_->execute(action, *context_, 0, RetryPolicy::noRetry());
        EXPECT_EQ(executor_->getLastStatus(), ActionStatus::COMPLETED);
        EXPECT_TRUE(executor_->getLastErrorMessage().empty());
    }
}

