#ifndef MXRC_SYSTEMD_ISYSTEMD_METRICS_COLLECTOR_HPP
#define MXRC_SYSTEMD_ISYSTEMD_METRICS_COLLECTOR_HPP

#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace mxrc {
namespace systemd {

/**
 * @brief systemd 서비스 메트릭 수집 인터페이스
 *
 * User Story 5: Prometheus 메트릭 수집 및 노출
 *
 * 이 인터페이스는 systemd 서비스로부터 다음 메트릭을 수집합니다:
 * - 서비스 상태 (ActiveState, SubState)
 * - CPU 사용량 (CPUUsageNSec)
 * - 메모리 사용량 (MemoryCurrent)
 * - 재시작 횟수 (NRestarts)
 */
class ISystemdMetricsCollector {
public:
    virtual ~ISystemdMetricsCollector() = default;

    /**
     * @brief 서비스 상태 조회
     *
     * @param serviceName 서비스 이름 (예: "mxrc-rt.service")
     * @return 서비스 상태 (active, inactive, failed 등)
     */
    virtual std::string getServiceState(const std::string& serviceName) = 0;

    /**
     * @brief CPU 사용량 조회 (나노초)
     *
     * @param serviceName 서비스 이름
     * @return CPU 사용량 (나노초 단위)
     */
    virtual uint64_t getCpuUsageNanoseconds(const std::string& serviceName) = 0;

    /**
     * @brief 메모리 사용량 조회 (바이트)
     *
     * @param serviceName 서비스 이름
     * @return 메모리 사용량 (바이트 단위)
     */
    virtual uint64_t getMemoryUsageBytes(const std::string& serviceName) = 0;

    /**
     * @brief 재시작 횟수 조회
     *
     * @param serviceName 서비스 이름
     * @return 재시작 횟수
     */
    virtual uint32_t getRestartCount(const std::string& serviceName) = 0;

    /**
     * @brief 모든 메트릭 한 번에 조회
     *
     * @param serviceName 서비스 이름
     * @return 메트릭 맵 (key: 메트릭 이름, value: 값)
     */
    virtual std::map<std::string, std::string> getAllMetrics(const std::string& serviceName) = 0;

    /**
     * @brief 여러 서비스의 메트릭 조회
     *
     * @param serviceNames 서비스 이름 목록
     * @return 서비스별 메트릭 맵
     */
    virtual std::map<std::string, std::map<std::string, std::string>>
    collectMetrics(const std::vector<std::string>& serviceNames) = 0;
};

} // namespace systemd
} // namespace mxrc

#endif // MXRC_SYSTEMD_ISYSTEMD_METRICS_COLLECTOR_HPP
