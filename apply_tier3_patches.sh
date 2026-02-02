#!/bin/bash
# Tier-3 PCIe Recovery - Patch Application Script

set -e  # Exit on error

DRIVER_DIR="$HOME/builds/gen4-mt7902"
PATCH_DIR="$HOME/builds/gen4-mt7902/files"

echo "=========================================="
echo "Tier-3 PCIe Recovery Patch Application"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "$DRIVER_DIR/Makefile" ]; then
    echo "ERROR: Cannot find driver at $DRIVER_DIR"
    echo "Please edit this script and set DRIVER_DIR correctly"
    exit 1
fi

cd "$DRIVER_DIR"
echo "Working directory: $(pwd)"
echo ""

# Backup current state
BACKUP_DIR="$HOME/driver_backup_$(date +%Y%m%d_%H%M%S)"
echo "Creating backup at: $BACKUP_DIR"
cp -r "$DRIVER_DIR" "$BACKUP_DIR"
echo "✓ Backup complete"
echo ""

# Apply patches in order
PATCHES=(
    "tier3_part1_detection.patch"
    "tier3_part2_typedef.patch"
    "tier3_part3_recovery_function.patch"
    "tier3_part4_hook_in_drvown.patch"
)

echo "Applying patches..."
echo ""

for patch in "${PATCHES[@]}"; do
    echo "→ Applying $patch..."
    if patch -p1 --dry-run < "$PATCH_DIR/$patch" > /dev/null 2>&1; then
        patch -p1 < "$PATCH_DIR/$patch"
        echo "  ✓ Success"
    else
        echo "  ✗ FAILED (dry-run failed)"
        echo ""
        echo "ERROR: Patch $patch cannot be applied cleanly"
        echo "This might mean:"
        echo "  1. Patches already applied"
        echo "  2. Code has changed since patches were created"
        echo "  3. Wrong directory"
        echo ""
        echo "Your backup is at: $BACKUP_DIR"
        exit 1
    fi
done

echo ""
echo "=========================================="
echo "All patches applied successfully!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Review changes: git diff (if using git)"
echo "  2. Build: make clean && make -j\$(nproc)"
echo "  3. Install: sudo make install"
echo "  4. Reload: sudo rmmod mt7902e && sudo modprobe mt7902e"
echo ""
echo "Backup location: $BACKUP_DIR"
echo ""
echo "To test recovery after installing:"
echo "  Watch dmesg: sudo dmesg -w | grep -E 'Tier-3|MMIO|recovery'"
echo "  Then let system sleep overnight"
echo ""
