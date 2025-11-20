# 요구사항 체크리스트 - RT 아키텍처 안정성 개선

**기능**: RT 아키텍처 안정성 개선
**생성일**: 2025-11-20

## 사용자 스토리 검증

### ✅ 사용자 스토리 1 - 메모리 안전성 보장 (P1)
- [ ] createFromPeriods()가 std::unique_ptr 반환
- [ ] 스마트 포인터로 자동 메모리 해제 검증
- [ ] 예외 발생 시 부분 생성 객체 정리 검증
- [ ] shared memory RAII 패턴 적용
- [ ] Valgrind로 메모리 누수 0바이트 검증

### ✅ 사용자 스토리 2 - 동시성 안전성 보장 (P1)
- [ ] running_ 플래그가 std::atomic<bool>
- [ ] stop() 호출 시 원자적 동기화 검증
- [ ] RTDataStore 동시 접근 시 데이터 레이스 없음
- [ ] state transition callback 재진입 안전성
- [ ] ThreadSanitizer로 데이터 레이스 0건 검증

### ✅ 사용자 스토리 3 - 에러 복구 메커니즘 (P2)
- [ ] RT priority 설정 실패 시 ERROR 상태 전환
- [ ] ERROR 상태에서 run() 실행 거부
- [ ] shared memory 생성 실패 시 명확한 에러 코드
- [ ] RESET 이벤트로 INIT 상태 복구 가능

### ✅ 사용자 스토리 4 - 타입 안전성 강화 (P3)
- [ ] DataKey 타입 불일치 시 에러 반환
- [ ] 타입 메타데이터 등록 및 검증 메커니즘
- [ ] DataKey 범위 초과 시 경계 검사

## 기능적 요구사항 검증

### 메모리 관리
- [ ] **FR-001**: createFromPeriods() std::unique_ptr 반환
- [ ] **FR-002**: RTDataStoreShared RAII 패턴, 소멸자 shm_unlink
- [ ] **FR-003**: 생성 실패 시 부분 리소스 자동 정리
- [ ] **FR-004**: 모든 동적 메모리 스마트 포인터 관리

### 동시성 안전성
- [ ] **FR-005**: running_ 플래그 std::atomic<bool>
- [ ] **FR-006**: stop() 원자적 동기화
- [ ] **FR-007**: RTDataStore read/write 데이터 레이스 없음
- [ ] **FR-008**: state transition callback 재진입 안전 처리

### 에러 처리
- [ ] **FR-009**: RT priority 실패 시 ERROR 상태 전환
- [ ] **FR-010**: ERROR 상태에서 run() 거부 및 -1 반환
- [ ] **FR-011**: shared memory 실패 시 명확한 에러 코드
- [ ] **FR-012**: run() 에러 발생 시 ERROR 상태 전환 및 중단

### 타입 안전성
- [ ] **FR-013**: DataKey-타입 불일치 검증
- [ ] **FR-014**: DataKey 범위 초과 에러 반환
- [ ] **FR-015**: 타입 메타데이터 등록/검증 메커니즘

### 테스트 및 검증
- [ ] **FR-016**: AddressSanitizer 메모리 안전성 검증
- [ ] **FR-017**: ThreadSanitizer 데이터 레이스 검증
- [ ] **FR-018**: Valgrind 메모리 누수 검증

## 성공 기준 검증

- [ ] **SC-001**: AddressSanitizer 빌드로 47 tests 실행 시 메모리 에러 0건
- [ ] **SC-002**: ThreadSanitizer 빌드로 멀티스레드 테스트 실행 시 데이터 레이스 0건
- [ ] **SC-003**: Valgrind로 RTExecutive 생성/소멸 1000회 반복 시 메모리 누수 0바이트
- [ ] **SC-004**: RT priority 설정 실패 시 ERROR 상태 전환 및 run() 실행 거부
- [ ] **SC-005**: shared memory 생성 실패 시 shm_unlink 호출 확인 (strace)
- [ ] **SC-006**: 잘못된 타입으로 RTDataStore 접근 시 에러 반환율 100%
- [ ] **SC-007**: 멀티스레드 환경에서 run()/stop() 1000회 반복 시 크래시 0건

## 엣지 케이스 검증

- [ ] RTExecutive 생성 중 예외 발생 시 리소스 정리
- [ ] mmap 실패 후 shm_unlink 호출 확인
- [ ] SHUTDOWN 상태에서 재사용 시도 시 에러 처리
- [ ] running_ 설정과 state transition 타이밍 윈도우 없음
- [ ] sequence number uint64_t 오버플로우 처리
- [ ] 멀티스레드 state transition callback 동시 호출 안전성
- [ ] RT priority 실패 후 run() 계속 실행 방지

## 구현 검증

### Phase 1: 메모리 안전성
- [ ] createFromPeriods() 반환 타입 변경
- [ ] RTDataStoreShared RAII 구현
- [ ] 테스트 코드 스마트 포인터 업데이트
- [ ] AddressSanitizer 검증 통과

### Phase 2: 동시성 안전성
- [ ] running_ std::atomic 변경
- [ ] stop() 동기화 로직 추가
- [ ] callback 재진입 방지 로직
- [ ] ThreadSanitizer 검증 통과

### Phase 3: 에러 처리
- [ ] RT priority 실패 시 ERROR 전환
- [ ] ERROR 상태 run() 거부
- [ ] shared memory 에러 처리 강화
- [ ] 에러 복구 테스트 추가

### Phase 4: 타입 안전성
- [ ] DataKey 타입 메타데이터 시스템
- [ ] RTDataStore 타입 검증 로직
- [ ] 타입 불일치 테스트 추가

## 제외 사항 확인

- [x] 성능 최적화는 범위 외
- [x] API 변경 최소화
- [x] Non-RT 프로세스는 범위 외

## 가정 확인

- [x] AddressSanitizer, ThreadSanitizer CMake 설정됨
- [x] RT 권한 없이도 테스트 가능
- [x] C++17 이상 사용 가능

## 최종 승인

- [ ] 모든 요구사항 구현 완료
- [ ] 모든 성공 기준 달성
- [ ] 모든 엣지 케이스 처리
- [ ] 코드 리뷰 완료
- [ ] 문서화 완료
