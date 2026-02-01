#!/bin/bash

# Check if pcie.c exists in the expected location
PCIE_FILE="os/linux/hif/pcie/pcie.c"

if [ ! -f "$PCIE_FILE" ]; then
    echo "ERROR: $PCIE_FILE not found"
    echo "Current directory: $(pwd)"
    echo "Contents:"
    ls -la
    exit 1
fi

echo "=== Showing lines around 304 in $PCIE_FILE ==="
sed -n '294,320p' "$PCIE_FILE" | nl -v 294

echo ""
echo "=== Looking for the mtk_pci_probe function ==="
grep -n "int mtk_pci_probe" "$PCIE_FILE"

echo ""
echo "=== Checking what patch expects (first few removed lines) ==="
grep "^-" /mnt/user-data/uploads/pcie.patch | head -10
