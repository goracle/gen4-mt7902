path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    lines = f.readlines()

# 1. Insert helpers after the last include (around line 90)
helpers = """
static int mt7902_mmio_dead(struct pci_dev *pdev) {
    u32 val;
    pci_read_config_dword(pdev, 0, &val);
    return (val == 0xffffffff);
}

void mt7902_schedule_recovery_from_atomic(struct GLUE_INFO *prGlueInfo) {
    struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
    if (test_and_set_bit(MTK_FLAG_MMIO_GONE, &prHifInfo->state_flags) == 0) {
        schedule_work(&prHifInfo->recovery_work);
    }
}
"""
for i, line in enumerate(lines):
    if '#include' in line:
        last_include = i
lines.insert(last_include + 1, helpers)

# 2. Insert the safety check at the start of the function body
# We look for the line AFTER line 1038 (the opening brace)
for i, line in enumerate(lines):
    if "uint32_t mt79xx_pci_function_recover" in line and "struct pci_dev *pdev" in line:
        if i > 500: # Ensure we are at the implementation, not the prototype
            # Find the opening brace '{'
            for j in range(i, i+10):
                if '{' in lines[j]:
                    lines.insert(j + 1, """    if (in_atomic() || in_interrupt()) {
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }\n""")
                    break
            break

with open(path, 'w') as f:
    f.writelines(lines)
