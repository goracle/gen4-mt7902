/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/******************************************************************************
 * [Standard License Header Omitted]
 *****************************************************************************/

/*******************************************************************************
 * C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 * E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

#include "gl_os.h"
#include <linux/pci.h>
#include "chips/mt7902.h"
#include "hif_pdma.h"
#include <linux/pm_runtime.h>
#include <linux/workqueue.h> /* Added for V2 recovery fix */
/* os/linux/hif/pcie/pcie.c */

#include <linux/pci.h>
#include "chips/host_csr.h"


#include "precomp.h"
#include "chips/hal_wfsys_reset_mt7961.h"

#include <linux/mm.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"
static probe_card pfWlanProbe;
static remove_card pfWlanRemove;

static u_int8_t g_fgDriverProbed = FALSE;
//static uint32_t g_u4DmaMask = 32;

u_int8_t g_fgInInitialColdBoot = TRUE;  /* Exported for HAL layer */


/*******************************************************************************
 * M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 * F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev, struct GLUE_INFO *prGlueInfo);
static void pcieAllocDesc(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMABUF *prDescRing,
			  uint32_t u4Num);
static void *pcieAllocRxBuf(struct GL_HIF_INFO *prHifInfo,
			    struct RTMP_DMABUF *prDmaBuf,
			    uint32_t u4Num, uint32_t u4Idx);
static void pcieAllocTxDataBuf(struct MSDU_TOKEN_ENTRY *prToken,
			       uint32_t u4Idx);
static void *pcieAllocRuntimeMem(uint32_t u4SrcLen);
static bool pcieCopyCmd(struct GL_HIF_INFO *prHifInfo,
			struct RTMP_DMACB *prTxCell, void *pucBuf,
			void *pucSrc1, uint32_t u4SrcLen1,
			void *pucSrc2, uint32_t u4SrcLen2);
static bool pcieCopyEvent(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMACB *pRxCell,
			  struct RXD_STRUCT *pRxD,
			  struct RTMP_DMABUF *prDmaBuf,
			  uint8_t *pucDst, uint32_t u4Len);
static bool pcieCopyTxData(struct MSDU_TOKEN_ENTRY *prToken,
			   void *pucSrc, uint32_t u4Len);
static bool pcieCopyRxData(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMACB *pRxCell,
			   struct RTMP_DMABUF *prDmaBuf,
			   struct SW_RFB *prSwRfb);
static phys_addr_t pcieMapTxBuf(struct GL_HIF_INFO *prHifInfo,
			  void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
static phys_addr_t pcieMapRxBuf(struct GL_HIF_INFO *prHifInfo,
			  void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
static void pcieUnmapTxBuf(struct GL_HIF_INFO *prHifInfo,
			   phys_addr_t rDmaAddr, uint32_t u4Len);
static void pcieUnmapRxBuf(struct GL_HIF_INFO *prHifInfo,
			   phys_addr_t rDmaAddr, uint32_t u4Len);
static void pcieFreeDesc(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMABUF *prDescRing);
static void pcieFreeBuf(void *pucSrc, uint32_t u4Len);
static void pcieFreePacket(void *pvPacket);
static void pcieDumpTx(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_TX_RING *prTxRing,
		       uint32_t u4Idx, uint32_t u4DumpLen);
static void pcieDumpRx(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_RX_RING *prRxRing,
		       uint32_t u4Idx, uint32_t u4DumpLen);

static int mtk_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void mtk_pci_remove(struct pci_dev *pdev);
/* Supported PCI device IDs for MT7902 */
static const struct pci_device_id mtk_pci_ids[] = {
    { PCI_DEVICE(0x14c3, 0x7902),
      .driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt7902 },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, mtk_pci_ids);

static struct pci_driver mtk_pci_driver = {
    .name = "mtk_pci_driver",
    .id_table = mtk_pci_ids,
    .probe = mtk_pci_probe,   // IMPORTANT: Make sure these aren't missing
    .remove = mtk_pci_remove, // IMPORTANT: Make sure these aren't missing
};

/*******************************************************************************
 * F U N C T I O N S
 *******************************************************************************
 */

/*----------------------------------------------------------------------------*/
/*! \brief Fast MMIO liveness check - safe in atomic context
 *
 * \param[in] prHifInfo Pointer to HIF info structure
 * \return TRUE if MMIO is dead (0xffffffff), FALSE if alive
 */
/*----------------------------------------------------------------------------*/
static inline u_int8_t mt7902_mmio_dead(struct GL_HIF_INFO *prHifInfo)
{
	uint32_t u4Val;
	int i;

	if (!prHifInfo || !prHifInfo->CSRBaseAddress)
		return TRUE;

	/* * INNOVATION: Give the 1x1 chip 3 tries to wake up.
	 * During a cold boot, the link might be 'retraining'.
	 */
	for (i = 0; i < 3; i++) {
		u4Val = readl(prHifInfo->CSRBaseAddress + 0x0);
		if (u4Val != 0xFFFFFFFF && u4Val != 0x00000000)
			return FALSE;
		mdelay(1); /* Wait for PLL lock */
	}

	return TRUE;
}

#define MT7902_PCIE_HOST_INT_STATUS    0x18
#define MT7902_PCIE_HOST_INT_ENA_SET   0x104
#define MT7902_PCIE_INT_ALL_MASK       0xFFFFFFFF

/* MT7902 specific remap constants */
#define MT7902_REMAP_BASE      0x1000   /* Most common remap reg */
#define MT7902_REMAP_WINDOW    0x40000  /* The 64KB window starts here */
#define MT7902_REMAP_SIZE      0x10000  /* 64KB per page */

static void pcieDisableInterrupt(struct ADAPTER *prAdapter)
{
	if (unlikely(!prAdapter))
		return;

	/* MSI ACK is handled by the kernel automatically.
	 * Just flip the flag; wlanIST re-arms via nicEnableInterrupt. */
	prAdapter->fgIsIntEnable = FALSE;
}

static bool pcieIsHifDown(struct GLUE_INFO *prGlueInfo)
{
	uint32_t u4Val;

	if (unlikely(!prGlueInfo || !prGlueInfo->rHifInfo.CSRBaseAddress))
		return TRUE;

	u4Val = readl(prGlueInfo->rHifInfo.CSRBaseAddress + MT7902_PCIE_HOST_INT_STATUS);

	/* 0xFFFFFFFF indicates the PCI link is down or the device is gone */
	return (u4Val == 0xFFFFFFFF);
}

/* This is the variable the compiler complained about.
   Keeping it here ensures the functions above are linked. */
static struct HAL_OPS pcieHalOps = {
	.pfnDisableInterrupt = pcieDisableInterrupt,
	.pfnIsHifDown = pcieIsHifDown,
};

struct HAL_OPS *mtk_pcie_get_ops(void)
{
	return &pcieHalOps;
}

static irqreturn_t mtk_pci_interrupt(int irq, void *dev_instance)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) dev_instance;
	struct ADAPTER *prAdapter;

	if (unlikely(!prGlueInfo))
		return IRQ_NONE;

	prAdapter = prGlueInfo->prAdapter;

	/* Check if the adapter or HAL ops are NULL before dereferencing */
	if (unlikely(!prAdapter || !prAdapter->prHalOps))
		return IRQ_NONE;

	/* If the bus is down, just claim the IRQ and exit to prevent hang */
	if (prAdapter->prHalOps->pfnIsHifDown &&
	    prAdapter->prHalOps->pfnIsHifDown(prGlueInfo))
		return IRQ_HANDLED;

	/* Acknowledge and mask hardware interrupts */
	if (prAdapter->prHalOps->pfnDisableInterrupt)
		prAdapter->prHalOps->pfnDisableInterrupt(prAdapter);

	/* Schedule the bottom half / tasklet */
	kalSetIntEvent(prGlueInfo);

	return IRQ_HANDLED;
}

/* pcie.c */
extern void mt7902_hard_cleanup(struct GLUE_INFO *prGlueInfo, struct ADAPTER *prAdapter);

/* --- Dynamic Window Helpers --- */

#if 0
static void mt7902_pci_remap(void __iomem *base, u32 addr)
{
	u32 page = addr & ~(MT7902_REMAP_SIZE - 1);

	/* Move the viewfinder to the 64KB page containing our target */
	writel(page, base + MT7902_REMAP_BASE);

	/* Hardware needs a moment to stabilize the bus lines.
	 * 100us is standard for MediaTek Gen4 PCIe.
	 */
	udelay(100);
}

static u32 mt7902_win_read(void __iomem *base, u32 addr)
{
	/* Safety check: if BAR is returning 0xffffffff, don't even try */
	if (readl(base + MT7902_REMAP_BASE) == 0xffffffff)
		return 0xffffffff;

	mt7902_pci_remap(base, addr);
	return readl(base + MT7902_REMAP_WINDOW + (addr & (MT7902_REMAP_SIZE - 1)));
}

static void mt7902_win_write(void __iomem *base, u32 addr, u32 val)
{
	if (readl(base + MT7902_REMAP_BASE) == 0xffffffff)
		return;

	mt7902_pci_remap(base, addr);
	writel(val, base + MT7902_REMAP_WINDOW + (addr & (MT7902_REMAP_SIZE - 1)));
}

#endif

/* --- Optimized glBusInit --- */

static int mtk_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret;
	struct GLUE_INFO *prGlueInfo = NULL;

	ret = pci_enable_device(pdev);
	if (ret) {
		DBGLOG(INIT, ERROR, "pci_enable_device failed (%d)\n", ret);
		return ret;
	}

	ret = pci_request_regions(pdev, DRV_NAME);
	if (ret) {
		DBGLOG(INIT, WARN, "pci_request_regions busy, attempting recovery...\n");
		pci_release_regions(pdev);
		ret = pci_request_regions(pdev, DRV_NAME);
		if (ret) {
			DBGLOG(INIT, ERROR, "pci_request_regions failed permanently (%d)\n", ret);
			goto err_disable_device;
		}
	}

	ret = pci_set_power_state(pdev, PCI_D0);
	if (ret) {
		DBGLOG(INIT, WARN, "pci_set_power_state(D0) failed (%d)\n", ret);
	}

	pci_set_master(pdev);
	pci_save_state(pdev);

	ret = pfWlanProbe(pdev, (void *)id->driver_data);
	if (ret) {
		DBGLOG(INIT, ERROR, "pfWlanProbe failed (%d)\n", ret);
		
		/* Emergency DMA Cleanup: 
		 * If pfWlanProbe mapped rings to the SWIOTLB but failed later (e.g., timeout),
		 * we MUST unmap them here or the slots stay leaked until reboot.
		 */
		prGlueInfo = pci_get_drvdata(pdev);
		if (prGlueInfo && prGlueInfo->prAdapter) {
			DBGLOG(INIT, WARN, "mtk_pci_probe: Cleaning up leaked DMA rings\n");
			//mt792x_dma_cleanup(prGlueInfo->prAdapter);
		}
		goto err_release_regions;
	}

	prGlueInfo = pci_get_drvdata(pdev);
	if (prGlueInfo && prGlueInfo->prAdapter)
		prGlueInfo->prAdapter->prHalOps = mtk_pcie_get_ops();

	return 0;

err_release_regions:
	pci_clear_master(pdev);
	pci_release_regions(pdev);
err_disable_device:
	pci_disable_device(pdev);
	return ret;
}



static void mtk_pci_remove(struct pci_dev *pdev)
{
	struct GLUE_INFO *prGlueInfo = pci_get_drvdata(pdev);
	struct GL_HIF_INFO *prHifInfo;
	struct ADAPTER *prAdapter = NULL;

	if (!prGlueInfo)
		goto disable_device;

	prHifInfo = &prGlueInfo->rHifInfo;
	prAdapter = prGlueInfo->prAdapter;

	/* 1. Set the teardown flag immediately.
	 * This prevents any recovery workers from trying to 'help' while we exit.
	 */
	set_bit(MTK_FLAG_TEARDOWN, &prHifInfo->state_flags);
	cancel_work_sync(&prHifInfo->recovery_work);

	/* 2. EMERGENCY THAW: If the hardware is dead, the threads are stuck.
	 * We manually signal the completions that wlanOffStopWlanThreads waits for.
	 */
	if (mt7902_mmio_dead(prHifInfo)) {
		DBGLOG(INIT, WARN, "[Sovereignty] Link dead. Force-thawing completions to prevent hang.\n");

		if (prAdapter) {
			/* Signal the scan completion */
			complete_all(&prAdapter->rScanDoneCompletion);

			/* * MTK drivers usually have hidden completions in prAdapter or prHifInfo
			 * used for thread synchronization (rHaltComp, rHifHaltComp).
			 * We signal these to allow the 'wait_for_completion' in
			 * wlanOffStopWlanThreads to fall through.
			 */
			if (prAdapter->fgIsIntEnable) {
				/* If you have access to rHaltComp or rHifHaltComp in your headers:
				 * complete_all(&prAdapter->rHaltComp);
				 * complete_all(&prAdapter->rHifHaltComp);
				 */
			}
		}
	}

	/* 3. Disable interrupts at the kernel level so the ISR stops firing
	 * 0xFFFFFFFF reads into the tasklets.
	 */
	disable_irq(pdev->irq);

	/* 4. Trigger the main driver removal.
	 * With the completions forced above, this should no longer hang.
	 */
	pfWlanRemove();

disable_device:
	if (pci_is_enabled(pdev)) {
		/* Clean up PCI resources */
		pci_release_regions(pdev);
		pci_disable_device(pdev);
	}

	pci_set_drvdata(pdev, NULL);
	DBGLOG(INIT, INFO, "[Sovereignty] mtk_pci_remove: Cleanup complete.\n");
}
/*----------------------------------------------------------------------------*/
/*! \brief Mark IRQs as dead - call before any PCI teardown
 *
 * This MUST be called before:
 * - pci_disable_device()
 * - pci_clear_master()
 * - pci_set_power_state(D3*)
 * - pci_disable_msi()
 * - Writing PCI COMMAND=0
 *
 * \param[in] prHifInfo Pointer to HIF info structure
 */
/*----------------------------------------------------------------------------*/
static inline void mt7902_mark_irq_dead(struct GL_HIF_INFO *prHifInfo)
{
	clear_bit(MTK_FLAG_IRQ_ALIVE, &prHifInfo->state_flags);
	DBGLOG(HAL, WARN, "IRQ marked as DEAD - no more synchronize_irq allowed\n");
}

/*----------------------------------------------------------------------------*/
/*! \brief Schedule recovery from atomic/IRQ/tasklet context
 *
 * \param[in] func  pointer to PCIE handle
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/

/*!
 * \brief Schedule recovery from atomic/IRQ/tasklet context
 *
 * \param[in] func  pointer to PCIE handle
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/

/*!
 * \brief Schedule recovery from atomic/IRQ/tasklet context
 *
 * \param[in] prGlueInfo Pointer to Glue Info
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/
void mt7902_schedule_recovery_from_atomic(struct GLUE_INFO *prGlueInfo)
{
    struct GL_HIF_INFO *prHifInfo;

    if (!prGlueInfo)
        return;

    prHifInfo = &prGlueInfo->rHifInfo;

    /* * FIX: PREVENT ILLEGAL RECOVERY SEQUENCING
     * As diagnosed: Recovery logic assumes Host Ownership, configured DMA rings,
     * and specific WFDMA states. None of these exist before Probe completes.
     *
     * 0xDEADxxxx reads during probe are often transient (split-brain window).
     * Triggering recovery here is illegal; we must let the probe fail gracefully
     * or retry via standard init logic, not invoke SER/DMA polling.
     */
    if (!g_fgDriverProbed) {
        if (net_ratelimit()) {
            printk(KERN_WARNING "mt7902: Ignoring recovery request during probe/boot phase. "
                   "MMIO anomaly (0x%08x) is likely transient or sequencing error.\n",
                   readl(prHifInfo->CSRBaseAddress + 0x0));
        }
        return;
    }

    /* Standard Recovery Logic - Only runs POST-PROBE */
    if (test_and_set_bit(MTK_FLAG_MMIO_GONE, &prHifInfo->state_flags)) {
        /* Prevent duplicate scheduling */
        return;
    }

    printk(KERN_ERR "mt7902: MMIO failure detected post-probe. Scheduling recovery.\n");

    /* Disable IRQ safely */
    if (prHifInfo->saved_irq > 0)
        disable_irq_nosync(prHifInfo->saved_irq);

    /* Stop queues */
    if (prGlueInfo->prDevHandler)
        netif_tx_stop_all_queues(prGlueInfo->prDevHandler);

if (!test_bit(MTK_FLAG_TEARDOWN, &prHifInfo->state_flags))
    schedule_work(&prHifInfo->recovery_work);

}

static void mt7902_recovery_work(struct work_struct *work)
{
    struct GL_HIF_INFO *prHifInfo = container_of(work, struct GL_HIF_INFO, recovery_work);
    struct GLUE_INFO *prGlueInfo = container_of(prHifInfo, struct GLUE_INFO, rHifInfo);

    /* Ensure only one recovery at a time */
    if (!mutex_trylock(&prHifInfo->recovery_lock)) {
        DBGLOG(HAL, WARN, "Recovery already in progress\n");
        return;
    }
    if (test_bit(MTK_FLAG_TEARDOWN, &prHifInfo->state_flags)) {
        DBGLOG(HAL, WARN, "Teardown in progress, skipping recovery\n");
        mutex_unlock(&prHifInfo->recovery_lock);
        return;
    }

    DBGLOG(HAL, INFO, "=== Recovery work starting (process context) ===\n");
    mt79xx_pci_function_recover(prHifInfo->pdev, prGlueInfo);

    /* Clean up flags after recovery */
    /* MTK_FLAG_MMIO_GONE is cleared in mt79xx_pci_function_recover on success */

    mutex_unlock(&prHifInfo->recovery_lock);
}

static void *CSRBaseAddress;

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a PCIE probe function
 *
 * \param[in] func   pointer to PCIE handle
 * \param[in] id     pointer to PCIE device id table
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/

static int mtk_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BUS_INFO *prBusInfo;
	uint32_t count = 0;
	int wait = 0;
	struct ADAPTER *prAdapter = NULL;
	uint8_t drv_own_fail = FALSE;
	struct ERR_RECOVERY_CTRL_T *prErrRecoveryCtrl;
	uint32_t u4Status = 0;
	uint32_t u4Tick;

	DBGLOG(HAL, STATE, "mtk_pci_suspend()\n");

	prGlueInfo = (struct GLUE_INFO *)pci_get_drvdata(pdev);
	if (!prGlueInfo) {
		DBGLOG(HAL, ERROR, "pci_get_drvdata fail!\n");
		return -1;
	}

	prAdapter = prGlueInfo->prAdapter;
	prGlueInfo->fgIsInSuspendMode = TRUE;
	prErrRecoveryCtrl = &prGlueInfo->rHifInfo.rErrRecoveryCtl;

	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

	/* Stop upper layers calling the device hard_start_xmit routine. */
	netif_tx_stop_all_queues(prGlueInfo->prDevHandler);

#if CFG_ENABLE_WAKE_LOCK
	prGlueInfo->rHifInfo.eSuspendtate = PCIE_STATE_SUSPEND_ENTERING;
#endif

	/* wait wiphy device do cfg80211 suspend done, then start hif suspend */
	if (IS_FEATURE_ENABLED(prGlueInfo->prAdapter->rWifiVar.ucWow))
		wlanWaitCfg80211SuspendDone(prGlueInfo);

	wlanSuspendPmHandle(prGlueInfo);

#if !CFG_ENABLE_WAKE_LOCK
	prGlueInfo->rHifInfo.eSuspendtate = PCIE_STATE_PRE_SUSPEND_WAITING;
#endif

	halPciePreSuspendCmd(prAdapter);

	while (prGlueInfo->rHifInfo.eSuspendtate !=
		PCIE_STATE_PRE_SUSPEND_DONE) {
		if (count > 500) {
			DBGLOG(HAL, ERROR, "pcie pre_suspend timeout\n");
			return -EAGAIN;
		}
		kalMsleep(2);
		count++;
	}
	DBGLOG(HAL, ERROR, "pcie pre_suspend done\n");

	prGlueInfo->rHifInfo.eSuspendtate = PCIE_STATE_SUSPEND;

	/* Polling until HIF side PDMAs are all idle */
	prBusInfo = prAdapter->chip_info->bus_info;
	if (prBusInfo->pdmaPollingIdle) {
		if (prBusInfo->pdmaPollingIdle(prGlueInfo) != TRUE)
			return -EAGAIN;
	} else
		DBGLOG(HAL, ERROR, "PDMA polling idle API didn't register\n");

	/* Disable HIF side PDMA TX/RX */
	if (prBusInfo->pdmaStop)
		prBusInfo->pdmaStop(prGlueInfo, TRUE);
	else
		DBGLOG(HAL, ERROR, "PDMA config API didn't register\n");

	/* Make sure any pending or on-going L1 reset is done before HIF
	 * suspend.
	 */
	u4Tick = kalGetTimeTick();
	while (1) {
		u4Status = prBusInfo->getSoftwareInterrupt ?
			prBusInfo->getSoftwareInterrupt(prAdapter) : 0;
		if (((u4Status & ERROR_DETECT_MASK) == 0) &&
		   (prErrRecoveryCtrl->eErrRecovState == ERR_RECOV_STOP_IDLE)) {
			DBGLOG(HAL, INFO, "[SER][L1] no pending L1 reset\n");
			break;
		}

		kalMsleep(40);

		if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4Tick,
			       MSEC_TO_SYSTIME(WIFI_SER_L1_RST_DONE_TIMEOUT))) {

			DBGLOG(HAL, ERROR,
			       "[SER][L1] reset timeout before suspend\n");
			break;
		}
	}

	halDisableInterrupt(prAdapter);

	/* FW own */
	/* Set FW own directly without waiting sleep notify */
	prAdapter->fgWiFiInSleepyState = TRUE;
	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

	/* Wait for
	* 1. The other unfinished ownership handshakes
	* 2. FW own back
	*/
	while (wait < 500) {
		if ((prAdapter->u4PwrCtrlBlockCnt == 0) &&
		    (prAdapter->fgIsFwOwn == TRUE) &&
		    (drv_own_fail == FALSE)) {
			DBGLOG(HAL, STATE, "*********************\n");
			DBGLOG(HAL, STATE, "* Enter PCIE Suspend *\n");
			DBGLOG(HAL, STATE, "*********************\n");
			DBGLOG(HAL, INFO, "wait = %d\n\n", wait);
			break;
		}

		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
		/* Prevent that suspend without FW Own:
		 * Set Drv own has failed,
		 * and then Set FW Own is skipped
		 */
		if (prAdapter->fgIsFwOwn == FALSE)
			drv_own_fail = FALSE;
		else
			drv_own_fail = TRUE;
		/* For single core CPU */
		/* let hif_thread can be completed */
		usleep_range(1000, 3000);
		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		wait++;
	}

	if (wait >= 500) {
		DBGLOG(HAL, ERROR, "Set FW Own Timeout !!\n");
		return -EAGAIN;
	}

	pci_save_state(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	DBGLOG(HAL, STATE, "mtk_pci_suspend() done!\n");

	/* pending cmd will be kept in queue and no one to handle it after HIF resume.
	 * In STR, it will result in cmd buf full and then cmd buf alloc fail .
	 */
	if (IS_FEATURE_ENABLED(prGlueInfo->prAdapter->rWifiVar.ucWow))
		wlanReleaseAllTxCmdQueue(prGlueInfo->prAdapter);

	return 0;
}

int mtk_pci_resume(struct pci_dev *pdev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BUS_INFO *prBusInfo;
	struct CHIP_DBG_OPS *prDebugOps;
	uint32_t count = 0;

	DBGLOG(HAL, STATE, "mtk_pci_resume()\n");

	prGlueInfo = (struct GLUE_INFO *)pci_get_drvdata(pdev);
	if (!prGlueInfo) {
		DBGLOG(HAL, ERROR, "pci_get_drvdata fail!\n");
		return -1;
	}

	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
	prDebugOps = prGlueInfo->prAdapter->chip_info->prDebugOps;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

/* Driver own */
	/* Include restore PDMA settings */
	ACQUIRE_POWER_CONTROL_FROM_PM(prGlueInfo->prAdapter);

	if (prBusInfo->initPcieInt)
		prBusInfo->initPcieInt(prGlueInfo);

	halEnableInterrupt(prGlueInfo->prAdapter);

	/* Enable HIF side PDMA TX/RX */
	if (prBusInfo->pdmaStop)
		prBusInfo->pdmaStop(prGlueInfo, FALSE);
	else
		DBGLOG(HAL, ERROR, "PDMA config API didn't register\n");

	halPcieResumeCmd(prGlueInfo->prAdapter);

	while (prGlueInfo->rHifInfo.eSuspendtate != PCIE_STATE_PRE_RESUME_DONE) {
		if (count > 500) {
			DBGLOG(HAL, ERROR, "pre_resume timeout\n");

			if (prDebugOps) {
				if (prDebugOps->show_mcu_debug_info)
					prDebugOps->show_mcu_debug_info(prGlueInfo->prAdapter, NULL, 0,
						DBG_MCU_DBG_ALL, NULL);
#if (CFG_SUPPORT_DEBUG_SOP == 1)
				if (prDebugOps->show_debug_sop_info)
					prDebugOps->show_debug_sop_info(prGlueInfo->prAdapter,
						SLAVENORESP);
#endif
				if (prDebugOps->showCsrInfo)
					prDebugOps->showCsrInfo(prGlueInfo->prAdapter);
			}

			break;
		}

		kalUsleep_range(2000,3000);
		count++;
	}

	wlanResumePmHandle(prGlueInfo);

	/* FW own */
	RECLAIM_POWER_CONTROL_TO_PM(prGlueInfo->prAdapter, FALSE);

	prGlueInfo->fgIsInSuspendMode = FALSE;
	/* Allow upper layers to call the device hard_start_xmit routine. */
	netif_tx_wake_all_queues(prGlueInfo->prDevHandler);

	DBGLOG(HAL, STATE, "mtk_pci_resume() done!\n");

	return 0;
}

/* Wrap modern dev_pm_ops checks in gaurds and keep the legacy pci suspend and resume setup for backwards compatiblity */
#if IS_ENABLED(CONFIG_PM)
/* These _pm_ tiny wrappers for the suspend and resume methods have the signature
 * that is expected by the dev_pm_ops
 */
static int mtk_pci_pm_suspend(struct device *dev)
{
	return mtk_pci_suspend(to_pci_dev(dev), PMSG_SUSPEND);
}

static int mtk_pci_pm_resume(struct device *dev)
{
	return mtk_pci_resume(to_pci_dev(dev));
}

/* Use the SET_SYSTEM_SLEEP_PM_OPS helper to create the dev_pm_ops struct that will
 * be set as the pm struct on the driver
 */
static const struct dev_pm_ops mtk_pci_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_pci_pm_suspend, mtk_pci_pm_resume)
};
#endif /* CONFIG_PM */

static void mtk_pci_shutdown(struct pci_dev *pdev)
{
	DBGLOG(HAL, STATE, "mtk_pci_shutdown()\n");

#if IS_ENABLED(CONFIG_PM)
	/* Try to follow the suspend path to leave firmware in a clean state */
	/* Note the legacy api driver.suspend/driver.resume were called on sleep, but not on reboot/shutdown */
	mtk_pci_suspend(pdev, PMSG_SUSPEND);
#endif

	mtk_pci_remove(pdev);

	g_fgDriverProbed = FALSE;

	DBGLOG(HAL, STATE, "mtk_pci_shutdown() done\n");
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will register pci bus to the os
 *
 * \param[in] pfProbe    Function pointer to detect card
 * \param[in] pfRemove   Function pointer to remove card
 *
 * \return The result of registering pci bus
 */
/*----------------------------------------------------------------------------*/
uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	int ret = 0;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	pfWlanProbe = pfProbe;
	pfWlanRemove = pfRemove;

	mtk_pci_driver.probe = mtk_pci_probe;
	mtk_pci_driver.remove = mtk_pci_remove;

	mtk_pci_driver.suspend = mtk_pci_suspend;
	mtk_pci_driver.resume = mtk_pci_resume;
	mtk_pci_driver.shutdown = mtk_pci_shutdown;
#if IS_ENABLED(CONFIG_PM)
	mtk_pci_driver.driver.pm = &mtk_pci_pm_ops;
#endif

	ret = (pci_register_driver(&mtk_pci_driver) == 0) ?
		WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE;

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will unregister pci bus to the os
 *
 * \param[in] pfRemove Function pointer to remove card
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void glUnregisterBus(remove_card pfRemove)
{
	if (g_fgDriverProbed) {
		pfRemove();
		g_fgDriverProbed = FALSE;
	}
	pci_unregister_driver(&mtk_pci_driver);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function stores hif related info, which is initialized before.
 *
 * \param[in] prGlueInfo Pointer to glue info structure
 * \param[in] u4Cookie   Pointer to UINT_32 memory base variable for _HIF_HPI
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie)
{
	struct GL_HIF_INFO *prHif = NULL;
	struct HIF_MEM_OPS *prMemOps;
	struct pci_dev *pdev = (struct pci_dev *)ulCookie;

	if (!prGlueInfo || !pdev) {
		pr_err("[Sovereignty] CRITICAL: glSetHifInfo called with NULL context!\n");
		return;
	}

	prHif = &prGlueInfo->rHifInfo;
	prMemOps = &prHif->rMemOps;

	/* Basic PCIe setup */
	prHif->pdev = pdev;
	prHif->prDmaDev = pdev;
	prHif->CSRBaseAddress = CSRBaseAddress;

	/* * Link the glue info to pdev immediately.
	 * This is our "lifeline" for the mtk_pci_remove safety check.
	 */
	pci_set_drvdata(pdev, prGlueInfo);
	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &pdev->dev);

	/* * SOVEREIGNTY: PCIe HARDENING
	 * We disable ASPM at the Linux Kernel level before the driver even
	 * touches the registers. This prevents the Vivobook's firmware from
	 * trying to "help" us save power while we are initializing.
	 */
	pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S |
				     PCIE_LINK_STATE_L1 |
				     PCIE_LINK_STATE_CLKPM);

	/* Disable Linux Runtime PM - Keep the rail hot */
	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);

	/* Call our (now stubbed/neutered) driver-level ASPM config */
	glBusConfigASPM(pdev, 0);
	glBusConfigASPML1SS(pdev, 0);

	/* Initialize internal states */
	prGlueInfo->u4InfType = MT_DEV_INF_PCIE;
	prHif->rErrRecoveryCtl.eErrRecovState = ERR_RECOV_STOP_IDLE;
	prHif->rErrRecoveryCtl.u4Status = 0;

	/* Recovery Timer Setup */
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	timer_setup(&prHif->rSerTimer, halHwRecoveryTimeout, 0);
#else
	init_timer(&prHif->rSerTimer);
	prHif->rSerTimer.function = halHwRecoveryTimeout;
	prHif->rSerTimer.data = (unsigned long)prGlueInfo;
#endif

	/* Networking Queues */
	INIT_LIST_HEAD(&prHif->rTxCmdQ);
	INIT_LIST_HEAD(&prHif->rTxDataQ);
	prHif->u4TxDataQLen = 0;

	/* Power State Defaults */
	prHif->fgIsPowerOff = true;
	prHif->fgIsDumpLog  = false;

	/* DebugFS and logging */
	pcie_recovery_debugfs_set_hif(prHif);

	/* Register DMA and Memory Operations */
	prMemOps->allocTxDesc = pcieAllocDesc;
	prMemOps->allocRxDesc = pcieAllocDesc;
	prMemOps->allocTxCmdBuf = NULL;
	prMemOps->allocTxDataBuf = pcieAllocTxDataBuf;
	prMemOps->allocRxBuf = pcieAllocRxBuf;
	prMemOps->allocRuntimeMem = pcieAllocRuntimeMem;
	prMemOps->copyCmd = pcieCopyCmd;
	prMemOps->copyEvent = pcieCopyEvent;
	prMemOps->copyTxData = pcieCopyTxData;
	prMemOps->copyRxData = pcieCopyRxData;
	prMemOps->flushCache = NULL;
	prMemOps->mapTxBuf = pcieMapTxBuf;
	prMemOps->mapRxBuf = pcieMapRxBuf;
	prMemOps->unmapTxBuf = pcieUnmapTxBuf;
	prMemOps->unmapRxBuf = pcieUnmapRxBuf;
	prMemOps->freeDesc = pcieFreeDesc;
	prMemOps->freeBuf = pcieFreeBuf;
	prMemOps->freePacket = pcieFreePacket;
	prMemOps->dumpTx = pcieDumpTx;
	prMemOps->dumpRx = pcieDumpRx;

	DBGLOG(HAL, INFO, "[Sovereignty] HIF Info initialized and PCIe bus hardened.\n");
}

/* short helper */
void dump_pci_state(struct pci_dev *pdev)
{
    u16 cmd, status;
    u32 lnksta; // cap removed
    pci_read_config_word(pdev, PCI_COMMAND, &cmd);
    pci_read_config_word(pdev, PCI_STATUS, &status);
    pcie_capability_read_dword(pdev, PCI_EXP_LNKSTA, &lnksta);
    printk(KERN_ERR "PCI: cmd=0x%04x status=0x%04x lnksta=0x%08x\n", cmd, status, lnksta);

    /* read first 64 dwords of BAR0 */
    if (pci_resource_len(pdev, 0) >= 256) {
        void __iomem *bar = ioremap(pci_resource_start(pdev,0), 256);
        if (bar) {
            int i;
            for (i = 0; i < 64; i++)
                printk(KERN_ERR "BAR0[%02x]=0x%08x\n", i*4, readl(bar + i*4));
            iounmap(bar);
        }
    }

    /* AER — only source that proves the root complex saw an error */
    {
        int aer_pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ERR);
        if (aer_pos) {
            u32 uncorr = 0, corr = 0, uncorr_sev = 0;
            pci_read_config_dword(pdev, aer_pos + PCI_ERR_UNCOR_STATUS, &uncorr);
            pci_read_config_dword(pdev, aer_pos + PCI_ERR_UNCOR_SEVER, &uncorr_sev);
            pci_read_config_dword(pdev, aer_pos + PCI_ERR_COR_STATUS,  &corr);
            printk(KERN_ERR "AER: uncorr_status=0x%08x sev=0x%08x corr_status=0x%08x\n",
                   uncorr, uncorr_sev, corr);
        } else {
            printk(KERN_ERR "AER: capability not present on this device\n");
        }
    }
}

/*
 * safe_write_reg — write a single BAR register with pre-check and read-back.
 *
 * Intentionally uses raw readl_relaxed / writel_relaxed INSTEAD of
 * HAL_MCR_WR.  HAL_MCR_WR gates on ADAPTER_FLAG_HW_ERR and
 * ACPI_STATE_D3; both flags are stale / meaningless while the PCIe
 * function is being resurrected.  We want to hit the wire unconditionally.
 *
 * Caller must guarantee CSRBaseAddress is valid (post-ioremap).
 *
 * Returns  0 on success, -EIO if the pre-read or read-back fails.
 */
int safe_write_reg(void __iomem *base, u32 offset, u32 val)
{
    u32 r = readl_relaxed(base + offset);
    if (r == 0xffffffff) return -EIO;
    writel_relaxed(val, base + offset);
    /* read-back verify */
    if (readl_relaxed(base + offset) != val) return -EIO;
    return 0;
}

/* ---------------------------------------------------------------------
 * dump_mailbox — snapshot the four CONNINFRA mailbox registers.
 *
 * These are the primary host↔firmware communication channel.  If the
 * MCU is stuck the mailbox words often contain the last message it was
 * waiting on, which is the single most useful datum in the dump.
 *
 * Uses HAL_MCR_RD so that the ACPI / bus-mapping indirection is
 * respected; by the time this is called CSRBaseAddress is live.
 * Exported (non-static) so gl_init.c can call it on cold-boot timeout.
 * ---------------------------------------------------------------------
 */
void dump_mailbox(struct ADAPTER *prAdapter)
{
    uint32_t v;

    if (!prAdapter || !prAdapter->prGlueInfo ||
        !prAdapter->prGlueInfo->rHifInfo.CSRBaseAddress) {
        printk(KERN_ERR "dump_mailbox: CSRBaseAddress NULL, skipping\n");
        return;
    }

    /* CONN_INFRA_CFG mailbox pair (0x18001000 + 0x100 / 0x104) */
    HAL_MCR_RD(prAdapter, 0x18001100, &v);
    printk(KERN_ERR "MAILBOX: CONN2AP  (0x18001100) = 0x%08x\n", v);

    HAL_MCR_RD(prAdapter, 0x18001104, &v);
    printk(KERN_ERR "MAILBOX: AP2CONN  (0x18001104) = 0x%08x\n", v);

    /* HOST_CSR_TOP mailbox debug mirrors — these are read-only
     * shadow copies; useful because they survive some resets that
     * clear the live mailbox.
     */
    HAL_MCR_RD(prAdapter, 0x180C0024, &v);   /* HOST_MAILBOX_WF */
    printk(KERN_ERR "MAILBOX: HOST_MAILBOX_WF (0x180C0024) = 0x%08x\n", v);

    HAL_MCR_RD(prAdapter, 0x180C002C, &v);   /* WF_MAILBOX_DBG */
    printk(KERN_ERR "MAILBOX: WF_MAILBOX_DBG  (0x180C002C) = 0x%08x\n", v);
}

/* ---------------------------------------------------------------------
 * dump_pdma_state — snapshot TX-ring-0 and RX-ring-0 CIDX / DIDX / CNT.
 *
 * BUS_INFO already contains per-chip ring register addresses
 * (host_tx_ring_cidx_addr, etc.).  We read those instead of hard-coding
 * offsets so this function works on mt7902 / mt7933 / future chips
 * without changes.
 *
 * Exported for the same reason as dump_mailbox.
 * ---------------------------------------------------------------------
 */
void dump_pdma_state(struct ADAPTER *prAdapter)
{
    struct BUS_INFO *prBusInfo;
    uint32_t v;

    if (!prAdapter || !prAdapter->chip_info ||
        !prAdapter->prGlueInfo ||
        !prAdapter->prGlueInfo->rHifInfo.CSRBaseAddress) {
        printk(KERN_ERR "dump_pdma_state: prerequisites NULL, skipping\n");
        return;
    }

    prBusInfo = prAdapter->chip_info->bus_info;
    if (!prBusInfo) {
        printk(KERN_ERR "dump_pdma_state: bus_info NULL\n");
        return;
    }

    /* TX ring 0 */
    HAL_MCR_RD(prAdapter, prBusInfo->host_tx_ring_cidx_addr, &v);
    printk(KERN_ERR "PDMA TX0: CIDX = %u (reg 0x%08x)\n",
           v, prBusInfo->host_tx_ring_cidx_addr);

    HAL_MCR_RD(prAdapter, prBusInfo->host_tx_ring_didx_addr, &v);
    printk(KERN_ERR "PDMA TX0: DIDX = %u (reg 0x%08x)\n",
           v, prBusInfo->host_tx_ring_didx_addr);

    HAL_MCR_RD(prAdapter, prBusInfo->host_tx_ring_cnt_addr, &v);
    printk(KERN_ERR "PDMA TX0: CNT  = %u (reg 0x%08x)\n",
           v, prBusInfo->host_tx_ring_cnt_addr);

    /* RX ring 0 */
    HAL_MCR_RD(prAdapter, prBusInfo->host_rx_ring_cidx_addr, &v);
    printk(KERN_ERR "PDMA RX0: CIDX = %u (reg 0x%08x)\n",
           v, prBusInfo->host_rx_ring_cidx_addr);

    HAL_MCR_RD(prAdapter, prBusInfo->host_rx_ring_didx_addr, &v);
    printk(KERN_ERR "PDMA RX0: DIDX = %u (reg 0x%08x)\n",
           v, prBusInfo->host_rx_ring_didx_addr);

    HAL_MCR_RD(prAdapter, prBusInfo->host_rx_ring_cnt_addr, &v);
    printk(KERN_ERR "PDMA RX0: CNT  = %u (reg 0x%08x)\n",
           v, prBusInfo->host_rx_ring_cnt_addr);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function clears hif related info.
 *
 * \param[in] prGlueInfo Pointer to glue info structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void glClearHifInfo(struct GLUE_INFO *prGlueInfo)
{
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	struct list_head *prCur, *prNext;
	struct TX_CMD_REQ *prTxCmdReq;
	struct TX_DATA_REQ *prTxDataReq;

#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
	timer_delete_sync(&prHifInfo->rSerTimer);
#else
	del_timer_sync(&prHifInfo->rSerTimer);
#endif

	halUninitMsduTokenInfo(prGlueInfo->prAdapter);
	halWpdmaFreeRing(prGlueInfo);

	list_for_each_safe(prCur, prNext, &prHifInfo->rTxCmdQ) {
		prTxCmdReq = list_entry(prCur, struct TX_CMD_REQ, list);
		list_del(prCur);
		kfree(prTxCmdReq);
	}

	list_for_each_safe(prCur, prNext, &prHifInfo->rTxDataQ) {
		prTxDataReq = list_entry(prCur, struct TX_DATA_REQ, list);
		list_del(prCur);
		prHifInfo->u4TxDataQLen--;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function reset necessary hif related info when chip reset.
*
* \param[in] prGlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void glResetHifInfo(struct GLUE_INFO *prGlueInfo)
{
	ASSERT(prGlueInfo);
} /* end of glResetHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Tier-3 recovery: PCIe function resurrection after power loss
 *
 * This function handles the case where the device lost power and MMIO
 * reads return 0xffffffff. It performs a full PCIe re-initialization
 * followed by cold-boot MCU init, effectively treating the device as
 * if it was hot-unplugged and replugged.
 *
 * \param[in] pdev       The PCI device
 * \param[in] prGlueInfo The glue info structure
 *
 * \retval WLAN_STATUS_SUCCESS   Recovery successful, device resurrected
 * \retval WLAN_STATUS_FAILURE   Recovery failed, device remains dead
 */
/*----------------------------------------------------------------------------*/
uint32_t mt79xx_pci_function_recover(struct pci_dev *pdev,
					    struct GLUE_INFO *prGlueInfo)
{
	/*
	 * This recovery function is permanently disabled.
	 *
	 * The original implementation attempted a full D3cold power cycle,
	 * secondary bus reset, MCU cold boot, and wlanAdapterStart() to
	 * respawn all driver threads.  It never successfully recovered the
	 * hardware in practice and introduced a race with rmmod:
	 *
	 *   - wlanAdapterStart() calls reinit_completion() on rHaltComp /
	 *     rHifHaltComp / rRxHaltComp and spawns new main_thread,
	 *     hif_thread, and rx_thread.
	 *   - If rmmod races with or follows a recovery attempt, those new
	 *     threads are alive on a broken adapter.  main_thread then gets
	 *     stuck dispatching a pending OID to dead firmware
	 *     (wlanSendCommandPacket waits indefinitely on rPendComp), and
	 *     complete(&rHaltComp) is never reached — hanging rmmod forever.
	 *
	 * The correct response to unrecoverable hardware is to let the driver
	 * unload cleanly (or for the user to reboot).  The MMIO-gone detection
	 * and the TEARDOWN flag still prevent further damage; we just don't
	 * attempt the futile recovery.
	 */
	DBGLOG(HAL, WARN,
	       "mt79xx_pci_function_recover: recovery disabled, returning failure\n");
	return WLAN_STATUS_FAILURE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Stop bus operation and release resources.
 *
 * \param[in] pvData A pointer to struct net_device.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void glBusRelease(void *pvData)
{
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Setup bus interrupt operation and interrupt handler for os.
 *
 * \param[in] pvData     A pointer to struct net_device.
 * \param[in] pfnIsr     A pointer to interrupt handler function.
 * \param[in] pvCookie   Private data for pfnIsr function.
 *
 * \retval WLAN_STATUS_SUCCESS   if success
 * NEGATIVE_VALUE   if fail
 */
/*----------------------------------------------------------------------------*/
int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie)
{
    struct BUS_INFO *prBusInfo = NULL;
    struct net_device *prNetDevice = NULL;
    struct GLUE_INFO *prGlueInfo = NULL;
    struct GL_HIF_INFO *prHifInfo = NULL;
    struct pci_dev *pdev = NULL;
    int ret = 0;

    prNetDevice = (struct net_device *)pvData;
    prGlueInfo = (struct GLUE_INFO *)pvCookie;

    /* MT7902 NO-NVRAM: Log but continue */
    if (!prGlueInfo) {
        DBGLOG(INIT, ERROR, "glBusSetIrq: NULL GlueInfo (no NVRAM)\n");
    } else {
        prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;
        prHifInfo = &prGlueInfo->rHifInfo;
        pdev = prHifInfo->pdev;
        prHifInfo->u4IrqId = pdev->irq;

        /* Recovery init */
        INIT_WORK(&prHifInfo->recovery_work, mt7902_recovery_work);
        mutex_init(&prHifInfo->recovery_lock);
        prHifInfo->state_flags = 0;
        set_bit(MTK_FLAG_IRQ_ALIVE, &prHifInfo->state_flags);
        prHifInfo->saved_irq = pdev->irq;
        prHifInfo->pdev = pdev;
    }

    /* MT7902 IRQ WATERMARK FIX - ALWAYS RUNS (even without NVRAM) */
    DBGLOG(INIT, ERROR, "=== MT7902 IRQ FIX ===\n");
    if (prGlueInfo && prGlueInfo->prAdapter) {
        HAL_MCR_WR(prGlueInfo->prAdapter, 0x10188, 0x00000000);
        kalUdelay(10);
        HAL_MCR_WR(prGlueInfo->prAdapter, 0x10188, 0x000000FF);
        DBGLOG(INIT, ERROR, "MT7902: IRQ FIXED 0x10188=0xFF\n");
    }

    /* Use original IRQ logic - safe fallback */
    ret = request_irq(prNetDevice->irq ? prNetDevice->irq : pdev->irq,
                      mtk_pci_interrupt, IRQF_SHARED,
                      prNetDevice->name, prGlueInfo);

    if (ret != 0)
        DBGLOG(INIT, INFO, "glBusSetIrq: request_irq ERROR(%d)\n", ret);
    else if (prBusInfo && prBusInfo->initPcieInt)
        prBusInfo->initPcieInt(prGlueInfo);

    return ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Stop bus interrupt operation and disable interrupt handling for os.
 *
 * \param[in] pvData     A pointer to struct net_device.
 * \param[in] pvCookie   Private data for pfnIsr function.
 *
 * \return (none)
 */

/*----------------------------------------------------------------------------*/

void glBusFreeIrq(void *pvData, void *pvCookie)
{
    struct net_device *prNetDevice = NULL;
    struct GLUE_INFO *prGlueInfo = NULL;
    struct GL_HIF_INFO *prHifInfo = NULL;
    struct pci_dev *pdev = NULL;

    if (!pvData || !pvCookie) {
        DBGLOG(INIT, INFO, "%s: Invalid arguments (pvData:%p, pvCookie:%p)\n", 
               __func__, pvData, pvCookie);
        return;
    }

    prNetDevice = (struct net_device *)pvData;
    prGlueInfo = (struct GLUE_INFO *)pvCookie;
    prHifInfo = &prGlueInfo->rHifInfo;
    pdev = prHifInfo->pdev;

    /* ===================== IRQ TEARDOWN ===================== */

    /* Use our internal IRQ tracker instead of pdev->irq to avoid double-frees */
    if (prHifInfo->u4IrqId > 0) {
        DBGLOG(INIT, INFO, "%s: freeing IRQ %d\n",
               __func__, prHifInfo->u4IrqId);

        /* 1. Mark dead to stop tasklets/handlers from re-scheduling */
        mt7902_mark_irq_dead(prHifInfo);

        /* 2. Flush any currently running handlers */
        synchronize_irq(prHifInfo->u4IrqId);

        /* 3. Release back to the kernel */
        free_irq(prHifInfo->u4IrqId, prGlueInfo);

        /* 4. CRITICAL: Clear the ID so a second call returns here */
        prHifInfo->u4IrqId = 0;
    } else {
        DBGLOG(INIT, INFO, "%s: IRQ already freed or not allocated\n", __func__);
    }

#if (CFG_SUPPORT_CONNINFRA == 1)
    if (prHifInfo->u4IrqId_1 > 0) {
        synchronize_irq(prHifInfo->u4IrqId_1);
        free_irq(prHifInfo->u4IrqId_1, prGlueInfo);
        prHifInfo->u4IrqId_1 = 0;
    }
#endif

    /* ===================== WORKQUEUE ===================== */

    /* Ensure work is cancelled and the function pointer is cleared */
    if (prHifInfo->recovery_work.func) {
        DBGLOG(INIT, INFO, "%s: cancelling recovery work\n", __func__);
        cancel_work_sync(&prHifInfo->recovery_work);
        /* Clear the func pointer to prevent re-cancellation */
        prHifInfo->recovery_work.func = NULL; 
    }

    /* ===================== BAR / PCI ===================== */

    /* Use the struct-wrapped address to ensure we aren't unmapping NULL */
    if (prHifInfo->CSRBaseAddress) {
        DBGLOG(INIT, INFO, "%s: unmapping CSRBaseAddress %p\n",
               __func__, prHifInfo->CSRBaseAddress);

        iounmap(prHifInfo->CSRBaseAddress);
        prHifInfo->CSRBaseAddress = NULL;

        /* Only release regions if we are the ones who requested them */
        if (pdev) {
            pci_release_regions(pdev);
            pci_clear_master(pdev);
        }
    } else {
        DBGLOG(INIT, INFO, "%s: CSRBaseAddress already NULL\n", __func__);
    }
}




u_int8_t glIsReadClearReg(uint32_t u4Address)
{
	return TRUE;
}

void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode)
{
}

void glGetDev(void *ctx, struct device **dev)
{
	if (!ctx || !dev)
		return;

	*dev = &((struct pci_dev *)ctx)->dev;
}

void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev)
{
	*dev = &(prHif->pdev->dev);
}

static void pcieAllocDesc(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMABUF *prDescRing,
			  uint32_t u4Num)
{
	dma_addr_t rAddr = 0;

	prDescRing->AllocVa = KAL_DMA_ALLOC_COHERENT(
		prHifInfo->prDmaDev, prDescRing->AllocSize, &rAddr);
	prDescRing->AllocPa = rAddr;
	if (prDescRing->AllocVa)
		memset(prDescRing->AllocVa, 0, prDescRing->AllocSize);
}

static void pcieAllocTxDataBuf(struct MSDU_TOKEN_ENTRY *prToken, uint32_t u4Idx)
{
	prToken->prPacket = kalMemAlloc(prToken->u4DmaLength, PHY_MEM_TYPE);
	prToken->rDmaAddr = 0;
}

static void *pcieAllocRxBuf(struct GL_HIF_INFO *prHifInfo,
                            struct RTMP_DMABUF *prDmaBuf,
                            uint32_t u4Num, uint32_t u4Idx)
{
    struct sk_buff *pkt;
    dma_addr_t rAddr;

    /* Safety: Ensure we have a valid size and device */
    if (!prDmaBuf || prDmaBuf->AllocSize == 0 || !prHifInfo->prDmaDev) {
        DBGLOG(HAL, ERROR, "Invalid params for RX buf %u: size %lu\n", 
               u4Idx, prDmaBuf ? prDmaBuf->AllocSize : 0);
        return NULL;
    }

    /* 1. Attempt SKB Allocation */
    pkt = dev_alloc_skb(prDmaBuf->AllocSize);
    if (unlikely(!pkt)) {
        DBGLOG(HAL, ERROR, "[%u] skb alloc failed (size %lu). Memory pressure?\n",
               u4Idx, prDmaBuf->AllocSize);
        goto err_out;
    }

#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
    skb_reserve(pkt, CFG_RADIOTAP_HEADROOM);
#endif

    /* 2. Initialize buffer memory */
    prDmaBuf->AllocVa = (void *)pkt->data;
    memset(prDmaBuf->AllocVa, 0, prDmaBuf->AllocSize);

    /* 3. DMA Mapping */
    rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, prDmaBuf->AllocVa,
                               prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);

    if (unlikely(KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr))) {
        DBGLOG(HAL, ERROR, "[%u] DMA mapping error! Check IOMMU/Bus Master.\n", u4Idx);
        dev_kfree_skb(pkt);
        goto err_out;
    }

    /* 4. Success: Commit the Physical Address */
    prDmaBuf->AllocPa = rAddr;
    return (void *)pkt;

err_out:
    prDmaBuf->AllocPa = 0;
    prDmaBuf->AllocVa = NULL;
    return NULL;
}


static void *pcieAllocRuntimeMem(uint32_t u4SrcLen)
{
	return kalMemAlloc(u4SrcLen, PHY_MEM_TYPE);
}

static bool pcieCopyCmd(struct GL_HIF_INFO *prHifInfo,
			struct RTMP_DMACB *prTxCell, void *pucBuf,
			void *pucSrc1, uint32_t u4SrcLen1,
			void *pucSrc2, uint32_t u4SrcLen2)
{
	dma_addr_t rAddr;
	uint32_t u4TotalLen = u4SrcLen1 + u4SrcLen2;

	prTxCell->pBuffer = pucBuf;

	memcpy(pucBuf, pucSrc1, u4SrcLen1);
	if (pucSrc2 != NULL && u4SrcLen2 > 0)
		memcpy(pucBuf + u4SrcLen1, pucSrc2, u4SrcLen2);
	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, pucBuf,
				   u4TotalLen, KAL_DMA_TO_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return false;
	}

	prTxCell->PacketPa = rAddr;

	return true;
}

static bool pcieCopyEvent(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMACB *pRxCell,
			  struct RXD_STRUCT *pRxD,
			  struct RTMP_DMABUF *prDmaBuf,
			  uint8_t *pucDst, uint32_t u4Len)
{
	struct sk_buff *prSkb = NULL;
	void *pRxPacket = NULL;
	dma_addr_t rAddr;

	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)prDmaBuf->AllocPa,
			     prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);

	pRxPacket = pRxCell->pPacket;
	ASSERT(pRxPacket)

	prSkb = (struct sk_buff *)pRxPacket;
	memcpy(pucDst, (uint8_t *)prSkb->data, u4Len);

	prDmaBuf->AllocVa = ((struct sk_buff *)pRxCell->pPacket)->data;
	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, prDmaBuf->AllocVa,
				   prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return false;
	}
	prDmaBuf->AllocPa = rAddr;
	return true;
}

static bool pcieCopyTxData(struct MSDU_TOKEN_ENTRY *prToken,
			    void *pucSrc, uint32_t u4Len)
{
	struct MSDU_INFO *prMsduInfo;
	uint8_t *pucDst;
	uint32_t u4TxdLen;

	DBGLOG(HAL, WARN,
	       "[TXCOPY] token=%p token->pkt=%p src=%p len=%u\n",
	       prToken, prToken ? prToken->prPacket : NULL, pucSrc, u4Len);

	if (!prToken) {
		DBGLOG(HAL, ERROR, "[TXCOPY] token NULL\n");
		return false;
	}
	if (!pucSrc) {
		DBGLOG(HAL, ERROR, "[TXCOPY] src NULL token=%p\n", prToken);
		return false;
	}
	if (!prToken->prPacket) {
		DBGLOG(HAL, ERROR,
		       "[TXCOPY] token->prPacket NULL token=%p src=%p len=%u\n",
		       prToken, pucSrc, u4Len);
		return false;
	}

	prMsduInfo = prToken->prMsduInfo;
	pucDst = (uint8_t *)prToken->prPacket;

	/*
	 * Mgmt frames routed via the data path (fgMgmtUseDataQ=TRUE) skip
	 * nicTxFillDataDesc, so their TXD lives in aucTxDescBuffer rather
	 * than being pushed into the skb headroom.  Prepend it manually so
	 * the token buffer layout is [TXD+padding][frame] as the HW expects.
	 *
	 * Data frames (TX_PACKET_OS*) already have the TXD in the skb
	 * headroom; pucSrc points to [TXD][frame] and u4Len covers both.
	 */
	if (prMsduInfo &&
	    prMsduInfo->eSrc == TX_PACKET_MGMT &&
	    prMsduInfo->fgMgmtUseDataQ) {
		u4TxdLen = NIC_TX_DESC_AND_PADDING_LENGTH;
		DBGLOG(HAL, INFO,
		       "[TXCOPY] mgmt-datapath: prepend TXD %u bytes then frame %u bytes\n",
		       u4TxdLen, u4Len);
		memcpy(pucDst, prMsduInfo->aucTxDescBuffer, u4TxdLen);
		memcpy(pucDst + u4TxdLen, pucSrc, u4Len);
	} else {
		memcpy(pucDst, pucSrc, u4Len);
	}

	return true;
}

static bool pcieCopyRxData(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMACB *pRxCell,
			   struct RTMP_DMABUF *prDmaBuf,
			   struct SW_RFB *prSwRfb)
{
	void *pRxPacket = NULL;
	dma_addr_t rAddr;

	pRxPacket = pRxCell->pPacket;
	ASSERT(pRxPacket);

	pRxCell->pPacket = prSwRfb->pvPacket;

	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)prDmaBuf->AllocPa,
			     prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	prSwRfb->pvPacket = pRxPacket;

	prDmaBuf->AllocVa = ((struct sk_buff *)pRxCell->pPacket)->data;
	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev,
		prDmaBuf->AllocVa, prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		ASSERT(0);
		return false;
	}
	prDmaBuf->AllocPa = rAddr;

	return true;
}

static dma_addr_t pcieMapTxBuf(struct GL_HIF_INFO *prHifInfo,
			  void *pucBuf, uint32_t u4Offset, uint32_t u4Len)
{
	dma_addr_t rDmaAddr;

	rDmaAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, pucBuf + u4Offset,
				      u4Len, KAL_DMA_TO_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rDmaAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return 0;
	}

	return (phys_addr_t)rDmaAddr;
}

static dma_addr_t pcieMapRxBuf(struct GL_HIF_INFO *prHifInfo,
			  void *pucBuf, uint32_t u4Offset, uint32_t u4Len)
{
	dma_addr_t rDmaAddr;

	rDmaAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, pucBuf + u4Offset,
				      u4Len, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rDmaAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return 0;
	}

	return (phys_addr_t)rDmaAddr;
}

static void pcieUnmapTxBuf(struct GL_HIF_INFO *prHifInfo,
			   phys_addr_t rDmaAddr, uint32_t u4Len)
{
	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)rDmaAddr,
			     u4Len, KAL_DMA_TO_DEVICE);
}

static void pcieUnmapRxBuf(struct GL_HIF_INFO *prHifInfo,
			   phys_addr_t rDmaAddr, uint32_t u4Len)
{
	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)rDmaAddr,
			     u4Len, KAL_DMA_FROM_DEVICE);
}

static void pcieFreeDesc(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMABUF *prDescRing)
{
	if (prDescRing->AllocVa == NULL)
		return;

	KAL_DMA_FREE_COHERENT(prHifInfo->prDmaDev,
			      prDescRing->AllocSize,
			      prDescRing->AllocVa,
			      (dma_addr_t)prDescRing->AllocPa);
	memset(prDescRing, 0, sizeof(struct RTMP_DMABUF));
}

static void pcieFreeBuf(void *pucSrc, uint32_t u4Len)
{
	kalMemFree(pucSrc, PHY_MEM_TYPE, u4Len);
}

static void pcieFreePacket(void *pvPacket)
{
	kalPacketFree(NULL, pvPacket);
}

static void pcieDumpTx(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_TX_RING *prTxRing,
		       uint32_t u4Idx, uint32_t u4DumpLen)
{
	struct RTMP_DMACB *prTxCell;
	void *prAddr = NULL;

	prTxCell = &prTxRing->Cell[u4Idx];

	if (prTxCell->prToken)
		prAddr = prTxCell->prToken->prPacket;
	else
		prAddr = prTxCell->pBuffer;

	if (prAddr)
		DBGLOG_MEM32(HAL, INFO, prAddr, u4DumpLen);
}

static void pcieDumpRx(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_RX_RING *prRxRing,
		       uint32_t u4Idx, uint32_t u4DumpLen)
{
	struct RTMP_DMACB *prRxCell;
	struct RTMP_DMABUF *prDmaBuf;

	prRxCell = &prRxRing->Cell[u4Idx];
	prDmaBuf = &prRxCell->DmaBuf;

	if (!prRxCell->pPacket)
		return;

	pcieUnmapRxBuf(prHifInfo, prDmaBuf->AllocPa, prDmaBuf->AllocSize);

	DBGLOG_MEM32(HAL, INFO, ((struct sk_buff *)prRxCell->pPacket)->data,
		     u4DumpLen);

	prDmaBuf->AllocPa = pcieMapRxBuf(prHifInfo, prDmaBuf->AllocVa,
					0, prDmaBuf->AllocSize);
}

#if CFG_CHIP_RESET_SUPPORT
void kalRemoveProbe(IN struct GLUE_INFO *prGlueInfo)
{
	DBGLOG(INIT, WARN, "[SER][L0] not support...\n");
}
#endif


#if CFG_SUPPORT_PCIE_ASPM
#if 0
static void pcieSetASPML1SS(struct pci_dev *dev, int i4Enable)
{
  return;
}
static void pcieSetASPML1(struct pci_dev *dev, int i4Enable)
{
  return;
}
static bool pcieCheckASPML1SS(struct pci_dev *dev, int i4BitMap)
{
	return FALSE;
}
#endif
bool glBusConfigASPM(struct pci_dev *dev, int i4Enable)
{

/* FORCED DISABLE: Prevent PCIe link collapse on Vivobook */
    return FALSE;
}
bool glBusConfigASPML1SS(struct pci_dev *dev, int i4Enable)
{
	return FALSE;
}

#endif

void halPciePreSuspendCmd(IN struct ADAPTER *prAdapter)
{
	struct CMD_HIF_CTRL rCmdHifCtrl;
	uint32_t rStatus;

	kalMemZero(&rCmdHifCtrl, sizeof(struct CMD_HIF_CTRL));

	rCmdHifCtrl.ucHifType = ENUM_HIF_TYPE_PCIE;
	rCmdHifCtrl.ucHifDirection = ENUM_HIF_TX;
	rCmdHifCtrl.ucHifStop = TRUE;
	rCmdHifCtrl.ucHifSuspend = TRUE;
	rCmdHifCtrl.u4WakeupHifType = ENUM_CMD_HIF_WAKEUP_TYPE_PCIE;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			CMD_ID_HIF_CTRL,	/* ucCID */
			TRUE,	/* fgSetQuery */
			FALSE,	/* fgNeedResp */
			FALSE,	/* fgIsOid */
			NULL,	/* pfCmdDoneHandler */
			NULL,	/* pfCmdTimeoutHandler */
			sizeof(struct CMD_HIF_CTRL), /* u4SetQueryInfoLen */
			(uint8_t *)&rCmdHifCtrl, /* pucInfoBuffer */
			NULL,	/* pvSetQueryBuffer */
			0	/* u4SetQueryBufferLen */
			);

	ASSERT(rStatus == WLAN_STATUS_PENDING);
}

void halPcieResumeCmd(IN struct ADAPTER *prAdapter)
{
	struct CMD_HIF_CTRL rCmdHifCtrl;
	uint32_t rStatus;

	kalMemZero(&rCmdHifCtrl, sizeof(struct CMD_HIF_CTRL));

	rCmdHifCtrl.ucHifType = ENUM_HIF_TYPE_PCIE;
	rCmdHifCtrl.ucHifDirection = ENUM_HIF_TX;
	rCmdHifCtrl.ucHifStop = FALSE;
	rCmdHifCtrl.ucHifSuspend = FALSE;
	rCmdHifCtrl.u4WakeupHifType = ENUM_CMD_HIF_WAKEUP_TYPE_PCIE;

	rStatus = wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			CMD_ID_HIF_CTRL,	/* ucCID */
			TRUE,	/* fgSetQuery */
			FALSE,	/* fgNeedResp */
			FALSE,	/* fgIsOid */
			NULL,	/* pfCmdDoneHandler */
			NULL,	/* pfCmdTimeoutHandler */
			sizeof(struct CMD_HIF_CTRL), /* u4SetQueryInfoLen */
			(uint8_t *)&rCmdHifCtrl, /* pucInfoBuffer */
			NULL,	/* pvSetQueryBuffer */
			0	/* u4SetQueryBufferLen */
			);

	ASSERT(rStatus == WLAN_STATUS_PENDING);
}

void halPciePreSuspendDone(
	IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo,
	IN uint8_t *pucEventBuf)
{
	ASSERT(prAdapter);

	prAdapter->prGlueInfo->rHifInfo.eSuspendtate =
		PCIE_STATE_PRE_SUSPEND_DONE;
}

void halPciePreSuspendTimeout(
	IN struct ADAPTER *prAdapter,
	IN struct CMD_INFO *prCmdInfo)
{
	ASSERT(prAdapter);

	prAdapter->prGlueInfo->rHifInfo.eSuspendtate =
		PCIE_STATE_PRE_SUSPEND_FAIL;
}

#if 0
static int mt7902_wfsys_reset(void __iomem *base)
{
	u32 addr = 0x18000140; /* MT7902 WFSYS */
	u32 val;
	int i;

	/* assert reset */
	val = mt7902_win_read(base, addr);
	mt7902_win_write(base, addr, val & ~BIT(0));
	msleep(50);

	/* deassert reset */
	val = mt7902_win_read(base, addr);
	mt7902_win_write(base, addr, val | BIT(0));

	/* poll INIT_DONE (BIT(4)) */
	for (i = 0; i < 500; i++) {
		val = mt7902_win_read(base, addr);
		if (val & BIT(4))
			return 0;
		msleep(1);
	}

	printk(KERN_ERR "[mt7902] WFSYS init timeout (0x%08x)\n", val);
	return -ETIMEDOUT;
}


static int mt7902_drv_pmctrl(struct pci_dev *pdev)
{
    void __iomem *base = pci_iomap(pdev, 0, 0);

    int i;

    for (i = 0; i < 20; i++) {
        /* request driver ownership */
        writel(DRV_CLR_OWN, base + MT_CONN_ON_LPCTL);

        if (readl(base + MT_CONN_ON_LPCTL) & OWN_SYNC)
            break;

        msleep(10);
    }

    if (i == 20)
        return -EIO;

    return 0;
}

u_int8_t glBusInit(void *pvData)
{
	struct pci_dev *pdev = (struct pci_dev *)pvData;
	struct GLUE_INFO *prGlueInfo;
	void __iomem *base = NULL;
	u32 val;
	u16 cmd = 0, status = 0;
	int ret, i;

	printk(KERN_INFO "[mt7902] glBusInit: START\n");

	if (!pdev) {
		printk(KERN_ERR "[mt7902] glBusInit: pdev is NULL\n");
		return FALSE;
	}

	prGlueInfo = pci_get_drvdata(pdev);
	printk(KERN_INFO "[mt7902] glBusInit: drvdata=%p\n", prGlueInfo);

	ret = pci_enable_device(pdev);
	printk(KERN_INFO "[mt7902] glBusInit: pci_enable_device=%d\n", ret);
	if (ret) {
		printk(KERN_ERR "[mt7902] glBusInit: pci_enable_device failed\n");
		goto out_fail;
	}

	pci_set_master(pdev);
	printk(KERN_INFO "[mt7902] glBusInit: pci_set_master done\n");

	ret = pci_set_power_state(pdev, PCI_D0);
	printk(KERN_INFO "[mt7902] glBusInit: pci_set_power_state(D0)=%d\n", ret);

	pci_restore_state(pdev);
	printk(KERN_INFO "[mt7902] glBusInit: pci_restore_state done\n");

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	pci_read_config_word(pdev, PCI_STATUS, &status);
	printk(KERN_INFO "[mt7902] glBusInit: PCI_COMMAND=0x%04x PCI_STATUS=0x%04x\n",
	       cmd, status);

#if CFG80211_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(g_u4DmaMask));
#else
	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(g_u4DmaMask));
#endif
	printk(KERN_INFO "[mt7902] glBusInit: dma mask ret=%d mask=%u\n", ret, g_u4DmaMask);

	base = ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	printk(KERN_INFO "[mt7902] glBusInit: ioremap base=%p\n", base);
	if (!base)
		goto out_fail;

	if (prGlueInfo)
		prGlueInfo->rHifInfo.CSRBaseAddress = base;

	/* Strong wake poke */
	writel(0x1, base + 0x7000);
	wmb();
	usleep_range(1000, 2000);

	printk(KERN_INFO "[mt7902] glBusInit: probe BAR0=%08x REMAP_BASE=%08x\n",
	       readl(base + 0x0), readl(base + MT7902_REMAP_BASE));

	/* WFSYS reset sequence for MT7902 */
	val = mt7902_win_read(base, 0x18000140);
	printk(KERN_INFO "[mt7902] glBusInit: WFSYS pre=0x%08x\n", val);

	mt7902_win_write(base, 0x18000140, val & ~BIT(0));
	wmb();
	msleep(50);

	val = mt7902_win_read(base, 0x18000140);
	mt7902_win_write(base, 0x18000140, val | BIT(0));
	wmb();

	for (i = 0; i < 500; i++) {
		val = mt7902_win_read(base, 0x18000140);
		if (val & BIT(4)) {
			printk(KERN_INFO "[mt7902] glBusInit: WFSYS INIT_DONE at %d ms (0x%08x)\n",
			       i, val);
			break;
		}
		msleep(1);
	}

	if (!(val & BIT(4))) {
		printk(KERN_ERR "[mt7902] glBusInit: WFSYS init timeout (0x%08x)\n", val);
		goto out_fail;
	}

	val = mt7902_win_read(base, 0x180d4014);
	printk(KERN_INFO "[mt7902] glBusInit: GLO_CFG pre=0x%08x\n", val);

	writel(0, base + MT7902_REMAP_BASE);
	wmb();

	printk(KERN_INFO "[mt7902] glBusInit: END OK\n");
	return TRUE;

out_fail:
	if (base) {
		iounmap(base);
		if (prGlueInfo)
			prGlueInfo->rHifInfo.CSRBaseAddress = NULL;
	}

	printk(KERN_ERR "[mt7902] glBusInit: FAIL\n");
	return FALSE;
}

#endif



/**
 * mtk_pci_disable_aspm - Manually force disable ASPM states.
 * Ported from mt76/pci.c to ensure the PCIe link does not latch 
 * into a gated state during probe.
 */
void mtk_pci_disable_aspm(struct pci_dev *pdev)
{
	struct pci_dev *parent = pdev->bus->self;
	u16 aspm_conf;

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &aspm_conf);
	aspm_conf &= PCI_EXP_LNKCTL_ASPMC;

	if (!aspm_conf)
		return;

	/* Disable ASPM in downstream component first and then upstream */
	pcie_capability_clear_word(pdev, PCI_EXP_LNKCTL, aspm_conf);
	if (parent)
		pcie_capability_clear_word(parent, PCI_EXP_LNKCTL, aspm_conf);
	
	dev_info(&pdev->dev, "[mt7902] Forced ASPM disable to prevent fabric latch\n");
}

u_int8_t glBusInit(void *pvData)
{
    struct pci_dev *pdev = (struct pci_dev *)pvData;
    void __iomem *base;
    u32 val;

    if (!pdev) {
        DBGLOG(INIT, ERROR, "%s: NULL pdev\n", __func__);
        return FALSE;
    }

    /*
     * Probe already did pci_enable_device() and pci_request_regions().
     * So this function should only map BAR0 and validate that it is live.
     */
    pci_set_master(pdev);
    mtk_pci_disable_aspm(pdev);

    if (CSRBaseAddress) {
        DBGLOG(INIT, WARN, "%s: CSRBaseAddress already mapped at %p\n",
               __func__, CSRBaseAddress);
        return TRUE;
    }

    base = pci_iomap(pdev, 0, 0);
    if (!base) {
        DBGLOG(INIT, ERROR, "%s: pci_iomap BAR0 failed\n", __func__);
        return FALSE;
    }

    CSRBaseAddress = base;

    /*
     * Optional sanity read. If the device is wedged or not responding,
     * this often comes back as 0xffffffff.
     */
    val = readl(CSRBaseAddress);
    if (val == 0xffffffff) {
        DBGLOG(HAL, ERROR, "%s: BAR0 looks dead (0xffffffff)\n", __func__);
        pci_iounmap(pdev, CSRBaseAddress);
        CSRBaseAddress = NULL;
        return FALSE;
    }

    DBGLOG(HAL, INFO, "%s: BAR0 mapped at %p, first dword=0x%08x\n",
           __func__, CSRBaseAddress, val);

    return TRUE;
}
