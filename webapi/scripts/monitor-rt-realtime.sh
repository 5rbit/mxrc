#!/bin/bash
# Real-time RT Performance Monitoring
# Displays live statistics about RT performance and WebAPI impact

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# 화면 클리어 및 커서 숨기기
clear
tput civis

# 종료 시 커서 복원
trap 'tput cnorm; exit' INT TERM EXIT

# 프로세스 찾기
find_process() {
    local name=$1
    pgrep -f "$name" | head -1
}

while true; do
    # 화면 상단으로 이동
    tput cup 0 0

    # 헤더
    echo -e "${BLUE}╔════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║${NC}  MXRC Real-Time Performance Monitor                 $(date +'%Y-%m-%d %H:%M:%S')  ${BLUE}║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════════════════╝${NC}"
    echo

    # RT 프로세스 정보
    RT_PID=$(find_process "mxrc-rt")
    if [ -n "$RT_PID" ]; then
        echo -e "${GREEN}■ RT Process (PID: $RT_PID)${NC}"

        # CPU 사용률
        RT_CPU=$(ps -p $RT_PID -o %cpu= 2>/dev/null | awk '{printf "%.1f", $1}')
        if (( $(echo "$RT_CPU > 50" | bc -l 2>/dev/null || echo 0) )); then
            RT_CPU_COLOR=$RED
        elif (( $(echo "$RT_CPU > 20" | bc -l 2>/dev/null || echo 0) )); then
            RT_CPU_COLOR=$YELLOW
        else
            RT_CPU_COLOR=$GREEN
        fi
        echo -e "  CPU Usage:    ${RT_CPU_COLOR}${RT_CPU}%${NC}"

        # 메모리 사용량
        RT_MEM=$(ps -p $RT_PID -o rss= 2>/dev/null | awk '{printf "%.1f", $1/1024}')
        echo -e "  Memory:       ${CYAN}${RT_MEM} MB${NC}"

        # CPU Affinity
        RT_AFFINITY=$(taskset -cp $RT_PID 2>/dev/null | awk -F: '{print $2}' | tr -d ' ')
        echo -e "  CPU Affinity: ${CYAN}${RT_AFFINITY}${NC}"

        # 스케줄링 정책
        RT_SCHED=$(chrt -p $RT_PID 2>/dev/null | grep policy | awk '{print $3}')
        if [ "$RT_SCHED" = "SCHED_FIFO" ]; then
            SCHED_COLOR=$GREEN
        else
            SCHED_COLOR=$YELLOW
        fi
        echo -e "  Scheduling:   ${SCHED_COLOR}${RT_SCHED}${NC}"
    else
        echo -e "${RED}■ RT Process: NOT RUNNING${NC}"
    fi

    echo

    # WebAPI 프로세스 정보
    WEBAPI_PID=$(find_process "mxrc.*webapi")
    if [ -n "$WEBAPI_PID" ]; then
        echo -e "${GREEN}■ WebAPI Process (PID: $WEBAPI_PID)${NC}"

        # CPU 사용률
        WEBAPI_CPU=$(ps -p $WEBAPI_PID -o %cpu= 2>/dev/null | awk '{printf "%.1f", $1}')
        if (( $(echo "$WEBAPI_CPU > 30" | bc -l 2>/dev/null || echo 0) )); then
            WEBAPI_CPU_COLOR=$RED
        elif (( $(echo "$WEBAPI_CPU > 15" | bc -l 2>/dev/null || echo 0) )); then
            WEBAPI_CPU_COLOR=$YELLOW
        else
            WEBAPI_CPU_COLOR=$GREEN
        fi
        echo -e "  CPU Usage:    ${WEBAPI_CPU_COLOR}${WEBAPI_CPU}%${NC}"

        # 메모리 사용량
        WEBAPI_MEM=$(ps -p $WEBAPI_PID -o rss= 2>/dev/null | awk '{printf "%.1f", $1/1024}')
        if (( $(echo "$WEBAPI_MEM > 384" | bc -l 2>/dev/null || echo 0) )); then
            WEBAPI_MEM_COLOR=$RED
        elif (( $(echo "$WEBAPI_MEM > 256" | bc -l 2>/dev/null || echo 0) )); then
            WEBAPI_MEM_COLOR=$YELLOW
        else
            WEBAPI_MEM_COLOR=$GREEN
        fi
        echo -e "  Memory:       ${WEBAPI_MEM_COLOR}${WEBAPI_MEM} MB${NC} / 512 MB"

        # CPU Affinity
        WEBAPI_AFFINITY=$(taskset -cp $WEBAPI_PID 2>/dev/null | awk -F: '{print $2}' | tr -d ' ')

        # RT 코어에서 실행 중인지 확인
        if [[ "$WEBAPI_AFFINITY" == *"2"* ]] || [[ "$WEBAPI_AFFINITY" == *"3"* ]]; then
            AFFINITY_COLOR=$RED
            AFFINITY_WARN=" ⚠️  RT CORES!"
        else
            AFFINITY_COLOR=$GREEN
            AFFINITY_WARN=""
        fi
        echo -e "  CPU Affinity: ${AFFINITY_COLOR}${WEBAPI_AFFINITY}${NC}${AFFINITY_WARN}"

        # systemd 상태
        if systemctl is-active --quiet mxrc-webapi; then
            echo -e "  Status:       ${GREEN}ACTIVE${NC}"
        else
            echo -e "  Status:       ${YELLOW}INACTIVE${NC}"
        fi

        # 연결 수 (WebSocket)
        WS_CONNECTIONS=$(curl -s http://localhost:8080/api/ws/stats 2>/dev/null | grep -o '"totalConnections":[0-9]*' | cut -d: -f2 || echo "N/A")
        echo -e "  Connections:  ${CYAN}${WS_CONNECTIONS}${NC}"
    else
        echo -e "${YELLOW}■ WebAPI Process: NOT RUNNING${NC}"
    fi

    echo

    # 코어별 CPU 사용률
    echo -e "${GREEN}■ CPU Usage per Core${NC}"
    if command -v mpstat &> /dev/null; then
        mpstat -P ALL 1 1 2>/dev/null | tail -5 | awk '
            NR==1 {print "  " $0}
            NR>1 && $2 ~ /^[0-9]/ {
                core=$2
                idle=$NF
                usage=100-idle
                color="\033[0;32m"  # GREEN
                if (usage > 50) color="\033[0;31m"  # RED
                if (usage > 20 && usage <= 50) color="\033[1;33m"  # YELLOW
                printf "  %s%-4s%s  ", color, core, "\033[0m"
                for(i=0; i<usage; i+=5) printf "▓"
                printf " %.1f%%\n", usage
            }
        '
    else
        echo "  mpstat not installed (apt-get install sysstat)"
    fi

    echo

    # 메모리 사용률
    echo -e "${GREEN}■ System Memory${NC}"
    free -m | awk '
        NR==1 {print "  " $0}
        NR==2 {
            total=$2
            used=$3
            free=$4
            pct=used*100/total
            color="\033[0;32m"
            if (pct > 80) color="\033[0;31m"
            if (pct > 60 && pct <= 80) color="\033[1;33m"
            printf "  %s%-8s%s %6d %6d %6d  ", color, $1, "\033[0m", total, used, free
            for(i=0; i<pct; i+=5) printf "▓"
            printf " %.0f%%\n", pct
        }
    '

    echo

    # 최근 로그 (RT 관련)
    echo -e "${GREEN}■ Recent RT Logs (last 5)${NC}"
    if journalctl -u mxrc-rt --since "10 seconds ago" -n 5 --no-pager -q 2>/dev/null | grep -v "^--" | head -5; then
        :
    else
        echo "  No recent logs"
    fi

    echo

    # 경고
    echo -e "${YELLOW}■ Warnings${NC}"
    WARNING_COUNT=0

    if [ -n "$RT_PID" ] && [ -n "$WEBAPI_PID" ]; then
        # CPU 간섭 체크
        if [[ "$WEBAPI_AFFINITY" == *"2"* ]] || [[ "$WEBAPI_AFFINITY" == *"3"* ]]; then
            echo -e "  ${RED}⚠️  WebAPI running on RT cores!${NC}"
            WARNING_COUNT=$((WARNING_COUNT + 1))
        fi

        # 메모리 체크
        if (( $(echo "$WEBAPI_MEM > 384" | bc -l 2>/dev/null || echo 0) )); then
            echo -e "  ${RED}⚠️  WebAPI memory usage high!${NC}"
            WARNING_COUNT=$((WARNING_COUNT + 1))
        fi

        # RT CPU 체크
        if (( $(echo "$RT_CPU > 50" | bc -l 2>/dev/null || echo 0) )); then
            echo -e "  ${RED}⚠️  RT CPU usage very high!${NC}"
            WARNING_COUNT=$((WARNING_COUNT + 1))
        fi
    fi

    if [ $WARNING_COUNT -eq 0 ]; then
        echo -e "  ${GREEN}✓ No warnings - system healthy${NC}"
    fi

    echo
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════════════${NC}"
    echo -e "Press Ctrl+C to exit | Refresh: every 2 seconds"

    sleep 2
done
