#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctime>
#include <csignal>
#include "../include/container_manager.hpp"

#define MAX_CONTAINER_ID 64

#define DEFAULT_MAX_CONTAINERS 10

static char *generate_container_id() {
    static int counter = 0;
    char *id = static_cast<char*>(malloc(MAX_CONTAINER_ID));

    if (!id) {
        return nullptr;
    }

    time_t now = time(nullptr);
    snprintf(id, MAX_CONTAINER_ID, "container_%ld_%d", now, counter++);
    return id;
}

static container_info_t *find_container(container_manager_t *cm, const char *container_id) {
    for (int i = 0; i < cm->container_count; i++) {
        if (strcmp(cm->containers[i]->id, container_id) == 0) {
            return cm->containers[i];
        }
    }
    return nullptr;
}

static int add_container(container_manager_t *cm, container_info_t *info) {
    if (cm->container_count >= cm->max_containers) {
        fprintf(stderr, "Error: maximum containers reached\n");
        return -1;
    }

    cm->containers[cm->container_count++] = info;
    return 0;
}

static void remove_container(container_manager_t *cm, const char *container_id) {
    for (int i = 0; i < cm->container_count; i++) {
        if (strcmp(cm->containers[i]->id, container_id) == 0) {
            free(cm->containers[i]->id);
            free(cm->containers[i]);
            // Shift remaining containers
            for (int j = i; j < cm->container_count - 1; j++) {
                cm->containers[j] = cm->containers[j + 1];
            }
            cm->container_count--;
            break;
        }
    }
}

int container_manager_init(container_manager_t *cm, int max_containers) {
    if (!cm) {
        fprintf(stderr, "Error: container manager is NULL\n");
        return -1;
    }

    cm->max_containers = max_containers > 0 ? max_containers : DEFAULT_MAX_CONTAINERS;
    cm->container_count = 0;
    cm->rm = static_cast<resource_manager_t*>(malloc(sizeof(resource_manager_t)));

    if (!cm->rm) {
        perror("malloc resource manager failed");
        return -1;
    }

    cm->containers = static_cast<container_info_t**>(calloc(cm->max_containers, sizeof(container_info_t *)));
    if (!cm->containers) {
        perror("calloc containers array failed");
        free(cm->rm);
        return -1;
    }

    if (resource_manager_init(cm->rm, "mini_container") != 0) {
        fprintf(stderr, "Failed to initialize resource manager\n");
        free(cm->containers);
        free(cm->rm);
        return -1;
    }

    return 0;
}

int container_manager_create(container_manager_t *cm,
                           const container_config_t *config) {
    if (!cm || !config) {
        fprintf(stderr, "Error: invalid parameters\n");
        return -1;
    }

    char *container_id = config->id ? strdup(config->id) : generate_container_id();
    if (!container_id) {
        perror("failed to generate container ID");
        return -1;
    }

    if (find_container(cm, container_id)) {
        fprintf(stderr, "Error: container %s already exists\n", container_id);
        free(container_id);
        return -1;
    }

    container_info_t *info = static_cast<container_info_t*>(calloc(1, sizeof(container_info_t)));
    if (!info) {
        perror("calloc container info failed");
        free(container_id);
        return -1;
    }

    info->id = container_id;
    info->state = CONTAINER_CREATED;
    info->created_at = time(nullptr);
    info->pid = 0;

    if (resource_manager_create_cgroup(cm->rm, container_id, &config->res_limits) != 0) {
        fprintf(stderr, "Failed to create resource cgroups\n");
        free(info->id);
        free(info);
        return -1;
    }

    if (config->fs_config.create_minimal_fs) {
        if (fs_create_minimal_root(config->fs_config.root_path) != 0) {
            fprintf(stderr, "Failed to create minimal root filesystem\n");
            resource_manager_destroy_cgroup(cm->rm, container_id);
            free(info->id);
            free(info);
            return -1;
        }

        if (fs_populate_container_root(config->fs_config.root_path, "/") != 0) {
            fprintf(stderr, "Warning: failed to populate container root\n");
        }
    }

    if (add_container(cm, info) != 0) {
        resource_manager_destroy_cgroup(cm->rm, container_id);
        free(info->id);
        free(info);
        return -1;
    }

    return 0;
}

int container_manager_start(container_manager_t *cm, const char *container_id) {
    container_info_t *info = find_container(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }

    if (info->state != CONTAINER_CREATED) {
        fprintf(stderr, "Error: container %s is not in CREATED state\n", container_id);
        return -1;
    }

    fprintf(stderr, "Error: container start requires config (limitation of current implementation)\n");
    return -1;
}

int container_manager_stop(container_manager_t *cm, const char *container_id) {
    container_info_t *info = find_container(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }

    if (info->state != CONTAINER_RUNNING) {
        fprintf(stderr, "Error: container %s is not running\n", container_id);
        return -1;
    }

    if (kill(info->pid, SIGTERM) == -1) {
        perror("kill SIGTERM failed");
        if (kill(info->pid, SIGKILL) == -1) {
            perror("kill SIGKILL failed");
            return -1;
        }
    }

    int status;
    if (waitpid(info->pid, &status, 0) == -1) {
        perror("waitpid failed");
        return -1;
    }

    info->state = CONTAINER_STOPPED;
    info->stopped_at = time(nullptr);

    return 0;
}

int container_manager_destroy(container_manager_t *cm, const char *container_id) {
    container_info_t *info = find_container(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }

    if (info->state == CONTAINER_RUNNING) {
        if (container_manager_stop(cm, container_id) != 0) {
            fprintf(stderr, "Warning: failed to stop container during destroy\n");
        }
    }

    resource_manager_destroy_cgroup(cm->rm, container_id);

    remove_container(cm, container_id);

    return 0;
}

int container_manager_exec(container_manager_t *cm,
                          const char *container_id,
                          char **command,
                          int argc) {
    container_info_t *info = find_container(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }

    if (info->state != CONTAINER_RUNNING) {
        fprintf(stderr, "Error: container %s is not running\n", container_id);
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {

        if (namespace_join(info->pid, CLONE_NEWPID) != 0) {
            fprintf(stderr, "Failed to join PID namespace\n");
            exit(EXIT_FAILURE);
        }

        if (namespace_join(info->pid, CLONE_NEWNS) != 0) {
            fprintf(stderr, "Failed to join mount namespace\n");
            exit(EXIT_FAILURE);
        }

        execvp(command[0], command);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid failed");
        return -1;
    }

    return WEXITSTATUS(status);
}

container_info_t **container_manager_list(container_manager_t *cm, int *count) {
    if (!cm || !count) {
        return nullptr;
    }

    *count = cm->container_count;
    return cm->containers;
}

container_info_t *container_manager_get_info(container_manager_t *cm,
                                           const char *container_id) {
    return find_container(cm, container_id);
}

void container_manager_cleanup(container_manager_t *cm) {
    if (!cm) return;

    while (cm->container_count > 0) {
        container_manager_destroy(cm, cm->containers[0]->id);
    }

    if (cm->rm) {
        resource_manager_cleanup(cm->rm);
        free(cm->rm);
    }

    if (cm->containers) {
        free(cm->containers);
    }
}

int container_manager_run(container_manager_t *cm, container_config_t *config) {
    if (!cm || !config) {
        return -1;
    }

    if (!config->id) {
        config->id = generate_container_id();
        if (!config->id) {
            return -1;
        }
    }

    // Create container
    if (container_manager_create(cm, config) != 0) {
        return -1;
    }

    pid_t pid = namespace_create_container(&config->ns_config,
                                         config->command,
                                         config->command_argc);

    if (pid == -1) {
        container_manager_destroy(cm, config->id);
        return -1;
    }

    container_info_t *info = find_container(cm, config->id);
    if (info) {
        info->pid = pid;
        info->state = CONTAINER_RUNNING;
        info->started_at = time(nullptr);
    }

    resource_manager_add_process(cm->rm, config->id, pid);

    return 0;
}

