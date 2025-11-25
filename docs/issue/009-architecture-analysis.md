# Issue 009: 아키텍처 및 프로젝트 방향성 분석 보고서

**작성일**: 2025-11-25
**작성자**: Antigravity (AI Assistant)
**관련 Feature**: 
- `016-pallet-shuttle-control`
- `019-architecture-improvements`
- `022-fix-architecture-issues`

---

## 1. 개요

현재 프로젝트는 **"핵심 아키텍처 안정화(Feature 022)"**와 **"비즈니스 로직 구현(Feature 016)"**이 동시에 진행되는 과도기에 있습니다. 본 보고서는 현재 아키텍처의 강점과 잠재적 리스크를 분석하고, 향후 개발 방향에 대한 제언을 담고 있습니다.

## 2. 현황 분석

### 2.1 긍정적인 점 (Strengths)

1.  **견고한 계층 구조**
    *   `Action` → `Sequence` → `Task`로 이어지는 제어 흐름이 명확합니다.
    *   각 계층의 인터페이스(`IAction`, `ITask`)가 깔끔하게 정의되어 있어 확장성이 높습니다.

2.  **안정성 중심 설계**
    *   `Feature 022`를 통해 Systemd 시작 순서, RT/Non-RT 분리 등 시스템 안정성을 최우선으로 다루고 있습니다.
    *   이는 로봇 제어 시스템에서 가장 중요한 신뢰성을 확보하는 올바른 접근입니다.

3.  **모던 C++ 표준 준수**
    *   `GEMINI.md` 가이드라인을 준수하여 Singleton 패턴의 남용을 피하고 `shared_ptr` 팩토리 패턴을 사용하고 있습니다.
    *   `tbb`, `spdlog` 등 검증된 고성능 라이브러리를 적절히 활용하고 있습니다.

4.  **Facade 패턴 적용**
    *   `DataStore`가 내부적으로 `ExpirationManager`, `AccessControlManager` 등으로 책임을 분산하고 있어, 거대 클래스(God Object)가 되는 것을 방지하려는 노력이 보입니다.

### 2.2 개선이 필요한 점 및 리스크 (Risks & Improvements)

1.  **DataStore 의존성 및 Accessor 패턴 과도기**
    *   **현황**: `Feature 016(팔렛 셔틀)`은 로봇의 상태 관리와 제어를 위해 `DataStore`를 헤비하게 사용해야 합니다.
    *   **문제점**: `Feature 022`에서 추진 중인 **"Data Accessor 패턴(P2)"**이 아직 완전히 적용되지 않았습니다.
    *   **Risk**: 지금 `016`을 구현하면서 기존 방식(Direct Access)으로 코드를 작성할 경우, 추후 `022` 완료 시 대규모 리팩토링이 필요하여 기술 부채가 발생할 수 있습니다.

2.  **복잡한 상태 관리와 동시성**
    *   `016`의 "행동 의사 결정(Behavior Arbitration)"은 단순한 값 변경이 아닌, 여러 조건에 따른 복잡한 로직 처리가 필요합니다.
    *   이를 단순히 `DataStore` 폴링으로 처리할 경우 RT 성능 저하 및 반응성 문제가 발생할 수 있습니다.

## 3. 제안 사항 (Recommendations)

### 3.1 Accessor 인터페이스 선행 정의
`Feature 016` 구현 시, `DataStore`에 직접 접근하는 코드를 작성하는 대신 `Feature 022`의 철학을 반영한 Accessor를 먼저 정의하고 사용하십시오.

*   **Action Item**: `PalletTaskAccessor`, `RobotStateAccessor` 등의 인터페이스를 먼저 정의합니다.
*   **Effect**: 내부 구현이 변경되더라도 비즈니스 로직(`016`)은 영향을 받지 않게 됩니다.

### 3.2 EventBus 적극 활용
알람 발생, 모드 전환 등 즉각적인 반응이 필요한 로직에는 `DataStore` 폴링보다는 `EventBus`를 활용하십시오.

*   **Action Item**: `Feature 022`의 P3(EventBus 우선순위 큐) 기능을 활용하여 중요 이벤트를 처리합니다.

### 3.3 테스트 격리 강화
단위 테스트 작성 시 `DataStore::createForTest()`를 적극 활용하여 글로벌 상태 의존성을 제거하십시오.

## 4. 결론

현재 프로젝트의 방향성은 매우 올바르며, 아키텍처적으로도 성숙한 단계로 진입하고 있습니다. 다만, `Feature 016` 구현 과정에서 `Feature 022`의 설계 원칙을 철저히 준수한다면, 향후 유지보수 비용을 최소화하고 고품질의 로봇 제어 시스템을 구축할 수 있을 것입니다.
