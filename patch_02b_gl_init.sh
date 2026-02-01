#!/bin/bash
# Auto-patch script for os/linux/gl_init.c
# Adds netif_carrier_on when driver becomes ready after reset

DRIVER_PATH=~/builds/gen4-mt7902
TARGET_FILE="$DRIVER_PATH/os/linux/gl_init.c"

# Backup first
cp "$TARGET_FILE" "$TARGET_FILE.backup_$(date +%Y%m%d_%H%M%S)"

echo "Patching $TARGET_FILE..."

# Find the line where u4ReadyFlag is set to 1 (around line 5040)
LINE_READY=$(grep -n "prGlueInfo->u4ReadyFlag = 1;" "$TARGET_FILE" | head -1 | cut -d: -f1)

echo "Found u4ReadyFlag = 1 at line: $LINE_READY"

if [ ! -z "$LINE_READY" ]; then
    # We need to add the code after a few lines, after update_driver_loaded_status
    # Let's find the line after that
    LINE_INSERT=$((LINE_READY + 3))
    
    sed -i "${LINE_INSERT} a\\
\\
	/* ===== MT7902 FIX #2B: Restore carrier after reset ===== */\\
	if (kalIsResetting()) {\\
		struct net_device *prDev = NULL;\\
		extern uint32_t u4WlanDevNum;\\
		extern struct WLANDEV_INFO arWlanDevInfo[];\\
		if (u4WlanDevNum > 0 && u4WlanDevNum <= CFG_MAX_WLAN_DEVICES)\\
			prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;\\
		if (prDev && netif_running(prDev)) {\\
			DBGLOG(INIT, WARN,\\
			       \"MT7902-FIX: Carrier ON after reset recovery\\\\n\");\\
			netif_carrier_on(prDev);\\
			netif_tx_wake_all_queues(prDev);\\
		}\\
	}\\
	/* ===== END FIX #2B ===== */\\
" "$TARGET_FILE"
    echo "Patched gl_init.c"
else
    echo "WARNING: Could not find u4ReadyFlag = 1 insertion point"
fi

echo ""
echo "Patch applied to $TARGET_FILE"
echo "Backup saved"
echo ""
echo "Verify with:"
echo "  grep -A8 'MT7902-FIX.*Carrier ON' $TARGET_FILE"
