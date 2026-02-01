#!/bin/bash
# Fix gl_init.c to use prGlueInfo->prDevHandler instead of extern static variables

DRIVER_PATH=~/builds/gen4-mt7902
TARGET_FILE="$DRIVER_PATH/os/linux/gl_init.c"

# Backup
cp "$TARGET_FILE" "$TARGET_FILE.backup_compilefix_$(date +%Y%m%d_%H%M%S)"

echo "Fixing gl_init.c..."

# Remove the old patch
sed -i '/MT7902 FIX #2B.*Restore carrier after reset/,/END FIX #2B/d' "$TARGET_FILE"

echo "Removed old patch"

# Find where u4ReadyFlag = 1 is set
LINE_READY=$(grep -n "prGlueInfo->u4ReadyFlag = 1;" "$TARGET_FILE" | head -1 | cut -d: -f1)

# Add the working version after the update_driver_loaded_status call (3 lines after)
LINE_INSERT=$((LINE_READY + 3))

sed -i "${LINE_INSERT} a\\
\\
	/* ===== MT7902 FIX #2B: Restore carrier after reset ===== */\\
	if (kalIsResetting() && prGlueInfo->prDevHandler) {\\
		DBGLOG(INIT, WARN,\\
		       \"MT7902-FIX: Carrier ON after reset recovery\\\\n\");\\
		netif_carrier_on(prGlueInfo->prDevHandler);\\
		netif_tx_wake_all_queues(prGlueInfo->prDevHandler);\\
	}\\
	/* ===== END FIX #2B ===== */\\
" "$TARGET_FILE"

echo "Applied working fix at line $LINE_INSERT"
echo ""
echo "Verify with:"
echo "  sed -n '5040,5055p' $TARGET_FILE"
