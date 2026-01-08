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

static void remove_container(container_manager_t *cm, const char *container_id) {
    for (int i = 0; i < cm->container_count; i++) {
        if (strcmp(cm->containers[i]->id, container_id) == 0) {
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
                kill_process_tree(child_pid); // Recursively kill children
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
        // Remove leading whitespace
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        
        // Skip empty lines and brackets
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
                if (state != CONTAINER_DESTROYED) {
                    container_info_t *info = static_cast<container_info_t*>(calloc(1, sizeof(container_info_t)));
                    if (info) {
                        info->id = strdup(container_id);
                        info->pid = pid;
                        info->state = static_cast<container_state_t>(state);
                        info->created_at = created_at;
                        info->started_at = started_at;
                        info->stopped_at = stopped_at;
                        
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

    // Load state from file
    int loaded = load_state(cm);
    if (loaded > 0) {
        fprintf(stderr, "Loaded %d container(s) from state file\n", loaded);
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

    // Save state after creating container
    save_state(cm);

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

    kill_process_tree(info->pid);
    char cgroup_procs_path[512];
    if (cm->rm->version == CGROUP_V2) {
        snprintf(cgroup_procs_path, sizeof(cgroup_procs_path), 
                 "/sys/fs/cgroup/%s_%s/cgroup.procs", cm->rm->cgroup_path, container_id);
    } else {
        snprintf(cgroup_procs_path, sizeof(cgroup_procs_path), 
                 "/sys/fs/cgroup/cpu,cpuacct/%s_%s/tasks", cm->rm->cgroup_path, container_id);
    }
    
    FILE *fp = fopen(cgroup_procs_path, "r");
    if (fp) {
        pid_t pid;
        while (fscanf(fp, "%d", &pid) == 1) {
            if (pid > 0 && pid != info->pid) {
                kill_process_tree(pid);
            }
        }
        fclose(fp);
    }

    // Wait for process to terminate
    int status;
    int waited = 0;
    for (int i = 0; i < 10; i++) { // Try up to 1 second
        if (waitpid(info->pid, &status, WNOHANG) == info->pid) {
            waited = 1;
            break;
        }
        usleep(100000); // Wait 100ms
    }
    
    // If still not terminated, force kill
    if (!waited && kill(info->pid, 0) == 0) {
        kill(info->pid, SIGKILL);
        waitpid(info->pid, &status, 0);
    }

    info->state = CONTAINER_STOPPED;
    info->stopped_at = time(nullptr);

    // Save state after stopping container
    save_state(cm);

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

    // Save state after destroying container
    save_state(cm);

    return 0;
}

int container_manager_exec(container_manager_t *cm,
                          const char *container_id,
                          char **command,
                          int argc) {
    (void)argc;  // Suppress unused parameter warning
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

    // Save state before cleanup (so we preserve state even if program crashes)
    save_state(cm);

    // Note: We don't destroy containers here - they should be explicitly destroyed
    // or they will be cleaned up on next load if their PIDs are dead

    if (cm->rm) {
        resource_manager_cleanup(cm->rm);
        free(cm->rm);
    }

    if (cm->containers) {
        // Free container info structures but don't destroy containers
        for (int i = 0; i < cm->container_count; i++) {
            if (cm->containers[i]) {
                free(cm->containers[i]->id);
                free(cm->containers[i]);
            }
        }
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

    if (container_manager_create(cm, config) != 0) {
        return -1;
    }

    // Create a callback structure to pass to namespace_create_container
    struct cgroup_callback_data {
        resource_manager_t *rm;
        const char *container_id;
    };
    
    cgroup_callback_data callback_data = {
        .rm = cm->rm,
        .container_id = config->id
    };
    
    // Callback function to add process to cgroup (called from child process)
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
        container_manager_destroy(cm, config->id);
        return -1;
    }

    container_info_t *info = find_container(cm, config->id);
    if (info) {
        info->pid = pid;
        info->state = CONTAINER_RUNNING;
        info->started_at = time(nullptr);
    }

    // Save state after starting container
    save_state(cm);

    // Note: PID is now added to cgroup from within the child process (before execvp)
    // This ensures the process is in the cgroup from the start

    return 0;
}

