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
            return kb * 1024; // Convert KB to bytes
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
            return kb * 1024; // Convert KB to bytes
        }
    }
    // Fallback to MemFree if MemAvailable not available
    meminfo.clear();
    meminfo.seekg(0);
    while (std::getline(meminfo, line)) {
        if (line.find("MemFree:") == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            unsigned long kb = std::stoul(value);
            return kb * 1024; // Convert KB to bytes
        }
    }
    return 0;
}

std::string SimpleWebServer::getSystemInfoJSON() {
    unsigned long total_mem = getSystemTotalMemory();
    unsigned long available_mem = getSystemAvailableMemory();
    unsigned long used_mem = total_mem - available_mem;
    
    // Calculate total CPU and memory usage from all containers
    int count;
    container_info_t** containers = container_manager_list(cm_, &count);
    
    unsigned long total_container_memory = 0;
    double total_container_cpu_percent = 0.0;
    int running_count = 0;
    
    for (int i = 0; i < count; i++) {
        container_info_t* info = containers[i];
        if (info->state == CONTAINER_RUNNING) {
            running_count++;
            unsigned long cpu_usage = 0, memory_usage = 0;
            resource_manager_get_stats(cm_->rm, info->id, &cpu_usage, &memory_usage);
            total_container_memory += memory_usage;
            
            if (info->started_at > 0) {
                time_t now = time(nullptr);
                long elapsed = now - info->started_at;
                if (elapsed > 0) {
                    double cpu_seconds = cpu_usage / 1e9;
                    double cpu_percent = (cpu_seconds / elapsed) * 100.0;
                    total_container_cpu_percent += cpu_percent;
                }
            }
        }
    }
    
    std::string json = "{";
    json += "\"total_memory\":" + std::to_string(total_mem) + ",";
    json += "\"available_memory\":" + std::to_string(available_mem) + ",";
    json += "\"used_memory\":" + std::to_string(used_mem) + ",";
    json += "\"total_container_memory\":" + std::to_string(total_container_memory) + ",";
    json += "\"total_container_cpu_percent\":" + std::to_string(total_container_cpu_percent);
    json += "}";
    return json;
}

std::string SimpleWebServer::generateHTML() {
        return R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Mini Container Monitor</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { color: #333; }
        .stats { margin: 20px 0; padding: 10px; background: #f5f5f5; }
        .stats span { margin-right: 20px; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #4CAF50; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        .running { color: green; }
        .stopped { color: red; }
        .created { color: orange; }
    </style>
</head>
<body>
    <h1>Mini Container Monitor</h1>
    
    <div class="stats">
        <span>Active: <strong id="running-count">0</strong></span>
        <span>Total: <strong id="total-count">0</strong></span>
        <span>Total CPU: <strong id="total-cpu">0.0%</strong></span>
        <span>Total Memory: <strong id="total-memory">--</strong></span>
        <span>Available Memory: <strong id="available-memory">--</strong></span>
        <span>Last Update: <strong id="update-time">--:--:--</strong></span>
    </div>

    <table>
        <thead>
            <tr>
                <th>ID</th>
                <th>PID</th>
                <th>Status</th>
                <th>CPU</th>
                <th>Memory</th>
                <th>Runtime</th>
                <th>Created</th>
            </tr>
        </thead>
        <tbody id="container-table-body">
            <tr><td colspan="7">Loading...</td></tr>
        </tbody>
    </table>

    <script>
        const prevCpuUsage = {};
        const prevUpdateTime = {};

        function formatBytes(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
        }

        function formatTime(seconds) {
            if (!seconds) return '--';
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;
            if (hours > 0) {
                return hours + ':' + String(minutes).padStart(2, '0') + ':' + String(secs).padStart(2, '0');
            }
            return minutes + ':' + String(secs).padStart(2, '0');
        }

        function formatDate(timestamp) {
            if (!timestamp) return '--';
            const date = new Date(timestamp * 1000);
            return date.toLocaleTimeString('en-US');
        }

        function calculateCPUPercent(containerId, cpuUsageNs, currentTime) {
            cpuUsageNs = typeof cpuUsageNs === 'string' ? parseFloat(cpuUsageNs) : cpuUsageNs;
            if (!cpuUsageNs || isNaN(cpuUsageNs) || cpuUsageNs === 0) return 0;
            
            const prev = prevCpuUsage[containerId];
            const prevTime = prevUpdateTime[containerId];
            
            if (!prev || !prevTime) {
                prevCpuUsage[containerId] = cpuUsageNs;
                prevUpdateTime[containerId] = currentTime;
                return 0;
            }
            
            const timeDiff = (currentTime - prevTime) / 1000;
            if (timeDiff <= 0 || timeDiff > 10) {
                prevCpuUsage[containerId] = cpuUsageNs;
                prevUpdateTime[containerId] = currentTime;
                return 0;
            }
            
            const cpuDiff = cpuUsageNs - prev;
            if (cpuDiff < 0) {
                prevCpuUsage[containerId] = cpuUsageNs;
                prevUpdateTime[containerId] = currentTime;
                return 0;
            }
            
            const cpuPercent = (cpuDiff / 1e9 / timeDiff) * 100;
            
            prevCpuUsage[containerId] = cpuUsageNs;
            prevUpdateTime[containerId] = currentTime;
            
            return Math.min(100, Math.max(0, cpuPercent));
        }

        function updateMonitor() {
            Promise.all([
                fetch('/api/containers').then(r => r.json()),
                fetch('/api/system').then(r => r.json())
            ])
                .then(([containerData, systemData]) => {
                    const tbody = document.getElementById('container-table-body');
                    const runningCount = document.getElementById('running-count');
                    const totalCount = document.getElementById('total-count');
                    const totalCpu = document.getElementById('total-cpu');
                    const totalMemory = document.getElementById('total-memory');
                    const availableMemory = document.getElementById('available-memory');
                    const updateTime = document.getElementById('update-time');

                    const running = containerData.containers ? containerData.containers.filter(c => c.state === 'RUNNING').length : 0;
                    const total = containerData.containers ? containerData.containers.length : 0;
                    runningCount.textContent = running;
                    totalCount.textContent = total;

                    if (systemData) {
                        if (systemData.total_container_cpu_percent !== undefined) {
                            totalCpu.textContent = parseFloat(systemData.total_container_cpu_percent).toFixed(1) + '%';
                        }
                        if (systemData.total_memory !== undefined) {
                            totalMemory.textContent = formatBytes(parseInt(systemData.total_memory));
                        }
                        if (systemData.available_memory !== undefined) {
                            availableMemory.textContent = formatBytes(parseInt(systemData.available_memory));
                        }
                    }

                    const now = new Date();
                    updateTime.textContent = now.toLocaleTimeString('en-US');

                    tbody.innerHTML = '';

                    if (!containerData.containers || containerData.containers.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="7" class="no-containers">No containers found</td></tr>';
                        return;
                    }

                    const sorted = [...containerData.containers].sort((a, b) => {
                        if (a.state === 'RUNNING' && b.state !== 'RUNNING') return -1;
                        if (a.state !== 'RUNNING' && b.state === 'RUNNING') return 1;
                        return (b.started_at || 0) - (a.started_at || 0);
                    });

                    sorted.forEach(container => {
                        const row = document.createElement('tr');
                        
                        const statusClass = 'status-' + container.state.toLowerCase();
                        const statusText = {
                            'created': 'CREATED',
                            'running': 'RUNNING',
                            'stopped': 'STOPPED',
                            'destroyed': 'DESTROYED'
                        }[container.state.toLowerCase()] || container.state;

                        let cpuDisplay = '--';
                        let cpuPercent = 0;
                        const currentTime = Date.now();
                        if (container.state === 'RUNNING') {
                            let cpuUsage = container.cpu_usage;
                            if (typeof cpuUsage === 'string') {
                                cpuUsage = parseFloat(cpuUsage);
                            }
                            if (cpuUsage !== undefined && cpuUsage !== null && !isNaN(cpuUsage) && cpuUsage >= 0) {
                                cpuPercent = calculateCPUPercent(container.id, cpuUsage, currentTime);
                                cpuDisplay = cpuPercent.toFixed(1) + '%';
                            } else {
                                cpuDisplay = '0.0%';
                            }
                        }

                        let memoryDisplay = '--';
                        let memoryPercent = 0;
                        if (container.state === 'RUNNING') {
                            let memoryUsage = container.memory_usage;
                            if (typeof memoryUsage === 'string') {
                                memoryUsage = parseFloat(memoryUsage);
                            }
                            if (memoryUsage !== undefined && memoryUsage !== null && !isNaN(memoryUsage) && memoryUsage >= 0) {
                                memoryDisplay = formatBytes(memoryUsage);
                            } else {
                                memoryDisplay = '0 B';
                            }
                        }

                        const runtime = container.started_at ? 
                            Math.floor(Date.now() / 1000) - container.started_at : 0;

                        const statusClassMap = {
                            'running': 'running',
                            'stopped': 'stopped',
                            'created': 'created'
                        };
                        const statusClass = statusClassMap[container.state.toLowerCase()] || '';

                        row.innerHTML = `
                            <td>${container.id}</td>
                            <td>${container.pid || '-----'}</td>
                            <td class="${statusClass}">${statusText}</td>
                            <td>${cpuDisplay}</td>
                            <td>${memoryDisplay}</td>
                            <td>${formatTime(runtime)}</td>
                            <td>${formatDate(container.created_at)}</td>
                        `;

                        tbody.appendChild(row);
                    });
                })
                .catch(error => {
                    console.error('Error:', error);
                    document.getElementById('container-table-body').innerHTML = 
                        '<tr><td colspan="7">Error loading data</td></tr>';
                });
        }

        updateMonitor();
        setInterval(updateMonitor, 5000);
    </script>
</body>
</html>
)HTML";
}
