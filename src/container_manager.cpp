#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <csignal>
#include <dirent.h>
#include "../include/container_manager.hpp"
using namespace std;
#define MAX_CONTAINER_ID 64
#define DEFAULT_MAX_CONTAINERS 10
#define STATE_FILE_PATH "/var/run/mini-container/state.json"
#define STATE_FILE_PATH_FALLBACK "/tmp/mini-container-state.json"

#define DEBUG_LOG(fmt, ...) \
    do { \
        fprintf(stderr, "[DEBUG] %s:%d [%s] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)

#define ERROR_LOG(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] %s:%d [%s] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while(0)
static int extract_numeric_id(const char *id) {
    if (!id) return -1;
    char *endptr;
    long num = strtol(id, &endptr, 10);
    if (*endptr == '\0' && num > 0) {
        return (int)num;
    }
    return -1;
}
static char *generate_container_id(container_manager_t *cm) {
    char *id = static_cast<char*>(malloc(MAX_CONTAINER_ID));
    if (!id) {
        return nullptr;
    }
    int max_id = 0;
    for (int i = 0; i < cm->container_count; i++) {
        int num_id = extract_numeric_id(cm->containers[i]->id);
        if (num_id > max_id) {
            max_id = num_id;
        }
    }
    int next_id = max_id + 1;
    snprintf(id, MAX_CONTAINER_ID, "%d", next_id);
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
        size_t new_size = cm->max_containers * 2;
        container_info_t **new_containers = static_cast<container_info_t**>(realloc(cm->containers, new_size * sizeof(container_info_t *)));
        if (!new_containers) {
            fprintf(stderr, "Error: failed to reallocate containers array\n");
            return -1;
        }
        cm->containers = new_containers;
        cm->max_containers = new_size;
    }
    cm->containers[cm->container_count++] = info;
    return 0;
}
static void free_container_config(container_config_t *config);
static void remove_container(container_manager_t *cm, const char *container_id) {
    for (int i = 0; i < cm->container_count; i++) {
        if (strcmp(cm->containers[i]->id, container_id) == 0) {
            free_container_config(cm->containers[i]->saved_config);
            free(cm->containers[i]->id);
            free(cm->containers[i]);
            for (int j = i; j < cm->container_count - 1; j++) {
                cm->containers[j] = cm->containers[j + 1];
            }
            cm->container_count--;
            break;
        }
    }
}
static int is_pid_alive(pid_t pid) {
    if (pid <= 0) return 0;
    if (kill(pid, 0) == 0) {
        return 1;
    }
    return (errno == ESRCH) ? 0 : 1;
}
static container_config_t* copy_container_config(const container_config_t *src) {
    if (!src) return nullptr;
    container_config_t *dst = static_cast<container_config_t*>(calloc(1, sizeof(container_config_t)));
    if (!dst) {
        return nullptr;
    }
    if (src->id) {
        dst->id = strdup(src->id);
        if (!dst->id) {
            free(dst);
            return nullptr;
        }
    }
    if (src->root_path) {
        dst->root_path = strdup(src->root_path);
        if (!dst->root_path) {
            free_container_config(dst);
            return nullptr;
        }
    }
    dst->ns_config = src->ns_config;
    dst->res_limits = src->res_limits;
    dst->fs_config = src->fs_config;
    if (src->fs_config.root_path) {
        dst->fs_config.root_path = strdup(src->fs_config.root_path);
        if (!dst->fs_config.root_path) {
            free_container_config(dst);
            return nullptr;
        }
    }
    if (src->command && src->command_argc > 0) {
        DEBUG_LOG("copy_container_config: Copying command array, argc=%d", src->command_argc);
        dst->command_argc = src->command_argc;
        dst->command = static_cast<char**>(calloc(src->command_argc + 1, sizeof(char*)));
        if (!dst->command) {
            ERROR_LOG("copy_container_config: calloc failed for command array");
            free_container_config(dst);
            return nullptr;
        }
        for (int i = 0; i < src->command_argc; i++) {
            if (src->command[i]) {
                DEBUG_LOG("copy_container_config: Copying command[%d] = %s", i, src->command[i]);
                dst->command[i] = strdup(src->command[i]);
                if (!dst->command[i]) {
                    ERROR_LOG("copy_container_config: strdup failed for command[%d]", i);
                    for (int j = 0; j < i; j++) {
                        free(dst->command[j]);
                    }
                    free(dst->command);
                    free_container_config(dst);
                    return nullptr;
                }
                DEBUG_LOG("copy_container_config: Copied command[%d] = %s (dst=%p)", i, dst->command[i], (void*)dst->command[i]);
            } else {
                dst->command[i] = nullptr;
            }
        }
        dst->command[src->command_argc] = nullptr;
        DEBUG_LOG("copy_container_config: Command array copied successfully");
    } else {
        DEBUG_LOG("copy_container_config: No command to copy");
        dst->command = nullptr;
        dst->command_argc = 0;
    }
    return dst;
}
static void free_container_config(container_config_t *config) {
    if (!config) return;
    if (config->id) free(config->id);
    if (config->root_path) free(config->root_path);
    if (config->fs_config.root_path) free(config->fs_config.root_path);
    if (config->command) {
        for (int i = 0; i < config->command_argc && config->command[i]; i++) {
            free(config->command[i]);
        }
        free(config->command);
    }
    free(config);
}
static void kill_process_tree(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);
    usleep(100000);
    if (kill(pid, 0) == 0) {
        kill(pid, SIGKILL);
        usleep(50000);
    }
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/task", pid);
    DIR *dir = opendir(proc_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            pid_t tid = atoi(entry->d_name);
            if (tid > 0 && tid != pid) {
                kill(tid, SIGKILL);
            }
        }
        closedir(dir);
    }
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/task/%d/children", pid, pid);
    FILE *fp = fopen(proc_path, "r");
    if (fp) {
        pid_t child_pid;
        while (fscanf(fp, "%d", &child_pid) == 1) {
            if (child_pid > 0) {
                kill_process_tree(child_pid);
            }
        }
        fclose(fp);
    }
}
static const char* get_state_file_path() {
    struct stat st;
    if (stat("/var/run/mini-container", &st) == 0 || mkdir("/var/run/mini-container", 0755) == 0) {
        return STATE_FILE_PATH;
    }
    return STATE_FILE_PATH_FALLBACK;
}
static void save_state(container_manager_t *cm) {
    const char* state_file = get_state_file_path();
    FILE* fp = fopen(state_file, "w");
    if (!fp) {
        return;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"containers\": [\n");
    for (int i = 0; i < cm->container_count; i++) {
        container_info_t* info = cm->containers[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"id\": \"%s\",\n", info->id);
        fprintf(fp, "      \"pid\": %d,\n", info->pid);
        fprintf(fp, "      \"state\": %d,\n", info->state);
        fprintf(fp, "      \"created_at\": %ld,\n", info->created_at);
        fprintf(fp, "      \"started_at\": %ld,\n", info->started_at);
        fprintf(fp, "      \"stopped_at\": %ld\n", info->stopped_at);
        fprintf(fp, "    }%s\n", (i < cm->container_count - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    fclose(fp);
}
static int load_state(container_manager_t *cm) {
    const char* state_file = get_state_file_path();
    FILE* fp = fopen(state_file, "r");
    if (!fp) {
        return 0;
    }
    char line[1024];
    char container_id[256] = {0};
    pid_t pid = 0;
    int state = 0;
    time_t created_at = 0, started_at = 0, stopped_at = 0;
    int loaded = 0;
    bool in_container = false;
    while (fgets(line, sizeof(line), fp)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\0' || *p == '{' || *p == '[' || *p == ']') {
            continue;
        }
        if (strstr(p, "\"id\"")) {
            in_container = true;
            sscanf(p, " \"id\": \"%255[^\"]\"", container_id);
        } else if (in_container && strstr(p, "\"pid\"")) {
            sscanf(p, " \"pid\": %d", &pid);
        } else if (in_container && strstr(p, "\"state\"")) {
            sscanf(p, " \"state\": %d", &state);
        } else if (in_container && strstr(p, "\"created_at\"")) {
            sscanf(p, " \"created_at\": %ld", &created_at);
        } else if (in_container && strstr(p, "\"started_at\"")) {
            sscanf(p, " \"started_at\": %ld", &started_at);
        } else if (in_container && strstr(p, "\"stopped_at\"")) {
            sscanf(p, " \"stopped_at\": %ld", &stopped_at);
        }
        if (in_container && (strstr(p, "}") || (strstr(p, "},")))) {
            if (container_id[0] != '\0') {
                if (state == CONTAINER_RUNNING && !is_pid_alive(pid)) {
                    state = CONTAINER_STOPPED;
                    stopped_at = time(nullptr);
                }
                if (state != CONTAINER_DESTROYED && state != CONTAINER_STOPPED) {
                    container_info_t *info = static_cast<container_info_t*>(calloc(1, sizeof(container_info_t)));
                    if (info) {
                        int num_id = extract_numeric_id(container_id);
                        if (num_id < 0) {
                            int max_id = 0;
                            for (int i = 0; i < cm->container_count; i++) {
                                int existing_num_id = extract_numeric_id(cm->containers[i]->id);
                                if (existing_num_id > max_id) {
                                    max_id = existing_num_id;
                                }
                            }
                            num_id = max_id + 1;
                            char new_id[64];
                            snprintf(new_id, sizeof(new_id), "%d", num_id);
                            info->id = strdup(new_id);
                        } else {
                            info->id = strdup(container_id);
                        }
                        info->pid = pid;
                        info->state = static_cast<container_state_t>(state);
                        info->created_at = created_at;
                        info->started_at = started_at;
                        info->stopped_at = stopped_at;
                        info->saved_config = nullptr;
                        if (add_container(cm, info) == 0) {
                            loaded++;
                        } else {
                            free(info->id);
                            free(info);
                        }
                    }
                }
                memset(container_id, 0, sizeof(container_id));
                pid = 0;
                state = 0;
                created_at = started_at = stopped_at = 0;
                in_container = false;
            }
        }
    }
    fclose(fp);
    return loaded;
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
    int loaded = load_state(cm);
    if (loaded > 0) {
        fprintf(stderr, "Loaded %d container(s) from state file\n", loaded);
        save_state(cm);
    }
    return 0;
}
int container_manager_create(container_manager_t *cm,
                           const container_config_t *config) {
    if (!cm || !config) {
        fprintf(stderr, "Error: invalid parameters\n");
        return -1;
    }
    char *container_id = config->id ? strdup(config->id) : generate_container_id(cm);
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
    info->saved_config = copy_container_config(config);
    if (!info->saved_config) {
        fprintf(stderr, "Failed to copy container configuration\n");
        free(info->id);
        free(info);
        return -1;
    }
    if (resource_manager_create_cgroup(cm->rm, container_id, &config->res_limits) != 0) {
        fprintf(stderr, "Failed to create resource cgroups\n");
        free_container_config(info->saved_config);
        free(info->id);
        free(info);
        return -1;
    }
    if (config->fs_config.create_minimal_fs) {
        if (fs_create_minimal_root(config->fs_config.root_path) != 0) {
            fprintf(stderr, "Failed to create minimal root filesystem\n");
            resource_manager_destroy_cgroup(cm->rm, container_id);
            free_container_config(info->saved_config);
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
        free_container_config(info->saved_config);
        free(info->id);
        free(info);
        return -1;
    }
    save_state(cm);
    return 0;
}
int container_manager_start(container_manager_t *cm, const char *container_id) {
    container_info_t *info = container_manager_get_info(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }
    if (info->state == CONTAINER_RUNNING) {
        fprintf(stderr, "Error: container %s is already running\n", container_id);
        return -1;
    }
    if (info->state == CONTAINER_DESTROYED) {
        fprintf(stderr, "Error: container %s has been destroyed\n", container_id);
        return -1;
    }
    if (info->state != CONTAINER_CREATED && info->state != CONTAINER_STOPPED) {
        fprintf(stderr, "Error: container %s is in invalid state for starting\n", container_id);
        return -1;
    }
    if (!info->saved_config) {
        fprintf(stderr, "Error: Cannot restart container %s - configuration not saved. This container was created before the restart feature was added.\n", container_id);
        return -1;
    }
    container_config_t *config = info->saved_config;
    if (resource_manager_create_cgroup(cm->rm, container_id, &config->res_limits) != 0) {
        fprintf(stderr, "Error: Failed to create/recreate resource cgroups for container %s\n", container_id);
        return -1;
    }
    struct cgroup_callback_data {
        resource_manager_t *rm;
        const char *container_id;
    };
    cgroup_callback_data callback_data = {
        .rm = cm->rm,
        .container_id = container_id
    };
    auto add_to_cgroup = [](pid_t pid, void *user_data) -> void {
        cgroup_callback_data *data = static_cast<cgroup_callback_data*>(user_data);
        resource_manager_add_process(data->rm, data->container_id, pid);
    };
    pid_t pid = namespace_create_container_with_cgroup(&config->ns_config,
                                                      config->command,
                                                      config->command_argc,
                                                      add_to_cgroup,
                                                      &callback_data);
    if (pid == -1) {
        fprintf(stderr, "Error: Failed to start container %s\n", container_id);
        return -1;
    }
    info->pid = pid;
    info->state = CONTAINER_RUNNING;
    info->started_at = time(nullptr);
    info->stopped_at = 0;
    save_state(cm);
    return 0;
}
int container_manager_stop(container_manager_t *cm, const char *container_id) {
    container_info_t *info = container_manager_get_info(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }
    if (info->state != CONTAINER_RUNNING) {
        fprintf(stderr, "Error: container %s is not running\n", container_id);
        return -1;
    }
    const char *actual_container_id = info->id;
    char cgroup_procs_path[512];
    if (cm->rm->version == CGROUP_V2) {
        snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
                 "/sys/fs/cgroup/%s_%s/cgroup.procs", cm->rm->cgroup_path, actual_container_id);
    } else {
        snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
                 "/sys/fs/cgroup/cpu,cpuacct/%s_%s/tasks", cm->rm->cgroup_path, actual_container_id);
    }
    FILE *fp = fopen(cgroup_procs_path, "r");
    if (fp) {
        pid_t pid;
        while (fscanf(fp, "%d", &pid) == 1) {
            if (pid > 0) {
                kill_process_tree(pid);
            }
        }
        fclose(fp);
    }
    kill_process_tree(info->pid);
    int status;
    int waited = 0;
    for (int i = 0; i < 50; i++) {
        if (waitpid(info->pid, &status, WNOHANG) == info->pid) {
            waited = 1;
            break;
        }
        usleep(100000);
    }
    if (!waited) {
        if (kill(info->pid, 0) == 0) {
            kill(info->pid, SIGKILL);
            usleep(200000);
            waitpid(info->pid, &status, WNOHANG);
        }
    }
    info->state = CONTAINER_STOPPED;
    info->stopped_at = time(nullptr);
    save_state(cm);
    return 0;
}
int container_manager_destroy(container_manager_t *cm, const char *container_id) {
    container_info_t *info = container_manager_get_info(cm, container_id);
    if (!info) {
        fprintf(stderr, "Error: container %s not found\n", container_id);
        return -1;
    }
    const char *actual_container_id = info->id;
    if (info->state == CONTAINER_RUNNING) {
        if (container_manager_stop(cm, actual_container_id) != 0) {
            fprintf(stderr, "Warning: failed to stop container during destroy\n");
        }
    }
    resource_manager_destroy_cgroup(cm->rm, actual_container_id);
    remove_container(cm, container_id);
    save_state(cm);
    return 0;
}
int container_manager_exec(container_manager_t *cm,
                          const char *container_id,
                          char **command,
                          int argc) {
    (void)argc;
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
static container_info_t *find_container_by_pid(container_manager_t *cm, pid_t pid) {
    for (int i = 0; i < cm->container_count; i++) {
        if (cm->containers[i]->pid == pid) {
            return cm->containers[i];
        }
    }
    return nullptr;
}
container_info_t *container_manager_get_info(container_manager_t *cm,
                                           const char *container_id) {
    container_info_t *info = find_container(cm, container_id);
    if (info) {
        return info;
    }
    char *endptr;
    long pid_num = strtol(container_id, &endptr, 10);
    if (*endptr == '\0' && pid_num > 0) {
        return find_container_by_pid(cm, (pid_t)pid_num);
    }
    return nullptr;
}
void container_manager_cleanup(container_manager_t *cm) {
    if (!cm) return;
    save_state(cm);
    if (cm->rm) {
        resource_manager_cleanup(cm->rm);
        free(cm->rm);
    }
    if (cm->containers) {
        for (int i = 0; i < cm->container_count; i++) {
            if (cm->containers[i]) {
                free_container_config(cm->containers[i]->saved_config);
                free(cm->containers[i]->id);
                free(cm->containers[i]);
            }
        }
        free(cm->containers);
    }
}
int container_manager_run(container_manager_t *cm, container_config_t *config) {
    DEBUG_LOG("container_manager_run called");
    if (!cm || !config) {
        ERROR_LOG("Invalid parameters: cm=%p, config=%p", (void*)cm, (void*)config);
        return -1;
    }
    DEBUG_LOG("Container ID: %s", config->id ? config->id : "NULL");
    if (!config->id) {
        config->id = generate_container_id(cm);
        if (!config->id) {
            ERROR_LOG("Failed to generate container ID");
            return -1;
        }
    }
    DEBUG_LOG("Calling container_manager_create");
    if (container_manager_create(cm, config) != 0) {
        ERROR_LOG("container_manager_create failed");
        return -1;
    }
    DEBUG_LOG("container_manager_create succeeded, finding container");
    container_info_t *info = find_container(cm, config->id);
    if (!info || !info->saved_config) {
        ERROR_LOG("Container not found or saved_config is NULL: info=%p, saved_config=%p", 
                  (void*)info, info ? (void*)info->saved_config : (void*)nullptr);
        container_manager_destroy(cm, config->id);
        return -1;
    }
    DEBUG_LOG("Found container, saved_config=%p", (void*)info->saved_config);
    DEBUG_LOG("saved_config->command=%p, saved_config->command_argc=%d", 
              (void*)info->saved_config->command, info->saved_config->command_argc);
    if (!info->saved_config->command) {
        ERROR_LOG("saved_config->command is NULL!");
        container_manager_destroy(cm, config->id);
        return -1;
    }
    for (int i = 0; i < info->saved_config->command_argc; i++) {
        DEBUG_LOG("saved_config->command[%d] = %p (%s)", i, 
                  (void*)info->saved_config->command[i],
                  info->saved_config->command[i] ? info->saved_config->command[i] : "NULL");
    }
    struct cgroup_callback_data {
        resource_manager_t *rm;
        const char *container_id;
    };
    cgroup_callback_data callback_data = {
        .rm = cm->rm,
        .container_id = config->id
    };
    auto add_to_cgroup = [](pid_t pid, void *user_data) -> void {
        cgroup_callback_data *data = static_cast<cgroup_callback_data*>(user_data);
        resource_manager_add_process(data->rm, data->container_id, pid);
    };
    DEBUG_LOG("Calling namespace_create_container_with_cgroup");
    pid_t pid = namespace_create_container_with_cgroup(&info->saved_config->ns_config,
                                                      info->saved_config->command,
                                                      info->saved_config->command_argc,
                                                      add_to_cgroup,
                                                      &callback_data);
    DEBUG_LOG("namespace_create_container_with_cgroup returned pid=%d", pid);
    if (pid == -1) {
        ERROR_LOG("namespace_create_container_with_cgroup failed");
        container_manager_destroy(cm, config->id);
        return -1;
    }
    if (info) {
        info->pid = pid;
        info->state = CONTAINER_RUNNING;
        info->started_at = time(nullptr);
    }
    save_state(cm);
    return 0;
}