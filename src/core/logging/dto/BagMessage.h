#ifndef MXRC_CORE_LOGGING_DTO_BAGMESSAGE_H
#define MXRC_CORE_LOGGING_DTO_BAGMESSAGE_H

#include "DataType.h"
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일에 저장되는 단일 메시지 단위
 *
 * JSONL 포맷으로 직렬화되어 Bag 파일에 기록됩니다.
 * 각 메시지는 나노초 정밀도 타임스탬프, topic(DataStore ID),
 * 데이터 타입, 직렬화된 값을 포함합니다.
 */
struct BagMessage {
    int64_t timestamp_ns;           ///< 나노초 정밀도 Unix 타임스탬프
    std::string topic;              ///< DataStore ID (예: "mission_state")
    DataType data_type;             ///< 데이터 유형 구분
    std::string serialized_value;   ///< JSON 직렬화된 값

    /**
     * @brief JSON 직렬화
     * @return JSON 객체
     */
    nlohmann::json toJson() const {
        return {
            {"timestamp", timestamp_ns},
            {"topic", topic},
            {"type", dataTypeToString(data_type)},
            {"value", nlohmann::json::parse(serialized_value)}
        };
    }

    /**
     * @brief JSON 역직렬화
     * @param j JSON 객체
     * @return BagMessage 인스턴스
     * @throws nlohmann::json::exception JSON 파싱 오류 시
     */
    static BagMessage fromJson(const nlohmann::json& j) {
        BagMessage msg;
        msg.timestamp_ns = j["timestamp"];
        msg.topic = j["topic"];
        msg.data_type = stringToDataType(j["type"]);
        msg.serialized_value = j["value"].dump();
        return msg;
    }

    /**
     * @brief 메시지 유효성 검증
     * @return 유효하면 true, 아니면 false
     *
     * 검증 규칙:
     * - timestamp_ns > 0
     * - topic 비어있지 않음
     * - topic 최대 256자
     * - serialized_value 비어있지 않음
     */
    bool isValid() const {
        return timestamp_ns > 0 &&
               !topic.empty() &&
               topic.size() <= 256 &&
               !serialized_value.empty();
    }

    /**
     * @brief JSONL 라인으로 직렬화 (개행 문자 포함)
     * @return JSONL 라인 문자열
     */
    std::string toJsonLine() const {
        return toJson().dump() + "\n";
    }

    /**
     * @brief JSONL 라인에서 역직렬화
     * @param line JSONL 라인 (개행 문자 제거 필요)
     * @return BagMessage 인스턴스
     * @throws nlohmann::json::exception JSON 파싱 오류 시
     */
    static BagMessage fromJsonLine(const std::string& line) {
        // 개행 문자 제거 (있으면)
        std::string trimmed = line;
        if (!trimmed.empty() && trimmed.back() == '\n') {
            trimmed.pop_back();
        }
        if (!trimmed.empty() && trimmed.back() == '\r') {
            trimmed.pop_back();
        }

        auto j = nlohmann::json::parse(trimmed);
        return fromJson(j);
    }
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_DTO_BAGMESSAGE_H
