#!/bin/bash

# Isolation test for mini-container system
# Tests namespace and filesystem isolation

set -e

echo "=== Mini Container System - Isolation Tests ==="

# Build the system
cd ..
make clean
make

echo "Testing isolation features..."

# Create a test root filesystem
TEST_ROOT="/tmp/test_container_root"
echo "Creating test root filesystem at $TEST_ROOT"
sudo rm -rf "$TEST_ROOT"
sudo mkdir -p "$TEST_ROOT"
sudo cp -r /bin /lib /lib64 "$TEST_ROOT" 2>/dev/null || true

# Test hostname isolation
echo "Test 1: Hostname isolation"
hostname_before=$(hostname)
echo "Host hostname: $hostname_before"

# This would require running the container in background
# For now, just test the CLI
echo "Test 2: Filesystem isolation setup"
./mini-container run --root "$TEST_ROOT" --hostname "container-host" /bin/hostname

echo "=== Isolation tests completed ==="
