import re

file_path = 'os/linux/hif/pcie/pcie.c'
with open(file_path, 'r') as f:
    content = f.read()

# 1. Remove the broken fragments and any previous injections to start fresh
# This regex looks for the function name and matches until the closing brace
content = re.sub(r'void mt7902_schedule_recovery_from_atomic.*?\{.*?\}', '', content, flags=re.DOTALL)
content = re.sub(r'static int mt7902_mmio_dead.*?\{.*?\}', '', content, flags=re.DOTALL)

# 2. Specifically target the broken DBGLOG lines that are causing the "missing terminating character"
content = re.sub(r'DBGLOG\(HAL, WARN, "Scheduling recovery workqueue\.\.\..*?;', '', content)

# 3. Now, inject the CLEAN versions at a known safe spot (before the ISR)
final_logic = """
static int mt7902_mmio_dead(struct GL_HIF_INFO *prHifInfo) {
    u32 val;
    pci_read_config_dword(prHifInfo->pdev, 0, &val);
    return (val == 0xffffffff);
}

void mt7902_schedule_recovery_from_atomic(struct GLUE_INFO *prGlueInfo)
{
    struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
    if (test_and_set_bit(MTK_FLAG_MMIO_GONE, &prHifInfo->state_flags) == 0) {
        DBGLOG(HAL, WARN, "Scheduling recovery workqueue...\\n");
        schedule_work(&prHifInfo->recovery_work);
    }
}
"""

# Insert before the ISR
content = content.replace('static irqreturn_t mtk_pci_interrupt', final_logic + '\nstatic irqreturn_t mtk_pci_interrupt')

with open(file_path, 'w') as f:
    f.write(content)
