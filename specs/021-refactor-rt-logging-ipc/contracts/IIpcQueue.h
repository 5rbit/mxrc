
#ifndef MXRC_CORE_IPC_IIPCQUEUE_H
#define MXRC_CORE_IPC_IIPCQUEUE_H

#include <cstdint>

namespace mxrc::core::ipc {

// 데이터 모델에 정의된 IPC 메시지 구조체
// 실제 구현은 data-model.md를 따라야 함
struct IpcMessage {
    enum class MessageType : uint8_t {
        HEARTBEAT,
        CONFIG_UPDATE_LOG_LEVEL
    };

    MessageType type;
    union {
        struct {
            uint32_t module_id;
            uint8_t new_level;
        } log_config;
        // 다른 payload 타입 추가 가능
    } payload;
};

/**
 * @class IIpcQueue
 * @brief 프로세스 간 통신을 위한 Lock-Free 큐 인터페이스.
 *
 * 이 인터페이스는 Heartbeat, 설정 변경 등 제어 메시지를
 * RT 프로세스와 Non-RT 프로세스 간에 교환하기 위한 계약을 정의합니다.
 */
class IIpcQueue {
public:
    virtual ~IIpcQueue() = default;

    /**
     * @brief 큐에 메시지를 푸시한다.
     * 이 함수는 non-blocking이며 real-time safe해야 한다.
     * @param msg 보낼 메시지.
     * @return 푸시 성공 시 true, 큐가 꽉 찼을 경우 false.
     */
    virtual bool push(const IpcMessage& msg) = 0;

    /**
     * @brief 큐에서 메시지를 팝한다.
     * @param msg 읽어온 메시지를 저장할 변수.
     * @return 팝 성공 시 true, 큐가 비어있을 경우 false.
     */
    virtual bool pop(IpcMessage& msg) = 0;
};

} // namespace mxrc::core::ipc

#endif // MXRC_CORE_IPC_IIPCQUEUE_H
