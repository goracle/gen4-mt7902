# gen4-mt7902 🔧

> ⚠️ **Experimental Driver/For Eductional Purposes Only** — Custom android->linux port fork optimized for the Mediatek **MT7902** PCIe card. Based on hmtheboy154's port of Mediatek's gen4-mt79xx driver (originally from Xiaomi's rodin BSP).

!!DO NOT USE IF YOU NEED WIFI NOW!!.  THERE IS AN OFFICIAL DRIVER BACKPORT OUT: 

https://github.com/hmtheboy154/mt7902

This repository is a heavily modified fork of the unofficial driver, focusing on stability improvements and connection reliability/teaching me how wifi works. Includes hacky systemd services to work around PCIe power management hangs and prevent kernel panics during shutdown.

---

## 📊 Current Status

### ✅ What Works
- **Ubuntu (6.8.0 kernel)**: Fully functional with wpa_supplicant
  - Frozen at commit `2d980de7082e65ad7b4d68f85a4895d56b55f7d4` on `main` branch
  - 2.4 GHz and 5 GHz working perfectly
  - Use this if you want something stable!

### 🚧 What's Being Worked On
- **Arch Linux (6.18+ kernel)**: Experimental iwd branch
  - Connection works but still ironing out bugs
  - Massive rewrites to AIS FSM and connection logic
  - **This is where active development happens** 🔨

### ❌ What Doesn't Work (Yet)
- 6 GHz band (missing firmware blobs)
- S3 Sleep/Resume
- Auto-loading at boot (timing issues—see below)

---

## 🚀 Quick Start

### 1) Build the Module
```bash
make -j$(nproc)
```

### 2) Install Firmware (Required)
The driver will crash without these binary blobs:
```bash
sudo make install_fw
```
*Note: Installs to `/usr/lib/firmware/mediatek/mt7902/`*

### 3) Blacklist Stock Drivers
The kernel's built-in MT792x drivers will conflict. Create a blacklist file:

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

# Alma Linux / RHEL / Fedora
sudo dracut --force
```

### 4) Configure Systemd Services
The MT7902 is *extremely* sensitive to timing. Manual loading after boot is required.

1. **Update Username in Service File:**
   ```bash
   # Replace <YOUR_USERNAME> with your actual username
   sed -i 's/<YOUR_USERNAME>/yourname/' mt7902-late.service
   ```

2. **Install and Enable Services:**
   ```bash
   sudo cp mt7902-late.service mt7902-reset.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now mt7902-late.service
   sudo systemctl enable mt7902-reset.service
   ```

   > ⚠️ **Arch Users**: The late-load service is unreliable. You'll probably need to run `sudo ./load.sh` manually after boot. This is a known issue with no current fix.

### 5) DKMS Installation (Auto-rebuild on Kernel Updates)
Recommended for rolling-release distros:

```bash
# 1. Create the versioned symlink in /usr/src
sudo ln -s $(pwd) /usr/src/gen4-mt7902-0.1

# 2. Register and Install
sudo dkms add -m gen4-mt7902 -v 0.1
sudo dkms install -m gen4-mt7902 -v 0.1
```

> ⚠️ **Don't run `depmod -a` yet!** Boot issues are still being debugged.

### 6) Manual Load (Recommended)
To start the interface without rebooting:
```bash
sudo ./load.sh
```

To reset the driver after making changes:
```bash
sudo rmmod mt7902
sudo ./load.sh
```

---

## 🌐 Network Management

### Use iwd (Recommended for Testing)
For testing and development, **use iwd directly** and mask other network managers:

```bash
# Mask NetworkManager and wpa_supplicant
sudo systemctl mask NetworkManager
sudo systemctl mask wpa_supplicant

# Enable and start iwd
sudo systemctl enable --now iwd

# Connect to a network
iwctl station wlan0 connect "YourSSID"
```

**Why iwd?** It's simpler, has less moving parts, and makes debugging connection issues way easier. NetworkManager adds too many layers of complexity.

### Alternative: wpa_supplicant (Ubuntu Stable Branch)
If you're on the Ubuntu stable branch, stick with wpa_supplicant:

```bash
# Disable NetworkManager for wlan0
sudo nmcli device set wlan0 managed no

# Use wpa_supplicant manually
sudo wpa_supplicant -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
```

---

## 💀 Critical Recovery (Hardware Latchup)

### ⚠️ BAR0 / Dead MMIO State
If the driver crashes or hangs, the MT7902 hardware often locks up in an unresponsive state. **You MUST perform a full power drain before the device will work again:**

1. Shut down the laptop completely
2. Unplug the AC adapter
3. Hold the **Power Button** for **40 seconds**
4. Plug back in and boot

**Symptoms of hardware latchup:**
- `insmod` or `modprobe` fails immediately
- `dmesg` shows BAR0 read errors
- Driver loads but interface never appears

This is a hardware issue with the MT7902's PCIe controller, not a driver bug (as far as we know 🤷).

---

## 🐛 Known Issues

### Boot-Time Loading Doesn't Work
The driver has unresolved race conditions during early system initialization. **The late-load systemd service is REQUIRED**, but even that's flaky on Arch.

- **Symptom**: Module loads but interface doesn't come up, OR kernel panic during boot
- **Workaround**: Use `mt7902-late.service` (unreliable on Arch), or just run `./load.sh` manually after boot
- **Root Cause**: Still investigating timing bugs in the init sequence

### Shutdown Hangs
Enable the reset service to cleanly unload the module before shutdown:
```bash
sudo systemctl enable mt7902-reset.service
```

This prevents the PCIe bus from hanging the entire kernel during poweroff.

### Log Spam 📢
Yeah, this driver logs *a lot*. We're working on it. Use `dmesg -w | grep mt7902` to see what's happening.

---

## 🔍 Troubleshooting

### Check Firmware Loading
```bash
sudo dmesg | grep mt7902
```

Look for:
- `kalRequestFirmware(): mediatek/mt7902/wifi.cfg OK`
- `Patch download start`
- `FW Version: ...`

### Check Network Interface
```bash
ip link show wlan0
iwctl station wlan0 scan
iwctl station wlan0 get-networks
```

### Known Quirks
- **No NVRAM**: The driver complains about "Glue is NULL" because there's no NVRAM calibration data. Country code is set via `wifi.cfg`—**you're responsible for setting your own country code correctly!**
- **Excessive Scanning**: The driver scans way too aggressively. We're working on it.
- **Slow Association**: Sometimes it takes 10-20 seconds to connect. Be patient.

---

## 🛠️ Development Notes

- This is a fork of hmtheboy154's gen4-mt7902 driver (which came from Xiaomi's rodin BSP)
- We're ripping and tearing through Android-isms in the code to make it more Linux-native
- Recent commits have been 4000+ lines of changes (yikes)
- Ubuntu branch is frozen until Arch branch stabilizes—too much code divergence to maintain both
- Use `git diff` before committing to verify changes

**Upstream**: https://github.com/hmtheboy154/gen4-mt7902

---

## 📜 License

GPL v2 (inherited from Mediatek/Xiaomi sources)

---

## 🙏 Contributing

If you want to help, great! But be warned: this code is a mess. We're heavily modifying hmtheboy154's driver port to improve stability and connection handling. If that doesn't scare you off, pull requests welcome! 😅

**Upstream Project**: Check out hmtheboy154's original work at https://github.com/hmtheboy154/gen4-mt7902

**Testing Checklist:**
- Does it load at boot? (probably not)
- Does `./load.sh` work after boot? (hopefully)
- Can you connect to 2.4 GHz networks?
- Can you connect to 5 GHz networks?
- Does it survive a reboot without kernel panic?

---

**Disclaimer**: This driver is experimental. It might brick your network card (temporarily—see recovery steps above). It might flood your logs. It might make you question your life choices. Use at your own risk! 🎲
