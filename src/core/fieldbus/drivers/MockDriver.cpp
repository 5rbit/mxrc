#include "MockDriver.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace mxrc::core::fieldbus {

// Mock 드라이버 생성자
MockDriver::MockDriver(const FieldbusConfig& config, size_t device_count)
    : config_(config),
      device_count_(device_count),
      sensor_data_(device_count, 0.0),
      actuator_data_(device_count, 0.0),
      digital_inputs_(device_count, false),
      digital_outputs_(device_count, false) {
    spdlog::debug("[MockDriver] Created with {} devices", device_count);
}

// Mock 드라이버 소멸자
MockDriver::~MockDriver() {
    if (status_ != FieldbusStatus::UNINITIALIZED &&
        status_ != FieldbusStatus::STOPPED) {
        shutdown();
    }
}

// 드라이버 초기화
bool MockDriver::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::UNINITIALIZED) {
        spdlog::warn("[MockDriver] Already initialized");
        return false;
    }

    // 초기화 지연 시뮬레이션
    spdlog::info("[MockDriver] Initializing {} devices...", device_count_);

    // 센서 데이터를 패턴으로 초기화
    for (size_t i = 0; i < device_count_; ++i) {
        sensor_data_[i] = std::sin(i * 0.1);
    }

    status_ = FieldbusStatus::INITIALIZED;
    spdlog::info("[MockDriver] Initialized successfully");
    return true;
}

// 드라이버 시작
bool MockDriver::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    // INITIALIZED 또는 STOPPED 상태에서 시작 허용
    if (status_ != FieldbusStatus::INITIALIZED &&
        status_ != FieldbusStatus::STOPPED) {
        last_error_ = "Cannot start: not initialized or stopped";
        spdlog::error("[MockDriver] {}", *last_error_);
        return false;
    }

    status_ = FieldbusStatus::RUNNING;
    last_cycle_time_ = std::chrono::steady_clock::now();
    simulation_tick_ = 0;
    emergency_stopped_ = false;

    spdlog::info("[MockDriver] Started cyclic communication");
    return true;
}

// 드라이버 정지
bool MockDriver::stop() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        spdlog::warn("[MockDriver] Not running");
        return false;
    }

    status_ = FieldbusStatus::STOPPED;
    spdlog::info("[MockDriver] Stopped cyclic communication");
    return true;
}

// 드라이버 종료
void MockDriver::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    spdlog::info("[MockDriver] Shutting down...");

    // 모든 데이터 클리어
    std::fill(sensor_data_.begin(), sensor_data_.end(), 0.0);
    std::fill(actuator_data_.begin(), actuator_data_.end(), 0.0);
    std::fill(digital_inputs_.begin(), digital_inputs_.end(), false);
    std::fill(digital_outputs_.begin(), digital_outputs_.end(), false);

    status_ = FieldbusStatus::UNINITIALIZED;
    last_error_.reset();
    emergency_stopped_ = false;

    spdlog::info("[MockDriver] Shutdown complete");
}

// 센서 데이터 읽기
bool MockDriver::readSensors(std::vector<double>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        last_error_ = "Cannot read: not running";
        return false;
    }

    if (emergency_stopped_) {
        // 비상 정지 시 0을 반환
        data.assign(device_count_, 0.0);
        return true;
    }

    // 센서 데이터 시뮬레이션 (사인파 + 액추에이터 에코)
    simulation_tick_++;
    for (size_t i = 0; i < device_count_; ++i) {
        sensor_data_[i] = actuator_data_[i] +
                          0.1 * std::sin(simulation_tick_ * 0.01 + i * 0.1);
    }

    // 출력으로 복사
    if (data.size() != device_count_) {
        data.resize(device_count_);
    }
    data = sensor_data_;

    // 통계 업데이트
    auto now = std::chrono::steady_clock::now();
    auto cycle_time_us = std::chrono::duration<double, std::micro>(
        now - last_cycle_time_).count();
    last_cycle_time_ = now;
    updateStatistics(cycle_time_us);

    return true;
}

// 액추에이터 데이터 쓰기
bool MockDriver::writeActuators(const std::vector<double>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        last_error_ = "Cannot write: not running";
        return false;
    }

    if (emergency_stopped_) {
        last_error_ = "Cannot write: emergency stopped";
        return false;
    }

    if (data.size() != device_count_) {
        last_error_ = "Data size mismatch: expected " +
                      std::to_string(device_count_) +
                      ", got " + std::to_string(data.size());
        return false;
    }

    // 액추에이터 명령 저장
    actuator_data_ = data;

    stats_.bytes_sent += data.size() * sizeof(double);
    return true;
}

// 디지털 입력 읽기
bool MockDriver::readDigitalInputs(std::vector<bool>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    if (data.size() != device_count_) {
        data.resize(device_count_);
    }

    data = digital_inputs_;
    return true;
}

// 디지털 출력 쓰기
bool MockDriver::writeDigitalOutputs(const std::vector<bool>& data) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != FieldbusStatus::RUNNING) {
        return false;
    }

    if (data.size() != device_count_) {
        last_error_ = "Digital output size mismatch";
        return false;
    }

    digital_outputs_ = data;
    return true;
}

// 현재 상태 반환
FieldbusStatus MockDriver::getStatus() const {
    return status_.load();
}

// 통계 정보 반환
FieldbusStats MockDriver::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

// 프로토콜 이름 반환
std::string MockDriver::getProtocolName() const {
    return "Mock";
}

// 장치 수 반환
size_t MockDriver::getDeviceCount() const {
    return device_count_;
}

// 마지막 오류 메시지 반환
std::optional<std::string> MockDriver::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

// 비상 정지
bool MockDriver::emergencyStop() {
    std::lock_guard<std::mutex> lock(mutex_);

    emergency_stopped_ = true;

    // 모든 액추에이터를 0으로 설정
    std::fill(actuator_data_.begin(), actuator_data_.end(), 0.0);
    std::fill(digital_outputs_.begin(), digital_outputs_.end(), false);

    spdlog::warn("[MockDriver] EMERGENCY STOP activated");
    return true;
}

// 오류 리셋
bool MockDriver::resetErrors() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ == FieldbusStatus::ERROR) {
        status_ = FieldbusStatus::INITIALIZED;
        last_error_.reset();
        emergency_stopped_ = false;
        spdlog::info("[MockDriver] Errors reset");
        return true;
    }

    return false;
}

// 시뮬레이션된 오류 설정
void MockDriver::setSimulatedError(const std::string& error_msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (error_msg.empty()) {
        last_error_.reset();
        if (status_ == FieldbusStatus::ERROR) {
            status_ = FieldbusStatus::INITIALIZED;
        }
    } else {
        last_error_ = error_msg;
        status_ = FieldbusStatus::ERROR;
        spdlog::error("[MockDriver] Simulated error: {}", error_msg);
    }
}

// 통계 업데이트
void MockDriver::updateStatistics(double cycle_time_us) {
    stats_.total_cycles++;
    stats_.bytes_received += device_count_ * sizeof(double);

    // 평균 사이클 시간 업데이트 (지수 이동 평균)
    const double alpha = 0.1;
    if (stats_.avg_cycle_time_us == 0.0) {
        stats_.avg_cycle_time_us = cycle_time_us;
    } else {
        stats_.avg_cycle_time_us = alpha * cycle_time_us +
                                    (1.0 - alpha) * stats_.avg_cycle_time_us;
    }

    // 최대 사이클 시간 업데이트
    if (cycle_time_us > stats_.max_cycle_time_us) {
        stats_.max_cycle_time_us = cycle_time_us;
    }

    // 마감 시간 누락 확인
    if (cycle_time_us > config_.cycle_time_us * 1.1) {  // 10% 허용 오차
        stats_.missed_cycles++;
    }
}

} // namespace mxrc::core::fieldbus
