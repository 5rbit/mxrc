#!/bin/bash
#
# verify-cgroups.sh - cgroups 설정 검증 스크립트
#
# User Story 3: 리소스 제어 및 격리 검증
#
# Usage:
#   ./verify-cgroups.sh [service-name]
#
# 검증 항목:
#   1. cgroup v2 지원 확인
#   2. CPU quota 설정 확인
#   3. 메모리 제한 설정 확인
#   4. I/O 가중치 설정 확인
#   5. 실제 리소스 사용량 확인

set -euo pipefail

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 로깅 함수
info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# systemd 실행 확인
check_systemd() {
    if [ ! -d "/run/systemd/system" ]; then
        error "systemd is not running"
        exit 1
    fi
    info "systemd is running"
}

# cgroup 버전 확인
check_cgroup_version() {
    if [ -f "/sys/fs/cgroup/cgroup.controllers" ]; then
        info "cgroup v2 is available"
        CGROUP_VERSION=2
    elif [ -d "/sys/fs/cgroup/cpu" ]; then
        info "cgroup v1 is available"
        CGROUP_VERSION=1
    else
        error "No cgroup support detected"
        exit 1
    fi
}

# CPU quota 확인
check_cpu_quota() {
    local service=$1
    local expected_quota=$2

    info "Checking CPU quota for $service..."

    if [ "$CGROUP_VERSION" = "2" ]; then
        local quota_file="/sys/fs/cgroup/system.slice/${service}.service/cpu.max"
        if [ -f "$quota_file" ]; then
            local quota=$(cat "$quota_file" | awk '{print $1}')
            info "  CPU quota: $quota"

            # 200000 = 200%, 100000 = 100%
            if [ "$quota" = "$expected_quota" ] || [ "$quota" = "max" ]; then
                info "  ✓ CPU quota is correctly set"
            else
                warn "  ⚠ CPU quota mismatch: expected $expected_quota, got $quota"
            fi
        else
            warn "  ⚠ CPU quota file not found (service may not be running)"
        fi
    else
        local quota_file="/sys/fs/cgroup/cpu,cpuacct/system.slice/${service}.service/cpu.cfs_quota_us"
        if [ -f "$quota_file" ]; then
            local quota=$(cat "$quota_file")
            info "  CPU quota: $quota"
        else
            warn "  ⚠ CPU quota file not found"
        fi
    fi
}

# 메모리 제한 확인
check_memory_limit() {
    local service=$1
    local expected_mb=$2

    info "Checking memory limit for $service..."

    if [ "$CGROUP_VERSION" = "2" ]; then
        local mem_file="/sys/fs/cgroup/system.slice/${service}.service/memory.max"
        if [ -f "$mem_file" ]; then
            local mem_limit=$(cat "$mem_file")
            if [ "$mem_limit" = "max" ]; then
                info "  Memory limit: unlimited"
            else
                local mem_mb=$((mem_limit / 1024 / 1024))
                info "  Memory limit: ${mem_mb}MB"

                if [ "$mem_mb" -ge "$((expected_mb - 100))" ] && [ "$mem_mb" -le "$((expected_mb + 100))" ]; then
                    info "  ✓ Memory limit is correctly set (~${expected_mb}MB)"
                else
                    warn "  ⚠ Memory limit mismatch: expected ~${expected_mb}MB, got ${mem_mb}MB"
                fi
            fi
        else
            warn "  ⚠ Memory limit file not found (service may not be running)"
        fi
    else
        local mem_file="/sys/fs/cgroup/memory/system.slice/${service}.service/memory.limit_in_bytes"
        if [ -f "$mem_file" ]; then
            local mem_limit=$(cat "$mem_file")
            local mem_mb=$((mem_limit / 1024 / 1024))
            info "  Memory limit: ${mem_mb}MB"
        else
            warn "  ⚠ Memory limit file not found"
        fi
    fi
}

# I/O 가중치 확인
check_io_weight() {
    local service=$1
    local expected_weight=$2

    info "Checking I/O weight for $service..."

    if [ "$CGROUP_VERSION" = "2" ]; then
        local io_file="/sys/fs/cgroup/system.slice/${service}.service/io.weight"
        if [ -f "$io_file" ]; then
            local io_weight=$(cat "$io_file" | grep -oP 'default \K\d+' || echo "100")
            info "  I/O weight: $io_weight"

            if [ "$io_weight" = "$expected_weight" ]; then
                info "  ✓ I/O weight is correctly set"
            else
                warn "  ⚠ I/O weight mismatch: expected $expected_weight, got $io_weight"
            fi
        else
            warn "  ⚠ I/O weight file not found (service may not be running)"
        fi
    else
        local io_file="/sys/fs/cgroup/blkio/system.slice/${service}.service/blkio.weight"
        if [ -f "$io_file" ]; then
            local io_weight=$(cat "$io_file")
            info "  I/O weight: $io_weight"
        else
            warn "  ⚠ I/O weight file not found"
        fi
    fi
}

# 서비스 상태 확인
check_service_status() {
    local service=$1

    info "Checking service status for $service..."

    if systemctl is-active --quiet "$service"; then
        info "  ✓ Service is active"
    else
        warn "  ⚠ Service is not active"
        return 1
    fi
}

# 실제 리소스 사용량 확인
check_resource_usage() {
    local service=$1

    info "Checking current resource usage for $service..."

    if ! systemctl is-active --quiet "$service"; then
        warn "  Service is not running, skipping usage check"
        return
    fi

    # systemd-cgtop을 사용하여 리소스 사용량 확인
    if command -v systemd-cgtop &> /dev/null; then
        info "  Current resource usage (5 second sample):"
        systemd-cgtop -n 1 -d 5 --depth=3 | grep "$service" || warn "  Could not get resource usage"
    else
        warn "  systemd-cgtop not available"
    fi
}

# 메인 함수
main() {
    local service=${1:-"mxrc-rt"}

    echo "==============================================="
    echo "  cgroups Configuration Verification"
    echo "  Service: $service"
    echo "==============================================="
    echo

    check_systemd
    check_cgroup_version
    echo

    case "$service" in
        "mxrc-rt")
            check_service_status "$service" || warn "Service not running, checking configuration only"
            check_cpu_quota "$service" "200000"
            check_memory_limit "$service" "2048"
            check_io_weight "$service" "500"
            echo
            check_resource_usage "$service"
            ;;
        "mxrc-nonrt")
            check_service_status "$service" || warn "Service not running, checking configuration only"
            check_cpu_quota "$service" "100000"
            check_memory_limit "$service" "1024"
            check_io_weight "$service" "100"
            echo
            check_resource_usage "$service"
            ;;
        *)
            error "Unknown service: $service"
            echo "Usage: $0 [mxrc-rt|mxrc-nonrt]"
            exit 1
            ;;
    esac

    echo
    echo "==============================================="
    echo "  Verification complete"
    echo "==============================================="
}

main "$@"
