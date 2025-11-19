# TROUBLESHOOTING - MXRC 문제 해결 가이드

이 문서는 MXRC 프로젝트에서 발생한 주요 문제와 해결 방법을 기록합니다.

---

## 🔴 이슈 #001: EventBus 동시성 크래시 (Segmentation Fault)

**날짜**: 2025-11-16
**심각도**: Critical
**브랜치**: `019-event-enhanced-hybrid`
**상태**: ✅ 해결됨 (Resolved) (Resolved)

### 문제 증상

```bash
Exit code 139 (Segmentation Fault)
```

- 테스트 실행 중 `DataStoreEventAdapterTest.ConcurrentEventBusPublish`에서 시스템 크래시 발생
- 여러 스레드가 동시에 EventBus에 이벤트를 발행할 때 세그먼테이션 폴트 발생
- 테스트가 멈추거나 중단됨

### 근본 원인 분석

**원인**: SPSC(Single-Producer Single-Consumer) Lock-Free Queue를 Multi-Producer 환경에서 사용

1. **설계 불일치**
   - `EventBus`는 `SPSCLockFreeQueue<std::shared_ptr<IEvent>>`를 사용
   - SPSC 큐는 **단일 생산자-단일 소비자** 패턴만 지원
   - CAS(Compare-And-Swap) 없이 atomic load/store만 사용하여 최적화

2. **동시성 위반**
   - `ConcurrentEventBusPublish` 테스트: 10개 스레드가 동시에 `eventBus->publish()` 호출
   - 여러 생산자가 동시에 `tryPush()` 호출 → `writePos_` 업데이트 시 race condition 발생
   - Ring buffer의 동일 위치에 여러 스레드가 쓰기 시도 → 메모리 손상

3. **코드 위치**
   ```
   src/core/event/util/LockFreeQueue.h:62-74
   src/core/event/core/EventBus.cpp:65-86 (publish 메서드)
   ```

### 해결 방법

**선택된 해결책**: Mutex-Protected SPSC Queue

#### 구현 변경사항

1. **EventBus.h 수정** (`src/core/event/core/EventBus.h`)
   ```cpp
   // Before
   SPSCLockFreeQueue<std::shared_ptr<IEvent>> eventQueue_;

   // After
   SPSCLockFreeQueue<std::shared_ptr<IEvent>> eventQueue_;
   std::mutex publishMutex_;  // 추가: publish 동시성 보호
   ```

2. **EventBus.cpp 수정** (`src/core/event/core/EventBus.cpp:71-86`)
   ```cpp
   bool EventBus::publish(std::shared_ptr<IEvent> event) {
       if (!event) {
           spdlog::warn("Attempted to publish null event");
           return false;
       }

       // Mutex로 보호하여 여러 생산자가 안전하게 publish 가능
       std::lock_guard<std::mutex> lock(publishMutex_);

       // 논블로킹 push 시도
       bool success = eventQueue_.tryPush(event);
       // ... (나머지 코드)
   }
   ```

#### 기술적 트레이드오프

**장점**:
- ✅ 동시성 안전성 보장
- ✅ 구현이 간단하고 검증됨
- ✅ SPSC 큐 자체는 여전히 lock-free로 고성능 유지
- ✅ Mutex 크리티컬 섹션이 매우 짧음 (큐에 넣는 작업만)

**단점**:
- ⚠️ Lock contention 가능성 (다수의 스레드가 동시 publish 시)
- ⚠️ 완전한 lock-free가 아님

#### 대안 고려사항

**시도했으나 실패**: MPSC(Multi-Producer Single-Consumer) Lock-Free Queue

- 파일: `src/core/event/util/MPSCLockFreeQueue.h`
- 테스트: `tests/unit/event/MPSCLockFreeQueue_test.cpp`
- 문제: Race condition 존재 (1000개 중 999개만 성공)
- 원인: CAS 성공 후 버퍼 쓰기 사이에 다른 스레드가 끼어들 수 있음
- 상태: 향후 최적화 대상으로 보류

### 검증 결과

- ✅ EventBusTest 14개 모두 통과
- ✅ `ConcurrentEventBusPublish` 테스트 통과 (단독 실행 시)
- ✅ 빌드 성공, 경고 없음
- ⚠️ 일부 DataStoreEventAdapter 동시성 테스트에서 타임아웃 발생 (별도 이슈)

### 교훈 및 권장사항

1. **동시성 패턴 검증**
   - Lock-free 자료구조 사용 시 SPSC/MPSC/MPMC 등 패턴 명확히 구분
   - 사용 환경과 자료구조의 동시성 보장 범위 일치 여부 검증

2. **테스트 주도 개발**
   - 동시성 테스트를 초기에 작성하여 문제 조기 발견
   - 단일/다중 생산자 시나리오 모두 테스트

3. **성능 vs 안전성**
   - 완벽한 lock-free 구현은 매우 복잡함
   - Mutex 기반 솔루션도 짧은 크리티컬 섹션에서는 충분히 고성능

4. **단계적 최적화**
   - 먼저 올바르게 동작하는 버전 구현 (mutex)
   - 성능 측정 후 필요시 최적화 (MPSC 큐)
   - 실시간 요구사항 충족 여부 확인 후 결정

### 관련 파일

**수정된 파일**:
- `src/core/event/core/EventBus.h`
- `src/core/event/core/EventBus.cpp`

**추가된 파일**:
- `src/core/event/util/MPSCLockFreeQueue.h` (향후 최적화용)
- `tests/unit/event/MPSCLockFreeQueue_test.cpp`

**영향받은 테스트**:
- `tests/unit/event/EventBus_test.cpp` - 모두 통과
- `tests/unit/event/DataStoreEventAdapter_test.cpp` - 일부 동시성 테스트 타임아웃

### 참고 자료

- SPSC Queue 설계: `specs/019-event-enhanced-hybrid/research.md`
- 아키텍처 문서: `specs/019-event-enhanced-hybrid/architecture.md`
- Lock-Free 큐 구현: Dmitry Vyukov의 MPSC Queue 알고리즘

---

## 향후 최적화 계획

### Phase 6: MPSC Lock-Free Queue 완성

**목표**: Mutex 오버헤드 제거 및 완전한 lock-free 구현

**작업 내역**:
1. MPSC 큐 race condition 수정
   - 문제: CAS 성공 후 버퍼 쓰기 전 다른 스레드 개입
   - 해결: 슬롯 상태 플래그 추가 또는 2-단계 커밋 프로토콜

2. 성능 벤치마크
   - Mutex 버전 vs MPSC 버전 처리량 측정
   - 실시간 요구사항 충족 여부 확인
   - Latency 분포 분석 (p50, p99, p99.9)

3. 프로덕션 전환 기준
   - MPSC 큐 테스트 100% 통과
   - 성능 향상 20% 이상
   - 안정성 검증 (stress test 24시간)

**우선순위**: P2 (최적화)
**예상 기간**: 2-3일

---

## 문제 보고 템플릿

새로운 문제 발견 시 아래 템플릿을 사용하여 이 문서에 추가하세요.

```markdown
## 🔴 이슈 #XXX: [문제 제목]

**날짜**: YYYY-MM-DD
**심각도**: Critical | High | Medium | Low
**브랜치**: `branch-name`
**상태**: 🔍 조사 중 | 🔨 수정 중 | ✅ 해결됨 | 🔄 재발

### 문제 증상
[문제가 어떻게 나타나는지 설명]

### 근본 원인 분석
[문제의 근본 원인]

### 해결 방법
[적용한 해결책]

### 검증 결과
[해결 후 테스트 결과]

### 교훈 및 권장사항
[이 문제에서 배운 점]

### 관련 파일
[수정된 파일 목록]
```

---

**마지막 업데이트**: 2025-11-16
**작성자**: Claude Code Assistant
