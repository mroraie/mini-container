#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#include <sys/types.h>

typedef enum {
    RESOURCE_CPU,       // CPU usage limits
    RESOURCE_MEMORY     // Memory usage limits
} resource_type_t;

typedef struct {
    int shares;          // CPU shares (relative weight)
    int quota_us;        // CPU quota in microseconds (-1 for unlimited)
    int period_us;       // CPU period in microseconds
} cpu_limits_t;

typedef struct {
    unsigned long limit_bytes;    // Memory limit in bytes
    unsigned long swap_limit_bytes; // Swap limit in bytes
} memory_limits_t;

typedef struct {
    cpu_limits_t cpu;
    memory_limits_t memory;
    int enabled;        // Whether resource limits are enabled
} resource_limits_t;

typedef struct resource_manager {
    char *cgroup_path;      // Base path for container cgroups
    int initialized;        // Whether cgroups are initialized
} resource_manager_t;

#ifdef __cplusplus
extern "C" {
#endif

int resource_manager_init(resource_manager_t *rm, const char *base_path);

int resource_manager_create_cgroup(resource_manager_t *rm,
                                  const char *container_id,
                                  const resource_limits_t *limits);

int resource_manager_add_process(resource_manager_t *rm,
                                const char *container_id,
                                pid_t pid);

int resource_manager_remove_process(resource_manager_t *rm,
                                   const char *container_id,
                                   pid_t pid);

int resource_manager_destroy_cgroup(resource_manager_t *rm,
                                   const char *container_id);

int resource_manager_get_stats(resource_manager_t *rm,
                              const char *container_id,
                              unsigned long *cpu_usage,
                              unsigned long *memory_usage);

void resource_limits_init(resource_limits_t *limits);

void resource_manager_cleanup(resource_manager_t *rm);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_MANAGER_HPP */

