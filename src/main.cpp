#include <sys/wait.h>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include "../include/container_manager.hpp"

using namespace std;

static container_manager_t cm;

static const char *state_names[] = {
    [CONTAINER_CREATED] = "CREATED",
    [CONTAINER_RUNNING] = "RUNNING",
    [CONTAINER_STOPPED] = "STOPPED",
    [CONTAINER_DESTROYED] = "DESTROYED"};

static void print_usage(const char *program_name)
{
    printf("Mini Container System - Lightweight container implementation\n\n");
    printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
    printf("Commands:\n");
    printf("  run <command> [args...]    Run a command in a new container\n");
    printf("  stop <container_id>        Stop a running container\n");
    printf("  list                       List all containers\n");
    printf("  exec <container_id> <cmd>  Execute command in running container\n");
    printf("  destroy <container_id>     Destroy a container\n");
    printf("  info <container_id>        Show container information\n");
    printf("\nOptions:\n");
    printf("  -h, --help                 Show this help message\n");
    printf("  -m, --memory <MB>          Memory limit in MB (default: 128)\n");
    printf("  -c, --cpu <shares>         CPU shares (default: 1024)\n");
    printf("  -r, --root <path>          Container root filesystem path\n");
    printf("  -n, --hostname <name>      Container hostname\n");
    printf("\nExamples:\n");
    printf("  %s run /bin/sh\n", program_name);
    printf("  %s run --memory 256 --cpu 512 /bin/echo \"Hello World\"\n", program_name);
    printf("  %s list\n", program_name);
    printf("  %s stop container_123\n", program_name);
    printf("  %s exec container_123 /bin/ps\n", program_name);
}

static int parse_run_options(int argc, char *argv[], container_config_t *config)
{
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"memory", required_argument, 0, 'm'},
        {"cpu", required_argument, 0, 'c'},
        {"root", required_argument, 0, 'r'},
        {"hostname", required_argument, 0, 'n'},
        {0, 0, 0, 0}};

    int option_index = 0;
    int c;

    config->id = nullptr;
    namespace_config_init(&config->ns_config);
    resource_limits_init(&config->res_limits);
    fs_config_init(&config->fs_config);

    while ((c = getopt_long(argc, argv, "hm:c:r:n:", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'h':
            print_usage(argv[0]);
            return -1;

        case 'm':
            config->res_limits.memory.limit_bytes = atoi(optarg) * 1024 * 1024;
            break;

        case 'c':
            config->res_limits.cpu.shares = atoi(optarg);
            break;

        case 'r':
            config->fs_config.root_path = strdup(optarg);
            break;

        case 'n':
            config->ns_config.hostname = strdup(optarg);
            break;

        default:
            fprintf(stderr, "Unknown option: %c\n", c);
            return -1;
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "Error: no command specified\n");
        return -1;
    }

    config->command = &argv[optind];
    config->command_argc = argc - optind;

    if (!config->fs_config.root_path)
    {
        char default_root[256];
        snprintf(default_root, sizeof(default_root), "/tmp/container_%d", getpid());
        config->fs_config.root_path = strdup(default_root);
    }

    return 0;
}

static int handle_run(int argc, char *argv[])
{
    container_config_t config;

    if (parse_run_options(argc, argv, &config) != 0)
    {
        return EXIT_FAILURE;
    }

    printf("Creating container...\n");

    if (container_manager_run(&cm, &config) != 0)
    {
        fprintf(stderr, "Failed to run container\n");
        return EXIT_FAILURE;
    }

    container_info_t *info = container_manager_get_info(&cm, config.id);
    if (info)
    {
        printf("Container %s started with PID %d\n", info->id, info->pid);

        if (info->pid > 0)
        {
            int status;
            waitpid(info->pid, &status, 0);
            printf("Container %s exited with status %d\n", info->id, WEXITSTATUS(status));
        }
    }

    free(config.id);
    free(config.ns_config.hostname);
    free(config.fs_config.root_path);

    return EXIT_SUCCESS;
}

static int handle_stop(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: container ID required\n");
        return EXIT_FAILURE;
    }

    const char *container_id = argv[1];

    if (container_manager_stop(&cm, container_id) != 0)
    {
        fprintf(stderr, "Failed to stop container %s\n", container_id);
        return EXIT_FAILURE;
    }

    printf("Container %s stopped\n", container_id);
    return EXIT_SUCCESS;
}

static int handle_list(int argc, char *argv[])
{
    int count;
    container_info_t **containers = container_manager_list(&cm, &count);

    if (count == 0)
    {
        printf("No containers\n");
        return EXIT_SUCCESS;
    }

    printf("%-20s %-10s %-10s %-15s %-15s\n",
           "CONTAINER ID", "STATE", "PID", "CREATED", "STARTED");
    printf("%-20s %-10s %-10s %-15s %-15s\n",
           "------------", "-----", "---", "-------", "-------");

    for (int i = 0; i < count; i++)
    {
        container_info_t *info = containers[i];
        char created_str[20] = "";
        char started_str[20] = "";

        if (info->created_at > 0)
        {
            strftime(created_str, sizeof(created_str), "%H:%M:%S",
                     localtime(&info->created_at));
        }

        if (info->started_at > 0)
        {
            strftime(started_str, sizeof(started_str), "%H:%M:%S",
                     localtime(&info->started_at));
        }

        printf("%-20s %-10s %-10d %-15s %-15s\n",
               info->id,
               state_names[info->state],
               info->pid,
               created_str,
               started_str);
    }

    return EXIT_SUCCESS;
}

static int handle_exec(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Error: container ID and command required\n");
        return EXIT_FAILURE;
    }

    const char *container_id = argv[1];
    char **command = &argv[2];
    int command_argc = argc - 2;

    int result = container_manager_exec(&cm, container_id, command, command_argc);

    if (result != 0)
    {
        fprintf(stderr, "Command failed with exit code %d\n", result);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int handle_destroy(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: container ID required\n");
        return EXIT_FAILURE;
    }

    const char *container_id = argv[1];

    if (container_manager_destroy(&cm, container_id) != 0)
    {
        fprintf(stderr, "Failed to destroy container %s\n", container_id);
        return EXIT_FAILURE;
    }

    printf("Container %s destroyed\n", container_id);
    return EXIT_SUCCESS;
}

static int handle_info(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: container ID required\n");
        return EXIT_FAILURE;
    }

    const char *container_id = argv[1];
    container_info_t *info = container_manager_get_info(&cm, container_id);

    if (!info)
    {
        fprintf(stderr, "Container %s not found\n", container_id);
        return EXIT_FAILURE;
    }

    printf("Container ID: %s\n", info->id);
    printf("State: %s\n", state_names[info->state]);
    printf("PID: %d\n", info->pid);
    printf("Created: %s", ctime(&info->created_at));
    if (info->started_at > 0)
    {
        printf("Started: %s", ctime(&info->started_at));
    }
    if (info->stopped_at > 0)
    {
        printf("Stopped: %s", ctime(&info->stopped_at));
    }

    if (info->state == CONTAINER_RUNNING)
    {
        unsigned long cpu_usage = 0, memory_usage = 0;
        resource_manager_get_stats(cm.rm, container_id, &cpu_usage, &memory_usage);
        printf("CPU Usage: %lu nanoseconds\n", cpu_usage);
        printf("Memory Usage: %lu bytes\n", memory_usage);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    if (container_manager_init(&cm, 10) != 0)
    {
        fprintf(stderr, "Failed to initialize container manager\n");
        return EXIT_FAILURE;
    }

    if (getuid() != 0)
    {
        fprintf(stderr, "Warning: container operations typically require root privileges\n");
    }

    if (argc < 2)
    {
        print_usage(argv[0]);
        container_manager_cleanup(&cm);
        return EXIT_FAILURE;
    }

    const char *command = argv[1];
    int result = EXIT_FAILURE;

    if (strcmp(command, "run") == 0)
    {
        result = handle_run(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "stop") == 0)
    {
        result = handle_stop(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "list") == 0)
    {
        result = handle_list(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "exec") == 0)
    {
        result = handle_exec(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "destroy") == 0)
    {
        result = handle_destroy(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "info") == 0)
    {
        result = handle_info(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "help") == 0 || strcmp(command, "-h") == 0 ||
             strcmp(command, "--help") == 0)
    {
        print_usage(argv[0]);
        result = EXIT_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        result = EXIT_FAILURE;
    }

    container_manager_cleanup(&cm);
    return result;
}
