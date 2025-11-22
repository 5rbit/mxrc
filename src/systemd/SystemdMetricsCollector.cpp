#include "mxrc/systemd/SystemdMetricsCollector.hpp"
#include <cstdio>
#include <stdexcept>
#include <sstream>

namespace mxrc {
namespace systemd {

std::string SystemdMetricsCollector::executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to execute command: " + command);
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    return result;
}

std::string SystemdMetricsCollector::executeSystemctl(
    const std::string& serviceName,
    const std::string& property) {

    std::string command = "systemctl show " + serviceName + " --property=" + property;
    std::string output = executeCommand(command);

    // 출력 형식: Property=Value
    size_t pos = output.find('=');
    if (pos == std::string::npos) {
        return "";
    }

    std::string value = output.substr(pos + 1);
    // Trim newline
    if (!value.empty() && value.back() == '\n') {
        value.pop_back();
    }

    return value;
}

std::string SystemdMetricsCollector::getServiceState(const std::string& serviceName) {
    return executeSystemctl(serviceName, "ActiveState");
}

uint64_t SystemdMetricsCollector::getCpuUsageNanoseconds(const std::string& serviceName) {
    std::string value = executeSystemctl(serviceName, "CPUUsageNSec");
    if (value.empty() || value == "[not set]") {
        return 0;
    }

    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        return 0;
    }
}

uint64_t SystemdMetricsCollector::getMemoryUsageBytes(const std::string& serviceName) {
    std::string value = executeSystemctl(serviceName, "MemoryCurrent");
    if (value.empty() || value == "[not set]") {
        return 0;
    }

    try {
        return std::stoull(value);
    } catch (const std::exception&) {
        return 0;
    }
}

uint32_t SystemdMetricsCollector::getRestartCount(const std::string& serviceName) {
    std::string value = executeSystemctl(serviceName, "NRestarts");
    if (value.empty() || value == "[not set]") {
        return 0;
    }

    try {
        return std::stoul(value);
    } catch (const std::exception&) {
        return 0;
    }
}

std::map<std::string, std::string> SystemdMetricsCollector::getAllMetrics(
    const std::string& serviceName) {

    std::map<std::string, std::string> metrics;

    // 한 번의 systemctl show로 모든 메트릭 조회
    std::string properties = "ActiveState,SubState,CPUUsageNSec,MemoryCurrent,NRestarts";
    std::string command = "systemctl show " + serviceName + " --property=" + properties;
    std::string output = executeCommand(command);

    // 출력 파싱
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            metrics[key] = value;
        }
    }

    return metrics;
}

std::map<std::string, std::map<std::string, std::string>>
SystemdMetricsCollector::collectMetrics(const std::vector<std::string>& serviceNames) {
    std::map<std::string, std::map<std::string, std::string>> allMetrics;

    for (const auto& serviceName : serviceNames) {
        allMetrics[serviceName] = getAllMetrics(serviceName);
    }

    return allMetrics;
}

} // namespace systemd
} // namespace mxrc
