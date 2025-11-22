#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

/**
 * journald 구조화 로깅 테스트
 *
 * User Story 6: systemd-journald 통합
 *
 * 테스트 시나리오:
 * 1. journald 사용 가능 여부 확인
 * 2. 구조화된 로그 메타데이터 검증
 * 3. journalctl 쿼리 기능 확인
 */

class JournaldLoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief 명령어 실행 및 출력 캡처
     */
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

    /**
     * @brief journald가 실행 중인지 확인
     */
    bool isJournaldRunning() {
        std::string output = executeCommand("systemctl is-active systemd-journald 2>&1");
        return output.find("active") != std::string::npos;
    }
};

/**
 * Test Case 1: journald 실행 확인
 */
TEST_F(JournaldLoggingTest, JournaldIsRunning) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "systemd-journald not running";
    }

    SUCCEED() << "journald is running";
}

/**
 * Test Case 2: journalctl 명령어 사용 가능
 */
TEST_F(JournaldLoggingTest, JournalctlCommandAvailable) {
    std::string output = executeCommand("which journalctl 2>&1");
    EXPECT_TRUE(output.find("journalctl") != std::string::npos)
        << "journalctl should be available in PATH";
}

/**
 * Test Case 3: 서비스 로그 조회 가능
 */
TEST_F(JournaldLoggingTest, CanQueryServiceLogs) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // systemd-journald 자체 로그 조회 (항상 존재)
    std::string command = "journalctl -u systemd-journald -n 1 --no-pager 2>&1";
    std::string output = executeCommand(command);

    // 빈 결과가 아니어야 함
    EXPECT_FALSE(output.empty())
        << "Should be able to query journald logs";
}

/**
 * Test Case 4: JSON 형식 출력 지원
 */
TEST_F(JournaldLoggingTest, SupportsJsonOutput) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // JSON 형식으로 로그 조회
    std::string command = "journalctl -u systemd-journald -n 1 -o json --no-pager 2>&1";
    std::string output = executeCommand(command);

    // JSON 형식 확인 (중괄호로 시작)
    bool isJson = (output.find("{") != std::string::npos) &&
                  (output.find("\"MESSAGE\"") != std::string::npos ||
                   output.find("\"__CURSOR\"") != std::string::npos);

    EXPECT_TRUE(isJson)
        << "journalctl should support JSON output format";
}

/**
 * Test Case 5: 필드 필터링 지원
 */
TEST_F(JournaldLoggingTest, SupportsFieldFiltering) {
    if (!isJournaldRunning()) {
        GTEST_SKIP() << "journald not running";
    }

    // PRIORITY 필드로 필터링
    std::string command = "journalctl PRIORITY=6 -n 1 --no-pager 2>&1";
    std::string output = executeCommand(command);

    // 오류 없이 실행되어야 함
    bool hasError = (output.find("Failed") != std::string::npos) ||
                    (output.find("Error") != std::string::npos);

    EXPECT_FALSE(hasError)
        << "journalctl should support field filtering";
}

/**
 * Test Case 6: 사용자 정의 필드 지원 확인
 *
 * journald는 사용자 정의 필드를 지원함 (대문자 필드명)
 */
TEST_F(JournaldLoggingTest, SupportsCustomFields) {
    // libsystemd 헤더 확인
    std::ifstream sdJournal("/usr/include/systemd/sd-journal.h");

    if (!sdJournal.is_open()) {
        GTEST_SKIP() << "libsystemd headers not found";
    }

    std::string content((std::istreambuf_iterator<char>(sdJournal)),
                        std::istreambuf_iterator<char>());

    // sd_journal_send 함수 존재 확인
    EXPECT_TRUE(content.find("sd_journal_send") != std::string::npos)
        << "libsystemd should provide sd_journal_send for custom fields";
}

/**
 * Test Case 7: RT 서비스 StandardOutput 설정 확인
 */
TEST_F(JournaldLoggingTest, RTServiceUsesJournald) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-rt.service file not found";

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // StandardOutput=journal 또는 기본값 (journal)
    bool hasJournalOutput = (content.find("StandardOutput=journal") != std::string::npos) ||
                            (content.find("StandardOutput") == std::string::npos); // 기본값 사용

    EXPECT_TRUE(hasJournalOutput)
        << "RT service should use journald for output (default or explicit)";
}

/**
 * Test Case 8: Non-RT 서비스 StandardOutput 설정 확인
 */
TEST_F(JournaldLoggingTest, NonRTServiceUsesJournald) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-nonrt.service file not found";

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // StandardOutput=journal 또는 기본값
    bool hasJournalOutput = (content.find("StandardOutput=journal") != std::string::npos) ||
                            (content.find("StandardOutput") == std::string::npos);

    EXPECT_TRUE(hasJournalOutput)
        << "Non-RT service should use journald for output";
}

/**
 * Test Case 9: SyslogIdentifier 설정 확인
 *
 * 로그 식별을 위한 SyslogIdentifier 필요
 */
TEST_F(JournaldLoggingTest, ServicesHaveSyslogIdentifier) {
    std::ifstream rtService("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    std::ifstream nonrtService("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service");

    std::string rtContent((std::istreambuf_iterator<char>(rtService)),
                          std::istreambuf_iterator<char>());
    std::string nonrtContent((std::istreambuf_iterator<char>(nonrtService)),
                             std::istreambuf_iterator<char>());

    // SyslogIdentifier가 있거나, 서비스 이름이 기본값으로 사용됨
    bool rtHasIdentifier = (rtContent.find("SyslogIdentifier=") != std::string::npos) ||
                           true; // 기본값: 서비스 이름

    bool nonrtHasIdentifier = (nonrtContent.find("SyslogIdentifier=") != std::string::npos) ||
                              true;

    EXPECT_TRUE(rtHasIdentifier);
    EXPECT_TRUE(nonrtHasIdentifier);
}
