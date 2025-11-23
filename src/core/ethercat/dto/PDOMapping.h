#pragma once

#include <cstdint>
#include <string>

namespace mxrc {
namespace ethercat {

// PDO 방향
enum class PDODirection {
    INPUT,      // Slave → Master (센서 데이터 읽기)
    OUTPUT      // Master → Slave (모터 명령 쓰기)
};

// PDO 데이터 타입
enum class PDODataType {
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    FLOAT,
    DOUBLE
};

// PDO 매핑 정보
struct PDOMapping {
    PDODirection direction;     // PDO 방향 (INPUT/OUTPUT)
    uint16_t index;             // PDO index (0x1600, 0x1A00 등)
    uint8_t subindex;           // PDO subindex
    uint8_t bit_length;         // 비트 길이 (8, 16, 32, 64)
    PDODataType data_type;      // 데이터 타입
    uint32_t offset;            // PDO domain 내 바이트 offset
    std::string description;    // 설명 (YAML에서 로드)

    PDOMapping()
        : direction(PDODirection::INPUT)
        , index(0)
        , subindex(0)
        , bit_length(0)
        , data_type(PDODataType::UINT8)
        , offset(0)
        , description("") {}
};

} // namespace ethercat
} // namespace mxrc
