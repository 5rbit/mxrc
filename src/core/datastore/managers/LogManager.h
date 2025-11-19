#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <deque>

namespace mxrc::core::datastore {

/**
 * @brief 접근 로그 엔트리 구조체
 */
struct AccessLogEntry {
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::string operation;  // "get", "set", "poll"
    std::string key;
    std::string module_id;

    /**
     * @brief 로그 엔트리를 문자열로 변환
     */
    std::string toString() const;
};

/**
 * @brief 에러 로그 엔트리 구조체
 */
struct ErrorLogEntry {
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::string error_type;  // "not_found", "type_mismatch", "access_denied"
    std::string message;
    std::string context;  // 추가 컨텍스트 정보

    /**
     * @brief 로그 엔트리를 문자열로 변환
     */
    std::string toString() const;
};

/**
 * @brief DataStore 로그 관리 클래스
 *
 * 책임:
 * - 접근 로그 수집 (get, set, poll 연산)
 * - 에러 로그 수집 (예외, 권한 거부 등)
 * - 순환 버퍼로 메모리 사용량 제한
 * - 스레드 안전한 로그 접근
 *
 * 특징:
 * - 순환 버퍼: 최대 크기 초과 시 오래된 로그 자동 삭제
 * - std::mutex로 스레드 안전성 보장
 * - 최소 성능 오버헤드 (<1%)
 *
 * 성능 목표:
 * - 로그 추가: O(1) amortized
 * - 로그 조회: O(n) where n = log count
 * - 메모리: O(max_size)
 */
class LogManager {
public:
    /**
     * @brief 생성자
     * @param max_access_logs 최대 접근 로그 개수 (기본값: 1000)
     * @param max_error_logs 최대 에러 로그 개수 (기본값: 1000)
     */
    explicit LogManager(size_t max_access_logs = 1000, size_t max_error_logs = 1000);

    /**
     * @brief 소멸자 (RAII 원칙)
     */
    ~LogManager() = default;

    // 복사 및 이동 금지 (unique ownership)
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    /**
     * @brief 접근 로그 기록
     * @param operation 연산 타입 ("get", "set", "poll")
     * @param key 데이터 키
     * @param module_id 모듈 식별자 (옵션)
     *
     * 시간 복잡도: O(1) amortized
     * 스레드 안전: mutex로 보호
     */
    void logAccess(const std::string& operation,
                   const std::string& key,
                   const std::string& module_id = "");

    /**
     * @brief 에러 로그 기록
     * @param error_type 에러 타입 ("not_found", "type_mismatch", "access_denied")
     * @param message 에러 메시지
     * @param context 추가 컨텍스트 정보 (옵션)
     *
     * 시간 복잡도: O(1) amortized
     * 스레드 안전: mutex로 보호
     */
    void logError(const std::string& error_type,
                  const std::string& message,
                  const std::string& context = "");

    /**
     * @brief 모든 접근 로그 조회
     * @return 접근 로그 문자열 벡터 (시간 순서대로)
     *
     * 시간 복잡도: O(n) where n = log count
     * 스레드 안전: mutex로 보호
     */
    std::vector<std::string> getAccessLogs() const;

    /**
     * @brief 모든 에러 로그 조회
     * @return 에러 로그 문자열 벡터 (시간 순서대로)
     *
     * 시간 복잡도: O(n) where n = log count
     * 스레드 안전: mutex로 보호
     */
    std::vector<std::string> getErrorLogs() const;

    /**
     * @brief 모든 로그 삭제
     *
     * 시간 복잡도: O(n) where n = total log count
     * 스레드 안전: mutex로 보호
     */
    void clear();

    /**
     * @brief 현재 접근 로그 개수 조회
     */
    size_t getAccessLogCount() const;

    /**
     * @brief 현재 에러 로그 개수 조회
     */
    size_t getErrorLogCount() const;

private:
    /**
     * @brief 접근 로그 순환 버퍼
     * - std::deque 사용으로 앞/뒤 삽입 O(1)
     * - 오래된 로그는 앞에서 제거
     */
    std::deque<AccessLogEntry> access_logs_;

    /**
     * @brief 에러 로그 순환 버퍼
     */
    std::deque<ErrorLogEntry> error_logs_;

    /**
     * @brief 최대 접근 로그 개수
     */
    const size_t max_access_logs_;

    /**
     * @brief 최대 에러 로그 개수
     */
    const size_t max_error_logs_;

    /**
     * @brief 로그 보호용 뮤텍스
     * - mutable로 const 메서드에서도 락 사용 가능
     */
    mutable std::mutex mutex_;
};

} // namespace mxrc::core::datastore

#endif // LOG_MANAGER_H
