#include "SequenceRegistry.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace mxrc::core::sequence {

void SequenceRegistry::registerSequence(const SequenceDefinition& definition) {
    // 정의 유효성 검증
    validateDefinition(definition);

    auto logger = spdlog::get("mxrc");

    // 동일 ID/버전이 이미 등록되어 있는지 확인
    auto it = sequences_.find(definition.id);
    if (it != sequences_.end()) {
        if (it->second.find(definition.version) != it->second.end()) {
            logger->error(
                "시퀀스 버전 이미 등록됨: id={}, version={}",
                definition.id, definition.version);
            throw std::runtime_error(
                "Sequence version already registered: " + definition.id + "@" + definition.version);
        }
    }

    // 시퀀스 등록
    sequences_[definition.id][definition.version] = definition;
    logger->info(
        "시퀀스 등록됨: id={}, version={}, actions.size={}",
        definition.id, definition.version, definition.actionIds.size());
}

std::shared_ptr<const SequenceDefinition> SequenceRegistry::getSequence(
    const std::string& sequenceId) const {
    auto it = sequences_.find(sequenceId);
    if (it == sequences_.end() || it->second.empty()) {
        return nullptr;
    }

    // 최신 버전 찾기 (마지막 등록된 버전)
    return std::make_shared<const SequenceDefinition>(it->second.rbegin()->second);
}

std::shared_ptr<const SequenceDefinition> SequenceRegistry::getSequence(
    const std::string& sequenceId,
    const std::string& version) const {
    auto it = sequences_.find(sequenceId);
    if (it == sequences_.end()) {
        return nullptr;
    }

    auto versionIt = it->second.find(version);
    if (versionIt == it->second.end()) {
        return nullptr;
    }

    return std::make_shared<const SequenceDefinition>(versionIt->second);
}

bool SequenceRegistry::hasSequence(const std::string& sequenceId) const {
    auto it = sequences_.find(sequenceId);
    return it != sequences_.end() && !it->second.empty();
}

std::vector<std::string> SequenceRegistry::getVersions(const std::string& sequenceId) const {
    std::vector<std::string> versions;
    auto it = sequences_.find(sequenceId);
    if (it != sequences_.end()) {
        // 역순으로 (최신 버전 먼저)
        for (auto vit = it->second.rbegin(); vit != it->second.rend(); ++vit) {
            versions.push_back(vit->first);
        }
    }
    return versions;
}

std::vector<std::string> SequenceRegistry::getAllSequenceIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : sequences_) {
        if (!pair.second.empty()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

bool SequenceRegistry::removeSequence(const std::string& sequenceId) {
    auto it = sequences_.find(sequenceId);
    if (it == sequences_.end()) {
        return false;
    }

    auto logger = spdlog::get("mxrc");

    sequences_.erase(it);
    logger->info("시퀀스 삭제됨: id={}", sequenceId);
    return true;
}

bool SequenceRegistry::removeSequenceVersion(
    const std::string& sequenceId,
    const std::string& version) {
    auto it = sequences_.find(sequenceId);
    if (it == sequences_.end()) {
        return false;
    }

    auto versionIt = it->second.find(version);
    if (versionIt == it->second.end()) {
        return false;
    }

    auto logger = spdlog::get("mxrc");

    it->second.erase(versionIt);
    logger->info("시퀀스 버전 삭제됨: id={}, version={}", sequenceId, version);

    // ID의 모든 버전이 삭제되었으면 ID도 제거
    if (it->second.empty()) {
        sequences_.erase(it);
    }

    return true;
}

void SequenceRegistry::clear() {
    sequences_.clear();
    auto logger = spdlog::get("mxrc");
    if (logger) {
        logger->info("시퀀스 레지스트리 초기화됨");
    }
}

size_t SequenceRegistry::getSequenceCount() const {
    return sequences_.size();
}

void SequenceRegistry::validateDefinition(const SequenceDefinition& definition) const {
    if (definition.id.empty()) {
        throw std::invalid_argument("Sequence ID cannot be empty");
    }

    if (definition.name.empty()) {
        throw std::invalid_argument("Sequence name cannot be empty");
    }

    if (definition.version.empty()) {
        throw std::invalid_argument("Sequence version cannot be empty");
    }

    if (definition.actionIds.empty()) {
        throw std::invalid_argument("Sequence must contain at least one action");
    }

    // actionIds 내에 중복이 있는지 확인 (선택 사항)
    std::vector<std::string> sortedIds = definition.actionIds;
    std::sort(sortedIds.begin(), sortedIds.end());
    if (std::adjacent_find(sortedIds.begin(), sortedIds.end()) != sortedIds.end()) {
        throw std::invalid_argument("Duplicate action IDs in sequence");
    }
}

bool SequenceRegistry::isVersionGreater(const std::string& v1, const std::string& v2) {
    // 간단한 버전 비교: "1.0.0" vs "1.0.1"
    // 실제 구현에서는 semantic versioning 라이브러리 사용 권장
    return v1 > v2;
}

void SequenceRegistry::registerTemplate(const SequenceTemplate& templateDef) {
    // 템플릿 유효성 검증
    if (templateDef.id.empty()) {
        throw std::invalid_argument("Template ID cannot be empty");
    }

    if (templateDef.name.empty()) {
        throw std::invalid_argument("Template name cannot be empty");
    }

    if (templateDef.actionIds.empty()) {
        throw std::invalid_argument("Template must contain at least one action");
    }

    auto logger = spdlog::get("mxrc");

    // 동일 ID가 이미 등록되어 있는지 확인
    if (templates_.find(templateDef.id) != templates_.end()) {
        if (logger) {
            logger->error("템플릿 이미 등록됨: id={}", templateDef.id);
        }
        throw std::runtime_error("Template already registered: " + templateDef.id);
    }

    // 템플릿 등록
    templates_[templateDef.id] = templateDef;
    templateInstanceMap_[templateDef.id] = {};  // 빈 인스턴스 목록

    if (logger) {
        logger->info(
            "템플릿 등록됨: id={}, name={}, parameters={}, actions={}",
            templateDef.id, templateDef.name, templateDef.parameters.size(),
            templateDef.actionIds.size());
    }
}

std::shared_ptr<const SequenceTemplate> SequenceRegistry::getTemplate(
    const std::string& templateId) const {
    auto it = templates_.find(templateId);
    if (it == templates_.end()) {
        return nullptr;
    }
    return std::make_shared<const SequenceTemplate>(it->second);
}

bool SequenceRegistry::hasTemplate(const std::string& templateId) const {
    return templates_.find(templateId) != templates_.end();
}

std::vector<std::string> SequenceRegistry::getAllTemplateIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : templates_) {
        ids.push_back(pair.first);
    }
    return ids;
}

void SequenceRegistry::saveTemplateInstance(const SequenceTemplateInstance& instance) {
    if (instance.instanceId.empty()) {
        throw std::invalid_argument("Instance ID cannot be empty");
    }

    if (instance.templateId.empty()) {
        throw std::invalid_argument("Template ID cannot be empty");
    }

    auto logger = spdlog::get("mxrc");

    // 템플릿이 존재하는지 확인
    if (templates_.find(instance.templateId) == templates_.end()) {
        if (logger) {
            logger->error("템플릿을 찾을 수 없음: id={}", instance.templateId);
        }
        throw std::runtime_error("Template not found: " + instance.templateId);
    }

    // 인스턴스 저장
    templateInstances_[instance.instanceId] = instance;
    templateInstanceMap_[instance.templateId].push_back(instance.instanceId);

    if (logger) {
        logger->info(
            "템플릿 인스턴스 저장됨: id={}, template={}, name={}",
            instance.instanceId, instance.templateId, instance.instanceName);
    }
}

std::shared_ptr<const SequenceTemplateInstance> SequenceRegistry::getTemplateInstance(
    const std::string& instanceId) const {
    auto it = templateInstances_.find(instanceId);
    if (it == templateInstances_.end()) {
        return nullptr;
    }
    return std::make_shared<const SequenceTemplateInstance>(it->second);
}

std::vector<std::string> SequenceRegistry::getTemplateInstances(
    const std::string& templateId) const {
    auto it = templateInstanceMap_.find(templateId);
    if (it == templateInstanceMap_.end()) {
        return {};
    }
    return it->second;
}

bool SequenceRegistry::removeTemplate(const std::string& templateId) {
    auto it = templates_.find(templateId);
    if (it == templates_.end()) {
        return false;
    }

    auto logger = spdlog::get("mxrc");

    // 템플릿으로부터 생성된 모든 인스턴스 삭제
    auto instancesIt = templateInstanceMap_.find(templateId);
    if (instancesIt != templateInstanceMap_.end()) {
        for (const auto& instanceId : instancesIt->second) {
            templateInstances_.erase(instanceId);
        }
        templateInstanceMap_.erase(instancesIt);
    }

    templates_.erase(it);
    if (logger) {
        logger->info("템플릿 삭제됨: id={}", templateId);
    }
    return true;
}

} // namespace mxrc::core::sequence

