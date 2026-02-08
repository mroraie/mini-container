# Mini Container System - Technical Report

## 1. Introduction

This technical report describes the implementation of the Mini Container system, a Docker-like environment designed as an Operating Systems course project. The system has been transitioned from C to C++, demonstrating core OS concepts including process isolation, kernel resource management, and system calls, while utilizing modern C++ programming practices.

## 2. System Architecture

The system is organized into modular components, each managing a specific aspect of containerization. The C++ implementation maintains a C-compatible API for system-level operations while introducing modern C++ features for improved memory management and code safety.

### 2.1 Core Components

- **Container Manager**: Coordinates container lifecycle operations.
- **Namespace Handler**: Manages Linux namespaces for process isolation.
- **Resource Manager**: Controls CPU and memory limits using cgroups.
- **Filesystem Manager**: Provides filesystem isolation via pivot_root.
- **CLI Interface**: Command-line interface for user interaction.
- **Web Server**: Web interface for container monitoring.

### 2.2 Component Interaction

```
CLI Interface
    ↓
Container Manager ←→ Resource Manager
    ↓               ←→ Filesystem Manager
Namespace Handler
```

<div style="page-break-after: always;"></div>

## 3. Process Isolation

### 3.1 Namespace Concepts

- **PID Namespace**: Isolates process IDs, giving each container its own process tree starting from PID 1.
- **Mount Namespace**: Isolates filesystem mount points, allowing for private mount tables.
- **Network Namespace**: Isolates network interfaces, routing tables, and firewall rules.
- **User Namespace**: Isolates user and group ID mappings.

### 3.2 Implementation Details

The Namespace Handler uses the `clone()` system call with namespace flags:

```cpp
pid_t pid = clone(child_func, stack, CLONE_NEWPID | CLONE_NEWNS, args);
```

Key implementation aspects:
- **Stack Allocation**: Child processes require their own stack space.
- **Namespace Setup**: The child function configures isolation after namespace creation.
- **PID Namespace**: Provides process ID isolation starting from PID 1.
- **Mount Namespace**: Enables private filesystem views.

### 3.3 Joining Namespaces

To execute commands in existing containers, the system joins the existing namespace:

```cpp
int namespace_join(pid_t target_pid, int ns_type) {
    // Implementation details for setns()
}
```

<div style="page-break-after: always;"></div>

## 4. Control Groups (cgroups) - Resource Management

### 4.1 Cgroup Concepts

Control Groups provide hierarchical resource limits and accounting:

- **CPU Control**: Limits CPU usage via shares and quotas.
- **Memory Control**: Restricts memory usage with hard limits and swap control.
- **Hierarchy**: Tree structure allows for nested resource control.
- **Accounting**: Tracks resource usage for each group.

### 4.2 Implementation Details

The Resource Manager interacts with the `/sys/fs/cgroup` filesystem:

```cpp
// Create cgroup
mkdir("/sys/fs/cgroup/cpu/mini_container/container_id", 0755);

// Setup CPU shares
FILE *fp = fopen("/sys/fs/cgroup/cpu/mini_container/container_id/cpu.shares", "w");
fprintf(fp, "%d\n", shares);

// Add process to cgroup
fp = fopen("/sys/fs/cgroup/cpu/mini_container/container_id/tasks", "w");
fprintf(fp, "%d\n", pid);
```

### 4.3 Resource Limits

- **CPU Shares**: Relative weight for CPU scheduling (default: 1024).
- **CPU Quota**: Absolute CPU time limits (microseconds per period).
- **Memory Limits**: Hard memory limits in bytes.
- **Swap Limits**: Control over swap space usage.

<div style="page-break-after: always;"></div>

## 5. Filesystem Isolation

### 5.1 Pivot Root

The system uses `pivot_root()` for filesystem isolation, which is a modern and secure method:

- **pivot_root()**: Allows unmounting the old root filesystem.
- **Enhanced Security**: More secure than chroot.
- **Better Control**: Enables full management of the root filesystem.

### 5.2 Implementation

```cpp
int fs_setup_pivot_root(const char *new_root, const char *put_old) {
    // Mount new root
    mount(new_root, new_root, "bind", MS_BIND | MS_REC, NULL);
    
    // Create put_old directory
    mkdir(put_old, 0755);
    
    // Pivot root
    syscall(SYS_pivot_root, new_root, put_old);
    
    // Change to new root
    chdir("/");
    
    // Unmount old root
    umount2(put_old, MNT_DETACH);
    rmdir(put_old);
}
```

### 5.3 Root Filesystem Creation

The system creates minimal container root filesystems:

- Essential directories: `/bin`, `/lib`, `/proc`, `/sys`, `/dev`
- Device nodes: `/dev/null`, `/dev/zero`, `/dev/random`
- Base configuration files: `/etc/passwd`, `/etc/group`

<div style="page-break-after: always;"></div>

## 6. System Calls and Kernel Interaction

### 6.1 Core System Calls

- **clone()**: Creates new processes within namespaces.
- **unshare()**: Moves existing processes to new namespaces.
- **pivot_root()**: Advanced and secure root switching.
- **mount()**: Filesystem mount operations.

### 6.2 Direct Kernel Interface

Unlike Docker, this system interacts directly with:
- `/sys/fs/cgroup` for resource management.
- `/proc/<pid>/ns/` for namespace operations.
- System calls without high-level abstractions.

## 7. Container Lifecycle Management

### 7.1 Container States

- **CREATED**: Container defined but not running.
- **RUNNING**: Container process is active.
- **STOPPED**: Container process has terminated.
- **DESTROYED**: Container resources have been cleaned up.

### 7.2 Lifecycle Operations

- **Create**: Setup cgroups, namespaces, and filesystem.
- **Run**: Immediate container creation and startup (Create + Start).
- **Start**: Launches container process with isolation (for stopped containers).
- **Stop**: Gracefully terminates the container process.
- **Destroy**: Cleans up all resources.

### 7.3 Configuration Persistence

The system saves container configurations to allow restarting stopped containers:
- **saved_config**: Full container configuration is stored in `container_info_t`.
- **Restart**: Stopped containers can be relaunched with their original configuration.
