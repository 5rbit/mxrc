#pragma once

#include "../../event/interfaces/IEvent.h"
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace mxrc {
namespace ethercat {

// EtherCAT 에러 타입
enum class EtherCATErrorType {
    SEND_FAILURE,           // 전송 실패
    RECEIVE_FAILURE,        // 수신 실패
    LINK_DOWN,              // 네트워크 링크 다운
    SLAVE_NOT_RESPONDING,   // Slave 응답 없음
    PDO_MAPPING_ERROR,      // PDO 매핑 에러
    DC_SYNC_ERROR,          // DC 동기화 에러
    DOMAIN_ERROR,           // Domain 처리 에러
    INITIALIZATION_ERROR    // 초기화 에러
};

// EtherCAT 에러 이벤트
class EtherCATErrorEvent : public mxrc::core::event::IEvent {
public:
    EtherCATErrorEvent(EtherCATErrorType error_type,
                       const std::string& description,
                       uint16_t slave_id = 0)
        : error_type_(error_type)
        , description_(description)
        , slave_id_(slave_id)
        , timestamp_(std::chrono::system_clock::now())
        , event_id_(generateEventId()) {
    }

    // IEvent 인터페이스 구현
    std::string getEventId() const override {
        return event_id_;
    }

    mxrc::core::event::EventType getType() const override {
        // EventType enum에 ETHERCAT_ERROR 추가 필요
        // 임시로 0으로 반환
        return static_cast<mxrc::core::event::EventType>(100);  // ETHERCAT_ERROR
    }

    std::chrono::system_clock::time_point getTimestamp() const override {
        return timestamp_;
    }

    std::string getTargetId() const override {
        return "ethercat_master";
    }

    std::string getTypeName() const override {
        return "ETHERCAT_ERROR";
    }

    // EtherCAT 전용 정보
    EtherCATErrorType getErrorType() const { return error_type_; }
    std::string getDescription() const { return description_; }
    uint16_t getSlaveId() const { return slave_id_; }

    std::string getErrorTypeString() const {
        switch (error_type_) {
            case EtherCATErrorType::SEND_FAILURE:
                return "SEND_FAILURE";
            case EtherCATErrorType::RECEIVE_FAILURE:
                return "RECEIVE_FAILURE";
            case EtherCATErrorType::LINK_DOWN:
                return "LINK_DOWN";
            case EtherCATErrorType::SLAVE_NOT_RESPONDING:
                return "SLAVE_NOT_RESPONDING";
            case EtherCATErrorType::PDO_MAPPING_ERROR:
                return "PDO_MAPPING_ERROR";
            case EtherCATErrorType::DC_SYNC_ERROR:
                return "DC_SYNC_ERROR";
            case EtherCATErrorType::DOMAIN_ERROR:
                return "DOMAIN_ERROR";
            case EtherCATErrorType::INITIALIZATION_ERROR:
                return "INITIALIZATION_ERROR";
            default:
                return "UNKNOWN";
        }
    }

private:
    std::string generateEventId() const {
        // 간단한 이벤트 ID 생성 (timestamp + error type)
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp_.time_since_epoch()).count();

        std::ostringstream oss;
        oss << "ethercat_error_"
            << ms << "_"
            << static_cast<int>(error_type_);
        return oss.str();
    }

    EtherCATErrorType error_type_;
    std::string description_;
    uint16_t slave_id_;
    std::chrono::system_clock::time_point timestamp_;
    std::string event_id_;
};

} // namespace ethercat
} // namespace mxrc
