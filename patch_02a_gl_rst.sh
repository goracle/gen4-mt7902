#!/bin/bash
# Auto-patch script for os/linux/gl_rst.c
# Adds netif_carrier_off when reset starts

DRIVER_PATH=~/builds/gen4-mt7902
TARGET_FILE="$DRIVER_PATH/os/linux/gl_rst.c"

# Backup first
cp "$TARGET_FILE" "$TARGET_FILE.backup_$(date +%Y%m%d_%H%M%S)"

echo "Patching $TARGET_FILE..."

# First, we need to find the exact line numbers for the two functions
# Let's find glResetTriggerMobile first

LINE_MOBILE=$(grep -n "fgIsResetting = TRUE;" "$TARGET_FILE" | grep -A2 -B2 "glResetTriggerMobile" | head -1 | cut -d: -f1)
if [ -z "$LINE_MOBILE" ]; then
    # Try alternative: just find first occurrence after function start
    LINE_MOBILE=$(awk '/void glResetTriggerMobile/,/fgIsResetting = TRUE;/ {if (/fgIsResetting = TRUE;/) print NR; exit}' "$TARGET_FILE")
fi

echo "Found glResetTriggerMobile fgIsResetting at line: $LINE_MOBILE"

# Patch glResetTriggerMobile - add after "fgIsResetting = TRUE;"
if [ ! -z "$LINE_MOBILE" ]; then
    sed -i "${LINE_MOBILE} a\\
\\
	/* ===== MT7902 FIX #2A: Quarantine netdev during reset ===== */\\
	{\\
		struct net_device *prDev = NULL;\\
		if (prAdapter && prAdapter->prGlueInfo) {\\
			extern uint32_t u4WlanDevNum;\\
			extern struct WLANDEV_INFO arWlanDevInfo[];\\
			if (u4WlanDevNum > 0 && u4WlanDevNum <= CFG_MAX_WLAN_DEVICES)\\
				prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;\\
			if (prDev && netif_running(prDev)) {\\
				DBGLOG(INIT, WARN,\\
				       \"MT7902-FIX: Carrier OFF during reset\\\\n\");\\
				netif_carrier_off(prDev);\\
				netif_tx_stop_all_queues(prDev);\\
			}\\
		}\\
	}\\
	/* ===== END FIX #2A ===== */\\
" "$TARGET_FILE"
    echo "Patched glResetTriggerMobile"
else
    echo "WARNING: Could not find glResetTriggerMobile insertion point"
fi

# Now patch glResetTriggerCe
LINE_CE=$(awk '/void glResetTriggerCe/,/fgIsResetting = TRUE;/ {if (/fgIsResetting = TRUE;/) {print NR; exit}}' "$TARGET_FILE")

echo "Found glResetTriggerCe fgIsResetting at line: $LINE_CE"

if [ ! -z "$LINE_CE" ]; then
    sed -i "${LINE_CE} a\\
\\
	/* ===== MT7902 FIX #2A: Quarantine netdev during reset (CE) ===== */\\
	{\\
		struct net_device *prDev = NULL;\\
		if (prAdapter && prAdapter->prGlueInfo) {\\
			extern uint32_t u4WlanDevNum;\\
			extern struct WLANDEV_INFO arWlanDevInfo[];\\
			if (u4WlanDevNum > 0 && u4WlanDevNum <= CFG_MAX_WLAN_DEVICES)\\
				prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;\\
			if (prDev && netif_running(prDev)) {\\
				DBGLOG(INIT, WARN,\\
				       \"MT7902-FIX: Carrier OFF during reset (CE)\\\\n\");\\
				netif_carrier_off(prDev);\\
				netif_tx_stop_all_queues(prDev);\\
			}\\
		}\\
	}\\
	/* ===== END FIX #2A ===== */\\
" "$TARGET_FILE"
    echo "Patched glResetTriggerCe"
else
    echo "WARNING: Could not find glResetTriggerCe insertion point"
fi

echo ""
echo "Patch applied to $TARGET_FILE"
echo "Backup saved"
echo ""
echo "Verify with:"
echo "  grep -A10 'MT7902-FIX.*Carrier OFF' $TARGET_FILE"
