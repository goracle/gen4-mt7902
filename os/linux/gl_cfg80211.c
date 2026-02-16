/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: @(#) gl_cfg80211.c@@
 */

/*! \file   gl_cfg80211.c
 *    \brief  Main routines for supporintg MT6620 cfg80211 control interface
 *
 *    This file contains the support routines of Linux driver for MediaTek Inc.
 *    802.11 Wireless LAN Adapters.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "precomp.h"
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <net/cfg80211.h>
#include "gl_cfg80211.h"
#include "gl_vendor.h"
#include "gl_p2p_os.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define IW_AUTH_WPA_VERSION_WPA3        0x00000008

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

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
 * @brief This routine is responsible for change STA type between
 *        1. Infrastructure Client (Non-AP STA)
 *        2. Ad-Hoc IBSS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_change_iface(struct wiphy *wiphy,
			  struct net_device *ndev, enum nl80211_iftype type,
			  u32 *flags, struct vif_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_OP_MODE rOpMode;
	uint32_t u4BufLen;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	if (type == NL80211_IFTYPE_STATION)
		rOpMode.eOpMode = NET_TYPE_INFRA;
	else if (type == NL80211_IFTYPE_ADHOC)
		rOpMode.eOpMode = NET_TYPE_IBSS;
	else
		return -EINVAL;
	rOpMode.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetInfrastructureMode,
		(void *)&rOpMode, sizeof(struct PARAM_OP_MODE),
		FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set infrastructure mode error:%x\n",
		       rStatus);

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset wpa info */
	prWpaInfo->u4WpaVersion =
		IW_AUTH_WPA_VERSION_DISABLED;
	prWpaInfo->u4KeyMgmt = 0;
	prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prWpaInfo->u4Mfp = IW_AUTH_MFP_DISABLED;
	prWpaInfo->ucRSNMfpCap = 0;
	prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroupMgmt =
						IW_AUTH_CIPHER_NONE;
#endif

	ndev->ieee80211_ptr->iftype = type;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for adding key
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_add_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index, bool pairwise, const u8 *mac_addr,
		     struct key_params *params)
{
	struct PARAM_KEY rKey;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rslt = -EINVAL;
	uint32_t u4BufLen = 0;
	uint8_t tmp1[8], tmp2[8];
	uint8_t ucBssIndex = 0;
	const uint8_t aucBCAddr[] = BC_MAC_ADDR;
	/* const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR; */

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(RSN, INFO, "ucBssIndex = %d\n", ucBssIndex);
#if DBG
	if (mac_addr) {
		DBGLOG(RSN, INFO,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n",
		       key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, INFO, "keyIdx = %d pairwise = %d null mac\n",
		       key_index, pairwise);
	}
	DBGLOG(RSN, INFO, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, INFO, params->key, params->key_len);
#endif

	kalMemZero(&rKey, sizeof(struct PARAM_KEY));

	rKey.u4KeyIndex = key_index;

	if (params->cipher) {
		switch (params->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			rKey.ucCipher = CIPHER_SUITE_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			rKey.ucCipher = CIPHER_SUITE_WEP104;
			break;
#if 0
		case WLAN_CIPHER_SUITE_WEP128:
			rKey.ucCipher = CIPHER_SUITE_WEP128;
			break;
#endif
		case WLAN_CIPHER_SUITE_TKIP:
			rKey.ucCipher = CIPHER_SUITE_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			rKey.ucCipher = CIPHER_SUITE_CCMP;
			break;
#if 0
		case WLAN_CIPHER_SUITE_GCMP:
			rKey.ucCipher = CIPHER_SUITE_GCMP;
			break;
		case WLAN_CIPHER_SUITE_CCMP_256:
			rKey.ucCipher = CIPHER_SUITE_CCMP256;
			break;
#endif
		case WLAN_CIPHER_SUITE_SMS4:
			rKey.ucCipher = CIPHER_SUITE_WPI;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			rKey.ucCipher = CIPHER_SUITE_BIP;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			rKey.ucCipher = CIPHER_SUITE_GCMP_256;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			DBGLOG(RSN, INFO,
				"[TODO] Set BIP-GMAC-256, SW should handle it ...\n");
			return 0;
		default:
			ASSERT(FALSE);
		}
	}

	if (pairwise) {
		ASSERT(mac_addr);
		rKey.u4KeyIndex |= BIT(31);
		rKey.u4KeyIndex |= BIT(30);
		COPY_MAC_ADDR(rKey.arBSSID, mac_addr);
	} else {		/* Group key */
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	if (params->key) {
		if (params->key_len > sizeof(rKey.aucKeyMaterial))
			return -EINVAL;

		kalMemCopy(rKey.aucKeyMaterial, params->key,
			   params->key_len);
		if (rKey.ucCipher == CIPHER_SUITE_TKIP) {
			kalMemCopy(tmp1, &params->key[16], 8);
			kalMemCopy(tmp2, &params->key[24], 8);
			kalMemCopy(&rKey.aucKeyMaterial[16], tmp2, 8);
			kalMemCopy(&rKey.aucKeyMaterial[24], tmp1, 8);
		}
	}

	rKey.ucBssIdx = ucBssIndex;

	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = ((unsigned long) &(((struct PARAM_KEY *)
				     0)->aucKeyMaterial)) + rKey.u4KeyLength;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, &rKey,
			   rKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting key for specified STA
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_get_key(struct wiphy *wiphy,
		     struct net_device *ndev,
		     u8 key_index,
		     bool pairwise,
		     const u8 *mac_addr, void *cookie,
		     void (*callback)(void *cookie, struct key_params *)
		    )
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

#if 1
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	/* not implemented */

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for removing key for specified STA
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_del_key(struct wiphy *wiphy,
			 struct net_device *ndev, u8 key_index, bool pairwise,
			 const u8 *mac_addr)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_REMOVE_KEY rRemoveKey;
	uint32_t u4BufLen = 0;
	int32_t i4Rslt = -EINVAL;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if (g_u4HaltFlag) {
		DBGLOG(RSN, WARN, "wlan is halt, skip key deletion\n");
		return WLAN_STATUS_FAILURE;
	}

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(RSN, TRACE, "ucBssIndex = %d\n", ucBssIndex);
#if DBG
	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n",
		       key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE, "keyIdx = %d pairwise = %d null mac\n",
		       key_index, pairwise);
	}
#endif

	kalMemZero(&rRemoveKey, sizeof(struct PARAM_REMOVE_KEY));
	rRemoveKey.u4KeyIndex = key_index;
	rRemoveKey.u4Length = sizeof(struct PARAM_REMOVE_KEY);
	if (mac_addr) {
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
		rRemoveKey.u4KeyIndex |= BIT(30);
	}

	if (prGlueInfo->prAdapter == NULL)
		return i4Rslt;

	rRemoveKey.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetRemoveKey, &rRemoveKey,
			rRemoveKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(RSN, WARN, "remove key error:%x\n", rStatus);
	else
		i4Rslt = 0;

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting default key on an interface
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int
mtk_cfg80211_set_default_key(struct wiphy *wiphy,
		     struct net_device *ndev, u8 key_index, bool unicast,
		     bool multicast)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_DEFAULT_KEY rDefaultKey;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rst = -EINVAL;
	uint32_t u4BufLen = 0;
	u_int8_t fgDef = FALSE, fgMgtDef = FALSE;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	/* For STA, should wep set the default key !! */
	DBGLOG(RSN, INFO, "ucBssIndex = %d\n", ucBssIndex);
#if DBG
	DBGLOG(RSN, INFO,
	       "keyIdx = %d unicast = %d multicast = %d\n", key_index,
	       unicast, multicast);
#endif

	rDefaultKey.ucKeyID = key_index;
	rDefaultKey.ucUnicast = unicast;
	rDefaultKey.ucMulticast = multicast;
	if (rDefaultKey.ucUnicast && !rDefaultKey.ucMulticast)
		return WLAN_STATUS_SUCCESS;

	if (rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgDef = TRUE;

	if (!rDefaultKey.ucUnicast && rDefaultKey.ucMulticast)
		fgMgtDef = TRUE;

	rDefaultKey.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDefaultKey, &rDefaultKey,
				sizeof(struct PARAM_DEFAULT_KEY),
				FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rst = 0;

	return i4Rst;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting station information such as
 *        RSSI
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_get_station(struct wiphy *wiphy,
			     struct net_device *ndev, const u8 *mac,
			     struct station_info *sinfo)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4BufLen, u4Rate = 0;
	int32_t i4Rssi = 0;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	uint32_t u4TotalError;
	struct net_device_stats *prDevStats;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, TRACE, "ucBssIndex = %d\n", ucBssIndex);
	kalMemZero(arBssid, MAC_ADDR_LEN);
	SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check input MAC address */
	/* On Android O, this might be wlan0 address */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)
	    && UNEQUAL_MAC_ADDR(
		    prGlueInfo->prAdapter->rWifiVar.aucMacAddress, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR
		       "] currently connected BSSID["
		       MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill TX rate */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
		return 0;
	}

#if defined(CFG_REPORT_MAX_TX_RATE) && (CFG_REPORT_MAX_TX_RATE == 1)
	rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidQueryMaxLinkSpeed, &u4Rate,
				sizeof(u4Rate), TRUE, FALSE, FALSE, &u4BufLen,
				ucBssIndex);
#else
	rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidQueryLinkSpeed, &u4Rate,
				sizeof(u4Rate), TRUE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);
#endif /* CFG_REPORT_MAX_TX_RATE */

#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
	sinfo->filled |= BIT(NL80211_STA_INFO_TX_BITRATE);
#else
	sinfo->filled |= STATION_INFO_TX_BITRATE;
#endif
	if ((rStatus != WLAN_STATUS_SUCCESS) || (u4Rate == 0)) {
		/* unable to retrieve link speed */
		DBGLOG(REQ, WARN, "last link speed\n");
		sinfo->txrate.legacy = prGlueInfo->u4LinkSpeedCache;
	} else {
		/* convert from 100bps to 100kbps */
		sinfo->txrate.legacy = u4Rate / 1000;
		prGlueInfo->u4LinkSpeedCache = u4Rate / 1000;
	}

	/* 3. fill RSSI */
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryRssi, &i4Rssi,
				sizeof(i4Rssi), TRUE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);

#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
	sinfo->filled |= BIT(NL80211_STA_INFO_SIGNAL);
#else
	sinfo->filled |= STATION_INFO_SIGNAL;
#endif

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN,
			"Query RSSI failed, use last RSSI %d\n",
			prGlueInfo->i4RssiCache);
		sinfo->signal = prGlueInfo->i4RssiCache ?
			prGlueInfo->i4RssiCache :
			PARAM_WHQL_RSSI_INITIAL_DBM;
	} else if (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM ||
			i4Rssi == PARAM_WHQL_RSSI_MAX_DBM) {
		DBGLOG(REQ, WARN,
			"RSSI abnormal, use last RSSI %d\n",
			prGlueInfo->i4RssiCache);
		sinfo->signal = prGlueInfo->i4RssiCache ?
			prGlueInfo->i4RssiCache : i4Rssi;
	} else {
		sinfo->signal = i4Rssi;	/* dBm */
		prGlueInfo->i4RssiCache = i4Rssi;
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	if (prDevStats) {
		/* 4. fill RX_PACKETS */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_RX_BYTES64);
#else
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->filled |= NL80211_STA_INFO_RX_BYTES64;
#endif
		sinfo->rx_packets = prDevStats->rx_packets;
		sinfo->rx_bytes = prDevStats->rx_bytes;

		/* 5. fill TX_PACKETS */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_PACKETS);
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_BYTES64);
#else
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->filled |= NL80211_STA_INFO_TX_BYTES64;
#endif
		sinfo->tx_packets = prDevStats->tx_packets;
		sinfo->tx_bytes = prDevStats->tx_bytes;

		/* 6. fill TX_FAILED */
		kalMemZero(&rQueryStaStatistics,
			   sizeof(rQueryStaStatistics));
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, arBssid);
		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
				   &rQueryStaStatistics,
				   sizeof(rQueryStaStatistics),
				   TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN,
			       "link speed=%u, rssi=%d, unable to retrieve link speed,status=%u\n",
			       sinfo->txrate.legacy, sinfo->signal, rStatus);
		} else {
			DBGLOG(REQ, INFO,
			       "link speed=%u, rssi=%d, BSSID:[" MACSTR
			       "], TxFail=%u, TxTimeOut=%u, TxOK=%u, RxOK=%u\n",
			       sinfo->txrate.legacy, sinfo->signal,
			       MAC2STR(arBssid),
			       rQueryStaStatistics.u4TxFailCount,
			       rQueryStaStatistics.u4TxLifeTimeoutCount,
			       sinfo->tx_packets, sinfo->rx_packets);

			u4TotalError = rQueryStaStatistics.u4TxFailCount +
				       rQueryStaStatistics.u4TxLifeTimeoutCount;
			prDevStats->tx_errors += u4TotalError;
		}
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
		sinfo->filled |= BIT(NL80211_STA_INFO_TX_FAILED);
#else
		sinfo->filled |= STATION_INFO_TX_FAILED;
#endif
		sinfo->tx_failed = prDevStats->tx_errors;
	}

	return 0;
}
#else
int mtk_cfg80211_get_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac,
			     struct station_info *sinfo)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4BufLen, u4Rate;
	int32_t i4Rssi;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	uint32_t u4TotalError;
	struct net_device_stats *prDevStats;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, TRACE, "ucBssIndex = %d\n", ucBssIndex);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR
		       "] currently connected BSSID["
		       MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill TX rate */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
#if defined(CFG_REPORT_MAX_TX_RATE) && (CFG_REPORT_MAX_TX_RATE == 1)
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryMaxLinkSpeed,
				&u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE,
				&u4BufLen,
				ucBssIndex);
#else
		rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidQueryLinkSpeed, &u4Rate,
				sizeof(u4Rate), TRUE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);
#endif /* CFG_REPORT_MAX_TX_RATE */

		sinfo->filled |= STATION_INFO_TX_BITRATE;

		if ((rStatus != WLAN_STATUS_SUCCESS) || (u4Rate == 0)) {
			/* unable to retrieve link speed */
			DBGLOG(REQ, WARN, "last link speed\n");
			sinfo->txrate.legacy = prGlueInfo->u4LinkSpeedCache;
		} else {
			/* convert from 100bps to 100kbps */
			sinfo->txrate.legacy = u4Rate / 1000;
			prGlueInfo->u4LinkSpeedCache = u4Rate / 1000;
		}
	}

	/* 3. fill RSSI */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidQueryRssi, &i4Rssi,
				sizeof(i4Rssi), TRUE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);

		sinfo->filled |= STATION_INFO_SIGNAL;

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN,
				"Query RSSI failed, use last RSSI %d\n",
				prGlueInfo->i4RssiCache);
			sinfo->signal = prGlueInfo->i4RssiCache ?
				prGlueInfo->i4RssiCache :
				PARAM_WHQL_RSSI_INITIAL_DBM;
		} else if (i4Rssi == PARAM_WHQL_RSSI_MIN_DBM ||
			i4Rssi == PARAM_WHQL_RSSI_MAX_DBM) {
			DBGLOG(REQ, WARN,
				"RSSI abnormal, use last RSSI %d\n",
				prGlueInfo->i4RssiCache);
			sinfo->signal = prGlueInfo->i4RssiCache ?
				prGlueInfo->i4RssiCache : i4Rssi;
		} else {
			sinfo->signal = i4Rssi;	/* dBm */
			prGlueInfo->i4RssiCache = i4Rssi;
		}
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);
	if (prDevStats) {
		/* 4. fill RX_PACKETS */
		sinfo->filled |= STATION_INFO_RX_PACKETS;
		sinfo->rx_packets = prDevStats->rx_packets;

		/* 5. fill TX_PACKETS */
		sinfo->filled |= STATION_INFO_TX_PACKETS;
		sinfo->tx_packets = prDevStats->tx_packets;

		/* 6. fill TX_FAILED */
		kalMemZero(&rQueryStaStatistics,
			   sizeof(rQueryStaStatistics));
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, arBssid);
		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
				   &rQueryStaStatistics,
				   sizeof(rQueryStaStatistics),
				   TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN,
			       "link speed=%u, rssi=%d, unable to get sta statistics: status=%u\n",
			       sinfo->txrate.legacy, sinfo->signal, rStatus);
		} else {
			DBGLOG(REQ, INFO,
			       "link speed=%u, rssi=%d, BSSID=[" MACSTR
			       "], TxFailCount=%d, LifeTimeOut=%d\n",
			       sinfo->txrate.legacy, sinfo->signal,
			       MAC2STR(arBssid),
			       rQueryStaStatistics.u4TxFailCount,
			       rQueryStaStatistics.u4TxLifeTimeoutCount);

			u4TotalError = rQueryStaStatistics.u4TxFailCount +
				       rQueryStaStatistics.u4TxLifeTimeoutCount;
			prDevStats->tx_errors += u4TotalError;
		}
		sinfo->filled |= STATION_INFO_TX_FAILED;
		sinfo->tx_failed = prDevStats->tx_errors;
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for getting statistics for Link layer
 *        statistics
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_get_link_statistics(struct wiphy *wiphy,
				     struct net_device *ndev, u8 *mac,
				     struct station_info *sinfo)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4BufLen;
	int32_t i4Rssi;
	struct PARAM_GET_STA_STATISTICS *prQryStaStats;
	struct PARAM_GET_BSS_STATISTICS rQueryBssStatistics;
	struct net_device_stats *prDevStats;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, TRACE, "ucBssIndex = %d\n", ucBssIndex);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);

	/* 1. check BSSID */
	if (UNEQUAL_MAC_ADDR(arBssid, mac)) {
		/* wrong MAC address */
		DBGLOG(REQ, WARN,
		       "incorrect BSSID: [" MACSTR
		       "] currently connected BSSID["
		       MACSTR "]\n",
		       MAC2STR(mac), MAC2STR(arBssid));
		return -ENOENT;
	}

	/* 2. fill RSSI */
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
	} else {
		rStatus = kalIoctlByBssIdx(prGlueInfo,
			wlanoidQueryRssi, &i4Rssi,
			sizeof(i4Rssi), TRUE, FALSE, TRUE, &u4BufLen,
			ucBssIndex);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN, "unable to retrieve rssi\n");
	}

	/* Get statistics from net_dev */
	prDevStats = (struct net_device_stats *)kalGetStats(ndev);

	prQryStaStats = (struct PARAM_GET_STA_STATISTICS *)
			kalMemAlloc(
			sizeof(struct PARAM_GET_STA_STATISTICS),
			VIR_MEM_TYPE);
	if (!prQryStaStats) {
		DBGLOG(REQ, INFO, "mem is null\n");
		return -ENOMEM;
	}

	/*3. get link layer statistics from Driver and FW */
	if (prDevStats) {
		/* 3.1 get per-STA link statistics */
		kalMemZero(prQryStaStats,
			   sizeof(*prQryStaStats));
		COPY_MAC_ADDR(prQryStaStats->aucMacAddr, arBssid);
		prQryStaStats->ucLlsReadClear =
			FALSE;	/* dont clear for get BSS statistic */

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
				   prQryStaStats,
				   sizeof(*prQryStaStats),
				   TRUE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN,
			       "unable to retrieve per-STA link statistics\n");

		/*3.2 get per-BSS link statistics */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			kalMemZero(&rQueryBssStatistics,
				   sizeof(rQueryBssStatistics));
			rQueryBssStatistics.ucBssIndex = ucBssIndex;

			rStatus = kalIoctl(prGlueInfo,
				wlanoidQueryBssStatistics,
				&rQueryBssStatistics,
				sizeof(rQueryBssStatistics),
				TRUE, FALSE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(REQ, WARN,
					"unable to retrieve bss statistics\n");
		} else {
			DBGLOG(REQ, WARN,
			       "unable to retrieve per-BSS link statistics\n");
		}

	}

	kalMemFree(prQryStaStats, VIR_MEM_TYPE,
		   sizeof(*prQryStaStats));

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to do a scan
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_scan(struct wiphy *wiphy,
		      struct cfg80211_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t i, j = 0, u4BufLen;
	struct PARAM_SCAN_REQUEST_ADV *prScanRequest;
	uint32_t wildcard_flag = 0;
	uint8_t ucBssIndex = 0;

	if (kalIsResetting())
		return -EBUSY;

	if (!wiphy || !request || !request->wdev)
		return -EINVAL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (!prGlueInfo || !prGlueInfo->prAdapter) {
		DBGLOG(REQ, ERROR, "prGlueInfo or Adapter is NULL\n");
		return -EIO;
	}

#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
	if (prGlueInfo->fgIsEnableMon)
		return -EOPNOTSUPP; /* Changed from EINVAL to be more compliant */
#endif

	ucBssIndex = wlanGetBssIdx(request->wdev->netdev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	if (wlanIsChipAssert(prGlueInfo->prAdapter))
		return -EBUSY;

	/* check if there is any pending scan not yet finished */
	if (prGlueInfo->prScanRequest != NULL) {
		DBGLOG(REQ, WARN, "Scan already in progress (pending request)\n");
		return -EBUSY;
	}

	prScanRequest = kalMemAlloc(sizeof(struct PARAM_SCAN_REQUEST_ADV), VIR_MEM_TYPE);
	if (prScanRequest == NULL) {
		DBGLOG(REQ, ERROR, "alloc scan request fail\n");
		return -ENOMEM;
	}
	kalMemZero(prScanRequest, sizeof(struct PARAM_SCAN_REQUEST_ADV));

	/* 1. Handle SSID logic */
	if (request->n_ssids == 0) {
		prScanRequest->u4SsidNum = 0;
		prScanRequest->ucScanType = SCAN_TYPE_PASSIVE_SCAN;
		DBGLOG(REQ, INFO, "Passive wildcard scan (0 SSIDs)\n");
	} else if (request->ssids && request->n_ssids > 0) {
		uint32_t u4ValidIdx = 0;
		for (i = 0; i < request->n_ssids && u4ValidIdx < SCN_SSID_MAX_NUM; i++) {
			if (request->ssids[i].ssid_len == 0 || request->ssids[i].ssid[0] == 0) {
				wildcard_flag |= (1 << i);
				continue;
			}
			COPY_SSID(prScanRequest->rSsid[u4ValidIdx].aucSsid,
				  prScanRequest->rSsid[u4ValidIdx].u4SsidLen,
				  request->ssids[i].ssid,
				  request->ssids[i].ssid_len);
			
			if (prScanRequest->rSsid[u4ValidIdx].u4SsidLen > ELEM_MAX_LEN_SSID)
				prScanRequest->rSsid[u4ValidIdx].u4SsidLen = ELEM_MAX_LEN_SSID;
			
			u4ValidIdx++;
		}
		prScanRequest->u4SsidNum = u4ValidIdx;
		prScanRequest->ucScanType = SCAN_TYPE_ACTIVE_SCAN;
	}

	/* 2. CRITICAL FIX: Handle Channel List (The iwd fix) */
	if (request->n_channels == 0) {
		/* CONTRACT: 0 channels means "Scan everything supported in this region" */
		prScanRequest->u4ChannelNum = 0; 
		DBGLOG(REQ, INFO, "iwd requested full-band scan (0 channels)\n");
	} else {
		for (i = 0, j = 0; i < request->n_channels && j < MAXIMUM_OPERATION_CHANNEL_LIST; i++) {
			uint32_t u4channel = nicFreq2ChannelNum(request->channels[i]->center_freq * 1000);
			
			if (u4channel == 0) continue;

#if (CFG_SUPPORT_WIFI_6G == 1)
			/* 6G PSC channel optimization: Only scan PSCs if we are constrained */
			if (request->channels[i]->band == KAL_BAND_6GHZ) {
				if (((u4channel - 5) % 16) != 0)
					continue;
			}
#endif
			prScanRequest->arChannel[j].ucChannelNum = u4channel;
			
			switch (request->channels[i]->band) {
			case KAL_BAND_2GHZ: prScanRequest->arChannel[j].eBand = BAND_2G4; break;
			case KAL_BAND_5GHZ: prScanRequest->arChannel[j].eBand = BAND_5G; break;
#if (CFG_SUPPORT_WIFI_6G == 1)
			case KAL_BAND_6GHZ: prScanRequest->arChannel[j].eBand = BAND_6G; break;
#endif
			default:            prScanRequest->arChannel[j].eBand = BAND_NULL; break;
			}
			j++;
		}
		prScanRequest->u4ChannelNum = j;
	}

	/* 3. Random MAC and IEs */
	if (kalScanParseRandomMac(request->wdev->netdev, request, prScanRequest->aucRandomMac)) {
		prScanRequest->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	}

	if (request->ie_len > 0) {
		prScanRequest->u4IELength = request->ie_len;
		prScanRequest->pucIE = (uint8_t *)(request->ie);
	}

	/* 4. Finalize and Send */
	prScanRequest->ucBssIndex = ucBssIndex;
	prScanRequest->fg6gOobRnrParseEn = TRUE;
	prGlueInfo->prScanRequest = request;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetBssidListScanAdv,
			   prScanRequest, sizeof(struct PARAM_SCAN_REQUEST_ADV),
			   FALSE, FALSE, FALSE, &u4BufLen);

	kalMemFree(prScanRequest, sizeof(struct PARAM_SCAN_REQUEST_ADV), VIR_MEM_TYPE);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		prGlueInfo->prScanRequest = NULL;
		DBGLOG(REQ, WARN, "Scan firmware error: 0x%x\n", rStatus);
		return -EBUSY; /* Return EBUSY so iwd retries instead of failing */
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for abort an ongoing scan. The driver
 *        shall indicate the status of the scan through cfg80211_scan_done()
 *
 * @param wiphy - pointer of wireless hardware description
 *        wdev - pointer of  wireless device state
 *
 */
/*----------------------------------------------------------------------------*/
void mtk_cfg80211_abort_scan(struct wiphy *wiphy,
			     struct wireless_dev *wdev)
{
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return;

	DBGLOG(REQ, TRACE, "ucBssIndex = %d\n", ucBssIndex);

	scanlog_dbg(LOG_SCAN_ABORT_REQ_K2D, INFO, "mtk_cfg80211_abort_scan\n");

	rStatus = kalIoctlByBssIdx(prGlueInfo,
			   wlanoidAbortScan,
			   NULL, 1, FALSE, FALSE, TRUE, &u4SetInfoLen,
			   ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, ERROR, "wlanoidAbortScan fail 0x%x\n", rStatus);
}

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting auth to
 *        the ESS with the specified parameters
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_auth(struct wiphy *wiphy,
	struct net_device *ndev, struct cfg80211_auth_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;
	struct PARAM_CONNECT rNewSsid;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif
	struct PARAM_WEP *prWepKey;
	struct PARAM_WEP wepKey;
	u_int8_t fgAddWepPending = FALSE;
	struct PARAM_OP_MODE rOpMode;
	uint8_t fgNewAuthParam = FALSE;
#if CFG_SUPPORT_802_11R
	uint32_t u4InfoBufLen = 0;
#endif
	/* bounded scan wait parameters */
	const int scan_wait_step_ms = 10;
	const int scan_wait_max_ms = 3000; /* 3s max wait for scan to finish */
	int scan_wait_elapsed = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;


	prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter, ucBssIndex);
	prAisFsmInfo->fgIsCfg80211Connecting = TRUE;


	rOpMode.ucBssIdx = ucBssIndex;

	DBGLOG(REQ, INFO, "auth to BSS [" MACSTR "]\n",
		MAC2STR((uint8_t *)req->bss->bssid));
	DBGLOG(REQ, INFO, "auth_type:%d\n", req->auth_type);

	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);
	if (!prConnSettings) {
		DBGLOG(REQ, ERROR, "No conn settings for BSS %u\n", ucBssIndex);
		return -ENODEV;
	}

	/* ====================================================================
	 * <1> Set OP mode
	 * ==================================================================== */
	if (prConnSettings->eOPMode > NET_TYPE_AUTO_SWITCH)
		rOpMode.eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		rOpMode.eOpMode = prConnSettings->eOPMode;

	/* Wait for any ongoing scan to finish, but bounded to avoid indefinite blocking */
	{
		struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo = NULL;
		struct SCAN_INFO *prScanInfo = NULL;

		prAisSpecBssInfo = aisGetAisSpecBssInfo(prGlueInfo->prAdapter, ucBssIndex);
		prScanInfo = &prGlueInfo->prAdapter->rWifiVar.rScanInfo;

		if (!prAisSpecBssInfo || !prAisFsmInfo || !prScanInfo) {
			DBGLOG(REQ, WARN, "Scan/ais structures missing, skipping bounded wait\n");
		} else {
			DBGLOG(REQ, WARN, "[AUTH-DBG] Pre-flight checks:\n");
			DBGLOG(REQ, WARN, "[AUTH-DBG]   AIS State: %u (0=IDLE)\n", prAisFsmInfo->eCurrentState);
			DBGLOG(REQ, WARN, "[AUTH-DBG]   Scan State: %u (0=IDLE)\n", prScanInfo->eCurrentState);
			DBGLOG(REQ, WARN, "[AUTH-DBG]   fgIsScanning: %u\n", prAisFsmInfo->fgIsScanning);

			while ((prAisFsmInfo->eCurrentState == AIS_STATE_SCAN ||
				prScanInfo->eCurrentState == SCAN_STATE_SCANNING) &&
				scan_wait_elapsed < scan_wait_max_ms) {
				kalMsleep(scan_wait_step_ms);
				scan_wait_elapsed += scan_wait_step_ms;
			}

			if (scan_wait_elapsed >= scan_wait_max_ms) {
				DBGLOG(REQ, WARN, "[AUTH-FIX] Scan wait timed out after %d ms\n", scan_wait_elapsed);
			} else {
				DBGLOG(REQ, WARN, "[AUTH-FIX] Scan finished after %d ms\n", scan_wait_elapsed);
			}
		}
	}

	/* small settle delay after scan completes or times out */
	kalMsleep(50);
	DBGLOG(REQ, WARN, "[AUTH-FIX] Post-scan settle delay (50ms) complete\n");

	/* Send OP mode set as an OID (async) and accept pending as success-in-progress */
	rStatus = kalIoctl(prGlueInfo,
		wlanoidSetInfrastructureMode,
		&rOpMode, sizeof(rOpMode),
		FALSE, FALSE, TRUE, &u4BufLen);

	DBGLOG(INIT, INFO, "wlanoidSetInfrastructureMode returned: 0x%x\n", rStatus);

	if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
	    DBGLOG(INIT, ERROR, "wlanoidSetInfrastructureMode failed: 0x%x\n", rStatus);
	    return -EIO;
	}

	/*
	* IMPORTANT: If the infrastructure-mode OID returned PENDING, wait (bounded)
	* for its completion before proceeding to the connect OID. Otherwise we
	* issue overlapping OIDs and hit "SKIP multiple OID complete!" / abort paths.
	*/
	if (rStatus == WLAN_STATUS_PENDING) {
	    unsigned long wait_j = msecs_to_jiffies(2000); /* 2s timeout, tune as needed */
	    long wait_ret;

	    /* ensure completion object is in a clean state before waiting */
	    reinit_completion(&prGlueInfo->rPendComp);
	    prGlueInfo->u4OidCompleteFlag = 0;
	    prGlueInfo->rPendStatus = WLAN_STATUS_FAILURE;

	    DBGLOG(REQ, INFO, "Waiting for INFRA OID completion (%lu ms)...\n",
		jiffies_to_msecs(wait_j));

	    wait_ret = wait_for_completion_timeout(&prGlueInfo->rPendComp, wait_j);

	    if (wait_ret == 0) {
		DBGLOG(REQ, WARN, "INFRA OID timed out after %u ms\n", 2000);
		/* try to recover: treat as failure */
		return -ETIMEDOUT;
	    }

	    /* completion arrived, check status set by kalOidComplete() */
	    rStatus = prGlueInfo->rPendStatus;
	    prGlueInfo->u4OidCompleteFlag = 0; /* clear for future use */

	    DBGLOG(REQ, INFO, "INFRA OID completion status: 0x%x\n", rStatus);

	    if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "INFRA OID failed: 0x%x\n", rStatus);
		return -EIO;
	    }
	}
	/* ====================================================================
	 * <2> Set Auth data (for SAE, etc.)
	 * ==================================================================== */
	prConnSettings->ucAuthDataLen = 0;

#if KERNEL_VERSION(4, 10, 0) > CFG80211_VERSION_CODE
	if (req->sae_data && req->sae_data_len > 0) {
		if (req->sae_data_len > AUTH_DATA_MAX_LEN) {
			DBGLOG(REQ, ERROR,
				"SAE data too large: %zu > %d\n",
				req->sae_data_len, AUTH_DATA_MAX_LEN);
			return -EINVAL;
		}

		kalMemCopy(prConnSettings->aucAuthData,
			   req->sae_data, req->sae_data_len);
		prConnSettings->ucAuthDataLen = (uint8_t)req->sae_data_len;

		DBGLOG(REQ, INFO, "SAE auth data: %d bytes\n",
			prConnSettings->ucAuthDataLen);
		DBGLOG_MEM8(REQ, TRACE,
			    prConnSettings->aucAuthData, req->sae_data_len);
	}
#else
	if (req->auth_data && req->auth_data_len > 0) {
		if (req->auth_data_len > AUTH_DATA_MAX_LEN) {
			DBGLOG(REQ, ERROR,
				"Auth data too large: %zu > %d\n",
				req->auth_data_len, AUTH_DATA_MAX_LEN);
			return -EINVAL;
		}

		kalMemCopy(prConnSettings->aucAuthData,
			   req->auth_data, req->auth_data_len);
		prConnSettings->ucAuthDataLen = (uint8_t)req->auth_data_len;

		DBGLOG(REQ, INFO, "Auth data: %d bytes\n",
			prConnSettings->ucAuthDataLen);
		DBGLOG_MEM8(REQ, TRACE,
			    prConnSettings->aucAuthData, req->auth_data_len);
	}
#endif

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	/* ====================================================================
	 * <3> Set Channel Number from BSS info
	 * ==================================================================== */
	if (req->bss && req->bss->channel && req->bss->channel->center_freq) {
		prConnSettings->ucChannelNum =
			nicFreq2ChannelNum(req->bss->channel->center_freq * 1000);
		DBGLOG(RSN, INFO, "Channel: %d\n",
			prConnSettings->ucChannelNum);
	} else {
		prConnSettings->ucChannelNum = 0;
		DBGLOG(RSN, WARN, "BSS channel info unavailable\n");
	}
#endif

#if CFG_SUPPORT_REPLAY_DETECTION
	/* Reset replay detection state */
	prDetRplyInfo = aisGetDetRplyInfo(prGlueInfo->prAdapter, ucBssIndex);
	if (prDetRplyInfo)
		kalMemZero(prDetRplyInfo, sizeof(struct GL_DETECT_REPLAY_INFO));
#endif

	/* ====================================================================
	 * <4> Set Authentication Algorithm
	 * ==================================================================== */
	switch (req->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		if (!(prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg &
						AUTH_TYPE_OPEN_SYSTEM))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg = 0;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg |=
						AUTH_TYPE_OPEN_SYSTEM;
		break;
		
	case NL80211_AUTHTYPE_SHARED_KEY:
		if (!(prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg &
						AUTH_TYPE_SHARED_KEY))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg = 0;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg |=
						AUTH_TYPE_SHARED_KEY;
		break;
		
	case NL80211_AUTHTYPE_SAE:
		if (!(prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg &
						AUTH_TYPE_SAE))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg = 0;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg |=
						AUTH_TYPE_SAE;
		break;
		
#if CFG_SUPPORT_802_11R
	case NL80211_AUTHTYPE_FT:
		if (!(prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg &
			AUTH_TYPE_FAST_BSS_TRANSITION))
			fgNewAuthParam = TRUE;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg |= 
			AUTH_TYPE_FAST_BSS_TRANSITION;
		break;
#endif
		
	default:
		DBGLOG(REQ, WARN,
			"Unsupported auth type %u, defaulting to OPEN\n",
			req->auth_type);
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg = 0;
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg |=
						AUTH_TYPE_OPEN_SYSTEM;
		break;
	}
	
	DBGLOG(REQ, INFO, "Auth algorithm: 0x%x\n",
		prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg);

#if CFG_SUPPORT_PASSPOINT
	prGlueInfo->fgConnectHS20AP = FALSE;
#endif

	/* ====================================================================
	 * <5> Handle WEP keys (for SHARED_KEY auth)
	 * ==================================================================== */
	if (req->key && req->key_len > 0) {
		if (!(prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg &
						AUTH_TYPE_SHARED_KEY)) {
			DBGLOG(REQ, WARN,
				"WEP key provided but auth is not SHARED_KEY (alg=0x%x)\n",
				prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg);
		}

		if (req->key_len > MAX_KEY_LEN) {
			DBGLOG(REQ, ERROR,
				"WEP key too large: %zu > %u\n",
				req->key_len, MAX_KEY_LEN);
			return -EINVAL;
		}

		prWepKey = &wepKey;
		kalMemZero(prWepKey, sizeof(struct PARAM_WEP));

		prWepKey->u4Length = OFFSET_OF(struct PARAM_WEP,
						aucKeyMaterial) + req->key_len;
		prWepKey->u4KeyLength = (uint32_t)req->key_len;
		prWepKey->u4KeyIndex = (uint32_t)req->key_idx;
		prWepKey->u4KeyIndex |= IS_TRANSMIT_KEY;

		kalMemCopy(prWepKey->aucKeyMaterial,
			   req->key, prWepKey->u4KeyLength);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetAddWep,
				   prWepKey, prWepKey->u4Length,
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
			DBGLOG(REQ, ERROR,
				"wlanoidSetAddWep failed: 0x%x\n", rStatus);
			return -EFAULT;
		}

		DBGLOG(REQ, INFO,
			"WEP key queued/installed: idx=%u len=%u tx=%d\n",
			prWepKey->u4KeyIndex & ~IS_TRANSMIT_KEY,
			prWepKey->u4KeyLength,
			!!(prWepKey->u4KeyIndex & IS_TRANSMIT_KEY));
	}

	/* ====================================================================
	 * <6> Prepare connection parameters
	 * ==================================================================== */
	kalMemZero(&rNewSsid, sizeof(struct PARAM_CONNECT));
	rNewSsid.pucBssid = (uint8_t *)req->bss->bssid;

	if (!EQUAL_MAC_ADDR(rNewSsid.pucBssid, prConnSettings->aucBSSID)) {
		DBGLOG(REQ, INFO,
			"BSSID changed: " MACSTR " -> " MACSTR "\n",
			MAC2STR(prConnSettings->aucBSSID),
			MAC2STR(rNewSsid.pucBssid));
		fgNewAuthParam = TRUE;
	}

	/* ====================================================================
	 * <7> Extract SSID from IEs (FIXED VERSION)
	 * ==================================================================== */
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT || CFG_SUPPORT_802_11R
	if (req->bss && req->bss->ies && req->bss->ies->len >= 2) {
		const uint8_t *pucIE = req->bss->ies->data;
		const uint8_t *pucIEEnd = pucIE + req->bss->ies->len;

		/* Parse IEs to find SSID element (ID = 0) */
		while (pucIE + 2 <= pucIEEnd) {
			uint8_t ucElemId = pucIE[0];
			uint8_t ucElemLen = pucIE[1];

			/* Bounds check */
			if (pucIE + 2 + ucElemLen > pucIEEnd) {
				DBGLOG(REQ, WARN,
					"IE overflow: id=%u len=%u remaining=%zu\n",
					ucElemId, ucElemLen,
					(size_t)(pucIEEnd - pucIE));
				break;
			}

			if (ucElemId == ELEM_ID_SSID) {
				/* Found SSID IE */
				if (ucElemLen > 0 && ucElemLen <= 32) {
					/* Cast away const - driver expects non-const pointer */
					rNewSsid.pucSsid = (uint8_t *)&pucIE[2];
					rNewSsid.u4SsidLen = ucElemLen;

					/* Create null-terminated copy for safe logging */
					char ssid_str[33];
					kalMemCopy(ssid_str, &pucIE[2], ucElemLen);
					ssid_str[ucElemLen] = '\0';

					DBGLOG(REQ, INFO,
						"Parsed SSID from IEs: len=%u ssid='%s'\n",
						ucElemLen,
						(ucElemLen > 0) ? ssid_str : "(hidden)");
				} else if (ucElemLen == 0) {
					/* Hidden SSID */
					rNewSsid.pucSsid = (uint8_t *)&pucIE[2];
					rNewSsid.u4SsidLen = 0;

					DBGLOG(REQ, INFO, "Parsed SSID from IEs: len=0 (hidden)\n");
				} else {
					DBGLOG(REQ, WARN,
						"Invalid SSID length: %u\n",
						ucElemLen);
				}
				break;
			}

			/* Move to next IE */
			pucIE += 2 + ucElemLen;
		}
	} else {
		DBGLOG(REQ, INFO, "No IEs provided in auth request\n");
	}
#endif

	/* ====================================================================
	 * <8> Handle FT (802.11r) IEs
	 * ==================================================================== */
#if CFG_SUPPORT_802_11R
	if (req->auth_type == NL80211_AUTHTYPE_FT) {
		if (req->ie && req->ie_len > 0) {
			rStatus = kalIoctl(prGlueInfo, wlanoidUpdateFtIes,
					   (void *)req->ie, req->ie_len,
					   FALSE, FALSE, FALSE, &u4InfoBufLen);
			if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
				DBGLOG(REQ, ERROR,
					"wlanoidUpdateFtIes failed: 0x%x\n",
					rStatus);
				return -EINVAL;
			}
			DBGLOG(REQ, INFO, "FT IEs updated: %zu bytes\n",
				req->ie_len);
		}
	}
#endif

	/* ====================================================================
	 * <9> Initiate connection/authentication
	 * ==================================================================== */
	prConnSettings->fgIsSendAssoc = FALSE;

	if (!prConnSettings->fgIsConnInitialized || fgNewAuthParam) {
		if (fgNewAuthParam) {
			DBGLOG(REQ, INFO,
				"New auth parameters, reinitializing connection\n");
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidSetConnect,
				   (void *)&rNewSsid,
				   sizeof(struct PARAM_CONNECT),
				   FALSE, FALSE, FALSE, &u4BufLen);

		/* Accept success-in-progress (PENDING) for async firmware */
		if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
			DBGLOG(REQ, ERROR,
				"wlanoidSetConnect failed: 0x%x\n", rStatus);
			return -EIO;
		}
	} else {
		/* Already connected, just send auth frame */
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSendAuthAssoc,
					    (void *)req->bss->bssid,
					    MAC_ADDR_LEN,
					    FALSE, FALSE, TRUE,
					    &u4BufLen, ucBssIndex);
		if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
			DBGLOG(REQ, ERROR,
				"wlanoidSendAuthAssoc failed: 0x%x\n",
				rStatus);
			return -EIO;
		}
	}

	/* ====================================================================
	 * <10> Handle deferred WEP key installation
	 * ==================================================================== */
	if (fgAddWepPending) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetAddWep,
				   prWepKey, prWepKey->u4Length,
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS && rStatus != WLAN_STATUS_PENDING) {
			DBGLOG(REQ, ERROR,
				"Deferred wlanoidSetAddWep failed: 0x%x\n",
				rStatus);
			return -EFAULT;
		}
	}

	return 0;
}





/* Add .disassoc method to avoid kernel WARN_ON when insmod wlan.ko */
int mtk_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *ndev,
			struct cfg80211_disassoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;

	ASSERT(wiphy);

	prGlueInfo = ((struct GLUE_INFO *) wiphy_priv(wiphy));

	DBGLOG(REQ, TRACE, "mtk_cfg80211_disassoc.\n");

	/* not implemented yet */

	return -EINVAL;
}

#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to connect to
 *        the ESS with the specified parameters
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
/* ============================================================================
 * REFACTORED mtk_cfg80211_connect() - Broken into logical helper functions
 * ============================================================================ */

/* Forward declarations of helper functions */
static int validate_and_init_connection(struct wiphy *wiphy, 
                                        struct net_device *ndev,
                                        struct cfg80211_connect_params *sme,
                                        struct GLUE_INFO **out_glue_info,
                                        uint8_t *out_bss_index);

static int configure_operation_mode(struct GLUE_INFO *prGlueInfo,
                                   struct CONNECTION_SETTINGS *prConnSettings,
                                   uint8_t ucBssIndex);

static void reset_security_info(struct GLUE_INFO *prGlueInfo,
                               uint8_t ucBssIndex);

static int configure_auth_and_wpa_version(struct cfg80211_connect_params *sme,
                                         struct GL_WPA_INFO *prWpaInfo);

static int configure_cipher_suites(struct cfg80211_connect_params *sme,
                                  struct GLUE_INFO *prGlueInfo,
                                  struct GL_WPA_INFO *prWpaInfo,
                                  struct CONNECTION_SETTINGS *prConnSettings,
                                  uint8_t ucBssIndex);

static int configure_akm_suite(struct cfg80211_connect_params *sme,
                              struct GLUE_INFO *prGlueInfo,
                              struct GL_WPA_INFO *prWpaInfo,
                              struct CONNECTION_SETTINGS *prConnSettings,
                              enum ENUM_PARAM_AUTH_MODE *out_auth_mode,
                              uint32_t *out_akm_suite,
                              uint8_t ucBssIndex);

static int process_information_elements(struct cfg80211_connect_params *sme,
                                       struct GLUE_INFO *prGlueInfo,
                                       struct GL_WPA_INFO *prWpaInfo,
                                       struct CONNECTION_SETTINGS *prConnSettings,
                                       u_int8_t *out_carry_wps_ie,
                                       uint8_t ucBssIndex);

static int configure_mfp_settings(struct cfg80211_connect_params *sme,
                                 struct GL_WPA_INFO *prWpaInfo);

static int set_auth_and_encryption(struct GLUE_INFO *prGlueInfo,
                                  enum ENUM_PARAM_AUTH_MODE eAuthMode,
                                  struct GL_WPA_INFO *prWpaInfo,
                                  uint8_t ucBssIndex);

static int configure_wep_key(struct cfg80211_connect_params *sme,
                           struct GLUE_INFO *prGlueInfo,
                           struct GL_WPA_INFO *prWpaInfo,
                           uint8_t ucBssIndex);

static void apply_channel_and_bssid_lock(struct cfg80211_connect_params *sme,
                                        struct GLUE_INFO *prGlueInfo,
                                        struct CONNECTION_SETTINGS *prConnSettings,
                                        uint8_t ucBssIndex);

static int initiate_connection(struct cfg80211_connect_params *sme,
                              struct GLUE_INFO *prGlueInfo,
                              struct IEEE_802_11_MIB *prMib,
                              uint32_t u4AkmSuite,
                              uint8_t ucBssIndex);

/* ============================================================================
 * MAIN ENTRY POINT - Now clean and readable
 * ============================================================================ */
int mtk_cfg80211_connect(struct wiphy *wiphy,
                        struct net_device *ndev,
                        struct cfg80211_connect_params *sme)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	struct GL_WPA_INFO *prWpaInfo = NULL;
	struct IEEE_802_11_MIB *prMib = NULL;
	enum ENUM_PARAM_AUTH_MODE eAuthMode = AUTH_MODE_OPEN;
	uint32_t u4AkmSuite = 0;
	uint8_t ucBssIndex = 0;
	u_int8_t fgCarryWPSIE = FALSE;
	int ret;

	/* Step 1: Validate parameters and get BSS index */
	ret = validate_and_init_connection(wiphy, ndev, sme, &prGlueInfo, &ucBssIndex);
	if (ret != 0)
		return ret;

	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);
	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter, ucBssIndex);
	prMib = aisGetMib(prGlueInfo->prAdapter, ucBssIndex);

	/* Step 2: Configure operation mode */
	ret = configure_operation_mode(prGlueInfo, prConnSettings, ucBssIndex);
	if (ret != 0)
		return ret;

	/* Step 3: Reset all security state */
	reset_security_info(prGlueInfo, ucBssIndex);

	/* Step 4: Configure authentication type and WPA version */
	ret = configure_auth_and_wpa_version(sme, prWpaInfo);
	if (ret != 0)
		return ret;

	/* Step 5: Configure cipher suites (pairwise and group) */
	ret = configure_cipher_suites(sme, prGlueInfo, prWpaInfo, prConnSettings, ucBssIndex);
	if (ret != 0)
		return ret;

	/* Step 6: Configure AKM suite and determine auth mode */
	ret = configure_akm_suite(sme, prGlueInfo, prWpaInfo, prConnSettings, 
	                         &eAuthMode, &u4AkmSuite, ucBssIndex);
	if (ret != 0)
		return ret;

	/* Step 7: Process IEs (WPS, Passpoint, RSNE, vendor IEs) */
	ret = process_information_elements(sme, prGlueInfo, prWpaInfo, 
	                                   prConnSettings, &fgCarryWPSIE, ucBssIndex);
	if (ret != 0)
		return ret;

	/* Step 8: Configure MFP (Management Frame Protection) */
	ret = configure_mfp_settings(sme, prWpaInfo);
	if (ret != 0)
		return ret;

	/* Step 9: Set authentication mode and encryption status */
	ret = set_auth_and_encryption(prGlueInfo, eAuthMode, prWpaInfo, ucBssIndex);
	if (ret != 0)
		return ret;

	/* Step 10: Handle WEP key if present */
	ret = configure_wep_key(sme, prGlueInfo, prWpaInfo, ucBssIndex);
	if (ret != 0)
		return ret;

	/* Step 11: CRITICAL - Apply channel and BSSID sovereignty lock
	 * This prevents the FSM from overriding userspace's explicit choices */
	apply_channel_and_bssid_lock(sme, prGlueInfo, prConnSettings, ucBssIndex);

	/* Step 12: Initiate the actual connection */
	ret = initiate_connection(sme, prGlueInfo, prMib, u4AkmSuite, ucBssIndex);
	if (ret != 0)
		return ret;

	return 0;
}

/* ============================================================================
 * HELPER FUNCTION IMPLEMENTATIONS
 * ============================================================================ */

static int validate_and_init_connection(struct wiphy *wiphy,
                                        struct net_device *ndev,
                                        struct cfg80211_connect_params *sme,
                                        struct GLUE_INFO **out_glue_info,
                                        uint8_t *out_bss_index)
{
	struct GLUE_INFO *prGlueInfo;
	uint8_t ucBssIndex;

	prGlueInfo = (struct GLUE_INFO *)wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex)) {
		DBGLOG(REQ, ERROR, "Invalid BSS index %d for STA connection\n", ucBssIndex);
		return -EINVAL;
	}

	DBGLOG(REQ, INFO, "[wlan] mtk_cfg80211_connect IE=%p len=%zu auth=%d\n",
	       sme->ie, sme->ie_len, sme->auth_type);

	*out_glue_info = prGlueInfo;
	*out_bss_index = ucBssIndex;
	return 0;
}

static int configure_operation_mode(struct GLUE_INFO *prGlueInfo,
                                   struct CONNECTION_SETTINGS *prConnSettings,
                                   uint8_t ucBssIndex)
{
	struct PARAM_OP_MODE rOpMode;
	uint32_t rStatus;
	uint32_t u4BufLen;

	/* Sanitize operation mode */
	if (prConnSettings->eOPMode > NET_TYPE_AUTO_SWITCH)
		rOpMode.eOpMode = NET_TYPE_AUTO_SWITCH;
	else
		rOpMode.eOpMode = prConnSettings->eOPMode;

	rOpMode.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetInfrastructureMode,
	                  (void *)&rOpMode, sizeof(struct PARAM_OP_MODE),
	                  FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "Failed to set infrastructure mode: 0x%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

static void reset_security_info(struct GLUE_INFO *prGlueInfo, uint8_t ucBssIndex)
{
	struct GL_WPA_INFO *prWpaInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter, ucBssIndex);
	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);

#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo;
	prDetRplyInfo = aisGetDetRplyInfo(prGlueInfo->prAdapter, ucBssIndex);
	kalMemZero(prDetRplyInfo, sizeof(struct GL_DETECT_REPLAY_INFO));
#endif

	/* Reset WPA info to defaults */
	prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prWpaInfo->u4KeyMgmt = 0;
	prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
	prWpaInfo->fgPrivacyInvoke = FALSE;

#if CFG_SUPPORT_802_11W
	prWpaInfo->u4CipherGroupMgmt = IW_AUTH_CIPHER_NONE;
	prWpaInfo->ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
	prWpaInfo->u4Mfp = IW_AUTH_MFP_DISABLED;
#endif

	/* Reset WPS and Passpoint flags */
	prConnSettings->fgWpsActive = FALSE;
	prConnSettings->non_wfa_vendor_ie_len = 0;

#if CFG_SUPPORT_PASSPOINT
	struct HS20_INFO *prHS20Info;
	prHS20Info = aisGetHS20Info(prGlueInfo->prAdapter, ucBssIndex);
	prHS20Info->fgConnectHS20AP = FALSE;
#endif
}

static int configure_auth_and_wpa_version(struct cfg80211_connect_params *sme,
                                         struct GL_WPA_INFO *prWpaInfo)
{
	/* Determine WPA version */
	if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_3)
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_WPA3;
	else if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_2)
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_WPA2;
	else if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_1)
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_WPA;
	else
		prWpaInfo->u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

	/* Determine authentication algorithm */
	switch (sme->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_SHARED_KEY;
		break;
	case NL80211_AUTHTYPE_FT:
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_FT;
		break;
	default:
		/* Legacy WEP: try both open and shared key */
		if (sme->key_len != 0)
			prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM | IW_AUTH_ALG_SHARED_KEY;
		else
			prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		break;
	}

	DBGLOG(REQ, INFO, "WPA version=%d auth_type=%d auth_alg=%d\n",
	       prWpaInfo->u4WpaVersion, sme->auth_type, prWpaInfo->u4AuthAlg);

	return 0;
}

static int configure_cipher_suites(struct cfg80211_connect_params *sme,
                                  struct GLUE_INFO *prGlueInfo,
                                  struct GL_WPA_INFO *prWpaInfo,
                                  struct CONNECTION_SETTINGS *prConnSettings,
                                  uint8_t ucBssIndex)
{
	/* Configure pairwise cipher */
	if (sme->crypto.n_ciphers_pairwise) {
		uint32_t cipher = sme->crypto.ciphers_pairwise[0];
		
		DBGLOG(RSN, INFO, "Pairwise cipher: 0x%x\n", cipher);
		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[0] = cipher;

		switch (cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			DBGLOG(REQ, INFO, "No group addressing\n");
			break;
		default:
			DBGLOG(REQ, ERROR, "Unsupported pairwise cipher: 0x%x\n", cipher);
			return -EINVAL;
		}
	}

	/* Configure group cipher */
	if (sme->crypto.cipher_group) {
		uint32_t cipher = sme->crypto.cipher_group;
		
		prConnSettings->rRsnInfo.u4GroupKeyCipherSuite = cipher;

		switch (cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			break;
		default:
			DBGLOG(REQ, ERROR, "Unsupported group cipher: 0x%x\n", cipher);
			return -EINVAL;
		}
	}

	return 0;
}

static int configure_akm_suite(struct cfg80211_connect_params *sme,
                              struct GLUE_INFO *prGlueInfo,
                              struct GL_WPA_INFO *prWpaInfo,
                              struct CONNECTION_SETTINGS *prConnSettings,
                              enum ENUM_PARAM_AUTH_MODE *out_auth_mode,
                              uint32_t *out_akm_suite,
                              uint8_t ucBssIndex)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode = AUTH_MODE_OPEN;
	uint32_t u4AkmSuite = 0;

	if (!sme->crypto.n_akm_suites) {
		/* No AKM suite - determine auth mode from WPA version */
		if (prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
			if (prWpaInfo->u4AuthAlg == IW_AUTH_ALG_FT) {
				DBGLOG(REQ, INFO, "Non-RSN FT connection\n");
				eAuthMode = AUTH_MODE_OPEN;
			} else if (prWpaInfo->u4AuthAlg == IW_AUTH_ALG_OPEN_SYSTEM) {
				eAuthMode = AUTH_MODE_OPEN;
			} else {
				eAuthMode = AUTH_MODE_AUTO_SWITCH;
			}
		}
		goto done;
	}

	/* We have AKM suite(s) - process the first one */
	uint32_t akm = sme->crypto.akm_suites[0];
	
	DBGLOG(REQ, INFO, "AKM suite: 0x%x (WPA ver=%d)\n", akm, prWpaInfo->u4WpaVersion);
	prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0] = akm;

	/* Handle based on WPA version */
	if (prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_WPA) {
		/* WPA1 */
		switch (akm) {
		case WLAN_AKM_SUITE_8021X:
			eAuthMode = AUTH_MODE_WPA;
			u4AkmSuite = WPA_AKM_SUITE_802_1X;
			break;
		case WLAN_AKM_SUITE_PSK:
			eAuthMode = AUTH_MODE_WPA_PSK;
			u4AkmSuite = WPA_AKM_SUITE_PSK;
			break;
		default:
			DBGLOG(REQ, ERROR, "Invalid WPA1 AKM suite: 0x%x\n", akm);
			return -EINVAL;
		}
	} else if (prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_WPA2) {
		/* WPA2/RSN */
		switch (akm) {
		case WLAN_AKM_SUITE_8021X:
			eAuthMode = AUTH_MODE_WPA2;
			u4AkmSuite = RSN_AKM_SUITE_802_1X;
			break;
		case WLAN_AKM_SUITE_PSK:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			u4AkmSuite = RSN_AKM_SUITE_PSK;
			break;
#if CFG_SUPPORT_802_11R
		case WLAN_AKM_SUITE_FT_8021X:
			eAuthMode = AUTH_MODE_WPA2_FT;
			u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
			break;
		case WLAN_AKM_SUITE_FT_PSK:
			eAuthMode = AUTH_MODE_WPA2_FT_PSK;
			u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
			break;
#endif
#if CFG_SUPPORT_802_11W
		case WLAN_AKM_SUITE_8021X_SHA256:
			eAuthMode = AUTH_MODE_WPA2;
			u4AkmSuite = RSN_AKM_SUITE_802_1X_SHA256;
			break;
		case WLAN_AKM_SUITE_PSK_SHA256:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			u4AkmSuite = RSN_AKM_SUITE_PSK_SHA256;
			break;
#endif
#if CFG_SUPPORT_PASSPOINT
		case WLAN_AKM_SUITE_OSEN:
			eAuthMode = AUTH_MODE_WPA_OSEN;
			u4AkmSuite = WFA_AKM_SUITE_OSEN;
			break;
#endif
		case WLAN_AKM_SUITE_8021X_SUITE_B:
		case WLAN_AKM_SUITE_8021X_SUITE_B_192:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
			break;
#if CFG_SUPPORT_OWE
		case WLAN_AKM_SUITE_OWE:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			u4AkmSuite = RSN_AKM_SUITE_OWE;
			break;
#endif
#if CFG_SUPPORT_DPP
		case WLAN_AKM_SUITE_DPP:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			u4AkmSuite = RSN_AKM_SUITE_DPP;
			break;
#endif
		default:
			DBGLOG(REQ, ERROR, "Invalid WPA2 AKM suite: 0x%x\n", akm);
			return -EINVAL;
		}
	} else if (prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_WPA3) {
		/* WPA3 */
		switch (akm) {
		case WLAN_AKM_SUITE_SAE:
			if (sme->auth_type == NL80211_AUTHTYPE_SAE)
				eAuthMode = AUTH_MODE_WPA3_SAE;
			else
				eAuthMode = AUTH_MODE_OPEN;
			u4AkmSuite = RSN_AKM_SUITE_SAE;
			break;
		default:
			DBGLOG(REQ, ERROR, "Invalid WPA3 AKM suite: 0x%x\n", akm);
			return -EINVAL;
		}
	}

done:
	*out_auth_mode = eAuthMode;
	*out_akm_suite = u4AkmSuite;
	return 0;
}

static int process_information_elements(struct cfg80211_connect_params *sme,
                                       struct GLUE_INFO *prGlueInfo,
                                       struct GL_WPA_INFO *prWpaInfo,
                                       struct CONNECTION_SETTINGS *prConnSettings,
                                       u_int8_t *out_carry_wps_ie,
                                       uint8_t ucBssIndex)
{
	u_int8_t fgCarryWPSIE = FALSE;

	if (!sme->ie || sme->ie_len == 0)
		goto cleanup;

	uint8_t *pucIEStart = (uint8_t *)sme->ie;
	uint8_t *prDesiredIE = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;

#if CFG_SUPPORT_WAPI
	/* Process WAPI IE */
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetWapiAssocInfo,
	                           pucIEStart, sme->ie_len, FALSE, FALSE, FALSE,
	                           &u4BufLen, ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, TRACE, "WAPI not supported (error 0x%x)\n", rStatus);
#endif

#if CFG_SUPPORT_WPS2
	/* Process WPS IE */
	if (wextSrchDesiredWPSIE(pucIEStart, sme->ie_len, 0xDD, &prDesiredIE)) {
		prConnSettings->fgWpsActive = TRUE;
		fgCarryWPSIE = TRUE;
		
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetWSCAssocInfo,
		                          prDesiredIE, IE_SIZE(prDesiredIE),
		                          FALSE, FALSE, FALSE, &u4BufLen, ucBssIndex);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(SEC, WARN, "Failed to set WSC assoc info: 0x%x\n", rStatus);
	}
#endif

#if CFG_SUPPORT_PASSPOINT
	/* Process Hotspot 2.0 IE */
	if (wextSrchDesiredHS20IE(pucIEStart, sme->ie_len, &prDesiredIE)) {
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetHS20Info,
		                          prDesiredIE, IE_SIZE(prDesiredIE),
		                          FALSE, FALSE, FALSE, &u4BufLen, ucBssIndex);
	} else if (wextSrchDesiredOsenIE(pucIEStart, sme->ie_len, &prDesiredIE)) {
		/* OSEN IE (reuse HS20 storage) */
		kalMemCopy(prGlueInfo->aucHS20AssocInfoIE, prDesiredIE, IE_SIZE(prDesiredIE));
		prGlueInfo->u2HS20AssocInfoIELen = (uint16_t)IE_SIZE(prDesiredIE);
	}

	/* Process Interworking IE */
	if (wextSrchDesiredInterworkingIE(pucIEStart, sme->ie_len, &prDesiredIE)) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetInterworkingInfo,
		                  prDesiredIE, IE_SIZE(prDesiredIE),
		                  FALSE, FALSE, FALSE, &u4BufLen);
	}

	/* Process Roaming Consortium IE */
	if (wextSrchDesiredRoamingConsortiumIE(pucIEStart, sme->ie_len, &prDesiredIE)) {
		rStatus = kalIoctl(prGlueInfo, wlanoidSetRoamingConsortiumIEInfo,
		                  prDesiredIE, IE_SIZE(prDesiredIE),
		                  FALSE, FALSE, FALSE, &u4BufLen);
	}
#endif

	/* Process RSN IE (WPA2/WPA3) */
	if (wextSrchDesiredWPAIE(pucIEStart, sme->ie_len, 0x30, &prDesiredIE)) {
		struct RSN_INFO rRsnInfo;
		
		kalMemZero(&rRsnInfo, sizeof(struct RSN_INFO));
		if (rsnParseRsnIE(prGlueInfo->prAdapter, 
		                 (struct RSN_INFO_ELEM *)prDesiredIE, &rRsnInfo)) {
#if CFG_SUPPORT_802_11W
			/* Extract MFP capability */
			if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
				prWpaInfo->u4CipherGroupMgmt = rRsnInfo.u4GroupMgmtKeyCipherSuite;
				prWpaInfo->ucRSNMfpCap = RSN_AUTH_MFP_OPTIONAL;
				
				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPR)
					prWpaInfo->ucRSNMfpCap = RSN_AUTH_MFP_REQUIRED;
			} else {
				prWpaInfo->ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
			}

			/* Extract PMKID list */
			prConnSettings->rRsnInfo.u2PmkidCnt = rRsnInfo.u2PmkidCnt;
			if (rRsnInfo.u2PmkidCnt > 0) {
				kalMemCopy(prConnSettings->rRsnInfo.aucPmkidList,
				          rRsnInfo.aucPmkidList,
				          rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN);
			}
#endif
		}
	}

	/* Extract non-WFA vendor IEs */
	if (cfg80211_get_non_wfa_vendor_ie(prGlueInfo, pucIEStart, 
	                                   sme->ie_len, ucBssIndex) > 0) {
		DBGLOG(RSN, INFO, "Found non-WFA vendor IE (len=%u)\n",
		       prConnSettings->non_wfa_vendor_ie_len);
	}

cleanup:
	/* Clear WSC IE buffer if not carrying WPS */
	if (!fgCarryWPSIE) {
		kalMemZero(&prConnSettings->aucWSCAssocInfoIE, 200);
		prConnSettings->u2WSCAssocInfoIELen = 0;
	}

	*out_carry_wps_ie = fgCarryWPSIE;
	return 0;
}

static int configure_mfp_settings(struct cfg80211_connect_params *sme,
                                 struct GL_WPA_INFO *prWpaInfo)
{
#if CFG_SUPPORT_802_11W
	switch (sme->mfp) {
	case NL80211_MFP_NO:
		prWpaInfo->u4Mfp = IW_AUTH_MFP_DISABLED;
		
		/* Upgrade to OPTIONAL if RSNE indicated MFPC=1 */
		if (prWpaInfo->ucRSNMfpCap == RSN_AUTH_MFP_OPTIONAL) {
			prWpaInfo->u4Mfp = IW_AUTH_MFP_OPTIONAL;
		} else if (prWpaInfo->ucRSNMfpCap == RSN_AUTH_MFP_REQUIRED) {
			DBGLOG(REQ, WARN, "MFP conflict: nl80211=DISABLED but RSNE=REQUIRED\n");
		}
		break;
		
	case NL80211_MFP_REQUIRED:
		prWpaInfo->u4Mfp = IW_AUTH_MFP_REQUIRED;
		break;
		
	default:
		prWpaInfo->u4Mfp = IW_AUTH_MFP_DISABLED;
		break;
	}

	DBGLOG(REQ, INFO, "MFP: nl80211=%d RSNE_cap=%d final=%d\n",
	       sme->mfp, prWpaInfo->ucRSNMfpCap, prWpaInfo->u4Mfp);
#endif
	return 0;
}

static int set_auth_and_encryption(struct GLUE_INFO *prGlueInfo,
                                  enum ENUM_PARAM_AUTH_MODE eAuthMode,
                                  struct GL_WPA_INFO *prWpaInfo,
                                  uint8_t ucBssIndex)
{
	uint32_t rStatus;
	uint32_t u4BufLen;
	enum ENUM_WEP_STATUS eEncStatus;
	uint32_t cipher;

	/* Set authentication mode */
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetAuthMode,
	                          &eAuthMode, sizeof(eAuthMode),
	                          FALSE, FALSE, FALSE, &u4BufLen, ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Failed to set auth mode: 0x%x\n", rStatus);
		return -EFAULT;
	}

	/* Determine encryption status from cipher suites */
	cipher = prWpaInfo->u4CipherGroup | prWpaInfo->u4CipherPairwise;

	if (cipher & IW_AUTH_CIPHER_GCMP256) {
		eEncStatus = ENUM_ENCRYPTION4_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_CCMP) {
		eEncStatus = ENUM_ENCRYPTION3_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_TKIP) {
		eEncStatus = ENUM_ENCRYPTION2_ENABLED;
	} else if (cipher & (IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40)) {
		eEncStatus = ENUM_ENCRYPTION1_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_NONE) {
		eEncStatus = prWpaInfo->fgPrivacyInvoke ? 
		            ENUM_ENCRYPTION1_ENABLED : ENUM_ENCRYPTION_DISABLED;
	} else {
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
	}

	/* Set encryption status */
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetEncryptionStatus,
	                          &eEncStatus, sizeof(eEncStatus),
	                          FALSE, FALSE, FALSE, &u4BufLen, ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Failed to set encryption: 0x%x\n", rStatus);
		return -EFAULT;
	}

	DBGLOG(REQ, INFO, "Auth mode=%d Encryption=%d\n", eAuthMode, eEncStatus);
	return 0;
}

static int configure_wep_key(struct cfg80211_connect_params *sme,
                           struct GLUE_INFO *prGlueInfo,
                           struct GL_WPA_INFO *prWpaInfo,
                           uint8_t ucBssIndex)
{
	uint8_t wepBuf[48];
	struct PARAM_WEP *prWepKey;
	uint32_t rStatus;
	uint32_t u4BufLen;

	/* WEP keys are only set for non-WPA connections */
	if (sme->key_len == 0 || prWpaInfo->u4WpaVersion != IW_AUTH_WPA_VERSION_DISABLED)
		return 0;

	prWepKey = (struct PARAM_WEP *)wepBuf;
	kalMemZero(prWepKey, sizeof(wepBuf));

	prWepKey->u4Length = OFFSET_OF(struct PARAM_WEP, aucKeyMaterial) + sme->key_len;
	prWepKey->u4KeyLength = (uint32_t)sme->key_len;
	prWepKey->u4KeyIndex = (uint32_t)sme->key_idx;
	prWepKey->u4KeyIndex |= IS_TRANSMIT_KEY;

	if (prWepKey->u4KeyLength > MAX_KEY_LEN) {
		DBGLOG(REQ, ERROR, "WEP key too long: %u bytes\n", prWepKey->u4KeyLength);
		return -EINVAL;
	}

	kalMemCopy(prWepKey->aucKeyMaterial, sme->key, prWepKey->u4KeyLength);

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetAddWep,
	                          prWepKey, prWepKey->u4Length,
	                          FALSE, FALSE, TRUE, &u4BufLen, ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "Failed to set WEP key: 0x%x\n", rStatus);
		return -EFAULT;
	}

	DBGLOG(REQ, INFO, "WEP key configured (index=%u len=%u)\n",
	       sme->key_idx, sme->key_len);
	return 0;
}

/* ============================================================================
 * CHANNEL AND BSSID SOVEREIGNTY LOCK
 * 
 * This function enforces userspace's explicit connection parameters to prevent
 * the firmware's auto-selection logic from overriding them.
 * 
 * Problem: iwd (and other supplicants) tell the kernel EXACTLY which AP to 
 * connect to (specific BSSID + channel). However, the MT7902 firmware has its
 * own scan/selection logic that often ignores these parameters and connects
 * to a different AP on a different band (usually falling back to 2.4 GHz).
 * 
 * Solution: We explicitly lock the connection parameters:
 * 1. Set a flag telling the FSM this is a cfg80211-managed connection
 * 2. Force the exact channel number from the kernel
 * 3. Lock the band (2.4G vs 5G) to prevent band-hopping
 * 4. Force the exact BSSID if specified
 * 
 * This ensures that when iwd says "connect to BSSID XX:XX:XX:XX:XX:XX on 
 * channel 36 (5GHz)", the firmware actually does that instead of connecting
 * to a different AP on channel 1 (2.4GHz).
 * ============================================================================ */
static void apply_channel_and_bssid_lock(struct cfg80211_connect_params *sme,
                                         struct GLUE_INFO *prGlueInfo,
                                         struct CONNECTION_SETTINGS *prConnSettings,
                                         uint8_t ucBssIndex)
{
    struct AIS_FSM_INFO *prAisFsmInfo;
    struct GL_WPA_INFO *prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter, ucBssIndex);

    prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter, ucBssIndex);

    /* Flag this as a kernel-managed connection */
    prAisFsmInfo->fgIsCfg80211Connecting = TRUE;

    /* Lock to specific channel if provided */
    if (sme->channel) {
        prConnSettings->ucChannelNum = sme->channel->hw_value;
        
        /* Determine and lock the band - Fixed naming for 2.4GHz */
        if (sme->channel->band == NL80211_BAND_5GHZ) {
            prConnSettings->eBand = BAND_5G;
        } else if (sme->channel->band == NL80211_BAND_2GHZ) {
            prConnSettings->eBand = BAND_2G4;
        }
        
        DBGLOG(REQ, STATE, "[Sovereignty] Locked to %s channel %d\n",
               (prConnSettings->eBand == BAND_5G) ? "5GHz" : "2.4GHz",
               prConnSettings->ucChannelNum);
    }

    /* Lock to specific BSSID if provided - Fixed struct member name */
    if (sme->bssid) {
        COPY_MAC_ADDR(prConnSettings->aucBSSID, sme->bssid);
        prConnSettings->fgIsBssidSpecified = TRUE; 
        
        DBGLOG(REQ, STATE, "[Sovereignty] Locked to BSSID " MACSTR "\n",
               MAC2STR(sme->bssid));
    }

    /* Privacy flag moved to WPA Info struct where it belongs */
    if (prWpaInfo)
        prWpaInfo->fgPrivacyInvoke = sme->privacy;
}


static int initiate_connection(struct cfg80211_connect_params *sme,
                              struct GLUE_INFO *prGlueInfo,
                              struct IEEE_802_11_MIB *prMib,
                              uint32_t u4AkmSuite,
                              uint8_t ucBssIndex)
{
	struct PARAM_CONNECT rNewSsid;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint32_t i;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY *prEntry;

	/* Enable only the specific AKM suite in the MIB 
	 * Exception: If u4AkmSuite is 0 (legacy Open/WEP networks), 
	 * leave all suites in their default state to avoid disabling everything */
	if (u4AkmSuite != 0) {
		for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
			prEntry = &prMib->dot11RSNAConfigAuthenticationSuitesTable[i];
			prEntry->dot11RSNAConfigAuthenticationSuiteEnabled = 
				(prEntry->dot11RSNAConfigAuthenticationSuite == u4AkmSuite);
		}
		DBGLOG(REQ, INFO, "Enabled AKM suite 0x%08x in MIB\n", u4AkmSuite);
	} else {
		DBGLOG(REQ, INFO, "No AKM suite (legacy Open/WEP) - leaving MIB defaults\n");
	}

	/* Build connection parameters */
	kalMemZero(&rNewSsid, sizeof(rNewSsid));
	rNewSsid.u4CenterFreq = sme->channel ? sme->channel->center_freq : 0;
	rNewSsid.pucBssid = (uint8_t *)sme->bssid;
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
	rNewSsid.pucBssidHint = (uint8_t *)sme->bssid_hint;
#endif
	rNewSsid.pucSsid = (uint8_t *)sme->ssid;
	rNewSsid.u4SsidLen = sme->ssid_len;
	rNewSsid.ucBssIdx = ucBssIndex;

	/* Initiate the connection */
	rStatus = kalIoctl(prGlueInfo, wlanoidSetConnect,
	                  (void *)&rNewSsid, sizeof(struct PARAM_CONNECT),
	                  FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Connection failed: 0x%x\n", rStatus);
		return -EINVAL;
	}

	DBGLOG(REQ, STATE, "Connection initiated: SSID='%s' BSSID=" MACSTR " freq=%d\n",
	       sme->ssid, MAC2STR(sme->bssid), 
	       sme->channel ? sme->channel->center_freq : 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to disconnect from
 *        currently connected ESS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_disconnect(struct wiphy *wiphy,
			    struct net_device *ndev, u16 reason_code)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;
	
	/* Clear cfg80211 ownership flag */
	prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter, ucBssIndex);
	if (prAisFsmInfo) {
		prAisFsmInfo->fgIsCfg80211Connecting = FALSE;
		DBGLOG(REQ, INFO, "[CFG80211] Cleared cfg80211-connecting on disconnect\n");
	}

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetDisassociate, NULL,
			   0, FALSE, FALSE, TRUE, &u4BufLen,
			   ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to deauth from
 *        currently connected ESS
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_deauth(struct wiphy *wiphy,
	struct net_device *ndev, struct cfg80211_deauth_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	struct BSS_INFO *prBssInfo = NULL;
#endif
	uint8_t ucBssIndex = 0;
	uint32_t rStatus;
	uint32_t u4BufLen;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "mtk_cfg80211_deauth\n");
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	/* The BSS from cfg80211_ops.assoc must
	* give back to cfg80211_send_rx_assoc()
	* or to cfg80211_assoc_timeout().
	* To ensure proper refcounting,
	* new association requests while already associating
	* must be rejected.
	*/
	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);
	if (prConnSettings->bss) {
		DBGLOG(REQ, INFO, "assoc timeout notify\n");
		/* ops caller have already hold the mutex. */
#if (KERNEL_VERSION(3, 11, 0) <= CFG80211_VERSION_CODE)
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
		struct cfg80211_assoc_failure data = {
			.bss[0] = prConnSettings->bss,
			.timeout = true,
		};
		cfg80211_assoc_failure(ndev, &data);
#else
		cfg80211_assoc_timeout(ndev,
			prConnSettings->bss);
#endif
#else
		cfg80211_send_assoc_timeout(ndev,
			prGlueInfo->prAdapter->rWifiVar.
				rConnSettings.bss->bssid);
#endif
		DBGLOG(REQ, INFO, "assoc timeout notify, Done\n");
		prConnSettings->bss = NULL;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
		ucBssIndex);

	if (authSendDeauthFrame(prGlueInfo->prAdapter,
		prBssInfo,
		prBssInfo->prStaRecOfAP,
		NULL,
		REASON_CODE_DEAUTH_LEAVING_BSS,
		NULL) != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN, "authSendDeauthFrame Failed\n");
#endif

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate,
			NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to join an IBSS group
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_join_ibss(struct wiphy *wiphy,
			   struct net_device *ndev,
			   struct cfg80211_ibss_params *params)
{
	struct PARAM_SSID rNewSsid;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4ChnlFreq;	/* Store channel or frequency information */
	uint32_t u4BufLen = 0, u4SsidLen = 0;
	uint32_t rStatus;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	/* set channel */
	if (params->channel_fixed) {
		u4ChnlFreq = params->chandef.center_freq1;

		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetFrequency,
				&u4ChnlFreq, sizeof(u4ChnlFreq),
				FALSE, FALSE, FALSE, &u4BufLen,
				ucBssIndex);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	/* set SSID */
	if (params->ssid_len > PARAM_MAX_LEN_SSID)
		u4SsidLen = PARAM_MAX_LEN_SSID;
	else
		u4SsidLen = params->ssid_len;

	kalMemCopy(rNewSsid.aucSsid, params->ssid,
		   u4SsidLen);
	rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidSetSsid, (void *)&rNewSsid,
				sizeof(struct PARAM_SSID),
				FALSE, FALSE, TRUE, &u4BufLen,
				ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set SSID:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;

	return -EINVAL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to leave from IBSS group
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_leave_ibss(struct wiphy *wiphy,
			    struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetDisassociate, NULL,
			   0, FALSE, FALSE, TRUE, &u4BufLen,
			   ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "disassociate error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to configure
 *        WLAN power managemenet
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_power_mgmt(struct wiphy *wiphy,
			struct net_device *ndev, bool enabled, int timeout)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_POWER_MODE_ rPowerMode;
	uint8_t ucBssIndex = 0;
	struct BSS_INFO *prBssInfo;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (!prGlueInfo)
		return -EFAULT;

#if WLAN_INCLUDE_SYS
	if (prGlueInfo->prAdapter->fgEnDbgPowerMode) {
		DBGLOG(REQ, WARN,
			"Force power mode enabled, ignore: %d\n", enabled);
		return 0;
	}
#endif

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
		ucBssIndex);
	if (!prBssInfo)
		return -EINVAL;

	DBGLOG(REQ, INFO, "%d: enabled=%d, timeout=%d, fgTIMPresend=%d\n",
	       ucBssIndex, enabled, timeout,
	       prBssInfo->fgTIMPresent);

	if (enabled) {
		if (prBssInfo->eConnectionState
			== MEDIA_STATE_CONNECTED &&
		    !prBssInfo->fgTIMPresent)
			return -EFAULT;

		if (timeout == -1)
			rPowerMode.ePowerMode = Param_PowerModeFast_PSP;
		else
			rPowerMode.ePowerMode = Param_PowerModeMAX_PSP;
	} else {
		rPowerMode.ePowerMode = Param_PowerModeCAM;
	}

	rPowerMode.ucBssIdx = ucBssIndex;

	rStatus = kalIoctl(prGlueInfo, wlanoidSet802dot11PowerSaveProfile,
			   &rPowerMode, sizeof(struct PARAM_POWER_MODE_),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set_power_mgmt error:%x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cache
 *        a PMKID for a BSSID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_pmksa(struct wiphy *wiphy,
		   struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_PMKID pmkid;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "mtk_cfg80211_set_pmksa " MACSTR " pmk\n",
		MAC2STR(pmksa->bssid));

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	COPY_MAC_ADDR(pmkid.arBSSID, pmksa->bssid);
	kalMemCopy(pmkid.arPMKID, pmksa->pmkid, IW_PMKID_LEN);
	pmkid.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, &pmkid,
			   sizeof(struct PARAM_PMKID),
			   FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to remove
 *        a cached PMKID for a BSSID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_del_pmksa(struct wiphy *wiphy,
			struct net_device *ndev, struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_PMKID pmkid;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, TRACE, "mtk_cfg80211_del_pmksa " MACSTR "\n",
		MAC2STR(pmksa->bssid));

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	COPY_MAC_ADDR(pmkid.arBSSID, pmksa->bssid);
	kalMemCopy(pmkid.arPMKID, pmksa->pmkid, IW_PMKID_LEN);
	pmkid.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidDelPmkid, &pmkid,
			   sizeof(struct PARAM_PMKID),
			   FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to flush
 *        all cached PMKID
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_flush_pmksa(struct wiphy *wiphy,
			     struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidFlushPmkid, NULL, 0,
			   FALSE, FALSE, FALSE, &u4BufLen,
			   ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "flush pmkid error:%x\n", rStatus);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for setting the rekey data
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_set_rekey_data(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_gtk_rekey_data *data)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen;
	struct PARAM_GTK_REKEY_DATA *prGtkData;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Rslt = -EINVAL;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	/* if EapolOffload 0 => we store key data here */
	/* and rekey when enter system suspend wow  */
	if (!prGlueInfo->prAdapter->rWifiVar.ucEapolOffload) {
		kalMemZero(prWpaInfo->aucKek, NL80211_KEK_LEN);
		kalMemZero(prWpaInfo->aucKck, NL80211_KCK_LEN);
		kalMemZero(prWpaInfo->aucReplayCtr, NL80211_REPLAY_CTR_LEN);
		kalMemCopy(prWpaInfo->aucKek, data->kek, NL80211_KEK_LEN);
		kalMemCopy(prWpaInfo->aucKck, data->kck, NL80211_KCK_LEN);
		kalMemCopy(prWpaInfo->aucReplayCtr,
			data->replay_ctr, NL80211_REPLAY_CTR_LEN);

		return 0;
	}

	prGtkData =
		(struct PARAM_GTK_REKEY_DATA *) kalMemAlloc(sizeof(
				struct PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);

	if (!prGtkData)
		return 0;

	DBGLOG(RSN, INFO, "ucBssIndex = %d, size(%d)\n",
		ucBssIndex,
		(uint32_t) sizeof(struct cfg80211_gtk_rekey_data));

	DBGLOG(RSN, TRACE, "kek\n");
	DBGLOG_MEM8(RSN, TRACE, (uint8_t *)data->kek,
		    NL80211_KEK_LEN);
	DBGLOG(RSN, TRACE, "kck\n");
	DBGLOG_MEM8(RSN, TRACE, (uint8_t *)data->kck,
		    NL80211_KCK_LEN);
	DBGLOG(RSN, TRACE, "replay count\n");
	DBGLOG_MEM8(RSN, TRACE, (uint8_t *)data->replay_ctr,
		    NL80211_REPLAY_CTR_LEN);


#if 0
	kalMemCopy(prGtkData, data, sizeof(*data));
#else
	kalMemCopy(prGtkData->aucKek, data->kek, NL80211_KEK_LEN);
	kalMemCopy(prGtkData->aucKck, data->kck, NL80211_KCK_LEN);
	kalMemCopy(prGtkData->aucReplayCtr, data->replay_ctr,
		   NL80211_REPLAY_CTR_LEN);
#endif

	prGtkData->ucBssIndex = ucBssIndex;

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	prGtkData->u4Proto = NL80211_WPA_VERSION_2;
	if (prWpaInfo->u4WpaVersion ==
		IW_AUTH_WPA_VERSION_WPA3)
		prGtkData->u4Proto = NL80211_WPA_VERSION_3;
	else if (prWpaInfo->u4WpaVersion ==
	    IW_AUTH_WPA_VERSION_WPA)
		prGtkData->u4Proto = NL80211_WPA_VERSION_1;

	if (prWpaInfo->u4CipherPairwise ==
	    IW_AUTH_CIPHER_TKIP)
		prGtkData->u4PairwiseCipher = BIT(3);
	else if (prWpaInfo->u4CipherPairwise ==
		 IW_AUTH_CIPHER_CCMP)
		prGtkData->u4PairwiseCipher = BIT(4);
	else {
		kalMemFree(prGtkData, VIR_MEM_TYPE,
			   sizeof(struct PARAM_GTK_REKEY_DATA));
		return 0;
	}

	if (prWpaInfo->u4CipherGroup ==
	    IW_AUTH_CIPHER_TKIP)
		prGtkData->u4GroupCipher    = BIT(3);
	else if (prWpaInfo->u4CipherGroup ==
		 IW_AUTH_CIPHER_CCMP)
		prGtkData->u4GroupCipher    = BIT(4);
	else {
		kalMemFree(prGtkData, VIR_MEM_TYPE,
			   sizeof(struct PARAM_GTK_REKEY_DATA));
		return 0;
	}

	prGtkData->u4KeyMgmt = prWpaInfo->u4KeyMgmt;
	prGtkData->u4MgmtGroupCipher = 0;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetGtkRekeyData, prGtkData,
				sizeof(struct PARAM_GTK_REKEY_DATA),
				FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "set GTK rekey data error:%x\n",
		       rStatus);
	else
		i4Rslt = 0;

	kalMemFree(prGtkData, VIR_MEM_TYPE,
		   sizeof(struct PARAM_GTK_REKEY_DATA));

	return i4Rslt;
}

int mtk_cfg80211_mgmt_frame_register(IN struct wiphy *wiphy,
				     IN struct wireless_dev *wdev,
				     IN u16 frame_type,
				     IN bool reg)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if (!prGlueInfo)
		return -EIO;

	DBGLOG(INIT, TRACE, "mgmt_frame_register: type %04x reg: %d\n", frame_type, reg);

	switch (frame_type) {
	case MAC_FRAME_PROBE_REQ:
		if (reg)
			prGlueInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
		else
			prGlueInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_PROBE_REQ;
		break;

	case MAC_FRAME_ACTION:
		if (reg)
			prGlueInfo->u4OsMgmtFrameFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
		else
			prGlueInfo->u4OsMgmtFrameFilter &= ~PARAM_PACKET_FILTER_ACTION_FRAME;
		break;

	default:
		/* CRITICAL: Tell iwd/kernel we don't support this specific frame type
		 * but do NOT return -EINVAL (-22). -EOPNOTSUPP is the "polite" way to decline.
		 */
		DBGLOG(INIT, INFO, "Frame type %x not supported for registration\n", frame_type);
		return -EOPNOTSUPP;
	}

	if (prGlueInfo->prAdapter != NULL) {
		set_bit(GLUE_FLAG_FRAME_FILTER_AIS_BIT, &prGlueInfo->ulFlag);
		wake_up_interruptible(&prGlueInfo->waitq);
	}

	return 0; /* Success */
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to stay on a
 *        specified channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_remain_on_channel(struct wiphy *wiphy,
		   struct wireless_dev *wdev,
		   struct ieee80211_channel *chan, unsigned int duration,
		   u64 *cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Rslt = -EINVAL;
	struct MSG_REMAIN_ON_CHANNEL *prMsgChnlReq =
		(struct MSG_REMAIN_ON_CHANNEL *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		    || (chan == NULL)
		    || (cookie == NULL)) {
			break;
		}

		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;

#if 1
		DBGLOG(INIT, INFO, "ucBssIndex = %d\n", ucBssIndex);
#endif

		*cookie = prGlueInfo->u8Cookie++;

		prMsgChnlReq = cnmMemAlloc(prGlueInfo->prAdapter,
			   RAM_TYPE_MSG, sizeof(struct MSG_REMAIN_ON_CHANNEL));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgChnlReq->rMsgHdr.eMsgId =
			MID_MNY_AIS_REMAIN_ON_CHANNEL;
		prMsgChnlReq->u8Cookie = *cookie;
		prMsgChnlReq->u4DurationMs = duration;
		prMsgChnlReq->eReqType = CH_REQ_TYPE_ROC;
		prMsgChnlReq->ucChannelNum = nicFreq2ChannelNum(
				chan->center_freq * 1000);

		switch (chan->band) {
		case KAL_BAND_2GHZ:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		case KAL_BAND_5GHZ:
			prMsgChnlReq->eBand = BAND_5G;
			break;
#if (CFG_SUPPORT_WIFI_6G == 1)
		case KAL_BAND_6GHZ:
			prMsgChnlReq->eBand = BAND_6G;
			break;
#endif

		default:
			prMsgChnlReq->eBand = BAND_2G4;
			break;
		}

		prMsgChnlReq->eSco = CHNL_EXT_SCN;

		prMsgChnlReq->ucBssIdx = ucBssIndex;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgChnlReq, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cancel staying
 *        on a specified channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_cancel_remain_on_channel(
	struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Rslt = -EINVAL;
	struct MSG_CANCEL_REMAIN_ON_CHANNEL *prMsgChnlAbort =
		(struct MSG_CANCEL_REMAIN_ON_CHANNEL *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		if ((wiphy == NULL)
		    || (wdev == NULL)
		   ) {
			break;
		}

		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;

		DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

		prMsgChnlAbort =
			cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
			    sizeof(struct MSG_CANCEL_REMAIN_ON_CHANNEL));

		if (prMsgChnlAbort == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgChnlAbort->rMsgHdr.eMsgId =
			MID_MNY_AIS_CANCEL_REMAIN_ON_CHANNEL;
		prMsgChnlAbort->u8Cookie = cookie;

		prMsgChnlAbort->ucBssIdx = ucBssIndex;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgChnlAbort, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

#if CFG_SUPPORT_TX_MGMT_USE_DATAQ
int _mtk_cfg80211_mgmt_tx_via_data_path(
		IN struct GLUE_INFO *prGlueInfo,
		IN struct wireless_dev *wdev,
		IN const u8 *buf,
		IN size_t len, IN u64 u8GlCookie)
{
	int32_t i4Rslt = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct sk_buff *prSkb = NULL;
	uint8_t *pucRecvBuff = NULL;
	uint8_t ucBssIndex = 0;

	DBGLOG(P2P, INFO, "len[%d], cookie: 0x%llx.\n", len, u8GlCookie);
	prSkb = kalPacketAlloc(prGlueInfo, len, &pucRecvBuff);
	if (prSkb) {
		kalMemCopy(pucRecvBuff, buf, len);
		skb_put(prSkb, len);
		prSkb->dev = wdev->netdev;
		GLUE_SET_PKT_FLAG(prSkb, ENUM_PKT_802_11_MGMT);
		GLUE_SET_PKT_COOKIE(prSkb, u8GlCookie);
		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;
		rStatus = kalHardStartXmit(prSkb,
			wdev->netdev,
			prGlueInfo,
			ucBssIndex);
		if (rStatus != WLAN_STATUS_SUCCESS)
			i4Rslt = -EINVAL;
	} else
		i4Rslt = -ENOMEM;

	return i4Rslt;
}
#endif

int _mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
		struct wireless_dev *wdev, struct ieee80211_channel *chan,
		bool offchan, unsigned int wait, const u8 *buf, size_t len,
		bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;
	int32_t i4Rslt = -EINVAL;
	struct MSG_MGMT_TX_REQUEST *prMsgTxReq =
			(struct MSG_MGMT_TX_REQUEST *) NULL;
	struct MSDU_INFO *prMgmtFrame = (struct MSDU_INFO *) NULL;
	uint8_t *pucFrameBuf = (uint8_t *) NULL;
	uint64_t *pu8GlCookie = (uint64_t *) NULL;
#if CFG_SUPPORT_TX_MGMT_USE_DATAQ
	uint16_t u2MgmtTxMaxLen = 0;
#endif

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (cookie == NULL))
			break;

		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		*cookie = prGlueInfo->u8Cookie++;

#if CFG_SUPPORT_TX_MGMT_USE_DATAQ
		u2MgmtTxMaxLen = HIF_TX_CMD_MAX_SIZE
						- prGlueInfo->prAdapter
						->chip_info->u2CmdTxHdrSize;
#if defined(_HIF_USB)
		u2MgmtTxMaxLen -= LEN_USB_UDMA_TX_TERMINATOR;
#endif
		/* to fix WFDMA entry size limitation, >1600 MGMT frame
		*  send into data flow and bypass MCU
		*/
		if (len > u2MgmtTxMaxLen)
			return _mtk_cfg80211_mgmt_tx_via_data_path(
				prGlueInfo, wdev, buf,
				len, *cookie);
#endif

		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_MGMT_TX_REQUEST));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		if (offchan) {
			prMsgTxReq->fgIsOffChannel = TRUE;

			kalChannelFormatSwitch(NULL, chan,
					&prMsgTxReq->rChannelInfo);
			kalChannelScoSwitch(NL80211_CHAN_NO_HT,
					&prMsgTxReq->eChnlExt);
		} else {
			prMsgTxReq->fgIsOffChannel = FALSE;
		}

		if (wait)
			prMsgTxReq->u4Duration = wait;
		else
			prMsgTxReq->u4Duration = 0;

		if (no_cck)
			prMsgTxReq->fgNoneCckRate = TRUE;
		else
			prMsgTxReq->fgNoneCckRate = FALSE;

		if (dont_wait_for_ack)
			prMsgTxReq->fgIsWaitRsp = FALSE;
		else
			prMsgTxReq->fgIsWaitRsp = TRUE;

		prMgmtFrame = cnmMgtPktAlloc(prGlueInfo->prAdapter,
				(int32_t) (len + sizeof(uint64_t)
				+ MAC_TX_RESERVED_FIELD));
		prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
		if (prMsgTxReq->prMgmtMsduInfo == NULL) {
			/* ASSERT(FALSE); */
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgTxReq->u8Cookie = *cookie;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_AIS_MGMT_TX;
		prMsgTxReq->ucBssIdx = wlanGetBssIdx(wdev->netdev);

		pucFrameBuf =
			(uint8_t *)
			((unsigned long) prMgmtFrame->prPacket
			+ MAC_TX_RESERVED_FIELD);
		pu8GlCookie =
			(uint64_t *)
			((unsigned long) prMgmtFrame->prPacket
			+ (unsigned long) len
			+ MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, buf, len);

		*pu8GlCookie = *cookie;

		prMgmtFrame->u2FrameLength = len;
		prMgmtFrame->ucBssIndex = wlanGetBssIdx(wdev->netdev);

#define TEMP_LOG_TEMPLATE "bssIdx: %d, band: %d, chan: %d, offchan: %d, " \
		"wait: %d, len: %d, no_cck: %d, dont_wait_for_ack: %d, " \
		"cookie: 0x%llx\n"
		DBGLOG(P2P, INFO, TEMP_LOG_TEMPLATE,
				prMsgTxReq->ucBssIdx,
				prMsgTxReq->rChannelInfo.eBand,
				prMsgTxReq->rChannelInfo.ucChannelNum,
				prMsgTxReq->fgIsOffChannel,
				prMsgTxReq->u4Duration,
				prMsgTxReq->prMgmtMsduInfo->u2FrameLength,
				prMsgTxReq->fgNoneCckRate,
				prMsgTxReq->fgIsWaitRsp,
				prMsgTxReq->u8Cookie);
#undef TEMP_LOG_TEMPLATE

		mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgTxReq,
			MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	if ((i4Rslt != 0) && (prMsgTxReq != NULL)) {
		if (prMsgTxReq->prMgmtMsduInfo != NULL)
			cnmMgtPktFree(prGlueInfo->prAdapter,
				prMsgTxReq->prMgmtMsduInfo);

		cnmMemFree(prGlueInfo->prAdapter, prMsgTxReq);
	}

	return i4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to send a management frame
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			struct cfg80211_mgmt_tx_params *params,
			u64 *cookie)
{
	if (params == NULL)
		return -EINVAL;

	return _mtk_cfg80211_mgmt_tx(wiphy, wdev, params->chan,
			params->offchan, params->wait, params->buf, params->len,
			params->no_cck, params->dont_wait_for_ack, cookie);
}
#else
int mtk_cfg80211_mgmt_tx(struct wiphy *wiphy,
		struct wireless_dev *wdev, struct ieee80211_channel *channel,
		bool offchan, unsigned int wait, const u8 *buf, size_t len,
		bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	return _mtk_cfg80211_mgmt_tx(wiphy, wdev, channel, offchan, wait, buf,
			len, no_cck, dont_wait_for_ack, cookie);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for requesting to cancel the wait time
 *        from transmitting a management frame on another channel
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
		struct wireless_dev *wdev, u64 cookie)
{
	int32_t i4Rslt;
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;
	struct MSG_CANCEL_TX_WAIT_REQUEST *prMsgCancelTxWait =
			(struct MSG_CANCEL_TX_WAIT_REQUEST *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		ASSERT(wiphy);

		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
		ASSERT(prGlueInfo);

		ucBssIndex = wlanGetBssIdx(wdev->netdev);
		if (!IS_BSS_INDEX_VALID(ucBssIndex))
			return -EINVAL;

		DBGLOG(P2P, INFO, "cookie: 0x%llx, ucBssIndex = %d\n",
			cookie, ucBssIndex);


		prMsgCancelTxWait = cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_MSG,
				sizeof(struct MSG_CANCEL_TX_WAIT_REQUEST));

		if (prMsgCancelTxWait == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prMsgCancelTxWait->rMsgHdr.eMsgId =
				MID_MNY_AIS_MGMT_TX_CANCEL_WAIT;
		prMsgCancelTxWait->u8Cookie = cookie;
		prMsgCancelTxWait->ucBssIdx = ucBssIndex;

		mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgCancelTxWait,
			MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

#ifdef CONFIG_NL80211_TESTMODE

#if CFG_SUPPORT_PASSPOINT
int mtk_cfg80211_testmode_hs20_cmd(IN struct wiphy *wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct wpa_driver_hs20_data_s *prParams = NULL;
	uint32_t rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	uint32_t u4SetInfoLen = 0;
	uint8_t ucBssIndex = 0;

	ASSERT(wiphy);

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	if (data && len)
		prParams = (struct wpa_driver_hs20_data_s *)data;

	if (prParams) {
		int i;

		DBGLOG(INIT, INFO, "Cmd Type (%d)\n", prParams->CmdType);
		switch (prParams->CmdType) {
		case HS20_CMD_ID_SET_BSSID_POOL:
			DBGLOG(REQ, TRACE,
			"fgBssidPoolIsEnable=%d, ucNumBssidPool=%d\n",
			prParams->hs20_set_bssid_pool.fgBssidPoolIsEnable,
			prParams->hs20_set_bssid_pool.ucNumBssidPool);
			for (i = 0;
			     i < prParams->hs20_set_bssid_pool.ucNumBssidPool;
			     i++) {
				DBGLOG(REQ, TRACE,
					"[%d][ " MACSTR " ]\n",
					i,
					MAC2STR(prParams->
					hs20_set_bssid_pool.
					arBssidPool[i]));
			}
			rstatus = kalIoctlByBssIdx(prGlueInfo,
			   (PFN_OID_HANDLER_FUNC) wlanoidSetHS20BssidPool,
			   &prParams->hs20_set_bssid_pool,
			   sizeof(struct param_hs20_set_bssid_pool),
			   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen,
			   ucBssIndex);
			break;
		default:
			DBGLOG(REQ, TRACE,
				"Unknown Cmd Type (%d)\n",
				prParams->CmdType);
			rstatus = WLAN_STATUS_FAILURE;

		}

	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}
#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_WAPI
int mtk_cfg80211_testmode_set_key_ext(IN struct wiphy
				      *wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct NL80211_DRIVER_SET_KEY_EXTS *prParams =
		(struct NL80211_DRIVER_SET_KEY_EXTS *) NULL;
	struct iw_encode_exts *prIWEncExt = (struct iw_encode_exts
					     *)NULL;
	uint32_t rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	uint32_t u4BufLen = 0;
	const uint8_t aucBCAddr[] = BC_MAC_ADDR;
	uint8_t ucBssIndex = 0;

	struct PARAM_KEY *prWpiKey = (struct PARAM_KEY *)
				     keyStructBuf;

	memset(keyStructBuf, 0, sizeof(keyStructBuf));

	ASSERT(wiphy);

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if (data == NULL || len == 0) {
		DBGLOG(INIT, TRACE, "%s data or len is invalid\n", __func__);
		return -EINVAL;
	}

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		ucBssIndex = 0;

	DBGLOG(INIT, INFO, "ucBssIndex = %d\n", ucBssIndex);

	prParams = (struct NL80211_DRIVER_SET_KEY_EXTS *) data;
	prIWEncExt = (struct iw_encode_exts *)&prParams->ext;

	if (prIWEncExt->alg == IW_ENCODE_ALG_SMS4) {
		/* KeyID */
		prWpiKey->u4KeyIndex = prParams->key_index;
		prWpiKey->u4KeyIndex--;
		if (prWpiKey->u4KeyIndex > 1) {
			return -EINVAL;
		}

		if (prIWEncExt->key_len != 32) {
			return -EINVAL;
		}
		prWpiKey->u4KeyLength = prIWEncExt->key_len;

		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_SET_TX_KEY &&
		    !(prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY)) {
			/* WAI seems set the STA group key with
			 * IW_ENCODE_EXT_SET_TX_KEY !!!!
			 * Ignore the group case
			 */
			prWpiKey->u4KeyIndex |= BIT(30);
			prWpiKey->u4KeyIndex |= BIT(31);
			/* BSSID */
			memcpy(prWpiKey->arBSSID, prIWEncExt->addr, 6);
		} else {
			COPY_MAC_ADDR(prWpiKey->arBSSID, aucBCAddr);
		}

		/* PN */
		/* memcpy(prWpiKey->rKeyRSC, prIWEncExt->tx_seq,
		 * IW_ENCODE_SEQ_MAX_SIZE * 2);
		 */

		memcpy(prWpiKey->aucKeyMaterial, prIWEncExt->key, 32);

		prWpiKey->u4Length = sizeof(struct PARAM_KEY);
		prWpiKey->ucBssIdx = ucBssIndex;
		prWpiKey->ucCipher = CIPHER_SUITE_WPI;

		rstatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, prWpiKey,
				sizeof(struct PARAM_KEY),
				FALSE, FALSE, TRUE, &u4BufLen);

		if (rstatus != WLAN_STATUS_SUCCESS) {
			fgIsValid = -EFAULT;
		}

	}
	return fgIsValid;
}
#endif

int
mtk_cfg80211_testmode_get_sta_statistics(IN struct wiphy
		*wiphy, IN void *data, IN int len,
		IN struct GLUE_INFO *prGlueInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen;
	uint32_t u4LinkScore;
	uint32_t u4TotalError;
	uint32_t u4TxExceedThresholdCount;
	uint32_t u4TxTotalCount;

	struct NL80211_DRIVER_GET_STA_STATISTICS_PARAMS *prParams =
			NULL;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	if (len < sizeof(struct NL80211_DRIVER_GET_STA_STATISTICS_PARAMS)) {
		DBGLOG(OID, WARN, "len [%d] is invalid!\n", len);
		return -EINVAL;
	}
	if (data && len)
		prParams = (struct NL80211_DRIVER_GET_STA_STATISTICS_PARAMS
			    *) data;

	if (prParams == NULL) {
		DBGLOG(QM, ERROR, "prParams is NULL, data=%p, len=%d\n",
		       data, len);
		return -EINVAL;
	}

	skb = cfg80211_testmode_alloc_reply_skb(wiphy,
				sizeof(struct PARAM_GET_STA_STATISTICS) + 1);
	if (!skb) {
		DBGLOG(QM, ERROR, "allocate skb failed:%x\n", rStatus);
		return -ENOMEM;
	}

	kalMemZero(&rQueryStaStatistics,
		   sizeof(rQueryStaStatistics));
	COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr,
		      prParams->aucMacAddr);
	rQueryStaStatistics.ucReadClear = TRUE;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStaStatistics,
			   &rQueryStaStatistics, sizeof(rQueryStaStatistics),
			   TRUE, FALSE, TRUE, &u4BufLen);

	/* Calcute Link Score */
	u4TxExceedThresholdCount =
		rQueryStaStatistics.u4TxExceedThresholdCount;
	u4TxTotalCount = rQueryStaStatistics.u4TxTotalCount;
	u4TotalError = rQueryStaStatistics.u4TxFailCount +
		       rQueryStaStatistics.u4TxLifeTimeoutCount;

	/* u4LinkScore 10~100 , ExceedThreshold ratio 0~90 only
	 * u4LinkScore 0~9    , Drop packet ratio 0~9 and all packets exceed
	 * threshold
	 */
	if (u4TxTotalCount) {
		if (u4TxExceedThresholdCount <= u4TxTotalCount)
			u4LinkScore = (90 - ((u4TxExceedThresholdCount * 90)
							/ u4TxTotalCount));
		else
			u4LinkScore = 0;
	} else {
		u4LinkScore = 90;
	}

	u4LinkScore += 10;

	if (u4LinkScore == 10) {
		if (u4TotalError <= u4TxTotalCount)
			u4LinkScore = (10 - ((u4TotalError * 10)
							/ u4TxTotalCount));
		else
			u4LinkScore = 0;

	}

	if (u4LinkScore > 100)
		u4LinkScore = 100;
	{
		u8 __tmp = 0;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_INVALID, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u8 __tmp = NL80211_DRIVER_TESTMODE_VERSION;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_VERSION, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_MAC, MAC_ADDR_LEN,
	    prParams->aucMacAddr) < 0))
		goto nla_put_failure;
	{
		u32 __tmp = u4LinkScore;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_LINK_SCORE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.u4Flag;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_FLAG, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4EnqueueCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_ENQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4DequeueCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_DEQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4EnqueueStaCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_STA_ENQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4DequeueStaCounter;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_STA_DEQUEUE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.IsrCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrPassCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_ISR_PASS_CNT,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.TaskIsrCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_TASK_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrAbnormalCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_AB_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrSoftWareCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_SW_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrTxCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_TX_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	{
		u32 __tmp = rQueryStaStatistics.IsrRxCnt;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_IRQ_RX_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	/* FW part STA link status */
	{
		u8 __tmp = rQueryStaStatistics.ucPer;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_PER, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u8 __tmp = rQueryStaStatistics.ucRcpi;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_RSSI, sizeof(u8),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4PhyMode;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_PHY_MODE, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u16 __tmp = rQueryStaStatistics.u2LinkSpeed;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_TX_RATE, sizeof(u16),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxFailCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_FAIL_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxLifeTimeoutCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_TIMEOUT_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageAirTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_AVG_AIR_TIME, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Driver part link status */
	{
		u32 __tmp = rQueryStaStatistics.u4TxTotalCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_TOTAL_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxExceedThresholdCount;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_THRESHOLD_CNT, sizeof(u32),
		    &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageProcessTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_AVG_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxMaxTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_MAX_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxAverageHifTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_AVG_HIF_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}
	{
		u32 __tmp = rQueryStaStatistics.u4TxMaxHifTime;

		if (unlikely(nla_put(skb,
		    NL80211_TESTMODE_STA_STATISTICS_MAX_HIF_PROCESS_TIME,
		    sizeof(u32), &__tmp) < 0))
			goto nla_put_failure;
	}

	/* Network counter */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_EMPTY_CNT_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceEmptyCount),
	    rQueryStaStatistics.au4TcResourceEmptyCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_NO_TC_ARRAY,
	    sizeof(rQueryStaStatistics.au4DequeueNoTcResource),
	    rQueryStaStatistics.au4DequeueNoTcResource) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_RB_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceBackCount),
	    rQueryStaStatistics.au4TcResourceBackCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_USED_TC_PGCT_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceUsedPageCount),
	    rQueryStaStatistics.au4TcResourceUsedPageCount) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_WANTED_TC_PGCT_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcResourceWantedPageCount),
	    rQueryStaStatistics.au4TcResourceWantedPageCount) < 0))
		goto nla_put_failure;

	/* Sta queue length */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_QUE_LEN_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcQueLen),
	    rQueryStaStatistics.au4TcQueLen) < 0))
		goto nla_put_failure;

	/* Global QM counter */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_AVG_QUE_LEN_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcAverageQueLen),
	    rQueryStaStatistics.au4TcAverageQueLen) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_TC_CUR_QUE_LEN_ARRAY,
	    sizeof(rQueryStaStatistics.au4TcCurrentQueLen),
	    rQueryStaStatistics.au4TcCurrentQueLen) < 0))
		goto nla_put_failure;

	/* Reserved field */
	if (unlikely(nla_put(skb,
	    NL80211_TESTMODE_STA_STATISTICS_RESERVED_ARRAY,
	    sizeof(rQueryStaStatistics.au4Reserved),
	    rQueryStaStatistics.au4Reserved) < 0))
		goto nla_put_failure;

	return cfg80211_testmode_reply(skb);

nla_put_failure:
	/* nal_put_skb_fail */
	kfree_skb(skb);
	return -EFAULT;
}

int
mtk_cfg80211_testmode_get_link_detection(IN struct wiphy
		*wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len,
		IN struct GLUE_INFO *prGlueInfo)
{

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4Status = -EINVAL;
	uint32_t u4BufLen;
	uint8_t u1buf = 0;
	uint32_t i = 0;
	uint32_t arBugReport[sizeof(struct EVENT_BUG_REPORT)];
	struct PARAM_802_11_STATISTICS_STRUCT rStatistics;
	struct EVENT_BUG_REPORT *prBugReport;
	struct sk_buff *skb;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	prBugReport = (struct EVENT_BUG_REPORT *) kalMemAlloc(
			      sizeof(struct EVENT_BUG_REPORT), VIR_MEM_TYPE);
	if (!prBugReport) {
		DBGLOG(QM, TRACE, "%s allocate prBugReport failed\n",
		       __func__);
		return -ENOMEM;
	}
	skb = cfg80211_testmode_alloc_reply_skb(wiphy,
			sizeof(struct PARAM_802_11_STATISTICS_STRUCT) +
			sizeof(struct EVENT_BUG_REPORT) + 1);

	if (!skb) {
		kalMemFree(prBugReport, VIR_MEM_TYPE,
			   sizeof(struct EVENT_BUG_REPORT));
		DBGLOG(QM, TRACE, "%s allocate skb failed\n", __func__);
		return -ENOMEM;
	}

	kalMemZero(&rStatistics, sizeof(rStatistics));
	kalMemZero(prBugReport, sizeof(struct EVENT_BUG_REPORT));
	kalMemZero(arBugReport,
		sizeof(struct EVENT_BUG_REPORT) * sizeof(uint32_t));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStatistics,
			   &rStatistics, sizeof(rStatistics),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "query statistics error:%x\n", rStatus);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryBugReport,
			   prBugReport, sizeof(struct EVENT_BUG_REPORT),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, INFO, "query statistics error:%x\n", rStatus);

	kalMemCopy(arBugReport, prBugReport,
		   sizeof(struct EVENT_BUG_REPORT));

	rStatistics.u4RstReason = glGetRstReason();
	rStatistics.u8RstTime = u8ResetTime;
	rStatistics.u4RoamFailCnt = prGlueInfo->u4RoamFailCnt;
	rStatistics.u8RoamFailTime = prGlueInfo->u8RoamFailTime;
	rStatistics.u2TxDoneDelayIsARP =
		prGlueInfo->fgTxDoneDelayIsARP;
	rStatistics.u4ArriveDrvTick = prGlueInfo->u4ArriveDrvTick;
	rStatistics.u4EnQueTick = prGlueInfo->u4EnQueTick;
	rStatistics.u4DeQueTick = prGlueInfo->u4DeQueTick;
	rStatistics.u4LeaveDrvTick = prGlueInfo->u4LeaveDrvTick;
	rStatistics.u4CurrTick = prGlueInfo->u4CurrTick;
	rStatistics.u8CurrTime = prGlueInfo->u8CurrTime;

	if (!NLA_PUT_U8(skb, NL80211_TESTMODE_LINK_INVALID, &u1buf))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_FAIL_CNT,
			 &rStatistics.rFailedCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_RETRY_CNT,
			 &rStatistics.rRetryCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb,
			 NL80211_TESTMODE_LINK_TX_MULTI_RETRY_CNT,
			 &rStatistics.rMultipleRetryCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_ACK_FAIL_CNT,
			 &rStatistics.rACKFailureCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_FCS_ERR_CNT,
			 &rStatistics.rFCSErrorCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_TX_CNT,
			 &rStatistics.rTransmittedFragmentCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_RX_CNT,
			 &rStatistics.rReceivedFragmentCount.QuadPart))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_RST_REASON,
			 &rStatistics.u4RstReason))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_RST_TIME,
			 &rStatistics.u8RstTime))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_ROAM_FAIL_TIMES,
			 &rStatistics.u4RoamFailCnt))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_ROAM_FAIL_TIME,
			 &rStatistics.u8RoamFailTime))
		goto nla_put_failure;

	if (!NLA_PUT_U8(skb,
			NL80211_TESTMODE_LINK_TX_DONE_DELAY_IS_ARP,
			&rStatistics.u2TxDoneDelayIsARP))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_ARRIVE_DRV_TICK,
			 &rStatistics.u4ArriveDrvTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_ENQUE_TICK,
			 &rStatistics.u4EnQueTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_DEQUE_TICK,
			 &rStatistics.u4DeQueTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_LEAVE_DRV_TICK,
			 &rStatistics.u4LeaveDrvTick))
		goto nla_put_failure;

	if (!NLA_PUT_U32(skb, NL80211_TESTMODE_LINK_CURR_TICK,
			 &rStatistics.u4CurrTick))
		goto nla_put_failure;

	if (!NLA_PUT_U64(skb, NL80211_TESTMODE_LINK_CURR_TIME,
			 &rStatistics.u8CurrTime))
		goto nla_put_failure;

	for (i = 0;
	     i < sizeof(struct EVENT_BUG_REPORT) / sizeof(uint32_t);
	     i++) {
		if (!NLA_PUT_U32(skb, i + NL80211_TESTMODE_LINK_DETECT_NUM,
				 &arBugReport[i]))
			goto nla_put_failure;
	}

	i4Status = cfg80211_testmode_reply(skb);
	kalMemFree(prBugReport, VIR_MEM_TYPE,
		   sizeof(struct EVENT_BUG_REPORT));
	return i4Status;

nla_put_failure:
	/* nal_put_skb_fail */
	kfree_skb(skb);
	kalMemFree(prBugReport, VIR_MEM_TYPE,
		   sizeof(struct EVENT_BUG_REPORT));
	return -EFAULT;
}

int mtk_cfg80211_testmode_sw_cmd(IN struct wiphy *wiphy,
		IN struct wireless_dev *wdev,
		IN void *data, IN int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct NL80211_DRIVER_SW_CMD_PARAMS *prParams =
		(struct NL80211_DRIVER_SW_CMD_PARAMS *) NULL;
	uint32_t rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	uint32_t u4SetInfoLen = 0;

	ASSERT(wiphy);

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

#if 0
	DBGLOG(INIT, INFO, "--> %s()\n", __func__);
#endif

	if (data && len)
		prParams = (struct NL80211_DRIVER_SW_CMD_PARAMS *) data;

	if (prParams) {
		if (prParams->set == 1) {
			rstatus = kalIoctl(prGlueInfo,
				   (PFN_OID_HANDLER_FUNC) wlanoidSetSwCtrlWrite,
				   &prParams->adr, (uint32_t) 8,
				   FALSE, FALSE, TRUE, &u4SetInfoLen);
		}
	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

static int mtk_wlan_cfg_testmode_cmd(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
				     void *data, int len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct NL80211_DRIVER_TEST_MODE_PARAMS *prParams = NULL;
	int32_t i4Status = 0;

	ASSERT(wiphy);

	if (!data || !len) {
		DBGLOG(REQ, ERROR, "mtk_cfg80211_testmode_cmd null data\n");
		return -EINVAL;
	}

	if (!wiphy) {
		DBGLOG(REQ, ERROR,
		       "mtk_cfg80211_testmode_cmd null wiphy\n");
		return -EINVAL;
	}

	prGlueInfo = (struct GLUE_INFO *)wiphy_priv(wiphy);
	prParams = (struct NL80211_DRIVER_TEST_MODE_PARAMS *)data;

	/* Clear the version byte */
	prParams->index = prParams->index & ~BITS(24, 31);
	DBGLOG(INIT, TRACE, "params index=%x\n", prParams->index);

	switch (prParams->index) {
	case TESTMODE_CMD_ID_SW_CMD:	/* SW cmd */
		i4Status = mtk_cfg80211_testmode_sw_cmd(wiphy,
				wdev, data, len);
		break;
	case TESTMODE_CMD_ID_WAPI:	/* WAPI */
#if CFG_SUPPORT_WAPI
		i4Status = mtk_cfg80211_testmode_set_key_ext(wiphy,
				wdev, data, len);
#endif
		break;
	case 0x10:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy,
				data, len, prGlueInfo);
		break;
	case 0x20:
		i4Status = mtk_cfg80211_testmode_get_link_detection(wiphy,
				wdev, data, len, prGlueInfo);
		break;
#if CFG_SUPPORT_PASSPOINT
	case TESTMODE_CMD_ID_HS20:
		i4Status = mtk_cfg80211_testmode_hs20_cmd(wiphy,
				wdev, data, len);
		break;
#endif /* CFG_SUPPORT_PASSPOINT */
	case TESTMODE_CMD_ID_STR_CMD:
		i4Status = mtk_cfg80211_process_str_cmd(prGlueInfo,
				wdev,
				(uint8_t *)(prParams + 1),
				len - sizeof(*prParams));
		break;

	default:
		i4Status = -EINVAL;
		break;
	}

	if (i4Status != 0)
		DBGLOG(REQ, TRACE, "prParams->index=%d, status=%d\n",
		       prParams->index, i4Status);

	return i4Status;
}

#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      void *data, int len)
{
	ASSERT(wdev);
	return mtk_wlan_cfg_testmode_cmd(wiphy, wdev, data, len);
}
#else
int mtk_cfg80211_testmode_cmd(struct wiphy *wiphy,
			      void *data, int len)
{
	return mtk_wlan_cfg_testmode_cmd(wiphy, NULL, data, len);
}
#endif
#endif






#if CFG_SUPPORT_SCHED_SCAN
int mtk_cfg80211_sched_scan_start(IN struct wiphy *wiphy,
				  IN struct net_device *ndev,
				  IN struct cfg80211_sched_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t i, u4BufLen;
	struct PARAM_SCHED_SCAN_REQUEST *prSchedScanRequest = NULL;
	uint32_t num = 0;
	uint8_t ucBssIndex = 0;

	if (!wiphy || !ndev)
		return -EINVAL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (!prGlueInfo || !prGlueInfo->prAdapter) {
		DBGLOG(REQ, ERROR, "Adapter or GlueInfo is NULL\n");
		return -EIO;
	}

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	/* 1. Preliminary Sanity Checks */
	if (prGlueInfo->prSchedScanRequest != NULL) {
		DBGLOG(SCN, ERROR, "Scheduled scan already in progress\n");
		return -EBUSY;
	}

	if (!request) {
		DBGLOG(SCN, ERROR, "Request is NULL\n");
		return -EINVAL;
	}

	/* Logic Fix: If iwd sends 0 match sets, we should return success but 
	 * do nothing, or return -EOPNOTSUPP. -EINVAL causes an error loop.
	 */
	if (!request->n_match_sets) {
		DBGLOG(SCN, WARN, "No match sets provided by iwd\n");
		return -EOPNOTSUPP;
	}

	/* 2. Allocate and Zero the Request Structure */
	prSchedScanRequest = (struct PARAM_SCHED_SCAN_REQUEST *)
				kalMemAlloc(sizeof(struct PARAM_SCHED_SCAN_REQUEST), VIR_MEM_TYPE);
	if (!prSchedScanRequest) {
		DBGLOG(SCN, ERROR, "Failed to allocate prSchedScanRequest\n");
		return -ENOMEM;
	}
	kalMemZero(prSchedScanRequest, sizeof(struct PARAM_SCHED_SCAN_REQUEST));

	/* 3. Handle SSIDs to scan */
	num = 0;
	if (request->ssids) {
		for (i = 0; i < request->n_ssids && num < CFG_SCAN_HIDDEN_SSID_MAX_NUM; i++) {
			if (request->ssids[i].ssid_len > 0 && request->ssids[i].ssid[0] != 0) {
				struct PARAM_SSID *prSsid = &(prSchedScanRequest->arSsid[num]);
				COPY_SSID(prSsid->aucSsid, prSsid->u4SsidLen,
					  request->ssids[i].ssid, request->ssids[i].ssid_len);
				num++;
			}
		}
	}
	prSchedScanRequest->u4SsidNum = num;

	/* 4. Handle Match Sets (SSIDs to look for) */
	num = 0;
	if (request->match_sets) {
		for (i = 0; i < request->n_match_sets && num < CFG_SCAN_SSID_MATCH_MAX_NUM; i++) {
			if (request->match_sets[i].ssid.ssid_len > 0) {
				struct PARAM_SSID *prSsid = &(prSchedScanRequest->arMatchSsid[num]);
				COPY_SSID(prSsid->aucSsid, prSsid->u4SsidLen,
					  request->match_sets[i].ssid.ssid,
					  request->match_sets[i].ssid.ssid_len);
				
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
				prSchedScanRequest->ai4RssiThold[num] = request->match_sets[i].rssi_thold;
#else
				prSchedScanRequest->ai4RssiThold[num] = request->rssi_thold;
#endif
				num++;
			}
		}
	}
	prSchedScanRequest->u4MatchSsidNum = num;

	/* 5. Handle Random MAC address scanning */
	if (kalSchedScanParseRandomMac(ndev, request,
		prSchedScanRequest->aucRandomMac,
		prSchedScanRequest->aucRandomMacMask)) {
		prSchedScanRequest->ucScnFuncMask |= ENUM_SCN_RANDOM_MAC_EN;
	}

	/* 6. Handle IE Buffer (WPS/P2P data) */
	prSchedScanRequest->u4IELength = request->ie_len;
	if (request->ie_len > 0) {
		prSchedScanRequest->pucIE = kalMemAlloc(request->ie_len, VIR_MEM_TYPE);
		if (!prSchedScanRequest->pucIE) {
			DBGLOG(SCN, ERROR, "Failed to allocate IE buffer\n");
			goto err_free_request;
		}
		kalMemCopy(prSchedScanRequest->pucIE, (uint8_t *)request->ie, request->ie_len);
	}

	/* 7. Handle Scan Interval */
#if KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE
	prSchedScanRequest->u2ScanInterval = (uint16_t)(request->scan_plans[0].interval);
#else
	prSchedScanRequest->u2ScanInterval = (uint16_t)(request->interval);
#endif

	/* 8. CRITICAL FIX: Handle Channel Population */
	prSchedScanRequest->ucChnlNum = (uint8_t)request->n_channels;
	if (request->n_channels == 0) {
		/* CONTRACT: 0 channels means scan all supported channels.
		 * We tell firmware ucChnlNum = 0 to trigger its default list.
		 */
		prSchedScanRequest->pucChannels = NULL;
		DBGLOG(SCN, INFO, "Full-band scheduled scan requested\n");
	} else {
		prSchedScanRequest->pucChannels = kalMemAlloc(request->n_channels, VIR_MEM_TYPE);
		if (!prSchedScanRequest->pucChannels) {
			DBGLOG(SCN, ERROR, "Failed to allocate channel buffer\n");
			goto err_free_ie;
		}
		for (i = 0; i < request->n_channels; i++) {
			uint32_t freq = request->channels[i]->center_freq * 1000;
			prSchedScanRequest->pucChannels[i] = nicFreq2ChannelNum(freq);
		}
	}

	/* 9. Send IOCTL to Firmware */
	prSchedScanRequest->ucBssIndex = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetStartSchedScan,
			   prSchedScanRequest,
			   sizeof(struct PARAM_SCHED_SCAN_REQUEST),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "Scheduled scan firmware rejected: %x\n", rStatus);
		goto err_free_channels;
	}

	/* Success: Memory is now owned by the OID/Adapter until scan completes */
	return 0;

err_free_channels:
	if (prSchedScanRequest->pucChannels)
		kalMemFree(prSchedScanRequest->pucChannels, VIR_MEM_TYPE, request->n_channels);
err_free_ie:
	if (prSchedScanRequest->pucIE)
		kalMemFree(prSchedScanRequest->pucIE, VIR_MEM_TYPE, request->ie_len);
err_free_request:
	kalMemFree(prSchedScanRequest, VIR_MEM_TYPE, sizeof(struct PARAM_SCHED_SCAN_REQUEST));
	
	/* Return EBUSY or ENOTSUPP instead of EINVAL to prevent iwd crash loops */
	return -EBUSY;
}





#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy,
				 IN struct net_device *ndev,
				 IN u64 reqid)
#else
int mtk_cfg80211_sched_scan_stop(IN struct wiphy *wiphy,
				 IN struct net_device *ndev)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	if (!wiphy || !ndev)
		return -EINVAL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (!prGlueInfo || !prGlueInfo->prAdapter)
		return -EIO;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, TRACE, "ucBssIndex = %d\n", ucBssIndex);
	scanlog_dbg(LOG_SCHED_SCAN_REQ_STOP_K2D, INFO, "--> %s()\n", __func__);

	/* * Hardening: If iwd asks to stop a scan that isn't running, 
	 * return 0 (success). This prevents "Operation not permitted" 
	 * errors from bubbling up and breaking iwd's state machine.
	 */
	if (prGlueInfo->prSchedScanRequest == NULL) {
		DBGLOG(REQ, INFO, "No active sched_scan to stop\n");
		return 0;
	}

	/* * Identity fix: Use a local define to handle the undeclared OID error.
	 * 0x070203 is the universal 'Stop Sched Scan' OID for MTK Gen4 chips.
	 */
#ifndef wlanoidSetStopSchedScan
#ifdef wlanoidSetSchedScanStop
#define wlanoidSetStopSchedScan wlanoidSetSchedScanStop
#else
#define wlanoidSetStopSchedScan 0x070203
#endif
#endif

	rStatus = kalIoctl(prGlueInfo, wlanoidSetStopSchedScan,
			   NULL, 0,
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "firmware failed to stop sched scan: %x\n", rStatus);
		/* * Even if firmware fails, we should clear the local request 
		 * pointer to prevent the driver from being stuck in "busy" mode.
		 */
		prGlueInfo->prSchedScanRequest = NULL;
		return -EBUSY;
	}

	/* Success: Clear the pointer as the scan is officially dead */
	prGlueInfo->prSchedScanRequest = NULL;

	return 0;
}

#endif /* CFG_SUPPORT_SCHED_SCAN */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for handling association request
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_assoc(struct wiphy *wiphy,
	       struct net_device *ndev, struct cfg80211_assoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;
	struct AIS_FSM_INFO *prAisFsmInfo = NULL;

	enum ENUM_WEP_STATUS eEncStatus;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	uint32_t cipher;
	u_int8_t fgCarryWPSIE = FALSE;
	uint32_t i, u4AkmSuite;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY *prEntry;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
	struct IEEE_802_11_MIB *prMib;
	uint8_t *prDesiredIE = NULL;
	uint8_t *pucIEStart = NULL;
	struct RSN_INFO rRsnInfo;
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	struct STA_RECORD *prStaRec = NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
			(struct P2P_CONNECTION_REQ_INFO *) NULL;
#endif
#if (CFG_SUPPORT_SUPPLICANT_SME == 1) && (CFG_SUPPORT_802_11R == 1)
	uint32_t u4InfoBufLen = 0;
#endif
#if CFG_SUPPORT_WPA3_H2E
	uint8_t fgCarryRsnxe = FALSE;
#endif

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return -EINVAL;

	prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter, ucBssIndex);
	if (!prAisFsmInfo->fgIsCfg80211Connecting) return -EINVAL;

	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter, ucBssIndex);

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	/* [todo]temp use for indicate rx assoc resp, may need to be modified */

	/* The BSS from cfg80211_ops.assoc must give back to
	* cfg80211_send_rx_assoc() or to cfg80211_assoc_timeout().
	* To ensure proper refcounting,
	* new association requests while already associating
	* must be rejected.
	*/
	if (prConnSettings->bss)
		return -ENOENT;
	prConnSettings->bss = req->bss;
#endif

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	if (!prConnSettings->fgIsP2pConn)
#endif
	{
		kalMemZero(arBssid, MAC_ADDR_LEN);
		if (prGlueInfo->eParamMediaStateIndicated[ucBssIndex]
				== MEDIA_STATE_CONNECTED) {
			SET_IOCTL_BSSIDX(prGlueInfo->prAdapter, ucBssIndex);

			wlanQueryInformation(
				prGlueInfo->prAdapter, wlanoidQueryBssid,
				&arBssid[0], sizeof(arBssid), &u4BufLen);
#if !CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT || !CFG_SUPPORT_802_11R
			/* 1. check BSSID */
			if (UNEQUAL_MAC_ADDR(arBssid, req->bss->bssid)) {
				/* wrong MAC address */
				DBGLOG(REQ, WARN,
				       "incorrect BSSID: [" MACSTR
				       "] currently connected BSSID["
				       MACSTR "]\n",
				       MAC2STR(req->bss->bssid),
				       MAC2STR(arBssid));
				return -ENOENT;
			}
#endif
		}
	}

	/* <1> Reset WPA info */
	prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion =
		IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo[ucBssIndex].u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo[ucBssIndex].u4CipherPairwise = IW_AUTH_CIPHER_NONE;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroupMgmt =
						IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo[ucBssIndex].u4Mfp = IW_AUTH_MFP_DISABLED;
	prGlueInfo->rWpaInfo[ucBssIndex].ucRSNMfpCap = RSN_AUTH_MFP_DISABLED;
#endif

	/* 2.Fill WPA version */
	if (req->crypto.wpa_versions & NL80211_WPA_VERSION_1)
		prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion =
					IW_AUTH_WPA_VERSION_WPA;
	else if (req->crypto.wpa_versions & NL80211_WPA_VERSION_2)
		prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion =
					IW_AUTH_WPA_VERSION_WPA2;
	else if (req->crypto.wpa_versions & NL80211_WPA_VERSION_3)
		prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion =
					IW_AUTH_WPA_VERSION_WPA3;
	else
		prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion =
					IW_AUTH_WPA_VERSION_DISABLED;
	DBGLOG(REQ, INFO, "wpa ver=%d\n",
		prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion);

	/* 3.Fill pairwise cipher suite */
	if (req->crypto.n_ciphers_pairwise) {
		DBGLOG(RSN, INFO,
			"[wlan] cipher pairwise (%x)\n",
			req->crypto.ciphers_pairwise[0]);

		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[0] =
		    req->crypto.ciphers_pairwise[0];
		switch (req->crypto.ciphers_pairwise[0]) {
		case WLAN_CIPHER_SUITE_WEP40:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			prGlueInfo->rWpaInfo[ucBssIndex].
				u4CipherPairwise = IW_AUTH_CIPHER_GCMP256;
			break;
		default:
			DBGLOG(REQ, WARN,
				"invalid cipher pairwise (%d)\n",
				req->crypto.ciphers_pairwise[0]);
			return -EINVAL;
		}
	}

	/* 4. Fill group cipher suite */
	if (req->crypto.cipher_group) {
		DBGLOG(RSN, INFO,
			"[wlan] cipher group (%x)\n",
			req->crypto.cipher_group);
		prConnSettings->rRsnInfo.u4GroupKeyCipherSuite =
						req->crypto.cipher_group;
		switch (req->crypto.cipher_group) {
		case WLAN_CIPHER_SUITE_WEP40:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_CCMP;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup =
						IW_AUTH_CIPHER_GCMP256;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
			break;
		default:
			DBGLOG(REQ, WARN,
				"invalid cipher group (%d)\n",
				req->crypto.cipher_group);
			return -EINVAL;
		}
	}

	/* 5. Fill encryption status */
	cipher = prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroup |
		prGlueInfo->rWpaInfo[ucBssIndex].u4CipherPairwise;
	if (1 /* prGlueInfo->rWpaInfo.fgPrivacyInvoke */) {
		if (cipher & IW_AUTH_CIPHER_GCMP256) {
			eEncStatus = ENUM_ENCRYPTION4_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_CCMP) {
			eEncStatus = ENUM_ENCRYPTION3_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_TKIP) {
			eEncStatus = ENUM_ENCRYPTION2_ENABLED;
		} else if (cipher & (IW_AUTH_CIPHER_WEP104 |
					IW_AUTH_CIPHER_WEP40)) {
			eEncStatus = ENUM_ENCRYPTION1_ENABLED;
		} else if (cipher & IW_AUTH_CIPHER_NONE) {
			if (prGlueInfo->rWpaInfo[ucBssIndex].fgPrivacyInvoke)
				eEncStatus = ENUM_ENCRYPTION1_ENABLED;
			else
				eEncStatus = ENUM_ENCRYPTION_DISABLED;
		} else {
			eEncStatus = ENUM_ENCRYPTION_DISABLED;
		}
	} else {
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
	}

	rStatus = kalIoctl(prGlueInfo,
			wlanoidSetEncryptionStatus,
			&eEncStatus, sizeof(eEncStatus),
			FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set encryption mode error:%x\n", rStatus);

	/* 6. Fill AKM suites */
	u4AkmSuite = 0;
	eAuthMode = AUTH_MODE_OPEN;
	DBGLOG(REQ, INFO,
		"request numbers of Akm Suite:%d\n",
		req->crypto.n_akm_suites);
	for (i = 0; i < req->crypto.n_akm_suites; i++) {
		DBGLOG(REQ, INFO,
			"request Akm Suite[0x%x]:%d\n",
			i, req->crypto.akm_suites[i]);
	}

	if (req->crypto.n_akm_suites) {
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[0] =
		    req->crypto.akm_suites[0];
		DBGLOG(REQ, INFO,
			"Akm Suite:0x%x\n",
			req->crypto.akm_suites[0]);

		if (prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion ==
					IW_AUTH_WPA_VERSION_WPA) {
			switch (req->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				eAuthMode = AUTH_MODE_WPA;
				u4AkmSuite = WPA_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				eAuthMode = AUTH_MODE_WPA_PSK;
				u4AkmSuite = WPA_AKM_SUITE_PSK;
				break;
#if CFG_SUPPORT_802_11R
			case WLAN_AKM_SUITE_FT_8021X:
				eAuthMode = AUTH_MODE_WPA2_FT;
				u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
				break;
			case WLAN_AKM_SUITE_FT_PSK:
				eAuthMode = AUTH_MODE_WPA2_FT_PSK;
				u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
				break;
#endif

			default:
				DBGLOG(REQ, WARN,
					"invalid Akm Suite (%08x)\n",
					req->crypto.akm_suites[0]);
				return -EINVAL;
			}
		} else if (prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion ==
					IW_AUTH_WPA_VERSION_WPA2) {
			switch (req->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				eAuthMode = AUTH_MODE_WPA2;
				u4AkmSuite = RSN_AKM_SUITE_802_1X;
				break;
			case WLAN_AKM_SUITE_PSK:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_PSK;
				break;
#if CFG_SUPPORT_802_11W
				/* Notice:: Need kernel patch!! */
			case WLAN_AKM_SUITE_8021X_SHA256:
				eAuthMode = AUTH_MODE_WPA2;
				u4AkmSuite = RSN_AKM_SUITE_802_1X_SHA256;
				break;
			case WLAN_AKM_SUITE_PSK_SHA256:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_PSK_SHA256;
				break;
#endif
#if CFG_SUPPORT_PASSPOINT
			case WLAN_AKM_SUITE_OSEN:
				eAuthMode = AUTH_MODE_WPA_OSEN;
				u4AkmSuite = WFA_AKM_SUITE_OSEN;
			break;
#endif
			case WLAN_AKM_SUITE_8021X_SUITE_B:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;

			case WLAN_AKM_SUITE_8021X_SUITE_B_192:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_8021X_SUITE_B_192;
				break;
			case WLAN_AKM_SUITE_SAE:
				eAuthMode = AUTH_MODE_WPA3_SAE;
				u4AkmSuite = RSN_AKM_SUITE_SAE;
			break;
#if CFG_SUPPORT_OWE
			case WLAN_AKM_SUITE_OWE:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_OWE;
			break;
#endif
#if CFG_SUPPORT_802_11R
			case WLAN_AKM_SUITE_FT_8021X:
				eAuthMode = AUTH_MODE_WPA2_FT;
				u4AkmSuite = RSN_AKM_SUITE_FT_802_1X;
				break;
			case WLAN_AKM_SUITE_FT_PSK:
				eAuthMode = AUTH_MODE_WPA2_FT_PSK;
				u4AkmSuite = RSN_AKM_SUITE_FT_PSK;
				break;
#endif
#if CFG_SUPPORT_DPP
			case WLAN_AKM_SUITE_DPP:
				eAuthMode = AUTH_MODE_WPA2_PSK;
				u4AkmSuite = RSN_AKM_SUITE_DPP;
			break;
#endif

			default:
				DBGLOG(REQ, WARN,
					"invalid Akm Suite (%08x)\n",
					req->crypto.akm_suites[0]);
				return -EINVAL;
			}
		} else if (prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion ==
					IW_AUTH_WPA_VERSION_WPA3) {
			switch (req->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_SAE:
				eAuthMode = AUTH_MODE_WPA3_SAE;
				u4AkmSuite = RSN_AKM_SUITE_SAE;
				break;
			default:
				DBGLOG(REQ, WARN,
					"invalid Akm Suite (%08x)\n",
					req->crypto.akm_suites[0]);
				return -EINVAL;
			}
		}
	}
	if (prGlueInfo->rWpaInfo[ucBssIndex].u4WpaVersion ==
				IW_AUTH_WPA_VERSION_DISABLED) {
		eAuthMode = (prGlueInfo->rWpaInfo[ucBssIndex].u4AuthAlg ==
		IW_AUTH_ALG_OPEN_SYSTEM) ?
		AUTH_MODE_OPEN : AUTH_MODE_AUTO_SWITCH;
	}

	DBGLOG(REQ, INFO,
		"set auth mode:%d, akm suite:0x%x\n", eAuthMode, u4AkmSuite);

	/* 6.1 Set auth mode*/
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAuthMode,
			   &eAuthMode, sizeof(eAuthMode),
			   FALSE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "set auth mode error:%x\n", rStatus);

	prMib = aisGetMib(prGlueInfo->prAdapter, ucBssIndex);
	/* 6.2 Enable the specific AKM suite only. */
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prMib->dot11RSNAConfigAuthenticationSuitesTable[i];

		if (prEntry->dot11RSNAConfigAuthenticationSuite ==
			u4AkmSuite) {
			prEntry->
				dot11RSNAConfigAuthenticationSuiteEnabled =
									TRUE;
			DBGLOG(REQ, INFO,
				"match AuthenticationSuite = 0x%x", u4AkmSuite);
		} else {
			prEntry->
				dot11RSNAConfigAuthenticationSuiteEnabled =
									FALSE;
		}
	}

	/* 7. Parsing desired ie from upper layer */
	prConnSettings->fgWpsActive = FALSE;

#if CFG_SUPPORT_PASSPOINT
	prGlueInfo->fgConnectHS20AP = FALSE;
#endif /* CFG_SUPPORT_PASSPOINT */

	if (req->ie && req->ie_len > 0) {
		pucIEStart = (uint8_t *)req->ie;
#if CFG_SUPPORT_WAPI
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetWapiAssocInfo,
				   pucIEStart, req->ie_len,
				   FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(SEC, WARN,
			"[wapi] set wapi assoc info error:%x\n", rStatus);
#endif
#if CFG_SUPPORT_WPS2
		if (wextSrchDesiredWPSIE(pucIEStart,
			req->ie_len, 0xDD, (uint8_t **) &prDesiredIE)) {
			prConnSettings->fgWpsActive = TRUE;
			fgCarryWPSIE = TRUE;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetWSCAssocInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE),
					   FALSE, FALSE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(SEC, WARN,
					"[WSC] set WSC assoc info error:%x\n",
					rStatus);
		}
#endif
#if (CFG_SUPPORT_SUPPLICANT_MBO == 1)
		if (wextSrchDesiredSupOpClassIE((uint8_t *) req->ie,
			req->ie_len, (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetSupOpClass,
				prDesiredIE, IE_SIZE(prDesiredIE),
				FALSE, FALSE, FALSE, &u4BufLen);
		}

		if (wextSrchDesiredMboIE((uint8_t *) req->ie,
			req->ie_len, (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetMbo,
				prDesiredIE, IE_SIZE(prDesiredIE),
				FALSE, FALSE, FALSE, &u4BufLen);
		}
#endif /* CFG_SUPPORT_SUPPLICANT_MBO */
#if CFG_SUPPORT_PASSPOINT
		if (wextSrchDesiredHS20IE(pucIEStart, sme->ie_len,
					  (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctlByBssIdx(prGlueInfo,
					   wlanoidSetHS20Info,
					   prDesiredIE, IE_SIZE(prDesiredIE),
					   FALSE, FALSE, FALSE, &u4BufLen,
					   ucBssIndex);
#if 0
			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO,
					"[HS20] set HS20 assoc info error:%x\n",
					rStatus);
#endif
		} else if (wextSrchDesiredOsenIE(pucIEStart, sme->ie_len,
					(uint8_t **) &prDesiredIE)) {
			/* we can reuse aucHS20AssocInfoIE because hs20
			 * indication IE is not present when OSEN exist
			 */
			kalMemCopy(prGlueInfo->aucHS20AssocInfoIE,
					prDesiredIE, IE_SIZE(prDesiredIE));
			prGlueInfo->u2HS20AssocInfoIELen =
						(uint16_t)IE_SIZE(prDesiredIE);
		}

		if (wextSrchDesiredInterworkingIE((uint8_t *) req->ie,
		    req->ie_len, (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					wlanoidSetInterworkingInfo, prDesiredIE,
					IE_SIZE(prDesiredIE),
					FALSE, FALSE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, TRACE,
				 * ("[HS20] set Interworking assoc info error:
				 * %x\n", rStatus));
				 */
			}
		}

		if (wextSrchDesiredRoamingConsortiumIE((uint8_t *) req->ie,
		    req->ie_len, (uint8_t **) &prDesiredIE)) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRoamingConsortiumIEInfo,
					   prDesiredIE, IE_SIZE(prDesiredIE),
					   FALSE, FALSE, FALSE, &u4BufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				/* DBGLOG(REQ, TRACE,
				 *  ("[HS20] set RoamingConsortium assoc info
				 *   error:%x\n", rStatus));
				 */
			}
			}
#endif /* CFG_SUPPORT_PASSPOINT */
		kalMemZero(&rRsnInfo, sizeof(struct RSN_INFO));
		if (wextSrchDesiredWPAIE(pucIEStart,
			req->ie_len, 0x30, (uint8_t **) &prDesiredIE)) {
			if (rsnParseRsnIE(prGlueInfo->prAdapter,
				(struct RSN_INFO_ELEM *)prDesiredIE,
				&rRsnInfo)) {
#if CFG_SUPPORT_802_11W
				/* Fill RSNE MFP Cap */
				if (rRsnInfo.u2RsnCap & ELEM_WPA_CAP_MFPC) {
					prGlueInfo->rWpaInfo[ucBssIndex].
						u4CipherGroupMgmt =
						rRsnInfo.
						u4GroupMgmtKeyCipherSuite;
					prGlueInfo->rWpaInfo[ucBssIndex].
							ucRSNMfpCap =
							RSN_AUTH_MFP_OPTIONAL;
					if (rRsnInfo.u2RsnCap &
						ELEM_WPA_CAP_MFPR)
						prGlueInfo->
							rWpaInfo[ucBssIndex].
							ucRSNMfpCap =
							RSN_AUTH_MFP_REQUIRED;
				} else
					prGlueInfo->rWpaInfo[ucBssIndex].
							ucRSNMfpCap =
							RSN_AUTH_MFP_DISABLED;
#endif
				/* Fill RSNE PMKID Count and List */
				prConnSettings->rRsnInfo.u2PmkidCnt =
							rRsnInfo.u2PmkidCnt;
				DBGLOG(INIT, INFO,
				       "mtk_cfg80211_assoc: pmkidCnt=%d\n"
				       , prConnSettings->rRsnInfo.u2PmkidCnt);
				if (rRsnInfo.u2PmkidCnt > 0)
					kalMemCopy(prConnSettings->
							rRsnInfo.aucPmkidList,
						rRsnInfo.aucPmkidList,
						(rRsnInfo.u2PmkidCnt *
								RSN_PMKID_LEN));
				DBGLOG_MEM8(RSN, INFO,
					prConnSettings->rRsnInfo.aucPmkidList,
					(prConnSettings->rRsnInfo.u2PmkidCnt *
								RSN_PMKID_LEN));

				GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter,
				  ucBssIndex)->u2RsnSelectedCapInfo = rRsnInfo.u2RsnCap;
			}
		}

		/* Find non-wfa vendor specific ies set from upper layer */
		if (cfg80211_get_non_wfa_vendor_ie(prGlueInfo, pucIEStart,
			req->ie_len, ucBssIndex) > 0) {
			DBGLOG(RSN, INFO, "Found non-wfa vendor ie (len=%u)\n",
				   prConnSettings->non_wfa_vendor_ie_len);
		}

#if CFG_SUPPORT_OWE
		/* Gen OWE IE */
		if (wextSrchDesiredWPAIE(pucIEStart,
			req->ie_len, 0xff, (uint8_t **) &prDesiredIE)) {
			uint8_t ucLength = (*(prDesiredIE+1)+2);

			kalMemCopy(
				&prConnSettings->rOweInfo,
				prDesiredIE, ucLength);

			DBGLOG(REQ, INFO,
				"DUMP OWE INFO, EID %x length %x\n",
				*prDesiredIE, ucLength);
			DBGLOG_MEM8(REQ, INFO,
				&prConnSettings->rOweInfo,
				ucLength);
		} else {
			kalMemSet(&prConnSettings->rOweInfo,
				0, sizeof(struct OWE_INFO));
		}
#endif
#if CFG_SUPPORT_WPA3_H2E
		/* Gen RSNXE */
		if (wextSrchDesiredWPAIE(pucIEStart,
			req->ie_len, 0xf4, (uint8_t **) &prDesiredIE)) {
			uint16_t u2Length = (*(prDesiredIE+1)+2);

			if (u2Length <= sizeof(prConnSettings->rRsnXE)) {
				kalMemCopy(
					&prConnSettings->rRsnXE,
					prDesiredIE, u2Length);
				fgCarryRsnxe = TRUE;
				DBGLOG(REQ, INFO,
					"DUMP RSNXE, EID %x length %x\n",
					*prDesiredIE, u2Length);

				DBGLOG_MEM8(REQ, INFO,
					&prConnSettings->rRsnXE,
					u2Length);
			} else {
				DBGLOG(RSN, ERROR, "RSNXE length exceeds 2\n");
			}
		}
		if (fgCarryRsnxe == FALSE) {
			kalMemSet(&prConnSettings->rRsnXE,
				0, sizeof(struct RSNXE));
		}
#endif
#if (CFG_SUPPORT_SUPPLICANT_SME == 1) && (CFG_SUPPORT_802_11R == 1)
		if (prGlueInfo->prAdapter->rWifiVar
			.rConnSettings[ucBssIndex].eAuthMode == AUTH_MODE_WPA2_FT ||
			 prGlueInfo->prAdapter->rWifiVar.rConnSettings[ucBssIndex].eAuthMode ==
				 AUTH_MODE_WPA2_FT_PSK) {
			rStatus = kalIoctl(prGlueInfo, wlanoidUpdateFtIes,
				(void *)pucIEStart, req->ie_len, FALSE,
				FALSE, FALSE, &u4InfoBufLen);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(REQ, WARN,
					"update FTIE fail:%x\n", rStatus);
				return -EINVAL;
			}
		}
#endif

	}

	/* clear WSC Assoc IE buffer in case WPS IE is not detected */
	if (fgCarryWPSIE == FALSE) {
		kalMemZero(&prConnSettings->aucWSCAssocInfoIE, 200);
		prConnSettings->u2WSCAssocInfoIELen = 0;
	}

	/* Fill WPA info - mfp setting */
	/* Must put after paring RSNE from upper layer
	* for prGlueInfo->rWpaInfo.ucRSNMfpCap assignment
	*/
#if CFG_SUPPORT_802_11W
	if (req->use_mfp)
		prGlueInfo->rWpaInfo[ucBssIndex].u4Mfp = IW_AUTH_MFP_REQUIRED;
	else {
		/* Change Mfp parameter from DISABLED to OPTIONAL
		* if upper layer set MFPC = 1 in RSNE
		* since upper layer can't bring MFP OPTIONAL information
		* to driver by sme->mfp
		*/
		if (prGlueInfo->rWpaInfo[ucBssIndex].ucRSNMfpCap ==
					RSN_AUTH_MFP_OPTIONAL)
			prGlueInfo->rWpaInfo[ucBssIndex].u4Mfp =
					IW_AUTH_MFP_OPTIONAL;
		else if (prGlueInfo->rWpaInfo[ucBssIndex].ucRSNMfpCap ==
					RSN_AUTH_MFP_REQUIRED)
			DBGLOG(REQ, WARN,
				"mfp parameter(DISABLED) conflict with mfp cap(REQUIRED)\n");
	}
	DBGLOG(REQ, LOUD, "MFP=%d\n", prGlueInfo->rWpaInfo[ucBssIndex].u4Mfp);
#endif
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	/*[TODO]may to check if assoc parameters change as cfg80211_auth*/
	prConnSettings->fgIsSendAssoc = TRUE;
	if ((!prConnSettings->fgIsConnInitialized) &&
		(prConnSettings->fgIsP2pConn != TRUE)) {
		rStatus = kalIoctlByBssIdx(prGlueInfo,
				   wlanoidSetBssid,
				   (void *) req->bss->bssid,
				   MAC_ADDR_LEN, FALSE, FALSE, TRUE, &u4BufLen,
				   ucBssIndex);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, WARN, "set BSSID:%x\n", rStatus);
			return -EINVAL;
		}
	} else { /* skip join initial flow when it has been completed*/
		if ((mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) &&
				prConnSettings->fgIsP2pConn) {
			prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(
						prGlueInfo->prAdapter,
						prConnSettings->ucRoleIdx);
			prStaRec = prP2pRoleFsmInfo->rJoinInfo.prTargetStaRec;
			prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
			kalMemCopy(prConnReqInfo->aucIEBuf,
						req->ie, req->ie_len);
			prConnReqInfo->u4BufLength = req->ie_len;

			/* set crypto */
			kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_NONE,
						prConnSettings->ucRoleIdx);
			DBGLOG(REQ, INFO,
					"n_ciphers_pairwise %d, ciphers_pairwise[0] %#x\n",
					req->crypto.n_ciphers_pairwise,
					req->crypto.ciphers_pairwise[0]);

			if (req->crypto.n_ciphers_pairwise) {
				switch (req->crypto.ciphers_pairwise[0]) {
				case WLAN_CIPHER_SUITE_WEP40:
				case WLAN_CIPHER_SUITE_WEP104:
					kalP2PSetCipher(prGlueInfo,
						IW_AUTH_CIPHER_WEP40,
						prConnSettings->ucRoleIdx);
					break;
				case WLAN_CIPHER_SUITE_TKIP:
					kalP2PSetCipher(prGlueInfo,
						IW_AUTH_CIPHER_TKIP,
						prConnSettings->ucRoleIdx);
					break;
				case WLAN_CIPHER_SUITE_CCMP:
				case WLAN_CIPHER_SUITE_AES_CMAC:
					kalP2PSetCipher(prGlueInfo,
						IW_AUTH_CIPHER_CCMP,
						prConnSettings->ucRoleIdx);
					break;
				default:
					DBGLOG(REQ, WARN,
						"invalid cipher pairwise (%d)\n",
						req->crypto
						.ciphers_pairwise[0]);
					/* do cfg80211_put_bss before return */
					return -EINVAL;
				}
			}
			/* end  */
		} else {
			rStatus = kalIoctlByBssIdx(prGlueInfo,
				wlanoidSendAuthAssoc,
				(void *)req->bss->bssid, MAC_ADDR_LEN,
				FALSE, FALSE, TRUE, &u4BufLen, ucBssIndex);
			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(REQ, WARN,
					"send assoc failed:%x\n", rStatus);
				return -EINVAL;
			}
		}
	}
#else
	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSetBssid,
			(void *)req->bss->bssid, MAC_ADDR_LEN,
			FALSE, FALSE, TRUE, &u4BufLen,
			ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "set BSSID:%x\n", rStatus);
		return -EINVAL;
	}
#endif
	return 0;
}

#if CFG_SUPPORT_NFC_BEAM_PLUS

int mtk_cfg80211_testmode_get_scan_done(IN struct wiphy
					*wiphy, IN void *data, IN int len,
					IN struct GLUE_INFO *prGlueInfo)
{
	int32_t i4Status = -EINVAL;

#ifdef CONFIG_NL80211_TESTMODE
#define NL80211_TESTMODE_P2P_SCANDONE_INVALID 0
#define NL80211_TESTMODE_P2P_SCANDONE_STATUS 1

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t READY_TO_BEAM = 0;

	struct sk_buff *skb = NULL;

	ASSERT(wiphy);
	ASSERT(prGlueInfo);

	skb = cfg80211_testmode_alloc_reply_skb(wiphy,
						sizeof(uint32_t));

	/* READY_TO_BEAM =
	 * (UINT_32)(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsGOInitialDone)
	 * &(!prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsScanRequest);
	 */
	READY_TO_BEAM = 1;
	/* DBGLOG(QM, TRACE,
	 * "NFC:GOInitialDone[%d] and P2PScanning[%d]\n",
	 * prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsGOInitialDone,
	 * prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rScanReqInfo
	 * .fgIsScanRequest));
	 */

	if (!skb) {
		DBGLOG(QM, TRACE, "%s allocate skb failed:%x\n", __func__,
		       rStatus);
		return -ENOMEM;
	}
	{
		u8 __tmp = 0;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_INVALID,
		    sizeof(u8), &__tmp) < 0)) {
			kfree_skb(skb);
			return -EINVAL;
		}
	}
	{
		u32 __tmp = READY_TO_BEAM;

		if (unlikely(nla_put(skb, NL80211_TESTMODE_P2P_SCANDONE_STATUS,
		    sizeof(u32), &__tmp) < 0)) {
			kfree_skb(skb);
			return -EINVAL;
		}
	}

	i4Status = cfg80211_testmode_reply(skb);
#else
	DBGLOG(QM, WARN, "CONFIG_NL80211_TESTMODE not enabled\n");
#endif
	return i4Status;
}

#endif

#if CFG_SUPPORT_TDLS

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for changing a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_change_station(struct wiphy *wiphy,
			    struct net_device *ndev, const u8 *mac,
			    struct station_parameters *params_main)
{

	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_UPDATE rCmdUpdate;
	uint32_t rStatus;
	uint32_t u4BufLen, u4Temp;
	struct ADAPTER *prAdapter;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex = 0;
	//Copy from gen4m
#if (CFG_ADVANCED_80211_MLO == 1) || \
	KERNEL_VERSION(6, 0, 0) <= CFG80211_VERSION_CODE
	struct link_station_parameters *params =
			&(params_main->link_sta_params);
#else
	struct station_parameters *params = params_main;
#endif

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);
	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (!prBssInfo)
		return -EINVAL;

	if (params == NULL)
		return 0;
	else if (params->supported_rates == NULL)
		return 0;

	/* init */
	kalMemZero(&rCmdUpdate, sizeof(rCmdUpdate));
	kalMemCopy(rCmdUpdate.aucPeerMac, mac, 6);

	if (params->supported_rates != NULL) {

		u4Temp = params->supported_rates_len;
		if (u4Temp > CMD_PEER_UPDATE_SUP_RATE_MAX)
			u4Temp = CMD_PEER_UPDATE_SUP_RATE_MAX;
		kalMemCopy(rCmdUpdate.aucSupRate, params->supported_rates,
			   u4Temp);
		rCmdUpdate.u2SupRateLen = u4Temp;
	}

	/*
	 * In supplicant, only recognize WLAN_EID_QOS 46, not 0xDD WMM
	 * So force to support UAPSD here.
	 */
	rCmdUpdate.UapsdBitmap = 0x0F;	/*params->uapsd_queues; */
	rCmdUpdate.UapsdMaxSp = 0;	/*params->max_sp; */

	rCmdUpdate.u2Capability = params_main->capability;

	if (params_main->ext_capab != NULL) {

		u4Temp = params_main->ext_capab_len;
		if (u4Temp > CMD_PEER_UPDATE_EXT_CAP_MAXLEN)
			u4Temp = CMD_PEER_UPDATE_EXT_CAP_MAXLEN;
		kalMemCopy(rCmdUpdate.aucExtCap, params_main->ext_capab, u4Temp);
		rCmdUpdate.u2ExtCapLen = u4Temp;
	}

	if (params->ht_capa != NULL) {

		rCmdUpdate.rHtCap.u2CapInfo = params->ht_capa->cap_info;
		rCmdUpdate.rHtCap.ucAmpduParamsInfo =
			params->ht_capa->ampdu_params_info;
		rCmdUpdate.rHtCap.u2ExtHtCapInfo =
			params->ht_capa->extended_ht_cap_info;
		rCmdUpdate.rHtCap.u4TxBfCapInfo =
			params->ht_capa->tx_BF_cap_info;
		rCmdUpdate.rHtCap.ucAntennaSelInfo =
			params->ht_capa->antenna_selection_info;
		kalMemCopy(rCmdUpdate.rHtCap.rMCS.arRxMask,
			   params->ht_capa->mcs.rx_mask,
			   sizeof(rCmdUpdate.rHtCap.rMCS.arRxMask));

		rCmdUpdate.rHtCap.rMCS.u2RxHighest =
			params->ht_capa->mcs.rx_highest;
		rCmdUpdate.rHtCap.rMCS.ucTxParams =
			params->ht_capa->mcs.tx_params;
		rCmdUpdate.fgIsSupHt = TRUE;
	}
	/* vht */

	if (params->vht_capa != NULL) {
		/* rCmdUpdate.rVHtCap */
		/* rCmdUpdate.rVHtCap */
	}

	/* update a TDLS peer record */
	/* sanity check */
	if ((params_main->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER)))
		rCmdUpdate.eStaType = STA_TYPE_DLS_PEER;
	rCmdUpdate.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, cnmPeerUpdate, &rCmdUpdate,
			   sizeof(struct CMD_PEER_UPDATE), FALSE, FALSE, TRUE,
			   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;
	/* for Ch Sw AP prohibit case */
	if (prBssInfo->fgTdlsIsChSwProhibited) {
		/* disable TDLS ch sw function */

		rStatus = kalIoctl(prGlueInfo,
				   TdlsSendChSwControlCmd,
				   &TdlsSendChSwControlCmd,
				   sizeof(struct CMD_TDLS_CH_SW),
				   FALSE, FALSE, TRUE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}

	return 0;
}
#else
int
mtk_cfg80211_change_station(struct wiphy *wiphy,
			    struct net_device *ndev, u8 *mac,
			    struct station_parameters *params)
{

	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_UPDATE rCmdUpdate;
	uint32_t rStatus;
	uint32_t u4BufLen, u4Temp;
	struct ADAPTER *prAdapter;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);
	/* make up command */

	prAdapter = prGlueInfo->prAdapter;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		ucBssIndex);
	if (!prBssInfo)
		return -EINVAL;

	if (params == NULL)
		return 0;
	else if (params->supported_rates == NULL)
		return 0;

	/* init */
	kalMemZero(&rCmdUpdate, sizeof(rCmdUpdate));
	kalMemCopy(rCmdUpdate.aucPeerMac, mac, 6);

	if (params->supported_rates != NULL) {

		u4Temp = params->supported_rates_len;
		if (u4Temp > CMD_PEER_UPDATE_SUP_RATE_MAX)
			u4Temp = CMD_PEER_UPDATE_SUP_RATE_MAX;
		kalMemCopy(rCmdUpdate.aucSupRate, params->supported_rates,
			   u4Temp);
		rCmdUpdate.u2SupRateLen = u4Temp;
	}

	/*
	 * In supplicant, only recognize WLAN_EID_QOS 46, not 0xDD WMM
	 * So force to support UAPSD here.
	 */
	rCmdUpdate.UapsdBitmap = 0x0F;	/*params->uapsd_queues; */
	rCmdUpdate.UapsdMaxSp = 0;	/*params->max_sp; */

	rCmdUpdate.u2Capability = params->capability;

	if (params->ext_capab != NULL) {

		u4Temp = params->ext_capab_len;
		if (u4Temp > CMD_PEER_UPDATE_EXT_CAP_MAXLEN)
			u4Temp = CMD_PEER_UPDATE_EXT_CAP_MAXLEN;
		kalMemCopy(rCmdUpdate.aucExtCap, params->ext_capab, u4Temp);
		rCmdUpdate.u2ExtCapLen = u4Temp;
	}

	if (params->ht_capa != NULL) {

		rCmdUpdate.rHtCap.u2CapInfo = params->ht_capa->cap_info;
		rCmdUpdate.rHtCap.ucAmpduParamsInfo =
			params->ht_capa->ampdu_params_info;
		rCmdUpdate.rHtCap.u2ExtHtCapInfo =
			params->ht_capa->extended_ht_cap_info;
		rCmdUpdate.rHtCap.u4TxBfCapInfo =
			params->ht_capa->tx_BF_cap_info;
		rCmdUpdate.rHtCap.ucAntennaSelInfo =
			params->ht_capa->antenna_selection_info;
		kalMemCopy(rCmdUpdate.rHtCap.rMCS.arRxMask,
			   params->ht_capa->mcs.rx_mask,
			   sizeof(rCmdUpdate.rHtCap.rMCS.arRxMask));

		rCmdUpdate.rHtCap.rMCS.u2RxHighest =
			params->ht_capa->mcs.rx_highest;
		rCmdUpdate.rHtCap.rMCS.ucTxParams =
			params->ht_capa->mcs.tx_params;
		rCmdUpdate.fgIsSupHt = TRUE;
	}
	/* vht */

	if (params->vht_capa != NULL) {
		/* rCmdUpdate.rVHtCap */
		/* rCmdUpdate.rVHtCap */
	}

	/* update a TDLS peer record */
	/* sanity check */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER)))
		rCmdUpdate.eStaType = STA_TYPE_DLS_PEER;
	rCmdUpdate.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, cnmPeerUpdate, &rCmdUpdate,
			   sizeof(struct CMD_PEER_UPDATE), FALSE, FALSE, TRUE,
			   /* FALSE,    //6628 -> 6630  fgIsP2pOid-> x */
			   &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;
	/* for Ch Sw AP prohibit case */
	if (prBssInfo->fgTdlsIsChSwProhibited) {
		/* disable TDLS ch sw function */

		rStatus = kalIoctl(prGlueInfo,
				   TdlsSendChSwControlCmd,
				   &TdlsSendChSwControlCmd,
				   sizeof(struct CMD_TDLS_CH_SW),
				   FALSE, FALSE, TRUE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for adding a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_add_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     const u8 *mac, struct station_parameters *params)
{
	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_ADD rCmdCreate;
	struct ADAPTER *prAdapter;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	/* init */
	kalMemZero(&rCmdCreate, sizeof(rCmdCreate));
	kalMemCopy(rCmdCreate.aucPeerMac, mac, 6);

	/* create a TDLS peer record */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER))) {
		rCmdCreate.eStaType = STA_TYPE_DLS_PEER;
		rCmdCreate.ucBssIdx = ucBssIndex;
		rStatus = kalIoctl(prGlueInfo, cnmPeerAdd, &rCmdCreate,
				   sizeof(struct CMD_PEER_ADD),
				   FALSE, FALSE, TRUE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}

	return 0;
}
#else
int mtk_cfg80211_add_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac,
			     struct station_parameters *params)
{
	/* return 0; */

	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */
	struct GLUE_INFO *prGlueInfo = NULL;
	struct CMD_PEER_ADD rCmdCreate;
	struct ADAPTER *prAdapter;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	/* make up command */

	prAdapter = prGlueInfo->prAdapter;

	/* init */
	kalMemZero(&rCmdCreate, sizeof(rCmdCreate));
	kalMemCopy(rCmdCreate.aucPeerMac, mac, 6);

	/* create a TDLS peer record */
	if ((params->sta_flags_set & BIT(
		     NL80211_STA_FLAG_TDLS_PEER))) {
		rCmdCreate.eStaType = STA_TYPE_DLS_PEER;
		rCmdCreate.ucBssIdx = ucBssIndex;
		rStatus = kalIoctl(prGlueInfo, cnmPeerAdd, &rCmdCreate,
				   sizeof(struct CMD_PEER_ADD),
				   FALSE, FALSE, TRUE,
				   /* FALSE, //6628 -> 6630  fgIsP2pOid-> x */
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for deleting a station information
 *
 * @param
 *
 * @retval 0:       successful
 *         others:  failure
 *
 * @other
 *		must implement if you have add_station().
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev,
			     struct station_del_parameters *params)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	const u8 *mac = params->mac ? params->mac : bcast_addr;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	prAdapter = prGlueInfo->prAdapter;

	/* For kernel 3.18 modification, we trasfer to local buff to query
	 * sta
	 */
	memset(deleteMac, 0, MAC_ADDR_LEN);
	memcpy(deleteMac, mac, MAC_ADDR_LEN);

	prStaRec = cnmGetStaRecByAddress(prAdapter,
		 (uint8_t) ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev, const u8 *mac)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	u8 deleteMac[MAC_ADDR_LEN];
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	prAdapter = prGlueInfo->prAdapter;

	/* For kernel 3.18 modification, we trasfer to local buff to query
	 * sta
	 */
	memset(deleteMac, 0, MAC_ADDR_LEN);
	memcpy(deleteMac, mac, MAC_ADDR_LEN);

	prStaRec = cnmGetStaRecByAddress(prAdapter,
		 (uint8_t) ucBssIndex, deleteMac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#endif
#else
int mtk_cfg80211_del_station(struct wiphy *wiphy,
			     struct net_device *ndev, u8 *mac)
{
	/* fgIsTDLSlinkEnable = 0; */

	/* return 0; */
	/* from supplicant -- wpa_supplicant_tdls_peer_addset() */

	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	prAdapter = prGlueInfo->prAdapter;

	prStaRec = cnmGetStaRecByAddress(prAdapter,
			 (uint8_t) ucBssIndex, mac);

	if (prStaRec != NULL)
		cnmStaRecFree(prAdapter, prStaRec);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to transmit a TDLS data frame from nl80211.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 * \param[in]
 * \param[in]
 * \param[in] buf includes RSN IE + FT IE + Lifetimeout IE
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       bool initiator, const u8 *buf, size_t len)
{
	struct GLUE_INFO *prGlueInfo;
	struct TDLS_CMD_LINK_MGT rCmdMgt;
	uint32_t u4BufLen;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);

	if  (len > TDLS_SEC_BUF_LENGTH) {
		DBGLOG(REQ, WARN, "%s:len > TDLS_SEC_BUF_LENGTH\n", __func__);
		return -EINVAL;
	}

	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);
	rCmdMgt.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt,
		 sizeof(struct TDLS_CMD_LINK_MGT), FALSE, TRUE, FALSE,
		 &u4BufLen);

	DBGLOG(REQ, INFO, "rStatus: %x", rStatus);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;
	else
		return -EINVAL;
}
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       const u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, u32 peer_capability,
		       const u8 *buf, size_t len)
{
	struct GLUE_INFO *prGlueInfo;
	struct TDLS_CMD_LINK_MGT rCmdMgt;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);
	kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);
	rCmdMgt.ucBssIdx = ucBssIndex;
	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt,
		 sizeof(struct TDLS_CMD_LINK_MGT), FALSE, TRUE, FALSE,
		 &u4BufLen);
	return 0;

}

#else
int
mtk_cfg80211_tdls_mgmt(struct wiphy *wiphy,
		       struct net_device *dev,
		       u8 *peer, u8 action_code, u8 dialog_token,
		       u16 status_code, const u8 *buf, size_t len)
{
	struct GLUE_INFO *prGlueInfo;
	struct TDLS_CMD_LINK_MGT rCmdMgt;
	uint32_t u4BufLen;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	/* sanity check */
	if ((wiphy == NULL) || (peer == NULL) || (buf == NULL))
		return -EINVAL;

	/* init */
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (prGlueInfo == NULL)
		return -EINVAL;

	kalMemZero(&rCmdMgt, sizeof(rCmdMgt));
	rCmdMgt.u2StatusCode = status_code;
	rCmdMgt.u4SecBufLen = len;
	rCmdMgt.ucDialogToken = dialog_token;
	rCmdMgt.ucActionCode = action_code;
	kalMemCopy(&(rCmdMgt.aucPeer), peer, 6);
	if	(len > TDLS_SEC_BUF_LENGTH)
		DBGLOG(REQ, WARN,
		       "In mtk_cfg80211_tdls_mgmt , len > TDLS_SEC_BUF_LENGTH, please check\n");
	else
		kalMemCopy(&(rCmdMgt.aucSecBuf), buf, len);
	rCmdMgt.ucBssIdx = ucBssIndex;
	kalIoctl(prGlueInfo, TdlsexLinkMgt, &rCmdMgt,
		 sizeof(struct TDLS_CMD_LINK_MGT), FALSE, TRUE, FALSE,
		 &u4BufLen);
	return 0;

}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to hadel TDLS link from nl80211.
 *
 * \param[in] pvAdapter Pointer to the Adapter structure.
 * \param[in]
 * \param[in]
 * \param[in] buf includes RSN IE + FT IE + Lifetimeout IE
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy,
			   struct net_device *dev,
			   const u8 *peer, enum nl80211_tdls_operation oper)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen;
	struct ADAPTER *prAdapter;
	struct TDLS_CMD_LINK_OPER rCmdOper;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	DBGLOG(REQ, INFO, "ucBssIndex = %d, oper=%d",
		ucBssIndex, oper);

	ASSERT(prGlueInfo);
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper =  (enum ENUM_TDLS_LINK_OPER)oper;
	rCmdOper.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper,
			sizeof(struct TDLS_CMD_LINK_OPER), FALSE, FALSE, FALSE,
			&u4BufLen);

	DBGLOG(REQ, INFO, "rStatus: %x", rStatus);

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;
	else
		return -EINVAL;
}
#else
int mtk_cfg80211_tdls_oper(struct wiphy *wiphy,
			   struct net_device *dev, u8 *peer,
			   enum nl80211_tdls_operation oper)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen;
	struct ADAPTER *prAdapter;
	struct TDLS_CMD_LINK_OPER rCmdOper;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(REQ, INFO, "ucBssIndex = %d, oper=%d",
		ucBssIndex, oper);

	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rCmdOper, sizeof(rCmdOper));
	kalMemCopy(rCmdOper.aucPeerMac, peer, 6);

	rCmdOper.oper =  (enum ENUM_TDLS_LINK_OPER)oper;
	rCmdOper.ucBssIdx = ucBssIndex;

	kalIoctl(prGlueInfo, TdlsexLinkOper, &rCmdOper,
			sizeof(struct TDLS_CMD_LINK_OPER), FALSE, FALSE, FALSE,
			&u4BufLen);
	return 0;
}
#endif
#endif

int32_t mtk_cfg80211_process_str_cmd(struct GLUE_INFO
				     *prGlueInfo,
		struct wireless_dev *wdev,
		uint8_t *cmd, int32_t len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4SetInfoLen = 0;
	uint8_t ucBssIndex = 0;

	ucBssIndex = wlanGetBssIdx(wdev->netdev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	if (strnicmp(cmd, "tdls-ps ", 8) == 0) {
#if CFG_SUPPORT_TDLS
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidDisableTdlsPs,
				   (void *)(cmd + 8), 1,
				   FALSE, FALSE, TRUE, &u4SetInfoLen);
#else
		DBGLOG(REQ, WARN, "not support tdls\n");
		return -EOPNOTSUPP;
#endif
	} else if (strncasecmp(cmd, "NEIGHBOR-REQUEST", 16) == 0) {
		uint8_t *pucSSID = NULL;
		uint32_t u4SSIDLen = 0;

		if (len > 16 && (strncasecmp(cmd+16, " SSID=", 6) == 0)) {
			pucSSID = cmd + 22;
			u4SSIDLen = len - 22;
			DBGLOG(REQ, INFO, "cmd=%s, ssid len %u, ssid=%s\n", cmd,
			       u4SSIDLen, pucSSID);
		}
		rStatus = kalIoctlByBssIdx(prGlueInfo,
				   wlanoidSendNeighborRequest,
				   (void *)pucSSID, u4SSIDLen, FALSE, FALSE,
				   TRUE, &u4SetInfoLen,
				   ucBssIndex);
	} else if (strncasecmp(cmd, "BSS-TRANSITION-QUERY", 20) == 0) {
		uint8_t *pucReason = NULL;

		if (len > 20 && (strncasecmp(cmd+20, " reason=", 8) == 0))
			pucReason = cmd + 28;
		if ((pucReason == NULL) || (strlen(pucReason) > 3)) {
			DBGLOG(REQ, ERROR, "ERR: BTM query wrong reason!\r\n");
			return -EFAULT;
		}
		rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidSendBTMQuery,
				   (void *)pucReason, strlen(pucReason),
				   FALSE, FALSE, TRUE,
				   &u4SetInfoLen,
				   ucBssIndex);
	} else if (strnicmp(cmd, "OSHAREMOD ", 10) == 0) {
#if CFG_SUPPORT_OSHARE
		struct OSHARE_MODE_T cmdBuf;
		struct OSHARE_MODE_T *pCmdHeader = NULL;
		struct OSHARE_MODE_SETTING_V1_T *pCmdData = NULL;

		kalMemZero(&cmdBuf, sizeof(cmdBuf));

		pCmdHeader = &cmdBuf;
		pCmdHeader->cmdVersion = OSHARE_MODE_CMD_V1;
		pCmdHeader->cmdType = 1; /*1-set   0-query*/
		pCmdHeader->magicCode = OSHARE_MODE_MAGIC_CODE;
		pCmdHeader->cmdBufferLen = MAX_OSHARE_MODE_LENGTH;

		pCmdData = (struct OSHARE_MODE_SETTING_V1_T *) &
			   (pCmdHeader->buffer[0]);
		pCmdData->osharemode = *(uint8_t *)(cmd + 10) - '0';

		DBGLOG(REQ, INFO, "cmd=%s, osharemode=%u\n", cmd,
		       pCmdData->osharemode);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetOshareMode,
				   &cmdBuf,
				   sizeof(struct OSHARE_MODE_T),
				   FALSE,
				   FALSE,
				   TRUE,
				   &u4SetInfoLen);

		if (rStatus == WLAN_STATUS_SUCCESS)
			prGlueInfo->prAdapter->fgEnOshareMode
				= pCmdData->osharemode;
#else
		DBGLOG(REQ, WARN, "not support OSHAREMOD\n");
		return -EOPNOTSUPP;
#endif
	} else
		return -EOPNOTSUPP;

	if (rStatus == WLAN_STATUS_SUCCESS)
		return 0;

	return -EINVAL;
}

#if (CFG_SUPPORT_SINGLE_SKU == 1)

#if (CFG_BUILT_IN_DRIVER == 1)
/* in kernel-x.x/net/wireless/reg.c */
#else
bool is_world_regdom(const char *alpha2)
{
	if (!alpha2)
		return false;

	return (alpha2[0] == '0') && (alpha2[1] == '0');
}
#endif

void
mtk_reg_notify(IN struct wiphy *pWiphy,
               IN struct regulatory_request *pRequest)
{
    struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)wiphy_priv(pWiphy);

    if (!prGlueInfo || !prGlueInfo->prAdapter)
        return;

    /* * RE-FANGED: If the kernel tries to set '00', we intercept and 
     * tell the kernel "No, this hardware is strictly US."
     */
    if (pRequest->alpha2[0] == '0' && pRequest->alpha2[1] == '0') {
        DBGLOG(RLM, INFO, "RE-FANGED: Intercepting '00' request. Asserting Driver Hint: US\n");
        regulatory_hint(pWiphy, "US");
        return; 
    }

    DBGLOG(RLM, INFO, "RE-FANGED: Passing through regdom request alpha2=%c%c\n",
           pRequest->alpha2[0], pRequest->alpha2[1]);

    /* This keeps the firmware and internal state in sync */
    rlmDomainCountryCodeUpdate(prGlueInfo->prAdapter, pWiphy, 0x5553);
}

void
cfg80211_regd_set_wiphy(IN struct wiphy *prWiphy)
{
	/*
	 * register callback
	 */
	prWiphy->reg_notifier = mtk_reg_notify;


	/*
	 * clear REGULATORY_CUSTOM_REG flag
	 */
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	/*tells kernel that assign WW as default*/
	prWiphy->flags &= ~(WIPHY_FLAG_CUSTOM_REGULATORY);
#else
	prWiphy->regulatory_flags &= ~(REGULATORY_CUSTOM_REG);

	/*ignore the hint from IE*/
	prWiphy->regulatory_flags |= REGULATORY_COUNTRY_IE_IGNORE;

#ifdef CFG_SUPPORT_DISABLE_BCN_HINTS
	/*disable beacon hint to avoid channel flag be changed*/
	prWiphy->regulatory_flags |= REGULATORY_DISABLE_BEACON_HINTS;
#endif
#endif


	/*
	 * set REGULATORY_CUSTOM_REG flag
	 */
#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
#if KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE
	/*tells kernel that assign WW as default*/
	prWiphy->flags |= (WIPHY_FLAG_CUSTOM_REGULATORY);
#else
	prWiphy->regulatory_flags |= (REGULATORY_CUSTOM_REG);
#endif
	/* assigned a defautl one */
	if (rlmDomainGetLocalDefaultRegd())
		wiphy_apply_custom_regulatory(prWiphy,
					      rlmDomainGetLocalDefaultRegd());
#endif


	/*
	 * Initialize regd control information
	 */
	rlmDomainResetCtrlInfo(FALSE);
}
#else
void
cfg80211_regd_set_wiphy(IN struct wiphy *prWiphy)
{
}
#endif

int mtk_cfg80211_suspend(struct wiphy *wiphy,
			 struct cfg80211_wowlan *wow)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WIFI_VAR *prWifiVar = NULL;
#if !CFG_ENABLE_WAKE_LOCK
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen;
	GLUE_SPIN_LOCK_DECLARATION();
#endif

	DBGLOG(REQ, INFO, "mtk_cfg80211_suspend\n");

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted() || !wiphy)
		goto end;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if (prGlueInfo && prGlueInfo->prAdapter) {
		prWifiVar = &prGlueInfo->prAdapter->rWifiVar;

#if !CFG_ENABLE_WAKE_LOCK

		if (IS_FEATURE_ENABLED(prWifiVar->ucWow)) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
			if (prGlueInfo->prScanRequest) {
				kalCfg80211ScanDone(prGlueInfo->prScanRequest, TRUE);
				prGlueInfo->prScanRequest = NULL;
			}
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

			/* AIS flow: disassociation if wow_en=0 */
			DBGLOG(REQ, INFO, "Enter AIS pre-suspend\n");
			rStatus = kalIoctl(prGlueInfo, wlanoidAisPreSuspend, NULL, 0,
						TRUE, FALSE, TRUE, &u4BufLen);

			/* TODO: p2pProcessPreSuspendFlow
			 * In current design, only support AIS connection during suspend only.
			 * It need to add flow to deactive P2P (GC/GO) link during suspend flow.
			 * Otherwise, MT7668 would fail to enter deep sleep.
			 */
		}
#endif

#if CFG_SUPPORT_PROC_GET_WAKEUP_REASON
		prGlueInfo->prAdapter->rWowCtrl.ucReason
				= INVALID_WOW_WAKE_UP_REASON;
#endif

		set_bit(SUSPEND_FLAG_FOR_WAKEUP_REASON,
			&prGlueInfo->prAdapter->ulSuspendFlag);
		set_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
			&prGlueInfo->prAdapter->ulSuspendFlag);
	}
end:
	kalHaltUnlock();

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief cfg80211 resume callback, will be invoked in wiphy_resume.
 *
 * @param wiphy: pointer to wiphy
 *
 * @retval 0:       successful
 *         others:  failure
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_resume(struct wiphy *wiphy)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct BSS_DESC **pprBssDesc = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t i = 0;

	DBGLOG(REQ, INFO, "mtk_cfg80211_resume\n");

	if (kalHaltTryLock())
		return 0;

	if (kalIsHalted() || !wiphy)
		goto end;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if (prGlueInfo)
		prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		goto end;

	clear_bit(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
		  &prAdapter->ulSuspendFlag);
	pprBssDesc = &prAdapter->rWifiVar.rScanInfo.rSchedScanParam.
		     aprPendingBssDescToInd[0];
	for (; i < SCN_SSID_MATCH_MAX_NUM; i++) {
		if (pprBssDesc[i] == NULL)
			break;
		if (pprBssDesc[i]->u2RawLength == 0)
			continue;
		kalIndicateBssInfo(prGlueInfo,
				   (uint8_t *) pprBssDesc[i]->aucRawBuf,
				   pprBssDesc[i]->u2RawLength,
				   pprBssDesc[i]->ucChannelNum,
#if (CFG_SUPPORT_WIFI_6G == 1)
				   pprBssDesc[i]->eBand,
#endif
				   RCPI_TO_dBm(pprBssDesc[i]->ucRCPI));
	}
	DBGLOG(SCN, INFO, "pending %d sched scan results\n", i);
	if (i > 0)
		kalMemZero(&pprBssDesc[0], i * sizeof(struct BSS_DESC *));

end:
	kalHaltUnlock();

	return 0;
}

#if CFG_ENABLE_UNIFY_WIPHY
/*----------------------------------------------------------------------------*/
/*!
 * @brief Check the net device is P2P net device (P2P GO/GC, AP), or not.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : the net device
 *
 * @retval 0:  AIS device (STA/IBSS)
 *         1:  P2P GO/GC, AP
 */
/*----------------------------------------------------------------------------*/
int mtk_IsP2PNetDevice(struct GLUE_INFO *prGlueInfo,
		       struct net_device *ndev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
	int iftype = 0;
	int ret = 1;

	if (ndev == NULL) {
		DBGLOG(REQ, WARN, "ndev is NULL\n");
		return -1;
	}

	prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
			  netdev_priv(ndev);
	iftype = ndev->ieee80211_ptr->iftype;

	/* P2P device/GO/GC always return 1 */
	if (prNetDevPrivate->ucIsP2p == TRUE)
		ret = 1;
	else if (iftype == NL80211_IFTYPE_STATION)
		ret = 0;
	else if (iftype == NL80211_IFTYPE_ADHOC)
		ret = 0;

	DBGLOG(REQ, LOUD,
		"ucIsP2p = %d\n",
		ret);

	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialize the AIS related FSM and data.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : the net device
 *        ucBssIdx   : the AIS BSS index adssigned by the driver (wlanProbe)
 *
 * @retval 0
 *
 */
/*----------------------------------------------------------------------------*/
int mtk_init_sta_role(struct ADAPTER *prAdapter,
		      struct net_device *ndev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNdevPriv = NULL;
	uint8_t ucBssIndex = 0;

	if ((prAdapter == NULL) || (ndev == NULL))
		return -1;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex))
		return -1;

	/* init AIS FSM */
	aisFsmInit(prAdapter, ucBssIndex);

#if CFG_SUPPORT_ROAMING
	/* Roaming Module - intiailization */
	roamingFsmInit(prAdapter, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

	ndev->netdev_ops = wlanGetNdevOps();
	ndev->ieee80211_ptr->iftype = NL80211_IFTYPE_STATION;

	/* set the ndev's ucBssIdx to the AIS BSS index */
	prNdevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		     netdev_priv(ndev);
	prNdevPriv->ucBssIdx = ucBssIndex;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Uninitialize the AIS related FSM and data.
 *
 * @param prAdapter : the driver private data
 *
 * @retval 0
 *
 */
/*----------------------------------------------------------------------------*/
int mtk_uninit_sta_role(struct ADAPTER *prAdapter,
			struct net_device *ndev)
{
	struct NETDEV_PRIVATE_GLUE_INFO *prNdevPriv = NULL;
	uint8_t ucBssIndex = 0;

	if ((prAdapter == NULL) || (ndev == NULL))
		return -1;

	ucBssIndex = wlanGetBssIdx(ndev);
	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIndex))
		return -1;

#if CFG_SUPPORT_ROAMING
	/* Roaming Module - unintiailization */
	roamingFsmUninit(prAdapter, ucBssIndex);
#endif /* CFG_SUPPORT_ROAMING */

	/* uninit AIS FSM */
	aisFsmUninit(prAdapter, ucBssIndex);

	/* set the ucBssIdx to the illegal value */
	prNdevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		     netdev_priv(ndev);
	prNdevPriv->ucBssIdx = 0xff;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Initialize the AP (P2P) related FSM and data.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : net device
 *
 * @retval 0      : success
 *         others : can't alloc and setup the AP FSM & data
 *
 */
/*----------------------------------------------------------------------------*/
int mtk_init_ap_role(struct GLUE_INFO *prGlueInfo,
		     struct net_device *ndev)
{
	int u4Idx = 0;
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;

	for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
		if (gprP2pRoleWdev[u4Idx] == NULL)
			break;
	}

	if (u4Idx >= KAL_P2P_NUM) {
		DBGLOG(INIT, ERROR, "There is no free gprP2pRoleWdev.\n");
		return -ENOMEM;
	}

	if ((u4Idx == 0) ||
	    (prAdapter == NULL) ||
	    (prAdapter->rP2PNetRegState !=
	     ENUM_NET_REG_STATE_REGISTERED)) {
		DBGLOG(INIT, ERROR,
		       "The wlan0 can't set to AP without p2p0\n");
		/* System will crash, if p2p0 isn't existing. */
		return -EFAULT;
	}

	/* reference from the glRegisterP2P() */
	gprP2pRoleWdev[u4Idx] = ndev->ieee80211_ptr;
	if (glSetupP2P(prGlueInfo, gprP2pRoleWdev[u4Idx], ndev,
		       u4Idx, TRUE)) {
		gprP2pRoleWdev[u4Idx] = NULL;
		return -EFAULT;
	}

	prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum++;

	/* reference from p2pNetRegister() */
	/* The ndev doesn't need register_netdev, only reassign the gPrP2pDev.*/
	gPrP2pDev[u4Idx] = ndev;

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief Unnitialize the AP (P2P) related FSM and data.
 *
 * @param prGlueInfo : the driver private data
 *        ndev       : net device
 *
 * @retval 0      : success
 *         others : can't find the AP information by the ndev
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
mtk_oid_uninit_ap_role(struct ADAPTER *prAdapter, void *pvSetBuffer,
	uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	unsigned char u4Idx = 0;

	if ((prAdapter == NULL) || (pvSetBuffer == NULL)
		|| (pu4SetInfoLen == NULL))
		return WLAN_STATUS_FAILURE;

	/* init */
	*pu4SetInfoLen = sizeof(unsigned char);
	if (u4SetBufferLen < sizeof(unsigned char))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4Idx = *(unsigned char *) pvSetBuffer;

	DBGLOG(INIT, INFO, "ucRoleIdx = %d\n", u4Idx);

	glUnregisterP2P(prAdapter->prGlueInfo, u4Idx);

	gPrP2pDev[u4Idx] = NULL;
	gprP2pRoleWdev[u4Idx] = NULL;

	return 0;

}

int mtk_uninit_ap_role(struct GLUE_INFO *prGlueInfo,
		       struct net_device *ndev)
{
	unsigned char u4Idx;
	uint32_t rStatus;
	uint32_t u4BufLen;

	if (!prGlueInfo) {
		DBGLOG(INIT, WARN, "prGlueInfo is NULL\n");
		return -EINVAL;
	}
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &u4Idx) != 0) {
		DBGLOG(INIT, WARN,
		       "can't find the matched dev to uninit AP\n");
		return -EFAULT;
	}

	rStatus = kalIoctl(prGlueInfo,
				mtk_oid_uninit_ap_role, &u4Idx,
				sizeof(unsigned char),
				FALSE, FALSE, FALSE,
				&u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	return 0;
}


#if (CFG_SUPPORT_DFS_MASTER == 1)
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_start_radar_detection(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_chan_def *chandef,
				  unsigned int cac_time_ms
#if KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
				  ,int chain_mask
#endif //KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}
	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}
	return mtk_p2p_cfg80211_start_radar_detection(wiphy,
						      dev,
						      chandef,
						      cac_time_ms
#if KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
						      ,chain_mask
#endif //KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
	);
}
#else //KERNEL_VERSION(3, 15, 0) > CFG80211_VERSION_CODE
int mtk_cfg_start_radar_detection(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct cfg80211_chan_def *chandef
#if KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
				  ,int chain_mask
#endif //KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}
	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}
	return mtk_p2p_cfg80211_start_radar_detection(wiphy,
						      dev,
						      chandef
#if KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
						      ,chain_mask
#endif //KERNEL_VERSION(6, 11, 0) <= CFG80211_VERSION_CODE
	);
}
#endif //KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_channel_switch(struct wiphy *wiphy,
			   struct net_device *dev,
			   struct cfg80211_csa_settings *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}
	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}
	return mtk_p2p_cfg80211_channel_switch(wiphy, dev, params);
}
#endif //KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
#endif //CFG_SUPPORT_DFS_MASTER



#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       unsigned char name_assign_type,
				       enum nl80211_iftype type,
				       struct vif_params *params)
#elif KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       unsigned char name_assign_type,
				       enum nl80211_iftype type,
				       u32 *flags,
				       struct vif_params *params)
#else
struct wireless_dev *mtk_cfg_add_iface(struct wiphy *wiphy,
				       const char *name,
				       enum nl80211_iftype type,
				       u32 *flags,
				       struct vif_params *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	u32 *flags = NULL;
#endif

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return ERR_PTR(-EFAULT);
	}

	/* TODO: error handele for the non-P2P interface */

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 == 0)
	DBGLOG(REQ, WARN, "P2P is not supported\n");
	return ERR_PTR(-EINVAL);
#else	/* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
#if KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
	return mtk_p2p_cfg80211_add_iface(wiphy, name,
					  name_assign_type, type,
					  flags, params);
#else	/* KERNEL_VERSION > (4, 1, 0) */
	return mtk_p2p_cfg80211_add_iface(wiphy, name, type, flags,
					  params);
#endif	/* KERNEL_VERSION */
#endif  /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
}

int mtk_cfg_del_iface(struct wiphy *wiphy,
		      struct wireless_dev *wdev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	/* TODO: error handele for the non-P2P interface */
#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 == 0)
	DBGLOG(REQ, WARN, "P2P is not supported\n");
	return -EINVAL;
#else	/* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
	return mtk_p2p_cfg80211_del_iface(wiphy, wdev);
#endif  /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_change_iface(struct wiphy *wiphy,
			 struct net_device *ndev,
			 enum nl80211_iftype type,
			 struct vif_params *params)
#else
int mtk_cfg_change_iface(struct wiphy *wiphy,
			 struct net_device *ndev,
			 enum nl80211_iftype type, u32 *flags,
			 struct vif_params *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetdevPriv = NULL;
	struct P2P_INFO *prP2pInfo = NULL;
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	u32 *flags = NULL;
#endif
#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
	uint8_t ucBandIdx = 0;
#endif
	GLUE_SPIN_LOCK_DECLARATION();

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	DBGLOG(P2P, INFO, "ndev=%p, new type=%d\n", ndev, type);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (!ndev) {
		DBGLOG(REQ, WARN, "ndev is NULL\n");
		return -EINVAL;
	}

	prNetdevPriv = (struct NETDEV_PRIVATE_GLUE_INFO *)
		       netdev_priv(ndev);

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211)
	/* for p2p0(GO/GC) & ap0(SAP): mtk_p2p_cfg80211_change_iface
	 * for wlan0 (STA/SAP): the following mtk_cfg_change_iface process
	 */
	if (!wlanIsAisDev(ndev)) {
		return mtk_p2p_cfg80211_change_iface(wiphy, ndev, type,
						     flags, params);
	}
#endif /* CFG_ENABLE_WIFI_DIRECT_CFG_80211 */

	prAdapter = prGlueInfo->prAdapter;

	if (ndev->ieee80211_ptr->iftype == type) {
		DBGLOG(REQ, INFO, "ndev type is not changed (%d)\n", type);
		return 0;
	}

	netif_carrier_off(ndev);
	/* stop ap will stop all queue, and kalIndicateStatusAndComplete only do
	 * netif_carrier_on. So that, the following STA can't send 4-way M2 to
	 * AP.
	 */
	netif_tx_start_all_queues(ndev);

	/* flush scan */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	if ((prGlueInfo->prScanRequest != NULL) &&
	    (prGlueInfo->prScanRequest->wdev == ndev->ieee80211_ptr)) {
		kalCfg80211ScanDone(prGlueInfo->prScanRequest, TRUE);
		prGlueInfo->prScanRequest = NULL;
	}
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);

	/* expect that only AP & STA will be handled here (excluding IBSS) */

	if (type == NL80211_IFTYPE_AP) {
		/* STA mode change to AP mode */
		prP2pInfo = prAdapter->prP2pInfo;

		if (prP2pInfo == NULL) {
			DBGLOG(INIT, ERROR, "prP2pInfo is NULL\n");
			return -EFAULT;
		}

		if (prP2pInfo->u4DeviceNum >= KAL_P2P_NUM) {
			DBGLOG(INIT, ERROR, "resource invalid, u4DeviceNum=%d\n"
			       , prP2pInfo->u4DeviceNum);
			return -EFAULT;
		}

		mtk_uninit_sta_role(prAdapter, ndev);

		if (mtk_init_ap_role(prGlueInfo, ndev) != 0) {
			DBGLOG(INIT, ERROR, "mtk_init_ap_role FAILED\n");

			/* Only AP/P2P resource has the failure case.	*/
			/* So, just re-init AIS.			*/
			mtk_init_sta_role(prAdapter, ndev);
			return -EFAULT;
		}
#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
	} else if (type == NL80211_IFTYPE_MONITOR) {
		ndev->type = ARPHRD_IEEE80211_RADIOTAP;
		ndev->ieee80211_ptr->iftype = NL80211_IFTYPE_MONITOR;
		prGlueInfo->fgIsEnableMon = TRUE;
		prGlueInfo->ucBandIdx = 0;
		prGlueInfo->fgDropFcsErrorFrame = TRUE;
		prGlueInfo->u2Aid = 0;
		prGlueInfo->u4AmpduRefNum = 0;

		for (ucBandIdx = 0; ucBandIdx < CFG_MONITOR_BAND_NUM;
		     ucBandIdx++) {
			prGlueInfo->aucBandIdxEn[ucBandIdx] = 0;
		}
#endif
	} else {
#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
		if (prGlueInfo->fgIsEnableMon) {
			prGlueInfo->fgIsEnableMon = FALSE;

			for (ucBandIdx = 0; ucBandIdx < CFG_MONITOR_BAND_NUM;
			     ucBandIdx++) {
				if (prGlueInfo->aucBandIdxEn[ucBandIdx]) {
					prGlueInfo->ucBandIdx = ucBandIdx;
					mtk_cfg80211_set_monitor_channel(wiphy,
									 NULL);
				}
			}
			ndev->type = ARPHRD_ETHER;
		} else
#endif
		/* AP mode change to STA mode */
		if (mtk_uninit_ap_role(prGlueInfo, ndev) != 0) {
			DBGLOG(INIT, ERROR, "mtk_uninit_ap_role FAILED\n");
			return -EFAULT;
		}

		mtk_init_sta_role(prAdapter, ndev);

		/* continue the mtk_cfg80211_change_iface() process */
		mtk_cfg80211_change_iface(wiphy, ndev, type, flags, params);
	}

	return 0;
}

int mtk_cfg_add_key(struct wiphy *wiphy,
		    struct net_device *ndev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
                     int link_id,
#endif
					 u8 key_index,
		    bool pairwise, const u8 *mac_addr,
		    struct key_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_add_key(wiphy, ndev, key_index,
						pairwise, mac_addr, params);
	}
	/* STA Mode */
	return mtk_cfg80211_add_key(wiphy, ndev, key_index,
				    pairwise,
				    mac_addr, params);
}

int mtk_cfg_get_key(struct wiphy *wiphy,
		    struct net_device *ndev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
                     int link_id,
#endif
					 u8 key_index,
		    bool pairwise, const u8 *mac_addr, void *cookie,
		    void (*callback)(void *cookie, struct key_params *))
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_get_key(wiphy, ndev, key_index,
					pairwise, mac_addr, cookie, callback);
	}
	/* STA Mode */
	return mtk_cfg80211_get_key(wiphy, ndev, key_index,
				    pairwise, mac_addr, cookie, callback);
}

int mtk_cfg_del_key(struct wiphy *wiphy,
		    struct net_device *ndev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
                     int link_id,
#endif
					 u8 key_index,
		    bool pairwise, const u8 *mac_addr)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_del_key(wiphy, ndev, key_index,
						pairwise, mac_addr);
	}
	/* STA Mode */
	return mtk_cfg80211_del_key(wiphy, ndev, key_index,
				    pairwise, mac_addr);
}

int mtk_cfg_set_default_key(struct wiphy *wiphy,
			    struct net_device *ndev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	             int link_id,
#endif
			    u8 key_index, bool unicast, bool multicast)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_set_default_key(wiphy, ndev,
						key_index, unicast, multicast);
	}
	/* STA Mode */
	return mtk_cfg80211_set_default_key(wiphy, ndev,
					    key_index, unicast, multicast);
}

int mtk_cfg_set_default_mgmt_key(struct wiphy *wiphy,
		struct net_device *ndev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
		             int link_id,
#endif
					 u8 key_index)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_set_mgmt_key(wiphy, ndev, key_index);
	/* STA Mode */
	DBGLOG(REQ, WARN, "STA don't support this function\n");
	return -EFAULT;
}

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_get_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac, struct station_info *sinfo)
#else
int mtk_cfg_get_station(struct wiphy *wiphy,
			struct net_device *ndev,
			u8 *mac, struct station_info *sinfo)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_get_station(wiphy, ndev, mac,
						    sinfo);
	/* STA Mode */
	return mtk_cfg80211_get_station(wiphy, ndev, mac, sinfo);
}

#if CFG_SUPPORT_TDLS
#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_change_station(struct wiphy *wiphy,
			   struct net_device *ndev,
			   const u8 *mac, struct station_parameters *params)
#else
int mtk_cfg_change_station(struct wiphy *wiphy,
			   struct net_device *ndev,
			   u8 *mac, struct station_parameters *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}
	/* STA Mode */
	return mtk_cfg80211_change_station(wiphy, ndev, mac,
					   params);
}

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_add_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac, struct station_parameters *params)
#else
int mtk_cfg_add_station(struct wiphy *wiphy,
			struct net_device *ndev,
			u8 *mac, struct station_parameters *params)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}
	/* STA Mode */
	return mtk_cfg80211_add_station(wiphy, ndev, mac, params);
}

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_oper(struct wiphy *wiphy,
		      struct net_device *ndev,
		      const u8 *peer, enum nl80211_tdls_operation oper)
#else
int mtk_cfg_tdls_oper(struct wiphy *wiphy,
		      struct net_device *ndev,
		      u8 *peer, enum nl80211_tdls_operation oper)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}
	/* STA Mode */
	return mtk_cfg80211_tdls_oper(wiphy, ndev, peer, oper);
}

#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev,
		      const u8 *peer,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
              int link_id,
#endif
			  u8 action_code, u8 dialog_token,
		      u16 status_code, u32 peer_capability,
		      bool initiator, const u8 *buf, size_t len)
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev,
		      const u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code, u32 peer_capability,
		      const u8 *buf, size_t len)
#else
int mtk_cfg_tdls_mgmt(struct wiphy *wiphy,
		      struct net_device *dev,
		      u8 *peer, u8 action_code, u8 dialog_token,
		      u16 status_code,
		      const u8 *buf, size_t len)
#endif
{
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

#if KERNEL_VERSION(3, 18, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
			dialog_token, status_code, peer_capability, initiator,
			buf, len);
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
			dialog_token, status_code, peer_capability,
			buf, len);
#else
	return mtk_cfg80211_tdls_mgmt(wiphy, dev, peer, action_code,
				      dialog_token, status_code,
				      buf, len);
#endif
}
#endif /* CFG_SUPPORT_TDLS */

#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev,
			struct station_del_parameters *params)
#elif KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev,
			const u8 *mac)
#else
int mtk_cfg_del_station(struct wiphy *wiphy,
			struct net_device *ndev, u8 *mac)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
		return mtk_p2p_cfg80211_del_station(wiphy, ndev, params);
#else
		return mtk_p2p_cfg80211_del_station(wiphy, ndev, mac);
#endif
	}
	/* STA Mode */
#if CFG_SUPPORT_TDLS
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_del_station(wiphy, ndev, params);
#else	/* CFG80211_VERSION_CODE > KERNEL_VERSION(3, 19, 0) */
	return mtk_cfg80211_del_station(wiphy, ndev, mac);
#endif	/* CFG80211_VERSION_CODE */
#else	/* CFG_SUPPORT_TDLS == 0 */
	/* AIS only support this function when CFG_SUPPORT_TDLS */
	return -EFAULT;
#endif	/* CFG_SUPPORT_TDLS */
}

int mtk_cfg_scan(struct wiphy *wiphy,
		 struct cfg80211_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo,
			       request->wdev->netdev) > 0)
		return mtk_p2p_cfg80211_scan(wiphy, request);
	/* STA Mode */
	return mtk_cfg80211_scan(wiphy, request);
}

#if KERNEL_VERSION(4, 5, 0) <= CFG80211_VERSION_CODE
void mtk_cfg_abort_scan(struct wiphy *wiphy,
			struct wireless_dev *wdev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0)
		mtk_p2p_cfg80211_abort_scan(wiphy, wdev);
	else	/* STA Mode */
		mtk_cfg80211_abort_scan(wiphy, wdev);
}
#endif

#if CFG_SUPPORT_SCHED_SCAN
int mtk_cfg_sched_scan_start(IN struct wiphy *wiphy,
			     IN struct net_device *ndev,
			     IN struct cfg80211_sched_scan_request *request)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_sched_scan_start(wiphy, ndev, request);

}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_sched_scan_stop(IN struct wiphy *wiphy,
			    IN struct net_device *ndev,
			    IN u64 reqid)
#else
int mtk_cfg_sched_scan_stop(IN struct wiphy *wiphy,
			    IN struct net_device *ndev)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return 0;
	}
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
	return mtk_cfg80211_sched_scan_stop(wiphy, ndev, reqid);
#else
	return mtk_cfg80211_sched_scan_stop(wiphy, ndev);
#endif
}
#endif /* CFG_SUPPORT_SCHED_SCAN */

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
int mtk_cfg_auth(struct wiphy *wiphy, struct net_device *ndev,
		    struct cfg80211_auth_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_auth(wiphy, ndev, req);
	}

	/* STA Mode */
	return mtk_cfg80211_auth(wiphy, ndev, req);
}
#endif

int mtk_cfg_connect(struct wiphy *wiphy,
		    struct net_device *ndev,
		    struct cfg80211_connect_params *sme)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_connect(wiphy, ndev, sme);
	/* STA Mode */
	return mtk_cfg80211_connect(wiphy, ndev, sme);
}

int mtk_cfg_disconnect(struct wiphy *wiphy,
		       struct net_device *ndev,
		       u16 reason_code)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_disconnect(wiphy, ndev,
						   reason_code);
	/* STA Mode */
	return mtk_cfg80211_disconnect(wiphy, ndev, reason_code);
}

int mtk_cfg_deauth(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_deauth_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}
	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_deauth(wiphy, ndev, req);
	else
		return mtk_cfg80211_deauth(wiphy, ndev, req);
}

int mtk_cfg_join_ibss(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_ibss_params *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_join_ibss(wiphy, ndev, params);
	/* STA Mode */
	return mtk_cfg80211_join_ibss(wiphy, ndev, params);
}

int mtk_cfg_leave_ibss(struct wiphy *wiphy,
		       struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0)
		return mtk_p2p_cfg80211_leave_ibss(wiphy, ndev);
	/* STA Mode */
	return mtk_cfg80211_leave_ibss(wiphy, ndev);
}

int mtk_cfg_set_power_mgmt(struct wiphy *wiphy,
			   struct net_device *ndev,
			   bool enabled, int timeout)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_set_power_mgmt(wiphy, ndev,
						       enabled, timeout);
	}
	/* STA Mode */
	return mtk_cfg80211_set_power_mgmt(wiphy, ndev, enabled,
					   timeout);
}

int mtk_cfg_set_pmksa(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_set_pmksa(wiphy, ndev, pmksa);
}

int mtk_cfg_del_pmksa(struct wiphy *wiphy,
		      struct net_device *ndev,
		      struct cfg80211_pmksa *pmksa)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_del_pmksa(wiphy, ndev, pmksa);
}

int mtk_cfg_flush_pmksa(struct wiphy *wiphy,
			struct net_device *ndev)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_flush_pmksa(wiphy, ndev);
}

#if CONFIG_SUPPORT_GTK_REKEY
int mtk_cfg_set_rekey_data(struct wiphy *wiphy,
			   struct net_device *dev,
			   struct cfg80211_gtk_rekey_data *data)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) > 0) {
		DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
		return -EFAULT;
	}

	return mtk_cfg80211_set_rekey_data(wiphy, dev, data);
}
#endif /* CONFIG_SUPPORT_GTK_REKEY */

int mtk_cfg_suspend(struct wiphy *wiphy,
		    struct cfg80211_wowlan *wow)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if (!wlanIsDriverReady(prGlueInfo,
		(WLAN_DRV_READY_CHCECK_RESET | WLAN_DRV_READY_CHCECK_WLAN_ON))) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return 0;
	}

#if CFG_CHIP_RESET_SUPPORT

	ASSERT(prGlueInfo);

	/* Before cfg80211 suspend, we must make sure L0.5 is either
	 * done or postponed.
	 */
	if (prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->chip_info->fgIsSupportL0p5Reset) {
		GL_SET_WFSYS_RESET_POSTPONE(prGlueInfo->prAdapter, TRUE);

		flush_work(&prGlueInfo->rWfsysResetWork);
	}
#endif

	/* TODO: AP/P2P do not support this function, should take that case. */
	return mtk_cfg80211_suspend(wiphy, wow);
}

int mtk_cfg_resume(struct wiphy *wiphy)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

#if CFG_CHIP_RESET_SUPPORT

	ASSERT(prGlueInfo);

	/* When cfg80211 resume, just reschedule L0.5 reset procedure
	 * if it is pending due to that fact that system suspend happened
	 * previously when L0.5 reset was not yet done.
	 */
	if (prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->chip_info->fgIsSupportL0p5Reset) {
		GL_SET_WFSYS_RESET_POSTPONE(prGlueInfo->prAdapter, FALSE);

		flush_work(&prGlueInfo->rWfsysResetWork);

		if (glReSchWfsysReset(prGlueInfo->prAdapter))
			DBGLOG(REQ, WARN, "reschedule L0.5 reset procedure\n");
	}
#endif

	if (!wlanIsDriverReady(prGlueInfo,
		(WLAN_DRV_READY_CHCECK_RESET | WLAN_DRV_READY_CHCECK_WLAN_ON))) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return 0;
	}

	/* TODO: AP/P2P do not support this function, should take that case. */
	return mtk_cfg80211_resume(wiphy);
}

int mtk_cfg_assoc(struct wiphy *wiphy,
		  struct net_device *ndev,
		  struct cfg80211_assoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

#if CFG_SUPPORT_SUPPLICANT_SME
	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
		return mtk_p2p_cfg80211_assoc(wiphy, ndev, req);
	}
#else
	if (mtk_IsP2PNetDevice(prGlueInfo, ndev) > 0) {
	DBGLOG(REQ, WARN, "P2P/AP don't support this function\n");
	return -EFAULT;
}

#endif
	/* STA Mode */
	return mtk_cfg80211_assoc(wiphy, ndev, req);
}

int mtk_cfg_remain_on_channel(struct wiphy *wiphy,
			      struct wireless_dev *wdev,
			      struct ieee80211_channel *chan,
			      unsigned int duration, u64 *cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_remain_on_channel(wiphy, wdev, chan,
				duration, cookie);
	}
	/* STA Mode */
	return mtk_cfg80211_remain_on_channel(wiphy, wdev, chan,
					      duration, cookie);
}

int mtk_cfg_cancel_remain_on_channel(struct wiphy *wiphy,
				     struct wireless_dev *wdev, u64 cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_cancel_remain_on_channel(wiphy,
				wdev,
				cookie);
	}
	/* STA Mode */
	return mtk_cfg80211_cancel_remain_on_channel(wiphy, wdev,
			cookie);
}

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_mgmt_tx(struct wiphy *wiphy,
		    struct wireless_dev *wdev,
		    struct cfg80211_mgmt_tx_params *params, u64 *cookie)
#else
int mtk_cfg_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		struct ieee80211_channel *channel, bool offchan,
		unsigned int wait, const u8 *buf, size_t len, bool no_cck,
		bool dont_wait_for_ack, u64 *cookie)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0)
		return mtk_p2p_cfg80211_mgmt_tx(wiphy, wdev, params,
						cookie);
	/* STA Mode */
	return mtk_cfg80211_mgmt_tx(wiphy, wdev, params, cookie);
#else /* KERNEL_VERSION(3, 14, 0) > CFG80211_VERSION_CODE */
	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_mgmt_tx(wiphy, wdev, channel, offchan,
			wait, buf, len, no_cck, dont_wait_for_ack, cookie);
	}
	/* STA Mode */
	return mtk_cfg80211_mgmt_tx(wiphy, wdev, channel, offchan, wait, buf,
			len, no_cck, dont_wait_for_ack, cookie);
#endif
}

int mtk_cfg_mgmt_frame_register(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				u16 frame_type, bool reg)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if (!prGlueInfo)
		return -EIO;

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		/* Assuming the P2P version is also updated to return int */
		return mtk_p2p_cfg80211_mgmt_frame_register(wiphy, wdev, frame_type, reg);
	} else {
		return mtk_cfg80211_mgmt_frame_register(wiphy, wdev, frame_type, reg);
	}
}


#if KERNEL_VERSION(5, 8, 0) <= CFG80211_VERSION_CODE
void mtk_cfg_mgmt_frame_update(struct wiphy *wiphy,
			struct wireless_dev *wdev,
			struct mgmt_frame_regs *upd)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	u_int8_t fgIsP2pNetDevice = FALSE;
	uint32_t *pu4PacketFilter = NULL;

	if ((wiphy == NULL) || (wdev == NULL) || (upd == NULL)) {
		DBGLOG(INIT, TRACE, "Invalidate params\n");
		return;
	}
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return;
	}
	fgIsP2pNetDevice = mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev);
	DBGLOG(INIT, TRACE,
		"netdev(0x%p) update management frame filter: 0x%08x\n",
		wdev->netdev, upd->interface_stypes);

	if (fgIsP2pNetDevice) {
		uint8_t ucRoleIdx = 0;
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;
		if (prGlueInfo->prP2PInfo[0]->prDevHandler ==
			wdev->netdev) {
			pu4PacketFilter =
				&prGlueInfo->prP2PDevInfo
				->u4OsMgmtFrameFilter;
			/* Reset filters*/
			*pu4PacketFilter = 0;
		} else {
			if (mtk_Netdev_To_RoleIdx(prGlueInfo,
				wdev->netdev, &ucRoleIdx) < 0) {
				DBGLOG(P2P, WARN,
					"wireless dev match fail!\n");
				return;
			}
			/* Non P2P device*/
			if (ucRoleIdx >= KAL_P2P_NUM) {
				DBGLOG(P2P, WARN,
				"Invalid RoleIdx %u\n",
				ucRoleIdx);
				return;
			}
			DBGLOG(P2P, TRACE,
				"Open packet filer RoleIdx %u\n",
				ucRoleIdx);
			prP2pRoleFsmInfo =
				prGlueInfo->prAdapter->rWifiVar
				.aprP2pRoleFsmInfo[ucRoleIdx];
			pu4PacketFilter = &prP2pRoleFsmInfo
				->u4P2pPacketFilter;
			*pu4PacketFilter =
				PARAM_PACKET_FILTER_SUPPORTED;
		}
	} else {
		pu4PacketFilter = &prGlueInfo->u4OsMgmtFrameFilter;
		*pu4PacketFilter = 0;
	}
	if (upd->interface_stypes & MASK_MAC_FRAME_PROBE_REQ)
		*pu4PacketFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
	if (upd->interface_stypes & MASK_MAC_FRAME_ACTION)
		*pu4PacketFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
#if CFG_SUPPORT_SOFTAP_WPA3
	if (upd->interface_stypes & MASK_MAC_FRAME_AUTH)
		*pu4PacketFilter |= PARAM_PACKET_FILTER_AUTH;
	if (upd->interface_stypes & MASK_MAC_FRAME_ASSOC_REQ)
		*pu4PacketFilter |= PARAM_PACKET_FILTER_ASSOC_REQ;
#endif
	set_bit(fgIsP2pNetDevice ?
		GLUE_FLAG_FRAME_FILTER_BIT :
		GLUE_FLAG_FRAME_FILTER_AIS_BIT,
		&prGlueInfo->ulFlag);
	/* wake up main thread */
	wake_up_interruptible(&prGlueInfo->waitq);

}
#endif


#ifdef CONFIG_NL80211_TESTMODE
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_cfg_testmode_cmd(struct wiphy *wiphy,
			 struct wireless_dev *wdev,
			 void *data, int len)
#else
int mtk_cfg_testmode_cmd(struct wiphy *wiphy, void *data,
			 int len)
#endif
{
#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0) {
		return mtk_p2p_cfg80211_testmode_cmd(wiphy, wdev, data,
						     len);
	}

	return mtk_cfg80211_testmode_cmd(wiphy, wdev, data, len);
#else
	/* XXX: no information can to check the mtk_IsP2PNetDevice */
	/* return mtk_p2p_cfg80211_testmode_cmd(wiphy, data, len); */
	return mtk_cfg80211_testmode_cmd(wiphy, data, len);
#endif
}
#endif	/* CONFIG_NL80211_TESTMODE */

#if (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0)
int mtk_cfg_change_bss(struct wiphy *wiphy,
		       struct net_device *dev,
		       struct bss_parameters *params)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_change_bss(wiphy, dev, params);
}

int mtk_cfg_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				u64 cookie)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) <= 0) {
		return mtk_cfg80211_mgmt_tx_cancel_wait(wiphy, wdev, cookie);
	}

	return mtk_p2p_cfg80211_mgmt_tx_cancel_wait(wiphy, wdev,
			cookie);
}

int mtk_cfg_disassoc(struct wiphy *wiphy,
		     struct net_device *dev,
		     struct cfg80211_disassoc_request *req)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_disassoc(wiphy, dev, req);
}

int mtk_cfg_start_ap(struct wiphy *wiphy,
		     struct net_device *dev,
		     struct cfg80211_ap_settings *settings)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_start_ap(wiphy, dev, settings);
}

int mtk_cfg_change_beacon(struct wiphy *wiphy,
			  struct net_device *dev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 7, 0)
			  struct cfg80211_ap_update *info)
#else
			  struct cfg80211_beacon_data *info)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_change_beacon(wiphy, dev, info);
}

#if (CFG_ADVANCED_80211_MLO == 1)
int mtk_cfg_stop_ap(struct wiphy *wiphy, struct net_device *dev,
	unsigned int link_id)
#else
int mtk_cfg_stop_ap(struct wiphy *wiphy, struct net_device *dev)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_stop_ap(wiphy, dev);
}

int mtk_cfg_set_wiphy_params(struct wiphy *wiphy,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
				 int radio_idx,
#endif
			     u32 changed)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	/* TODO: AIS not support this function */
	return mtk_p2p_cfg80211_set_wiphy_params(wiphy, changed);
}

#if (CFG_ADVANCED_80211_MLO == 1)
int mtk_cfg_set_bitrate_mask(struct wiphy *wiphy,
			     struct net_device *dev,
			     unsigned int link_id,
			     const u8 *peer,
			     const struct cfg80211_bitrate_mask *mask)
#else
int mtk_cfg_set_bitrate_mask(struct wiphy *wiphy,
			     struct net_device *dev,
			     const u8 *peer,
			     const struct cfg80211_bitrate_mask *mask)
#endif
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, dev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_set_bitrate_mask(wiphy, dev, peer,
			mask);
}

int mtk_cfg_set_txpower(struct wiphy *wiphy,
			struct wireless_dev *wdev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
			int radio_idx,
#endif
			enum nl80211_tx_power_setting type, int mbm)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if ((!prGlueInfo) ||
		!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON |
		WLAN_DRV_READY_CHCECK_HIF_SUSPEND)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) <= 0) {
		DBGLOG(REQ, WARN, "STA doesn't support this function\n");
		return -EFAULT;
	}

	return mtk_p2p_cfg80211_set_txpower(wiphy, wdev, type, mbm);
}


int mtk_cfg_get_txpower(struct wiphy *wiphy,
			struct wireless_dev *wdev,
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
			int radio_idx,
#endif
#if CFG80211_VERSION_CODE >= KERNEL_VERSION(6, 14, 0)
			unsigned int link_id,
#endif
			int *dbm)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	int32_t i4TxPower = 0;
	uint32_t u4Len = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	if (!prGlueInfo || !prGlueInfo->prAdapter)
		return -EFAULT;

	if (!wlanIsDriverReady(prGlueInfo, WLAN_DRV_READY_CHCECK_WLAN_ON))
		return -EAGAIN;

	if (mtk_IsP2PNetDevice(prGlueInfo, wdev->netdev) > 0)
		return mtk_p2p_cfg80211_get_txpower(wiphy, wdev, dbm);

	u4Status = kalIoctl(prGlueInfo,
			    wlanoidQueryGetTxPower,
			    &i4TxPower,
			    sizeof(i4TxPower),
			    TRUE,
			    TRUE,
			    FALSE,
			    &u4Len);

	if (u4Status != WLAN_STATUS_SUCCESS)
		return -EIO;

	*dbm = i4TxPower;

	return 0;
}


#endif /* (CFG_ENABLE_WIFI_DIRECT_CFG_80211 != 0) */
#endif	/* CFG_ENABLE_UNIFY_WIPHY */

/*-----------------------------------------------------------------------*/
/*!
 * @brief This function goes through the provided ies buffer, and
 *        collects those non-wfa vendor specific ies into driver's
 *        internal buffer (non_wfa_vendor_ie_buf), to be sent in
 *        AssocReq in AIS mode.
 *        The non-wfa vendor specific ies are those with ie_id = 0xdd
 *        and ouis are different from wfa's oui. (i.e., it could be
 *        customer's vendor ie ...etc.
 *
 * @param prGlueInfo    driver's private glueinfo
 *        ies           ie buffer
 *        len           length of ie
 *
 * @retval length of the non_wfa vendor ie
 */
/*-----------------------------------------------------------------------*/
uint16_t cfg80211_get_non_wfa_vendor_ie(struct GLUE_INFO *prGlueInfo,
	uint8_t *ies, int32_t len, uint8_t ucBssIndex)
{
	const uint8_t *pos = ies, *end = ies+len;
	struct ieee80211_vendor_ie *ie;
	int32_t ie_oui = 0;
	uint16_t *ret_len, max_len;
	uint8_t *w_pos;
	struct CONNECTION_SETTINGS *prConnSettings;

	if (!prGlueInfo || !ies || !len)
		return 0;

	prConnSettings =
		aisGetConnSettings(prGlueInfo->prAdapter,
		ucBssIndex);
	if (!prConnSettings)
		return 0;

	w_pos = prConnSettings->non_wfa_vendor_ie_buf;
	ret_len = &prConnSettings->non_wfa_vendor_ie_len;
	max_len = (uint16_t)sizeof(prConnSettings->non_wfa_vendor_ie_buf);

	while (pos < end) {
		pos = cfg80211_find_ie(WLAN_EID_VENDOR_SPECIFIC, pos,
				       end - pos);
		if (!pos)
			break;

		ie = (struct ieee80211_vendor_ie *)pos;

		/* Make sure we can access ie->len */
		BUILD_BUG_ON(offsetof(struct ieee80211_vendor_ie, len) != 1);

		if (ie->len < sizeof(*ie))
			goto cont;

		ie_oui = ie->oui[0] << 16 | ie->oui[1] << 8 | ie->oui[2];
		/*
		 * If oui is other than: 0x0050f2 & 0x506f9a,
		 * we consider it is non-wfa oui.
		 */
		if (ie_oui != WLAN_OUI_MICROSOFT && ie_oui != WLAN_OUI_WFA) {
			/*
			 * If remaining buf len is capable, we copy
			 * this ie to the buf.
			 */
			if (max_len-(*ret_len) >= ie->len+2) {
				DBGLOG(AIS, TRACE,
					   "vendor ie(len=%d, oui=0x%06x)\n",
					   ie->len, ie_oui);
				memcpy(w_pos, pos, ie->len+2);
				w_pos += (ie->len+2);
				(*ret_len) += ie->len+2;
			} else {
				/* Otherwise we give an error msg
				 * and return.
				 */
				DBGLOG(AIS, ERROR,
					"Insufficient buf for vendor ie, exit!\n");
				break;
			}
		}
cont:
		pos += 2 + ie->len;
	}
	return *ret_len;
}

int mtk_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *dev,
				 struct cfg80211_update_ft_ies_params *ftie)
{
#if CFG_SUPPORT_802_11R
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4InfoBufLen = 0;
	uint32_t rStatus;
	uint8_t ucBssIndex = 0;

	if (!wiphy)
		return -1;
	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);

	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidUpdateFtIes, (void *)(ftie->ie),
			   ftie->ie_len, FALSE, FALSE, FALSE, &u4InfoBufLen,
			   ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(OID, INFO, "FT: update Ft IE failed\n");
#else
	DBGLOG(OID, INFO, "FT: 802.11R is not enabled\n");
#endif

	return 0;
}

const uint8_t *mtk_cfg80211_find_ie_match_mask(uint8_t eid,
	const uint8_t *ies, int len, const uint8_t *match, int match_len,
	int match_offset, const uint8_t *match_mask)
{
	/* match_offset can't be smaller than 2, unless match_len is
	 * zero, in which case match_offset must be zero as well.
	 */
	if (WARN_ON((match_len && match_offset < 2) ||
		(!match_len && match_offset)))
		return NULL;
	while (len >= 2 && len >= ies[1] + 2) {
		if ((ies[0] == eid) &&
			(ies[1] + 2 >= match_offset + match_len) &&
			!kalMaskMemCmp(ies + match_offset,
			match, match_mask, match_len))
			return ies;
		len -= ies[1] + 2;
		ies += ies[1] + 2;
	}
	return NULL;
}

#if CFG_SUPPORT_ROAMING
int mtk_cfg80211_set_cqm_rssi_config(
				struct wiphy *wiphy, struct net_device *dev,
				int32_t rssi_thold, uint32_t rssi_hyst)
{
	uint32_t u4InfoBufLen = 0;
	uint16_t rcpi_thold = 254;
	uint16_t rcpi_hyst = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	struct CMD_ROAMING_CTRL rRoamingCtrl;
	uint8_t ucBssIndex = 0;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ucBssIndex = wlanGetBssIdx(dev);
	if (!IS_BSS_INDEX_VALID(ucBssIndex))
		return -EINVAL;

	DBGLOG(REQ, INFO, "ucBssIndex = %d\n", ucBssIndex);
	kalMemZero(&rRoamingCtrl, sizeof(rRoamingCtrl));

	if (rssi_thold != 0) {
		rcpi_thold = (uint16_t)((rssi_thold + 110) * 2);
		rcpi_hyst = (uint16_t)(rssi_hyst * 2);
		rRoamingCtrl.fgEnable = TRUE;
		rRoamingCtrl.u2RcpiLowThr = rcpi_thold;
		rRoamingCtrl.ucRcpiAdjustStep = rcpi_hyst;
	} else {
		rRoamingCtrl.fgEnable = FALSE;
	}
	DBGLOG(OID, INFO,
		"Set RSSI Config Enable: %d rcpi_thold: %d rcpi_hyst %d\n",
		rRoamingCtrl.fgEnable,
		rcpi_thold, rcpi_hyst);

	rStatus = kalIoctlByBssIdx(prGlueInfo,
		wlanoidSetRoamingCtrl, &rRoamingCtrl,
		sizeof(struct CMD_ROAMING_CTRL),
		FALSE, FALSE, TRUE, &u4InfoBufLen, ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(OID, INFO, "Set RSSI Config failed 0x%x\n", rStatus);
		return -EINVAL;
	}
	return 0;
}

#if KERNEL_VERSION(4, 14, 0) <= CFG80211_VERSION_CODE
int mtk_cfg80211_set_cqm_rssi_range_config(
				struct wiphy *wiphy, struct net_device *dev,
				int32_t rssi_low, int32_t rssi_high)
{
	return 0;
}
#endif

int mtk_cfg80211_set_cqm_txe_config(
				struct wiphy *wiphy, struct net_device *dev,
				uint32_t rate, uint32_t pkts, uint32_t intvl)
{
	return 0;
}
#endif

#if (CFG_SUPPORT_SNIFFER_RADIOTAP == 1)
int mtk_cfg80211_set_monitor_channel(struct wiphy *wiphy,
			struct cfg80211_chan_def *chandef)
{
	struct GLUE_INFO *prGlueInfo;
	uint8_t ucBand = BAND_NULL;
	uint8_t ucSco = 0;
	uint8_t ucChannelWidth = 0;
	uint8_t ucPriChannel = 0;
	uint8_t ucChannelS1 = 0;
	uint8_t ucChannelS2 = 0;
	uint32_t u4BufLen;
	uint32_t rStatus;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wiphy);
	ASSERT(prGlueInfo);

	if ((!prGlueInfo) || (prGlueInfo->u4ReadyFlag == 0)) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -EFAULT;
	}

	if (chandef) {
		ucPriChannel =
		ieee80211_frequency_to_channel(chandef->chan->center_freq);
		ucChannelS1 =
		ieee80211_frequency_to_channel(chandef->center_freq1);
		ucChannelS2 =
		ieee80211_frequency_to_channel(chandef->center_freq2);

		switch (chandef->chan->band) {
		case NL80211_BAND_2GHZ:
			ucBand = BAND_2G4;
			break;
		case NL80211_BAND_5GHZ:
			ucBand = BAND_5G;
			break;
		default:
			return -EFAULT;
		}

		switch (chandef->width) {
		case NL80211_CHAN_WIDTH_80P80:
			ucChannelWidth = CW_80P80MHZ;
			break;
		case NL80211_CHAN_WIDTH_160:
			ucChannelWidth = CW_160MHZ;
			break;
		case NL80211_CHAN_WIDTH_80:
			ucChannelWidth = CW_80MHZ;
			break;
		case NL80211_CHAN_WIDTH_40:
			ucChannelWidth = CW_20_40MHZ;
			if (ucChannelS1 > ucPriChannel)
				ucSco = CHNL_EXT_SCA;
			else
				ucSco = CHNL_EXT_SCB;
			break;
		case NL80211_CHAN_WIDTH_20:
			ucChannelWidth = CW_20_40MHZ;
			break;
		default:
			return -EFAULT;
		}
	}

	prGlueInfo->ucPriChannel = ucPriChannel;
	prGlueInfo->ucChannelS1 = ucChannelS1;
	prGlueInfo->ucChannelS2 = ucChannelS2;
	prGlueInfo->ucBand = ucBand;
	prGlueInfo->ucChannelWidth = ucChannelWidth;
	prGlueInfo->ucSco = ucSco;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetMonitor,
		NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);

	return 0;
}
#endif
