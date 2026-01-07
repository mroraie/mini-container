# پشتیبانی از Cgroup v2

این سند توضیح می‌دهد که چگونه Mini Container UI از cgroup v2 پشتیبانی می‌کند و چگونه به صورت خودکار بین cgroup v1 و cgroup v2 تشخیص می‌دهد.

## نمای کلی

Mini Container UI اکنون از هر دو نسخه cgroup پشتیبانی می‌کند:
- **Cgroup v2** (ترجیح داده می‌شود): نسخه جدیدتر با سلسله مراتب یکپارچه
- **Cgroup v1** (fallback): نسخه قدیمی‌تر با زیرسیستم‌های جداگانه

سیستم به صورت خودکار نسخه موجود را تشخیص می‌دهد و از API مناسب استفاده می‌کند.

## تشخیص خودکار نسخه

سیستم به ترتیب زیر نسخه cgroup را تشخیص می‌دهد:

1. **بررسی cgroup v2**: بررسی وجود `/sys/fs/cgroup/cgroup.controllers`
2. **بررسی cgroup v1**: بررسی وجود `/sys/fs/cgroup/cpu` و `/sys/fs/cgroup/memory`
3. **Fallback**: در صورت عدم وجود هر دو، به cgroup v1 به عنوان پیش‌فرض برمی‌گردد

## تفاوت‌های API

### CPU Limits

#### Cgroup v2
```bash
# محدودیت CPU با فرمت "quota period"
echo "50000 100000" > /sys/fs/cgroup/mini_container_<id>/cpu.max

# CPU نامحدود
echo "max" > /sys/fs/cgroup/mini_container_<id>/cpu.max

# وزن CPU (مشابه shares)
echo "100" > /sys/fs/cgroup/mini_container_<id>/cpu.weight
```

#### Cgroup v1
```bash
# سهمیه CPU
echo "50000" > /sys/fs/cgroup/cpu/mini_container_<id>/cpu.cfs_quota_us
echo "100000" > /sys/fs/cgroup/cpu/mini_container_<id>/cpu.cfs_period_us

# CPU shares
echo "1024" > /sys/fs/cgroup/cpu/mini_container_<id>/cpu.shares
```

### Memory Limits

#### Cgroup v2
```bash
# محدودیت حافظه
echo "134217728" > /sys/fs/cgroup/mini_container_<id>/memory.max

# حافظه نامحدود
echo "max" > /sys/fs/cgroup/mini_container_<id>/memory.max

# محدودیت swap
echo "268435456" > /sys/fs/cgroup/mini_container_<id>/memory.swap.max
```

#### Cgroup v1
```bash
# محدودیت حافظه
echo "134217728" > /sys/fs/cgroup/memory/mini_container_<id>/memory.limit_in_bytes

# محدودیت swap
echo "268435456" > /sys/fs/cgroup/memory/mini_container_<id>/memory.memsw.limit_in_bytes
```

### Process Management

#### Cgroup v2
```bash
# افزودن فرایند (از cgroup.procs استفاده می‌کند)
echo "1234" > /sys/fs/cgroup/mini_container_<id>/cgroup.procs
```

#### Cgroup v1
```bash
# افزودن فرایند (از tasks استفاده می‌کند)
echo "1234" > /sys/fs/cgroup/cpu/mini_container_<id>/tasks
echo "1234" > /sys/fs/cgroup/memory/mini_container_<id>/tasks
```

### Statistics

#### Cgroup v2
```bash
# استفاده CPU (از cpu.stat)
cat /sys/fs/cgroup/mini_container_<id>/cpu.stat
# خروجی: usage_usec 123456789

# استفاده حافظه
cat /sys/fs/cgroup/mini_container_<id>/memory.current
```

#### Cgroup v1
```bash
# استفاده CPU
cat /sys/fs/cgroup/cpu/mini_container_<id>/cpuacct.usage

# استفاده حافظه
cat /sys/fs/cgroup/memory/mini_container_<id>/memory.usage_in_bytes
```

## بررسی نسخه Cgroup در سیستم شما

### بررسی Cgroup v2

```bash
# بررسی وجود cgroup v2
ls -la /sys/fs/cgroup/cgroup.controllers

# بررسی کنترلرهای فعال
cat /sys/fs/cgroup/cgroup.controllers

# بررسی mount point
mount | grep cgroup2
```

### بررسی Cgroup v1

```bash
# بررسی زیرسیستم‌های cgroup v1
ls -la /sys/fs/cgroup/cpu
ls -la /sys/fs/cgroup/memory

# بررسی mount point
mount | grep cgroup
```

## سیستم‌های پشتیبانی شده

### سیستم‌های با Cgroup v2

- **Ubuntu 22.04+**: به صورت پیش‌فرض از cgroup v2 استفاده می‌کند
- **Fedora 31+**: به صورت پیش‌فرض از cgroup v2 استفاده می‌کند
- **Debian 11+**: می‌تواند با cgroup v2 پیکربندی شود
- **Kernel 5.2+**: پشتیبانی کامل از cgroup v2

### سیستم‌های با Cgroup v1

- **Ubuntu 20.04 و قدیمی‌تر**: به صورت پیش‌فرض از cgroup v1 استفاده می‌کند
- **CentOS 7/8**: از cgroup v1 استفاده می‌کند
- **Debian 10 و قدیمی‌تر**: از cgroup v1 استفاده می‌کند

## عیب‌یابی

### خطا: "CPU cgroup subsystem not available"

این خطا معمولاً زمانی رخ می‌دهد که:
1. سیستم از cgroup v2 استفاده می‌کند اما کد قدیمی فقط cgroup v1 را بررسی می‌کند
2. **راه حل**: از نسخه جدید Mini Container UI استفاده کنید که از cgroup v2 پشتیبانی می‌کند

### خطا: "memory cgroup subsystem not available"

مشابه خطای CPU، این خطا زمانی رخ می‌دهد که:
1. سیستم از cgroup v2 استفاده می‌کند
2. **راه حل**: از نسخه جدید استفاده کنید

### بررسی دسترسی

```bash
# بررسی دسترسی به cgroup
ls -la /sys/fs/cgroup/

# اگر دسترسی ندارید، با sudo اجرا کنید
sudo ./mini-container-ui
```

## مثال‌های استفاده

### اجرای با Cgroup v2

```bash
# ساخت و اجرا
make ui
sudo ./mini-container-ui

# سیستم به صورت خودکار cgroup v2 را تشخیص می‌دهد
```

### اجرای با Cgroup v1

```bash
# ساخت و اجرا
make ui
sudo ./mini-container-ui

# سیستم به صورت خودکار cgroup v1 را تشخیص می‌دهد
```

## تغییرات پیاده‌سازی

### فایل‌های تغییر یافته

- `include/resource_manager.hpp`: اضافه شدن enum `cgroup_version_t` و فیلد `version`
- `src/resource_manager.cpp`: 
  - تابع `detect_cgroup_version()` برای تشخیص خودکار
  - به‌روزرسانی تمام توابع برای پشتیبانی از هر دو نسخه

### توابع به‌روزرسانی شده

1. `resource_manager_init()`: تشخیص و اعتبارسنجی نسخه cgroup
2. `set_cpu_limits()`: پشتیبانی از `cpu.max` (v2) و `cpu.cfs_quota_us` (v1)
3. `set_memory_limits()`: پشتیبانی از `memory.max` (v2) و `memory.limit_in_bytes` (v1)
4. `resource_manager_create_cgroup()`: ایجاد دایرکتوری مناسب برای هر نسخه
5. `resource_manager_add_process()`: استفاده از `cgroup.procs` (v2) یا `tasks` (v1)
6. `resource_manager_get_stats()`: خواندن آمار از فایل‌های مناسب هر نسخه

## منابع بیشتر

- [Linux Kernel Documentation: Control Groups v2](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html)
- [Red Hat: Using cgroups v2](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/managing_monitoring_and_updating_the_kernel/using-cgroups-v2_managing-monitoring-and-updating-the-kernel)
- [Ubuntu: Cgroup v2](https://ubuntu.com/blog/cgroup-v2)

---

**نکته**: این پشتیبانی به صورت شفاف کار می‌کند و نیازی به تغییر در کد کاربر نیست. سیستم به صورت خودکار نسخه مناسب را انتخاب می‌کند.


