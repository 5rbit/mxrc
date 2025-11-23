#!/bin/bash
#
# monitor-cgroups.sh - cgroups 리소스 사용량 실시간 모니터링
#
# User Story 3: 리소스 제어 모니터링
#
# Usage:
#   ./monitor-cgroups.sh [interval]
#
# 기능:
#   - CPU, 메모리, I/O 사용량 실시간 모니터링
#   - systemd-cgtop 통합
#   - 리소스 제한 대비 사용률 표시

set -euo pipefail

# 기본 값
INTERVAL=${1:-5}

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

clear

echo "================================================================"
echo "  MXRC cgroups Resource Monitoring"
echo "  Refresh interval: ${INTERVAL}s (Ctrl+C to exit)"
echo "================================================================"
echo

# systemd-cgtop이 있는지 확인
if ! command -v systemd-cgtop &> /dev/null; then
    echo -e "${RED}ERROR:${NC} systemd-cgtop not found"
    echo "Please install systemd package"
    exit 1
fi

# 모니터링 루프
while true; do
    clear
    echo "================================================================"
    echo "  MXRC cgroups Resource Monitoring"
    echo "  Time: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "================================================================"
    echo

    # systemd-cgtop으로 리소스 사용량 확인
    echo -e "${BLUE}Real-time Resource Usage:${NC}"
    echo
    systemd-cgtop -n 1 -d "$INTERVAL" --depth=3 2>/dev/null | head -20 || {
        echo -e "${YELLOW}Could not get resource usage${NC}"
    }

    echo
    echo "================================================================"
    echo -e "${GREEN}RT Process (mxrc-rt):${NC} CPUQuota=200%, MemoryMax=2G, IOWeight=500"
    echo -e "${GREEN}Non-RT Process (mxrc-nonrt):${NC} CPUQuota=100%, MemoryMax=1G, IOWeight=100"
    echo "================================================================"
    echo
    echo "Press Ctrl+C to exit, waiting ${INTERVAL}s for next update..."

    sleep "$INTERVAL"
done
