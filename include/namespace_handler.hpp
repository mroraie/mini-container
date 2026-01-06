#ifndef NAMESPACE_HANDLER_HPP
#define NAMESPACE_HANDLER_HPP

#include <sched.h>
#include <unistd.h>
#include <sys/types.h>

typedef enum {
    NS_PID = CLONE_NEWPID,
    NS_MNT = CLONE_NEWNS,
    NS_UTS = CLONE_NEWUTS
} namespace_type_t;

#define CONTAINER_NAMESPACES (NS_PID | NS_MNT | NS_UTS)

typedef struct {
    int flags;
    char *hostname;
} namespace_config_t;

#ifdef __cplusplus
extern "C" {
#endif

void namespace_config_init(namespace_config_t *config);

int namespace_set_hostname(const char *hostname);

pid_t namespace_clone_process(int flags, void *child_stack, int stack_size,
                             int (*child_func)(void *), void *arg);

int namespace_setup_isolation(const namespace_config_t *config);

pid_t namespace_create_container(const namespace_config_t *config,
                               char **command, int argc);

// Extended version with cgroup callback
pid_t namespace_create_container_with_cgroup(const namespace_config_t *config,
                                            char **command, int argc,
                                            void (*add_to_cgroup_callback)(pid_t pid, void *user_data),
                                            void *cgroup_user_data);

int namespace_join(pid_t target_pid, int ns_type);

#ifdef __cplusplus
}
#endif

#endif

