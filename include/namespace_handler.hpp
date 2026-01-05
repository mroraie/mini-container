#ifndef NAMESPACE_HANDLER_HPP
#define NAMESPACE_HANDLER_HPP

#include <sched.h>
#include <unistd.h>
#include <sys/types.h>

typedef enum {
    NS_PID = CLONE_NEWPID,       // Process ID isolation
    NS_MNT = CLONE_NEWNS,        // Mount namespace isolation
    NS_UTS = CLONE_NEWUTS        // Hostname isolation
} namespace_type_t;

#define CONTAINER_NAMESPACES (NS_PID | NS_MNT | NS_UTS)

typedef struct {
    int flags;              // Combined namespace flags
    char *hostname;         // Container hostname (for UTS namespace)
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

int namespace_join(pid_t target_pid, int ns_type);

#ifdef __cplusplus
}
#endif

#endif /* NAMESPACE_HANDLER_HPP */

