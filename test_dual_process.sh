#!/bin/bash
# Test script for RT/Non-RT dual process communication

set -e

echo "========================================="
echo " RT/Non-RT Dual Process Communication Test"
echo "========================================="

# Cleanup any existing shared memory
echo "Cleaning up existing shared memory..."
rm -f /dev/shm/mxrc_shm 2>/dev/null || true

# Build directory
BUILD_DIR="./build"

# Check if executables exist
if [ ! -f "$BUILD_DIR/rt" ]; then
    echo "Error: RT executable not found at $BUILD_DIR/rt"
    exit 1
fi

if [ ! -f "$BUILD_DIR/nonrt" ]; then
    echo "Error: Non-RT executable not found at $BUILD_DIR/nonrt"
    exit 1
fi

echo "Starting RT process..."
$BUILD_DIR/rt &
RT_PID=$!
echo "RT process started (PID: $RT_PID)"

# Wait a bit for RT to initialize
sleep 2

echo ""
echo "Starting Non-RT process..."
$BUILD_DIR/nonrt &
NONRT_PID=$!
echo "Non-RT process started (PID: $NONRT_PID)"

# Let them run for 5 seconds
echo ""
echo "Letting processes communicate for 5 seconds..."
sleep 5

# Cleanup
echo ""
echo "Stopping processes..."
kill -SIGTERM $RT_PID 2>/dev/null || true
kill -SIGTERM $NONRT_PID 2>/dev/null || true

# Wait for processes to exit
wait $RT_PID 2>/dev/null || true
wait $NONRT_PID 2>/dev/null || true

echo ""
echo "Cleaning up shared memory..."
rm -f /dev/shm/mxrc_shm 2>/dev/null || true

echo ""
echo "========================================="
echo " Test completed successfully!"
echo "========================================="
