/**
 * @file data-accessor-interface.h
 * @brief IDataAccessor 인터페이스 정의 (문서화용 스니펫)
 *
 * Feature: 022-fix-architecture-issues
 * Phase: 1 (Design)
 * Date: 2025-01-22
 *
 * 본 파일은 실제 구현 코드가 아닌, 설계 문서화를 위한 헤더 스니펫입니다.
 * 실제 구현은 src/core/datastore/interfaces/IDataAccessor.h에 위치합니다.
 */

#pragma once

#include <string>

namespace mxrc {
namespace datastore {

/**
 * @brief 모든 도메인별 Accessor의 기본 인터페이스
 *
 * DataStore의 특정 도메인(sensor, robot_state, task_status 등)에 대한
 * 타입 안전 접근을 제공하는 Accessor 클래스의 기본 인터페이스입니다.
 *
 * 설계 원칙:
 * - 순수 가상 인터페이스 (구현 없음)
 * - I-prefix 네이밍 규칙 준수 (Principle II)
 * - 도메인별 Accessor가 상속하여 구현
 * - DataStore 참조를 보유하되 소유권은 갖지 않음 (RAII 준수)
 *
 * 사용 예시:
 * @code
 * class SensorDataAccessor : public IDataAccessor {
 * public:
 *     std::string getDomain() const override {
 *         return "sensor";
 *     }
 * };
 * @endcode
 *
 * @see ISensorDataAccessor
 * @see IRobotStateAccessor
 * @see ITaskStatusAccessor
 */
class IDataAccessor {
public:
    /**
     * @brief 가상 소멸자 (다형성 지원)
     */
    virtual ~IDataAccessor() = default;

    /**
     * @brief Accessor가 접근 가능한 도메인 이름 반환
     *
     * DataStore의 키 네임스페이스를 식별합니다.
     * 예: "sensor", "robot_state", "task_status"
     *
     * @return 도메인 이름 문자열
     *
     * @note 이 메서드는 디버깅 및 로깅 목적으로 사용됩니다.
     * @note 성능에 민감하지 않은 경로에서만 호출해야 합니다.
     *
     * 사용 예시:
     * @code
     * auto accessor = std::make_unique<SensorDataAccessor>(datastore);
     * spdlog::info("Accessor domain: {}", accessor->getDomain());
     * // Output: "Accessor domain: sensor"
     * @endcode
     */
    virtual std::string getDomain() const = 0;

protected:
    /**
     * @brief 기본 생성자 (protected)
     *
     * 추상 인터페이스이므로 직접 인스턴스화 불가.
     * 상속받은 클래스에서만 생성자 호출 가능.
     */
    IDataAccessor() = default;

    /**
     * @brief 복사 생성자 삭제
     *
     * Accessor는 DataStore 참조를 보유하므로 복사 불가.
     * move semantics만 허용.
     */
    IDataAccessor(const IDataAccessor&) = delete;

    /**
     * @brief 복사 대입 연산자 삭제
     */
    IDataAccessor& operator=(const IDataAccessor&) = delete;

    /**
     * @brief 이동 생성자 허용 (default)
     */
    IDataAccessor(IDataAccessor&&) = default;

    /**
     * @brief 이동 대입 연산자 허용 (default)
     */
    IDataAccessor& operator=(IDataAccessor&&) = default;
};

}  // namespace datastore
}  // namespace mxrc
