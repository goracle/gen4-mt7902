import os

path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    content = f.read()

# 1. Define the MMIO dead helper and the Scheduler correctly
new_scheduler = """
static int mt7902_mmio_dead(struct GL_HIF_INFO *prHifInfo) {
    u32 val;
    pci_read_config_dword(prHifInfo->pdev, 0, &val);
    return (val == 0xffffffff);
}

void mt7902_schedule_recovery_from_atomic(struct GLUE_INFO *prGlueInfo)
{
    struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
    if (test_and_set_bit(MTK_FLAG_MMIO_GONE, &prHifInfo->state_flags) == 0) {
        DBGLOG(HAL, WARN, "Scheduling recovery workqueue...\n");
        schedule_work(&prHifInfo->recovery_work);
    }
}
"""

# 2. Define the protected ISR
new_isr = """
static irqreturn_t mtk_pci_interrupt(int irq, void *dev_instance)
{
    struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)dev_instance;
    struct GL_HIF_INFO *prHifInfo;

    if (!prGlueInfo) {
        return IRQ_NONE;
    }
    prHifInfo = &prGlueInfo->rHifInfo;

    if (mt7902_mmio_dead(prHifInfo)) {
        disable_irq_nosync(irq);
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return IRQ_HANDLED;
    }

    halDisableInterrupt(prGlueInfo->prAdapter);
"""

# Replace the broken scheduler and ISR using markers
import re
content = re.sub(r'void mt7902_schedule_recovery_from_atomic.*?\{.*?\}', new_scheduler, content, flags=re.DOTALL)
content = re.sub(r'static irqreturn_t mtk_pci_interrupt.*?\{.*?halDisableInterrupt\(prGlueInfo->prAdapter\);', new_isr, content, flags=re.DOTALL)

with open(path, 'w') as f:
    f.write(content)
