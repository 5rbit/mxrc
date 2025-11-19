#ifndef EXPIRATION_MANAGER_H
#define EXPIRATION_MANAGER_H

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <list>

namespace mxrc::core::datastore {

/**
 * @brief 데이터 만료 정책 관리 및 만료 데이터 정리를 담당하는 클래스
 *
 * 책임:
 * - 키별 만료 시간 정책 적용 및 제거 (TTL)
 * - LRU (Least Recently Used) 정책 관리
 * - 만료된 키 목록 조회 (O(log N) 성능)
 * - 스레드 안전성 보장
 *
 * 성능 목표:
 * - 100개 데이터: <0.1ms
 * - 1,000개 데이터: <1ms
 * - 10,000개 데이터: <10ms
 *
 * 자료구조 (TTL):
 * - std::map<timestamp, set<key>>: 만료 시간 순서로 키 정렬 (O(log N) 검색)
 * - std::unordered_map<key, timestamp>: 키별 만료 시간 조회 (O(1) 검색)
 *
 * 자료구조 (LRU):
 * - std::list<key>: 접근 순서 추적 (MRU at front, LRU at back)
 * - std::unordered_map<key, list::iterator>: O(1) 리스트 노드 접근
 * - size_t max_lru_capacity_: LRU 용량 제한 (기본: 1000)
 *
 * LRU 동작:
 * - applyLRUPolicy(): 키를 LRU 추적 대상으로 등록
 * - recordAccess(): 키 접근 시 MRU 위치로 이동 (O(1))
 * - getExpiredKeysLRU(): 용량 초과 시 LRU 키 반환 (O(K), K=제거 개수)
 */
class ExpirationManager {
public:
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

    /**
     * @brief 생성자
     */
    ExpirationManager() = default;

    /**
     * @brief 소멸자 (RAII 원칙)
     */
    ~ExpirationManager() = default;

    // 복사 및 이동 금지 (unique ownership)
    ExpirationManager(const ExpirationManager&) = delete;
    ExpirationManager& operator=(const ExpirationManager&) = delete;
    ExpirationManager(ExpirationManager&&) = delete;
    ExpirationManager& operator=(ExpirationManager&&) = delete;

    /**
     * @brief 만료 정책 적용
     * @param key 데이터 키
     * @param expiration_time 만료 시간
     *
     * 시간 복잡도: O(log N)
     * - map insert: O(log N)
     * - set insert: O(log M), M = 동일 시간에 만료되는 키 개수
     *
     * 스레드 안전: mutex로 보호됨
     */
    void applyPolicy(const std::string& key, const TimePoint& expiration_time);

    /**
     * @brief 만료 정책 제거
     * @param key 데이터 키
     *
     * 시간 복잡도: O(log N)
     * - map erase: O(log N)
     * - set erase: O(log M)
     *
     * 스레드 안전: mutex로 보호됨
     *
     * @note 존재하지 않는 키 제거 시도는 무시됨 (예외 발생 안 함)
     */
    void removePolicy(const std::string& key);

    /**
     * @brief 만료된 키 목록 조회
     * @return 만료된 키 목록 (벡터)
     *
     * 시간 복잡도: O(log N + K)
     * - lower_bound: O(log N)
     * - K개 만료 키 수집: O(K)
     *
     * 성능 목표:
     * - 100개 데이터: <100 microseconds
     * - 1,000개 데이터: <1000 microseconds
     * - 10,000개 데이터: <10000 microseconds
     *
     * 스레드 안전: mutex로 보호됨
     */
    std::vector<std::string> getExpiredKeys() const;

    /**
     * @brief 만료 정책 존재 여부 확인
     * @param key 데이터 키
     * @return 만료 정책 존재 여부
     *
     * 시간 복잡도: O(1) (unordered_map)
     * 스레드 안전: mutex로 보호됨
     */
    bool hasPolicy(const std::string& key) const;

    /**
     * @brief 만료 시간 조회
     * @param key 데이터 키
     * @return 만료 시간
     * @throws std::out_of_range 키가 존재하지 않을 경우
     *
     * 시간 복잡도: O(1) (unordered_map)
     * 스레드 안전: mutex로 보호됨
     */
    TimePoint getExpirationTime(const std::string& key) const;

    /**
     * @brief 전체 정책 개수 조회 (테스트용)
     * @return 정책 개수
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: mutex로 보호됨
     */
    size_t getPolicyCount() const;

    // ===================================================================
    // LRU (Least Recently Used) 정책 메서드
    // ===================================================================

    /**
     * @brief LRU 정책 적용 (키를 LRU 추적 대상으로 등록)
     * @param key 데이터 키
     * @param capacity LRU 용량 (0이면 기본값 사용)
     *
     * 시간 복잡도: O(1)
     * - list push_front: O(1)
     * - unordered_map insert: O(1)
     *
     * 스레드 안전: mutex로 보호됨
     *
     * @note 이미 LRU 추적 중인 키는 무시됨
     */
    void applyLRUPolicy(const std::string& key, size_t capacity = 0);

    /**
     * @brief 키 접근 기록 (MRU 위치로 이동)
     * @param key 데이터 키
     *
     * 시간 복잡도: O(1)
     * - list splice: O(1)
     *
     * 스레드 안전: mutex로 보호됨
     *
     * @note LRU 추적 중이 아닌 키는 무시됨
     */
    void recordAccess(const std::string& key);

    /**
     * @brief LRU 용량 초과로 제거할 키 목록 조회
     * @return 제거 대상 키 목록 (LRU부터 순서대로)
     *
     * 시간 복잡도: O(K), K = 제거할 키 개수
     *
     * 스레드 안전: mutex로 보호됨
     *
     * @note 내부적으로 제거된 키는 LRU 추적에서도 제거됨
     */
    std::vector<std::string> getExpiredKeysLRU();

    /**
     * @brief LRU 정책 제거 (키를 LRU 추적에서 제외)
     * @param key 데이터 키
     *
     * 시간 복잡도: O(1)
     * - list erase: O(1) (iterator 있을 때)
     * - unordered_map erase: O(1)
     *
     * 스레드 안전: mutex로 보호됨
     *
     * @note 존재하지 않는 키 제거 시도는 무시됨
     */
    void removeLRUPolicy(const std::string& key);

    /**
     * @brief LRU 추적 여부 확인
     * @param key 데이터 키
     * @return LRU 추적 여부
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: mutex로 보호됨
     */
    bool hasLRUPolicy(const std::string& key) const;

    /**
     * @brief 현재 LRU 용량 조회
     * @return LRU 용량
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: mutex로 보호됨
     */
    size_t getLRUCapacity() const;

    /**
     * @brief 현재 LRU 추적 중인 키 개수 조회
     * @return LRU 추적 키 개수
     *
     * 시간 복잡도: O(1)
     * 스레드 안전: mutex로 보호됨
     */
    size_t getLRUSize() const;

private:
    /**
     * @brief 만료 시간 순서로 키를 정렬하는 맵
     * - Key: 만료 시간 (TimePoint)
     * - Value: 해당 시간에 만료되는 키 집합 (set<string>)
     *
     * 목적: O(log N) 시간 복잡도로 만료된 키 검색
     */
    std::map<TimePoint, std::set<std::string>> expiration_map_;

    /**
     * @brief 키별 만료 시간 조회를 위한 맵
     * - Key: 데이터 키 (string)
     * - Value: 만료 시간 (TimePoint)
     *
     * 목적: O(1) 시간 복잡도로 키의 만료 시간 조회
     */
    std::unordered_map<std::string, TimePoint> key_to_expiration_;

    /**
     * @brief 스레드 안전성을 위한 뮤텍스
     * - expiration_map_ 및 key_to_expiration_ 동시 접근 보호
     * - LRU 자료구조 동시 접근 보호
     */
    mutable std::mutex mutex_;

    // ===================================================================
    // LRU (Least Recently Used) 정책 관련 멤버 변수
    // ===================================================================

    /**
     * @brief LRU 접근 순서를 추적하는 리스트
     * - Front: Most Recently Used (MRU)
     * - Back: Least Recently Used (LRU)
     *
     * 목적: O(1) 시간 복잡도로 접근 순서 업데이트
     */
    std::list<std::string> lru_list_;

    /**
     * @brief 키별 리스트 노드 조회를 위한 맵
     * - Key: 데이터 키 (string)
     * - Value: lru_list_의 iterator
     *
     * 목적: O(1) 시간 복잡도로 리스트 노드 접근 및 이동
     */
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;

    /**
     * @brief LRU 최대 용량 (기본값: 1000)
     * - 용량 초과 시 getExpiredKeysLRU()가 LRU 키 반환
     */
    size_t max_lru_capacity_ = 1000;
};

} // namespace mxrc::core::datastore

#endif // EXPIRATION_MANAGER_H
