#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
// Windows demo mode - no actual container functionality
#define DEMO_MODE
#else
#include "../include/container_manager.hpp"
#endif

#ifdef _WIN32
enum container_state_t {
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_DESTROYED
};
#endif

struct ContainerDisplay {
    std::string id;
    container_state_t state;
    int pid;
    time_t created_at;
    time_t started_at;
    unsigned long cpu_usage;
    unsigned long memory_usage;
};

class TerminalUI {
private:
#ifdef _WIN32
    void* cm; // Dummy for Windows demo
#else
    container_manager_t cm;
#endif
    std::vector<ContainerDisplay> containers;
    bool running;

    void clearScreen() {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }

    void drawHeader() {
        std::cout << "+==============================================================================+\n";
        std::cout << "|                          سیستم مینی کانتینر                               |\n";
        std::cout << "|                              Mini Container System                          |\n";
        std::cout << "+==============================================================================+\n";
        std::cout << "\n";
    }

    void drawContainer(ContainerDisplay& container, int index) {
        std::string state_str;
        std::string state_symbol;

        switch(container.state) {
            case CONTAINER_CREATED:
                state_str = "ایجاد شده";
                state_symbol = "[C]";
                break;
            case CONTAINER_RUNNING:
                state_str = "در حال اجرا";
                state_symbol = "[R]";
                break;
            case CONTAINER_STOPPED:
                state_str = "متوقف شده";
                state_symbol = "[S]";
                break;
            case CONTAINER_DESTROYED:
                state_str = "نابود شده";
                state_symbol = "[D]";
                break;
        }

        std::cout << "+--- کانتینر " << std::setw(2) << index + 1 << " ";
        std::cout << std::string(60, '-') << "+\n";
        std::cout << "| ID: " << std::left << std::setw(30) << container.id;
        std::cout << "وضعیت: " << state_symbol << " " << state_str << std::string(15, ' ') << "|\n";

        if (container.pid > 0) {
            std::cout << "| PID: " << std::left << std::setw(10) << container.pid;
        } else {
            std::cout << "| PID: " << std::left << std::setw(10) << "N/A";
        }

        if (container.state == CONTAINER_RUNNING) {
            std::cout << " CPU: " << std::setw(8) << container.cpu_usage << "ns";
            std::cout << " RAM: " << std::setw(6) << (container.memory_usage / 1024) << "KB |\n";
        } else {
            std::cout << std::string(30, ' ') << "|\n";
        }

        std::cout << "+" << std::string(72, '-') << "+\n";
    }

    void drawContainers() {
        if (containers.empty()) {
            std::cout << "+" << std::string(72, '-') << "+\n";
            std::cout << "| هیچ کانتینری وجود ندارد                                      |\n";
            std::cout << "+" << std::string(72, '-') << "+\n\n";
            return;
        }

        for (size_t i = 0; i < containers.size(); ++i) {
            drawContainer(containers[i], i);
            std::cout << "\n";
        }
    }

    void drawMenu() {
        std::cout << "+======================================= منو ========================================+\n";
        std::cout << "| 1. ایجاد کانتینر جدید                | 2. شروع کانتینر                         |\n";
        std::cout << "| 3. توقف کانتینر                      | 4. نابودی کانتینر                       |\n";
        std::cout << "| 5. نمایش اطلاعات کانتینر             | 6. لیست همه کانتینرها                   |\n";
        std::cout << "| 7. اجرای دستور در کانتینر            | 8. به‌روزرسانی نمایش                    |\n";
        std::cout << "| 0. خروج                               |                                         |\n";
        std::cout << "+==================================================================================+\n";
        std::cout << "\nانتخاب شما: ";
    }

    void createContainerDemo() {
        static int counter = 1;
        std::string container_id = "demo_container_" + std::to_string(counter++);

#ifndef _WIN32
        container_config_t config = {};
        config.id = strdup(container_id.c_str());

        namespace_config_t ns_config = {};
        namespace_config_init(&ns_config);
        config.ns_config = ns_config;

        resource_limits_t res_limits = {};
        resource_limits_init(&res_limits);
        config.res_limits = res_limits;

        fs_config_t fs_config = {};
        fs_config_init(&fs_config);
        config.fs_config = fs_config;

        static const char* cmd[] = {"/bin/sleep", "5", nullptr};
        config.command = const_cast<char**>(cmd);
        config.command_argc = 2;

        if (container_manager_create(&cm, &config) == 0) {
#endif
            ContainerDisplay display = {container_id, CONTAINER_CREATED, 0, time(nullptr), 0, 0, 0};
            containers.push_back(display);
            std::cout << "✅ کانتینر " << container_id << " ایجاد شد!\n";
#ifndef _WIN32
        } else {
            std::cout << "❌ خطا در ایجاد کانتینر!\n";
        }
#endif

        std::cout << "برای ادامه Enter را فشار دهید...";
        std::cin.ignore();
        std::cin.get();
    }

    void startContainerDemo() {
        if (containers.empty()) {
            std::cout << "❌ هیچ کانتینری وجود ندارد!\n";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::cout << "شماره کانتینر برای شروع (1-" << containers.size() << "): ";
        int index;
        std::cin >> index;

        if (index < 1 || index > (int)containers.size()) {
            std::cout << "❌ شماره نامعتبر!\n";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::string container_id = containers[index-1].id;

#ifndef _WIN32
        container_config_t config = {};
        config.id = strdup(container_id.c_str());

        namespace_config_t ns_config = {};
        namespace_config_init(&ns_config);
        config.ns_config = ns_config;

        resource_limits_t res_limits = {};
        resource_limits_init(&res_limits);
        config.res_limits = res_limits;

        fs_config_t fs_config = {};
        fs_config_init(&fs_config);
        config.fs_config = fs_config;

        static const char* cmd[] = {"/bin/sleep", "30", nullptr};
        config.command = const_cast<char**>(cmd);
        config.command_argc = 2;

        if (container_manager_run(&cm, &config) == 0) {
#endif
            containers[index-1].state = CONTAINER_RUNNING;
            containers[index-1].started_at = time(nullptr);
            containers[index-1].pid = rand() % 10000 + 1000;
            std::cout << "✅ کانتینر " << container_id << " شروع شد!\n";
#ifndef _WIN32
        } else {
            std::cout << "❌ خطا در شروع کانتینر!\n";
        }
#endif

        std::cin.ignore();
        std::cin.get();
    }

    void stopContainerDemo() {
        if (containers.empty()) {
            std::cout << "❌ هیچ کانتینری وجود ندارد!\n";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::cout << "شماره کانتینر برای توقف (1-" << containers.size() << "): ";
        int index;
        std::cin >> index;

        if (index < 1 || index > (int)containers.size()) {
            std::cout << "❌ شماره نامعتبر!\n";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::string container_id = containers[index-1].id;
#ifndef _WIN32
        if (container_manager_stop(&cm, container_id.c_str()) == 0) {
#endif
            containers[index-1].state = CONTAINER_STOPPED;
            std::cout << "✅ کانتینر " << container_id << " متوقف شد!\n";
#ifndef _WIN32
        } else {
            std::cout << "❌ خطا در توقف کانتینر!\n";
        }
#endif

        std::cin.ignore();
        std::cin.get();
    }

    void destroyContainerDemo() {
        if (containers.empty()) {
            std::cout << "❌ هیچ کانتینری وجود ندارد!\n";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::cout << "شماره کانتینر برای نابودی (1-" << containers.size() << "): ";
        int index;
        std::cin >> index;

        if (index < 1 || index > (int)containers.size()) {
            std::cout << "❌ شماره نامعتبر!\n";
            std::cin.ignore();
            std::cin.get();
            return;
        }

        std::string container_id = containers[index-1].id;
#ifndef _WIN32
        if (container_manager_destroy(&cm, container_id.c_str()) == 0) {
#endif
            containers[index-1].state = CONTAINER_DESTROYED;
            std::cout << "✅ کانتینر " << container_id << " نابود شد!\n";
#ifndef _WIN32
        } else {
            std::cout << "❌ خطا در نابودی کانتینر!\n";
        }
#endif

        std::cin.ignore();
        std::cin.get();
    }

    void updateDisplay() {
        for (auto& container : containers) {
            if (container.state == CONTAINER_RUNNING) {
                container.cpu_usage = rand() % 1000000 + 500000;
                container.memory_usage = rand() % 10000000 + 5000000;
            }
        }
    }

public:
    TerminalUI() : running(true) {
        srand(time(nullptr));
#ifndef _WIN32
        if (container_manager_init(&cm, 10) != 0) {
            std::cerr << "خطا در مقداردهی اولیه مدیریت‌کننده کانتینر!\n";
            exit(1);
        }
#endif
    }

    ~TerminalUI() {
#ifndef _WIN32
        container_manager_cleanup(&cm);
#endif
    }

    void run() {
        while (running) {
            clearScreen();
            drawHeader();
            drawContainers();
            drawMenu();

            int choice;
            std::cin >> choice;

            switch (choice) {
                case 1:
                    createContainerDemo();
                    break;
                case 2:
                    startContainerDemo();
                    break;
                case 3:
                    stopContainerDemo();
                    break;
                case 4:
                    destroyContainerDemo();
                    break;
                case 5:
                    // Show container info - for now just refresh display
                    break;
                case 6:
                    // List containers - already shown
                    break;
                case 7:
                    // Execute command - would need more implementation
                    std::cout << "این ویژگی هنوز پیاده‌سازی نشده!\n";
                    std::cin.ignore();
                    std::cin.get();
                    break;
                case 8:
                    updateDisplay();
                    break;
                case 0:
                    running = false;
                    break;
                default:
                    std::cout << "انتخاب نامعتبر!\n";
                    std::cin.ignore();
                    std::cin.get();
                    break;
            }
        }
    }
};

int main() {
    setlocale(LC_ALL, "fa_IR.UTF-8");

    TerminalUI ui;
    ui.run();

    std::cout << "\nخداحافظ! 👋\n";
    return 0;
}
