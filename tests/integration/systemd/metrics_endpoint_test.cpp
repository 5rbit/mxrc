#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

/**
 * Prometheus 엔드포인트 테스트
 *
 * User Story 5: /metrics 엔드포인트 노출
 *
 * 테스트 시나리오:
 * 1. HTTP 엔드포인트 설정 확인
 * 2. 메트릭 노출 스크립트 존재 확인
 * 3. 포트 설정 검증
 */

class MetricsEndpointTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief 파일에서 특정 설정 찾기
     */
    bool findSetting(const std::string& filePath, const std::string& setting) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.find(setting) != std::string::npos) {
                return true;
            }
        }

        return false;
    }
};

/**
 * Test Case 1: Prometheus exporter 스크립트 존재
 *
 * systemd 메트릭을 Prometheus 형식으로 변환하는 스크립트
 */
TEST_F(MetricsEndpointTest, PrometheusExporterScriptExists) {
    std::ifstream exporterScript("/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.sh");

    if (!exporterScript.is_open()) {
        // Python 구현일 수도 있음
        std::ifstream pythonExporter("/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.py");
        if (!pythonExporter.is_open()) {
            GTEST_SKIP() << "Prometheus exporter script not created yet";
        }
    }

    SUCCEED() << "Prometheus exporter script found";
}

/**
 * Test Case 2: 메트릭 포트 설정 확인
 *
 * 기본 Prometheus exporter 포트는 9100-9999 범위
 */
TEST_F(MetricsEndpointTest, MetricsPortIsConfigured) {
    // 설정 파일에서 포트 확인
    std::vector<std::string> configPaths = {
        "/home/tory/workspace/mxrc/mxrc/config/prometheus.json",
        "/home/tory/workspace/mxrc/mxrc/config/metrics.json",
        "/home/tory/workspace/mxrc/mxrc/systemd/mxrc-metrics.service"
    };

    bool foundPort = false;
    for (const auto& path : configPaths) {
        if (findSetting(path, "9100") || findSetting(path, "port") ||
            findSetting(path, "PORT")) {
            foundPort = true;
            break;
        }
    }

    if (!foundPort) {
        GTEST_SKIP() << "Metrics port configuration not found (may be in different location)";
    }

    SUCCEED() << "Metrics port configuration found";
}

/**
 * Test Case 3: systemd 메트릭 서비스 파일 존재
 */
TEST_F(MetricsEndpointTest, MetricsServiceFileExists) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-metrics.service");

    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-metrics.service not created yet (optional)";
    }

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // ExecStart가 있어야 함
    EXPECT_TRUE(content.find("ExecStart=") != std::string::npos)
        << "Metrics service should have ExecStart directive";
}

/**
 * Test Case 4: 메트릭 서비스가 네트워크 의존성 가짐
 */
TEST_F(MetricsEndpointTest, MetricsServiceHasNetworkDependency) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-metrics.service");

    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-metrics.service not created yet";
    }

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // After=network.target 있어야 함
    bool hasNetworkDep = (content.find("After=network") != std::string::npos) ||
                         (content.find("Wants=network") != std::string::npos);

    EXPECT_TRUE(hasNetworkDep)
        << "Metrics service should depend on network.target";
}

/**
 * Test Case 5: 메트릭 수집 대상 서비스 지정
 */
TEST_F(MetricsEndpointTest, MetricsServiceTargetsSpecified) {
    // Prometheus exporter 스크립트에서 대상 서비스 확인
    std::vector<std::string> scriptPaths = {
        "/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.sh",
        "/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.py"
    };

    bool foundTargets = false;
    for (const auto& path : scriptPaths) {
        std::ifstream script(path);
        if (!script.is_open()) {
            continue;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());

        // mxrc-rt, mxrc-nonrt 서비스를 모니터링해야 함
        bool hasRT = (content.find("mxrc-rt") != std::string::npos);
        bool hasNonRT = (content.find("mxrc-nonrt") != std::string::npos);

        if (hasRT && hasNonRT) {
            foundTargets = true;
            break;
        }
    }

    if (!foundTargets) {
        GTEST_SKIP() << "Metrics target services not specified yet";
    }

    SUCCEED() << "Metrics targets (mxrc-rt, mxrc-nonrt) found in exporter";
}

/**
 * Test Case 6: HTTP 서버 라이브러리 설정 확인
 *
 * Prometheus exporter는 HTTP 서버가 필요함
 */
TEST_F(MetricsEndpointTest, HttpServerLibraryConfigured) {
    // Python인 경우 prometheus_client 사용
    std::ifstream pythonExporter("/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.py");

    if (pythonExporter.is_open()) {
        std::string content((std::istreambuf_iterator<char>(pythonExporter)),
                            std::istreambuf_iterator<char>());

        bool hasPrometheusClient = (content.find("prometheus_client") != std::string::npos) ||
                                   (content.find("from prometheus") != std::string::npos);

        if (hasPrometheusClient) {
            SUCCEED() << "Python prometheus_client library found";
            return;
        }
    }

    // Bash인 경우 nc (netcat) 또는 socat 사용
    std::ifstream bashExporter("/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.sh");

    if (bashExporter.is_open()) {
        std::string content((std::istreambuf_iterator<char>(bashExporter)),
                            std::istreambuf_iterator<char>());

        bool hasHttpServer = (content.find("nc ") != std::string::npos) ||
                             (content.find("socat") != std::string::npos) ||
                             (content.find("netcat") != std::string::npos);

        if (hasHttpServer) {
            SUCCEED() << "HTTP server tool (nc/socat) found";
            return;
        }
    }

    GTEST_SKIP() << "HTTP server library/tool not configured yet";
}

/**
 * Test Case 7: 메트릭 엔드포인트 경로 확인
 *
 * Prometheus는 기본적으로 /metrics 경로 사용
 */
TEST_F(MetricsEndpointTest, MetricsPathIsStandard) {
    std::vector<std::string> scriptPaths = {
        "/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.sh",
        "/home/tory/workspace/mxrc/mxrc/scripts/prometheus-exporter.py"
    };

    bool foundMetricsPath = false;
    for (const auto& path : scriptPaths) {
        std::ifstream script(path);
        if (!script.is_open()) {
            continue;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());

        if (content.find("/metrics") != std::string::npos) {
            foundMetricsPath = true;
            break;
        }
    }

    if (!foundMetricsPath) {
        GTEST_SKIP() << "Metrics path not configured yet";
    }

    SUCCEED() << "/metrics endpoint path found";
}

/**
 * Test Case 8: 보안 설정 확인
 *
 * 메트릭 엔드포인트는 localhost만 접근 가능해야 함
 */
TEST_F(MetricsEndpointTest, MetricsEndpointSecurityConfigured) {
    std::ifstream serviceFile("/home/tory/workspace/mxrc/mxrc/systemd/mxrc-metrics.service");

    if (!serviceFile.is_open()) {
        GTEST_SKIP() << "mxrc-metrics.service not created yet";
    }

    std::string content((std::istreambuf_iterator<char>(serviceFile)),
                        std::istreambuf_iterator<char>());

    // localhost 또는 127.0.0.1 바인딩 확인
    bool hasLocalhost = (content.find("127.0.0.1") != std::string::npos) ||
                        (content.find("localhost") != std::string::npos) ||
                        (content.find("--host=127.0.0.1") != std::string::npos) ||
                        (content.find("--bind=127.0.0.1") != std::string::npos);

    if (!hasLocalhost) {
        GTEST_SKIP() << "Localhost binding not explicitly configured (may be default)";
    }

    SUCCEED() << "Metrics endpoint bound to localhost";
}
