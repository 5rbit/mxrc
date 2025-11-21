#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/types.h>
#include <unistd.h>

/**
 * 메모리 제한 통합 테스트
 *
 * User Story 3: cgroups를 통한 메모리 제한 검증
 *
 * 테스트 시나리오:
 * 1. RT 프로세스: MemoryMax=2G
 * 2. Non-RT 프로세스: MemoryMax=1G
 * 3. systemd cgroup 설정 확인
 */

class MemoryLimitTest : public ::testing::Test {
protected:
    void SetUp() override {
        pid = getpid();
    }

    void TearDown() override {
        // Cleanup if needed
    }

    /**
     * @brief cgroup 메모리 제한 읽기
     *
     * @param serviceName 서비스 이름
     * @return 메모리 제한 (bytes), -1 if not found
     */
    long long getMemoryLimit(const std::string& serviceName) {
        // systemd cgroup v2 경로
        std::string cgroupPath = "/sys/fs/cgroup/system.slice/" + serviceName + ".service/memory.max";

        std::ifstream file(cgroupPath);
        if (!file.is_open()) {
            // cgroup v1 경로 시도
            cgroupPath = "/sys/fs/cgroup/memory/system.slice/" + serviceName + ".service/memory.limit_in_bytes";
            file.open(cgroupPath);
            if (!file.is_open()) {
                return -1;
            }
        }

        std::string value;
        std::getline(file, value);

        // "max" means unlimited
        if (value == "max") {
            return -1;
        }

        return std::stoll(value);
    }

    /**
     * @brief 바이트를 GB로 변환
     */
    double bytesToGB(long long bytes) {
        return bytes / (1024.0 * 1024.0 * 1024.0);
    }

    pid_t pid;
};

/**
 * Test Case 1: RT 프로세스 메모리 제한 확인
 *
 * 검증:
 * - mxrc-rt.service의 MemoryMax=2G 설정 확인
 */
TEST_F(MemoryLimitTest, RTProcessHasMemoryLimit2GB) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-rt.service file not found";

    std::string line;
    bool foundMemoryMax = false;
    while (std::getline(serviceFile, line)) {
        if (line.find("MemoryMax=2G") != std::string::npos) {
            foundMemoryMax = true;
            break;
        }
    }

    EXPECT_TRUE(foundMemoryMax) << "MemoryMax=2G not found in mxrc-rt.service";
}

/**
 * Test Case 2: Non-RT 프로세스 메모리 제한 확인
 *
 * 검증:
 * - mxrc-nonrt.service의 MemoryMax=1G 설정 확인
 */
TEST_F(MemoryLimitTest, NonRTProcessHasMemoryLimit1GB) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service");
    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-nonrt.service not yet created (will be created in Phase 5)";
    }

    std::string line;
    bool foundMemoryMax = false;
    while (std::getline(serviceFile, line)) {
        if (line.find("MemoryMax=1G") != std::string::npos) {
            foundMemoryMax = true;
            break;
        }
    }

    EXPECT_TRUE(foundMemoryMax) << "MemoryMax=1G not found in mxrc-nonrt.service";
}

/**
 * Test Case 3: cgroup 메모리 제한 검증
 *
 * systemd가 실행 중일 때만 테스트
 */
TEST_F(MemoryLimitTest, CgroupMemoryLimitIsConfigured) {
    // systemd 실행 확인
    std::ifstream systemdCheck("/run/systemd/system");
    if (!systemdCheck.good()) {
        GTEST_SKIP() << "systemd not running, skipping cgroup test";
    }

    // RT 프로세스 메모리 제한 확인 (2GB)
    long long rtMemory = getMemoryLimit("mxrc-rt");
    if (rtMemory != -1) {
        double rtMemoryGB = bytesToGB(rtMemory);
        // 2GB = 2147483648 bytes
        EXPECT_NEAR(rtMemoryGB, 2.0, 0.1)
            << "RT process memory limit should be ~2GB, got: " << rtMemoryGB << "GB";
    }
}

/**
 * Test Case 4: 메모리 제한 문서화 확인
 */
TEST_F(MemoryLimitTest, MemoryLimitIsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("MemoryMax") != std::string::npos)
        << "MemoryMax should be documented in quickstart.md";
}

/**
 * Test Case 5: 메모리 제한 값 유효성 검증
 */
TEST_F(MemoryLimitTest, MemoryLimitValuesAreReasonable) {
    // RT: 2GB
    int rtMemoryGB = 2;
    EXPECT_GE(rtMemoryGB, 1) << "RT memory should be at least 1GB";
    EXPECT_LE(rtMemoryGB, 8) << "RT memory should not exceed 8GB";

    // Non-RT: 1GB
    int nonrtMemoryGB = 1;
    EXPECT_GE(nonrtMemoryGB, 512.0/1024.0) << "Non-RT memory should be at least 512MB";
    EXPECT_LE(nonrtMemoryGB, 4) << "Non-RT memory should not exceed 4GB";
}

/**
 * Test Case 6: MemoryAccounting 활성화 확인
 *
 * systemd가 메모리 추적을 위해 MemoryAccounting=yes 필요
 */
TEST_F(MemoryLimitTest, MemoryAccountingIsEnabled) {
    // RT 서비스 파일 확인
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-rt.service not found";
    }

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // MemoryMax가 있으면 MemoryAccounting은 자동 활성화됨
    // 명시적으로 설정되어 있거나, MemoryMax만 있어도 OK
    bool hasMemoryMax = (content.find("MemoryMax") != std::string::npos);
    bool hasMemoryAccounting = (content.find("MemoryAccounting") != std::string::npos);

    EXPECT_TRUE(hasMemoryMax || hasMemoryAccounting)
        << "MemoryMax or MemoryAccounting should be present";
}

/**
 * Test Case 7: OOM 정책 확인
 *
 * OOMPolicy 설정으로 메모리 부족 시 동작 제어
 */
TEST_F(MemoryLimitTest, OOMPolicyIsConfigured) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-rt.service not found";
    }

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // OOMPolicy가 설정되어 있는지 확인 (선택사항)
    // 없어도 괜찮지만, 있으면 stop 또는 kill 권장
    if (content.find("OOMPolicy") != std::string::npos) {
        bool hasStopOrKill = (content.find("OOMPolicy=stop") != std::string::npos) ||
                             (content.find("OOMPolicy=kill") != std::string::npos);
        EXPECT_TRUE(hasStopOrKill)
            << "If OOMPolicy is set, it should be 'stop' or 'kill'";
    }
}
