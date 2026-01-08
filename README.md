<div style="display: flex; align-items: flex-start; margin-bottom: 30px;">
  <img src="docs/mini-continer.png" alt="لوگو مینی کانتینر" width="150" style="margin-right: 25px; border-radius: 10px; box-shadow: 2px 2px 6px rgba(0,0,0,0.2);">
  <div>
    <h1>مینی کانتنینر</h1>
    <p>
      این پروژه پیاده‌سازی سبک‌وزن کانتینرها با استفاده از <strong>Linux namespaces</strong>، <strong>cgroups</strong> و <strong>filesystem isolation</strong> است.
      هدف آن نمایش مفاهیم اصلی سیستم‌عامل شامل <strong>ایزولاسیون فرایند</strong>، <strong>مدیریت منابع</strong> و <strong>امنیت فایل‌سیستم</strong> است.
    </p>
  </div>
</div>



<div style="text-align: center; margin-bottom: 40px;">
  <img src="docs/imgs/dr0k22dr0k22dr0k.png" alt="ویژگی‌های کلیدی" width="500" style="border-radius: 12px; box-shadow: 2px 2px 10px rgba(0,0,0,0.3);">
</div>

<h2 style="text-align: center;">ویژگی‌های کلیدی</h2>
<ul style="line-height: 1.8; max-width: 700px; margin: 0 auto;">
  <li><strong>ایزولاسیون فرایند</strong>: از فضای نام لینوکس استفاده می‌کند (PID, mount, UTS, network, user)</li>
  <li><strong>مدیریت منابع</strong>: محدودیت‌های CPU و حافظه از طریق گروه‌های کنترل (cgroups)</li>
  <li><strong>ایزولاسیون فایل‌سیستم</strong>: chroot برای دسترسی امن فایل‌سیستم</li>
  <li><strong>چرخه حیات کانتینر</strong>: عملیات ایجاد، شروع، توقف و نابودی کانتینر</li>
  <li><strong>رابط خط فرمان</strong>: ابزارهای CLI و منوی گرافیکی برای مدیریت کانتینر</li>
</ul>


<h2>پیش‌نیازها</h2>
<ul style="line-height: 1.6;">
  <li><strong>هسته لینوکس</strong>: ۳.۸+ (برای پشتیبانی فضای نام)</li>
  <li><strong>کامپایلر C++</strong>: GCC (g++) با پشتیبانی C++11</li>
  <li><strong>دسترسی root</strong>: برای عملیات کانتینر ضروری است</li>
  <li><strong>سی‌گروپ‌ها (Cgroups)</strong>: mount شده در <code>/sys/fs/cgroup</code></li>
  <li><strong>sudo</strong>: برای اجرای کانتینرهای privileged</li>
</ul>

## پیش‌نیازها برای اجرا
* **هسته لینوکس**: نسخه ۳.۸ یا بالاتر.
* **کامپایلر**: GCC (g++) با پشتیبانی از C++11.
* **دسترسی**: اجرای دستورات با سطح دسترسی `root` یا `sudo`.
* **تقسیم منابع با Cgroups**: باید در مسیر `/sys/fs/cgroup` مونت شده باشد.

## نصب و راه‌اندازی

کامپایل پروژه:
```bash
git clone https://github.com/mroraie/mini-container.git
cd mini-container
make clean
make
./mini-container

```

## نحوه استفاده

### 1) رابط گرافیکی ترمینال

برای تجربه بصری مدیریت کانتینرها:

```bash
./mini-container-ui

```
### 2) دستورات خط فرمان (CLI)
* **اجرای یک دستور در کانتینر:**
`./mini-container run /bin/echo "Hello World"`
* **اجرا با محدودیت منابع:**
`./mini-container run --memory 128 --cpu 512 /bin/sh`
* **مشاهده وضعیت:**
`./mini-container list`
`./mini-container info <container_id>`
* **توقف و حذف:**
`./mini-container stop <container_id>`
`./mini-container destroy <container_id>`

### 3) نمایش در وب سرور




## مفاهیم سیستم‌عامل به کار رفته

### ۱. Linux Namespaces

این تکنولوژی باعث می‌شود فرایند داخل کانتینر فکر کند سیستم کاملاً متعلق به خودش می باشد و کانتینر دیگری در حال اجرا نمی باشد:

* **PID**: فرایندها فقط خودشان را می‌بینند.
* **Mount**: فایل‌سیستم ایزوله.
* **UTS**: نام میزبان (Hostname) جداگانه.
* **Network**: کارت شبکه مجازی مستقل.

### ۲. Control Groups (Cgroups)

برای جلوگیری از مصرف تمام منابع سیستم توسط یک کانتینر:

* **CPU**: تعیین سهمیه پردازش.
* **Memory**: تعیین سقف مصرف رم و Swap.

### ۳. فراخوانی‌های سیستمی (System Calls)

* `clone()`: برای ایجاد فرایند جدید با فضاهای نام مشخص.
* `unshare()`: برای جدا کردن بخش‌های مشترک فرایند.
* `mount()` و `chroot()`: برای ایزولاسیون دایرکتوری‌ها.


## مقایسه مینی کانتینر با داکر (Docker)
برای ساخت این پروژه از داکر الهام گرفته شده است و در طراحی و پیاده سازی سعی شده است که مفاهیم اولیه ای از داکر در این پروژه استفاده شود:

| ویژگی | مینی کانتینر | Docker |
| --- | --- | --- |
| **ایزولاسیون** | فضای نام هسته (Namespace) | فضای نام + لایه‌های امنیتی اضافی |
| **مدیریت منابع** | cgroups ساده | cgroups پیشرفته + سهمیه‌بندی دقیق |
| **فایل‌سیستم** | chroot ساده | سیستم لایه‌ای (OverlayFS) |
| **شبکه** | فضای نام پایه | پل‌های مجازی (Bridge) و شبکه پیچیده |
| **پیچیدگی** | بسیار کم (آموزشی) | بالا (صنعتی) |

## تست‌های عملکرد

### مثال: CPU Usage Test

```bash
while true; do :; done   # حلقه بی‌نهایت برای تست CPU
```

### مثال: Memory Limit Test

```bash
dd if=/dev/zero of=/tmp/mem bs=1M count=80  # تست محدودیت حافظه
```

### اجرای همه تست‌ها

```bash
sudo ./mini-container
# گزینه 7: Run Tests
```

### تست‌های منو (گزینه‌های 8-17)

در منوی اصلی می‌توانید با انتخاب گزینه‌های 8 تا 17، کانتینرهای تست مختلف را ایجاد کنید:

- **گزینه 8**: CPU Intensive Test - 4 فرآیند `yes` همزمان
- **گزینه 9**: RAM Intensive Test - تخصیص 150MB حافظه
- **گزینه 10**: CPU+RAM Heavy Test - تست ترکیبی CPU و RAM
- **گزینه 11**: CPU Calc Test - محاسبات Python سنگین
- **گزینه 12**: Memory Stress Test - تخصیص 150MB حافظه
- **گزینه 13**: Mixed Workload Test - CPU + I/O همزمان
- **گزینه 14**: High CPU Test - 3 فرآیند `yes`
- **گزینه 15**: High Memory Test - تخصیص 200MB حافظه
- **گزینه 16**: Balanced Test - کاربرد متعادل CPU و RAM
- **گزینه 17**: Max Stress Test - 5 فرآیند `yes` + محاسبات سنگین

**مشخصات مشترک همه کانتینرهای تست:**
- Memory Limit: 128 MB
- CPU Limit: 5% یک هسته (quota: 5000us, period: 100000us)
- CPU Shares: 512
- Runtime: 600 ثانیه (10 دقیقه)

### فرمت دستورات Memory و CPU

#### محدودیت Memory (حافظه)

```bash
# فرمت: --memory <MB>
# مثال: محدودیت 1GB (1024 MB)
./mini-container run --memory 1024 /bin/sh

# مثال: محدودیت 512 MB
./mini-container run --memory 512 /bin/sh

# مثال: محدودیت 256 MB
./mini-container run --memory 256 /bin/sh
```

**نکات:**
- مقدار به مگابایت (MB) وارد می‌شود
- به صورت خودکار به بایت تبدیل می‌شود (MB × 1024 × 1024)
- در cgroups به صورت `memory.limit_in_bytes` تنظیم می‌شود

#### محدودیت CPU

```bash
# فرمت: --cpu <shares>
# مثال: CPU shares پیش‌فرض (1024)
./mini-container run --cpu 1024 /bin/sh

# مثال: نصف CPU (512 shares)
./mini-container run --cpu 512 /bin/sh

# مثال: یک چهارم CPU (256 shares)
./mini-container run --cpu 256 /bin/sh
```

**نکات:**
- CPU shares برای توزیع نسبی CPU بین کانتینرها استفاده می‌شود
- مقدار بالاتر = سهم بیشتر از CPU
- در cgroups به صورت `cpu.shares` تنظیم می‌شود

#### محدودیت CPU با Quota و Period (پیشرفته)

برای محدودیت دقیق‌تر CPU، از `cpu.quota_us` و `cpu.period_us` استفاده می‌شود:

```c
// مثال: 50% یک هسته
cpu_quota_us = 50000;   // 50ms
cpu_period_us = 100000; // 100ms
// نتیجه: 50000/100000 = 0.5 = 50%

// مثال: 25% یک هسته
cpu_quota_us = 25000;   // 25ms
cpu_period_us = 100000; // 100ms
// نتیجه: 25000/100000 = 0.25 = 25%

// مثال: 100% یک هسته
cpu_quota_us = 100000;  // 100ms
cpu_period_us = 100000; // 100ms
// نتیجه: 100000/100000 = 1.0 = 100%
```

**فرمول محاسبه:**
```
CPU Percentage = (cpu_quota_us / cpu_period_us) × 100%
```

#### ترکیب Memory و CPU

```bash
# مثال: کانتینر با 512MB RAM و 512 CPU shares
./mini-container run --memory 512 --cpu 512 /bin/sh

# مثال: کانتینر با 1GB RAM و 1024 CPU shares
./mini-container run --memory 1024 --cpu 1024 /bin/sh
```

<!-- =================== مقایسه با Docker =================== -->

<h2>مقایسه با Docker</h2>
<table style="border-collapse: collapse; width: 100%;">
  <thead>
    <tr>
      <th style="border: 1px solid #ddd; padding: 8px;">ویژگی</th>
      <th style="border: 1px solid #ddd; padding: 8px;">مینی کانتینر</th>
      <th style="border: 1px solid #ddd; padding: 8px;">Docker</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td style="border: 1px solid #ddd; padding: 8px;">ایزولاسیون</td>
      <td style="border: 1px solid #ddd; padding: 8px;">فضای نام هسته</td>
      <td style="border: 1px solid #ddd; padding: 8px;">فضای نام + لایه‌های اضافی</td>
    </tr>
    <tr>
      <td style="border: 1px solid #ddd; padding: 8px;">مدیریت منابع</td>
      <td style="border: 1px solid #ddd; padding: 8px;">cgroups</td>
      <td style="border: 1px solid #ddd; padding: 8px;">cgroups + کنترل‌های اضافی</td>
    </tr>
    <tr>
      <td style="border: 1px solid #ddd; padding: 8px;">فایل‌سیستم</td>
      <td style="border: 1px solid #ddd; padding: 8px;">chroot/pivot_root</td>
      <td style="border: 1px solid #ddd; padding: 8px;">فایل‌سیستم لایه‌ای + overlay</td>
    </tr>
    <tr>
      <td style="border: 1px solid #ddd; padding: 8px;">شبکه</td>
      <td style="border: 1px solid #ddd; padding: 8px;">فضای نام پایه</td>
      <td style="border: 1px solid #ddd; padding: 8px;">شبکه پیشرفته</td>
    </tr>
    <tr>
      <td style="border: 1px solid #ddd; padding: 8px;">ایمیج‌ها</td>
      <td style="border: 1px solid #ddd; padding: 8px;">تنظیم دستی</td>
      <td style="border: 1px solid #ddd; padding: 8px;">لایه‌های ایمیج + registry</td>
    </tr>
    <tr>
      <td style="border: 1px solid #ddd; padding: 8px;">پیچیدگی</td>
      <td style="border: 1px solid #ddd; padding: 8px;">حداقل</td>
      <td style="border: 1px solid #ddd; padding: 8px;">ویژگی کامل</td>
    </tr>
  </tbody>
</table>
```

---

این نسخه:

* **تمام تصاویر کنار متن هستند** و فاصله/سایه دارند
* **لیست‌ها خوانا و فاصله‌دار** شده‌اند
* **کدها قالب‌بندی شده با `bash` یا `pre`**
* **جدول مقایسه مرتب و خوانا**
* ظاهر **حرفه‌ای برای GitHub و VSCode**
