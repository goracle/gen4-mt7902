#!/bin/bash
# apply-auth-fix.sh - Apply MT7902 auth frame flush fix patches

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCHES=(
    "01-add-cfg80211-flag.patch"
    "02-guard-auto-reconnect.patch"
    "03-set-flag-in-cfg80211-auth.patch"
    "04-clear-flag-on-join-completion.patch"
)

echo "======================================================================"
echo "MT7902 Auth Frame Flush Fix - Patch Application"
echo "======================================================================"
echo ""

# Check if we're in the driver source directory
if [[ ! -f "Makefile" ]] || [[ ! -d "mgmt" ]]; then
    echo "❌ Error: This script must be run from the driver source root directory"
    echo "   (the directory containing Makefile and mgmt/)"
    echo ""
    echo "Usage:"
    echo "  cd /path/to/gen4-mt7902"
    echo "  bash /path/to/apply-auth-fix.sh"
    exit 1
fi

echo "✓ Found driver source tree"
echo ""

# Check if patches exist
for patch in "${PATCHES[@]}"; do
    if [[ ! -f "$SCRIPT_DIR/$patch" ]]; then
        echo "❌ Error: Patch file not found: $patch"
        exit 1
    fi
done

echo "✓ All patch files found"
echo ""

# Create backup
BACKUP_DIR="backup-before-auth-fix-$(date +%Y%m%d-%H%M%S)"
echo "Creating backup in $BACKUP_DIR ..."
mkdir -p "$BACKUP_DIR"
cp -r include mgmt os "$BACKUP_DIR/"
echo "✓ Backup created"
echo ""

# Apply patches
echo "Applying patches..."
echo ""

APPLIED=0
for patch in "${PATCHES[@]}"; do
    echo "Applying $patch ..."
    if patch -p1 < "$SCRIPT_DIR/$patch"; then
        echo "  ✓ Success"
        ((APPLIED++))
    else
        echo "  ❌ Failed"
        echo ""
        echo "Patch application failed. Restoring from backup..."
        rm -rf include mgmt os
        cp -r "$BACKUP_DIR"/* .
        rmdir "$BACKUP_DIR"
        echo "Backup restored."
        exit 1
    fi
    echo ""
done

echo "======================================================================"
echo "✓ All patches applied successfully ($APPLIED/${#PATCHES[@]})"
echo "======================================================================"
echo ""
echo "Next steps:"
echo "  1. Rebuild the module:"
echo "       make clean"
echo "       make -j\$(nproc)"
echo ""
echo "  2. Reload the module:"
echo "       sudo rmmod mt7902"
echo "       sudo ./load.sh"
echo ""
echo "  3. Test connection:"
echo "       iwctl station wlan0 connect \"YourSSID\""
echo ""
echo "  4. Monitor logs:"
echo "       sudo dmesg -w | grep -E 'CFG80211|cfg80211|TX_DONE|FLUSHED'"
echo ""
echo "Look for these success indicators:"
echo "  ✓ '[CFG80211] Marked cfg80211-connecting for BSS 0'"
echo "  ✓ 'cfg80211 owns connection (skip driver auto-reconnect)'"
echo "  ✓ No 'TX_DONE Status=FLUSHED' errors"
echo ""
echo "Backup saved in: $BACKUP_DIR"
echo "======================================================================"
