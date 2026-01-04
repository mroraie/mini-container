#ifndef CONTAINER_MANAGER_HPP
#define CONTAINER_MANAGER_HPP

#include "namespace_handler.hpp"
#include "resource_manager.hpp"
#include "filesystem_manager.hpp"
#include <sys/types.h>

typedef enum {
    CONTAINER_CREATED,      // Container created but not started
    CONTAINER_RUNNING,      // Container is running
    CONTAINER_STOPPED,      // Container stopped
    CONTAINER_DESTROYED     // Container destroyed
} container_state_t;

typedef struct {
    char *id;                       // Unique container identifier
    char *root_path;                // Container root filesystem path
    namespace_config_t ns_config;   // Namespace configuration
    resource_limits_t res_limits;   // Resource limits
    fs_config_t fs_config;          // Filesystem configuration
    char **command;                 // Command to run in container
    int command_argc;               // Number of command arguments
} container_config_t;

typedef struct {
    char *id;               // Container ID
    pid_t pid;              // Main container process PID
    container_state_t state; // Current container state
    time_t created_at;      // Creation timestamp
    time_t started_at;      // Start timestamp
    time_t stopped_at;      // Stop timestamp
} container_info_t;

typedef struct container_manager {
    resource_manager_t *rm;         // Resource manager
    container_info_t **containers;  // Array of container infos
    int container_count;            // Number of containers
    int max_containers;             // Maximum containers supported
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

#endif /* CONTAINER_MANAGER_HPP */

