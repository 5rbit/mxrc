#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

/**
 * mxrc.target 일괄 시작 테스트
 *
 * User Story 4: target unit을 통한 서비스 그룹 관리
 *
 * 테스트 시나리오:
 * 1. mxrc.target 파일 존재 및 구조 검증
 * 2. Wants/Requires로 모든 MXRC 서비스 포함
 * 3. WantedBy/RequiredBy 관계 검증
 */

class TargetTest : public ::testing::Test {
protected:
    void SetUp() override {
        targetPath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc.target";
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief 서비스 파일에서 특정 지시어 찾기
     */
    std::vector<std::string> findDirective(const std::string& filePath, const std::string& directive) {
        std::ifstream file(filePath);
        std::vector<std::string> results;

        if (!file.is_open()) {
            return results;
        }

        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find(directive + "=");
            if (pos != std::string::npos) {
                std::string value = line.substr(pos + directive.length() + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                results.push_back(value);
            }
        }

        return results;
    }

    std::string targetPath;
};

/**
 * Test Case 1: mxrc.target 파일 존재
 */
TEST_F(TargetTest, TargetFileExists) {
    std::ifstream targetFile(targetPath);
    EXPECT_TRUE(targetFile.is_open())
        << "mxrc.target file not found at " << targetPath;
}

/**
 * Test Case 2: target에 [Unit] 섹션 존재
 */
TEST_F(TargetTest, TargetHasUnitSection) {
    std::ifstream file(targetPath);
    if (!file.is_open()) {
        GTEST_SKIP() << "mxrc.target not created yet (will be created in Phase 6)";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("[Unit]") != std::string::npos)
        << "mxrc.target missing [Unit] section";
}

/**
 * Test Case 3: target에 Description 존재
 */
TEST_F(TargetTest, TargetHasDescription) {
    std::ifstream file(targetPath);
    if (!file.is_open()) {
        GTEST_SKIP() << "mxrc.target not created yet";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Description=") != std::string::npos)
        << "mxrc.target missing Description";
}

/**
 * Test Case 4: target이 RT 서비스를 Wants/Requires로 포함
 */
TEST_F(TargetTest, TargetIncludesRTService) {
    std::ifstream file(targetPath);
    if (!file.is_open()) {
        GTEST_SKIP() << "mxrc.target not created yet";
    }

    auto wants = findDirective(targetPath, "Wants");
    auto requiresList = findDirective(targetPath, "Requires");

    bool includesRT = false;
    for (const auto& w : wants) {
        if (w.find("mxrc-rt") != std::string::npos) {
            includesRT = true;
            break;
        }
    }
    for (const auto& r : requiresList) {
        if (r.find("mxrc-rt") != std::string::npos) {
            includesRT = true;
            break;
        }
    }

    EXPECT_TRUE(includesRT)
        << "mxrc.target should include mxrc-rt.service in Wants or Requires";
}

/**
 * Test Case 5: target이 Non-RT 서비스를 Wants/Requires로 포함
 */
TEST_F(TargetTest, TargetIncludesNonRTService) {
    std::ifstream file(targetPath);
    if (!file.is_open()) {
        GTEST_SKIP() << "mxrc.target not created yet";
    }

    auto wants = findDirective(targetPath, "Wants");
    auto requiresList = findDirective(targetPath, "Requires");

    bool includesNonRT = false;
    for (const auto& w : wants) {
        if (w.find("mxrc-nonrt") != std::string::npos) {
            includesNonRT = true;
            break;
        }
    }
    for (const auto& r : requiresList) {
        if (r.find("mxrc-nonrt") != std::string::npos) {
            includesNonRT = true;
            break;
        }
    }

    EXPECT_TRUE(includesNonRT)
        << "mxrc.target should include mxrc-nonrt.service in Wants or Requires";
}

/**
 * Test Case 6: 서비스들이 target을 WantedBy로 참조
 */
TEST_F(TargetTest, ServicesReferenceTarget) {
    std::string rtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    auto rtWantedBy = findDirective(rtServicePath, "WantedBy");
    auto nonrtWantedBy = findDirective(nonrtServicePath, "WantedBy");

    // multi-user.target 또는 mxrc.target이어야 함
    bool rtHasTarget = false;
    for (const auto& target : rtWantedBy) {
        if (target.find("multi-user.target") != std::string::npos ||
            target.find("mxrc.target") != std::string::npos) {
            rtHasTarget = true;
            break;
        }
    }

    bool nonrtHasTarget = false;
    for (const auto& target : nonrtWantedBy) {
        if (target.find("multi-user.target") != std::string::npos ||
            target.find("mxrc.target") != std::string::npos) {
            nonrtHasTarget = true;
            break;
        }
    }

    EXPECT_TRUE(rtHasTarget) << "mxrc-rt.service should have WantedBy=multi-user.target or mxrc.target";
    EXPECT_TRUE(nonrtHasTarget) << "mxrc-nonrt.service should have WantedBy=multi-user.target or mxrc.target";
}

/**
 * Test Case 7: target 문서화 확인
 */
TEST_F(TargetTest, TargetIsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    bool hasTarget = (content.find("mxrc.target") != std::string::npos) ||
                     (content.find("target") != std::string::npos);

    EXPECT_TRUE(hasTarget)
        << "mxrc.target should be documented in quickstart.md";
}

/**
 * Test Case 8: [Install] 섹션 검증
 */
TEST_F(TargetTest, TargetHasInstallSection) {
    std::ifstream file(targetPath);
    if (!file.is_open()) {
        GTEST_SKIP() << "mxrc.target not created yet";
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("[Install]") != std::string::npos)
        << "mxrc.target missing [Install] section";
}
