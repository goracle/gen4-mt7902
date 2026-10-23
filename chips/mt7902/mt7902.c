/* SPDX-License-Identifier: GPL-2.0 */
/* 
* Copyright (c) 2020 MediaTek Inc.
*/

/*! \file   mt7902.c
*    \brief  Internal driver stack will export
*    the required procedures here for GLUE Layer.
*
*    This file contains all routines which are exported
     from MediaTek 802.11 Wireless LAN driver stack to GLUE Layer.
*/

#ifdef MT7902

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "coda/mt7902/wf_wfdma_host_dma0.h"
#include "coda/mt7902/wf_cr_sw_def.h"
#include "precomp.h"
#include "mt7961.h"
#include "mt7902.h"
#include "hal_dmashdl_mt7902.h"
#include "hal_wfsys_reset_mt7961.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define CONN_MCU_CONFG_BASE 0x88000000
#define CONN_MCU_CONFG_COM_REG0_ADDR (CONN_MCU_CONFG_BASE + 0x200)

#define PATCH_SEMAPHORE_COMM_REG 0
#define PATCH_SEMAPHORE_COMM_REG_PATCH_DONE 1 /* bit0 is for patch. */

#define RX_DATA_RING_BASE_IDX 0

#define FLAVOR_VERSION_DEFAULT (1)

#define MT7902_CMD_TX_RING_IDX 15

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define MT7902_BUS2CHIP_MEM_MAP_TBLE_SIZE    \
	(sizeof(mt7902_bus2chip_cr_mapping) /	\
	 sizeof(mt7902_bus2chip_cr_mapping[0]))

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************


*/


void wlanMt7902InitPcieInt(struct GLUE_INFO *prGlueInfo)
{
    struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
    
    /* MT7902 PCIe IRQ watermark fix - clears RX flood interference */
    HAL_MCR_WR(prAdapter, 0x10188, 0x00000000);  // Clear RX watermark mask
    kalUdelay(10);
    
    /* Original PCIE interrupt DMA end enable */
    HAL_MCR_WR(prGlueInfo->prAdapter, 0x10188, 0x000000FF);
    DBGLOG(INIT, INFO, "MT7902: IRQ watermark cleared\n");
}



/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

struct ECO_INFO mt7902_eco_table[] = {
	/* HW version,  ROM version, Factory version, EcoVer */
	{0x00, 0x00, 0xA, 0x1},	/* E1 */
	{0x00, 0x00, 0x0, 0x0}	/* End of table */
};

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
struct PCIE_CHIP_CR_MAPPING mt7902_bus2chip_cr_mapping[] = {
	/* chip addr, bus addr, range */
	{0x54000000, 0x02000, 0x1000},  /* WFDMA PCIE0 MCU DMA0 */
	{0x55000000, 0x03000, 0x1000},  /* WFDMA PCIE0 MCU DMA1 */
	{0x56000000, 0x04000, 0x1000},  /* WFDMA reserved */
	{0x57000000, 0x05000, 0x1000},  /* WFDMA MCU wrap CR */
	{0x58000000, 0x06000, 0x1000},  /* WFDMA PCIE1 MCU DMA0 (MEM_DMA) */
	{0x59000000, 0x07000, 0x1000},  /* WFDMA PCIE1 MCU DMA1 */
	{0x820c0000, 0x08000, 0x4000},  /* WF_UMAC_TOP (PLE) */
	{0x820c8000, 0x0c000, 0x2000},  /* WF_UMAC_TOP (PSE) */
	{0x820cc000, 0x0e000, 0x2000},  /* WF_UMAC_TOP (PP) */
	{0x74030000, 0x10000, 0x10000}, /* PCIE_MAC_IREG */
	{0x820e0000, 0x20000, 0x0400},  /* WF_LMAC_TOP BN0 (WF_CFG) */
	{0x820e1000, 0x20400, 0x0200},  /* WF_LMAC_TOP BN0 (WF_TRB) */
	{0x820e2000, 0x20800, 0x0400},  /* WF_LMAC_TOP BN0 (WF_AGG) */
	{0x820e3000, 0x20c00, 0x0400},  /* WF_LMAC_TOP BN0 (WF_ARB) */
	{0x820e4000, 0x21000, 0x0400},  /* WF_LMAC_TOP BN0 (WF_TMAC) */
	{0x820e5000, 0x21400, 0x0800},  /* WF_LMAC_TOP BN0 (WF_RMAC) */
	{0x820ce000, 0x21c00, 0x0200},  /* WF_LMAC_TOP (WF_SEC) */
	{0x820e7000, 0x21e00, 0x0200},  /* WF_LMAC_TOP BN0 (WF_DMA) */
	{0x820cf000, 0x22000, 0x1000},  /* WF_LMAC_TOP (WF_PF) */
	{0x820e9000, 0x23400, 0x0200},  /* WF_LMAC_TOP BN0 (WF_WTBLOFF) */
	{0x820ea000, 0x24000, 0x0200},  /* WF_LMAC_TOP BN0 (WF_ETBF) */
	{0x820eb000, 0x24200, 0x0400},  /* WF_LMAC_TOP BN0 (WF_LPON) */
	{0x820ec000, 0x24600, 0x0200},  /* WF_LMAC_TOP BN0 (WF_INT) */
	{0x820ed000, 0x24800, 0x0800},  /* WF_LMAC_TOP BN0 (WF_MIB) */
	{0x820ca000, 0x26000, 0x2000},  /* WF_LMAC_TOP BN0 (WF_MUCOP) */
	{0x820d0000, 0x30000, 0x10000}, /* WF_LMAC_TOP (WF_WTBLON) */
	{0x88000000, 0x40000, 0x10000}, /* CONN_MCU_CONFG_LS */
	{0x40000000, 0x70000, 0x10000}, /* WF_UMAC_SYSRAM */
	{0x00400000, 0x80000, 0x10000}, /* WF_MCU_SYSRAM */
	{0x00410000, 0x90000, 0x10000}, /* WF_MCU_SYSRAM (configure register) */
	{0x820f0000, 0xa0000, 0x0400},  /* WF_LMAC_TOP BN1 (WF_CFG) */
	{0x820f1000, 0xa0600, 0x0200},  /* WF_LMAC_TOP BN1 (WF_TRB) */
	{0x820f2000, 0xa0800, 0x0400},  /* WF_LMAC_TOP BN1 (WF_AGG) */
	{0x820f3000, 0xa0c00, 0x0400},  /* WF_LMAC_TOP BN1 (WF_ARB) */
	{0x820f4000, 0xa1000, 0x0400},  /* WF_LMAC_TOP BN1 (WF_TMAC) */
	{0x820f5000, 0xa1400, 0x0800},  /* WF_LMAC_TOP BN1 (WF_RMAC) */
	{0x820f7000, 0xa1e00, 0x0200},  /* WF_LMAC_TOP BN1 (WF_DMA) */
	{0x820f9000, 0xa3400, 0x0200},  /* WF_LMAC_TOP BN1 (WF_WTBLOFF) */
	{0x820fa000, 0xa4000, 0x0200},  /* WF_LMAC_TOP BN1 (WF_ETBF) */
	{0x820fb000, 0xa4200, 0x0400},  /* WF_LMAC_TOP BN1 (WF_LPON) */
	{0x820fc000, 0xa4600, 0x0200},  /* WF_LMAC_TOP BN1 (WF_INT) */
	{0x820fd000, 0xa4800, 0x0800},  /* WF_LMAC_TOP BN1 (WF_MIB) */
	{0x820cc000, 0xa5000, 0x2000},  /* WF_LMAC_TOP BN1 (WF_MUCOP) */
	{0x820c4000, 0xa8000, 0x4000},  /* WF_LMAC_TOP BN1 (WF_MUCOP) */
	{0x820b0000, 0xae000, 0x1000},  /* [APB2] WFSYS_ON */
	{0x80020000, 0xb0000, 0x10000}, /* WF_TOP_MISC_OFF */
	{0x81020000, 0xc0000, 0x10000}, /* WF_TOP_MISC_ON */
	{0x7c020000, 0xd0000, 0x10000}, /* CONN_INFRA, wfdma */
	{0x7c060000, 0xe0000, 0x10000}, /* CONN_INFRA, conn_host_csr_top */
	{0x7c000000, 0xf0000, 0x10000}, /* CONN_INFRA */
};
static void mt7902CapInit(struct ADAPTER *prAdapter)
{
    struct mt66xx_chip_info *prChipInfo;

    asicConnac2xCapInit(prAdapter);

    prChipInfo = prAdapter->chip_info;
    prChipInfo->u2RxSwPktBitMap = CONNAC2X_RX_STATUS_PKT_TYPE_SW_BITMAP;
    prChipInfo->u2RxSwPktEvent  = CONNAC2X_RX_STATUS_PKT_TYPE_SW_EVENT;
    prChipInfo->u2RxSwPktFrame  = CONNAC2X_RX_STATUS_PKT_TYPE_SW_FRAME;
}

void mt7902EnableInterrupt(
	struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	union WPDMA_INT_MASK IntMask;
	uint32_t u4HostWpdamBase = 0;
#if (CFG_CHIP_RESET_SUPPORT == 1)
	uint32_t u4IntEna;
#endif

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->is_support_wfdma1)
		u4HostWpdamBase = CONNAC2X_HOST_WPDMA_1_BASE;
	else
		u4HostWpdamBase = CONNAC2X_HOST_WPDMA_0_BASE;

	prAdapter->fgIsIntEnable = TRUE;

	HAL_MCR_RD(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &IntMask.word);
	IntMask.word = 0;
	IntMask.field_conn2x_single.wfdma0_rx_done_0 = 1;
	IntMask.field_conn2x_single.wfdma0_rx_done_1 = 1;
	IntMask.field_conn2x_single.wfdma0_rx_done_2 = 1;
	IntMask.field_conn2x_single.wfdma0_rx_done_3 = 1;
	IntMask.field_conn2x_single.wfdma0_rx_done_4 = 1;
	IntMask.field_conn2x_single.wfdma0_rx_done_5 = 1;
	IntMask.field_conn2x_single.wfdma0_tx_done_0 = 1;
	IntMask.field_conn2x_single.wfdma0_tx_done_16 = 1;
	/* MT7902 CMD TX ring is 15, but its interrupt idx is 17 */
	IntMask.field_conn2x_single.wfdma0_tx_done_17 = 1;
	IntMask.field_conn2x_single.wfdma0_mcu2host_sw_int_en = 1;

	IntMask.field_conn2x_single.wfdma0_rx_coherent = 0;
	IntMask.field_conn2x_single.wfdma0_tx_coherent = 0;
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, IntMask.word);

	if (prChipInfo->is_support_asic_lp)
		HAL_MCR_WR_FIELD(prAdapter,
				 CONNAC2X_WPDMA_MCU2HOST_SW_INT_MASK
				 (u4HostWpdamBase),
				 BITS(0, 15),
				 0,
				 BITS(0, 15));

#if (CFG_CHIP_RESET_SUPPORT == 1)
	/* WF WDT interrupt to host via PCIE */
	if (prAdapter->eWfsysResetState == WFSYS_RESET_STATE_IDLE &&
	    prChipInfo->fgIsSupportL0p5Reset == TRUE) {
		HAL_MCR_RD(prAdapter, PCIE_MAC_IREG_IMASK_HOST_ADDR, &u4IntEna);
		u4IntEna |= PCIE_MAC_IREG_IMASK_HOST_WDT_INT_MASK;
		HAL_MCR_WR(prAdapter, PCIE_MAC_IREG_IMASK_HOST_ADDR, u4IntEna);
	}
#endif /* CFG_CHIP_RESET_SUPPORT */

	DBGLOG(HAL, WARN, "[INT-ENA] re-armed IntMask=0x%08x\n", IntMask.word);
} /* end of nicEnableInterrupt() */

void mt7902DisableInterrupt(
	struct ADAPTER *prAdapter)
{
	union WPDMA_INT_MASK IntMask;
#if (CFG_CHIP_RESET_SUPPORT == 1)
	uint32_t u4IntEna;
#endif

	if (prAdapter == NULL) {
		DBGLOG(HAL, ERROR, "prAdapter is NULL\n");
		return;
	}

	IntMask.word = 0;

	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, IntMask.word);
	/* guarantee the WR cmd had done */
	HAL_MCR_RD(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &IntMask.word);

#if (CFG_CHIP_RESET_SUPPORT == 1)
	if (prAdapter->chip_info->fgIsSupportL0p5Reset == TRUE) {
		/* WF WDT interrupt to host via PCIE */
		HAL_MCR_RD(prAdapter, PCIE_MAC_IREG_IMASK_HOST_ADDR, &u4IntEna);
		u4IntEna &= ~PCIE_MAC_IREG_IMASK_HOST_WDT_INT_MASK;
		HAL_MCR_WR(prAdapter, PCIE_MAC_IREG_IMASK_HOST_ADDR, u4IntEna);
	}
#endif /* CFG_CHIP_RESET_SUPPORT */

	prAdapter->fgIsIntEnable = FALSE;

	DBGLOG(HAL, TRACE, "%s\n", __func__);
}


uint8_t mt7902SetRxRingHwAddr(
	struct RTMP_RX_RING *prRxRing,
	struct BUS_INFO *prBusInfo,
	uint32_t u4SwRingIdx)
{
	uint32_t offset = 0;
	uint32_t val = 0;

	if (!prRxRing) {
		DBGLOG(HAL, ERROR, "prRxRing is NULL for idx %d\n", u4SwRingIdx);
		return FALSE;
	}

	switch (u4SwRingIdx) {
	case RX_RING_EVT_IDX_1:      /* Sw Ring 0 -> HW Ring 0 */
		val = 0;
		break;
	case WFDMA0_RX_RING_IDX_3:   /* Sw Ring 1 -> HW Ring 1 */
		val = 1;
		break;
	case RX_RING_DATA_IDX_0:     /* Sw Ring 2 -> HW Ring 2 */
		val = 2;
		break;
	case WFDMA0_RX_RING_IDX_2:   /* Sw Ring 3 -> HW Ring 3 */
		val = 3;
		break;
	case RX_RING_DATA1_IDX_2:                      /* Sw Ring 4 -> HW Ring 4 (Data 1) */
		val = 4;
		break;
	case RX_RING_DATA2_IDX_5:                      /* Sw Ring 5 -> HW Ring 5 (WFDMA0 4) */
		val = 5;
		break;
	case RX_RING_TXDONE2_IDX_6:                      /* Sw Ring 6 -> HW Ring 6 (WFDMA0 5) */
		val = 6;
		break;
	default:
		/* Safety for any other rings reported by HW */
		if (u4SwRingIdx < 16) {
			val = u4SwRingIdx;
		} else {
			DBGLOG(HAL, ERROR, "not supported ring idx %d\n", u4SwRingIdx);
			return FALSE;
		}
		break;
	}

	offset = val * MT_RINGREG_DIFF;

	prRxRing->hw_desc_base = prBusInfo->host_rx_ring_base + offset;
	prRxRing->hw_cidx_addr = prBusInfo->host_rx_ring_cidx_addr + offset;
	prRxRing->hw_didx_addr = prBusInfo->host_rx_ring_didx_addr + offset;
	prRxRing->hw_cnt_addr = prBusInfo->host_rx_ring_cnt_addr + offset;

	return TRUE;
}




bool mt7902LiteWfdmaAllocRxRing(// idk, fam
	struct GLUE_INFO *prGlueInfo,
	bool fgAllocMem)
{
	/* Band0 Data Rx (RX_RING_DATA_IDX_0) is alloced at halWpdmaAllocRing */

	/* Band1 Data Rx path */
	if (!halWpdmaAllocRxRing(prGlueInfo,
			WFDMA0_RX_RING_IDX_2, RX_RING0_SIZE,
			RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[0] fail\n");
		return false;
	}

	/* Band0 Tx Free Done Event + MCU EVT use the RX Ring 0
	 * The RX_RING_EVT_IDX_1 resource is alloced at halWpdmaAllocRing.
	 * The halWpdmaAllocRing set size as RX_RING1_SIZE (16) not
	 * MT7961_HOST_RX_WM_EVENT_FROM_PSE_RX_RING4_SIZE (32).
	 */

	/* Band1 Tx Free Done Event */
	if (!halWpdmaAllocRxRing(prGlueInfo,
			WFDMA0_RX_RING_IDX_3, RX_RING1_SIZE,
			RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem)) {
		DBGLOG(HAL, ERROR, "AllocRxRing[1] fail\n");
		return false;
	}
	return true;
}

bool mt7902WfdmaAllocRxRing(struct GLUE_INFO *prGlueInfo, bool fgAllocMem)
{
	/* 1. Base allocation (halWpdmaAllocRing) already allocated SW Ring 0 (Data) 
	 * and SW Ring 1 (Event). However, for MT7902, SW Ring 1 maps to HW Ring 0, 
	 * which absorbs BOTH Events and TX-Done. It must be size 512, not 16. 
	 */
	if (fgAllocMem) {
		/* Free the undersized default Event Ring first to avoid leaks */
		//halWpdmaFreeRing(prGlueInfo, RX_RING_EVT_IDX_1);
		
		if (!halWpdmaAllocRxRing(prGlueInfo, RX_RING_EVT_IDX_1, 512, 
					RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem)) {
			DBGLOG(HAL, ERROR, "Failed to reallocate HW Ring 0 to 512\n");
			return false;
		}
	}

	/* 2. Allocate the remaining MT7902 rings */
	
	/* HW Ring 1 (WFDMA0_RX_RING_IDX_3) - Band1 Tx Free Done (Event) */
	if (!halWpdmaAllocRxRing(prGlueInfo, WFDMA0_RX_RING_IDX_3, 16, 
				RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem))
		return false;

	/* HW Ring 3 (WFDMA0_RX_RING_IDX_2) - Band 1 Rx Data */
	if (!halWpdmaAllocRxRing(prGlueInfo, WFDMA0_RX_RING_IDX_2, 512, 
				RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem))
		return false;

	/* HW Ring 4 (RX_RING_DATA1_IDX_2) - Extra Data */
	if (!halWpdmaAllocRxRing(prGlueInfo, RX_RING_DATA1_IDX_2, 512, 
				RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem))
		return false;

	/* HW Ring 5 (RX_RING_DATA2_IDX_5) - Extra Data */
	if (!halWpdmaAllocRxRing(prGlueInfo, RX_RING_DATA2_IDX_5, 512, 
				RXD_SIZE, CFG_RX_MAX_PKT_SIZE, fgAllocMem))
		return false;

	return true;
}



void mt7902Connac2xProcessTxInterrupt(
	struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	union WPDMA_INT_STA_STRUCT rIntrStatus;

	rIntrStatus = (union WPDMA_INT_STA_STRUCT)prHifInfo->u4IntStatus;
	DBGLOG(HAL, WARN, "[TXINT-PROC] raw=0x%08x tx_done0=%d tx_done1=%d tx_done16=%d tx_done17=%d\n",
		prHifInfo->u4IntStatus,
		rIntrStatus.field_conn2x_single.wfdma0_tx_done_0,
		rIntrStatus.field_conn2x_single.wfdma0_tx_done_1,
		rIntrStatus.field_conn2x_single.wfdma0_tx_done_16,
		rIntrStatus.field_conn2x_single.wfdma0_tx_done_17);
	if (rIntrStatus.field_conn2x_single.wfdma0_tx_done_16)
		halWpdmaProcessCmdDmaDone(
			prAdapter->prGlueInfo, TX_RING_FWDL_IDX_3);

	/* MT7902 CMD ring is TXRING 15, but the interrupt status CR has no
	 * free field for the tx ring 15. So that, the MT7902 use tx ring 17's
	 * interrupt CR field.
	 */
	if (rIntrStatus.field_conn2x_single.wfdma0_tx_done_17)
		halWpdmaProcessCmdDmaDone(
			prAdapter->prGlueInfo, TX_RING_CMD_IDX_2);

	if (rIntrStatus.field_conn2x_single.wfdma0_tx_done_0) {
		halWpdmaProcessDataDmaDone(
			prAdapter->prGlueInfo, TX_RING_DATA0_IDX_0);
#if CFG_SUPPORT_MULTITHREAD
		if (!HAL_IS_TX_DIRECT())
			kalSetTxEvent2Hif(prAdapter->prGlueInfo);
#endif
	}

	if (rIntrStatus.field_conn2x_single.wfdma0_tx_done_1) {
		halWpdmaProcessDataDmaDone(
			prAdapter->prGlueInfo, TX_RING_DATA1_IDX_1);
#if CFG_SUPPORT_MULTITHREAD
		if (!HAL_IS_TX_DIRECT())
			kalSetTxEvent2Hif(prAdapter->prGlueInfo);
#endif
	}
}


void mt7902Connac2xProcessRxInterrupt(struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	union WPDMA_INT_STA_STRUCT rIntrStatus;

	rIntrStatus = (union WPDMA_INT_STA_STRUCT)prHifInfo->u4IntStatus;

	/* HW Ring 0: MCU Events & TX Free Done Events.
	 * Handled strictly as an Event ring (FALSE).
	 */
	if (rIntrStatus.field_conn2x_single.wfdma0_rx_done_0)
		halRxReceiveRFBs(prAdapter, RX_RING_EVT_IDX_1, FALSE);

	/* HW Ring 1: Band1 TX Free Done Event.
	 * Handled strictly as an Event ring (FALSE).
	 */
	if (rIntrStatus.field_conn2x_single.wfdma0_rx_done_1)
		halRxReceiveRFBs(prAdapter, WFDMA0_RX_RING_IDX_3, FALSE);

	/* HW Ring 2: Band0 RX Data */
	if (rIntrStatus.field_conn2x_single.wfdma0_rx_done_2)
		halRxReceiveRFBs(prAdapter, RX_RING_DATA_IDX_0, TRUE);

	/* HW Ring 3: Band1 RX Data */
	if (rIntrStatus.field_conn2x_single.wfdma0_rx_done_3)
		halRxReceiveRFBs(prAdapter, WFDMA0_RX_RING_IDX_2, TRUE);

	/* HW Ring 4: Additional Data 1 */
	if (rIntrStatus.field_conn2x_single.wfdma0_rx_done_4)
		halRxReceiveRFBs(prAdapter, RX_RING_DATA1_IDX_2, TRUE);

	/* HW Ring 5: Additional Data 2 */
	if (rIntrStatus.field_conn2x_single.wfdma0_rx_done_5)
		halRxReceiveRFBs(prAdapter, RX_RING_DATA2_IDX_5, TRUE);
}

void mt7902WfdmaTxRingExtCtrl(
	struct GLUE_INFO *prGlueInfo,
	struct RTMP_TX_RING *tx_ring,
	uint32_t index)
{
	struct BUS_INFO *prBusInfo;
	uint32_t u4Offset = 0;

	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	/*
	 * TX_RING_DATA0_IDX_0   (TX_Ring0)  - Band0 Tx Data
	 * TX_RING_DATA1_IDX_1   (TX_Ring1)  - Band1 Tx Data
	 * TX_RING_CMD_IDX_2     (TX_Ring15) - FW CMD
	 * TX_RING_FWDL_IDX_3    (TX_Ring16) - FW download
	 */
	switch (index) {
	case TX_RING_DATA0_IDX_0:
		u4Offset = HW_WFDMA0_TX_RING_IDX_0 * MT_RINGREG_EXT_DIFF;
		break;

	case TX_RING_DATA1_IDX_1:
		u4Offset = HW_WFDMA0_TX_RING_IDX_1 * MT_RINGREG_EXT_DIFF;
		break;

	case TX_RING_CMD_IDX_2:
		u4Offset = HW_WFDMA0_TX_RING_IDX_15 * MT_RINGREG_EXT_DIFF;
		break;

	case TX_RING_FWDL_IDX_3:
		u4Offset = HW_WFDMA0_TX_RING_IDX_16 * MT_RINGREG_EXT_DIFF;
		break;

	default:
		return;
	}

	tx_ring->hw_desc_base_ext =
		prBusInfo->host_tx_ring_ext_ctrl_base + u4Offset;
}

void mt7902WfdmaRxRingExtCtrl(
	struct GLUE_INFO *prGlueInfo,
	struct RTMP_RX_RING *rx_ring,
	uint32_t index)
{
	struct BUS_INFO *prBusInfo;
	uint32_t u4Offset = 0;

	prBusInfo = prGlueInfo->prAdapter->chip_info->bus_info;

	/*
	 * RX_RING_EVT_IDX_1     (RX_Ring0) - Rx Event
	 *  - include Band0 Tx Free Done Event
	 * WFDMA0_RX_RING_IDX_3  (RX_Ring1) - Band1 Tx Free Done Event
	 * RX_RING_DATA_IDX_0    (RX_Ring2) - Band0 Rx Data
	 * WFDMA0_RX_RING_IDX_2  (RX_Ring3) - Band1 Rx Data
	 */
	switch (index) {
	case RX_RING_EVT_IDX_1:
		u4Offset = HW_WFDMA0_RX_RING_IDX_0 * MT_RINGREG_EXT_DIFF;
		break;

	case WFDMA0_RX_RING_IDX_3:
		u4Offset = HW_WFDMA0_RX_RING_IDX_1 * MT_RINGREG_EXT_DIFF;
		break;

	case RX_RING_DATA_IDX_0:
		u4Offset = HW_WFDMA0_RX_RING_IDX_2 * MT_RINGREG_EXT_DIFF;
		break;

	case WFDMA0_RX_RING_IDX_2:
		u4Offset = HW_WFDMA0_RX_RING_IDX_3 * MT_RINGREG_EXT_DIFF;
		break;

	default:
		return;
	}

	rx_ring->hw_desc_base_ext =
		prBusInfo->host_rx_ring_ext_ctrl_base + u4Offset;
}


void mt7902_disable_interrupt(struct ADAPTER *prAdapter)
{
    /* * Writing 0 to the Host Interrupt Enable register 
     * masks all WFDMA interrupts for this DMA engine.
     */
    HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, 0);

    /* * Optional: Read back to ensure the write posted to the hardware 
     * before continuing with sensitive cleanup operations.
     */
    {
        uint32_t u4Dummy;
        HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &u4Dummy);
    }
}


void mt7902_enable_interrupt(struct ADAPTER *prAdapter)
{
    uint32_t val;
    
    // Read current mask
    HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &val);
    
    // Enable Data Rings (0-3) and MCU Command Ring (15/Bit 17)
    val |= (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(17));
    
    HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, val);
}


#ifndef MT_WFDMA0_GLO_CFG_CLK_GAT_DIS
#define MT_WFDMA0_GLO_CFG_CLK_GAT_DIS          BIT(30)
#endif
#ifndef MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN
#define MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN   BIT(15)
#endif

extern void mt79xx_wfdma_dump(struct ADAPTER *prAdapter, const char *tag);

/* CONN_ON_MISC FW state (upstream mt76 patch "ensure MCU ready before ROM
 * patch download" polls this before the first command).  The address and
 * field values below are from memory of mt792x_regs.h / mt76_connac.h --
 * verify against your headers.  Override with -D if they differ.
 * 0x7c060000 is already in this file's remap table (conn_host_csr_top). */
#ifndef MT7902_CONN_ON_MISC_ADDR
#define MT7902_CONN_ON_MISC_ADDR	0x7c0600f0
#endif
#ifndef MT7902_TOP_MISC_FW_STATE_MASK
#define MT7902_TOP_MISC_FW_STATE_MASK	0x7
#endif
#ifndef MT7902_TOP_MISC_FW_PWR_ON
#define MT7902_TOP_MISC_FW_PWR_ON	0x1
#endif

void mt79xx_conn_on_misc_log(struct ADAPTER *prAdapter, const char *tag)
{
	uint32_t v = 0;
	uint32_t rst = 0;
	uint32_t romidx = 0;
	uint32_t wdt = 0;

	HAL_MCR_RD(prAdapter, MT7902_CONN_ON_MISC_ADDR, &v);
	/* WFSYS_SW_RST reg: mt76 expects bit0=1 (out of reset) AND bit4
	 * (INIT_DONE) = 1 after the reset.  Your earlier log only showed 0x01. */
	HAL_MCR_RD(prAdapter, 0x7c000140, &rst);
	/* ROMCODE_INDEX: mt76's MT7927 init polls this for 0x1d1e ("MCU idle");
	 * 0x81021604 is BAR 0xc1604 through the 0x81020000 window in this
	 * file's remap table.  Likely what mt7961HalPollWfsysSwInitDone checks
	 * (that function isn't in the uploads, so unverified). */
	HAL_MCR_RD(prAdapter, 0x81021604, &romidx);
	/* PCIE_MAC_IREG ISTATUS_HOST (hal_wfsys_reset_mt7961.h): bit 8 = WFSYS
	 * watchdog.  If it sets after the first command, the MCU reset itself. */
	HAL_MCR_RD(prAdapter, 0x7403018c, &wdt);
	DBGLOG(INIT, ERROR,
	       "[CONN_ON_MISC %s] @0x%08x=0x%08x fw_state=0x%x (want 0x%x) | WFSYS_RST@0x7c000140=0x%08x bit4=%u | ROMCODE_INDEX@0x81021604=0x%08x (idle=0x1d1e) | PCIE_ISTATUS@0x7403018c=0x%08x wdt(bit8)=%u\n",
	       tag, (uint32_t)MT7902_CONN_ON_MISC_ADDR, v,
	       v & MT7902_TOP_MISC_FW_STATE_MASK,
	       (uint32_t)MT7902_TOP_MISC_FW_PWR_ON, rst, (rst >> 4) & 1,
	       romidx, wdt, (wdt >> 8) & 1);
}

/* Poll CONN_ON_MISC until FW state == PWR_ON.  Non-fatal: logs every value
 * change with the elapsed ms so the log shows what the ROM actually reports
 * even if the expected value assumed above is wrong. */
uint32_t mt79xx_wait_mcu_ready(struct ADAPTER *prAdapter, uint32_t u4TimeoutMs)
{
	uint32_t v = 0, last = 0xffffffff, i;

	for (i = 0; i <= u4TimeoutMs; i++) {
		HAL_MCR_RD(prAdapter, MT7902_CONN_ON_MISC_ADDR, &v);
		if (v != last) {
			DBGLOG(INIT, ERROR,
			       "[MCU-READY] t=%ums CONN_ON_MISC=0x%08x fw_state=0x%x\n",
			       i, v, v & MT7902_TOP_MISC_FW_STATE_MASK);
			last = v;
		}
		/* 0x1 = ROM up (pre-FWDL), 0x3 = RAM FW running (seen in dmesg
		 * after FWDL; mt76 polls FW_N9_RDY = GENMASK(1,0)).  Accept both
		 * so the post-FWDL / SER reinit paths don't busy-wait 20 ms in
		 * softirq context. */
		if ((v & MT7902_TOP_MISC_FW_STATE_MASK) ==
		    MT7902_TOP_MISC_FW_PWR_ON ||
		    (v & MT7902_TOP_MISC_FW_STATE_MASK) == 0x3) {
			DBGLOG(INIT, ERROR, "[MCU-READY] ready after %u ms\n", i);
			return WLAN_STATUS_SUCCESS;
		}
		kalMdelay(1);
	}
	DBGLOG(INIT, ERROR,
	       "[MCU-READY] TIMEOUT %u ms, last CONN_ON_MISC=0x%08x (continuing)\n",
	       u4TimeoutMs, v);
	return WLAN_STATUS_FAILURE;
}

/* ---- Debug/behaviour toggles (override with -D on the build line) ----
 * MT7902_CLEAR_LBK_RX_Q_SEL: clear GLO_CFG bit 20 (CSR_LBK_RX_Q_SEL_EN) before
 *   enabling TX/RX DMA.  mt76 never sets it; vendor code left it set from
 *   reset/ROM (GLO_CFG reads 0x5030ba7d).  Default ON: top lead.
 */
/* mt76 final GLO_CFG = 0x5030b875: bit20 set, bit9 clear. */
#ifndef MT7902_SET_FWDL_BYPASS_BIT
#define MT7902_SET_FWDL_BYPASS_BIT	0
#endif
#ifndef MT7902_CLEAR_LBK_RX_Q_SEL
#define MT7902_CLEAR_LBK_RX_Q_SEL	0
#endif
#define MT7902_GLO_CFG_LBK_RX_Q_SEL_EN	BIT(20)

/* One-shot dump of every non-zero WFDMA0 (0x7c024000..+0x7ff) and DMASHDL
 * (0x7c026000..+0xff) word, for a register-by-register diff against a
 * working mt76 (see mt76_regdump.py).  Read-only. */
static void mt79xx_wfdma_regblock_dump(struct ADAPTER *prAdapter, const char *tag)
{
	static bool done;
	static const struct { uint32_t base, len; } blk[] = {
		{ 0x7c024000, 0x800 }, { 0x7c026000, 0x100 },
		{ 0x7c027000, 0x100 }, { 0x7c060000, 0x100 }, { 0x74030000, 0x200 },
	};
	unsigned int b, o;
	uint32_t v;

	if (done)
		return;
	done = true;
	for (b = 0; b < ARRAY_SIZE(blk); b++)
		for (o = 0; o < blk[b].len; o += 4) {
			v = 0;
			HAL_MCR_RD(prAdapter, blk[b].base + o, &v);
			if (v)
				DBGLOG(INIT, ERROR, "[REGDUMP %s] 0x%08x=0x%08x\n",
				       tag, blk[b].base + o, v);
		}
}

/* After a command timeout: is the ROM trying to tell us something through the
 * MCU->host software interrupt / mailbox path?  Read-only. */
void mt79xx_mcu2host_dump(struct ADAPTER *prAdapter, const char *tag)
{
	mt79xx_wfdma_regblock_dump(prAdapter, tag);
	uint32_t sw_sta = 0, sw_msk = 0, h_sta = 0, h_ena = 0, mbox = 0;
	uint32_t glo = 0, romidx = 0;

	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_BASE + 0x1f0, &sw_sta);
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_BASE + 0x1f4, &sw_msk);
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, &h_sta);
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &h_ena);
	HAL_MCR_RD(prAdapter, 0x7c0600fc, &mbox);
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &glo);
	HAL_MCR_RD(prAdapter, 0x81021604, &romidx);
	DBGLOG(INIT, ERROR,
	       "[MCU2HOST %s] SW_INT_STA@0x7c0241f0=0x%08x SW_INT_MASK@0x7c0241f4=0x%08x | HOST_INT_STA=0x%08x ENA=0x%08x | MBOX@0x7c0600fc=0x%08x | GLO_CFG=0x%08x lbk(b20)=%u | ROMCODE_INDEX=0x%08x\n",
	       tag, sw_sta, sw_msk, h_sta, h_ena, mbox, glo,
	       (glo >> 20) & 1, romidx);
}

void mt7902Connac2xWfdmaManualPrefetch(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	uint32_t glo = 0;
	static unsigned int u4PrefetchCalls;

	u4PrefetchCalls++;
	DBGLOG(INIT, ERROR, "%s: call #%u ADAPTER_FLAG_HW_ERR=%d fgIsBusAccessFailed=%d\n",
		__func__, u4PrefetchCalls,
		(int)HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR),
		(int)fgIsBusAccessFailed);
	if (u4PrefetchCalls >= 2)
		dump_stack();	/* who re-enters prefetch between ECO reads and SEMA? */

	mt79xx_conn_on_misc_log(prAdapter, "prefetch-entry");

	/* Local aliases for GLO_CFG bits not in vendor headers.
	 * Values verified against mt792x_regs.h and hw datasheet. */
#ifndef MT7902_GLO_CFG_FW_DWLD_BYPASS_DMASHDL
#define MT7902_GLO_CFG_FW_DWLD_BYPASS_DMASHDL	BIT(9)
#endif
#ifndef MT7902_GLO_CFG_TX_WB_DDONE
#define MT7902_GLO_CFG_TX_WB_DDONE		BIT(6)
#endif
#ifndef MT7902_GLO_CFG_FIFO_LITTLE_ENDIAN
#define MT7902_GLO_CFG_FIFO_LITTLE_ENDIAN	BIT(12)
#endif
#ifndef MT7902_GLO_CFG_RX_WB_DDONE
#define MT7902_GLO_CFG_RX_WB_DDONE		BIT(13)
#endif
#ifndef MT7902_GLO_CFG_FIFO_DIS_CHECK
#define MT7902_GLO_CFG_FIFO_DIS_CHECK		BIT(11)
#endif
#ifndef MT7902_GLO_CFG_DMA_SIZE_3
#define MT7902_GLO_CFG_DMA_SIZE_3		(3 << 4)  /* DMA_SIZE[5:4] = 3 */
#endif
#ifndef MT7902_GLO_CFG_OMIT_TX_INFO
#define MT7902_GLO_CFG_OMIT_TX_INFO		BIT(28)
#endif
#ifndef MT7902_GLO_CFG_OMIT_RX_INFO_PFET2
#define MT7902_GLO_CFG_OMIT_RX_INFO_PFET2	BIT(21)
#endif

	/* RST_DTX_PTR: 0x7c024000 + 0x20c */
#define MT7902_WFDMA0_RST_DTX_PTR	(WF_WFDMA_HOST_DMA0_BASE + 0x20c)

#define PREFETCH(base, depth)	(((base) << 16) | (depth))

	/* Disable DMA unconditionally - EXT_CTRL writes are silently dropped
	 * while TX/RX_DMA_EN is set. The firmware may leave DMA enabled after
	 * cold boot, so we own the disable here rather than relying on caller. */
	{
		uint32_t g = 0;
		HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &g);
		DBGLOG(INIT, ERROR, "%s: GLO_CFG on entry=0x%08x\n", __func__, g);
		g &= ~(MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN);
		HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, g);
		{
			uint32_t retries = 100;
			do {
				HAL_MCR_RD(prAdapter,
					WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &g);
				if (!(g & (BIT(1) | BIT(3))))
					break;
				kalMdelay(1);
			} while (--retries);
			if (!retries)
				DBGLOG(INIT, WARN, "%s: DMA busy timeout\n", __func__);
		}
		HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &g);
		DBGLOG(INIT, ERROR, "%s: GLO_CFG after disable=0x%08x\n", __func__, g);
	}

	/* Reset WFDMA logic - this is what actually opens the EXT_CTRL write
	 * window. mt792x_dma_disable(force=true) does this in the upstream
	 * driver before any prefetch writes. Without it EXT_CTRL ghosts at 0. */
	{
		uint32_t rst_addr = WF_WFDMA_HOST_DMA0_BASE + 0x100;
		uint32_t rst_val  = 0;
		HAL_MCR_RD(prAdapter, rst_addr, &rst_val);
		rst_val &= ~(BIT(4) | BIT(5));
		HAL_MCR_WR(prAdapter, rst_addr, rst_val);
		kalMdelay(1);
		rst_val |= BIT(4) | BIT(5);
		HAL_MCR_WR(prAdapter, rst_addr, rst_val);
		kalMdelay(1);
		DBGLOG(INIT, ERROR, "%s: WFDMA logic reset done\n", __func__);
	}

	/* The logic reset above also clears every ring BASE/CNT/CIDX/DIDX CSR
	 * (seen in dmesg: R0/R15/R16 base 0x0126.... -> 0x0, cnt 0x800 -> 0x200).
	 * halWpdmaInitRing() programmed them BEFORE calling us, so without this
	 * DMA would be enabled below with base=0.  mt76 orders it
	 * disable/reset -> queue_reset (ring CSRs) -> prefetch -> enable.
	 * Re-program the ring CSRs here, after the reset and before enable.
	 */
	{
		extern void halWpdmaInitTxRing(struct GLUE_INFO *prGlueInfo);
		extern void halWpdmaInitRxRing(struct GLUE_INFO *prGlueInfo);

		halWpdmaInitTxRing(prGlueInfo);
		halWpdmaInitRxRing(prGlueInfo);
#ifndef MT7902_RX_DIDX_RESET
#define MT7902_RX_DIDX_RESET 1
#endif
#if MT7902_RX_DIDX_RESET
		/* mt76 queue reset writes RX ring 0 and 2 DIDX <- 0 */
		HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_BASE + 0x50c, 0);
		HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_BASE + 0x52c, 0);
#endif
		mt79xx_wfdma_dump(prAdapter, "prefetch-after-ring-reprogram");
	}

	mt79xx_wfdma_dump(prAdapter, "prefetch-after-logic-reset");

	/* Diagnostic: can TX_RING0_EXT_CTRL hold an arbitrary value at all?
	 * Write a throwaway pattern via the macro address and via the raw
	 * mt76 offset (WFDMA0 + 0x600) and read both back.  A pattern that
	 * sticks at one address but not the other means a wrong macro; a
	 * pattern that sticks nowhere means the block is gated/unclocked.
	 */
	{
		uint32_t rb_m = 0xbad0bad0, rb_r = 0xbad0bad0;
		const uint32_t raw = WF_WFDMA_HOST_DMA0_BASE + 0x600;

		HAL_MCR_WR(prAdapter,
			   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR,
			   0x5a5a0004);
		HAL_MCR_RD(prAdapter,
			   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR, &rb_m);
		HAL_MCR_WR(prAdapter, raw, 0x5a5a0004);
		HAL_MCR_RD(prAdapter, raw, &rb_r);
		DBGLOG(INIT, ERROR,
		       "%s: EXT_CTRL pattern test macro@0x%08x rb=0x%08x raw@0x%08x rb=0x%08x (wrote 0x5a5a0004)\n",
		       __func__,
		       (uint32_t)WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR,
		       rb_m, raw, rb_r);
	}

	/* Step 1: write EXT_CTRL prefetch registers. */

	/* RX rings */
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_EXT_CTRL_ADDR,
		   PREFETCH(0x000, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_RX_RING1_EXT_CTRL_ADDR,
		   PREFETCH(0x040, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_RX_RING2_EXT_CTRL_ADDR,
		   PREFETCH(0x080, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_RX_RING3_EXT_CTRL_ADDR,
		   PREFETCH(0x0c0, 0x4));

	/* TX rings */
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR,
		   PREFETCH(0x100, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING1_EXT_CTRL_ADDR,
		   PREFETCH(0x140, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING2_EXT_CTRL_ADDR,
		   PREFETCH(0x180, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING3_EXT_CTRL_ADDR,
		   PREFETCH(0x1c0, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING4_EXT_CTRL_ADDR,
		   PREFETCH(0x200, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING5_EXT_CTRL_ADDR,
		   PREFETCH(0x240, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING6_EXT_CTRL_ADDR,
		   PREFETCH(0x280, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING15_EXT_CTRL_ADDR,
		   PREFETCH(0x2c0, 0x4));
	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING16_EXT_CTRL_ADDR,
		   PREFETCH(0x300, 0x4));

	/* Step 2: reset DMA TX index pointers.
	 * mt76 does this between prefetch and GLO_CFG enable.
	 * Ensures hardware latches the prefetch config cleanly. */
	HAL_MCR_WR(prAdapter, MT7902_WFDMA0_RST_DTX_PTR, 0xffffffff);
	wmb();
	/* mt76 order (good trace): PRI_DLY_INT_CFG0 cleared right after
	 * RST_DTX_PTR and before GLO_CFG is written/enabled. */
	HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_BASE + 0x2f0, 0x0);

	/* Step 3: readback TX_RING0_EXT_CTRL to confirm writes stuck */
	{
		uint32_t ext = 0;
		HAL_MCR_RD(prAdapter,
			   WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR,
			   &ext);
		DBGLOG(INIT, INFO,
		       "%s: TX_RING0_EXT_CTRL after prefetch=0x%08x\n",
		       __func__, ext);
		DBGLOG(INIT, ERROR,
		       "%s: TX_RING0_EXT_CTRL readback=0x%08x (want 0x01000004)\n",
		       __func__, ext);
		if (ext == 0x0)
			DBGLOG(INIT, ERROR,
			       "%s: EXT_CTRL still ghosting after RST_DTX_PTR\n",
			       __func__);
		mt79xx_wfdma_dump(prAdapter, "prefetch-after-writes");
	}

	/* DMASHDL bypass, as mt792x_dma_disable() does and as the vendor
	 * FWDL path does (cmm_asic_connac2x.c ~1500-1536).  The logic reset above
	 * also resets DMASHDL (RST bit 5), wiping what DmaShdlInit programmed, so
	 * with the scheduler enabled but unconfigured the MCU/FWDL TX rings stall:
	 * dmesg shows TX_DMA_BUSY set (GLO_CFG bit1), R15 CIDX=1, DIDX stuck at 0.
	 * Addresses are mt76's: GLO_CFG_EXT0 = WFDMA0+0x2b0 (bit6 = DMASHDL
	 * enable), DMASHDL_SW_CONTROL = DMASHDL base (WFDMA0+0x2000) + 4 (bit28
	 * = bypass).  Verify against your headers; raw values are logged.
	 */
	{
		const uint32_t ext0_addr = WF_WFDMA_HOST_DMA0_BASE + 0x2b0;
		const uint32_t swc_addr  = WF_WFDMA_HOST_DMA0_BASE + 0x2000 + 0x04;
		uint32_t ext0 = 0xbad0bad0, swc = 0xbad0bad0;
		uint32_t ext0_b, swc_b;

		HAL_MCR_RD(prAdapter, ext0_addr, &ext0);
		HAL_MCR_RD(prAdapter, swc_addr, &swc);
		ext0_b = ext0;
		swc_b = swc;
		ext0 &= ~BIT(6);
		swc |= BIT(28);
		HAL_MCR_WR(prAdapter, ext0_addr, ext0);
		HAL_MCR_WR(prAdapter, swc_addr, swc);
		ext0 = swc = 0xbad0bad0;
		HAL_MCR_RD(prAdapter, ext0_addr, &ext0);
		HAL_MCR_RD(prAdapter, swc_addr, &swc);
		DBGLOG(INIT, ERROR,
		       "%s: DMASHDL bypass EXT0@0x%08x 0x%08x->0x%08x  SW_CTRL@0x%08x 0x%08x->0x%08x\n",
		       __func__, ext0_addr, ext0_b, ext0, swc_addr, swc_b, swc);
	}

	/* Step 4: set GLO_CFG config bits (no TX/RX_DMA_EN yet).
	 * Matches mt792x_dma_enable() bit set exactly. */
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &glo);
	glo |= MT7902_GLO_CFG_TX_WB_DDONE        |
	       MT7902_GLO_CFG_FIFO_LITTLE_ENDIAN  |
	       MT_WFDMA0_GLO_CFG_CLK_GAT_DIS      |
	       MT7902_GLO_CFG_OMIT_TX_INFO        |
	       MT7902_GLO_CFG_DMA_SIZE_3          |
	       MT7902_GLO_CFG_FIFO_DIS_CHECK      |
	       MT7902_GLO_CFG_RX_WB_DDONE         |
	       MT_WFDMA0_GLO_CFG_CSR_DISP_BASE_PTR_CHAIN_EN |
	       MT7902_GLO_CFG_OMIT_RX_INFO_PFET2  |
	       (MT7902_SET_FWDL_BYPASS_BIT ? MT7902_GLO_CFG_FW_DWLD_BYPASS_DMASHDL : 0);
#if MT7902_CLEAR_LBK_RX_Q_SEL
	glo &= ~MT7902_GLO_CFG_LBK_RX_Q_SEL_EN;
#endif
	HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, glo);

	/* Step 5: enable DMA engines separately, matching mt76 two-write
	 * sequence which ensures config bits are posted before DMA starts. */
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &glo);
#if MT7902_CLEAR_LBK_RX_Q_SEL
	glo &= ~MT7902_GLO_CFG_LBK_RX_Q_SEL_EN;
#endif
	glo |= MT_WFDMA0_GLO_CFG_TX_DMA_EN | MT_WFDMA0_GLO_CFG_RX_DMA_EN;
	HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, glo);

	/* Step 6: final readback for log confirmation */
	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &glo);
	DBGLOG(INIT, ERROR,
	       "%s: GLO_CFG after enable=0x%08x lbk_rx_q_sel(b20)=%u (clear_enabled=%d)\n",
	       __func__, glo, (glo >> 20) & 1, MT7902_CLEAR_LBK_RX_Q_SEL);
	mt79xx_wfdma_dump(prAdapter, "prefetch-after-enable");

	/* Writes seen in the mt76 mmiotrace (mt76-mmio-good.txt) between DMA
	 * enable and the first MCU command that this driver never issued:
	 *   0x7c0241f4 <- 1          (WFDMA0 + 0x1f4, MCU2HOST SW INT mask)
	 *   0x74030194 |= 0x100      (PCIE_MAC_IREG, read-modify-write)
	 * Addresses and values are trace-verified; the register names are not.
	 * Without the MCU2HOST SW INT unmasked, a ROM reply may never be
	 * signalled to the host.  Disable with -DMT7902_MT76_EXTRA_WRITES=0. */
#ifndef MT7902_MT76_EXTRA_WRITES
#define MT7902_MT76_EXTRA_WRITES	1
#endif
#if MT7902_MT76_EXTRA_WRITES
	{
		uint32_t sw_msk = 0xbad0bad0, pm = 0xbad0bad0, lp = 0xbad0bad0;
		const uint32_t sw_msk_addr = WF_WFDMA_HOST_DMA0_BASE + 0x1f4;

		/* Tail in the exact order of the good mt76 trace (good_1.txt):
		 *   0x54000120 |= NEED_REINIT (0xffff0002)
		 *   0x7c0241f4 = 1
		 *   INT_ENA = 0, INT_STA = 0, INT_ENA = 0x2807fff5
		 *   0x41f23c = 0
		 *   0x7c00e24c = 0x18451806
		 *   LPCTL = 2 (CLR_OWN)  -> first command ~20 us later
		 * -DMT7902_GOOD_INT_ENA=0 skips the INT_ENA part (IRQ-handler race);
		 * -DMT7902_E24C_SEQ=0 skips the 0x7c00e24c write. */
#ifndef MT7902_GOOD_INT_ENA
#define MT7902_GOOD_INT_ENA	1
#endif
#ifndef MT7902_E24C_SEQ
#define MT7902_E24C_SEQ	1
#endif
		{
			uint32_t dummy = 0;

			HAL_MCR_RD(prAdapter, 0x54000120, &dummy);
			HAL_MCR_WR(prAdapter, 0x54000120, dummy | BIT(1));
		}
		HAL_MCR_WR(prAdapter, sw_msk_addr, 0x1);
#if MT7902_GOOD_INT_ENA
		HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, 0x0);
		HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, 0x0);
		HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, 0x2807fff5);
#endif
		HAL_MCR_WR(prAdapter, 0x0041f23c, 0x0);
#if MT7902_E24C_SEQ
		HAL_MCR_WR(prAdapter, 0x7c00e24c, 0x18451806);
#endif
#ifndef MT7902_SET_OWN_FIRST
#define MT7902_SET_OWN_FIRST 1
#endif
#if MT7902_SET_OWN_FIRST
		/* good trace: LPCTL(0x7c060010) <- 1 (SET_OWN) before <- 2 (CLR_OWN). */
		HAL_MCR_WR(prAdapter, CONNAC2X_BN0_LPCTL_ADDR, 0x1);
#endif
		HAL_MCR_WR(prAdapter, CONNAC2X_BN0_LPCTL_ADDR, 0x2);
		kalMdelay(5);
		HAL_MCR_RD(prAdapter, CONNAC2X_BN0_LPCTL_ADDR, &lp);
		/* mt76 does this after CLR_OWN (not in the first 54 writes) */
		HAL_MCR_RD(prAdapter, 0x74030194, &pm);
		pm |= BIT(8);
		HAL_MCR_WR(prAdapter, 0x74030194, pm);
		HAL_MCR_RD(prAdapter, sw_msk_addr, &sw_msk);
		HAL_MCR_RD(prAdapter, 0x74030194, &pm);
		DBGLOG(INIT, ERROR,
		       "%s: mt76 tail: SW_INT_MASK=0x%08x (want 1) LPCTL after CLR_OWN=0x%08x PCIE_PM=0x%08x\n",
		       __func__, sw_msk, lp, pm);

		/* Experiment (inference from the trace, unconfirmed): mt76 acks
		 * INT_STA bit 27 (0x08000000, MCU2HOST SW INT) right after
		 * CLR_OWN with INT_ENA=0 around it, then re-enables INT_ENA.
		 * Here INT_STA stays 0x08000000 forever and RX0 DIDX never moves. */
#ifndef MT7902_ACK_MCU2HOST
#define MT7902_ACK_MCU2HOST	0	/* disproved: INT_STA b27 is TX-done(ring15), not MCU2HOST */
#endif
#ifndef MT7902_MT76_INT_ENA
#define MT7902_MT76_INT_ENA	0	/* 1: write INT_ENA=0x2807fff5 like mt76 (races with IRQ handler) */
#endif
#if MT7902_ACK_MCU2HOST
		{
			uint32_t sta0 = 0, sta1 = 0, cmd0 = 0, cmd1 = 0, ena = 0;
			const uint32_t cmd_addr = WF_WFDMA_HOST_DMA0_BASE + 0x1f0;

			HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, &sta0);
			HAL_MCR_RD(prAdapter, cmd_addr, &cmd0);
			HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, 0x0);
			HAL_MCR_WR(prAdapter, cmd_addr, cmd0);
			HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR,
				   sta0 ? sta0 : 0x08000000);
			HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, &sta1);
			HAL_MCR_RD(prAdapter, cmd_addr, &cmd1);
#if MT7902_MT76_INT_ENA
			HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, 0x2807fff5);
#endif
			HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &ena);
			DBGLOG(INIT, ERROR,
			       "%s: ack: INT_STA 0x%08x->0x%08x MCU_CMD(0x1f0) 0x%08x->0x%08x INT_ENA=0x%08x\n",
			       __func__, sta0, sta1, cmd0, cmd1, ena);
		}
#endif
	}
#endif

	/* Upstream waits for the MCU to report ready before the first ROM
	 * command.  Non-fatal here so the log shows what the ROM reports. */
	mt79xx_wait_mcu_ready(prAdapter, 20);

#undef PREFETCH
#undef MT7902_WFDMA0_RST_DTX_PTR
}

/* Ensure MCU Ring (15) is included in the host interrupt mask */
#define MT7902_MCU_RING_INDEX    15
#define MT7902_TX_DONE_INT_MCU   BIT(17) // Mapping for Ring 15 on this chip

void mt7902_enable_irq(struct ADAPTER *prAdapter)
{
    uint32_t u4Val;

    /* Read current enable mask */
    HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, &u4Val);

    /* Enable Data Rings AND the MCU Command Ring */
    u4Val |= (MT7902_TX_DONE_INT0 | MT7902_TX_DONE_INT1 | 
              MT7902_TX_DONE_INT2 | MT7902_TX_DONE_INT3 |
              MT7902_TX_DONE_INT_MCU); 

    HAL_MCR_WR(prAdapter, WF_WFDMA_HOST_DMA0_HOST_INT_ENA_ADDR, u4Val);
}



#if 0
static void mt7902WfdmaPreInit(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	uint32_t _glo = 0;

#define MT7902_WFDMA0_RST		(WF_WFDMA_HOST_DMA0_BASE + 0x100)
#define MT7902_WFDMA0_RST_LOGIC	BIT(4)
#define MT7902_WFDMA0_RST_DMASHDL	BIT(5)

#ifndef MT_WFDMA0_GLO_CFG_CLK_GAT_DIS
#define MT_WFDMA0_GLO_CFG_CLK_GAT_DIS	BIT(30)
#endif

	DBGLOG(HAL, ERROR, "[PRE-INIT] WFDMA logic reset\n");

	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &_glo);
	DBGLOG(HAL, ERROR, "[PRE-INIT] GLO_CFG before=0x%08x\n", _glo);

	HAL_MCR_WR(prAdapter,
		   WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR,
		   _glo | MT_WFDMA0_GLO_CFG_CLK_GAT_DIS);
	kalUdelay(10);

	HAL_MCR_RD(prAdapter, WF_WFDMA_HOST_DMA0_WPDMA_GLO_CFG_ADDR, &_glo);
	DBGLOG(HAL, ERROR, "[PRE-INIT] GLO_CFG after=0x%08x\n", _glo);

	HAL_MCR_WR(prAdapter, MT7902_WFDMA0_RST, 0);
	HAL_MCR_WR(prAdapter, MT7902_WFDMA0_RST,
		   MT7902_WFDMA0_RST_LOGIC | MT7902_WFDMA0_RST_DMASHDL);
	kalUdelay(1000);
}
#endif


void mt7902ReadIntStatus(
	struct ADAPTER *prAdapter,
	uint32_t *pu4IntStatus)
{
	uint32_t u4RegValue;
	struct GL_HIF_INFO *prHifInfo = &prAdapter->prGlueInfo->rHifInfo;
	struct BUS_INFO *prBusInfo = prAdapter->chip_info->bus_info;
	union WPDMA_INT_STA_STRUCT *prIntrStatus;
#if (CFG_CHIP_RESET_SUPPORT == 1)
	uint32_t u4IntSta;
#endif

	*pu4IntStatus = 0;
	prIntrStatus = (union WPDMA_INT_STA_STRUCT *)&u4RegValue;

	HAL_MCR_RD(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, &u4RegValue);
	{
	  //static int once;
		DBGLOG(HAL, LOUD, "[INT-RAW] u4RegValue=0x%08x rxdone_bits=0x%08x\n", u4RegValue, prBusInfo->host_int_rxdone_bits);
	}

	if (HAL_IS_CONNAC2X_EXT_RX_DONE_INTR(u4RegValue,
				       prBusInfo->host_int_rxdone_bits))
		*pu4IntStatus |= WHISR_RX0_DONE_INT;

	if (HAL_IS_CONNAC2X_EXT_TX_DONE_INTR(u4RegValue,
				       prBusInfo->host_int_txdone_bits))
		*pu4IntStatus |= WHISR_TX_DONE_INT;

	if (u4RegValue & CONNAC_MCU_SW_INT)
		*pu4IntStatus |= WHISR_D2H_SW_INT;

	if (prAdapter->u4NoMoreRfb)
		*pu4IntStatus |= WHISR_RX0_DONE_INT;


	if (prAdapter->u4NoMoreRfb & BIT(RX_RING_DATA_IDX_0)) {
		prIntrStatus->field_conn2x_single.wfdma0_rx_done_2 = 1;
		DBGLOG(HAL, ERROR, "retry process RX_RING_DATA_IDX_0\n");
	}

	if (prAdapter->u4NoMoreRfb & BIT(WFDMA0_RX_RING_IDX_2)) {
		prIntrStatus->field_conn2x_single.wfdma0_rx_done_3 = 1;
		DBGLOG(HAL, ERROR, "retry process WFDMA0_RX_RING_IDX_2\n");
	}

	/* MT7902 use RXRING 2 for WFDMA0_RX_RING_IDX_3 & RX_RING_EVT_IDX_1
	 * purpose. Keep the RX_RING_EVT_IDX_1 ID for the general drv usage.
	 */
	if (prAdapter->u4NoMoreRfb & BIT(RX_RING_EVT_IDX_1)) {
		prIntrStatus->field_conn2x_single.wfdma0_rx_done_0 = 1;
		DBGLOG(HAL, ERROR, "retry process RX_RING_EVT_IDX_1\n");
	}

	if (prAdapter->u4NoMoreRfb & BIT(WFDMA0_RX_RING_IDX_3)) {
		prIntrStatus->field_conn2x_single.wfdma0_rx_done_1 = 1;
		DBGLOG(HAL, ERROR, "retry process WFDMA0_RX_RING_IDX_3\n");
	}

	prHifInfo->u4IntStatus = u4RegValue;

	/* clear interrupt */
	HAL_MCR_WR(prAdapter,
		WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR, u4RegValue);

#if (CFG_CHIP_RESET_SUPPORT == 1)
	/* WF WDT interrupt to host via PCIE */
	if (prAdapter->eWfsysResetState == WFSYS_RESET_STATE_IDLE &&
	    prAdapter->chip_info->fgIsSupportL0p5Reset == TRUE) {
		HAL_MCR_RD(prAdapter, PCIE_MAC_IREG_ISTATUS_HOST_ADDR,
			   &u4IntSta);

		if (u4IntSta & PCIE_MAC_IREG_ISTATUS_HOST_WDT_INT_MASK)
			*pu4IntStatus |= WHISR_WDT_INT;

		HAL_MCR_WR(prAdapter, PCIE_MAC_IREG_ISTATUS_HOST_ADDR,
			   u4IntSta);
	}
#endif /* CFG_CHIP_RESET_SUPPORT */
}

#endif /*_HIF_PCIE || _HIF_AXI */

void mt7902_icapRiseVcoreClockRate(void)
{
	DBGLOG(HAL, INFO, "%s need to implement!\n", __func__);
}

void mt7902_icapDownVcoreClockRate(void)
{
	DBGLOG(HAL, INFO, "%s need to implement!\n", __func__);
}

void mt7902ConstructFirmwarePrio(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucNameTable, uint8_t **apucName,
	uint8_t *pucNameIdx, uint8_t ucMaxNameIdx)
{
	uint8_t sub_idx = 0;
	uint32_t chip_id = 0;
	int32_t ret = 0;
	struct ADAPTER *prAdapter = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	uint32_t u4FlavorVer = FLAVOR_VERSION_DEFAULT;

	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return;
	}

	prChipInfo = prAdapter->chip_info;
	if (prChipInfo == NULL) {
		DBGLOG(INIT, ERROR, "prChipInfo is NULL.\n");
		return;
	}

	chip_id = prChipInfo->chip_id;

	for (sub_idx = 0; apucNameTable[sub_idx]; sub_idx++) {
		if (((*pucNameIdx) + 3) < ucMaxNameIdx) {
#if (CFG_SUPPORT_WIFI_DL_TEST_MODE == 1)
			if (prChipInfo->fgFwdlTestMode){
				/* Type test mode. WIFI_RAM_CODE_MTxxxx_xt.bin */
				ret = snprintf(*(apucName + (*pucNameIdx)),
					CFG_FW_NAME_MAX_LEN, "mediatek/mt7902/%s%x_%xt.bin",
					apucNameTable[sub_idx], chip_id, u4FlavorVer);
				if (ret < 0)
					DBGLOG(INIT, ERROR,
					       "Type test mode snprintf failed (%d)\n",
					       ret);
				(*pucNameIdx) += 1;
				continue;
			}
#endif
			/* Type 1. WIFI_RAM_CODE_MTxxxx_x.bin */
			ret = snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "mediatek/mt7902/%s%x_%x.bin",
				apucNameTable[sub_idx], chip_id, u4FlavorVer);
			if (ret < 0)
				DBGLOG(INIT, ERROR,
				       "Type 1 snprintf failed (%d)\n", ret);
			(*pucNameIdx) += 1;

			/* Type 2. WIFI_RAM_CODE_MTxxxx_x */
			ret = snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s%x_%x",
				apucNameTable[sub_idx], chip_id, u4FlavorVer);
			if (ret < 0)
				DBGLOG(INIT, ERROR,
				       "Type 2 snprintf failed (%d)\n", ret);
			(*pucNameIdx) += 1;

			/* Type 3. WIFI_RAM_CODE_MTxxxx.bin */
			ret = snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "mediatek/mt7902/%s%x.bin",
				apucNameTable[sub_idx], chip_id);
			if (ret < 0)
				DBGLOG(INIT, ERROR,
				       "Type 3 snprintf failed (%d)\n", ret);
			(*pucNameIdx) += 1;

			/* Type 4. WIFI_RAM_CODE_MTxxxx */
			ret = snprintf(*(apucName + (*pucNameIdx)),
				CFG_FW_NAME_MAX_LEN, "%s%x",
				apucNameTable[sub_idx], chip_id);
			if (ret < 0)
				DBGLOG(INIT, ERROR,
				       "Type 4 snprintf failed (%d)\n", ret);
			(*pucNameIdx) += 1;
		} else {
			/* the table is not large enough */
			DBGLOG(INIT, ERROR,
				"kalFirmwareImageMapping >> file name array is not enough.\n");
		}
	}
}

void mt7902ConstructPatchName(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucName, uint8_t *pucNameIdx)
{
	struct ADAPTER *prAdapter = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	uint32_t u4FlavorVer = FLAVOR_VERSION_DEFAULT;
	int32_t s4Result;

	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return;
	}

	prChipInfo = prAdapter->chip_info;
	if (prChipInfo == NULL) {
		DBGLOG(INIT, ERROR, "prChipInfo is NULL.\n");
		return;
	}

	s4Result = snprintf(apucName[(*pucNameIdx)],
		CFG_FW_NAME_MAX_LEN, "mediatek/mt7902/WIFI_MT%x_patch_mcu_%x_%x_hdr.bin",
		prChipInfo->chip_id, u4FlavorVer,
		mt7961GetFwVer(prAdapter));

	if (s4Result < 0)
		DBGLOG(INIT, ERROR, "Can't setup the patch file name\n");
}

#if defined(_HIF_USB)
uint8_t mt7902Connac2xUsbEventEpDetected(IN struct ADAPTER *prAdapter)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = prAdapter->prGlueInfo;

	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		goto out;
	}

	prHifInfo = &prGlueInfo->rHifInfo;
	prHifInfo->eEventEpType = EVENT_EP_TYPE_DATA_EP;

out:
	/* MT7902 EVT use PSE, expect that the EVT is always from the EP4 */
	return USB_DATA_EP_IN;
}

uint16_t mt7902Connac2xUsbRxByteCount(
	struct ADAPTER *prAdapter,
	struct BUS_INFO *prBusInfo,
	uint8_t *pRXD)
{

	uint16_t u2RxByteCount;
	uint8_t ucPacketType;

	ucPacketType = HAL_MAC_CONNAC3X_RX_STATUS_GET_PKT_TYPE(
		(struct HW_MAC_CONNAC3X_RX_DESC *)pRXD);
	u2RxByteCount = HAL_MAC_CONNAC3X_RX_STATUS_GET_RX_BYTE_CNT(
		(struct HW_MAC_CONNAC3X_RX_DESC *)pRXD);

	/* According to Barry's rule, it can be summarized as below formula:
	 * 1. packets from WFDMA
	   -> RX padding for 4B alignment
	 * 2. packets from UMAC
	 * -> RX padding for 8B alignment first,
				then extra 4B padding
	 * 3. MT7961 Rx data packets and event packets should are all from UMAC
	 *    because of HW limitation
	 */
	/* 7902 EVT is always from PSE, and expect that
	 * CFG_SUPPORT_HOST_RX_WM_EVENT_FROM_PSE isn't enabled.
	 * Need fix this function to meet the mt7902 usage.
	 */

	u2RxByteCount = ALIGN_16(u2RxByteCount) + 12;
	DBGLOG(RX, INFO, "[mt7902 rxbc] pkt_type=%u bytecount=%u\n", ucPacketType, u2RxByteCount);

	return u2RxByteCount;
}
#endif /*_HIF_USB */

#if CFG_SUPPORT_WIFI_DL_BT_PATCH || CFG_SUPPORT_WIFI_DL_ZB_PATCH
/* BT/ZB co-download common part */

uint32_t mt7902CoDlImageSectGetPatchInfo(IN struct ADAPTER *prAdapter,
	IN void *pvFwImageMapFile, IN uint32_t u4FwImageFileLength,
	IN enum ENUM_IMG_DL_IDX_T eDlIdx,
	OUT uint32_t *pu4DataMode, OUT struct patch_dl_target *target,
	OUT uint32_t *pu4SecInfo)
{
	struct PATCH_FORMAT_V2_T *prPatchFormat;
	struct PATCH_GLO_DESC *glo_desc;
	struct PATCH_SEC_MAP *sec_map;
	struct patch_dl_buf *region;
	uint32_t section_type;
	uint32_t num_of_region, i;
	uint32_t u4Status = WLAN_STATUS_FAILURE;
	uint32_t u4BinType = 0;
	uint8_t *img_ptr;
	uint8_t aucBuffer[32];

	/* Only for MT7902 BT/ZB Patch download */

	if (!pu4DataMode || !target || !pu4SecInfo) {
		DBGLOG(INIT, ERROR, "Can't handle NULL pointer\n");
		return WLAN_STATUS_FAILURE;
	}

#if CFG_SUPPORT_WIFI_DL_BT_PATCH
	if (eDlIdx == IMG_DL_IDX_BT_PATCH)
		u4BinType = FW_SECT_BINARY_TYPE_BT_PATCH;
	else
#endif
#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
	if (eDlIdx == IMG_DL_IDX_ZB_PATCH)
		u4BinType = FW_SECT_BINARY_TYPE_ZB_FW;
	else
#endif
	{
		DBGLOG(INIT, ERROR, "MT7902 doesn't support %d type\n", eDlIdx);
		return WLAN_STATUS_FAILURE;
	}

	/* patch header */
	img_ptr = pvFwImageMapFile;
	prPatchFormat = (struct PATCH_FORMAT_V2_T *)img_ptr;

	/* Dump image information */
	kalMemZero(aucBuffer, 32);
	kalStrnCpy(aucBuffer, prPatchFormat->aucPlatform, 4);
	DBGLOG(INIT, INFO,
	       "PATCH INFO: platform[%s] HW/SW ver[0x%04X] ver[0x%04X]\n",
	       aucBuffer, prPatchFormat->u4SwHwVersion,
	       prPatchFormat->u4PatchVersion);

	kalStrnCpy(aucBuffer, prPatchFormat->aucBuildDate, 16);
	DBGLOG(INIT, INFO, "date[%s]\n", aucBuffer);

	if (prPatchFormat->u4PatchVersion != PATCH_VERSION_MAGIC_NUM) {
		DBGLOG(INIT, ERROR, "Patch format isn't V2\n");
		return WLAN_STATUS_FAILURE;
	}

	/* global descriptor */
	img_ptr += sizeof(struct PATCH_FORMAT_V2_T);
	glo_desc = (struct PATCH_GLO_DESC *)img_ptr;
	num_of_region = le2cpu32(glo_desc->section_num);
	DBGLOG(INIT, INFO,
			"\tPatch ver: 0x%x, Section num: 0x%x, subsys: 0x%x\n",
			glo_desc->patch_ver,
			num_of_region,
			le2cpu32(glo_desc->subsys));

	if (num_of_region < 0) {
		DBGLOG(INIT, WARN, "parse patch failed! num_of_region < 0.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* section map */
	img_ptr += sizeof(struct PATCH_GLO_DESC);

	/* XXX: Expect that BT PATCH & ZB FW only occupy one section */
	target->num_of_region = 1;
	target->patch_region = (struct patch_dl_buf *)kalMemAlloc(
				sizeof(struct patch_dl_buf), PHY_MEM_TYPE);

	if (!target->patch_region) {
		DBGLOG(INIT, WARN, "No memory to allocate.\n");
		return WLAN_STATUS_FAILURE;
	}

	region = &target->patch_region[0];
	region->img_ptr = NULL;
	for (i = 0; i < num_of_region; i++) {
		sec_map = (struct PATCH_SEC_MAP *)img_ptr;
		img_ptr += sizeof(struct PATCH_SEC_MAP);

		section_type = le2cpu32(sec_map->section_type);

		if ((section_type & PATCH_SEC_TYPE_MASK) !=
		     PATCH_SEC_TYPE_BIN_INFO)
			continue;

		region->bin_type = le2cpu32(sec_map->bin_info_spec.bin_type);
		/* only handle BT/ZB Patch */
		if (region->bin_type != u4BinType)
			continue;

		region->img_dest_addr =
			le2cpu32(sec_map->bin_info_spec.dl_addr);
		/* PDA needs 16-byte aligned length */
		region->img_size =
			le2cpu32(sec_map->bin_info_spec.dl_size) +
			le2cpu32(sec_map->bin_info_spec.align_len);
		if (!(region->img_size % 16))
			DBGLOG(INIT, WARN,
			       "%d Patch is not 16-byte aligned\n", eDlIdx);
		region->img_ptr = pvFwImageMapFile +
			le2cpu32(sec_map->section_offset);
		*pu4SecInfo = le2cpu32(sec_map->bin_info_spec.sec_info);

		DBGLOG(INIT, INFO, "%d Patch addr=0x%x: size=%d, ptr=0x%p\n",
			eDlIdx, region->img_dest_addr, region->img_size,
			region->img_ptr);

		u4Status = WLAN_STATUS_SUCCESS;
	}

	*pu4DataMode = wlanGetPatchDataModeV2(prAdapter, *pu4SecInfo);

	if (region->img_ptr == NULL) {
		DBGLOG(INIT, ERROR, "Can't find the FW\n");
		kalMemFree(target->patch_region, PHY_MEM_TYPE,
			sizeof(struct patch_dl_buf));
	}

	return u4Status;
}


uint32_t mt7902CoDlPatchSendSemaCtl(IN struct ADAPTER *prAdapter,
				    IN uint32_t u4Addr,
				    IN enum ENUM_IMG_DL_IDX_T eDlIdx,
				    IN uint32_t u4SecInfo,
				    OUT uint8_t *pucSeqNum)
{
	struct mt66xx_chip_info *prChipInfo = NULL;
	struct CMD_INFO *prCmdInfo;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
#if CFG_SUPPORT_WIFI_DL_BT_PATCH
	struct INIT_CMD_BT_PATCH_SEMA_CTRL *prPatchSemaCtrl;
	uint32_t u4SemaCtrlSize = sizeof(struct INIT_CMD_BT_PATCH_SEMA_CTRL);
#elif CFG_SUPPORT_WIFI_DL_ZB_PATCH
	struct INIT_CMD_ZB_PATCH_SEMA_CTRL *prPatchSemaCtrl;
	uint32_t u4SemaCtrlSize = sizeof(struct INIT_CMD_ZB_PATCH_SEMA_CTRL);
#endif
	uint32_t u4CmdSize = sizeof(struct INIT_HIF_TX_HEADER) +
		    sizeof(struct INIT_HIF_TX_HEADER_PENDING_FOR_HW_32BYTES) +
		    u4SemaCtrlSize;
	char *ucHifName = NULL;

	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prChipInfo = prAdapter->chip_info;
	if (prChipInfo == NULL) {
		DBGLOG(INIT, ERROR, "prChipInfo is NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 1. Allocate CMD Info Packet and its Buffer. */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, u4CmdSize);

	if (!prCmdInfo) {
		DBGLOG(INIT, ERROR, "Allocate CMD_INFO_T ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	prCmdInfo->u2InfoBufLen = u4CmdSize;

#if CFG_SUPPORT_WIFI_DL_BT_PATCH
	if (eDlIdx == IMG_DL_IDX_BT_PATCH) {
		prCmdInfo->ucCID = INIT_CMD_ID_BT_PATCH_SEMAPHORE_CONTROL;
		ucHifName = "BT";
	} else
#endif
#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
	if (eDlIdx == IMG_DL_IDX_ZB_PATCH) {
		prCmdInfo->ucCID = INIT_CMD_ID_ZB_PATCH_SEMAPHORE_CONTROL;
		ucHifName = "ZB";
	} else
#endif
	{
		DBGLOG(INIT, ERROR, "Not support %d Patch\n", eDlIdx);
		goto out;
	}

	NIC_FILL_CMD_TX_HDR(prAdapter,
		prCmdInfo->pucInfoBuffer,
		prCmdInfo->u2InfoBufLen,
		prCmdInfo->ucCID,
		INIT_CMD_PDA_PACKET_TYPE_ID,
		pucSeqNum, FALSE,
		(void **)&prPatchSemaCtrl, TRUE, 0, S2D_INDEX_CMD_H2N,
		FALSE);

	/* Setup DOWNLOAD_BUF. BT's/ZB's prPatchSemaCtrl have the same size */
	kalMemZero(prPatchSemaCtrl, u4SemaCtrlSize);
	prPatchSemaCtrl->ucGetSemaphore = PATCH_GET_SEMA_CONTROL;
	prPatchSemaCtrl->u4Addr = u4Addr;
#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
	if (eDlIdx == IMG_DL_IDX_ZB_PATCH)
		((struct INIT_CMD_ZB_PATCH_SEMA_CTRL *)prPatchSemaCtrl)
				->u4SecInfo = u4SecInfo;
#endif /* CFG_SUPPORT_WIFI_DL_ZB_PATCH */

	/* 4. Send FW_Download command */
	if (nicTxInitCmd(prAdapter, prCmdInfo,
			 prChipInfo->u2TxInitCmdPort) != WLAN_STATUS_SUCCESS) {
		u4Status = WLAN_STATUS_FAILURE;
		DBGLOG(INIT, ERROR, "send Get %s Patch Sema fail\n", ucHifName);
	} else
		DBGLOG(INIT, INFO, "send Get %s Patch Sema success\n",
			ucHifName);

out:
	/* 5. Free CMD Info Packet. */
	cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	return u4Status;
}

uint32_t mt7902CoDlPatchRecvSemaResp(IN struct ADAPTER *prAdapter,
				     IN uint8_t ucCmdSeqNum,
				     IN enum ENUM_IMG_DL_IDX_T eDlIdx,
				     OUT uint8_t *pucPatchStatus,
				     OUT uint32_t *u4RemapAddr)
{
	struct mt66xx_chip_info *prChipInfo;
	uint8_t *aucBuffer;
	uint32_t u4EventSize;
	struct INIT_WIFI_EVENT *prInitEvent;
	/* Expect that ZB use the BT's event format */
	struct INIT_EVENT_BT_PATCH_SEMA_CTRL *prEventCmdResult;
	uint32_t u4RxPktLength = 0;
	uint32_t u4Status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
	    || fgIsBusAccessFailed == TRUE)
		return WLAN_STATUS_FAILURE;

	u4EventSize = prChipInfo->rxd_size + prChipInfo->init_event_size +
		sizeof(struct INIT_EVENT_BT_PATCH_SEMA_CTRL);
	aucBuffer = kalMemAlloc(u4EventSize, PHY_MEM_TYPE);
	if (aucBuffer == NULL) {
		DBGLOG(INIT, ERROR, "Alloc CMD buffer failed\n");
		return WLAN_STATUS_FAILURE;
	}

	if (nicRxWaitResponse(prAdapter, 0, aucBuffer, u4EventSize,
			      &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, WARN, "Wait patch semaphore response fail\n");
		goto out;
	}

	prInitEvent = (struct INIT_WIFI_EVENT *)
		(aucBuffer + prChipInfo->rxd_size);

#if CFG_SUPPORT_WIFI_DL_BT_PATCH
	if ((eDlIdx == IMG_DL_IDX_BT_PATCH)
		&& (prInitEvent->ucEID == INIT_EVENT_ID_BT_PATCH_SEMA_CTRL))
		goto get_resp;
#endif
#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
	if ((eDlIdx == IMG_DL_IDX_ZB_PATCH)
		&& (prInitEvent->ucEID == INIT_EVENT_ID_ZB_PATCH_SEMA_CTRL))
		goto get_resp;
#endif

	DBGLOG(INIT, ERROR, "Unexpected EVENT ID 0x%0x\n", prInitEvent->ucEID);
	goto out;

get_resp:
	/* BT always response prInitEvent->ucSeqNum=0 */
	prEventCmdResult = (struct INIT_EVENT_BT_PATCH_SEMA_CTRL *)
		prInitEvent->aucBuffer;

	*pucPatchStatus = prEventCmdResult->ucStatus;
	*u4RemapAddr = prEventCmdResult->u4RemapAddr;

	u4Status = WLAN_STATUS_SUCCESS;
out:
	kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);

	return u4Status;
}

int32_t mt7902CoDlPatchIsDownloaded(IN struct ADAPTER *prAdapter,
				IN uint32_t u4DestAddr,
				IN enum ENUM_IMG_DL_IDX_T eDlIdx,
				IN uint32_t u4SecInfo,
				OUT uint32_t *u4TargetAddr)
{
	uint8_t ucSeqNum = 0, ucPatchStatus;
	uint32_t rStatus;
	uint32_t u4Count;
	uint32_t u4RemapAddr;
	int32_t s4RetStatus = -1;

	ucPatchStatus = PATCH_STATUS_NO_SEMA_NEED_PATCH;
	u4Count = 0;

	while (ucPatchStatus == PATCH_STATUS_NO_SEMA_NEED_PATCH) {
		if (u4Count)
			kalMdelay(100);

		rStatus = mt7902CoDlPatchSendSemaCtl(prAdapter, u4DestAddr,
						eDlIdx,	u4SecInfo, &ucSeqNum);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, WARN,
			       "Send patch SEMA control CMD failed!!\n");
			goto out;
		}

		rStatus = mt7902CoDlPatchRecvSemaResp(prAdapter, ucSeqNum,
					eDlIdx, &ucPatchStatus, &u4RemapAddr);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, WARN,
			       "Recv patch SEMA control EVT failed!!\n");
			goto out;
		}

		u4Count++;

		if (u4Count > 50) {
			DBGLOG(INIT, WARN, "Patch status check timeout!!\n");
			break;
		}
	}

	if (ucPatchStatus != PATCH_STATUS_NO_NEED_TO_PATCH)
		*u4TargetAddr = u4RemapAddr;

	s4RetStatus = (ucPatchStatus == PATCH_STATUS_NO_NEED_TO_PATCH);

out:
	return s4RetStatus;
}

uint32_t mt7902PatchCoDownload(IN struct ADAPTER *prAdapter,
			       IN enum ENUM_IMG_DL_IDX_T eDlIdx)
{
	uint32_t u4FwSize = 0;
	uint32_t u4Status = WLAN_STATUS_FAILURE;
	uint32_t u4DataMode;
	uint32_t u4RemapAddr;
	uint32_t u4SecInfo = 0;
	int32_t s4PatchCheck;
	uint8_t ucPatchType;
	struct patch_dl_target target;
	struct patch_dl_buf *region = NULL;
	void *prFwBuffer = NULL;
	char *ucHifName = NULL;

#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	#pragma message("WARN: Download BT Patch doesn't support COMPRESSION")
#endif
#if CFG_DOWNLOAD_DYN_MEMORY_MAP
	#pragma message("WARN: Download BT Patch doesn't support DYN_MEM_MAP")
#endif
#if CFG_ROM_PATCH_NO_SEM_CTRL
	#pragma message("WARN: Download BT Patch doesn't support NO_SEM_CTRL")
#endif

	/* Expect the eDlIdx == IMG_DL_IDX_BT_PATCH || IMG_DL_IDX_ZB_PATCH */

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;

#if CFG_SUPPORT_WIFI_DL_BT_PATCH
	if (eDlIdx == IMG_DL_IDX_BT_PATCH) {
		ucHifName = "BT";
		ucPatchType = PATCH_FNSH_TYPE_BT;
	} else
#endif /* CFG_SUPPORT_WIFI_DL_BT_PATCH */
#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
	if (eDlIdx == IMG_DL_IDX_ZB_PATCH) {
		ucHifName = "ZB";
		ucPatchType = PATCH_FNSH_TYPE_ZB;
	} else
#endif /* CFG_SUPPORT_WIFI_DL_ZB_PATCH */
	{
		DBGLOG(INIT, ERROR, "Not expect Radio Type [%d]\n", eDlIdx);
		return WLAN_STATUS_FAILURE;
	}

	/* Always check BT Patch Download for L0.5 reset case */

	/* refer from wlanImageSectionDownloadStage */

	/* step.1 open the PATCH file */
	kalFirmwareImageMapping(prAdapter->prGlueInfo, &prFwBuffer,
				&u4FwSize, eDlIdx);
	if (prFwBuffer == NULL) {
		DBGLOG(INIT, WARN, "FW[%s] load error!\n", ucHifName);
		return WLAN_STATUS_FAILURE;
	}

	/* step 2. get Addr info. Refer from : wlanImageSectionDownloadStage.
	 *	   ZB needs the usSecInfo.
	 */
	u4Status = mt7902CoDlImageSectGetPatchInfo(prAdapter, prFwBuffer,
			u4FwSize, eDlIdx, &u4DataMode, &target, &u4SecInfo);

	if (u4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "Can't find the Patch Section\n");
		goto out;
	}

	/* step 3. check BT/ZB doesn't download PATCH */
	region = &target.patch_region[0];

	s4PatchCheck = mt7902CoDlPatchIsDownloaded(prAdapter,
		region->img_dest_addr, eDlIdx, u4SecInfo, &u4RemapAddr);
	if (s4PatchCheck < 0) {
		DBGLOG(INIT, ERROR, "Get %s Semaphore Fail\n", ucHifName);
		u4Status =  WLAN_STATUS_FAILURE;
		goto out;
	} else if (s4PatchCheck == 1) {
		DBGLOG(INIT, STATE, "No need to download %s patch\n",
			ucHifName);
		u4Status =  WLAN_STATUS_SUCCESS;
		goto out;
	}

	region->img_dest_addr = u4RemapAddr;

	/* step 4. download patch */
	u4Status = wlanDownloadSectionV2(prAdapter, u4DataMode,
					eDlIdx, &target);
	if (u4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "%s Patch download Fail\n", ucHifName);
		goto out;
	}

	/* step 5. send INIT_CMD_PATCH_FINISH */
	u4Status = wlanPatchSendComplete(prAdapter, ucPatchType);

	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "Send INIT_CMD_PATCH_FINISH Fail\n");

out:
	if (target.patch_region != NULL) {
		/* This case is that the BT patch isn't downloaded this time.
		 * The original free action is in wlanDownloadSectionV2().
		 */
		kalMemFree(target.patch_region, PHY_MEM_TYPE,
			sizeof(struct patch_dl_buf) * target.num_of_region);
		target.patch_region = NULL;
		target.num_of_region = 0;
	}

	kalFirmwareImageUnmapping(prAdapter->prGlueInfo, NULL, prFwBuffer);

	return u4Status;
}
#endif /* CFG_SUPPORT_WIFI_DL_BT_PATCH || CFG_SUPPORT_WIFI_DL_ZB_PATCH */

#if CFG_SUPPORT_WIFI_DL_BT_PATCH
void mt7902ConstructBtPatchName(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucName, uint8_t *pucNameIdx)
{
	struct ADAPTER *prAdapter = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	uint32_t u4FlavorVer = FLAVOR_VERSION_DEFAULT;
	int32_t s4Result;

	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return;
	}

	if ((pucNameIdx == NULL) || (apucName == NULL)) {
		DBGLOG(INIT, ERROR, "pucNameIdx or apucName are NULL.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return;
	}

	prChipInfo = prAdapter->chip_info;
	if (prChipInfo == NULL) {
		DBGLOG(INIT, ERROR, "prChipInfo is NULL.\n");
		return;
	}

	s4Result = snprintf(apucName[(*pucNameIdx)],
		CFG_FW_NAME_MAX_LEN, "mediatek/mt7902/BT_RAM_CODE_MT%x_%x_%x_hdr.bin",
		prChipInfo->chip_id, u4FlavorVer,
		mt7961GetFwVer(prAdapter));
	if (s4Result < 0)
		DBGLOG(INIT, ERROR, "Can't setup the BT patch file name\n");
}

uint32_t mt7902DownloadBtPatch(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Status = WLAN_STATUS_FAILURE;

	u4Status = mt7902PatchCoDownload(prAdapter, IMG_DL_IDX_BT_PATCH);

	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "Download BT Patch fail\n");
	else
		DBGLOG(INIT, STATE, "Download BT Patch success\n");

	return u4Status;
}
#endif /* CFG_SUPPORT_WIFI_DL_BT_PATCH */

#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
void mt7902ConstructZbPatchName(struct GLUE_INFO *prGlueInfo,
	uint8_t **apucName, uint8_t *pucNameIdx)
{
	struct ADAPTER *prAdapter = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	uint32_t u4FlavorVer = FLAVOR_VERSION_DEFAULT;
	int32_t s4Result;

	if (prGlueInfo == NULL) {
		DBGLOG(INIT, ERROR, "prGlueInfo is NULL.\n");
		return;
	}

	if ((pucNameIdx == NULL) || (apucName == NULL)) {
		DBGLOG(INIT, ERROR, "pucNameIdx or apucName are NULL.\n");
		return;
	}

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter is NULL.\n");
		return;
	}

	prChipInfo = prAdapter->chip_info;
	if (prChipInfo == NULL) {
		DBGLOG(INIT, ERROR, "prChipInfo is NULL.\n");
		return;
	}

	s4Result = snprintf(apucName[(*pucNameIdx)],
		CFG_FW_NAME_MAX_LEN, "mediatek/mt7902/ZB_RAM_CODE_MT%x_%x_%x_hdr.bin",
		prChipInfo->chip_id, u4FlavorVer,
		mt7961GetFwVer(prAdapter));
	if (s4Result < 0)
		DBGLOG(INIT, ERROR, "Can't setup the ZB patch file name\n");
}

uint32_t mt7902DownloadZbPatch(IN struct ADAPTER *prAdapter)
{
	uint32_t u4Status = WLAN_STATUS_FAILURE;

	u4Status = mt7902PatchCoDownload(prAdapter, IMG_DL_IDX_ZB_PATCH);

	if (u4Status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "Download ZB Patch fail\n");
	else
		DBGLOG(INIT, STATE, "Download ZB Patch success\n");

	return u4Status;
}
#endif /* CFG_SUPPORT_WIFI_DL_ZB_PATCH */

uint32_t mt7902ConstructBufferBinFileName(struct ADAPTER *prAdapter,
					  uint8_t *aucEeprom)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4FlavorVer = FLAVOR_VERSION_DEFAULT;
	int32_t s4Result;

	if (prAdapter == NULL) {
		DBGLOG(INIT, ERROR, "prAdapter == NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo == NULL) {
		DBGLOG(INIT, ERROR, "prChipInfo == NULL\n");
		return WLAN_STATUS_FAILURE;
	}

	s4Result = snprintf(aucEeprom, 32, "mediatek/mt7902/EEPROM_MT%x_%x.bin",
		 prChipInfo->chip_id, u4FlavorVer);

	if (s4Result < 0) {
		DBGLOG(INIT, ERROR, "Can't setup the Buffer BIN file name\n");
		return WLAN_STATUS_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}



u_int8_t mt7902GetRxDbgInfoSrc(struct ADAPTER *prAdapter)
{
	return TRUE;
}

struct BUS_INFO mt7902_bus_info = {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	.top_cfg_base = MT7902_TOP_CFG_BASE,

	/* host_dma0 for TXP */
	.host_dma0_base = WF_WFDMA_HOST_DMA0_BASE,
	.host_int_status_addr = WF_WFDMA_HOST_DMA0_HOST_INT_STA_ADDR,

	.host_int_txdone_bits =
		(CONNAC2X_WFDMA_TX_DONE_INT0 | CONNAC2X_WFDMA_TX_DONE_INT1 |
		CONNAC2X_WFDMA_TX_DONE_INT2 | CONNAC2X_WFDMA_TX_DONE_INT3 |
		CONNAC2X_WFDMA_TX_DONE_INT4 | CONNAC2X_WFDMA_TX_DONE_INT5 |
		CONNAC2X_WFDMA_TX_DONE_INT6 | CONNAC2X_WFDMA_TX_DONE_INT16 |
		/* MT7902 CMD TX ring id is 15, but its interrupt id is 17 */
		CONNAC2X_WFDMA_TX_DONE_INT17),
	.host_int_rxdone_bits =
		(CONNAC2X_WFDMA_RX_DONE_INT0 | CONNAC2X_WFDMA_RX_DONE_INT1 |
		 CONNAC2X_WFDMA_RX_DONE_INT2 | CONNAC2X_WFDMA_RX_DONE_INT3),

	.host_tx_ring_base = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL0_ADDR,
	.host_tx_ring_ext_ctrl_base =
		WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_EXT_CTRL_ADDR,
	.host_tx_ring_cidx_addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL2_ADDR,
	.host_tx_ring_didx_addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL3_ADDR,
	.host_tx_ring_cnt_addr = WF_WFDMA_HOST_DMA0_WPDMA_TX_RING0_CTRL1_ADDR,

	.host_rx_ring_base = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL0_ADDR,
	.host_rx_ring_ext_ctrl_base =
		WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_EXT_CTRL_ADDR,
	.host_rx_ring_cidx_addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL2_ADDR,
	.host_rx_ring_didx_addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL3_ADDR,
	.host_rx_ring_cnt_addr = WF_WFDMA_HOST_DMA0_WPDMA_RX_RING0_CTRL1_ADDR,

	.bus2chip = mt7902_bus2chip_cr_mapping,
	.bus2chip_tbl_size = MT7902_BUS2CHIP_MEM_MAP_TBLE_SIZE,
	.max_static_map_addr = 0x00100000,

	.tx_ring_fwdl_idx = CONNAC2X_FWDL_TX_RING_IDX,
	.tx_ring_cmd_idx = MT7902_CMD_TX_RING_IDX,
	.tx_ring0_data_idx = 0,
	.tx_ring1_data_idx = 1,
	.fw_own_clear_addr = CONNAC2X_BN0_IRQ_STAT_ADDR,

	/* Hook your IRQ functions here */
    .enableInterrupt = mt7902_enable_interrupt,
    .disableInterrupt = mt7902_disable_interrupt,

	.fw_own_clear_bit = PCIE_LPCR_FW_CLR_OWN,
	.fgCheckDriverOwnInt = FALSE,
	.u4DmaMask = 32,
	.pdmaSetup = asicConnac2xWpdmaConfig,
	.pdmaStop = asicConnac2xWfdmaStop,
	.pdmaPollingIdle = asicConnac2xWfdmaPollingAllIdle,
	.enableInterrupt = mt7902EnableInterrupt,
	.disableInterrupt = mt7902DisableInterrupt,
	.processTxInterrupt = mt7902Connac2xProcessTxInterrupt,
	.processRxInterrupt = mt7902Connac2xProcessRxInterrupt,
	.tx_ring_ext_ctrl = mt7902WfdmaTxRingExtCtrl,
	.rx_ring_ext_ctrl = mt7902WfdmaRxRingExtCtrl,
	/* MT7902 event & rx freed done evt use the same ring.
	 * Dut to the sw rfb usage, the buf size is set as CFG_RX_MAX_PKT_SIZE.
	 */
	.rx_evt_ring_buf_size = CFG_RX_MAX_PKT_SIZE,
	/* null wfdmaManualPrefetch if want to disable manual mode */
	.wfdmaManualPrefetch = mt7902Connac2xWfdmaManualPrefetch,
	//.wfdmaPreInit = mt7902WfdmaPreInit,
	.lowPowerOwnRead = asicConnac2xLowPowerOwnRead,
	.lowPowerOwnSet = asicConnac2xLowPowerOwnSet,
	.lowPowerOwnClear = asicConnac2xLowPowerOwnClear,
	.wakeUpWiFi = asicWakeUpWiFi,
	.getSoftwareInterrupt = asicConnac2xGetSoftwareInterrupt,
	.processSoftwareInterrupt = asicConnac2xProcessSoftwareInterrupt,
	.softwareInterruptMcu = asicConnac2xSoftwareInterruptMcu,
	.hifRst = asicConnac2xHifRst,
	.initPcieInt = wlanMt7902InitPcieInt,
	.devReadIntStatus = mt7902ReadIntStatus,
	.DmaShdlInit = mt7902DmashdlInit,
	.DmaShdlReInit = mt7902DmashdlReInit,
	.setRxRingHwAddr = mt7902SetRxRingHwAddr,
	.wfdmaAllocRxRing = mt7902WfdmaAllocRxRing,
#if CFG_SUPPORT_PCIE_ASPM_IMPROVE
	.configPcieASPM = mt7961ConfigPcieASPM,
	.setCTSbyRate = mt7961SetCTSbyRate,
#endif
#if (CFG_COALESCING_INTERRUPT == 1)
#if defined(_HIF_PCIE)
	.setWfdmaCoalescingInt = mt7961setWfdmaCoalescingInt,
#else
	.setWfdmaCoalescingInt = NULL,
#endif
#endif
#endif /*_HIF_PCIE || _HIF_AXI */

#if defined(_HIF_USB)
	.u4UdmaWlCfg_0_Addr = CONNAC2X_UDMA_WLCFG_0,
	.u4UdmaWlCfg_1_Addr = CONNAC2X_UDMA_WLCFG_1,
	.u4UdmaWlCfg_0 =
	    (CONNAC2X_UDMA_WLCFG_0_WL_TX_EN(1) |
	     CONNAC2X_UDMA_WLCFG_0_WL_RX_EN(1) |
	     CONNAC2X_UDMA_WLCFG_0_WL_RX_MPSZ_PAD0(1) |
	     CONNAC2X_UDMA_WLCFG_0_TICK_1US_EN(1)),
	.u4UdmaTxQsel = CONNAC2X_UDMA_TX_QSEL,
	.u4UdmaConnInfraStatusSelAddr = MT7902_UDMA_CONN_INFRA_STATUS_SEL,
	.u4UdmaConnInfraStatusSelVal = MT7902_UDMA_CONN_INFRA_STATUS_VAL,
	.u4UdmaConnInfraStatusAddr = MT7902_UDMA_CONN_INFRA_STATUS,
	.u4device_vender_request_in = DEVICE_VENDOR_REQUEST_IN_CONNAC2,
	.u4device_vender_request_out = DEVICE_VENDOR_REQUEST_OUT_CONNAC2,
	.fgIsSupportWdtEp = TRUE,
	.asicUsbEventEpDetected = mt7902Connac2xUsbEventEpDetected,
	.asicUsbRxByteCount = mt7902Connac2xUsbRxByteCount,
	.u4SuspendVer = SUSPEND_V2,
	.asicUsbSuspend = NULL, /*asicUsbSuspend*/
	.asicUsbResume = asicConnac2xUsbResume,
	.DmaShdlInit = mt7902DmashdlInit,
	.DmaShdlReInit = mt7902DmashdlReInit,
	.updateTxRingMaxQuota = mt7902UpdateDmashdlQuota,
	.asicUdmaRxFlush = asicConnac2xUdmaRxFlush,
#if CFG_CHIP_RESET_SUPPORT
	.asicUsbEpctlRstOpt = mt7961HalUsbEpctlRstOpt,
#endif
#endif /* _HIF_USB */
#if defined(_HIF_SDIO)
	.halTxGetFreeResource = halTxGetFreeResource_v1,
	.halTxReturnFreeResource = halTxReturnFreeResource_v1,
	.halRestoreTxResource = halRestoreTxResource_v1,
	.halUpdateTxDonePendingCount = halUpdateTxDonePendingCount_v1,
#endif /* _HIF_SDIO */
};

#if CFG_ENABLE_FW_DOWNLOAD
struct FWDL_OPS_T mt7902_fw_dl_ops = {
	.constructFirmwarePrio = mt7902ConstructFirmwarePrio,
	.constructPatchName = mt7902ConstructPatchName,
	.downloadPatch = wlanDownloadPatch,
	.downloadFirmware = wlanConnacFormatDownload,
	.getFwInfo = wlanGetConnacFwInfo,
	.getFwDlInfo = asicGetFwDlInfo,
#if CFG_SUPPORT_WIFI_DL_BT_PATCH
	.constructBtPatchName = mt7902ConstructBtPatchName,
	.downloadBtPatch = mt7902DownloadBtPatch,
#endif
#if CFG_SUPPORT_WIFI_DL_ZB_PATCH
	.constructZbPatchName = mt7902ConstructZbPatchName,
	.downloadZbPatch = mt7902DownloadZbPatch,
#endif
};
#endif /* CFG_ENABLE_FW_DOWNLOAD */

struct TX_DESC_OPS_T mt7902TxDescOps = {
	.fillNicAppend = fillNicTxDescAppend,
	.fillHifAppend = fillTxDescAppendByHostV2,
	.fillTxByteCount = fillConnac2xTxDescTxByteCount,
};

struct RX_DESC_OPS_T mt7902RxDescOps = {
	.nic_rxd_get_rx_byte_count = nic_rxd_v2_get_rx_byte_count,
	.nic_rxd_get_pkt_type = nic_rxd_v2_get_packet_type,
	.nic_rxd_get_wlan_idx = nic_rxd_v2_get_wlan_idx,
	.nic_rxd_get_sec_mode = nic_rxd_v2_get_sec_mode,
	.nic_rxd_get_sw_class_error_bit = nic_rxd_v2_get_sw_class_error_bit,
	.nic_rxd_get_ch_num = nic_rxd_v2_get_ch_num,
	.nic_rxd_get_rf_band = nic_rxd_v2_get_rf_band,
	.nic_rxd_get_tcl = nic_rxd_v2_get_tcl,
	.nic_rxd_get_ofld = nic_rxd_v2_get_ofld,
	.nic_rxd_get_HdrTrans = nic_rxd_v2_get_HdrTrans,
	.nic_rxd_fill_rfb = nic_rxd_v2_fill_rfb,
	.nic_rxd_sanity_check = nic_rxd_v2_sanity_check,
	.nic_rxd_check_wakeup_reason = nic_rxd_v2_check_wakeup_reason,
	
};


/* Wrapper: the chip's SOP dump kills ROMCODE_INDEX/bus (0x1d1e -> 0xdead0003)
 * when run pre-FW, and it is reached from paths in nic_rx.c that the hal_pdma.c
 * guard does not cover.  Skip it until firmware is downloaded. */
#ifndef MT7902_SKIP_SOP_DUMP_PREFW
#define MT7902_SKIP_SOP_DUMP_PREFW 1
#endif
static u_int8_t mt7902_show_debug_sop_info_guarded(struct ADAPTER *prAdapter,
						   uint8_t ucCase)
{
#if MT7902_SKIP_SOP_DUMP_PREFW
	if (!prAdapter->fgIsFwDownloaded) {
		printk(KERN_ERR "[mt7902] [SOP-GUARD] pre-FW: skipping show_debug_sop_info\n");
		return 0;
	}
#endif
	return mt7902_show_debug_sop_info(prAdapter, ucCase);
}

struct CHIP_DBG_OPS mt7902DebugOps = {
	.showPdmaInfo = mt7902_show_wfdma_info,
	.showPseInfo = mt7961_show_pse_info,
	.showPleInfo = mt7902_show_ple_info,
	.showTxdInfo = connac2x_show_txd_Info,
	.showWtblInfo = connac2x_show_wtbl_info,
	.showUmacFwtblInfo = connac2x_show_umac_wtbl_info,
	.showCsrInfo = NULL,
	.showDmaschInfo = NULL,
	.showHifInfo = NULL,
	.printHifDbgInfo = NULL,
	.show_rx_rate_info = connac2x_show_rx_rate_info,
	.show_rx_rssi_info = connac2x_show_rx_rssi_info,
	.show_stat_info = connac2x_show_stat_info,
#if (CFG_SUPPORT_LINK_QUALITY_MONITOR == 1)
	.get_rx_rate_info = connac2x_get_rx_rate_info,
#endif
#if defined(_HIF_SDIO)
	.show_mcu_debug_info = mt7902_sdio_show_mcu_debug_info,
#elif defined(_HIF_USB)
	.show_mcu_debug_info = usb_show_mcu_debug_info,
#elif defined(_HIF_PCIE)
	.show_mcu_debug_info = pcie_show_mcu_debug_info,
#endif
	.show_conninfra_debug_info = NULL,
#if (CFG_SUPPORT_DEBUG_SOP == 1)
	.show_debug_sop_info = mt7902_show_debug_sop_info_guarded,
#endif
};

#if CFG_SUPPORT_QA_TOOL
struct ATE_OPS_T mt7902_AteOps = {
	/*ICapStart phase out , wlan_service instead*/
	.setICapStart = connacSetICapStart,
	/*ICapStatus phase out , wlan_service instead*/
	.getICapStatus = connacGetICapStatus,
	/*CapIQData phase out , wlan_service instead*/
	.getICapIQData = connacGetICapIQData,
	.getRbistDataDumpEvent = nicExtEventICapIQData,
	.icapRiseVcoreClockRate = mt7902_icapRiseVcoreClockRate,
	.icapDownVcoreClockRate = mt7902_icapDownVcoreClockRate,
	.u4EnBitWidth = 4, /*no translate, keep 128 bit*/
	.u4Architech = 0,  /*0:on-chip*/
	.u4PhyIdx = 0,
	.u4EmiStartAddress = 0,
	.u4EmiEndAddress = 0,
	.u4EmiMsbAddress = 0,
};
#endif

u_int8_t mt7902_check_mmio_alive(struct ADAPTER *prAdapter)
{
    uint32_t u4Val = 0;
    
    /* Using the Connectivity Config Chip ID register for the heartbeat check */
    HAL_MCR_RD(prAdapter, MT7902_SW_SYNC0, &u4Val);
    
    /* If the bus is hanging or disconnected, we get all 1s or all 0s */
    if (u4Val == 0xFFFFFFFF)
        return FALSE;
        
    return TRUE;
}


/* Litien code refine to support multi chip */
struct mt66xx_chip_info mt66xx_chip_info_mt7902 = {
  .checkMmioAlive = mt7902_check_mmio_alive,
	.bus_info = &mt7902_bus_info,
#if CFG_ENABLE_FW_DOWNLOAD
	.fw_dl_ops = &mt7902_fw_dl_ops,
#endif /* CFG_ENABLE_FW_DOWNLOAD */
#if CFG_SUPPORT_QA_TOOL
	.prAteOps = &mt7902_AteOps,
#endif
	.prTxDescOps = &mt7902TxDescOps,
	.prRxDescOps = &mt7902RxDescOps,
	.prDebugOps = &mt7902DebugOps,
	.chip_id = MT7902_CHIP_ID,
	.should_verify_chip_id = FALSE,
	.sw_sync0 = MT7902_SW_SYNC0,
	.sw_ready_bits = WIFI_FUNC_NO_CR4_READY_BITS,
	.sw_ready_bit_offset = MT7902_SW_SYNC0_RDY_OFFSET,
	.patch_addr = MT7902_PATCH_START_ADDR,
	.is_support_cr4 = FALSE,
	.is_support_wacpu = FALSE,
	.txd_append_size = MT7902_TX_DESC_APPEND_LENGTH,
	.rxd_size = MT7902_RX_DESC_LENGTH,
	.pse_header_length = CONNAC2X_NIC_TX_PSE_HEADER_LENGTH,
	.init_event_size = CONNAC2X_RX_INIT_EVENT_LENGTH,
	.event_hdr_size = CONNAC2X_RX_EVENT_HDR_LENGTH,
	.eco_info = mt7902_eco_table,
	.isNicCapV1 = FALSE,
	.is_support_efuse = TRUE,
	.top_hcr = CONNAC2X_TOP_HCR,
	.top_hvr = CONNAC2X_TOP_HVR,
	.top_fvr = CONNAC2X_TOP_FVR,
	#if (CFG_SUPPORT_802_11AX == 1)
	.arb_ac_mode_addr = MT7902_ARB_AC_MODE_ADDR,
	#endif
	.asicCapInit = mt7902CapInit,
#if CFG_ENABLE_FW_DOWNLOAD
	.asicEnableFWDownload = asicEnableFWDownload,
#endif /* CFG_ENABLE_FW_DOWNLOAD */
#if defined(_HIF_USB) || defined(_HIF_SDIO)
	.asicDumpSerDummyCR = mt7961DumpSerDummyCR,
#endif
	.downloadBufferBin = wlanConnac2XDownloadBufferBin,
	.constructBufferBinFileName = mt7902ConstructBufferBinFileName,
	.is_support_hw_amsdu = TRUE,
	.is_support_asic_lp = TRUE,
	.is_support_wfdma1 = FALSE,
	.RxDbgInfoFromRxdGrp3 = mt7902GetRxDbgInfoSrc,
	.asicWfdmaReInit = asicConnac2xWfdmaReInit,
	.asicWfdmaReInit_handshakeInit = asicConnac2xWfdmaDummyCrWrite,
#if defined(_HIF_USB)
	.asicUsbInit = asicConnac2xWfdmaInitForUSB,
	.asicUsbInit_ic_specific = NULL,
	.u4SerUsbMcuEventAddr = WF_SW_DEF_CR_USB_MCU_EVENT_ADD,
	.u4SerUsbHostAckAddr = WF_SW_DEF_CR_USB_HOST_ACK_ADDR,
#endif
	.u4SerSuspendSyncAddr = CONNAC2X_HOST_MAILBOX_WF_ADDR,
	.group5_size = sizeof(struct HW_MAC_RX_STS_GROUP_5),
	.u4LmacWtblDUAddr = MT7902_WIFI_LWTBL_BASE,
	.u4UmacWtblDUAddr = MT7902_WIFI_UWTBL_BASE,

#if CFG_CHIP_RESET_SUPPORT
	.asicWfsysRst = mt7961HalCbtopRguWfRst,
	.asicPollWfsysSwInitDone = mt7961HalPollWfsysSwInitDone,
#if defined(_HIF_SDIO)
	.asicSetNoBTFwOwnEn = NULL,
#endif
#endif
	.asicSerInit = mt7961SerInit,
	.prTxPwrLimitFile = "TxPwrLimit_MT79x1.dat",
	.ucTxPwrLimitBatchSize = 8,

	/* buzzard capability:
	 * 1. MAC RX AMPDU max number is 256.
	 * 2. MAC RX MPDU max length is 11454.
	 * So, MAC RX AMPDU max length is 256 * 11454 = 2,932,224
	 *
	 * Set HE_CAP MAX_AMPDU_LEN_EXP to maximum which meets the condition
	 *     1. In 2.4G, 2 ^ (16 + MAX_AMPDU_LEN_EXP) - 1 <= 2,932,224
	 *     2. In 5G, 2 ^ (20 + MAX_AMPDU_LEN_EX) - 1 <= 2,932,224
	 */
	.uc2G4HeCapMaxAmpduLenExp = 3, /* 2 ^ (16 + 3) - 1 = 524287 */
	.uc5GHeCapMaxAmpduLenExp = 1, /* 2 ^ (20 + 1) - 1 = 2,097,151 */

#if defined(_HIF_PCIE) || defined(_HIF_AXI)
	/* owner set true when feature is ready. */
	.fgIsSupportL0p5Reset = TRUE,
#elif defined(_HIF_USB)
	.fgIsSupportL0p5Reset = TRUE,
#elif defined(_HIF_SDIO)
	/* owner set true when feature is ready. */
	.fgIsSupportL0p5Reset = TRUE,
#endif

	.loadCfgSetting = NULL,
};

struct mt66xx_hif_driver_data mt66xx_driver_data_mt7902 = {
	.chip_info = &mt66xx_chip_info_mt7902,
};



#endif /* MT7902 */
