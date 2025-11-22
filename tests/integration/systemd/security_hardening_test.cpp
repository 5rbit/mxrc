#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>

/**
 * 보안 강화 설정 테스트
 *
 * User Story 7: systemd 보안 기능 활용
 *
 * 테스트 시나리오:
 * 1. NoNewPrivileges 설정 확인
 * 2. ProtectSystem 설정 확인
 * 3. ProtectHome 설정 확인
 * 4. PrivateTmp 설정 확인
 */

class SecurityHardeningTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief 서비스 파일에서 설정 찾기
     */
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

    /**
     * @brief 서비스 파일에서 설정 값 추출
     */
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
                // Trim whitespace
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                return value;
            }
        }

        return "";
    }
};

/**
 * Test Case 1: RT 서비스 NoNewPrivileges 설정
 *
 * setuid/setgid 방지
 */
TEST_F(SecurityHardeningTest, RTServiceHasNoNewPrivileges) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";

    bool hasNoNewPrivileges = findSetting(rtService, "NoNewPrivileges=true");

    EXPECT_TRUE(hasNoNewPrivileges)
        << "mxrc-rt.service should have NoNewPrivileges=true";
}

/**
 * Test Case 2: Non-RT 서비스 NoNewPrivileges 설정
 */
TEST_F(SecurityHardeningTest, NonRTServiceHasNoNewPrivileges) {
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    bool hasNoNewPrivileges = findSetting(nonrtService, "NoNewPrivileges=true");

    EXPECT_TRUE(hasNoNewPrivileges)
        << "mxrc-nonrt.service should have NoNewPrivileges=true";
}

/**
 * Test Case 3: ProtectSystem 설정 확인
 *
 * 읽기 전용 파일시스템 보호
 */
TEST_F(SecurityHardeningTest, ServicesHaveProtectSystem) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtProtect = getSetting(rtService, "ProtectSystem");
    std::string nonrtProtect = getSetting(nonrtService, "ProtectSystem");

    // ProtectSystem=strict 또는 full
    bool rtHasProtection = (rtProtect == "strict") || (rtProtect == "full");
    bool nonrtHasProtection = (nonrtProtect == "strict") || (nonrtProtect == "full");

    EXPECT_TRUE(rtHasProtection)
        << "RT service should have ProtectSystem=strict or full";
    EXPECT_TRUE(nonrtHasProtection)
        << "Non-RT service should have ProtectSystem=strict or full";
}

/**
 * Test Case 4: ProtectHome 설정 확인
 *
 * 홈 디렉토리 접근 차단
 */
TEST_F(SecurityHardeningTest, ServicesHaveProtectHome) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    bool rtHasProtectHome = findSetting(rtService, "ProtectHome=true") ||
                            findSetting(rtService, "ProtectHome=read-only");
    bool nonrtHasProtectHome = findSetting(nonrtService, "ProtectHome=true") ||
                               findSetting(nonrtService, "ProtectHome=read-only");

    EXPECT_TRUE(rtHasProtectHome)
        << "RT service should have ProtectHome enabled";
    EXPECT_TRUE(nonrtHasProtectHome)
        << "Non-RT service should have ProtectHome enabled";
}

/**
 * Test Case 5: PrivateTmp 설정 확인
 *
 * 격리된 /tmp 사용
 */
TEST_F(SecurityHardeningTest, ServicesHavePrivateTmp) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    bool rtHasPrivateTmp = findSetting(rtService, "PrivateTmp=true");
    bool nonrtHasPrivateTmp = findSetting(nonrtService, "PrivateTmp=true");

    EXPECT_TRUE(rtHasPrivateTmp)
        << "RT service should have PrivateTmp=true";
    EXPECT_TRUE(nonrtHasPrivateTmp)
        << "Non-RT service should have PrivateTmp=true";
}

/**
 * Test Case 6: ReadWritePaths 설정 확인
 *
 * 쓰기 가능한 경로 명시
 */
TEST_F(SecurityHardeningTest, ServicesHaveReadWritePaths) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtPaths = getSetting(rtService, "ReadWritePaths");
    std::string nonrtPaths = getSetting(nonrtService, "ReadWritePaths");

    // /var/lib/mxrc, /var/log/mxrc 등 필요한 경로
    bool rtHasPaths = (rtPaths.find("/var/lib/mxrc") != std::string::npos) ||
                      (rtPaths.find("/var/log/mxrc") != std::string::npos);
    bool nonrtHasPaths = (nonrtPaths.find("/var/lib/mxrc") != std::string::npos) ||
                         (nonrtPaths.find("/var/log/mxrc") != std::string::npos);

    EXPECT_TRUE(rtHasPaths)
        << "RT service should specify ReadWritePaths";
    EXPECT_TRUE(nonrtHasPaths)
        << "Non-RT service should specify ReadWritePaths";
}

/**
 * Test Case 7: User/Group 설정 확인
 *
 * root 권한으로 실행하지 않음
 */
TEST_F(SecurityHardeningTest, ServicesRunAsNonRoot) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtUser = getSetting(rtService, "User");
    std::string nonrtUser = getSetting(nonrtService, "User");

    // User=mxrc (root가 아님)
    EXPECT_FALSE(rtUser.empty())
        << "RT service should specify User";
    EXPECT_NE(rtUser, "root")
        << "RT service should not run as root";

    EXPECT_FALSE(nonrtUser.empty())
        << "Non-RT service should specify User";
    EXPECT_NE(nonrtUser, "root")
        << "Non-RT service should not run as root";
}

/**
 * Test Case 8: 보안 설정 문서화
 */
TEST_F(SecurityHardeningTest, SecuritySettingsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    // 보안 관련 내용
    bool hasSecurityDocs = (content.find("보안") != std::string::npos) ||
                           (content.find("Security") != std::string::npos) ||
                           (content.find("NoNewPrivileges") != std::string::npos);

    EXPECT_TRUE(hasSecurityDocs)
        << "Security settings should be documented";
}
