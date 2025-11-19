#ifndef ACCESS_CONTROL_MANAGER_H
#define ACCESS_CONTROL_MANAGER_H

#include <map>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

namespace mxrc::core::datastore {

/**
 * @brief 데이터 접근 제어 정책 관리를 담당하는 클래스
 *
 * 책임:
 * - 키별, 모듈별 접근 권한 설정 및 제거
 * - 접근 권한 검증 (읽기 병렬성 지원)
 * - 스레드 안전성 보장
 *
 * 설계 원칙:
 * - shared_lock (읽기) / unique_lock (쓰기) 패턴
 * - 기본 정책: 접근 거부 (명시적 허용 필요)
 * - RAII 원칙 준수
 *
 * 자료구조:
 * - std::map<key, map<module_id, can_access>>: 키별 모듈 접근 정책
 */
class AccessControlManager {
public:
    /**
     * @brief 생성자
     */
    AccessControlManager() = default;

    /**
     * @brief 소멸자 (RAII 원칙)
     */
    ~AccessControlManager() = default;

    // 복사 및 이동 금지 (unique ownership)
    AccessControlManager(const AccessControlManager&) = delete;
    AccessControlManager& operator=(const AccessControlManager&) = delete;
    AccessControlManager(AccessControlManager&&) = delete;
    AccessControlManager& operator=(AccessControlManager&&) = delete;

    /**
     * @brief 접근 정책 설정
     * @param key 데이터 키
     * @param module_id 모듈 ID
     * @param can_access 접근 허용 여부 (true: 허용, false: 거부)
     *
     * 시간 복잡도: O(log N)
     * - map insert: O(log N)
     *
     * 스레드 안전: unique_lock으로 보호됨
     */
    void setPolicy(const std::string& key, const std::string& module_id, bool can_access);

    /**
     * @brief 접근 권한 검증
     * @param key 데이터 키
     * @param module_id 모듈 ID
     * @return 접근 허용 여부 (true: 허용, false: 거부)
     *
     * 기본 정책: 정책이 없으면 거부 (false 반환)
     *
     * 시간 복잡도: O(log N)
     * - map find: O(log N)
     *
     * 스레드 안전: shared_lock으로 보호됨 (읽기 병렬성)
     */
    bool hasAccess(const std::string& key, const std::string& module_id) const;

    /**
     * @brief 키별 접근 정책 제거
     * @param key 데이터 키
     *
     * 시간 복잡도: O(log N)
     * - map erase: O(log N)
     *
     * 스레드 안전: unique_lock으로 보호됨
     *
     * @note 존재하지 않는 키 제거 시도는 무시됨 (예외 발생 안 함)
     */
    void removePolicy(const std::string& key);

    /**
     * @brief 특정 키-모듈 조합의 접근 정책 제거
     * @param key 데이터 키
     * @param module_id 모듈 ID
     *
     * 시간 복잡도: O(log N)
     * - map find + erase: O(log N)
     *
     * 스레드 안전: unique_lock으로 보호됨
     *
     * @note 존재하지 않는 정책 제거 시도는 무시됨 (예외 발생 안 함)
     */
    void removePolicy(const std::string& key, const std::string& module_id);

    /**
     * @brief 전체 접근 정책 조회
     * @return 전체 접근 정책 맵 (key -> (module_id -> can_access))
     *
     * 시간 복잡도: O(N)
     * - 전체 맵 복사: O(N)
     *
     * 스레드 안전: shared_lock으로 보호됨 (읽기 병렬성)
     */
    std::map<std::string, std::map<std::string, bool>> getAllPolicies() const;

    /**
     * @brief 특정 키에 대한 접근 정책 존재 여부 확인
     * @param key 데이터 키
     * @return 정책 존재 여부
     *
     * 시간 복잡도: O(log N)
     * - map find: O(log N)
     *
     * 스레드 안전: shared_lock으로 보호됨 (읽기 병렬성)
     */
    bool hasPolicy(const std::string& key) const;

    /**
     * @brief 전체 정책 개수 조회 (테스트용)
     * @return 정책 개수 (키 개수)
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: shared_lock으로 보호됨 (읽기 병렬성)
     */
    size_t getPolicyCount() const;

private:
    /**
     * @brief 접근 정책 저장소
     * - 1차 Key: 데이터 키 (string)
     * - 2차 Key: 모듈 ID (string)
     * - Value: 접근 허용 여부 (bool)
     *
     * 목적: O(log N) 시간 복잡도로 접근 정책 조회 및 설정
     */
    std::map<std::string, std::map<std::string, bool>> access_policies_;

    /**
     * @brief 스레드 안전성을 위한 shared_mutex
     * - access_policies_ 동시 접근 보호
     * - 읽기 병렬성 지원 (shared_lock)
     * - 쓰기 독점 접근 (unique_lock)
     */
    mutable std::shared_mutex mutex_;
};

} // namespace mxrc::core::datastore

#endif // ACCESS_CONTROL_MANAGER_H
