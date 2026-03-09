#!/bin/bash
# 🔧 MT7902 WiFi Driver Loader 🔧
# Optimized to kill "prions" and handle incrementing interface names

set -e

MODULE_NAME="mt7902"
# Use a dynamic path that works regardless of the username
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODULE_PATH="$SCRIPT_DIR/${MODULE_NAME}.ko"
LOG_PATH="./mt7902_dmesg.log"

log() {
    echo "[$MODULE_NAME] $1"
}

# 1. Identify and Kill the "Zombie" Interfaces
log "🔍 Locating ghost interfaces..."
# This finds any interface starting with 'enp' or 'usb' that uses the usbnet stack
GHOST_IFACE=$(ip -o link show | grep -E "enp0|usb|enx" | awk -F': ' '{print $2}' | head -n 1)

if [ -n "$GHOST_IFACE" ]; then
    log "🛑 Bringing down zombie interface: $GHOST_IFACE"
    sudo ip link set dev "$GHOST_IFACE" down || true
fi

# 2. The "Deep Scrub" Cleanup
log "🗑️  Removing old module and cleaning the kitchen..."
sudo rmmod $MODULE_NAME 2>/dev/null || true

# Unload the helpers. Note: it's 'cdc_ether', not 'cdc_ethernet'
sudo modprobe -r cdc_ether usbnet 2>/dev/null || true

# Verify they are gone
if lsmod | grep -q "usbnet"; then
    log "⚠️  Warning: usbnet still resident. Force killing..."
    sudo rmmod -f usbnet 2>/dev/null || true
fi

sleep 2

# 3. Fresh Start
log "🧼 Reloading clean helper modules..."
sudo modprobe usbnet cdc_ether
sudo iw reg set US

# 4. Load the Franken-Driver
log "🚀 Loading module from $MODULE_PATH..."
sudo dmesg -C
sudo insmod "$MODULE_PATH"
# Saving log to your current home directory
sudo dmesg > "$LOG_PATH"
# 5. Wait for the new interface to settle
log "⏳ Waiting for interface to settle..."
sleep 3

# Look for 'wlan' or the new incremented 'enp' name
INTERFACE=$(ip -o link show | grep -E "wlan|enp0|enx" | awk -F': ' '{print $2}' | tail -n 1)

if [ -n "$INTERFACE" ]; then
    log "✨ Interface $INTERFACE created successfully! ✨"
    log "📊 Current MTU: $(cat /sys/class/net/$INTERFACE/mtu 2>/dev/null || echo 'unknown')"
else
    log "⚠️  WARNING: No network interface detected."
    log "🔍 Check $LOG_PATH for details."
fi

log "🎉 Cleanup complete. If it panics now, it's a code-level NULL dereference."

sleep 5; sudo dmesg | grep -m1 'AUTH TX START' -A400 > err.txt
