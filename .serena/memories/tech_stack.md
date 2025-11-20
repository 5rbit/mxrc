# MXRC 기술 스택

## 언어 및 표준
- **언어**: C++20
- **컴파일러**: GCC 11+ or Clang 14+
- **OS**: Ubuntu 24.04 LTS (PREEMPT_RT)

## 빌드 시스템
- **빌드 도구**: CMake 3.16+
- **병렬 빌드**: `make -j$(nproc)`
- **테스트**: GoogleTest framework

## 핵심 의존성
- **spdlog >= 1.x**: 비동기 로깅 시스템
- **GTest**: 테스트 프레임워크
- **TBB** (Intel Threading Building Blocks): 스레드 안전한 데이터 구조
- **nlohmann_json >= 3.11.0**: JSON 처리

## 빌드 설정
- **C++ 표준**: C++20 (CMAKE_CXX_STANDARD 20)
- **메모리 안전**: AddressSanitizer 항상 활성화
  - `-fsanitize=address -fno-omit-frame-pointer`
- **선택적 기능**: backward-cpp (스택 트레이스, USE_BACKWARD 옵션)

## 성능 목표
- **실시간 성능**: PREEMPT_RT 커널 활용
- **저지연**: Task 실행 오버헤드 < 1ms
- **로깅 성능**: 평균 0.111μs (동기식 대비 9,000배 개선)
- **처리량**: 5,000,000 msg/sec
- **안정성**: 메모리 누수 없음, 예외 안전성 보장