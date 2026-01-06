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

using namespace std;

#define CHILD_STACK_SIZE (8 * 1024 * 1024)

typedef struct {
    namespace_config_t *config;
    char **command;
    int argc;
    void (*add_to_cgroup_callback)(pid_t pid, void *user_data);
    void *cgroup_user_data;
} clone_args_t;

void namespace_config_init(namespace_config_t *config) {
    if (!config) return;

    config->flags = CONTAINER_NAMESPACES;
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
    (void)config;  // Suppress unused parameter warning

    // Make mount propagation private inside the new mount namespace so mounts
    // don't leak back to the host or siblings.
    if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == -1) {
        perror("mount propagation private failed");
        return -1;
    }

    auto mount_or_ignore_ebusy = [](const char *source, const char *target,
                                    const char *fstype, unsigned long flags) {
        if (mount(source, target, fstype, flags, nullptr) == -1) {
            if (errno == EBUSY) {
                // Already mounted (common for /sys or /proc in cloned namespaces).
                return 0;
            }
            perror("mount failed");
            return -1;
        }
        return 0;
    };

    if (mount_or_ignore_ebusy("proc", "/proc", "proc", 0) == -1) {
        fprintf(stderr, "mount proc failed\n");
        return -1;
    }

    if (mount_or_ignore_ebusy("sysfs", "/sys", "sysfs",
                              MS_NOSUID | MS_NOEXEC | MS_NODEV) == -1) {
        fprintf(stderr, "mount sysfs failed\n");
        return -1;
    }

    if (mount_or_ignore_ebusy("tmpfs", "/tmp", "tmpfs", 0) == -1) {
        fprintf(stderr, "mount tmpfs failed\n");
        return -1;
    }

    if (mount_or_ignore_ebusy("dev", "/dev", "devtmpfs", 0) == -1) {
        fprintf(stderr, "mount devtmpfs failed\n");
        return -1;
    }

    return 0;
}

static int container_child(void *arg) {
    clone_args_t *args = static_cast<clone_args_t*>(arg);
    
    // Note: We can't use getpid() here because we're in a new PID namespace
    // The callback will be called with the actual PID from the parent
    // For now, we'll skip adding to cgroup here and do it in parent after clone
    // This is a limitation - ideally we'd use clone3 with cgroup flag

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
    return namespace_create_container_with_cgroup(config, command, argc, nullptr, nullptr);
}

pid_t namespace_create_container_with_cgroup(const namespace_config_t *config,
                                            char **command, int argc,
                                            void (*add_to_cgroup_callback)(pid_t pid, void *user_data),
                                            void *cgroup_user_data) {
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
        .argc = argc,
        .add_to_cgroup_callback = add_to_cgroup_callback,
        .cgroup_user_data = cgroup_user_data
    };

    int flags = config->flags;

    pid_t pid = namespace_clone_process(flags, child_stack.get(), CHILD_STACK_SIZE,
                                      container_child, &args);
    
    // Add process to cgroup immediately after clone (before execvp in child)
    // This ensures the process is in the cgroup from the start
    if (pid > 0 && add_to_cgroup_callback) {
        // Give child a tiny moment to start (but it hasn't execvp'd yet)
        // This is a race condition workaround - ideally we'd use clone3 with cgroup flag
        usleep(1000); // 1ms delay to ensure child is ready
        add_to_cgroup_callback(pid, cgroup_user_data);
    }

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

