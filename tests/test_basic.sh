#!/bin/bash

# Basic functionality test for mini-container system
# This script tests the core container operations

set -e

echo "=== Mini Container System - Basic Tests ==="

# Build the system
echo "Building mini-container..."
cd ..
make clean
make

echo "Testing basic functionality..."

# Test 1: List containers (should be empty)
echo "Test 1: List containers (should be empty)"
./mini-container list

# Test 2: Run a simple command
echo "Test 2: Run a simple command"
./mini-container run /bin/echo "Hello from container!"

# Test 3: Run a command with resource limits
echo "Test 3: Run command with resource limits"
./mini-container run --memory 64 --cpu 512 /bin/echo "Limited container"

# Test 4: List containers again
echo "Test 4: List containers"
./mini-container list

echo "=== All basic tests completed ==="
