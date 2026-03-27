#!/usr/bin/env python3
import subprocess, re, sys, os

def get_lspci_output(slot):
    """Runs lspci -vvv for a specific slot."""
    try:
        # Fixed: changed capture_code to capture_output
        result = subprocess.run(['lspci', '-vvv', '-s', slot],
                                capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError:
        return None

def analyze_latchedness(device_output, bridge_output=None):
    """
    Analyzes lspci output for both the MT7902 and its parent Bridge.
    """
    status = {"is_latched": False, "reasons": []}

    # --- 1. BRIDGE CONTROL CHECKS (The 'Master Kill Switch') ---
    if bridge_output:
        # Check for Secondary Bus Reset (SBR) bit 0x40 in BridgeControl
        if "BusRst+" in bridge_output or "SecondaryBusReset+" in bridge_output:
            status["is_latched"] = True
            status["reasons"].append("CRITICAL: Parent Bridge (00:1c.6) is asserting Bus Reset!")

    # --- 2. HARD LATCH CHECKS (Hardware Ghosting) ---
    if "ff:ff:ff:ff" in device_output.lower():
        status["is_latched"] = True
        status["reasons"].append("Bus returning 0xFFFFFFFF (Hardware is completely non-responsive)")

    if re.search(r"LnkCtl:.*LnkDisable\+", device_output):
        status["is_latched"] = True
        status["reasons"].append("Link is explicitly disabled (LnkDisable+)")

    # --- 3. MT7902 SPECIFIC 'SOFT-LATCH' INDICATORS ---

    # Check if driver is actually bound (prevents false positives on Master-)
    is_driver_loaded = "Kernel driver in use: mtk_pci_driver" in device_output

    if "BusMaster-" in device_output:
        if is_driver_loaded:
            status["is_latched"] = True
            status["reasons"].append("Bus Mastering dropped (Master-) while driver is loaded! (Hardware Crash)")
        else:
            # Not necessarily a latch, but a warning that DMA is impossible
            status["reasons"].append("Bus Mastering is currently disabled (Standard for unbound device)")

    if is_driver_loaded and "MSI: Enable-" in device_output:
        status["reasons"].append("MSI Interrupts failed to initialize (Handshake failed)")

    if "[disabled]" in device_output or "Memory at <ignored>" in device_output:
        status["is_latched"] = True
        status["reasons"].append("PCI Memory BARs are disabled or ignored by the kernel")

    if "Status: D3" in device_output:
        status["is_latched"] = True
        status["reasons"].append("Device stuck in D3 (Low Power) state - likely missed wake-up signal")

    return status

def main():
    device_slot = "02:00.0"
    bridge_slot = "00:1c.6"

    print(f"--- Analyzing MediaTek MT7902 Chain ---")

    device_data = get_lspci_output(device_slot)
    bridge_data = get_lspci_output(bridge_slot) # New call

    if not device_data:
        print(f"Error: Could not find device at {device_slot}.")
        sys.exit(1)

    analysis = analyze_latchedness(device_data, bridge_data)

    if analysis["is_latched"]:
        print("\033[91m[!] LATCHED STATE DETECTED\033[0m")
        for reason in analysis["reasons"]:
            print(f"  - {reason}")
        sys.exit(1)
    else:
        print("\033[92m[+] DEVICE APPEARS ACTIVE\033[0m")
        # Print a warning if Mastering is off but link is otherwise fine
        for reason in analysis["reasons"]:
             print(f"  (Note: {reason})")
        sys.exit(0)

if __name__ == "__main__":
    main()
