// PalletShuttleStateAccessor.h - 팔렛 셔틀 상태 접근 구현
// Copyright (C) 2025 MXRC Project

#pragma once

#include "../interfaces/IPalletShuttleStateAccessor.h"
#include <memory>
#include <mutex>

// Forward declarations
class DataStore;
namespace mxrc::core::event {
    class IEventBus;
}

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 셔틀 상태 접근 구현 클래스
 *
 * DataStore를 내부적으로 사용하여 상태를 관리합니다.
 * Feature 022의 Accessor 패턴에 따라 구현되어,
 * 향후 DataStore 구조 변경 시 이 클래스만 수정하면 됩니다.
 *
 * 키 네이밍 규칙:
 * - "pallet_shuttle/position/current" : 현재 위치
 * - "pallet_shuttle/position/target" : 목표 위치
 * - "pallet_shuttle/state" : 운영 상태
 * - "pallet_shuttle/pallet/loaded" : 적재된 팔렛 정보
 * - "pallet_shuttle/metrics/*" : 성능 메트릭
 */
class PalletShuttleStateAccessor : public IPalletShuttleStateAccessor {
public:
    /**
     * @brief 생성자
     * @param data_store DataStore 인스턴스
     * @param event_bus EventBus 인스턴스 (optional, 상태 변경 이벤트 발행용)
     */
    PalletShuttleStateAccessor(
        std::shared_ptr<DataStore> data_store,
        std::shared_ptr<mxrc::core::event::IEventBus> event_bus = nullptr);

    ~PalletShuttleStateAccessor() override = default;

    // ===== 위치 정보 구현 =====

    std::optional<Position> getCurrentPosition() const override;
    std::optional<Position> getTargetPosition() const override;
    bool updatePosition(const Position& position) override;
    bool setTargetPosition(const Position& position) override;

    // ===== 상태 정보 구현 =====

    ShuttleState getState() const override;
    bool setState(ShuttleState state) override;

    // ===== 팔렛 정보 구현 =====

    std::optional<PalletInfo> getLoadedPallet() const override;
    bool updateLoadedPallet(const PalletInfo& pallet) override;
    bool clearLoadedPallet() override;

    // ===== 성능 메트릭 구현 =====

    double getCurrentSpeed() const override;
    double getBatteryLevel() const override;
    double getTotalDistance() const override;
    uint32_t getCompletedTaskCount() const override;
    void incrementCompletedTaskCount() override;

    // ===== 시간 정보 구현 =====

    std::chrono::system_clock::time_point getLastUpdateTime() const override;
    std::optional<std::chrono::system_clock::time_point> getTaskStartTime() const override;
    void setTaskStartTime(const std::chrono::system_clock::time_point& time) override;
    void clearTaskStartTime() override;

private:
    /**
     * @brief 상태 변경 이벤트 발행
     * @param event_type 이벤트 타입
     * @param details 상세 정보
     */
    void publishStateChangeEvent(const std::string& event_type,
                                 const std::string& details);

    /**
     * @brief Position을 map으로 변환
     */
    std::map<std::string, std::any> positionToMap(const Position& pos) const;

    /**
     * @brief map을 Position으로 변환
     */
    std::optional<Position> mapToPosition(const std::any& data) const;

    /**
     * @brief PalletInfo를 map으로 변환
     */
    std::map<std::string, std::any> palletInfoToMap(const PalletInfo& info) const;

    /**
     * @brief map을 PalletInfo로 변환
     */
    std::optional<PalletInfo> mapToPalletInfo(const std::any& data) const;

private:
    std::shared_ptr<DataStore> data_store_;
    std::shared_ptr<mxrc::core::event::IEventBus> event_bus_;

    // Thread safety
    mutable std::mutex mutex_;

    // 키 상수 (typo 방지)
    static constexpr const char* KEY_CURRENT_POSITION = "pallet_shuttle/position/current";
    static constexpr const char* KEY_TARGET_POSITION = "pallet_shuttle/position/target";
    static constexpr const char* KEY_STATE = "pallet_shuttle/state";
    static constexpr const char* KEY_LOADED_PALLET = "pallet_shuttle/pallet/loaded";
    static constexpr const char* KEY_CURRENT_SPEED = "pallet_shuttle/metrics/current_speed";
    static constexpr const char* KEY_BATTERY_LEVEL = "pallet_shuttle/metrics/battery_level";
    static constexpr const char* KEY_TOTAL_DISTANCE = "pallet_shuttle/metrics/total_distance";
    static constexpr const char* KEY_COMPLETED_TASKS = "pallet_shuttle/metrics/completed_tasks";
    static constexpr const char* KEY_LAST_UPDATE_TIME = "pallet_shuttle/time/last_update";
    static constexpr const char* KEY_TASK_START_TIME = "pallet_shuttle/time/task_start";
};

} // namespace mxrc::robot::pallet_shuttle