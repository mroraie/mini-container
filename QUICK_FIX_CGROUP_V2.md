# راهنمای سریع رفع مشکل Cgroup v2

## مشکل
```
Error: CPU cgroup subsystem not available
Failed to initialize resource manager
Failed to initialize container manager
```

## راه حل گام به گام

### مرحله 1: Pull آخرین تغییرات

```bash
cd ~/Projects/mini-container
git pull origin master
```

### مرحله 2: بررسی تغییرات

```bash
# بررسی اینکه کد cgroup v2 وجود دارد
grep -n "detect_cgroup_version" src/resource_manager.cpp

# باید خطی شبیه به این ببینید:
# 23:static cgroup_version_t detect_cgroup_version() {
```

### مرحله 3: Build مجدد

```bash
# پاک کردن فایل‌های قبلی
make clean

# ساخت مجدد
make ui
```

### مرحله 4: بررسی باینری

```bash
# بررسی اینکه فایل ساخته شده است
ls -lh mini-container-ui

# باید فایل اجرایی را ببینید
```

### مرحله 5: اجرا با دستور صحیح

**⚠️ مهم**: دستور صحیح این است:
```bash
./mini-container-ui
```

**❌ اشتباه**: این دستور کار نمی‌کند:
```bash
./mini-container ui  # ❌ اشتباه!
```

### مرحله 6: اجرا با sudo (اگر نیاز بود)

```bash
sudo ./mini-container-ui
```

## بررسی وضعیت Cgroup در سیستم شما

```bash
# بررسی cgroup v2
ls /sys/fs/cgroup/cgroup.controllers

# اگر این فایل وجود داشت، سیستم شما از cgroup v2 استفاده می‌کند
# و کد جدید باید به صورت خودکار آن را تشخیص دهد

# بررسی کنترلرهای فعال
cat /sys/fs/cgroup/cgroup.controllers

# باید cpu و memory را ببینید
```

## اگر هنوز مشکل دارید

### 1. بررسی لاگ‌های دقیق‌تر

کد را با debug اطلاعات بیشتر اجرا کنید:

```bash
# بررسی اینکه resource_manager_init فراخوانی می‌شود
strace -e trace=open,access ./mini-container-ui 2>&1 | grep cgroup
```

### 2. بررسی دسترسی

```bash
# بررسی دسترسی به cgroup
ls -la /sys/fs/cgroup/

# اگر دسترسی ندارید:
sudo ls -la /sys/fs/cgroup/
```

### 3. بررسی mount points

```bash
mount | grep cgroup

# باید چیزی شبیه به این ببینید:
# cgroup2 on /sys/fs/cgroup type cgroup2 (rw,nosuid,nodev,noexec,relatime)
```

### 4. اجرای اسکریپت عیب‌یابی

```bash
chmod +x fix_cgroup_v2.sh
./fix_cgroup_v2.sh
```

## خلاصه دستورات

```bash
# 1. Pull تغییرات
git pull origin master

# 2. Build مجدد
make clean && make ui

# 3. اجرا (با دستور صحیح!)
sudo ./mini-container-ui
```

## نکات مهم

1. **دستور صحیح**: `./mini-container-ui` (نه `./mini-container ui`)
2. **Build مجدد ضروری است**: بعد از pull کردن تغییرات، حتماً build مجدد کنید
3. **sudo ممکن است لازم باشد**: برای دسترسی به cgroups
4. **بررسی cgroup v2**: سیستم شما باید cgroup v2 داشته باشد

## اگر هیچ‌کدام کار نکرد

لطفاً خروجی این دستورات را ارسال کنید:

```bash
# 1. بررسی git status
git status

# 2. بررسی آخرین commit
git log --oneline -5

# 3. بررسی cgroup
ls -la /sys/fs/cgroup/
cat /sys/fs/cgroup/cgroup.controllers 2>/dev/null || echo "cgroup v2 not found"

# 4. بررسی mount
mount | grep cgroup

# 5. بررسی باینری
file mini-container-ui
ldd mini-container-ui | head -5
```

