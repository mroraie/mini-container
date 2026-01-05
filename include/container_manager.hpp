#ifndef CONTAINER_MANAGER_HPP
#define CONTAINER_MANAGER_HPP

#include "namespace_handler.hpp"
#include "resource_manager.hpp"
#include "filesystem_manager.hpp"
#include <sys/types.h>

typedef enum {
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_DESTROYED
} container_state_t;

typedef struct {
    char *id;
    char *root_path;
    namespace_config_t ns_config;
    resource_limits_t res_limits;
    fs_config_t fs_config;
    char **command;
    int command_argc;
} container_config_t;

typedef struct {
    char *id;
    pid_t pid;
    container_state_t state;
    time_t created_at;
    time_t started_at;
    time_t stopped_at;
} container_info_t;

typedef struct container_manager {
    resource_manager_t *rm;
    container_info_t **containers;
    int container_count;
    int max_containers;
} container_manager_t;

#ifdef __cplusplus
extern "C" {
#endif

int container_manager_init(container_manager_t *cm, int max_containers);

int container_manager_create(container_manager_t *cm,
                           const container_config_t *config);

int container_manager_start(container_manager_t *cm, const char *container_id);

int container_manager_stop(container_manager_t *cm, const char *container_id);

int container_manager_destroy(container_manager_t *cm, const char *container_id);

int container_manager_exec(container_manager_t *cm,
                          const char *container_id,
                          char **command,
                          int argc);

container_info_t **container_manager_list(container_manager_t *cm, int *count);

container_info_t *container_manager_get_info(container_manager_t *cm,
                                           const char *container_id);

void container_manager_cleanup(container_manager_t *cm);

int container_manager_run(container_manager_t *cm, container_config_t *config);

#ifdef __cplusplus
}
#endif

#endif

