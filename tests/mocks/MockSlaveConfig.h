#pragma once

#include "core/ethercat/interfaces/ISlaveConfig.h"
#include <map>
#include <vector>

namespace mxrc {
namespace ethercat {
namespace test {

// 테스트용 가상 Slave 설정
class MockSlaveConfig : public ISlaveConfig {
public:
    MockSlaveConfig() = default;
    ~MockSlaveConfig() override = default;

    // ISlaveConfig 인터페이스 구현
    const SlaveConfig* getSlaveConfig(uint16_t slave_id) const override {
        auto it = slave_configs_.find(slave_id);
        if (it == slave_configs_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    const std::vector<PDOMapping>& getPDOMappings(uint16_t slave_id) const override {
        static const std::vector<PDOMapping> empty;
        auto it = pdo_mappings_.find(slave_id);
        if (it == pdo_mappings_.end()) {
            return empty;
        }
        return it->second;
    }

    size_t getSlaveCount() const override {
        return slave_configs_.size();
    }

    // 테스트 헬퍼: Slave 설정 추가
    void addSlaveConfig(uint16_t slave_id, const SlaveConfig& config) {
        slave_configs_[slave_id] = config;
    }

    // 테스트 헬퍼: PDO 매핑 추가
    void addPDOMapping(uint16_t slave_id, const PDOMapping& mapping) {
        pdo_mappings_[slave_id].push_back(mapping);
    }

    // 테스트 헬퍼: 전체 PDO 매핑 설정
    void setPDOMappings(uint16_t slave_id, const std::vector<PDOMapping>& mappings) {
        pdo_mappings_[slave_id] = mappings;
    }

    // 테스트 헬퍼: 모든 설정 초기화
    void clear() {
        slave_configs_.clear();
        pdo_mappings_.clear();
    }

private:
    std::map<uint16_t, SlaveConfig> slave_configs_;
    std::map<uint16_t, std::vector<PDOMapping>> pdo_mappings_;
};

} // namespace test
} // namespace ethercat
} // namespace mxrc
