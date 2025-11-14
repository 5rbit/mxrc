#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include "core/sequence/dto/SequenceDto.h"
#include "SequenceTemplate.h"

namespace mxrc::core::sequence {

/**
 * @brief 시퀀스 정의 레지스트리
 *
 * 시퀀스 정의를 등록, 저장, 조회하는 기능을 제공합니다.
 * 버전 관리를 통해 같은 ID의 여러 버전 관리 지원.
 */
class SequenceRegistry {
public:
    SequenceRegistry() = default;
    ~SequenceRegistry() = default;

    // Copy/Move operations
    SequenceRegistry(const SequenceRegistry&) = delete;
    SequenceRegistry& operator=(const SequenceRegistry&) = delete;
    SequenceRegistry(SequenceRegistry&&) = default;
    SequenceRegistry& operator=(SequenceRegistry&&) = default;

    /**
     * @brief 시퀀스 정의 등록
     * @param definition 등록할 시퀀스 정의
     * @throw std::invalid_argument 정의가 유효하지 않으면 예외
     * @throw std::runtime_error 동일 ID/버전 존재 시 예외
     */
    void registerSequence(const SequenceDefinition& definition);

    /**
     * @brief 시퀀스 정의 조회 (최신 버전)
     * @param sequenceId 시퀀스 ID
     * @return 찾은 경우 정의, 없으면 nullptr
     */
    std::shared_ptr<const SequenceDefinition> getSequence(const std::string& sequenceId) const;

    /**
     * @brief 시퀀스 정의 조회 (특정 버전)
     * @param sequenceId 시퀀스 ID
     * @param version 버전 문자열
     * @return 찾은 경우 정의, 없으면 nullptr
     */
    std::shared_ptr<const SequenceDefinition> getSequence(
        const std::string& sequenceId,
        const std::string& version) const;

    /**
     * @brief 시퀀스 정의 존재 여부 확인
     * @param sequenceId 시퀀스 ID
     * @return 존재 여부
     */
    bool hasSequence(const std::string& sequenceId) const;

    /**
     * @brief 특정 ID의 모든 버전 조회
     * @param sequenceId 시퀀스 ID
     * @return 버전 목록 (최신순 정렬)
     */
    std::vector<std::string> getVersions(const std::string& sequenceId) const;

    /**
     * @brief 등록된 모든 시퀀스 ID 조회
     * @return 시퀀스 ID 목록
     */
    std::vector<std::string> getAllSequenceIds() const;

    /**
     * @brief 시퀀스 정의 삭제
     * @param sequenceId 시퀀스 ID
     * @return 삭제 여부
     */
    bool removeSequence(const std::string& sequenceId);

    /**
     * @brief 특정 버전 삭제
     * @param sequenceId 시퀀스 ID
     * @param version 버전
     * @return 삭제 여부
     */
    bool removeSequenceVersion(const std::string& sequenceId, const std::string& version);

    /**
     * @brief 레지스트리 초기화
     */
    void clear();

    /**
     * @brief 등록된 시퀀스 개수 조회
     * @return 시퀀스 개수
     */
    size_t getSequenceCount() const;

    /**
     * @brief 시퀀스 템플릿 등록
     * @param templateDef 등록할 템플릿 정의
     * @throw std::invalid_argument 템플릿이 유효하지 않으면 예외
     * @throw std::runtime_error 동일 ID 존재 시 예외
     */
    void registerTemplate(const SequenceTemplate& templateDef);

    /**
     * @brief 시퀀스 템플릿 조회
     * @param templateId 템플릿 ID
     * @return 찾은 경우 템플릿, 없으면 nullptr
     */
    std::shared_ptr<const SequenceTemplate> getTemplate(const std::string& templateId) const;

    /**
     * @brief 시퀀스 템플릿 존재 여부 확인
     * @param templateId 템플릿 ID
     * @return 존재 여부
     */
    bool hasTemplate(const std::string& templateId) const;

    /**
     * @brief 등록된 모든 템플릿 ID 조회
     * @return 템플릿 ID 목록
     */
    std::vector<std::string> getAllTemplateIds() const;

    /**
     * @brief 템플릿 인스턴스 저장
     * @param instance 저장할 템플릿 인스턴스
     */
    void saveTemplateInstance(const SequenceTemplateInstance& instance);

    /**
     * @brief 템플릿 인스턴스 조회
     * @param instanceId 인스턴스 ID
     * @return 찾은 경우 인스턴스, 없으면 nullptr
     */
    std::shared_ptr<const SequenceTemplateInstance> getTemplateInstance(
        const std::string& instanceId) const;

    /**
     * @brief 특정 템플릿으로부터 생성된 모든 인스턴스 조회
     * @param templateId 템플릿 ID
     * @return 인스턴스 ID 목록
     */
    std::vector<std::string> getTemplateInstances(const std::string& templateId) const;

    /**
     * @brief 템플릿 삭제
     * @param templateId 템플릿 ID
     * @return 삭제 여부
     */
    bool removeTemplate(const std::string& templateId);

private:
    /**
     * @brief 정의 유효성 검증
     * @param definition 검증할 정의
     * @throw std::invalid_argument 유효하지 않으면 예외
     */
    void validateDefinition(const SequenceDefinition& definition) const;

    /**
     * @brief 버전 비교 함수 (버전 문자열 대소 비교)
     * @param v1 버전 1
     * @param v2 버전 2
     * @return v1 > v2 면 true
     */
    static bool isVersionGreater(const std::string& v1, const std::string& v2);

    // Key: sequenceId, Value: <Version, SequenceDefinition>
    std::map<std::string, std::map<std::string, SequenceDefinition>> sequences_;

    // 템플릿 저장소
    // Key: templateId, Value: SequenceTemplate
    std::map<std::string, SequenceTemplate> templates_;

    // 템플릿 인스턴스 저장소
    // Key: instanceId, Value: SequenceTemplateInstance
    std::map<std::string, SequenceTemplateInstance> templateInstances_;

    // 템플릿별 인스턴스 맵핑
    // Key: templateId, Value: 인스턴스 ID 목록
    std::map<std::string, std::vector<std::string>> templateInstanceMap_;
};

} // namespace mxrc::core::sequence

