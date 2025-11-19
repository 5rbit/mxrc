// IBagWriter.h
// Bag 파일 작성 인터페이스 (FR-001 ~ FR-011)
//
// 이 인터페이스는 MXRC Bag 파일 작성을 위한 추상화 계층을 제공합니다.
// 비동기 I/O, 파일 회전, 보관 정책, 통계 수집 등의 기능을 포함합니다.

#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "../data-model.md"  // BagMessage, BagStats, RotationPolicy, RetentionPolicy

namespace mxrc::core::logging {

/// @brief Bag 파일 작성 인터페이스
///
/// 이 인터페이스는 다음 기능 요구사항을 충족합니다:
/// - FR-001: 나노초 정밀도 타임스탬프 기록
/// - FR-002: 성능 저하 < 1% (비동기 I/O)
/// - FR-003: JSONL 포맷 저장
/// - FR-004: 파일 회전 (SIZE/TIME 기반)
/// - FR-005: 자동 파일 삭제 정책 (보관 정책)
/// - FR-006: 디스크 공간 부족 시 자동 파일 삭제
/// - FR-007: 비동기 쓰기 + 명시적 flush
/// - FR-008: 통계 수집 (messagesWritten, bytesWritten, writeLatencyUs)
/// - FR-023: 큐 오버플로우 시 드롭 정책 및 통계 기록
class IBagWriter {
public:
    virtual ~IBagWriter() = default;

    /// @brief 메시지를 비동기로 Bag 파일에 추가 (FR-007, FR-002)
    ///
    /// 이 메서드는 논블로킹으로 동작하며, 내부 큐에 메시지를 추가합니다.
    /// 백그라운드 I/O 스레드가 실제 디스크 쓰기를 수행합니다.
    ///
    /// @param msg 기록할 BagMessage
    /// @throws std::runtime_error 큐가 가득 찬 경우 (FR-023)
    /// @note 큐 오버플로우 시 메시지는 드롭되고 통계에 기록됩니다.
    virtual void appendAsync(const BagMessage& msg) = 0;

    /// @brief 메시지를 동기적으로 Bag 파일에 추가
    ///
    /// 디버깅이나 테스트 목적으로 사용됩니다. 프로덕션 코드에서는
    /// appendAsync를 사용하여 성능 저하를 방지하세요.
    ///
    /// @param msg 기록할 BagMessage
    virtual void append(const BagMessage& msg) = 0;

    /// @brief 버퍼링된 데이터를 즉시 디스크에 기록 (FR-007)
    ///
    /// 정상 종료 전이나 크리티컬 데이터 보존이 필요한 시점에 호출합니다.
    /// 비동기 큐의 모든 메시지가 디스크에 기록될 때까지 블로킹됩니다.
    virtual void flush() = 0;

    /// @brief 현재 Bag 파일 작성 통계 조회 (FR-008)
    ///
    /// @return BagStats 구조체 (messagesWritten, messagesDropped, bytesWritten, writeLatencyUs)
    virtual BagStats getStats() const = 0;

    /// @brief 파일 회전 정책 설정 (FR-004)
    ///
    /// SIZE 기반 또는 TIME 기반 회전을 설정합니다.
    ///
    /// @param policy RotationPolicy 구조체
    /// @example
    ///   RotationPolicy sizePolicy;
    ///   sizePolicy.type = RotationType::SIZE;
    ///   sizePolicy.maxSizeMB = 1024;  // 1GB
    ///   writer->setRotationPolicy(sizePolicy);
    virtual void setRotationPolicy(const RotationPolicy& policy) = 0;

    /// @brief 파일 보관 정책 설정 (FR-005, FR-006)
    ///
    /// TIME 기반 또는 COUNT 기반 자동 삭제 정책을 설정합니다.
    /// 디스크 공간 부족 시에도 자동으로 오래된 파일을 삭제합니다.
    ///
    /// @param policy RetentionPolicy 구조체
    /// @example
    ///   RetentionPolicy timePolicy;
    ///   timePolicy.type = RetentionType::TIME;
    ///   timePolicy.maxAgeDays = 7;  // 7일 이상 파일 자동 삭제
    ///   writer->setRetentionPolicy(timePolicy);
    virtual void setRetentionPolicy(const RetentionPolicy& policy) = 0;

    /// @brief Bag 파일 작성 시작
    ///
    /// 새로운 Bag 파일을 생성하고 헤더를 작성합니다.
    /// 비동기 I/O 스레드를 시작합니다.
    ///
    /// @param filepath Bag 파일 경로 (예: "logs/mission_2025-11-19_14-30-00.bag")
    /// @throws std::runtime_error 파일 생성 실패 시
    virtual void open(const std::string& filepath) = 0;

    /// @brief Bag 파일 작성 종료
    ///
    /// 남은 메시지를 모두 기록하고, 인덱스 블록과 footer를 작성한 후 파일을 닫습니다.
    /// 비동기 I/O 스레드를 안전하게 종료합니다.
    virtual void close() = 0;

    /// @brief 현재 작성 중인 Bag 파일 경로 조회
    ///
    /// @return 파일 경로 문자열 (파일이 열리지 않았으면 빈 문자열)
    virtual std::string getCurrentFilePath() const = 0;

    /// @brief 파일 회전이 필요한지 확인 (FR-004)
    ///
    /// @return true if rotation needed, false otherwise
    virtual bool shouldRotate() const = 0;

    /// @brief 파일 회전 수행 (FR-004)
    ///
    /// 현재 파일을 닫고, 타임스탬프 기반 새 파일명으로 파일을 생성합니다.
    /// 예: mission_2025-11-19_14-30-00.bag → mission_2025-11-19_14-31-00.bag
    ///
    /// @throws std::runtime_error 파일 회전 실패 시
    virtual void rotate() = 0;
};

} // namespace mxrc::core::logging
