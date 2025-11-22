#ifndef MXRC_SYSTEMD_SYSTEMD_METRICS_COLLECTOR_HPP
#define MXRC_SYSTEMD_SYSTEMD_METRICS_COLLECTOR_HPP

#include "ISystemdMetricsCollector.hpp"
#include <memory>
#include <array>

namespace mxrc {
namespace systemd {

/**
 * @brief systemd 메트릭 수집 구현체
 *
 * systemctl show 명령을 사용하여 메트릭 수집
 */
class SystemdMetricsCollector : public ISystemdMetricsCollector {
public:
    SystemdMetricsCollector() = default;
    ~SystemdMetricsCollector() override = default;

    std::string getServiceState(const std::string& serviceName) override;
    uint64_t getCpuUsageNanoseconds(const std::string& serviceName) override;
    uint64_t getMemoryUsageBytes(const std::string& serviceName) override;
    uint32_t getRestartCount(const std::string& serviceName) override;
    std::map<std::string, std::string> getAllMetrics(const std::string& serviceName) override;
    std::map<std::string, std::map<std::string, std::string>>
    collectMetrics(const std::vector<std::string>& serviceNames) override;

private:
    /**
     * @brief systemctl show 실행
     *
     * @param serviceName 서비스 이름
     * @param property 조회할 속성
     * @return 속성 값
     */
    std::string executeSystemctl(const std::string& serviceName, const std::string& property);

    /**
     * @brief 명령어 실행 및 출력 캡처
     */
    std::string executeCommand(const std::string& command);
};

} // namespace systemd
} // namespace mxrc

#endif // MXRC_SYSTEMD_SYSTEMD_METRICS_COLLECTOR_HPP
