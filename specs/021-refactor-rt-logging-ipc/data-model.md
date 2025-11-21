# Data Model: RT/Non-RT Architecture

**Date**: 2025-11-21
**Status**: Draft
**Relevant Plan**: [plan.md](plan.md)

이 문서는 `021-refactor-rt-logging-ipc` 기능에서 사용되는 주요 데이터 엔티티를 정의합니다.

## 1. LogRecord

RT 프로세스에서 생성되어 공유 메모리 링 버퍼를 통해 Non-RT 프로세스로 전달되는 구조화된 로그 메시지입니다.

- **설명**: 시스템의 특정 이벤트에 대한 정보를 담고 있는 불변(immutable) 데이터 구조체입니다. 효율적인 직렬화를 위해 고정 크기 필드를 우선적으로 사용합니다.
- **필드**:
    | Field | Type | Description |
    | :--- | :--- | :--- |
    | `timestamp` | `uint64_t` | 이벤트 발생 시점의 POSIX timestamp (nanoseconds). |
    | `level` | `uint8_t` | 로그 레벨 (예: TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL). |
    | `source_process` | `uint8_t` | 로그 발생 프로세스 (0: RT, 1: Non-RT). |
    | `thread_id` | `uint64_t` | 로그를 발생시킨 스레드의 ID. |
    | `message` | `char[256]`| 로그 메시지 본문. 고정 크기 배열을 사용하여 직렬화 오버헤드를 최소화합니다. |

- **상태 전환**: 해당 없음 (LogRecord는 생성 후 변경되지 않음).

## 2. IpcMessage

프로세스 간 통신(IPC)을 위해 Lock-Free 큐를 통해 전달되는 메시지입니다. 로그 외에 Heartbeat, 설정 변경 등 다양한 종류의 데이터를 전달하는 데 사용됩니다.

- **설명**: 프로세스 간의 제어 및 데이터 교환을 위한 범용 메시지 구조체. `type` 필드를 통해 메시지의 종류를 구분합니다.
- **필드**:
    | Field | Type | Description |
    | :--- | :--- | :--- |
    | `type` | `uint8_t` | 메시지 종류를 나타내는 열거형 (e.g., `HEARTBEAT`, `CONFIG_UPDATE_LOG_LEVEL`). |
    | `payload` | `union` | 메시지 종류에 따른 데이터. 예를 들어, `CONFIG_UPDATE_LOG_LEVEL`의 경우 `(module_id, new_level)` 쌍을 포함할 수 있습니다. |

- **상태 전환**: 해당 없음.

## 3. SharedMemoryHeader

공유 메모리 링 버퍼의 시작 부분에 위치하여 버퍼의 상태를 관리하는 헤더 구조체입니다.

- **설명**: 링 버퍼의 동시성 제어를 위한 메타데이터를 포함합니다. 모든 멤버는 원자적(atomic)으로 접근되어야 합니다.
- **필드**:
    | Field | Type | Description |
    | :--- | :--- | :--- |
    | `head` | `std::atomic<size_t>` | 다음 로그가 쓰여질 위치(인덱스). |
    | `tail` | `std::atomic<size_t>` | 다음에 읽어야 할 로그의 위치(인덱스). |
    | `dropped_count`| `std::atomic<size_t>` | 버퍼 오버플로우로 인해 덮어쓰여진 로그의 수. |
