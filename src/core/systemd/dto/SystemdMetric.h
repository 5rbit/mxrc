#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace mxrc {
namespace systemd {
namespace dto {

/**
 * @brief systemd 메트릭 데이터를 표현하는 DTO
 *
 * systemctl show 명령어 출력을 파싱하여 Prometheus 형식으로 내보내기 위한 데이터 구조.
 *
 * @example
 * SystemdMetric metric;
 * metric.serviceName = "mxrc-rt.service";
 * metric.metricName = "ActiveState";
 * metric.value = 1.0;  // active = 1, inactive = 0
 * metric.timestamp = std::chrono::system_clock::now();
 */
struct SystemdMetric {
    /// 서비스 이름 (예: "mxrc-rt.service")
    std::string serviceName;

    /// 메트릭 이름 (예: "ActiveState", "CPUUsageNSec", "MemoryCurrent")
    std::string metricName;

    /// 메트릭 값 (숫자로 변환된 값)
    double value;

    /// 메트릭 수집 시각
    std::chrono::system_clock::time_point timestamp;

    /// 메트릭 단위 (선택적, 예: "bytes", "nanoseconds")
    std::string unit;

    /// 메트릭 레이블 (선택적, key=value 형태)
    std::string labels;

    /**
     * @brief 기본 생성자
     */
    SystemdMetric() : value(0.0) {}

    /**
     * @brief 매개변수 생성자
     */
    SystemdMetric(const std::string& service,
                  const std::string& name,
                  double val)
        : serviceName(service)
        , metricName(name)
        , value(val)
        , timestamp(std::chrono::system_clock::now()) {}
};

} // namespace dto
} // namespace systemd
} // namespace mxrc
