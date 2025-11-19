#ifndef MXRC_CORE_LOGGING_DTO_DATATYPE_H
#define MXRC_CORE_LOGGING_DTO_DATATYPE_H

#include <string>
#include <stdexcept>

namespace mxrc::core::logging {

/**
 * @brief DataStore에 저장되는 데이터의 타입 구분
 *
 * 로깅 전략 설정 시 타입별로 차별화된 처리가 가능합니다:
 * - RobotMode: 로봇 동작 모드 (MANUAL, AUTO, etc.)
 * - InterfaceData: 고빈도 센서 데이터 (순환 버퍼 권장)
 * - Config: 설정 데이터 (변경 빈도 낮음)
 * - Para: 파라미터 (변경 빈도 낮음)
 * - Alarm: 알람 데이터 (이벤트 기반 로깅 권장)
 * - Event: 이벤트 데이터
 * - MissionState: 미션 상태 (전체 로깅 권장)
 * - TaskState: 태스크 상태 (전체 로깅 권장)
 */
enum class DataType {
    RobotMode,      ///< 로봇 동작 모드
    InterfaceData,  ///< 인터페이스/센서 데이터 (고빈도)
    Config,         ///< 설정 데이터
    Para,           ///< 파라미터
    Alarm,          ///< 알람
    Event,          ///< 이벤트
    MissionState,   ///< 미션 상태
    TaskState       ///< 태스크 상태
};

/**
 * @brief DataType을 문자열로 변환
 * @param type DataType 열거형 값
 * @return 문자열 표현
 */
inline std::string dataTypeToString(DataType type) {
    switch (type) {
        case DataType::RobotMode:      return "RobotMode";
        case DataType::InterfaceData:  return "InterfaceData";
        case DataType::Config:         return "Config";
        case DataType::Para:           return "Para";
        case DataType::Alarm:          return "Alarm";
        case DataType::Event:          return "Event";
        case DataType::MissionState:   return "MissionState";
        case DataType::TaskState:      return "TaskState";
        default:                       return "Unknown";
    }
}

/**
 * @brief 문자열을 DataType으로 변환
 * @param str 문자열 표현
 * @return DataType 열거형 값
 * @throws std::invalid_argument 유효하지 않은 문자열인 경우
 */
inline DataType stringToDataType(const std::string& str) {
    if (str == "RobotMode")      return DataType::RobotMode;
    if (str == "InterfaceData")  return DataType::InterfaceData;
    if (str == "Config")         return DataType::Config;
    if (str == "Para")           return DataType::Para;
    if (str == "Alarm")          return DataType::Alarm;
    if (str == "Event")          return DataType::Event;
    if (str == "MissionState")   return DataType::MissionState;
    if (str == "TaskState")      return DataType::TaskState;
    throw std::invalid_argument("Unknown DataType: " + str);
}

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_DTO_DATATYPE_H
