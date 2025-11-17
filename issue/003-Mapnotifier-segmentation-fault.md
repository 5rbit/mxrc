## 🔴 이슈 #003: MapNotifier 소멸자에서 발생하는 세그멘테이션 폴트

**날짜**: 2025-11-17
**심각도**: High
**브랜치**: `develop` (추정)
**상태**: 🔍 조사 중

### 문제 증상

- 특정 조건에서 `DataStore`가 소멸될 때, 내부적으로 사용되는 `MapNotifier`의 소멸자에서 세그멘테이션 폴트(Segmentation Fault)가 발생합니다.
- 이로 인해 프로그램이 비정상적으로 종료됩니다.
- 주로 동시성 테스트나 특정 이벤트 처리 후 데이터 저장소가 정리되는 과정에서 문제가 관찰됩니다.

```bash
Exit code 139 (Segmentation Fault)
# Backtrace
...
DataStore::~DataStore()
MapNotifier::~MapNotifier()
...
```

### 근본 원인 분석 (가설)

1.  **Dangling Pointer/Reference**: `MapNotifier`가 참조하는 `DataStore`의 다른 멤버(예: 데이터 맵)가 `MapNotifier`보다 먼저 소멸되어, 소멸자에서 유효하지 않은 메모리에 접근할 가능성이 있습니다.
2.  **스레드 경쟁 상태 (Race Condition)**: `DataStore`가 여러 스레드에서 사용되다가 메인 스레드에서 소멸될 때, 다른 스레드가 여전히 `MapNotifier` 또는 관련 리소스에 접근하려고 시도할 수 있습니다.
3.  **이벤트 시스템과의 상호작용**: `MapNotifier`가 `EventBus`와 같은 다른 시스템에 콜백을 등록한 경우, `DataStore`가 소멸된 후에도 해당 콜백이 호출되어 문제가 발생할 수 있습니다.

### 재현 단계 (예상)

1. 여러 스레드를 생성하여 `DataStore`에 동시에 값을 쓰고 읽는 작업을 수행합니다.
2. `DataStore` 인스턴스의 수명이 다하여 소멸자가 호출되도록 합니다.
3. 간헐적으로 세그멘테이션 폴트가 발생합니다.

### 문제 파악 방법

#### 1. TBB 런타임 환경 검증
*   **명령**: `echo $DYLD_LIBRARY_PATH`
*   **목적**: TBB 라이브러리 경로 누락이 SegFault의 흔한 원인이므로, 환경 변수부터 검증.
*   **결과**: 경로가 `/opt/homebrew/opt/tbb/lib:...`로 올바르게 설정된 것을 확인. TBB 로드 실패는 원인이 아님을 확정.

#### 2. 디버거를 이용한 크래시 지점 포착
*   **명령**: `lldb ./run_tests` 실행 후 `run` 명령으로 테스트 시작.
*   **목적**: Segment Fault가 발생한 정확한 시점에 프로세스를 중단시키기 위함.
*   **결과**: LLDB는 **스레드 #2**의 `MapNotifier::notify(SharedData const&)` 함수 내부에서 `EXC_BAD_ACCESS (code=1, address=0x10)` 메시지와 함께 멈춤.

```text
[2025-11-17 21:45:22.504] [action] [info] [DataStoreEventAdapter] Subscribed to ACTION_COMPLETED events (prefix: action.results.)
Process 164 stopped
* thread #2, stop reason = EXC_BAD_ACCESS (code=1, address=0x10)
    frame #0: 0x0000000100208004 run_tests`MapNotifier::notify(SharedData const&) + 128
run_tests`MapNotifier::notify:
->  0x100208004 <+128>: ldr    x8, [x8, #0x10]
    0x100208008 <+132>: blr    x8
    0x10020800c <+136>: b      0x100208010    ; <+140>
    0x100208010 <+140>: b      0x100208014    ; <+144>
Target 0: (run_tests) stopped.
```

#### 3. 스택 트레이스 및 레지스터 분석
*   **명령**: 크래시 발생 후 `bt` (backtrace) 명령 실행.
*   **목적**: 오류 발생까지의 함수 호출 경로를 확인하고, 오류의 성격을 파악.
*   **오류 지점 상세**:
    *   `address=0x10`: 접근 오류가 발생한 주소가 `0x10`임. 이는 NULL 포인터(`0x0`)에서 16바이트 오프셋만큼 떨어진 위치.
    *   **어셈블리 코드**: 중단된 명령은 `ldr x8, [x8, #0x10]` 이었음.
*   **결론**: 레지스터 `x8`이 `0x0` (NULL) 값을 가지고 있었고, CPU가 이 NULL 주소에서 16바이트 떨어진 위치의 데이터를 읽으려 시도하면서 **NULL 포인터 역참조(Dereference)** 오류가 발생했음을 확인.

### 해결 방법 (제안)

- **소멸 순서 명확화**: `DataStore` 클래스 내에서 멤버 변수들의 선언 순서를 조정하여, `MapNotifier`가 의존하는 다른 리소스들보다 나중에 소멸되도록 보장합니다.
- **RAII 및 스마트 포인터 활용**: `MapNotifier`와 관련된 리소스들이 `shared_ptr` 또는 `weak_ptr`를 통해 안전하게 관리되고 있는지 확인하고, 순환 참조가 없는지 점검합니다.
- **소멸자에서 락 사용**: `DataStore`의 소멸자와 `MapNotifier`의 소멸자에서 관련된 리소스에 접근할 때, 동시 접근을 막기 위한 락(lock)을 사용하여 스레드 안전성을 확보합니다.

### 검증 전략

- `MapNotifier`의 생명주기와 관련된 동시성 단위 테스트를 작성합니다.
- 여러 스레드가 `DataStore`를 생성하고 파괴하는 스트레스 테스트를 추가하여 문제가 재현되지 않는지 확인합니다.
- Valgrind나 AddressSanitizer와 같은 메모리 디버깅 도구를 사용하여 메모리 접근 오류를 탐지합니다.

### 관련 파일

**문제 발생 예상 지점**:
- `src/core/datastore/DataStore.cpp` (특히 `MapNotifier` 클래스 구현부 및 `DataStore` 소멸자)
- `src/core/datastore/DataStore.h`

**영향받을 수 있는 모듈**:
- `src/core/event/adapters/DataStoreEventAdapter.cpp`
- `tests/unit/datastore/DataStore_test.cpp`
