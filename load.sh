#!/bin/bash

# 🏎️ MT7902 WiFi Driver Loading Script - Stabilization Overdrive Edition 🏎️
set -e

MODULE_NAME="mt7902"
# Use absolute path to ensure sudo doesn't lose the location
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
MODULE_PATH="$SCRIPT_DIR/${MODULE_NAME}.ko"

log() {
    echo -e "[\e[34m${MODULE_NAME}\e[0m] $1"
}

# 1. Clean up existing state
log "🧹 Cleaning up old module instances..."
sudo rmmod $MODULE_NAME 2>/dev/null || true

# 2. Check dependencies
log "🔗 Ensuring kernel dependencies are loaded..."
for dep in cfg80211 mac80211 mt76-connac-lib; do
    if ! lsmod | grep -q "$dep"; then
        log "  🛠️  Loading $dep..."
        sudo modprobe "$dep" || log "  ⚠️  Warning: could not modprobe $dep"
    fi
done

# 3. PCI Check
if ! lspci -d 14c3:7902 >/dev/null 2>&1; then
    log "\e[31m❌ ERROR: MT7902 PCI device not visible. Hardware is ghosting us. 👻\e[0m"
    exit 1
fi

# 4. Insert Module
log "🔌 Inserting module from $MODULE_PATH..."
# Clear dmesg so we only see current attempt logs
sudo dmesg -C 
sudo insmod "$MODULE_PATH"

# 5. Validation and PHY Kick
log "⏳ Waiting for hardware initialization..."
MAX_RETRIES=15
INTERFACE=""

for i in $(seq 1 $MAX_RETRIES); do
    # Find the interface name (usually wlan0 or similar)
    INTERFACE=$(ip -o link show | awk -F': ' '/mt7902/ || /wlan/ {print $2}' | head -n 1)
    
    if [ -n "$INTERFACE" ]; then
        log "\e[32m✨ WiFi interface $INTERFACE appeared! ✨\e[0m"
        
        # Kick the PHY state machine to prevent "Channel 0" hangs
        log "🥊 Priming PHY state machine..."
        sudo iw reg set US
        sleep 1
        
        log "🔄 Cycling interface state..."
        sudo ip link set "$INTERFACE" up || log "  🌬️  Warning: Initial 'up' failed (expected if PHY is dusty)"
        sleep 2
        sudo ip link set "$INTERFACE" down
        
        log "\e[32m🚀 PHY initialized and ready for NetworkManager 🚀\e[0m"
        break
    fi

    if [ $i -eq $MAX_RETRIES ]; then
        log "\e[31m💥 ERROR: Interface did not appear after $MAX_RETRIES seconds. 💥\e[0m"
        log "🔎 Check dmesg for 'kalRequestFirmware' errors."
        exit 1
    fi
    sleep 1
done

# 6. Final Regdom check
log "🇺🇲 Setting final regulatory domain..."
sudo iw reg set US
log "🏁 Load sequence complete. We are in business. 👔"
