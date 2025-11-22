#ifndef MXRC_SYSTEMD_IJOURNALD_LOGGER_HPP
#define MXRC_SYSTEMD_IJOURNALD_LOGGER_HPP

#include <string>
#include <map>
#include <cstdint>

namespace mxrc {
namespace systemd {

/**
 * @brief systemd-journald 구조화 로깅 인터페이스
 *
 * User Story 6: systemd-journald 통합
 *
 * 이 인터페이스는 systemd-journald로 구조화된 로그를 전송합니다:
 * - Trace ID, Span ID를 포함한 분산 추적
 * - Component 필드로 로그 출처 식별
 * - journalctl로 쿼리 가능한 사용자 정의 필드
 */
class IJournaldLogger {
public:
    virtual ~IJournaldLogger() = default;

    /**
     * @brief 로그 레벨 (spdlog 호환)
     */
    enum class LogLevel {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        CRITICAL = 5
    };

    /**
     * @brief 구조화된 로그 메타데이터
     */
    struct LogMetadata {
        std::string trace_id;     ///< 분산 추적 Trace ID (16진수 문자열)
        std::string span_id;      ///< 분산 추적 Span ID (16진수 문자열)
        std::string component;    ///< 컴포넌트 이름 (task, action, sequence 등)
        std::map<std::string, std::string> custom_fields; ///< 추가 사용자 정의 필드
    };

    /**
     * @brief 메타데이터와 함께 로그 전송
     *
     * @param level 로그 레벨
     * @param message 로그 메시지
     * @param metadata 구조화된 메타데이터
     */
    virtual void log(LogLevel level, const std::string& message, const LogMetadata& metadata) = 0;

    /**
     * @brief 간단한 로그 전송 (메타데이터 없음)
     *
     * @param level 로그 레벨
     * @param message 로그 메시지
     */
    virtual void log(LogLevel level, const std::string& message) = 0;

    /**
     * @brief Trace 레벨 로그
     */
    virtual void trace(const std::string& message, const LogMetadata& metadata = {}) = 0;

    /**
     * @brief Debug 레벨 로그
     */
    virtual void debug(const std::string& message, const LogMetadata& metadata = {}) = 0;

    /**
     * @brief Info 레벨 로그
     */
    virtual void info(const std::string& message, const LogMetadata& metadata = {}) = 0;

    /**
     * @brief Warn 레벨 로그
     */
    virtual void warn(const std::string& message, const LogMetadata& metadata = {}) = 0;

    /**
     * @brief Error 레벨 로그
     */
    virtual void error(const std::string& message, const LogMetadata& metadata = {}) = 0;

    /**
     * @brief Critical 레벨 로그
     */
    virtual void critical(const std::string& message, const LogMetadata& metadata = {}) = 0;
};

} // namespace systemd
} // namespace mxrc

#endif // MXRC_SYSTEMD_IJOURNALD_LOGGER_HPP
