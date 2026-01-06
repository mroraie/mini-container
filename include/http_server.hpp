#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <functional>
#include <unordered_map>

class HttpServer {
public:
    HttpServer(int port = 8080);
    ~HttpServer();

    // Start the server
    bool start();

    // Stop the server
    void stop();

    // Register a route handler
    void registerRoute(const std::string& path, std::function<std::string()> handler);

    // Get server port
    int getPort() const { return port_; }

private:
    int port_;
    int server_socket_;
    bool running_;
    std::unordered_map<std::string, std::function<std::string()>> routes_;

    // Handle incoming connections
    void handleConnections();

    // Process HTTP request
    std::string processRequest(const std::string& request);

    // Parse HTTP request
    std::string getPathFromRequest(const std::string& request);
};

#endif // HTTP_SERVER_HPP
