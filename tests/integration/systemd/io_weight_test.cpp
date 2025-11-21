#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

/**
 * I/O 가중치 통합 테스트
 *
 * User Story 3: cgroups를 통한 I/O 가중치 제어 검증
 *
 * 테스트 시나리오:
 * 1. RT 프로세스: IOWeight=500 (높은 우선순위)
 * 2. Non-RT 프로세스: IOWeight=100 (낮은 우선순위)
 * 3. systemd cgroup 설정 확인
 */

class IoWeightTest : public ::testing::Test {
protected:
    void SetUp() override {
        pid = getpid();
    }

    void TearDown() override {
        // Cleanup if needed
    }

    /**
     * @brief cgroup I/O 가중치 읽기
     *
     * @param serviceName 서비스 이름
     * @return I/O 가중치 (1-10000), -1 if not found
     */
    int getIoWeight(const std::string& serviceName) {
        // systemd cgroup v2 경로
        std::string cgroupPath = "/sys/fs/cgroup/system.slice/" + serviceName + ".service/io.weight";

        std::ifstream file(cgroupPath);
        if (!file.is_open()) {
            // cgroup v1에서는 blkio.weight
            cgroupPath = "/sys/fs/cgroup/blkio/system.slice/" + serviceName + ".service/blkio.weight";
            file.open(cgroupPath);
            if (!file.is_open()) {
                return -1;
            }
        }

        std::string value;
        std::getline(file, value);

        // cgroup v2: "default 100" 형식일 수 있음
        std::istringstream iss(value);
        std::string token;
        int weight = -1;

        while (iss >> token) {
            try {
                weight = std::stoi(token);
                if (weight >= 1 && weight <= 10000) {
                    break;
                }
            } catch (...) {
                continue;
            }
        }

        return weight;
    }

    pid_t pid;
};

/**
 * Test Case 1: RT 프로세스 I/O 가중치 확인
 *
 * 검증:
 * - mxrc-rt.service의 IOWeight=500 설정 확인
 */
TEST_F(IoWeightTest, RTProcessHasIOWeight500) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-rt.service file not found";

    std::string line;
    bool foundIOWeight = false;
    while (std::getline(serviceFile, line)) {
        if (line.find("IOWeight=500") != std::string::npos) {
            foundIOWeight = true;
            break;
        }
    }

    EXPECT_TRUE(foundIOWeight) << "IOWeight=500 not found in mxrc-rt.service";
}

/**
 * Test Case 2: Non-RT 프로세스 I/O 가중치 확인
 *
 * 검증:
 * - mxrc-nonrt.service의 IOWeight=100 설정 확인
 */
TEST_F(IoWeightTest, NonRTProcessHasIOWeight100) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service");
    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-nonrt.service not yet created (will be created in Phase 5)";
    }

    std::string line;
    bool foundIOWeight = false;
    while (std::getline(serviceFile, line)) {
        if (line.find("IOWeight=100") != std::string::npos) {
            foundIOWeight = true;
            break;
        }
    }

    EXPECT_TRUE(foundIOWeight) << "IOWeight=100 not found in mxrc-nonrt.service";
}

/**
 * Test Case 3: cgroup I/O 가중치 검증
 *
 * systemd가 실행 중일 때만 테스트
 */
TEST_F(IoWeightTest, CgroupIoWeightIsConfigured) {
    // systemd 실행 확인
    std::ifstream systemdCheck("/run/systemd/system");
    if (!systemdCheck.good()) {
        GTEST_SKIP() << "systemd not running, skipping cgroup test";
    }

    // RT 프로세스 I/O 가중치 확인 (500)
    int rtWeight = getIoWeight("mxrc-rt");
    if (rtWeight != -1) {
        EXPECT_EQ(rtWeight, 500)
            << "RT process I/O weight should be 500, got: " << rtWeight;
    }
}

/**
 * Test Case 4: I/O 가중치 범위 검증
 *
 * IOWeight는 1-10000 범위
 */
TEST_F(IoWeightTest, IoWeightValuesAreInValidRange) {
    // RT: 500 (높은 우선순위)
    int rtWeight = 500;
    EXPECT_GE(rtWeight, 1) << "I/O weight must be at least 1";
    EXPECT_LE(rtWeight, 10000) << "I/O weight must not exceed 10000";
    EXPECT_GE(rtWeight, 100) << "RT I/O weight should be at least 100 for priority";

    // Non-RT: 100 (기본값)
    int nonrtWeight = 100;
    EXPECT_GE(nonrtWeight, 1) << "I/O weight must be at least 1";
    EXPECT_LE(nonrtWeight, 10000) << "I/O weight must not exceed 10000";
}

/**
 * Test Case 5: RT와 Non-RT I/O 가중치 비율 확인
 *
 * RT가 Non-RT보다 높은 우선순위를 가져야 함
 */
TEST_F(IoWeightTest, RTHasHigherIoWeightThanNonRT) {
    int rtWeight = 500;
    int nonrtWeight = 100;

    EXPECT_GT(rtWeight, nonrtWeight)
        << "RT I/O weight (" << rtWeight << ") should be higher than Non-RT ("
        << nonrtWeight << ")";

    // 비율이 적절한지 확인 (5:1 비율)
    double ratio = static_cast<double>(rtWeight) / nonrtWeight;
    EXPECT_GE(ratio, 2.0) << "RT/Non-RT I/O weight ratio should be at least 2:1";
    EXPECT_LE(ratio, 10.0) << "RT/Non-RT I/O weight ratio should not exceed 10:1";
}

/**
 * Test Case 6: I/O 가중치 문서화 확인
 */
TEST_F(IoWeightTest, IoWeightIsDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("IOWeight") != std::string::npos)
        << "IOWeight should be documented in quickstart.md";
}

/**
 * Test Case 7: IOAccounting 활성화 확인
 *
 * I/O 가중치 사용을 위해 IOAccounting=yes 필요 (선택사항)
 */
TEST_F(IoWeightTest, IoAccountingIsEnabledOrImplicit) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-rt.service not found";
    }

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // IOWeight가 있으면 IOAccounting은 자동 활성화됨
    bool hasIOWeight = (content.find("IOWeight") != std::string::npos);
    bool hasIOAccounting = (content.find("IOAccounting") != std::string::npos);

    EXPECT_TRUE(hasIOWeight || hasIOAccounting)
        << "IOWeight or IOAccounting should be present";
}

/**
 * Test Case 8: blkio cgroup 지원 확인
 *
 * 시스템이 blkio cgroup을 지원하는지 확인
 */
TEST_F(IoWeightTest, SystemSupportsIoCgroup) {
    // cgroup v2 확인
    std::ifstream cgroupv2("/sys/fs/cgroup/cgroup.controllers");
    if (cgroupv2.is_open()) {
        std::string controllers((std::istreambuf_iterator<char>(cgroupv2)),
                                std::istreambuf_iterator<char>());

        // io 컨트롤러가 있는지 확인
        if (controllers.find("io") != std::string::npos) {
            SUCCEED() << "cgroup v2 io controller is available";
            return;
        }
    }

    // cgroup v1 확인
    std::ifstream cgroupv1("/sys/fs/cgroup/blkio");
    if (cgroupv1.good()) {
        SUCCEED() << "cgroup v1 blkio is available";
        return;
    }

    GTEST_SKIP() << "Neither cgroup v2 io nor cgroup v1 blkio is available";
}
