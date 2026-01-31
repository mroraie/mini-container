#include "web_server_simple.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>
#include <ctime>

SimpleWebServer::SimpleWebServer(container_manager_t* cm, int port)
    : cm_(cm), port_(port), running_(false), server_socket_(-1) {
}

SimpleWebServer::~SimpleWebServer() {
    stop();
}

void SimpleWebServer::start() {
    if (running_) return;

    running_ = true;
    server_thread_ = std::thread(&SimpleWebServer::serverThread, this);
}

void SimpleWebServer::stop() {
    if (!running_) return;

    running_ = false;

    if (server_socket_ != -1) {
        close(server_socket_);
        server_socket_ = -1;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void SimpleWebServer::serverThread() {
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return;
        }

        int reuse = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            std::cerr << "Warning: Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Failed to bind socket on port " << port_ << ": " << strerror(errno) << std::endl;
            close(server_socket_);
            server_socket_ = -1;
            return;
        }

        if (listen(server_socket_, 5) == -1) {
            std::cerr << "Failed to listen on socket" << std::endl;
            close(server_socket_);
            return;
        }

        std::cout << "Web server started on port " << port_ << std::endl;
        std::cout << "Open http://localhost:" << port_ << " in your browser" << std::endl;

        while (running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            int client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);
            if (client_socket == -1) {
                if (running_) {
                    std::cerr << "Failed to accept connection" << std::endl;
                }
                continue;
            }

            char buffer[4096];
            ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                std::string response = handleRequest(buffer);
                write(client_socket, response.c_str(), response.size());
            }

            close(client_socket);
        }

        close(server_socket_);
        server_socket_ = -1;
}

std::string SimpleWebServer::handleRequest(const std::string& request) {
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;

        if (method == "GET") {
            if (path == "/" || path == "/index.html") {
                return "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html\r\n"
                      "Connection: close\r\n\r\n" +
                      generateHTML();
            } else if (path == "/api/containers") {
                return "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Connection: close\r\n\r\n" +
                      getContainerListJSON();
            } else if (path == "/api/system") {
                return "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Connection: close\r\n\r\n" +
                      getSystemInfoJSON();
            } else {
                return "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n\r\n"
                      "Not Found";
            }
        } else if (method == "OPTIONS") {
            return "HTTP/1.1 200 OK\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\n"
                  "Connection: close\r\n\r\n";
        } else {
            return "HTTP/1.1 405 Method Not Allowed\r\n"
                  "Content-Type: text/plain\r\n"
                  "Connection: close\r\n\r\n"
                  "Method Not Allowed";
        }
}

std::string SimpleWebServer::getContainerListJSON() {
        std::string json = "{\"containers\":[";

        int count;
        container_info_t** containers = container_manager_list(cm_, &count);

        for (int i = 0; i < count; i++) {
            container_info_t* info = containers[i];
            if (i > 0) json += ",";

            unsigned long cpu_usage = 0, memory_usage = 0;
            if (info->state == CONTAINER_RUNNING) {
                resource_manager_get_stats(cm_->rm, info->id, &cpu_usage, &memory_usage);
            }

            const char* state_names[] = {"CREATED", "RUNNING", "STOPPED", "DESTROYED"};
            const char* state_str = "UNKNOWN";
            if ((int)info->state >= 0 && (int)info->state < (int)(sizeof(state_names) / sizeof(state_names[0]))) {
                state_str = state_names[info->state];
            }
            json += "{";
            json += "\"id\":\"" + std::string(info->id) + "\",";
            json += "\"pid\":" + std::to_string(info->pid) + ",";
            json += "\"state\":\"" + std::string(state_str) + "\",";
            json += "\"created_at\":" + std::to_string(info->created_at) + ",";
            json += "\"started_at\":" + std::to_string(info->started_at) + ",";
            json += "\"stopped_at\":" + std::to_string(info->stopped_at);
            if (info->state == CONTAINER_RUNNING) {
                json += ",\"cpu_usage\":" + std::to_string(cpu_usage) + ",";
                json += "\"memory_usage\":" + std::to_string(memory_usage);
            }
            json += "}";
        }

        json += "]}";
        return json;
}

unsigned long SimpleWebServer::getSystemTotalMemory() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            unsigned long kb = std::stoul(value);
            return kb * 1024; 
        }
    }
    return 0;
}

unsigned long SimpleWebServer::getSystemAvailableMemory() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            unsigned long kb = std::stoul(value);
            return kb * 1024; 
        }
    }
    
    meminfo.clear();
    meminfo.seekg(0);
    while (std::getline(meminfo, line)) {
        if (line.find("MemFree:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            unsigned long kb = std::stoul(value);
            return kb * 1024; 
        }
    }
    return 0;
}

double SimpleWebServer::getSystemCPUPercent() {
    static unsigned long prev_idle = 0, prev_total = 0;
    
    std::ifstream stat("/proc/stat");
    if (!stat.is_open()) {
        return 0.0;
    }
    
    std::string line;
    if (std::getline(stat, line)) {
        if (line.find("cpu ") == 0) {
            std::istringstream iss(line);
            std::string cpu_label;
            unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
            iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            unsigned long total = user + nice + system + idle + iowait + irq + softirq + steal;
            unsigned long total_idle = idle + iowait;
            
            if (prev_total > 0) {
                unsigned long total_diff = total - prev_total;
                unsigned long idle_diff = total_idle - prev_idle;
                
                if (total_diff > 0) {
                    double cpu_percent = 100.0 * (1.0 - ((double)idle_diff / (double)total_diff));
                    prev_idle = total_idle;
                    prev_total = total;
                    return cpu_percent;
                }
            }
            
            prev_idle = total_idle;
            prev_total = total;
        }
    }
    
    return 0.0;
}

std::string SimpleWebServer::getSystemInfoJSON() {
    unsigned long total_mem = getSystemTotalMemory();
    unsigned long available_mem = getSystemAvailableMemory();
    unsigned long used_mem = total_mem - available_mem;
    double cpu_percent = getSystemCPUPercent();
    
    std::string json = "{";
    json += "\"used_memory\":" + std::to_string(used_mem) + ",";
    json += "\"cpu_percent\":" + std::to_string(cpu_percent);
    json += "}";
    return json;
}

std::string SimpleWebServer::generateHTML() {
        return R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>System Monitor</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; margin-bottom: 30px; }
        .stat-box { margin: 20px 0; padding: 20px; background: #f8f8f8; border-radius: 8px; border-left: 4px solid #4CAF50; }
        .stat-label { font-size: 14px; color: #666; margin-bottom: 8px; }
        .stat-value { font-size: 32px; font-weight: bold; color: #333; }
        .cpu { border-left-color: #2196F3; }
        .ram { border-left-color: #FF9800; }
    </style>
</head>
<body>
    <div class="container">
        <h1>System Monitor</h1>
        
        <div class="stat-box cpu">
            <div class="stat-label">CPU Usage</div>
            <div class="stat-value" id="cpu-usage">0.0%</div>
        </div>
        
        <div class="stat-box ram">
            <div class="stat-label">RAM Usage</div>
            <div class="stat-value" id="ram-usage">0 MB</div>
        </div>
    </div>

    <script>
        function formatBytes(bytes) {
            if (!bytes || bytes === 0) return '0 MB';
            const mb = bytes / (1024 * 1024);
            return mb.toFixed(2) + ' MB';
        }

        function updateStats() {
            fetch('/api/system')
                .then(r => r.json())
                .then(data => {
                    if (data.cpu_percent !== undefined) {
                        document.getElementById('cpu-usage').textContent = 
                            parseFloat(data.cpu_percent).toFixed(1) + '%';
                    }
                    if (data.used_memory !== undefined) {
                        document.getElementById('ram-usage').textContent = 
                            formatBytes(parseInt(data.used_memory));
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                });
        }

        updateStats();
        setInterval(updateStats, 1000);
    </script>
</body>
</html>
)HTML";
}
