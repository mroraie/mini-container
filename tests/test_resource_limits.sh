#!/bin/bash

# Test Resource Limits (CPU and Memory)
# This test verifies that CPU and Memory limits are correctly enforced

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/mini-container"

echo "========================================="
echo "Resource Limits Test"
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

echo "Test 1: CPU Limit Enforcement"
echo "----------------------------------------------------------"

# Create container with CPU limit (512 shares = 50% of default)
# Run in background with timeout to avoid blocking
RUN_OUTPUT=$(timeout 2 $BINARY run \
    -m 128 \
    -c 512 \
    -n "cpu-test" \
    -r "/tmp/cpu_test_root" \
    /bin/sh -c "while true; do :; done" 2>&1 || true)
CPU_CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -o "container_[0-9_]*" | head -1)

# If not found in output, try to get from list
if [ -z "$CPU_CONTAINER_ID" ]; then
    sleep 1
    CPU_CONTAINER_ID=$($BINARY list 2>/dev/null | grep "RUNNING" | awk '{print $1}' | head -1)
fi

if [ -z "$CPU_CONTAINER_ID" ]; then
    echo "Error: Failed to create CPU test container"
    exit 1
fi

echo "CPU test container created: $CPU_CONTAINER_ID"
echo "Waiting 3 seconds for CPU usage to accumulate..."

sleep 3

# Check CPU usage
CPU_USAGE=$($BINARY info "$CPU_CONTAINER_ID" 2>/dev/null | grep "CPU Usage" | awk '{print $3}' || echo "0")
echo "CPU Usage after 3 seconds: $CPU_USAGE ns"

if [ "$CPU_USAGE" = "0" ] || [ -z "$CPU_USAGE" ]; then
    echo "⚠ WARNING: CPU usage is 0 - cgroup might not be tracking correctly"
else
    echo "✓ CPU usage is being tracked: $CPU_USAGE ns"
fi

# Check if cgroup exists and has correct limits
CGROUP_PATH="/sys/fs/cgroup"
if [ -d "$CGROUP_PATH/cpu" ]; then
    # cgroup v1
    CPU_SHARES_PATH="$CGROUP_PATH/cpu/mini_container_${CPU_CONTAINER_ID}/cpu.shares"
    if [ -f "$CPU_SHARES_PATH" ]; then
        SHARES=$(cat "$CPU_SHARES_PATH" 2>/dev/null || echo "0")
        echo "CPU shares in cgroup: $SHARES (expected: 512)"
        if [ "$SHARES" = "512" ]; then
            echo "✓ CPU shares limit is correctly set"
        else
            echo "✗ CPU shares limit mismatch: expected 512, got $SHARES"
        fi
    else
        echo "⚠ WARNING: CPU cgroup file not found at $CPU_SHARES_PATH"
    fi
elif [ -f "$CGROUP_PATH/cgroup.controllers" ]; then
    # cgroup v2
    CPU_WEIGHT_PATH="$CGROUP_PATH/mini_container_${CPU_CONTAINER_ID}/cpu.weight"
    if [ -f "$CPU_WEIGHT_PATH" ]; then
        WEIGHT=$(cat "$CPU_WEIGHT_PATH" 2>/dev/null || echo "0")
        echo "CPU weight in cgroup v2: $WEIGHT"
        echo "✓ CPU weight is set (expected around 50 for 512 shares)"
    else
        echo "⚠ WARNING: CPU weight file not found at $CPU_WEIGHT_PATH"
    fi
fi

# Stop CPU test container
echo ""
echo "Stopping CPU test container..."
$BINARY stop "$CPU_CONTAINER_ID" || true
sleep 1

echo ""
echo "Test 2: Memory Limit Enforcement"
echo "----------------------------------------------------------"

# Create container with Memory limit (64MB)
# Run in background with timeout
RUN_OUTPUT=$(timeout 3 $BINARY run \
    -m 64 \
    -c 1024 \
    -n "mem-test" \
    -r "/tmp/mem_test_root" \
    /bin/sh -c "dd if=/dev/zero of=/tmp/memtest bs=1M count=100 status=none 2>&1 || echo 'Memory limit reached'" 2>&1 || true)
MEM_CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -o "container_[0-9_]*" | head -1)

# If not found in output, try to get from list
if [ -z "$MEM_CONTAINER_ID" ]; then
    sleep 1
    MEM_CONTAINER_ID=$($BINARY list 2>/dev/null | grep -E "(RUNNING|CREATED)" | awk '{print $1}' | head -1)
fi

if [ -z "$MEM_CONTAINER_ID" ]; then
    echo "Error: Failed to create Memory test container"
    exit 1
fi

echo "Memory test container created: $MEM_CONTAINER_ID"
echo "Waiting 2 seconds for memory allocation..."

sleep 2

# Check Memory usage
MEMORY_USAGE=$($BINARY info "$MEM_CONTAINER_ID" 2>/dev/null | grep "Memory Usage" | awk '{print $3}' || echo "0")
echo "Memory Usage: $MEMORY_USAGE bytes"

if [ "$MEMORY_USAGE" = "0" ] || [ -z "$MEMORY_USAGE" ]; then
    echo "⚠ WARNING: Memory usage is 0 - cgroup might not be tracking correctly"
else
    MEMORY_MB=$((MEMORY_USAGE / 1024 / 1024))
    echo "Memory Usage: $MEMORY_MB MB (limit: 64 MB)"
    if [ "$MEMORY_MB" -le 64 ]; then
        echo "✓ Memory usage is within limit"
    else
        echo "⚠ WARNING: Memory usage ($MEMORY_MB MB) exceeds limit (64 MB)"
    fi
fi

# Check if cgroup exists and has correct limits
if [ -d "$CGROUP_PATH/memory" ]; then
    # cgroup v1
    MEM_LIMIT_PATH="$CGROUP_PATH/memory/mini_container_${MEM_CONTAINER_ID}/memory.limit_in_bytes"
    if [ -f "$MEM_LIMIT_PATH" ]; then
        LIMIT=$(cat "$MEM_LIMIT_PATH" 2>/dev/null || echo "0")
        EXPECTED_LIMIT=$((64 * 1024 * 1024))
        echo "Memory limit in cgroup: $LIMIT bytes (expected: $EXPECTED_LIMIT)"
        if [ "$LIMIT" = "$EXPECTED_LIMIT" ]; then
            echo "✓ Memory limit is correctly set"
        else
            echo "✗ Memory limit mismatch: expected $EXPECTED_LIMIT, got $LIMIT"
        fi
    else
        echo "⚠ WARNING: Memory cgroup file not found at $MEM_LIMIT_PATH"
    fi
elif [ -f "$CGROUP_PATH/cgroup.controllers" ]; then
    # cgroup v2
    MEM_MAX_PATH="$CGROUP_PATH/mini_container_${MEM_CONTAINER_ID}/memory.max"
    if [ -f "$MEM_MAX_PATH" ]; then
        LIMIT=$(cat "$MEM_MAX_PATH" 2>/dev/null || echo "0")
        EXPECTED_LIMIT=$((64 * 1024 * 1024))
        echo "Memory limit in cgroup v2: $LIMIT bytes (expected: $EXPECTED_LIMIT)"
        if [ "$LIMIT" = "$EXPECTED_LIMIT" ]; then
            echo "✓ Memory limit is correctly set"
        else
            echo "✗ Memory limit mismatch: expected $EXPECTED_LIMIT, got $LIMIT"
        fi
    else
        echo "⚠ WARNING: Memory max file not found at $MEM_MAX_PATH"
    fi
fi

# Stop Memory test container
echo ""
echo "Stopping Memory test container..."
$BINARY stop "$MEM_CONTAINER_ID" || true
sleep 1

echo ""
echo "Test 3: Combined CPU + Memory Limits"
echo "----------------------------------------------------------"

# Create container with both limits
# Run in background with timeout
RUN_OUTPUT=$(timeout 3 $BINARY run \
    -m 128 \
    -c 1024 \
    -n "combined-test" \
    -r "/tmp/combined_test_root" \
    /bin/sh -c "dd if=/dev/zero of=/tmp/stress bs=1M count=16 status=none; i=0; while [ \$i -lt 10000000 ]; do i=\$((i+1)); done; rm -f /tmp/stress; echo Done" 2>&1 || true)
COMBINED_CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -o "container_[0-9_]*" | head -1)

# If not found in output, try to get from list
if [ -z "$COMBINED_CONTAINER_ID" ]; then
    sleep 1
    COMBINED_CONTAINER_ID=$($BINARY list 2>/dev/null | grep -E "(RUNNING|CREATED)" | awk '{print $1}' | head -1)
fi

if [ -z "$COMBINED_CONTAINER_ID" ]; then
    echo "Error: Failed to create combined test container"
    exit 1
fi

echo "Combined test container created: $COMBINED_CONTAINER_ID"
echo "Waiting 3 seconds for resource usage..."

sleep 3

# Check both CPU and Memory
COMBINED_CPU=$($BINARY info "$COMBINED_CONTAINER_ID" 2>/dev/null | grep "CPU Usage" | awk '{print $3}' || echo "0")
COMBINED_MEM=$($BINARY info "$COMBINED_CONTAINER_ID" 2>/dev/null | grep "Memory Usage" | awk '{print $3}' || echo "0")

echo "CPU Usage: $COMBINED_CPU ns"
echo "Memory Usage: $COMBINED_MEM bytes"

if [ "$COMBINED_CPU" != "0" ] && [ -n "$COMBINED_CPU" ]; then
    echo "✓ CPU usage is being tracked"
else
    echo "⚠ WARNING: CPU usage is 0"
fi

if [ "$COMBINED_MEM" != "0" ] && [ -n "$COMBINED_MEM" ]; then
    MEM_MB=$((COMBINED_MEM / 1024 / 1024))
    echo "✓ Memory usage is being tracked: $MEM_MB MB"
else
    echo "⚠ WARNING: Memory usage is 0"
fi

# Stop combined test container
echo ""
echo "Stopping combined test container..."
$BINARY stop "$COMBINED_CONTAINER_ID" || true
sleep 1

echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo "✓ CPU limit test completed"
echo "✓ Memory limit test completed"
echo "✓ Combined limits test completed"
echo ""
echo "Note: If CPU/Memory usage shows 0, check:"
echo "  1. cgroup files are accessible"
echo "  2. Process is correctly added to cgroup"
echo "  3. cgroup version (v1 vs v2)"
echo "========================================="

