# Implementation Plan: RT/Non-RT 아키텍처 및 로깅 시스템 개선

**Branch**: `021-refactor-rt-logging-ipc` | **Date**: 2025-11-21 | **Spec**: [spec.md](spec.md)
**Status**: Planning
**Progress**: Phase 0 (Research) → Phase 1 (Design) → Phase 2 (Tasks)
**Last Updated**: 2025-11-21
**Input**: Feature specification from `/home/tory/workspace/mxrc/mxrc/specs/021-refactor-rt-logging-ipc/spec.md`

---

## 작성 가이드라인 ⚠️

**언어 사용 규칙**:
- 모든 문서는 **한글**로 작성합니다
- **기술 용어만 영어로 표기**합니다 (예: Task, Action, Sequence, API, JSON, CMake 등)
- 일반 설명, 구현 계획, 설계 결정은 모두 한글로 작성합니다

---

## Summary

본 계획은 RT/Non-RT 프로세스 간의 통신 및 로깅 아키텍처를 개선하는 것을 목표로 합니다. 주요 구현 내용은 RT 프로세스의 로깅이 실시간 성능을 저해하지 않도록 Lock-Free 공유 메모리 큐를 도입하고, 두 프로세스 간의 상태를 감시하기 위한 Heartbeat 메커니즘을 추가하며, 모든 로그를 구조화된 JSON 형식으로 통합하여 관측 가능성을 높이는 것입니다.

## Technical Context

**Language/Version**: C++20
**Primary Dependencies**:
- `spdlog`
- `gtest`
- `TBB`
- **`Boost.Lockfree`**: Lock-Free 큐 구현을 위해 이 라이브러리를 사용합니다. (`research.md` 참고)
**Storage**: Log 저장을 위한 파일 시스템
**Testing**: Google Test
**Target Platform**: Ubuntu 24.04 LTS with `PREEMPT_RT` kernel
**Project Type**: Single C++ Project
**Constraints**:
- RT 프로세스 내에서 블로킹(blocking) 호출 절대 금지.
- 두 개의 독립적인 프로세스(rt, non-rt) 간의 안정적인 통신 보장.
- **실시간-안전 공유 메모리 링 버퍼**: `mmap`으로 파일을 공유하고, `std::atomic` 인덱스를 사용하는 링 버퍼 설계를 따릅니다. (`research.md` 참고)

## Constitution Check

*이 단계에서는 프로젝트의 기본 원칙(constitution)을 확인합니다. 현재 spec은 기존 아키텍처를 개선하는 것이므로, 주요 위반 사항은 없을 것으로 예상됩니다.*

- **통과**: 새 외부 의존성(`Boost`) 추가는 신중해야 하지만, Lock-free 구현의 복잡성을 고려할 때 잘 알려진 라이브러리 사용은 정당화될 수 있습니다.

## Project Structure

### Documentation (this feature)

```text
specs/021-refactor-rt-logging-ipc/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── ILogBuffer.h
│   └── IIpcQueue.h
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/core/
├── ipc/                  # 신규: IPC 관련 컴포넌트 (Lock-Free 큐 구현)
│   ├── shm_ring_buffer.h
│   └── shm_ring_buffer.cpp
├── logging/
│   ├── handlers/         # 신규: RT 로그 핸들러 등
│   │   └── rt_log_handler.h
│   └── sinks/            # spdlog 커스텀 sink
│       └── shm_sink.h
├── rt/
│   └── health_monitor.h  # 신규: Heartbeat 모니터
└── nonrt/
    ├── log_consumer.h    # 신규: 공유 메모리 로그 소비자
    └── health_monitor.h  # 신규: Heartbeat 모니터
```

**Structure Decision**: 기존 `src/core` 구조를 확장하여 `ipc` 디렉토리를 신설하고, `logging`, `rt`, `nonrt` 각 계층에 필요한 모듈을 추가하는 방식을 사용합니다.

## Complexity Tracking

*해당 없음*
