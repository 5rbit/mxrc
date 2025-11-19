#ifndef MXRC_CORE_LOGGING_INTERFACES_IBAGWRITER_H
#define MXRC_CORE_LOGGING_INTERFACES_IBAGWRITER_H

#include "dto/BagMessage.h"
#include "dto/RotationPolicy.h"
#include "dto/RetentionPolicy.h"
#include <string>
#include <cstdint>
#include <memory>
#include <future>

namespace mxrc::core::logging {

/**
 * @brief Bag 파일 쓰기 통계
 */
struct BagWriterStats {
    uint64_t messagesWritten = 0;     ///< 쓰기 성공한 메시지 개수
    uint64_t messagesDropped = 0;      ///< 드롭된 메시지 개수
    uint64_t bytesWritten = 0;         ///< 쓰기 성공한 바이트 수
    uint64_t rotationCount = 0;        ///< 파일 순환 횟수
    std::string currentFilePath;       ///< 현재 활성 파일 경로
    uint64_t currentFileSize = 0;      ///< 현재 파일 크기 (바이트)
};

/**
 * @brief Bag 파일 Writer 인터페이스
 *
 * BagMessage를 JSONL 형식으로 파일에 기록하는 인터페이스입니다.
 * 파일 순환(Rotation), 보존(Retention) 정책을 지원하며,
 * 비동기 및 동기 쓰기를 모두 제공합니다.
 *
 * FR-001: JSONL 형식 메시지 기록
 * FR-002: 비동기 쓰기 지원
 * FR-003: 파일 순환 정책 지원
 * FR-004: 파일 보존 정책 지원
 * FR-008: 통계 정보 제공
 * FR-009: 버퍼 플러시 기능
 * FR-010: 파일 생명주기 관리
 */
class IBagWriter {
public:
    virtual ~IBagWriter() = default;

    /**
     * @brief Bag 파일 열기
     * @param filepath 파일 경로
     * @return 성공 시 true
     *
     * FR-010: 파일 열기
     */
    virtual bool open(const std::string& filepath) = 0;

    /**
     * @brief Bag 파일 닫기
     *
     * FR-010: 파일 닫기
     * 남은 버퍼를 플러시하고 파일을 안전하게 닫습니다.
     */
    virtual void close() = 0;

    /**
     * @brief 메시지 비동기 쓰기
     * @param msg 기록할 메시지
     * @return 성공 시 true (큐에 추가 성공)
     *
     * FR-001: JSONL 메시지 기록
     * FR-002: 비동기 쓰기
     * 큐가 가득 찬 경우 false 반환 (드롭 정책)
     */
    virtual bool appendAsync(const BagMessage& msg) = 0;

    /**
     * @brief 메시지 동기 쓰기
     * @param msg 기록할 메시지
     * @return 성공 시 true
     *
     * FR-001: JSONL 메시지 기록
     * 즉시 파일에 쓰기를 수행합니다. (블로킹)
     */
    virtual bool append(const BagMessage& msg) = 0;

    /**
     * @brief 버퍼 플러시
     * @param timeoutMs 타임아웃 (밀리초)
     * @return 성공 시 true
     *
     * FR-009: 버퍼 플러시
     * 큐에 남은 메시지를 모두 파일에 쓸 때까지 대기합니다.
     */
    virtual bool flush(uint32_t timeoutMs = 5000) = 0;

    /**
     * @brief 파일 순환 정책 설정
     * @param policy 순환 정책
     *
     * FR-003: 파일 순환 정책
     */
    virtual void setRotationPolicy(const RotationPolicy& policy) = 0;

    /**
     * @brief 파일 보존 정책 설정
     * @param policy 보존 정책
     *
     * FR-004: 파일 보존 정책
     */
    virtual void setRetentionPolicy(const RetentionPolicy& policy) = 0;

    /**
     * @brief 순환 조건 확인
     * @return 순환해야 하면 true
     *
     * FR-003: 순환 조건 확인
     * 현재 파일 크기 또는 기록 시간이 정책을 초과하는지 확인합니다.
     */
    virtual bool shouldRotate() const = 0;

    /**
     * @brief 파일 순환 수행
     * @return 성공 시 true
     *
     * FR-003: 파일 순환
     * 현재 파일을 닫고, 새 타임스탬프 파일을 생성합니다.
     */
    virtual bool rotate() = 0;

    /**
     * @brief 통계 정보 조회
     * @return BagWriterStats
     *
     * FR-008: 통계 정보
     */
    virtual BagWriterStats getStats() const = 0;

    /**
     * @brief 현재 파일 경로 조회
     * @return 파일 경로
     */
    virtual std::string getCurrentFilePath() const = 0;

    /**
     * @brief Writer 열림 상태 확인
     * @return 열려 있으면 true
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief Writer 시작 (비동기 스레드 시작)
     *
     * FR-002: 비동기 쓰기 스레드 시작
     */
    virtual void start() = 0;

    /**
     * @brief Writer 정지 (비동기 스레드 정지)
     *
     * FR-002: 비동기 쓰기 스레드 정지
     * 남은 메시지를 모두 처리한 후 종료합니다.
     */
    virtual void stop() = 0;
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_INTERFACES_IBAGWRITER_H
