<div style="display: flex; align-items: flex-start;">
  <img src="docs/mini-continer.png" alt="لوگو مینی کانتینر" width="150" style="margin-right: 20px;">
  <div>
    <h2>نمای کلی</h2>
    <p>
      در این پروژه سعی شده است که پیاده‌سازی سبک یک کانتینر شبیه سازی شود که مفاهیم اصلی سیستم‌عامل را نشان می‌دهد
      شامل ایزولاسیون فرایند، مدیریت منابع و امنیت فایل‌سیستم. این سیستم به طور مستقیم با مکانیزم‌های هسته لینوکس
      تعامل می‌کند و از فراخوانی‌های سیستمی و رابط‌های هسته استفاده می‌کند.
    </p>
  </div>
</div>

<div style="display: flex; align-items: flex-start; margin-bottom: 20px;">
  <img src="docs/imgs/dr0k22dr0k22dr0k.png" alt="ویژگی‌های کلیدی" width="200" style="margin-right: 20px;">
  <div>
    <h2>ویژگی‌های کلیدی</h2>
    <ul>
      <li><strong>ایزولاسیون فرایند</strong>: از فضای نام لینوکس استفاده می‌کند (PID, mount, UTS, network, user)</li>
      <li><strong>مدیریت منابع</strong>: محدودیت‌های CPU و حافظه از طریق گروه‌های کنترل (cgroups)</li>
      <li><strong>ایزولاسیون فایل‌سیستم</strong>: chroot و برای دسترسی امن فایل‌سیستم</li>
      <li><strong>چرخه حیات کانتینر</strong>: عملیات ایجاد، شروع، توقف و نابودی کانتینر</li>
      <li><strong>رابط خط فرمان</strong>: ابزارهای خط فرمان برای مدیریت کانتینر</li>
    </ul>
  </div>
</div>

<h2>پیش‌نیازها</h2>
<ul>
  <li><strong>هسته لینوکس</strong>: ۳.۸+ (برای پشتیبانی فضای نام)</li>
  <li><strong>کامپایلر سی‌پلاس‌پلاس</strong>: GCC (g++) یا سازگار با C++11</li>
  <li><strong>دسترسی روت</strong>: برای عملیات کانتینر ضروری است</li>
  <li><strong>سی گروپ ها Cgroups</strong>: در <code>/sys/fs/cgroup</code> mount شده</li>
  <li><strong>امتیازات sudo</strong>: برای اجرای کانتینرهای privileged</li>
</ul>

### ساخت فایل های مربوط

```bash
git clone https://github.com/mroraie/mini-container.git
cd mini-container
make
# for installing the UI
make ui
```

```bash
# use this shell scripts
./deploy.sh
./deploy-dev.sh
```

## استفاده

### رابط گرافیکی ترمینال

```bash
./mini-container-ui
```

این رابط امکان مدیریت کانتینرها را با منوی گرافیکی فراهم می‌کند.
### دستورات پایه (خط فرمان)

```bash
# اجرای دستور در کانتینر
./mini-container run /bin/echo "Hello World"

# اجرای با محدودیت منابع
./mini-container run --memory 128 --cpu 512 /bin/sh

# لیست کانتینرها
./mini-container list

# دریافت اطلاعات کانتینر
./mini-container info container_id

# توقف کانتینر
./mini-container stop container_id

# نابودی کانتینر
./mini-container destroy container_id
```

## مفاهیم سیستم‌عامل نشان داده شده

### ۱. فضای نام لینوکس
- **فضای نام PID**: ایزولاسیون شناسه فرایند
- **فضای نام Mount**: ایزولاسیون فایل‌سیستم
- **فضای نام UTS**: ایزولاسیون نام میزبان
- **فضای نام Network**: ایزولاسیون پشته شبکه
- **فضای نام User**: ایزولاسیون شناسه کاربر/گروه

![فضاها](docs/imgs/namespace.png)
### ۲. گروه‌های کنترل (cgroups)
- **کنترل CPU**: مدیریت زمان‌بندی و سهمیه
- **کنترل حافظه**: محدودیت‌های RAM و swap


### ۳. امنیت فایل‌سیستم
- **chroot**: تغییر دایرکتوری ریشه
- **فضای نام Mount**: جداول mount خصوصی

### ۴. فراخوانی‌های سیستمی
- **clone()**: ایجاد فرایند با فضای نام
- **unshare()**: دستکاری فضای نام
- **mount()**: عملیات فایل‌سیستم
- **chroot()**: تغییرات دایرکتوری ریشه

## ساختار پروژه

```
mini-container/
├── src/                    # کد منبع
│   ├── main.cpp           # رابط CLI
│   ├── container_manager.cpp # چرخه حیات کانتینر
│   ├── namespace_handler.cpp # عملیات فضای نام
│   ├── resource_manager.cpp  # مدیریت Cgroup
│   ├── filesystem_manager.cpp # ایزولاسیون فایل‌سیستم
│   └── terminal_ui.cpp   # رابط گرافیکی ترمینال
├── include/               # فایل‌های سرآیند
├── tests/                 # اسکریپت‌های تست
├── examples/              # مثال‌های استفاده
├── docs/                  # مستندات
├── Dockerfile            # تنظیمات Docker
├── docker-compose.yml    # تنظیمات Docker Compose
├── deploy.sh            # اسکریپت استقرار
├── deploy-dev.sh        # اسکریپت استقرار توسعه
├── .dockerignore        # فایل‌های نادیده گرفته شده Docker
├── Makefile             # سیستم ساخت
└── README.md            # این فایل
```

## تست

تست‌های عملکرد پایه را اجرا کنید:

```bash
# تست‌های پایه
./tests/test_basic.sh

# تست‌های ایزولاسیون
./tests/test_isolation.sh
```

## مستندات

- **[گزارش فنی](docs/technical_report.md)**: توضیح مفصل مفاهیم سیستم‌عامل و پیاده‌سازی
- **[مثال‌ها](examples/README.md)**: سناریوهای استفاده و مثال‌ها
- **[مستندات API](docs/api.md)**: مرجع API داخلی

## نکات امنیتی

- **دسترسی روت**: اکثر عملیات نیاز به امتیازات روت دارند
- **هسته مشترک**: همه کانتینرها هسته را به اشتراک می‌گذارند
- **ایزولاسیون فضای نام**: ایزولاسیون سطح فرایند فراهم می‌کند
- **محدودیت منابع**: از طریق cgroups اعمال می‌شود

## تنظیمات Docker

### پیکربندی امنیتی

کانتینر Docker با امتیازات زیر اجرا می‌شود:
- `--privileged`: دسترسی کامل به دستگاه میزبان
- `--cap-add SYS_ADMIN`: قابلیت مدیریت سیستم
- `--cap-add NET_ADMIN`: قابلیت مدیریت شبکه
- `--security-opt apparmor:unconfined`: غیرفعال کردن AppArmor
- `--security-opt seccomp:unconfined`: غیرفعال کردن Seccomp

### Mount های ضروری

```yaml
volumes:
  - /sys/fs/cgroup:/sys/fs/cgroup:rw  # برای cgroups
  - /tmp:/tmp:rw                      # برای فایل‌های موقت
```

## محدودیت‌ها

- **فرایند تکی**: هر کانتینر یک فرایند اصلی اجرا می‌کند
- **بدون مدیریت ایمیج**: تنظیم فایل‌سیستم دستی ضروری است
- **شبکه پایه**: پشتیبانی فضای نام شبکه حداقل است
- **بدون ماندگاری**: کانتینرها ریبوت را تحمل نمی‌کنند

## مقایسه با داکر

| ویژگی            | مینی کانتینر      | داکر                           |
| ---------------- | ----------------- | ------------------------------ |
| **ایزولاسیون**   | فضای نام هسته     | فضای نام هسته + لایه‌های اضافی |
| **مدیریت منابع** | cgroups           | cgroups + کنترل‌های اضافی      |
| **فایل‌سیستم**   | chroot/pivot_root | فایل‌سیستم لایه‌ای + overlay   |
| **شبکه**         | فضای نام پایه     | شبکه پیشرفته                   |
| **ایمیج‌ها**     | تنظیم دستی        | لایه‌های ایمیج + registry      |
| **پیچیدگی**      | حداقل             | کامل‌ویژگی                     |





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
