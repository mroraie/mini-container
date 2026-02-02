#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP
#include <sys/types.h>
typedef enum {
    RESOURCE_CPU,
    RESOURCE_MEMORY
} resource_type_t;
typedef struct {
    int shares;
    int quota_us;
    int period_us;
} cpu_limits_t;
typedef struct {
    unsigned long limit_bytes;
    unsigned long swap_limit_bytes;
} memory_limits_t;
typedef struct {
    cpu_limits_t cpu;
    memory_limits_t memory;
    int enabled;
} resource_limits_t;
typedef enum {
    CGROUP_V1,
    CGROUP_V2
} cgroup_version_t;
typedef struct resource_manager {
    char *cgroup_path;
    int initialized;
    cgroup_version_t version;
    void (*debug_log_callback)(const char*);
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
#endif