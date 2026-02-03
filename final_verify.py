import re

path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    text = f.read()

# 1. Ensure the Recovery Function (mt79xx_pci_function_recover) is clean
# We search for the start of the function and ensure the context check is the first thing inside
rec_pattern = r'uint32_t mt79xx_pci_function_recover\(struct pci_dev \*pdev,\n\s+struct GLUE_INFO \*prGlueInfo\)\n\{'
rec_replacement = """uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev,
					 struct GLUE_INFO *prGlueInfo)
{
    if (in_atomic() || in_interrupt()) {
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }
"""
text = re.sub(rec_pattern, rec_replacement, text)

# 2. Scrub any potential leftover broken DBGLOG lines from previous failed seds
# This looks for any DBGLOG that starts a quote but doesn't end it on the same line
text = re.sub(r'DBGLOG\(HAL, WARN, "Scheduling recovery workqueue\.\.\.\s*\n', 'DBGLOG(HAL, WARN, "Scheduling recovery workqueue...\\n");\n', text)

with open(path, 'w') as f:
    f.write(text)
