#!/bin/bash
# MT7902 Gen4 Driver Diagnostic Script

echo "=== MT7902 Gen4 Driver Diagnostics ==="
echo

echo "1. Checking for MT7902 device..."
lspci -d 14c3:7902 -nn -vvv

echo
echo "2. Checking loaded modules..."
lsmod | grep -E "(wlan|mt76|mt79|debug)"

echo
echo "3. Checking for network interfaces..."
ip link show | grep -E "(wlan|wl)"
ls -la /sys/class/net/ | grep -E "(wlan|wl)"

echo
echo "4. Checking firmware files..."
ls -lah /lib/firmware/mediatek/ 2>/dev/null | grep -i mt7902
ls -lah /lib/firmware/ 2>/dev/null | grep -i mt7902

echo
echo "5. Recent kernel messages (last 50 lines)..."
dmesg | tail -50

echo
echo "6. Checking module parameters..."
if [ -d /sys/module/wlan ]; then
    echo "wlan module parameters:"
    ls -la /sys/module/wlan/parameters/
fi

if [ -d /sys/module/debug ]; then
    echo "debug module parameters:"
    ls -la /sys/module/debug/parameters/
fi

echo
echo "7. PCI device state..."
if [ -d /sys/bus/pci/devices/0000:* ]; then
    for dev in /sys/bus/pci/devices/0000:*/device; do
        if [ -f "$dev" ]; then
            vendor=$(cat $dev | grep "0x7902" || true)
            if [ -n "$vendor" ]; then
                basedir=$(dirname "$dev")
                echo "Found MT7902 at $basedir"
                echo "  Vendor: $(cat $basedir/vendor)"
                echo "  Device: $(cat $basedir/device)"
                echo "  Enable: $(cat $basedir/enable)"
                echo "  IRQ: $(cat $basedir/irq)"
                [ -f "$basedir/power/runtime_status" ] && echo "  Power: $(cat $basedir/power/runtime_status)"
            fi
        fi
    done
fi

echo
echo "8. Checking for error messages..."
dmesg | grep -i -E "(error|fail|warn)" | grep -i -E "(wlan|mt79|7902)" | tail -20

echo
echo "=== Diagnostics Complete ==="
