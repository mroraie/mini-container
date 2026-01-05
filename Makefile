CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -Iinclude
LDFLAGS =

# Source files
SRCS = src/main.cpp src/container_manager.cpp src/namespace_handler.cpp src/resource_manager.cpp src/filesystem_manager.cpp
OBJS = $(SRCS:.cpp=.o)

# Target executable
TARGET = mini-container

# Default target
all: $(TARGET)

# Build main application
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
