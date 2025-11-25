// IPalletShuttleStateAccessor.h - 팔렛 셔틀 상태 접근 인터페이스
// Copyright (C) 2025 MXRC Project
//
// DataStore Accessor 패턴 적용 (Feature 022 권장사항)
// 직접 DataStore 접근 대신 이 인터페이스를 통해 상태 관리

#pragma once

#include <memory>
#include <string>
#include <optional>
#include <chrono>

namespace mxrc::robot::pallet_shuttle {

/**
 * @brief 팔렛 셔틀 위치 정보
 */
struct Position {
    double x;  ///< X 좌표 (mm)
    double y;  ///< Y 좌표 (mm)
    double z;  ///< Z 좌표 (mm)
    double theta;  ///< 회전 각도 (rad)
};

/**
 * @brief 팔렛 셔틀 운영 상태
 */
enum class ShuttleState {
    IDLE,        ///< 대기 중
    MOVING,      ///< 이동 중
    PICKING,     ///< 팔렛 픽업 중
    PLACING,     ///< 팔렛 배치 중
    ERROR,       ///< 오류 상태
    MAINTENANCE  ///< 정비 모드
};

/**
 * @brief 팔렛 정보
 */
struct PalletInfo {
    std::string pallet_id;  ///< 팔렛 ID
    double weight;          ///< 중량 (kg)
    bool is_loaded;         ///< 적재 여부
};

/**
 * @brief 팔렛 셔틀 상태 접근 인터페이스
 *
 * DataStore에 직접 접근하는 대신 이 인터페이스를 통해
 * 팔렛 셔틀의 상태를 읽고 쓸 수 있습니다.
 *
 * Feature 022의 Accessor 패턴을 따라 구현되어,
 * 향후 DataStore 구조 변경 시에도 비즈니스 로직에
 * 영향을 주지 않습니다.
 */
class IPalletShuttleStateAccessor {
public:
    virtual ~IPalletShuttleStateAccessor() = default;

    // ===== 위치 정보 =====

    /**
     * @brief 현재 위치 조회
     * @return 현재 위치, 없으면 nullopt
     */
    virtual std::optional<Position> getCurrentPosition() const = 0;

    /**
     * @brief 목표 위치 조회
     * @return 목표 위치, 없으면 nullopt
     */
    virtual std::optional<Position> getTargetPosition() const = 0;

    /**
     * @brief 위치 업데이트
     * @param position 새로운 위치
     * @return 성공 여부
     */
    virtual bool updatePosition(const Position& position) = 0;

    /**
     * @brief 목표 위치 설정
     * @param position 목표 위치
     * @return 성공 여부
     */
    virtual bool setTargetPosition(const Position& position) = 0;

    // ===== 상태 정보 =====

    /**
     * @brief 현재 운영 상태 조회
     * @return 현재 상태
     */
    virtual ShuttleState getState() const = 0;

    /**
     * @brief 운영 상태 변경
     * @param state 새로운 상태
     * @return 성공 여부
     */
    virtual bool setState(ShuttleState state) = 0;

    // ===== 팔렛 정보 =====

    /**
     * @brief 현재 적재된 팔렛 정보 조회
     * @return 팔렛 정보, 없으면 nullopt
     */
    virtual std::optional<PalletInfo> getLoadedPallet() const = 0;

    /**
     * @brief 팔렛 적재 상태 업데이트
     * @param pallet 팔렛 정보
     * @return 성공 여부
     */
    virtual bool updateLoadedPallet(const PalletInfo& pallet) = 0;

    /**
     * @brief 팔렛 하역 (적재 상태 해제)
     * @return 성공 여부
     */
    virtual bool clearLoadedPallet() = 0;

    // ===== 성능 메트릭 =====

    /**
     * @brief 현재 속도 조회
     * @return 속도 (mm/s)
     */
    virtual double getCurrentSpeed() const = 0;

    /**
     * @brief 배터리 잔량 조회
     * @return 배터리 잔량 (0.0 ~ 1.0)
     */
    virtual double getBatteryLevel() const = 0;

    /**
     * @brief 총 이동 거리 조회
     * @return 누적 이동 거리 (m)
     */
    virtual double getTotalDistance() const = 0;

    /**
     * @brief 작업 완료 횟수 조회
     * @return 완료된 작업 수
     */
    virtual uint32_t getCompletedTaskCount() const = 0;

    /**
     * @brief 작업 완료 횟수 증가
     */
    virtual void incrementCompletedTaskCount() = 0;

    // ===== 시간 정보 =====

    /**
     * @brief 마지막 업데이트 시간 조회
     * @return 타임스탬프
     */
    virtual std::chrono::system_clock::time_point getLastUpdateTime() const = 0;

    /**
     * @brief 현재 작업 시작 시간 조회
     * @return 타임스탬프, 없으면 nullopt
     */
    virtual std::optional<std::chrono::system_clock::time_point> getTaskStartTime() const = 0;

    /**
     * @brief 작업 시작 시간 설정
     * @param time 시작 시간
     */
    virtual void setTaskStartTime(const std::chrono::system_clock::time_point& time) = 0;

    /**
     * @brief 작업 시작 시간 초기화
     */
    virtual void clearTaskStartTime() = 0;
};

} // namespace mxrc::robot::pallet_shuttle