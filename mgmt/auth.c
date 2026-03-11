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
/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/auth.c#1
 */

/*! \file   "auth.c"
 *    \brief  This file includes the authentication-related functions.
 *
 *   This file includes the authentication-related functions.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "hif_pdma.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
struct APPEND_VAR_IE_ENTRY txAuthIETable[] = {
	{(ELEM_HDR_LEN + ELEM_MAX_LEN_CHALLENGE_TEXT), NULL,
	 authAddIEChallengeText},
	{0, authCalculateRSNIELen, authAddRSNIE}, /* Element ID: 48 */
	{(ELEM_HDR_LEN + 1), NULL, authAddMDIE}, /* Element ID: 54 */
	{0, rsnCalculateFTIELen, rsnGenerateFTIE}, /* Element ID: 55 */
};

struct HANDLE_IE_ENTRY rxAuthIETable[] = {
	{ELEM_ID_CHALLENGE_TEXT, authHandleIEChallengeText}
};

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the Authentication frame header and
 *        fixed fields.
 *
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucPeerMACAddress      Given Peer MAC Address.
 * @param[in] aucMACAddress          Given Our MAC Address.
 * @param[in] u2AuthAlgNum           Authentication Algorithm Number
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 * @param[in] u2StatusCode           Status Code
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
authComposeAuthFrameHeaderAndFF(IN struct ADAPTER *prAdapter,
                IN struct STA_RECORD *prStaRec,
                IN uint8_t ucBssIndex,
                IN uint8_t *pucBuffer,
                IN uint8_t aucPeerMACAddress[],
                IN uint8_t aucMACAddress[],
                IN uint16_t u2AuthAlgNum,
                IN uint16_t u2TransactionSeqNum,
                IN uint16_t u2StatusCode)
{
    struct WLAN_AUTH_FRAME *prAuthFrame;
    uint16_t u2FrameCtrl;

    ASSERT(pucBuffer);
    ASSERT(aucPeerMACAddress);
    ASSERT(aucMACAddress);

    prAuthFrame = (struct WLAN_AUTH_FRAME *)pucBuffer;

    /* ====================================================================
     * Compose 802.11 MAC Header
     * ==================================================================== */

    /* Frame Control: set to Authentication frame type */
    u2FrameCtrl = MAC_FRAME_AUTH;

    /* Shared Key Auth frame 3 must be encrypted (protected bit). */
    if ((u2AuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY) &&
        (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3)) {
        u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
    }

    prAuthFrame->u2FrameCtrl = u2FrameCtrl;
    prAuthFrame->u2DurationID = 0; /* Will be filled by hardware */

    /* Address fields */
    COPY_MAC_ADDR(prAuthFrame->aucDestAddr, aucPeerMACAddress);
    COPY_MAC_ADDR(prAuthFrame->aucSrcAddr, aucMACAddress);

    /* BSSID: For AP STA, use peer MAC; otherwise use our MAC */
    if (prStaRec != NULL && IS_AP_STA(prStaRec)) {
        COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucPeerMACAddress);
    } else {
        COPY_MAC_ADDR(prAuthFrame->aucBSSID, aucMACAddress);
    }

    prAuthFrame->u2SeqCtrl = 0; /* Will be filled by hardware */

    /* ====================================================================
     * Compose Authentication Frame Body - Fixed Fields
     * ==================================================================== */

    prAuthFrame->u2AuthAlgNum = u2AuthAlgNum;

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
    /*
     * SUPPLICANT_SME: allow userspace-provided auth body (e.g. SAE).
     * Also emit a compact but useful diagnostic showing what driver/scan/userspace think
     * about security so we can debug mismatch (open vs protected, cipher / AKM).
     */
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);


    prAisFsmInfo = aisGetAisFsmInfo(prAdapter, ucBssIndex);

    DBGLOG(SAA, INFO, "=== AUTH SECURITY DIAGNOSTIC ===\n");

    if (prConnSettings) {
        DBGLOG(SAA, INFO,
               "[ConnSettings] AuthMode=%d EncStatus=%d AuthDataLen=%u\n",
               prConnSettings->eAuthMode,
               prConnSettings->eEncStatus,
               prConnSettings->ucAuthDataLen);
    } else {
        DBGLOG(SAA, INFO, "[ConnSettings] <null>\n");
    }

    if (prBssInfo) {
        DBGLOG(SAA, INFO,
               "[BSS] cap=0x%04x pairwise=0x%08x group=0x%08x akm=0x%08x\n",
               prBssInfo->u2CapInfo,
               prBssInfo->u4RsnSelectedPairwiseCipher,
               prBssInfo->u4RsnSelectedGroupCipher,
               prBssInfo->u4RsnSelectedAKMSuite);
    } else {
        DBGLOG(SAA, INFO, "[BSS] <null>\n");
    }

    if (prStaRec) {
        DBGLOG(SAA, INFO,
               "[StaRec] AuthAlg=%d StaType=%d WlanIdx=%d\n",
               prStaRec->ucAuthAlgNum,
               prStaRec->eStaType,
               prStaRec->ucWlanIndex);
    } else {
        DBGLOG(SAA, INFO, "[StaRec] <null>\n");
    }

    if (prAisFsmInfo) {
        DBGLOG(SAA, INFO,
               "[AIS FSM] State=%d\n",
               prAisFsmInfo->eCurrentState);
    }

    DBGLOG(SAA, INFO,
           "[AuthFrame] Alg=%d TransSeq=%d Status=%d Protected=%d\n",
           u2AuthAlgNum,
           u2TransactionSeqNum,
           u2StatusCode,
           !!(u2FrameCtrl & MASK_FC_PROTECTED_FRAME));

    DBGLOG(SAA, INFO, "===============================\n");

    /* If userspace supplied a custom auth payload (SAE etc), use it */
    if (prConnSettings &&
        (prConnSettings->ucAuthDataLen > 0) &&
        (prConnSettings->ucAuthDataLen <= AUTH_DATA_MAX_LEN) &&
        prStaRec &&
        !IS_STA_IN_P2P(prStaRec)) {

        kalMemCopy(prAuthFrame->aucAuthData,
                   prConnSettings->aucAuthData,
                   prConnSettings->ucAuthDataLen);

        DBGLOG(SAA, INFO,
               "Auth with custom data: TransSN=%d Status=%d Len=%d\n",
               prConnSettings->aucAuthData[0],
               (prConnSettings->ucAuthDataLen > 2 ? prConnSettings->aucAuthData[2] : 0),
               prConnSettings->ucAuthDataLen);
    } else {
        /* Build the standard fields (transaction seq + status) */
        if (prConnSettings && prConnSettings->ucAuthDataLen > AUTH_DATA_MAX_LEN) {
            DBGLOG(SAA, ERROR,
                   "Auth data too large (%d > %d), truncating\n",
                   prConnSettings->ucAuthDataLen,
                   AUTH_DATA_MAX_LEN);
        }

        prAuthFrame->aucAuthData[0] = (uint8_t)(u2TransactionSeqNum & 0xFF);
        prAuthFrame->aucAuthData[1] = (uint8_t)((u2TransactionSeqNum >> 8) & 0xFF);
        prAuthFrame->aucAuthData[2] = (uint8_t)(u2StatusCode & 0xFF);
        prAuthFrame->aucAuthData[3] = (uint8_t)((u2StatusCode >> 8) & 0xFF);

        DBGLOG(SAA, INFO,
               "Auth with standard fields: TransSN=%d Status=%d\n",
               u2TransactionSeqNum, u2StatusCode);
    }

#else
    /*
     * SUPPLICANT_SME == 0: kernel handles auth bodies with simple fixed fields.
     * Keep the simple original behavior.
     */
    prAuthFrame->u2AuthTransSeqNo = u2TransactionSeqNum;
    prAuthFrame->u2StatusCode = u2StatusCode;

    DBGLOG(SAA, INFO,
           "Auth with standard fields: TransSN=%d Status=%d\n",
           u2TransactionSeqNum, u2StatusCode);
#endif
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will append Challenge Text IE to the Authentication
 *        frame
 *
 * @param[in] prMsduInfo     Pointer to the composed MSDU_INFO_T.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void authAddIEChallengeText(IN struct ADAPTER *prAdapter,
			    IN OUT struct MSDU_INFO *prMsduInfo)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TransactionSeqNum;

	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	ASSERT(prStaRec);

	/* For Management, frame header and payload are in a continuous
	 * buffer
	 */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prMsduInfo->prPacket;
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	WLAN_GET_FIELD_16(
		&prAuthFrame->aucAuthData[0],
		&u2TransactionSeqNum)
#else
	WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo, &u2TransactionSeqNum)
#endif
	    /* Only consider SEQ_3 for Challenge Text */
	if ((u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_3) &&
		(prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)
		&& (prStaRec->prChallengeText != NULL)) {

		COPY_IE(((unsigned long)(prMsduInfo->prPacket) +
			 prMsduInfo->u2FrameLength),
			(prStaRec->prChallengeText));

		prMsduInfo->u2FrameLength += IE_SIZE(prStaRec->prChallengeText);
	}

	return;

}				/* end of authAddIEChallengeText() */

uint32_t
authSendAuthFrame(struct ADAPTER *prAdapter,
                  struct STA_RECORD *prStaRec,
                  uint8_t ucBssIndex,
                  struct SW_RFB *prFalseAuthSwRfb,
                  uint16_t u2TransactionSeqNum,
                  uint16_t u2StatusCode)
{
    struct MSDU_INFO *prMsduInfo;
    struct BSS_INFO *prBssInfo = NULL;
    uint8_t *pucTxFrame;
    uint8_t ucFinalBssIndex;
    uint16_t u2EstimatedFrameLen;
    uint16_t u2EstimatedExtraIELen;
    uint16_t u2PayloadLen;
    uint32_t i;

    if (!prAdapter) {
        DBGLOG(SAA, ERROR, "AUTH TX: NULL adapter\n");
        return WLAN_STATUS_FAILURE;
    }

    DBGLOG(SAA, INFO, "=== AUTH TX START === seq=%u status=%u\n",
           u2TransactionSeqNum, u2StatusCode);

    ucFinalBssIndex = prStaRec ? prStaRec->ucBssIndex : ucBssIndex;

    prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucFinalBssIndex);
    if (!prBssInfo) {
        DBGLOG(SAA, ERROR, "AUTH TX: invalid BSS index %u\n", ucFinalBssIndex);
        return WLAN_STATUS_FAILURE;
    }

    u2EstimatedFrameLen = MAC_TX_RESERVED_FIELD +
        WLAN_MAC_MGMT_HEADER_LEN +
        AUTH_ALGORITHM_NUM_FIELD_LEN +
        AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
        STATUS_CODE_FIELD_LEN;
    u2EstimatedExtraIELen = 0;

    for (i = 0; i < ARRAY_SIZE(txAuthIETable); i++) {
        if (txAuthIETable[i].u2EstimatedFixedIELen)
            u2EstimatedExtraIELen += txAuthIETable[i].u2EstimatedFixedIELen;
        else if (txAuthIETable[i].pfnCalculateVariableIELen)
            u2EstimatedExtraIELen +=
                txAuthIETable[i].pfnCalculateVariableIELen(
                    prAdapter, ucFinalBssIndex, prStaRec);
    }
    u2EstimatedFrameLen += u2EstimatedExtraIELen;

    prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
    if (!prMsduInfo) {
        DBGLOG(SAA, WARN, "AUTH TX: cnmMgtPktAlloc failed len=%u\n",
               u2EstimatedFrameLen);
        return WLAN_STATUS_RESOURCES;
    }
    if (!prMsduInfo->prPacket) {
        DBGLOG(SAA, ERROR, "AUTH TX: prPacket NULL after alloc\n");
        cnmMgtPktFree(prAdapter, prMsduInfo);
        return WLAN_STATUS_RESOURCES;
    }

    pucTxFrame = (uint8_t *)((unsigned long)prMsduInfo->prPacket +
                              MAC_TX_RESERVED_FIELD);

    if (!prStaRec && prFalseAuthSwRfb) {
        struct WLAN_AUTH_FRAME *prFalseAuthFrame =
            (struct WLAN_AUTH_FRAME *)prFalseAuthSwRfb->pvHeader;
        u2TransactionSeqNum =
            (prFalseAuthFrame->aucAuthData[1] << 8) |
            (prFalseAuthFrame->aucAuthData[0] + 1);
        DBGLOG(SAA, INFO, "AUTH TX (AP/legacy path) seq=%u\n",
               u2TransactionSeqNum);
    }

    if (u2TransactionSeqNum == 0)
        u2TransactionSeqNum = 1;

    DBGLOG(SAA, INFO, "AUTH TX (STA): BSS=%u StaRecIdx=%u\n",
           ucFinalBssIndex,
           prStaRec ? prStaRec->ucIndex : 0xFF);

    authComposeAuthFrameHeaderAndFF(
        prAdapter,
        prStaRec,
        ucFinalBssIndex,
        pucTxFrame,
        prStaRec ? prStaRec->aucMacAddr : NULL,
        prBssInfo->aucOwnMacAddr,
        prStaRec ? prStaRec->ucAuthAlgNum : 0,
        u2TransactionSeqNum,
        u2StatusCode);

    u2PayloadLen = AUTH_ALGORITHM_NUM_FIELD_LEN +
                   AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
                   STATUS_CODE_FIELD_LEN;

    TX_SET_MMPDU(prAdapter,
                 prMsduInfo,
                 ucFinalBssIndex,
                 prStaRec ? prStaRec->ucIndex : STA_REC_INDEX_BMCAST,
                 WLAN_MAC_MGMT_HEADER_LEN,
                 WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen,
                 saaFsmRunEventTxDone,
                 MSDU_RATE_MODE_AUTO);

    nicTxSetPktLifeTime(prMsduInfo, 5000);
    nicTxSetPktRetryLimit(prMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);
    prMsduInfo->fgMgmtUseDataQ = FALSE; //use mgmt tx queue, not data queue

    for (i = 0; i < ARRAY_SIZE(txAuthIETable); i++) {
        if (txAuthIETable[i].pfnAppendIE)
            txAuthIETable[i].pfnAppendIE(prAdapter, prMsduInfo);
    }

    nicTxConfigPktControlFlag(prMsduInfo, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

    prMsduInfo->ucWlanIndex = nicTxGetWlanIdx(prAdapter,
        prMsduInfo->ucBssIndex,
        prMsduInfo->ucStaRecIndex);

    DBGLOG(SAA, INFO,
           "AUTH TX FINAL: BSS=%u StaRecIdx=%u wlanIdx=%u Len=%u pkt=%p\n",
           ucFinalBssIndex,
           prStaRec ? prStaRec->ucIndex : 0xFF,
           prMsduInfo->ucWlanIndex,
           prMsduInfo->u2FrameLength,
           prMsduInfo->prPacket);


    DBGLOG_MEM8(SAA, WARN, ((struct WLAN_MAC_HEADER *)prMsduInfo->prPacket),
                prMsduInfo->u2FrameLength);

    nicTxEnqueueMsdu(prAdapter, prMsduInfo);

    DBGLOG(SAA, INFO, "=== AUTH TX ENQUEUED ===\n");

    return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will strictly check the TX Authentication frame
 *        for SAA/AAA event handling.
 *
 * @param[in] prMsduInfo             Pointer of MSDU_INFO_T
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 *
 * @retval WLAN_STATUS_FAILURE       This is not the frame we should handle
 *                                   at current state.
 * @retval WLAN_STATUS_SUCCESS       This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t authCheckTxAuthFrame(IN struct ADAPTER *prAdapter,
			      IN struct MSDU_INFO *prMsduInfo,
			      IN uint16_t u2TransactionSeqNum)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TxFrameCtrl;
	uint16_t u2TxAuthAlgNum;
	uint16_t u2TxTransactionSeqNum;

	ASSERT(prMsduInfo);

	if (!prMsduInfo || !prMsduInfo->prPacket) {
		DBGLOG(SAA, WARN, "[AUTH-CHECK] prMsduInfo or prPacket is NULL\n");
		return WLAN_STATUS_INVALID_PACKET;
	}

/* prPacket may be a raw cnmMemAlloc buffer OR an skb (after nicTxEnsureSkbHeadroom).
 * For the data-path (fgMgmtUseDataQ), it's an skb — use skb->data directly.
 * For the cmd-path, it's a raw buffer — add MAC_TX_RESERVED_FIELD offset.
 */
if (prMsduInfo->fgMgmtUseDataQ) {
    struct sk_buff *prSkb = (struct sk_buff *)prMsduInfo->prPacket;
    prAuthFrame = (struct WLAN_AUTH_FRAME *)prSkb->data;
} else {
    prAuthFrame = (struct WLAN_AUTH_FRAME *)((unsigned long)(prMsduInfo->prPacket)
                             + MAC_TX_RESERVED_FIELD);
}

	ASSERT(prAuthFrame);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
	ASSERT(prStaRec);

	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	u2TxFrameCtrl = prAuthFrame->u2FrameCtrl;
	u2TxFrameCtrl &= MASK_FRAME_TYPE;
	if (u2TxFrameCtrl != MAC_FRAME_AUTH)
		return WLAN_STATUS_FAILURE;

	u2TxAuthAlgNum = prAuthFrame->u2AuthAlgNum;
	if (u2TxAuthAlgNum != (uint16_t)(prStaRec->ucAuthAlgNum)) {
		DBGLOG(SAA, WARN,
		       "[AUTH-CHECK] alg mismatch: frame=%u staRec=%u eAuthAssocSent=%u\n",
		       u2TxAuthAlgNum, prStaRec->ucAuthAlgNum,
		       prStaRec->eAuthAssocSent);
		return WLAN_STATUS_FAILURE;
	}

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	u2TxTransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
				 prAuthFrame->aucAuthData[0];
#else
	u2TxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
#endif
	if (u2TxTransactionSeqNum != u2TransactionSeqNum)
		return WLAN_STATUS_FAILURE;

	return WLAN_STATUS_SUCCESS;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will check the incoming Auth Frame's Transaction
 *        Sequence Number before delivering it to the corresponding
 *        SAA or AAA Module.
 *
 * @param[in] prSwRfb            Pointer to the SW_RFB_T structure.
 *
 * @retval WLAN_STATUS_SUCCESS   Always not retain authentication frames
 */
/*----------------------------------------------------------------------------*/
uint32_t authCheckRxAuthFrameTransSeq(IN struct ADAPTER *prAdapter,
				      IN struct SW_RFB *prSwRfb)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2RxTransactionSeqNum;
	uint16_t u2MinPayloadLen;
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;

	ASSERT(prSwRfb);

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Authentication Frame. */
	u2MinPayloadLen = (AUTH_ALGORITHM_NUM_FIELD_LEN +
			   AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN +
			   STATUS_CODE_FIELD_LEN);
	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) < u2MinPayloadLen) {
		DBGLOG(SAA, WARN,
		       "Rx Auth payload: len[%u] < min expected len[%u]\n",
		       (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen),
		       u2MinPayloadLen);
		DBGLOG(SAA, WARN, "=== Dump Rx Auth ===\n");
		DBGLOG_MEM8(SAA, WARN, prAuthFrame, prSwRfb->u2PacketLen);
		return WLAN_STATUS_SUCCESS;
	}
	/* 4 <3> Parse the Fixed Fields of Authentication Frame Body. */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2RxTransactionSeqNum);
	 */
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	u2RxTransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
#else
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
#endif

	prStaRec = prSwRfb->prStaRec;

	/* SAE auth */
	if (prAuthFrame->u2AuthAlgNum == AUTH_ALGORITHM_NUM_SAE) {
		if ((u2RxTransactionSeqNum != AUTH_TRANSACTION_SEQ_1) &&
		(u2RxTransactionSeqNum != AUTH_TRANSACTION_SEQ_2)) {
			DBGLOG(SAA, WARN,
				"RX SAE unexpected auth TransSeqNum:%d\n",
				u2RxTransactionSeqNum);
			return WLAN_STATUS_SUCCESS;
		}

		/* Processs auth by SAA or AAA depneds on OP mode */
		if (prStaRec)
			prBssInfo = GET_BSS_INFO_BY_INDEX(
				prAdapter, prStaRec->ucBssIndex);
		else
			prBssInfo = p2pFuncBSSIDFindBssInfo(
				prAdapter, prAuthFrame->aucBSSID);

		if (prBssInfo == NULL)
			return WLAN_STATUS_SUCCESS;

		if (prBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE)
			saaFsmRunEventRxAuth(prAdapter, prSwRfb);
#if CFG_SUPPORT_AAA
		else if (prBssInfo->eCurrentOPMode ==
					OP_MODE_ACCESS_POINT)
			aaaFsmRunEventRxAuth(prAdapter, prSwRfb);
#endif
		else
			DBGLOG(SAA, WARN, "Not support SAE for non-AIS/P2P\n");
	} else { /* non-SAE auth */
		switch (u2RxTransactionSeqNum) {
		case AUTH_TRANSACTION_SEQ_2:
		case AUTH_TRANSACTION_SEQ_4:
			saaFsmRunEventRxAuth(prAdapter, prSwRfb);
			break;

		case AUTH_TRANSACTION_SEQ_1:
		case AUTH_TRANSACTION_SEQ_3:
#if CFG_SUPPORT_AAA
			aaaFsmRunEventRxAuth(prAdapter, prSwRfb);
#endif /* CFG_SUPPORT_AAA */
			break;

		default:
			DBGLOG(SAA, WARN,
			"Strange Auth: Trans Seq No = %d\n",
			       u2RxTransactionSeqNum);
#if CFG_IGNORE_INVALID_AUTH_TSN
			if (!prStaRec)
				return WLAN_STATUS_SUCCESS;
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
			if ((prStaRec->eAuthAssocSent >= AA_SENT_AUTH1) &&
				(prStaRec->eAuthAssocSent <= AA_SENT_AUTH4))
				saaFsmRunEventRxAuth(prAdapter, prSwRfb);
#else
				switch (prStaRec->eAuthAssocState) {
				case SAA_STATE_SEND_AUTH1:
				case SAA_STATE_WAIT_AUTH2:
				case SAA_STATE_SEND_AUTH3:
				case SAA_STATE_WAIT_AUTH4:
					saaFsmRunEventRxAuth(
						prAdapter, prSwRfb);
					break;
				default:
					break;
				}
#endif
#endif
			break;
		}
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckRxAuthFrameTransSeq() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will validate the incoming Authentication Frame and
 *        take the status code out.
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[in] u2TransactionSeqNum    Transaction Sequence Number
 * @param[out] pu2StatusCode         Pointer to store the Status Code from
 *                                   Authentication.
 *
 * @retval WLAN_STATUS_FAILURE       This is not the frame we should handle
 *                                   at current state.
 * @retval WLAN_STATUS_SUCCESS       This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t
authCheckRxAuthFrameStatus(IN struct ADAPTER *prAdapter,
			   IN struct SW_RFB *prSwRfb,
			   IN uint16_t u2TransactionSeqNum,
			   OUT uint16_t *pu2StatusCode)
{
	struct STA_RECORD *prStaRec;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2RxAuthAlgNum;
	uint16_t u2RxTransactionSeqNum;
	/* UINT_16 u2RxStatusCode; // NOTE(Kevin): Optimized for ARM */

	ASSERT(prSwRfb);
	ASSERT(pu2StatusCode);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (!prStaRec)
		return WLAN_STATUS_INVALID_PACKET;

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Fixed Fields of Authentication Frame Body. */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthAlgNum, &u2RxAuthAlgNum); */
	u2RxAuthAlgNum = prAuthFrame->u2AuthAlgNum;
	/* NOTE(Kevin): Optimized for ARM */
	if (u2RxAuthAlgNum != (uint16_t) prStaRec->ucAuthAlgNum) {
		DBGLOG(SAA, WARN,
		       "Discard Auth frame with auth type = %d, current = %d\n",
		       u2RxAuthAlgNum, prStaRec->ucAuthAlgNum);
		*pu2StatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;
		return WLAN_STATUS_SUCCESS;
	}
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2RxTransactionSeqNum);
	 */
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	u2RxTransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
						prAuthFrame->aucAuthData[0];
	if (u2RxTransactionSeqNum < u2TransactionSeqNum) {
		/* Still report to upper layer
		* to let it do the error handling
		*/
		DBGLOG(SAA, WARN,
			"Rx Auth frame with unexpected Transaction Seq No = %d\n",
			u2RxTransactionSeqNum);

		/*Add for support WEP when enable wpa3*/
		*pu2StatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;
		return WLAN_STATUS_FAILURE;
	}
#else
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
	if (u2RxTransactionSeqNum != u2TransactionSeqNum) {
		DBGLOG(SAA, WARN,
		       "Discard Auth frame with Transaction Seq No = %d\n",
		       u2RxTransactionSeqNum);
		*pu2StatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;
		return WLAN_STATUS_FAILURE;
	}
#endif
	/* 4 <3> Get the Status code */
	/* WLAN_GET_FIELD_16(&prAuthFrame->u2StatusCode, &u2RxStatusCode); */
	/* *pu2StatusCode = u2RxStatusCode; */
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	*pu2StatusCode = (prAuthFrame->aucAuthData[3] << 8) +
					prAuthFrame->aucAuthData[2];
#else
	*pu2StatusCode = prAuthFrame->u2StatusCode;
	/* NOTE(Kevin): Optimized for ARM */
#endif
	return WLAN_STATUS_SUCCESS;

}				/* end of authCheckRxAuthFrameStatus() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will handle the Challenge Text IE from
 *        the Authentication frame
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[in] prIEHdr                Pointer to start address of IE
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void authHandleIEChallengeText(struct ADAPTER *prAdapter,
			       struct SW_RFB *prSwRfb, struct IE_HDR *prIEHdr)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2TransactionSeqNum;

	ASSERT(prSwRfb);
	ASSERT(prIEHdr);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	ASSERT(prStaRec);

	if (!prStaRec)
		return;

	/* For Management, frame header and payload are in
	 * a continuous buffer
	 */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* WLAN_GET_FIELD_16(&prAuthFrame->u2AuthTransSeqNo,
	 *	&u2TransactionSeqNum)
	 */
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	u2TransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
					prAuthFrame->aucAuthData[0];
#else
	u2TransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
#endif
	/* Only consider SEQ_2 for Challenge Text */
	if ((u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_2) &&
	    (prStaRec->ucAuthAlgNum == AUTH_ALGORITHM_NUM_SHARED_KEY)) {

		/* Free previous allocated TCM memory */
		if (prStaRec->prChallengeText) {
			/* ASSERT(0); */
			cnmMemFree(prAdapter, prStaRec->prChallengeText);
			prStaRec->prChallengeText =
			    (struct IE_CHALLENGE_TEXT *)NULL;
		}
		prStaRec->prChallengeText =
		    cnmMemAlloc(prAdapter, RAM_TYPE_MSG, IE_SIZE(prIEHdr));
		if (prStaRec->prChallengeText == NULL)
			return;

		/* Save the Challenge Text from Auth Seq 2 Frame,
		 * before sending Auth Seq 3 Frame
		 */
		COPY_IE(prStaRec->prChallengeText, prIEHdr);
	}

	return;

}				/* end of authAddIEChallengeText() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Authentication
 *        frame.
 *
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 *
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t authProcessRxAuth2_Auth4Frame(IN struct ADAPTER *prAdapter,
				       IN struct SW_RFB *prSwRfb)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint8_t *pucIEsBuffer;
	uint16_t u2IEsLen;
	uint16_t u2Offset;
	uint8_t ucIEID;
	uint32_t i;
	uint16_t u2TransactionSeqNum;

	ASSERT(prSwRfb);

	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;
	/*Add for support WEP when enable wpa3*/
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	pucIEsBuffer = (uint8_t *)&prAuthFrame->aucAuthData[0] + 4;
#else
	pucIEsBuffer = &prAuthFrame->aucInfoElem[0];
#endif
	u2IEsLen = (prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) -
	    (AUTH_ALGORITHM_NUM_FIELD_LEN +
	     AUTH_TRANSACTION_SEQENCE_NUM_FIELD_LEN + STATUS_CODE_FIELD_LEN);

	IE_FOR_EACH(pucIEsBuffer, u2IEsLen, u2Offset) {
		ucIEID = IE_ID(pucIEsBuffer);

		for (i = 0;
		     i <
		     (sizeof(rxAuthIETable) / sizeof(struct HANDLE_IE_ENTRY));
		     i++) {
			if ((ucIEID == rxAuthIETable[i].ucElemID)
			    && (rxAuthIETable[i].pfnHandleIE != NULL))
				rxAuthIETable[i].pfnHandleIE(prAdapter,
					prSwRfb,
					(struct IE_HDR *)pucIEsBuffer);
		}
	}
	/*Add for support WEP when enable wpa3*/
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	u2TransactionSeqNum = (prAuthFrame->aucAuthData[1] << 8) +
		prAuthFrame->aucAuthData[0];
#else
	u2TransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
	/* NOTE(Kevin): Optimized for ARM */
#endif
	if (prAuthFrame->u2AuthAlgNum ==
	    AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION) {
		if (u2TransactionSeqNum == AUTH_TRANSACTION_SEQ_4) {
			/* todo: check MIC, if mic error, return
			 * WLAN_STATUS_FAILURE
			 */
		} else if (u2TransactionSeqNum ==
			   AUTH_TRANSACTION_SEQ_2) {
			struct cfg80211_ft_event_params *prFtEvent =
				aisGetFtEventParam(prAdapter,
				secGetBssIdxByRfb(prAdapter,
				prSwRfb));

			prFtEvent->ies =
			    &prAuthFrame->aucInfoElem[0];
			prFtEvent->ies_len = u2IEsLen;
		}
	}
	return WLAN_STATUS_SUCCESS;

}				/* end of authProcessRxAuth2_Auth4Frame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will compose the Deauthentication frame
 *
 * @param[in] pucBuffer              Pointer to the frame buffer.
 * @param[in] aucPeerMACAddress      Given Peer MAC Address.
 * @param[in] aucMACAddress          Given Our MAC Address.
 * @param[in] u2StatusCode           Status Code
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
static __KAL_INLINE__ void
authComposeDeauthFrameHeaderAndFF(IN uint8_t *pucBuffer,
				  IN uint8_t aucPeerMACAddress[],
				  IN uint8_t aucMACAddress[],
				  IN uint8_t aucBssid[],
				  IN uint16_t u2ReasonCode)
{
	struct WLAN_DEAUTH_FRAME *prDeauthFrame;
	uint16_t u2FrameCtrl;

	ASSERT(pucBuffer);
	ASSERT(aucPeerMACAddress);
	ASSERT(aucMACAddress);
	ASSERT(aucBssid);

	if (pucBuffer) {
	  DBGLOG(SAA, ERROR, "[AUTH-BYTES] Algorithm field: 0x%02x 0x%02x\n",
		 pucBuffer[26], pucBuffer[27]);
	}

	prDeauthFrame = (struct WLAN_DEAUTH_FRAME *)pucBuffer;

	DBGLOG(SAA, INFO,
	       "pucBuffer=%p first16=%*ph\n",
	       pucBuffer, 16, pucBuffer);



	/* 4 <1> Compose the frame header of the Deauthentication frame. */
	/* Fill the Frame Control field. */
	u2FrameCtrl = MAC_FRAME_DEAUTH;

	/* WLAN_SET_FIELD_16(&prDeauthFrame->u2FrameCtrl, u2FrameCtrl); */
	prDeauthFrame->u2FrameCtrl = u2FrameCtrl;
	/* NOTE(Kevin): Optimized for ARM */

	/* Fill the DA field with Target BSSID. */
	COPY_MAC_ADDR(prDeauthFrame->aucDestAddr, aucPeerMACAddress);

	/* Fill the SA field with our MAC Address. */
	COPY_MAC_ADDR(prDeauthFrame->aucSrcAddr, aucMACAddress);

	/* Fill the BSSID field with Target BSSID. */
	COPY_MAC_ADDR(prDeauthFrame->aucBSSID, aucBssid);

	/* Clear the SEQ/FRAG_NO field(HW won't overide the FRAG_NO,
	 * so we need to clear it).
	 */
	prDeauthFrame->u2SeqCtrl = 0;

	/* 4 <2> Compose the frame body's fixed field part of
	 *       the Authentication frame.
	 */
	/* Fill the Status Code field. */
	/* WLAN_SET_FIELD_16(&prDeauthFrame->u2ReasonCode, u2ReasonCode); */
	prDeauthFrame->u2ReasonCode = u2ReasonCode;
	/* NOTE(Kevin): Optimized for ARM */
}			/* end of authComposeDeauthFrameHeaderAndFF() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will send the Deauthenticiation frame
 *
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 * @param[in] prClassErrSwRfb    Pointer to the SW_RFB_T which is Class Error.
 * @param[in] u2ReasonCode       A reason code to indicate why to leave BSS.
 * @param[in] pfTxDoneHandler    TX Done call back function
 *
 * @retval WLAN_STATUS_RESOURCES No available resource for frame composing.
 * @retval WLAN_STATUS_SUCCESS   Successfully send frame to TX Module
 * @retval WLAN_STATUS_FAILURE   Didn't send Deauth frame for various reasons.
 */
/*----------------------------------------------------------------------------*/
uint32_t
authSendDeauthFrame(IN struct ADAPTER *prAdapter,
		    IN struct BSS_INFO *prBssInfo,
		    IN struct STA_RECORD *prStaRec,
		    IN struct SW_RFB *prClassErrSwRfb, IN uint16_t u2ReasonCode,
		    IN PFN_TX_DONE_HANDLER pfTxDoneHandler)
{
	uint8_t *pucReceiveAddr;
	uint8_t *pucTransmitAddr;
	uint8_t *pucBssid = NULL;
	struct MSDU_INFO *prMsduInfo;
	uint16_t u2EstimatedFrameLen;

	struct DEAUTH_INFO *prDeauthInfo;
	OS_SYSTIME rCurrentTime;
	int32_t i4NewEntryIndex, i;
	uint8_t ucStaRecIdx = STA_REC_INDEX_NOT_FOUND;
	uint8_t ucBssIndex = prAdapter->ucHwBssIdNum;
	uint8_t aucBMC[] = BC_MAC_ADDR;
	static OS_SYSTIME last_deauth_time[KAL_AIS_NUM] = {0};
	static uint16_t deauth_count[KAL_AIS_NUM] = {0};
	OS_SYSTIME current_time = kalGetTimeTick();
	
	/* Rate limit deauth frames: max 5 per second per BSS */
	if (ucBssIndex < KAL_AIS_NUM) {
		if (CHECK_FOR_TIMEOUT(current_time, last_deauth_time[ucBssIndex], SEC_TO_SYSTIME(1))) {
			deauth_count[ucBssIndex] = 0;
		}
		last_deauth_time[ucBssIndex] = current_time;
		
		if (++deauth_count[ucBssIndex] > 5) {
			DBGLOG(SAA, WARN, 
			       "BSS[%u] Deauth rate limit hit (%u/sec), dropping\n",
			       ucBssIndex, deauth_count[ucBssIndex]);
			return WLAN_STATUS_FAILURE;
		}
	}


	DBGLOG(RSN, INFO, "authSendDeauthFrame\n");

	/* NOTE(Kevin): The best way to reply the Deauth is according to
	 * the incoming data frame
	 */
	/* 4 <1.1> Find the Receiver Address */
	if (prClassErrSwRfb) {
		u_int8_t fgIsAbleToSendDeauth = FALSE;
		uint16_t u2RxFrameCtrl;
		struct WLAN_MAC_HEADER_A4 *prWlanMacHeader = NULL;

		prWlanMacHeader =
		    (struct WLAN_MAC_HEADER_A4 *)prClassErrSwRfb->pvHeader;

		/* WLAN_GET_FIELD_16(&prWlanMacHeader->u2FrameCtrl,
		 *   &u2RxFrameCtrl);
		 */
		u2RxFrameCtrl = prWlanMacHeader->u2FrameCtrl;
		/* NOTE(Kevin): Optimized for ARM */

		/* TODO(Kevin): Currently we won't send Deauth for IBSS node.
		 * How about DLS ?
		 */
		if ((prWlanMacHeader->u2FrameCtrl & MASK_TO_DS_FROM_DS) == 0)
			return WLAN_STATUS_FAILURE;

		DBGLOG(SAA, INFO,
		       "u2FrameCtrl=0x%x, DestAddr=" MACSTR
		       " srcAddr=" MACSTR " BSSID=" MACSTR
		       ", u2SeqCtrl=0x%x\n",
		       prWlanMacHeader->u2FrameCtrl,
		       MAC2STR(prWlanMacHeader->aucAddr1),
		       MAC2STR(prWlanMacHeader->aucAddr2),
		       MAC2STR(prWlanMacHeader->aucAddr3),
		       prWlanMacHeader->u2SeqCtrl);
		/* Check if corresponding BSS is able to send Deauth */
		for (i = 0; i < prAdapter->ucHwBssIdNum; i++) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);

			if (IS_NET_ACTIVE(prAdapter, i) &&
			    (EQUAL_MAC_ADDR
			     (prWlanMacHeader->aucAddr1,
			      prBssInfo->aucOwnMacAddr))) {

				fgIsAbleToSendDeauth = TRUE;
				ucBssIndex = (uint8_t) i;
				break;
			}
		}

		if (!fgIsAbleToSendDeauth)
			return WLAN_STATUS_FAILURE;

		pucReceiveAddr = prWlanMacHeader->aucAddr2;
	} else if (prStaRec) {
		prBssInfo =
		    GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
		ucStaRecIdx = prStaRec->ucIndex;
		ucBssIndex = prBssInfo->ucBssIndex;

		pucReceiveAddr = prStaRec->aucMacAddr;
	} else if (prBssInfo) {
		ucBssIndex = prBssInfo->ucBssIndex;
		ucStaRecIdx = STA_REC_INDEX_BMCAST;

		pucReceiveAddr = aucBMC;
	} else {
		DBGLOG(SAA, WARN, "Not to send Deauth, invalid data!\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	/* 4 <1.2> Find Transmitter Address and BSSID. */
	pucTransmitAddr = prBssInfo->aucOwnMacAddr;
	pucBssid = prBssInfo->aucBSSID;

	if (ucStaRecIdx != STA_REC_INDEX_BMCAST) {
		/* 4 <2> Check if already send a Deauth frame in
		 * MIN_DEAUTH_INTERVAL_MSEC
		 */
		GET_CURRENT_SYSTIME(&rCurrentTime);

		i4NewEntryIndex = -1;
		for (i = 0; i < MAX_DEAUTH_INFO_COUNT; i++) {
			prDeauthInfo = &(prAdapter->rWifiVar.arDeauthInfo[i]);

			/* For continuously sending Deauth frame, the minimum
			 * interval is MIN_DEAUTH_INTERVAL_MSEC.
			 */
			if (CHECK_FOR_TIMEOUT(rCurrentTime,
					      prDeauthInfo->rLastSendTime,
					      MSEC_TO_SYSTIME
					      (MIN_DEAUTH_INTERVAL_MSEC))) {

				i4NewEntryIndex = i;
			} else
			if (EQUAL_MAC_ADDR
				(pucReceiveAddr, prDeauthInfo->aucRxAddr)
				&& (!pfTxDoneHandler)) {

				return WLAN_STATUS_FAILURE;
			}
		}

		/* 4 <3> Update information. */
		if (i4NewEntryIndex > 0) {

			prDeauthInfo =
			    &(prAdapter->
			      rWifiVar.arDeauthInfo[i4NewEntryIndex]);

			COPY_MAC_ADDR(prDeauthInfo->aucRxAddr, pucReceiveAddr);
			prDeauthInfo->rLastSendTime = rCurrentTime;
		} else {
			/* NOTE(Kevin): for the case of AP mode, we may
			 * encounter this case
			 * if deauth all the associated clients.
			 */
			DBGLOG(SAA, WARN, "No unused DEAUTH_INFO_T !\n");
		}
	}
	/* 4 <5> Allocate a PKT_INFO_T for Deauthentication Frame */
	/* Init with MGMT Header Length + Length of Fixed Fields + IE Length */
	u2EstimatedFrameLen =
	    (MAC_TX_RESERVED_FIELD + WLAN_MAC_MGMT_HEADER_LEN +
	     REASON_CODE_FIELD_LEN);

	/* Allocate a MSDU_INFO_T */
	prMsduInfo = cnmMgtPktAlloc(prAdapter, u2EstimatedFrameLen);
	if (prMsduInfo == NULL) {
		DBGLOG(SAA, WARN,
		       "No PKT_INFO_T for sending Deauth Request.\n");
		return WLAN_STATUS_RESOURCES;
	}
	/* 4 <6> compose Deauthentication frame header and some fixed fields */
	authComposeDeauthFrameHeaderAndFF((uint8_t *)
					  ((unsigned long)(prMsduInfo->prPacket)
					   + MAC_TX_RESERVED_FIELD),
					  pucReceiveAddr, pucTransmitAddr,
					  pucBssid, u2ReasonCode);

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		/* PMF certification 4.3.3.1, 4.3.3.2 send unprotected
		 * deauth reason 6/7
		 * if (AP mode & not for PMF reply case) OR (STA PMF)
		 */
		if (((GET_BSS_INFO_BY_INDEX
		      (prAdapter,
		       prStaRec->ucBssIndex)->eCurrentOPMode ==
		      OP_MODE_ACCESS_POINT)
		     && (prStaRec->rPmfCfg.fgRxDeauthResp != TRUE))
		    ||
		    (GET_BSS_INFO_BY_INDEX
		     (prAdapter,
		      prStaRec->ucBssIndex)->eNetworkType ==
		     (uint8_t) NETWORK_TYPE_AIS)) {

			struct WLAN_DEAUTH_FRAME *prDeauthFrame;

			prDeauthFrame = (struct WLAN_DEAUTH_FRAME *)(uint8_t *)
			    ((unsigned long)(prMsduInfo->prPacket)
			     + MAC_TX_RESERVED_FIELD);

			prDeauthFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
			DBGLOG(SAA, INFO,
			       "Reason=%d, DestAddr=" MACSTR
			       " srcAddr=" MACSTR " BSSID=" MACSTR "\n",
			       prDeauthFrame->u2ReasonCode,
			       MAC2STR(prDeauthFrame->aucDestAddr),
			       MAC2STR(prDeauthFrame->aucSrcAddr),
			       MAC2STR(prDeauthFrame->aucBSSID));
		}
	}
#endif
	nicTxSetPktLifeTime(prMsduInfo, 5000);

	nicTxSetPktRetryLimit(prMsduInfo, TX_DESC_TX_COUNT_NO_LIMIT);

	/* 4 <7> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     ucBssIndex,
		     ucStaRecIdx,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + REASON_CODE_FIELD_LEN,
		     pfTxDoneHandler, MSDU_RATE_MODE_AUTO);

#if CFG_SUPPORT_802_11W
	/* AP PMF */
	/* caution: access prStaRec only if true */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		/* 4.3.3.1 send unprotected deauth reason 6/7 */
		if (prStaRec->rPmfCfg.fgRxDeauthResp != TRUE) {
			DBGLOG(RSN, INFO,
			       "Deauth Set MSDU_OPT_PROTECTED_FRAME\n");
			nicTxConfigPktOption(prMsduInfo,
					     MSDU_OPT_PROTECTED_FRAME, TRUE);
		}

		prStaRec->rPmfCfg.fgRxDeauthResp = FALSE;
	}
#endif
	DBGLOG(SAA, INFO, "ucTxSeqNum=%d ucStaRecIndex=%d u2ReasonCode=%d\n",
	       prMsduInfo->ucTxSeqNum, prMsduInfo->ucStaRecIndex, u2ReasonCode);

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	{
		struct WLAN_DEAUTH_FRAME *prDeauthFrame;
		struct net_device *prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, ucBssIndex);

		prDeauthFrame = (struct WLAN_DEAUTH_FRAME *) (uint8_t *)
			((unsigned long)
			(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);
		DBGLOG(SAA, INFO,
			"notification of TX deauthentication, %d\n",
			prMsduInfo->u2FrameLength);
		kalIndicateTxDeauthToUpperLayer(
				prDevHandler,
				(uint8_t *)prDeauthFrame,
				(size_t)prMsduInfo->u2FrameLength);

		DBGLOG(SAA, INFO,
			"notification of TX deauthentication, Done\n");
	}
#endif

	/* 4 <8> Inform TXM to send this Deauthentication frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	return WLAN_STATUS_SUCCESS;
}				/* end of authSendDeauthFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Deauthentication
 *        frame if the given BSSID is matched.
 *
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 * @param[in] aucBSSID           Given BSSID
 * @param[out] pu2ReasonCode     Pointer to store the Reason Code from
 *                               Deauthentication.
 *
 * @retval WLAN_STATUS_FAILURE   This is not the frame we should handle at
 *                               current state.
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 */
/*----------------------------------------------------------------------------*/
uint32_t authProcessRxDeauthFrame(IN struct SW_RFB *prSwRfb,
				  IN uint8_t aucBSSID[],
				  OUT uint16_t *pu2ReasonCode)
{
	struct WLAN_DEAUTH_FRAME *prDeauthFrame;
	uint16_t u2RxReasonCode;

	if (!prSwRfb || !aucBSSID || !pu2ReasonCode) {
		DBGLOG(SAA, WARN, "Invalid parameters, ignore pkt!\n");
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <1> locate the Deauthentication Frame. */
	prDeauthFrame = (struct WLAN_DEAUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Parse the Header of Deauthentication Frame. */
#if 0				/* Kevin: Seems redundant */
	WLAN_GET_FIELD_16(&prDeauthFrame->u2FrameCtrl, &u2RxFrameCtrl)
	    u2RxFrameCtrl &= MASK_FRAME_TYPE;
	if (u2RxFrameCtrl != MAC_FRAME_DEAUTH)
		return WLAN_STATUS_FAILURE;

#endif

	if ((prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen) <
	    REASON_CODE_FIELD_LEN) {
		DBGLOG(SAA, WARN, "Invalid Deauth packet length");
		return WLAN_STATUS_FAILURE;
	}

	/* Check if this Deauth Frame is coming from Target BSSID */
	if (UNEQUAL_MAC_ADDR(prDeauthFrame->aucBSSID, aucBSSID)) {
		DBGLOG(SAA, LOUD,
		       "Ignore Deauth Frame from other BSS [" MACSTR "]\n",
		       MAC2STR(prDeauthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}
	/* 4 <3> Parse the Fixed Fields of Deauthentication Frame Body. */
	WLAN_GET_FIELD_16(&prDeauthFrame->u2ReasonCode, &u2RxReasonCode);
	*pu2ReasonCode = u2RxReasonCode;

	return WLAN_STATUS_SUCCESS;

}		/* end of authProcessRxDeauthFrame() */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will parse and process the incoming Authentication
 *        frame.
 *
 * @param[in] prSwRfb                Pointer to SW RFB data structure.
 * @param[in] aucExpectedBSSID       Given Expected BSSID.
 * @param[in] u2ExpectedAuthAlgNum   Given Expected Authentication Algorithm
 *                                   Number
 * @param[in] u2ExpectedTransSeqNum  Given Expected Transaction Sequence Number.
 * @param[out] pu2ReturnStatusCode   Return Status Code.
 *
 * @retval WLAN_STATUS_SUCCESS   This is the frame we should handle.
 * @retval WLAN_STATUS_FAILURE   The frame we will ignore.
 */
/*----------------------------------------------------------------------------*/
uint32_t
authProcessRxAuth1Frame(IN struct ADAPTER *prAdapter,
			IN struct SW_RFB *prSwRfb,
			IN uint8_t aucExpectedBSSID[],
			IN uint16_t u2ExpectedAuthAlgNum,
			IN uint16_t u2ExpectedTransSeqNum,
			OUT uint16_t *pu2ReturnStatusCode)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2ReturnStatusCode = STATUS_CODE_SUCCESSFUL;

	ASSERT(prSwRfb);
	ASSERT(aucExpectedBSSID);
	ASSERT(pu2ReturnStatusCode);

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

	/* 4 <2> Check the BSSID */
	if (UNEQUAL_MAC_ADDR(prAuthFrame->aucBSSID, aucExpectedBSSID))
		return WLAN_STATUS_FAILURE;	/* Just Ignore this MMPDU */

	/* 4 <3> Check the SA, which should not be MC/BC */
	if (prAuthFrame->aucSrcAddr[0] & BIT(0)) {
		DBGLOG(P2P, WARN,
		       "Invalid STA MAC with MC/BC bit set: " MACSTR "\n",
		       MAC2STR(prAuthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <4> Parse the Fixed Fields of Authentication Frame Body. */
	if (prAuthFrame->u2AuthAlgNum != u2ExpectedAuthAlgNum)
		u2ReturnStatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;
#if CFG_SUPPORT_SUPPLICANT_SME
	if (prAuthFrame->aucAuthData[0] != u2ExpectedTransSeqNum)
#else
	if (prAuthFrame->u2AuthTransSeqNo != u2ExpectedTransSeqNum)
#endif
		u2ReturnStatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;

	*pu2ReturnStatusCode = u2ReturnStatusCode;

	return WLAN_STATUS_SUCCESS;

}				/* end of authProcessRxAuth1Frame() */

uint32_t
authProcessRxAuthFrame(IN struct ADAPTER *prAdapter,
			IN struct SW_RFB *prSwRfb,
			IN struct BSS_INFO *prBssInfo,
			OUT uint16_t *pu2ReturnStatusCode)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint16_t u2ReturnStatusCode = STATUS_CODE_SUCCESSFUL;
	uint16_t u2RxTransactionSeqNum = 0;

	if (!prBssInfo)
		return WLAN_STATUS_FAILURE;

	/* 4 <1> locate the Authentication Frame. */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	u2RxTransactionSeqNum = prAuthFrame->aucAuthData[0];
#else
	u2RxTransactionSeqNum = prAuthFrame->u2AuthTransSeqNo;
#endif

	/* 4 <2> Check the BSSID */
	if (UNEQUAL_MAC_ADDR(prAuthFrame->aucBSSID,
		prBssInfo->aucBSSID))
		return WLAN_STATUS_FAILURE;	/* Just Ignore this MMPDU */

	/* 4 <3> Check the SA, which should not be MC/BC */
	if (prAuthFrame->aucSrcAddr[0] & BIT(0)) {
		DBGLOG(P2P, WARN,
		       "Invalid STA MAC with MC/BC bit set: " MACSTR "\n",
		       MAC2STR(prAuthFrame->aucSrcAddr));
		return WLAN_STATUS_FAILURE;
	}

	/* 4 <4> Parse the Fixed Fields of Authentication Frame Body. */
	if (prAuthFrame->u2AuthAlgNum != AUTH_ALGORITHM_NUM_OPEN_SYSTEM &&
		prAuthFrame->u2AuthAlgNum != AUTH_ALGORITHM_NUM_SAE)
		u2ReturnStatusCode = STATUS_CODE_AUTH_ALGORITHM_NOT_SUPPORTED;
	else if (prAuthFrame->u2AuthAlgNum == AUTH_ALGORITHM_NUM_OPEN_SYSTEM &&
		u2RxTransactionSeqNum != AUTH_TRANSACTION_SEQ_1)
		u2ReturnStatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;
	else if (prAuthFrame->u2AuthAlgNum == AUTH_ALGORITHM_NUM_SAE &&
		u2RxTransactionSeqNum != AUTH_TRANSACTION_SEQ_1 &&
		u2RxTransactionSeqNum != AUTH_TRANSACTION_SEQ_2)
		u2ReturnStatusCode = STATUS_CODE_AUTH_OUT_OF_SEQ;

	DBGLOG(AAA, LOUD, "u2ReturnStatusCode = %d\n", u2ReturnStatusCode);

	*pu2ReturnStatusCode = u2ReturnStatusCode;

	return WLAN_STATUS_SUCCESS;

}

/* ToDo: authAddRicIE, authHandleFtIEs, authAddTimeoutIE */

void authAddMDIE(IN struct ADAPTER *prAdapter,
		 IN OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx);

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prMDIE)
		return;
	prMsduInfo->u2FrameLength +=
		5; /* IE size for MD IE is fixed, it is 5 */
	kalMemCopy(pucBuffer, prFtIEs->prMDIE, 5);
}

uint32_t authCalculateRSNIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			       struct STA_RECORD *prStaRec)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
	    aisGetAuthMode(prAdapter, ucBssIdx);
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx);

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prRsnIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				  eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return 0;
	return IE_SIZE(prFtIEs->prRsnIE);
}

void authAddRSNIE(IN struct ADAPTER *prAdapter,
		  IN OUT struct MSDU_INFO *prMsduInfo)
{
	authAddRSNIE_impl(prAdapter, prMsduInfo);
}

uint32_t authAddRSNIE_impl(IN struct ADAPTER *prAdapter,
		  IN OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint32_t ucRSNIeSize = 0;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;
	enum ENUM_PARAM_AUTH_MODE eAuthMode =
	    aisGetAuthMode(prAdapter, ucBssIdx);
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx);

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prRsnIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				  eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return FALSE;

	ucRSNIeSize = IE_SIZE(prFtIEs->prRsnIE);
	prMsduInfo->u2FrameLength += ucRSNIeSize;
	kalMemCopy(pucBuffer, prFtIEs->prRsnIE, ucRSNIeSize);
	return TRUE;
}

/*---------------------------------------------------------------------------*/
/*!
 * @brief This function will validate the Rx Auth Frame and then return
 *        the status code to AAA to indicate
 *        if need to perform following actions
 *        when the specified conditions were matched.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prSwRfb            Pointer to SW RFB data structure.
 *
 * @retval TRUE      Reply the Auth
 * @retval FALSE     Don't reply the Auth
 */
/*---------------------------------------------------------------------------*/
u_int8_t
authFloodingCheck(IN struct ADAPTER *prAdapter,
		IN struct BSS_INFO *prP2pBssInfo,
		IN struct SW_RFB *prSwRfb)
{

	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame = (struct WLAN_AUTH_FRAME *) NULL;

	DBGLOG(SAA, TRACE, "authFloodingCheck Authentication Frame\n");

	prAuthFrame = (struct WLAN_AUTH_FRAME *) prSwRfb->pvHeader;

	if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
		(prP2pBssInfo->eIntendOPMode != OP_MODE_NUM)) {
		/* We are not under AP Mode yet. */
		DBGLOG(P2P, WARN,
			"Current OP mode is not under AP mode. (%d)\n",
			prP2pBssInfo->eCurrentOPMode);
		return FALSE;
	}

	prStaRec = cnmGetStaRecByAddress(prAdapter,
		prP2pBssInfo->ucBssIndex, prAuthFrame->aucSrcAddr);

	if (!prStaRec) {
		DBGLOG(SAA, TRACE, "Need reply.\n");
		return TRUE;
	}

	DBGLOG(SAA, WARN, "Auth Flooding Attack, don't reply.\n");
	return FALSE;
}

