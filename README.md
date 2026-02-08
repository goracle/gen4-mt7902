# gen4-mt7902

> ⚠️ **Experimental Driver** — Custom fork optimized for the Mediatek **MT7902** PCIe card. Developed and tested on Asus Vivobook (2023) hardware.

This repository provides an out-of-tree driver derived from Mediatek's `gen4-mt79xx` BSP. It includes a "Magic Delay" systemd service to bypass PCIe power management hangs and a reset service to prevent kernel panics during shutdown.

---

## Quick Start

### 1) Build the Module
```bash
make -j$(nproc)
```

### 2) Install Firmware (Required)
The driver will fail to initialize without the binary blobs in the system path.
```bash
sudo make install_fw
```
*Note: This usually installs to `/usr/lib/firmware/mediatek/mt7902/`.*

### 3) Blacklist Stock Drivers
The kernel's built-in MT792x drivers will conflict with this custom module. Create a blacklist file:

```bash
sudo tee /etc/modprobe.d/blacklist-mt7921.conf > /dev/null << 'EOF'
# Blacklist stock MediaTek WiFi drivers - using custom mt7902.ko instead
blacklist mt7921e
blacklist mt7902
blacklist mt7902e
blacklist mt7921_common
blacklist mt76_connac_lib
blacklist mt7921s
blacklist mt7921u
EOF
```

**Regenerate initramfs:**
```bash
# Ubuntu/Debian
sudo update-initramfs -u

# Arch/Manjaro
sudo mkinitcpio -P
```

### 4) Configure Systemd Services
To handle the sensitive timing of the MT7902, use the provided "Late Load" service.

1. **Update Username:**
   ```bash
   # Sets the working directory to your home folder
   sed -i 's/<YOUR_USERNAME>/dan/' mt7902-late.service
   ```

2. **Enable Services:**
   ```bash
   sudo cp mt7902-late.service mt7902-reset.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now mt7902-late.service
   sudo systemctl enable mt7902-reset.service
   ```

### 5) DKMS Installation (Auto-rebuild on Update)
Since Arch updates the kernel frequently, DKMS is recommended. The directory name must match the version in `dkms.conf`.

```bash
# 1. Create the versioned symlink in /usr/src
sudo ln -s $(pwd) /usr/src/gen4-mt7902-0.1

# 2. Register and Install
sudo dkms add -m gen4-mt7902 -v 0.1
sudo dkms install -m gen4-mt7902 -v 0.1
```

### 6) Manual Load
To start the interface immediately without rebooting:
```bash
sudo ./load.sh
```

---

## Status & Hardware Recovery

- **Kernels Tested:** `6.8.0` (Ubuntu) and `6.18+` (Arch).
- **Working:** 2.4 GHz and 5 GHz STA mode.
- **Not Working:** 6 GHz (missing firmware), S3 Sleep/Resume.

### ⚠️ Critical Recovery (BAR0 / Dead MMIO)
If the driver crashes, the hardware often latches into an unresponsive state where `insmod` or `modprobe` will fail. You **must** perform a full power drain before the device will function again:
1. Shut down the laptop and unplug the AC adapter.
2. Hold the **Power Button** for 40 seconds.
3. Plug back in and boot.

---

## Known Issues

### Early Boot Race Conditions
The driver has unresolved timing bugs during early system initialization. **The late-load systemd service is required** to avoid crashes. Loading via DKMS auto-load or early boot will likely fail.

- **Symptom:** Module loads but interface doesn't come up, or kernel panic during boot
- **Workaround:** Use `mt7902-late.service` to defer module loading until after system stabilization
- **Status:** ⚠️ Arch users report the late-load service is unreliable; manual `./load.sh` after boot may be necessary

### NetworkManager vs. wpa_supplicant
For best stability, use **wpa_supplicant** directly instead of NetworkManager-based tools:

```bash
# Disable NetworkManager for wlan0
sudo nmcli device set wlan0 managed no

# Use wpa_supplicant manually
sudo wpa_supplicant -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
```

**iwd** (iNet Wireless Daemon) is **not recommended** — it triggers firmware state machine bugs. Stick with wpa_supplicant for now.

---

## Troubleshooting
* **Shutdown Hang:** Ensure `mt7902-reset.service` is enabled; it unloads the module to prevent the PCIe bus from hanging the kernel.
* **Logs:** Check `dmesg | grep mt7902` for firmware loading status.