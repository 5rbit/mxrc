#include "SensorDataManager.h"
#include "../util/PDOHelper.h"
#include <spdlog/spdlog.h>
#include <chrono>

namespace mxrc {
namespace ethercat {

SensorDataManager::SensorDataManager(
    std::shared_ptr<IEtherCATMaster> master,
    std::shared_ptr<ISlaveConfig> config)
    : master_(master)
    , config_(config)
    , domain_ptr_(nullptr) {
}

int SensorDataManager::readPositionSensor(uint16_t slave_id, PositionSensorData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        spdlog::error("Slave {} PDO 매핑 없음", slave_id);
        return -1;
    }

    // 위치 센서: 0x1A00:01 (position), 0x1A00:02 (velocity)
    uint32_t pos_offset = 0;
    uint32_t vel_offset = 0;
    bool found_pos = false;
    bool found_vel = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::INPUT) {
            if (mapping.index == 0x1A00 && mapping.subindex == 0x01) {
                pos_offset = mapping.offset;
                found_pos = true;
            } else if (mapping.index == 0x1A00 && mapping.subindex == 0x02) {
                vel_offset = mapping.offset;
                found_vel = true;
            }
        }
    }

    if (!found_pos) {
        spdlog::warn("Slave {} position PDO 매핑 없음", slave_id);
        return -1;
    }

    // PDO domain에서 INT32 데이터 읽기
    data.position = PDOHelper::readInt32(domain_ptr_, pos_offset);

    if (found_vel) {
        data.velocity = PDOHelper::readInt32(domain_ptr_, vel_offset);
    } else {
        data.velocity = 0;
    }

    // 메타데이터 설정
    data.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    data.valid = master_->isActive();
    data.slave_id = slave_id;

    return 0;
}

int SensorDataManager::readVelocitySensor(uint16_t slave_id, VelocitySensorData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        return -1;
    }

    // 속도 센서: 0x1A01:01 (velocity), 0x1A01:02 (acceleration)
    // DOUBLE 타입으로 저장됨
    uint32_t vel_offset = 0;
    uint32_t acc_offset = 0;
    bool found_vel = false;
    bool found_acc = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::INPUT) {
            if (mapping.index == 0x1A01 && mapping.subindex == 0x01) {
                vel_offset = mapping.offset;
                found_vel = true;
            } else if (mapping.index == 0x1A01 && mapping.subindex == 0x02) {
                acc_offset = mapping.offset;
                found_acc = true;
            }
        }
    }

    if (!found_vel) {
        return -1;
    }

    // DOUBLE 타입으로 읽기
    data.velocity = PDOHelper::readDouble(domain_ptr_, vel_offset);

    if (found_acc) {
        data.acceleration = PDOHelper::readDouble(domain_ptr_, acc_offset);
    } else {
        data.acceleration = 0.0;
    }

    data.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    data.valid = master_->isActive();
    data.slave_id = slave_id;

    return 0;
}

int SensorDataManager::readTorqueSensor(uint16_t slave_id, TorqueSensorData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        return -1;
    }

    // 토크 센서 6축: 0x1A02:01~06
    // 01: force_x, 02: force_y, 03: force_z
    // 04: torque_x, 05: torque_y, 06: torque_z
    bool found_any = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::INPUT && mapping.index == 0x1A02) {
            found_any = true;
            double value = PDOHelper::readDouble(domain_ptr_, mapping.offset);

            switch (mapping.subindex) {
                case 0x01: data.force_x = value; break;
                case 0x02: data.force_y = value; break;
                case 0x03: data.force_z = value; break;
                case 0x04: data.torque_x = value; break;
                case 0x05: data.torque_y = value; break;
                case 0x06: data.torque_z = value; break;
            }
        }
    }

    if (!found_any) {
        return -1;
    }

    data.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    data.valid = master_->isActive();
    data.slave_id = slave_id;

    return 0;
}

int SensorDataManager::readDigitalInput(uint16_t slave_id, uint8_t channel, DigitalInputData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        return -1;
    }

    // Digital Input: 0x1A03:01 (모든 채널이 비트맵으로 인코딩됨)
    // 예: UINT8 또는 UINT16에서 각 비트가 채널
    uint32_t di_offset = 0;
    PDODataType data_type = PDODataType::UINT8;
    bool found = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::INPUT &&
            mapping.index == 0x1A03 && mapping.subindex == 0x01) {
            di_offset = mapping.offset;
            data_type = mapping.data_type;
            found = true;
            break;
        }
    }

    if (!found) {
        return -1;
    }

    // 비트맵 읽기
    uint16_t bitmap = 0;
    if (data_type == PDODataType::UINT8) {
        bitmap = PDOHelper::readUInt8(domain_ptr_, di_offset);
    } else if (data_type == PDODataType::UINT16) {
        bitmap = PDOHelper::readUInt16(domain_ptr_, di_offset);
    } else {
        spdlog::error("지원하지 않는 DI 데이터 타입");
        return -1;
    }

    // 해당 채널의 비트 추출
    data.channel = channel;
    data.value = (bitmap & (1 << channel)) != 0;
    data.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    data.valid = master_->isActive();
    data.slave_id = slave_id;

    return 0;
}

int SensorDataManager::readAnalogInput(uint16_t slave_id, uint8_t channel, AnalogInputData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        return -1;
    }

    // Analog Input: 0x1A04:01~04 (채널별 개별 매핑)
    // subindex = 0x01 + channel
    uint8_t target_subindex = 0x01 + channel;
    uint32_t ai_offset = 0;
    PDODataType data_type = PDODataType::INT16;
    bool found = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::INPUT &&
            mapping.index == 0x1A04 && mapping.subindex == target_subindex) {
            ai_offset = mapping.offset;
            data_type = mapping.data_type;
            found = true;
            break;
        }
    }

    if (!found) {
        return -1;
    }

    // 데이터 타입에 따라 읽고 double로 변환
    data.channel = channel;

    if (data_type == PDODataType::INT16) {
        data.value = static_cast<double>(PDOHelper::readInt16(domain_ptr_, ai_offset));
    } else if (data_type == PDODataType::INT32) {
        data.value = static_cast<double>(PDOHelper::readInt32(domain_ptr_, ai_offset));
    } else if (data_type == PDODataType::FLOAT) {
        data.value = static_cast<double>(PDOHelper::readFloat(domain_ptr_, ai_offset));
    } else if (data_type == PDODataType::DOUBLE) {
        data.value = PDOHelper::readDouble(domain_ptr_, ai_offset);
    } else {
        spdlog::error("지원하지 않는 AI 데이터 타입");
        return -1;
    }

    data.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    data.valid = master_->isActive();
    data.slave_id = slave_id;

    return 0;
}

int SensorDataManager::writeDigitalOutput(uint16_t slave_id, uint8_t channel,
                                           const DigitalOutputData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        return -1;
    }

    // Digital Output: 0x1600:01 (8bit or 16bit bitmap)
    uint32_t do_offset = 0;
    PDODataType data_type = PDODataType::UINT8;
    bool found = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::OUTPUT &&
            mapping.index == 0x1600 && mapping.subindex == 0x01) {
            do_offset = mapping.offset;
            data_type = mapping.data_type;
            found = true;
            break;
        }
    }

    if (!found) {
        return -1;
    }

    // 기존 bitmap 읽기
    uint16_t bitmap = 0;
    if (data_type == PDODataType::UINT8) {
        bitmap = PDOHelper::readUInt8(domain_ptr_, do_offset);
    } else if (data_type == PDODataType::UINT16) {
        bitmap = PDOHelper::readUInt16(domain_ptr_, do_offset);
    } else {
        spdlog::error("지원하지 않는 DO 데이터 타입");
        return -1;
    }

    // 해당 채널의 비트 설정/해제
    if (data.value) {
        bitmap |= (1 << channel);  // 비트 SET
    } else {
        bitmap &= ~(1 << channel); // 비트 CLEAR
    }

    // 수정된 bitmap 쓰기
    if (data_type == PDODataType::UINT8) {
        PDOHelper::writeUInt8(domain_ptr_, do_offset, static_cast<uint8_t>(bitmap));
    } else if (data_type == PDODataType::UINT16) {
        PDOHelper::writeUInt16(domain_ptr_, do_offset, bitmap);
    }

    spdlog::debug("Digital Output 쓰기: slave_id={}, channel={}, value={}",
                 slave_id, channel, data.value);

    return 0;
}

int SensorDataManager::writeAnalogOutput(uint16_t slave_id, uint8_t channel,
                                          const AnalogOutputData& data) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(slave_id);
    if (mappings.empty()) {
        return -1;
    }

    // Analog Output: 0x1601:01~04 (채널별 개별 매핑)
    // subindex = 0x01 + channel
    uint8_t target_subindex = 0x01 + channel;
    uint32_t ao_offset = 0;
    PDODataType pdo_data_type = PDODataType::INT16;
    bool found = false;

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::OUTPUT &&
            mapping.index == 0x1601 && mapping.subindex == target_subindex) {
            ao_offset = mapping.offset;
            pdo_data_type = mapping.data_type;
            found = true;
            break;
        }
    }

    if (!found) {
        return -1;
    }

    // 범위 체크
    if (!data.isInRange()) {
        spdlog::warn("Analog Output 범위 초과: value={}, range=[{}, {}]",
                    data.value, data.min_value, data.max_value);
        return -1;
    }

    // 데이터 타입에 따라 쓰기
    if (pdo_data_type == PDODataType::INT16) {
        PDOHelper::writeInt16(domain_ptr_, ao_offset, static_cast<int16_t>(data.value));
    } else if (pdo_data_type == PDODataType::INT32) {
        PDOHelper::writeInt32(domain_ptr_, ao_offset, static_cast<int32_t>(data.value));
    } else if (pdo_data_type == PDODataType::FLOAT) {
        PDOHelper::writeFloat(domain_ptr_, ao_offset, static_cast<float>(data.value));
    } else if (pdo_data_type == PDODataType::DOUBLE) {
        PDOHelper::writeDouble(domain_ptr_, ao_offset, data.value);
    } else {
        spdlog::error("지원하지 않는 AO 데이터 타입");
        return -1;
    }

    spdlog::debug("Analog Output 쓰기: slave_id={}, channel={}, value={}",
                 slave_id, channel, data.value);

    return 0;
}

int SensorDataManager::findPDOOffset(uint16_t slave_id, uint16_t index,
                                      uint8_t subindex, uint32_t& out_offset) {
    const auto& mappings = config_->getPDOMappings(slave_id);

    for (const auto& mapping : mappings) {
        if (mapping.index == index && mapping.subindex == subindex) {
            out_offset = mapping.offset;
            return 0;
        }
    }

    return -1;
}

} // namespace ethercat
} // namespace mxrc
