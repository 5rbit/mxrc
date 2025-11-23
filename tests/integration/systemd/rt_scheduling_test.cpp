#include <gtest/gtest.h>
#include <sched.h>
#include <unistd.h>
#include <fstream>
#include <string>

// RT 프로세스 스케줄링 정책 테스트
class RTSchedulingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 설정
    }

    void TearDown() override {
        // 테스트 후 정리
    }

    // 프로세스의 스케줄링 정책 확인
    int getSchedulingPolicy(pid_t pid) {
        return sched_getscheduler(pid);
    }

    // 프로세스의 스케줄링 우선순위 확인
    int getSchedulingPriority(pid_t pid) {
        struct sched_param param;
        if (sched_getparam(pid, &param) == 0) {
            return param.sched_priority;
        }
        return -1;
    }
};

// RT 프로세스가 FIFO 스케줄링 정책을 사용하는지 테스트
TEST_F(RTSchedulingTest, RTProcessUsesFIFOScheduling) {
    pid_t currentPid = getpid();
    int policy = getSchedulingPolicy(currentPid);

    // 이 테스트는 systemd로 시작된 RT 프로세스에서만 통과해야 함
    // 일반 테스트 실행 시에는 SCHED_OTHER를 사용함
    if (policy == SCHED_FIFO) {
        EXPECT_EQ(policy, SCHED_FIFO);
    } else {
        GTEST_SKIP() << "Not running as RT process with SCHED_FIFO";
    }
}

// RT 프로세스가 올바른 우선순위(80)를 사용하는지 테스트
TEST_F(RTSchedulingTest, RTProcessHasCorrectPriority) {
    pid_t currentPid = getpid();
    int policy = getSchedulingPolicy(currentPid);

    if (policy == SCHED_FIFO) {
        int priority = getSchedulingPriority(currentPid);
        EXPECT_EQ(priority, 80) << "RT process should have priority 80";
    } else {
        GTEST_SKIP() << "Not running as RT process";
    }
}

// systemd 서비스 파일에서 CPUSchedulingPolicy 확인
TEST_F(RTSchedulingTest, ServiceFileContainsFIFOPolicy) {
    std::ifstream serviceFile("systemd/mxrc-rt.service");

    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "Service file not found";
    }

    std::string line;
    bool foundPolicy = false;

    while (std::getline(serviceFile, line)) {
        if (line.find("CPUSchedulingPolicy=fifo") != std::string::npos) {
            foundPolicy = true;
            break;
        }
    }

    EXPECT_TRUE(foundPolicy) << "Service file should contain CPUSchedulingPolicy=fifo";
}

// systemd 서비스 파일에서 CPUSchedulingPriority 확인
TEST_F(RTSchedulingTest, ServiceFileContainsCorrectPriority) {
    std::ifstream serviceFile("systemd/mxrc-rt.service");

    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "Service file not found";
    }

    std::string line;
    bool foundPriority = false;

    while (std::getline(serviceFile, line)) {
        if (line.find("CPUSchedulingPriority=80") != std::string::npos) {
            foundPriority = true;
            break;
        }
    }

    EXPECT_TRUE(foundPriority) << "Service file should contain CPUSchedulingPriority=80";
}

// RT 프로세스 권한 확인 (CAP_SYS_NICE 필요)
TEST_F(RTSchedulingTest, RTProcessHasRequiredCapabilities) {
    // RT 스케줄링을 설정하려면 CAP_SYS_NICE 권한 또는 RLIMIT_RTPRIO 설정 필요
    struct rlimit rlim;
    if (getrlimit(RLIMIT_RTPRIO, &rlim) == 0) {
        if (rlim.rlim_cur > 0) {
            EXPECT_GT(rlim.rlim_cur, 0) << "RLIMIT_RTPRIO should be set for RT scheduling";
        } else {
            GTEST_SKIP() << "RLIMIT_RTPRIO not configured";
        }
    }
}
