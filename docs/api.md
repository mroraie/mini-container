# Mini Container System - API Documentation

## Overview

This document describes the C/C++ API of the Mini Container system. Please read the [README](../README.md) first.

## Container Management (Container Manager)

### Initialization and Cleanup

```cpp
int container_manager_init(container_manager_t *cm, int max_containers);
void container_manager_cleanup(container_manager_t *cm);
```

Initializes the container manager with a maximum number of containers.

### Container Lifecycle

```cpp
int container_manager_create(container_manager_t *cm, const container_config_t *config);
int container_manager_run(container_manager_t *cm, container_config_t *config);
int container_manager_start(container_manager_t *cm, const char *container_id);
int container_manager_stop(container_manager_t *cm, const char *container_id);
int container_manager_destroy(container_manager_t *cm, const char *container_id);
```

Core lifecycle operations:
- `container_manager_create`: Creates a container but does not start it (state: CREATED).
- `container_manager_run`: Creates and immediately starts a container (state: RUNNING).
- `container_manager_start`: Starts a previously stopped container.
- `container_manager_stop`: Stops a running container (state: STOPPED).
- `container_manager_destroy`: Removes a container and cleans up resources (state: DESTROYED).

### Container Operations

```cpp
int container_manager_exec(container_manager_t *cm, const char *container_id, char **command, int argc);
container_info_t **container_manager_list(container_manager_t *cm, int *count);
container_info_t *container_manager_get_info(container_manager_t *cm, const char *container_id);
```

Execute commands inside containers and query container information.




## Namespace Management

### Configuration

```cpp
void namespace_config_init(namespace_config_t *config);
```

Initializes the namespace configuration.

### Process Creation

```cpp
pid_t namespace_clone_process(int flags, void *child_stack, int stack_size,
                             void (*child_func)(void *), void *arg);
int namespace_setup_isolation(const namespace_config_t *config);
```

Creates isolated processes and sets up namespace isolation.

### Namespace Operations

```cpp
int namespace_join(pid_t target_pid, int ns_type);
pid_t namespace_create_container(const namespace_config_t *config,
                               char **command, int argc);
```

Join existing namespaces and create container processes.

## Resource Management (Resource Manager)

### Initialization

```cpp
int resource_manager_init(resource_manager_t *rm, const char *base_path);
void resource_limits_init(resource_limits_t *limits);
```

Initializes the resource manager and sets default limits.

### Cgroup Operations

```cpp
int resource_manager_create_cgroup(resource_manager_t *rm, const char *container_id,
                                  const resource_limits_t *limits);
int resource_manager_add_process(resource_manager_t *rm, const char *container_id, pid_t pid);
int resource_manager_remove_process(resource_manager_t *rm, const char *container_id, pid_t pid);
int resource_manager_destroy_cgroup(resource_manager_t *rm, const char *container_id);
```

Manage control groups (cgroups) for resource isolation.

### Statistics

```cpp
int resource_manager_get_stats(resource_manager_t *rm, const char *container_id,
                              unsigned long *cpu_usage, unsigned long *memory_usage);
```

Retrieves CPU and memory usage statistics.

## Filesystem Management (Filesystem Manager)

### Configuration

```cpp
void fs_config_init(fs_config_t *config);
```

Initializes filesystem configuration.

### Root Filesystem Operations

```cpp
int fs_create_minimal_root(const char *root_path);
int fs_populate_container_root(const char *root_path, const char *host_root);
int fs_cleanup_container_root(const char *root_path);
```

Create and manage container root filesystems.

### Isolation Methods

```cpp
int fs_setup_pivot_root(const char *new_root, const char *put_old);
int fs_mount_container_filesystems(const char *root_path);
```

Sets up filesystem isolation using pivot_root. `pivot_root` is the recommended and more secure approach because it allows unmounting the old root.

## Data Structures

### Container Configuration

```cpp
typedef struct {
    char *id;
    char *root_path;
    namespace_config_t ns_config;
    resource_limits_t res_limits;
    fs_config_t fs_config;
    char **command;
    int command_argc;
} container_config_t;
```

Container configuration including all isolation settings.

### Resource Limits

```cpp
typedef struct {
    struct {
        int shares;
        int quota_us;
        int period_us;
    } cpu;
    struct {
        unsigned long limit_bytes;
        unsigned long swap_limit_bytes;
    } memory;
    int enabled;
} resource_limits_t;
```

CPU and memory resource limits.

### Container Information

```cpp
typedef struct {
    char *id;
    pid_t pid;
    container_state_t state;
    time_t created_at;
    time_t started_at;
    time_t stopped_at;
    container_config_t *saved_config;  // saved config for restart
} container_info_t;
```

Runtime information about containers.

Fields:
- `id`: Unique container identifier.
- `pid`: Container process ID (0 if not running).
- `state`: Current state (CREATED, RUNNING, STOPPED, DESTROYED).
- `created_at`: Creation time.
- `started_at`: Start time.
- `stopped_at`: Stop time.
- `saved_config`: Stored configuration for restarting.

## Enumerations

### Namespace Types

```cpp
enum {
    NS_PID = CLONE_NEWPID,
    NS_MNT = CLONE_NEWNS,
    NS_NET = CLONE_NEWNET,
    NS_USER = CLONE_NEWUSER
};
```

### Container States

```cpp
typedef enum {
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_DESTROYED
} container_state_t;
```

### Filesystem Isolation Methods

```cpp
typedef enum {
    FS_PIVOT_ROOT
} fs_isolation_method_t;
```

Note: `FS_PIVOT_ROOT` is the recommended isolation method for better security and flexibility.

## Web Server API

The system includes a simple web server for monitoring containers.

### `SimpleWebServer` Class

```cpp
class SimpleWebServer {
public:
    SimpleWebServer(container_manager_t* cm, int port = 808);
    ~SimpleWebServer();
    void start();
    void stop();
};
```

### HTTP Endpoints

#### GET `/` or `/index.html`
Returns the monitoring dashboard HTML.

#### GET `/api/containers`
Returns a JSON list of containers.

Response:
```json
{
  "containers": [
    {
      "id": "container_id",
      "pid": 12345,
      "state": "RUNNING",
      "cpu_usage": 1000000000,
      "cpu_limit": 25000,
      "cpu_percent": 25.5,
      "memory_usage": 67108864,
      "memory_limit": 134217728,
      "memory_percent": 50.0
    }
  ]
}
```

#### GET `/api/system`
Returns system resource usage.

Response:
```json
{
  "used_memory": 2147483648,
  "total_memory": 8589934592,
  "cpu_percent": 45.3
}
```

## Signals and Graceful Shutdown

The system uses a signal handler for graceful shutdown.

### Signal Handler

```cpp
void signal_handler(int signum);
```

Supported signals:
- `SIGINT` (Ctrl+C)
- `SIGTERM`

Behavior:
1. Sends SIGTERM to all RUNNING containers.
2. Waits for processes to terminate gracefully.
3. Transitions container states to STOPPED.
4. Sends SIGKILL if any processes remain.
5. Stops the web server.
6. Releases resources and exits.

## Test Helpers

### Memory and CPU Test

```cpp
void run_memory_cpu_test();
```

Creates test containers for memory and CPU stress tests.

### System Helper Functions

```cpp
static unsigned long get_total_system_memory();
static int get_cpu_count();
```

- `get_total_system_memory()`: Returns total system memory in bytes.
- `get_cpu_count()`: Returns CPU core count.

## Examples

### Create and Run a Container

```cpp
container_config_t config;
namespace_config_init(&config.ns_config);
resource_limits_init(&config.res_limits);
fs_config_init(&config.fs_config);

config.id = strdup("my_container");
config.res_limits.memory.limit_bytes = 128 * 1024 * 1024; // 128 MB
config.res_limits.cpu.shares = 1024;
config.fs_config.root_path = strdup("/");

char *args[] = {"/bin/sh", "-c", "echo Hello World", nullptr};
config.command = args;
config.command_argc = 3;

if (container_manager_run(&cm, &config) == 0) {
    printf("Container created and started\n");
}
```

### Get Container Info

```cpp
container_info_t *info = container_manager_get_info(&cm, "my_container");
if (info) {
    printf("Container ID: %s\n", info->id);
    printf("State: %d\n", info->state);
    printf("PID: %d\n", info->pid);

    if (info->state == CONTAINER_RUNNING) {
        unsigned long cpu_usage = 0, memory_usage = 0;
        resource_manager_get_stats(cm.rm, info->id, &cpu_usage, &memory_usage);
        printf("CPU Usage: %lu ns\n", cpu_usage);
        printf("Memory Usage: %lu bytes\n", memory_usage);
    }
}
```

### Stop and Restart a Container

```cpp
if (container_manager_stop(&cm, "my_container") == 0) {
    printf("Container stopped\n");
}

if (container_manager_start(&cm, "my_container") == 0) {
    printf("Container restarted\n");
}
```
