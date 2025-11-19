#ifndef MXRC_CORE_LOGGING_UTIL_SERIALIZER_H
#define MXRC_CORE_LOGGING_UTIL_SERIALIZER_H

#include "dto/DataType.h"
#include <nlohmann/json.hpp>
#include <any>
#include <string>

namespace mxrc::core::logging {

/**
 * @brief std::any 값을 JSON으로 직렬화하는 유틸리티
 *
 * MISRA C++ 2023 Rule 8.2.9 준수를 위해 RTTI 대신
 * DataType enum 기반 switch문을 사용합니다.
 *
 * 성능 영향: ~1-5ns (switch문 오버헤드)
 */
class Serializer {
public:
    /**
     * @brief std::any 값을 JSON으로 직렬화
     * @param value std::any로 감싼 값
     * @param type 데이터 타입 구분자
     * @return JSON 객체
     * @throws std::bad_any_cast 타입 불일치 시
     */
    static nlohmann::json serialize(const std::any& value, DataType type);

    /**
     * @brief JSON을 std::any로 역직렬화
     * @param json JSON 객체
     * @param type 데이터 타입 구분자
     * @return std::any로 감싼 값
     * @throws std::invalid_argument 타입 불일치 시
     */
    static std::any deserialize(const nlohmann::json& json, DataType type);

private:
    // DataType별 직렬화 헬퍼 함수들
    static nlohmann::json serializeRobotMode(const std::any& value);
    static nlohmann::json serializeInterfaceData(const std::any& value);
    static nlohmann::json serializeConfig(const std::any& value);
    static nlohmann::json serializePara(const std::any& value);
    static nlohmann::json serializeAlarm(const std::any& value);
    static nlohmann::json serializeEvent(const std::any& value);
    static nlohmann::json serializeMissionState(const std::any& value);
    static nlohmann::json serializeTaskState(const std::any& value);

    // DataType별 역직렬화 헬퍼 함수들
    static std::any deserializeRobotMode(const nlohmann::json& json);
    static std::any deserializeInterfaceData(const nlohmann::json& json);
    static std::any deserializeConfig(const nlohmann::json& json);
    static std::any deserializePara(const nlohmann::json& json);
    static std::any deserializeAlarm(const nlohmann::json& json);
    static std::any deserializeEvent(const nlohmann::json& json);
    static std::any deserializeMissionState(const nlohmann::json& json);
    static std::any deserializeTaskState(const nlohmann::json& json);
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_UTIL_SERIALIZER_H
