#!/bin/bash
# MXRC WebAPI Docker Deployment Script

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_section() { echo -e "\n${BLUE}=== $1 ===${NC}\n"; }

# 스크립트 디렉토리
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEBAPI_ROOT="$(dirname "$SCRIPT_DIR")"

log_info "MXRC WebAPI Docker Deployment"
log_info "=============================="

# 1. Prerequisites
log_section "Checking Prerequisites"

if ! command -v docker &> /dev/null; then
    log_error "Docker not installed. Install with:"
    log_info "  curl -fsSL https://get.docker.com | sh"
    exit 1
fi

if ! command -v docker-compose &> /dev/null; then
    log_warn "docker-compose not found, trying docker compose plugin..."
    if ! docker compose version &> /dev/null; then
        log_error "docker compose not available"
        exit 1
    fi
    COMPOSE_CMD="docker compose"
else
    COMPOSE_CMD="docker-compose"
fi

log_info "Docker version: $(docker --version)"
log_info "Compose command: $COMPOSE_CMD"

# 2. Check Docker daemon
if ! docker ps &> /dev/null; then
    log_error "Docker daemon not running. Start with:"
    log_info "  sudo systemctl start docker"
    exit 1
fi

# 3. Navigate to webapi directory
cd "$WEBAPI_ROOT"

# 4. Build image
log_section "Building Docker Image"

log_info "Building mxrc-webapi:latest..."
$COMPOSE_CMD build

if [ $? -ne 0 ]; then
    log_error "Build failed"
    exit 1
fi

log_info "Build successful"

# 5. Check prerequisites (IPC socket, RT process)
log_section "Checking Runtime Prerequisites"

if [ ! -S /var/run/mxrc/ipc.sock ]; then
    log_warn "IPC socket not found at /var/run/mxrc/ipc.sock"
    log_warn "Make sure mxrc-rt and mxrc-nonrt are running"
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    [[ ! $REPLY =~ ^[Yy]$ ]] && exit 1
fi

# 6. Stop old container
log_section "Stopping Old Container"

if docker ps -a | grep -q mxrc-webapi; then
    log_info "Stopping and removing old container..."
    $COMPOSE_CMD down
else
    log_info "No existing container found"
fi

# 7. Start new container
log_section "Starting New Container"

log_info "Starting mxrc-webapi..."
$COMPOSE_CMD up -d

if [ $? -ne 0 ]; then
    log_error "Failed to start container"
    exit 1
fi

# 8. Wait for health check
log_section "Waiting for Health Check"

log_info "Waiting for container to be healthy..."
sleep 5

MAX_WAIT=30
WAIT_COUNT=0
while [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    if docker inspect mxrc-webapi | grep -q '"Status": "healthy"'; then
        log_info "Container is healthy!"
        break
    fi

    if [ $WAIT_COUNT -eq 0 ]; then
        echo -n "Waiting"
    fi
    echo -n "."
    sleep 1
    WAIT_COUNT=$((WAIT_COUNT + 1))
done
echo

if [ $WAIT_COUNT -eq $MAX_WAIT ]; then
    log_warn "Health check timeout, but container may still be starting..."
fi

# 9. Verify deployment
log_section "Verifying Deployment"

# Check container status
if ! docker ps | grep -q mxrc-webapi; then
    log_error "Container not running"
    log_info "Checking logs:"
    $COMPOSE_CMD logs --tail=50
    exit 1
fi

log_info "Container is running"

# Test health endpoint
if curl -sf http://localhost:8080/api/health/live > /dev/null 2>&1; then
    log_info "Health endpoint responding: ✓"
else
    log_warn "Health endpoint not responding yet"
fi

# 10. Show status
log_section "Deployment Status"

$COMPOSE_CMD ps

echo
log_info "Container logs (last 20 lines):"
$COMPOSE_CMD logs --tail=20

# 11. Resource usage
echo
log_info "Resource usage:"
docker stats mxrc-webapi --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}"

log_section "Deployment Complete"

log_info "WebAPI is now running in Docker"
log_info ""
log_info "Useful commands:"
log_info "  View logs:  $COMPOSE_CMD logs -f"
log_info "  Stop:       $COMPOSE_CMD stop"
log_info "  Restart:    $COMPOSE_CMD restart"
log_info "  Shell:      docker exec -it mxrc-webapi sh"
log_info "  Stats:      docker stats mxrc-webapi"
log_info ""
log_info "Test endpoints:"
log_info "  curl http://localhost:8080/api/health/live"
log_info "  curl http://localhost:8080/api/health"
log_info "  curl http://localhost:8080/api/ws/stats"
log_info ""
