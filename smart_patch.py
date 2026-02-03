path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    content = f.read()

# 1. Check if the recovery helper already exists
if 'void mt7902_schedule_recovery_from_atomic' not in content:
    # Insert at the top after includes
    insertion_point = content.find('#include "mt66xx_reg.h"')
    if insertion_point != -1:
        end_of_line = content.find('\n', insertion_point) + 1
        helpers = """
void mt7902_schedule_recovery_from_atomic(struct GLUE_INFO *prGlueInfo) {
    struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
    if (test_and_set_bit(MTK_FLAG_MMIO_GONE, &prHifInfo->state_flags) == 0) {
        schedule_work(&prHifInfo->recovery_work);
    }
}
"""
        content = content[:end_of_line] + helpers + content[end_of_line:]

# 2. Add the safety check to the implementation body
# We look for the line where the function is defined
# and insert the check right after the first '{' after that line
func_def = content.find('uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev,')
if func_def != -1:
    # Find implementation (not prototype) by checking for a '{' nearby
    # Prototypes end in ';', implementations have a '{'
    brace_pos = content.find('{', func_def)
    # Check if the safety check is already there
    if 'in_atomic()' not in content[brace_pos:brace_pos+100]:
        safety = """
    if (in_atomic() || in_interrupt()) {
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }
"""
        content = content[:brace_pos+1] + safety + content[brace_pos+1:]

with open(path, 'w') as f:
    f.write(content)
