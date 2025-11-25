#!/bin/bash
# MXRC WebAPI systemd 서비스 제거 스크립트
#
# 사용법: sudo ./scripts/uninstall-systemd.sh

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

# Root 권한 확인
if [[ $EUID -ne 0 ]]; then
   log_error "This script must be run as root (use sudo)"
   exit 1
fi

# 변수 설정
SERVICE_NAME="mxrc-webapi.service"
SYSTEMD_DIR="/etc/systemd/system"
INSTALL_DIR="/opt/mxrc/webapi"

log_info "MXRC WebAPI systemd Uninstallation"
log_info "===================================="

# 1. 서비스 중지
log_info "Stopping service..."
if systemctl is-active --quiet "$SERVICE_NAME"; then
    systemctl stop "$SERVICE_NAME"
    log_info "Service stopped"
else
    log_warn "Service is not running"
fi

# 2. 서비스 비활성화
log_info "Disabling service..."
if systemctl is-enabled --quiet "$SERVICE_NAME"; then
    systemctl disable "$SERVICE_NAME"
    log_info "Service disabled"
else
    log_warn "Service is not enabled"
fi

# 3. 서비스 파일 제거
log_info "Removing service file..."
if [[ -f "$SYSTEMD_DIR/$SERVICE_NAME" ]]; then
    rm "$SYSTEMD_DIR/$SERVICE_NAME"
    log_info "Service file removed"
else
    log_warn "Service file not found"
fi

# 4. systemd 리로드
log_info "Reloading systemd daemon..."
systemctl daemon-reload
systemctl reset-failed

# 5. 설치 디렉토리 제거 (옵션)
read -p "Do you want to remove installation directory ($INSTALL_DIR)? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [[ -d "$INSTALL_DIR" ]]; then
        rm -rf "$INSTALL_DIR"
        log_info "Installation directory removed"
    fi
else
    log_info "Installation directory kept"
fi

# 6. 런타임 디렉토리 정리 (IPC 소켓 등)
log_warn "Runtime directory (/var/run/mxrc) was not removed (shared with other MXRC services)"

log_info ""
log_info "===================================="
log_info "Uninstallation completed!"
log_info "===================================="
log_info ""
log_info "Note: User 'mxrc' and log files were not removed."
log_info "To remove them manually:"
log_info "  User: sudo userdel mxrc"
log_info "  Logs: sudo rm -rf /var/log/mxrc/webapi*"
log_info ""
