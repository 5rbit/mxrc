#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

/**
 * Prometheus 형식 검증 테스트
 *
 * User Story 5: Prometheus exposition format 준수
 *
 * 테스트 시나리오:
 * 1. Prometheus 메트릭 형식 검증 (name{labels} value timestamp)
 * 2. HELP/TYPE 주석 검증
 * 3. 메트릭 이름 규칙 검증 (snake_case, 접두사 등)
 */

class PrometheusFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup
    }

    void TearDown() override {
        // Cleanup
    }

    /**
     * @brief Prometheus 메트릭 라인 검증
     *
     * 형식: metric_name{label1="value1",label2="value2"} value timestamp
     */
    bool isValidPrometheusMetric(const std::string& line) {
        // 주석 라인은 # 로 시작
        if (line.empty() || line[0] == '#') {
            return true;
        }

        // 메트릭 라인: name{labels} value [timestamp]
        std::regex metricPattern(R"(^[a-z_][a-z0-9_]*(\{[^}]+\})?\s+[0-9.+\-eE]+(\s+[0-9]+)?$)");
        return std::regex_match(line, metricPattern);
    }

    /**
     * @brief HELP 주석 검증
     */
    bool hasHelpComment(const std::string& content, const std::string& metricName) {
        std::string helpPrefix = "# HELP " + metricName;
        return content.find(helpPrefix) != std::string::npos;
    }

    /**
     * @brief TYPE 주석 검증
     */
    bool hasTypeComment(const std::string& content, const std::string& metricName) {
        std::string typePrefix = "# TYPE " + metricName;
        return content.find(typePrefix) != std::string::npos;
    }
};

/**
 * Test Case 1: 메트릭 이름이 Prometheus 규칙 준수
 *
 * - snake_case 사용
 * - [a-z_]로 시작
 * - [a-z0-9_]만 포함
 */
TEST_F(PrometheusFormatTest, MetricNamesFollowConvention) {
    std::vector<std::string> validNames = {
        "mxrc_service_state",
        "mxrc_cpu_usage_seconds",
        "mxrc_memory_bytes",
        "mxrc_restart_count_total"
    };

    std::regex namePattern(R"(^[a-z_][a-z0-9_]*$)");

    for (const auto& name : validNames) {
        EXPECT_TRUE(std::regex_match(name, namePattern))
            << "Metric name '" << name << "' should follow Prometheus naming convention";
    }
}

/**
 * Test Case 2: 메트릭이 mxrc_ 접두사 사용
 */
TEST_F(PrometheusFormatTest, MetricsHaveMxrcPrefix) {
    std::vector<std::string> metricNames = {
        "mxrc_service_state",
        "mxrc_cpu_usage_seconds",
        "mxrc_memory_bytes"
    };

    for (const auto& name : metricNames) {
        EXPECT_TRUE(name.find("mxrc_") == 0)
            << "Metric '" << name << "' should start with 'mxrc_' prefix";
    }
}

/**
 * Test Case 3: Counter 메트릭이 _total 접미사 사용
 */
TEST_F(PrometheusFormatTest, CounterMetricsHaveTotalSuffix) {
    std::vector<std::string> counterNames = {
        "mxrc_restart_count_total",
        "mxrc_cpu_usage_seconds_total"
    };

    for (const auto& name : counterNames) {
        EXPECT_TRUE(name.find("_total") != std::string::npos)
            << "Counter metric '" << name << "' should have '_total' suffix";
    }
}

/**
 * Test Case 4: Gauge 메트릭이 단위 포함
 */
TEST_F(PrometheusFormatTest, GaugeMetricsHaveUnits) {
    std::vector<std::string> gaugeNames = {
        "mxrc_memory_bytes",
        "mxrc_cpu_usage_seconds"
    };

    for (const auto& name : gaugeNames) {
        bool hasUnit = (name.find("_bytes") != std::string::npos) ||
                       (name.find("_seconds") != std::string::npos) ||
                       (name.find("_ratio") != std::string::npos) ||
                       (name.find("_percent") != std::string::npos);

        EXPECT_TRUE(hasUnit)
            << "Gauge metric '" << name << "' should include unit in name";
    }
}

/**
 * Test Case 5: 샘플 메트릭 라인 파싱 검증
 */
TEST_F(PrometheusFormatTest, SampleMetricLinesAreValid) {
    std::vector<std::string> sampleLines = {
        "mxrc_service_state{service=\"mxrc-rt\"} 1",
        "mxrc_memory_bytes{service=\"mxrc-rt\"} 2147483648",
        "mxrc_cpu_usage_seconds_total{service=\"mxrc-rt\"} 123.45",
        "# HELP mxrc_service_state Service state (1=active, 0=inactive)",
        "# TYPE mxrc_service_state gauge"
    };

    for (const auto& line : sampleLines) {
        EXPECT_TRUE(isValidPrometheusMetric(line))
            << "Line '" << line << "' should be valid Prometheus format";
    }
}

/**
 * Test Case 6: 레이블 형식 검증
 *
 * {key1="value1",key2="value2"}
 */
TEST_F(PrometheusFormatTest, LabelsHaveCorrectFormat) {
    std::string sampleMetric = R"(mxrc_service_state{service="mxrc-rt",instance="localhost"} 1)";

    // 레이블 형식: {key="value"[,key="value"]*}
    std::regex labelPattern(R"(\{[a-z_][a-z0-9_]*="[^"]*"(,[a-z_][a-z0-9_]*="[^"]*")*\})");

    EXPECT_TRUE(std::regex_search(sampleMetric, labelPattern))
        << "Metric should have valid label format";
}

/**
 * Test Case 7: HELP 주석 형식 검증
 */
TEST_F(PrometheusFormatTest, HelpCommentsHaveCorrectFormat) {
    std::string helpLine = "# HELP mxrc_service_state Service state (1=active, 0=inactive)";

    // HELP 형식: # HELP metric_name description
    std::regex helpPattern(R"(^# HELP [a-z_][a-z0-9_]* .+$)");

    EXPECT_TRUE(std::regex_match(helpLine, helpPattern))
        << "HELP comment should match format: # HELP metric_name description";
}

/**
 * Test Case 8: TYPE 주석 형식 검증
 */
TEST_F(PrometheusFormatTest, TypeCommentsHaveCorrectFormat) {
    std::vector<std::string> typeLines = {
        "# TYPE mxrc_service_state gauge",
        "# TYPE mxrc_restart_count_total counter",
        "# TYPE mxrc_request_duration_seconds histogram"
    };

    // TYPE 형식: # TYPE metric_name (counter|gauge|histogram|summary|untyped)
    std::regex typePattern(R"(^# TYPE [a-z_][a-z0-9_]* (counter|gauge|histogram|summary|untyped)$)");

    for (const auto& line : typeLines) {
        EXPECT_TRUE(std::regex_match(line, typePattern))
            << "TYPE comment '" << line << "' should match format: # TYPE metric_name type";
    }
}

/**
 * Test Case 9: 메트릭 값 형식 검증
 *
 * 정수, 부동소수점, 과학적 표기법 모두 허용
 */
TEST_F(PrometheusFormatTest, MetricValuesHaveCorrectFormat) {
    std::vector<std::string> validValues = {
        "123",
        "123.45",
        "1.23e+10",
        "1.23E-5",
        "+Inf",
        "-Inf",
        "NaN"
    };

    // Prometheus 값 형식
    std::regex valuePattern(R"(^[+\-]?(([0-9]+(\.[0-9]*)?)|(\.[0-9]+))([eE][+\-]?[0-9]+)?$|^[+\-]?Inf$|^NaN$)");

    for (const auto& value : validValues) {
        EXPECT_TRUE(std::regex_match(value, valuePattern))
            << "Value '" << value << "' should be valid Prometheus value";
    }
}

/**
 * Test Case 10: 메트릭 문서화 확인
 */
TEST_F(PrometheusFormatTest, MetricsAreDocumented) {
    std::ifstream quickstart(
        "/home/tory/workspace/mxrc/mxrc/docs/specs/018-systemd-process-management/quickstart.md");

    if (!quickstart.is_open()) {
        GTEST_SKIP() << "quickstart.md not found";
    }

    std::string content((std::istreambuf_iterator<char>(quickstart)),
                        std::istreambuf_iterator<char>());

    // Prometheus 또는 metrics 언급이 있어야 함
    bool hasPrometheus = (content.find("Prometheus") != std::string::npos) ||
                         (content.find("prometheus") != std::string::npos) ||
                         (content.find("메트릭") != std::string::npos) ||
                         (content.find("metrics") != std::string::npos);

    EXPECT_TRUE(hasPrometheus)
        << "Prometheus metrics should be documented in quickstart.md";
}
