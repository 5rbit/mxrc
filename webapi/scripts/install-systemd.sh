#!/bin/bash
# MXRC WebAPI systemd 서비스 설치 스크립트
#
# 사용법: sudo ./scripts/install-systemd.sh

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
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEBAPI_ROOT="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$WEBAPI_ROOT")"
SERVICE_FILE="${PROJECT_ROOT}/systemd/mxrc-webapi.service"
SYSTEMD_DIR="/etc/systemd/system"
INSTALL_DIR="/opt/mxrc"
RUNTIME_DIR="/var/run/mxrc"
CONFIG_DIR="/opt/mxrc/config"
LOG_DIR="/var/log/mxrc"

log_info "MXRC WebAPI systemd Installation"
log_info "=================================="

# 1. Node.js 설치 확인
log_info "Checking Node.js installation..."
if ! command -v node &> /dev/null; then
    log_error "Node.js is not installed. Please install Node.js 18+ first."
    log_info "Install with: curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - && sudo apt-get install -y nodejs"
    exit 1
fi

NODE_VERSION=$(node --version)
log_info "Node.js version: $NODE_VERSION"

# 2. systemd 서비스 파일 확인
log_info "Checking systemd service file..."
if [[ ! -f "$SERVICE_FILE" ]]; then
    log_error "Service file not found: $SERVICE_FILE"
    exit 1
fi

# 3. mxrc 사용자 및 그룹 생성
log_info "Creating mxrc user and group..."
if ! id -u mxrc &> /dev/null; then
    useradd --system --no-create-home --shell /bin/false mxrc
    log_info "Created user: mxrc"
else
    log_warn "User 'mxrc' already exists"
fi

# 4. 디렉토리 생성
log_info "Creating directories..."
mkdir -p "$INSTALL_DIR/webapi"
mkdir -p "$RUNTIME_DIR"
mkdir -p "$CONFIG_DIR/ipc"
mkdir -p "$LOG_DIR"

# 5. 파일 복사
log_info "Copying application files..."

# Build가 안되어 있으면 빌드
if [[ ! -d "$WEBAPI_ROOT/dist" ]]; then
    log_info "Building TypeScript..."
    cd "$WEBAPI_ROOT"
    npm install --production=false
    npm run build
fi

# 빌드된 파일 복사
cp -r "$WEBAPI_ROOT/dist" "$INSTALL_DIR/webapi/"
cp -r "$WEBAPI_ROOT/node_modules" "$INSTALL_DIR/webapi/"
cp "$WEBAPI_ROOT/package.json" "$INSTALL_DIR/webapi/"

log_info "Application files copied to $INSTALL_DIR/webapi"

# 6. 설정 파일 복사 (있으면)
if [[ -f "$PROJECT_ROOT/config/ipc/ipc-schema.yaml" ]]; then
    cp "$PROJECT_ROOT/config/ipc/ipc-schema.yaml" "$CONFIG_DIR/ipc/"
    log_info "IPC schema copied"
fi

# 7. 권한 설정
log_info "Setting permissions..."
chown -R mxrc:mxrc "$INSTALL_DIR/webapi"
chown -R mxrc:mxrc "$RUNTIME_DIR"
chown -R mxrc:mxrc "$CONFIG_DIR"
chown -R mxrc:mxrc "$LOG_DIR"

chmod 755 "$INSTALL_DIR/webapi"
chmod 755 "$RUNTIME_DIR"
chmod 644 "$CONFIG_DIR/ipc/"*.yaml 2>/dev/null || true

# 8. systemd 서비스 파일 설치
log_info "Installing systemd service..."
cp "$SERVICE_FILE" "$SYSTEMD_DIR/mxrc-webapi.service"
chmod 644 "$SYSTEMD_DIR/mxrc-webapi.service"

# 9. systemd 리로드
log_info "Reloading systemd daemon..."
systemctl daemon-reload

# 10. 서비스 활성화 (자동 시작)
log_info "Enabling mxrc-webapi service..."
systemctl enable mxrc-webapi.service

log_info ""
log_info "=================================="
log_info "Installation completed successfully!"
log_info "=================================="
log_info ""
log_info "Next steps:"
log_info "  1. Configure environment in: $SYSTEMD_DIR/mxrc-webapi.service"
log_info "  2. Ensure MXRC RT/Non-RT services are running"
log_info "  3. Start the service: sudo systemctl start mxrc-webapi"
log_info "  4. Check status: sudo systemctl status mxrc-webapi"
log_info "  5. View logs: sudo journalctl -u mxrc-webapi -f"
log_info ""
log_info "Service management commands:"
log_info "  Start:   sudo systemctl start mxrc-webapi"
log_info "  Stop:    sudo systemctl stop mxrc-webapi"
log_info "  Restart: sudo systemctl restart mxrc-webapi"
log_info "  Status:  sudo systemctl status mxrc-webapi"
log_info "  Logs:    sudo journalctl -u mxrc-webapi -f"
log_info ""
