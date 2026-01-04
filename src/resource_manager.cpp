#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <memory>
#include "../include/resource_manager.hpp"

#define CGROUP_ROOT "/sys/fs/cgroup"

#define CPU_CGROUP_PATH CGROUP_ROOT "/cpu"
#define MEMORY_CGROUP_PATH CGROUP_ROOT "/memory"

#define BUF_SIZE 256

int resource_manager_init(resource_manager_t *rm, const char *base_path) {
    if (!rm) {
        fprintf(stderr, "Error: resource manager is NULL\n");
        return -1;
    }

    if (access(CGROUP_ROOT, F_OK) != 0) {
        fprintf(stderr, "Error: cgroups not available at %s\n", CGROUP_ROOT);
        return -1;
    }

    if (access(CPU_CGROUP_PATH, F_OK) != 0) {
        fprintf(stderr, "Error: CPU cgroup subsystem not available\n");
        return -1;
    }

    if (access(MEMORY_CGROUP_PATH, F_OK) != 0) {
        fprintf(stderr, "Error: memory cgroup subsystem not available\n");
        return -1;
    }

    rm->cgroup_path = strdup(base_path ? base_path : "mini_container");
    if (!rm->cgroup_path) {
        perror("strdup failed");
        return -1;
    }

    rm->initialized = 1;
    return 0;
}

int resource_manager_create_cgroup(resource_manager_t *rm,
                                  const char *container_id,
                                  const resource_limits_t *limits) {
    if (!rm || !rm->initialized || !container_id) {
        return -1;
    }

    char path[BUF_SIZE];

    snprintf(path, sizeof(path), "%s/%s_%s", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir CPU cgroup failed");
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%s_%s", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir memory cgroup failed");
        return -1;
    }

    if (limits->enabled && limits->cpu.shares > 0) {
        if (set_cpu_limits(rm, container_id, &limits->cpu) != 0) {
            return -1;
        }
    }

    if (limits->enabled && limits->memory.limit_bytes > 0) {
        if (set_memory_limits(rm, container_id, &limits->memory) != 0) {
            return -1;
        }
    }

    return 0;
}

int resource_manager_add_process(resource_manager_t *rm,
                                const char *container_id,
                                pid_t pid) {
    if (!rm || !rm->initialized || !container_id || pid <= 0) {
        return -1;
    }

    char path[BUF_SIZE];
    char pid_str[32];

    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    snprintf(path, sizeof(path), "%s/%s_%s/tasks", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
    if (write_file(path, pid_str) != 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%s_%s/tasks", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
    if (write_file(path, pid_str) != 0) {
        return -1;
    }

    return 0;
}

int resource_manager_remove_process(resource_manager_t *rm,
                                   const char *container_id,
                                   pid_t pid) {
    (void)rm;
    (void)container_id;
    (void)pid;
    return 0;
}

int resource_manager_destroy_cgroup(resource_manager_t *rm,
                                   const char *container_id) {
    if (!rm || !rm->initialized || !container_id) {
        return -1;
    }

    char path[BUF_SIZE];

    snprintf(path, sizeof(path), "%s/%s_%s", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
    rmdir(path);  // Ignore errors - directory might not be empty yet

    snprintf(path, sizeof(path), "%s/%s_%s", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
    rmdir(path);  // Ignore errors

    return 0;
}

int resource_manager_get_stats(resource_manager_t *rm,
                              const char *container_id,
                              unsigned long *cpu_usage,
                              unsigned long *memory_usage) {
    if (!rm || !rm->initialized || !container_id) {
        return -1;
    }

    char path[BUF_SIZE];
    char buffer[BUF_SIZE];

    if (cpu_usage) {
        snprintf(path, sizeof(path), "%s/%s_%s/cpuacct.usage", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
        if (read_file(path, buffer, sizeof(buffer)) == 0) {
            *cpu_usage = strtoul(buffer, nullptr, 10);
        } else {
            *cpu_usage = 0;
        }
    }

    if (memory_usage) {
        snprintf(path, sizeof(path), "%s/%s_%s/memory.usage_in_bytes", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
        if (read_file(path, buffer, sizeof(buffer)) == 0) {
            *memory_usage = strtoul(buffer, nullptr, 10);
        } else {
            *memory_usage = 0;
        }
    }

    return 0;
}

static int set_cpu_limits(resource_manager_t *rm, const char *container_id,
                         const cpu_limits_t *limits) {
    char path[BUF_SIZE];
    char value[32];

    if (limits->shares > 0) {
        snprintf(path, sizeof(path), "%s/%s_%s/cpu.shares", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
        snprintf(value, sizeof(value), "%d", limits->shares);
        if (write_file(path, value) != 0) {
            return -1;
        }
    }

    if (limits->quota_us > 0) {
        snprintf(path, sizeof(path), "%s/%s_%s/cpu.cfs_quota_us", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
        snprintf(value, sizeof(value), "%d", limits->quota_us);
        if (write_file(path, value) != 0) {
            return -1;
        }

        snprintf(path, sizeof(path), "%s/%s_%s/cpu.cfs_period_us", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
        snprintf(value, sizeof(value), "%d", limits->period_us > 0 ? limits->period_us : 100000);
        if (write_file(path, value) != 0) {
            return -1;
        }
    }

    return 0;
}

static int set_memory_limits(resource_manager_t *rm, const char *container_id,
                           const memory_limits_t *limits) {
    char path[BUF_SIZE];
    char value[32];

    if (limits->limit_bytes > 0) {
        snprintf(path, sizeof(path), "%s/%s_%s/memory.limit_in_bytes", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
        snprintf(value, sizeof(value), "%lu", limits->limit_bytes);
        if (write_file(path, value) != 0) {
            return -1;
        }
    }

    if (limits->swap_limit_bytes > 0) {
        snprintf(path, sizeof(path), "%s/%s_%s/memory.memsw.limit_in_bytes", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
        snprintf(value, sizeof(value), "%lu", limits->swap_limit_bytes);
        if (write_file(path, value) != 0) {
            return -1;
        }
    }

    return 0;
}

static int write_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("open cgroup file failed");
        return -1;
    }

    ssize_t written = write(fd, value, strlen(value));
    if (written == -1) {
        perror("write to cgroup file failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int read_file(const char *path, char *buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("open cgroup file failed");
        return -1;
    }

    ssize_t read_bytes = read(fd, buffer, size - 1);
    if (read_bytes == -1) {
        perror("read from cgroup file failed");
        close(fd);
        return -1;
    }

    buffer[read_bytes] = '\0';
    close(fd);
    return 0;
}

void resource_limits_init(resource_limits_t *limits) {
    if (!limits) return;

    limits->cpu.shares = 1024;        // Default CPU shares
    limits->cpu.quota_us = -1;        // No CPU quota by default
    limits->cpu.period_us = 100000;   // Default period

    limits->memory.limit_bytes = 128 * 1024 * 1024;  // 128MB default
    limits->memory.swap_limit_bytes = 0;             // No swap limit

    limits->enabled = 1;  // Enable resource limits by default
}

void resource_manager_cleanup(resource_manager_t *rm) {
    if (!rm) return;

    free(rm->cgroup_path);
    rm->initialized = 0;
}

