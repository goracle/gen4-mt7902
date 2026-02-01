#!/bin/bash
# Master script to apply all MT7902 Fix #2 patches
# Fixes the reset recovery zombie loop

set -e

DRIVER_PATH=~/builds/gen4-mt7902
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "========================================"
echo "MT7902 Fix #2: Reset Carrier Management"
echo "========================================"
echo ""

# Check if we're in the right place
if [ ! -d "$DRIVER_PATH" ]; then
    echo "ERROR: Driver directory not found: $DRIVER_PATH"
    exit 1
fi

cd "$DRIVER_PATH"

echo "Step 1/3: Patching os/linux/gl_rst.c (carrier off at reset start)..."
bash "$SCRIPT_DIR/patch_02a_gl_rst.sh"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to patch gl_rst.c"
    exit 1
fi
echo ""

echo "Step 2/3: Patching os/linux/gl_init.c (carrier on at reset complete)..."
bash "$SCRIPT_DIR/patch_02b_gl_init.sh"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to patch gl_init.c"
    exit 1
fi
echo ""

echo "Step 3/3: Patching common/wlan_lib.c (rate-limit log spam)..."
bash "$SCRIPT_DIR/patch_02c_wlan_lib.sh"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to patch wlan_lib.c"
    exit 1
fi
echo ""

echo "========================================"
echo "All patches applied successfully!"
echo "========================================"
echo ""
echo "Summary of changes:"
echo "  ✓ gl_rst.c:   netif_carrier_off() when reset starts"
echo "  ✓ gl_init.c:  netif_carrier_on() when reset completes"
echo "  ✓ wlan_lib.c: rate-limited driver state logs"
echo ""
echo "Backups saved with timestamp suffix"
echo ""
echo "Next steps:"
echo "  1. Review changes:"
echo "     grep -n 'MT7902-FIX' os/linux/gl_rst.c os/linux/gl_init.c common/wlan_lib.c"
echo ""
echo "  2. Rebuild driver:"
echo "     make clean && make"
echo ""
echo "  3. Install and test:"
echo "     sudo rmmod mt7902e"
echo "     sudo insmod mt7902e.ko"
echo ""
