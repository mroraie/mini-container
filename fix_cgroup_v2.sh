#!/bin/bash

# اسکریپت عیب‌یابی و رفع مشکل cgroup v2

echo "=== بررسی وضعیت Cgroup ==="
echo ""

# بررسی وجود cgroup root
echo "1. بررسی /sys/fs/cgroup:"
if [ -d "/sys/fs/cgroup" ]; then
    echo "   ✓ /sys/fs/cgroup وجود دارد"
else
    echo "   ✗ /sys/fs/cgroup وجود ندارد!"
    exit 1
fi

# بررسی cgroup v2
echo ""
echo "2. بررسی Cgroup v2:"
if [ -f "/sys/fs/cgroup/cgroup.controllers" ]; then
    echo "   ✓ Cgroup v2 تشخیص داده شد"
    echo "   کنترلرهای فعال:"
    cat /sys/fs/cgroup/cgroup.controllers | sed 's/^/      /'
    
    # بررسی کنترلرهای CPU و Memory
    if grep -q "cpu" /sys/fs/cgroup/cgroup.controllers && grep -q "memory" /sys/fs/cgroup/cgroup.controllers; then
        echo "   ✓ کنترلرهای CPU و Memory فعال هستند"
    else
        echo "   ✗ کنترلرهای CPU یا Memory فعال نیستند!"
        echo "   برای فعال کردن:"
        echo "   sudo mount -o remount,rw /sys/fs/cgroup"
    fi
else
    echo "   ✗ Cgroup v2 یافت نشد"
fi

# بررسی cgroup v1
echo ""
echo "3. بررسی Cgroup v1:"
if [ -d "/sys/fs/cgroup/cpu" ] && [ -d "/sys/fs/cgroup/memory" ]; then
    echo "   ✓ Cgroup v1 تشخیص داده شد"
else
    echo "   ✗ Cgroup v1 یافت نشد"
fi

# بررسی mount points
echo ""
echo "4. بررسی Mount Points:"
mount | grep cgroup | sed 's/^/   /'

# بررسی دسترسی
echo ""
echo "5. بررسی دسترسی:"
if [ -w "/sys/fs/cgroup" ]; then
    echo "   ✓ دسترسی نوشتن به /sys/fs/cgroup وجود دارد"
else
    echo "   ✗ دسترسی نوشتن به /sys/fs/cgroup وجود ندارد"
    echo "   باید با sudo اجرا کنید"
fi

# بررسی آخرین تغییرات git
echo ""
echo "6. بررسی آخرین تغییرات Git:"
if [ -d ".git" ]; then
    echo "   Pull کردن آخرین تغییرات..."
    git pull origin master
    
    echo ""
    echo "   بررسی فایل resource_manager.cpp:"
    if grep -q "detect_cgroup_version" src/resource_manager.cpp; then
        echo "   ✓ کد cgroup v2 در فایل وجود دارد"
    else
        echo "   ✗ کد cgroup v2 در فایل یافت نشد!"
        echo "   باید آخرین تغییرات را pull کنید"
    fi
else
    echo "   ✗ دایرکتوری .git یافت نشد"
fi

# پیشنهادات
echo ""
echo "=== پیشنهادات ==="
echo ""
echo "اگر مشکل ادامه داشت، این مراحل را انجام دهید:"
echo ""
echo "1. Pull آخرین تغییرات:"
echo "   git pull origin master"
echo ""
echo "2. Build مجدد:"
echo "   make clean"
echo "   make ui"
echo ""
echo "3. اجرا با sudo:"
echo "   sudo ./mini-container-ui"
echo ""
echo "4. اگر هنوز مشکل دارید، بررسی کنید:"
echo "   ls -la /sys/fs/cgroup/"
echo "   cat /sys/fs/cgroup/cgroup.controllers"


