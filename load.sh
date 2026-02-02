#!/bin/bash
set -euo pipefail

if [ "$EUID" -ne 0 ]; then
    echo "Run this script as root (sudo -i or su -)"
    exit 1
fi

MODULE="./mt7902.ko"
LOGFILE="mt7902_dmesg.log"

echo "--- Using module: $(realpath "$MODULE") ---"

# Load dependencies
DEPS=$(modinfo -F depends "$MODULE" | tr ',' ' ')
for dep in $DEPS; do
    modprobe "$dep" || true
done

# Unload old module
if lsmod | grep -q "^mt7902"; then
    rmmod -f mt7902 || true
fi

# Clear logs
dmesg -C

# Insert module
insmod "$MODULE"

# Capture logs (NO pipes to sudo)
dmesg | grep -i mt7902 > "$LOGFILE"

echo "--- Done. Logs saved to $LOGFILE ---"
