# Interface Contract: ISystemdMetricsCollector

**Feature**: 018-systemd-process-management
**Component**: Metrics Collection
**Status**: Approved
**Last Updated**: 2025-01-21

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: interface, API, method 등)
- 일반 설명, 동작 설명은 모두 한글로 작성합니다

---

## 개요

`ISystemdMetricsCollector`는 systemd 서비스의 메트릭을 수집하는 인터페이스입니다. systemctl show 명령을 사용하여 서비스 상태, 리소스 사용량, watchdog 정보 등을 주기적으로 수집하고 Prometheus 형식으로 노출합니다.

---

## 인터페이스 정의

### C++ 헤더

**파일 위치**: `src/core/systemd/interfaces/ISystemdMetricsCollector.h`

```cpp
#ifndef MXRC_SYSTEMD_IMETRICS_COLLECTOR_H
#define MXRC_SYSTEMD_IMETRICS_COLLECTOR_H

#include "systemd/dto/SystemdMetric.h"
#include <vector>
#include <string>
#include <memory>

namespace mxrc::systemd {

/**
 * @brief systemd 서비스 메트릭 수집 인터페이스
 *
 * systemctl show 명령을 통해 systemd 서비스의 메트릭을 수집합니다.
 * 수집된 메트릭은 Prometheus 형식으로 노출됩니다.
 *
 * @note 구현체는 스레드 안전해야 합니다.
 */
class ISystemdMetricsCollector {
public:
    virtual ~ISystemdMetricsCollector() = default;

    /**
     * @brief 특정 서비스의 메트릭 수집
     *
     * systemctl show 명령을 실행하여 서비스 메트릭을 수집합니다.
     *
     * @param service_name 서비스 이름 (예: "mxrc-rt", "mxrc-nonrt")
     * @return std::vector<SystemdMetric> 수집된 메트릭 목록
     * @throws std::runtime_error systemctl 명령 실패 시
     * @throws std::invalid_argument 잘못된 서비스 이름 시
     *
     * @note 성능 요구사항: 수집 주기 1초
     * @note 스레드 안전: 동시 호출 가능
     */
    virtual std::vector<SystemdMetric> collect(const std::string& service_name) = 0;

    /**
     * @brief 여러 서비스의 메트릭 동시 수집
     *
     * 여러 서비스의 메트릭을 효율적으로 수집합니다.
     *
     * @param service_names 서비스 이름 목록
     * @return std::vector<SystemdMetric> 모든 서비스의 메트릭
     * @throws std::runtime_error systemctl 명령 실패 시
     *
     * @note 병렬 수집으로 성능 최적화
     */
    virtual std::vector<SystemdMetric> collectMultiple(
        const std::vector<std::string>& service_names) = 0;

    /**
     * @brief 수집할 속성 목록 설정
     *
     * systemctl show에서 조회할 속성을 지정합니다.
     * 기본값: ["ActiveState", "RestartCount", "CPUUsageNSec", "MemoryCurrent"]
     *
     * @param properties 속성 이름 목록
     *
     * @note 속성 개수가 많을수록 수집 시간 증가
     */
    virtual void setProperties(const std::vector<std::string>& properties) = 0;

    /**
     * @brief 현재 설정된 속성 목록 조회
     *
     * @return std::vector<std::string> 속성 이름 목록
     */
    virtual std::vector<std::string> getProperties() const = 0;

    /**
     * @brief 마지막 수집 시각 조회
     *
     * @return uint64_t Unix timestamp (밀리초)
     */
    virtual uint64_t getLastCollectionTimestamp() const = 0;

    /**
     * @brief 수집 실패 카운트 조회
     *
     * @return uint64_t 누적 실패 횟수
     */
    virtual uint64_t getFailureCount() const = 0;

    /**
     * @brief 수집 성공 카운트 조회
     *
     * @return uint64_t 누적 성공 횟수
     */
    virtual uint64_t getSuccessCount() const = 0;

    /**
     * @brief 통계 초기화
     *
     * 실패/성공 카운트를 0으로 리셋합니다.
     */
    virtual void resetStats() = 0;
};

} // namespace mxrc::systemd

#endif // MXRC_SYSTEMD_IMETRICS_COLLECTOR_H
```

---

## 메서드 계약

### collect()

**목적**: 단일 서비스 메트릭 수집

**사전 조건**:
- service_name이 유효한 systemd 서비스 이름
- systemd 서비스가 존재함

**사후 조건**:
- SystemdMetric 객체 목록 반환
- timestamp_ms 필드가 현재 시각으로 설정됨

**실행 순서**:
1. `systemctl show <service_name> --property=<properties>` 실행
2. 출력 파싱 (Key=Value 형식)
3. SystemdMetric 객체 생성
4. timestamp 추가
5. 결과 반환

**오류 처리**:
- systemctl 명령 실패 → `std::runtime_error`
- 잘못된 서비스 이름 → `std::invalid_argument`
- 파싱 오류 → 해당 메트릭 건너뜀 (부분 성공)

**성능**:
- 수집 시간 < 100ms (15개 속성 기준)
- 주기: 1초

### collectMultiple()

**목적**: 여러 서비스 메트릭 동시 수집

**사전 조건**:
- service_names가 비어있지 않음
- 모든 서비스가 존재함

**사후 조건**:
- 모든 서비스의 메트릭을 하나의 벡터로 반환

**최적화**:
- 병렬 실행 (std::async 또는 스레드 풀)
- 각 서비스별 독립적 수집

**오류 처리**:
- 일부 서비스 실패 시 나머지는 계속 수집 (best effort)
- 모든 서비스 실패 시 `std::runtime_error`

### setProperties()

**목적**: 수집할 속성 지정

**유효한 속성**:
```
ActiveState           - 서비스 활성 상태 (active, inactive 등)
SubState              - 세부 상태 (running, dead 등)
RestartCount          - 재시작 횟수
CPUUsageNSec          - CPU 사용 시간 (나노초)
MemoryCurrent         - 현재 메모리 사용량 (바이트)
TasksCurrent          - 현재 Task 개수
WatchdogTimestampMonotonic - Watchdog 마지막 알림 시각
WatchdogUSec          - Watchdog 타임아웃 (마이크로초)
CPUQuotaPerSecUSec    - CPU quota (마이크로초/초)
MemoryMax             - 메모리 제한 (바이트)
IOReadBytes           - I/O 읽기 바이트
IOWriteBytes          - I/O 쓰기 바이트
ExecMainStartTimestamp - 프로세스 시작 시각
ExecMainExitTimestamp  - 프로세스 종료 시각
InvocationID          - 서비스 Invocation ID (UUID)
```

**기본값**:
```cpp
{"ActiveState", "RestartCount", "CPUUsageNSec", "MemoryCurrent"}
```

### getLastCollectionTimestamp()

**목적**: 마지막 수집 시각 조회

**반환값**: Unix timestamp (밀리초)

**사용 예시**:
```cpp
uint64_t last_ts = collector->getLastCollectionTimestamp();
uint64_t now_ts = getCurrentTimeMs();
if (now_ts - last_ts > 60000) {
    // 1분 이상 수집 안 됨 → 경고
}
```

### resetStats()

**목적**: 통계 초기화

**사용 시나리오**:
- 서비스 재시작 시
- 테스트 케이스 간 초기화

---

## 구현 예시

### SystemdMetricsCollector (systemctl 기반 구현)

**파일 위치**: `src/core/systemd/impl/SystemdMetricsCollector.h`

```cpp
#ifndef MXRC_SYSTEMD_METRICS_COLLECTOR_H
#define MXRC_SYSTEMD_METRICS_COLLECTOR_H

#include "systemd/interfaces/ISystemdMetricsCollector.h"
#include <mutex>
#include <atomic>
#include <spdlog/spdlog.h>

namespace mxrc::systemd {

class SystemdMetricsCollector : public ISystemdMetricsCollector {
public:
    SystemdMetricsCollector()
        : properties_({"ActiveState", "RestartCount", "CPUUsageNSec", "MemoryCurrent"}),
          last_collection_ts_(0),
          success_count_(0),
          failure_count_(0) {
    }

    std::vector<SystemdMetric> collect(const std::string& service_name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            // systemctl show 실행
            auto output = executeSystemctlShow(service_name);

            // 파싱
            auto metrics = parseSystemctlOutput(service_name, output);

            // 통계 업데이트
            last_collection_ts_ = getCurrentTimeMs();
            success_count_++;

            return metrics;

        } catch (const std::exception& e) {
            failure_count_++;
            spdlog::error("Failed to collect metrics for {}: {}",
                          service_name, e.what());
            throw;
        }
    }

    std::vector<SystemdMetric> collectMultiple(
        const std::vector<std::string>& service_names) override {

        std::vector<std::future<std::vector<SystemdMetric>>> futures;

        // 병렬 수집
        for (const auto& service_name : service_names) {
            futures.push_back(std::async(std::launch::async, [this, service_name]() {
                return this->collect(service_name);
            }));
        }

        // 결과 수집
        std::vector<SystemdMetric> all_metrics;
        for (auto& future : futures) {
            try {
                auto metrics = future.get();
                all_metrics.insert(all_metrics.end(), metrics.begin(), metrics.end());
            } catch (const std::exception& e) {
                spdlog::warn("One service collection failed: {}", e.what());
            }
        }

        return all_metrics;
    }

    void setProperties(const std::vector<std::string>& properties) override {
        std::lock_guard<std::mutex> lock(mutex_);
        properties_ = properties;
    }

    std::vector<std::string> getProperties() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return properties_;
    }

    uint64_t getLastCollectionTimestamp() const override {
        return last_collection_ts_.load();
    }

    uint64_t getFailureCount() const override {
        return failure_count_.load();
    }

    uint64_t getSuccessCount() const override {
        return success_count_.load();
    }

    void resetStats() override {
        success_count_ = 0;
        failure_count_ = 0;
    }

private:
    std::string executeSystemctlShow(const std::string& service_name);
    std::vector<SystemdMetric> parseSystemctlOutput(
        const std::string& service_name, const std::string& output);
    uint64_t getCurrentTimeMs();

    mutable std::mutex mutex_;
    std::vector<std::string> properties_;
    std::atomic<uint64_t> last_collection_ts_;
    std::atomic<uint64_t> success_count_;
    std::atomic<uint64_t> failure_count_;
};

} // namespace mxrc::systemd

#endif // MXRC_SYSTEMD_METRICS_COLLECTOR_H
```

### executeSystemctlShow() 구현

```cpp
std::string SystemdMetricsCollector::executeSystemctlShow(
    const std::string& service_name) {

    // 속성 목록을 --property 인자로 변환
    std::string props = "--property=" + join(properties_, ",");

    // 명령 실행
    std::string cmd = "systemctl show " + service_name + " " + props;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed");
    }

    // 출력 읽기
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int ret = pclose(pipe);
    if (ret != 0) {
        throw std::runtime_error("systemctl show failed: exit code " +
                                 std::to_string(ret));
    }

    return result;
}
```

### parseSystemctlOutput() 구현

```cpp
std::vector<SystemdMetric> SystemdMetricsCollector::parseSystemctlOutput(
    const std::string& service_name, const std::string& output) {

    std::vector<SystemdMetric> metrics;
    uint64_t timestamp_ms = getCurrentTimeMs();

    // Key=Value 형식 파싱
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;  // 잘못된 줄 건너뛰기
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        SystemdMetric metric;
        metric.service_name = service_name;
        metric.property_name = key;
        metric.property_value = value;
        metric.timestamp_ms = timestamp_ms;

        metrics.push_back(metric);
    }

    return metrics;
}
```

---

## 사용 예시

### 기본 사용법

```cpp
#include "systemd/impl/SystemdMetricsCollector.h"

int main() {
    auto collector = std::make_shared<mxrc::systemd::SystemdMetricsCollector>();

    // 수집할 속성 지정
    collector->setProperties({
        "ActiveState",
        "RestartCount",
        "CPUUsageNSec",
        "MemoryCurrent",
        "TasksCurrent",
        "WatchdogTimestampMonotonic"
    });

    // 메트릭 수집 (1초 주기)
    while (true) {
        try {
            auto metrics = collector->collect("mxrc-rt");

            // Prometheus 형식으로 노출
            for (const auto& metric : metrics) {
                std::cout << metric.toPrometheusMetric() << std::endl;
            }

        } catch (const std::exception& e) {
            spdlog::error("Metric collection failed: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
```

### 여러 서비스 수집

```cpp
auto collector = std::make_shared<mxrc::systemd::SystemdMetricsCollector>();

std::vector<std::string> services = {"mxrc-rt", "mxrc-nonrt", "mxrc-monitor"};
auto all_metrics = collector->collectMultiple(services);

// 서비스별로 그룹화
std::map<std::string, std::vector<SystemdMetric>> grouped;
for (const auto& metric : all_metrics) {
    grouped[metric.service_name].push_back(metric);
}
```

### 통계 모니터링

```cpp
// 수집 성공률 계산
uint64_t success = collector->getSuccessCount();
uint64_t failure = collector->getFailureCount();
double success_rate = (double)success / (success + failure) * 100.0;

spdlog::info("Metric collection success rate: {:.2f}%", success_rate);

// 1시간마다 통계 초기화
if (hourChanged()) {
    collector->resetStats();
}
```

---

## Prometheus 메트릭 예시

```
# systemctl show mxrc-rt --property=ActiveState,RestartCount,CPUUsageNSec,MemoryCurrent

systemd_service_active_state{service="mxrc-rt",state="active"} 1 1705862400000
systemd_service_restart_count{service="mxrc-rt"} 0 1705862400000
systemd_service_cpu_usage_nsec{service="mxrc-rt"} 125000000 1705862400000
systemd_service_memory_current_bytes{service="mxrc-rt"} 134217728 1705862400000
```

---

## 성능 요구사항

- **수집 주기**: 1초
- **수집 시간**: < 100ms (15개 속성 기준)
- **병렬 수집**: 3개 서비스 동시 수집 가능
- **메모리 사용량**: < 10MB (메트릭 버퍼 포함)

---

## 보안 고려사항

- **권한**: systemctl show는 일반 사용자도 실행 가능 (서비스 상태 조회만)
- **Seccomp**: `@process` 세트에 포함 (popen 사용)
- **입력 검증**: service_name에 shell injection 방지 필요

---

## 의존성

- **빌드 의존성**: 없음 (systemctl CLI 사용)
- **런타임 의존성**: systemd (v255+), systemctl 명령
- **라이브러리**: spdlog (로깅), nlohmann_json (JSON 직렬화)

---

## 변경 이력

| 버전 | 날짜       | 변경 내용           | 작성자 |
|------|------------|---------------------|--------|
| 1.0  | 2025-01-21 | 초기 인터페이스 정의 | MXRC   |

---

**작성자**: MXRC Development Team
**검토자**: TBD
**승인 날짜**: TBD
