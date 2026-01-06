#ifndef WEB_SERVER_HPP
#define WEB_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "container_manager.hpp"

class WebServer {
public:
    WebServer(container_manager_t* cm, int port = 8080);
    ~WebServer();

    void start();
    void stop();
    bool isRunning() const { return running_; }

    // Callback functions for container operations
    using ContainerListCallback = std::function<std::string()>;
    using ContainerInfoCallback = std::function<std::string(const std::string&)>;
    using ContainerRunCallback = std::function<std::string(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&)>;
    using ExecutionLogCallback = std::function<std::string(const std::string&)>;

    void setContainerListCallback(ContainerListCallback cb) { list_callback_ = cb; }
    void setContainerInfoCallback(ContainerInfoCallback cb) { info_callback_ = cb; }
    void setContainerRunCallback(ContainerRunCallback cb) { run_callback_ = cb; }
    void setRunCallback(ContainerRunCallback cb) { run_callback_ = cb; } // Alias for convenience
    void setExecutionLogCallback(ExecutionLogCallback cb) { execution_log_callback_ = cb; }

    // Execution tracking methods
    void addExecutionLog(const std::string& container_id, const std::string& message);
    std::string getExecutionLogs(const std::string& container_id);

private:
    void serverThread();
    std::string handleRequest(const std::string& request);
    std::string generateHTML();
    std::string getContainerListJSON();
    std::string getContainerInfoJSON(const std::string& id);

    container_manager_t* cm_;
    int port_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    int server_socket_;

    ContainerListCallback list_callback_;
    ContainerInfoCallback info_callback_;
    ContainerRunCallback run_callback_;
    ExecutionLogCallback execution_log_callback_;

    // Execution tracking
    std::unordered_map<std::string, std::vector<std::string>> execution_logs_;
    std::mutex logs_mutex_;
};

#endif // WEB_SERVER_HPP
