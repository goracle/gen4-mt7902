#!/bin/bash

MODULE_PATH="./mt7902.ko"

# Check if the module file exists
if [ ! -f "$MODULE_PATH" ]; then
    echo "Error: $MODULE_PATH not found. Run this from your build directory."
    exit 1
fi

echo "--- Scanning dependencies for $MODULE_PATH ---"

# Extract dependencies using modinfo
# The 'depends' line looks like: depends: cfg80211,mac80211,bluetooth
#DEPS=$(modinfo -F depends "$MODULE_PATH" | tr ',' ' ')
# To this (adding the absolute path helps modinfo):
DEPS=$(modinfo -F depends "$(realpath $MODULE_PATH)" | tr ',' ' ')

if [ -z "$DEPS" ]; then
    echo "No dependencies found."
else
    echo "Found dependencies: $DEPS"
    for dep in $DEPS; do
        echo "Loading dependency: $dep..."
        sudo modprobe "$dep" || { echo "Failed to load $dep"; exit 1; }
    done
fi

sudo rmmod -f mt7902
sudo lsmod

echo "--- Inserting mt7902.ko ---"
sudo insmod "$MODULE_PATH"
sudo dmesg > err.txt

if [ $? -eq 0 ]; then
    echo "Success! Module mt7902 inserted."
    # Check dmesg to confirm it initialized correctly
    sudo dmesg | tail -n 10
else
    echo "Failed to insert mt7902.ko."
    echo "Check 'dmesg | tail' for specific missing symbols."
fi
