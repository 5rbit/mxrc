<!--
Sync Impact Report:
- Version change: None -> 1.0.0
- List of modified principles: Initial creation of all principles.
- Added sections: All sections are newly created.
- Removed sections: None.
- Templates requiring updates:
  - ✅ .specify/templates/plan-template.md
  - ✅ .specify/templates/spec-template.md
  - ✅ .specify/templates/tasks-template.md
- Follow-up TODOs: None.
-->
# 리눅스 RTOS 기반 C++ 로봇 컨트롤러 Constitution

## 핵심 원칙

### I. 실시간성 보장 (Hard Real-Time Guarantee)
모든 제어 루프와 태스크는 지정된 시간 제약(deadline)을 반드시 준수해야 한다.이를 위해 RTOS 스케줄링 정책을 엄격히 따르고, 예측 불가능한 지연을 유발하는 시스템 콜이나 라이브러리 사용을 금지한다.

### II. 최고 수준의 신뢰성 및 안전성 (Maximum Reliability and Safety)
시스템은 어떠한 상황에서도 예측 가능하고 안전하게 동작해야 한다. 메모리 안전성, 스레드 안전성을 보장하는 C++ 기능을 사용하고, 위험 분석 및 FMEA(고장 형태 및 영향 분석)를 통해 잠재적 위험을 사전에 식별하고 완화한다. MISRA C++과 같은 코딩 표준을 준수한다.

### III. 엄격한 테스트 주도 개발 (Rigorous Test-Driven Development)
모든 코드는 단위 테스트, 통합 테스트, 회귀 테스트를 포함하는 포괄적인 테스트 스위트와 함께 제공되어야 한다. 시뮬레이션 기반 테스트와 실제 하드웨어 테스트를 모두 수행하여 기능적 정확성과 실시간 성능을 검증한다.

### IV. 유지보수성과 확장성을 고려한 모듈식 설계 (Modular Design for Maintainability and Scalability)
시스템은 독립적으로 개발, 테스트, 배포할 수 있는 모듈의 집합으로 구성된다. 각 모듈은 명확한 API를 통해 통신하며, 기능 추가 및 수정이 용이하도록 설계한다.

### V. 명확하고 일관된 한글 문서화 (Clear and Consistent Korean Documentation)
모든 설계 결정, API, 설정 절차는 한글로 명확하게 문서화되어야 한다. 주석, API 문서, 아키처 다이어그램을 최신 상태로 유지하여 새로운 팀원이 빠르게 프로젝트에 적응할 수 있도록 한다.

### VI. 소스코드 형상 관리 및 버전 관리 (Source Configuration Management and Versioning)
모든 소스코드 및 관련 산출물은 Git을 통해 엄격하게 관리한다. Semantic Versioning 2.0.0을 준수하여 API 변경 사항을 명확히 전달하고, 모든 릴리스는 재현 가능해야 한다.

## Governance
본 Constitution은 다른 모든 개발 관행에 우선한다. Constitution의 개정은 문서화, 승인, 그리고 마이그레이션 계획을 포함해야 한다. 모든 코드 리뷰와 설계 결정은 본 Constitution의 원칙을 준수해야 한다.

**Version**: 1.0.0 | **Ratified**: 2025-11-02 | **Last Amended**: 2025-11-02