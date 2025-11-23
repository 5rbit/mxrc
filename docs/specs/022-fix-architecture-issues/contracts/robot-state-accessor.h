/**
 * @file robot-state-accessor.h
 * @brief IRobotStateAccessor 인터페이스 정의 (문서화용 스니펫)
 *
 * Feature: 022-fix-architecture-issues
 * Phase: 1 (Design)
 * Date: 2025-01-22
 *
 * 본 파일은 실제 구현 코드가 아닌, 설계 문서화를 위한 헤더 스니펫입니다.
 * 실제 구현은 src/core/datastore/interfaces/IRobotStateAccessor.h에 위치합니다.
 */

#pragma once

#include "data-accessor-interface.h"
#include "versioned-data.h"
#include <Eigen/Dense>
#include <vector>

namespace mxrc {
namespace datastore {

/**
 * @brief 제어 모드 enum
 */
enum class ControlMode {
    POSITION = 0,  ///< 위치 제어
    VELOCITY = 1,  ///< 속도 제어
    FORCE = 2,     ///< 힘 제어
    IDLE = 3       ///< 유휴 상태
};

/**
 * @brief 로봇 상태 도메인(robot_state.*)에 대한 타입 안전 접근 인터페이스
 *
 * DataStore의 robot_state.* 키에 대한 읽기/쓰기 작업을 제공합니다.
 * 로봇의 위치, 속도, 관절 각도 등 제어 상태를 관리합니다.
 *
 * 접근 가능 키:
 * - robot_state.position (로봇 엔드이펙터 위치, m)
 * - robot_state.velocity (로봇 속도, m/s)
 * - robot_state.joint_angles (6축 관절 각도, rad)
 * - robot_state.control_mode (제어 모드, enum)
 *
 * 접근 권한:
 * - 읽기: RT 경로 + Non-RT 경로
 * - 쓰기: RT 경로 (Control Loop만)
 *
 * 사용 예시 (RT 경로):
 * @code
 * void ControlLoop::updateRobotState(const Eigen::Vector3d& pos) {
 *     robotAccessor->setPosition(pos);
 *     // ✅ RT 안전: 인라인 함수, lock-free atomic increment
 * }
 * @endcode
 *
 * 사용 예시 (Non-RT 경로, 버전 일관성 체크):
 * @code
 * void Monitor::readConsistentState() {
 *     auto pos1 = robotAccessor->getPosition();
 *     auto vel = robotAccessor->getVelocity();
 *     auto pos2 = robotAccessor->getPosition();
 *
 *     if (!pos1.isConsistentWith(pos2)) {
 *         // 버전 불일치 → 재시도
 *         return readConsistentState();
 *     }
 *
 *     processState(pos1.value, vel.value);
 * }
 * @endcode
 *
 * @see IDataAccessor
 * @see VersionedData
 */
class IRobotStateAccessor : public IDataAccessor {
public:
    /**
     * @brief 가상 소멸자
     */
    virtual ~IRobotStateAccessor() = default;

    /**
     * @brief 도메인 이름 반환
     * @return "robot_state"
     */
    std::string getDomain() const override { return "robot_state"; }

    // ==================== 읽기 메서드 (const) ====================

    /**
     * @brief 로봇 엔드이펙터 위치 읽기
     *
     * @return VersionedData<Eigen::Vector3d> (위치 [x, y, z], m)
     *
     * @note RT 안전: 인라인 함수, lock-free 읽기
     * @note 지연 시간: < 80ns (목표, Eigen::Vector3d 복사 포함)
     *
     * 사용 예시:
     * @code
     * auto pos = accessor->getPosition();
     * Eigen::Vector3d position_vec = pos.value;
     * spdlog::info("Position: [{}, {}, {}]", position_vec.x(), position_vec.y(), position_vec.z());
     * @endcode
     */
    virtual VersionedData<Eigen::Vector3d> getPosition() const = 0;

    /**
     * @brief 로봇 속도 읽기
     *
     * @return VersionedData<Eigen::Vector3d> (속도 [vx, vy, vz], m/s)
     */
    virtual VersionedData<Eigen::Vector3d> getVelocity() const = 0;

    /**
     * @brief 관절 각도 읽기
     *
     * @return VersionedData<std::vector<double>> (6축 관절 각도, rad)
     *
     * @note 벡터는 사전 할당됨 (6개 요소, RT 경로 메모리 할당 금지)
     */
    virtual VersionedData<std::vector<double>> getJointAngles() const = 0;

    /**
     * @brief 제어 모드 읽기
     *
     * @return VersionedData<ControlMode> (제어 모드 enum)
     */
    virtual VersionedData<ControlMode> getControlMode() const = 0;

    // ==================== 쓰기 메서드 (RT 경로 전용) ====================

    /**
     * @brief 로봇 엔드이펙터 위치 쓰기
     *
     * @param value 위치 벡터 (m)
     *
     * @warning RT 경로에서만 호출 가능 (Non-RT 경로에서 호출 금지)
     * @warning Control Loop만 호출해야 함 (다른 모듈 금지)
     *
     * 사용 예시:
     * @code
     * void ControlLoop::updatePosition() {
     *     Eigen::Vector3d new_pos = calculateNewPosition();
     *     accessor->setPosition(new_pos);
     *     // ✅ RT 안전: 인라인 함수, atomic increment
     * }
     * @endcode
     */
    virtual void setPosition(const Eigen::Vector3d& value) = 0;

    /**
     * @brief 로봇 속도 쓰기
     *
     * @param value 속도 벡터 (m/s)
     */
    virtual void setVelocity(const Eigen::Vector3d& value) = 0;

    /**
     * @brief 관절 각도 쓰기
     *
     * @param value 6축 관절 각도 (rad)
     *
     * @pre value.size() == 6 (6축 로봇 가정)
     */
    virtual void setJointAngles(const std::vector<double>& value) = 0;

    /**
     * @brief 제어 모드 쓰기
     *
     * @param value 제어 모드 enum
     *
     * @note 제어 모드 변경 시 CRITICAL 이벤트 발행 권장
     */
    virtual void setControlMode(ControlMode value) = 0;

protected:
    /**
     * @brief 기본 생성자 (protected)
     */
    IRobotStateAccessor() = default;
};

}  // namespace datastore
}  // namespace mxrc
