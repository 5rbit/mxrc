#pragma once

#include "../dto/SlaveConfig.h"
#include "../dto/PDOMapping.h"
#include "../dto/DCConfiguration.h"
#include <string>
#include <vector>
#include <map>

namespace mxrc {
namespace ethercat {

// YAML 설정 파일 파서
// config/ethercat/slaves.yaml 파일을 읽어서 EtherCAT 설정을 로드
class YAMLConfigParser {
public:
    YAMLConfigParser();
    ~YAMLConfigParser() = default;

    // YAML 파일에서 설정 로드
    // 반환: 성공 0, 실패 -1
    int loadFromFile(const std::string& file_path);

    // Slave 설정 조회
    const SlaveConfig* getSlaveConfig(size_t index) const;

    // PDO 매핑 조회
    const std::vector<PDOMapping>& getPDOMappings(size_t slave_index) const;

    // DC 설정 조회
    int getDCConfig(DCConfiguration& out_config) const;

    // Master 설정 조회
    int getMasterIndex() const { return master_index_; }
    uint32_t getCycleTimeNs() const { return cycle_time_ns_; }
    int getPriority() const { return priority_; }
    int getCPUAffinity() const { return cpu_affinity_; }

    // Slave 개수 조회
    size_t getSlaveCount() const { return slave_configs_.size(); }

private:
    // YAML 파싱 내부 함수들
    int parseMasterConfig(const void* yaml_node);
    int parseSlaves(const void* yaml_node);
    int parseDCConfig(const void* yaml_node);
    DeviceType parseDeviceType(const std::string& type_str);
    PDODirection parsePDODirection(const std::string& dir_str);
    PDODataType parsePDODataType(const std::string& type_str);

    // 설정 데이터
    std::vector<SlaveConfig> slave_configs_;
    std::map<size_t, std::vector<PDOMapping>> pdo_mappings_;
    DCConfiguration dc_config_;

    // Master 설정
    int master_index_;
    uint32_t cycle_time_ns_;
    int priority_;
    int cpu_affinity_;
};

} // namespace ethercat
} // namespace mxrc
