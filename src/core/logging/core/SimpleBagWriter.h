#ifndef MXRC_CORE_LOGGING_CORE_SIMPLEBAGWRITER_H
#define MXRC_CORE_LOGGING_CORE_SIMPLEBAGWRITER_H

#include "interfaces/IBagWriter.h"
#include "core/AsyncWriter.h"
#include "util/RetentionManager.h"
#include <memory>
#include <chrono>
#include <mutex>

namespace mxrc::core::logging {

/**
 * @brief 간단한 Bag 파일 Writer 구현
 *
 * IBagWriter 인터페이스의 기본 구현으로,
 * AsyncWriter를 사용하여 비동기 쓰기를 수행하고,
 * RotationPolicy와 RetentionPolicy를 적용합니다.
 *
 * 주요 기능:
 * - JSONL 형식으로 메시지 기록
 * - 파일 크기/시간 기반 자동 순환
 * - 오래된 파일 자동 삭제
 * - 비동기/동기 쓰기 지원
 * - 통계 정보 제공
 */
class SimpleBagWriter : public IBagWriter {
public:
    /**
     * @brief SimpleBagWriter 생성자
     * @param bagDirectory Bag 파일 저장 디렉토리
     * @param baseFilename 기본 파일 이름 (타임스탬프 자동 추가)
     * @param queueCapacity 비동기 큐 용량 (기본 10,000)
     */
    explicit SimpleBagWriter(const std::string& bagDirectory,
                             const std::string& baseFilename = "mxrc",
                             size_t queueCapacity = 10000);

    ~SimpleBagWriter() override;

    // IBagWriter 인터페이스 구현
    bool open(const std::string& filepath) override;
    void close() override;
    bool appendAsync(const BagMessage& msg) override;
    bool append(const BagMessage& msg) override;
    bool flush(uint32_t timeoutMs = 5000) override;
    void setRotationPolicy(const RotationPolicy& policy) override;
    void setRetentionPolicy(const RetentionPolicy& policy) override;
    bool shouldRotate() const override;
    bool rotate() override;
    BagWriterStats getStats() const override;
    std::string getCurrentFilePath() const override;
    bool isOpen() const override;
    void start() override;
    void stop() override;

private:
    /**
     * @brief 새 Bag 파일 생성
     * @return 생성된 파일 경로
     */
    std::string createNewBagFile();

    /**
     * @brief 순환 조건 확인 및 자동 순환
     */
    void checkAndRotate();

    /**
     * @brief 순환 조건 확인 (내부용, mutex 이미 잡혀있음)
     * @return 순환 필요 시 true
     */
    bool shouldRotateInternal() const;

    /**
     * @brief 보존 정책 적용
     */
    void applyRetentionPolicy();

    std::string bagDirectory_;                  ///< Bag 파일 디렉토리
    std::string baseFilename_;                  ///< 기본 파일 이름
    size_t queueCapacity_;                      ///< 큐 용량
    std::string currentFilePath_;               ///< 현재 파일 경로
    std::unique_ptr<AsyncWriter> asyncWriter_;  ///< 비동기 Writer
    std::unique_ptr<RetentionManager> retentionManager_; ///< 보존 관리자

    RotationPolicy rotationPolicy_;             ///< 순환 정책
    RetentionPolicy retentionPolicy_;           ///< 보존 정책

    std::chrono::steady_clock::time_point fileStartTime_; ///< 파일 시작 시간
    uint64_t rotationCount_ = 0;                ///< 순환 횟수
    uint64_t totalMessagesWritten_ = 0;         ///< 전체 메시지 수 (순환 포함)
    uint64_t totalMessagesDropped_ = 0;         ///< 전체 드롭 수 (순환 포함)
    uint64_t totalBytesWritten_ = 0;            ///< 전체 바이트 수 (순환 포함)
    bool isOpen_ = false;                       ///< 열림 상태

    mutable std::mutex mutex_;                  ///< 동기화 뮤텍스
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_CORE_SIMPLEBAGWRITER_H
