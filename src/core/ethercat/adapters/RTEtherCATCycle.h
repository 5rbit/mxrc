#pragma once

#include "../interfaces/IEtherCATMaster.h"
#include "../interfaces/ISensorDataManager.h"
#include "../interfaces/IMotorCommandManager.h"
#include "../dto/MotorCommand.h"
#include "../events/EtherCATErrorEvent.h"
#include "../../rt/RTContext.h"
#include "../../rt/RTDataStore.h"
#include "../../rt/RTStateMachine.h"
#include "../../event/interfaces/IEventBus.h"
#include <atomic>
#include <memory>
#include <vector>

namespace mxrc {
namespace ethercat {

// RT Executive의 주기적 실행에 통합되는 EtherCAT Cycle 어댑터
// 매 RT cycle마다 EtherCAT 통신 수행 (send → receive → 센서 읽기 + 모터 명령 쓰기)
class RTEtherCATCycle {
public:
    // 생성자: EtherCAT Master, 센서 데이터 매니저, 모터 명령 매니저 주입
    RTEtherCATCycle(
        std::shared_ptr<IEtherCATMaster> master,
        std::shared_ptr<ISensorDataManager> sensor_manager,
        std::shared_ptr<IMotorCommandManager> motor_manager = nullptr,
        std::shared_ptr<mxrc::core::event::IEventBus> event_bus = nullptr,
        std::shared_ptr<mxrc::core::rt::RTStateMachine> state_machine = nullptr);

    ~RTEtherCATCycle() = default;

    // RT Cycle에서 호출되는 메인 함수
    // RTContext를 통해 RTDataStore 접근
    void execute(core::rt::RTContext& ctx);

    // 센서 읽기를 수행할 slave 등록
    // slave_id: EtherCAT slave 주소
    // position_key: Position 저장 키 (DOUBLE)
    // velocity_key: Velocity 저장 키 (DOUBLE)
    // scale_factor: 엔코더 카운트 → 실제 단위 변환 (예: 0.001 = 1 count = 0.001 mm)
    int registerPositionSensor(uint16_t slave_id,
                               core::rt::DataKey position_key,
                               core::rt::DataKey velocity_key,
                               double scale_factor = 1.0);

    // 범용 센서 등록 (이전 호환성)
    int registerSensor(uint16_t slave_id, core::rt::DataKey data_key, const std::string& sensor_type);

    // Digital Output 등록
    // slave_id: EtherCAT slave 주소
    // channel: 출력 채널 번호
    // data_key: RTDataStore에서 읽을 키
    int registerDigitalOutput(uint16_t slave_id, uint8_t channel, core::rt::DataKey data_key);

    // Analog Output 등록
    // slave_id: EtherCAT slave 주소
    // channel: 출력 채널 번호
    // data_key: RTDataStore에서 읽을 키
    // min_value, max_value: 출력 범위 제한
    int registerAnalogOutput(uint16_t slave_id, uint8_t channel, core::rt::DataKey data_key,
                             double min_value = -10.0, double max_value = 10.0);

    // BLDC 모터 등록
    // slave_id: EtherCAT slave 주소
    // velocity_key: Target velocity를 읽을 키 (ControlMode::VELOCITY 시)
    // torque_key: Target torque를 읽을 키 (ControlMode::TORQUE 시)
    // control_mode_key: Control mode를 읽을 키 (INT32: 0=DISABLED, 2=VELOCITY, 3=TORQUE)
    // enable_key: Enable 플래그를 읽을 키 (INT32: 0=false, 1=true)
    int registerBLDCMotor(uint16_t slave_id,
                          core::rt::DataKey velocity_key,
                          core::rt::DataKey torque_key,
                          core::rt::DataKey control_mode_key,
                          core::rt::DataKey enable_key);

    // Servo 모터 등록
    // slave_id: EtherCAT slave 주소
    // position_key: Target position을 읽을 키 (ControlMode::POSITION 시)
    // velocity_key: Target velocity를 읽을 키
    // torque_key: Target torque를 읽을 키
    // control_mode_key: Control mode를 읽을 키 (INT32: 0=DISABLED, 1=POSITION, 2=VELOCITY, 3=TORQUE)
    // enable_key: Enable 플래그를 읽을 키
    // max_velocity: 최대 속도 제한 (기본값 10.0)
    // max_torque: 최대 토크 제한 (기본값 100.0)
    int registerServoMotor(uint16_t slave_id,
                           core::rt::DataKey position_key,
                           core::rt::DataKey velocity_key,
                           core::rt::DataKey torque_key,
                           core::rt::DataKey control_mode_key,
                           core::rt::DataKey enable_key,
                           double max_velocity = 10.0,
                           double max_torque = 100.0);

    // 통계 조회
    uint64_t getTotalCycles() const { return total_cycles_.load(std::memory_order_relaxed); }
    uint64_t getErrorCount() const { return error_count_.load(std::memory_order_relaxed); }
    uint64_t getReadSuccessCount() const { return read_success_count_.load(std::memory_order_relaxed); }
    uint64_t getWriteSuccessCount() const { return write_success_count_.load(std::memory_order_relaxed); }
    uint64_t getMotorCommandCount() const { return motor_command_count_.load(std::memory_order_relaxed); }

    // DC (Distributed Clock) 통계 조회
    bool isDCEnabled() const;
    int32_t getDCSystemTimeOffset() const;

private:
    // 센서 정보 구조체
    struct SensorInfo {
        uint16_t slave_id;
        core::rt::DataKey data_key;
        core::rt::DataKey data_key2;  // 2축용 (position의 velocity)
        std::string sensor_type;      // "POSITION", "VELOCITY", "TORQUE", "DI", "AI"
        uint8_t channel;              // DI/AI의 경우 채널 번호
        double scale_factor;          // 스케일 팩터 (엔코더 → 실제 단위)
    };

    // 의존성
    std::shared_ptr<IEtherCATMaster> master_;
    std::shared_ptr<ISensorDataManager> sensor_manager_;
    std::shared_ptr<IMotorCommandManager> motor_manager_;
    std::shared_ptr<mxrc::core::event::IEventBus> event_bus_;
    std::shared_ptr<mxrc::core::rt::RTStateMachine> state_machine_;

    // 출력 정보 구조체
    struct OutputInfo {
        uint16_t slave_id;
        uint8_t channel;
        core::rt::DataKey data_key;
        std::string output_type;  // "DO", "AO"
        double min_value;         // AO용 범위 제한
        double max_value;
    };

    // 모터 정보 구조체
    struct MotorInfo {
        uint16_t slave_id;
        std::string motor_type;      // "BLDC", "SERVO"
        core::rt::DataKey position_key;      // Servo POSITION 모드용
        core::rt::DataKey velocity_key;      // VELOCITY 모드용
        core::rt::DataKey torque_key;        // TORQUE 모드용
        core::rt::DataKey control_mode_key;  // Control mode
        core::rt::DataKey enable_key;        // Enable 플래그
        double max_velocity;         // Servo용 최대 속도
        double max_torque;           // Servo용 최대 토크
    };

    // 등록된 센서 목록
    std::vector<SensorInfo> sensors_;

    // 등록된 출력 목록
    std::vector<OutputInfo> outputs_;

    // 등록된 모터 목록
    std::vector<MotorInfo> motors_;

    // 통계 (atomic for thread-safety)
    std::atomic<uint64_t> total_cycles_;
    std::atomic<uint64_t> error_count_;
    std::atomic<uint64_t> read_success_count_;
    std::atomic<uint64_t> write_success_count_;
    std::atomic<uint64_t> motor_command_count_;

    // 에러 임계값 상수
    static constexpr uint64_t ERROR_THRESHOLD = 10;

    // 헬퍼: 센서 데이터 읽고 RTDataStore에 저장
    void readAndStoreSensor(const SensorInfo& sensor, core::rt::RTDataStore* data_store);

    // 헬퍼: RTDataStore에서 읽고 출력 쓰기
    void readAndWriteOutput(const OutputInfo& output, core::rt::RTDataStore* data_store);

    // 헬퍼: RTDataStore에서 모터 명령 읽고 EtherCAT 전송
    void readAndWriteMotorCommand(const MotorInfo& motor, core::rt::RTDataStore* data_store);

    // 헬퍼: EtherCAT 에러 처리 (중복 코드 제거)
    void handleEtherCATError(EtherCATErrorType error_type, const std::string& message);
};

} // namespace ethercat
} // namespace mxrc
