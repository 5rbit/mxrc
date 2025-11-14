#ifndef MXRC_CORE_SEQUENCE_SEQUENCE_REGISTRY_H
#define MXRC_CORE_SEQUENCE_SEQUENCE_REGISTRY_H

#include "core/sequence/dto/SequenceDefinition.h"
#include "core/action/util/Logger.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mxrc::core::sequence {

/**
 * @brief Sequence 정의 레지스트리
 *
 * Sequence 정의를 등록하고 조회하는 중앙 저장소입니다.
 * 스레드 안전성을 보장합니다.
 */
class SequenceRegistry {
public:
    SequenceRegistry() = default;
    ~SequenceRegistry() = default;

    // 복사 및 이동 금지
    SequenceRegistry(const SequenceRegistry&) = delete;
    SequenceRegistry& operator=(const SequenceRegistry&) = delete;
    SequenceRegistry(SequenceRegistry&&) = delete;
    SequenceRegistry& operator=(SequenceRegistry&&) = delete;

    /**
     * @brief Sequence 정의 등록
     *
     * @param definition Sequence 정의
     */
    void registerDefinition(const SequenceDefinition& definition);

    /**
     * @brief Sequence 정의 조회
     *
     * @param id Sequence ID
     * @return Sequence 정의 포인터 (존재하지 않으면 nullptr)
     */
    std::shared_ptr<SequenceDefinition> getDefinition(const std::string& id) const;

    /**
     * @brief Sequence 정의 존재 여부 확인
     *
     * @param id Sequence ID
     * @return 존재하면 true
     */
    bool hasDefinition(const std::string& id) const;

    /**
     * @brief 모든 Sequence ID 조회
     *
     * @return Sequence ID 목록
     */
    std::vector<std::string> getAllDefinitionIds() const;

    /**
     * @brief Sequence 정의 제거
     *
     * @param id Sequence ID
     * @return 제거 성공 여부
     */
    bool removeDefinition(const std::string& id);

    /**
     * @brief 모든 정의 삭제
     */
    void clear();

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<SequenceDefinition>> definitions_;
};

} // namespace mxrc::core::sequence

#endif // MXRC_CORE_SEQUENCE_SEQUENCE_REGISTRY_H
