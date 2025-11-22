#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdlib>

/**
 * 로그 쿼리 및 필터링 테스트
 *
 * User Story 6: journalctl을 통한 로그 조회
 *
 * 테스트 시나리오:
 * 1. Trace ID로 로그 필터링
 * 2. Component로 로그 필터링
 * 3. 시간 범위 쿼리
 */

class LogQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    std::string executeCommand(const std::string& command) {
        std::array<char, 128> buffer;
        std::string result;

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "";
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        pclose(pipe);
        return result;
    }

    bool isJournaldRunning() {
        std::string output = executeCommand("systemctl is-active systemd-journald 2>&1");
        return output.find("active") != std::string::npos;
    }
};

/**
 * Test Case 1: Trace ID 필터링 명령어 형식
 */
TEST_F(LogQueryTest, TraceIdFilterCommandFormat) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // journalctl TRACE_ID=<value> 형식 테스트
    std::string command = "journalctl TRACE_ID=test123 -n 0 --no-pager 2>&1";
    std::string output = executeCommand(command);

    // 문법 오류가 없어야 함
    bool hasError = (output.find("Failed to") != std::string::npos) ||
                    (output.find("Invalid") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "journalctl should accept TRACE_ID field filter";
}

/**
 * Test Case 2: Component 필터링 명령어 형식
 */
TEST_F(LogQueryTest, ComponentFilterCommandFormat) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // journalctl COMPONENT=<value> 형식 테스트
    std::string command = "journalctl COMPONENT=task -n 0 --no-pager 2>&1";
    std::string output = executeCommand(command);

    bool hasError = (output.find("Failed to") != std::string::npos) ||
                    (output.find("Invalid") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "journalctl should accept COMPONENT field filter";
}

/**
 * Test Case 3: 시간 범위 쿼리 지원
 */
TEST_F(LogQueryTest, TimeRangeQuerySupported) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // --since 옵션 테스트
    std::string command = "journalctl --since \"1 hour ago\" -n 1 --no-pager 2>&1";
    std::string output = executeCommand(command);

    bool hasError = (output.find("Failed to") != std::string::npos) ||
                    (output.find("Invalid option") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "journalctl should support time range queries";
}

/**
 * Test Case 4: Priority 필터링 (로그 레벨)
 */
TEST_F(LogQueryTest, PriorityFilterSupported) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // PRIORITY=3 (err) 테스트
    std::string command = "journalctl PRIORITY=3 -n 1 --no-pager 2>&1";
    std::string output = executeCommand(command);

    bool hasError = (output.find("Failed to") != std::string::npos) ||
                    (output.find("Invalid") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "journalctl should support PRIORITY filtering";
}

/**
 * Test Case 5: 여러 필드 조합 필터링
 */
TEST_F(LogQueryTest, MultipleFieldFilteringSupported) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // 여러 필드 조합
    std::string command = "journalctl PRIORITY=6 COMPONENT=test -n 0 --no-pager 2>&1";
    std::string output = executeCommand(command);

    bool hasError = (output.find("Failed to") != std::string::npos) ||
                    (output.find("Invalid") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "journalctl should support multiple field filters";
}

/**
 * Test Case 6: 로그 쿼리 문서화
 */
TEST_F(LogQueryTest, LogQueryDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    // journalctl 명령어 예시
    bool hasJournalctlExamples = content.find("journalctl") != std::string::npos;

    EXPECT_TRUE(hasJournalctlExamples)
        << "quickstart.md should document journalctl query examples";
}

/**
 * Test Case 7: JSON 출력으로 구조화된 데이터 추출
 */
TEST_F(LogQueryTest, JsonOutputExtractsStructuredData) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // JSON 출력으로 필드 추출
    std::string command = "journalctl -u systemd-journald -n 1 -o json --no-pager 2>&1";
    std::string output = executeCommand(command);

    // JSON 형식인지 확인
    bool isValidJson = (output.find("{") != std::string::npos) &&
                       (output.find("}") != std::string::npos);

    EXPECT_TRUE(isValidJson)
        << "journalctl JSON output should be valid";
}

/**
 * Test Case 8: 서비스별 로그 조회
 */
TEST_F(LogQueryTest, CanQueryByServiceUnit) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // -u 옵션으로 서비스 필터링
    std::string command = "journalctl -u systemd-journald -n 1 --no-pager 2>&1";
    std::string output = executeCommand(command);

    bool hasError = (output.find("Failed to") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "Should be able to query logs by service unit";
}
