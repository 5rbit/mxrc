# MXRC 개발 명령어

## 빌드

### 기본 빌드 (Linux)
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### backward-cpp 활성화 빌드
```bash
mkdir -p build && cd build
cmake -DUSE_BACKWARD=ON ..
make -j$(nproc)
```

### 클린 빌드
```bash
rm -rf build
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## 테스트

### 모든 테스트 실행
```bash
cd build
./run_tests
```

### 간략한 출력으로 테스트 실행
```bash
./run_tests --gtest_brief=1
```

### 특정 테스트 스위트 실행
```bash
# Action Layer 테스트
./run_tests --gtest_filter=ActionExecutor*

# Sequence Layer 테스트
./run_tests --gtest_filter=SequenceEngine*

# Task Layer 테스트
./run_tests --gtest_filter=TaskExecutor*

# DataStore 테스트
./run_tests --gtest_filter=DataStore*

# Async Logging 테스트
./run_tests --gtest_filter=AsyncLogger*
```

### 특정 테스트만 실행
```bash
./run_tests --gtest_filter=ActionExecutor.Execute_ValidAction_ReturnsSuccess
```

## 실행

### 메인 애플리케이션 실행
```bash
cd build
./mxrc
```

## 유용한 명령어

### 현재 디렉토리 확인
```bash
pwd
```

### 빌드 디렉토리로 이동
```bash
cd build
```

### 프로젝트 루트로 이동
```bash
cd ..
```

### 파일 찾기
```bash
find . -name "*.cpp"
find . -name "*.h"
```

### 코드 검색
```bash
grep -r "ActionExecutor" src/
grep -r "TODO" src/
```

### 로그 파일 확인
```bash
tail -f logs/mxrc.log
```

### Git 상태 확인
```bash
git status
git log --oneline -10
```