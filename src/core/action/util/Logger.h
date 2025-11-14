#ifndef MXRC_CORE_ACTION_LOGGER_H
#define MXRC_CORE_ACTION_LOGGER_H

#include <spdlog/spdlog.h>
#include <memory>

namespace mxrc::core::action {

/**
 * @brief Action 시스템 로거
 *
 * Action Layer의 로깅을 위한 전역 로거
 */
class Logger {
public:
    static std::shared_ptr<spdlog::logger> get() {
        static auto logger = spdlog::default_logger()->clone("action");
        return logger;
    }
};

} // namespace mxrc::core::action

#endif // MXRC_CORE_ACTION_LOGGER_H
