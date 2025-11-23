#pragma once

#include "../dto/SlaveConfig.h"
#include "../dto/PDOMapping.h"
#include <vector>

namespace mxrc {
namespace ethercat {

// Slave 설정 관리 인터페이스
// YAML 파일에서 로드한 설정에 접근
class ISlaveConfig {
public:
    virtual ~ISlaveConfig() = default;

    // Slave 설정 조회
    virtual const SlaveConfig* getSlaveConfig(uint16_t slave_id) const = 0;

    // PDO 매핑 정보 조회
    virtual const std::vector<PDOMapping>& getPDOMappings(uint16_t slave_id) const = 0;

    // 모든 slave 개수 조회
    virtual size_t getSlaveCount() const = 0;
};

} // namespace ethercat
} // namespace mxrc
