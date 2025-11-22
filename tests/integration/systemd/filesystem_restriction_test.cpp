#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>

/**
 * 파일시스템 접근 제한 테스트
 *
 * User Story 7: 파일시스템 격리
 *
 * 테스트 시나리오:
 * 1. ReadWritePaths 제한 확인
 * 2. ProtectSystem 레벨 확인
 * 3. 임시 파일 격리 확인
 */

class FilesystemRestrictionTest : public ::testing::Test {
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
 * Test Case 1: ReadWritePaths가 최소한으로 제한
 *
 * 필요한 경로만 쓰기 허용
 */
TEST_F(FilesystemRestrictionTest, ReadWritePathsAreMinimal) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtPaths = getSetting(rtService, "ReadWritePaths");
    std::string nonrtPaths = getSetting(nonrtService, "ReadWritePaths");

    // 필요한 경로: /var/lib/mxrc, /var/log/mxrc
    // 불필요한 경로: /home, /root, /etc 등은 없어야 함

    // RT
    bool rtHasLib = rtPaths.find("/var/lib/mxrc") != std::string::npos;
    bool rtHasLog = rtPaths.find("/var/log/mxrc") != std::string::npos;
    bool rtHasHome = rtPaths.find("/home") != std::string::npos &&
                     rtPaths.find("/home") != rtPaths.find("/var/lib/mxrc"); // /home이 별도로 있는지
    bool rtHasRoot = rtPaths.find("/root") != std::string::npos;

    EXPECT_TRUE(rtHasLib || rtHasLog)
        << "RT service should have necessary write paths";
    EXPECT_FALSE(rtHasHome)
        << "RT service should not have /home in ReadWritePaths";
    EXPECT_FALSE(rtHasRoot)
        << "RT service should not have /root in ReadWritePaths";

    // Non-RT
    bool nonrtHasLib = nonrtPaths.find("/var/lib/mxrc") != std::string::npos;
    bool nonrtHasLog = nonrtPaths.find("/var/log/mxrc") != std::string::npos;

    EXPECT_TRUE(nonrtHasLib || nonrtHasLog)
        << "Non-RT service should have necessary write paths";
}

/**
 * Test Case 2: ProtectSystem=strict 설정
 *
 * /usr, /boot, /efi 읽기 전용
 */
TEST_F(FilesystemRestrictionTest, ProtectSystemIsStrict) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtProtect = getSetting(rtService, "ProtectSystem");
    std::string nonrtProtect = getSetting(nonrtService, "ProtectSystem");

    EXPECT_EQ(rtProtect, "strict")
        << "RT service should have ProtectSystem=strict";
    EXPECT_EQ(nonrtProtect, "strict")
        << "Non-RT service should have ProtectSystem=strict";
}

/**
 * Test Case 3: PrivateTmp로 임시 파일 격리
 */
TEST_F(FilesystemRestrictionTest, PrivateTmpIsolation) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    bool rtHasPrivateTmp = findSetting(rtService, "PrivateTmp=true");
    bool nonrtHasPrivateTmp = findSetting(nonrtService, "PrivateTmp=true");

    EXPECT_TRUE(rtHasPrivateTmp)
        << "RT service should have PrivateTmp=true for isolation";
    EXPECT_TRUE(nonrtHasPrivateTmp)
        << "Non-RT service should have PrivateTmp=true for isolation";
}

/**
 * Test Case 4: ProtectHome으로 홈 디렉토리 보호
 */
TEST_F(FilesystemRestrictionTest, ProtectHomeEnabled) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    bool rtHasProtectHome = findSetting(rtService, "ProtectHome=true");
    bool nonrtHasProtectHome = findSetting(nonrtService, "ProtectHome=true");

    EXPECT_TRUE(rtHasProtectHome)
        << "RT service should have ProtectHome=true";
    EXPECT_TRUE(nonrtHasProtectHome)
        << "Non-RT service should have ProtectHome=true";
}

/**
 * Test Case 5: /tmp/mxrc 쓰기 권한 확인
 */
TEST_F(FilesystemRestrictionTest, TmpMxrcWriteAccess) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtPaths = getSetting(rtService, "ReadWritePaths");
    std::string nonrtPaths = getSetting(nonrtService, "ReadWritePaths");

    // /tmp/mxrc 쓰기 권한
    bool rtHasTmpMxrc = rtPaths.find("/tmp/mxrc") != std::string::npos;
    bool nonrtHasTmpMxrc = nonrtPaths.find("/tmp/mxrc") != std::string::npos;

    EXPECT_TRUE(rtHasTmpMxrc)
        << "RT service should have write access to /tmp/mxrc";
    EXPECT_TRUE(nonrtHasTmpMxrc)
        << "Non-RT service should have write access to /tmp/mxrc";
}

/**
 * Test Case 6: 파일시스템 제한 문서화
 */
TEST_F(FilesystemRestrictionTest, FilesystemRestrictionsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    bool hasFilesystemDocs = (content.find("ProtectSystem") != std::string::npos) ||
                             (content.find("ReadWritePaths") != std::string::npos) ||
                             (content.find("파일시스템") != std::string::npos);

    EXPECT_TRUE(hasFilesystemDocs)
        << "Filesystem restrictions should be documented";
}
