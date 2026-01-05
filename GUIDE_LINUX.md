# راهنمای اجرای GUI در لینوکس

این راهنما نحوه اجرای رابط گرافیکی مینی کانتینر را در سیستم لینوکس توضیح می‌دهد.

## پیش‌نیازها

### 1. نصب کامپایلر C++

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y g++ make

# CentOS/RHEL/Fedora
sudo yum install -y gcc-c++ make
# یا برای Fedora جدید:
sudo dnf install -y gcc-c++ make

# Arch Linux
sudo pacman -S gcc make
```

### 2. بررسی دسترسی root

برای اجرای کامل کانتینرها، نیاز به دسترسی root دارید:

```bash
# بررسی دسترسی root
whoami
# باید 'root' نمایش دهد

# یا استفاده از sudo
sudo whoami
```

### 3. بررسی پشتیبانی از Namespaces

```bash
# بررسی نسخه هسته (باید 3.8+ باشد)
uname -r

# بررسی پشتیبانی از namespaces
ls /proc/self/ns/
# باید فایل‌های pid, mnt, uts و ... را ببینید
```

### 4. بررسی Cgroups

```bash
# بررسی mount شدن cgroups
mount | grep cgroup
# برای cgroup v2 باید چیزی شبیه به این ببینید:
# cgroup2 on /sys/fs/cgroup type cgroup2 (rw,nosuid,nodev,noexec,relatime)
# برای cgroup v1 باید چیزی شبیه به این ببینید:
# cgroup on /sys/fs/cgroup/cpu type cgroup (rw,nosuid,nodev,noexec,relatime,cpu)
# cgroup on /sys/fs/cgroup/memory type cgroup (rw,nosuid,nodev,noexec,relatime,memory)

# بررسی نسخه cgroup
ls /sys/fs/cgroup/cgroup.controllers
# اگر این فایل وجود داشت، سیستم از cgroup v2 استفاده می‌کند
# اگر وجود نداشت، سیستم از cgroup v1 استفاده می‌کند

# نکته: Mini Container UI به صورت خودکار نسخه cgroup را تشخیص می‌دهد
# نیازی به mount دستی نیست مگر اینکه سیستم شما cgroups را mount نکرده باشد
```

## نصب و ساخت

### 1. کلون کردن یا دانلود پروژه

```bash
# اگر از git استفاده می‌کنید:
git clone https://github.com/mroraie/mini-container.git
cd mini-container

# یا اگر فایل‌ها را دارید، به دایرکتوری پروژه بروید:
cd mini-container
```

### 2. ساخت پروژه

```bash
# ساخت فقط GUI
make ui

# یا ساخت همه چیز (CLI + GUI)
make

# بررسی فایل‌های ساخته شده
ls -lh mini-container-ui
```

### 3. نصب (اختیاری)

```bash
# نصب در سیستم
sudo make install

# این فایل‌ها را در /usr/local/bin نصب می‌کند
# بعد از نصب می‌توانید از هر جا اجرا کنید:
mini-container-ui
```

## اجرای GUI

### روش 1: اجرای مستقیم

```bash
# اجرا با پورت پیش‌فرض (8080)
./mini-container-ui

# یا با پورت دلخواه
./mini-container-ui 9000
```

### روش 2: اجرا با sudo (برای دسترسی کامل)

```bash
# اجرا با دسترسی root
sudo ./mini-container-ui

# یا با پورت دلخواه
sudo ./mini-container-ui 9000
```

### روش 3: اجرا در پس‌زمینه

```bash
# اجرا در پس‌زمینه
nohup sudo ./mini-container-ui > /tmp/mini-container-ui.log 2>&1 &

# مشاهده PID
echo $!

# مشاهده لاگ‌ها
tail -f /tmp/mini-container-ui.log

# متوقف کردن
sudo pkill mini-container-ui
```

## دسترسی به GUI

### از همان سیستم

```bash
# باز کردن در مرورگر
xdg-open http://localhost:8080

# یا در Firefox
firefox http://localhost:8080

# یا در Chrome/Chromium
google-chrome http://localhost:8080
```

### از سیستم دیگر (Remote Access)

اگر روی سرور لینوکس اجرا می‌کنید و می‌خواهید از سیستم دیگری به آن دسترسی داشته باشید:

#### 1. بررسی IP سیستم

```bash
# نمایش IP آدرس
ip addr show
# یا
hostname -I
```

#### 2. باز کردن پورت در فایروال

```bash
# Ubuntu/Debian (ufw)
sudo ufw allow 8080/tcp
sudo ufw reload

# CentOS/RHEL (firewalld)
sudo firewall-cmd --permanent --add-port=8080/tcp
sudo firewall-cmd --reload

# یا غیرفعال کردن فایروال موقتاً (فقط برای تست)
sudo systemctl stop firewalld  # CentOS/RHEL
sudo ufw disable  # Ubuntu/Debian
```

#### 3. دسترسی از مرورگر

```
http://YOUR_SERVER_IP:8080
```

**⚠️ هشدار امنیتی:** در محیط production، حتماً از HTTPS و احراز هویت استفاده کنید!

## تست GUI

### تست از طریق مرورگر

1. باز کردن `http://localhost:8080`
2. کلیک روی "اجرای تست جامع"
3. مشاهده نتایج تست

### تست از خط فرمان

```bash
# اجرای تست جامع
./tests/test_gui_comprehensive.sh

# یا اگر نصب شده:
tests/test_gui_comprehensive.sh
```

## عیب‌یابی

### مشکل: خطای "Permission denied"

```bash
# بررسی دسترسی اجرا
ls -l mini-container-ui

# دادن دسترسی اجرا
chmod +x mini-container-ui

# یا اجرا با sudo
sudo ./mini-container-ui
```

### مشکل: پورت در حال استفاده است

```bash
# بررسی استفاده از پورت
sudo netstat -tulpn | grep 8080
# یا
sudo ss -tulpn | grep 8080

# پیدا کردن و کشتن فرایند
sudo lsof -i :8080
sudo kill -9 <PID>

# یا استفاده از پورت دیگر
./mini-container-ui 9000
```

### مشکل: خطای "Failed to bind socket"

```bash
# بررسی دسترسی به پورت
sudo netstat -tulpn | grep 8080

# اگر نیاز به دسترسی root دارید:
sudo ./mini-container-ui
```

### مشکل: کانتینرها اجرا نمی‌شوند

```bash
# بررسی دسترسی root
whoami

# بررسی cgroups
ls -la /sys/fs/cgroup/

# بررسی namespaces
ls /proc/self/ns/

# اجرا با sudo
sudo ./mini-container-ui
```

### مشکل: GUI باز نمی‌شود

```bash
# بررسی اجرا بودن سرور
ps aux | grep mini-container-ui

# بررسی لاگ‌ها
tail -f /tmp/mini-container-ui.log

# بررسی اتصال
curl http://localhost:8080

# بررسی فایروال
sudo iptables -L -n | grep 8080
```

### مشکل: خطای "CPU cgroup subsystem not available" یا "memory cgroup subsystem not available"

این خطا معمولاً زمانی رخ می‌دهد که سیستم از cgroup v2 استفاده می‌کند:

```bash
# بررسی نسخه cgroup
ls /sys/fs/cgroup/cgroup.controllers
# اگر این فایل وجود داشت، سیستم از cgroup v2 استفاده می‌کند

# بررسی mount شدن cgroup
mount | grep cgroup

# برای cgroup v2 باید چیزی شبیه به این ببینید:
# cgroup2 on /sys/fs/cgroup type cgroup2 (rw,nosuid,nodev,noexec,relatime)

# اگر cgroup mount نشده است:
sudo mkdir -p /sys/fs/cgroup
sudo mount -t cgroup2 none /sys/fs/cgroup

# بررسی کنترلرهای فعال
cat /sys/fs/cgroup/cgroup.controllers
# باید cpu و memory را ببینید

# نکته: نسخه جدید Mini Container UI به صورت خودکار cgroup v2 را تشخیص می‌دهد
# مطمئن شوید که از آخرین نسخه استفاده می‌کنید:
git pull origin master
make clean
make ui
```

برای اطلاعات بیشتر به [CGROUP_V2_SUPPORT.md](CGROUP_V2_SUPPORT.md) مراجعه کنید.

## استفاده با systemd (اجرای خودکار)

### ایجاد سرویس systemd

```bash
# ایجاد فایل سرویس
sudo nano /etc/systemd/system/mini-container-ui.service
```

محتوای فایل:

```ini
[Unit]
Description=Mini Container Web UI
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/path/to/mini-container
ExecStart=/path/to/mini-container/mini-container-ui
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### فعال‌سازی سرویس

```bash
# بارگذاری مجدد systemd
sudo systemctl daemon-reload

# فعال‌سازی سرویس
sudo systemctl enable mini-container-ui

# شروع سرویس
sudo systemctl start mini-container-ui

# بررسی وضعیت
sudo systemctl status mini-container-ui

# مشاهده لاگ‌ها
sudo journalctl -u mini-container-ui -f
```

## استفاده با Docker

اگر می‌خواهید در Docker اجرا کنید:

```bash
# ساخت و اجرا
docker-compose up --build

# یا فقط اجرا
docker-compose up

# اجرا در پس‌زمینه
docker-compose up -d

# مشاهده لاگ‌ها
docker-compose logs -f

# متوقف کردن
docker-compose down
```

## نکات مهم

1. **دسترسی root**: برای اجرای کامل کانتینرها، نیاز به root دارید
2. **پورت**: پورت پیش‌فرض 8080 است، می‌توانید تغییر دهید
3. **فایروال**: اگر از راه دور دسترسی دارید، پورت را باز کنید
4. **امنیت**: در محیط production از HTTPS و احراز هویت استفاده کنید
5. **لاگ‌ها**: لاگ‌ها در console نمایش داده می‌شوند

## مثال کامل

```bash
# 1. نصب پیش‌نیازها
sudo apt update
sudo apt install -y g++ make

# 2. کلون پروژه
git clone https://github.com/mroraie/mini-container.git
cd mini-container

# 3. ساخت
make ui

# 4. اجرا
sudo ./mini-container-ui

# 5. باز کردن در مرورگر (در ترمینال دیگر)
xdg-open http://localhost:8080
```

## پشتیبانی

اگر مشکلی داشتید:
- بررسی کنید که تمام پیش‌نیازها نصب شده‌اند
- بررسی کنید که دسترسی root دارید
- لاگ‌ها را بررسی کنید
- Issues را در GitHub بررسی کنید

