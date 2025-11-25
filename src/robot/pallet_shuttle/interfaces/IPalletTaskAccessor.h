// IPalletTaskAccessor.h - 팔렛 작업 정보 접근 인터페이스
// Copyright (C) 2025 MXRC Project
//
// DataStore Accessor 패턴 적용 (Feature 022 권장사항)
// 작업 큐 및 작업 상태 관리 인터페이스

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 운반 작업 정보
 */
struct PalletTransportTask {
    std::string task_id;           ///< 작업 ID
    std::string pallet_id;         ///< 팔렛 ID

    // 픽업 위치
    double pickup_x;               ///< 픽업 X 좌표 (mm)
    double pickup_y;               ///< 픽업 Y 좌표 (mm)
    double pickup_z;               ///< 픽업 Z 좌표 (mm)

    // 배치 위치
    double place_x;                ///< 배치 X 좌표 (mm)
    double place_y;                ///< 배치 Y 좌표 (mm)
    double place_z;                ///< 배치 Z 좌표 (mm)

    // 우선순위 및 제약
    uint32_t priority;             ///< 우선순위 (낮을수록 높은 우선순위)
    std::chrono::system_clock::time_point deadline;  ///< 마감 시간

    // 상태 정보
    enum class Status {
        PENDING,    ///< 대기 중
        EXECUTING,  ///< 실행 중
        COMPLETED,  ///< 완료
        FAILED,     ///< 실패
        CANCELLED   ///< 취소됨
    } status = Status::PENDING;

    std::string error_message;    ///< 오류 메시지 (실패 시)
    double progress = 0.0;         ///< 진행률 (0.0 ~ 1.0)

    // 시간 정보
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
};

/**
 * @brief 작업 통계 정보
 */
struct TaskStatistics {
    uint32_t total_tasks = 0;       ///< 총 작업 수
    uint32_t completed_tasks = 0;   ///< 완료된 작업 수
    uint32_t failed_tasks = 0;      ///< 실패한 작업 수
    uint32_t cancelled_tasks = 0;   ///< 취소된 작업 수
    double average_completion_time_sec = 0.0;  ///< 평균 완료 시간 (초)
    double success_rate = 0.0;      ///< 성공률 (0.0 ~ 1.0)
};

/**
 * @brief 팔렛 작업 정보 접근 인터페이스
 *
 * 작업 큐 관리, 작업 상태 추적, 통계 정보 제공
 *
 * Feature 022의 Accessor 패턴을 따라 구현되어,
 * DataStore 직접 접근을 방지하고 추상화를 제공합니다.
 */
class IPalletTaskAccessor {
public:
    virtual ~IPalletTaskAccessor() = default;

    // ===== 작업 큐 관리 =====

    /**
     * @brief 새 작업 추가
     * @param task 작업 정보
     * @return 성공 여부
     */
    virtual bool addTask(const PalletTransportTask& task) = 0;

    /**
     * @brief 다음 작업 가져오기 (우선순위 기반)
     * @return 다음 작업, 없으면 nullopt
     */
    virtual std::optional<PalletTransportTask> getNextTask() = 0;

    /**
     * @brief 현재 실행 중인 작업 조회
     * @return 실행 중인 작업, 없으면 nullopt
     */
    virtual std::optional<PalletTransportTask> getCurrentTask() const = 0;

    /**
     * @brief 대기 중인 작업 목록 조회
     * @param limit 최대 개수 (0 = 무제한)
     * @return 대기 중인 작업 목록
     */
    virtual std::vector<PalletTransportTask> getPendingTasks(size_t limit = 0) const = 0;

    /**
     * @brief 대기 중인 작업 수 조회
     * @return 대기 작업 수
     */
    virtual size_t getPendingTaskCount() const = 0;

    // ===== 작업 상태 관리 =====

    /**
     * @brief 작업 상태 업데이트
     * @param task_id 작업 ID
     * @param status 새로운 상태
     * @return 성공 여부
     */
    virtual bool updateTaskStatus(const std::string& task_id,
                                  PalletTransportTask::Status status) = 0;

    /**
     * @brief 작업 진행률 업데이트
     * @param task_id 작업 ID
     * @param progress 진행률 (0.0 ~ 1.0)
     * @return 성공 여부
     */
    virtual bool updateTaskProgress(const std::string& task_id,
                                    double progress) = 0;

    /**
     * @brief 작업 오류 설정
     * @param task_id 작업 ID
     * @param error_message 오류 메시지
     * @return 성공 여부
     */
    virtual bool setTaskError(const std::string& task_id,
                              const std::string& error_message) = 0;

    /**
     * @brief 작업 취소
     * @param task_id 작업 ID
     * @return 성공 여부
     */
    virtual bool cancelTask(const std::string& task_id) = 0;

    /**
     * @brief 모든 대기 작업 취소
     * @return 취소된 작업 수
     */
    virtual size_t cancelAllPendingTasks() = 0;

    // ===== 작업 조회 =====

    /**
     * @brief 특정 작업 조회
     * @param task_id 작업 ID
     * @return 작업 정보, 없으면 nullopt
     */
    virtual std::optional<PalletTransportTask> getTask(const std::string& task_id) const = 0;

    /**
     * @brief 완료된 작업 이력 조회
     * @param limit 최대 개수 (0 = 무제한)
     * @return 완료된 작업 목록
     */
    virtual std::vector<PalletTransportTask> getCompletedTasks(size_t limit = 0) const = 0;

    /**
     * @brief 실패한 작업 이력 조회
     * @param limit 최대 개수 (0 = 무제한)
     * @return 실패한 작업 목록
     */
    virtual std::vector<PalletTransportTask> getFailedTasks(size_t limit = 0) const = 0;

    // ===== 통계 정보 =====

    /**
     * @brief 작업 통계 조회
     * @return 통계 정보
     */
    virtual TaskStatistics getStatistics() const = 0;

    /**
     * @brief 통계 초기화
     */
    virtual void resetStatistics() = 0;

    // ===== 우선순위 관리 =====

    /**
     * @brief 작업 우선순위 변경
     * @param task_id 작업 ID
     * @param new_priority 새로운 우선순위
     * @return 성공 여부
     */
    virtual bool updateTaskPriority(const std::string& task_id,
                                   uint32_t new_priority) = 0;

    /**
     * @brief 긴급 작업으로 승격
     * @param task_id 작업 ID
     * @return 성공 여부
     */
    virtual bool promoteToUrgent(const std::string& task_id) = 0;
};

} // namespace mxrc::robot::pallet_shuttle