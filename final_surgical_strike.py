path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    # Find the implementation of the recover function
    if "uint32_t mt79xx_pci_function_recover" in line and "{" in lines[i+1]:
        # Insert the check inside the brace
        lines.insert(i + 2, """    if (in_atomic() || in_interrupt()) {
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }\n""")
        break

with open(path, 'w') as f:
    f.writelines(lines)
