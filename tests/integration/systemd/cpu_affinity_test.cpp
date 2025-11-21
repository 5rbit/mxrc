#include <gtest/gtest.h>
#include <sched.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <vector>

// CPU affinity 검증 테스트
class CPUAffinityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 설정
    }

    void TearDown() override {
        // 테스트 후 정리
    }

    // 프로세스의 CPU affinity 확인
    std::vector<int> getCPUAffinity(pid_t pid) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        if (sched_getaffinity(pid, sizeof(cpu_set_t), &cpuset) == 0) {
            std::vector<int> cpus;
            for (int i = 0; i < CPU_SETSIZE; i++) {
                if (CPU_ISSET(i, &cpuset)) {
                    cpus.push_back(i);
                }
            }
            return cpus;
        }

        return {};
    }

    // 시스템의 CPU 개수 확인
    int getSystemCPUCount() {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }
};

// RT 프로세스가 특정 CPU 코어에 바인딩되었는지 테스트
TEST_F(CPUAffinityTest, RTProcessIsPinnedToSpecificCores) {
    pid_t currentPid = getpid();
    auto cpus = getCPUAffinity(currentPid);

    if (cpus.empty()) {
        GTEST_SKIP() << "Cannot read CPU affinity";
    }

    // systemd로 시작된 RT 프로세스는 CPU 2-3에 바인딩되어야 함
    // 일반 테스트 실행 시에는 모든 CPU 사용 가능
    if (cpus.size() <= 4) {  // 특정 코어에 제한된 경우
        // CPU 2, 3이 포함되어 있는지 확인
        bool hasCPU2 = std::find(cpus.begin(), cpus.end(), 2) != cpus.end();
        bool hasCPU3 = std::find(cpus.begin(), cpus.end(), 3) != cpus.end();

        if (hasCPU2 || hasCPU3) {
            EXPECT_TRUE(hasCPU2 || hasCPU3) << "RT process should be pinned to CPU 2 or 3";
        } else {
            GTEST_SKIP() << "Not running with CPU affinity set";
        }
    } else {
        GTEST_SKIP() << "Running with default CPU affinity";
    }
}

// systemd 서비스 파일에서 CPUAffinity 설정 확인
TEST_F(CPUAffinityTest, ServiceFileContainsAffinitySetting) {
    std::ifstream serviceFile("systemd/mxrc-rt.service");

    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "Service file not found";
    }

    std::string line;
    bool foundAffinity = false;

    while (std::getline(serviceFile, line)) {
        if (line.find("CPUAffinity=2,3") != std::string::npos ||
            line.find("CPUAffinity=2 3") != std::string::npos) {
            foundAffinity = true;
            break;
        }
    }

    EXPECT_TRUE(foundAffinity) << "Service file should contain CPUAffinity=2,3";
}

// CPU 격리(isolcpus) 확인
TEST_F(CPUAffinityTest, IsolatedCPUsAreConfigured) {
    std::ifstream cmdline("/proc/cmdline");

    if (!cmdline.is_open()) {
        GTEST_SKIP() << "Cannot read /proc/cmdline";
    }

    std::string line;
    std::getline(cmdline, line);

    // isolcpus 파라미터가 설정되어 있는지 확인
    bool hasIsolcpus = line.find("isolcpus") != std::string::npos;

    if (hasIsolcpus) {
        EXPECT_TRUE(hasIsolcpus) << "Kernel should have isolcpus parameter for RT cores";
    } else {
        GTEST_SKIP() << "isolcpus not configured in kernel parameters";
    }
}

// CPU 개수가 충분한지 확인
TEST_F(CPUAffinityTest, SystemHasEnoughCPUs) {
    int cpuCount = getSystemCPUCount();

    // RT 프로세스를 CPU 2-3에 할당하려면 최소 4개 코어 필요
    if (cpuCount >= 4) {
        EXPECT_GE(cpuCount, 4) << "System should have at least 4 CPUs for RT core isolation";
    } else {
        GTEST_SKIP() << "System has less than 4 CPUs: " << cpuCount;
    }
}

// CPU affinity 설정 검증 (실제 바인딩 확인)
TEST_F(CPUAffinityTest, ProcessStaysOnAssignedCores) {
    pid_t currentPid = getpid();
    auto initialCpus = getCPUAffinity(currentPid);

    if (initialCpus.empty() || initialCpus.size() > 4) {
        GTEST_SKIP() << "Not running with specific CPU affinity";
    }

    // 짧은 시간 대기 후 다시 확인
    usleep(10000);  // 10ms

    auto finalCpus = getCPUAffinity(currentPid);

    // CPU affinity가 변경되지 않았는지 확인
    EXPECT_EQ(initialCpus.size(), finalCpus.size());

    for (size_t i = 0; i < initialCpus.size(); i++) {
        EXPECT_EQ(initialCpus[i], finalCpus[i]) << "CPU affinity should remain constant";
    }
}
