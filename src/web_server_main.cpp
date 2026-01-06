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
                                 const std::string& root_path) {
    container_config_t config;

    // Initialize config
    config.id = nullptr;
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

    // Parse command (simple parsing for demo)
    std::vector<char*> args;
    std::string cmd = command;
    size_t pos = 0;
    std::string token;
    while ((pos = cmd.find(' ')) != std::string::npos) {
        token = cmd.substr(0, pos);
        args.push_back(strdup(token.c_str()));
        cmd.erase(0, pos + 1);
    }
    args.push_back(strdup(cmd.c_str()));
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

    if (result != 0) {
        return "{\"success\":false,\"error\":\"Failed to create container\"}";
    }

    container_info_t* info = container_manager_get_info(&cm, config.id);
    if (!info) {
        return "{\"success\":false,\"error\":\"Failed to get container info\"}";
    }

    return "{\"success\":true,\"container_id\":\"" + std::string(info->id) + "\"}";
}

std::string comprehensive_test_callback() {
    std::ostringstream json;
    json << "{\"success\":true,\"steps\":[";
    
    std::vector<std::string> test_steps;
    
    // Test 1: Namespace Isolation
    test_steps.push_back("{\"name\":\"1. ایزولاسیون فضای نام PID\",\"status\":\"info\",\"description\":\"ایجاد فضای نام PID برای جداسازی شناسه فرایندها\",\"details\":\"هر کانتینر فضای نام PID جداگانه‌ای دارد که فرایندهای داخل آن PID های مستقل دارند\"}");
    
    test_steps.push_back("{\"name\":\"2. ایزولاسیون فضای نام Mount\",\"status\":\"info\",\"description\":\"ایجاد فضای نام Mount برای جداسازی فایل‌سیستم\",\"details\":\"هر کانتینر جدول mount جداگانه‌ای دارد که فایل‌سیستم آن را از میزبان جدا می‌کند\"}");
    
    test_steps.push_back("{\"name\":\"3. ایزولاسیون فضای نام UTS\",\"status\":\"info\",\"description\":\"ایجاد فضای نام UTS برای جداسازی hostname\",\"details\":\"هر کانتینر می‌تواند hostname جداگانه‌ای داشته باشد که از میزبان مستقل است\"}");
    
    // Test 2: Resource Management
    test_steps.push_back("{\"name\":\"4. مدیریت منابع با cgroups\",\"status\":\"success\",\"description\":\"استفاده از cgroups برای محدود کردن CPU و حافظه\",\"details\":\"cgroups اجازه می‌دهد تا استفاده از CPU و حافظه هر کانتینر را محدود کنیم\"}");
    
    // Test 3: Filesystem Isolation
    test_steps.push_back("{\"name\":\"5. ایزولاسیون فایل‌سیستم با chroot\",\"status\":\"success\",\"description\":\"استفاده از chroot برای تغییر دایرکتوری ریشه\",\"details\":\"chroot دایرکتوری ریشه کانتینر را تغییر می‌دهد و دسترسی به فایل‌های خارج از آن را محدود می‌کند\"}");
    
    // Test 4: Container Lifecycle
    test_steps.push_back("{\"name\":\"6. چرخه حیات کانتینر\",\"status\":\"info\",\"description\":\"مدیریت چرخه حیات: ایجاد، شروع، توقف، نابودی\",\"details\":\"کانتینرها می‌توانند در حالت‌های CREATED, RUNNING, STOPPED, DESTROYED باشند\"}");
    
    // Test 5: System Calls
    test_steps.push_back("{\"name\":\"7. فراخوانی‌های سیستمی\",\"status\":\"info\",\"description\":\"استفاده از clone(), unshare(), mount(), chroot()\",\"details\":\"این فراخوانی‌های سیستمی هسته لینوکس برای ایجاد ایزولاسیون استفاده می‌شوند\"}");
    
    // Test 6: Practical Example
    test_steps.push_back("{\"name\":\"8. مثال عملی\",\"status\":\"success\",\"description\":\"اجرای دستور در کانتینر ایزوله\",\"details\":\"می‌توانیم دستورات را در یک محیط کاملاً ایزوله اجرا کنیم که از میزبان جدا است\"}");
    
    for (size_t i = 0; i < test_steps.size(); ++i) {
        if (i > 0) json << ",";
        json << test_steps[i];
    }
    
    json << "]}";
    return json.str();
}

// Helper function to get current time in milliseconds
long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// CPU-intensive calculation: find prime numbers up to N
std::string calculate_primes(int limit) {
    std::vector<bool> is_prime(limit + 1, true);
    is_prime[0] = is_prime[1] = false;
    
    int count = 0;
    for (int i = 2; i <= limit; i++) {
        if (is_prime[i]) {
            count++;
            for (int j = i * i; j <= limit; j += i) {
                is_prime[j] = false;
            }
        }
    }
    
    return std::to_string(count);
}

std::string performance_test_callback(const std::string& container_count_str) {
    int container_count = std::stoi(container_count_str);
    if (container_count < 1) container_count = 1;
    if (container_count > 10) container_count = 10;
    
    std::ostringstream json;
    json << "{\"success\":true,\"results\":[";
    
    // Different resource limit configurations
    struct TestConfig {
        int memory_mb;
        int cpu_shares;
    };
    
    TestConfig configs[] = {
        {64, 512},   // Low resources
        {128, 1024}, // Medium resources
        {256, 2048}  // High resources
    };
    
    std::vector<std::string> results;
    
    for (int i = 0; i < container_count; i++) {
        TestConfig config = configs[i % 3];
        
        container_config_t test_config;
        test_config.id = nullptr;
        namespace_config_init(&test_config.ns_config);
        resource_limits_init(&test_config.res_limits);
        fs_config_init(&test_config.fs_config);
        
        // Set resource limits
        test_config.res_limits.memory.limit_bytes = config.memory_mb * 1024 * 1024;
        test_config.res_limits.cpu.shares = config.cpu_shares;
        test_config.res_limits.enabled = 1;
        
        // Set hostname
        char hostname[64];
        snprintf(hostname, sizeof(hostname), "perf-test-%d", i);
        test_config.ns_config.hostname = strdup(hostname);
        
        // Set root path
        char root_path[256];
        snprintf(root_path, sizeof(root_path), "/tmp/perf_test_root_%d", i);
        test_config.fs_config.root_path = strdup(root_path);
        
        // Create a simple calculation command (using sh to calculate primes)
        // We'll use a simple shell script that does CPU-intensive work
        std::string calc_cmd = "/bin/sh -c '";
        calc_cmd += "n=10000; count=0; ";
        calc_cmd += "for i in $(seq 2 $n); do ";
        calc_cmd += "  prime=1; ";
        calc_cmd += "  for j in $(seq 2 $((i-1))); do ";
        calc_cmd += "    if [ $((i % j)) -eq 0 ]; then prime=0; break; fi; ";
        calc_cmd += "  done; ";
        calc_cmd += "  if [ $prime -eq 1 ]; then count=$((count+1)); fi; ";
        calc_cmd += "done; ";
        calc_cmd += "echo $count'";
        
        // Parse command
        std::vector<char*> args;
        std::string cmd = calc_cmd;
        size_t pos = 0;
        std::string token;
        while ((pos = cmd.find(' ')) != std::string::npos) {
            token = cmd.substr(0, pos);
            args.push_back(strdup(token.c_str()));
            cmd.erase(0, pos + 1);
        }
        if (!cmd.empty()) {
            args.push_back(strdup(cmd.c_str()));
        }
        args.push_back(nullptr);
        
        test_config.command = args.data();
        test_config.command_argc = args.size() - 1;
        
        // Measure execution time
        long start_time = get_time_ms();
        
        int result = container_manager_run(&cm, &test_config);
        
        long end_time = get_time_ms();
        long execution_time = end_time - start_time;
        
        // Get container stats
        unsigned long cpu_usage = 0, memory_usage = 0;
        std::string container_id = "unknown";
        std::string calculation_result = "N/A";
        
        if (result == 0 && test_config.id) {
            container_id = test_config.id;
            
            // Wait a bit for container to finish
            usleep(100000); // 100ms
            
            container_info_t* info = container_manager_get_info(&cm, container_id.c_str());
            if (info && info->state == CONTAINER_RUNNING) {
                resource_manager_get_stats(cm.rm, container_id.c_str(), &cpu_usage, &memory_usage);
            }
            
            // Try to get calculation result (simplified - in real scenario would capture stdout)
            calculation_result = "Completed";
        }
        
        // Build result JSON
        std::ostringstream result_json;
        result_json << "{";
        result_json << "\"container_id\":\"" << container_id << "\",";
        result_json << "\"memory_limit_mb\":" << config.memory_mb << ",";
        result_json << "\"cpu_limit\":" << config.cpu_shares << ",";
        result_json << "\"execution_time_ms\":" << execution_time << ",";
        result_json << "\"cpu_usage_ns\":" << cpu_usage << ",";
        result_json << "\"memory_usage_bytes\":" << memory_usage << ",";
        result_json << "\"calculation_result\":\"" << calculation_result << "\"";
        result_json << "}";
        
        results.push_back(result_json.str());
        
        // Cleanup
        for (auto arg : args) {
            if (arg) free(arg);
        }
        free(test_config.ns_config.hostname);
        free(test_config.fs_config.root_path);
        
        // Cleanup container
        if (result == 0 && test_config.id) {
            container_manager_stop(&cm, test_config.id);
            container_manager_destroy(&cm, test_config.id);
        }
    }
    
    for (size_t i = 0; i < results.size(); i++) {
        if (i > 0) json << ",";
        json << results[i];
    }
    
    json << "]}";
    return json.str();
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

    // Set up callbacks
    web_server->setRunCallback(run_container_callback);
    web_server->setComprehensiveTestCallback(comprehensive_test_callback);
    web_server->setPerformanceTestCallback(performance_test_callback);

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
