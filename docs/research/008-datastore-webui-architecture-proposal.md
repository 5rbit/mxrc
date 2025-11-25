# Datastore WebUI 구성을 위한 아키텍처 분석 및 개선 제안

## 1. 개요

본 문서는 MXRC 프로젝트의 Datastore에 WebUI (Node.js/React 기반)를 구성하는 방안에 대한 아키텍처 분석 및 평가 결과를 담고 있다. 초기 제안된 직접 통합 방식의 위험성을 평가하고, 이를 해소하기 위한 Decoupled API 서버 아키텍처 및 Systemd를 활용한 프로세스 관리 방안을 제안한다.

## 2. 현재 아키텍처 및 요구사항

MXRC는 C++20 기반의 고성능 실시간 로봇 제어 컨트롤러로, 엄격한 실시간성 및 안정성이 요구된다. WebUI는 Node.js 백엔드와 React 프레임워크를 사용하여 Datastore 정보를 시각화하고 제어하는 기능을 목표로 한다.

## 3. 직접 통합 시 위험성 평가

MXRC Core (C++) 애플리케이션에 Node.js 기반 웹 서버를 직접 통합할 경우 다음과 같은 심각한 위험성이 발생한다.

*   **안정성 저하 (실시간성 침해)**: Node.js의 비동기 I/O 및 이벤트 루프 특성상, 웹 요청 증가 시 지연 또는 블로킹이 발생하여 코어 C++ 애플리케이션의 실시간 제어에 예측 불가능한 영향을 미칠 수 있다. 이는 로봇 제어 시스템의 오작동을 유발할 수 있다.
*   **복잡성 증가**: C++와 Node.js 간의 직접적인 연동(예: N-API)은 구현 및 디버깅이 매우 복잡하며, 언어 간의 메모리 관리 및 스레딩 모델 차이로 인한 오류 발생 가능성이 높다. 이는 장기적인 유지보수 비용을 증가시킨다.
*   **보안 취약점 증가**: 핵심 Datastore를 웹 서버에 직접 노출하면 시스템의 공격 표면이 넓어져 외부 공격에 대한 보안 취약점이 커진다.

## 4. 제안 아키텍처: Decoupled API 서버 방식

위험성을 해소하고 안정적이며 확장 가능한 WebUI를 구성하기 위해 **Decoupled API 서버 아키텍처**를 제안한다.

### 4.1. 아키텍처 구성도

```
+----------------+      +------------------------+      +-----------------+
|   Web UI       |      |   Intermediate API     |      |   MXRC Core     |
|   (React)      | <--> |   Server (Node.js)     | <--> |   (C++)         |
+----------------+      +------------------------+      ------------------
  (HTTP/WebSocket)         (IPC or other)                (Datastore)
```

### 4.2. 각 컴포넌트의 역할

*   **MXRC Core (C++)**:
    *   기존과 동일하게 실시간 로봇 제어 로직을 수행하고 Datastore를 관리한다.
    *   Intermediate API Server와의 통신을 위한 최소한의 IPC (Inter-Process Communication) 인터페이스만 노출한다. (`config/ipc/ipc-schema.yaml` 존재를 바탕으로 IPC 활용 가능성 높음)
*   **Intermediate API Server (Node.js)**:
    *   MXRC Core와 **별도의 독립적인 프로세스**로 동작한다.
    *   Web UI (React)로부터 오는 HTTP/WebSocket 요청을 처리한다.
    *   IPC를 통해 MXRC Core의 Datastore에 안전하게 접근하여 데이터를 조회하거나, 제어 명령을 전달한다.
    *   이 서버는 사용자 요청을 처리하는 동시에 MXRC Core로부터의 비동기 데이터를 수신할 수 있다.
*   **Web UI (React)**:
    *   사용자 브라우저에서 실행되며, Intermediate API Server와 HTTP/WebSocket을 통해 통신한다.
    *   MXRC Core의 존재나 구현 상세에 대해 알 필요가 없어 의존성이 완전히 분리된다.
    *   데이터 시각화, 사용자 입력 처리, 제어 명령 전송 등의 프레젠테이션 로직을 담당한다.

### 4.3. 기대 효과

*   **안정성 및 실시간성 보존**: MXRC Core와 WebUI/API 서버가 프로세스 수준에서 분리되어, 웹 서버의 부하 또는 오류가 실시간 제어 시스템에 영향을 미치지 않는다.
*   **유연성 및 확장성**: 각 컴포넌트(MXRC Core, API 서버, Web UI)를 독립적으로 개발, 배포, 확장할 수 있다. 예를 들어, API 서버의 부하가 높으면 해당 서버만 스케일 아웃할 수 있다.
*   **유지보수 용이성**: 각 계층의 책임이 명확히 분리되어 코드 베이스 이해 및 유지보수가 훨씬 용이해진다. WebUI 개발자는 C++ 코어에 대한 지식 없이 API 명세만으로 개발할 수 있다.
*   **기술 스택 활용**: Node.js와 React라는 사용자 선호 기술 스택을 활용하면서도, MXRC Core의 고유한 특성을 안전하게 보호할 수 있다.

## 5. Systemd 통합 평가

제안된 Node.js API 서버를 시스템 프로세스 관리 도구인 Systemd와 통합하는 것은 프로젝트의 기존 관리 방식과 일치하며, 높은 안정성과 운영 편의성을 제공한다.

### 5.1. 기존 프로젝트의 Systemd 활용 현황

MXRC 프로젝트는 `systemd/` 디렉토리와 `dev/agent/GEMINI.md` 문서에서 언급된 `libsystemd-dev (sd_notify API)`를 통해 Systemd를 적극 활용하고 있다. `mxrc-rt.service`, `mxrc-nonrt.service`, `mxrc.target` 등 핵심 서비스들이 Systemd에 의해 관리되고 있으며, `sd_notify` 사용은 서비스의 준비 상태 및 헬스 체크를 위한 고급 통합이 이루어졌음을 시사한다.

### 5.2. Node.js API 서버를 위한 Systemd 통합 방안 (`mxrc-webapi.service`)

새로운 Node.js API 서버를 `systemd/mxrc-webapi.service` 유닛으로 Systemd에 등록하여 관리할 것을 제안한다.

```ini
# systemd/mxrc-webapi.service
[Unit]
Description=MXRC Web API Server for Datastore
# mxrc-rt.service가 실행된 후에만 이 서비스를 시작합니다.
After=mxrc-rt.service
# mxrc-rt.service가 중지되면 이 서비스도 자동 중지됩니다.
BindsTo=mxrc-rt.service
# mxrc.target 그룹의 일부로 포함시킵니다.
PartOf=mxrc.target

[Service]
# 서비스 실행을 위한 비특권 사용자 (보안 강화)
User=mxrc
# Node.js 애플리케이션 실행 명령어
ExecStart=/usr/bin/node /opt/mxrc/webapi/server.js
# 서비스의 작업 디렉토리
WorkingDirectory=/opt/mxrc/webapi
# 비정상 종료 시 자동으로 재시작
Restart=on-failure
# 재시작 전 5초 대기
RestartSec=5s
# 운영 환경 변수 설정 예시
Environment=NODE_ENV=production

[Install]
# mxrc.target 활성화 시 이 서비스도 함께 활성화되도록 합니다.
WantedBy=mxrc.target
```

### 5.3. Systemd 통합 시 장점 평가

1.  **프로세스 생명주기 관리 자동화**:
    *   **장점**: API 서버가 크래시되더라도 `Restart=on-failure` 설정에 따라 Systemd가 자동으로 재시작하여 서비스 가용성을 유지한다. 이는 운영자의 개입 없이 서비스 안정성을 보장한다.
    *   **평가**: **매우 중요**. 무인 시스템 환경에서 서비스 연속성 확보에 필수적이다.

2.  **명시적인 의존성 관리**:
    *   **장점**: `After=mxrc-rt.service`는 MXRC 코어 프로세스가 완전히 준비된 후에만 API 서버가 시작되도록 보장한다. `BindsTo=mxrc-rt.service`는 MXRC 코어 프로세스 중단 시 API 서버도 함께 안전하게 중단되도록 한다.
    *   **평가**: **매우 중요**. 분리된 프로세스 간의 실행 순서 및 연동 안정성을 극대화한다.

3.  **중앙화된 로깅**:
    *   **장점**: Node.js 애플리케이션의 모든 표준 출력(stdout) 및 표준 에러(stderr)는 Systemd의 `journald`에 의해 자동으로 수집된다. `journalctl -u mxrc-webapi.service -f`와 같은 표준 명령어로 통합된 로그 관리가 가능하다.
    *   **평가**: **매우 중요**. 시스템 운영 및 디버깅 시 로그 관리의 일관성과 효율성을 제공한다.

4.  **자원 제어 및 격리**:
    *   **장점**: Systemd는 cgroups를 활용하여 각 서비스의 CPU, 메모리 사용량 등을 제한할 수 있다. 이는 API 서버의 예기치 않은 자원 소모가 실시간 코어 프로세스에 미치는 영향을 최소화한다.
    *   **평가**: **보안 및 안정성 강화**. 시스템 전체의 견고성을 높이는 추가적인 보호 장치이다.

5.  **표준화된 관리 및 운영**:
    *   **장점**: MXRC 프로젝트의 기존 프로세스 관리 방식과 동일하게 Systemd를 사용하여, 새로운 컴포넌트의 도입에도 불구하고 시스템 관리 및 운영의 일관성을 유지할 수 있다.
    *   **평가**: **운영 효율성 증대**. 시스템 관리자의 학습 곡선을 줄이고 오류 가능성을 낮춘다.

## 6. 결론 및 권고

MXRC Datastore를 위한 WebUI 구성은 **Decoupled API 서버 아키텍처**를 채택하고, 이 API 서버를 **Systemd를 통해 통합 관리**하는 것이 가장 안정적이고 효율적이다.

이 접근 방식은:
*   MXRC Core의 실시간성 및 안정성을 완벽하게 보존하면서,
*   Node.js/React 기반의 유연하고 확장 가능한 WebUI를 제공하며,
*   프로젝트의 기존 시스템 관리 철학에 부합하는 일관된 운영 환경을 구축할 수 있다.

이 제안은 MXRC 프로젝트의 장기적인 안정성과 확장성에 기여할 것으로 확신한다.
