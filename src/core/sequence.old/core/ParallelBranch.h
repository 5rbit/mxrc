#pragma once

#include <string>
#include <vector>
#include <memory>

namespace mxrc::core::sequence {

/**
 * @brief 병렬 분기 정의
 *
 * 여러 Action 그룹을 병렬로 실행합니다.
 * 모든 분기가 완료될 때까지 대기한 후 다음으로 진행합니다.
 *
 * 예시:
 * ParallelBranch parallel;
 * parallel.id = "setup";
 * parallel.branches = {
 *     {"move_arm", "calibrate_arm"},      // 첫 번째 병렬 작업
 *     {"move_legs"},                       // 두 번째 병렬 작업
 *     {"open_gripper", "check_gripper"}   // 세 번째 병렬 작업
 * };
 *
 * 실행 흐름:
 * 1. 각 branch의 모든 action이 병렬로 시작
 * 2. 각 branch 내에서는 순차 실행
 * 3. 모든 branch 완료 대기
 * 4. 다음 sequence item으로 진행
 */
struct ParallelBranch {
    /**
     * @brief 병렬 분기의 고유 ID
     */
    std::string id;

    /**
     * @brief 병렬로 실행할 Action 그룹 목록
     *
     * 각 요소는 순차적으로 실행될 Action ID 목록입니다.
     * 예: [["action_1a", "action_1b"], ["action_2a"], ["action_3a", "action_3b"]]
     *
     * 세 개 그룹이 병렬로 시작되며:
     * - Group 1: action_1a → action_1b (순차)
     * - Group 2: action_2a
     * - Group 3: action_3a → action_3b (순차)
     * 모두 완료 후 다음으로 진행
     */
    std::vector<std::vector<std::string>> branches;

    /**
     * @brief 병렬 분기 설명
     */
    std::string description;

    ParallelBranch() = default;

    ParallelBranch(
        const std::string& id,
        const std::vector<std::vector<std::string>>& branches)
        : id(id), branches(branches) {}

    ParallelBranch(
        const std::string& id,
        const std::vector<std::vector<std::string>>& branches,
        const std::string& description)
        : id(id), branches(branches), description(description) {}
};

/**
 * @brief 병렬 실행 결과
 *
 * 각 병렬 분기의 실행 결과를 나타냅니다.
 */
struct ParallelExecutionResult {
    /**
     * @brief 각 분기가 성공했는지 여부
     *
     * 하나라도 실패하면 전체 병렬 실행은 실패
     */
    std::vector<bool> branchResults;

    /**
     * @brief 모든 분기 완료 시간 (ms)
     */
    long long totalTime;

    /**
     * @brief 각 분기별 실행 시간 (ms)
     */
    std::vector<long long> branchTimes;

    ParallelExecutionResult() = default;

    /**
     * @brief 모든 분기 성공 여부
     */
    bool allSuccess() const {
        for (bool result : branchResults) {
            if (!result) return false;
        }
        return true;
    }

    /**
     * @brief 실패한 분기 개수
     */
    size_t failureCount() const {
        size_t count = 0;
        for (bool result : branchResults) {
            if (!result) ++count;
        }
        return count;
    }
};

} // namespace mxrc::core::sequence

