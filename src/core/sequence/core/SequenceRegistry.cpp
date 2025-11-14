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

} // namespace mxrc::core::sequence

