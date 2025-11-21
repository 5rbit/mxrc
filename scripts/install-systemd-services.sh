#!/bin/bash
#
# MXRC systemd 서비스 설치 스크립트
#
# 이 스크립트는 mxrc-rt.service를 시스템에 설치하고
# RT 성능에 필요한 시스템 설정을 검증합니다.
#

set -e  # 에러 발생 시 즉시 종료

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'  # No Color

# 로깅 함수
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# root 권한 확인
if [[ $EUID -ne 0 ]]; then
   log_error "This script must be run as root (use sudo)"
   exit 1
fi

log_info "Starting MXRC systemd service installation..."

# 1. isolcpus 커널 파라미터 검증 (T029)
log_info "Checking isolcpus kernel parameter..."

if grep -q "isolcpus" /proc/cmdline; then
    ISOLCPUS=$(grep -o "isolcpus=[^ ]*" /proc/cmdline | cut -d= -f2)
    log_info "isolcpus configured: $ISOLCPUS"

    # CPU 2,3이 격리되어 있는지 확인
    if echo "$ISOLCPUS" | grep -qE "(2|3|2-3|2,3)"; then
        log_info "✓ CPU cores 2-3 are isolated"
    else
        log_warn "CPU cores 2-3 are not isolated. RT performance may be affected."
        log_warn "Consider adding 'isolcpus=2,3' to kernel boot parameters in /etc/default/grub"
    fi
else
    log_warn "isolcpus kernel parameter not found"
    log_warn "For optimal RT performance, add 'isolcpus=2,3' to GRUB_CMDLINE_LINUX in /etc/default/grub"
    log_warn "Then run: sudo update-grub && sudo reboot"
fi

# 2. RLIMIT_RTPRIO 설정 확인 (T030)
log_info "Checking RLIMIT_RTPRIO configuration..."

LIMITS_CONF="/etc/security/limits.conf"
RT_LIMIT_FOUND=false

if [ -f "$LIMITS_CONF" ]; then
    if grep -q "rtprio" "$LIMITS_CONF"; then
        log_info "✓ RLIMIT_RTPRIO is configured in $LIMITS_CONF"
        RT_LIMIT_FOUND=true
    fi
fi

# limits.d 디렉토리도 확인
if [ -d "/etc/security/limits.d" ]; then
    if grep -r "rtprio" /etc/security/limits.d/ 2>/dev/null; then
        log_info "✓ RLIMIT_RTPRIO is configured in /etc/security/limits.d/"
        RT_LIMIT_FOUND=true
    fi
fi

if [ "$RT_LIMIT_FOUND" = false ]; then
    log_warn "RLIMIT_RTPRIO not configured"
    log_info "Adding RT priority limits to $LIMITS_CONF..."

    # mxrc 사용자에 대한 RT 우선순위 설정
    cat >> "$LIMITS_CONF" << EOF

# MXRC RT process limits
mxrc    soft    rtprio    99
mxrc    hard    rtprio    99
EOF

    log_info "✓ RLIMIT_RTPRIO configured for mxrc user"
fi

# 3. CPU 코어 개수 확인
log_info "Checking system CPU count..."

CPU_COUNT=$(nproc)
log_info "System has $CPU_COUNT CPU cores"

if [ "$CPU_COUNT" -lt 4 ]; then
    log_error "System has less than 4 CPU cores. RT core isolation requires at least 4 cores."
    exit 1
fi

log_info "✓ System has enough CPU cores for RT isolation"

# 4. systemd 버전 확인
log_info "Checking systemd version..."

SYSTEMD_VERSION=$(systemctl --version | head -n1 | awk '{print $2}')
log_info "systemd version: $SYSTEMD_VERSION"

if [ "$SYSTEMD_VERSION" -lt 255 ]; then
    log_warn "systemd version is less than 255. Some features may not work correctly."
else
    log_info "✓ systemd version is compatible"
fi

# 5. 서비스 파일 복사
log_info "Installing service files..."

SERVICE_SRC="systemd/mxrc-rt.service"
SERVICE_DEST="/etc/systemd/system/mxrc-rt.service"

if [ ! -f "$SERVICE_SRC" ]; then
    log_error "Service file not found: $SERVICE_SRC"
    log_error "Please run this script from the MXRC project root directory"
    exit 1
fi

cp "$SERVICE_SRC" "$SERVICE_DEST"
log_info "✓ Copied $SERVICE_SRC to $SERVICE_DEST"

# 6. 실행 파일 경로 확인
RT_BINARY="/usr/local/bin/mxrc-rt"

if [ ! -f "$RT_BINARY" ]; then
    log_warn "RT binary not found at $RT_BINARY"
    log_warn "You need to install the RT executive binary first"
    log_warn "Example: sudo cp build/rt $RT_BINARY && sudo chmod +x $RT_BINARY"
fi

# 7. systemd 설정 리로드
log_info "Reloading systemd daemon..."
systemctl daemon-reload
log_info "✓ systemd daemon reloaded"

# 8. 서비스 활성화 (자동 시작)
log_info "Enabling mxrc-rt service..."
systemctl enable mxrc-rt.service
log_info "✓ mxrc-rt service enabled"

# 9. 설치 완료
log_info ""
log_info "=========================================="
log_info "MXRC systemd service installation complete!"
log_info "=========================================="
log_info ""
log_info "Next steps:"
log_info "  1. Install the RT binary: sudo cp build/rt /usr/local/bin/mxrc-rt"
log_info "  2. Start the service: sudo systemctl start mxrc-rt"
log_info "  3. Check status: sudo systemctl status mxrc-rt"
log_info "  4. View logs: sudo journalctl -u mxrc-rt -f"
log_info ""

if [ "$RT_LIMIT_FOUND" = false ]; then
    log_warn "IMPORTANT: Log out and log back in for RLIMIT_RTPRIO changes to take effect"
fi

if ! grep -q "isolcpus" /proc/cmdline; then
    log_warn "IMPORTANT: For best RT performance, configure isolcpus and reboot:"
    log_warn "  1. Edit /etc/default/grub"
    log_warn "  2. Add 'isolcpus=2,3' to GRUB_CMDLINE_LINUX"
    log_warn "  3. Run: sudo update-grub && sudo reboot"
fi

exit 0
