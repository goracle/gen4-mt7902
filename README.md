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

### 3) Configure Systemd Services
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

### 4) DKMS Installation (Auto-rebuild on Update)
Since Arch updates the kernel frequently, DKMS is recommended. The directory name must match the version in `dkms.conf`.

```bash
# 1. Create the versioned symlink in /usr/src
sudo ln -s $(pwd) /usr/src/gen4-mt7902-0.1

# 2. Register and Install
sudo dkms add -m gen4-mt7902 -v 0.1
sudo dkms install -m gen4-mt7902 -v 0.1
```

### 5) Manual Load
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

## Troubleshooting
* **Shutdown Hang:** Ensure `mt7902-reset.service` is enabled; it unloads the module to prevent the PCIe bus from hanging the kernel.
* **Logs:** Check `dmesg | grep mt7902` for firmware loading status.