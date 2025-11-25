#pragma once

#include "core/task/interfaces/ITask.h"
#include "core/task/dto/TaskStatus.h"
#include "core/task/dto/TaskDefinition.h"
#include "../sequences/PalletTransportSequence.h"
#include <string>
#include <atomic>
#include <memory>

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 운반 Task
 *
 * PalletTransportSequence를 실행하는 Task입니다.
 *
 * Feature 016: Pallet Shuttle Control System
 * Phase 5: User Story 1 - 팔렛 픽업 및 배치
 */
class PalletTransportTask : public mxrc::core::task::ITask {
public:
    /**
     * @brief 생성자
     *
     * @param task_id Task ID
     * @param pickup_x 픽업 위치 X
     * @param pickup_y 픽업 위치 Y
     * @param place_x 배치 위치 X
     * @param place_y 배치 위치 Y
     * @param pallet_id 팔렛 ID
     */
    PalletTransportTask(
        const std::string& task_id,
        double pickup_x,
        double pickup_y,
        double place_x,
        double place_y,
        const std::string& pallet_id
    );

    ~PalletTransportTask() override = default;

    // ITask 인터페이스 구현
    std::string getId() const override;
    std::string start() override;
    void stop() override;
    void pause() override;
    void resume() override;
    mxrc::core::task::TaskStatus getStatus() const override;
    float getProgress() const override;
    const mxrc::core::task::TaskDefinition& getDefinition() const override;

private:
    std::string task_id_;
    std::unique_ptr<PalletTransportSequence> sequence_;
    mxrc::core::task::TaskDefinition definition_;
    std::atomic<mxrc::core::task::TaskStatus> status_{mxrc::core::task::TaskStatus::IDLE};
    std::atomic<float> progress_{0.0f};
    std::string execution_id_;
};

} // namespace mxrc::robot::pallet_shuttle
