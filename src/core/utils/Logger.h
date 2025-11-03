#ifndef MXRC_LOGGER_H
#define MXRC_LOGGER_H

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace mxrc {
namespace utils {

    inline void initialize_logger() {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        spdlog::set_default_logger(std::make_shared<spdlog::logger>("mxrc_logger", console_sink));
    }

    inline std::shared_ptr<spdlog::logger>& get_logger() {
        return spdlog::default_logger();
    }

} // namespace utils
} // namespace mxrc

#endif // MXRC_LOGGER_H
