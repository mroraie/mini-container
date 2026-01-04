# سیستم مینی کانتینر - مستندات API

## نمای کلی

این مستندات API پیاده‌سازی سی‌پلاس‌پلاس سیستم مینی کانتینر را پوشش می‌دهد. API سازگاری با C را از طریق اعلانات `extern "C"` حفظ می‌کند در حالی که در داخل از ویژگی‌های مدرن سی‌پلاس‌پلاس برای مدیریت بهتر حافظه و ایمنی استفاده می‌کند.

## API مدیریت کانتینر

### مقداردهی اولیه و پاکسازی

```cpp
int container_manager_init(container_manager_t *cm, int max_containers);
void container_manager_cleanup(container_manager_t *cm);
```

مدیریت‌کننده کانتینر را با حداکثر تعداد کانتینرها مقداردهی کنید.

### چرخه حیات کانتینر

```c
int container_manager_create(container_manager_t *cm, const container_config_t *config);
int container_manager_start(container_manager_t *cm, const char *container_id);
int container_manager_stop(container_manager_t *cm, const char *container_id);
int container_manager_destroy(container_manager_t *cm, const char *container_id);
```

عملیات اساسی چرخه حیات برای کانتینرها.

### عملیات کانتینر

```c
int container_manager_exec(container_manager_t *cm, const char *container_id, char **command, int argc);
container_info_t **container_manager_list(container_manager_t *cm, int *count);
container_info_t *container_manager_get_info(container_manager_t *cm, const char *container_id);
```

دستورات را در کانتینرها اجرا کرده و اطلاعات کانتینر را دریافت کنید.

## API مدیریت فضای نام

### پیکربندی

```c
void namespace_config_init(namespace_config_t *config);
int namespace_set_hostname(const char *hostname);
```

تنظیمات فضای نام را پیکربندی کرده و نام میزبان کانتینر را تنظیم کنید.

### ایجاد فرایند

```c
pid_t namespace_clone_process(int flags, void *child_stack, int stack_size,
                             void (*child_func)(void *), void *arg);
int namespace_setup_isolation(const namespace_config_t *config);
```

فرایندهای ایزوله ایجاد کرده و ایزولاسیون فضای نام را تنظیم کنید.

### عملیات فضای نام

```c
int namespace_join(pid_t target_pid, int ns_type);
pid_t namespace_create_container(const namespace_config_t *config,
                               char **command, int argc);
```

به فضای نام‌های موجود بپیوندید و فرایندهای کانتینر ایجاد کنید.

## API مدیریت منابع

### مقداردهی اولیه

```c
int resource_manager_init(resource_manager_t *rm, const char *base_path);
void resource_limits_init(resource_limits_t *limits);
```

مدیریت منابع را مقداردهی کرده و محدودیت‌های پیش‌فرض را تنظیم کنید.

### عملیات Cgroup

```c
int resource_manager_create_cgroup(resource_manager_t *rm, const char *container_id,
                                  const resource_limits_t *limits);
int resource_manager_add_process(resource_manager_t *rm, const char *container_id, pid_t pid);
int resource_manager_remove_process(resource_manager_t *rm, const char *container_id, pid_t pid);
int resource_manager_destroy_cgroup(resource_manager_t *rm, const char *container_id);
```

گروه‌های کنترل را برای کنترل منابع مدیریت کنید.

### آمار

```c
int resource_manager_get_stats(resource_manager_t *rm, const char *container_id,
                              unsigned long *cpu_usage, unsigned long *memory_usage);
```

آمار استفاده از منابع را دریافت کنید.

## API مدیریت فایل‌سیستم

### پیکربندی

```c
void fs_config_init(fs_config_t *config);
```

پیکربندی فایل‌سیستم را مقداردهی کنید.

### عملیات فایل‌سیستم ریشه

```c
int fs_create_minimal_root(const char *root_path);
int fs_populate_container_root(const char *root_path, const char *host_root);
int fs_cleanup_container_root(const char *root_path);
```

فایل‌سیستم‌های ریشه کانتینر را ایجاد و مدیریت کنید.

### روش‌های ایزولاسیون

```c
int fs_setup_chroot(const char *root_path);
int fs_setup_pivot_root(const char *new_root, const char *put_old);
int fs_mount_container_filesystems(const char *root_path);
```

ایزولاسیون فایل‌سیستم را با استفاده از chroot یا pivot_root تنظیم کنید.

## ساختارهای داده

### پیکربندی کانتینر

```c
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

پیکربندی کانتینر شامل تمام تنظیمات ایزولاسیون.

### محدودیت‌های منابع

```c
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

محدودیت‌های منابع CPU و حافظه.

### اطلاعات کانتینر

```c
typedef struct {
    char *id;
    pid_t pid;
    container_state_t state;
    time_t created_at;
    time_t started_at;
    time_t stopped_at;
} container_info_t;
```

اطلاعات زمان اجرا درباره کانتینرها.

## Constants and Enums

### Namespace Types

```c
enum {
    NS_PID = CLONE_NEWPID,
    NS_MNT = CLONE_NEWNS,
    NS_UTS = CLONE_NEWUTS,
    NS_NET = CLONE_NEWNET,
    NS_USER = CLONE_NEWUSER
};
```

Namespace types for isolation.

### Container States

```c
typedef enum {
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_DESTROYED
} container_state_t;
```

Possible container states.

### Filesystem Methods

```c
typedef enum {
    FS_CHROOT,
    FS_PIVOT_ROOT
} fs_isolation_method_t;
```

Filesystem isolation methods.

## Error Handling

All functions return 0 on success, -1 on failure. Error messages are printed to stderr. Check errno for system-level errors.

## Thread Safety

The container manager is not thread-safe. All operations should be performed from a single thread.

## Memory Management

- Configuration structures must be allocated by the caller
- String fields (id, hostname, root_path) are duplicated internally
- Use container_manager_cleanup() to free all resources
