CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude
LDFLAGS =

# Source files
SRCS = src/main.cpp src/container_manager.cpp src/namespace_handler.cpp src/resource_manager.cpp src/filesystem_manager.cpp
WEB_SRCS = src/web_server_main.cpp src/web_server.cpp src/container_manager.cpp src/namespace_handler.cpp src/resource_manager.cpp src/filesystem_manager.cpp
OBJS = $(SRCS:.cpp=.o)
WEB_OBJS = $(WEB_SRCS:.cpp=.o)

# Target executables
TARGET = mini-container
WEB_TARGET = mini-container-ui

# Default target
all: $(TARGET) $(WEB_TARGET)

# Build UI only
ui: $(WEB_TARGET)

# Build main application
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Build web UI application
$(WEB_TARGET): $(WEB_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ -lpthread

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(WEB_OBJS) $(TARGET) $(WEB_TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET) $(WEB_TARGET)
	sudo cp $(TARGET) $(WEB_TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET) /usr/local/bin/$(WEB_TARGET)

.PHONY: all clean install uninstall
