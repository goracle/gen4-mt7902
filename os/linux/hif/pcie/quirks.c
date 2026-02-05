#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/init.h>

/* MT7902 PCIe quirk to disable L1/L1.1/L1.2 substates */
static void mt7902_disable_aspm(struct pci_dev *pdev)
{
	/* Only run if this is MT7902 */
	if (pdev->vendor == PCI_VENDOR_ID_MEDIATEK && pdev->device == 0x7902) {
		pr_info("MT7902 quirk: disabling L1/L1.1/L1.2 ASPM states\n");
		pci_disable_link_state(pdev,
			PCIE_LINK_STATE_L1 |
			PCIE_LINK_STATE_L1_1 |
			PCIE_LINK_STATE_L1_2);
	}
}

/* Register the quirk to run after PCI device is probed but before driver init */
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_MEDIATEK, 0x7902,
	mt7902_disable_aspm);
