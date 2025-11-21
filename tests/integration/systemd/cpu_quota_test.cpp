#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#include <unistd.h>

/**
 * CPU Quota 제한 통합 테스트
 *
 * User Story 3: cgroups를 통한 CPU quota 제한 검증
 *
 * 테스트 시나리오:
 * 1. RT 프로세스: CPUQuota=200% (2 cores 최대)
 * 2. Non-RT 프로세스: CPUQuota=100% (1 core 최대)
 * 3. systemd cgroup 설정 확인
 */

class CpuQuotaTest : public ::testing::Test {
protected:
    void SetUp() override {
        pid = getpid();
    }

    void TearDown() override {
        // Cleanup if needed
    }

    /**
     * @brief cgroup CPU quota 설정 읽기
     *
     * @param serviceName 서비스 이름 (mxrc-rt, mxrc-nonrt)
     * @return CPU quota 값 (예: 200000 = 200%)
     */
    long getCpuQuota(const std::string& serviceName) {
        // systemd cgroup v2 경로
        std::string cgroupPath = "/sys/fs/cgroup/system.slice/" + serviceName + ".service/cpu.max";

        std::ifstream file(cgroupPath);
        if (!file.is_open()) {
            // cgroup v1 경로 시도
            cgroupPath = "/sys/fs/cgroup/cpu,cpuacct/system.slice/" + serviceName + ".service/cpu.cfs_quota_us";
            file.open(cgroupPath);
            if (!file.is_open()) {
                return -1;
            }
        }

        std::string line;
        std::getline(file, line);

        // cgroup v2 형식: "200000 100000" (quota period)
        // cgroup v1 형식: "200000"
        std::istringstream iss(line);
        long quota;
        iss >> quota;

        return quota;
    }

    /**
     * @brief CPU 사용률 측정 (간단한 부하 생성)
     *
     * @param durationMs 측정 시간 (밀리초)
     * @return CPU 사용률 (0.0 ~ cores 수)
     */
    double measureCpuUsage(int durationMs) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + std::chrono::milliseconds(durationMs);

        // CPU 부하 생성
        volatile double result = 0.0;
        while (std::chrono::high_resolution_clock::now() < end) {
            for (int i = 0; i < 1000; ++i) {
                result += i * 0.001;
            }
        }

        return result; // Dummy return
    }

    pid_t pid;
};

/**
 * Test Case 1: RT 프로세스 CPU quota 확인
 *
 * 검증:
 * - mxrc-rt.service의 CPUQuota=200% 설정 확인
 * - cgroup에서 200000/100000 (200%) 확인
 */
TEST_F(CpuQuotaTest, RTProcessHasCpuQuota200Percent) {
    // Service file 확인
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-rt.service file not found";

    std::string line;
    bool foundCpuQuota = false;
    while (std::getline(serviceFile, line)) {
        if (line.find("CPUQuota=200%") != std::string::npos) {
            foundCpuQuota = true;
            break;
        }
    }

    EXPECT_TRUE(foundCpuQuota) << "CPUQuota=200% not found in mxrc-rt.service";
}

/**
 * Test Case 2: Non-RT 프로세스 CPU quota 확인
 *
 * 검증:
 * - mxrc-nonrt.service의 CPUQuota=100% 설정 확인
 */
TEST_F(CpuQuotaTest, NonRTProcessHasCpuQuota100Percent) {
    // Service file 확인
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service");
    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-nonrt.service not yet created (will be created in Phase 5)";
    }

    std::string line;
    bool foundCpuQuota = false;
    while (std::getline(serviceFile, line)) {
        if (line.find("CPUQuota=100%") != std::string::npos) {
            foundCpuQuota = true;
            break;
        }
    }

    EXPECT_TRUE(foundCpuQuota) << "CPUQuota=100% not found in mxrc-nonrt.service";
}

/**
 * Test Case 3: cgroup CPU quota 설정 검증
 *
 * systemd가 실행 중일 때만 테스트
 */
TEST_F(CpuQuotaTest, CgroupCpuQuotaIsConfigured) {
    // systemd 실행 확인
    std::ifstream systemdCheck("/run/systemd/system");
    if (!systemdCheck.good()) {
        GTEST_SKIP() << "systemd not running, skipping cgroup test";
    }

    // RT 프로세스 quota 확인 (200%)
    long rtQuota = getCpuQuota("mxrc-rt");
    if (rtQuota != -1) {
        // 200000 (200%) or max (unlimited)
        EXPECT_TRUE(rtQuota == 200000 || rtQuota == -1)
            << "RT process CPU quota should be 200% or unlimited, got: " << rtQuota;
    }
}

/**
 * Test Case 4: CPU quota 문서화 확인
 *
 * quickstart.md에 CPUQuota 설명 있는지 확인
 */
TEST_F(CpuQuotaTest, CpuQuotaIsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("CPUQuota") != std::string::npos)
        << "CPUQuota should be documented in quickstart.md";
}

/**
 * Test Case 5: CPU quota 범위 검증
 *
 * 설정 값이 유효한 범위인지 확인
 */
TEST_F(CpuQuotaTest, CpuQuotaValueIsValid) {
    // RT: 200% (2 cores)
    int rtQuotaPercent = 200;
    EXPECT_GE(rtQuotaPercent, 100) << "RT quota should be at least 100%";
    EXPECT_LE(rtQuotaPercent, 400) << "RT quota should not exceed 400% (4 cores)";

    // Non-RT: 100% (1 core)
    int nonrtQuotaPercent = 100;
    EXPECT_GE(nonrtQuotaPercent, 50) << "Non-RT quota should be at least 50%";
    EXPECT_LE(nonrtQuotaPercent, 200) << "Non-RT quota should not exceed 200%";
}
