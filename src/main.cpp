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
        {"hostname", required_argument, 0, 'n'},
        {"detach", no_argument, 0, 'd'},
        {0, 0, 0, 0}};

    int option_index = 0;
    int c;

    config->id = nullptr;
    *detach = 0;
    namespace_config_init(&config->ns_config);
    resource_limits_init(&config->res_limits);
    fs_config_init(&config->fs_config);

    while ((c = getopt_long(argc, argv, "hm:c:r:n:d", long_options, &option_index)) != -1)
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

        case 'd':
            *detach = 1;
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
        config->fs_config.root_path = strdup("/");
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
    (void)argc;  // Suppress unused parameter warning
    (void)argv;  // Suppress unused parameter warning
    int count;
    container_info_t **containers = container_manager_list(&cm, &count);

    if (count == 0)
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

    for (int i = 0; i < count; i++)
    {
        container_info_t *info = containers[i];
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
    
    int running_count = 0;
    double total_cpu_percent = 0.0;
    unsigned long total_memory_bytes = 0;
    
    for (int i = 0; i < count; i++) {
        if (containers[i]->state == CONTAINER_RUNNING) {
            running_count++;
            unsigned long cpu_usage = 0, memory_usage = 0;
            resource_manager_get_stats(cm.rm, containers[i]->id, &cpu_usage, &memory_usage);
            total_memory_bytes += memory_usage;
            if (containers[i]->started_at > 0) {
                total_cpu_percent += calculate_cpu_percent(cpu_usage, containers[i]->started_at);
            }
        }
    }
    
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    Mini Container Monitor (Live)                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    reset_color();
    
    set_color(COLOR_GREEN);
    printf("Running: %d  ", running_count);
    reset_color();
    printf("Total: %d  ", count);
    
    set_color(COLOR_YELLOW);
    printf("Total CPU: %.1f%%  ", total_cpu_percent);
    reset_color();
    
    set_color(COLOR_CYAN);
    printf("Total RAM: %s  ", format_bytes(total_memory_bytes).c_str());
    reset_color();
    
    time_t now = time(nullptr);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
    printf("Time: %s\n", time_str);
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    
    set_color(COLOR_BOLD);
    printf("%-20s %-8s %-10s %-12s %-12s %-10s\n",
           "CONTAINER ID", "PID", "STATE", "CPU%", "MEMORY", "RUNTIME");
    reset_color();
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    
    if (count == 0) {
        set_color(COLOR_YELLOW);
        printf("No containers running\n");
        reset_color();
    } else {
        vector<container_info_t*> sorted_containers;
        for (int i = 0; i < count; i++) {
            sorted_containers.push_back(containers[i]);
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
                
                printf(" %-12.1f %-12s", cpu_percent, mem_str.c_str());
            } else {
                printf(" %-12s %-12s", "--", "--");
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
        printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
        printf("║                    Mini Container Monitor (htop-like)                        ║\n");
        printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
        reset_color();
        
        int count;
        container_info_t **containers = container_manager_list(&cm, &count);
        
        int running_count = 0;
        for (int i = 0; i < count; i++) {
            if (containers[i]->state == CONTAINER_RUNNING) {
                running_count++;
            }
        }
        
        set_color(COLOR_GREEN);
        printf("Running: %d  ", running_count);
        reset_color();
        printf("Total: %d  ", count);
        
        time_t now = time(nullptr);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
        printf("Time: %s\n", time_str);
        printf("───────────────────────────────────────────────────────────────────────────────\n");
        
        set_color(COLOR_BOLD);
        printf("%-20s %-8s %-10s %-12s %-12s %-10s %-10s\n",
               "CONTAINER ID", "PID", "STATE", "CPU%", "MEMORY", "RUNTIME", "CREATED");
        reset_color();
        printf("───────────────────────────────────────────────────────────────────────────────\n");
        
        if (count == 0) {
            set_color(COLOR_YELLOW);
            printf("No containers running\n");
            reset_color();
        } else {
            vector<container_info_t*> sorted_containers;
            for (int i = 0; i < count; i++) {
                sorted_containers.push_back(containers[i]);
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
        
        usleep(5000000); // 5 seconds
    }
    
    show_cursor();
    clear_screen();
}

// Interactive container creation
void interactive_create_container() {
    clear_screen();
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        Create New Container                                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    reset_color();
    
    char command[1024];
    char container_name[256] = "";
    char hostname[256] = "mini-container";
    char root_path[512] = "/";  // Use real root for demo (allows /bin/sh to work)
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
    
    printf("Command to run: ");
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
    
    printf("Hostname (default mini-container): ");
    fflush(stdout);
    char hostname_input[256];
    if (fgets(hostname_input, sizeof(hostname_input), stdin)) {
        size_t hlen = strlen(hostname_input);
        if (hlen > 0 && hostname_input[hlen-1] == '\n') {
            hostname_input[hlen-1] = '\0';
        }
        if (strlen(hostname_input) > 0) {
            strncpy(hostname, hostname_input, sizeof(hostname)-1);
        }
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
                args.push_back(strdup(current_arg.c_str()));
                current_arg.clear();
            }
        } else {
            current_arg += c;
        }
    }
    
    if (!current_arg.empty()) {
        args.push_back(strdup(current_arg.c_str()));
    }
    args.push_back(nullptr);
    
    container_config_t config;
    if (strlen(container_name) > 0) {
        config.id = strdup(container_name);
    } else {
        config.id = nullptr;
    }
    namespace_config_init(&config.ns_config);
    resource_limits_init(&config.res_limits);
    fs_config_init(&config.fs_config);
    
    config.res_limits.memory.limit_bytes = memory * 1024 * 1024;
    config.res_limits.cpu.shares = cpu;
    config.ns_config.hostname = strdup(hostname);
    config.fs_config.root_path = strdup(root_path);
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
    
    // Cleanup
    for (auto arg : args) {
        if (arg) free(arg);
    }
    free(config.ns_config.hostname);
    free(config.fs_config.root_path);
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Run tests
void run_tests() {
    clear_screen();
    set_color(COLOR_BOLD);
    set_color(COLOR_CYAN);
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                            System Tests                                        ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
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
        // CPU test
        printf("\n[Test 1] CPU Usage Test...\n");
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.ns_config.hostname = strdup("cpu-test");
        config.fs_config.root_path = strdup("/");  // Use real root for demo
        
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
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    if (test_num == 2 || test_num == 5) {
        // Memory test
        printf("\n[Test 2] Memory Limit Test...\n");
        snprintf(container_id, sizeof(container_id), "test_mem_%ld", time(nullptr));
        config.id = container_id;
        config.res_limits.memory.limit_bytes = 64 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.ns_config.hostname = strdup("mem-test");
        config.fs_config.root_path = strdup("/");  // Use real root for demo
        
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
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    if (test_num == 3 || test_num == 5) {
        // CPU limit test
        printf("\n[Test 3] CPU Limit Test...\n");
        snprintf(container_id, sizeof(container_id), "test_cpu_limit_%ld", time(nullptr));
        config.id = container_id;
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 512;
        config.ns_config.hostname = strdup("cpu-limit-test");
        config.fs_config.root_path = strdup("/");  // Use real root for demo
        
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
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    if (test_num == 4 || test_num == 5) {
        // Combined test
        printf("\n[Test 4] Combined Test (CPU + Memory)...\n");
        snprintf(container_id, sizeof(container_id), "test_combined_%ld", time(nullptr));
        config.id = container_id;
        config.res_limits.memory.limit_bytes = 128 * 1024 * 1024;
        config.res_limits.cpu.shares = 1024;
        config.ns_config.hostname = strdup("combined-test");
        config.fs_config.root_path = strdup("/");  // Use real root for demo
        
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
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    printf("\nPress Enter to continue...");
    getchar();
}

// Initialize 10 containers with different resource consumption patterns
void init_containers() {
    // printf("Initializing 10 containers with different resource patterns...\n");
    
    time_t base_time = time(nullptr);
    int counter = 0;
    const int runtime_seconds = 600;  // 10 minutes
    char cmd_buffer[512];
    
    // Container 1: CPU intensive - multiple CPU-bound processes
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "cpu_intensive_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 80 * 1024 * 1024;  // 80 MB limit
        config.res_limits.cpu.shares = 256;  // Limited CPU shares
        config.ns_config.hostname = strdup("cpu-intensive");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "for i in 1 2 3 4; do yes > /dev/null & done; sleep %d", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 1 (CPU intensive) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 2: RAM intensive - large memory allocation
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "ram_intensive_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 200 * 1024 * 1024;  // 200 MB limit
        config.res_limits.cpu.shares = 128;  // Low CPU
        config.ns_config.hostname = strdup("ram-intensive");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'a = [bytearray(50*1024*1024) for _ in range(3)]; import time; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 2 (RAM intensive) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 3: CPU + RAM both - heavy computation
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "cpu_ram_heavy_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 150 * 1024 * 1024;  // 150 MB limit
        config.res_limits.cpu.shares = 256;  // Limited CPU
        config.ns_config.hostname = strdup("cpu-ram-heavy");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "yes > /dev/null & python3 -c 'a = bytearray(120*1024*1024); import time; [i*i for i in range(10000000)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 3 (CPU + RAM heavy) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 4: CPU calculation intensive
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "cpu_calc_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 100 * 1024 * 1024;  // 100 MB limit
        config.res_limits.cpu.shares = 300;  // Medium CPU limit
        config.ns_config.hostname = strdup("cpu-calc");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'import time; start=time.time(); [sum(range(i)) for i in range(50000)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 4 (CPU calc) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 5: Memory stress test
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "mem_stress_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 180 * 1024 * 1024;  // 180 MB limit
        config.res_limits.cpu.shares = 100;  // Low CPU
        config.ns_config.hostname = strdup("mem-stress");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'import time; data = [bytearray(30*1024*1024) for _ in range(5)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 5 (Memory stress) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 6: Mixed workload - CPU and I/O
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "mixed_workload_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 120 * 1024 * 1024;  // 120 MB limit
        config.res_limits.cpu.shares = 200;  // Medium CPU
        config.ns_config.hostname = strdup("mixed-workload");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "yes > /dev/null & dd if=/dev/zero of=/tmp/test bs=1M count=50 status=none & sleep %d", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 6 (Mixed workload) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 7: High CPU, low memory
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "high_cpu_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 60 * 1024 * 1024;  // 60 MB limit
        config.res_limits.cpu.shares = 400;  // Higher CPU limit
        config.ns_config.hostname = strdup("high-cpu");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "for i in 1 2 3; do yes > /dev/null & done; sleep %d", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 7 (High CPU) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 8: High memory, low CPU
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "high_mem_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 250 * 1024 * 1024;  // 250 MB limit
        config.res_limits.cpu.shares = 80;  // Very low CPU
        config.ns_config.hostname = strdup("high-mem");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "python3 -c 'a = [bytearray(40*1024*1024) for _ in range(5)]; import time; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 8 (High memory) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 9: Balanced workload
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "balanced_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 140 * 1024 * 1024;  // 140 MB limit
        config.res_limits.cpu.shares = 250;  // Medium CPU
        config.ns_config.hostname = strdup("balanced");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "yes > /dev/null & python3 -c 'a = bytearray(100*1024*1024); import time; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 9 (Balanced) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // Container 10: Maximum stress - both CPU and RAM
    {
        container_config_t config;
        namespace_config_init(&config.ns_config);
        resource_limits_init(&config.res_limits);
        fs_config_init(&config.fs_config);
        
        char container_id[64];
        snprintf(container_id, sizeof(container_id), "max_stress_%ld_%d", base_time, counter++);
        config.id = strdup(container_id);
        config.res_limits.memory.limit_bytes = 220 * 1024 * 1024;  // 220 MB limit
        config.res_limits.cpu.shares = 350;  // Higher CPU limit
        config.ns_config.hostname = strdup("max-stress");
        config.fs_config.root_path = strdup("/");
        
        vector<char*> args;
        args.push_back(strdup("/bin/sh"));
        args.push_back(strdup("-c"));
        snprintf(cmd_buffer, sizeof(cmd_buffer), "for i in 1 2 3 4 5; do yes > /dev/null & done; python3 -c 'a = [bytearray(35*1024*1024) for _ in range(5)]; import time; [i**2 for i in range(2000000)]; time.sleep(%d)'", runtime_seconds);
        args.push_back(strdup(cmd_buffer));
        args.push_back(nullptr);
        config.command = args.data();
        config.command_argc = 3;
        
        if (container_manager_run(&cm, &config) == 0) {
            // printf("  ✓ Container 10 (Max stress) started: %s\n", container_id);
        }
        
        for (auto arg : args) if (arg) free(arg);
        free(config.id);
        free(config.ns_config.hostname);
        free(config.fs_config.root_path);
    }
    
    // printf("Container initialization complete!\n\n");
}

// Signal handler
void signal_handler(int signum) {
    (void)signum;
    running = false;
    monitor_mode = false;
    show_cursor();
    
    // Stop web server
    if (web_server) {
        web_server->stop();
        delete web_server;
        web_server = nullptr;
    }
    
    // Cleanup container manager
    container_manager_cleanup(&cm);
}

// Interactive menu with live monitor
void interactive_menu() {
    // Signal handlers already set in main()
    
    // Set terminal to non-canonical mode for non-blocking input
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 0;
    new_term.c_cc[VTIME] = 0;
    
    hide_cursor();
    
    while (running) {
        clear_screen();
        
        // Display live monitor at top
        display_compact_monitor();
        
        // Display menu at bottom
        printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
        set_color(COLOR_BOLD);
        set_color(COLOR_CYAN);
        printf("║                          Commands Menu                                      ║\n");
        reset_color();
        printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
        printf("║  1. Create Container         2. Full Monitor (htop)     3. List Containers  ║\n");
        printf("║  4. Stop Container           5. Destroy Container        6. Container Info  ║\n");
        printf("║  7. Run Tests                0. Exit                                        ║\n");
        printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        set_color(COLOR_YELLOW);
        printf("Select option (auto-refresh every 5 seconds): ");
        reset_color();
        fflush(stdout);
        
        // Check for input (non-blocking)
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
                // User pressed a key
                int option = choice - '0';
                
                // Restore terminal settings for blocking input
                tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
                show_cursor();
                
                // Handle menu option
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
                            char cmd_stop[] = "stop";
                            char* argv[] = {cmd_stop, id, nullptr};
                            handle_stop(2, argv);
                            printf("\nPress Enter to continue...");
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
                            char cmd_destroy[] = "destroy";
                            char* argv[] = {cmd_destroy, id, nullptr};
                            handle_destroy(2, argv);
                            printf("\nPress Enter to continue...");
                            getchar();
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
                    case 7:
                        clear_screen();
                        run_tests();
                        break;
                    case 0:
                        running = false;
                        break;
                    default:
                        // Invalid option, just continue
                        break;
                }
                
                // Restore non-blocking mode
                tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
                hide_cursor();
            }
        } else if (select_result == 0) {
            // Timeout (5 seconds) - refresh screen automatically
            // Continue loop to refresh
        }
        
        // No additional delay needed - select timeout handles it
    }
    
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    show_cursor();
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

    // Initialize 5 containers with different resource patterns
    init_containers();

    // Start web server automatically
    web_server = new SimpleWebServer(&cm, 808);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    web_server->start();
    // printf("Web server started on port 808\n");
    // printf("Open http://localhost:808 in your browser\n");

    // If no arguments, run interactive menu
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

    // Otherwise, handle command line arguments
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

    // Stop web server before cleanup (only for commands that exit immediately)
    // For 'run' command, web server is kept running until Ctrl+C (handled in signal handler)
    if (strcmp(command, "run") != 0) {
        if (web_server) {
            web_server->stop();
            delete web_server;
            web_server = nullptr;
        }
        container_manager_cleanup(&cm);
    }
    // For 'run' command, cleanup is done in signal handler when Ctrl+C is pressed
    
    return result;
}
