#!/bin/bash
# Auto-patch script for nic/que_mgt.c
# Adds the critical fix to block absence during scans

DRIVER_PATH=~/builds/gen4-mt7902
TARGET_FILE="$DRIVER_PATH/nic/que_mgt.c"

# Backup first
cp "$TARGET_FILE" "$TARGET_FILE.backup_$(date +%Y%m%d_%H%M%S)"

# The fix: Insert after line 7293 (after "fgIsNetAbsentOld = prBssInfo->fgIsNetAbsent;")
sed -i '7293 a\
\
	/* ========== MT7902 CRITICAL FIX: Block absence during scan ========== */\
	/*\
	 * Problem: Firmware asserts BSS absence during online scans while TX active,\
	 * causing TX ring overflow and crash (RST_SER_L1_FAIL).\
	 *\
	 * Fix: If connected AND scanning, ignore firmware absence request.\
	 */\
	if (prEventBssStatus->ucIsAbsent &&\
	    prBssInfo->eConnectionState == MEDIA_STATE_CONNECTED &&\
	    prAdapter->rWifiVar.rScanInfo.eCurrentState != SCAN_STATE_IDLE) {\
		\
		DBGLOG(QM, WARN,\
		       "MT7902-FIX: Blocking absence during scan! BSS=%u ScanState=%d\\n",\
		       prEventBssStatus->ucBssIndex,\
		       prAdapter->rWifiVar.rScanInfo.eCurrentState);\
		\
		/* Keep network present */\
		prBssInfo->fgIsNetAbsent = FALSE;\
		prBssInfo->ucBssFreeQuota = prEventBssStatus->ucBssFreeQuota;\
		\
		DBGLOG(QM, INFO, "NAF=%d,%d,%d (BLOCKED)\\n",\
		       prEventBssStatus->ucBssIndex,\
		       prEventBssStatus->ucIsAbsent,\
		       prEventBssStatus->ucBssFreeQuota);\
		\
		return;\
	}\
	/* ========== END CRITICAL FIX ========== */\
' "$TARGET_FILE"

echo "Patch applied to $TARGET_FILE"
echo "Backup saved as: $TARGET_FILE.backup_*"
echo ""
echo "Verify with:"
echo "  sed -n '7290,7330p' $TARGET_FILE"
