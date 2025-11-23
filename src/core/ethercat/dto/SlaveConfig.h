#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mxrc {
namespace ethercat {

// EtherCAT slave 장치 타입
enum class DeviceType {
    SENSOR,         // 센서 (위치, 속도, 토크 등)
    MOTOR,          // 모터 드라이버
    IO_MODULE,      // I/O 모듈 (DI/DO/AI/AO)
    UNKNOWN         // 알 수 없는 타입
};

// EtherCAT slave 설정
struct SlaveConfig {
    uint16_t alias;             // Slave alias (YAML에서 설정)
    uint16_t position;          // Slave position (버스 상 위치)
    uint32_t vendor_id;         // Vendor ID (제조사 코드)
    uint32_t product_code;      // Product code (제품 코드)
    std::string device_name;    // 장치 이름 (사용자 정의)
    DeviceType device_type;     // 장치 타입

    SlaveConfig()
        : alias(0)
        , position(0)
        , vendor_id(0)
        , product_code(0)
        , device_name("")
        , device_type(DeviceType::UNKNOWN) {}
};

} // namespace ethercat
} // namespace mxrc
