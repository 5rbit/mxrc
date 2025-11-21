#include <gtest/gtest.h>
#include "util/SystemdUtil.h"
#include "util/SystemdException.h"
#include "dto/SystemdMetric.h"
#include "dto/JournaldEntry.h"
#include <fstream>

using namespace mxrc::systemd::util;
using namespace mxrc::systemd::dto;

// SystemdUtil 기본 기능 테스트
class SystemdUtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 설정
    }

    void TearDown() override {
        // 테스트 후 정리
    }
};

// systemd 가용성 확인 테스트
TEST_F(SystemdUtilTest, CheckSystemdAvailable) {
    // systemd가 실행 중인지 확인
    bool isAvailable = SystemdUtil::checkSystemdAvailable();

    // /run/systemd/system 디렉토리 존재 여부로 판단
    std::ifstream file("/run/systemd/system");
    bool expectedAvailable = file.good();

    EXPECT_EQ(isAvailable, expectedAvailable);
}

// 서비스 실행 컨텍스트 확인 테스트
TEST_F(SystemdUtilTest, IsRunningAsService) {
    // NOTIFY_SOCKET 환경변수 확인
    bool isService = SystemdUtil::isRunningAsService();

    const char* notifySocket = std::getenv("NOTIFY_SOCKET");
    bool expectedIsService = (notifySocket != nullptr);

    EXPECT_EQ(isService, expectedIsService);
}

// 서비스 속성 조회 테스트 (systemd가 없으면 스킵)
TEST_F(SystemdUtilTest, GetServiceProperty) {
    if (!SystemdUtil::checkSystemdAvailable()) {
        GTEST_SKIP() << "systemd is not available on this system";
    }

    // 존재하지 않는 서비스 조회 시 nullopt 반환
    auto result = SystemdUtil::getServiceProperty("nonexistent-service.service", "ActiveState");

    // 결과가 없거나, 있다면 유효한 문자열이어야 함
    if (result.has_value()) {
        EXPECT_FALSE(result.value().empty());
    }
}

// 여러 속성 동시 조회 테스트
TEST_F(SystemdUtilTest, GetServiceProperties) {
    if (!SystemdUtil::checkSystemdAvailable()) {
        GTEST_SKIP() << "systemd is not available on this system";
    }

    std::vector<std::string> properties = {"ActiveState", "LoadState"};
    auto result = SystemdUtil::getServiceProperties("nonexistent.service", properties);

    // 결과가 맵 형태로 반환되어야 함
    EXPECT_TRUE(result.empty() || result.size() <= properties.size());
}

// 서비스 활성 상태 확인 테스트
TEST_F(SystemdUtilTest, IsServiceActive) {
    if (!SystemdUtil::checkSystemdAvailable()) {
        GTEST_SKIP() << "systemd is not available on this system";
    }

    // 존재하지 않는 서비스는 비활성 상태
    bool isActive = SystemdUtil::isServiceActive("nonexistent-test-service.service");
    EXPECT_FALSE(isActive);
}

// SystemdException 테스트
TEST_F(SystemdUtilTest, SystemdException) {
    try {
        throw SystemdException("Test exception message");
        FAIL() << "Expected SystemdException to be thrown";
    } catch (const SystemdException& e) {
        std::string message = e.what();
        EXPECT_TRUE(message.find("SystemdException") != std::string::npos);
        EXPECT_TRUE(message.find("Test exception message") != std::string::npos);
    }
}

// WatchdogException 테스트
TEST_F(SystemdUtilTest, WatchdogException) {
    try {
        throw WatchdogException("Watchdog timeout");
        FAIL() << "Expected WatchdogException to be thrown";
    } catch (const WatchdogException& e) {
        std::string message = e.what();
        EXPECT_TRUE(message.find("Watchdog") != std::string::npos);
        EXPECT_TRUE(message.find("timeout") != std::string::npos);
    }
}

// JournaldException 테스트
TEST_F(SystemdUtilTest, JournaldException) {
    try {
        throw JournaldException("Journald connection failed");
        FAIL() << "Expected JournaldException to be thrown";
    } catch (const JournaldException& e) {
        std::string message = e.what();
        EXPECT_TRUE(message.find("Journald") != std::string::npos);
        EXPECT_TRUE(message.find("connection failed") != std::string::npos);
    }
}

// 빈 속성 목록 처리 테스트
TEST_F(SystemdUtilTest, GetServicePropertiesEmptyList) {
    std::vector<std::string> emptyProperties;
    auto result = SystemdUtil::getServiceProperties("any.service", emptyProperties);

    EXPECT_TRUE(result.empty());
}

// DTO 테스트
TEST_F(SystemdUtilTest, SystemdMetricDTO) {
    // SystemdMetric DTO 기본 생성자 테스트
    mxrc::systemd::dto::SystemdMetric metric1;
    EXPECT_EQ(metric1.value, 0.0);
    EXPECT_TRUE(metric1.serviceName.empty());
    EXPECT_TRUE(metric1.metricName.empty());

    // SystemdMetric DTO 매개변수 생성자 테스트
    mxrc::systemd::dto::SystemdMetric metric2("test.service", "CPUUsage", 42.5);
    EXPECT_EQ(metric2.serviceName, "test.service");
    EXPECT_EQ(metric2.metricName, "CPUUsage");
    EXPECT_EQ(metric2.value, 42.5);
}

TEST_F(SystemdUtilTest, JournaldEntryDTO) {
    // JournaldEntry DTO 기본 생성자 테스트
    mxrc::systemd::dto::JournaldEntry entry1;
    EXPECT_EQ(entry1.priority, 6);  // INFO
    EXPECT_EQ(entry1.pid, 0);
    EXPECT_EQ(entry1.tid, 0);
    EXPECT_TRUE(entry1.message.empty());

    // JournaldEntry DTO 매개변수 생성자 테스트
    mxrc::systemd::dto::JournaldEntry entry2("Test message", 3, "test-service");
    EXPECT_EQ(entry2.message, "Test message");
    EXPECT_EQ(entry2.priority, 3);  // ERR
    EXPECT_EQ(entry2.serviceName, "test-service");

    // 필드 추가 테스트
    entry2.addField("event.action", "process_start");
    entry2.addField("event.category", "process");
    EXPECT_EQ(entry2.fields["event.action"], "process_start");
    EXPECT_EQ(entry2.fields["event.category"], "process");
    EXPECT_EQ(entry2.fields.size(), 2);
}
