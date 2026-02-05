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

#include "hif_pdma.h"
#include <linux/pm_runtime.h>
#include <linux/workqueue.h> /* Added for V2 recovery fix */

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
static uint32_t g_u4DmaMask = 32;

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
	
	if (!prHifInfo || !prHifInfo->CSRBaseAddress)
		return TRUE;
	
	/* Read a safe register - chip ID at offset 0x0 */
	u4Val = readl(prHifInfo->CSRBaseAddress + 0x0);
	
	return (u4Val == 0xFFFFFFFF);
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
/*----------------------------------------------------------------------------*/
/*!
 * \brief Schedule recovery from atomic/IRQ/tasklet context
 *
 * \param[in] func  pointer to PCIE handle
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/
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

    DBGLOG(HAL, INFO, "=== Recovery work starting (process context) ===\n");
    mt79xx_pci_function_recover(prHifInfo->pdev, prGlueInfo);
    
    /* Clean up flags after recovery */
    /* MTK_FLAG_MMIO_GONE is cleared in mt79xx_pci_function_recover on success */
    
    mutex_unlock(&prHifInfo->recovery_lock);
}


static void *CSRBaseAddress;

static irqreturn_t mtk_pci_interrupt(int irq, void *dev_instance)
{
	struct GLUE_INFO *prGlueInfo = NULL;
    struct GL_HIF_INFO *prHifInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) dev_instance;
	if (!prGlueInfo) {
		DBGLOG(HAL, INFO, "No glue info in mtk_pci_interrupt()\n");
		return IRQ_NONE;
	}
    
    prHifInfo = &prGlueInfo->rHifInfo;

    /* CRITICAL: Check MMIO before touching hardware (V2 Fix) */
    if (mt7902_mmio_dead(prHifInfo)) {
        DBGLOG(HAL, ERROR, "MMIO dead in ISR - scheduling recovery\n");
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return IRQ_HANDLED;
    }

	halDisableInterrupt(prGlueInfo->prAdapter);

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(HAL, INFO, "GLUE_FLAG_HALT skip INT\n");
		return IRQ_NONE;
	}

	kalSetIntEvent(prGlueInfo);

	return IRQ_HANDLED;
}

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
static int mtk_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret = 0;
	struct mt66xx_chip_info *prChipInfo;
	
	ASSERT(pdev);
	ASSERT(id);
	
	ret = pci_enable_device(pdev);
	if (ret) {
		DBGLOG(INIT, INFO, "pci_enable_device failed!\n");
		goto out;
	}
	
	/* Manual ASPM Disable because we are an out-of-tree module */
	pr_info("MT7902: Disabling ASPM L1/L1.1/L1.2 manually in probe\n");
	pci_disable_link_state(pdev, PCIE_LINK_STATE_L1 | 
	                             PCIE_LINK_STATE_L1_1 | 
	                             PCIE_LINK_STATE_L1_2);
	
#if defined(SOC3_0)
	if ((void *)&mt66xx_driver_data_soc3_0 == (void *)id->driver_data)
		DBGLOG(INIT, INFO,
			"[MJ]&mt66xx_driver_data_soc3_0 == id->driver_data\n");
#endif
	
	DBGLOG(INIT, INFO, "pci_enable_device done!\n");
	
	prChipInfo = ((struct mt66xx_hif_driver_data *)
				id->driver_data)->chip_info;
	prChipInfo->pdev = (void *)pdev;
	
	/* Always call the probe path and set global 'probed' only on success */
	if (pfWlanProbe((void *) pdev, (void *) id->driver_data) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "pfWlanProbe fail!call pfWlanRemove()\n");
		pfWlanRemove();
		ret = -1;
	} else {
		g_fgDriverProbed = TRUE;
		g_u4DmaMask = prChipInfo->bus_info->u4DmaMask;
		/* Clear initial cold boot flag - probe completed successfully */
		g_fgInInitialColdBoot = FALSE;
		DBGLOG(INIT, INFO, "Initial cold boot complete, WFDMA polling now enabled\n");
	}
	
out:
	DBGLOG(INIT, INFO, "mtk_pci_probe() done(%d)\n", ret);
	return ret;
}


static void mtk_pci_remove(struct pci_dev *pdev)
{
	ASSERT(pdev);

	if (g_fgDriverProbed)
		pfWlanRemove();
	DBGLOG(INIT, INFO, "pfWlanRemove done\n");
	
	/* Reset the flag in case module is reloaded */
	g_fgInInitialColdBoot = TRUE;

	/* Unmap CSR base address */
	iounmap(CSRBaseAddress);

	/* release memory region */
	pci_release_regions(pdev);

	pci_disable_device(pdev);
	pci_disable_msi(pdev);
	DBGLOG(INIT, INFO, "mtk_pci_remove() done\n");
}


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

	prHif = &prGlueInfo->rHifInfo;

	prHif->pdev = (struct pci_dev *)ulCookie;
	prMemOps = &prHif->rMemOps;
	prHif->prDmaDev = prHif->pdev;

	prHif->CSRBaseAddress = CSRBaseAddress;

	pci_set_drvdata(prHif->pdev, prGlueInfo);

	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &prHif->pdev->dev);

	/* Disable runtime PM — MTK firmware assumes the platform never does
	 * surprise PM. On Android the WiFi service owns this. On Linux, nothing
	 * does. This card draws ~2mW idle anyway. */
	pm_runtime_forbid(&prHif->pdev->dev);
	pm_runtime_set_active(&prHif->pdev->dev);
	/* Force ASPM off - link state transitions can wedge PDMA during reset */
	glBusConfigASPM(prHif->pdev, 0);

	prGlueInfo->u4InfType = MT_DEV_INF_PCIE;

	prHif->rErrRecoveryCtl.eErrRecovState = ERR_RECOV_STOP_IDLE;
	prHif->rErrRecoveryCtl.u4Status = 0;

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	timer_setup(&prHif->rSerTimer, halHwRecoveryTimeout, 0);
#else
	init_timer(&prHif->rSerTimer);
	prHif->rSerTimer.function = halHwRecoveryTimeout;
	prHif->rSerTimer.data = (unsigned long)prGlueInfo;
	prHif->rSerTimer.expires =
		jiffies + HIF_SER_TIMEOUT * HZ / MSEC_PER_SEC;
#endif

	INIT_LIST_HEAD(&prHif->rTxCmdQ);
	INIT_LIST_HEAD(&prHif->rTxDataQ);
	prHif->u4TxDataQLen = 0;

	prHif->fgIsPowerOff = true;
	prHif->fgIsDumpLog  = false;
	pcie_recovery_debugfs_set_hif(prHif);
	prHif->fgIsDumpLog = false;

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
    /* CRITICAL V2 FIX: Check if called from atomic context */
    if (in_atomic() || in_interrupt() || irqs_disabled()) {
        DBGLOG(HAL, ERROR, 
            "Recovery called in atomic context - scheduling workqueue\n");
        mt7902_schedule_recovery_from_atomic(prGlueInfo);
        return WLAN_STATUS_PENDING;
    }
    
    DBGLOG(HAL, INFO, "Recovery in process context - safe to sleep\n");

	struct ADAPTER *prAdapter;
	struct mt66xx_chip_info *prChipInfo;
	struct REG_INFO *prRegInfo;
	uint32_t u4Status;
	struct GL_HIF_INFO *prHifInfo;
	uint32_t u4Val;
	int pm_ret;

	/* Tier-4: SAFETY FIRST - Silence the Bus Immediately */
	/* This prevents the "Hold Power Button" hard lock */
	if (pdev) {
		pci_clear_master(pdev); /* Stop DMA transactions */
		pci_disable_msi(pdev);  /* Stop Interrupts */
	}
    
    prHifInfo = &prGlueInfo->rHifInfo;

    /* CRITICAL V2 FIX: Mark IRQ as dead BEFORE any further PCI operations */
    mt7902_mark_irq_dead(prHifInfo);

	if (!pdev || !prGlueInfo) {
		DBGLOG(HAL, ERROR, "Tier-3 recovery: NULL parameters\n");
		return WLAN_STATUS_FAILURE;
	}

	prAdapter = prGlueInfo->prAdapter;
	prChipInfo = prAdapter->chip_info;

	/* Prevent concurrent recovery attempts */
	if (prHifInfo->fgInPciRecovery) {
		DBGLOG(HAL, WARN, "Tier-3 recovery already in progress\n");
		return WLAN_STATUS_FAILURE;
	}

	prHifInfo->fgInPciRecovery = TRUE;
	prHifInfo->fgMmioGone = true;
	prHifInfo->u8RecoveryStage      = RECOV_STAGE_QUIESCE;
	prHifInfo->u8RecoveryFailReason = RECOV_FAIL_NONE;
	prHifInfo->u4LastBarSample      = 0;
	prHifInfo->u_recovery_fail_time = 0;

	DBGLOG(HAL, WARN, "=== Tier-3 PCIe Recovery Start (Safe Mode) ===\n");

	/* Tier-4.5: Diplomatic Wake (NOW it is safe to try this) */
	pm_ret = pm_runtime_get_sync(&pdev->dev);
	pci_set_power_state(pdev, PCI_D0);
	
	/* Force disable ASPM to prevent link entry/exit issues during reset */
	pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 | PCIE_LINK_STATE_CLKPM);
	
	DBGLOG(HAL, WARN, "Tier-4.5: PM Wake (ret %d). ASPM Disabled. Bus Master Cleared.\n", pm_ret);

	/* Force-clear the MMIO mapping to stop pending CPU transactions */
	if (prHifInfo->CSRBaseAddress) {
		iounmap(prHifInfo->CSRBaseAddress);
		prHifInfo->CSRBaseAddress = NULL;
	}

	/* Step 1: Stop upper layer activity */
	DBGLOG(HAL, INFO, "Tier-3: Stopping network queues\n");
    /* V2 FIX: Removed unsafe kernel quiesce block that contained synchronize_irq() */
	netif_tx_stop_all_queues(prGlueInfo->prDevHandler);

	/* Step 2: Disable interrupts */
    /* V2 FIX: Use nosync since IRQ might be dead */
    DBGLOG(HAL, WARN, "Disabling IRQs (no sync - IRQ is dead)\n");
	if (prHifInfo->u4IrqId) {
		disable_irq_nosync(prHifInfo->u4IrqId);
	}

	/* Step 3: Tear down PCIe function state */
	pci_disable_device(pdev);

	/* Step 4: Force device to D0 power state (Deep Sleep Exit) */
	pci_set_power_state(pdev, PCI_D0);
	msleep(100);

	/* Step 5: Re-enable PCIe function */
	/* Tier-6: ACPI D3Cold Power Cycle */
	DBGLOG(HAL, WARN, "Tier-6: Triggering ACPI D3cold power-cycle\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_D3COLD_CYCLE;
	pci_save_state(pdev);
	pci_set_power_state(pdev, PCI_D3cold);
	msleep(200);
	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	u4Status = pci_enable_device(pdev);
	if (u4Status != 0) {
		DBGLOG(HAL, ERROR, "Tier-3: pci_enable_device failed: %d\n", u4Status);
		prHifInfo->u8RecoveryFailReason = RECOV_FAIL_ENABLE_DEVICE;
		goto recovery_failed;
	}
	prHifInfo->u8RecoveryStage = RECOV_STAGE_REENABLE;

	/* Step 6: Restore PCI state */
	pci_restore_state(pdev);
	
	/* Step 7: Re-map MMIO BAR space */
	DBGLOG(HAL, WARN, "Tier-3: Re-mapping MMIO BAR\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_BAR_REMAP;
	CSRBaseAddress = ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	if (!CSRBaseAddress) {
		DBGLOG(HAL, ERROR, "Tier-3: ioremap failed\n");
		prHifInfo->u8RecoveryFailReason = RECOV_FAIL_IOREMAP;
		goto recovery_failed;
	}
	/* Update the global handle so macros work */
	prHifInfo->CSRBaseAddress = CSRBaseAddress;

	/* Tier-4: Manual Secondary Bus Reset (SBR) */
	prHifInfo->u8RecoveryStage = RECOV_STAGE_SBR;
	if (pdev->bus && pdev->bus->self) {
		uint16_t ctrl;
		struct pci_dev *bridge = pdev->bus->self;
		DBGLOG(HAL, WARN, "Tier-4: Forcing Manual SBR via Bridge %04x:%02x\n", 
			pci_domain_nr(bridge->bus), bridge->devfn);
		pci_read_config_word(bridge, PCI_BRIDGE_CONTROL, &ctrl);
		ctrl |= PCI_BRIDGE_CTL_BUS_RESET;
		pci_write_config_word(bridge, PCI_BRIDGE_CONTROL, ctrl);
		msleep(100); 
		ctrl &= ~PCI_BRIDGE_CTL_BUS_RESET;
		pci_write_config_word(bridge, PCI_BRIDGE_CONTROL, ctrl);
		msleep(200);
		pci_restore_state(pdev);
	}

	/* Tier-3.5: Platform Reset via PERST# */
	DBGLOG(HAL, WARN, "Tier-3: Requesting PCIe Fundamental Reset\n");
	if (pci_reset_function(pdev) != 0) {
		if (pdev->bus->self)
			pci_bridge_secondary_bus_reset(pdev->bus->self);
	}
	msleep(100);
	pci_restore_state(pdev);

	/* Before any HAL_MCR_WR */
	prHifInfo->u8RecoveryStage = RECOV_STAGE_BAR_PREFLIGHT;
	HAL_MCR_RD(prAdapter, 0x18000000, &u4Val);
	prHifInfo->u4LastBarSample = u4Val;
	if (u4Val == 0xffffffff) {
	    dump_pci_state(pdev);
	    DBGLOG(HAL, ERROR, "BAR blind before any WFSYS writes - aborting\n");
	    prHifInfo->u8RecoveryFailReason = RECOV_FAIL_BAR_BLIND_PRE;
	    goto recovery_failed;
	}

	/* Tier-3.5: Force WFSYS Power Domain Cycle */
	DBGLOG(HAL, WARN, "Tier-3: Power-cycling WFSYS domain\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_WFSYS_PWRCYCLE;
	HAL_MCR_RD(prAdapter, 0x18000100, &u4Val);
	u4Val |= BIT(3);
	HAL_MCR_WR(prAdapter, 0x18000100, u4Val);
	mdelay(50);
	u4Val &= ~BIT(3);
	HAL_MCR_WR(prAdapter, 0x18000100, u4Val);
	mdelay(100);

	/* Tier-5: Forcing WFSYS clocks and performing hard reset */
	DBGLOG(HAL, WARN, "Tier-5: Forcing WFSYS clocks and performing hard reset\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_CLOCKS;
	HAL_MCR_WR(prAdapter, 0x18000100, 0x00010001);
	udelay(500);
	HAL_MCR_WR(prAdapter, 0x18000100, 0x00010000);
	msleep(100);
	HAL_MCR_WR(prAdapter, 0x18000100, 0x00000000);
	msleep(50);
	
	/* Reset Latch Toggle */
	HAL_MCR_RD(prAdapter, 0x18000100, &u4Val);
	u4Val |= BIT(0);
	HAL_MCR_WR(prAdapter, 0x18000100, u4Val);
	udelay(100);
	u4Val &= ~BIT(0);
	HAL_MCR_WR(prAdapter, 0x18000100, u4Val);
	msleep(50);

	/* Wait for Link Training */
	prHifInfo->u8RecoveryStage = RECOV_STAGE_LINK_WAIT;
	{
		uint16_t lnksta;
		int retry = 50;
		while (retry--) {
			pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &lnksta);
			if (!(lnksta & PCI_EXP_LNKSTA_LT)) break;
			mdelay(10);
		}
		DBGLOG(HAL, WARN, "Tier-4: Link stable (LnkSta: 0x%04x)\n", lnksta);
	}

	/* Tier-4.7: Quiesce and Phased Clock Wake */
	DBGLOG(HAL, WARN, "Tier-4: Phased WFSYS wake-up sequence\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_PHASED_WAKE;
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x100, 0x00000000);
	udelay(50);
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x158, 0xFFFFFFFF);
	mdelay(10);
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x108, 0x00000000);
	msleep(20);

	/* Tier-6: Purging DMA engine */
	prHifInfo->u8RecoveryStage = RECOV_STAGE_DMA_PURGE;
	HAL_MCR_WR(prAdapter, 0x1802b000 + 0x008, 0x00000001);
	HAL_MCR_WR(prAdapter, 0x1802b000 + 0x010, 0x00000000);
	udelay(500);
	HAL_MCR_WR(prAdapter, 0x1802b000 + 0x008, 0x00000000);
	msleep(100);

	/* Final MCU Kickstart */
	prHifInfo->u8RecoveryStage = RECOV_STAGE_MCU_KICKSTART;
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x150, 0x00000001);
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x100, 0x00000000);
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x108, 0x00000000);
	mdelay(20);
	HAL_MCR_RD(prAdapter, 0x18000000 + 0x150, &u4Val);
	u4Val |= BIT(0);
	HAL_MCR_WR(prAdapter, 0x18000000 + 0x150, u4Val);

	/* Tier-7: BAR Re-Sync / Link Verification */
	DBGLOG(HAL, WARN, "Tier-7: Verifying BAR visibility before MCU boot\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_BAR_TIER7;
	HAL_MCR_RD(prAdapter, 0x18000000, &u4Val);
	prHifInfo->u4LastBarSample = u4Val;
	if (u4Val == 0xffffffff) {
	    uint16_t cmd;
	    DBGLOG(HAL, ERROR, "Tier-7: BAR is blind (0xffffffff). Attempting Command Register Kick.\n");
	    pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	    pci_write_config_word(pdev, PCI_COMMAND, cmd & ~PCI_COMMAND_MEMORY);
	    udelay(100);
	    pci_write_config_word(pdev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
	    msleep(50);

	    /* Check again */
	    HAL_MCR_RD(prAdapter, 0x18000000, &u4Val);
	    prHifInfo->u4LastBarSample = u4Val;
	    if (u4Val == 0xffffffff) {
		DBGLOG(HAL, ERROR, "Tier-7: Hardware is physically detached. Aborting to prevent lockup.\n");
		prHifInfo->u8RecoveryFailReason = RECOV_FAIL_BAR_BLIND_TIER7;
		goto recovery_failed;
	    }
	}
	DBGLOG(HAL, WARN, "Tier-7: BAR is alive. Proceeding to MCU boot.\n");

	DBGLOG(HAL, WARN, "Tier-3: Performing WFSYS MCU cold boot\n");
	prAdapter->fgIsFwOwn = TRUE;
	prHifInfo->u8RecoveryStage = RECOV_STAGE_MCU_COLDBOOT;

	/* CRITICAL FIX: During recovery cold boot, we CAN poll WFDMA because 
	 * fabric should be properly reset by this point (we did D3cold, SBR, etc.)
	 * Unlike initial cold boot where fabric is uninitialized, here we're
	 * recovering from a known state and WFDMA polling is appropriate.
	 */
	g_fgInInitialColdBoot = FALSE;
	DBGLOG(HAL, INFO, "Recovery context: enabling WFDMA polling for cold boot\n");

	{
		int coldboot_ret = mt79xx_wfsys_cold_boot_and_wait(prAdapter);
		if (coldboot_ret != 0) {
			DBGLOG(HAL, ERROR, "Tier-3: MCU cold boot failed (ret=%d)\n", coldboot_ret);
			if (coldboot_ret == -EIO)
				prHifInfo->u8RecoveryFailReason = RECOV_FAIL_COLDBOOT_BLIND;
			else if (coldboot_ret == -ETIMEDOUT)
				prHifInfo->u8RecoveryFailReason = RECOV_FAIL_COLDBOOT_TIMEOUT;
			else
				prHifInfo->u8RecoveryFailReason = RECOV_FAIL_COLDBOOT_OTHER;
			goto recovery_failed;
		}
	}
	DBGLOG(HAL, INFO, "Tier-3: MCU is alive and initialized\n");
	
	/* NOW it is safe to re-enable Bus Mastering */
	pci_set_master(pdev);
	pm_runtime_put_sync(&pdev->dev);

	DBGLOG(HAL, WARN, "Tier-3: Re-initializing adapter\n");
	prHifInfo->u8RecoveryStage = RECOV_STAGE_ADAPTER_START;
	prRegInfo = &prGlueInfo->rRegInfo;

	u4Status = wlanAdapterStart(prAdapter, prRegInfo, FALSE);
	if (u4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(HAL, ERROR, "Tier-3: wlanAdapterStart failed: 0x%x\n", u4Status);
		prHifInfo->u8RecoveryFailReason = RECOV_FAIL_ADAPTER_START;
		goto recovery_failed;
	}

	if (prHifInfo->u4IrqId) {
		enable_irq(prHifInfo->u4IrqId);
	}
    
    /* V2 FIX: Mark IRQ as alive again after successful recovery */
    set_bit(MTK_FLAG_IRQ_ALIVE, &prHifInfo->state_flags);
    clear_bit(MTK_FLAG_MMIO_GONE, &prHifInfo->state_flags);
    
	netif_tx_wake_all_queues(prGlueInfo->prDevHandler);

	prHifInfo->fgInPciRecovery = FALSE;
	prHifInfo->fgMmioGone = FALSE;
	prHifInfo->u8RecoveryStage = RECOV_STAGE_COMPLETE;

	DBGLOG(HAL, WARN, "=== Tier-3 PCIe Recovery Complete ===\n");
	return WLAN_STATUS_SUCCESS;

recovery_failed:
	prHifInfo->u_recovery_fail_time = jiffies;
	DBGLOG(HAL, ERROR,
		"=== Tier-3 PCIe Recovery FAILED === stage=%u reason=%u lastBAR=0x%08x\n",
		(unsigned)prHifInfo->u8RecoveryStage,
		(unsigned)prHifInfo->u8RecoveryFailReason,
		prHifInfo->u4LastBarSample);

	/* Full-state dump — gives us PCI config + AER, mailbox words,
	 * and PDMA ring indices in a single dmesg burst.  Each helper
	 * guards itself against NULL CSRBaseAddress.
	 */
	dump_pci_state(pdev);
	dump_mailbox(prAdapter);
	dump_pdma_state(prAdapter);

	prHifInfo->fgInPciRecovery = FALSE;
	return WLAN_STATUS_FAILURE;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize bus operation and hif related information, request
 * resources.
 *
 * \param[out] pvData    A pointer to HIF-specific data type buffer.
 * For eHPI, pvData is a pointer to UINT_32 type and
 * stores a mapped base address.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
u_int8_t glBusInit(void *pvData)
{
	int ret = 0;
	struct pci_dev *pdev = NULL;

	ASSERT(pvData);

	pdev = (struct pci_dev *)pvData;

#if CFG80211_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(g_u4DmaMask));
#else
	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(g_u4DmaMask));
#endif
	if (ret != 0) {
		DBGLOG(INIT, INFO, "set DMA mask failed!errno=%d\n", ret);
		return FALSE;
	}

	ret = pci_request_regions(pdev, pci_name(pdev));
	if (ret != 0) {
		DBGLOG(INIT, INFO,
			"Request PCI resource failed, errno=%d!\n", ret);
	}

	/* map physical address to virtual address for accessing register */
	CSRBaseAddress = ioremap(pci_resource_start(pdev, 0),
		pci_resource_len(pdev, 0));
	DBGLOG(INIT, INFO, "ioremap for device %s, region 0x%lX @ 0x%lX\n",
		pci_name(pdev), (unsigned long) pci_resource_len(pdev, 0),
		(unsigned long) pci_resource_start(pdev, 0));
	if (!CSRBaseAddress) {
		DBGLOG(INIT, INFO,
			"ioremap failed for device %s, region 0x%lX @ 0x%lX\n",
			pci_name(pdev),
			(unsigned long) pci_resource_len(pdev, 0),
			(unsigned long) pci_resource_start(pdev, 0));
		goto err_out_free_res;
	}

#if CFG_CONTROL_ASPM_BY_FW
#if CFG_SUPPORT_PCIE_ASPM
	glBusConfigASPM(pdev,
			DISABLE_ASPM_L1);
	glBusConfigASPML1SS(pdev,
		PCI_L1PM_CTR1_ASPM_L12_EN |
		PCI_L1PM_CTR1_ASPM_L11_EN);
	glBusConfigASPM(pdev,
			ENABLE_ASPM_L1);
#endif
#endif

	/* Set DMA master */
	pci_set_master(pdev);

	return TRUE;

err_out_free_res:
	pci_release_regions(pdev);

	pci_disable_device(pdev);
    pci_disable_msi(pdev);

	return FALSE;
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
	struct BUS_INFO *prBusInfo;
	struct net_device *prNetDevice = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct pci_dev *pdev = NULL;
	int ret = 0;

	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (struct GLUE_INFO *)pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		return -1;

	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;

	prHifInfo->u4IrqId = pdev->irq;

	/* V2 FIX: Initialize recovery infrastructure */
	INIT_WORK(&prHifInfo->recovery_work, mt7902_recovery_work);
	mutex_init(&prHifInfo->recovery_lock);
	prHifInfo->state_flags = 0;
	set_bit(MTK_FLAG_IRQ_ALIVE, &prHifInfo->state_flags); /* IRQ starts alive */
	prHifInfo->saved_irq = pdev->irq;
	prHifInfo->pdev = pdev;

	ret = request_irq(prHifInfo->u4IrqId, mtk_pci_interrupt,
		IRQF_SHARED, prNetDevice->name, prGlueInfo);
	if (ret != 0)
		DBGLOG(INIT, INFO,
			"glBusSetIrq: request_irq  ERROR(%d)\n", ret);
	else if (prBusInfo->initPcieInt)
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

	ASSERT(pvData);
	if (!pvData) {
		DBGLOG(INIT, INFO, "%s null pvData\n", __func__);
		return;
	}
	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (struct GLUE_INFO *) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, INFO, "%s no glue info\n", __func__);
		return;
	}

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;

    /* V2 FIX: Mark IRQ dead before freeing */
    mt7902_mark_irq_dead(prHifInfo);

	synchronize_irq(pdev->irq);
	free_irq(pdev->irq, prGlueInfo);
    
    /* V2 FIX: Clean up recovery infrastructure */
    cancel_work_sync(&prHifInfo->recovery_work);
    mutex_destroy(&prHifInfo->recovery_lock);
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
	prDescRing->AllocPa = (phys_addr_t)rAddr;
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
	struct sk_buff *pkt = dev_alloc_skb(prDmaBuf->AllocSize);
	dma_addr_t rAddr;

	if (!pkt) {
		DBGLOG(HAL, ERROR, "can't allocate rx %lu size packet\n",
		       prDmaBuf->AllocSize);
		prDmaBuf->AllocPa = 0;
		prDmaBuf->AllocVa = NULL;
		return NULL;
	}

#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
	skb_reserve(pkt, CFG_RADIOTAP_HEADROOM);
#endif

	prDmaBuf->AllocVa = (void *)pkt->data;
	memset(prDmaBuf->AllocVa, 0, prDmaBuf->AllocSize);

	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, prDmaBuf->AllocVa,
				   prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "sk_buff dma mapping error!\n");
		dev_kfree_skb(pkt);
		return NULL;
	}
	prDmaBuf->AllocPa = (phys_addr_t)rAddr;
	return (void *)pkt;
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

	prTxCell->PacketPa = (phys_addr_t)rAddr;

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
	prDmaBuf->AllocPa = (phys_addr_t)rAddr;
	return true;
}

static bool pcieCopyTxData(struct MSDU_TOKEN_ENTRY *prToken,
			   void *pucSrc, uint32_t u4Len)
{
	memcpy(prToken->prPacket, pucSrc, u4Len);
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
	prDmaBuf->AllocPa = (phys_addr_t)rAddr;

	return true;
}

static phys_addr_t pcieMapTxBuf(struct GL_HIF_INFO *prHifInfo,
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

static phys_addr_t pcieMapRxBuf(struct GL_HIF_INFO *prHifInfo,
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
static void pcieSetASPML1SS(struct pci_dev *dev, int i4Enable)
{
	int pos;
	uint32_t u4Reg = 0;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_L1PMSS);

	if (!pos) {
		DBGLOG(INIT, INFO, "L1 PM Substate capability is not found!\n");
		return;
	}

	pci_read_config_dword(dev, pos + PCI_L1PMSS_CTR1, &u4Reg);
	u4Reg &= ~PCI_L1PMSS_ENABLE_MASK;
	u4Reg |= i4Enable;
	pci_write_config_dword(dev, pos + PCI_L1PMSS_CTR1, u4Reg);
}
static void pcieSetASPML1(struct pci_dev *dev, int i4Enable)
{
	uint16_t u2Reg = 0;
	int i4Pos = dev->pcie_cap;

	pci_read_config_word(dev, i4Pos + PCI_EXP_LNKCTL, &u2Reg);
	u2Reg &= ~PCI_L1PM_ENABLE_MASK;
	u2Reg |= i4Enable;
	pci_write_config_word(dev, i4Pos + PCI_EXP_LNKCTL, u2Reg);
}
static bool pcieCheckASPML1SS(struct pci_dev *dev, int i4BitMap)
{
	int i4Pos;
	uint32_t u4Reg = 0;

	i4Pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_L1PMSS);

	if (!i4Pos) {
		DBGLOG(INIT, INFO, "L1 PM Substate capability is not found!\n");
		return FALSE;
	}

	pci_read_config_dword(dev, i4Pos + PCI_L1PMSS_CAP, &u4Reg);
	if (i4BitMap != 0) {
		if ((i4BitMap & PCI_L1PM_CAP_ASPM_L12) &&
				(!(u4Reg & PCI_L1PM_CAP_ASPM_L12))) {
			DBGLOG(INIT, INFO, "not support ASPM L1.2!\n");
			return FALSE;
		}
		if ((i4BitMap & PCI_L1PM_CAP_ASPM_L11) &&
				(!(u4Reg & PCI_L1PM_CAP_ASPM_L11))) {
			DBGLOG(INIT, INFO, "not support ASPM L1.1!\n");
			return FALSE;
		}
	}
	return TRUE;
}
bool glBusConfigASPM(struct pci_dev *dev, int i4Enable)
{

/* FORCED DISABLE: Prevent PCIe link collapse on Vivobook */
    return FALSE;

	uint32_t u4Reg = 0;
	struct pci_dev *parent = dev->bus->self;
	int pos = parent->pcie_cap;


	pci_read_config_dword(parent, pos + PCI_EXP_LNKCAP, &u4Reg);
	if (PCIE_ASPM_CHECK_L1(u4Reg)) {
		pos = dev->pcie_cap;
		pci_read_config_dword(dev, pos + PCI_EXP_LNKCAP, &u4Reg);
		if (PCIE_ASPM_CHECK_L1(u4Reg)) {
			pcieSetASPML1(parent, i4Enable);
			pcieSetASPML1(dev, i4Enable);
			DBGLOG(INIT, INFO, "ASPM STATUS %d\n", i4Enable);
			return TRUE;
		}
	}
	return FALSE;

}
bool glBusConfigASPML1SS(struct pci_dev *dev, int i4Enable)
{
	struct pci_dev *parent = dev->bus->self;

	if (pcieCheckASPML1SS(parent, i4Enable)) {
		if (pcieCheckASPML1SS(dev, i4Enable)) {
			pcieSetASPML1SS(parent, i4Enable);
			pcieSetASPML1SS(dev, i4Enable);
			DBGLOG(INIT, INFO, "Config ASPM-L1SS\n");
			return TRUE;
		}
	}
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



