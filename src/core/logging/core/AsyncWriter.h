#ifndef MXRC_CORE_LOGGING_CORE_ASYNCWRITER_H
#define MXRC_CORE_LOGGING_CORE_ASYNCWRITER_H

#include "dto/BagMessage.h"
#include "dto/DataType.h"
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <memory>

namespace mxrc::core::logging {

/**
 * @brief 비동기 Bag 파일 쓰기 클래스
 *
 * std::thread + std::queue를 사용한 논블로킹 파일 I/O를 제공합니다.
 * EventBus 기반 아키텍처와 일관성을 유지하며, 실시간 성능 영향을 최소화합니다.
 *
 * 주요 특징:
 * - tryPush(): 논블로킹 (큐 가득 차면 false 반환)
 * - 큐 오버플로우 시 드롭 정책 (통계 기록)
 * - RAII 원칙 준수 (소멸자에서 안전한 종료)
 * - 성능 영향: ~50-200ns (mutex lock + queue push)
 */
class AsyncWriter {
public:
    /**
     * @brief AsyncWriter 생성자
     * @param filepath 쓰기 대상 파일 경로
     * @param queueCapacity 큐 최대 용량 (기본값: 10,000)
     */
    explicit AsyncWriter(const std::string& filepath, size_t queueCapacity = 10000);

    /**
     * @brief 소멸자 - 큐를 비우고 스레드 안전하게 종료
     */
    ~AsyncWriter();

    // 복사/이동 금지
    AsyncWriter(const AsyncWriter&) = delete;
    AsyncWriter& operator=(const AsyncWriter&) = delete;

    /**
     * @brief Writer 스레드 시작
     */
    void start();

    /**
     * @brief Writer 스레드 중지 (큐가 빌 때까지 대기)
     */
    void stop();

    /**
     * @brief 메시지를 큐에 추가 (논블로킹)
     * @param msg Bag 메시지
     * @return 성공하면 true, 큐가 가득 차면 false
     */
    bool tryPush(const BagMessage& msg);

    /**
     * @brief 모든 메시지가 디스크에 쓰일 때까지 대기
     * @param timeoutMs 타임아웃 (밀리초), 0이면 무한 대기
     * @return 성공하면 true, 타임아웃 시 false
     */
    bool flush(uint32_t timeoutMs = 5000);

    /**
     * @brief 현재 큐에 대기 중인 메시지 개수
     * @return 큐 크기
     */
    size_t queueSize() const;

    /**
     * @brief 드롭된 메시지 개수 조회
     * @return 드롭 카운트
     */
    uint64_t getDroppedCount() const;

    /**
     * @brief 쓰기 완료된 메시지 개수 조회
     * @return 쓰기 카운트
     */
    uint64_t getWrittenCount() const;

    /**
     * @brief 총 쓰기 바이트 수 조회
     * @return 바이트 수
     */
    uint64_t getBytesWritten() const;

    /**
     * @brief 파일이 열려 있는지 확인
     * @return 열려 있으면 true
     */
    bool isOpen() const;

private:
    /**
     * @brief 백그라운드 쓰기 스레드 루프
     */
    void writerLoop();

    std::string filepath_;                          ///< 쓰기 대상 파일 경로
    size_t queueCapacity_;                          ///< 큐 최대 용량
    std::queue<BagMessage> messageQueue_;           ///< 메시지 큐
    mutable std::mutex queueMutex_;                 ///< 큐 보호 뮤텍스
    std::condition_variable cv_;                    ///< 조건 변수
    std::thread writerThread_;                      ///< 백그라운드 스레드
    std::atomic<bool> running_{false};              ///< 실행 상태
    std::ofstream ofs_;                             ///< 파일 스트림

    // 통계
    std::atomic<uint64_t> droppedCount_{0};         ///< 드롭된 메시지 수
    std::atomic<uint64_t> writtenCount_{0};         ///< 쓰기 완료된 메시지 수
    std::atomic<uint64_t> bytesWritten_{0};         ///< 총 쓰기 바이트 수
};

} // namespace mxrc::core::logging

#endif // MXRC_CORE_LOGGING_CORE_ASYNCWRITER_H
