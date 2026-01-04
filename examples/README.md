# Mini Container System - Examples

This directory contains example usage scenarios for the mini containerization system.

## Basic Usage

### Running a simple command
```bash
./mini-container run /bin/echo "Hello World"
```

### Running an interactive shell
```bash
./mini-container run /bin/sh
```

### Running with resource limits
```bash
# Limit memory to 64MB and CPU shares to 512
./mini-container run --memory 64 --cpu 512 /bin/sh
```

### Running with custom hostname
```bash
./mini-container run --hostname "my-container" /bin/hostname
```

## Advanced Usage

### Creating a container with custom root filesystem
```bash
# Create a minimal root filesystem
mkdir /tmp/my_container_root
./mini-container run --root /tmp/my_container_root /bin/sh
```

### Listing containers
```bash
./mini-container list
```

### Getting container information
```bash
./mini-container info container_1234567890_0
```

### Executing commands in running containers
```bash
# First start a container in background (not implemented yet)
# Then execute commands in it
./mini-container exec container_id /bin/ps aux
```

### Stopping containers
```bash
./mini-container stop container_id
```

### Destroying containers
```bash
./mini-container destroy container_id
```

## Example Scenarios

### 1. Web Server Container
```bash
# Create a simple web server container
./mini-container run --memory 128 --cpu 1024 python3 -m http.server 8000
```

### 2. Database Container
```bash
# Run a database with resource limits
./mini-container run --memory 512 --cpu 2048 /usr/bin/mysqld_safe
```

### 3. Development Environment
```bash
# Create a development container with custom hostname
./mini-container run --hostname "dev-env" --memory 1024 /bin/bash
```

## Security Considerations

- The system requires root privileges for most operations
- Network namespace isolation is optional (use --network flag)
- User namespace isolation is optional (use --user flag)
- Filesystem isolation uses chroot by default
- Resource limits are enforced using cgroups

## Troubleshooting

### Common Issues

1. **Permission denied**: Make sure you're running as root
2. **Cgroups not available**: Check if `/sys/fs/cgroup` is mounted
3. **Namespace operations fail**: Kernel may not support required features
4. **Command not found**: Container may not have the required binaries

### Debug Information

Use the `info` command to get detailed container information:
```bash
./mini-container info container_id
```
