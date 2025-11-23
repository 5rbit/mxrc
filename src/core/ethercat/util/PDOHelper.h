#pragma once

#include "../dto/SensorData.h"
#include "../dto/MotorCommand.h"
#include "../dto/PDOMapping.h"
#include <cstdint>
#include <cstring>

namespace mxrc {
namespace ethercat {

// PDO domain helper
// PDO domain (raw byte array)에서 데이터를 읽거나 쓰는 유틸리티
class PDOHelper {
public:
    // PDO domain에서 INT32 읽기
    static int32_t readInt32(const uint8_t* domain, uint32_t offset) {
        int32_t value;
        std::memcpy(&value, domain + offset, sizeof(int32_t));
        return value;
    }

    // PDO domain에서 INT16 읽기
    static int16_t readInt16(const uint8_t* domain, uint32_t offset) {
        int16_t value;
        std::memcpy(&value, domain + offset, sizeof(int16_t));
        return value;
    }

    // PDO domain에서 UINT16 읽기
    static uint16_t readUInt16(const uint8_t* domain, uint32_t offset) {
        uint16_t value;
        std::memcpy(&value, domain + offset, sizeof(uint16_t));
        return value;
    }

    // PDO domain에서 UINT8 읽기
    static uint8_t readUInt8(const uint8_t* domain, uint32_t offset) {
        return domain[offset];
    }

    // PDO domain에서 DOUBLE 읽기
    static double readDouble(const uint8_t* domain, uint32_t offset) {
        double value;
        std::memcpy(&value, domain + offset, sizeof(double));
        return value;
    }

    // PDO domain에서 FLOAT 읽기
    static float readFloat(const uint8_t* domain, uint32_t offset) {
        float value;
        std::memcpy(&value, domain + offset, sizeof(float));
        return value;
    }

    // PDO domain에 INT32 쓰기
    static void writeInt32(uint8_t* domain, uint32_t offset, int32_t value) {
        std::memcpy(domain + offset, &value, sizeof(int32_t));
    }

    // PDO domain에 INT16 쓰기
    static void writeInt16(uint8_t* domain, uint32_t offset, int16_t value) {
        std::memcpy(domain + offset, &value, sizeof(int16_t));
    }

    // PDO domain에 UINT16 쓰기
    static void writeUInt16(uint8_t* domain, uint32_t offset, uint16_t value) {
        std::memcpy(domain + offset, &value, sizeof(uint16_t));
    }

    // PDO domain에 UINT8 쓰기
    static void writeUInt8(uint8_t* domain, uint32_t offset, uint8_t value) {
        domain[offset] = value;
    }

    // PDO domain에 DOUBLE 쓰기
    static void writeDouble(uint8_t* domain, uint32_t offset, double value) {
        std::memcpy(domain + offset, &value, sizeof(double));
    }

    // PDO domain에 FLOAT 쓰기
    static void writeFloat(uint8_t* domain, uint32_t offset, float value) {
        std::memcpy(domain + offset, &value, sizeof(float));
    }

    // PDO mapping 정보를 기반으로 데이터 읽기
    static int readByMapping(const uint8_t* domain, const PDOMapping& mapping, void* out_value) {
        if (!domain || !out_value) {
            return -1;
        }

        switch (mapping.data_type) {
            case PDODataType::INT32:
                *(int32_t*)out_value = readInt32(domain, mapping.offset);
                break;
            case PDODataType::UINT32:
                *(uint32_t*)out_value = *(uint32_t*)(domain + mapping.offset);
                break;
            case PDODataType::INT16:
                *(int16_t*)out_value = *(int16_t*)(domain + mapping.offset);
                break;
            case PDODataType::UINT16:
                *(uint16_t*)out_value = readUInt16(domain, mapping.offset);
                break;
            case PDODataType::INT8:
                *(int8_t*)out_value = *(int8_t*)(domain + mapping.offset);
                break;
            case PDODataType::UINT8:
                *(uint8_t*)out_value = readUInt8(domain, mapping.offset);
                break;
            case PDODataType::FLOAT:
                *(float*)out_value = readFloat(domain, mapping.offset);
                break;
            case PDODataType::DOUBLE:
                *(double*)out_value = readDouble(domain, mapping.offset);
                break;
            default:
                return -1;
        }

        return 0;
    }
};

} // namespace ethercat
} // namespace mxrc
