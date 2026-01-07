# Mini Container

پیاده‌سازی سبک‌وزن کانتینرها با استفاده از Linux namespaces، cgroups و filesystem isolation.

## ویژگی‌ها

- مدیریت چرخه حیات کانتینر (ایجاد، اجرا، توقف، حذف)
- محدودیت منابع: CPU (50% یک هسته) و Memory (1GB)
- ایزولاسیون با Linux Namespaces (PID, Mount, UTS, Network)
- رابط CLI و منوی تعاملی
- وب سرور با Dashboard
- مانیتورینگ زنده و نمایش آمار منابع

## نیازمندی‌ها

- Linux
- C++11 compiler (g++)
- دسترسی root
- pthread library
- cgroups (v1 یا v2)

## نصب

```bash
make
sudo make install
```

## استفاده

### حالت تعاملی
```bash
sudo ./mini-container
```

### CLI
```bash
# اجرای کانتینر
sudo ./mini-container run /bin/sh

# لیست کانتینرها
sudo ./mini-container list

# توقف کانتینر
sudo ./mini-container stop <container_id>

# اطلاعات کانتینر
sudo ./mini-container info <container_id>

# حذف کانتینر
sudo ./mini-container destroy <container_id>
```

### وب سرور
```bash
# دسترسی به http://localhost:808
sudo ./mini-container-web
```

## ساختار پروژه

```
mini-container/
├── include/          # فایل‌های هدر
├── src/             # کد منبع
├── docs/            # مستندات
└── Makefile
```

## تنظیمات پیش‌فرض

- Memory: 1GB
- CPU: 50% یک هسته
- Web Port: 808
- Max Containers: 10

## تست‌ها

برنامه شامل تست‌های داخلی برای بررسی عملکرد سیستم است. برای اجرای تست‌ها:

```bash
sudo ./mini-container
# گزینه 7 را انتخاب کنید (Run Tests)
```

### تست 1: CPU Usage Test

**هدف**: بررسی مصرف CPU کانتینر

**مشخصات**:
- Memory Limit: 128 MB
- CPU Shares: 1024
- دستور: `while true; do :; done` (حلقه بی‌نهایت)
- مدت زمان: 3 ثانیه
- خروجی: نمایش CPU Usage (nanoseconds) و Memory Usage

**نحوه کار**: یک کانتینر با حلقه بی‌نهایت ایجاد می‌کند و مصرف CPU را اندازه‌گیری می‌کند.

### تست 2: Memory Limit Test

**هدف**: بررسی محدودیت حافظه

**مشخصات**:
- Memory Limit: 64 MB
- CPU Shares: 1024
- دستور: `dd if=/dev/zero of=/tmp/mem bs=1M count=80` (تلاش برای نوشتن 80MB)
- مدت زمان: 2 ثانیه
- خروجی: نمایش Memory Usage و بررسی محدودیت

**نحوه کار**: تلاش می‌کند 80MB حافظه بنویسد در حالی که محدودیت 64MB است تا محدودیت حافظه را تست کند.

### تست 3: CPU Limit Test

**هدف**: بررسی محدودیت CPU

**مشخصات**:
- Memory Limit: 128 MB
- CPU Shares: 512 (نصف پیش‌فرض)
- دستور: `while true; do :; done` (حلقه بی‌نهایت)
- مدت زمان: 3 ثانیه
- خروجی: نمایش CPU Usage با محدودیت اعمال شده

**نحوه کار**: یک کانتینر با CPU shares محدود ایجاد می‌کند و مصرف CPU را بررسی می‌کند.

### تست 4: Combined Test (CPU + Memory)

**هدف**: بررسی همزمان CPU و Memory

**مشخصات**:
- Memory Limit: 128 MB
- CPU Shares: 1024
- دستور: ترکیبی از نوشتن فایل و محاسبات (16MB فایل + حلقه محاسباتی)
- مدت زمان: 3 ثانیه
- خروجی: نمایش CPU Usage و Memory Usage

**نحوه کار**: همزمان CPU و Memory را تحت فشار قرار می‌دهد تا عملکرد ترکیبی را تست کند.

### تست 5: Run All Tests

اجرای همه تست‌های بالا به ترتیب.

### کانتینرهای تست خودکار

هنگام راه‌اندازی برنامه، 10 کانتینر تست به صورت خودکار ایجاد می‌شوند:

1. **cpu_intensive**: 4 فرآیند `yes` همزمان - تست CPU
2. **ram_intensive**: تخصیص 150MB حافظه - تست Memory
3. **cpu_ram_heavy**: CPU + RAM سنگین - تست ترکیبی
4. **cpu_calc**: محاسبات Python سنگین - تست CPU
5. **mem_stress**: تخصیص 150MB حافظه - تست Memory
6. **mixed_workload**: CPU + I/O همزمان - تست ترکیبی
7. **high_cpu**: 3 فرآیند `yes` - تست CPU
8. **high_mem**: تخصیص 200MB حافظه - تست Memory
9. **balanced**: کاربرد متعادل CPU و RAM
10. **max_stress**: 5 فرآیند `yes` + محاسبات سنگین - تست حداکثری

**مشخصات مشترک همه کانتینرهای تست**:
- Memory Limit: 1GB
- CPU Limit: 50% یک هسته (quota: 50000us, period: 100000us)
- Runtime: 600 ثانیه (10 دقیقه)

## محدودیت‌ها

- نیاز به root
- فقط Linux
- پیاده‌سازی پایه (غیر تولیدی)

## مجوز

پروژه آموزشی - استفاده آزاد
