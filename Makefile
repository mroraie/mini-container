CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude
LDFLAGS =

# Source files
SRCS = src/main.cpp src/container_manager.cpp src/namespace_handler.cpp src/resource_manager.cpp src/filesystem_manager.cpp
OBJS = $(SRCS:.cpp=.o)

# UI Source files
UI_SRCS = src/terminal_ui.cpp src/container_manager.cpp src/namespace_handler.cpp src/resource_manager.cpp src/filesystem_manager.cpp
UI_OBJS = $(UI_SRCS:.cpp=.o)

# Target executables
TARGET = mini-container
UI_TARGET = mini-container-ui

# Default target
all: $(TARGET)

# Build main application
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Build UI application
ui: $(UI_TARGET)

ifdef _WIN32
$(UI_TARGET): src/terminal_ui.o
	$(CXX) $(LDFLAGS) -o $@ $<
else
$(UI_TARGET): $(UI_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^
endif

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(UI_OBJS) $(TARGET) $(UI_TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
