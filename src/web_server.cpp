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
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
            direction: rtl;
            min-height: 100vh;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }

        .header {
            text-align: center;
            margin-bottom: 30px;
            color: white;
        }

        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }

        .header p {
            font-size: 1.2em;
            opacity: 0.9;
        }

        .card {
            background: white;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }

        .card h2 {
            color: #667eea;
            margin-bottom: 15px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }

        .form-group {
            margin-bottom: 15px;
        }

        .form-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
            color: #555;
        }

        .form-group input, .form-group select {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 16px;
            transition: border-color 0.3s;
        }

        .form-group input:focus, .form-group select:focus {
            outline: none;
            border-color: #667eea;
        }

        .btn {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 12px 24px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            font-weight: bold;
            transition: transform 0.2s;
            width: 100%;
        }

        .btn:hover {
            transform: translateY(-2px);
        }

        .btn:disabled {
            background: #ccc;
            cursor: not-allowed;
            transform: none;
        }

        .status {
            padding: 10px;
            border-radius: 5px;
            margin-bottom: 15px;
            font-weight: bold;
        }

        .status.success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }

        .status.error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }

        .status.info {
            background-color: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }

        .container-list {
            margin-top: 20px;
        }

        .container-item {
            border: 1px solid #ddd;
            border-radius: 5px;
            padding: 15px;
            margin-bottom: 10px;
            background: #f9f9f9;
        }

        .container-item.running {
            border-color: #28a745;
            background: #d4edda;
        }

        .container-item.stopped {
            border-color: #dc3545;
            background: #f8d7da;
        }

        .container-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }

        .container-id {
            font-weight: bold;
            font-size: 18px;
        }

        .container-status {
            padding: 4px 8px;
            border-radius: 3px;
            font-size: 12px;
            font-weight: bold;
        }

        .container-status.running {
            background: #28a745;
            color: white;
        }

        .container-status.stopped {
            background: #dc3545;
            color: white;
        }

        .container-status.created {
            background: #ffc107;
            color: black;
        }

        .container-details {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 10px;
            font-size: 14px;
        }

        .concept-explanation {
            background: #f8f9fa;
            border-radius: 8px;
            padding: 15px;
            margin-bottom: 15px;
        }

        .concept-title {
            font-weight: bold;
            color: #667eea;
            margin-bottom: 5px;
        }

        .concept-desc {
            color: #666;
            font-size: 14px;
        }

        .progress-bar {
            width: 100%;
            height: 20px;
            background-color: #f0f0f0;
            border-radius: 10px;
            overflow: hidden;
            margin: 10px 0;
        }

        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #28a745, #20c997);
            transition: width 0.3s ease;
        }

        .execution-log {
            background: #f8f9fa;
            border: 1px solid #dee2e6;
            border-radius: 5px;
            padding: 15px;
            margin-top: 15px;
            max-height: 300px;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            font-size: 13px;
        }

        .log-entry {
            padding: 5px 0;
            border-bottom: 1px solid #e9ecef;
        }

        .log-entry:last-child {
            border-bottom: none;
        }

        .log-time {
            color: #6c757d;
            margin-left: 10px;
        }

        .test-section {
            margin-top: 20px;
        }

        .test-results {
            margin-top: 15px;
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
            max-height: 400px;
            overflow-y: auto;
        }

        .test-step {
            padding: 10px;
            margin-bottom: 10px;
            border-radius: 5px;
            border-right: 4px solid #667eea;
        }

        .test-step.success {
            background: #d4edda;
            border-right-color: #28a745;
        }

        .test-step.error {
            background: #f8d7da;
            border-right-color: #dc3545;
        }

        .test-step.info {
            background: #d1ecf1;
            border-right-color: #17a2b8;
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
                <div style="font-weight: bold; margin-bottom: 10px; color: #667eea;">روند اجرا:</div>
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