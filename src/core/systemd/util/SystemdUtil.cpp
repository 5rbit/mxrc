#include "SystemdUtil.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <iostream>
#include <map>

namespace mxrc {
namespace systemd {
namespace util {

bool SystemdUtil::checkSystemdAvailable() {
    // systemd가 실행 중인지 확인: /run/systemd/system 디렉토리 존재 여부
    std::ifstream file("/run/systemd/system");
    return file.good();
}

bool SystemdUtil::executeCommand(const std::string& command, std::string& output) {
    std::array<char, 128> buffer;
    output.clear();

    // popen으로 명령어 실행 및 출력 캡처
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        return false;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    return true;
}

std::map<std::string, std::string> SystemdUtil::parseSystemctlShow(const std::string& output) {
    std::map<std::string, std::string> properties;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            properties[key] = value;
        }
    }

    return properties;
}

std::optional<std::string> SystemdUtil::getServiceProperty(
    const std::string& serviceName,
    const std::string& property) {

    if (!checkSystemdAvailable()) {
        return std::nullopt;
    }

    std::string command = "systemctl show " + serviceName + " --property=" + property;
    std::string output;

    if (!executeCommand(command, output)) {
        return std::nullopt;
    }

    auto properties = parseSystemctlShow(output);
    auto it = properties.find(property);

    if (it != properties.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::map<std::string, std::string> SystemdUtil::getServiceProperties(
    const std::string& serviceName,
    const std::vector<std::string>& properties) {

    std::map<std::string, std::string> result;

    if (!checkSystemdAvailable() || properties.empty()) {
        return result;
    }

    // 모든 속성을 한 번에 조회
    std::string propertyList;
    for (const auto& prop : properties) {
        if (!propertyList.empty()) {
            propertyList += ",";
        }
        propertyList += prop;
    }

    std::string command = "systemctl show " + serviceName + " --property=" + propertyList;
    std::string output;

    if (!executeCommand(command, output)) {
        return result;
    }

    return parseSystemctlShow(output);
}

bool SystemdUtil::isServiceActive(const std::string& serviceName) {
    auto state = getServiceProperty(serviceName, "ActiveState");
    return state.has_value() && state.value() == "active";
}

bool SystemdUtil::restartService(const std::string& serviceName) {
    if (!checkSystemdAvailable()) {
        return false;
    }

    std::string command = "systemctl restart " + serviceName;
    std::string output;

    return executeCommand(command, output);
}

bool SystemdUtil::isRunningAsService() {
    // NOTIFY_SOCKET 환경변수가 설정되어 있으면 systemd 서비스로 실행 중
    const char* notifySocket = std::getenv("NOTIFY_SOCKET");
    return notifySocket != nullptr;
}

} // namespace util
} // namespace systemd
} // namespace mxrc
