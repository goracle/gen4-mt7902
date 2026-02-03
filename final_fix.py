import re

file_path = 'os/linux/hif/pcie/pcie.c'
with open(file_path, 'r') as f:
    lines = f.readlines()

# We'll reconstruct the file logic cleanly
new_content = []
skip = False

# This logic removes the broken versions we just tried to inject
for line in lines:
    if 'void mt7902_schedule_recovery_from_atomic' in line:
        skip = True
    if skip and '}' in line:
        skip = False
        continue
    if not skip:
        new_content.append(line)

full_text = "".join(new_content)

# Define the clean, robust logic
helper_logic = """
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

# 1. Insert the helper/scheduler before the recovery work function
full_text = re.sub(r'static void mt7902_recovery_work', helper_logic + '\nstatic void mt7902_recovery_work', full_text)

# 2. Fix the ISR (Interrupt Service Routine)
isr_pattern = r'static irqreturn_t mtk_pci_interrupt\(int irq, void \*dev_instance\)\n\{.*?\n\thalDisableInterrupt'
protected_isr = """static irqreturn_t mtk_pci_interrupt(int irq, void *dev_instance)
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

    halDisableInterrupt"""

full_text = re.sub(isr_pattern, protected_isr, full_text, flags=re.DOTALL)

# 3. Fix the context check in the recovery function
rec_pattern = r'uint32_t mt79xx_pci_function_recover\(struct pci_dev \*pdev,\n\s+struct GLUE_INFO \*prGlueInfo\)\n\{'
protected_rec = """uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev,
					 struct GLUE_INFO *prGlueInfo)
{
    if (in_atomic() || in_interrupt()) {
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }
"""
full_text = re.sub(rec_pattern, protected_rec, full_text)

with open(file_path, 'w') as f:
    f.write(full_text)
