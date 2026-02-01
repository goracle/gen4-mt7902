#!/usr/bin/env bash
set -euo pipefail
LOCAL_KO="./mt7902.ko"

echo "[*] Cleaning environment..."
# 1. Kill any existing instances of our debug or target modules
sudo rmmod debug 2>/dev/null || true
sudo rmmod mt7902 2>/dev/null || true

# 2. Unload the kernel's built-in MediaTek drivers that might be squatting
# These are the ones that usually cause "Device or resource busy"
sudo modprobe -r mt7921e 2>/dev/null || true
sudo modprobe -r mt792x_lib 2>/dev/null || true
sudo modprobe -r mt76_connac_lib 2>/dev/null || true
sudo modprobe -r mt76 2>/dev/null || true

# 3. Load base wireless stack dependencies
echo "[*] Loading dependencies..."
sudo modprobe cfg80211
sudo modprobe mac80211

# Clear logs so we can see the exact moment of insertion
sudo dmesg -C

echo "[*] Inserting local mt7902.ko..."
# Using insmod here is correct for a local file
if sudo insmod "$LOCAL_KO"; then
    echo "[+] Insertion successful!"
else
    echo "[-] Insertion failed with error $?. Check dmesg for resource conflicts."
fi

echo "[*] Checking dmesg..."
sudo dmesg
