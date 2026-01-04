#!/bin/bash

# Mini Container System - VPS Deployment Script
# This script helps deploy the mini container system to a VPS

set -e

echo "🚀 شروع استقرار سیستم مینی کانتینر روی VPS"
echo "=========================================="

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "❌ Docker نصب نیست. لطفا Docker را نصب کنید."
    echo "برای Ubuntu/Debian:"
    echo "  sudo apt update && sudo apt install -y docker.io docker-compose"
    echo "  sudo systemctl start docker && sudo systemctl enable docker"
    echo "  sudo usermod -aG docker \$USER"
    exit 1
fi

# Check if Docker Compose is available
if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "❌ Docker Compose نصب نیست. لطفا Docker Compose را نصب کنید."
    echo "برای Ubuntu/Debian:"
    echo "  sudo apt install -y docker-compose"
    exit 1
fi

echo "✅ Docker و Docker Compose نصب هستند."

# Check if docker daemon is running
if ! docker info &> /dev/null; then
    echo "❌ Docker daemon در حال اجرا نیست. لطفا Docker daemon را راه‌اندازی کنید:"
    echo "  sudo systemctl start docker"
    exit 1
fi

echo "✅ Docker daemon در حال اجرا است."

# Build and run the container
echo "🔨 ساخت و اجرای کانتینر..."
if command -v docker-compose &> /dev/null; then
    docker-compose up --build -d
    echo "✅ کانتینر با موفقیت ساخته و اجرا شد!"
    echo ""
    echo "📋 برای اتصال به کانتینر:"
    echo "  docker-compose exec mini-container bash"
    echo ""
    echo "📋 برای مشاهده لاگ‌ها:"
    echo "  docker-compose logs -f mini-container"
    echo ""
    echo "📋 برای متوقف کردن:"
    echo "  docker-compose down"
else
    # Use newer docker compose command
    docker compose up --build -d
    echo "✅ کانتینر با موفقیت ساخته و اجرا شد!"
    echo ""
    echo "📋 برای اتصال به کانتینر:"
    echo "  docker compose exec mini-container bash"
    echo ""
    echo "📋 برای مشاهده لاگ‌ها:"
    echo "  docker compose logs -f mini-container"
    echo ""
    echo "📋 برای متوقف کردن:"
    echo "  docker compose down"
fi

echo ""
echo "🎉 استقرار کامل شد!"
echo "اکنون می‌توانید از سیستم مینی کانتینر در محیط Docker استفاده کنید."
