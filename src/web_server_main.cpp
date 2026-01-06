#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <ctime>
#include <sys/time.h>
#include <sys/wait.h>
#include "../include/container_manager.hpp"
#include "web_server.hpp"

using namespace std;

static container_manager_t cm;
static WebServer* web_server = nullptr;
static bool running = true;

// Global debug log callback
extern "C" void debug_log_callback(const char* message) {
    if (web_server) {
        web_server->addDebugLog(message);
    }
    // Also print to stderr
    fprintf(stderr, "%s", message);
}

void signal_handler(int signum) {
    (void)signum;  // Suppress unused parameter warning
    cout << "\nShutting down web server..." << endl;
    running = false;

    if (web_server) {
        web_server->stop();
    }
}

std::string run_container_callback(const std::string& command, const std::string& memory,
                                 const std::string& cpu, const std::string& hostname,
                                 const std::string& root_path, const std::string& container_name = "") {
    container_config_t config;

    // Initialize config
    if (!container_name.empty()) {
        config.id = strdup(container_name.c_str());
    } else {
        config.id = nullptr;
    }
    namespace_config_init(&config.ns_config);
    resource_limits_init(&config.res_limits);
    fs_config_init(&config.fs_config);

    // Set resource limits
    config.res_limits.memory.limit_bytes = std::stoi(memory) * 1024 * 1024;
    config.res_limits.cpu.shares = std::stoi(cpu);

    // Set hostname
    config.ns_config.hostname = strdup(hostname.c_str());

    // Set root path
    config.fs_config.root_path = strdup(root_path.c_str());

    // Parse command - handle quoted strings properly
    std::vector<char*> args;
    std::string cmd = command;
    bool in_quotes = false;
    char quote_char = 0;
    std::string current_arg;
    
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

    config.command = args.data();
    config.command_argc = args.size() - 1;

    int result = container_manager_run(&cm, &config);

    // Clean up
    for (auto arg : args) {
        if (arg) free(arg);
    }
    free(config.ns_config.hostname);
    free(config.fs_config.root_path);
    // Note: config.id is managed by container_manager, don't free it here

    if (result != 0) {
        return "{\"success\":false,\"error\":\"Failed to create container\"}";
    }

    container_info_t* info = container_manager_get_info(&cm, config.id);
    if (!info) {
        return "{\"success\":false,\"error\":\"Failed to get container info\"}";
    }

    return "{\"success\":true,\"container_id\":\"" + std::string(info->id) + "\"}";
}


int main(int argc, char* argv[]) {
    // Default port for the mini-container web UI
    int port = 803;

    // Parse command line arguments
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    // Initialize container manager
    if (container_manager_init(&cm, 10) != 0) {
        cerr << "Failed to initialize container manager" << endl;
        return EXIT_FAILURE;
    }

    // Check privileges
    if (getuid() != 0) {
        cout << "Warning: Container operations typically require root privileges" << endl;
    }

    // Create web server
    web_server = new WebServer(&cm, port);
    
    // Set debug log callback for resource manager
    if (cm.rm && cm.rm->initialized) {
        cm.rm->debug_log_callback = debug_log_callback;
    }

    // Set up callbacks
    web_server->setRunCallback(run_container_callback);

    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Start web server
    web_server->start();

    cout << "Web server started on port " << port << endl;
    cout << "Open http://localhost:" << port << " in your browser" << endl;
    cout << "Press Ctrl+C to stop" << endl;

    // Main loop
    while (running) {
        sleep(1);
    }

    // Cleanup
    delete web_server;
    container_manager_cleanup(&cm);

    cout << "Web server stopped" << endl;
    return EXIT_SUCCESS;
}
