# Mini Container System Dockerfile
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the application
RUN make clean && make

# Create necessary directories
RUN mkdir -p /tmp/containers

# Set proper permissions
RUN chmod +x mini-container mini-container-ui demo_ui.sh

# Expose any necessary ports (none needed for this container system)
# EXPOSE

# Set the default command
CMD ["./demo_ui.sh"]
