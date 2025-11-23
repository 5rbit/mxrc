# MXRC - Mission and eXecution Robot Controller

[![Build Status](https://github.com/5rbit/mxrc/actions/workflows/main.yml/badge.svg)](https://github.com/5rbit/mxrc/actions/workflows/main.yml)

MXRC(Mission and eXecution Robot Controller)는 C++20 기반의 고성능, 고신뢰성 범용 로봇 제어 시스템입니다. 실시간(Real-Time) 제어의 예측성과 안정성을 보장하면서, 비실시간(Non-Real-Time) 작업의 유연성을 확보하기 위해 **듀얼-프로세스 아키텍처**를 핵심 설계로 채택했습니다.

## 주요 특징

-   **듀얼-프로세스 아키텍처**: 실시간(RT)과 비실시간(Non-RT) 작업을 별도의 프로세스로 분리하여 시스템 안정성을 극대화합니다.
-   **계층형 실행 구조**: `Task` → `Sequence` → `Action`의 3단계 계층 구조를 통해 명확한 책임 분리와 높은 모듈성을 제공합니다.
-   **고성능 IPC**: 공유 메모리(`DataStore`)와 Lock-Free 큐(`EventBus`)를 통해 두 프로세스 간에 효율적인 통신을 수행합니다.
-   **실시간 필드버스 지원**: EtherCAT을 통해 다중 축 모터 및 센서와의 정밀한 실시간 통신 및 동기화(DC)를 지원합니다.
-   **고가용성(HA)**: Watchdog 및 상호 프로세스 모니터링을 통해 장애 발생 시 시스템을 안전하게 복구합니다.
-   **포괄적인 모니터링**: Prometheus 및 Grafana 연동을 통해 시스템의 모든 핵심 지표를 실시간으로 모니터링합니다.
-   **최신 C++ 적용**: C++20 표준을 기반으로 하며, RAII, 스마트 포인터, 동시성 라이브러리(TBB, Boost.Lockfree) 등을 적극 활용합니다.

## 아키텍처

MXRC는 **RT 프로세스(`mxrc-rt`)**와 **Non-RT 프로세스(`mxrc-nonrt`)** 두 개의 독립된 프로세스로 구성됩니다.

```mermaid
graph TD
    subgraph "External Systems"
        UI[User Interface / API]
        Metrics[Prometheus / Grafana]
        Systemd[systemd]
    end

    subgraph "MXRC System"
        direction LR
        
        subgraph "Non-RT Process (mxrc-nonrt)"
            direction TB
            NRE[NonRTExecutive]
            NRE -- Manages --> Config[Config]
            NRE -- Manages --> Logging[Logging]
            NRE -- Manages --> Monitoring[Monitoring]
            NRE -- Manages --> NHA[HA Agent]
        end

        subgraph "RT Process (mxrc-rt)"
            direction TB
            RTE[RTExecutive]
            subgraph "Execution Layers"
                T(Task Layer) --> S(Sequence Layer) --> A(Action Layer)
            end
            RTE -- Drives --> T
            A -- Controls --> EC[EtherCAT]
        end

        subgraph "Inter-Process Communication (IPC)"
            direction TB
            DS[DataStore<br>(Shared Memory)]
            EB[EventBus<br>(Lock-Free Queue)]
        end
    end

    UI --> |Commands| NRE
    NRE <-->|State/Config| DS
    NRE <--|Events| EB
    
    RTE <-->|State/Data| DS
    RTE -->|Events| EB
    
    Monitoring -- Exposes --> Metrics
    Systemd -- Manages --> NRE & RTE
```

더 상세한 아키텍처 정보는 아래 문서를 참고하십시오:
-   **[통합 시스템 아키텍처](./docs/architecture/unified_system_architecture.md)**
-   **[모듈별 아키텍처](./docs/architecture/)**

## 기술 스택

-   **언어**: C++20
-   **빌드 시스템**: CMake 3.16+
-   **컴파일러**: GCC 11+ 또는 Clang 14+
-   **주요 라이브러리**:
    -   **로깅**: spdlog
    -   **테스트**: Google Test
    -   **동시성**: Intel TBB, Boost.Lockfree
    -   **IPC**: 공유 메모리, Lock-Free 큐
-   **운영 환경**: Ubuntu 24.04 LTS (PREEMPT_RT 권장)

## 빌드 및 실행

### Linux (Ubuntu)
```bash
# 의존성 설치
sudo apt-get update && sudo apt-get install -y cmake libspdlog-dev libgtest-dev libtbb-dev

# 빌드
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# 테스트 실행
./run_tests

# 프로그램 실행
./mxrc
```

### macOS (Homebrew)
```bash
# 의존성 설치
brew install cmake spdlog googletest tbb

# TBB 및 googletest 경로를 지정하여 빌드
TBB_ROOT=$(brew --prefix tbb) && \
GTEST_ROOT=$(brew --prefix googletest) && \
mkdir -p build && \
cd build && \
cmake .. -DTBB_DIR=${TBB_ROOT}/lib/cmake/TBB -DCMAKE_PREFIX_PATH=${GTEST_ROOT} && \
make -j$(sysctl -n hw.ncpu)

# 테스트 실행
./run_tests
```

## 테스트 현황

MXRC 프로젝트는 엄격한 테스트 주도 개발(TDD) 원칙을 따르며, 높은 코드 품질과 안정성을 보장하기 위해 광범위한 단위, 통합 및 벤치마크 테스트를 수행합니다. 현재 **총 1057개**의 테스트가 성공적으로 통과되었습니다.

### 모듈별 테스트 현황 (2025-11-23 기준)

| 모듈         | 단위 테스트 | 통합 테스트 | 벤치마크 | 총계 |
| :----------- | :---------- | :---------- | :------- | :--- |
| Action       | 23          | 4           | 0        | 27   |
| DataStore    | 123         | 3           | 13       | 139  |
| EtherCAT     | 47          | 0           | 0        | 47   |
| Event        | 138         | 22          | 0        | 160  |
| HA           | 41          | 0           | 0        | 41   |
| Logging      | 115         | 10          | 8        | 133  |
| Monitoring   | 81          | 8           | 0        | 89   |
| Perf         | 34          | 7           | 0        | 41   |
| RT           | 59          | 8           | 0        | 67   |
| Sequence     | 21          | 0           | 0        | 21   |
| Systemd      | 32          | 124         | 0        | 156  |
| Task         | 67          | 1           | 0        | 68   |
| Tracing      | 60          | 9           | 0        | 69   |
| **총계**     | **841**     | **195**     | **21**   | **1057** |

*(참고: HA, EtherCAT, Sequence, Task 모듈의 통합 테스트 수는 스크립트의 테스트 스위트 매핑 또는 gtest_list_tests 출력 해석 방식에 따라 실제 통합 테스트 파일 수와 약간의 차이가 있을 수 있습니다.)*

### 테스트 실행

```bash
# build 디렉토리에서 전체 테스트 실행
./run_tests

# 특정 모듈(예: ActionExecutor)의 단위 테스트만 실행
./run_tests --gtest_filter=ActionExecutor*

# 특정 통합 테스트만 실행 (예: Systemd 모듈의 StartupOrderTest)
./run_tests --gtest_filter=StartupOrderTest*

# 간략한 결과만 확인
./run_tests --gtest_brief=1
```

## 문서

프로젝트의 모든 아키텍처 문서는 `docs/architecture` 디렉토리에서 확인할 수 있습니다.

-   **[통합 시스템 아키텍처 (Unified System Architecture)](./docs/architecture/unified_system_architecture.md)**: 시스템 전체의 동작 방식과 모듈 간의 상호작용을 설명합니다.
-   **[DataStore 아키텍처](./docs/architecture/datastore.md)**: IPC의 핵심인 DataStore 모듈을 설명합니다.
-   **[실행 계층 아키텍처](./docs/architecture/)**: `Task`, `Sequence`, `Action` 계층을 각각 설명합니다.
-   **[EtherCAT 아키텍처](./docs/architecture/ethercat.md)**: 실시간 필드버스 통신 모듈을 설명합니다.