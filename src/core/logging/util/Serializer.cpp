#include "util/Serializer.h"
#include <spdlog/spdlog.h>

namespace mxrc::core::logging {

nlohmann::json Serializer::serialize(const std::any& value, DataType type) {
    nlohmann::json j;

    try {
        switch (type) {
            case DataType::RobotMode:
                j = serializeRobotMode(value);
                break;
            case DataType::InterfaceData:
                j = serializeInterfaceData(value);
                break;
            case DataType::Config:
                j = serializeConfig(value);
                break;
            case DataType::Para:
                j = serializePara(value);
                break;
            case DataType::Alarm:
                j = serializeAlarm(value);
                break;
            case DataType::Event:
                j = serializeEvent(value);
                break;
            case DataType::MissionState:
                j = serializeMissionState(value);
                break;
            case DataType::TaskState:
                j = serializeTaskState(value);
                break;
            default:
                j = nlohmann::json::object();
                j["error"] = "unknown_type";
                spdlog::warn("Unknown DataType in serialization");
                break;
        }
    } catch (const std::bad_any_cast& e) {
        spdlog::error("Type mismatch in serialization: {}", e.what());
        j = nlohmann::json::object();
        j["error"] = "type_mismatch";
    }

    return j;
}

std::any Serializer::deserialize(const nlohmann::json& json, DataType type) {
    switch (type) {
        case DataType::RobotMode:
            return deserializeRobotMode(json);
        case DataType::InterfaceData:
            return deserializeInterfaceData(json);
        case DataType::Config:
            return deserializeConfig(json);
        case DataType::Para:
            return deserializePara(json);
        case DataType::Alarm:
            return deserializeAlarm(json);
        case DataType::Event:
            return deserializeEvent(json);
        case DataType::MissionState:
            return deserializeMissionState(json);
        case DataType::TaskState:
            return deserializeTaskState(json);
        default:
            throw std::invalid_argument("Unknown DataType in deserialization");
    }
}

// RobotMode 직렬화/역직렬화
nlohmann::json Serializer::serializeRobotMode(const std::any& value) {
    // RobotMode는 정수형으로 가정 (enum을 int로 저장)
    if (value.type() == typeid(int)) {
        int mode = std::any_cast<int>(value);
        return nlohmann::json{{"mode", mode}};
    } else if (value.type() == typeid(std::string)) {
        // 문자열로도 저장 가능
        return nlohmann::json{{"mode", std::any_cast<std::string>(value)}};
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeRobotMode(const nlohmann::json& json) {
    if (json.contains("mode")) {
        if (json["mode"].is_number()) {
            return json["mode"].get<int>();
        } else if (json["mode"].is_string()) {
            return json["mode"].get<std::string>();
        }
    }
    throw std::invalid_argument("Invalid RobotMode JSON");
}

// InterfaceData 직렬화/역직렬화 (고빈도 센서 데이터)
nlohmann::json Serializer::serializeInterfaceData(const std::any& value) {
    // InterfaceData는 구조화된 데이터로 가정
    if (value.type() == typeid(std::string)) {
        // 이미 JSON 문자열인 경우
        return nlohmann::json::parse(std::any_cast<std::string>(value));
    } else if (value.type() == typeid(double)) {
        // 단일 값 (예: 센서 읽기값)
        return nlohmann::json{{"value", std::any_cast<double>(value)}};
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeInterfaceData(const nlohmann::json& json) {
    // JSON 객체를 문자열로 변환하여 저장
    return json.dump();
}

// Config 직렬화/역직렬화
nlohmann::json Serializer::serializeConfig(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        std::string str = std::any_cast<std::string>(value);
        // JSON 문자열인지 확인
        try {
            return nlohmann::json::parse(str);
        } catch (...) {
            // 일반 문자열
            return nlohmann::json{{"value", str}};
        }
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeConfig(const nlohmann::json& json) {
    return json.dump();
}

// Para 직렬화/역직렬화
nlohmann::json Serializer::serializePara(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        return nlohmann::json{{"parameter", std::any_cast<std::string>(value)}};
    } else if (value.type() == typeid(double)) {
        return nlohmann::json{{"parameter", std::any_cast<double>(value)}};
    } else if (value.type() == typeid(int)) {
        return nlohmann::json{{"parameter", std::any_cast<int>(value)}};
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializePara(const nlohmann::json& json) {
    if (json.contains("parameter")) {
        if (json["parameter"].is_number_float()) {
            return json["parameter"].get<double>();
        } else if (json["parameter"].is_number_integer()) {
            return json["parameter"].get<int>();
        } else if (json["parameter"].is_string()) {
            return json["parameter"].get<std::string>();
        }
    }
    throw std::invalid_argument("Invalid Para JSON");
}

// Alarm 직렬화/역직렬화
nlohmann::json Serializer::serializeAlarm(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        std::string str = std::any_cast<std::string>(value);
        try {
            return nlohmann::json::parse(str);
        } catch (...) {
            return nlohmann::json{{"message", str}};
        }
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeAlarm(const nlohmann::json& json) {
    return json.dump();
}

// Event 직렬화/역직렬화
nlohmann::json Serializer::serializeEvent(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        std::string str = std::any_cast<std::string>(value);
        try {
            return nlohmann::json::parse(str);
        } catch (...) {
            return nlohmann::json{{"event", str}};
        }
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeEvent(const nlohmann::json& json) {
    return json.dump();
}

// MissionState 직렬화/역직렬화
nlohmann::json Serializer::serializeMissionState(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        std::string str = std::any_cast<std::string>(value);
        // MissionState는 구조화된 JSON으로 가정
        try {
            return nlohmann::json::parse(str);
        } catch (...) {
            return nlohmann::json{{"state", str}};
        }
    } else if (value.type() == typeid(int)) {
        // enum을 int로 저장
        return nlohmann::json{{"state", std::any_cast<int>(value)}};
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeMissionState(const nlohmann::json& json) {
    return json.dump();
}

// TaskState 직렬화/역직렬화
nlohmann::json Serializer::serializeTaskState(const std::any& value) {
    if (value.type() == typeid(std::string)) {
        std::string str = std::any_cast<std::string>(value);
        try {
            return nlohmann::json::parse(str);
        } catch (...) {
            return nlohmann::json{{"task_state", str}};
        }
    }
    throw std::bad_any_cast();
}

std::any Serializer::deserializeTaskState(const nlohmann::json& json) {
    return json.dump();
}

} // namespace mxrc::core::logging
