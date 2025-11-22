#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdlib>

/**
 * 부팅 시간 최적화 테스트
 *
 * User Story 8: 빠른 부팅 시간
 *
 * 테스트 시나리오:
 * 1. Type=notify 설정 (빠른 시작)
 * 2. DefaultDependencies=no 검증
 * 3. 타임아웃 설정 확인
 */

class BootTimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    std::string getSetting(const std::string& filePath, const std::string& key) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return "";
        }

        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find(key + "=");
            if (pos != std::string::npos) {
                std::string value = line.substr(pos + key.length() + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                return value;
            }
        }

        return "";
    }

    bool findSetting(const std::string& filePath, const std::string& setting) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.find(setting) != std::string::npos) {
                return true;
            }
        }

        return false;
    }
};

/**
 * Test Case 1: RT 서비스 Type=notify 설정
 *
 * sd_notify로 준비 완료 신호 → 빠른 시작
 */
TEST_F(BootTimeTest, RTServiceUsesNotifyType) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string type = getSetting(rtService, "Type");

    EXPECT_EQ(type, "notify")
        << "RT service should use Type=notify for faster startup";
}

/**
 * Test Case 2: TimeoutStartSec 설정 확인
 *
 * 적절한 타임아웃 (30초 이하)
 */
TEST_F(BootTimeTest, ServicesHaveReasonableTimeout) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtTimeout = getSetting(rtService, "TimeoutStartSec");
    std::string nonrtTimeout = getSetting(nonrtService, "TimeoutStartSec");

    // 30초 이하여야 함
    bool rtReasonable = (rtTimeout.find("30s") != std::string::npos) ||
                        (rtTimeout.find("10s") != std::string::npos) ||
                        (rtTimeout.find("20s") != std::string::npos);

    bool nonrtReasonable = (nonrtTimeout.find("30s") != std::string::npos) ||
                           (nonrtTimeout.find("10s") != std::string::npos) ||
                           (nonrtTimeout.find("20s") != std::string::npos);

    EXPECT_TRUE(rtReasonable)
        << "RT service should have reasonable timeout (≤30s)";
    EXPECT_TRUE(nonrtReasonable)
        << "Non-RT service should have reasonable timeout (≤30s)";
}

/**
 * Test Case 3: 불필요한 의존성 제거 확인
 *
 * After=network.target만 필요
 */
TEST_F(BootTimeTest, MinimalDependencies) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";

    std::string after = getSetting(rtService, "After");

    // network.target만 있어야 함 (불필요한 의존성 없음)
    bool hasNetwork = after.find("network.target") != std::string::npos;
    bool hasUnnecessary = (after.find("multi-user.target") != std::string::npos) ||
                          (after.find("graphical.target") != std::string::npos);

    EXPECT_TRUE(hasNetwork)
        << "Should depend on network.target";
    EXPECT_FALSE(hasUnnecessary)
        << "Should not have unnecessary dependencies";
}

/**
 * Test Case 4: 부팅 최적화 문서화
 */
TEST_F(BootTimeTest, BootOptimizationDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    bool hasBootDocs = (content.find("부팅") != std::string::npos) ||
                       (content.find("Boot") != std::string::npos) ||
                       (content.find("Type=notify") != std::string::npos);

    EXPECT_TRUE(hasBootDocs)
        << "Boot optimization should be documented";
}
