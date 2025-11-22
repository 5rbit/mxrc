#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>

/**
 * Capability 제한 테스트
 *
 * User Story 7: 최소 권한 원칙
 *
 * 테스트 시나리오:
 * 1. RT 프로세스 필요 capability 확인 (CAP_SYS_NICE, CAP_IPC_LOCK)
 * 2. Non-RT 프로세스 capability 최소화
 * 3. CapabilityBoundingSet 제한
 */

class CapabilityTest : public ::testing::Test {
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
};

/**
 * Test Case 1: RT 서비스 AmbientCapabilities 설정
 *
 * RT 스케줄링: CAP_SYS_NICE
 * 메모리 락: CAP_IPC_LOCK
 */
TEST_F(CapabilityTest, RTServiceHasRequiredCapabilities) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";

    std::string ambient = getSetting(rtService, "AmbientCapabilities");

    // CAP_SYS_NICE, CAP_IPC_LOCK 필요
    bool hasSysNice = ambient.find("CAP_SYS_NICE") != std::string::npos;
    bool hasIpcLock = ambient.find("CAP_IPC_LOCK") != std::string::npos;

    EXPECT_TRUE(hasSysNice)
        << "RT service needs CAP_SYS_NICE for RT scheduling";
    EXPECT_TRUE(hasIpcLock)
        << "RT service needs CAP_IPC_LOCK for memory locking";
}

/**
 * Test Case 2: RT 서비스 CapabilityBoundingSet 제한
 *
 * 필요한 capability만 허용
 */
TEST_F(CapabilityTest, RTServiceLimitsCapabilities) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";

    std::string bounding = getSetting(rtService, "CapabilityBoundingSet");

    // CAP_SYS_NICE, CAP_IPC_LOCK만 있어야 함
    bool hasSysNice = bounding.find("CAP_SYS_NICE") != std::string::npos;
    bool hasIpcLock = bounding.find("CAP_IPC_LOCK") != std::string::npos;

    // 위험한 capability는 없어야 함
    bool hasNetAdmin = bounding.find("CAP_NET_ADMIN") != std::string::npos;
    bool hasSysAdmin = bounding.find("CAP_SYS_ADMIN") != std::string::npos;

    EXPECT_TRUE(hasSysNice && hasIpcLock)
        << "CapabilityBoundingSet should include required capabilities";
    EXPECT_FALSE(hasNetAdmin || hasSysAdmin)
        << "CapabilityBoundingSet should not include unnecessary capabilities";
}

/**
 * Test Case 3: Non-RT 서비스 capability 최소화
 *
 * Non-RT는 특별한 capability 불필요
 */
TEST_F(CapabilityTest, NonRTServiceMinimalCapabilities) {
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string ambient = getSetting(nonrtService, "AmbientCapabilities");
    std::string bounding = getSetting(nonrtService, "CapabilityBoundingSet");

    // AmbientCapabilities가 비어있거나 최소한이어야 함
    // CapabilityBoundingSet도 제한적이어야 함
    bool hasMinimalAmbient = ambient.empty() ||
                             (ambient.find("CAP_") == std::string::npos);

    // 만약 설정되어 있다면, 위험한 capability는 없어야 함
    if (!bounding.empty()) {
        bool hasDangerousCaps = (bounding.find("CAP_SYS_ADMIN") != std::string::npos) ||
                                (bounding.find("CAP_NET_ADMIN") != std::string::npos) ||
                                (bounding.find("CAP_SYS_MODULE") != std::string::npos);
        EXPECT_FALSE(hasDangerousCaps)
            << "Non-RT service should not have dangerous capabilities";
    }

    // Ambient는 비어있는 것이 이상적
    EXPECT_TRUE(hasMinimalAmbient)
        << "Non-RT service should have minimal ambient capabilities";
}

/**
 * Test Case 4: Capability 문서화
 */
TEST_F(CapabilityTest, CapabilitiesDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    // Capability 관련 내용
    bool hasCapabilityDocs = (content.find("Capability") != std::string::npos) ||
                             (content.find("CAP_SYS_NICE") != std::string::npos) ||
                             (content.find("권한") != std::string::npos);

    EXPECT_TRUE(hasCapabilityDocs)
        << "Capabilities should be documented";
}

/**
 * Test Case 5: RT 프로세스만 특권 capability 보유
 */
TEST_F(CapabilityTest, OnlyRTProcessHasPrivilegedCapabilities) {
    std::string rtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service";
    std::string nonrtService = "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service";

    std::string rtAmbient = getSetting(rtService, "AmbientCapabilities");
    std::string nonrtAmbient = getSetting(nonrtService, "AmbientCapabilities");

    // RT는 CAP_SYS_NICE를 가짐
    bool rtHasSysNice = rtAmbient.find("CAP_SYS_NICE") != std::string::npos;

    // Non-RT는 CAP_SYS_NICE를 가지지 않음
    bool nonrtHasSysNice = nonrtAmbient.find("CAP_SYS_NICE") != std::string::npos;

    EXPECT_TRUE(rtHasSysNice)
        << "RT process should have CAP_SYS_NICE";
    EXPECT_FALSE(nonrtHasSysNice)
        << "Non-RT process should not have CAP_SYS_NICE";
}
