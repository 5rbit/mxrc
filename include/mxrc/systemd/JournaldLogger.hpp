#ifndef MXRC_SYSTEMD_JOURNALD_LOGGER_HPP
#define MXRC_SYSTEMD_JOURNALD_LOGGER_HPP

#include "IJournaldLogger.hpp"
#include <memory>

namespace mxrc {
namespace systemd {

/**
 * @brief systemd-journald 로거 구현체
 *
 * libsystemd의 sd_journal_send를 사용하여 구조화된 로그 전송
 */
class JournaldLogger : public IJournaldLogger {
public:
    JournaldLogger() = default;
    ~JournaldLogger() override = default;

    void log(LogLevel level, const std::string& message, const LogMetadata& metadata) override;
    void log(LogLevel level, const std::string& message) override;

    void trace(const std::string& message, const LogMetadata& metadata = {}) override;
    void debug(const std::string& message, const LogMetadata& metadata = {}) override;
    void info(const std::string& message, const LogMetadata& metadata = {}) override;
    void warn(const std::string& message, const LogMetadata& metadata = {}) override;
    void error(const std::string& message, const LogMetadata& metadata = {}) override;
    void critical(const std::string& message, const LogMetadata& metadata = {}) override;

private:
    /**
     * @brief spdlog 로그 레벨을 journald priority로 변환
     *
     * @param level spdlog 로그 레벨
     * @return journald priority (0-7)
     */
    int toPriority(LogLevel level);

    /**
     * @brief journald로 로그 전송 (실제 구현)
     *
     * @param priority journald priority
     * @param message 로그 메시지
     * @param metadata 메타데이터
     */
    void sendToJournal(int priority, const std::string& message, const LogMetadata& metadata);
};

} // namespace systemd
} // namespace mxrc

#endif // MXRC_SYSTEMD_JOURNALD_LOGGER_HPP
