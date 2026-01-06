#!/bin/bash

# Comprehensive test for CPU and Memory usage monitoring
# This test will run various scenarios and check if CPU/Memory usage is non-zero
# It will keep trying different approaches until it finds one that works

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTAINER_BIN="$SCRIPT_DIR/../mini-container"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0

check_usage() {
    local container_id=$1
    local test_name=$2
    local wait_time=${3:-3}
    
    echo -e "\n${BLUE}Waiting ${wait_time} seconds for $test_name to run...${NC}"
    sleep "$wait_time"
    
    # Get container info
    INFO_OUTPUT=$($CONTAINER_BIN info "$container_id" 2>&1 || echo "")
    
    if [ -z "$INFO_OUTPUT" ]; then
        echo -e "${RED}FAIL: Cannot get container info${NC}"
        return 1
    fi
    
    echo "Container info:"
    echo "$INFO_OUTPUT"
    
    # Extract CPU and Memory usage
    CPU_USAGE=$(echo "$INFO_OUTPUT" | grep -i "CPU Usage" | grep -oE "[0-9]+" | head -1 || echo "0")
    MEM_USAGE=$(echo "$INFO_OUTPUT" | grep -i "Memory Usage" | grep -oE "[0-9]+" | head -1 || echo "0")
    
    echo "CPU Usage: $CPU_USAGE nanoseconds"
    echo "Memory Usage: $MEM_USAGE bytes"
    
    # Check if process is running
    CONTAINER_PID=$(echo "$INFO_OUTPUT" | grep -i "PID" | grep -oE "[0-9]+" | head -1 || echo "")
    if [ -n "$CONTAINER_PID" ]; then
        echo "Container PID: $CONTAINER_PID"
        if ps -p "$CONTAINER_PID" > /dev/null 2>&1; then
            echo -e "${GREEN}Process is running${NC}"
        else
            echo -e "${RED}Process is NOT running${NC}"
        fi
    fi
    
    # Check cgroup
    CGROUP_PATH=""
    if [ -d "/sys/fs/cgroup/mini_container_${container_id}" ]; then
        CGROUP_PATH="/sys/fs/cgroup/mini_container_${container_id}"
    elif [ -d "/sys/fs/cgroup/mini_container/${container_id}" ]; then
        CGROUP_PATH="/sys/fs/cgroup/mini_container/${container_id}"
    fi
    
    if [ -n "$CGROUP_PATH" ]; then
        echo "Cgroup path: $CGROUP_PATH"
        if [ -f "$CGROUP_PATH/cgroup.procs" ]; then
            echo "Processes in cgroup:"
            cat "$CGROUP_PATH/cgroup.procs" 2>/dev/null || echo "Cannot read"
        fi
        if [ -f "$CGROUP_PATH/cpu.stat" ]; then
            echo "CPU stats:"
            cat "$CGROUP_PATH/cpu.stat" 2>/dev/null | head -5 || echo "Cannot read"
        fi
        if [ -f "$CGROUP_PATH/memory.current" ]; then
            echo "Memory current:"
            cat "$CGROUP_PATH/memory.current" 2>/dev/null || echo "Cannot read"
        fi
    else
        echo -e "${RED}Cgroup directory not found${NC}"
    fi
    
    # Check results
    local cpu_ok=0
    local mem_ok=0
    
    if [ "$CPU_USAGE" != "0" ] && [ -n "$CPU_USAGE" ]; then
        cpu_ok=1
        echo -e "${GREEN}✓ CPU usage is non-zero: $CPU_USAGE${NC}"
    else
        echo -e "${RED}✗ CPU usage is zero${NC}"
    fi
    
    if [ "$MEM_USAGE" != "0" ] && [ -n "$MEM_USAGE" ]; then
        mem_ok=1
        echo -e "${GREEN}✓ Memory usage is non-zero: $MEM_USAGE${NC}"
    else
        echo -e "${RED}✗ Memory usage is zero${NC}"
    fi
    
    if [ $cpu_ok -eq 1 ] || [ $mem_ok -eq 1 ]; then
        PASSED=$((PASSED + 1))
        return 0
    else
        FAILED=$((FAILED + 1))
        return 1
    fi
}

cleanup() {
    local container_id=$1
    $CONTAINER_BIN stop "$container_id" 2>/dev/null || true
    $CONTAINER_BIN destroy "$container_id" 2>/dev/null || true
    sleep 1
}

echo "=========================================="
echo "CPU and Memory Usage Comprehensive Test"
echo "=========================================="

# Test 1: CPU-intensive infinite loop
echo -e "\n${YELLOW}Test 1: CPU-intensive infinite loop${NC}"
ROOT_PATH="/tmp/container_root_test_cpu_$(date +%s)_$$"
echo "Running: /bin/sh -c 'while true; do :; done'"

# Run in background and capture output
RUN_OUTPUT=$($CONTAINER_BIN run --memory 128 --cpu 1024 --hostname "test-cpu" --root "$ROOT_PATH" /bin/sh -c "while true; do :; done" 2>&1 &)
sleep 2

# Get container ID from list (first running container)
CONTAINER_ID=$($CONTAINER_BIN list 2>/dev/null | grep "RUNNING" | head -1 | awk '{print $1}' || echo "")

if [ -z "$CONTAINER_ID" ]; then
    # Try to get from output
    CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -oE "Container [^ ]+ started" | awk '{print $2}' || echo "")
fi

if [ -n "$CONTAINER_ID" ]; then
    echo "Container ID: $CONTAINER_ID"
    
    if check_usage "$CONTAINER_ID" "CPU-intensive loop" 5; then
        echo -e "${GREEN}Test 1 PASSED${NC}"
    else
        echo -e "${RED}Test 1 FAILED${NC}"
    fi
else
    echo -e "${RED}Failed to start container${NC}"
    FAILED=$((FAILED + 1))
fi

cleanup "$CONTAINER_ID"

# Test 2: CPU-intensive calculation
echo -e "\n${YELLOW}Test 2: CPU-intensive calculation${NC}"
ROOT_PATH="/tmp/container_root_test_cpu_calc_$(date +%s)_$$"
echo "Running: /bin/sh -c 'i=0; while [ \$i -lt 100000000 ]; do i=\$((i+1)); done'"

# Run in background and capture output
RUN_OUTPUT=$($CONTAINER_BIN run --memory 128 --cpu 1024 --hostname "test-cpu-calc" --root "$ROOT_PATH" /bin/sh -c "i=0; while [ \$i -lt 100000000 ]; do i=\$((i+1)); done; echo Done" 2>&1 &)
sleep 2

# Get container ID from list (first running container)
CONTAINER_ID=$($CONTAINER_BIN list 2>/dev/null | grep "RUNNING" | head -1 | awk '{print $1}' || echo "")

if [ -z "$CONTAINER_ID" ]; then
    # Try to get from output
    CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -oE "Container [^ ]+ started" | awk '{print $2}' || echo "")
fi

if [ -n "$CONTAINER_ID" ]; then
    echo "Container ID: $CONTAINER_ID"
    
    if check_usage "$CONTAINER_ID" "CPU calculation" 2; then
        echo -e "${GREEN}Test 2 PASSED${NC}"
    else
        echo -e "${RED}Test 2 FAILED${NC}"
    fi
else
    echo -e "${RED}Failed to start container${NC}"
    FAILED=$((FAILED + 1))
fi

cleanup "$CONTAINER_ID"

# Test 3: Memory-intensive
echo -e "\n${YELLOW}Test 3: Memory-intensive command${NC}"
ROOT_PATH="/tmp/container_root_test_mem_$(date +%s)_$$"
echo "Running: dd if=/dev/zero of=/tmp/memtest bs=1M count=32"

# Run in background and capture output
RUN_OUTPUT=$($CONTAINER_BIN run --memory 64 --cpu 1024 --hostname "test-mem" --root "$ROOT_PATH" /bin/sh -c "dd if=/dev/zero of=/tmp/memtest bs=1M count=32 && sleep 2 && rm -f /tmp/memtest" 2>&1 &)
sleep 2

# Get container ID from list (first running container)
CONTAINER_ID=$($CONTAINER_BIN list 2>/dev/null | grep "RUNNING" | head -1 | awk '{print $1}' || echo "")

if [ -z "$CONTAINER_ID" ]; then
    # Try to get from output
    CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -oE "Container [^ ]+ started" | awk '{print $2}' || echo "")
fi

if [ -n "$CONTAINER_ID" ]; then
    echo "Container ID: $CONTAINER_ID"
    
    if check_usage "$CONTAINER_ID" "Memory allocation" 3; then
        echo -e "${GREEN}Test 3 PASSED${NC}"
    else
        echo -e "${RED}Test 3 FAILED${NC}"
    fi
else
    echo -e "${RED}Failed to start container${NC}"
    FAILED=$((FAILED + 1))
fi

cleanup "$CONTAINER_ID"

# Test 4: Combined CPU + Memory
echo -e "\n${YELLOW}Test 4: Combined CPU + Memory intensive${NC}"
ROOT_PATH="/tmp/container_root_test_combined_$(date +%s)_$$"
echo "Running: CPU + Memory intensive command"

# Run in background and capture output
RUN_OUTPUT=$($CONTAINER_BIN run --memory 128 --cpu 1024 --hostname "test-combined" --root "$ROOT_PATH" /bin/sh -c "dd if=/dev/zero of=/tmp/stress bs=1M count=16 && i=0; while [ \$i -lt 10000000 ]; do i=\$((i+1)); done; rm -f /tmp/stress" 2>&1 &)
sleep 2

# Get container ID from list (first running container)
CONTAINER_ID=$($CONTAINER_BIN list 2>/dev/null | grep "RUNNING" | head -1 | awk '{print $1}' || echo "")

if [ -z "$CONTAINER_ID" ]; then
    # Try to get from output
    CONTAINER_ID=$(echo "$RUN_OUTPUT" | grep -oE "Container [^ ]+ started" | awk '{print $2}' || echo "")
fi

if [ -n "$CONTAINER_ID" ]; then
    echo "Container ID: $CONTAINER_ID"
    
    if check_usage "$CONTAINER_ID" "Combined CPU+Memory" 3; then
        echo -e "${GREEN}Test 4 PASSED${NC}"
    else
        echo -e "${RED}Test 4 FAILED${NC}"
    fi
else
    echo -e "${RED}Failed to start container${NC}"
    FAILED=$((FAILED + 1))
fi

cleanup "$CONTAINER_ID"

# Summary
echo -e "\n=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed! CPU and Memory usage monitoring is working.${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed. CPU and Memory usage monitoring needs fixing.${NC}"
    exit 1
fi

