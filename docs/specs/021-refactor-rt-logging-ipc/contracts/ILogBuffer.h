
#ifndef MXRC_CORE_IPC_ILOGBUFFER_H
#define MXRC_CORE_IPC_ILOGBUFFER_H

#include <cstddef>
#include <cstdint>

namespace mxrc::core::ipc {

// 데이터 모델에 정의된 로그 레코드 구조체
// 실제 구현은 data-model.md를 따라야 함
struct LogRecord {
    uint64_t timestamp;
    uint8_t level;
    uint8_t source_process;
    uint64_t thread_id;
    char message[256];
};

/**
 * @class ILogBuffer
 * @brief 실시간-안전 공유 메모리 링 버퍼에 대한 인터페이스.
 *
 * 이 인터페이스는 생산자(RT 프로세스)와 소비자(Non-RT 프로세스) 간의
 * 로그 데이터 교환을 위한 계약을 정의합니다.
 */
class ILogBuffer {
public:
    virtual ~ILogBuffer() = default;

    /**
     * @brief 버퍼에 로그 레코드를 쓴다 (생산자 측).
     * 이 함수는 non-blocking이며 real-time safe해야 한다.
     * @param record 쓸 로그 레코드.
     * @return 쓰기 성공 시 true, 버퍼가 꽉 찼을 경우 false (오래된 데이터를 덮어쓰는 정책).
     */
    virtual bool write(const LogRecord& record) = 0;

    /**
     * @brief 버퍼에서 로그 레코드를 읽는다 (소비자 측).
     * @param record 읽어온 로그 레코드를 저장할 변수.
     * @return 읽기 성공 시 true, 버퍼가 비어있을 경우 false.
     */
    virtual bool read(LogRecord& record) = 0;

    /**
     * @brief 오버플로우로 인해 유실된 로그의 수를 반환한다.
     * @return 유실된 로그의 수.
     */
    virtual size_t get_dropped_count() const = 0;
};

} // namespace mxrc::core::ipc

#endif // MXRC_CORE_IPC_ILOGBUFFER_H
