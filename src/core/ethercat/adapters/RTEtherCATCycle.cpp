#include "RTEtherCATCycle.h"
#include <spdlog/spdlog.h>

namespace mxrc {
namespace ethercat {

RTEtherCATCycle::RTEtherCATCycle(
    std::shared_ptr<IEtherCATMaster> master,
    std::shared_ptr<ISensorDataManager> sensor_manager,
    std::shared_ptr<IMotorCommandManager> motor_manager,
    std::shared_ptr<mxrc::core::event::IEventBus> event_bus,
    std::shared_ptr<mxrc::core::rt::RTStateMachine> state_machine)
    : master_(master)
    , sensor_manager_(sensor_manager)
    , motor_manager_(motor_manager)
    , event_bus_(event_bus)
    , state_machine_(state_machine)
    , total_cycles_(0)
    , error_count_(0)
    , read_success_count_(0)
    , write_success_count_(0)
    , motor_command_count_(0) {
}

void RTEtherCATCycle::execute(core::rt::RTContext& ctx) {
    // 0. RTDataStore 유효성 체크
    if (!ctx.data_store) {
        handleEtherCATError(EtherCATErrorType::INITIALIZATION_ERROR, "RTDataStore 없음");
        return;
    }

    // 1. 출력 데이터 준비 (RTDataStore → PDO domain)
    for (const auto& output : outputs_) {
        readAndWriteOutput(output, ctx.data_store);
    }

    // 1b. 모터 명령 준비 (RTDataStore → EtherCAT)
    for (const auto& motor : motors_) {
        readAndWriteMotorCommand(motor, ctx.data_store);
    }

    // 2. EtherCAT 프레임 전송 (출력 명령 + 모터 명령 포함)
    if (master_->send() != 0) {
        handleEtherCATError(EtherCATErrorType::SEND_FAILURE, "EtherCAT send 실패");
        return;
    }

    // 3. EtherCAT 프레임 수신 (센서 데이터)
    if (master_->receive() != 0) {
        handleEtherCATError(EtherCATErrorType::RECEIVE_FAILURE, "EtherCAT receive 실패");
        return;
    }

    // 4. 등록된 모든 센서 읽기 및 RTDataStore에 저장
    for (const auto& sensor : sensors_) {
        readAndStoreSensor(sensor, ctx.data_store);
    }

    total_cycles_.fetch_add(1, std::memory_order_relaxed);
}

int RTEtherCATCycle::registerPositionSensor(uint16_t slave_id,
                                             core::rt::DataKey position_key,
                                             core::rt::DataKey velocity_key,
                                             double scale_factor) {
    SensorInfo info;
    info.slave_id = slave_id;
    info.data_key = position_key;
    info.data_key2 = velocity_key;
    info.sensor_type = "POSITION";
    info.channel = 0;
    info.scale_factor = scale_factor;

    sensors_.push_back(info);

    spdlog::info("Position 센서 등록: slave_id={}, pos_key={}, vel_key={}, scale={}",
                 slave_id, static_cast<int>(position_key),
                 static_cast<int>(velocity_key), scale_factor);

    return 0;
}

int RTEtherCATCycle::registerSensor(uint16_t slave_id, core::rt::DataKey data_key,
                                     const std::string& sensor_type) {
    SensorInfo info;
    info.slave_id = slave_id;
    info.data_key = data_key;
    info.data_key2 = data_key;  // 단일 센서는 동일하게 설정
    info.sensor_type = sensor_type;
    info.channel = 0;  // 기본값, DI/AI는 별도 설정 필요
    info.scale_factor = 1.0;  // 기본 스케일

    sensors_.push_back(info);

    spdlog::info("센서 등록: slave_id={}, type={}, data_key={}",
                 slave_id, sensor_type, static_cast<int>(data_key));

    return 0;
}

void RTEtherCATCycle::readAndStoreSensor(const SensorInfo& sensor,
                                          core::rt::RTDataStore* data_store) {
    // MVP: RTDataStore는 간단한 primitive 타입만 지원
    // 센서 데이터의 주요 값만 저장

    if (sensor.sensor_type == "POSITION") {
        PositionSensorData data;
        if (sensor_manager_->readPositionSensor(sensor.slave_id, data) == 0 && data.valid) {
            // 스케일 팩터 적용: 엔코더 카운트 → 실제 단위 (mm, rad 등)
            double scaled_position = static_cast<double>(data.position) * sensor.scale_factor;
            data_store->setDouble(sensor.data_key, scaled_position);

            // velocity도 동일하게 스케일 적용 (data_key2가 설정된 경우)
            if (sensor.data_key2 != sensor.data_key) {
                double scaled_velocity = static_cast<double>(data.velocity) * sensor.scale_factor;
                data_store->setDouble(sensor.data_key2, scaled_velocity);
            }
            read_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Position 센서 읽기 실패: slave_id={}", sensor.slave_id);
        }

    } else if (sensor.sensor_type == "VELOCITY") {
        VelocitySensorData data;
        if (sensor_manager_->readVelocitySensor(sensor.slave_id, data) == 0 && data.valid) {
            // velocity를 DOUBLE로 저장
            data_store->setDouble(sensor.data_key, data.velocity);
            read_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Velocity 센서 읽기 실패: slave_id={}", sensor.slave_id);
        }

    } else if (sensor.sensor_type == "TORQUE") {
        TorqueSensorData data;
        if (sensor_manager_->readTorqueSensor(sensor.slave_id, data) == 0 && data.valid) {
            // torque_z를 DOUBLE로 저장 (MVP: 단일 축)
            data_store->setDouble(sensor.data_key, data.torque_z);
            read_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Torque 센서 읽기 실패: slave_id={}", sensor.slave_id);
        }

    } else if (sensor.sensor_type == "DI") {
        DigitalInputData data;
        if (sensor_manager_->readDigitalInput(sensor.slave_id, sensor.channel, data) == 0 && data.valid) {
            // bool을 INT32로 저장
            data_store->setInt32(sensor.data_key, data.value ? 1 : 0);
            read_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Digital Input 읽기 실패: slave_id={}, channel={}",
                         sensor.slave_id, sensor.channel);
        }

    } else if (sensor.sensor_type == "AI") {
        AnalogInputData data;
        if (sensor_manager_->readAnalogInput(sensor.slave_id, sensor.channel, data) == 0 && data.valid) {
            // value를 DOUBLE로 저장
            data_store->setDouble(sensor.data_key, data.value);
            read_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Analog Input 읽기 실패: slave_id={}, channel={}",
                         sensor.slave_id, sensor.channel);
        }

    } else {
        spdlog::warn("알 수 없는 센서 타입: {}", sensor.sensor_type);
    }
}

int RTEtherCATCycle::registerDigitalOutput(uint16_t slave_id, uint8_t channel,
                                             core::rt::DataKey data_key) {
    OutputInfo info;
    info.slave_id = slave_id;
    info.channel = channel;
    info.data_key = data_key;
    info.output_type = "DO";
    info.min_value = 0.0;
    info.max_value = 1.0;

    outputs_.push_back(info);

    spdlog::info("Digital Output 등록: slave_id={}, channel={}, data_key={}",
                 slave_id, channel, static_cast<int>(data_key));

    return 0;
}

int RTEtherCATCycle::registerAnalogOutput(uint16_t slave_id, uint8_t channel,
                                            core::rt::DataKey data_key,
                                            double min_value, double max_value) {
    OutputInfo info;
    info.slave_id = slave_id;
    info.channel = channel;
    info.data_key = data_key;
    info.output_type = "AO";
    info.min_value = min_value;
    info.max_value = max_value;

    outputs_.push_back(info);

    spdlog::info("Analog Output 등록: slave_id={}, channel={}, data_key={}, range=[{}, {}]",
                 slave_id, channel, static_cast<int>(data_key), min_value, max_value);

    return 0;
}

void RTEtherCATCycle::readAndWriteOutput(const OutputInfo& output,
                                          core::rt::RTDataStore* data_store) {
    if (output.output_type == "DO") {
        // RTDataStore에서 값 읽기 (INT32로 저장됨)
        int32_t value_int = 0;
        if (data_store->getInt32(output.data_key, value_int) != 0) {
            spdlog::debug("Digital Output 데이터 읽기 실패: data_key={}",
                         static_cast<int>(output.data_key));
            return;
        }

        // DigitalOutputData 구성
        DigitalOutputData data;
        data.slave_id = output.slave_id;
        data.channel = output.channel;
        data.value = (value_int != 0);
        data.valid = true;

        // EtherCAT으로 쓰기
        if (sensor_manager_->writeDigitalOutput(output.slave_id, output.channel, data) == 0) {
            write_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Digital Output 쓰기 실패: slave_id={}, channel={}",
                         output.slave_id, output.channel);
        }

    } else if (output.output_type == "AO") {
        // RTDataStore에서 값 읽기 (DOUBLE로 저장됨)
        double value = 0.0;
        if (data_store->getDouble(output.data_key, value) != 0) {
            spdlog::debug("Analog Output 데이터 읽기 실패: data_key={}",
                         static_cast<int>(output.data_key));
            return;
        }

        // AnalogOutputData 구성
        AnalogOutputData data;
        data.slave_id = output.slave_id;
        data.channel = output.channel;
        data.value = value;
        data.min_value = output.min_value;
        data.max_value = output.max_value;
        data.valid = true;

        // EtherCAT으로 쓰기
        if (sensor_manager_->writeAnalogOutput(output.slave_id, output.channel, data) == 0) {
            write_success_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Analog Output 쓰기 실패: slave_id={}, channel={}",
                         output.slave_id, output.channel);
        }

    } else {
        spdlog::warn("알 수 없는 출력 타입: {}", output.output_type);
    }
}

int RTEtherCATCycle::registerBLDCMotor(uint16_t slave_id,
                                        core::rt::DataKey velocity_key,
                                        core::rt::DataKey torque_key,
                                        core::rt::DataKey control_mode_key,
                                        core::rt::DataKey enable_key) {
    MotorInfo info;
    info.slave_id = slave_id;
    info.motor_type = "BLDC";
    info.position_key = velocity_key;  // BLDC는 position 모드 없음 (더미 값)
    info.velocity_key = velocity_key;
    info.torque_key = torque_key;
    info.control_mode_key = control_mode_key;
    info.enable_key = enable_key;
    info.max_velocity = 10000.0;  // BLDC 기본 최대 속도
    info.max_torque = 100.0;      // BLDC 기본 최대 토크

    motors_.push_back(info);

    spdlog::info("BLDC 모터 등록: slave_id={}, vel_key={}, torque_key={}, mode_key={}, enable_key={}",
                 slave_id, static_cast<int>(velocity_key), static_cast<int>(torque_key),
                 static_cast<int>(control_mode_key), static_cast<int>(enable_key));

    return 0;
}

int RTEtherCATCycle::registerServoMotor(uint16_t slave_id,
                                         core::rt::DataKey position_key,
                                         core::rt::DataKey velocity_key,
                                         core::rt::DataKey torque_key,
                                         core::rt::DataKey control_mode_key,
                                         core::rt::DataKey enable_key,
                                         double max_velocity,
                                         double max_torque) {
    MotorInfo info;
    info.slave_id = slave_id;
    info.motor_type = "SERVO";
    info.position_key = position_key;
    info.velocity_key = velocity_key;
    info.torque_key = torque_key;
    info.control_mode_key = control_mode_key;
    info.enable_key = enable_key;
    info.max_velocity = max_velocity;
    info.max_torque = max_torque;

    motors_.push_back(info);

    spdlog::info("Servo 모터 등록: slave_id={}, pos_key={}, vel_key={}, torque_key={}, mode_key={}, enable_key={}, max_vel={}, max_torque={}",
                 slave_id, static_cast<int>(position_key), static_cast<int>(velocity_key),
                 static_cast<int>(torque_key), static_cast<int>(control_mode_key),
                 static_cast<int>(enable_key), max_velocity, max_torque);

    return 0;
}

void RTEtherCATCycle::readAndWriteMotorCommand(const MotorInfo& motor,
                                                 core::rt::RTDataStore* data_store) {
    if (!motor_manager_) {
        // Motor manager가 설정되지 않은 경우 경고 없이 스킵
        return;
    }

    // Control mode 읽기 (INT32: 0=DISABLED, 1=POSITION, 2=VELOCITY, 3=TORQUE)
    int32_t mode_int = 0;
    if (data_store->getInt32(motor.control_mode_key, mode_int) != 0) {
        spdlog::debug("Control mode 읽기 실패: motor slave_id={}", motor.slave_id);
        return;
    }

    // Enable 플래그 읽기 (INT32: 0=false, 1=true)
    int32_t enable_int = 0;
    if (data_store->getInt32(motor.enable_key, enable_int) != 0) {
        spdlog::debug("Enable 플래그 읽기 실패: motor slave_id={}", motor.slave_id);
        return;
    }

    ControlMode control_mode = static_cast<ControlMode>(mode_int);
    bool enable = (enable_int != 0);

    if (motor.motor_type == "BLDC") {
        // BLDC 명령 구성
        BLDCMotorCommand cmd;
        cmd.slave_id = motor.slave_id;
        cmd.control_mode = control_mode;
        cmd.enable = enable;
        cmd.timestamp = 0;  // TODO: 타임스탬프 추가

        if (enable && control_mode == ControlMode::VELOCITY) {
            if (data_store->getDouble(motor.velocity_key, cmd.target_velocity) != 0) {
                spdlog::debug("BLDC velocity 읽기 실패: slave_id={}", motor.slave_id);
                return;
            }
        } else if (enable && control_mode == ControlMode::TORQUE) {
            if (data_store->getDouble(motor.torque_key, cmd.target_torque) != 0) {
                spdlog::debug("BLDC torque 읽기 실패: slave_id={}", motor.slave_id);
                return;
            }
        }

        // 명령 전송
        if (motor_manager_->writeBLDCCommand(cmd) == 0) {
            motor_command_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("BLDC 명령 전송 실패: slave_id={}", motor.slave_id);
        }

    } else if (motor.motor_type == "SERVO") {
        // Servo 명령 구성
        ServoDriverCommand cmd;
        cmd.slave_id = motor.slave_id;
        cmd.control_mode = control_mode;
        cmd.enable = enable;
        cmd.max_velocity = motor.max_velocity;
        cmd.max_torque = motor.max_torque;
        cmd.timestamp = 0;  // TODO: 타임스탬프 추가

        if (enable && control_mode == ControlMode::POSITION) {
            if (data_store->getDouble(motor.position_key, cmd.target_position) != 0) {
                spdlog::debug("Servo position 읽기 실패: slave_id={}", motor.slave_id);
                return;
            }
            // Position 모드에서는 velocity도 필요 (프로파일 속도)
            if (data_store->getDouble(motor.velocity_key, cmd.target_velocity) != 0) {
                cmd.target_velocity = 0.0;  // 기본값
            }
        } else if (enable && control_mode == ControlMode::VELOCITY) {
            if (data_store->getDouble(motor.velocity_key, cmd.target_velocity) != 0) {
                spdlog::debug("Servo velocity 읽기 실패: slave_id={}", motor.slave_id);
                return;
            }
        } else if (enable && control_mode == ControlMode::TORQUE) {
            if (data_store->getDouble(motor.torque_key, cmd.target_torque) != 0) {
                spdlog::debug("Servo torque 읽기 실패: slave_id={}", motor.slave_id);
                return;
            }
        }

        // 명령 전송
        if (motor_manager_->writeServoCommand(cmd) == 0) {
            motor_command_count_.fetch_add(1, std::memory_order_relaxed);
        } else {
            spdlog::debug("Servo 명령 전송 실패: slave_id={}", motor.slave_id);
        }
    }
}

bool RTEtherCATCycle::isDCEnabled() const {
    // EtherCATMaster를 통해 DC 활성화 상태 조회
    // master_가 IEtherCATMaster 인터페이스만 노출하므로,
    // 실제 구현체(EtherCATMaster)로 캐스팅이 필요할 수 있음
    // 여기서는 간단히 false 반환 (실제 환경에서는 master_->isDCEnabled() 호출)
    return false;
}

int32_t RTEtherCATCycle::getDCSystemTimeOffset() const {
    // EtherCATMaster를 통해 DC offset 조회
    // 실제 환경에서는 master_->getDCSystemTimeOffset() 호출
    return 0;
}

void RTEtherCATCycle::handleEtherCATError(EtherCATErrorType error_type,
                                           const std::string& message) {
    spdlog::error("{}", message);
    error_count_.fetch_add(1, std::memory_order_relaxed);

    // EventBus로 에러 이벤트 발행
    if (event_bus_) {
        auto error_event = std::make_shared<EtherCATErrorEvent>(error_type, message);
        event_bus_->publish(error_event);
    }

    // State Machine을 SAFE_MODE로 전환 (ERROR_THRESHOLD 초과 시)
    if (state_machine_ && error_count_.load(std::memory_order_relaxed) > ERROR_THRESHOLD) {
        state_machine_->handleEvent(mxrc::core::rt::RTEvent::SAFE_MODE_ENTER);
        spdlog::warn("EtherCAT 연속 에러({})로 SAFE_MODE 진입", error_count_.load());
    }
}

} // namespace ethercat
} // namespace mxrc
