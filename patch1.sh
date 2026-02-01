#!/bin/bash
# Manual patch for glResetTriggerMobile in os/linux/gl_rst.c
# This adds the carrier_off code that the automatic script missed

DRIVER_PATH=~/builds/gen4-mt7902
TARGET_FILE="$DRIVER_PATH/os/linux/gl_rst.c"

# Backup first
cp "$TARGET_FILE" "$TARGET_FILE.backup_mobile_$(date +%Y%m%d_%H%M%S)"

echo "Patching glResetTriggerMobile in $TARGET_FILE..."

# Insert after line 399 (fgIsResetting = TRUE;)
sed -i '399 a\
\
	/* ===== MT7902 FIX #2A: Quarantine netdev during reset (Mobile) ===== */\
	{\
		struct net_device *prDev = NULL;\
		if (prAdapter && prAdapter->prGlueInfo) {\
			extern uint32_t u4WlanDevNum;\
			extern struct WLANDEV_INFO arWlanDevInfo[];\
			if (u4WlanDevNum > 0 && u4WlanDevNum <= CFG_MAX_WLAN_DEVICES)\
				prDev = arWlanDevInfo[u4WlanDevNum - 1].prDev;\
			if (prDev && netif_running(prDev)) {\
				DBGLOG(INIT, WARN,\
				       "MT7902-FIX: Carrier OFF during reset (Mobile)\\n");\
				netif_carrier_off(prDev);\
				netif_tx_stop_all_queues(prDev);\
			}\
		}\
	}\
	/* ===== END FIX #2A ===== */\
' "$TARGET_FILE"

echo "Patch applied!"
echo ""
echo "Verify with:"
echo "  sed -n '393,425p' $TARGET_FILE"
echo ""
echo "You should now see the carrier_off code after fgIsResetting = TRUE"
