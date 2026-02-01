#!/bin/bash

MODULE="./mt7902.ko"
LOGFILE="mt7902_dmesg.log"

# --- Check module exists ---
if [ ! -f "$MODULE" ]; then
    echo "Error: $MODULE not found. Run this from your build directory."
    exit 1
fi

MODULE_PATH=$(realpath "$MODULE")
echo "--- Using module: $MODULE_PATH ---"

# --- Scan and load dependencies ---
DEPS=$(modinfo -F depends "$MODULE_PATH" | tr ',' ' ')
if [ -z "$DEPS" ]; then
    echo "No dependencies found."
else
    echo "Found dependencies: $DEPS"
    for dep in $DEPS; do
        if ! lsmod | grep -q "^$dep"; then
            echo "Loading dependency: $dep..."
            sudo modprobe "$dep" || { echo "Failed to load $dep"; exit 1; }
        else
            echo "$dep already loaded."
        fi
    done
fi

# --- Unload existing module if present ---
if lsmod | grep -q "^mt7902"; then
    echo "Unloading existing mt7902 module..."
    sudo rmmod -f mt7902
fi

# --- Clear old dmesg logs ---
sudo dmesg -C

# --- Insert module ---
echo "--- Inserting mt7902.ko ---"
if sudo insmod "$MODULE_PATH"; then
    echo "Module inserted successfully!"
else
    echo "Failed to insert module. Exiting..."
    exit 1
fi

# --- Capture initialization logs ---
echo "--- Capturing dmesg logs ---"
sudo dmesg | tee "$LOGFILE" | grep --color=always -i mt7902

echo "--- Done. Logs saved to $LOGFILE ---"
