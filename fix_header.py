import re

file_path = 'os/linux/hif/pcie/pcie.c'
with open(file_path, 'r') as f:
    lines = f.readlines()

# Reconstruct the file, stripping out any lines between the function start and the first real code
new_lines = []
skip_until_brace = False

for line in lines:
    if 'uint32_t mt79xx_pci_function_recover' in line and '(' in line:
        new_lines.append('uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev,\n')
        new_lines.append('                                     struct GLUE_INFO *prGlueInfo)\n')
        new_lines.append('{\n')
        new_lines.append('    if (in_atomic() || in_interrupt()) {\n')
        new_lines.append('        mt7902_schedule_recovery_from_atomic(prGlueInfo);\n')
        new_lines.append('        return WLAN_STATUS_PENDING;\n')
        new_lines.append('    }\n')
        skip_until_brace = True
        continue
    
    if skip_until_brace:
        if '{' in line:
            skip_until_brace = False
        continue
        
    new_lines.append(line)

with open(file_path, 'w') as f:
    f.writelines(new_lines)
