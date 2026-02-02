#ifndef WEB_SERVER_SIMPLE_HPP
#define WEB_SERVER_SIMPLE_HPP
#include <string>
#include <thread>
#include <atomic>
#include "../include/container_manager.hpp"
class SimpleWebServer {
public:
    SimpleWebServer(container_manager_t* cm, int port = 808);
    ~SimpleWebServer();
    void start();
    void stop();
private:
    void serverThread();
    std::string handleRequest(const std::string& request);
    std::string generateHTML();
    std::string getContainerListJSON();
    std::string getSystemInfoJSON();
    unsigned long getSystemTotalMemory();
    unsigned long getSystemAvailableMemory();
    double getSystemCPUPercent();
    container_manager_t* cm_;
    int port_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    int server_socket_;
};
#endif