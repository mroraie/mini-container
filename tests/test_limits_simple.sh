#!/bin/bash

# Simple Resource Limits Test
# Quick test to verify CPU and Memory limits are working

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/mini-container"

echo "========================================="
echo "Simple Resource Limits Test"
echo "========================================="

if [ ! -f "$BINARY" ]; then
    echo "Error: mini-container binary not found"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "Error: This test requires root privileges"
    exit 1
fi

cleanup() {
    echo ""
    echo "Cleaning up..."
    CONTAINERS=$($BINARY list 2>/dev/null | grep "container_" | awk '{print $1}' || true)
    for container in $CONTAINERS; do
        $BINARY stop "$container" 2>/dev/null || true
        $BINARY destroy "$container" 2>/dev/null || true
    done
}

trap cleanup EXIT

echo ""
echo "Test 1: CPU Limit (512 shares)"
echo "----------------------------------------"
# Run in background with timeout to avoid blocking
RUN_OUTPUT=$(timeout 2 $BINARY run \
    -m 128 \
    -c 512 \
    -n "cpu-limit-test" \
    -r "/tmp/cpu_limit_test" \
    /bin/sh -c "while true; do :; done" 2>&1 || true)
CPU_ID=$(echo "$RUN_OUTPUT" | grep -o "container_[0-9_]*" | head -1)

# If not found in output, try to get from list
if [ -z "$CPU_ID" ]; then
    sleep 1
    CPU_ID=$($BINARY list 2>/dev/null | grep -E "(RUNNING|CREATED)" | awk '{print $1}' | head -1)
fi

if [ -z "$CPU_ID" ]; then
    echo "✗ FAILED: Could not create CPU test container"
    exit 1
fi

echo "Container created: $CPU_ID"
sleep 2

CPU_USAGE=$($BINARY info "$CPU_ID" 2>&1 | grep "CPU Usage" | awk '{print $3}' || echo "0")
echo "CPU Usage: $CPU_USAGE ns"

if [ "$CPU_USAGE" != "0" ] && [ -n "$CPU_USAGE" ]; then
    echo "✓ PASS: CPU usage is being tracked"
else
    echo "✗ FAIL: CPU usage is 0 (cgroup might not be working)"
fi

# Check cgroup CPU limit
CGROUP_BASE="/sys/fs/cgroup"
if [ -d "$CGROUP_BASE/cpu" ]; then
    # cgroup v1
    SHARES_FILE="$CGROUP_BASE/cpu/mini_container_${CPU_ID}/cpu.shares"
    if [ -f "$SHARES_FILE" ]; then
        SHARES=$(cat "$SHARES_FILE" 2>/dev/null || echo "0")
        if [ "$SHARES" = "512" ]; then
            echo "✓ PASS: CPU shares limit correctly set to 512"
        else
            echo "✗ FAIL: CPU shares is $SHARES, expected 512"
        fi
    fi
elif [ -f "$CGROUP_BASE/cgroup.controllers" ]; then
    # cgroup v2
    WEIGHT_FILE="$CGROUP_BASE/mini_container_${CPU_ID}/cpu.weight"
    if [ -f "$WEIGHT_FILE" ]; then
        WEIGHT=$(cat "$WEIGHT_FILE" 2>/dev/null || echo "0")
        echo "✓ CPU weight set: $WEIGHT (expected ~50 for 512 shares)"
    fi
fi

$BINARY stop "$CPU_ID" 2>/dev/null || true
sleep 1

echo ""
echo "Test 2: Memory Limit (64 MB)"
echo "----------------------------------------"
# Run in background with timeout
RUN_OUTPUT=$(timeout 3 $BINARY run \
    -m 64 \
    -c 1024 \
    -n "mem-limit-test" \
    -r "/tmp/mem_limit_test" \
    /bin/sh -c "dd if=/dev/zero of=/tmp/mem bs=1M count=80 status=none 2>&1; echo Exit code: \$?" 2>&1 || true)
MEM_ID=$(echo "$RUN_OUTPUT" | grep -o "container_[0-9_]*" | head -1)

# If not found in output, try to get from list
if [ -z "$MEM_ID" ]; then
    sleep 1
    MEM_ID=$($BINARY list 2>/dev/null | grep -E "(RUNNING|CREATED)" | awk '{print $1}' | head -1)
fi

if [ -z "$MEM_ID" ]; then
    echo "✗ FAILED: Could not create Memory test container"
    exit 1
fi

echo "Container created: $MEM_ID"
sleep 2

MEM_USAGE=$($BINARY info "$MEM_ID" 2>&1 | grep "Memory Usage" | awk '{print $3}' || echo "0")
echo "Memory Usage: $MEM_USAGE bytes"

if [ "$MEM_USAGE" != "0" ] && [ -n "$MEM_USAGE" ]; then
    MEM_MB=$((MEM_USAGE / 1024 / 1024))
    echo "Memory Usage: $MEM_MB MB (limit: 64 MB)"
    if [ "$MEM_MB" -le 70 ]; then
        echo "✓ PASS: Memory usage is within reasonable range"
    else
        echo "⚠ WARNING: Memory usage ($MEM_MB MB) might exceed limit"
    fi
else
    echo "✗ FAIL: Memory usage is 0 (cgroup might not be working)"
fi

# Check cgroup Memory limit
if [ -d "$CGROUP_BASE/memory" ]; then
    # cgroup v1
    LIMIT_FILE="$CGROUP_BASE/memory/mini_container_${MEM_ID}/memory.limit_in_bytes"
    if [ -f "$LIMIT_FILE" ]; then
        LIMIT=$(cat "$LIMIT_FILE" 2>/dev/null || echo "0")
        EXPECTED=$((64 * 1024 * 1024))
        if [ "$LIMIT" = "$EXPECTED" ]; then
            echo "✓ PASS: Memory limit correctly set to 64 MB"
        else
            echo "✗ FAIL: Memory limit is $LIMIT, expected $EXPECTED"
        fi
    fi
elif [ -f "$CGROUP_BASE/cgroup.controllers" ]; then
    # cgroup v2
    MAX_FILE="$CGROUP_BASE/mini_container_${MEM_ID}/memory.max"
    if [ -f "$MAX_FILE" ]; then
        LIMIT=$(cat "$MAX_FILE" 2>/dev/null || echo "0")
        EXPECTED=$((64 * 1024 * 1024))
        if [ "$LIMIT" = "$EXPECTED" ]; then
            echo "✓ PASS: Memory limit correctly set to 64 MB"
        else
            echo "✗ FAIL: Memory limit is $LIMIT, expected $EXPECTED"
        fi
    fi
fi

$BINARY stop "$MEM_ID" 2>/dev/null || true
sleep 1

echo ""
echo "========================================="
echo "Test Complete"
echo "========================================="

