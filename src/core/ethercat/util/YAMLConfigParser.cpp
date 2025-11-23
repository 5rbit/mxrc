#include "YAMLConfigParser.h"
#include <spdlog/spdlog.h>

#ifdef __has_include
#if __has_include(<yaml-cpp/yaml.h>)
#define HAVE_YAML_CPP 1
#include <yaml-cpp/yaml.h>
#endif
#endif

namespace mxrc {
namespace ethercat {

YAMLConfigParser::YAMLConfigParser()
    : master_index_(0)
    , cycle_time_ns_(10000000)  // 기본 10ms
    , priority_(99)
    , cpu_affinity_(1) {
}

#ifdef HAVE_YAML_CPP

int YAMLConfigParser::loadFromFile(const std::string& file_path) {
    try {
        YAML::Node config = YAML::LoadFile(file_path);

        // Master 설정 파싱
        if (config["master"]) {
            YAML::Node master_node = config["master"];
            parseMasterConfig(&master_node);
        }

        // Slaves 설정 파싱
        if (config["slaves"]) {
            YAML::Node slaves_node = config["slaves"];
            parseSlaves(&slaves_node);
        }

        // DC 설정 파싱 (선택적)
        if (config["dc_config"]) {
            YAML::Node dc_node = config["dc_config"];
            parseDCConfig(&dc_node);
        }

        spdlog::info("EtherCAT YAML 설정 로드 완료: {} slaves", slave_configs_.size());
        return 0;

    } catch (const YAML::Exception& e) {
        spdlog::error("YAML 파싱 실패: {}", e.what());
        return -1;
    } catch (const std::exception& e) {
        spdlog::error("YAML 로드 실패: {}", e.what());
        return -1;
    }
}

int YAMLConfigParser::parseMasterConfig(const void* yaml_node) {
    const YAML::Node* node = static_cast<const YAML::Node*>(yaml_node);

    if ((*node)["index"]) {
        master_index_ = (*node)["index"].as<int>();
    }
    if ((*node)["cycle_time_ns"]) {
        cycle_time_ns_ = (*node)["cycle_time_ns"].as<uint32_t>();
    }
    if ((*node)["priority"]) {
        priority_ = (*node)["priority"].as<int>();
    }
    if ((*node)["cpu_affinity"]) {
        cpu_affinity_ = (*node)["cpu_affinity"].as<int>();
    }

    return 0;
}

int YAMLConfigParser::parseSlaves(const void* yaml_node) {
    const YAML::Node* slaves_node = static_cast<const YAML::Node*>(yaml_node);

    for (size_t i = 0; i < slaves_node->size(); ++i) {
        const YAML::Node& slave = (*slaves_node)[i];

        SlaveConfig config;
        config.alias = slave["alias"].as<uint16_t>();
        config.position = slave["position"].as<uint16_t>();

        // Hex 값 파싱
        std::string vendor_hex = slave["vendor_id"].as<std::string>();
        std::string product_hex = slave["product_code"].as<std::string>();
        config.vendor_id = std::stoul(vendor_hex, nullptr, 16);
        config.product_code = std::stoul(product_hex, nullptr, 16);

        config.device_name = slave["device_name"].as<std::string>();
        config.device_type = parseDeviceType(slave["device_type"].as<std::string>());

        slave_configs_.push_back(config);

        // PDO mappings 파싱
        if (slave["pdo_mappings"]) {
            std::vector<PDOMapping> mappings;
            const YAML::Node& pdo_list = slave["pdo_mappings"];

            for (size_t j = 0; j < pdo_list.size(); ++j) {
                const YAML::Node& pdo = pdo_list[j];

                PDOMapping mapping;
                mapping.direction = parsePDODirection(pdo["direction"].as<std::string>());

                std::string index_hex = pdo["index"].as<std::string>();
                mapping.index = std::stoul(index_hex, nullptr, 16);

                std::string subindex_hex = pdo["subindex"].as<std::string>();
                mapping.subindex = std::stoul(subindex_hex, nullptr, 16);

                mapping.data_type = parsePDODataType(pdo["data_type"].as<std::string>());
                mapping.offset = pdo["offset"].as<uint32_t>();
                mapping.description = pdo["description"].as<std::string>();

                // bit_length 계산
                switch (mapping.data_type) {
                    case PDODataType::INT8:
                    case PDODataType::UINT8:
                        mapping.bit_length = 8;
                        break;
                    case PDODataType::INT16:
                    case PDODataType::UINT16:
                        mapping.bit_length = 16;
                        break;
                    case PDODataType::INT32:
                    case PDODataType::UINT32:
                    case PDODataType::FLOAT:
                        mapping.bit_length = 32;
                        break;
                    case PDODataType::DOUBLE:
                        mapping.bit_length = 64;
                        break;
                }

                mappings.push_back(mapping);
            }

            pdo_mappings_[i] = mappings;
        }
    }

    return 0;
}

int YAMLConfigParser::parseDCConfig(const void* yaml_node) {
    const YAML::Node* node = static_cast<const YAML::Node*>(yaml_node);

    dc_config_.enable = (*node)["enable"].as<bool>();
    dc_config_.reference_slave = (*node)["reference_slave"].as<uint16_t>();
    dc_config_.sync0_cycle_time = (*node)["sync0_cycle_time"].as<uint32_t>();
    dc_config_.sync0_shift_time = (*node)["sync0_shift_time"].as<int32_t>();

    if ((*node)["sync1_cycle_time"]) {
        dc_config_.sync1_cycle_time = (*node)["sync1_cycle_time"].as<uint32_t>();
    }

    return 0;
}

DeviceType YAMLConfigParser::parseDeviceType(const std::string& type_str) {
    if (type_str == "SENSOR") return DeviceType::SENSOR;
    if (type_str == "MOTOR") return DeviceType::MOTOR;
    if (type_str == "IO_MODULE") return DeviceType::IO_MODULE;
    return DeviceType::UNKNOWN;
}

PDODirection YAMLConfigParser::parsePDODirection(const std::string& dir_str) {
    if (dir_str == "INPUT") return PDODirection::INPUT;
    if (dir_str == "OUTPUT") return PDODirection::OUTPUT;
    return PDODirection::INPUT;
}

PDODataType YAMLConfigParser::parsePDODataType(const std::string& type_str) {
    if (type_str == "INT8") return PDODataType::INT8;
    if (type_str == "UINT8") return PDODataType::UINT8;
    if (type_str == "INT16") return PDODataType::INT16;
    if (type_str == "UINT16") return PDODataType::UINT16;
    if (type_str == "INT32") return PDODataType::INT32;
    if (type_str == "UINT32") return PDODataType::UINT32;
    if (type_str == "FLOAT") return PDODataType::FLOAT;
    if (type_str == "DOUBLE") return PDODataType::DOUBLE;
    return PDODataType::UINT8;
}

#else  // yaml-cpp 미설치 시 stub 구현

int YAMLConfigParser::loadFromFile(const std::string& file_path) {
    spdlog::warn("yaml-cpp가 설치되지 않음. YAML 파싱 불가: {}", file_path);
    return -1;
}

int YAMLConfigParser::parseMasterConfig(const void* yaml_node) { return -1; }
int YAMLConfigParser::parseSlaves(const void* yaml_node) { return -1; }
int YAMLConfigParser::parseDCConfig(const void* yaml_node) { return -1; }
DeviceType YAMLConfigParser::parseDeviceType(const std::string& type_str) { return DeviceType::UNKNOWN; }
PDODirection YAMLConfigParser::parsePDODirection(const std::string& dir_str) { return PDODirection::INPUT; }
PDODataType YAMLConfigParser::parsePDODataType(const std::string& type_str) { return PDODataType::UINT8; }

#endif  // HAVE_YAML_CPP

const SlaveConfig* YAMLConfigParser::getSlaveConfig(size_t index) const {
    if (index >= slave_configs_.size()) {
        return nullptr;
    }
    return &slave_configs_[index];
}

const std::vector<PDOMapping>& YAMLConfigParser::getPDOMappings(size_t slave_index) const {
    static const std::vector<PDOMapping> empty;
    auto it = pdo_mappings_.find(slave_index);
    if (it == pdo_mappings_.end()) {
        return empty;
    }
    return it->second;
}

int YAMLConfigParser::getDCConfig(DCConfiguration& out_config) const {
    out_config = dc_config_;
    return 0;
}

} // namespace ethercat
} // namespace mxrc
