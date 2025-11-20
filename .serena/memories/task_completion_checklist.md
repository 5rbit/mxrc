# MXRC 작업 완료 시 체크리스트

## 코드 작성 후 필수 단계

### 1. 컴파일 검증
```bash
cd build
make -j$(nproc)
```
- 컴파일 에러 없음
- 경고 메시지 최소화

### 2. 테스트 실행
```bash
./run_tests
```
- **모든 기존 테스트 통과 필수** (195/195)
- 새로운 기능에 대한 단위 테스트 작성
- 실패 케이스 테스트 포함

### 3. 메모리 안전성 검증
- AddressSanitizer 자동 활성화됨
- 테스트 실행 시 메모리 누수, 버퍼 오버플로우 자동 감지
- 경고/에러 없이 테스트 통과 확인

### 4. 로그 검증
```bash
# 로그 파일 확인
cat logs/mxrc.log
tail -f logs/mxrc.log
```
- 적절한 로그 레벨 사용 확인
- 에러 로그 없는지 확인

### 5. 코드 리뷰 자가 체크리스트
- [ ] RAII 원칙 준수 (자동 리소스 관리)
- [ ] 스마트 포인터 사용 (shared_ptr, unique_ptr)
- [ ] 인터페이스 기반 설계
- [ ] 명확한 네이밍 (클래스, 함수, 변수)
- [ ] 적절한 주석 및 문서화
- [ ] 테스트 커버리지 충분

### 6. Git 커밋 전
```bash
git status
git diff
```
- 변경 사항 확인
- 의도하지 않은 파일 제외 (.gitignore 확인)
- 커밋 메시지 명확하게 작성

## 특정 계층별 체크리스트

### Action Layer
- [ ] IAction 인터페이스 구현
- [ ] execute(), cancel(), getStatus() 메서드 구현
- [ ] ActionFactory에 등록
- [ ] ActionExecutor 테스트 통과

### Sequence Layer
- [ ] SequenceDefinition 올바르게 정의
- [ ] SequenceEngine 실행 검증
- [ ] 조건부 분기 테스트
- [ ] 재시도 정책 테스트

### Task Layer
- [ ] TaskDefinition 정의
- [ ] 실행 모드 설정 (ONCE, PERIODIC, TRIGGERED)
- [ ] TaskExecutor 실행 검증
- [ ] 상태 전환 테스트

### DataStore
- [ ] 스레드 안전성 확인
- [ ] TTL/LRU 정책 동작 검증
- [ ] 접근 제어 테스트
- [ ] 메트릭 수집 확인

## 문서 업데이트

### 필요 시 업데이트할 문서
- [ ] README.md (새로운 기능 추가 시)
- [ ] CLAUDE.md (개발 가이드 변경 시)
- [ ] docs/specs/ (사양 변경 시)
- [ ] 인라인 코드 주석 (복잡한 로직 설명)