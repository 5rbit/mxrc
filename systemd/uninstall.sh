#!/bin/bash
# MXRC Systemd Services Uninstallation Script

set -e

echo "========================================="
echo " MXRC Systemd Services Uninstallation"
echo "========================================="

# Root 권한 확인
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

echo ""
echo "Step 1: Stopping services..."
systemctl stop mxrc-rt.service 2>/dev/null || true
systemctl stop mxrc-nonrt.service 2>/dev/null || true
echo "✓ Services stopped"

echo ""
echo "Step 2: Disabling services..."
systemctl disable mxrc-rt.service 2>/dev/null || true
systemctl disable mxrc-nonrt.service 2>/dev/null || true
echo "✓ Services disabled"

echo ""
echo "Step 3: Removing service files..."
rm -f /etc/systemd/system/mxrc-rt.service
rm -f /etc/systemd/system/mxrc-nonrt.service
echo "✓ Service files removed"

echo ""
echo "Step 4: Reloading systemd..."
systemctl daemon-reload
systemctl reset-failed
echo "✓ Systemd reloaded"

echo ""
echo "Step 5: Removing executables..."
rm -f /usr/local/bin/mxrc-rt
rm -f /usr/local/bin/mxrc-nonrt
echo "✓ Executables removed"

echo ""
echo "Step 6: Cleaning up shared memory..."
rm -f /dev/shm/mxrc_shm
echo "✓ Shared memory cleaned up"

echo ""
read -p "Remove user 'mxrc' and data directory? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Step 7: Removing user and data..."
    userdel mxrc 2>/dev/null || true
    rm -rf /var/lib/mxrc
    echo "✓ User and data removed"
else
    echo "Step 7: Skipped (user and data preserved)"
fi

echo ""
echo "========================================="
echo " Uninstallation completed successfully!"
echo "========================================="
