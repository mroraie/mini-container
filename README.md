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

## محدودیت‌ها

- نیاز به root
- فقط Linux
- پیاده‌سازی پایه (غیر تولیدی)

## مجوز

پروژه آموزشی - استفاده آزاد
