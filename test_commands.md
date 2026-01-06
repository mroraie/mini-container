# دستورات آماده برای تست کانتینرها

این فایل شامل دستورات آماده برای تست عملکرد کانتینرها با بارهای مختلف CPU و Memory است.

## دستورات CPU-Intensive

### 1. حلقه بی‌نهایت ساده (ساده‌ترین)
```
/bin/sh -c "while true; do :; done"
```

### 2. محاسبات ریاضی سنگین
```
/bin/sh -c "i=0; while [ $i -lt 100000000 ]; do i=$((i+1)); done; echo Done"
```

### 3. محاسبه اعداد اول (CPU-intensive)
```
/bin/sh -c "n=2; while [ $n -lt 10000 ]; do i=2; prime=1; while [ $i -lt $n ]; do if [ $((n % i)) -eq 0 ]; then prime=0; break; fi; i=$((i+1)); done; if [ $prime -eq 1 ]; then echo $n; fi; n=$((n+1)); done"
```

### 4. محاسبات پیچیده ریاضی
```
/bin/sh -c "for i in {1..10000000}; do result=$((i * i % 7919)); done; echo $result"
```

### 5. فشرده‌سازی CPU (بسیار سنگین)
```
/bin/sh -c "i=0; sum=0; while [ $i -lt 50000000 ]; do sum=$((sum + (i*i)%7919)); i=$((i+1)); done; echo $sum"
```

## دستورات Memory-Intensive

### 6. تخصیص حافظه (16MB)
```
/bin/sh -c "dd if=/dev/zero of=/tmp/memtest bs=1M count=16 status=none && rm -f /tmp/memtest && echo Memory test done"
```

### 7. تخصیص حافظه (32MB)
```
/bin/sh -c "dd if=/dev/zero of=/tmp/memtest bs=1M count=32 status=none && rm -f /tmp/memtest && echo Memory test done"
```

### 8. تخصیص حافظه (64MB)
```
/bin/sh -c "dd if=/dev/zero of=/tmp/memtest bs=1M count=64 status=none && rm -f /tmp/memtest && echo Memory test done"
```

## دستورات ترکیبی (CPU + Memory)

### 9. CPU + Memory (متوسط)
```
/bin/sh -c "dd if=/dev/zero of=/tmp/stress bs=1M count=16 status=none; i=0; while [ $i -lt 10000000 ]; do i=$((i+1)); done; rm -f /tmp/stress; echo Done"
```

### 10. CPU + Memory (سنگین)
```
/bin/sh -c "dd if=/dev/zero of=/tmp/stress bs=1M count=32 status=none; n=20000000; i=0; sum=0; while [ $i -lt $n ]; do sum=$((sum + (i*i)%7919)); i=$((i+1)); done; rm -f /tmp/stress; echo $sum"
```

## دستورات ساده برای تست

### 11. دستور ساده (برای تست اولیه)
```
/bin/echo "Hello from container!"
```

### 12. لیست فایل‌ها
```
/bin/ls -la /
```

### 13. نمایش اطلاعات سیستم
```
/bin/sh -c "echo Hostname: $(hostname); echo PID: $$; echo Uptime test"
```

### 14. تست شبکه (اگر در دسترس باشد)
```
/bin/sh -c "echo 'Network test'; ping -c 3 127.0.0.1 2>/dev/null || echo 'Ping not available'"
```

## نحوه استفاده

در رابط وب، این دستورات را در فیلد "دستور اجرا" کپی کنید و کانتینر را اجرا کنید.

### تنظیمات پیشنهادی:

**برای تست CPU:**
- Memory: 128 MB
- CPU: 1024 shares

**برای تست Memory:**
- Memory: 64-128 MB (بسته به دستور)
- CPU: 1024 shares

**برای تست ترکیبی:**
- Memory: 128 MB
- CPU: 1024 shares

## نکات مهم

1. دستورات CPU-intensive ممکن است CPU را 100% درگیر کنند
2. دستورات Memory-intensive باید با محدودیت حافظه مناسب استفاده شوند
3. برخی دستورات ممکن است زمان زیادی طول بکشند
4. برای توقف کانتینر، می‌توانید از CLI استفاده کنید: `./mini-container stop <container_id>`

