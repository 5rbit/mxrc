#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

/**
 * systemd 메트릭 수집 테스트
 *
 * User Story 5: Prometheus 메트릭 수집
 *
 * 테스트 시나리오:
 * 1. systemd 서비스 상태 메트릭 수집
 * 2. CPU/메모리 사용률 메트릭 수집
 * 3. 재시작 횟수 메트릭 수집
 */

class MetricsCollectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief 명령어 실행 및 출력 캡처
     */
    std::string executeCommand(const std::string& command) {
        std::array<char, 128> buffer;
        std::string result;

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "";
        }

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }

        pclose(pipe);
        return result;
    }

    /**
     * @brief systemd가 실행 중인지 확인
     */
    bool isSystemdRunning() {
        std::ifstream systemdCheck("/run/systemd/system");
        return systemdCheck.good();
    }
};

/**
 * Test Case 1: systemctl show로 서비스 상태 조회 가능
 *
 * Prometheus exporter는 systemctl show 출력을 파싱
 */
TEST_F(MetricsCollectionTest, CanGetServiceStatusWithSystemctl) {
    if (!isSystemdRunning()) {
        GTEST_SKIP() << "systemd not running, skipping metrics test";
    }

    // systemctl show 명령으로 서비스 상태 조회
    std::string command = "systemctl show sshd.service --property=ActiveState,SubState,LoadState 2>&1";
    std::string output = executeCommand(command);

    // 기본 속성이 있어야 함
    EXPECT_TRUE(output.find("ActiveState=") != std::string::npos)
        << "systemctl show should return ActiveState";
    EXPECT_TRUE(output.find("SubState=") != std::string::npos)
        << "systemctl show should return SubState";
    EXPECT_TRUE(output.find("LoadState=") != std::string::npos)
        << "systemctl show should return LoadState";
}

/**
 * Test Case 2: 메모리 사용량 메트릭 조회 가능
 */
TEST_F(MetricsCollectionTest, CanGetMemoryMetrics) {
    if (!isSystemdRunning()) {
        GTEST_SKIP() << "systemd not running, skipping metrics test";
    }

    // systemctl show로 메모리 사용량 조회
    std::string command = "systemctl show sshd.service --property=MemoryCurrent 2>&1";
    std::string output = executeCommand(command);

    EXPECT_TRUE(output.find("MemoryCurrent=") != std::string::npos)
        << "systemctl show should return MemoryCurrent";
}

/**
 * Test Case 3: CPU 사용량 메트릭 조회 가능
 */
TEST_F(MetricsCollectionTest, CanGetCpuMetrics) {
    if (!isSystemdRunning()) {
        GTEST_SKIP() << "systemd not running, skipping metrics test";
    }

    // systemctl show로 CPU 사용량 조회
    std::string command = "systemctl show sshd.service --property=CPUUsageNSec 2>&1";
    std::string output = executeCommand(command);

    EXPECT_TRUE(output.find("CPUUsageNSec=") != std::string::npos)
        << "systemctl show should return CPUUsageNSec";
}

/**
 * Test Case 4: 재시작 횟수 메트릭 조회 가능
 */
TEST_F(MetricsCollectionTest, CanGetRestartMetrics) {
    if (!isSystemdRunning()) {
        GTEST_SKIP() << "systemd not running, skipping metrics test";
    }

    // systemctl show로 재시작 횟수 조회
    std::string command = "systemctl show sshd.service --property=NRestarts 2>&1";
    std::string output = executeCommand(command);

    EXPECT_TRUE(output.find("NRestarts=") != std::string::npos)
        << "systemctl show should return NRestarts";
}

/**
 * Test Case 5: MXRC RT 서비스 메트릭 수집 설정 확인
 *
 * systemd 서비스 파일에 필요한 설정이 있는지 확인
 */
TEST_F(MetricsCollectionTest, RTServiceHasMetricsSettings) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-rt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-rt.service file not found";

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // CPUAccounting, MemoryAccounting이 활성화되어야 메트릭 수집 가능
    bool hasCpuAccounting = (content.find("CPUAccounting=") != std::string::npos);
    bool hasMemoryAccounting = (content.find("MemoryAccounting=") != std::string::npos);

    // 최소한 하나는 있어야 함
    EXPECT_TRUE(hasCpuAccounting || hasMemoryAccounting)
        << "Service should have CPUAccounting or MemoryAccounting enabled";
}

/**
 * Test Case 6: MXRC Non-RT 서비스 메트릭 수집 설정 확인
 */
TEST_F(MetricsCollectionTest, NonRTServiceHasMetricsSettings) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-nonrt.service");
    ASSERT_TRUE(serviceFile.is_open()) << "mxrc-nonrt.service file not found";

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // CPUAccounting, MemoryAccounting이 활성화되어야 메트릭 수집 가능
    bool hasCpuAccounting = (content.find("CPUAccounting=") != std::string::npos);
    bool hasMemoryAccounting = (content.find("MemoryAccounting=") != std::string::npos);

    // 최소한 하나는 있어야 함
    EXPECT_TRUE(hasCpuAccounting || hasMemoryAccounting)
        << "Service should have CPUAccounting or MemoryAccounting enabled";
}

/**
 * Test Case 7: 메트릭 수집 간격 설정 확인
 *
 * 메트릭 수집 스크립트나 타이머가 있어야 함
 */
TEST_F(MetricsCollectionTest, MetricsCollectionTimerExists) {
    // 메트릭 수집 타이머 파일 확인
    std::ifstream timerFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-metrics.timer");

    if (!timerFile.is_open()) {
        GTEST_SKIP() << "mxrc-metrics.timer not created yet (optional)";
    }

    std::string content((std::istreambuf_iterator<char>(timerFile)),
                        std::istreambuf_iterator<char>());

    // OnUnitActiveSec 또는 OnCalendar가 있어야 주기적 실행
    bool hasInterval = (content.find("OnUnitActiveSec=") != std::string::npos) ||
                       (content.find("OnCalendar=") != std::string::npos);

    EXPECT_TRUE(hasInterval)
        << "Metrics timer should have OnUnitActiveSec or OnCalendar";
}
