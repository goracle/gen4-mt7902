#!/bin/bash

# MT7902 WiFi Driver Loading Script with PCI wait
# Handles dependency loading and status reporting

set -e

MODULE_NAME="mt7902"
MODULE_PATH="$(dirname "$0")/${MODULE_NAME}.ko"
LOG_FILE="$(dirname "$0")/${MODULE_NAME}_dmesg.log"

log() {
    echo "[${MODULE_NAME}] $1"
}

log "Starting WiFi driver initialization"

# Check if module file exists
if [ ! -f "$MODULE_PATH" ]; then
    log "ERROR: Module not found at $MODULE_PATH"
    exit 1
fi

log "Module path: $MODULE_PATH"

# Wait for PCI device to be enumerated
log "Waiting for PCI device to be ready..."
PCI_READY=0
for i in {1..30}; do
    if lspci -d 14c3:7902 >/dev/null 2>&1; then
        log "✓ PCI device found after $i seconds"
        PCI_READY=1
        break
    fi
    sleep 1
done

if [ $PCI_READY -eq 0 ]; then
    log "ERROR: PCI device not found after 30 seconds!"
    lspci | grep -i mediatek || log "No MediaTek devices found at all"
    exit 1
fi

# Wait for firmware to be available
log "Checking firmware availability..."
FIRMWARE_READY=0
for i in {1..30}; do
    if [ -f /lib/firmware/mediatek/mt7902/wifi.cfg ]; then
        log "✓ Firmware found after $i attempts"
        FIRMWARE_READY=1
        break
    fi
    sleep 1
done

if [ $FIRMWARE_READY -eq 0 ]; then
    log "ERROR: Firmware not found!"
    exit 1
fi

# Load dependencies
log "Loading module dependencies..."
if ! lsmod | grep -q cfg80211; then
    log "  Loading dependency: cfg80211"
    modprobe cfg80211
fi

# Wait for PCI bus to stabilize
log "Waiting for PCI bus to stabilize..."
sleep 2

# Insert the module
log "Inserting ${MODULE_NAME}.ko..."
insmod "$MODULE_PATH"

# Wait for driver to initialize
log "Waiting for driver to initialize..."
sleep 5

# Verify firmware actually loaded
log "Verifying firmware load..."
FIRMWARE_LOADED=0
for i in {1..10}; do
    if dmesg | tail -100 | grep -q "kalRequestFirmware.*wifi.cfg OK"; then
        log "✓ Firmware loaded successfully"
        FIRMWARE_LOADED=1
        break
    fi
    sleep 1
done

if [ $FIRMWARE_LOADED -eq 0 ]; then
    log "ERROR: Firmware did not load! Retrying..."
    rmmod mt7902 2>/dev/null || true
    sleep 2
    log "Retry: Inserting ${MODULE_NAME}.ko..."
    insmod "$MODULE_PATH"
    sleep 5
    
    if ! dmesg | tail -100 | grep -q "kalRequestFirmware.*wifi.cfg OK"; then
        log "ERROR: Firmware load failed on retry!"
        exit 1
    fi
    log "✓ Firmware loaded on retry"
fi

# Check for WiFi interface
log "Checking for WiFi interface..."
if ip link show | grep -q "wlan"; then
    IFACE=$(ip link show | grep -oP 'wlan\d+' | head -1)
    log "✓ WiFi interface detected: $IFACE"
else
    log "WARNING: No WiFi interface detected yet"
fi

# Save dmesg
dmesg > "$LOG_FILE"
log "✓ Driver initialization complete"
log "Logs saved to: $LOG_FILE"
