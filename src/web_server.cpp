#include "web_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <algorithm>

WebServer::WebServer(container_manager_t* cm, int port)
    : cm_(cm), port_(port), running_(false), server_socket_(-1) {
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (running_) return;

    running_ = true;
    server_thread_ = std::thread(&WebServer::serverThread, this);
}

void WebServer::stop() {
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

void WebServer::serverThread() {
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    // Bind socket
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(server_socket_);
        return;
    }

    // Listen
    if (listen(server_socket_, 5) == -1) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(server_socket_);
        return;
    }

    std::cout << "Web server started on port " << port_ << std::endl;

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

std::string WebServer::handleRequest(const std::string& request) {
    std::string response;

    // Parse HTTP request
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html\r\n"
                      "Connection: close\r\n\r\n" +
                      generateHTML();
        } else if (path == "/monitor" || path == "/monitor.html") {
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/html\r\n"
                      "Connection: close\r\n\r\n" +
                      generateMonitorHTML();
        } else if (path == "/api/containers") {
            response = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Connection: close\r\n\r\n" +
                      getContainerListJSON();
        } else if (path.find("/api/containers/") == 0) {
            std::string id = path.substr(16); // Remove "/api/containers/"
            if (path.find("/logs") != std::string::npos) {
                // Extract container ID from path like "/api/containers/{id}/logs"
                size_t logs_pos = id.find("/logs");
                if (logs_pos != std::string::npos) {
                    id = id.substr(0, logs_pos);
                }
                response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/json\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Connection: close\r\n\r\n" +
                          getExecutionLogs(id);
            } else {
                response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/json\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Connection: close\r\n\r\n" +
                          getContainerInfoJSON(id);
            }
        } else {
            response = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n\r\n"
                      "Not Found";
        }
    } else if (method == "POST") {
        if (path == "/api/containers/run") {
            // Extract POST data (simplified)
            std::string post_data = request.substr(request.find("\r\n\r\n") + 4);

            // Parse form data
            std::string command = "echo 'Hello World'";
            std::string memory = "128";
            std::string cpu = "1024";
            std::string hostname = "container";
            std::string root_path = "/tmp/container_root";

            // Simple parsing (in production, use proper form parsing)
            size_t pos = 0;
            while ((pos = post_data.find('&', pos)) != std::string::npos) {
                std::string pair = post_data.substr(0, pos);
                size_t eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = pair.substr(0, eq_pos);
                    std::string value = pair.substr(eq_pos + 1);

                    if (key == "command") command = value;
                    else if (key == "memory") memory = value;
                    else if (key == "cpu") cpu = value;
                    else if (key == "hostname") hostname = value;
                    else if (key == "root_path") root_path = value;
                }
                post_data = post_data.substr(pos + 1);
            }

            if (run_callback_) {
                // Add execution log
                std::string container_id = "pending";
                addExecutionLog(container_id, "شروع ایجاد کانتینر...");
                addExecutionLog(container_id, "تنظیم محدودیت حافظه: " + memory + " MB");
                addExecutionLog(container_id, "تنظیم سهمیه CPU: " + cpu);
                addExecutionLog(container_id, "تنظیم hostname: " + hostname);
                addExecutionLog(container_id, "تنظیم root path: " + root_path);
                addExecutionLog(container_id, "اجرای دستور: " + command);
                
                std::string result = run_callback_(command, memory, cpu, hostname, root_path);
                
                // Parse container ID from result
                size_t id_pos = result.find("\"container_id\":\"");
                if (id_pos != std::string::npos) {
                    size_t id_start = id_pos + 16;
                    size_t id_end = result.find("\"", id_start);
                    if (id_end != std::string::npos) {
                        container_id = result.substr(id_start, id_end - id_start);
                        addExecutionLog(container_id, "کانتینر با موفقیت ایجاد شد: " + container_id);
                    }
                }
                
                response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: application/json\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Connection: close\r\n\r\n" +
                          result;
            } else {
                response = "HTTP/1.1 500 Internal Server Error\r\n"
                          "Content-Type: text/plain\r\n"
                          "Connection: close\r\n\r\n"
                          "Run callback not set";
            }
        } else {
            response = "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n\r\n"
                      "Not Found";
        }
    } else if (method == "OPTIONS") {
        response = "HTTP/1.1 200 OK\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\n"
                  "Connection: close\r\n\r\n";
    } else {
        response = "HTTP/1.1 405 Method Not Allowed\r\n"
                  "Content-Type: text/plain\r\n"
                  "Connection: close\r\n\r\n"
                  "Method Not Allowed";
    }

    return response;
}

std::string WebServer::generateMonitorHTML() {
    return R"HTML(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>مانیتور کانتینرها</title>
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

        .link {
            color: #00ff00;
            text-decoration: none;
            margin-left: 10px;
        }

        .link:hover {
            text-decoration: underline;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>مانیتور کانتینرها - Mini Container Monitor</h1>
        <a href="/" class="link">بازگشت به صفحه اصلی</a>
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
        فشردن F5 برای به‌روزرسانی | به‌روزرسانی خودکار هر 1 ثانیه
    </div>

    <script>
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

        // Store previous CPU usage for each container
        const prevCpuUsage = {};
        const prevUpdateTime = {};

        function calculateCPUPercent(containerId, cpuUsageNs, currentTime) {
            if (!cpuUsageNs || cpuUsageNs === 0) return 0;
            
            const prev = prevCpuUsage[containerId];
            const prevTime = prevUpdateTime[containerId];
            
            if (!prev || !prevTime) {
                // First measurement, store and return 0
                prevCpuUsage[containerId] = cpuUsageNs;
                prevUpdateTime[containerId] = currentTime;
                return 0;
            }
            
            const timeDiff = currentTime - prevTime;
            if (timeDiff <= 0) return 0;
            
            const cpuDiff = cpuUsageNs - prev;
            if (cpuDiff < 0) {
                // CPU counter might have reset, just store current
                prevCpuUsage[containerId] = cpuUsageNs;
                prevUpdateTime[containerId] = currentTime;
                return 0;
            }
            
            // CPU usage in nanoseconds, convert to percentage
            // Assuming 1 CPU core, 1 second = 1e9 nanoseconds
            // Percentage = (cpu_time_used / wall_clock_time) * 100
            const cpuPercent = (cpuDiff / 1e9 / timeDiff) * 100;
            
            // Update stored values
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

                    // Update stats
                    const running = data.containers ? data.containers.filter(c => c.state === 'RUNNING').length : 0;
                    const total = data.containers ? data.containers.length : 0;
                    runningCount.textContent = running;
                    totalCount.textContent = total;

                    // Update time
                    const now = new Date();
                    updateTime.textContent = now.toLocaleTimeString('fa-IR');

                    // Clear table
                    tbody.innerHTML = '';

                    if (!data.containers || data.containers.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="7" class="no-containers">هیچ کانتینری یافت نشد</td></tr>';
                        return;
                    }

                    // Sort: running first, then by start time
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
                            // Check if cpu_usage exists and is a valid number
                            const cpuUsage = container.cpu_usage;
                            if (cpuUsage !== undefined && cpuUsage !== null && typeof cpuUsage === 'number' && cpuUsage >= 0) {
                                cpuPercent = calculateCPUPercent(container.id, cpuUsage, currentTime);
                                cpuDisplay = cpuPercent.toFixed(1) + '%';
                            } else {
                                cpuDisplay = '0.0%';
                            }
                        }

                        let memoryDisplay = '--';
                        let memoryPercent = 0;
                        if (container.state === 'RUNNING') {
                            // Check if memory_usage exists and is a valid number
                            const memoryUsage = container.memory_usage;
                            if (memoryUsage !== undefined && memoryUsage !== null && typeof memoryUsage === 'number' && memoryUsage >= 0) {
                                memoryDisplay = formatBytes(memoryUsage);
                                // Assuming 128MB default limit for percentage calculation
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

std::string WebServer::generateHTML() {
    return R"HTML(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>مینی کانتینر - رابط گرافیکی</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
            background: #ffffff;
            color: #000000;
            direction: rtl;
            min-height: 100vh;
            margin: 0;
            padding: 0;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 40px 20px;
        }

        .header {
            text-align: center;
            margin-bottom: 40px;
            color: #000000;
        }

        .header h1 {
            font-size: 2em;
            margin-bottom: 10px;
            font-weight: 400;
        }

        .header p {
            font-size: 1em;
            color: #666666;
            font-weight: 300;
        }

        .card {
            background: #ffffff;
            border: 1px solid #e0e0e0;
            padding: 30px;
            margin-bottom: 30px;
        }

        .card h2 {
            color: #000000;
            margin-bottom: 20px;
            border-bottom: 1px solid #e0e0e0;
            padding-bottom: 10px;
            font-weight: 400;
            font-size: 1.3em;
        }

        .form-group {
            margin-bottom: 15px;
        }

        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 400;
            color: #000000;
            font-size: 0.9em;
        }

        .form-group input, .form-group select {
            width: 100%;
            padding: 10px;
            border: 1px solid #e0e0e0;
            font-size: 14px;
            background: #ffffff;
            color: #000000;
        }

        .form-group input:focus, .form-group select:focus {
            outline: none;
            border-color: #000000;
        }

        .btn {
            background: #000000;
            color: #ffffff;
            padding: 12px 24px;
            border: 1px solid #000000;
            cursor: pointer;
            font-size: 14px;
            font-weight: 400;
            width: 100%;
        }

        .btn:hover {
            background: #333333;
        }

        .btn:disabled {
            background: #cccccc;
            border-color: #cccccc;
            cursor: not-allowed;
        }

        .status {
            padding: 12px;
            margin-bottom: 15px;
            font-weight: 400;
            border: 1px solid #e0e0e0;
            background: #ffffff;
        }

        .status.success {
            color: #000000;
            border-color: #000000;
        }

        .status.error {
            color: #000000;
            border-color: #000000;
        }

        .status.info {
            color: #666666;
            border-color: #e0e0e0;
        }

        .container-list {
            margin-top: 20px;
        }

        .container-item {
            border: 1px solid #e0e0e0;
            padding: 15px;
            margin-bottom: 10px;
            background: #ffffff;
        }

        .container-item.running {
            border-color: #000000;
        }

        .container-item.stopped {
            border-color: #e0e0e0;
        }

        .container-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }

        .container-id {
            font-weight: 400;
            font-size: 16px;
        }

        .container-status {
            padding: 4px 8px;
            font-size: 12px;
            font-weight: 400;
            border: 1px solid #000000;
            background: #ffffff;
            color: #000000;
        }

        .container-status.running {
            border-color: #000000;
        }

        .container-status.stopped {
            border-color: #e0e0e0;
            color: #666666;
        }

        .container-status.created {
            border-color: #e0e0e0;
            color: #666666;
        }

        .container-details {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 10px;
            font-size: 14px;
        }

        .concept-explanation {
            background: #ffffff;
            border: 1px solid #e0e0e0;
            padding: 20px;
            margin-bottom: 30px;
        }

        .concept-title {
            font-weight: 400;
            color: #000000;
            margin-bottom: 10px;
            font-size: 0.9em;
        }

        .concept-desc {
            color: #666666;
            font-size: 13px;
            line-height: 1.6;
        }

        .progress-bar {
            width: 100%;
            height: 2px;
            background-color: #e0e0e0;
            overflow: hidden;
            margin: 10px 0;
        }

        .progress-fill {
            height: 100%;
            background: #000000;
            transition: width 0.3s ease;
        }

        .execution-log {
            background: #ffffff;
            border: 1px solid #e0e0e0;
            padding: 15px;
            margin-top: 15px;
            max-height: 300px;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            font-size: 12px;
        }

        .log-entry {
            padding: 5px 0;
            border-bottom: 1px solid #e9ecef;
        }

        .log-entry:last-child {
            border-bottom: none;
        }

        .log-time {
            color: #999999;
            margin-left: 10px;
        }

        @media (max-width: 768px) {
            .container {
                padding: 10px;
            }

            .header h1 {
                font-size: 2em;
            }

            .container-details {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>مینی کانتینر</h1>
            <p>سیستم کانتینر ساده برای نمایش مفاهیم سیستم‌عامل</p>
            <p style="margin-top: 10px;">
                <a href="/monitor" style="color: #000000; text-decoration: underline;">📊 مشاهده مانیتور (htop-like)</a>
            </p>
        </div>

        <div class="concept-explanation">
            <div class="concept-title">مفاهیم نمایش داده شده:</div>
            <div class="concept-desc">
                • فضای نام PID: ایزولاسیون شناسه فرایند<br>
                • فضای نام Mount: ایزولاسیون فایل‌سیستم<br>
                • فضای نام UTS: ایزولاسیون نام میزبان<br>
                • cgroups: مدیریت منابع CPU و حافظه<br>
                • chroot: ایزولاسیون دایرکتوری ریشه
            </div>
        </div>

        <div class="card">
            <h2>اجرای کانتینر جدید</h2>
            <div id="run-status" class="status info" style="display: none;"></div>

            <form id="run-form">
                <div class="form-group">
                    <label for="command">دستور اجرا:</label>
                    <input type="text" id="command" name="command" value="/bin/echo 'Hello from container!'" required>
                </div>

                <div class="form-group">
                    <label for="memory">محدودیت حافظه (MB):</label>
                    <input type="number" id="memory" name="memory" value="128" min="16" max="1024">
                </div>

                <div class="form-group">
                    <label for="cpu">سهمیه CPU:</label>
                    <input type="number" id="cpu" name="cpu" value="1024" min="64" max="4096">
                </div>

                <div class="form-group">
                    <label for="hostname">نام میزبان:</label>
                    <input type="text" id="hostname" name="hostname" value="mini-container">
                </div>

                <div class="form-group">
                    <label for="root_path">مسیر ریشه فایل‌سیستم:</label>
                    <input type="text" id="root_path" name="root_path" value="/tmp/container_root">
                </div>

                <button type="submit" class="btn" id="run-btn">اجرای کانتینر</button>
            </form>
            
            <div id="execution-log" class="execution-log" style="display: none;">
                <div style="font-weight: 400; margin-bottom: 10px; color: #000000;">روند اجرا:</div>
                <div id="log-entries"></div>
            </div>
        </div>

        <div class="card">
            <h2>لیست کانتینرها</h2>
            <div id="container-list" class="container-list">
                <div class="status info">در حال بارگذاری...</div>
            </div>
            <button class="btn" onclick="refreshContainers()">بروزرسانی لیست</button>
        </div>
    </div>

    <script>
        let refreshInterval;

        function showStatus(message, type = 'info') {
            const statusDiv = document.getElementById('run-status');
            statusDiv.textContent = message;
            statusDiv.className = `status ${type}`;
            statusDiv.style.display = 'block';

            setTimeout(() => {
                statusDiv.style.display = 'none';
            }, 5000);
        }

        function refreshContainers() {
            fetch('/api/containers')
                .then(response => response.json())
                .then(data => {
                    const containerList = document.getElementById('container-list');
                    containerList.innerHTML = '';

                    if (data.containers && data.containers.length > 0) {
                        data.containers.forEach(container => {
                            const containerDiv = document.createElement('div');
                            containerDiv.className = `container-item ${container.state.toLowerCase()}`;

                            const statusClass = container.state.toLowerCase();
                            const statusText = {
                                'created': 'ایجاد شده',
                                'running': 'در حال اجرا',
                                'stopped': 'متوقف شده',
                                'destroyed': 'نابود شده'
                            }[statusClass] || container.state;

                            containerDiv.innerHTML = `
                                <div class="container-header">
                                    <div class="container-id">${container.id}</div>
                                    <div class="container-status ${statusClass}">${statusText}</div>
                                </div>
                                <div class="container-details">
                                    <div><strong>PID:</strong> ${container.pid || 'N/A'}</div>
                                    <div><strong>ایجاد شده:</strong> ${new Date(container.created_at * 1000).toLocaleString('fa-IR')}</div>
                                    ${container.started_at ? `<div><strong>شروع شده:</strong> ${new Date(container.started_at * 1000).toLocaleString('fa-IR')}</div>` : ''}
                                    ${container.cpu_usage !== undefined ? `<div><strong>استفاده CPU:</strong> ${container.cpu_usage} ns</div>` : ''}
                                    ${container.memory_usage !== undefined ? `<div><strong>استفاده حافظه:</strong> ${container.memory_usage} bytes</div>` : ''}
                                </div>
                            `;

                            containerList.appendChild(containerDiv);
                        });
                    } else {
                        containerList.innerHTML = '<div class="status info">هیچ کانتینری یافت نشد</div>';
                    }
                })
                .catch(error => {
                    console.error('Error fetching containers:', error);
                    document.getElementById('container-list').innerHTML =
                        '<div class="status error">خطا در بارگذاری لیست کانتینرها</div>';
                });
        }

        let currentContainerId = null;
        let logPollInterval = null;

        function updateExecutionLog(containerId) {
            if (!containerId || containerId === 'pending') return;
            
            fetch(`/api/containers/${containerId}/logs`)
                .then(response => response.json())
                .then(logs => {
                    const logDiv = document.getElementById('execution-log');
                    const logEntries = document.getElementById('log-entries');
                    
                    if (logs && logs.length > 0) {
                        logDiv.style.display = 'block';
                        logEntries.innerHTML = '';
                        logs.forEach(log => {
                            const entry = document.createElement('div');
                            entry.className = 'log-entry';
                            const time = new Date().toLocaleTimeString('fa-IR');
                            entry.innerHTML = `<span class="log-time">[${time}]</span> ${log}`;
                            logEntries.appendChild(entry);
                        });
                        logEntries.scrollTop = logEntries.scrollHeight;
                    }
                })
                .catch(error => console.error('Error fetching logs:', error));
        }

        document.getElementById('run-form').addEventListener('submit', function(e) {
            e.preventDefault();

            const formData = new FormData(this);
            const btn = document.getElementById('run-btn');
            const originalText = btn.textContent;
            const logDiv = document.getElementById('execution-log');
            const logEntries = document.getElementById('log-entries');

            btn.disabled = true;
            btn.textContent = 'در حال اجرا...';
            logDiv.style.display = 'block';
            logEntries.innerHTML = '<div class="log-entry">در حال شروع...</div>';
            currentContainerId = 'pending';

            // Start polling for logs
            if (logPollInterval) clearInterval(logPollInterval);
            logPollInterval = setInterval(() => {
                if (currentContainerId) {
                    updateExecutionLog(currentContainerId);
                }
            }, 500);

            fetch('/api/containers/run', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: new URLSearchParams(formData)
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    currentContainerId = data.container_id;
                    showStatus(`کانتینر با موفقیت اجرا شد: ${data.container_id}`, 'success');
                    updateExecutionLog(data.container_id);
                    setTimeout(refreshContainers, 1000);
                } else {
                    showStatus(`خطا در اجرای کانتینر: ${data.error}`, 'error');
                    if (logPollInterval) clearInterval(logPollInterval);
                }
            })
            .catch(error => {
                console.error('Error:', error);
                showStatus('خطا در ارتباط با سرور', 'error');
                if (logPollInterval) clearInterval(logPollInterval);
            })
            .finally(() => {
                btn.disabled = false;
                btn.textContent = originalText;
            });
        });

        // Auto refresh every 5 seconds
        refreshInterval = setInterval(refreshContainers, 5000);

        // Initial load
        refreshContainers();

        // Cleanup on page unload
        window.addEventListener('beforeunload', function() {
            if (refreshInterval) {
                clearInterval(refreshInterval);
            }
        });
    </script>
</body>
</html>
)HTML";
}

std::string WebServer::getContainerListJSON() {
    std::string json = "{\"containers\":[";

    if (list_callback_) {
        json = list_callback_();
    } else {
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
    }

    json += "]}";
    return json;
}

std::string WebServer::getContainerInfoJSON(const std::string& id) {
    if (info_callback_) {
        return info_callback_(id);
    }

    container_info_t* info = container_manager_get_info(cm_, id.c_str());
    if (!info) {
        return "{\"error\":\"Container not found\"}";
    }

    unsigned long cpu_usage = 0, memory_usage = 0;
    if (info->state == CONTAINER_RUNNING) {
        resource_manager_get_stats(cm_->rm, info->id, &cpu_usage, &memory_usage);
    }

    const char* state_names[] = {"CREATED", "RUNNING", "STOPPED", "DESTROYED"};
    std::string json = "{";
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

    return json;
}

void WebServer::addExecutionLog(const std::string& container_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    execution_logs_[container_id].push_back(message);
}

std::string WebServer::getExecutionLogs(const std::string& container_id) {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    auto it = execution_logs_.find(container_id);
    if (it == execution_logs_.end()) {
        return "[]";
    }

    std::string json = "[";
    for (size_t i = 0; i < it->second.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + it->second[i] + "\"";
    }
    json += "]";

    return json;
}