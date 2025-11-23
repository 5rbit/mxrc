# MXRC Systemd Services

MXRC RT/Non-RT 이중 프로세스를 systemd 서비스로 관리하기 위한 설정 파일입니다.

## 서비스 구성

### mxrc-rt.service
RT (Real-Time) 프로세스 서비스
- **스케줄링**: SCHED_FIFO (RT 스케줄링)
- **권한**: CAP_SYS_NICE, CAP_IPC_LOCK
- **CPU 친화성**: CPU 1번에 고정
- **우선순위**: 최대 99
- **메모리**: mlockall로 페이징 방지

### mxrc-nonrt.service
Non-RT 프로세스 서비스
- **스케줄링**: SCHED_OTHER (일반 스케줄링)
- **의존성**: RT 프로세스가 먼저 시작되어야 함

## 설치 방법

### 1. 시스템 사용자 및 그룹 생성

```bash
sudo useradd -r -s /bin/false -d /var/lib/mxrc -M mxrc
sudo mkdir -p /var/lib/mxrc
sudo chown mxrc:mxrc /var/lib/mxrc
```

### 2. 실행 파일 설치

```bash
# 빌드된 실행 파일을 시스템 경로로 복사
sudo cp build/rt /usr/local/bin/mxrc-rt
sudo cp build/nonrt /usr/local/bin/mxrc-nonrt

# 실행 권한 설정
sudo chmod 755 /usr/local/bin/mxrc-rt
sudo chmod 755 /usr/local/bin/mxrc-nonrt

# 소유자 설정
sudo chown root:root /usr/local/bin/mxrc-rt
sudo chown root:root /usr/local/bin/mxrc-nonrt
```

### 3. Systemd 서비스 파일 설치

```bash
# 서비스 파일 복사
sudo cp systemd/mxrc-rt.service /etc/systemd/system/
sudo cp systemd/mxrc-nonrt.service /etc/systemd/system/

# 권한 설정
sudo chmod 644 /etc/systemd/system/mxrc-rt.service
sudo chmod 644 /etc/systemd/system/mxrc-nonrt.service

# systemd 재로드
sudo systemctl daemon-reload
```

### 4. 서비스 활성화 및 시작

```bash
# 서비스 활성화 (부팅 시 자동 시작)
sudo systemctl enable mxrc-rt.service
sudo systemctl enable mxrc-nonrt.service

# 서비스 시작
sudo systemctl start mxrc-rt.service
sudo systemctl start mxrc-nonrt.service
```

## 서비스 관리

### 상태 확인

```bash
# 전체 상태 확인
sudo systemctl status mxrc-rt.service
sudo systemctl status mxrc-nonrt.service

# 간단한 상태만 확인
sudo systemctl is-active mxrc-rt.service
sudo systemctl is-active mxrc-nonrt.service
```

### 로그 확인

```bash
# RT 프로세스 로그 (실시간)
sudo journalctl -u mxrc-rt.service -f

# Non-RT 프로세스 로그 (실시간)
sudo journalctl -u mxrc-nonrt.service -f

# 최근 100줄
sudo journalctl -u mxrc-rt.service -n 100
sudo journalctl -u mxrc-nonrt.service -n 100

# 시간 범위 지정
sudo journalctl -u mxrc-rt.service --since "1 hour ago"
sudo journalctl -u mxrc-nonrt.service --since "1 hour ago"
```

### 서비스 재시작

```bash
# RT 프로세스 재시작
sudo systemctl restart mxrc-rt.service

# Non-RT 프로세스 재시작
sudo systemctl restart mxrc-nonrt.service

# 양쪽 모두 재시작
sudo systemctl restart mxrc-rt.service mxrc-nonrt.service
```

### 서비스 중지

```bash
# RT 프로세스 중지
sudo systemctl stop mxrc-rt.service

# Non-RT 프로세스 중지
sudo systemctl stop mxrc-nonrt.service

# 양쪽 모두 중지
sudo systemctl stop mxrc-rt.service mxrc-nonrt.service
```

### 서비스 비활성화 (부팅 시 자동 시작 해제)

```bash
sudo systemctl disable mxrc-rt.service
sudo systemctl disable mxrc-nonrt.service
```

## RT 권한 설정

### Capabilities 확인

서비스가 올바른 capabilities를 가지고 있는지 확인:

```bash
# RT 프로세스의 PID 확인
RT_PID=$(pgrep -f mxrc-rt)

# Capabilities 확인
sudo getpcaps $RT_PID
# 예상 출력: cap_sys_nice,cap_ipc_lock=eip
```

### RT 스케줄링 확인

```bash
# RT 프로세스의 스케줄링 정책 확인
RT_PID=$(pgrep -f mxrc-rt)
chrt -p $RT_PID
# 예상 출력: SCHED_FIFO priority
```

### CPU 친화성 확인

```bash
# RT 프로세스의 CPU 친화성 확인
RT_PID=$(pgrep -f mxrc-rt)
taskset -cp $RT_PID
# 예상 출력: cpu list: 1
```

## 문제 해결

### RT 우선순위 설정 실패

**증상**: 로그에 "Failed to set RT priority" 에러

**원인**:
- CAP_SYS_NICE capability가 없음
- LimitRTPRIO 설정이 충분하지 않음

**해결**:
1. 서비스 파일의 AmbientCapabilities 확인
2. systemctl daemon-reload 실행
3. 서비스 재시작

### 메모리 잠금 실패

**증상**: 로그에 "Cannot allocate memory" 에러

**원인**:
- LimitMEMLOCK 설정이 충분하지 않음

**해결**:
1. 서비스 파일에서 `LimitMEMLOCK=infinity` 확인
2. systemctl daemon-reload 실행
3. 서비스 재시작

### 공유 메모리 접근 실패

**증상**: RT/Non-RT 프로세스 간 통신 실패

**원인**:
- /dev/shm 접근 권한 문제
- 공유 메모리 크기 제한

**해결**:
```bash
# 공유 메모리 확인
ls -la /dev/shm/mxrc_shm

# 공유 메모리 정리
sudo rm -f /dev/shm/mxrc_shm
sudo systemctl restart mxrc-rt.service
```

### 서비스 시작 순서 문제

**증상**: Non-RT가 RT보다 먼저 시작됨

**원인**:
- systemd 의존성 설정 문제

**해결**:
1. 서비스 파일의 After/Before/Requires 확인
2. systemctl daemon-reload 실행
3. 양쪽 서비스 재시작

## 보안 고려사항

### 최소 권한 원칙

RT 프로세스는 다음 capabilities만 가집니다:
- **CAP_SYS_NICE**: RT 스케줄링 설정 권한
- **CAP_IPC_LOCK**: 메모리 페이징 방지 권한

다른 권한은 모두 제거되어 보안을 강화합니다.

### 격리

- **ProtectSystem=strict**: 시스템 파일 수정 불가
- **ProtectHome=yes**: 홈 디렉토리 접근 불가
- **PrivateTmp=yes**: 독립적인 /tmp 사용
- **NoNewPrivileges=yes**: 권한 상승 방지

### 사용자 격리

서비스는 전용 `mxrc` 사용자로 실행되어 시스템과 격리됩니다.

## 성능 튜닝

### CPU 분리 (Isolation)

RT 프로세스가 사용하는 CPU를 완전히 격리하려면:

```bash
# 부팅 파라미터에 추가 (예: CPU 1번 격리)
# /etc/default/grub 편집
GRUB_CMDLINE_LINUX="isolcpus=1 nohz_full=1 rcu_nocbs=1"

# grub 업데이트
sudo update-grub
sudo reboot
```

### 커널 파라미터 튜닝

```bash
# /etc/sysctl.conf에 추가
kernel.sched_rt_runtime_us=-1  # RT 스케줄링 제한 해제
vm.swappiness=0                 # 스왑 최소화
```

## 모니터링

### 시스템 리소스 모니터링

```bash
# RT 프로세스 리소스 사용량
RT_PID=$(pgrep -f mxrc-rt)
top -p $RT_PID

# 자세한 정보
ps -eLo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,comm | grep mxrc
```

### 지연 시간 분석

RT 프로세스의 실제 지연 시간을 측정하려면 cyclictest 사용:

```bash
sudo apt-get install rt-tests
sudo cyclictest -p 99 -t 1 -n -i 1000 -l 100000
```

## 업데이트

새 버전으로 업데이트:

```bash
# 서비스 중지
sudo systemctl stop mxrc-rt.service mxrc-nonrt.service

# 새 실행 파일 복사
sudo cp build/rt /usr/local/bin/mxrc-rt
sudo cp build/nonrt /usr/local/bin/mxrc-nonrt

# 서비스 시작
sudo systemctl start mxrc-rt.service mxrc-nonrt.service

# 버전 확인 (로그에서)
sudo journalctl -u mxrc-rt.service -n 10
```

## 제거

서비스 완전 제거:

```bash
# 서비스 중지 및 비활성화
sudo systemctl stop mxrc-rt.service mxrc-nonrt.service
sudo systemctl disable mxrc-rt.service mxrc-nonrt.service

# 서비스 파일 제거
sudo rm /etc/systemd/system/mxrc-rt.service
sudo rm /etc/systemd/system/mxrc-nonrt.service
sudo systemctl daemon-reload

# 실행 파일 제거
sudo rm /usr/local/bin/mxrc-rt
sudo rm /usr/local/bin/mxrc-nonrt

# 사용자 및 데이터 제거 (선택적)
sudo userdel mxrc
sudo rm -rf /var/lib/mxrc

# 공유 메모리 정리
sudo rm -f /dev/shm/mxrc_shm
```
