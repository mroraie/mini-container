#!/bin/bash

# Test CPU usage monitoring for containers
# This test verifies that CPU usage is correctly tracked when running CPU-intensive tasks

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/mini-container"

echo "========================================="
echo "CPU Usage Test"
echo "========================================="

# Check if binary exists
if [ ! -f "$BINARY" ]; then
    echo "Error: mini-container binary not found at $BINARY"
    echo "Please build the project first: make"
    exit 1
fi

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This test requires root privileges"
    exit 1
fi

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    # Kill any running containers
    CONTAINERS=$($BINARY list 2>/dev/null | grep "container_" | awk '{print $1}' || true)
    for container in $CONTAINERS; do
        $BINARY stop "$container" 2>/dev/null || true
        $BINARY destroy "$container" 2>/dev/null || true
    done
}

trap cleanup EXIT

echo "Test 1: Running CPU-intensive task and checking CPU usage"
echo "----------------------------------------------------------"

# Create a container with CPU-intensive task
# Run in background with timeout to avoid blocking
RUN_OUTPUT=$(timeout 2 $BINARY run \
    -m 128 \
    -c 1024 \
    -n "cpu-test" \
    -r "/tmp/cpu_test_root" \
    /bin/sh -c "while true; do :; done" 2>&1 || true)
CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -o "container_[0-9_]*" | head -1)

# If not found in output, try to get from list
if [ -z "$CONTAINER_ID" ]; then
    sleep 1
    CONTAINER_ID=$($BINARY list 2>/dev/null | grep -E "(RUNNING|CREATED)" | awk '{print $1}' | head -1)
fi

if [ -z "$CONTAINER_ID" ]; then
    echo "Error: Failed to create container"
    exit 1
fi

echo "Container created: $CONTAINER_ID"

# Wait a bit for the process to start consuming CPU
echo "Waiting 2 seconds for CPU usage to accumulate..."
sleep 2

# Check CPU usage multiple times
echo ""
echo "Checking CPU usage (should increase over time):"

for i in {1..5}; do
    CPU_USAGE=$($BINARY info "$CONTAINER_ID" 2>/dev/null | grep "CPU Usage" | awk '{print $3}' || echo "0")
    MEMORY_USAGE=$($BINARY info "$CONTAINER_ID" 2>/dev/null | grep "Memory Usage" | awk '{print $3}' || echo "0")
    
    echo "  Check $i: CPU=$CPU_USAGE ns, Memory=$MEMORY_USAGE bytes"
    
    if [ "$CPU_USAGE" != "0" ] && [ "$CPU_USAGE" != "" ]; then
        echo "  ✓ CPU usage is being tracked!"
        break
    fi
    
    if [ $i -lt 5 ]; then
        sleep 1
    fi
done

# Final check
CPU_USAGE=$($BINARY info "$CONTAINER_ID" 2>/dev/null | grep "CPU Usage" | awk '{print $3}' || echo "0")
MEMORY_USAGE=$($BINARY info "$CONTAINER_ID" 2>/dev/null | grep "Memory Usage" | awk '{print $3}' || echo "0")

echo ""
if [ "$CPU_USAGE" = "0" ] || [ -z "$CPU_USAGE" ]; then
    echo "✗ FAILED: CPU usage is still 0 after running CPU-intensive task"
    echo "  This indicates that CPU usage tracking is not working correctly"
    exit 1
else
    echo "✓ SUCCESS: CPU usage is being tracked: $CPU_USAGE ns"
fi

if [ "$MEMORY_USAGE" = "0" ] || [ -z "$MEMORY_USAGE" ]; then
    echo "⚠ WARNING: Memory usage is 0 (might be expected for simple processes)"
else
    echo "✓ Memory usage is being tracked: $MEMORY_USAGE bytes"
fi

# Stop the container
echo ""
echo "Stopping container..."
$BINARY stop "$CONTAINER_ID" || true
sleep 1

echo ""
echo "========================================="
echo "All tests passed!"
echo "========================================="

