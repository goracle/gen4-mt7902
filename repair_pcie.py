import re

path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    content = f.read()

# 1. DELETE the "Frankenstein" block at the top (around line 208)
# We look for the duplicate recovery function and everything until the start of glResetHifInfo
content = re.sub(r'uint32_t mt79xx_pci_function_recover\(struct pci_dev \*pdev,.*?void glResetHifInfo', 'void glResetHifInfo', content, flags=re.DOTALL)

# 2. DELETE the messy duplicates in the middle (around line 837)
# This removes the stray quotes and the repeated headers
content = re.sub(r'uint32_t mt79xx_pci_function_recover\(struct pci_dev \*pdev,.*?struct ADAPTER \*prAdapter;', 'struct ADAPTER *prAdapter;', content, flags=re.DOTALL)

# 3. Clean out the ISR and previous helper attempts
content = re.sub(r'static int mt7902_mmio_dead.*?static irqreturn_t mtk_pci_interrupt', 'static irqreturn_t mtk_pci_interrupt', content, flags=re.DOTALL)
content = re.sub(r'void mt7902_schedule_recovery_from_atomic.*?static irqreturn_t mtk_pci_interrupt', 'static irqreturn_t mtk_pci_interrupt', content, flags=re.DOTALL)

# 4. DEFINE the clean logic block
clean_logic = """
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

static irqreturn_t mtk_pci_interrupt(int irq, void *dev_instance)
{
    struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)dev_instance;
    struct GL_HIF_INFO *prHifInfo;

    if (!prGlueInfo) return IRQ_NONE;
    prHifInfo = &prGlueInfo->rHifInfo;

    if (mt7902_mmio_dead(prHifInfo)) {
        disable_irq_nosync(irq);
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return IRQ_HANDLED;
    }

    halDisableInterrupt(prGlueInfo->prAdapter);
"""

# Replace the old ISR with the new one + helpers
content = re.sub(r'static irqreturn_t mtk_pci_interrupt.*?halDisableInterrupt\(.*?\);', clean_logic, content, flags=re.DOTALL)

# 5. Fix the main recovery function header
content = content.replace('struct ADAPTER *prAdapter;', 'uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev, struct GLUE_INFO *prGlueInfo)\n{\n    struct ADAPTER *prAdapter;')

with open(path, 'w') as f:
    f.write(content)
