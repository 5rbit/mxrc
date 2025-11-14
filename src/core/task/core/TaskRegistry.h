#ifndef MXRC_CORE_TASK_TASK_REGISTRY_H
#define MXRC_CORE_TASK_TASK_REGISTRY_H

#include "core/task/dto/TaskDefinition.h"
#include "core/action/util/Logger.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mxrc::core::task {

/**
 * @brief Task 정의 레지스트리
 *
 * Task 정의를 등록하고 조회하는 중앙 저장소입니다.
 */
class TaskRegistry {
public:
    TaskRegistry() = default;
    ~TaskRegistry() = default;

    // 복사 및 이동 금지
    TaskRegistry(const TaskRegistry&) = delete;
    TaskRegistry& operator=(const TaskRegistry&) = delete;
    TaskRegistry(TaskRegistry&&) = delete;
    TaskRegistry& operator=(TaskRegistry&&) = delete;

    /**
     * @brief Task 정의 등록
     */
    void registerDefinition(const TaskDefinition& definition);

    /**
     * @brief Task 정의 조회
     */
    std::shared_ptr<TaskDefinition> getDefinition(const std::string& id) const;

    /**
     * @brief Task 정의 존재 여부 확인
     */
    bool hasDefinition(const std::string& id) const;

    /**
     * @brief 모든 Task ID 조회
     */
    std::vector<std::string> getAllDefinitionIds() const;

    /**
     * @brief Task 정의 제거
     */
    bool removeDefinition(const std::string& id);

    /**
     * @brief 모든 정의 삭제
     */
    void clear();

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<TaskDefinition>> definitions_;
};

} // namespace mxrc::core::task

#endif // MXRC_CORE_TASK_TASK_REGISTRY_H
