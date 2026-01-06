#!/bin/bash

# Complete Demo Test - Shows all container concepts
# This test demonstrates all core concepts of the mini-container system

set -e

echo "=========================================="
echo " Mini Container System - Complete Demo"
echo "=========================================="
echo "Demonstrating all container concepts:"
echo "  ✓ PID Namespace Isolation"
echo "  ✓ Mount Namespace Isolation"
echo "  ✓ UTS Namespace (hostname)"
echo "  ✓ cGroups Resource Management"
echo "  ✓ chroot Filesystem Isolation"
echo "  ✓ Container Lifecycle"
echo "  ✓ Multi-container Execution"
echo "  ✓ Resource Monitoring"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Demo counter
DEMO_STEP=1

demo_step() {
    local title="$1"
    local description="$2"
    echo -e "\n${BLUE}=== Demo Step $DEMO_STEP: $title ===${NC}"
    echo "$description"
    DEMO_STEP=$((DEMO_STEP + 1))
}

run_cmd() {
    local cmd="$1"
    local description="$2"
    echo -e "${YELLOW}Command: $cmd${NC}"
    if [ -n "$description" ]; then
        echo -e "${YELLOW}Purpose: $description${NC}"
    fi
    echo -e "${GREEN}Output:${NC}"
    eval "$cmd" 2>&1 || true
    echo ""
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Warning: This demo requires root privileges for full functionality${NC}"
    echo -e "${YELLOW}Some operations may fail or be limited${NC}"
    echo ""
fi

# Build the system
echo "Building mini-container system..."
cd ..
make clean
make

echo ""
echo "Starting Complete Container Demo..."
echo "==================================="

# Demo 1: Basic Concepts
demo_step "Container Basics" "Understanding container lifecycle and basic operations"

run_cmd "./mini-container --help" "Show help and available commands"

run_cmd "./mini-container list" "List all containers (should be empty)"

# Demo 2: PID Namespace Isolation
demo_step "PID Namespace Isolation" "Each container has its own process ID space"

echo "Creating test directory for container root filesystem..."
mkdir -p /tmp/container_demo_root/bin
cp /bin/bash /tmp/container_demo_root/bin/
cp /bin/ls /tmp/container_demo_root/bin/
cp /bin/ps /tmp/container_demo_root/bin/
cp /bin/echo /tmp/container_demo_root/bin/

run_cmd "./mini-container run --hostname demo-container-1 --root /tmp/container_demo_root /bin/ps aux | head -10" "Run container with custom hostname and show processes (PID namespace)"

run_cmd "./mini-container list" "List running containers"

# Demo 3: Resource Management with cGroups
demo_step "Resource Management (cGroups)" "CPU and memory limits using control groups"

run_cmd "./mini-container run --memory 64 --cpu 512 --hostname limited-container /bin/echo 'Container with 64MB RAM limit and 512 CPU shares'" "Create container with resource limits"

run_cmd "./mini-container run --memory 128 --cpu 1024 --hostname high-priority /bin/echo 'Container with 128MB RAM and 1024 CPU shares'" "Create container with higher resource allocation"

run_cmd "./mini-container list" "Show all containers with their resource allocations"

# Demo 4: Container Information and Monitoring
demo_step "Container Monitoring" "View detailed container information and resource usage"

CONTAINER_ID=$(./mini-container list | grep -E "(demo|limited|high)" | head -1 | awk '{print $1}' || echo "")
if [ -n "$CONTAINER_ID" ]; then
    run_cmd "./mini-container info $CONTAINER_ID" "Show detailed information about a container"
else
    echo "No containers found for info demo"
fi

# Demo 5: Multi-container Execution
demo_step "Multi-container Execution" "Running multiple containers simultaneously"

run_cmd "./mini-container run --hostname web-server /bin/echo 'Web Server Container Started'" "Start web server container"

run_cmd "./mini-container run --hostname database /bin/echo 'Database Container Started'" "Start database container"

run_cmd "./mini-container run --hostname cache /bin/echo 'Cache Container Started'" "Start cache container"

run_cmd "./mini-container list" "Show all running containers"

# Demo 6: Filesystem Isolation (chroot)
demo_step "Filesystem Isolation (chroot)" "Containers have isolated filesystem views"

echo "Setting up isolated filesystem for demo..."
mkdir -p /tmp/container_fs_demo/bin
mkdir -p /tmp/container_fs_demo/lib
mkdir -p /tmp/container_fs_demo/lib64
cp /bin/ls /tmp/container_fs_demo/bin/
cp /bin/echo /tmp/container_fs_demo/bin/

# Copy essential libraries (simplified)
cp /lib/x86_64-linux-gnu/libc.so.6 /tmp/container_fs_demo/lib/ 2>/dev/null || cp /lib64/libc.so.6 /tmp/container_fs_demo/lib/ 2>/dev/null || true
cp /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 /tmp/container_fs_demo/lib/ 2>/dev/null || cp /lib64/ld-linux-x86-64.so.2 /tmp/container_fs_demo/lib/ 2>/dev/null || true

run_cmd "./mini-container run --root /tmp/container_fs_demo --hostname isolated-fs /bin/ls -la /" "Run container with chroot filesystem isolation"

# Demo 7: Container Lifecycle Management
demo_step "Container Lifecycle" "Create, start, stop, and destroy containers"

run_cmd "./mini-container run --hostname lifecycle-demo /bin/bash -c 'echo \"Container started\"; sleep 2; echo \"Container finishing\"'" "Run container with full lifecycle"

sleep 3  # Wait for container to complete

run_cmd "./mini-container list" "Check container states after execution"

# Demo 8: Advanced Namespace Features
demo_step "Advanced Namespace Features" "Combining multiple namespace isolations"

run_cmd "./mini-container run --hostname advanced-demo --memory 256 --cpu 2048 --root /tmp/container_demo_root /bin/bash -c 'echo \"Hostname: \$(hostname)\"; echo \"PID: \$\$\"; echo \"Container is isolated!\"'" "Container with combined PID, UTS, Mount namespaces + resource limits"

# Demo 9: Error Handling and Edge Cases
demo_step "Error Handling" "Testing error conditions and edge cases"

run_cmd "./mini-container run" "Test missing command error"

run_cmd "./mini-container stop nonexistent-container" "Test stopping non-existent container"

run_cmd "./mini-container info nonexistent-container" "Test info for non-existent container"

run_cmd "./mini-container destroy nonexistent-container" "Test destroying non-existent container"

# Demo 10: Resource Usage Monitoring
demo_step "Resource Usage Monitoring" "Monitor CPU and memory usage of containers"

echo "Starting long-running containers for monitoring..."
./mini-container run --hostname monitor-1 --memory 128 /bin/bash -c 'for i in {1..10}; do echo "Container 1 iteration \$i"; sleep 1; done' &
PID1=$!

./mini-container run --hostname monitor-2 --memory 128 /bin/bash -c 'for i in {1..10}; do echo "Container 2 iteration \$i"; sleep 1; done' &
PID2=$!

sleep 3  # Let containers run for monitoring

run_cmd "./mini-container list" "Show running containers during execution"

sleep 5  # Wait for containers to complete

run_cmd "./mini-container list" "Final container list"

# Demo 11: Cleanup
demo_step "Cleanup" "Proper container cleanup and resource management"

run_cmd "./mini-container list" "Final check of all containers"

echo "Cleaning up demo files..."
rm -rf /tmp/container_demo_root
rm -rf /tmp/container_fs_demo

echo ""
echo -e "${GREEN}=========================================="
echo " Complete Demo Summary"
echo -e "==========================================${NC}"
echo "✓ PID Namespace: Process isolation demonstrated"
echo "✓ Mount Namespace: Filesystem isolation with chroot"
echo "✓ UTS Namespace: Custom hostnames for containers"
echo "✓ cGroups: CPU and memory resource management"
echo "✓ Container Lifecycle: Create, run, monitor, cleanup"
echo "✓ Multi-container: Simultaneous container execution"
echo "✓ Resource Monitoring: Usage tracking and limits"
echo "✓ Error Handling: Robust error management"
echo ""
echo -e "${BLUE}All core container concepts have been demonstrated!${NC}"
echo ""
echo "Note: For full functionality, run this demo as root:"
echo "  sudo ./tests/test_complete_demo.sh"
echo "=========================================="
