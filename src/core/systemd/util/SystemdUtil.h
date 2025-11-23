#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <map>

namespace mxrc {
namespace systemd {
namespace util {

/**
 * @brief systemd 유틸리티 함수 모음
 *
 * systemd 서비스 속성 조회, 가용성 확인 등의 공통 기능 제공.
 * systemctl 명령어를 래핑하거나 libsystemd API를 직접 호출.
 *
 * @example
 * if (SystemdUtil::checkSystemdAvailable()) {
 *     auto state = SystemdUtil::getServiceProperty("mxrc-rt.service", "ActiveState");
 *     if (state == "active") {
 *         // 서비스 실행 중
 *     }
 * }
 */
class SystemdUtil {
public:
    /**
     * @brief systemd가 시스템에서 사용 가능한지 확인
     *
     * /run/systemd/system 디렉토리 존재 여부로 확인.
     *
     * @return systemd가 사용 가능하면 true
     */
    static bool checkSystemdAvailable();

    /**
     * @brief 서비스 속성 값 조회
     *
     * `systemctl show <service> --property=<property>` 명령어 실행하여 파싱.
     *
     * @param serviceName 서비스 이름 (예: "mxrc-rt.service")
     * @param property 속성 이름 (예: "ActiveState", "CPUUsageNSec")
     * @return 속성 값 (실패 시 std::nullopt)
     */
    static std::optional<std::string> getServiceProperty(
        const std::string& serviceName,
        const std::string& property);

    /**
     * @brief 여러 서비스 속성을 한 번에 조회
     *
     * 성능 최적화를 위해 단일 systemctl show 호출로 여러 속성 조회.
     *
     * @param serviceName 서비스 이름
     * @param properties 조회할 속성 목록
     * @return 속성-값 맵 (실패한 속성은 제외됨)
     */
    static std::map<std::string, std::string> getServiceProperties(
        const std::string& serviceName,
        const std::vector<std::string>& properties);

    /**
     * @brief 서비스가 활성 상태인지 확인
     *
     * ActiveState=active 여부를 체크.
     *
     * @param serviceName 서비스 이름
     * @return 활성 상태이면 true
     */
    static bool isServiceActive(const std::string& serviceName);

    /**
     * @brief 서비스 재시작
     *
     * `systemctl restart <service>` 명령어 실행.
     *
     * @param serviceName 서비스 이름
     * @return 성공 시 true
     */
    static bool restartService(const std::string& serviceName);

    /**
     * @brief 프로세스가 systemd 서비스로 실행 중인지 확인
     *
     * NOTIFY_SOCKET 환경변수 존재 여부로 확인.
     *
     * @return systemd 서비스로 실행 중이면 true
     */
    static bool isRunningAsService();

private:
    /**
     * @brief 명령어 실행 및 출력 캡처
     *
     * @param command 실행할 명령어
     * @param output 명령어 출력 (성공 시)
     * @return 성공 시 true
     */
    static bool executeCommand(const std::string& command, std::string& output);

    /**
     * @brief systemctl show 출력 파싱
     *
     * "Property=Value" 형식의 줄을 파싱하여 맵으로 변환.
     *
     * @param output systemctl show 출력
     * @return 속성-값 맵
     */
    static std::map<std::string, std::string> parseSystemctlShow(const std::string& output);
};

} // namespace util
} // namespace systemd
} // namespace mxrc
