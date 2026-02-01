#!/bin/bash
# Auto-patch script for common/wlan_lib.c
# Rate-limits the "driver state[0]" spam

DRIVER_PATH=~/builds/gen4-mt7902
TARGET_FILE="$DRIVER_PATH/common/wlan_lib.c"

# Backup first
cp "$TARGET_FILE" "$TARGET_FILE.backup_$(date +%Y%m%d_%H%M%S)"

echo "Patching $TARGET_FILE..."

# Find and replace the WARN with LOUD (rate-limited)
# Line should be around 649: DBGLOG(REQ, WARN, "driver state[%d]\n", u1State);

sed -i 's/DBGLOG(REQ, WARN, "driver state\[%d\]\\n", u1State);/DBGLOG_LIMITED(REQ, LOUD, "driver state[%d]\\n", u1State); \/\* MT7902-FIX: Rate-limited \*\//' "$TARGET_FILE"

if [ $? -eq 0 ]; then
    echo "Patched wlan_lib.c - rate-limited driver state log"
else
    echo "WARNING: Could not patch wlan_lib.c"
fi

echo ""
echo "Patch applied to $TARGET_FILE"
echo "Backup saved"
echo ""
echo "Verify with:"
echo "  grep 'driver state' $TARGET_FILE | grep LOUD"
