
# Watchdog Service Guide

This document describes how to set up the **Userspace Watchdog Daemon** to interact with the Avalue Kernel Driver. The service is responsible for "kicking" (feeding) the watchdog timer to prevent the system from resetting.

## Prerequisites

Before setting up the service, ensure the kernel driver is loaded:

```bash
# Verify the driver is loaded
lsmod | grep wdt

# Verify the device node exists
ls -l /dev/watchdog*
```

---

## Using Standard Linux Watchdog Daemon

For production environments, it is recommended to use the standard Linux `watchdog` package. It provides robust monitoring features (CPU load, memory usage, network status, etc.).

### 1. Installation

**Ubuntu/Debian:**

```bash
sudo apt update
sudo apt install watchdog
```

**CentOS/RHEL:**

```bash
sudo yum install watchdog
```

### 2. Configuration

Edit the configuration file `/etc/watchdog.conf`:

```bash
sudo nano /etc/watchdog.conf
```

Uncomment and modify the following lines to match the hardware driver:

```ini
# The device node created by this driver
watchdog-device = /dev/watchdog # or /dev/watchdog0

# Interval between heartbeats (seconds)
# Must be smaller than the hardware timeout (default is usually 60s)
interval = 10
```

### 3. Service Management

Enable and start the service:

```bash
sudo systemctl enable watchdog
sudo systemctl start watchdog
```

Check status:

```bash
sudo systemctl status watchdog
```
---

## ⚠️ Verification & Testing

**WARNING: The following tests will cause a system reboot.**

To verify that the hardware watchdog is actually working (i.e., it reboots the system when not fed), perform the following test:

### Step 1: Check Timeout

Check the current timeout setting

```bash
cat /sys/class/watchdog/watchdog0/timeleft
```

### Step 2: Stop All Watchdog Services

Stop all watchdog-related services to simulate a system freeze.

```bash
sudo killall -9 watchdog
sudo killall -9 wd_keepalive
```

### Step 3: Verify Watchdog Device is Released

Ensure no process is holding the `/dev/watchdog0` device:

```bash
sudo lsof /dev/watchdog0
```

If any process is still holding it, kill it explicitly:

```bash
sudo kill -9 <pid>  # e.g., sudo kill -9 234
```

### Step 4: Wait for Reboot

Wait for the timeout period (e.g., 60 seconds). The system should automatically reset.
