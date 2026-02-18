#!/bin/bash
# 🔧 MT7902 WiFi Driver Loader 🔧
# Minimal script - no interface manipulation to avoid kernel panics

set -e

sudo iw reg set US

MODULE_NAME="mt7902"
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODULE_PATH="$SCRIPT_DIR/${MODULE_NAME}.ko"

log() {
    echo "[$MODULE_NAME] $1"
}

# 1. Remove old module if loaded
log "🗑️  Removing old module..."
sudo rmmod $MODULE_NAME 2>/dev/null || true

sleep 2

# 2. Verify hardware is visible on PCIe bus
#if ! lspci -d 14c3:7902 >/dev/null 2>&1; then
#    log "❌ ERROR: MT7902 not found on PCIe bus 👻"
#    log "⚡ Need full power cycle (hold power button 40s)"
#    exit 1
#fi

# 3. Clear dmesg and load module
log "🚀 Loading module from $MODULE_PATH..."
sudo dmesg -C
sudo insmod "$MODULE_PATH"

# 4. Wait for interface to appear
log "⏳ Waiting for interface..."
sleep 2

INTERFACE=$(ip -o link show | awk -F': ' '/wlan/ {print $2}' | head -n 1)
if [ -n "$INTERFACE" ]; then
    log "✨ Interface $INTERFACE created successfully! ✨"
else
    log "⚠️  WARNING: No wlan interface detected"
    log "🔍 Check dmesg for firmware loading errors"
fi

log "🎉 Module loaded! Use 'dmesg' to check status."
