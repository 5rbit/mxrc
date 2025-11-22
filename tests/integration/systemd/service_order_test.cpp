#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

/**
 * 서비스 시작 순서 통합 테스트
 *
 * User Story 4: 서비스 의존성 관리 및 순서 제어
 *
 * 테스트 시나리오:
 * 1. Non-RT 프로세스가 RT 프로세스보다 먼저 시작
 * 2. Before/After 지시어 검증
 * 3. Wants/Requires 의존성 검증
 */

class ServiceOrderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief 서비스 파일에서 Before/After 지시어 찾기
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
                // Trim whitespace
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                results.push_back(value);
            }
        }

        return results;
    }
};

/**
 * Test Case 1: RT 서비스가 Non-RT를 After로 지정
 *
 * 검증:
 * - mxrc-rt.service에 "After=mxrc-nonrt.service" 있어야 함
 */
TEST_F(ServiceOrderTest, RTServiceStartsAfterNonRT) {
    std::string rtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";

    auto afterDirectives = findDirective(rtServicePath, "After");

    bool hasNonRTDependency = false;
    for (const auto& after : afterDirectives) {
        if (after.find("mxrc-nonrt") != std::string::npos) {
            hasNonRTDependency = true;
            break;
        }
    }

    EXPECT_TRUE(hasNonRTDependency)
        << "mxrc-rt.service should have 'After=mxrc-nonrt.service'";
}

/**
 * Test Case 2: Non-RT 서비스가 RT를 Before로 지정
 *
 * 검증:
 * - mxrc-nonrt.service에 "Before=mxrc-rt.service" 있어야 함
 */
TEST_F(ServiceOrderTest, NonRTServiceStartsBeforeRT) {
    std::string nonrtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    auto beforeDirectives = findDirective(nonrtServicePath, "Before");

    bool hasRTDependency = false;
    for (const auto& before : beforeDirectives) {
        if (before.find("mxrc-rt") != std::string::npos) {
            hasRTDependency = true;
            break;
        }
    }

    EXPECT_TRUE(hasRTDependency)
        << "mxrc-nonrt.service should have 'Before=mxrc-rt.service'";
}

/**
 * Test Case 3: RT 서비스가 Non-RT를 Wants로 지정
 *
 * Wants는 soft dependency (Non-RT 실패해도 RT는 시작)
 */
TEST_F(ServiceOrderTest, RTServiceWantsNonRT) {
    std::string rtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";

    auto wantsDirectives = findDirective(rtServicePath, "Wants");

    bool hasWantsDependency = false;
    for (const auto& wants : wantsDirectives) {
        if (wants.find("mxrc-nonrt") != std::string::npos) {
            hasWantsDependency = true;
            break;
        }
    }

    // Wants 또는 Requires 중 하나만 있어도 OK
    auto requiresDirectives = findDirective(rtServicePath, "Requires");
    bool hasRequiresDependency = false;
    for (const auto& req : requiresDirectives) {
        if (req.find("mxrc-nonrt") != std::string::npos) {
            hasRequiresDependency = true;
            break;
        }
    }

    EXPECT_TRUE(hasWantsDependency || hasRequiresDependency)
        << "mxrc-rt.service should have 'Wants=' or 'Requires=' for mxrc-nonrt.service";
}

/**
 * Test Case 4: 순환 의존성 없음
 *
 * A가 B를 After로, B가 A를 After로 하면 순환 의존성
 */
TEST_F(ServiceOrderTest, NoCyclicDependencies) {
    std::string rtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtServicePath = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    auto rtAfter = findDirective(rtServicePath, "After");
    auto rtBefore = findDirective(rtServicePath, "Before");
    auto nonrtAfter = findDirective(nonrtServicePath, "After");
    auto nonrtBefore = findDirective(nonrtServicePath, "Before");

    // RT After Non-RT이고, Non-RT Before RT면 OK
    bool rtAfterNonRT = false;
    for (const auto& after : rtAfter) {
        if (after.find("mxrc-nonrt") != std::string::npos) {
            rtAfterNonRT = true;
        }
    }

    bool nonrtBeforeRT = false;
    for (const auto& before : nonrtBefore) {
        if (before.find("mxrc-rt") != std::string::npos) {
            nonrtBeforeRT = true;
        }
    }

    // 순환: RT Before Non-RT이면서 Non-RT After RT (잘못됨)
    bool cyclicDependency = false;
    for (const auto& before : rtBefore) {
        if (before.find("mxrc-nonrt") != std::string::npos) {
            for (const auto& after : nonrtAfter) {
                if (after.find("mxrc-rt") != std::string::npos) {
                    cyclicDependency = true;
                }
            }
        }
    }

    EXPECT_FALSE(cyclicDependency) << "Cyclic dependency detected between RT and Non-RT services";
}

/**
 * Test Case 5: 의존성 문서화 확인
 */
TEST_F(ServiceOrderTest, DependencyIsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    // Before 또는 After가 문서화되어 있는지 확인
    bool hasBeforeAfter = (content.find("Before=") != std::string::npos) ||
                          (content.find("After=") != std::string::npos) ||
                          (content.find("의존성") != std::string::npos) ||
                          (content.find("dependency") != std::string::npos);

    EXPECT_TRUE(hasBeforeAfter)
        << "Service dependency should be documented in quickstart.md";
}
