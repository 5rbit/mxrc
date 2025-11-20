#!/bin/bash
# MXRC Systemd Services Installation Script

set -e

echo "========================================="
echo " MXRC Systemd Services Installation"
echo "========================================="

# Root 권한 확인
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

# 빌드 파일 확인
if [ ! -f "build/rt" ] || [ ! -f "build/nonrt" ]; then
    echo "Error: Build files not found. Please run 'cmake --build build' first."
    exit 1
fi

echo ""
echo "Step 1: Creating system user and group..."
if ! id -u mxrc > /dev/null 2>&1; then
    useradd -r -s /bin/false -d /var/lib/mxrc -M mxrc
    echo "✓ User 'mxrc' created"
else
    echo "✓ User 'mxrc' already exists"
fi

mkdir -p /var/lib/mxrc
chown mxrc:mxrc /var/lib/mxrc
echo "✓ Working directory created: /var/lib/mxrc"

echo ""
echo "Step 2: Installing executables..."
cp build/rt /usr/local/bin/mxrc-rt
cp build/nonrt /usr/local/bin/mxrc-nonrt
chmod 755 /usr/local/bin/mxrc-rt
chmod 755 /usr/local/bin/mxrc-nonrt
chown root:root /usr/local/bin/mxrc-rt
chown root:root /usr/local/bin/mxrc-nonrt
echo "✓ Installed: /usr/local/bin/mxrc-rt"
echo "✓ Installed: /usr/local/bin/mxrc-nonrt"

echo ""
echo "Step 3: Installing systemd service files..."
cp systemd/mxrc-rt.service /etc/systemd/system/
cp systemd/mxrc-nonrt.service /etc/systemd/system/
chmod 644 /etc/systemd/system/mxrc-rt.service
chmod 644 /etc/systemd/system/mxrc-nonrt.service
echo "✓ Installed: /etc/systemd/system/mxrc-rt.service"
echo "✓ Installed: /etc/systemd/system/mxrc-nonrt.service"

echo ""
echo "Step 4: Reloading systemd..."
systemctl daemon-reload
echo "✓ Systemd reloaded"

echo ""
echo "Step 5: Enabling services..."
systemctl enable mxrc-rt.service
systemctl enable mxrc-nonrt.service
echo "✓ Services enabled"

echo ""
echo "========================================="
echo " Installation completed successfully!"
echo "========================================="
echo ""
echo "Next steps:"
echo "  1. Start services:"
echo "     sudo systemctl start mxrc-rt.service"
echo "     sudo systemctl start mxrc-nonrt.service"
echo ""
echo "  2. Check status:"
echo "     sudo systemctl status mxrc-rt.service"
echo "     sudo systemctl status mxrc-nonrt.service"
echo ""
echo "  3. View logs:"
echo "     sudo journalctl -u mxrc-rt.service -f"
echo "     sudo journalctl -u mxrc-nonrt.service -f"
echo ""
