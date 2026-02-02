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
using namespace std;
#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_V2_CONTROLLERS CGROUP_ROOT "/cgroup.controllers"
#define CPU_CGROUP_PATH CGROUP_ROOT "/cpu"
#define CPUACCT_CGROUP_PATH CGROUP_ROOT "/cpuacct"
#define CPU_CPUACCT_CGROUP_PATH CGROUP_ROOT "/cpu,cpuacct"
#define MEMORY_CGROUP_PATH CGROUP_ROOT "/memory"
#define BUF_SIZE 512
#define DEBUG_LOG(rm, fmt, ...) \
    do { \
        fprintf(stderr, "[DEBUG] %s:%d [resource_manager] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)
static int find_cpuacct_usage_path(resource_manager_t *rm, const char *container_id, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s_%s/cpuacct.usage", CPU_CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
    if (access(path, R_OK) == 0) {
        return 0;
    }
    snprintf(path, path_size, "%s/%s_%s/cpuacct.usage", CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
    if (access(path, R_OK) == 0) {
        return 0;
    }
    snprintf(path, path_size, "%s/%s_%s/cpuacct.usage", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
    if (access(path, R_OK) == 0) {
        return 0;
    }
    return -1;
}
static cgroup_version_t detect_cgroup_version() {
    if (access(CGROUP_V2_CONTROLLERS, F_OK) == 0) {
        return CGROUP_V2;
    }
    if (access(CPU_CGROUP_PATH, F_OK) == 0 && access(MEMORY_CGROUP_PATH, F_OK) == 0) {
        return CGROUP_V1;
    }
    return CGROUP_V1;
}
static int write_file(const char *path, const char *value, resource_manager_t *rm = nullptr) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        (void)rm;
        fprintf(stderr, "Error: failed to open cgroup file %s for writing: %s\n", path, strerror(errno));
        return -1;
    }
    ssize_t written = write(fd, value, strlen(value));
    if (written == -1) {
        (void)rm;
        fprintf(stderr, "Error: failed to write to cgroup file %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}
static int read_file(const char *path, char *buffer, size_t size, resource_manager_t *rm = nullptr) {
    (void)rm;
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        if (errno != ENOENT) {
            DEBUG_LOG(rm, "Warning: failed to open cgroup file %s: %s\n", path, strerror(errno));
        }
        return -1;
    }
    ssize_t read_bytes = read(fd, buffer, size - 1);
    if (read_bytes == -1) {
        DEBUG_LOG(rm, "Warning: failed to read from cgroup file %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    if (read_bytes == 0) {
        close(fd);
        return -1;
    }
    buffer[read_bytes] = '\0';
    close(fd);
    return 0;
}
static int set_cpu_limits(resource_manager_t *rm, const char *container_id,
                         const cpu_limits_t *limits) {
    char path[BUF_SIZE];
    char value[64];
    int quota_us = limits->quota_us;
    int period_us = limits->period_us > 0 ? limits->period_us : 100000;
    if (quota_us <= 0 && limits->shares > 0) {
        quota_us = (limits->shares * period_us) / 1024;
        if (quota_us < 1000) quota_us = 1000;
    }
    if (rm->version == CGROUP_V2) {
        if (quota_us > 0) {
            snprintf(path, sizeof(path), "%s/%s_%s/cpu.max", CGROUP_ROOT, rm->cgroup_path, container_id);
            snprintf(value, sizeof(value), "%d %d", quota_us, period_us);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        } else {
            snprintf(path, sizeof(path), "%s/%s_%s/cpu.max", CGROUP_ROOT, rm->cgroup_path, container_id);
            if (write_file(path, "max", rm) != 0) {
                return -1;
            }
        }
    } else {
        char cpu_path[BUF_SIZE];
        snprintf(cpu_path, sizeof(cpu_path), "%s/%s_%s", CPU_CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
        if (access(cpu_path, F_OK) != 0) {
            snprintf(cpu_path, sizeof(cpu_path), "%s/%s_%s", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
        }
        if (quota_us > 0) {
            int ret = snprintf(path, sizeof(path), "%s/cpu.cfs_quota_us", cpu_path);
            if (ret < 0 || (size_t)ret >= sizeof(path)) {
                return -1;
            }
            snprintf(value, sizeof(value), "%d", quota_us);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
            ret = snprintf(path, sizeof(path), "%s/cpu.cfs_period_us", cpu_path);
            if (ret < 0 || (size_t)ret >= sizeof(path)) {
                return -1;
            }
            snprintf(value, sizeof(value), "%d", period_us);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        }
        if (limits->shares > 0) {
            int ret = snprintf(path, sizeof(path), "%s/cpu.shares", cpu_path);
            if (ret < 0 || (size_t)ret >= sizeof(path)) {
                return -1;
            }
            snprintf(value, sizeof(value), "%d", limits->shares);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        }
    }
    return 0;
}
static int set_memory_limits(resource_manager_t *rm, const char *container_id,
                           const memory_limits_t *limits) {
    char path[BUF_SIZE];
    char value[64];
    if (rm->version == CGROUP_V2) {
        if (limits->limit_bytes > 0) {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.max", CGROUP_ROOT, rm->cgroup_path, container_id);
            snprintf(value, sizeof(value), "%lu", limits->limit_bytes);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        } else {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.max", CGROUP_ROOT, rm->cgroup_path, container_id);
            if (write_file(path, "max", rm) != 0) {
                return -1;
            }
        }
        if (limits->swap_limit_bytes > 0) {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.swap.max", CGROUP_ROOT, rm->cgroup_path, container_id);
            snprintf(value, sizeof(value), "%lu", limits->swap_limit_bytes);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        }
    } else {
        if (limits->limit_bytes > 0) {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.limit_in_bytes", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
            snprintf(value, sizeof(value), "%lu", limits->limit_bytes);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        }
        if (limits->swap_limit_bytes > 0) {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.memsw.limit_in_bytes", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
            snprintf(value, sizeof(value), "%lu", limits->swap_limit_bytes);
            if (write_file(path, value, rm) != 0) {
                return -1;
            }
        }
    }
    return 0;
}
int resource_manager_init(resource_manager_t *rm, const char *base_path) {
    if (!rm) {
        fprintf(stderr, "Error: resource manager is NULL\n");
        return -1;
    }
    if (access(CGROUP_ROOT, F_OK) != 0) {
        fprintf(stderr, "Error: cgroups not available at %s\n", CGROUP_ROOT);
        return -1;
    }
    rm->version = detect_cgroup_version();
    if (rm->version == CGROUP_V2) {
        char controllers_path[BUF_SIZE];
        snprintf(controllers_path, sizeof(controllers_path), "%s/cgroup.controllers", CGROUP_ROOT);
        if (access(controllers_path, F_OK) != 0) {
            fprintf(stderr, "Error: cgroup2 controllers not available\n");
            return -1;
        }
        char buffer[BUF_SIZE];
        if (read_file(controllers_path, buffer, sizeof(buffer), rm) == 0) {
            if (strstr(buffer, "cpu") == nullptr || strstr(buffer, "memory") == nullptr) {
                fprintf(stderr, "Error: CPU or memory controllers not enabled in cgroup2\n");
                return -1;
            }
        }
    } else {
        if (access(CPU_CGROUP_PATH, F_OK) != 0) {
            fprintf(stderr, "Error: CPU cgroup subsystem not available\n");
            return -1;
        }
        if (access(MEMORY_CGROUP_PATH, F_OK) != 0) {
            fprintf(stderr, "Error: memory cgroup subsystem not available\n");
            return -1;
        }
    }
    rm->cgroup_path = strdup(base_path ? base_path : "mini_container");
    if (!rm->cgroup_path) {
        perror("strdup failed");
        return -1;
    }
    rm->debug_log_callback = nullptr;
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
    if (rm->version == CGROUP_V2) {
        snprintf(path, sizeof(path), "%s/%s_%s", CGROUP_ROOT, rm->cgroup_path, container_id);
        if (mkdir(path, 0755) == -1 && errno != EEXIST) {
            perror("mkdir cgroup2 directory failed");
            return -1;
        }
    } else {
        snprintf(path, sizeof(path), "%s/%s_%s", CPU_CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
        if (mkdir(path, 0755) == -1 && errno != EEXIST) {
            snprintf(path, sizeof(path), "%s/%s_%s", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
            if (mkdir(path, 0755) == -1 && errno != EEXIST) {
                perror("mkdir CPU cgroup failed");
                return -1;
            }
            snprintf(path, sizeof(path), "%s/%s_%s", CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
            if (mkdir(path, 0755) == -1 && errno != EEXIST) {
            }
        }
        snprintf(path, sizeof(path), "%s/%s_%s", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
        if (mkdir(path, 0755) == -1 && errno != EEXIST) {
            perror("mkdir memory cgroup failed");
            return -1;
        }
    }
    if (limits->cpu.shares > 0 || limits->cpu.quota_us > 0) {
        if (set_cpu_limits(rm, container_id, &limits->cpu) != 0) {
            fprintf(stderr, "Warning: failed to set CPU limits for container %s\n", container_id);
        }
    }
    if (limits->memory.limit_bytes > 0) {
        if (set_memory_limits(rm, container_id, &limits->memory) != 0) {
            fprintf(stderr, "Warning: failed to set memory limits for container %s\n", container_id);
        }
    }
    return 0;
}
static int add_all_threads_to_cgroup(resource_manager_t *rm, const char *container_id, pid_t pid, const char *cgroup_path) {
    (void)container_id;
    char task_dir_path[BUF_SIZE];
    snprintf(task_dir_path, sizeof(task_dir_path), "/proc/%d/task", pid);
    DIR *dir = opendir(task_dir_path);
    if (!dir) {
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        return write_file(cgroup_path, pid_str, rm);
    }
    struct dirent *entry;
    int added = 0;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char tid_str[32];
        strncpy(tid_str, entry->d_name, sizeof(tid_str) - 1);
        tid_str[sizeof(tid_str) - 1] = '\0';
        if (write_file(cgroup_path, tid_str, rm) == 0) {
            added = 1;
        }
    }
    closedir(dir);
    return added ? 0 : -1;
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
    if (rm->version == CGROUP_V2) {
        snprintf(path, sizeof(path), "%s/%s_%s/cgroup.procs", CGROUP_ROOT, rm->cgroup_path, container_id);
        DEBUG_LOG(rm, "Debug: Adding process %d to cgroup v2: %s\n", pid, path);
        if (write_file(path, pid_str, rm) != 0) {
            DEBUG_LOG(rm, "Warning: failed to add process %d to cgroup v2: %s (errno=%d: %s)\n", pid, path, errno, strerror(errno));
            return -1;
        }
        DEBUG_LOG(rm, "Debug: Successfully added process %d to cgroup v2\n", pid);
    } else {
        int added = 0;
        snprintf(path, sizeof(path), "%s/%s_%s/tasks", CPU_CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
        DEBUG_LOG(rm, "Debug: Trying to add process %d to cgroup v1: %s\n", pid, path);
        if (add_all_threads_to_cgroup(rm, container_id, pid, path) == 0) {
            added = 1;
            DEBUG_LOG(rm, "Debug: Successfully added process %d threads to cpu,cpuacct cgroup\n", pid);
        } else {
            DEBUG_LOG(rm, "Debug: Failed to add to cpu,cpuacct (errno=%d: %s), trying fallback...\n", errno, strerror(errno));
            snprintf(path, sizeof(path), "%s/%s_%s/tasks", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
            if (add_all_threads_to_cgroup(rm, container_id, pid, path) == 0) {
                added = 1;
                DEBUG_LOG(rm, "Debug: Successfully added process %d threads to CPU cgroup\n", pid);
            } else {
                DEBUG_LOG(rm, "Warning: failed to add process %d threads to CPU cgroup: %s (errno=%d: %s)\n", pid, path, errno, strerror(errno));
            }
            snprintf(path, sizeof(path), "%s/%s_%s/tasks", CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
            if (add_all_threads_to_cgroup(rm, container_id, pid, path) == 0) {
                added = 1;
                DEBUG_LOG(rm, "Debug: Successfully added process %d threads to cpuacct cgroup\n", pid);
            } else {
                DEBUG_LOG(rm, "Debug: Failed to add to cpuacct cgroup (errno=%d: %s)\n", errno, strerror(errno));
            }
        }
        if (!added) {
            DEBUG_LOG(rm, "Warning: failed to add process %d to any CPU cgroup\n", pid);
            return -1;
        }
        snprintf(path, sizeof(path), "%s/%s_%s/tasks", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
        DEBUG_LOG(rm, "Debug: Adding process %d threads to memory cgroup: %s\n", pid, path);
        if (add_all_threads_to_cgroup(rm, container_id, pid, path) != 0) {
            DEBUG_LOG(rm, "Warning: failed to add process %d threads to memory cgroup: %s (errno=%d: %s)\n", pid, path, errno, strerror(errno));
            return -1;
        } else {
            DEBUG_LOG(rm, "Debug: Successfully added process %d threads to memory cgroup\n", pid);
        }
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
    if (rm->version == CGROUP_V2) {
        snprintf(path, sizeof(path), "%s/%s_%s", CGROUP_ROOT, rm->cgroup_path, container_id);
        rmdir(path);
    } else {
        snprintf(path, sizeof(path), "%s/%s_%s", CPU_CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
        rmdir(path);
        snprintf(path, sizeof(path), "%s/%s_%s", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
        rmdir(path);
        snprintf(path, sizeof(path), "%s/%s_%s", CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
        rmdir(path);
        snprintf(path, sizeof(path), "%s/%s_%s", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
        rmdir(path);
    }
    return 0;
}
int resource_manager_get_stats(resource_manager_t *rm,
                              const char *container_id,
                              unsigned long *cpu_usage,
                              unsigned long *memory_usage) {
    if (!rm || !rm->initialized || !container_id) {
        if (cpu_usage) *cpu_usage = 0;
        if (memory_usage) *memory_usage = 0;
        return -1;
    }
    char path[BUF_SIZE];
    char buffer[BUF_SIZE];
    if (cpu_usage) *cpu_usage = 0;
    if (memory_usage) *memory_usage = 0;
    DEBUG_LOG(rm, "Debug: Getting stats for container %s, cgroup_path=%s, version=%s\n",
            container_id, rm->cgroup_path, rm->version == CGROUP_V2 ? "v2" : "v1");
    if (rm->version == CGROUP_V2) {
        snprintf(path, sizeof(path), "%s/%s_%s/cgroup.procs", CGROUP_ROOT, rm->cgroup_path, container_id);
        if (read_file(path, buffer, sizeof(buffer), rm) == 0) {
            if (strlen(buffer) > 0 && buffer[0] != '\n' && buffer[0] != '\0') {
                DEBUG_LOG(rm, "Debug: Found processes in cgroup.procs: '%s'\n", buffer);
            } else {
                DEBUG_LOG(rm, "Debug: No processes found in cgroup.procs (container may have finished)\n");
            }
        }
    }
    if (cpu_usage) {
        *cpu_usage = 0;
        if (rm->version == CGROUP_V2) {
            snprintf(path, sizeof(path), "%s/%s_%s/cpu.stat", CGROUP_ROOT, rm->cgroup_path, container_id);
            DEBUG_LOG(rm, "Debug: Attempting to read cpu.stat from %s\n", path);
            if (read_file(path, buffer, sizeof(buffer), rm) == 0) {
                DEBUG_LOG(rm, "Debug: Successfully read cpu.stat, content length: %zu, content: '%s'\n", strlen(buffer), buffer);
                char *usage_line = strstr(buffer, "usage_usec");
                if (usage_line) {
                    DEBUG_LOG(rm, "Debug: Found usage_usec line in cpu.stat\n");
                    char *value_start = usage_line;
                    while (*value_start && *value_start != ' ' && *value_start != '\t') value_start++;
                    while (*value_start && (*value_start == ' ' || *value_start == '\t')) value_start++;
                    if (*value_start) {
                        char *endptr;
                        unsigned long val = strtoul(value_start, &endptr, 10);
                        if (endptr != value_start) {
                            *cpu_usage = val * 1000;
                            DEBUG_LOG(rm, "Debug: Parsed CPU usage from cpu.stat: %lu microseconds = %lu nanoseconds\n", val, *cpu_usage);
                        } else {
                            DEBUG_LOG(rm, "Debug: Failed to parse usage_usec value from '%s'\n", value_start);
                        }
                    } else {
                        DEBUG_LOG(rm, "Debug: No value found after usage_usec in cpu.stat\n");
                    }
                } else {
                    DEBUG_LOG(rm, "Debug: usage_usec not found in cpu.stat content\n");
                }
            } else {
                DEBUG_LOG(rm, "Debug: Failed to read cpu.stat from %s (errno=%d: %s)\n", path, errno, strerror(errno));
            }
        } else {
            char cpuacct_path[BUF_SIZE];
            if (find_cpuacct_usage_path(rm, container_id, cpuacct_path, sizeof(cpuacct_path)) == 0) {
                DEBUG_LOG(rm, "Debug: Found cpuacct.usage at %s\n", cpuacct_path);
                if (read_file(cpuacct_path, buffer, sizeof(buffer), rm) == 0) {
                    DEBUG_LOG(rm, "Debug: Read from cpuacct.usage: '%s'\n", buffer);
                    char *endptr;
                    unsigned long val = strtoul(buffer, &endptr, 10);
                    if (endptr != buffer && (*endptr == '\0' || *endptr == '\n' || *endptr == ' ')) {
                        *cpu_usage = val;
                        DEBUG_LOG(rm, "Debug: Parsed CPU usage: %lu ns\n", val);
                    } else {
                        DEBUG_LOG(rm, "Debug: Failed to parse CPU usage from %s: '%s' (endptr='%s')\n", cpuacct_path, buffer, endptr);
                    }
                } else {
                    DEBUG_LOG(rm, "Debug: Failed to read cpuacct.usage from %s (errno=%d: %s)\n", cpuacct_path, errno, strerror(errno));
                }
            } else {
                DEBUG_LOG(rm, "Debug: Could not find cpuacct.usage path for container %s\n", container_id);
                snprintf(path, sizeof(path), "%s/%s_%s/cpuacct.usage", CPU_CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
                DEBUG_LOG(rm, "Debug: Tried path: %s (exists: %s)\n", path, access(path, F_OK) == 0 ? "yes" : "no");
                snprintf(path, sizeof(path), "%s/%s_%s/cpuacct.usage", CPUACCT_CGROUP_PATH, rm->cgroup_path, container_id);
                DEBUG_LOG(rm, "Debug: Tried path: %s (exists: %s)\n", path, access(path, F_OK) == 0 ? "yes" : "no");
                snprintf(path, sizeof(path), "%s/%s_%s/cpuacct.usage", CPU_CGROUP_PATH, rm->cgroup_path, container_id);
                DEBUG_LOG(rm, "Debug: Tried path: %s (exists: %s)\n", path, access(path, F_OK) == 0 ? "yes" : "no");
            }
        }
    }
    if (memory_usage) {
        *memory_usage = 0;
        if (rm->version == CGROUP_V2) {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.current", CGROUP_ROOT, rm->cgroup_path, container_id);
            DEBUG_LOG(rm, "Debug: Attempting to read memory.current from %s\n", path);
            DEBUG_LOG(rm, "Debug: File exists: %s\n", access(path, F_OK) == 0 ? "yes" : "no");
            if (read_file(path, buffer, sizeof(buffer), rm) == 0) {
                DEBUG_LOG(rm, "Debug: Successfully read memory.current, content: '%s' (length: %zu)\n", buffer, strlen(buffer));
                char *endptr;
                unsigned long val = strtoul(buffer, &endptr, 10);
                if (endptr != buffer && (*endptr == '\0' || *endptr == '\n' || *endptr == ' ')) {
                    *memory_usage = val;
                    DEBUG_LOG(rm, "Debug: Parsed memory usage: %lu bytes (%.2f MB)\n", val, val / (1024.0 * 1024.0));
                } else {
                    DEBUG_LOG(rm, "Debug: Failed to parse memory usage from %s: '%s' (endptr='%s')\n", path, buffer, endptr);
                }
            } else {
                DEBUG_LOG(rm, "Debug: Failed to read memory.current from %s (errno=%d: %s)\n", path, errno, strerror(errno));
            }
        } else {
            snprintf(path, sizeof(path), "%s/%s_%s/memory.usage_in_bytes", MEMORY_CGROUP_PATH, rm->cgroup_path, container_id);
            DEBUG_LOG(rm, "Debug: Reading memory from %s (exists: %s)\n", path, access(path, F_OK) == 0 ? "yes" : "no");
            if (read_file(path, buffer, sizeof(buffer), rm) == 0) {
                DEBUG_LOG(rm, "Debug: Read from memory.usage_in_bytes: '%s' (length: %zu)\n", buffer, strlen(buffer));
                char *endptr;
                unsigned long val = strtoul(buffer, &endptr, 10);
                if (endptr != buffer && (*endptr == '\0' || *endptr == '\n' || *endptr == ' ')) {
                    *memory_usage = val;
                    DEBUG_LOG(rm, "Debug: Parsed memory usage: %lu bytes (%.2f MB)\n", val, val / (1024.0 * 1024.0));
                } else {
                    DEBUG_LOG(rm, "Debug: Failed to parse memory usage from %s: '%s' (endptr='%s')\n", path, buffer, endptr);
                }
            } else {
                DEBUG_LOG(rm, "Debug: Failed to read memory.usage_in_bytes from %s (errno=%d: %s)\n", path, errno, strerror(errno));
            }
        }
    }
    return 0;
}
void resource_limits_init(resource_limits_t *limits) {
    if (!limits) return;
    limits->cpu.shares = 1024;
    limits->cpu.quota_us = -1;
    limits->cpu.period_us = 100000;
    limits->memory.limit_bytes = 128 * 1024 * 1024;
    limits->memory.swap_limit_bytes = 0;
    limits->enabled = 1;
}
void resource_manager_cleanup(resource_manager_t *rm) {
    if (!rm) return;
    free(rm->cgroup_path);
    rm->initialized = 0;
}