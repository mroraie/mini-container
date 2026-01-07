#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cstring>
#include "../include/container_manager.hpp"
#include "web_server_simple.hpp"

using namespace std;

static container_manager_t cm;
static SimpleWebServer* web_server = nullptr;
static bool running = true;

void signal_handler(int signum) {
    (void)signum;
    cout << "\nShutting down web server..." << endl;
    running = false;

    if (web_server) {
        web_server->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 808;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    if (container_manager_init(&cm, 10) != 0) {
        cerr << "Failed to initialize container manager" << endl;
        return EXIT_FAILURE;
    }

    if (getuid() != 0) {
        cout << "Warning: Container operations typically require root privileges" << endl;
    }

    web_server = new SimpleWebServer(&cm, port);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    web_server->start();

    cout << "Web server started on port " << port << endl;
    cout << "Open http://localhost:" << port << " in your browser" << endl;
    cout << "Press Ctrl+C to stop" << endl;

    while (running) {
        sleep(1);
    }

    delete web_server;
    container_manager_cleanup(&cm);

    cout << "Web server stopped" << endl;
    return EXIT_SUCCESS;
}


