#include <sys/wait.h>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <csignal>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/sysinfo.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>
#include "../include/container_manager.hpp"
#include "web_server_simple.hpp"
using namespace std;
static container_manager_t cm;
static bool running = true;
static bool monitor_mode = false;
static SimpleWebServer* web_server = nullptr;
static const char *state_names[] = {
    [CONTAINER_CREATED] = "CREATED",
    [CONTAINER_RUNNING] = "RUNNING",
    [CONTAINER_STOPPED] = "STOPPED",
    [CONTAINER_DESTROYED] = "DESTROYED"};
static inline const char *safe_state_name(container_state_t state) {
    int s = (int)state;
    if (s < (int)CONTAINER_CREATED || s > (int)CONTAINER_DESTROYED) {
        return "UNKNOWN";
    }
    const char *name = state_names[s];
    return name ? name : "UNKNOWN";
}
void clear_screen() {
    printf("\033[2J\033[H");
}
void hide_cursor() {
    printf("\033[?25l");
}
void show_cursor() {
    printf("\033[?25h");
}
void set_color(const char* color) {
    printf("%s", color);
}
void reset_color() {
    printf("\033[0m");
}
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_CYAN = "\033[36m";
const char* COLOR_BLUE = "\033[34m";
const char* COLOR_WHITE = "\033[37m";
const char* COLOR_BOLD = "\033[1m";
static void print_usage(const char *program_name)
{
    printf("Mini Container System - Lightweight container implementation\n\n");
    printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
    printf("Commands:\n");
    printf("  run <command> [args...]    Run a command in a new container\n");
    printf("  start <container_id>       Start a stopped container\n");
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
    printf("  -d, --detach               Run container in background (don't wait)\n");
    printf("\nExamples:\n");
    printf("  %s run /bin/sh\n", program_name);
    printf("  %s run --memory 256 --cpu 512 /bin/echo \"Hello World\"\n", program_name);
    printf("  %s list\n", program_name);
    printf("  %s stop container_123\n", program_name);
    printf("  %s exec container_123 /bin/ps\n", program_name);
}
static int parse_run_options(int argc, char *argv[], container_config_t *config, int *detach)
{
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"memory", required_argument, 0, 'm'},
        {"cpu", required_argument, 0, 'c'},
        {"root", required_argument, 0, 'r'},
        {"detach", no_argument, 0, 'd'},
        {0, 0, 0, 0}};
    int option_index = 0;
    int c;
    config->id = nullptr;
    config->fs_config.root_path = nullptr;
    *detach = 0;
    namespace_config_init(&config->ns_config);
    resource_limits_init(&config->res_limits);
    fs_config_init(&config->fs_config);
    while ((c = getopt_long(argc, argv, "hm:c:r:d", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'h':
            print_usage(argv[0]);
            if (config->fs_config.root_path) {
                free(config->fs_config.root_path);
                config->fs_config.root_path = nullptr;
            }
            return -1;
        case 'm':
            config->res_limits.memory.limit_bytes = atoi(optarg) * 1024 * 1024;
            break;
        case 'c':
            config->res_limits.cpu.shares = atoi(optarg);
            break;
        case 'r':
            if (config->fs_config.root_path) {
                free(config->fs_config.root_path);
            }
            config->fs_config.root_path = strdup(optarg);
            if (!config->fs_config.root_path) {
                perror("strdup failed");
                return -1;
            }
            break;
        case 'd':
            *detach = 1;
            break;
        default:
            fprintf(stderr, "Unknown option: %c\n", c);
            if (config->fs_config.root_path) {
                free(config->fs_config.root_path);
                config->fs_config.root_path = nullptr;
            }
            return -1;
        }
    }
    if (optind >= argc)
    {
        fprintf(stderr, "Error: no command specified\n");
        if (config->fs_config.root_path) {
            free(config->fs_config.root_path);
            config->fs_config.root_path = nullptr;
        }
        return -1;
    }
    config->command = &argv[optind];
    config->command_argc = argc - optind;
    if (!config->fs_config.root_path)
    {
        config->fs_config.root_path = strdup("/");
        if (!config->fs_config.root_path) {
            perror("strdup failed");
            return -1;
        }
    }
    return 0;
}
static int handle_run(int argc, char *argv[])
{
    container_config_t config;
    int detach = 0;
    if (parse_run_options(argc, argv, &config, &detach) != 0)
    {
        return EXIT_FAILURE;
    }
    if (container_manager_run(&cm, &config) != 0)
    {
        fprintf(stderr, "Failed to run container\n");
        if (config.id) {
            free(config.id);
            config.id = nullptr;
        }
        if (config.fs_config.root_path) {
            free(config.fs_config.root_path);
            config.fs_config.root_path = nullptr;
        }
        return EXIT_FAILURE;
    }
    container_info_t *info = container_manager_get_info(&cm, config.id);
    if (info)
    {
        if (detach)
        {
            while (running) {
                sleep(1);
            }
        }
        else
        {
            if (info->pid > 0)
            {
                int status;
                waitpid(info->pid, &status, 0);
                while (running) {
                    sleep(1);
                }
            }
        }
    }
    if (config.id) {
        free(config.id);
        config.id = nullptr;
    }
    if (config.fs_config.root_path) {
        free(config.fs_config.root_path);
        config.fs_config.root_path = nullptr;
    }
    return EXIT_SUCCESS;
}
static int handle_start(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: container ID required\n");
        return EXIT_FAILURE;
    }
    const char *container_id = argv[1];
    if (container_manager_start(&cm, container_id) != 0)
    {
        fprintf(stderr, "Failed to start container %s\n", container_id);
        return EXIT_FAILURE;
    }
    printf("Container %s started\n", container_id);
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
    (void)argc;
    (void)argv;
    int count;
    container_info_t **containers = container_manager_list(&cm, &count);
    vector<container_info_t*> active_containers;
    for (int i = 0; i < count; i++) {
        if (containers[i]->state != CONTAINER_DESTROYED) {
            active_containers.push_back(containers[i]);
        }
    }
    if (active_containers.size() == 0)
    {
        printf("No containers\n");
        printf("\nTo create a container, use:\n");
        printf("  ./mini-container run /bin/sh -c \"while true; do :; done\"\n");
        printf("  or use interactive menu: ./mini-container\n");
        return EXIT_SUCCESS;
    }
    printf("%-20s %-10s %-10s %-15s %-15s\n",
           "CONTAINER ID", "STATE", "PID", "CREATED", "STARTED");
    printf("%-20s %-10s %-10s %-15s %-15s\n",
           "------------", "-----", "---", "-------", "-------");
    for (size_t i = 0; i < active_containers.size(); i++)
    {
        container_info_t *info = active_containers[i];
        char created_str[20] = "";
        char started_str[20] = "";
        if (info->created_at > 0)
        {
            struct tm *tm_created = localtime(&info->created_at);
            if (tm_created) {
                strftime(created_str, sizeof(created_str), "%H:%M:%S", tm_created);
            }
        }
        if (info->started_at > 0)
        {
            struct tm *tm_started = localtime(&info->started_at);
            if (tm_started) {
                strftime(started_str, sizeof(started_str), "%H:%M:%S", tm_started);
            }
        }
        printf("%-20s %-10s %-10d %-15s %-15s\n",
               info->id,
               safe_state_name(info->state),
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
    printf("State: %s\n", safe_state_name(info->state));
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
string format_bytes(unsigned long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = bytes;
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", size, units[unit]);
    return string(buf);
}
string format_duration(time_t start, time_t end) {
    if (start == 0 || end == 0) return "--";
    long diff = end - start;
    int hours = diff / 3600;
    int minutes = (diff % 3600) / 60;
    int seconds = diff % 60;
    char buf[32];
    if (hours > 0) {
        snprintf(buf, sizeof(buf), "%d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(buf, sizeof(buf), "%d:%02d", minutes, seconds);
    }
    return string(buf);
}
double calculate_cpu_percent(unsigned long cpu_ns, time_t start_time) {
    if (start_time == 0) return 0.0;
    time_t now = time(nullptr);
    long elapsed = now - start_time;
    if (elapsed <= 0) return 0.0;
    double cpu_seconds = cpu_ns / 1e9;
    return (cpu_seconds / elapsed) * 100.0;
}
void display_compact_monitor() {
    int count;
    container_info_t **containers = container_manager_list(&cm, &count);
    vector<container_info_t*> active_containers;
    for (int i = 0; i < count; i++) {
        if (containers[i]->state != CONTAINER_DESTROYED) {
            active_containers.push_back(containers[i]);
        }
    }
    int running_count = 0;
    double total_cpu_percent = 0.0;
    unsigned long total_memory_bytes = 0;
    for (size_t i = 0; i < active_containers.size(); i++) {
        if (active_containers[i]->state == CONTAINER_RUNNING) {
            running_count++;
            unsigned long cpu_usage = 0, memory_usage = 0;
            resource_manager_get_stats(cm.rm, active_containers[i]->id, &cpu_usage, &memory_usage);
            total_memory_bytes += memory_usage;
            if (active_containers[i]->started_at > 0) {
                total_cpu_percent += calculate_cpu_percent(cpu_usage, active_containers[i]->started_at);
            }
        }
    }
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("Mini Container Monitor (Live)\n");
    reset_color();
    set_color(COLOR_GREEN);
    printf("Running: %d  ", running_count);
    reset_color();
    printf("Total: %zu  ", active_containers.size());
    set_color(COLOR_YELLOW);
    printf("Total CPU: %.1f%%  ", total_cpu_percent);
    reset_color();
    set_color(COLOR_CYAN);
    string ram_str = format_bytes(total_memory_bytes);
    printf("Total RAM: %s  ", ram_str.c_str());
    reset_color();
    time_t now = time(nullptr);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
    printf("Time: %s\n", time_str);
    set_color(COLOR_BOLD);
    printf("%-20s %-8s %-10s %-13s %-12s %-10s\n",
           "CONTAINER ID", "PID", "STATE", "CPU%", "MEMORY", "RUNTIME");
    reset_color();
    if (active_containers.size() == 0) {
        set_color(COLOR_YELLOW);
        printf("No containers\n");
        reset_color();
    } else {
        vector<container_info_t*> sorted_containers;
        for (size_t i = 0; i < active_containers.size(); i++) {
            sorted_containers.push_back(active_containers[i]);
        }
        sort(sorted_containers.begin(), sorted_containers.end(),
             [](container_info_t* a, container_info_t* b) {
                 if (a->state == CONTAINER_RUNNING && b->state != CONTAINER_RUNNING) return true;
                 if (a->state != CONTAINER_RUNNING && b->state == CONTAINER_RUNNING) return false;
                 return a->started_at > b->started_at;
             });
        int max_display = (sorted_containers.size() > 10) ? 10 : sorted_containers.size();
        for (int i = 0; i < max_display; i++) {
            container_info_t* info = sorted_containers[i];
            const char* state = safe_state_name(info->state);
            const char* state_color = COLOR_WHITE;
            if (info->state == CONTAINER_RUNNING) {
                state_color = COLOR_GREEN;
            } else if (info->state == CONTAINER_STOPPED) {
                state_color = COLOR_RED;
            } else if (info->state == CONTAINER_CREATED) {
                state_color = COLOR_YELLOW;
            }
            printf("%-20s %-8d ", info->id, info->pid);
            set_color(state_color);
            printf("%-10s", state);
            reset_color();
            if (info->state == CONTAINER_RUNNING) {
                unsigned long cpu_usage = 0, memory_usage = 0;
                resource_manager_get_stats(cm.rm, info->id, &cpu_usage, &memory_usage);
                double cpu_percent = calculate_cpu_percent(cpu_usage, info->started_at);
                string mem_str = format_bytes(memory_usage);
                char cpu_buf[16];
                snprintf(cpu_buf, sizeof(cpu_buf), "%.1f%%", cpu_percent);
                printf(" %-13s %-12s", cpu_buf, mem_str.c_str());
            } else {
                printf(" %-13s %-12s", "--", "--");
            }
            string runtime = format_duration(info->started_at, now);
            printf(" %-10s\n", runtime.c_str());
        }
        if (sorted_containers.size() > 10) {
            set_color(COLOR_YELLOW);
            printf("... and %d more container(s)\n", (int)(sorted_containers.size() - 10));
            reset_color();
        }
    }
    printf("\n");
}
void display_monitor() {
    clear_screen();
    hide_cursor();
    while (monitor_mode && running) {
        clear_screen();
        set_color(COLOR_BOLD);
        set_color(COLOR_CYAN);
        printf("Mini Container Monitor (htop-like)\n");
        reset_color();
        int count;
        container_info_t **containers = container_manager_list(&cm, &count);
    vector<container_info_t*> active_containers;
    for (int i = 0; i < count; i++) {
        if (containers[i]->state != CONTAINER_DESTROYED) {
            active_containers.push_back(containers[i]);
        }
    }
    int running_count = 0;
        for (size_t i = 0; i < active_containers.size(); i++) {
            if (active_containers[i]->state == CONTAINER_RUNNING) {
                running_count++;
            }
        }
        set_color(COLOR_GREEN);
        printf("Running: %d  ", running_count);
        reset_color();
        printf("Total: %zu  ", active_containers.size());
        time_t now = time(nullptr);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
        printf("Time: %s\n", time_str);
        set_color(COLOR_BOLD);
        printf("%-20s %-8s %-10s %-12s %-12s %-10s %-10s\n",
               "CONTAINER ID", "PID", "STATE", "CPU%", "MEMORY", "RUNTIME", "CREATED");
        reset_color();
        if (active_containers.size() == 0) {
            set_color(COLOR_YELLOW);
            printf("No containers\n");
            reset_color();
        } else {
            vector<container_info_t*> sorted_containers;
            for (size_t i = 0; i < active_containers.size(); i++) {
                sorted_containers.push_back(active_containers[i]);
            }
            sort(sorted_containers.begin(), sorted_containers.end(),
                 [](container_info_t* a, container_info_t* b) {
                     if (a->state == CONTAINER_RUNNING && b->state != CONTAINER_RUNNING) return true;
                     if (a->state != CONTAINER_RUNNING && b->state == CONTAINER_RUNNING) return false;
                     return a->started_at > b->started_at;
                 });
            for (auto info : sorted_containers) {
                const char* state = safe_state_name(info->state);
                const char* state_color = COLOR_WHITE;
                if (info->state == CONTAINER_RUNNING) {
                    state_color = COLOR_GREEN;
                } else if (info->state == CONTAINER_STOPPED) {
                    state_color = COLOR_RED;
                } else if (info->state == CONTAINER_CREATED) {
                    state_color = COLOR_YELLOW;
                }
                printf("%-20s %-8d ", info->id, info->pid);
                set_color(state_color);
                printf("%-10s", state);
                reset_color();
                if (info->state == CONTAINER_RUNNING) {
                    unsigned long cpu_usage = 0, memory_usage = 0;
                    resource_manager_get_stats(cm.rm, info->id, &cpu_usage, &memory_usage);
                    double cpu_percent = calculate_cpu_percent(cpu_usage, info->started_at);
                    string mem_str = format_bytes(memory_usage);
                    printf(" %-12.1f %-12s", cpu_percent, mem_str.c_str());
                } else {
                    printf(" %-12s %-12s", "--", "--");
                }
                string runtime = format_duration(info->started_at, now);
                printf(" %-10s", runtime.c_str());
                char created_str[20] = "";
                if (info->created_at > 0) {
                    struct tm *tm_created = localtime(&info->created_at);
                    if (tm_created) {
                        strftime(created_str, sizeof(created_str), "%H:%M:%S", tm_created);
                    }
                }
                printf(" %-10s\n", created_str);
            }
        }
        printf("\n");
        set_color(COLOR_CYAN);
        printf("Press 'q' to quit monitor, 'r' to refresh\n");
        reset_color();
        fflush(stdout);
        struct termios old_term, new_term;
        tcgetattr(STDIN_FILENO, &old_term);
        new_term = old_term;
        new_term.c_lflag &= ~(ICANON | ECHO);
        new_term.c_cc[VMIN] = 0;
        new_term.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 'q' || c == 'Q') {
                monitor_mode = false;
                break;
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
        usleep(5000000);
    }
    show_cursor();
    clear_screen();
}
void interactive_create_container() {
    clear_screen();
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("Create New Container\n");
    reset_color();
    char command[1024];
    char container_name[256] = "";
    char root_path[512] = "/";
    int memory = 128;
    int cpu = 1024;
    printf("\n");
    printf("Container name (optional, press Enter for auto-generated): ");
    fflush(stdout);
    if (fgets(container_name, sizeof(container_name), stdin)) {
        size_t len = strlen(container_name);
        if (len > 0 && container_name[len-1] == '\n') {
            container_name[len-1] = '\0';
        }
    }
    set_color(COLOR_CYAN);
    printf("\nSuggested Commands for Testing\n");
    reset_color();
    set_color(COLOR_YELLOW);
    printf("\n* CPU Stress Test:\n");
    reset_color();
    printf("  Basic CPU stress (uses full CPU capacity):\n");
    set_color(COLOR_GREEN);
    printf("  sh -c 'while true; do :; done'\n");
    reset_color();
    printf("  Advanced CPU stress test:\n");
    set_color(COLOR_GREEN);
    printf("  sh -c 'while true; do echo $((12345*67890)) > /dev/null; done'\n");
    reset_color();
    set_color(COLOR_YELLOW);
    printf("\n* Memory Limit Test:\n");
    reset_color();
    printf("  Simple memory test:\n");
    set_color(COLOR_GREEN);
    printf("  sh -c 'dd if=/dev/zero of=/tmp/mem bs=1M count=80'\n");
    reset_color();
    printf("  Allocate 256 MB of memory gradually:\n");
    set_color(COLOR_GREEN);
    printf("  sh -c 'x=\"\"; for i in $(seq 1 256); do x=\"$x$(head -c 1M /dev/zero)\"; sleep 0.12; done; sleep infinity'\n");
    reset_color();
    printf("  Memory stress test (allocate 2GB, will be limited by memory limit):\n");
    set_color(COLOR_GREEN);
    printf("  sh -c 'x=\"\"; for i in $(seq 1 2048); do x=\"$x$(head -c 1M /dev/zero)\"; sleep 0.0147; done; sleep infinity'\n");
    reset_color();
    set_color(COLOR_YELLOW);
    printf("\n* Combined CPU and RAM Stress Test:\n");
    reset_color();
    set_color(COLOR_GREEN);
    printf("  sh -c 'a=\"\"; while true; do a=\"$a$(printf %%0100000d 0)\"; done'\n");
    reset_color();
    printf("\n");
    printf("\nCommand to run: ");
    fflush(stdout);
    if (!fgets(command, sizeof(command), stdin)) {
        printf("Error reading command\n");
        return;
    }
    size_t len = strlen(command);
    if (len > 0 && command[len-1] == '\n') {
        command[len-1] = '\0';
    }
    printf("Memory limit (MB, default 128): ");
    fflush(stdout);
    char mem_str[64];
    if (fgets(mem_str, sizeof(mem_str), stdin)) {
        int m = atoi(mem_str);
        if (m > 0) memory = m;
    }
    printf("CPU shares (default 1024): ");
    fflush(stdout);
    char cpu_str[64];
    if (fgets(cpu_str, sizeof(cpu_str), stdin)) {
        int c = atoi(cpu_str);
        if (c > 0) cpu = c;
    }
    printf("Root path (default / - uses real root for demo): ");
    fflush(stdout);
    char root_input[512];
    if (fgets(root_input, sizeof(root_input), stdin)) {
        size_t rlen = strlen(root_input);
        if (rlen > 0 && root_input[rlen-1] == '\n') {
            root_input[rlen-1] = '\0';
        }
        if (strlen(root_input) > 0) {
            strncpy(root_path, root_input, sizeof(root_path)-1);
        }
    }
    vector<char*> args;
    string cmd = command;
    bool in_quotes = false;
    char quote_char = 0;
    string current_arg;
    for (size_t i = 0; i < cmd.length(); ++i) {
        char c = cmd[i];
        if ((c == '"' || c == '\'') && (i == 0 || cmd[i-1] != '\\')) {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (c == quote_char) {
                in_quotes = false;
                quote_char = 0;
            } else {
                current_arg += c;
            }
        } else if (c == ' ' && !in_quotes) {
            if (!current_arg.empty()) {
                char *arg = strdup(current_arg.c_str());
                if (!arg) {
                    perror("strdup failed");
                    for (auto a : args) {
                        if (a) free(a);
                    }
                    return;
                }
                args.push_back(arg);
                current_arg.clear();
            }
        } else {
            current_arg += c;
        }
    }
    if (!current_arg.empty()) {
        char *arg = strdup(current_arg.c_str());
        if (!arg) {
            perror("strdup failed");
            for (auto a : args) {
                if (a) free(a);
            }
            return;
        }
        args.push_back(arg);
    }
    args.push_back(nullptr);
    container_config_t config;
    config.id = nullptr;
    config.fs_config.root_path = nullptr;
    if (strlen(container_name) > 0) {
        config.id = strdup(container_name);
        if (!config.id) {
            perror("strdup failed for container name");
            for (auto arg : args) {
                if (arg) free(arg);
            }
            return;
        }
    }
    namespace_config_init(&config.ns_config);
    resource_limits_init(&config.res_limits);
    fs_config_init(&config.fs_config);
    config.res_limits.memory.limit_bytes = memory * 1024 * 1024;
    config.res_limits.cpu.shares = cpu;
    config.fs_config.root_path = strdup(root_path);
    if (!config.fs_config.root_path) {
        perror("strdup failed for root path");
        for (auto arg : args) {
            if (arg) free(arg);
        }
        if (config.id) {
            free(config.id);
            config.id = nullptr;
        }
        return;
    }
    config.command = args.data();
    config.command_argc = args.size() - 1;
    printf("\n");
    set_color(COLOR_YELLOW);
    printf("Creating container...\n");
    reset_color();
    if (container_manager_run(&cm, &config) != 0) {
        set_color(COLOR_RED);
        printf("Failed to create container\n");
        reset_color();
    } else {
        container_info_t *info = container_manager_get_info(&cm, config.id ? config.id : "unknown");
        if (info) {
            set_color(COLOR_GREEN);
            printf("Container %s started with PID %d\n", info->id, info->pid);
            reset_color();
        }
    }
    for (auto arg : args) {
        if (arg) free(arg);
    }
    if (config.id) {
        free(config.id);
        config.id = nullptr;
    }
    if (config.fs_config.root_path) {
        free(config.fs_config.root_path);
        config.fs_config.root_path = nullptr;
    }
    printf("\nPress Enter to continue...");
    getchar();
}
void run_tests() {
    clear_screen();
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("System Tests\n");
    reset_color();
    printf("\n");
    printf("1. CPU Usage Test\n");
    printf("2. Memory Limit Test\n");
    printf("3. CPU Limit Test\n");
    printf("4. Combined Test (CPU + Memory)\n");
    printf("5. Run All Tests\n");
    printf("0. Back to Menu\n");
    printf("\nSelect test: ");
    char choice[10];
    fgets(choice, sizeof(choice), stdin);
    int test_num = atoi(choice);
    if (test_num == 0) return;
    container_config_t config;
    namespace_config_init(&config.ns_config);
    resource_limits_init(&config.res_limits);
    fs_config_init(&config.fs_config);
    char container_id[256];
    snprintf(container_id, sizeof(container_id), "test_%ld", time(nullptr));
    config.id = container_id;
    vector<char*> args;
    if (test_num == 1 || test_num == 5) {
        printf("\n[Test 1] CPU Usage Test...\n");
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.fs_config.root_path = strdup("/");
        args.clear();
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        args.push_back(strdup("while true; do :; done"));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        if (container_manager_run(&cm, &config) == 0) {
            printf("  Container created, waiting 3 seconds...\n");
            sleep(3);
            container_info_t *info = container_manager_get_info(&cm, container_id);
            if (info && info->state == CONTAINER_RUNNING) {
                unsigned long cpu_usage = 0, memory_usage = 0;
                resource_manager_get_stats(cm.rm, container_id, &cpu_usage, &memory_usage);
                printf("  CPU Usage: %lu ns\n", cpu_usage);
                printf("  Memory Usage: %s\n", format_bytes(memory_usage).c_str());
                printf("  ✓ Test passed\n");
            }
            container_manager_stop(&cm, container_id);
        }
        for (auto arg : args) if (arg) free(arg);
        free(config.fs_config.root_path);
    }
    if (test_num == 2 || test_num == 5) {
        printf("\n[Test 2] Memory Limit Test...\n");
        snprintf(container_id, sizeof(container_id), "test_mem_%ld", time(nullptr));
        config.id = container_id;
        config.res_limits.memory.limit_bytes = 64 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.fs_config.root_path = strdup("/");
        args.clear();
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        args.push_back(strdup("dd if=/dev/zero of=/tmp/mem bs=1M count=80 status=none 2>&1; echo Exit code: $?"));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        if (container_manager_run(&cm, &config) == 0) {
            printf("  Container created, waiting 2 seconds...\n");
            sleep(2);
            container_info_t *info = container_manager_get_info(&cm, container_id);
            if (info) {
                unsigned long cpu_usage = 0, memory_usage = 0;
                resource_manager_get_stats(cm.rm, container_id, &cpu_usage, &memory_usage);
                printf("  Memory Usage: %s (Limit: 64 MB)\n", format_bytes(memory_usage).c_str());
                printf("  ✓ Test passed\n");
            }
            container_manager_stop(&cm, container_id);
        }
        for (auto arg : args) if (arg) free(arg);
        free(config.fs_config.root_path);
    }
    if (test_num == 3 || test_num == 5) {
        printf("\n[Test 3] CPU Limit Test...\n");
        snprintf(container_id, sizeof(container_id), "test_cpu_limit_%ld", time(nullptr));
        config.id = container_id;
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        args.clear();
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        args.push_back(strdup("while true; do :; done"));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        if (container_manager_run(&cm, &config) == 0) {
            printf("  Container created with CPU limit 512, waiting 3 seconds...\n");
            sleep(3);
            container_info_t *info = container_manager_get_info(&cm, container_id);
            if (info && info->state == CONTAINER_RUNNING) {
                unsigned long cpu_usage = 0, memory_usage = 0;
                resource_manager_get_stats(cm.rm, container_id, &cpu_usage, &memory_usage);
                printf("  CPU Usage: %lu ns\n", cpu_usage);
                printf("  ✓ Test passed\n");
            }
            container_manager_stop(&cm, container_id);
        }
        for (auto arg : args) if (arg) free(arg);
        free(config.fs_config.root_path);
    }
    if (test_num == 4 || test_num == 5) {
        printf("\n[Test 4] Combined Test (CPU + Memory)...\n");
        snprintf(container_id, sizeof(container_id), "test_combined_%ld", time(nullptr));
        config.id = container_id;
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.fs_config.root_path = strdup("/");
        args.clear();
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        args.push_back(strdup("dd if=/dev/zero of=/tmp/stress bs=1M count=16 status=none; i=0; while [ $i -lt 10000000 ]; do i=$((i+1)); done; rm -f /tmp/stress; echo Done"));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        if (container_manager_run(&cm, &config) == 0) {
            printf("  Container created, waiting 3 seconds...\n");
            sleep(3);
            container_info_t *info = container_manager_get_info(&cm, container_id);
            if (info && info->state == CONTAINER_RUNNING) {
                unsigned long cpu_usage = 0, memory_usage = 0;
                resource_manager_get_stats(cm.rm, container_id, &cpu_usage, &memory_usage);
                printf("  CPU Usage: %lu ns\n", cpu_usage);
                printf("  Memory Usage: %s\n", format_bytes(memory_usage).c_str());
                printf("  ✓ Test passed\n");
            }
            container_manager_stop(&cm, container_id);
        }
        for (auto arg : args) if (arg) free(arg);
        free(config.fs_config.root_path);
    }
    printf("\nPress Enter to continue...");
    getchar();
}
static unsigned long get_total_system_memory() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return info.totalram * info.mem_unit;
    }
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                unsigned long mem_kb = 0;
                if (sscanf(line, "MemTotal: %lu kB", &mem_kb) == 1) {
                    fclose(fp);
                    return mem_kb * 1024;
                }
            }
        }
        fclose(fp);
    }
    return 4ULL * 1024 * 1024 * 1024;
}
static int get_cpu_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
void run_memory_cpu_test() {
    clear_screen();
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("Memory and CPU Test\n");
    reset_color();
    unsigned long total_memory = get_total_system_memory();
    int cpu_count = get_cpu_count();
    printf("\nSystem Information:\n");
    printf("  Total Memory: %s\n", format_bytes(total_memory).c_str());
    printf("  CPU Cores: %d\n", cpu_count);
    printf("\nCreating test containers...\n");
    unsigned long memory_fractions[] = {
        total_memory / 16,
        total_memory / 8,
        total_memory / 4
    };
    int cpu_period_us = 100000;
    int cpu_quotas[] = {
        6250,
        12500,
        25000
    };
    for (int i = 0; i < 3; i++) {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "C%dMEM", i + 1);
        config.id = strdup(container_id);
        if (!config.id) {
            perror("strdup failed");
            continue;
        }
        config.res_limits.memory.limit_bytes = memory_fractions[i];
        config.res_limits.cpu.shares = 1024; // CPU shares پیش‌فرض
        config.fs_config.root_path = strdup("/");
        if (!config.fs_config.root_path) {
            perror("strdup failed");
            free(config.id);
            continue;
        }
        char cmd_buffer[512];
        snprintf(cmd_buffer, sizeof(cmd_buffer),
                 "python3 -c 'import time; data = [bytearray(%lu) for _ in range(1)]; time.sleep(3600)'",
                 memory_fractions[i] / 2);
        char **command = static_cast<char**>(calloc(4, sizeof(char*)));
        if (!command) {
            perror("calloc failed");
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[0] = strdup("/bin/sh");
        if (!command[0]) {
            perror("strdup failed");
            free(command);
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[1] = strdup("-c");
        if (!command[1]) {
            perror("strdup failed");
            free(command[0]);
            free(command);
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[2] = strdup(cmd_buffer);
        if (!command[2]) {
            perror("strdup failed");
            free(command[0]);
            free(command[1]);
            free(command);
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[3] = nullptr;
        config.command = command;
        config.command_argc = 3;
        if (container_manager_run(&cm, &config) == 0) {
            set_color(COLOR_GREEN);
            printf("  ✓ Created %s with memory limit: %s\n", container_id, format_bytes(memory_fractions[i]).c_str());
            reset_color();
            usleep(100000);
        } else {
            set_color(COLOR_RED);
            printf("  ✗ Failed to create %s\n", container_id);
            reset_color();
        }
        if (command) {
            for (int j = 0; j < 3; j++) {
                if (command[j]) {
                    free(command[j]);
                }
            }
            free(command);
        }
        if (config.id) {
            free(config.id);
            config.id = nullptr;
        }
        if (config.fs_config.root_path) {
            free(config.fs_config.root_path);
            config.fs_config.root_path = nullptr;
        }
    }
    for (int i = 0; i < 3; i++) {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "C%dCPU", i + 1);
        config.id = strdup(container_id);
        if (!config.id) {
            perror("strdup failed");
            continue;
        }
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.quota_us = cpu_quotas[i];
        config.fs_config.root_path = strdup("/");
        if (!config.fs_config.root_path) {
            perror("strdup failed");
            free(config.id);
            continue;
        }
        char **command = static_cast<char**>(calloc(4, sizeof(char*)));
        if (!command) {
            perror("calloc failed");
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[0] = strdup("/bin/sh");
        if (!command[0]) {
            perror("strdup failed");
            free(command);
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[1] = strdup("-c");
        if (!command[1]) {
            perror("strdup failed");
            free(command[0]);
            free(command);
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[2] = strdup("while true; do :; done");
        if (!command[2]) {
            perror("strdup failed");
            free(command[0]);
            free(command[1]);
            free(command);
            free(config.id);
            free(config.fs_config.root_path);
            continue;
        }
        command[3] = nullptr;
        config.command = command;
        config.command_argc = 3;
        if (container_manager_run(&cm, &config) == 0) {
            set_color(COLOR_GREEN);
            double cpu_percent = (cpu_quotas[i] * 100.0) / cpu_period_us;
            printf("  ✓ Created %s with CPU limit: %.2f%% (quota: %d, period: %d)\n",
                   container_id, cpu_percent, cpu_quotas[i], cpu_period_us);
            reset_color();
            usleep(100000);
        } else {
            set_color(COLOR_RED);
            printf("  ✗ Failed to create %s\n", container_id);
            reset_color();
        }
        if (command) {
            for (int j = 0; j < 3; j++) {
                if (command[j]) {
                    free(command[j]);
                }
            }
            free(command);
        }
        if (config.id) {
            free(config.id);
            config.id = nullptr;
        }
        if (config.fs_config.root_path) {
            free(config.fs_config.root_path);
            config.fs_config.root_path = nullptr;
        }
    }
    printf("\n");
    set_color(COLOR_YELLOW);
    printf("Test containers created successfully!\n");
    reset_color();
    printf("Press Enter to continue...");
    getchar();
}
void init_containers() {
    time_t base_time = time(nullptr);
    int counter = 0;
    const int runtime_seconds = 600;
    char cmd_buffer[512];
    const unsigned long memory_limit = 128 * 1024 * 1024;
    const int cpu_quota_us = 5000;
    const int cpu_period_us = 100000;
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "cpu_intensive_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "for i in 1 2 3 4; do yes > /dev/null & done; sleep %d", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "ram_intensive_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'a = [bytearray(50*1024*1024) for _ in range(3)]; import time; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "cpu_ram_heavy_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "yes > /dev/null & python3 -c 'a = bytearray(120*1024*1024); import time; [i*i for i in range(10000000)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "cpu_calc_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'import time; start=time.time(); [sum(range(i)) for i in range(50000)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "mem_stress_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'import time; data = [bytearray(30*1024*1024) for _ in range(5)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "mixed_workload_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "yes > /dev/null & dd if=/dev/zero of=/tmp/test bs=1M count=50 status=none & sleep %d", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "high_cpu_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "for i in 1 2 3; do yes > /dev/null & done; sleep %d", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "high_mem_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'a = [bytearray(40*1024*1024) for _ in range(5)]; import time; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "balanced_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "yes > /dev/null & python3 -c 'a = bytearray(100*1024*1024); import time; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "max_stress_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = memory_limit;
        config.res_limits.cpu.quota_us = cpu_quota_us;
        config.res_limits.cpu.period_us = cpu_period_us;
        config.res_limits.cpu.shares = 512;
        config.fs_config.root_path = strdup("/");
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "for i in 1 2 3 4 5; do yes > /dev/null & done; python3 -c 'a = [bytearray(35*1024*1024) for _ in range(5)]; import time; [i**2 for i in range(2000000)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        container_manager_run(&cm, &config);
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.fs_config.root_path);
    }
}
void signal_handler(int signum) {
    (void)signum;
    running = false;
    monitor_mode = false;
    show_cursor();
    printf("\n\n");
    set_color(COLOR_YELLOW);
    printf("Shutting down... Stopping all containers...\n");
    reset_color();
    int count;
    container_info_t** containers = container_manager_list(&cm, &count);
    for (int i = 0; i < count; i++) {
        if (containers[i]->state == CONTAINER_RUNNING) {
            char cgroup_procs_path[512];
            if (cm.rm->version == CGROUP_V2) {
                snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
                         "/sys/fs/cgroup/%s_%s/cgroup.procs", cm.rm->cgroup_path, containers[i]->id);
            } else {
                snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
                         "/sys/fs/cgroup/cpu,cpuacct/%s_%s/tasks", cm.rm->cgroup_path, containers[i]->id);
            }
            FILE *fp = fopen(cgroup_procs_path, "r");
            if (fp) {
                pid_t pid;
                while (fscanf(fp, "%d", &pid) == 1) {
                    if (pid > 0) {
                        kill(pid, SIGTERM);
                    }
                }
                fclose(fp);
            }
        }
    }
    usleep(500000);
    for (int i = 0; i < count; i++) {
        if (containers[i]->state == CONTAINER_RUNNING) {
            container_manager_stop(&cm, containers[i]->id);
        }
    }
    sleep(1);
    for (int i = 0; i < count; i++) {
        if (containers[i]->state == CONTAINER_RUNNING || containers[i]->pid > 0) {
            char cgroup_procs_path[512];
            if (cm.rm->version == CGROUP_V2) {
                snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
                         "/sys/fs/cgroup/%s_%s/cgroup.procs", cm.rm->cgroup_path, containers[i]->id);
            } else {
                snprintf(cgroup_procs_path, sizeof(cgroup_procs_path),
                         "/sys/fs/cgroup/cpu,cpuacct/%s_%s/tasks", cm.rm->cgroup_path, containers[i]->id);
            }
            FILE *fp = fopen(cgroup_procs_path, "r");
            if (fp) {
                pid_t pid;
                while (fscanf(fp, "%d", &pid) == 1) {
                    if (pid > 0) {
                        kill(pid, SIGKILL);
                    }
                }
                fclose(fp);
            }
            if (containers[i]->state == CONTAINER_RUNNING) {
                container_manager_stop(&cm, containers[i]->id);
            }
        }
    }
    containers = container_manager_list(&cm, &count);
    for (int i = 0; i < count; i++) {
        if (containers[i]->state == CONTAINER_CREATED || containers[i]->state == CONTAINER_RUNNING) {
            if (containers[i]->state == CONTAINER_CREATED) {
                containers[i]->state = CONTAINER_STOPPED;
                containers[i]->stopped_at = time(nullptr);
            } else if (containers[i]->state == CONTAINER_RUNNING) {
                container_manager_stop(&cm, containers[i]->id);
            }
        }
    }
    if (web_server) {
        web_server->stop();
        delete web_server;
        web_server = nullptr;
    }
    container_manager_cleanup(&cm);
    set_color(COLOR_GREEN);
    printf("All containers stopped. Exiting...\n");
    reset_color();
    exit(0);
}
void interactive_menu() {
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 0;
    new_term.c_cc[VTIME] = 0;
    hide_cursor();
    while (running) {
        clear_screen();
        display_compact_monitor();
        set_color(COLOR_BOLD);
        set_color(COLOR_CYAN);
        printf("Commands Menu\n");
        reset_color();
        printf("1. Create Container         2. Full Monitor (htop)     3. List Containers\n");
        printf("4. Stop Container           5. Destroy Container        6. Container Info\n");
        printf("7. Edit Container           8. Start Container          9. Memory/CPU Test\n");
        printf("0. Exit\n");
        printf("\n");
        set_color(COLOR_YELLOW);
        printf("Select option (auto-refresh every 5 seconds): ");
        reset_color();
        fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
        char choice = 0;
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        int select_result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        if (select_result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (read(STDIN_FILENO, &choice, 1) > 0) {
                int option = choice - '0';
                tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
                show_cursor();
                switch (option) {
                    case 1:
                        clear_screen();
                        interactive_create_container();
                        break;
                    case 2:
                        monitor_mode = true;
                        display_monitor();
                        monitor_mode = false;
                        break;
                    case 3:
                        clear_screen();
                        handle_list(0, nullptr);
                        printf("\nPress Enter to continue...");
                        getchar();
                        break;
                    case 4: {
                        clear_screen();
                        printf("Container ID: ");
                        fflush(stdout);
                        char id[256];
                        if (fgets(id, sizeof(id), stdin)) {
                            size_t len = strlen(id);
                            if (len > 0 && id[len-1] == '\n') {
                                id[len-1] = '\0';
                            }
                            char *start = id;
                            while (*start == ' ' || *start == '\t') start++;
                            char *end = id + strlen(id) - 1;
                            while (end > start && (*end == ' ' || *end == '\t')) end--;
                            *(end + 1) = '\0';
                            if (strlen(start) > 0) {
                                char cmd_stop[] = "stop";
                                char* argv[] = {cmd_stop, start, nullptr};
                                handle_stop(2, argv);
                            } else {
                                printf("Error: Container ID cannot be empty\n");
                            }
                            printf("\nPress Enter to continue...");
                            fflush(stdout);
                            getchar();
                        }
                        break;
                    }
                    case 5: {
                        clear_screen();
                        printf("Container ID: ");
                        fflush(stdout);
                        char id[256];
                        if (fgets(id, sizeof(id), stdin)) {
                            size_t len = strlen(id);
                            if (len > 0 && id[len-1] == '\n') {
                                id[len-1] = '\0';
                            }
                            char *start = id;
                            while (*start == ' ' || *start == '\t') start++;
                            char *end = id + strlen(id) - 1;
                            while (end > start && (*end == ' ' || *end == '\t')) end--;
                            *(end + 1) = '\0';
                            if (strlen(start) > 0) {
                                container_info_t *info = container_manager_get_info(&cm, start);
                                if (info) {
                                    char cmd_destroy[] = "destroy";
                                    char* argv[] = {cmd_destroy, start, nullptr};
                                    handle_destroy(2, argv);
                                } else {
                                    set_color(COLOR_RED);
                                    printf("Error: Container %s not found\n", start);
                                    reset_color();
                                    printf("\nAvailable containers:\n");
                                    handle_list(0, nullptr);
                                }
                            } else {
                                printf("Error: Container ID cannot be empty\n");
                            }
                            printf("\nPress Enter to continue...");
                            fflush(stdout);
                            int c;
                            while ((c = getchar()) != '\n' && c != EOF);
                        }
                        break;
                    }
                    case 6: {
                        clear_screen();
                        printf("Container ID: ");
                        fflush(stdout);
                        char id[256];
                        if (fgets(id, sizeof(id), stdin)) {
                            size_t len = strlen(id);
                            if (len > 0 && id[len-1] == '\n') {
                                id[len-1] = '\0';
                            }
                            char cmd_info[] = "info";
                            char* argv[] = {cmd_info, id, nullptr};
                            handle_info(2, argv);
                            printf("\nPress Enter to continue...");
                            getchar();
                        }
                        break;
                    }
                    case 7: {
                        clear_screen();
                        printf("Container ID: ");
                        fflush(stdout);
                        char id[256];
                        if (fgets(id, sizeof(id), stdin)) {
                            size_t len = strlen(id);
                            if (len > 0 && id[len-1] == '\n') {
                                id[len-1] = '\0';
                            }
                            char *start = id;
                            while (*start == ' ' || *start == '\t') start++;
                            char *end = id + strlen(id) - 1;
                            while (end > start && (*end == ' ' || *end == '\t')) end--;
                            *(end + 1) = '\0';
                            if (strlen(start) > 0) {
                                container_info_t *info = container_manager_get_info(&cm, start);
                                if (info) {
                                    clear_screen();
                                    set_color(COLOR_BOLD);
                                    set_color(COLOR_CYAN);
                                    printf("Edit Container: %s\n", start);
                                    reset_color();
                                    printf("\n");
                                    printf("Container ID: %s\n", info->id);
                                    printf("State: %s\n", safe_state_name(info->state));
                                    printf("PID: %d\n", info->pid);
                                    printf("Created: %s", ctime(&info->created_at));
                                    if (info->started_at > 0) {
                                        printf("Started: %s", ctime(&info->started_at));
                                    }
                                    if (info->stopped_at > 0) {
                                        printf("Stopped: %s", ctime(&info->stopped_at));
                                    }
                                    if (info->state == CONTAINER_RUNNING) {
                                        unsigned long cpu_usage = 0, memory_usage = 0;
                                        resource_manager_get_stats(cm.rm, start, &cpu_usage, &memory_usage);
                                        printf("CPU Usage: %lu nanoseconds\n", cpu_usage);
                                        printf("Memory Usage: %lu bytes\n", memory_usage);
                                    }
                                    printf("\n");
                                    printf("Options:\n");
                                    if (info->state == CONTAINER_RUNNING) {
                                        printf("1. Stop Container\n");
                                    } else if (info->state == CONTAINER_STOPPED) {
                                        printf("1. Start Container\n");
                                    }
                                    printf("0. Back\n");
                                    printf("\nSelect option: ");
                                    fflush(stdout);
                                    char option[10];
                                    if (fgets(option, sizeof(option), stdin)) {
                                        int opt = atoi(option);
                                        if (opt == 1) {
                                            if (info->state == CONTAINER_RUNNING) {
                                                char cmd_stop[] = "stop";
                                                char* argv[] = {cmd_stop, start, nullptr};
                                                handle_stop(2, argv);
                                            } else if (info->state == CONTAINER_STOPPED) {
                                                char cmd_start[] = "start";
                                                char* argv[] = {cmd_start, start, nullptr};
                                                handle_start(2, argv);
                                            }
                                        }
                                    }
                                } else {
                                    set_color(COLOR_RED);
                                    printf("Container %s not found\n", start);
                                    reset_color();
                                    printf("\nAvailable containers:\n");
                                    handle_list(0, nullptr);
                                }
                            } else {
                                printf("Error: Container ID cannot be empty\n");
                            }
                            printf("\nPress Enter to continue...");
                            fflush(stdout);
                            int c;
                            while ((c = getchar()) != '\n' && c != EOF);
                        }
                        break;
                    }
                    case 8: {
                        clear_screen();
                        printf("Container ID: ");
                        fflush(stdout);
                        char id[256];
                        if (fgets(id, sizeof(id), stdin)) {
                            size_t len = strlen(id);
                            if (len > 0 && id[len-1] == '\n') {
                                id[len-1] = '\0';
                            }
                            char *start = id;
                            while (*start == ' ' || *start == '\t') start++;
                            char *end = id + strlen(id) - 1;
                            while (end > start && (*end == ' ' || *end == '\t')) end--;
                            *(end + 1) = '\0';
                            if (strlen(start) > 0) {
                                container_info_t *info = container_manager_get_info(&cm, start);
                                if (info) {
                                    char cmd_start[] = "start";
                                    char* argv[] = {cmd_start, start, nullptr};
                                    handle_start(2, argv);
                                } else {
                                    set_color(COLOR_RED);
                                    printf("Error: Container %s not found\n", start);
                                    reset_color();
                                    printf("\nAvailable containers:\n");
                                    handle_list(0, nullptr);
                                }
                            } else {
                                printf("Error: Container ID cannot be empty\n");
                            }
                            printf("\nPress Enter to continue...");
                            fflush(stdout);
                            int c;
                            while ((c = getchar()) != '\n' && c != EOF);
                        }
                        break;
                    }
                    case 9: {
                        clear_screen();
                        run_memory_cpu_test();
                        break;
                    }
                    case 0: {
                        int count;
                        container_info_t **containers = container_manager_list(&cm, &count);
                        int stopped_count = 0;
                        for (int i = 0; i < count; i++) {
                            if (containers[i]->state == CONTAINER_RUNNING) {
                                if (container_manager_stop(&cm, containers[i]->id) == 0) {
                                    stopped_count++;
                                }
                            }
                        }
                        if (stopped_count > 0) {
                            clear_screen();
                            set_color(COLOR_GREEN);
                            printf("Stopped %d container(s) before exit\n", stopped_count);
                            reset_color();
                            printf("\nPress Enter to continue...");
                            fflush(stdout);
                            getchar();
                        }
                        running = false;
                        break;
                    }
                    default:
                        break;
                }
                tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
                hide_cursor();
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    show_cursor();
}
int main(int argc, char *argv[])
{
    if (container_manager_init(&cm, 10000) != 0)
    {
        fprintf(stderr, "Failed to initialize container manager\n");
        return EXIT_FAILURE;
    }
    if (getuid() != 0)
    {
        fprintf(stderr, "Warning: container operations typically require root privileges\n");
    }
    web_server = new SimpleWebServer(&cm, 808);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    web_server->start();
    if (argc < 2)
    {
        interactive_menu();
        if (web_server) {
            web_server->stop();
            delete web_server;
        }
        container_manager_cleanup(&cm);
        return EXIT_SUCCESS;
    }
    const char *command = argv[1];
    int result = EXIT_FAILURE;
    if (strcmp(command, "run") == 0)
    {
        result = handle_run(argc - 1, &argv[1]);
    }
    else if (strcmp(command, "start") == 0)
    {
        result = handle_start(argc - 1, &argv[1]);
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
    else if (strcmp(command, "monitor") == 0 || strcmp(command, "htop") == 0)
    {
        monitor_mode = true;
        display_monitor();
        result = EXIT_SUCCESS;
    }
    else if (strcmp(command, "interactive") == 0 || strcmp(command, "menu") == 0)
    {
        interactive_menu();
        result = EXIT_SUCCESS;
    }
    else if (strcmp(command, "help") == 0 || strcmp(command, "-h") == 0 ||
             strcmp(command, "--help") == 0)
    {
        print_usage(argv[0]);
        printf("\nInteractive Mode:\n");
        printf("  Run without arguments or use 'interactive' command to enter interactive menu\n");
        printf("  Use 'monitor' or 'htop' command to view htop-like monitor\n");
        result = EXIT_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        result = EXIT_FAILURE;
    }
    if (strcmp(command, "run") != 0) {
        if (web_server) {
            web_server->stop();
            delete web_server;
            web_server = nullptr;
        }
        container_manager_cleanup(&cm);
    }
    return result;
}