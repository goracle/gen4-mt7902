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
/*! \file   "hif.h"
 *    \brief  Functions for the driver to register bus and setup the IRQ
 *
 *    Functions for the driver to register bus and setup the IRQ
 */

/******************************************************************************
 * [Standard License Header Omitted]
 *****************************************************************************/

#ifndef _HIF_H
#define _HIF_H

#include "hif_pdma.h"

#if defined(_HIF_PCIE)
#define HIF_NAME "PCIE"
#else
#error "No HIF defined!"
#endif

/* State flags for atomic-safe recovery management (V2 Fix) */
#define MTK_FLAG_MMIO_GONE     0  /* bit 0: MMIO dead, needs recovery */
#define MTK_FLAG_IRQ_ALIVE     1  /* bit 1: IRQ still valid for sync */

/*******************************************************************************
 * C O N S T A N T S
 *******************************************************************************
 */
/* ASPM Definitions - Moved outside guard to ensure visibility for hal_pdma.c */
#ifndef PCI_EXP_LNKCAP_ASPMS
#define PCI_EXP_LNKCAP_ASPMS 0xc00
#endif

#define ENABLE_ASPM_L1 2
#define DISABLE_ASPM_L1 0

/* Define these if kernel version < 4.x or missing from pci_regs.h */
#ifndef PCI_EXT_CAP_ID_L1PMSS
#define PCI_EXT_CAP_ID_L1PMSS 0x1E
#endif

#ifndef PCI_L1PMSS_CAP
#define PCI_L1PMSS_CAP 4
#endif

#ifndef PCI_L1PMSS_CTR1
#define PCI_L1PMSS_CTR1 8
#endif

/* Bit Definitions */
#define PCI_L1PM_CAP_PCIPM_L12         0x00000001
#define PCI_L1PM_CAP_PCIPM_L11         0x00000002
#define PCI_L1PM_CAP_ASPM_L12          0x00000004
#define PCI_L1PM_CAP_ASPM_L11          0x00000008
#define PCI_L1PM_CAP_L1PM_SS           0x00000010
#define PCI_L1PM_CAP_PORT_CMR_TIME_MASK 0x0000FF00
#define PCI_L1PM_CAP_PWR_ON_SCALE_MASK  0x00030000
#define PCI_L1PM_CAP_PWR_ON_VALUE_MASK  0x00F80000

#define PCI_L1PM_CTR1_PCIPM_L12_EN     0x00000001
#define PCI_L1PM_CTR1_PCIPM_L11_EN     0x00000002
#define PCI_L1PM_CTR1_ASPM_L12_EN      0x00000004
#define PCI_L1PM_CTR1_ASPM_L11_EN      0x00000008

#define PCI_L1PMSS_ENABLE_MASK (PCI_L1PM_CTR1_PCIPM_L12_EN | \
                                PCI_L1PM_CTR1_PCIPM_L11_EN | \
                                PCI_L1PM_CTR1_ASPM_L12_EN |  \
                                PCI_L1PM_CTR1_ASPM_L11_EN)

#define PCI_L1PM_ENABLE_MASK           0x3

#define PCIE_ASPM_CHECK_L1(reg) ((((reg) & PCI_EXP_LNKCAP_ASPMS) >> 10) & 0x2)


/*******************************************************************************
 * D A T A   T Y P E S
 *******************************************************************************
 */

struct GL_HIF_INFO;

struct HIF_MEM_OPS {
    void (*allocTxDesc)(struct GL_HIF_INFO *prHifInfo,
                struct RTMP_DMABUF *prDescRing,
                uint32_t u4Num);
    void (*allocRxDesc)(struct GL_HIF_INFO *prHifInfo,
                struct RTMP_DMABUF *prDescRing,
                uint32_t u4Num);
    bool (*allocTxCmdBuf)(struct RTMP_DMABUF *prDmaBuf,
                  uint32_t u4Num, uint32_t u4Idx);
    void (*allocTxDataBuf)(struct MSDU_TOKEN_ENTRY *prToken,
                   uint32_t u4Idx);
    void *(*allocRxBuf)(struct GL_HIF_INFO *prHifInfo,
                struct RTMP_DMABUF *prDmaBuf,
                uint32_t u4Num, uint32_t u4Idx);
    void *(*allocRuntimeMem)(uint32_t u4SrcLen);
    bool (*copyCmd)(struct GL_HIF_INFO *prHifInfo,
            struct RTMP_DMACB *prTxCell, void *pucBuf,
            void *pucSrc1, uint32_t u4SrcLen1,
            void *pucSrc2, uint32_t u4SrcLen2);
    bool (*copyEvent)(struct GL_HIF_INFO *prHifInfo,
              struct RTMP_DMACB *pRxCell,
              struct RXD_STRUCT *pRxD,
              struct RTMP_DMABUF *prDmaBuf,
              uint8_t *pucDst, uint32_t u4Len);
    bool (*copyTxData)(struct MSDU_TOKEN_ENTRY *prToken,
               void *pucSrc, uint32_t u4Len);
    bool (*copyRxData)(struct GL_HIF_INFO *prHifInfo,
               struct RTMP_DMACB *pRxCell,
               struct RTMP_DMABUF *prDmaBuf,
               struct SW_RFB *prSwRfb);
    void (*flushCache)(struct GL_HIF_INFO *prHifInfo,
               void *pucSrc, uint32_t u4Len);
    phys_addr_t (*mapTxBuf)(struct GL_HIF_INFO *prHifInfo,
              void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
    phys_addr_t (*mapRxBuf)(struct GL_HIF_INFO *prHifInfo,
              void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
    void (*unmapTxBuf)(struct GL_HIF_INFO *prHifInfo,
               phys_addr_t rDmaAddr, uint32_t u4Len);
    void (*unmapRxBuf)(struct GL_HIF_INFO *prHifInfo,
               phys_addr_t rDmaAddr, uint32_t u4Len);
    void (*freeDesc)(struct GL_HIF_INFO *prHifInfo,
             struct RTMP_DMABUF *prDescRing);
    void (*freeBuf)(void *pucSrc, uint32_t u4Len);
    void (*freePacket)(void *pvPacket);
    void (*dumpTx)(struct GL_HIF_INFO *prHifInfo,
               struct RTMP_TX_RING *prTxRing,
               uint32_t u4Idx, uint32_t u4DumpLen);
    void (*dumpRx)(struct GL_HIF_INFO *prHifInfo,
               struct RTMP_RX_RING *prRxRing,
               uint32_t u4Idx, uint32_t u4DumpLen);
};

/* Recovery Enums */
enum pcie_recovery_stage {
    RECOV_STAGE_IDLE            = 0,
    RECOV_STAGE_QUIESCE         = 1,
    RECOV_STAGE_D3COLD_CYCLE    = 2,
    RECOV_STAGE_REENABLE        = 3,
    RECOV_STAGE_BAR_REMAP       = 4,
    RECOV_STAGE_SBR             = 5,
    RECOV_STAGE_BAR_PREFLIGHT   = 6,
    RECOV_STAGE_WFSYS_PWRCYCLE  = 7,
    RECOV_STAGE_CLOCKS          = 8,
    RECOV_STAGE_LINK_WAIT       = 9,
    RECOV_STAGE_PHASED_WAKE     = 10,
    RECOV_STAGE_DMA_PURGE       = 11,
    RECOV_STAGE_MCU_KICKSTART   = 12,
    RECOV_STAGE_BAR_TIER7       = 13,
    RECOV_STAGE_MCU_COLDBOOT    = 14,
    RECOV_STAGE_ADAPTER_START   = 15,
    RECOV_STAGE_COMPLETE        = 16,
    RECOV_STAGE_NUM
};

enum pcie_recovery_fail_reason {
    RECOV_FAIL_NONE              = 0,
    RECOV_FAIL_BAR_BLIND_PRE     = 1,
    RECOV_FAIL_BAR_BLIND_TIER7   = 2,
    RECOV_FAIL_COLDBOOT_BLIND    = 3,
    RECOV_FAIL_COLDBOOT_TIMEOUT  = 4,
    RECOV_FAIL_COLDBOOT_OTHER    = 5,
    RECOV_FAIL_ADAPTER_START     = 6,
    RECOV_FAIL_ENABLE_DEVICE     = 7,
    RECOV_FAIL_IOREMAP           = 8,
    RECOV_FAIL_NUM
};

enum pcie_suspend_state {
    PCIE_STATE_PRE_SUSPEND_WAITING,
    PCIE_STATE_PRE_SUSPEND_DONE,
    PCIE_STATE_PRE_SUSPEND_FAIL,
    PCIE_STATE_SUSPEND_ENTERING,
    PCIE_STATE_PRE_RESUME_DONE,
    PCIE_STATE_SUSPEND
};

struct GL_HIF_INFO {
    struct pci_dev *pdev;
    struct pci_dev *prDmaDev;
    struct HIF_MEM_OPS rMemOps;

    uint32_t u4IrqId;
    int32_t u4HifCnt;

    /* PCI MMIO Base Address */
    void *CSRBaseAddress;

    struct RTMP_DMABUF TxDescRing[NUM_OF_TX_RING];
    struct RTMP_TX_RING TxRing[NUM_OF_TX_RING];

    struct RTMP_DMABUF RxDescRing[NUM_OF_RX_RING];
    struct RTMP_RX_RING RxRing[NUM_OF_RX_RING];

    u_int8_t fgIntReadClear;
    u_int8_t fgMbxReadClear;

    uint32_t u4IntStatus;

    struct MSDU_TOKEN_INFO rTokenInfo;

    struct ERR_RECOVERY_CTRL_T rErrRecoveryCtl;
    struct timer_list rSerTimer;
    struct list_head rTxCmdQ;
    struct list_head rTxDataQ;
    uint32_t u4TxDataQLen;

    bool fgIsPowerOff;
    bool fgIsDumpLog;
    
    /* Recovery Status Tracking */
    bool fgMmioGone;
    bool fgInPciRecovery;
    uint8_t  u8RecoveryStage;
    uint8_t  u8RecoveryFailReason;
    uint32_t u4LastBarSample;
    unsigned long u_recovery_fail_time;
    struct dentry *debugfs_recovery_state;

    enum pcie_suspend_state eSuspendtate;

    uint32_t u4PcieLTR;
    uint32_t u4PcieASPM;

    /* V2 ATOMIC RECOVERY INFRASTRUCTURE */
    unsigned long state_flags;          /* Atomic state flags (MTK_FLAG_*) */
    struct work_struct recovery_work;   /* Workqueue for deferred recovery */
    struct mutex recovery_lock;         /* Protects recovery sequence */
    int saved_irq;                      /* Saved IRQ number */
};

struct BUS_INFO {
    /* [All original BUS_INFO members retained] */
    const uint32_t top_cfg_base;
    const struct PCIE_CHIP_CR_MAPPING *bus2chip;
    const uint32_t bus2chip_tbl_size;
    const uint32_t tx_ring_cmd_idx;
    const uint32_t tx_ring_wa_cmd_idx;
    const uint32_t tx_ring_fwdl_idx;
    const uint32_t tx_ring0_data_idx;
    const uint32_t tx_ring1_data_idx;
    const unsigned int max_static_map_addr;
    const uint32_t fw_own_clear_addr;
    const uint32_t fw_own_clear_bit;
    const bool fgCheckDriverOwnInt;
    const uint32_t u4DmaMask;
    const uint32_t host_dma0_base;
    const uint32_t host_dma1_base;
    const uint32_t host_ext_conn_hif_wrap_base;
    const uint32_t host_int_status_addr;
    const uint32_t host_int_txdone_bits;
    const uint32_t host_int_rxdone_bits;
    const uint32_t host_tx_ring_base;
    const uint32_t host_tx_ring_ext_ctrl_base;
    const uint32_t host_tx_ring_cidx_addr;
    const uint32_t host_tx_ring_didx_addr;
    const uint32_t host_tx_ring_cnt_addr;
    const uint32_t host_rx_ring_base;
    const uint32_t host_rx_ring_ext_ctrl_base;
    const uint32_t host_rx_ring_cidx_addr;
    const uint32_t host_rx_ring_didx_addr;
    const uint32_t host_rx_ring_cnt_addr;
    const uint32_t skip_tx_ring;

#if (CFG_SUPPORT_CONNAC2X == 1)
    const uint32_t host_wfdma1_rx_ring_base;
    const uint32_t host_wfdma1_rx_ring_cidx_addr;
    const uint32_t host_wfdma1_rx_ring_didx_addr;
    const uint32_t host_wfdma1_rx_ring_cnt_addr;
    const uint32_t host_wfdma1_rx_ring_ext_ctrl_base;
    const uint32_t rx_evt_ring_buf_size;
#endif

#if (CFG_SUPPORT_CONNAC3X == 1)
    const uint32_t pcie2ap_remap_2;
    const uint32_t ap2wf_remap_1;
    struct wfdma_group_info *wfmda_host_tx_group;
    const uint32_t wfmda_host_tx_group_len;
    struct wfdma_group_info *wfmda_host_rx_group;
    const uint32_t wfmda_host_rx_group_len;
    struct wfdma_group_info *wfmda_wm_tx_group;
    const uint32_t wfmda_wm_tx_group_len;
    struct wfdma_group_info *wfmda_wm_rx_group;
    const uint32_t wfmda_wm_rx_group_len;

    struct DMASHDL_CFG *prDmashdlCfg;
    struct PLE_TOP_CR *prPleTopCr;
    struct PSE_TOP_CR *prPseTopCr;
    struct PP_TOP_CR *prPpTopCr;
    struct pse_group_info *prPseGroup;
    const uint32_t u4PseGroupLen;
#endif

    void (*pdmaSetup)(struct GLUE_INFO *prGlueInfo, u_int8_t enable);
    uint32_t (*updateTxRingMaxQuota)(struct ADAPTER *prAdapter,
        uint8_t ucWmmIndex, uint32_t u4MaxQuota);
    void (*pdmaStop)(struct GLUE_INFO *prGlueInfo, u_int8_t enable);
    u_int8_t (*pdmaPollingIdle)(struct GLUE_INFO *prGlueInfo);
    void (*enableInterrupt)(struct ADAPTER *prAdapter);
    void (*disableInterrupt)(struct ADAPTER *prAdapter);
    void (*processTxInterrupt)(struct ADAPTER *prAdapter);
    void (*processRxInterrupt)(struct ADAPTER *prAdapter);
    void (*lowPowerOwnRead)(struct ADAPTER *prAdapter, u_int8_t *pfgResult);
    void (*lowPowerOwnSet)(struct ADAPTER *prAdapter, u_int8_t *pfgResult);
    void (*lowPowerOwnClear)(struct ADAPTER *prAdapter,
        u_int8_t *pfgResult);
    void (*wakeUpWiFi)(struct ADAPTER *prAdapter);
    bool (*isValidRegAccess)(struct ADAPTER *prAdapter,
                 uint32_t u4Register);
    void (*getMailboxStatus)(struct ADAPTER *prAdapter, uint32_t *pu4Val);
    void (*setDummyReg)(struct GLUE_INFO *prGlueInfo);
    void (*checkDummyReg)(struct GLUE_INFO *prGlueInfo);
    void (*tx_ring_ext_ctrl)(struct GLUE_INFO *prGlueInfo,
        struct RTMP_TX_RING *tx_ring, uint32_t index);
    void (*rx_ring_ext_ctrl)(struct GLUE_INFO *prGlueInfo,
        struct RTMP_RX_RING *rx_ring, uint32_t index);
    void (*wfdmaManualPrefetch)(struct GLUE_INFO *prGlueInfo);
    uint32_t (*getSoftwareInterrupt)(IN struct ADAPTER *prAdapter);
    void (*processSoftwareInterrupt)(IN struct ADAPTER *prAdapter);
    void (*softwareInterruptMcu)(IN struct ADAPTER *prAdapter,
        u_int32_t intrBitMask);
    void (*hifRst)(struct GLUE_INFO *prGlueInfo);
    void (*initPcieInt)(struct GLUE_INFO *prGlueInfo);
    void (*configPcieASPM)(struct GLUE_INFO *prGlueInfo, bool fgActivate);
    void (*setCTSbyRate)(struct GLUE_INFO *prGlueInfo,
        struct MSDU_INFO *prMsduInfo, void *prTxDesc);
    void (*devReadIntStatus)(struct ADAPTER *prAdapter,
        OUT uint32_t *pu4IntStatus);
    void (*DmaShdlInit)(IN struct ADAPTER *prAdapter);
    void (*DmaShdlReInit)(IN struct ADAPTER *prAdapter);
    uint8_t (*setRxRingHwAddr)(struct RTMP_RX_RING *prRxRing,
        struct BUS_INFO *prBusInfo,
        uint32_t u4SwRingIdx);
    bool (*wfdmaAllocRxRing)(
        struct GLUE_INFO *prGlueInfo,
        bool fgAllocMem);
#if CFG_SUPPORT_HOST_RX_WM_EVENT_FROM_PSE
    u_int8_t (*checkPortForRxEventFromPse)(struct ADAPTER *prAdapter,
        uint8_t u2Port);
#endif
#if (CFG_COALESCING_INTERRUPT == 1)
    uint32_t (*setWfdmaCoalescingInt)(struct ADAPTER *prAdapter,
        u_int8_t fgEnable);
#endif
};

/*******************************************************************************
 * F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove);
void glUnregisterBus(remove_card pfRemove);
void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie);
void glClearHifInfo(struct GLUE_INFO *prGlueInfo);
void glResetHifInfo(struct GLUE_INFO *prGlueInfo);
u_int8_t glBusInit(void *pvData);
void glBusRelease(void *pData);
int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie);
void glBusFreeIrq(void *pvData, void *pvCookie);
void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode);
void glGetDev(void *ctx, struct device **dev);
void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev);
void halPciePreSuspendDone(IN struct ADAPTER *prAdapter,
    IN struct CMD_INFO *prCmdInfo, IN uint8_t *pucEventBuf);
void halPciePreSuspendTimeout(IN struct ADAPTER *prAdapter,
    IN struct CMD_INFO *prCmdInfo);
void halPciePreSuspendCmd(IN struct ADAPTER *prAdapter);
void halPcieResumeCmd(IN struct ADAPTER *prAdapter);

#if CFG_SUPPORT_PCIE_ASPM
bool glBusConfigASPM(struct pci_dev *dev, int val);
bool glBusConfigASPML1SS(struct pci_dev *dev, int enable);
#endif

extern void dump_pci_state(struct pci_dev *pdev);
extern void dump_mailbox(struct ADAPTER *prAdapter);
extern void dump_pdma_state(struct ADAPTER *prAdapter);

#endif /* _HIF_H */
