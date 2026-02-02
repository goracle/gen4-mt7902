#!/bin/bash

# Get the root of the git repo or current dir
ROOT_DIR=$(git rev-parse --show-toplevel 2>/dev/null || pwd)

echo "Starting UBSAN Refactor from: $ROOT_DIR"

# Define the structural patterns to fix
# Pattern 1: Any struct member ending in [0]; or [1];
# Pattern 2: Specific variable name found in rlm_domain logic
patterns=(
    's/\[0\];/\[\];/g'
    's/\[1\];/\[\];/g'
    's/rChannelLegacyPowerLimit\[0\]/rChannelLegacyPowerLimit\[\]/g'
)

# Target specific files by name to avoid touching hardware bitmask comments
targets=(
    "wsys_cmd_handler_fw.h"
    "wlan_oid.h"
    "he_ie.h"
    "fw_dl.h"
    "rlm_domain.h"
    "rlm_domain.c"
)

for target in "${targets[@]}"; do
    # Find the file path dynamically
    FILE_PATH=$(find "$ROOT_DIR" -name "$target" -print -quit)
    
    if [ -n "$FILE_PATH" ]; then
        echo "Updating $FILE_PATH..."
        for pattern in "${patterns[@]}"; do
            sed -i "$pattern" "$FILE_PATH"
        done
    else
        echo "Could not find $target in $ROOT_DIR"
    fi
done

echo "Refactor complete."
