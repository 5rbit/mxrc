# 연구 결과: Task & Mission Management (Task 및 임무 관리) 고도화

**기능 브랜치**: `004-task-mission-management`
**생성일**: 2025-11-03
**사양서**: [specs/004-task-mission-management/spec.md](specs/004-task-mission-management/spec.md)
**계획**: [specs/004-task-mission-management/plan.md](specs/004-task-mission-management/plan.md)

## 1. Behavior Tree 라이브러리 선택

- **결정**: BehaviorTree.CPP
- **선정 이유**: BehaviorTree.CPP는 C++ 기반의 강력하고 유연한 오픈소스 Behavior Tree 라이브러리입니다. 활발한 커뮤니티 지원, 풍부한 문서화, 다양한 노드 유형 지원, 그리고 실시간 시스템 통합에 적합한 성능 특성을 가지고 있어 로봇 제어 시스템에 이상적입니다. 특히, XML을 통한 트리 정의 및 동적 로딩 기능은 Mission 정의의 유연성을 극대화합니다.
- **고려했던 대안**: 자체 Behavior Tree 구현 (개발 시간 및 유지보수 비용 증가), 다른 Behavior Tree 라이브러리 (C++ 생태계 내에서 BehaviorTree.CPP만큼 성숙하고 활발한 대안 부족).

## 2. DataStore 모듈 구현 전략

- **결정**: SQLite (엣지/로컬 배포용) 및 PostgreSQL (서버/중앙 집중식 배포용) 지원을 위한 추상화 계층 구현.
- **선정 이유**: 로봇 엣지 환경에서는 경량의 파일 기반 데이터베이스인 SQLite가 시스템 리소스 사용을 최소화하면서 영속성을 제공하는 데 적합합니다. 반면, 중앙 집중식 Mission 관리 및 대규모 데이터 분석이 필요한 경우 PostgreSQL과 같은 강력한 관계형 데이터베이스가 필요합니다. 두 데이터베이스를 모두 지원하는 추상화 계층을 통해 배포 환경에 따른 유연성을 확보하고, 향후 다른 데이터베이스로의 확장성도 고려할 수 있습니다.
- **고려했던 대안**: 단일 데이터베이스만 사용 (배포 환경 유연성 저하), 자체 데이터 저장 방식 구현 (개발 복잡성 및 데이터 무결성 보장 어려움).
