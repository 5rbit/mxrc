# 데이터 모델 (Data Model)

## 1. Task 엔티티

Task는 시스템에서 관리되고 실행되는 개별 작업을 나타냅니다. Task는 종류에 상관없이 일관된 방식으로 처리될 수 있도록 추상화된 개념입니다.

### 속성:

- **id (UUID/String)**: Task의 고유 식별자. 시스템 내에서 Task를 유일하게 구분합니다.
- **name (String)**: Task의 식별 가능한 이름. 사용자 친화적인 이름으로, 중복될 수 없습니다.
- **type (String)**: Task의 종류를 나타내는 문자열 (예: "DriveToPosition", "LiftPallet"). 이를 통해 Task Factory가 적절한 Task 구현체를 생성할 수 있습니다.
- **parameters (JSON/Map<String, String>)**: Task 실행에 필요한 동적 파라미터 집합. Task 종류에 따라 달라집니다.
- **status (Enum)**: Task의 현재 상태 (예: "PENDING", "RUNNING", "COMPLETED", "FAILED", "CANCELLED").
- **progress (Integer)**: Task의 진행률 (0-100%).
- **created_at (Timestamp)**: Task가 시스템에 등록된 시간.
- **updated_at (Timestamp)**: Task의 상태 또는 파라미터가 마지막으로 업데이트된 시간.

### 관계:

- Task는 TaskManager에 의해 관리됩니다.

## 2. TaskManager 엔티티

TaskManager는 Task의 생명주기(등록, 조회, 실행 요청, 상태 모니터링)를 관리하는 핵심 모듈입니다.

### 속성:

- **managed_tasks (Map<UUID, Task>)**: 현재 시스템에 등록되어 관리되는 Task 객체들의 컬렉션.

### 관계:

- TaskManager는 여러 Task 엔티티를 관리합니다.
- TaskManager는 외부 저장소 모듈에 의존하여 Task 정의를 영구적으로 저장하고 검색합니다.
- TaskManager는 외부 Task 실행기에 의존하여 실제 Task 실행 로직을 위임합니다.
