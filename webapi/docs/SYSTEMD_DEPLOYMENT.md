# MXRC WebAPI systemd Deployment Guide

Complete guide for deploying MXRC WebAPI as a systemd service on Ubuntu 24.04 LTS with PREEMPT_RT kernel.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Installation](#quick-installation)
- [Manual Installation](#manual-installation)
- [Configuration](#configuration)
- [Service Management](#service-management)
- [Monitoring](#monitoring)
- [Troubleshooting](#troubleshooting)
- [Security](#security)
- [Performance Tuning](#performance-tuning)

## Prerequisites

### System Requirements

- **OS**: Ubuntu 24.04 LTS with PREEMPT_RT kernel
- **Node.js**: 18.x or 20.x LTS
- **Memory**: Minimum 512MB available
- **CPU**: 2+ cores (WebAPI runs on Non-RT cores 0-1)
- **systemd**: Version 249 or later

### Required Services

- `mxrc-rt.service` - MXRC Real-Time Process (must be running)
- `mxrc-nonrt.service` - MXRC Non-RT Process (recommended)

### Required Permissions

- IPC socket access: `/var/run/mxrc/ipc.sock`
- Network binding: Port 8080 (default)
- File system: Read/write to `/var/run/mxrc`, `/var/log/mxrc`

## Quick Installation

### Automatic Installation

```bash
# 1. Navigate to webapi directory
cd /path/to/mxrc/webapi

# 2. Run installation script
sudo ./scripts/install-systemd.sh
```

The script will:
- ✅ Check Node.js installation
- ✅ Create `mxrc` user and group
- ✅ Build TypeScript application
- ✅ Copy files to `/opt/mxrc/webapi`
- ✅ Install systemd service file
- ✅ Set proper permissions
- ✅ Enable auto-start on boot

### Start the Service

```bash
# Start service
sudo systemctl start mxrc-webapi

# Check status
sudo systemctl status mxrc-webapi

# View logs
sudo journalctl -u mxrc-webapi -f
```

## Manual Installation

### Step 1: Install Node.js

```bash
# Install Node.js 20.x LTS
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs

# Verify installation
node --version  # Should be v20.x.x
npm --version   # Should be 10.x.x
```

### Step 2: Create System User

```bash
# Create mxrc user (no login shell)
sudo useradd --system --no-create-home --shell /bin/false mxrc
```

### Step 3: Create Directory Structure

```bash
# Create directories
sudo mkdir -p /opt/mxrc/webapi
sudo mkdir -p /var/run/mxrc
sudo mkdir -p /opt/mxrc/config/ipc
sudo mkdir -p /var/log/mxrc

# Set ownership
sudo chown -R mxrc:mxrc /opt/mxrc
sudo chown -R mxrc:mxrc /var/run/mxrc
sudo chown -R mxrc:mxrc /var/log/mxrc
```

### Step 4: Build and Install Application

```bash
# Build TypeScript
cd /path/to/mxrc/webapi
npm install
npm run build

# Copy files to installation directory
sudo cp -r dist /opt/mxrc/webapi/
sudo cp -r node_modules /opt/mxrc/webapi/
sudo cp package.json /opt/mxrc/webapi/

# Copy IPC schema
sudo cp ../config/ipc/ipc-schema.yaml /opt/mxrc/config/ipc/

# Set ownership
sudo chown -R mxrc:mxrc /opt/mxrc/webapi
```

### Step 5: Install systemd Service

```bash
# Copy service file
sudo cp ../systemd/mxrc-webapi.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable auto-start
sudo systemctl enable mxrc-webapi
```

### Step 6: Start Service

```bash
# Start service
sudo systemctl start mxrc-webapi

# Verify it's running
sudo systemctl status mxrc-webapi
```

## Configuration

### Environment Variables

Edit `/etc/systemd/system/mxrc-webapi.service`:

```ini
[Service]
Environment="NODE_ENV=production"
Environment="PORT=8080"
Environment="HOST=0.0.0.0"
Environment="LOG_LEVEL=info"
Environment="IPC_SOCKET_PATH=/var/run/mxrc/ipc.sock"
Environment="IPC_TIMEOUT_MS=5000"
Environment="SYSTEMD_NOTIFY=true"
Environment="WATCHDOG_ENABLED=true"
Environment="CORS_ORIGIN=https://dashboard.example.com"
```

After editing, reload:

```bash
sudo systemctl daemon-reload
sudo systemctl restart mxrc-webapi
```

### Resource Limits

Adjust CPU and memory limits in the service file:

```ini
[Service]
# CPU limit (150% = 1.5 cores)
CPUQuota=150%

# Memory limits
MemoryMax=512M      # Hard limit
MemoryHigh=384M     # Soft limit (triggers pressure)

# I/O priority
IOWeight=100
```

### Watchdog Configuration

The service includes systemd watchdog for automatic recovery:

```ini
[Service]
WatchdogSec=60s           # Watchdog timeout
Restart=on-failure        # Restart policy
RestartSec=10s           # Delay before restart
StartLimitBurst=5        # Max restart attempts
StartLimitIntervalSec=120s  # Time window for restarts
```

The WebAPI sends watchdog pings every 30 seconds to systemd. If it fails to respond within 60 seconds, systemd will restart the service.

## Service Management

### Basic Commands

```bash
# Start service
sudo systemctl start mxrc-webapi

# Stop service
sudo systemctl stop mxrc-webapi

# Restart service
sudo systemctl restart mxrc-webapi

# Reload configuration (graceful)
sudo systemctl reload-or-restart mxrc-webapi

# Check status
sudo systemctl status mxrc-webapi

# Enable auto-start on boot
sudo systemctl enable mxrc-webapi

# Disable auto-start
sudo systemctl disable mxrc-webapi
```

### Service Status

```bash
# Detailed status
systemctl status mxrc-webapi

# Check if service is active
systemctl is-active mxrc-webapi

# Check if service is enabled
systemctl is-enabled mxrc-webapi

# Show service properties
systemctl show mxrc-webapi
```

### Dependency Management

```bash
# List dependencies
systemctl list-dependencies mxrc-webapi

# Start with dependencies
sudo systemctl start mxrc.target

# Check what depends on this service
systemctl list-dependencies --reverse mxrc-webapi
```

## Monitoring

### View Logs

```bash
# Follow logs in real-time
sudo journalctl -u mxrc-webapi -f

# View last 100 lines
sudo journalctl -u mxrc-webapi -n 100

# View logs since boot
sudo journalctl -u mxrc-webapi -b

# View logs from today
sudo journalctl -u mxrc-webapi --since today

# View error logs only
sudo journalctl -u mxrc-webapi -p err

# Export logs to file
sudo journalctl -u mxrc-webapi > webapi-logs.txt
```

### Parse JSON Logs

WebAPI logs are in JSON format. Use `jq` to parse:

```bash
# Install jq
sudo apt-get install -y jq

# Pretty-print logs
sudo journalctl -u mxrc-webapi -o json | jq -r '.MESSAGE | fromjson'

# Filter by level
sudo journalctl -u mxrc-webapi -o json | jq -r 'select(.MESSAGE | fromjson | .level == 50) | .MESSAGE | fromjson'

# Extract errors
sudo journalctl -u mxrc-webapi -o json | jq -r 'select(.MESSAGE | fromjson | .level >= 50) | .MESSAGE | fromjson | "\(.time) [\(.level)] \(.msg)"'
```

### Resource Usage

```bash
# CPU and memory usage
systemctl status mxrc-webapi | grep -E "(CPU|Memory)"

# Detailed resource accounting
systemd-cgtop -1 | grep mxrc-webapi

# Show resource limits
systemctl show mxrc-webapi --property=CPUQuota,MemoryMax,IOWeight

# Resource usage over time
journalctl -u mxrc-webapi | grep "memory_usage_mb\|uptime_seconds"
```

### Health Check

```bash
# Check health endpoint
curl http://localhost:8080/api/health

# Check readiness
curl http://localhost:8080/api/health/ready

# Check liveness
curl http://localhost:8080/api/health/live

# WebSocket statistics
curl http://localhost:8080/api/ws/stats
```

### Watchdog Status

```bash
# Check watchdog status
systemctl show mxrc-webapi --property=WatchdogTimestamp,WatchdogTimestampMonotonic

# View watchdog events in logs
sudo journalctl -u mxrc-webapi | grep -i watchdog
```

## Troubleshooting

### Service Won't Start

**Check logs:**

```bash
sudo journalctl -u mxrc-webapi -n 50 --no-pager
```

**Common issues:**

1. **Node.js not found**
   ```bash
   # Install Node.js
   curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
   sudo apt-get install -y nodejs
   ```

2. **Permission denied**
   ```bash
   # Fix permissions
   sudo chown -R mxrc:mxrc /opt/mxrc/webapi
   sudo chown -R mxrc:mxrc /var/run/mxrc
   ```

3. **IPC socket not found**
   ```bash
   # Check if MXRC RT is running
   sudo systemctl status mxrc-rt

   # Check socket exists
   ls -la /var/run/mxrc/ipc.sock
   ```

4. **Port already in use**
   ```bash
   # Check what's using port 8080
   sudo lsof -i :8080

   # Change port in service file
   sudo systemctl edit mxrc-webapi
   # Add: Environment="PORT=8081"
   ```

### Service Keeps Restarting

**Check restart count:**

```bash
systemctl show mxrc-webapi --property=NRestarts
```

**View crash logs:**

```bash
sudo journalctl -u mxrc-webapi --since "1 hour ago" | grep -i "error\|failed\|exit"
```

**Common causes:**

1. **Watchdog timeout** - Service not responding to health checks
2. **Memory limit exceeded** - Increase MemoryMax
3. **Unhandled exceptions** - Check application logs
4. **IPC connection failures** - Ensure MXRC RT is stable

### High Memory Usage

```bash
# Check current memory
systemctl status mxrc-webapi | grep Memory

# View memory trend
journalctl -u mxrc-webapi | grep "memory_usage_mb" | tail -20

# Increase memory limit
sudo systemctl edit mxrc-webapi
# Add: MemoryMax=1G
sudo systemctl restart mxrc-webapi
```

### Connection Issues

**Check if service is listening:**

```bash
sudo ss -tlnp | grep 8080
```

**Test locally:**

```bash
curl -v http://localhost:8080/api/health/live
```

**Check firewall:**

```bash
sudo ufw status
sudo ufw allow 8080/tcp
```

**Check SELinux (if enabled):**

```bash
sudo getenforce
sudo ausearch -m avc -ts recent | grep mxrc-webapi
```

## Security

### Security Hardening Features

The service file includes comprehensive security hardening:

```ini
[Service]
# Prevent privilege escalation
NoNewPrivileges=true

# Filesystem protection
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
PrivateDevices=true

# Kernel protection
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectKernelLogs=true
ProtectControlGroups=true

# Restrict system calls
SystemCallFilter=@system-service
SystemCallFilter=~@privileged @resources @obsolete

# Network restrictions
RestrictAddressFamilies=AF_UNIX AF_INET AF_INET6
IPAddressAllow=localhost 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16

# Capabilities (none required)
CapabilityBoundingSet=
AmbientCapabilities=

# Memory protection
MemoryDenyWriteExecute=true
LockPersonality=true
RestrictRealtime=true
```

### Audit Security

```bash
# Run systemd-analyze security
systemd-analyze security mxrc-webapi

# Expected score: 2.1 SAFE (lower is better)
```

### File Permissions

```bash
# Verify permissions
ls -la /opt/mxrc/webapi
# Should be owned by mxrc:mxrc

ls -la /var/run/mxrc
# Should be owned by mxrc:mxrc with 755

ls -la /etc/systemd/system/mxrc-webapi.service
# Should be owned by root:root with 644
```

## Performance Tuning

### CPU Affinity

WebAPI runs on Non-RT cores (0-1) by default. Adjust if needed:

```ini
[Service]
# Use cores 0, 1, 4, 5 (example for 8-core system)
CPUAffinity=0 1 4 5
```

### Connection Limits

Increase if handling many concurrent connections:

```ini
[Service]
# Increase file descriptor limit
LimitNOFILE=65536

# Task limit (threads/processes)
TasksMax=512
```

### Network Performance

```bash
# Increase socket buffer sizes (system-wide)
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216

# Make permanent
echo "net.core.rmem_max=16777216" | sudo tee -a /etc/sysctl.conf
echo "net.core.wmem_max=16777216" | sudo tee -a /etc/sysctl.conf
```

### Node.js Performance

Add Node.js flags to service file:

```ini
[Service]
Environment="NODE_OPTIONS=--max-old-space-size=384 --max-semi-space-size=32"
```

## Uninstallation

### Automatic Uninstallation

```bash
sudo ./scripts/uninstall-systemd.sh
```

### Manual Uninstallation

```bash
# Stop and disable service
sudo systemctl stop mxrc-webapi
sudo systemctl disable mxrc-webapi

# Remove service file
sudo rm /etc/systemd/system/mxrc-webapi.service

# Reload systemd
sudo systemctl daemon-reload

# Remove installation (optional)
sudo rm -rf /opt/mxrc/webapi

# Remove user (optional, if not used by other services)
# sudo userdel mxrc
```

## Integration with mxrc.target

The WebAPI service is part of the `mxrc.target` group:

```bash
# Start all MXRC services
sudo systemctl start mxrc.target

# Stop all MXRC services
sudo systemctl stop mxrc.target

# Check target status
systemctl status mxrc.target
```

## References

- [systemd Service Documentation](https://www.freedesktop.org/software/systemd/man/systemd.service.html)
- [systemd Security Features](https://www.freedesktop.org/software/systemd/man/systemd.exec.html#Security)
- [Node.js Production Best Practices](https://nodejs.org/en/docs/guides/nodejs-docker-webapp/)
- [MXRC WebAPI README](../README.md)

## Support

For issues or questions:
- Check logs: `sudo journalctl -u mxrc-webapi -f`
- Review configuration: `systemctl cat mxrc-webapi`
- Test manually: `sudo -u mxrc node /opt/mxrc/webapi/dist/server.js`
- Contact: dev-team@mxrc-project.local
