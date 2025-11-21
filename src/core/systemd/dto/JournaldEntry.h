#pragma once

#include <string>
#include <chrono>
#include <map>
#include <cstdint>

namespace mxrc {
namespace systemd {
namespace dto {

/**
 * @brief journald 로그 엔트리를 표현하는 DTO
 *
 * sd_journal_send API를 통해 journald로 전송되는 구조화된 로그 데이터.
 * ECS (Elastic Common Schema) 형식을 따르며 중앙 집중식 로그 관리를 지원.
 *
 * @example
 * JournaldEntry entry;
 * entry.message = "RT cycle completed";
 * entry.priority = 6; // INFO
 * entry.serviceName = "mxrc-rt";
 * entry.fields["cycle_time_us"] = "250";
 * entry.fields["jitter_us"] = "12";
 */
struct JournaldEntry {
    /// 로그 메시지 (MESSAGE)
    std::string message;

    /// 로그 우선순위 (PRIORITY: 0=emerg, 1=alert, 2=crit, 3=err, 4=warning, 5=notice, 6=info, 7=debug)
    int priority;

    /// 서비스 이름 (SYSLOG_IDENTIFIER)
    std::string serviceName;

    /// 로그 생성 시각
    std::chrono::system_clock::time_point timestamp;

    /// ECS 필드 (event.action, event.category, event.type 등)
    std::map<std::string, std::string> fields;

    /// 프로세스 ID (선택적)
    pid_t pid;

    /// 스레드 ID (선택적)
    uint64_t tid;

    /**
     * @brief 기본 생성자
     */
    JournaldEntry()
        : priority(6)  // INFO
        , pid(0)
        , tid(0) {}

    /**
     * @brief 매개변수 생성자
     */
    JournaldEntry(const std::string& msg,
                  int prio,
                  const std::string& service)
        : message(msg)
        , priority(prio)
        , serviceName(service)
        , timestamp(std::chrono::system_clock::now())
        , pid(0)
        , tid(0) {}

    /**
     * @brief 필드 추가 헬퍼
     */
    void addField(const std::string& key, const std::string& value) {
        fields[key] = value;
    }
};

} // namespace dto
} // namespace systemd
} // namespace mxrc
