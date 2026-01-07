#include "web_server_simple.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iostream>

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
    // Create socket
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == -1) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return;
        }

        // Set SO_REUSEADDR
        int reuse = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            std::cerr << "Warning: Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        }

        // Bind socket
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

        // Listen
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

            // Handle request
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
            json += "{";
            json += "\"id\":\"" + std::string(info->id) + "\",";
            json += "\"pid\":" + std::to_string(info->pid) + ",";
            json += "\"state\":\"" + std::string(state_names[info->state]) + "\",";
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

std::string SimpleWebServer::generateHTML() {
        return R"HTML(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mini Container Monitor - Live</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Courier New', monospace;
            background: #000000;
            color: #00ff00;
            direction: rtl;
            padding: 10px;
            font-size: 12px;
        }

        .header {
            border-bottom: 1px solid #00ff00;
            padding: 10px 0;
            margin-bottom: 10px;
        }

        .header h1 {
            font-size: 16px;
            font-weight: normal;
            color: #00ff00;
        }

        .stats-bar {
            display: flex;
            gap: 20px;
            margin-bottom: 10px;
            padding: 5px 0;
            border-bottom: 1px solid #333333;
        }

        .stat-item {
            display: flex;
            gap: 5px;
        }

        .stat-label {
            color: #888888;
        }

        .stat-value {
            color: #00ff00;
        }

        .table {
            width: 100%;
            border-collapse: collapse;
        }

        .table thead {
            border-bottom: 1px solid #00ff00;
        }

        .table th {
            text-align: right;
            padding: 5px 10px;
            font-weight: normal;
            color: #00ff00;
            border-bottom: 1px solid #333333;
        }

        .table td {
            padding: 3px 10px;
            border-bottom: 1px solid #222222;
        }

        .table tr:hover {
            background: #111111;
        }

        .status-running {
            color: #00ff00;
        }

        .status-stopped {
            color: #ff0000;
        }

        .status-created {
            color: #ffff00;
        }

        .cpu-bar {
            display: inline-block;
            width: 60px;
            height: 10px;
            background: #222222;
            border: 1px solid #00ff00;
            position: relative;
            vertical-align: middle;
        }

        .cpu-bar-fill {
            height: 100%;
            background: #00ff00;
            transition: width 0.3s;
        }

        .memory-bar {
            display: inline-block;
            width: 60px;
            height: 10px;
            background: #222222;
            border: 1px solid #00ff00;
            position: relative;
            vertical-align: middle;
        }

        .memory-bar-fill {
            height: 100%;
            background: #00ff00;
            transition: width 0.3s;
        }

        .footer {
            margin-top: 10px;
            padding-top: 10px;
            border-top: 1px solid #333333;
            color: #888888;
            font-size: 10px;
            text-align: center;
        }

        .no-containers {
            text-align: center;
            padding: 20px;
            color: #888888;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>مانیتور کانتینرها - Mini Container Monitor (Live)</h1>
    </div>

    <div class="stats-bar">
        <div class="stat-item">
            <span class="stat-label">کانتینرهای فعال:</span>
            <span class="stat-value" id="running-count">0</span>
        </div>
        <div class="stat-item">
            <span class="stat-label">کل کانتینرها:</span>
            <span class="stat-value" id="total-count">0</span>
        </div>
        <div class="stat-item">
            <span class="stat-label">به‌روزرسانی:</span>
            <span class="stat-value" id="update-time">--:--:--</span>
        </div>
    </div>

    <table class="table">
        <thead>
            <tr>
                <th style="width: 20%;">ID</th>
                <th style="width: 10%;">PID</th>
                <th style="width: 10%;">وضعیت</th>
                <th style="width: 15%;">CPU</th>
                <th style="width: 15%;">حافظه</th>
                <th style="width: 15%;">زمان اجرا</th>
                <th style="width: 15%;">زمان ایجاد</th>
            </tr>
        </thead>
        <tbody id="container-table-body">
            <tr>
                <td colspan="7" class="no-containers">در حال بارگذاری...</td>
            </tr>
        </tbody>
    </table>

    <div class="footer">
        به‌روزرسانی خودکار هر 1 ثانیه | فشردن F5 برای به‌روزرسانی دستی
    </div>

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
            return date.toLocaleTimeString('fa-IR');
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
            fetch('/api/containers')
                .then(response => response.json())
                .then(data => {
                    const tbody = document.getElementById('container-table-body');
                    const runningCount = document.getElementById('running-count');
                    const totalCount = document.getElementById('total-count');
                    const updateTime = document.getElementById('update-time');

                    const running = data.containers ? data.containers.filter(c => c.state === 'RUNNING').length : 0;
                    const total = data.containers ? data.containers.length : 0;
                    runningCount.textContent = running;
                    totalCount.textContent = total;

                    const now = new Date();
                    updateTime.textContent = now.toLocaleTimeString('fa-IR');

                    tbody.innerHTML = '';

                    if (!data.containers || data.containers.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="7" class="no-containers">هیچ کانتینری یافت نشد</td></tr>';
                        return;
                    }

                    const sorted = [...data.containers].sort((a, b) => {
                        if (a.state === 'RUNNING' && b.state !== 'RUNNING') return -1;
                        if (a.state !== 'RUNNING' && b.state === 'RUNNING') return 1;
                        return (b.started_at || 0) - (a.started_at || 0);
                    });

                    sorted.forEach(container => {
                        const row = document.createElement('tr');
                        
                        const statusClass = 'status-' + container.state.toLowerCase();
                        const statusText = {
                            'created': 'ایجاد شده',
                            'running': 'در حال اجرا',
                            'stopped': 'متوقف شده',
                            'destroyed': 'نابود شده'
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
                                const limit = 128 * 1024 * 1024;
                                memoryPercent = (memoryUsage / limit) * 100;
                                memoryPercent = Math.min(100, Math.max(0, memoryPercent));
                            } else {
                                memoryDisplay = '0 B';
                            }
                        }

                        const runtime = container.started_at ? 
                            Math.floor(Date.now() / 1000) - container.started_at : 0;

                        row.innerHTML = `
                            <td>${container.id}</td>
                            <td>${container.pid || '--'}</td>
                            <td class="${statusClass}">${statusText}</td>
                            <td>
                                ${container.state === 'RUNNING' ? `
                                    <div class="cpu-bar">
                                        <div class="cpu-bar-fill" style="width: ${cpuPercent}%"></div>
                                    </div>
                                    ${cpuDisplay}
                                ` : '--'}
                            </td>
                            <td>
                                ${container.state === 'RUNNING' ? `
                                    <div class="memory-bar">
                                        <div class="memory-bar-fill" style="width: ${memoryPercent}%"></div>
                                    </div>
                                    ${memoryDisplay}
                                ` : '--'}
                            </td>
                            <td>${formatTime(runtime)}</td>
                            <td>${formatDate(container.created_at)}</td>
                        `;

                        tbody.appendChild(row);
                    });
                })
                .catch(error => {
                    console.error('Error:', error);
                    document.getElementById('container-table-body').innerHTML = 
                        '<tr><td colspan="7" class="no-containers" style="color: #ff0000;">خطا در بارگذاری داده‌ها</td></tr>';
                });
        }

        // Initial load
        updateMonitor();

        // Auto refresh every 1 second
        setInterval(updateMonitor, 1000);

        // Also refresh on F5
        document.addEventListener('keydown', function(e) {
            if (e.key === 'F5') {
                e.preventDefault();
                updateMonitor();
            }
        });
    </script>
</body>
</html>
)HTML";
}
