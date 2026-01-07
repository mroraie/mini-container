# Mini Container

A lightweight container implementation in C++ that provides Docker-like container management capabilities using Linux namespaces, cgroups, and filesystem isolation.

## Features

- **Container Lifecycle Management**: Create, start, stop, destroy containers
- **Resource Limits**: CPU shares and memory limits using cgroups
- **Namespace Isolation**: PID, mount, UTS, and network namespaces
- **Filesystem Management**: Root filesystem configuration and isolation
- **Interactive CLI**: User-friendly terminal interface with live monitoring
- **Web Dashboard**: Built-in web server for container management via browser
- **Real-time Monitoring**: htop-like monitor showing CPU, memory usage, and runtime statistics
- **Container Execution**: Execute commands in running containers

## Requirements

- Linux operating system (uses Linux-specific features)
- C++11 compatible compiler (g++ recommended)
- Root privileges (for namespace and cgroup operations)
- pthread library

## Building

```bash
# Build both main application and web server
make

# Build only main application
make mini-container

# Build only web server
make web

# Clean build artifacts
make clean

# Install to /usr/local/bin (requires sudo)
make install
```

## Usage

### Interactive Mode

Run without arguments to enter interactive mode:

```bash
sudo ./mini-container
```

This will display a live monitor and menu with options to:
1. Create Container
2. Full Monitor (htop-like view)
3. List Containers
4. Stop Container
5. Destroy Container
6. Container Info
7. Run Tests

### Command Line Interface

```bash
# Run a command in a new container
sudo ./mini-container run /bin/sh -c "echo Hello World"

# Run with resource limits
sudo ./mini-container run --memory 256 --cpu 512 /bin/echo "Hello"

# Run container in background
sudo ./mini-container run --detach /bin/sh -c "while true; do sleep 1; done"

# List all containers
sudo ./mini-container list

# Stop a container
sudo ./mini-container stop container_123

# Execute command in running container
sudo ./mini-container exec container_123 /bin/ps

# Get container information
sudo ./mini-container info container_123

# Destroy a container
sudo ./mini-container destroy container_123

# View htop-like monitor
sudo ./mini-container monitor

# Show help
sudo ./mini-container help
```

### Web Dashboard

The web server starts automatically on port 808. Open your browser and navigate to:

```
http://localhost:808
```

You can also run the standalone web server:

```bash
sudo ./mini-container-web
```

## Architecture

The project consists of several key components:

- **Container Manager** (`container_manager.cpp`): Main orchestration layer managing container lifecycle
- **Namespace Handler** (`namespace_handler.cpp`): Linux namespace isolation (PID, mount, UTS, network)
- **Resource Manager** (`resource_manager.cpp`): CPU and memory limits via cgroups
- **Filesystem Manager** (`filesystem_manager.cpp`): Root filesystem setup and management
- **Web Server** (`web_server_simple.cpp`): HTTP server for web-based container management

## Project Structure

```
mini-container/
├── include/
│   ├── container_manager.hpp
│   ├── namespace_handler.hpp
│   ├── resource_manager.hpp
│   └── filesystem_manager.hpp
├── src/
│   ├── main.cpp              # Main CLI application
│   ├── container_manager.cpp
│   ├── namespace_handler.cpp
│   ├── resource_manager.cpp
│   ├── filesystem_manager.cpp
│   ├── web_server_main.cpp   # Standalone web server
│   ├── web_server_simple.cpp
│   └── web_server_simple.hpp
├── examples/
├── Makefile
└── README.md
```

## Container States

Containers can be in one of the following states:

- **CREATED**: Container created but not started
- **RUNNING**: Container is currently running
- **STOPPED**: Container was stopped
- **DESTROYED**: Container has been destroyed

## Resource Limits

- **Memory**: Specified in MB using `--memory` flag (default: 128 MB)
- **CPU**: Specified as CPU shares using `--cpu` flag (default: 1024)

## Examples

### Basic Container

```bash
sudo ./mini-container run /bin/sh
```

### CPU-Intensive Container

```bash
sudo ./mini-container run --cpu 512 /bin/sh -c "while true; do :; done"
```

### Memory-Limited Container

```bash
sudo ./mini-container run --memory 64 /bin/sh -c "dd if=/dev/zero of=/tmp/test bs=1M count=100"
```

### Custom Hostname

```bash
sudo ./mini-container run --hostname my-container /bin/hostname
```

## Development

### Building for Development

```bash
# Enable debug symbols
make CXXFLAGS="-Wall -Wextra -std=c++11 -g -Iinclude"
```

### Testing

The interactive menu includes built-in tests:
- CPU Usage Test
- Memory Limit Test
- CPU Limit Test
- Combined Test (CPU + Memory)

## Limitations

- Requires root privileges for namespace and cgroup operations
- Linux-specific (uses Linux namespaces and cgroups)
- Basic implementation compared to production container runtimes
- Network namespace support is limited

## License

This project is provided as-is for educational and development purposes.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Disclaimer

This is an educational project demonstrating container concepts. For production use, consider using established container runtimes like Docker, Podman, or containerd.

