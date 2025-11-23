#include "MotorCommandManager.h"
#include "../util/PDOHelper.h"
#include <spdlog/spdlog.h>

namespace mxrc {
namespace ethercat {

MotorCommandManager::MotorCommandManager(
    std::shared_ptr<IEtherCATMaster> master,
    std::shared_ptr<ISlaveConfig> config)
    : master_(master)
    , config_(config)
    , domain_ptr_(nullptr) {
}

int MotorCommandManager::writeBLDCCommand(const BLDCMotorCommand& command) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    // 검증: 안전성 체크
    if (!command.isValid()) {
        spdlog::error("유효하지 않은 BLDC 명령: slave_id={}, mode={}, vel={}, torque={}",
                     command.slave_id, static_cast<int>(command.control_mode),
                     command.target_velocity, command.target_torque);
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(command.slave_id);
    if (mappings.empty()) {
        spdlog::error("Slave {} PDO 매핑 없음", command.slave_id);
        return -1;
    }

    // 1. Control Word 작성 (0x1602:01)
    uint16_t control_word = 0;
    if (command.enable && command.control_mode != ControlMode::DISABLED) {
        // Bit 0: Enable
        control_word |= 0x0001;

        // Bit 1-2: Control Mode
        if (command.control_mode == ControlMode::VELOCITY) {
            control_word |= (0x02 << 1);  // 0b010
        } else if (command.control_mode == ControlMode::TORQUE) {
            control_word |= (0x03 << 1);  // 0b011
        }
    }

    int result = writeControlWord(command.slave_id, control_word);
    if (result != 0) {
        spdlog::error("Control Word 쓰기 실패: slave_id={}", command.slave_id);
        return -1;
    }

    // 2. Target Velocity 작성 (0x1602:02, VELOCITY 모드일 때)
    if (command.enable && command.control_mode == ControlMode::VELOCITY) {
        uint32_t offset = 0;
        PDODataType data_type;

        if (findPDOOffset(command.slave_id, 0x1602, 0x02, offset, data_type) == 0) {
            if (data_type == PDODataType::INT32) {
                PDOHelper::writeInt32(domain_ptr_, offset, static_cast<int32_t>(command.target_velocity));
            } else {
                spdlog::error("지원하지 않는 BLDC velocity 데이터 타입");
                return -1;
            }
        }
    }

    // 3. Target Torque 작성 (0x1602:03, TORQUE 모드일 때)
    if (command.enable && command.control_mode == ControlMode::TORQUE) {
        uint32_t offset = 0;
        PDODataType data_type;

        if (findPDOOffset(command.slave_id, 0x1602, 0x03, offset, data_type) == 0) {
            if (data_type == PDODataType::INT16) {
                PDOHelper::writeInt16(domain_ptr_, offset, static_cast<int16_t>(command.target_torque));
            } else if (data_type == PDODataType::DOUBLE) {
                PDOHelper::writeDouble(domain_ptr_, offset, command.target_torque);
            } else {
                spdlog::error("지원하지 않는 BLDC torque 데이터 타입");
                return -1;
            }
        }
    }

    spdlog::debug("BLDC 명령 전송: slave_id={}, mode={}, vel={}, torque={}, enable={}",
                 command.slave_id, static_cast<int>(command.control_mode),
                 command.target_velocity, command.target_torque, command.enable);

    return 0;
}

int MotorCommandManager::writeServoCommand(const ServoDriverCommand& command) {
    if (!domain_ptr_) {
        spdlog::error("PDO domain 포인터가 설정되지 않음");
        return -1;
    }

    // 검증: 안전성 체크
    if (!command.isValid()) {
        spdlog::error("유효하지 않은 Servo 명령: slave_id={}, mode={}, pos={}, vel={}, torque={}",
                     command.slave_id, static_cast<int>(command.control_mode),
                     command.target_position, command.target_velocity, command.target_torque);
        return -1;
    }

    const auto& mappings = config_->getPDOMappings(command.slave_id);
    if (mappings.empty()) {
        spdlog::error("Slave {} PDO 매핑 없음", command.slave_id);
        return -1;
    }

    // 1. Control Word 작성 (0x1603:01)
    uint16_t control_word = 0;
    if (command.enable && command.control_mode != ControlMode::DISABLED) {
        // Bit 0: Enable
        control_word |= 0x0001;

        // Bit 1-3: Control Mode
        if (command.control_mode == ControlMode::POSITION) {
            control_word |= (0x01 << 1);  // 0b001
        } else if (command.control_mode == ControlMode::VELOCITY) {
            control_word |= (0x02 << 1);  // 0b010
        } else if (command.control_mode == ControlMode::TORQUE) {
            control_word |= (0x03 << 1);  // 0b011
        }
    }

    int result = writeControlWord(command.slave_id, control_word);
    if (result != 0) {
        spdlog::error("Control Word 쓰기 실패: slave_id={}", command.slave_id);
        return -1;
    }

    // 2. Target Position 작성 (0x1603:02, POSITION 모드일 때)
    if (command.enable && command.control_mode == ControlMode::POSITION) {
        uint32_t pos_offset = 0, max_vel_offset = 0;
        PDODataType pos_type, max_vel_type;

        if (findPDOOffset(command.slave_id, 0x1603, 0x02, pos_offset, pos_type) == 0) {
            if (pos_type == PDODataType::DOUBLE) {
                PDOHelper::writeDouble(domain_ptr_, pos_offset, command.target_position);
            } else {
                spdlog::error("지원하지 않는 Servo position 데이터 타입");
                return -1;
            }
        }

        // Max Velocity도 함께 작성 (0x1603:03)
        if (findPDOOffset(command.slave_id, 0x1603, 0x03, max_vel_offset, max_vel_type) == 0) {
            if (max_vel_type == PDODataType::DOUBLE) {
                PDOHelper::writeDouble(domain_ptr_, max_vel_offset, command.max_velocity);
            }
        }
    }

    // 3. Target Velocity 작성 (0x1603:04, VELOCITY 모드일 때)
    if (command.enable && command.control_mode == ControlMode::VELOCITY) {
        uint32_t offset = 0;
        PDODataType data_type;

        if (findPDOOffset(command.slave_id, 0x1603, 0x04, offset, data_type) == 0) {
            if (data_type == PDODataType::DOUBLE) {
                PDOHelper::writeDouble(domain_ptr_, offset, command.target_velocity);
            } else {
                spdlog::error("지원하지 않는 Servo velocity 데이터 타입");
                return -1;
            }
        }
    }

    // 4. Target Torque 작성 (0x1603:05, TORQUE 모드일 때)
    if (command.enable && command.control_mode == ControlMode::TORQUE) {
        uint32_t offset = 0;
        PDODataType data_type;

        if (findPDOOffset(command.slave_id, 0x1603, 0x05, offset, data_type) == 0) {
            if (data_type == PDODataType::DOUBLE) {
                PDOHelper::writeDouble(domain_ptr_, offset, command.target_torque);
            } else {
                spdlog::error("지원하지 않는 Servo torque 데이터 타입");
                return -1;
            }
        }
    }

    spdlog::debug("Servo 명령 전송: slave_id={}, mode={}, pos={}, vel={}, torque={}, enable={}",
                 command.slave_id, static_cast<int>(command.control_mode),
                 command.target_position, command.target_velocity,
                 command.target_torque, command.enable);

    return 0;
}

int MotorCommandManager::writeControlWord(uint16_t slave_id, uint16_t control_word) {
    uint32_t offset = 0;
    PDODataType data_type;

    // BLDC: 0x1602:01, Servo: 0x1603:01
    // 먼저 BLDC 시도
    if (findPDOOffset(slave_id, 0x1602, 0x01, offset, data_type) == 0) {
        if (data_type == PDODataType::UINT16) {
            PDOHelper::writeUInt16(domain_ptr_, offset, control_word);
            return 0;
        }
    }

    // Servo 시도
    if (findPDOOffset(slave_id, 0x1603, 0x01, offset, data_type) == 0) {
        if (data_type == PDODataType::UINT16) {
            PDOHelper::writeUInt16(domain_ptr_, offset, control_word);
            return 0;
        }
    }

    spdlog::error("Control Word PDO 매핑을 찾을 수 없음: slave_id={}", slave_id);
    return -1;
}

int MotorCommandManager::findPDOOffset(uint16_t slave_id, uint16_t index,
                                        uint8_t subindex, uint32_t& out_offset,
                                        PDODataType& out_type) {
    const auto& mappings = config_->getPDOMappings(slave_id);

    for (const auto& mapping : mappings) {
        if (mapping.direction == PDODirection::OUTPUT &&
            mapping.index == index && mapping.subindex == subindex) {
            out_offset = mapping.offset;
            out_type = mapping.data_type;
            return 0;
        }
    }

    return -1;
}

} // namespace ethercat
} // namespace mxrc
