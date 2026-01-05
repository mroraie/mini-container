#!/bin/bash

# Mini Container System - Development Deployment Script
# This script deploys the system in development mode with volume mounting

set -e

echo "شروع استقرار محیط توسعه سیستم مینی کانتینر"
echo "=============================================="

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "خطا: Docker نصب نیست. لطفا Docker را نصب کنید."
    exit 1
fi

echo "انجام شد: Docker نصب است."

# Check docker daemon
if ! docker info &> /dev/null; then
    echo "خطا: Docker daemon در حال اجرا نیست."
    exit 1
fi

echo "انجام شد: Docker daemon در حال اجرا است."

# Build and run in development mode
echo "در حال ساخت ساخت و اجرای محیط توسعه..."
if command -v docker-compose &> /dev/null; then
    docker-compose --profile dev up --build
else
    docker compose --profile dev up --build
fi

echo ""
echo "تبریک! محیط توسعه آماده است!"
echo "فایل‌های پروژه در کانتینر mount شده‌اند و تغییرات شما بلافاصله اعمال می‌شوند."
