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

## Installation

> Install kernel headers and build tools for your distribution first.

```bash
# clone
git clone https://github.com/<you>/gen4-mt7902.git
cd gen4-mt7902

# build
make -j$(nproc)

# install (root)
sudo make install -j$(nproc)

# install firmware
sudo make install_fw

# reboot
sudo reboot
```

### Post-install: systemd service (required)

You need to start the driver after user login. A systemd service file `mt7902-late.service` is provided in the main directory.

1. Edit `mt7902-late.service` and replace `<YOUR_USERNAME>` with your actual username
2. Copy and enable:

```bash
sudo cp mt7902-late.service /etc/systemd/system/
sudo systemctl enable mt7902-late.service
sudo systemctl start mt7902-late.service
```

### Kernel command line parameters

You may need to add these boot parameters to `/etc/default/grub`:

```bash
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash pci=pcie_bus_perf pcie_aspm=off pcie_port_pm=off clkreq.enable=0 intel_iommu=on iommu=pt ramoops.mem_size=1048576 ramoops.record_size=65536 ramoops.console_size=65536 panic_on_oops=1 softlockup_panic=1 hung_task_timeout_secs=120 nmi_watchdog=1"
```

After editing, run:

```bash
sudo update-grub
sudo reboot
```

**Note:** I have not tested which parameters are strictly necessary; this is my current working configuration.

### DKMS installation (optional)

```bash
sudo apt install dkms          # Debian/Ubuntu
sudo mkdir -p /usr/src/gen4-mt7902-0.1
sudo cp -r . /usr/src/gen4-mt7902-0.1
sudo dkms add -m gen4-mt7902 -v 0.1
sudo dkms build -m gen4-mt7902 -v 0.1
sudo dkms install -m gen4-mt7902 -v 0.1
```

---

## Firmware & licensing

This repository includes firmware files used for reproducible testing. Firmware may contain vendor licensing — see `FIRMWARE_LICENSES.md` for provenance notes and license text (if available).

**Power dBm settings source:** Radio power/dBm configuration values were obtained from the [Amazon Kara device dump](https://github.com/r0rt1z2-dumpyard/amazon_kara_dump). YMMV — maximum sketch, but seems to work well on my laptop.

**If you prefer not to include firmware in your public repo:** remove `firmware/` and instruct users to obtain firmware separately.

---

## Troubleshooting & recovery

### Shutdown/reboot hang

If your system hangs during shutdown or reboot, manually unload the module first:

```bash
sudo rmmod mt7902
```

I don't yet know how to make this automatic.

### If the device dies with `BAR0 = 0xdead0003` pattern (MMIO dead)

1. Shut down the machine
2. Unplug AC and remove battery (if easily removable)
3. Hold the power button 20–40 seconds to drain charge and clear retention latches
4. Wait ~1–2 minutes, reattach power and boot

This performs a full hardware power cycle to clear stuck power/reset domains.

### Soft recovery attempts

May not work if the PCIe fabric is latched:

```bash
sudo rmmod mt7902
sudo modprobe mt7902

# or try bus rescan
echo 1 | sudo tee /sys/bus/pci/rescan
```

### Testing and debugging

**Disable ASPM temporarily** (for Wi-Fi device only):

```bash
# for device 0000:02:00.0 (check your device with lspci)
sudo setpci -s 02:00.0 CAP_EXP+0x10.w=0x0000
```

**Check runtime PM status:**

```bash
cat /sys/bus/pci/devices/0000:02:00.0/power/runtime_status
echo on | sudo tee /sys/bus/pci/devices/0000:02:00.0/power/control
```

**Warning:** Avoid `setpci -s <bus> COMMAND=0x0` on a working device.

---

## Development notes

* Use `make clean` and `git clean -fdX` to clear build artifacts
* Suggested branch workflow: keep `backup-raw-history` with original commits before history surgery. Use a `tidy-history` branch for the public-clean history you push
* If you fork upstream, set `upstream` remote and keep `origin` as your fork for PRs

---

## Contributing

See `CONTRIBUTING.md` for how to run tests, write patches, and prepare PRs. If you submit firmware or vendor settings, explicitly document provenance in `FIRMWARE_LICENSES.md`.

---

## CHANGELOG

See `CHANGELOG.md` for the full shortlog. Example release:

```
v0.1.0 — initial BSP import, basic MT7902 bring-up, firmware packaging, initial fixes for runtime PM and bring-up.
```
