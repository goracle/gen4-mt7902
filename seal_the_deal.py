import re

path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    content = f.read()

# 1. Clean up the main recovery function body
# We look for our clean header and ensure the very next lines are the safety check
recovery_header = r'uint32_t mt79xx_pci_function_recover\(struct pci_dev \*pdev, struct GLUE_INFO \*prGlueInfo\)\n\{'
safety_check = """uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev, struct GLUE_INFO *prGlueInfo)
{
    if (in_atomic() || in_interrupt()) {
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }
"""
# Use a substitution that doesn't duplicate if already present
if 'in_atomic()' not in content.split('mt79xx_pci_function_recover')[1][:200]:
    content = re.sub(recovery_header, safety_check, content)

# 2. Final check on the ISR: Ensure it uses the correct glue pointer
# Your earlier snippet showed halDisableInterrupt(prAdapter), but it needs prGlueInfo->prAdapter
content = content.replace('halDisableInterrupt(prAdapter);', 'halDisableInterrupt(prGlueInfo->prAdapter);')

with open(path, 'w') as f:
    f.write(content)
