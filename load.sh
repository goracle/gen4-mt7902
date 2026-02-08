#!/bin/bash

# MT7902 WiFi Driver Loading Script - Refactored
set -e

MODULE_NAME="mt7902"
# Use absolute path to ensure sudo doesn't lose the location
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODULE_PATH="$SCRIPT_DIR/${MODULE_NAME}.ko"

log() {
    echo -e "[\e[34m${MODULE_NAME}\e[0m] $1"
}

# 1. Clean up existing state
log "Cleaning up old module instances..."
rmmod $MODULE_NAME 2>/dev/null || true

# 2. Check dependencies
log "Ensuring kernel dependencies are loaded..."
for dep in cfg80211 mac80211 mt76-connac-lib; do
    if ! lsmod | grep -q "$dep"; then
        log "  Loading $dep..."
        modprobe "$dep" || log "  Warning: could not modprobe $dep"
    fi
done

# 3. PCI Check
if ! lspci -d 14c3:7902 >/dev/null 2>&1; then
    log "\e[31mERROR: MT7902 PCI device not visible.\e[0m"
    exit 1
fi

# 4. Insert Module
log "Inserting module from $MODULE_PATH..."
# Clear dmesg so we only see NEW logs
dmesg -C 
insmod "$MODULE_PATH"

# 5. Validation with more flexibility
log "Waiting for hardware initialization..."
MAX_RETRIES=10
for i in $(seq 1 $MAX_RETRIES); do
    # Check for ANY mt7902 related success in dmesg
    if dmesg | grep -Ei "mt7902|kalRequestFirmware.*OK|ready" > /dev/null; then
        log "\e[32m✓ Driver reports success in dmesg\e[0m"
        break
    fi
    
    # Also check if the interface actually appeared
    if ip link show | grep -q "wlan"; then
        log "\e[32m✓ WiFi interface appeared!\e[0m"
        break
    fi

    if [ $i -eq $MAX_RETRIES ]; then
        log "\e[31mERROR: Driver loaded but hardware not responding.\e[0m"
        dmesg | tail -n 20
        exit 1
    fi
    sleep 1
done

log "Finalizing..."
# Trigger a scan to wake it up
IFACE=$(ip link show | grep -oP 'wlan\d+' | head -1)
if [ -n "$IFACE" ]; then
    ip link set "$IFACE" up || true
    log "Device $IFACE is UP."
fi
sudo dmesg > /home/dan/builds/gen4-mt7902/mt7902_dmesg.log
