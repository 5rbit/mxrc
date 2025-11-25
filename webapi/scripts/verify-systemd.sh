#!/bin/bash
# MXRC WebAPI systemd 설치 검증 스크립트

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 카운터
PASS=0
FAIL=0

check() {
    local name="$1"
    local command="$2"

    echo -n "  Checking $name... "
    if eval "$command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASS++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        ((FAIL++))
        return 1
    fi
}

echo "MXRC WebAPI systemd Verification"
echo "================================="
echo

# 1. Prerequisites
echo "1. Prerequisites"
check "Node.js installation" "command -v node"
check "systemd availability" "command -v systemctl"
check "mxrc user exists" "id -u mxrc"

echo

# 2. Files and Directories
echo "2. Installation Files"
check "Service file" "test -f /etc/systemd/system/mxrc-webapi.service"
check "Application directory" "test -d /opt/mxrc/webapi"
check "Application dist" "test -d /opt/mxrc/webapi/dist"
check "Node modules" "test -d /opt/mxrc/webapi/node_modules"
check "Runtime directory" "test -d /var/run/mxrc"
check "Config directory" "test -d /opt/mxrc/config"

echo

# 3. Permissions
echo "3. Permissions"
check "Application ownership" "test \"$(stat -c '%U' /opt/mxrc/webapi)\" = 'mxrc'"
check "Runtime ownership" "test \"$(stat -c '%U' /var/run/mxrc)\" = 'mxrc'"

echo

# 4. systemd Service
echo "4. systemd Service"
check "Service loaded" "systemctl list-unit-files | grep -q mxrc-webapi.service"
check "Service enabled" "systemctl is-enabled --quiet mxrc-webapi.service || test $? -eq 1"

# Check if service is running (optional)
if systemctl is-active --quiet mxrc-webapi.service; then
    echo -e "  Service is running: ${GREEN}YES${NC}"

    # 5. Runtime Checks (only if running)
    echo
    echo "5. Runtime Checks"
    check "Process is running" "pgrep -f 'node.*mxrc.*webapi' > /dev/null"
    check "Port 8080 listening" "ss -tlnp | grep -q ':8080 '"
    check "Health endpoint" "curl -sf http://localhost:8080/api/health/live > /dev/null"

    echo
    echo "6. Resource Usage"
    systemctl status mxrc-webapi --no-pager | grep -E "(Active|Memory|CPU)" || true
else
    echo -e "  Service is running: ${YELLOW}NO${NC} (not started)"
    echo
    echo "  To start the service: sudo systemctl start mxrc-webapi"
fi

echo
echo "================================="
echo "Summary"
echo "================================="
echo -e "Passed: ${GREEN}$PASS${NC}"
echo -e "Failed: ${RED}$FAIL${NC}"
echo

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Some checks failed. Review output above.${NC}"
    exit 1
fi
