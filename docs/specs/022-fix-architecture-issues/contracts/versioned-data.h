/**
 * @file versioned-data.h
 * @brief VersionedData 구조체 정의 (문서화용 스니펫)
 *
 * Feature: 022-fix-architecture-issues
 * Phase: 1 (Design)
 * Date: 2025-01-22
 *
 * 본 파일은 실제 구현 코드가 아닌, 설계 문서화를 위한 헤더 스니펫입니다.
 * 실제 구현은 src/core/datastore/core/VersionedData.h에 위치합니다.
 */

#pragma once

#include <cstdint>
#include <chrono>

namespace mxrc {
namespace datastore {

/**
 * @brief 버전 관리 데이터 래퍼 (템플릿)
 *
 * DataStore에 저장되는 모든 값에 버전 정보를 추가하여 데이터 일관성을 보장합니다.
 * Linux kernel의 seqlock 메커니즘을 참조한 설계입니다.
 *
 * 주요 기능:
 * - 단조 증가하는 버전 번호 (atomic increment)
 * - 나노초 정밀도 타임스탬프 (monotonic clock)
 * - 버전 일관성 검증 메서드
 * - RT 안전 (POD 타입, 메모리 할당 없음)
 *
 * 사용 예시 (RT 경로, 쓰기):
 * @code
 * // DataStore 내부
 * template <typename T>
 * void DataStore::setVersioned(const std::string& key, const T& value) {
 *     VersionedData<T>& data = data_[key];
 *     data.value = value;
 *     data.version++;  // atomic increment
 *     data.timestamp_ns = getCurrentTimeNs();
 * }
 * @endcode
 *
 * 사용 예시 (Non-RT 경로, 일관성 체크):
 * @code
 * auto temp1 = accessor->getTemperature();
 * auto pressure = accessor->getPressure();
 * auto temp2 = accessor->getTemperature();
 *
 * if (!temp1.isConsistentWith(temp2)) {
 *     // 버전 불일치 → 재시도
 *     spdlog::warn("Version mismatch detected");
 *     retry();
 * }
 * @endcode
 *
 * @tparam T 값의 타입 (double, int, Eigen::Vector3d 등)
 *
 * @note POD 타입 (Plain Old Data): 스택 할당 가능, 메모리 할당 없음
 * @note 캐시 라인 정렬: 64-byte aligned (false sharing 방지)
 */
template <typename T>
struct VersionedData {
    /**
     * @brief 실제 데이터 값
     *
     * 센서 값, 로봇 상태 등 도메인별로 다양한 타입 사용.
     */
    T value;

    /**
     * @brief 단조 증가 시퀀스 번호
     *
     * setVersioned() 호출 시마다 1씩 증가합니다.
     * 동일한 값으로 쓰기를 해도 버전은 증가합니다.
     *
     * @note atomic<uint64_t>로 관리됨 (내부 구현)
     * @note 초기값: 0
     * @note 최대값: 2^64-1 (오버플로우는 고려하지 않음)
     */
    uint64_t version;

    /**
     * @brief 나노초 정밀도 타임스탬프
     *
     * std::chrono::steady_clock을 사용하여 monotonic time을 측정합니다.
     * 시스템 시간 변경(NTP 동기화 등)의 영향을 받지 않습니다.
     *
     * @note 단위: 나노초 (1e-9초)
     * @note 용도: 데이터 신선도(freshness) 확인
     *
     * 사용 예시:
     * @code
     * auto data = accessor->getTemperature();
     * uint64_t age_ns = getCurrentTimeNs() - data.timestamp_ns;
     * if (age_ns > 1'000'000'000) {  // 1초 이상 오래된 데이터
     *     spdlog::warn("Stale data detected: {} ns old", age_ns);
     * }
     * @endcode
     */
    uint64_t timestamp_ns;

    /**
     * @brief 기본 생성자
     *
     * 모든 필드를 기본값으로 초기화합니다.
     * - value: T의 기본 생성자 호출
     * - version: 0
     * - timestamp_ns: 0
     */
    VersionedData()
        : value{}, version(0), timestamp_ns(0) {}

    /**
     * @brief 값 초기화 생성자
     *
     * @param val 초기값
     * @param ver 초기 버전 (기본값: 0)
     * @param ts 초기 타임스탬프 (기본값: 0)
     */
    VersionedData(const T& val, uint64_t ver = 0, uint64_t ts = 0)
        : value(val), version(ver), timestamp_ns(ts) {}

    /**
     * @brief 버전 일관성 검증
     *
     * 두 VersionedData 객체가 동일한 버전인지 확인합니다.
     * Non-RT 경로에서 일관된 스냅샷을 읽을 때 사용됩니다.
     *
     * @param other 비교 대상 VersionedData
     * @return true: 버전 동일, false: 버전 불일치
     *
     * @note RT 경로에서는 호출하지 않음 (성능 이유)
     * @note Non-RT 경로에서만 사용 (재시도 로직 포함)
     *
     * 사용 예시:
     * @code
     * auto temp1 = accessor->getTemperature();
     * // ... 다른 작업 ...
     * auto temp2 = accessor->getTemperature();
     *
     * if (!temp1.isConsistentWith(temp2)) {
     *     // 버전 불일치: RT에서 쓰기가 발생함
     *     spdlog::warn("Version mismatch: {} vs {}", temp1.version, temp2.version);
     *     retry();
     * }
     * @endcode
     */
    bool isConsistentWith(const VersionedData<T>& other) const {
        return version == other.version;
    }

    /**
     * @brief 최신 여부 확인
     *
     * 현재 객체가 다른 객체보다 새로운지 확인합니다.
     *
     * @param other 비교 대상 VersionedData
     * @return true: 현재 객체가 더 최신, false: 그렇지 않음
     *
     * 사용 예시:
     * @code
     * auto old_temp = accessor->getTemperature();
     * std::this_thread::sleep_for(std::chrono::milliseconds(100));
     * accessor->setTemperature(26.0);  // 쓰기 발생
     * auto new_temp = accessor->getTemperature();
     *
     * assert(new_temp.isNewerThan(old_temp));  // true
     * @endcode
     */
    bool isNewerThan(const VersionedData<T>& other) const {
        return version > other.version;
    }

    /**
     * @brief 데이터 신선도 확인 (나노초)
     *
     * 현재 시각 대비 데이터가 얼마나 오래되었는지 계산합니다.
     *
     * @param current_time_ns 현재 시각 (나노초)
     * @return 경과 시간 (나노초)
     *
     * 사용 예시:
     * @code
     * auto data = accessor->getTemperature();
     * uint64_t age_ns = data.getAge(getCurrentTimeNs());
     * if (age_ns > 1'000'000'000) {  // 1초 이상
     *     spdlog::warn("Stale data: {} ms old", age_ns / 1'000'000);
     * }
     * @endcode
     */
    uint64_t getAge(uint64_t current_time_ns) const {
        return current_time_ns - timestamp_ns;
    }

    /**
     * @brief 데이터 신선도 확인 (자동, steady_clock 사용)
     *
     * @return 경과 시간 (나노초)
     */
    uint64_t getAge() const {
        auto now = std::chrono::steady_clock::now();
        uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()
        ).count();
        return now_ns - timestamp_ns;
    }
};

/**
 * @brief VersionedData<double> 메모리 레이아웃
 *
 * Offset  | Size | Field        | Alignment
 * --------|------|--------------|----------
 * 0x00    | 8    | value        | 8-byte aligned
 * 0x08    | 8    | version      | 8-byte aligned
 * 0x10    | 8    | timestamp_ns | 8-byte aligned
 * --------|------|--------------|----------
 * Total: 24 bytes (cache-line friendly, < 64 bytes)
 */
static_assert(sizeof(VersionedData<double>) == 24, "VersionedData<double> size mismatch");

}  // namespace datastore
}  // namespace mxrc
