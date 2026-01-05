#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <memory>
#include "../include/namespace_handler.hpp"

#define CHILD_STACK_SIZE (8 * 1024 * 1024)

typedef struct {
    namespace_config_t *config;
    char **command;
    int argc;
} clone_args_t;

void namespace_config_init(namespace_config_t *config) {
    if (!config) return;

    config->flags = CONTAINER_NAMESPACES;  // PID, Mount, UTS by default
    config->hostname = nullptr;
}

int namespace_set_hostname(const char *hostname) {
    if (!hostname) {
        fprintf(stderr, "Error: hostname cannot be NULL\n");
        return -1;
    }

    if (sethostname(hostname, strlen(hostname)) == -1) {
        perror("sethostname failed");
        return -1;
    }

    return 0;
}

static int setup_container_filesystem(const namespace_config_t *config) {
    if (mount("proc", "/proc", "proc", 0, nullptr) == -1) {
        perror("mount proc failed");
        return -1;
    }

    if (mount("sysfs", "/sys", "sysfs", 0, nullptr) == -1) {
        perror("mount sysfs failed");
        return -1;
    }

    if (mount("tmpfs", "/tmp", "tmpfs", 0, nullptr) == -1) {
        perror("mount tmpfs failed");
        return -1;
    }

    if (mount("dev", "/dev", "devtmpfs", 0, nullptr) == -1) {
        perror("mount devtmpfs failed");
        return -1;
    }

    return 0;
}

static int container_child(void *arg) {
    clone_args_t *args = static_cast<clone_args_t*>(arg);

    if (namespace_setup_isolation(args->config) != 0) {
        fprintf(stderr, "Failed to setup namespace isolation\n");
        exit(EXIT_FAILURE);
    }

    execvp(args->command[0], args->command);
    perror("execvp failed");
    exit(EXIT_FAILURE);
}

pid_t namespace_clone_process(int flags, void *child_stack, int stack_size,
                             int (*child_func)(void *), void *arg) {
    pid_t pid = clone(child_func, static_cast<char*>(child_stack) + stack_size,
                     flags | SIGCHLD, arg);

    if (pid == -1) {
        perror("clone failed");
        return -1;
    }

    return pid;
}

int namespace_setup_isolation(const namespace_config_t *config) {
    if (config->hostname) {
        if (namespace_set_hostname(config->hostname) != 0) {
            return -1;
        }
    }

    if (setup_container_filesystem(config) != 0) {
        return -1;
    }

    return 0;
}

pid_t namespace_create_container(const namespace_config_t *config,
                               char **command, int argc) {
    if (!config || !command || argc <= 0) {
        fprintf(stderr, "Error: invalid parameters\n");
        return -1;
    }

    std::unique_ptr<char[]> child_stack(new char[CHILD_STACK_SIZE]);
    if (!child_stack) {
        perror("malloc child stack failed");
        return -1;
    }

    clone_args_t args = {
        .config = const_cast<namespace_config_t*>(config),
        .command = command,
        .argc = argc
    };

    int flags = config->flags;

    pid_t pid = namespace_clone_process(flags, child_stack.get(), CHILD_STACK_SIZE,
                                      container_child, &args);

    return pid;
}

int namespace_join(pid_t target_pid, int ns_type) {
    char ns_path[256];
    int fd;

    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", target_pid,
             ns_type == CLONE_NEWPID ? "pid" :
             ns_type == CLONE_NEWNS ? "mnt" :
             ns_type == CLONE_NEWUTS ? "uts" : "unknown");

    fd = open(ns_path, O_RDONLY);
    if (fd == -1) {
        perror("open namespace file failed");
        return -1;
    }

    if (setns(fd, ns_type) == -1) {
        perror("setns failed");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

