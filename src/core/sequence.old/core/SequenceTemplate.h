#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <any>
#include "core/sequence/dto/SequenceDto.h"

namespace mxrc::core::sequence {

/**
 * @brief 시퀀스 템플릿의 파라미터 정의
 *
 * 템플릿을 인스턴스화할 때 사용할 수 있는 파라미터를 정의합니다.
 */
struct TemplateParameter {
    std::string name;              // 파라미터 이름
    std::string type;              // 파라미터 타입 (string, int, float, bool)
    bool required;                 // 필수 파라미터 여부
    std::string defaultValue;      // 기본값 (선택사항)
    std::string description;       // 파라미터 설명
};

/**
 * @brief 시퀀스 템플릿 정의
 *
 * 공통 패턴을 템플릿으로 정의하여 다양한 파라미터로 재사용할 수 있습니다.
 * 예: "Pick and Place" 템플릿은 좌표를 파라미터로 받아 다양한 위치에서 실행 가능
 */
struct SequenceTemplate {
    std::string id;                          // 템플릿 고유 ID
    std::string name;                        // 템플릿 이름
    std::string version;                     // 버전 (e.g., "1.0.0")
    std::string description;                 // 템플릿 설명

    // 템플릿 파라미터 정의
    std::vector<TemplateParameter> parameters;

    // 기본 시퀀스 정의 (파라미터 치환 전)
    std::vector<std::string> actionIds;      // 원본 액션 ID 목록
    std::map<std::string, std::string> metadata;  // 메타데이터

    // 액션 ID의 파라미터 치환 규칙
    // 예: {"action_move_to_${x}_${y}", "action_pick_at_${location}"}
    // 이렇게 정의하면 인스턴스화할 때 ${x}, ${y}, ${location}이 실제 값으로 치환됨
    std::map<std::string, std::string> parameterSubstitutions;
};

/**
 * @brief 시퀀스 템플릿 인스턴스
 *
 * 템플릿에서 파라미터를 치환하여 생성된 구체적인 시퀀스 정의
 */
struct SequenceTemplateInstance {
    std::string templateId;                  // 원본 템플릿 ID
    std::string instanceId;                  // 인스턴스 고유 ID
    std::string instanceName;                // 인스턴스 이름

    // 사용된 파라미터 값들
    std::map<std::string, std::any> parameters;

    // 최종 생성된 시퀀스 정의
    std::shared_ptr<SequenceDefinition> sequenceDefinition;

    // 인스턴스 생성 타임스탐프
    long long createdAtMs;
};

/**
 * @brief 템플릿 인스턴스화 결과
 */
struct TemplateInstantiationResult {
    bool success;                            // 성공 여부
    std::string instanceId;                  // 생성된 인스턴스 ID (성공 시)
    std::string errorMessage;                // 에러 메시지 (실패 시)
    std::vector<std::string> validationErrors;  // 검증 오류 목록
};

} // namespace mxrc::core::sequence
