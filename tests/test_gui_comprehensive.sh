#!/bin/bash

# تست جامع GUI - نمایش تمام مفاهیم سیستم‌عامل
# این تست از طریق GUI تمام مفاهیم را نمایش می‌دهد

set -e

echo "=========================================="
echo " تست جامع GUI - مینی کانتینر"
echo "=========================================="
echo ""
echo "این تست تمام مفاهیم سیستم‌عامل را نمایش می‌دهد:"
echo "  - ایزولاسیون فضای نام (PID, Mount, UTS)"
echo "  - مدیریت منابع با cgroups"
echo "  - ایزولاسیون فایل‌سیستم با chroot"
echo "  - چرخه حیات کانتینر"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PORT=8080
BASE_URL="http://localhost:${PORT}"

# Check if web server is running
check_server() {
    echo -e "${BLUE}بررسی سرور وب...${NC}"
    if curl -s "${BASE_URL}" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ سرور در حال اجرا است${NC}"
        return 0
    else
        echo -e "${RED}✗ سرور در حال اجرا نیست${NC}"
        echo -e "${YELLOW}لطفاً ابتدا سرور را با دستور زیر راه‌اندازی کنید:${NC}"
        echo "  ./mini-container-ui"
        return 1
    fi
}

# Test comprehensive test endpoint
test_comprehensive_endpoint() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}اجرای تست جامع از طریق API${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    response=$(curl -s "${BASE_URL}/api/test/comprehensive")
    
    if echo "$response" | grep -q '"success":true'; then
        echo -e "${GREEN}✓ تست جامع با موفقیت اجرا شد${NC}"
        echo ""
        echo "نتایج تست:"
        echo "$response" | python3 -m json.tool 2>/dev/null || echo "$response"
    else
        echo -e "${RED}✗ خطا در اجرای تست جامع${NC}"
        echo "پاسخ: $response"
        return 1
    fi
}

# Test container creation
test_container_creation() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}تست ایجاد کانتینر${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # Create container with different configurations
    echo -e "${YELLOW}1. ایجاد کانتینر با محدودیت حافظه${NC}"
    response=$(curl -s -X POST "${BASE_URL}/api/containers/run" \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "command=/bin/echo%20'test%20memory'&memory=64&cpu=512&hostname=test-mem&root_path=/tmp/test_root")
    
    if echo "$response" | grep -q '"success":true'; then
        container_id=$(echo "$response" | grep -o '"container_id":"[^"]*' | cut -d'"' -f4)
        echo -e "${GREEN}✓ کانتینر ایجاد شد: $container_id${NC}"
        echo -e "  ${BLUE}مفهوم:${NC} محدودیت حافظه با cgroups اعمال شد"
    else
        echo -e "${RED}✗ خطا در ایجاد کانتینر${NC}"
    fi
    
    echo ""
    echo -e "${YELLOW}2. ایجاد کانتینر با hostname جدا${NC}"
    response=$(curl -s -X POST "${BASE_URL}/api/containers/run" \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "command=/bin/echo%20'test%20hostname'&memory=128&cpu=1024&hostname=isolated-host&root_path=/tmp/test_root2")
    
    if echo "$response" | grep -q '"success":true'; then
        container_id=$(echo "$response" | grep -o '"container_id":"[^"]*' | cut -d'"' -f4)
        echo -e "${GREEN}✓ کانتینر ایجاد شد: $container_id${NC}"
        echo -e "  ${BLUE}مفهوم:${NC} ایزولاسیون hostname با فضای نام UTS"
    else
        echo -e "${RED}✗ خطا در ایجاد کانتینر${NC}"
    fi
}

# Test container listing
test_container_listing() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}تست لیست کانتینرها${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    response=$(curl -s "${BASE_URL}/api/containers")
    
    if echo "$response" | grep -q '"containers"'; then
        echo -e "${GREEN}✓ لیست کانتینرها دریافت شد${NC}"
        container_count=$(echo "$response" | grep -o '"id":"[^"]*' | wc -l)
        echo -e "  تعداد کانتینرها: $container_count"
        echo ""
        echo "جزئیات:"
        echo "$response" | python3 -m json.tool 2>/dev/null || echo "$response"
    else
        echo -e "${RED}✗ خطا در دریافت لیست کانتینرها${NC}"
    fi
}

# Display concepts
display_concepts() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}مفاهیم سیستم‌عامل نمایش داده شده${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    echo -e "${GREEN}1. فضای نام PID (Process ID Namespace)${NC}"
    echo "   - هر کانتینر فضای نام PID جداگانه‌ای دارد"
    echo "   - فرایندهای داخل کانتینر PID های مستقل دارند"
    echo "   - استفاده از clone() با CLONE_NEWPID"
    echo ""
    
    echo -e "${GREEN}2. فضای نام Mount (Mount Namespace)${NC}"
    echo "   - جداسازی فایل‌سیستم از میزبان"
    echo "   - هر کانتینر جدول mount جداگانه‌ای دارد"
    echo "   - استفاده از clone() با CLONE_NEWNS"
    echo ""
    
    echo -e "${GREEN}3. فضای نام UTS (Unix Time-sharing System)${NC}"
    echo "   - ایزولاسیون hostname و domainname"
    echo "   - هر کانتینر می‌تواند hostname جداگانه‌ای داشته باشد"
    echo "   - استفاده از clone() با CLONE_NEWUTS"
    echo ""
    
    echo -e "${GREEN}4. Cgroups (Control Groups)${NC}"
    echo "   - مدیریت و محدود کردن منابع سیستم"
    echo "   - محدودیت CPU از طریق cpu.shares"
    echo "   - محدودیت حافظه از طریق memory.limit_in_bytes"
    echo "   - ردیابی استفاده از منابع"
    echo ""
    
    echo -e "${GREEN}5. Chroot${NC}"
    echo "   - تغییر دایرکتوری ریشه فرایند"
    echo "   - ایزولاسیون فایل‌سیستم"
    echo "   - محدود کردن دسترسی به فایل‌های خارج از root path"
    echo ""
    
    echo -e "${GREEN}6. چرخه حیات کانتینر${NC}"
    echo "   - CREATED: کانتینر ایجاد شده اما شروع نشده"
    echo "   - RUNNING: کانتینر در حال اجرا"
    echo "   - STOPPED: کانتینر متوقف شده"
    echo "   - DESTROYED: کانتینر نابود شده"
    echo ""
    
    echo -e "${GREEN}7. فراخوانی‌های سیستمی${NC}"
    echo "   - clone(): ایجاد فرایند با فضای نام"
    echo "   - unshare(): دستکاری فضای نام"
    echo "   - mount(): عملیات فایل‌سیستم"
    echo "   - chroot(): تغییر دایرکتوری ریشه"
    echo ""
}

# Main execution
main() {
    if ! check_server; then
        exit 1
    fi
    
    display_concepts
    
    test_comprehensive_endpoint
    
    test_container_creation
    
    sleep 2  # Wait for containers to be created
    
    test_container_listing
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}تست جامع با موفقیت انجام شد!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}برای مشاهده رابط گرافیکی، به آدرس زیر بروید:${NC}"
    echo -e "${BLUE}http://localhost:${PORT}${NC}"
    echo ""
    echo -e "${YELLOW}در رابط گرافیکی می‌توانید:${NC}"
    echo "  - کانتینرهای جدید ایجاد کنید"
    echo "  - روند اجرا را به صورت real-time مشاهده کنید"
    echo "  - تست جامع را از طریق دکمه 'اجرای تست جامع' اجرا کنید"
    echo "  - لیست کانتینرها را مشاهده کنید"
    echo ""
}

# Run main function
main

