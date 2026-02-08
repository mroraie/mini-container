#include "../include/web_server_simple.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>
#include <ctime>
#include <vector>
#include <cstdlib>
#include <map>
#include <string>
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
static unsigned long read_cgroup_limit(const char* path) {
    char buffer[256];
    std::ifstream file(path);
    if (!file.is_open()) {
        return 0;
    }
    file.getline(buffer, sizeof(buffer));
    file.close();
    if (strcmp(buffer, "max") == 0) {
        return 0;
    }
    char* endptr;
    unsigned long val = strtoul(buffer, &endptr, 10);
    if (*endptr == '\0' || *endptr == '\n' || *endptr == ' ') {
        return val;
    }
    return 0;
}
std::string SimpleWebServer::getContainerListJSON() {
        static std::map<std::string, unsigned long> prev_cpu_usage;
        static std::map<std::string, time_t> prev_time;
        std::string json = "{\"containers\":[";
        int count;
        container_info_t** containers = container_manager_list(cm_, &count);
        std::vector<container_info_t*> active_containers;
        for (int i = 0; i < count; i++) {
            if (containers[i]->state != CONTAINER_DESTROYED) {
                active_containers.push_back(containers[i]);
            }
        }
        time_t current_time = time(nullptr);
        for (size_t i = 0; i < active_containers.size(); i++) {
            container_info_t* info = active_containers[i];
            if (i > 0) json += ",";
            unsigned long cpu_usage = 0, memory_usage = 0;
            unsigned long cpu_limit = 0, memory_limit = 0;
            double cpu_percent = 0.0;
            double memory_percent = 0.0;
            if (info->state == CONTAINER_RUNNING) {
                resource_manager_get_stats(cm_->rm, info->id, &cpu_usage, &memory_usage);
                char path[1024];
                unsigned long cpu_quota_us = 0;
                unsigned long cpu_period_us = 100000;
                if (cm_->rm->version == CGROUP_V2) {
                    snprintf(path, sizeof(path), "%s/%s_%s/cpu.max", "/sys/fs/cgroup", cm_->rm->cgroup_path, info->id);
                    std::string cpu_max_line;
                    std::ifstream cpu_file(path);
                    if (cpu_file.is_open()) {
                        std::getline(cpu_file, cpu_max_line);
                        cpu_file.close();
                        if (cpu_max_line != "max") {
                            size_t space_pos = cpu_max_line.find(' ');
                            if (space_pos != std::string::npos) {
                                cpu_quota_us = std::stoul(cpu_max_line.substr(0, space_pos));
                                cpu_period_us = std::stoul(cpu_max_line.substr(space_pos + 1));
                            }
                        }
                    }
                    snprintf(path, sizeof(path), "%s/%s_%s/memory.max", "/sys/fs/cgroup", cm_->rm->cgroup_path, info->id);
                    memory_limit = read_cgroup_limit(path);
                } else {
                    char cpu_path[512];
                    snprintf(cpu_path, sizeof(cpu_path), "%s/%s_%s", "/sys/fs/cgroup/cpu,cpuacct", cm_->rm->cgroup_path, info->id);
                    if (access(cpu_path, F_OK) != 0) {
                        snprintf(cpu_path, sizeof(cpu_path), "%s/%s_%s", "/sys/fs/cgroup/cpu", cm_->rm->cgroup_path, info->id);
                    }
                    int ret = snprintf(path, sizeof(path), "%s/cpu.cfs_quota_us", cpu_path);
                    if (ret > 0 && (size_t)ret < sizeof(path)) {
                        cpu_quota_us = read_cgroup_limit(path);
                    }
                    ret = snprintf(path, sizeof(path), "%s/cpu.cfs_period_us", cpu_path);
                    if (ret > 0 && (size_t)ret < sizeof(path)) {
                        unsigned long period = read_cgroup_limit(path);
                        if (period > 0) {
                            cpu_period_us = period;
                        }
                    }
                    snprintf(path, sizeof(path), "%s/%s_%s/memory.limit_in_bytes", "/sys/fs/cgroup/memory", cm_->rm->cgroup_path, info->id);
                    memory_limit = read_cgroup_limit(path);
                }
                std::string container_id_str = std::string(info->id);
                if (prev_cpu_usage.find(container_id_str) != prev_cpu_usage.end() &&
                    prev_time.find(container_id_str) != prev_time.end()) {
                    unsigned long cpu_diff = cpu_usage - prev_cpu_usage[container_id_str];
                    time_t time_diff = current_time - prev_time[container_id_str];
                    if (time_diff > 0 && cpu_diff > 0) {
                        double cpu_diff_sec = (double)cpu_diff / 1000000000.0;
                        double time_diff_sec = (double)time_diff;
                        if (time_diff_sec > 0) {
                            double cpu_usage_ratio = cpu_diff_sec / time_diff_sec;
                            if (cpu_quota_us > 0 && cpu_period_us > 0) {
                                cpu_limit = cpu_quota_us;
                                double max_cpu = (double)cpu_quota_us / (double)cpu_period_us;
                                cpu_percent = 100.0 * (cpu_usage_ratio / max_cpu);
                            } else {
                                cpu_percent = 100.0 * cpu_usage_ratio;
                            }
                            if (cpu_percent > 100.0) cpu_percent = 100.0;
                            if (cpu_percent < 0.0) cpu_percent = 0.0;
                        }
                    }
                }
                prev_cpu_usage[container_id_str] = cpu_usage;
                prev_time[container_id_str] = current_time;
                if (memory_limit > 0) {
                    memory_percent = 100.0 * ((double)memory_usage / (double)memory_limit);
                    if (memory_percent > 100.0) memory_percent = 100.0;
                } else {
                    memory_percent = 0.0;
                }
            } else {
                std::string container_id_str = std::string(info->id);
                prev_cpu_usage.erase(container_id_str);
                prev_time.erase(container_id_str);
            }
            const char* state_names[] = {"CREATED", "RUNNING", "STOPPED", "DESTROYED"};
            const char* state_str = "UNKNOWN";
            if ((int)info->state >= 0 && (int)info->state < (int)(sizeof(state_names) / sizeof(state_names[0]))) {
                state_str = state_names[info->state];
            }
            json += "{";
            json += "\"id\":\"" + std::string(info->id) + "\",";
            json += "\"pid\":" + std::to_string(info->pid) + ",";
            json += "\"state\":\"" + std::string(state_str) + "\"";
            if (info->state == CONTAINER_RUNNING) {
                json += ",\"cpu_usage\":" + std::to_string(cpu_usage);
                json += ",\"cpu_limit\":" + std::to_string(cpu_limit);
                json += ",\"cpu_percent\":" + std::to_string(cpu_percent);
                json += ",\"memory_usage\":" + std::to_string(memory_usage);
                json += ",\"memory_limit\":" + std::to_string(memory_limit);
                json += ",\"memory_percent\":" + std::to_string(memory_percent);
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
    json += "\"total_memory\":" + std::to_string(total_mem) + ",";
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
    <title>Container Monitor</title>
    <script src="https:
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; margin-bottom: 30px; }
        .container-section { margin-bottom: 40px; padding: 20px; background: #f8f8f8; border-radius: 8px; }
        .container-title { font-size: 20px; font-weight: bold; margin-bottom: 20px; color: #333; text-align: center; }
        .charts { display: flex; justify-content: space-around; flex-wrap: wrap; gap: 30px; }
        .chart-wrapper { text-align: center; }
        .chart-container { width: 250px; height: 250px; position: relative; margin: 0 auto; }
        .chart-title { text-align: center; font-size: 16px; font-weight: bold; margin-bottom: 10px; color: #555; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Container Monitor</h1>
        <div id="containers-list"></div>
    </div>
    <script>
        const charts = {};
        function formatBytes(bytes) {
            if (!bytes || bytes === 0) return '0 MB';
            const mb = bytes / (1024 * 1024);
            return mb.toFixed(2) + ' MB';
        }
        function createChart(containerId, type, canvasId) {
            const ctx = document.getElementById(canvasId).getContext('2d');
            const color = type === 'cpu' ? '#2196F3' : '#FF9800';
            return new Chart(ctx, {
                type: 'doughnut',
                data: {
                    labels: ['Used', 'Free'],
                    datasets: [{
                        data: [0, 100],
                        backgroundColor: [color, '#E0E0E0'],
                        borderWidth: 0
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    plugins: {
                        legend: {
                            display: true,
                            position: 'bottom'
                        },
                        tooltip: {
                            callbacks: {
                                label: function(context) {
                                    return context.label + ': ' + context.parsed.toFixed(1) + '%';
                                }
                            }
                        }
                    }
                }
            });
        }
        function updateContainers() {
            fetch('/api/containers')
                .then(r => r.json())
                .then(data => {
                    const containersList = document.getElementById('containers-list');
                    const existingContainers = new Set();
                    if (!data.containers || data.containers.length === 0) {
                        containersList.innerHTML = '<p style="text-align: center; color: #666;">No active containers</p>';
                        charts = {};
                        return;
                    }
                    data.containers.forEach(container => {
                        existingContainers.add(container.id);
                        const containerId = 'container-' + container.id;
                        let containerDiv = document.getElementById(containerId);
                        if (!containerDiv) {
                            containerDiv = document.createElement('div');
                            containerDiv.className = 'container-section';
                            containerDiv.id = containerId;
                            const containerTitle = document.createElement('div');
                            containerTitle.className = 'container-title';
                            containerTitle.textContent = 'Container: ' + container.id;
                            containerDiv.appendChild(containerTitle);
                            const chartsDiv = document.createElement('div');
                            chartsDiv.className = 'charts';
                            const cpuWrapper = document.createElement('div');
                            cpuWrapper.className = 'chart-wrapper';
                            const cpuTitle = document.createElement('div');
                            cpuTitle.className = 'chart-title';
                            cpuTitle.textContent = 'CPU Usage';
                            cpuWrapper.appendChild(cpuTitle);
                            const cpuCanvas = document.createElement('canvas');
                            cpuCanvas.id = 'cpu-' + container.id;
                            const cpuContainer = document.createElement('div');
                            cpuContainer.className = 'chart-container';
                            cpuContainer.appendChild(cpuCanvas);
                            cpuWrapper.appendChild(cpuContainer);
                            chartsDiv.appendChild(cpuWrapper);
                            const ramWrapper = document.createElement('div');
                            ramWrapper.className = 'chart-wrapper';
                            const ramTitle = document.createElement('div');
                            ramTitle.className = 'chart-title';
                            ramTitle.textContent = 'RAM Usage';
                            ramWrapper.appendChild(ramTitle);
                            const ramCanvas = document.createElement('canvas');
                            ramCanvas.id = 'ram-' + container.id;
                            const ramContainer = document.createElement('div');
                            ramContainer.className = 'chart-container';
                            ramContainer.appendChild(ramCanvas);
                            ramWrapper.appendChild(ramContainer);
                            chartsDiv.appendChild(ramWrapper);
                            containerDiv.appendChild(chartsDiv);
                            containersList.appendChild(containerDiv);
                            charts['cpu-' + container.id] = createChart(container.id, 'cpu', 'cpu-' + container.id);
                            charts['ram-' + container.id] = createChart(container.id, 'ram', 'ram-' + container.id);
                        }
                        if (container.state === 'RUNNING') {
                            if (container.cpu_percent !== undefined && !isNaN(container.cpu_percent)) {
                                const cpuPercent = Math.min(100, Math.max(0, parseFloat(container.cpu_percent)));
                                if (charts['cpu-' + container.id]) {
                                    charts['cpu-' + container.id].data.datasets[0].data = [cpuPercent, 100 - cpuPercent];
                                    charts['cpu-' + container.id].update('none');
                                }
                            }
                            if (container.memory_percent !== undefined && !isNaN(container.memory_percent)) {
                                const memPercent = Math.min(100, Math.max(0, parseFloat(container.memory_percent)));
                                if (charts['ram-' + container.id]) {
                                    charts['ram-' + container.id].data.datasets[0].data = [memPercent, 100 - memPercent];
                                    charts['ram-' + container.id].update('none');
                                }
                            }
                        } else {
                            if (charts['cpu-' + container.id]) {
                                charts['cpu-' + container.id].data.datasets[0].data = [0, 100];
                                charts['cpu-' + container.id].update('none');
                            }
                            if (charts['ram-' + container.id]) {
                                charts['ram-' + container.id].data.datasets[0].data = [0, 100];
                                charts['ram-' + container.id].update('none');
                            }
                        }
                    });
                    Object.keys(charts).forEach(chartKey => {
                        const containerId = chartKey.replace('cpu-', '').replace('ram-', '');
                        if (!existingContainers.has(containerId)) {
                            const containerDiv = document.getElementById('container-' + containerId);
                            if (containerDiv) {
                                containerDiv.remove();
                            }
                            delete charts[chartKey];
                        }
                    });
                })
                .catch(error => {
                    console.error('Error:', error);
                });
        }
        updateContainers();
        setInterval(updateContainers, 10000);
    </script>
</body>
</html>
)HTML";
}