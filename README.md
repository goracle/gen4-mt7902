````markdown
# gen4-mt7902

> ⚠️ **Work-In-Progress — Use at your own risk.** This driver is experimental and intended for advanced users who accept manual recovery steps. It is **not** recommended for daily use on systems where stable networking is critical.

A focused out-of-tree driver derived from Mediatek's `gen4-mt79xx` (Xiaomi rodin BSP) to support the Mediatek **MT7902** PCIe M.2 card. This repository includes driver sources and the firmware files used during testing.

I started from [hmtheboy154/gen4-mt7902](https://github.com/hmtheboy154/gen4-mt7902) and made changes until it was stable enough on my laptop.

---

## Quick summary

- **Status:** buildable, loadable, connects to 2.4 GHz and 5 GHz (no 6 GHz firmware included).  
- **Kernel tested:** Ubuntu 24.04.3 LTS with `6.8.0-40-generic` (your results may vary).  
- **Known:** intermittent long-uptime instability under heavy DMA + deep-idle transitions.  
- **Recovery:** if the card becomes unresponsive, a full power drain (hold power button with AC unplugged) reliably recovers the device.

---

## Status & known issues

### Working
- Basic STA mode: 2.4 GHz and 5 GHz connections

### Known issues
- Flaky band switching on dual-band SSIDs (antenna/regulatory influence)
- WPA3: reported problems with `iwd`; `wpa_supplicant` is more reliable
- AP/hotspot mode is not fully tested/functional
- Large build size (~100 MB with debug blobs). Consider removing debug for distribution builds
- S2Idle may work; S3 resume often fails (black-screen)
- Intermittent kernel panic reports on some hardware (see issues)
- **Shutdown/reboot hang:** you may need to manually `sudo rmmod mt7902` before shutdown/reboot

---

## Installation (what you *must* do)

> The firmware installation step below is **required** — the driver will not function correctly without the firmware installed into the system firmware directory. The module install/start behavior is currently not automated by `sudo make install` (see note below); choose **either** the systemd path (optional) **or** the manual insmod/modprobe path after login.

### 1) Prereqs
Install kernel headers and build tools for your distribution first (example for Debian/Ubuntu):

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) bc libelf-dev
````

### 2) Clone & build

```bash
git clone https://github.com/<you>/gen4-mt7902.git
cd gen4-mt7902
make -j$(nproc)
```

### 3) **Firmware (required)**

**This is required** — copy the firmware files to your system firmware directory. You can use the provided helper:

```bash
# installs firmware to the system firmware directory (usually /lib/firmware)
sudo make install_fw
```

If you prefer to do it manually:

```bash
# example (adjust if your distro uses /usr/lib/firmware instead)
sudo cp -v firmware/* /lib/firmware/
sudo chmod 644 /lib/firmware/WIFI_RAM_CODE_MT7902_1.bin
sudo update-initramfs -u   # optional, only if you want firmware included in initramfs
```

Confirm firmware exists where your kernel expects it (e.g. `/lib/firmware/WIFI_RAM_CODE_MT7902_1.bin`).

> **Why:** without the firmware in the system firmware path, the kernel driver will fail to load/initialize the device even if the module is present.

---

## Important note about `sudo make install` (behavior)

`sudo make install` in this tree **does not** currently provide a robust, automatic "install & enable the driver at boot" experience.

* It may copy kernel objects / artifacts, but **it does not guarantee the module will be loaded at boot**.
* At present, you must choose one of the two approaches below to get the module loaded for interactive use:

### A) Recommended for systemd users — Optional systemd helper (explicit)

(Use this if you want the module to be started automatically *after user login* via the supplied service.)

1. Edit the service file to set your username:

   ```bash
   # Replace <YOUR_USERNAME> in the file
   sed -i 's/<YOUR_USERNAME>/your-username-here/' mt7902-late.service
   ```

2. Copy and enable the service:

   ```bash
   sudo cp mt7902-late.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now mt7902-late.service
   # Check status:
   sudo systemctl status mt7902-late.service
   ```

3. The `mt7902-late.service` will run after login to bind/load the module. This path is **optional** — use it only if you want the service-managed load behavior.

### B) Simple manual / per-login method (no systemd)

(Use this if you do **not** want to use the systemd service.)

After building, load the module manually after you log in (or add to your login scripts):

```bash
# as root (or via sudo)
sudo insmod ./mt7902.ko            # insert local built module (path to the .ko)
# OR prefer modprobe if installed to kernel modules path:
sudo cp -v out/mt7902.ko /lib/modules/$(uname -r)/kernel/drivers/net/wireless/
sudo depmod -a
sudo modprobe mt7902
```

If you want the module loaded on every boot without systemd, consider adding the module name to `/etc/modules-load.d/gen4-mt7902.conf`:

```bash
echo mt7902 | sudo tee /etc/modules-load.d/gen4-mt7902.conf
```

> Note: Because of current quirks, some systems may still require you to `insmod` or `modprobe` after login rather than at early boot. If you hit issues, prefer the systemd `mt7902-late.service` or an explicit user login hook.

---

## DKMS (optional)

If you prefer DKMS so the module rebuilds for new kernels:

```bash
sudo apt install dkms
sudo mkdir -p /usr/src/gen4-mt7902-0.1
sudo cp -r . /usr/src/gen4-mt7902-0.1
sudo dkms add -m gen4-mt7902 -v 0.1
sudo dkms build -m gen4-mt7902 -v 0.1
sudo dkms install -m gen4-mt7902 -v 0.1
```

**Important:** DKMS handles module compilation/installation but **does not** replace the required firmware step above. Make sure `make install_fw` (or manual copy) is done first.

---

## Post-install: troubleshooting & recovery

### Shutdown/reboot hang

If your system hangs during shutdown or reboot, manually unload the module first:

```bash
sudo rmmod mt7902
```

I don't yet know how to make this automatic.

### BAR0 = `0xdead0003` (MMIO dead) recovery

1. Shut down the machine
2. Unplug AC and remove battery (if easily removable)
3. Hold the power button 20–40 seconds to drain charge and clear retention latches
4. Wait ~1–2 minutes, reattach power and boot

This performs a full hardware power cycle to clear stuck power/reset domains.

### Soft recovery attempts

May not work if the PCIe fabric is latched:

```bash
sudo rmmod mt7902 || true
sudo modprobe mt7902
# or try bus rescan
echo 1 | sudo tee /sys/bus/pci/rescan
```

---

## Troubleshooting tips & useful commands

**Check runtime PM status:**

```bash
cat /sys/bus/pci/devices/0000:02:00.0/power/runtime_status
echo on | sudo tee /sys/bus/pci/devices/0000:02:00.0/power/control
```

**Disable ASPM temporarily** (for Wi-Fi device only):

```bash
# for device 0000:02:00.0 (check your device with lspci)
sudo setpci -s 02:00.0 CAP_EXP+0x10.w=0x0000
```

**Check firmware load errors:**

```bash
dmesg | grep -i mt7902
journalctl -k | grep mt7902
```

---

## Development notes

* Use `make clean` and `git clean -fdX` to clear build artifacts
* Suggested branch workflow: keep `backup-raw-history` with original commits before history surgery. Use a `tidy-history` branch for the public-clean history you push
* If you fork upstream, set `upstream` remote and keep `origin` as your fork for PRs

---

## Firmware & licensing

This repo includes firmware files used for reproducible testing. Firmware may contain vendor licensing — see `FIRMWARE_LICENSES.md` for provenance notes and license text (if available).

If you prefer not to include firmware in your public repo: remove `firmware/` from the tree and instruct users to obtain firmware separately — but remember that **local testing requires the firmware** in the system firmware directory.

---

## Contributing

See `CONTRIBUTING.md` for how to run tests, write patches, and prepare PRs. If you submit firmware or vendor settings, explicitly document provenance in `FIRMWARE_LICENSES.md`.

---

## CHANGELOG

See `CHANGELOG.md` for the full shortlog. Example release:

```
v0.1.0 — initial BSP import, basic MT7902 bring-up, firmware packaging, initial fixes for runtime PM and bring-up.
```

```
```

