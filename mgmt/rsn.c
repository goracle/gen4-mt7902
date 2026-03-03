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
 * Id: mgmt/rsn.c#3
 */

/*! \file   "rsn.c"
 *  \brief  This file including the 802.11i, wpa and wpa2(rsn) related function.
 *
 *    This file provided the macros and functions library to support
 *	the wpa/rsn ie parsing, cipher and AKM check to help the AP seleced
 *	deciding, tkip mic error handler and rsn PMKID support.
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
 * \brief This routine is called to parse RSN IE.
 *
 * \param[in]  prInfoElem Pointer to the RSN IE
 * \param[out] prRsnInfo Pointer to the BSSDescription structure to store the
 *                  RSN information from the given RSN IE
 *
 * \retval TRUE - Succeeded
 * \retval FALSE - Failed
 */
/*----------------------------------------------------------------------------*/
/*
 * Refactored rsnParseRsnIE
 *
 * - Strong bounds checking for every field
 * - Helper readers (big-endian for 4-octet suite fields,
 *   little-endian for 16-bit counts/caps where appropriate)
 * - Caps number of suites/PMKIDs to safe limits
 * - Initializes prRsnInfo (zero) at entry
 * - Clear, small helper functions for readability
 */

#include <linux/types.h>
//#include <string.h> /* memset */
//#include <stdio.h>  /* for DBGLOG usage consistency */

/* assume these come from your headers */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* helper readers (no alignment assumptions) */

/* consume 1 byte */
static inline int
rsn_read_u8(uint8_t **pp, int *premain, uint8_t *out)
{
	if (*premain < 1)
		return 0;
	*out = *((*pp)++);
	(*premain)--;
	return 1;
}

/* consume 2 bytes, little-endian (as used for version, counts, capabilities) */
static inline int
rsn_read_le16(uint8_t **pp, int *premain, uint16_t *out)
{
	if (*premain < 2)
		return 0;
	*out = (uint16_t)((uint16_t)(*pp)[0] | ((uint16_t)(*pp)[1] << 8));
	*pp += 2;
	*premain -= 2;
	return 1;
}

/* consume 4 bytes, big-endian/network-order (used for 4-octet suite fields) */
static inline int
rsn_read_be32(uint8_t **pp, int *premain, uint32_t *out)
{
	if (*premain < 4)
		return 0;
	*out = ((uint32_t)(*pp)[0] << 24) |
	       ((uint32_t)(*pp)[1] << 16) |
	       ((uint32_t)(*pp)[2] << 8) |
	       ((uint32_t)(*pp)[3]);
	*pp += 4;
	*premain -= 4;
	return 1;
}

/* copy N bytes (with bound check) */
static inline int
rsn_copy_bytes(uint8_t **pp, int *premain, void *dst, size_t n)
{
	if (n == 0)
		return 1;
	if (*premain < (int)n)
		return 0;
	memcpy(dst, *pp, n);
	*pp += n;
	*premain -= (int)n;
	return 1;
}

/* main refactored function */
u_int8_t
rsnParseRsnIE(IN struct ADAPTER *prAdapter,
              IN struct RSN_INFO_ELEM *prInfoElem,
              OUT struct RSN_INFO *prRsnInfo)
{
	/* locals */
	uint32_t i;
	int32_t remain;
	uint16_t version = 0;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint16_t u2DesiredPmkidCnt = 0;
	uint16_t u2SupportedPmkidCnt = 0;
	uint16_t u2Cap = 0;
	uint32_t groupSuite = RSN_CIPHER_SUITE_CCMP;      /* default if missing */
	uint32_t groupMgmtSuite = 0;
	uint8_t *cp;
	uint8_t *pair_list_ptr = NULL;
	uint8_t *auth_list_ptr = NULL;

	DEBUGFUNC("rsnParseRsnIE");

	/* basic validation */
	if (!prInfoElem || !prRsnInfo) {
		DBGLOG(RSN, TRACE, "rsnParseRsnIE: NULL arg\n");
		return FALSE;
	}

	/* zero/initialize output structure to avoid leaking old data */
	memset(prRsnInfo, 0, sizeof(*prRsnInfo));

	/* RSN IE must contain at least the version (2 bytes) */
	if (prInfoElem->ucLength < 2) {
		DBGLOG(RSN, TRACE, "RSN IE length too short (length=%u)\n",
		       prInfoElem->ucLength);
		return FALSE;
	}

	/* Read version (2 bytes, little-endian) */
	/* version field in the element is stored as little-endian per spec */
	cp = (uint8_t *)&prInfoElem->u2Version; /* pointer to version low-level */
	remain = (int)prInfoElem->ucLength - 2; /* we'll treat version separately */

	/* read version via safe reader */
	{
		uint8_t *pv = (uint8_t *)&prInfoElem->u2Version;
		/* version provided by struct field already in wire order; use helper */
		uint16_t tmp_ver = 0;
		if (!rsn_read_le16(&pv, &((int){2}), &tmp_ver)) {
			/* fallback: try macro used in legacy code to read version */
			WLAN_GET_FIELD_16(&prInfoElem->u2Version, &version);
		} else {
			version = tmp_ver;
		}
	}
	/* The above block ensures version is set; but to keep consistent with prior code,
	   we still check version value directly: */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &version);

	if (version != 1) {
		DBGLOG(RSN, TRACE, "Unsupported RSN IE version: %u\n", version);
		return FALSE;
	}

	/* cp should now point to first byte after version; align with prior layout:
	 * original code used &prInfoElem->u4GroupKeyCipherSuite as start.
	 * We'll use same start to remain compatible with structure layout.
	 */
	cp = (uint8_t *)&prInfoElem->u4GroupKeyCipherSuite;

	/* remaining length is the element length minus 2 (version) */
	remain = (int)prInfoElem->ucLength - 2;

	/* Top-level parse loop (structured, with early exits on truncation). */
	do {
		/* If nothing left, we're done (valid RSNE with only version). */
		if (remain <= 0)
			break;

		/* 1) Group Key Cipher Suite (4 octets) - network order (big-endian) */
		if (remain < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in group cipher suite (IE len: %u)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}
		if (!rsn_read_be32(&cp, &remain, &groupSuite)) {
			/* unreachable due to check, but keep defensive */
			return FALSE;
		}

		/* 2) Pairwise Key Cipher Suite Count (2 bytes, little-endian) */
		if (remain == 0)
			break; /* no pairwise suites present */

		if (remain < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite count (IE len: %u)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}
		if (!rsn_read_le16(&cp, &remain, &u2PairSuiteCount))
			return FALSE;

		/* cap pairwise count */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES) {
			DBGLOG(RSN, WARN,
			       "Pairwise suite count %u > max (%u); capping\n",
			       u2PairSuiteCount, MAX_NUM_SUPPORTED_CIPHER_SUITES);
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;
		}

		/* 3) Pairwise Key Cipher Suite List (each 4 octets, big-endian) */
		i = (uint32_t)u2PairSuiteCount * 4U;
		if (remain < (int32_t)i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %u)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}
		if (u2PairSuiteCount > 0) {
			pair_list_ptr = cp; /* remember pointer for later safe copy */
			/* advance cp */
			cp += i;
			remain -= (int32_t)i;
		}

		/* 4) Auth & Key Management Suite Count (2 bytes, little-endian) */
		if (remain == 0)
			break; /* no AKM suites present */

		if (remain < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %u)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}
		if (!rsn_read_le16(&cp, &remain, &u2AuthSuiteCount))
			return FALSE;

		/* cap auth count */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES) {
			DBGLOG(RSN, WARN,
			       "Auth suite count %u > max (%u); capping\n",
			       u2AuthSuiteCount, MAX_NUM_SUPPORTED_AKM_SUITES);
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;
		}

		/* 5) Auth & Key Management Suite List (each 4 octets, big-endian) */
		i = (uint32_t)u2AuthSuiteCount * 4U;
		if (remain < (int32_t)i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %u)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}
		if (u2AuthSuiteCount > 0) {
			auth_list_ptr = cp;
			cp += i;
			remain -= (int32_t)i;
		}

		/* 6) RSN Capabilities (2 bytes, little-endian) */
		if (remain == 0)
			break; /* truncated cap is tolerated per wpa_supplicant behaviour */

		if (remain < 2) {
			DBGLOG(RSN, TRACE,
			       "Ignore truncated RSN capabilities (IE len: %u)\n",
			       prInfoElem->ucLength);
			break;
		}
		if (!rsn_read_le16(&cp, &remain, &u2Cap))
			return FALSE;

		/* 7) PMKID count & list (optional; counts are little-endian) */
		if (remain == 0)
			break;

		/* PMKID count */
		if (remain < 2) {
			DBGLOG(RSN, TRACE,
			       "Ignore truncated PMKID count (IE len: %u)\n",
			       prInfoElem->ucLength);
			break;
		}
		if (!rsn_read_le16(&cp, &remain, &u2DesiredPmkidCnt))
			return FALSE;

		/* clamp supported pmkid count to hard maximum */
		if (u2DesiredPmkidCnt > MAX_NUM_SUPPORTED_PMKID) {
			u2SupportedPmkidCnt = MAX_NUM_SUPPORTED_PMKID;
			DBGLOG(RSN, WARN,
			       "Support maximum PMKID cnt = %u (desired=%u)\n",
			       (unsigned)MAX_NUM_SUPPORTED_PMKID,
			       (unsigned)u2DesiredPmkidCnt);
		} else {
			u2SupportedPmkidCnt = u2DesiredPmkidCnt;
		}

		/* PMKID list length check */
		i = (uint32_t)u2DesiredPmkidCnt * RSN_PMKID_LEN;
		if (remain < (int32_t)i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in PMKID list (IE len: %u)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		if (u2SupportedPmkidCnt > 0) {
			/* safe copy */
			if (!rsn_copy_bytes(&cp, &remain,
			                    prRsnInfo->aucPmkidList,
			                    (size_t)u2SupportedPmkidCnt * RSN_PMKID_LEN)) {
				return FALSE;
			}
			DBGLOG(RSN, INFO, "== Dump cached PMKIDs ==\n");
			DBGLOG_MEM8(RSN, INFO,
			            prRsnInfo->aucPmkidList,
			            (u2SupportedPmkidCnt * RSN_PMKID_LEN));
			/* if desired > supported we still need to advance cp by desired count */
			if (u2DesiredPmkidCnt > u2SupportedPmkidCnt) {
				int32_t extra = (int32_t)((u2DesiredPmkidCnt - u2SupportedPmkidCnt) * RSN_PMKID_LEN);
				if (remain < extra) /* we already validated above, defensive */
					return FALSE;
				cp += extra;
				remain -= extra;
			}
		} else {
			/* no supported PMKIDs but still consume bytes for desired count */
			cp += (u2DesiredPmkidCnt * RSN_PMKID_LEN);
			remain -= (int32_t)(u2DesiredPmkidCnt * RSN_PMKID_LEN);
		}

		/* 8) Group Management Cipher Suite (4 octets, big-endian) - optional */
		if (remain == 0)
			break;
		if (remain < 4) {
			/* per previous implementation: continue to connect if truncated after PMKID */
			DBGLOG(RSN, TRACE,
			       "Fail to parse RSN IE in RSN Group Mgmt Cipher (IE len: %u) - treat as absent\n",
			       prInfoElem->ucLength);
			break;
		}
		if (!rsn_read_be32(&cp, &remain, &groupMgmtSuite))
			return FALSE;

	} while (0); /* single-run block for structured flow */

	/* Fill prRsnInfo fields (safe assignments only) */
	prRsnInfo->ucElemId = ELEM_ID_RSN;
	prRsnInfo->u2Version = version;
	prRsnInfo->u4GroupKeyCipherSuite = groupSuite;
	prRsnInfo->u4GroupMgmtKeyCipherSuite = groupMgmtSuite;
	prRsnInfo->u2PmkidCnt = u2SupportedPmkidCnt;
	prRsnInfo->u2RsnCap = u2Cap;
	prRsnInfo->fgRsnCapPresent = TRUE;

	/* Pairwise list: copy interpreted big-endian 4-octet suites into array */
	if (pair_list_ptr) {
		prRsnInfo->u4PairwiseKeyCipherSuiteCount = (uint32_t)u2PairSuiteCount;
		if (prRsnInfo->u4PairwiseKeyCipherSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			prRsnInfo->u4PairwiseKeyCipherSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		/* copy each 4-octet as big-endian value */
		for (i = 0; i < prRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
			uint32_t v = ((uint32_t)pair_list_ptr[i*4 + 0] << 24) |
			             ((uint32_t)pair_list_ptr[i*4 + 1] << 16) |
			             ((uint32_t)pair_list_ptr[i*4 + 2] << 8) |
			             ((uint32_t)pair_list_ptr[i*4 + 3]);
			prRsnInfo->au4PairwiseKeyCipherSuite[i] = v;
			DBGLOG(RSN, LOUD,
			       "RSN: pairwise key cipher suite [%u]: 0x%08x\n",
			       (unsigned)i, SWAP32(v));
		}
	} else {
		/* default to CCMP */
		prRsnInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prRsnInfo->au4PairwiseKeyCipherSuite[0] = RSN_CIPHER_SUITE_CCMP;
		DBGLOG(RSN, LOUD,
		       "RSN: pairwise key cipher suite: 0x%08x (default)\n",
		       SWAP32(prRsnInfo->au4PairwiseKeyCipherSuite[0]));
	}

	/* Auth / AKM list */
	if (auth_list_ptr) {
		prRsnInfo->u4AuthKeyMgtSuiteCount = (uint32_t)u2AuthSuiteCount;
		if (prRsnInfo->u4AuthKeyMgtSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			prRsnInfo->u4AuthKeyMgtSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		for (i = 0; i < prRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
			uint32_t v = ((uint32_t)auth_list_ptr[i*4 + 0] << 24) |
			             ((uint32_t)auth_list_ptr[i*4 + 1] << 16) |
			             ((uint32_t)auth_list_ptr[i*4 + 2] << 8) |
			             ((uint32_t)auth_list_ptr[i*4 + 3]);
			prRsnInfo->au4AuthKeyMgtSuite[i] = v;
			DBGLOG(RSN, LOUD,
			       "RSN: AKM suite [%u]: 0x%08x\n", (unsigned)i, SWAP32(v));
		}
	} else {
		prRsnInfo->u4AuthKeyMgtSuiteCount = 1;
		prRsnInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;
		DBGLOG(RSN, LOUD, "RSN: AKM suite: 0x%08x (default)\n",
		       SWAP32(prRsnInfo->au4AuthKeyMgtSuite[0]));
	}

	/* Logging summary (similar to original) */
	DBGLOG(RSN, INFO,
	       "Parsed RSN IE: version=%u pairCnt=%u authCnt=%u pmkidCnt=%u group=0x%08x gm=0x%08x cap=0x%04x\n",
	       (unsigned)prRsnInfo->u2Version,
	       (unsigned)prRsnInfo->u4PairwiseKeyCipherSuiteCount,
	       (unsigned)prRsnInfo->u4AuthKeyMgtSuiteCount,
	       (unsigned)prRsnInfo->u2PmkidCnt,
	       prRsnInfo->u4GroupKeyCipherSuite,
	       prRsnInfo->u4GroupMgmtKeyCipherSuite,
	       prRsnInfo->u2RsnCap);

	/* dump raw IE bytes to log (original did prInfoElem->ucLength + 2 maybe)
	 * Keep same behaviour for debugging.
	 */
	DBGLOG_MEM8(RSN, INFO, (uint8_t *)prInfoElem, prInfoElem->ucLength + 2);

	return TRUE;
} /* rsnParseRsnIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to parse WPA IE.
 *
 * \param[in]  prInfoElem Pointer to the WPA IE.
 * \param[out] prWpaInfo Pointer to the BSSDescription structure to store the
 *                       WPA information from the given WPA IE.
 *
 * \retval TRUE Succeeded.
 * \retval FALSE Failed.
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnParseWpaIE(IN struct ADAPTER *prAdapter,
		       IN struct WPA_INFO_ELEM *prInfoElem,
		       OUT struct RSN_INFO *prWpaInfo)
{
	uint32_t i;
	int32_t u4RemainWpaIeLen;
	uint16_t u2Version;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = WPA_CIPHER_SUITE_TKIP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;
	u_int8_t fgCapPresent = FALSE;

	DEBUGFUNC("rsnParseWpaIE");

	ASSERT(prInfoElem);
	ASSERT(prWpaInfo);

	/* Verify the length of the WPA IE. */
	if (prInfoElem->ucLength < 6) {
		DBGLOG(RSN, TRACE, "WPA IE length too short (length=%d)\n",
		       prInfoElem->ucLength);
		return FALSE;
	}

	/* Check WPA version: currently, we only support version 1. */
	WLAN_GET_FIELD_16(&prInfoElem->u2Version, &u2Version);
	if (u2Version != 1) {
		DBGLOG(RSN, TRACE, "Unsupported WPA IE version: %d\n",
		       u2Version);
		return FALSE;
	}

	cp = (uint8_t *) &prInfoElem->u4GroupKeyCipherSuite;
	u4RemainWpaIeLen = (int32_t) prInfoElem->ucLength - 6;

	do {
		if (u4RemainWpaIeLen == 0)
			break;

		/* WPA_OUI      : 4
		 *  Version      : 2
		 *  GroupSuite   : 4
		 *  PairwiseCount: 2
		 *  PairwiseSuite: 4 * pairSuiteCount
		 *  AuthCount    : 2
		 *  AuthSuite    : 4 * authSuiteCount
		 *  Cap          : 2
		 */

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainWpaIeLen < 4) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainWpaIeLen -= 4;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainWpaIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in pairwise cipher suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (int32_t) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite
		 * Count field.
		 */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainWpaIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite
		 * List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainWpaIeLen < (int32_t) i) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainWpaIeLen -= (int32_t) i;

		if (u4RemainWpaIeLen == 0)
			break;

		/* Parse the WPA u2Capabilities field. */
		if (u4RemainWpaIeLen < 2) {
			DBGLOG(RSN, TRACE,
			       "Fail to parse WPA IE in WPA capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		fgCapPresent = TRUE;
		WLAN_GET_FIELD_16(cp, &u2Cap);
		u4RemainWpaIeLen -= 2;
	} while (FALSE);

	/* Save the WPA information for the BSS. */

	prWpaInfo->ucElemId = ELEM_ID_WPA;

	prWpaInfo->u2Version = u2Version;

	prWpaInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, LOUD, "WPA: version %d, group key cipher suite 0x%x\n",
		u2Version, SWAP32(u4GroupSuite));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prWpaInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
					  &prWpaInfo->au4PairwiseKeyCipherSuite
					  [i]);
			pucPairSuite += 4;

			DBGLOG(RSN, LOUD,
			   "WPA: pairwise key cipher suite [%d]: 0x%x\n", i,
			   SWAP32(prWpaInfo->au4PairwiseKeyCipherSuite[i]));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present.
		 * Use the default chipher suite for WPA: TKIP.
		 */
		prWpaInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prWpaInfo->au4PairwiseKeyCipherSuite[0] = WPA_CIPHER_SUITE_TKIP;

		DBGLOG(RSN, LOUD,
			"WPA: pairwise key cipher suite: 0x%x (default)\n",
			SWAP32(prWpaInfo->au4PairwiseKeyCipherSuite[0]));
	}

	if (pucAuthSuite) {
		/* The information about the authentication and
		 * key management suites is present.
		 */
		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prWpaInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prWpaInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, LOUD,
			       "WPA: AKM suite [%d]: 0x%x\n", i,
			       SWAP32(prWpaInfo->au4AuthKeyMgtSuite[i]));
		}
	} else {
		/* The information about the authentication
		 * and key management suites is not present.
		 * Use the default AKM suite for WPA.
		 */
		prWpaInfo->u4AuthKeyMgtSuiteCount = 1;
		prWpaInfo->au4AuthKeyMgtSuite[0] = WPA_AKM_SUITE_802_1X;

		DBGLOG(RSN, LOUD,
		       "WPA: AKM suite: 0x%x (default)\n",
		       SWAP32(prWpaInfo->au4AuthKeyMgtSuite[0]));
	}

	if (fgCapPresent) {
		prWpaInfo->fgRsnCapPresent = TRUE;
		prWpaInfo->u2RsnCap = u2Cap;
		DBGLOG(RSN, LOUD, "WPA: RSN cap: 0x%04x\n",
		       prWpaInfo->u2RsnCap);
	} else {
		prWpaInfo->fgRsnCapPresent = FALSE;
		prWpaInfo->u2RsnCap = 0;
	}

	return TRUE;
}				/* rsnParseWpaIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to search the desired pairwise
 *        cipher suite from the MIB Pairwise Cipher Suite
 *        configuration table.
 *
 * \param[in] u4Cipher The desired pairwise cipher suite to be searched
 * \param[out] pu4Index Pointer to the index of the desired pairwise cipher in
 *                      the table
 *
 * \retval TRUE - The desired pairwise cipher suite is found in the table.
 * \retval FALSE - The desired pairwise cipher suite is not found in the
 *                 table.
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnSearchSupportedCipher(IN struct ADAPTER *prAdapter,
				  IN uint32_t u4Cipher, OUT uint32_t *pu4Index,
				  IN uint8_t ucBssIndex)
{
	uint8_t i;
	struct DOT11_RSNA_CONFIG_PAIRWISE_CIPHERS_ENTRY *prEntry;
	struct IEEE_802_11_MIB *prMib;

	DEBUGFUNC("rsnSearchSupportedCipher");

	ASSERT(pu4Index);

	prMib = aisGetMib(prAdapter, ucBssIndex);

	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++) {
		prEntry =
		    &prMib->dot11RSNAConfigPairwiseCiphersTable[i];
		if (prEntry->dot11RSNAConfigPairwiseCipher == u4Cipher &&
		    prEntry->dot11RSNAConfigPairwiseCipherEnabled) {
			*pu4Index = i;
			return TRUE;
		}
	}
	return FALSE;
}				/* rsnSearchSupportedCipher */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Whether BSS RSN is matched from upper layer set.
 *
 * \param[in] prAdapter Pointer to the Adapter structure, BSS RSN Information
 *
 * \retval BOOLEAN
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnIsSuitableBSS(IN struct ADAPTER *prAdapter,
			  IN struct RSN_INFO *prBssRsnInfo,
			  IN uint8_t ucBssIndex)
{
	uint32_t i, c, s, k;
	struct CONNECTION_SETTINGS *prConnSettings;

	DEBUGFUNC("rsnIsSuitableBSS");

	prConnSettings =
		aisGetConnSettings(prAdapter, ucBssIndex);

	s = prConnSettings->rRsnInfo.u4GroupKeyCipherSuite;
	k = prBssRsnInfo->u4GroupKeyCipherSuite;

	if ((s & 0x000000FF) != GET_SELECTOR_TYPE(k)) {
		DBGLOG(RSN, WARN, "Break by GroupKey s=0x%x k=0x%x\n",
			s, SWAP32(k));
		return FALSE;
	}

	c = prBssRsnInfo->u4PairwiseKeyCipherSuiteCount;
	for (i = 0; i < c; i++) {
		s = prConnSettings->
			rRsnInfo.au4PairwiseKeyCipherSuite[0];
		k = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
		if ((s & 0x000000FF) == GET_SELECTOR_TYPE(k)) {
			break;
		} else if (i == c - 1) {
			DBGLOG(RSN, WARN, "Break by PairwisKey s=0x%x k=0x%x\n",
				s, SWAP32(k));
			return FALSE;
		}
	}

	c = prBssRsnInfo->u4AuthKeyMgtSuiteCount;
	for (i = 0; i < c; i++) {
	  s = prConnSettings->
	    rRsnInfo.au4AuthKeyMgtSuite[0];
	  k = prBssRsnInfo->au4AuthKeyMgtSuite[i];
	  if ((s & 0x000000FF) == GET_SELECTOR_TYPE(k)) {
	    break;
	  } else if (i == c - 1) {
	    DBGLOG(RSN, WARN, "Break by AuthKey s=0x%x k=0x%x\n",
		   s, SWAP32(k));
	    return FALSE;
	  }
	}

	DBGLOG(RSN, ERROR,
	       "RSN RESULT: group=%x pairwise_count=%d akm_count=%d",
	       prBssRsnInfo->u4GroupKeyCipherSuite,
	       prBssRsnInfo->u4PairwiseKeyCipherSuiteCount,
	       prBssRsnInfo->u4AuthKeyMgtSuiteCount);

	for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
	  DBGLOG(RSN, ERROR,
		 "  pairwise[%d]=%x",
		 i,
		 prBssRsnInfo->au4PairwiseKeyCipherSuite[i]);
	}

	for (i = 0; i < prBssRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
	  DBGLOG(RSN, ERROR,
		 "  akm[%d]=%x",
		 i,
		 prBssRsnInfo->au4AuthKeyMgtSuite[i]);
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to search the desired
 *        authentication and key management (AKM) suite from the
 *        MIB Authentication and Key Management Suites table.
 *
 * \param[in]  u4AkmSuite The desired AKM suite to be searched
 * \param[out] pu4Index   Pointer to the index of the desired AKM suite in the
 *                        table
 *
 * \retval TRUE  The desired AKM suite is found in the table.
 * \retval FALSE The desired AKM suite is not found in the table.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t rsnSearchAKMSuite(IN struct ADAPTER *prAdapter,
			   IN uint32_t u4AkmSuite, OUT uint32_t *pu4Index,
			   IN uint8_t ucBssIndex)
{
	uint8_t i;
	struct DOT11_RSNA_CONFIG_AUTHENTICATION_SUITES_ENTRY
	*prEntry;
	struct IEEE_802_11_MIB *prMib;

	DEBUGFUNC("rsnSearchAKMSuite");

	ASSERT(pu4Index);

	prMib = aisGetMib(prAdapter, ucBssIndex);

	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++) {
		prEntry = &prMib->
				dot11RSNAConfigAuthenticationSuitesTable[i];
		if (prEntry->dot11RSNAConfigAuthenticationSuite ==
		    u4AkmSuite &&
		    prEntry->dot11RSNAConfigAuthenticationSuiteEnabled) {
			*pu4Index = i;
			return TRUE;
		}
	}
	return FALSE;
}				/* rsnSearchAKMSuite */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to perform RSNA or TSN policy
 *        selection for a given BSS.
 *
 * \param[in]  prBss Pointer to the BSS description
 *
 * \retval TRUE - The RSNA/TSN policy selection for the given BSS is
 *                successful. The selected pairwise and group cipher suites
 *                are returned in the BSS description.
 * \retval FALSE - The RSNA/TSN policy selection for the given BSS is failed.
 *                 The driver shall not attempt to join the given BSS.
 *
 * \note The Encrypt status matched score will save to bss for final ap select.
 */
/*----------------------------------------------------------------------------*/
/* Helper: return pointer to the RSN/WPA/OSEN info element for this BSS,
 * or NULL if it doesn't exist or is inappropriate for configured auth mode.
 */
/* Helper prototypes (static to this file) */
static struct RSN_INFO *rsn_get_bss_rsn_info(IN struct ADAPTER *prAdapter,
                                            IN struct BSS_DESC *prBss,
                                            IN enum ENUM_PARAM_AUTH_MODE eAuthMode);
static u_int8_t rsn_select_pair_group_for_enc(IN struct ADAPTER *prAdapter,
                                               IN struct RSN_INFO *prBssRsnInfo,
                                               IN enum ENUM_WEP_STATUS eEncStatus,
                                               OUT uint32_t *pu4Pairwise,
                                               OUT uint32_t *pu4Group);
static u_int8_t rsn_select_akm(IN struct ADAPTER *prAdapter,
                               IN struct RSN_INFO *prBssRsnInfo,
                               IN enum ENUM_PARAM_AUTH_MODE eAuthMode,
                               IN uint8_t ucBssIndex,
                               OUT uint32_t *pu4Akm);




u_int8_t rsnPerformPolicySelection(IN struct ADAPTER *prAdapter,
                                   IN struct BSS_DESC *prBss,
                                   IN uint8_t ucBssIndex)
{
    uint32_t u4PairwiseCipher = 0;
    uint32_t u4GroupCipher = 0;
    uint32_t u4AkmSuite = 0;
    uint32_t u4Tmp = 0;
    struct RSN_INFO *prBssRsnInfo = NULL;
    struct AIS_SPECIFIC_BSS_INFO *prAisSpec = NULL;

    enum ENUM_PARAM_AUTH_MODE eAuthMode;
    enum ENUM_PARAM_OP_MODE eOPMode;
    enum ENUM_WEP_STATUS eEncStatus;

    u_int8_t fgSuiteSupported = FALSE;

    if (!prAdapter || !prBss) {
        DBGLOG(RSN, ERROR, "rsnPerformPolicySelection: NULL param(s)\n");
        return FALSE;
    }

    prAisSpec = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
    if (!prAisSpec) {
        DBGLOG(RSN, ERROR, "rsnPerformPolicySelection: NULL AIS_SPEC_BSS_INFO\n");
        return FALSE;
    }
    prAisSpec->fgMgmtProtection = FALSE; /* default */

    eAuthMode = aisGetAuthMode(prAdapter, ucBssIndex);
    eOPMode = aisGetOPMode(prAdapter, ucBssIndex);
    eEncStatus = aisGetEncStatus(prAdapter, ucBssIndex);

    /* If BSS advertises privacy but AIS doesn't have security enabled -> reject */
    if ((prBss->u2CapInfo & CAP_INFO_PRIVACY) && !secEnabledInAis(prAdapter, ucBssIndex)) {
        DBGLOG(RSN, INFO, "Protected BSS but security disabled in AIS (BSSID %pM)\n",
               prBss->aucBSSID);
        return FALSE;
    }

    /* Get the RSN/WPA/OSEN info for the BSS */
    prBssRsnInfo = rsn_get_bss_rsn_info(prAdapter, prBss, eAuthMode);
    if (!prBssRsnInfo) {
        DBGLOG(RSN, ERROR, "rsn_get_bss_rsn_info failed (BSSID %pM)\n", prBss->aucBSSID);
        return FALSE;
    }

    /* Basic sanity checks */
    if (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES) {
        DBGLOG(RSN, ERROR, "rsnPerformPolicySelection: absurd pairwise count=%u (BSSID %pM)\n",
               prBssRsnInfo->u4PairwiseKeyCipherSuiteCount, prBss->aucBSSID);
        return FALSE;
    }
    if (prBssRsnInfo->u4AuthKeyMgtSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES) {
        DBGLOG(RSN, ERROR, "rsnPerformPolicySelection: absurd akm count=%u (BSSID %pM)\n",
               prBssRsnInfo->u4AuthKeyMgtSuiteCount, prBss->aucBSSID);
        return FALSE;
    }

    /* Diagnostics: what the RSN IE actually contains (first entries) */
    DBGLOG(RSN, INFO,
           "RSN IE (BSSID %pM): group=0x%08x pairCnt=%u authCnt=%u pmkidCnt=%u cap=0x%04x\n",
           prBss->aucBSSID,
           prBssRsnInfo->u4GroupKeyCipherSuite,
           prBssRsnInfo->u4PairwiseKeyCipherSuiteCount,
           prBssRsnInfo->u4AuthKeyMgtSuiteCount,
           prBssRsnInfo->u2PmkidCnt,
           prBssRsnInfo->u2RsnCap);

    if (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount > 0) {
        DBGLOG(RSN, INFO, "  first pairwise: 0x%08x\n",
               prBssRsnInfo->au4PairwiseKeyCipherSuite[0]);
    }
    if (prBssRsnInfo->u4AuthKeyMgtSuiteCount > 0) {
        DBGLOG(RSN, INFO, "  first AKM: 0x%08x\n",
               prBssRsnInfo->au4AuthKeyMgtSuite[0]);
    }
    if (prBssRsnInfo->u2PmkidCnt) {
        DBGLOG(RSN, INFO, "  PMKID list (%u):", prBssRsnInfo->u2PmkidCnt);
        DBGLOG_MEM8(RSN, INFO, prBssRsnInfo->aucPmkidList,
                    prBssRsnInfo->u2PmkidCnt * RSN_PMKID_LEN);
    }

    /* select pairwise & group cipher according to current enc status */
    if (!rsn_select_pair_group_for_enc(prAdapter, prBssRsnInfo, eEncStatus,
                                       &u4PairwiseCipher, &u4GroupCipher)) {
        /* Fixed diagnostics: log RSN IE content and the failed outputs */
        DBGLOG(RSN, ERROR,
               "rsn_select_pair_group_for_enc FAILED (BSSID %pM): enc=%d -> pair=0x%08x group=0x%08x\n",
               prBss->aucBSSID, eEncStatus, u4PairwiseCipher, u4GroupCipher);

        DBGLOG(RSN, ERROR,
               "  RSN IE summary: group=0x%08x first_pair=0x%08x first_akm=0x%08x pairCnt=%u akmCnt=%u pmkid=%u\n",
               prBssRsnInfo->u4GroupKeyCipherSuite,
               (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount ? prBssRsnInfo->au4PairwiseKeyCipherSuite[0] : 0),
               (prBssRsnInfo->u4AuthKeyMgtSuiteCount ? prBssRsnInfo->au4AuthKeyMgtSuite[0] : 0),
               prBssRsnInfo->u4PairwiseKeyCipherSuiteCount,
               prBssRsnInfo->u4AuthKeyMgtSuiteCount,
               prBssRsnInfo->u2PmkidCnt);

        return FALSE;
    }

    /* sanity */
    if (u4PairwiseCipher == 0 || u4GroupCipher == 0) {
        DBGLOG(RSN, ERROR,
               "rsnPerformPolicySelection: selected cipher invalid (BSSID %pM) pair=0x%08x group=0x%08x\n",
               prBss->aucBSSID, u4PairwiseCipher, u4GroupCipher);
        return FALSE;
    }

    /* verify selected ciphers are supported by driver/firmware */
    fgSuiteSupported = rsnSearchSupportedCipher(prAdapter, u4PairwiseCipher, &u4Tmp, ucBssIndex);
    if (fgSuiteSupported)
        fgSuiteSupported = rsnSearchSupportedCipher(prAdapter, u4GroupCipher, &u4Tmp, ucBssIndex);

    if (!fgSuiteSupported) {
        DBGLOG(RSN, ERROR,
               "rsnPerformPolicySelection: driver does not support pair/group (BSSID %pM) pair=0x%08x group=0x%08x\n",
               prBss->aucBSSID, u4PairwiseCipher, u4GroupCipher);
        return FALSE;
    }

    /* select AKM */
    if (!rsn_select_akm(prAdapter, prBssRsnInfo, eAuthMode, ucBssIndex, &u4AkmSuite)) {
        DBGLOG(RSN, ERROR,
               "rsn_select_akm failed (BSSID %pM) pair=0x%08x group=0x%08x\n",
               prBss->aucBSSID, u4PairwiseCipher, u4GroupCipher);
        DBGLOG(RSN, INFO, "  AKM candidates (%u):\n", prBssRsnInfo->u4AuthKeyMgtSuiteCount);
        for (u4Tmp = 0; u4Tmp < prBssRsnInfo->u4AuthKeyMgtSuiteCount; u4Tmp++)
            DBGLOG(RSN, INFO, "    AKM[%u]=0x%08x\n", u4Tmp, prBssRsnInfo->au4AuthKeyMgtSuite[u4Tmp]);
        return FALSE;
    }

#if CFG_ENABLE_WIFI_DIRECT
    if ((prAdapter->fgIsP2PRegistered) &&
        (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P)) {
        /* P2P policy: require CCMP Pair/Group and PSK AKM for typical P2P */
        if (u4PairwiseCipher != RSN_CIPHER_SUITE_CCMP ||
            u4GroupCipher != RSN_CIPHER_SUITE_CCMP ||
            u4AkmSuite != RSN_AKM_SUITE_PSK) {
            DBGLOG(RSN, INFO,
                   "P2P policy mismatch (BSSID %pM) pair=0x%08x group=0x%08x akm=0x%08x\n",
                   prBss->aucBSSID, u4PairwiseCipher, u4GroupCipher, u4AkmSuite);
            return FALSE;
        }
    }
#endif

    /* Commit selections into BSS record */
    prBss->u4RsnSelectedPairwiseCipher = u4PairwiseCipher;
    prBss->u4RsnSelectedGroupCipher = u4GroupCipher;
    prBss->u4RsnSelectedAKMSuite = u4AkmSuite;

    DBGLOG(RSN, INFO,
           "rsnPerformPolicySelection: selected (BSSID %pM) pair=0x%08x group=0x%08x akm=0x%08x\n",
           prBss->aucBSSID, u4PairwiseCipher, u4GroupCipher, u4AkmSuite);

    return TRUE;
}








/* Helper: pick the RSN/WPA/OSEN info pointer based on auth mode */
static struct RSN_INFO *rsn_get_bss_rsn_info(IN struct ADAPTER *prAdapter,
                                            IN struct BSS_DESC *prBss,
                                            IN enum ENUM_PARAM_AUTH_MODE eAuthMode)
{
    if (!prAdapter || !prBss)
        return NULL;

    if (eAuthMode == AUTH_MODE_WPA || eAuthMode == AUTH_MODE_WPA_PSK || eAuthMode == AUTH_MODE_WPA_NONE) {
        if (prBss->fgIEWPA)
            return &prBss->rWPAInfo;
        DBGLOG(RSN, INFO, "WPA IE does not exist\n");
        return NULL;
    }

    if (eAuthMode == AUTH_MODE_WPA2 || eAuthMode == AUTH_MODE_WPA2_PSK ||
        eAuthMode == AUTH_MODE_WPA2_FT_PSK || eAuthMode == AUTH_MODE_WPA2_FT ||
        eAuthMode == AUTH_MODE_WPA3_SAE) {
        if (prBss->fgIERSN)
            return &prBss->rRSNInfo;
        DBGLOG(RSN, INFO, "RSN IE does not exist\n");
        return NULL;
    }

#if CFG_SUPPORT_PASSPOINT
    if (eAuthMode == AUTH_MODE_WPA_OSEN) {
        if (prBss->fgIEOsen)
            return &prBss->rRSNInfo;
        DBGLOG(RSN, WARN, "OSEN IE does not exist\n");
        return NULL;
    }
#endif

    /* Otherwise fallback to WEP handling above in caller. */
    return NULL;
}

/* Helper: determine pairwise & group cipher given encryption status */
static u_int8_t
rsn_select_pair_group_for_enc(IN struct ADAPTER *prAdapter,
                             IN struct RSN_INFO *prBssRsnInfo,
                             IN enum ENUM_WEP_STATUS eEncStatus,
                             OUT uint32_t *pu4Pairwise,
                             OUT uint32_t *pu4Group)
{
    uint32_t i;

    if (!prAdapter || !prBssRsnInfo || !pu4Pairwise || !pu4Group)
        return FALSE;

    *pu4Pairwise = 0;
    *pu4Group = 0;

#define IS_CCMP(x) ((x) == RSN_CIPHER_SUITE_CCMP || (x) == WPA_CIPHER_SUITE_CCMP)
#define IS_TKIP(x) ((x) == RSN_CIPHER_SUITE_TKIP || (x) == WPA_CIPHER_SUITE_TKIP)
#define IS_WEP(x)  (GET_SELECTOR_TYPE(x) == CIPHER_SUITE_WEP40 || \
                    GET_SELECTOR_TYPE(x) == CIPHER_SUITE_WEP104)

    /* === Group-only BSS (pairwise NONE) === */
    if (prBssRsnInfo->u4PairwiseKeyCipherSuiteCount == 1 &&
        GET_SELECTOR_TYPE(prBssRsnInfo->au4PairwiseKeyCipherSuite[0]) == CIPHER_SUITE_NONE) {

        uint32_t grp = prBssRsnInfo->u4GroupKeyCipherSuite;

        if ((grp == RSN_CIPHER_SUITE_GCMP_256 && eEncStatus == ENUM_ENCRYPTION4_ENABLED) ||
            (IS_CCMP(grp) && eEncStatus == ENUM_ENCRYPTION3_ENABLED) ||
            (IS_TKIP(grp) && eEncStatus == ENUM_ENCRYPTION2_ENABLED) ||
            (IS_WEP(grp)  && eEncStatus == ENUM_ENCRYPTION1_ENABLED)) {

            *pu4Group = grp;
            *pu4Pairwise = WPA_CIPHER_SUITE_NONE;
            return TRUE;
        }

#if DBG
        DBGLOG(RSN, TRACE,
               "Group-only BSS mismatch: enc=%d group=0x%08x\n",
               eEncStatus, grp);
#endif
        return FALSE;
    }

    /* === Normal case === */
    for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
        uint32_t sel = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];

        switch (eEncStatus) {
        case ENUM_ENCRYPTION4_ENABLED:
            if (sel == RSN_CIPHER_SUITE_GCMP_256)
                *pu4Pairwise = sel;
            break;

        case ENUM_ENCRYPTION3_ENABLED:
            if (IS_CCMP(sel))
                *pu4Pairwise = sel;
            break;

        case ENUM_ENCRYPTION2_ENABLED:
            if (IS_TKIP(sel))
                *pu4Pairwise = sel;
            break;

        case ENUM_ENCRYPTION1_ENABLED:
            if (IS_WEP(sel))
                *pu4Pairwise = sel;
            break;

        default:
            DBGLOG(RSN, WARN, "Unsupported encryption status %d\n", eEncStatus);
            return FALSE;
        }
    }

    /* === Group cipher validation (FIXED) === */
    {
        uint32_t grp = prBssRsnInfo->u4GroupKeyCipherSuite;

        switch (eEncStatus) {
        case ENUM_ENCRYPTION4_ENABLED:
            if (grp == RSN_CIPHER_SUITE_GCMP_256)
                *pu4Group = grp;
            break;

        case ENUM_ENCRYPTION3_ENABLED:
            if (IS_CCMP(grp))
                *pu4Group = grp;
            break;

        case ENUM_ENCRYPTION2_ENABLED:
            if (!IS_CCMP(grp))  /* TKIP mode must reject CCMP-only BSS */
                *pu4Group = grp;
            else
                DBGLOG(RSN, TRACE,
                       "Reject: TKIP mode vs CCMP group (0x%08x)\n", grp);
            break;

        case ENUM_ENCRYPTION1_ENABLED:
            if (!IS_CCMP(grp) && !IS_TKIP(grp))
                *pu4Group = grp;
            else
                DBGLOG(RSN, TRACE,
                       "Reject: WEP mode vs modern group (0x%08x)\n", grp);
            break;
	default:
	  DBGLOG(RSN, WARN,
		 "Unsupported/invalid enc status %d (reject)\n",
		 eEncStatus);
	  return FALSE;

        }
    }

    /* === Diagnostics (safe + meaningful) === */
    DBGLOG(RSN, INFO,
           "[RSN SELECT] enc=%d pairwise=0x%08x group=0x%08x (count=%u)",
           eEncStatus,
           *pu4Pairwise,
           *pu4Group,
           prBssRsnInfo->u4PairwiseKeyCipherSuiteCount);

#if DBG
    for (i = 0; i < prBssRsnInfo->u4PairwiseKeyCipherSuiteCount; i++) {
        uint32_t sel = prBssRsnInfo->au4PairwiseKeyCipherSuite[i];
        DBGLOG(RSN, TRACE,
               "  cand[%u]=0x%08x type=0x%x",
               i, sel, GET_SELECTOR_TYPE(sel));
    }
#endif

    return (*pu4Pairwise != 0 && *pu4Group != 0) ? TRUE : FALSE;

#undef IS_CCMP
#undef IS_TKIP
#undef IS_WEP
}



/* Helper: choose an AKM suite that we support and matches policy */
static u_int8_t rsn_select_akm(IN struct ADAPTER *prAdapter,
                               IN struct RSN_INFO *prBssRsnInfo,
                               IN enum ENUM_PARAM_AUTH_MODE eAuthMode,
                               IN uint8_t ucBssIndex,
                               OUT uint32_t *pu4Akm)
{
    uint32_t j;
#if CFG_SUPPORT_802_11W
    int32_t ii;
#endif

    if (!prAdapter || !prBssRsnInfo || !pu4Akm)
        return FALSE;

    *pu4Akm = 0;

    /* special-case FT */
    if (eAuthMode == AUTH_MODE_WPA2_FT_PSK &&
        rsnSearchAKMSuite(prAdapter, RSN_AKM_SUITE_FT_PSK, &j, ucBssIndex)) {
        *pu4Akm = RSN_AKM_SUITE_FT_PSK;
        return TRUE;
    } else if (eAuthMode == AUTH_MODE_WPA2_FT &&
               rsnSearchAKMSuite(prAdapter, RSN_AKM_SUITE_FT_802_1X, &j, ucBssIndex)) {
        *pu4Akm = RSN_AKM_SUITE_FT_802_1X;
        return TRUE;
    }

#if CFG_SUPPORT_802_11W
    /* iterate backward if required by MFP policy in original code */
    for (ii = (int32_t)(prBssRsnInfo->u4AuthKeyMgtSuiteCount - 1); ii >= 0; ii--) {
        if (rsnSearchAKMSuite(prAdapter, prBssRsnInfo->au4AuthKeyMgtSuite[ii], &j, ucBssIndex)) {
            *pu4Akm = prBssRsnInfo->au4AuthKeyMgtSuite[ii];
            return TRUE;
        }
    }
#else
    for (i = 0; i < prBssRsnInfo->u4AuthKeyMgtSuiteCount; i++) {
        if (rsnSearchAKMSuite(prAdapter, prBssRsnInfo->au4AuthKeyMgtSuite[i], &j, ucBssIndex)) {
            *pu4Akm = prBssRsnInfo->au4AuthKeyMgtSuite[i];
            return TRUE;
        }
    }
#endif

    return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate WPA IE for beacon frame.
 *
 * \param[in] pucIeStartAddr Pointer to put the generated WPA IE.
 *
 * \return The append WPA-None IE length
 * \note
 *      Called by: JOIN module, compose beacon IE
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateWpaNoneIE(IN struct ADAPTER *prAdapter,
			  IN struct MSDU_INFO *prMsduInfo)
{
	uint32_t i;
	struct WPA_INFO_ELEM *prWpaIE;
	uint32_t u4Suite;
	uint16_t u2SuiteCount;
	uint8_t *cp, *cp2;
	uint8_t ucExpendedLen = 0;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;

	DEBUGFUNC("rsnGenerateWpaNoneIE");

	ASSERT(prMsduInfo);

	ucBssIndex = prMsduInfo->ucBssIndex;

	if (GET_BSS_INFO_BY_INDEX(prAdapter,
				  ucBssIndex)->eNetworkType != NETWORK_TYPE_AIS)
		return;

	if (aisGetAuthMode(prAdapter, ucBssIndex) != AUTH_MODE_WPA_NONE)
		return;

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	prWpaIE = (struct WPA_INFO_ELEM *)(pucBuffer);

	/* Start to construct a WPA IE. */
	/* Fill the Element ID field. */
	prWpaIE->ucElemId = ELEM_ID_WPA;

	/* Fill the OUI and OUI Type fields. */
	prWpaIE->aucOui[0] = 0x00;
	prWpaIE->aucOui[1] = 0x50;
	prWpaIE->aucOui[2] = 0xF2;
	prWpaIE->ucOuiType = VENDOR_OUI_TYPE_WPA;

	/* Fill the Version field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2Version, 1);	/* version 1 */
	ucExpendedLen = 6;

	/* Fill the Pairwise Key Cipher Suite List field. */
	u2SuiteCount = 0;
	cp = (uint8_t *) &prWpaIE->aucPairwiseKeyCipherSuite1[0];

	if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_CCMP, &i, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_CCMP;
	else if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_TKIP, &i, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_TKIP;
	else if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_WEP104, &i, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_WEP104;
	else if (rsnSearchSupportedCipher(prAdapter,
		WPA_CIPHER_SUITE_WEP40, &i, ucBssIndex))
		u4Suite = WPA_CIPHER_SUITE_WEP40;
	else
		u4Suite = WPA_CIPHER_SUITE_TKIP;

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;

	/*Add the length of aucPairwiseKeyCipherSuite1
	 * which should be sizeof u4RsnSelectedPairwiseCipher
	 */
	cp += 4;

	/* Fill the Group Key Cipher Suite field as
	 * the same in pair-wise key.
	 */
	WLAN_SET_FIELD_32(&prWpaIE->u4GroupKeyCipherSuite, u4Suite);
	ucExpendedLen += 4;

	/* Fill the Pairwise Key Cipher Suite Count field. */
	WLAN_SET_FIELD_16(&prWpaIE->u2PairwiseKeyCipherSuiteCount,
			  u2SuiteCount);
	ucExpendedLen += 2;

	cp2 = cp;

	/* Fill the Authentication and Key Management Suite
	 * List field.
	 */
	u2SuiteCount = 0;
	cp += 2;

	if (rsnSearchAKMSuite(prAdapter,
		WPA_AKM_SUITE_802_1X, &i, ucBssIndex))
		u4Suite = WPA_AKM_SUITE_802_1X;
	else if (rsnSearchAKMSuite(prAdapter,
		WPA_AKM_SUITE_PSK, &i, ucBssIndex))
		u4Suite = WPA_AKM_SUITE_PSK;
	else
		u4Suite = WPA_AKM_SUITE_NONE;

	/* This shall be the only available value for current implementation */
	ASSERT(u4Suite == WPA_AKM_SUITE_NONE);

	WLAN_SET_FIELD_32(cp, u4Suite);
	u2SuiteCount++;
	ucExpendedLen += 4;
	cp += 4;

	/* Fill the Authentication and Key Management Suite Count field. */
	WLAN_SET_FIELD_16(cp2, u2SuiteCount);
	ucExpendedLen += 2;

	/* Fill the Length field. */
	prWpaIE->ucLength = (uint8_t) ucExpendedLen;

	/* Increment the total IE length for the Element ID
	 * and Length fields.
	 */
	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);

}				/* rsnGenerateWpaNoneIE */

uint32_t _addWPAIE_impl(IN struct ADAPTER *prAdapter,
	IN OUT struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prAdapter->rWifiVar.fgReuseRSNIE)
		return FALSE;

	if (!prBssInfo)
		return FALSE;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return FALSE;

	/* AP only */
	if (!p2pFuncIsAPMode(
		prAdapter->rWifiVar.
		prP2PConnSettings[prBssInfo->u4PrivateData]))
		return FALSE;

	/* PMF only */
	if (prBssInfo->rApPmfCfg.fgMfpc)
		return FALSE;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.
		prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&
		(prP2pSpecBssInfo->u2WpaIeLen != 0)) {
		uint8_t *pucBuffer =
			(uint8_t *) ((unsigned long)
			prMsduInfo->prPacket + (unsigned long)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucWpaIeBuffer,
			prP2pSpecBssInfo->u2WpaIeLen);
		prMsduInfo->u2FrameLength += prP2pSpecBssInfo->u2WpaIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant WPA IE content w/o update\n");

		return TRUE;
	}

	return FALSE;
}


uint32_t _addRSNIE_impl(IN struct ADAPTER *prAdapter,
	IN OUT struct MSDU_INFO *prMsduInfo)
{
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo;
	struct BSS_INFO *prBssInfo;
	uint8_t ucBssIndex;

	ucBssIndex = prMsduInfo->ucBssIndex;
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prAdapter->rWifiVar.fgReuseRSNIE)
		return FALSE;

	if (!prBssInfo)
		return FALSE;

	/* AP + GO */
	if (!IS_BSS_APGO(prBssInfo))
		return FALSE;

	/* AP only */
	if (!p2pFuncIsAPMode(
		prAdapter->rWifiVar.
		prP2PConnSettings[prBssInfo->u4PrivateData]))
		return FALSE;

	/* PMF only */
	if (prBssInfo->rApPmfCfg.fgMfpc)
		return FALSE;

	prP2pSpecBssInfo =
		prAdapter->rWifiVar.
		prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	if (prP2pSpecBssInfo &&
		(prP2pSpecBssInfo->u2RsnIeLen != 0)) {
		uint8_t *pucBuffer =
			(uint8_t *) ((unsigned long)
			prMsduInfo->prPacket + (unsigned long)
			prMsduInfo->u2FrameLength);

		kalMemCopy(pucBuffer,
			prP2pSpecBssInfo->aucRsnIeBuffer,
			prP2pSpecBssInfo->u2RsnIeLen);
		prMsduInfo->u2FrameLength += prP2pSpecBssInfo->u2RsnIeLen;

		DBGLOG(RSN, INFO,
			"Keep supplicant RSN IE content w/o update\n");

		return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate WPA IE for
 *        associate request frame.
 *
 * \param[in]  prCurrentBss     The Selected BSS description
 *
 * \retval The append WPA IE length
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateWPAIE(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo)
{
	uint8_t *cp;
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;

	DEBUGFUNC("rsnGenerateWPAIE");

	ASSERT(prMsduInfo);

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);

	ucBssIndex = prMsduInfo->ucBssIndex;
	eAuthMode =
	    aisGetAuthMode(prAdapter, ucBssIndex);
	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar.
			prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */
	if (_addWPAIE_impl(prAdapter, prMsduInfo))
		return;

#if CFG_ENABLE_WIFI_DIRECT
	if ((prAdapter->fgIsP2PRegistered &&
	     GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_P2P &&
	     kalP2PGetTkipCipher(prAdapter->prGlueInfo,
				 (uint8_t) prBssInfo->u4PrivateData)) ||
	    (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_AIS &&
	     (eAuthMode ==
			AUTH_MODE_WPA ||
	      eAuthMode ==
			AUTH_MODE_WPA_PSK))) {
#else
	if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_AIS &&
	    (eAuthMode ==
			AUTH_MODE_WPA ||
	     eAuthMode ==
			AUTH_MODE_WPA_PSK)) {
#endif
		if (prAdapter->fgIsP2PRegistered && prP2pSpecificBssInfo
		    && (prP2pSpecificBssInfo->u2WpaIeLen != 0)) {
			kalMemCopy(pucBuffer,
				prP2pSpecificBssInfo->aucWpaIeBuffer,
				prP2pSpecificBssInfo->u2WpaIeLen);
			prMsduInfo->u2FrameLength +=
			    prP2pSpecificBssInfo->u2WpaIeLen;
			return;
		}
		/* Construct a WPA IE for association request frame. */
		WPA_IE(pucBuffer)->ucElemId = ELEM_ID_WPA;
		WPA_IE(pucBuffer)->ucLength = ELEM_ID_WPA_LEN_FIXED;
		WPA_IE(pucBuffer)->aucOui[0] = 0x00;
		WPA_IE(pucBuffer)->aucOui[1] = 0x50;
		WPA_IE(pucBuffer)->aucOui[2] = 0xF2;
		WPA_IE(pucBuffer)->ucOuiType = VENDOR_OUI_TYPE_WPA;
		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->u2Version, 1);

#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && GET_BSS_INFO_BY_INDEX(prAdapter,
			     ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(
				&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
				WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(
				&WPA_IE(pucBuffer)->u4GroupKeyCipherSuite,
				prBssInfo->
					u4RsnSelectedGroupCipher);

		cp = (uint8_t *) &
		    WPA_IE(pucBuffer)->aucPairwiseKeyCipherSuite1[0];

		WLAN_SET_FIELD_16(&WPA_IE(pucBuffer)->
			u2PairwiseKeyCipherSuiteCount, 1);
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
			&& GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(cp, WPA_CIPHER_SUITE_TKIP);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prBssInfo
						->u4RsnSelectedPairwiseCipher);

		/*Add the length of aucPairwiseKeyCipherSuite1
		 * which should be sizeof u4RsnSelectedPairwiseCipher
		 */
		cp += 4;

		WLAN_SET_FIELD_16(cp, 1);
		cp += 2;
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered
		    && GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
			eNetworkType == NETWORK_TYPE_P2P) {
			WLAN_SET_FIELD_32(cp, WPA_AKM_SUITE_PSK);
		} else
#endif
			WLAN_SET_FIELD_32(cp, prBssInfo
						->u4RsnSelectedAKMSuite);
		cp += 4;

		WPA_IE(pucBuffer)->ucLength = ELEM_ID_WPA_LEN_FIXED;

		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}

}				/* rsnGenerateWPAIE */

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate RSN IE for
 *        associate request frame.
 *
 * \param[in]  prMsduInfo     The Selected BSS description
 *
 * \retval The append RSN IE length
 *
 * \note
 *      Called by: AIS module, P2P module, BOW module Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateRSNIE(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo)
{
	uint8_t *cp;
	/* UINT_8                ucExpendedLen = 0; */
	uint8_t *pucBuffer;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	uint32_t u4GroupMgmt = 0;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
	struct CONNECTION_SETTINGS *prConnSettings = NULL;
#else
	struct PMKID_ENTRY *entry = NULL;
	struct STA_RECORD *prStaRec;
#endif

	DEBUGFUNC("rsnGenerateRSNIE");

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	DBGLOG_MEM8(RSN, ERROR,
		    pucBuffer,
		    IE_SIZE(pucBuffer));

	/* Todo:: network id */
	ucBssIndex = prMsduInfo->ucBssIndex;
	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	eAuthMode = aisGetAuthMode(prAdapter, ucBssIndex);

	/* For FT, we reuse the RSN Element composed in userspace */
	if (authAddRSNIE_impl(prAdapter, prMsduInfo))
	  return;

	if (_addRSNIE_impl(prAdapter, prMsduInfo))
	  return;

	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];


	DBGLOG(RSN, ERROR,
	       "TX RSN IE: group=%x pairwise=%x AKM=%x cap=%x",
	       prBssInfo->u4RsnSelectedGroupCipher,
	       prBssInfo->u4RsnSelectedPairwiseCipher,
	       prBssInfo->u4RsnSelectedAKMSuite,
	       prBssInfo->u2RsnSelectedCapInfo);


	if (
#if CFG_ENABLE_WIFI_DIRECT
		((prAdapter->fgIsP2PRegistered) &&
		 (GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType == NETWORK_TYPE_P2P)
		 && (kalP2PGetCcmpCipher(prAdapter->prGlueInfo,
			(uint8_t) prBssInfo->u4PrivateData))) ||
#endif
#if CFG_ENABLE_BT_OVER_WIFI
		(GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType == NETWORK_TYPE_BOW)
		||
#endif
		(GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType ==
		 NETWORK_TYPE_AIS /* prCurrentBss->fgIERSN */  &&
		 ((eAuthMode ==
		   AUTH_MODE_WPA2)
		  || (eAuthMode ==
		      AUTH_MODE_WPA2_PSK)
		  || (eAuthMode ==
		      AUTH_MODE_WPA2_FT_PSK)
		  || (eAuthMode ==
		      AUTH_MODE_WPA2_FT)
		|| (eAuthMode ==
		      AUTH_MODE_WPA3_SAE)))) {
		/* Construct a RSN IE for association request frame. */
		RSN_IE(pucBuffer)->ucElemId = ELEM_ID_RSN;
		RSN_IE(pucBuffer)->ucLength = ELEM_ID_RSN_LEN_FIXED;

		/* Version */
		WLAN_SET_FIELD_16(&RSN_IE(pucBuffer)->u2Version, 1);
		WLAN_SET_FIELD_32(&RSN_IE(pucBuffer)->u4GroupKeyCipherSuite,
				GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
				u4RsnSelectedGroupCipher);
				/* Group key suite */
		cp = (uint8_t *) &RSN_IE(
			     pucBuffer)->aucPairwiseKeyCipherSuite1[0];
		WLAN_SET_FIELD_16(&RSN_IE(
			pucBuffer)->u2PairwiseKeyCipherSuiteCount, 1);
		WLAN_SET_FIELD_32(cp, GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->u4RsnSelectedPairwiseCipher);
		/*Add the length of aucPairwiseKeyCipherSuite1
		 * which should be sizeof u4RsnSelectedPairwiseCipher
		 */
		cp += 4;

		if ((prBssInfo->eNetworkType == NETWORK_TYPE_P2P) &&
			(prBssInfo->u4RsnSelectedAKMSuite ==
			RSN_AKM_SUITE_SAE)) {
			struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo =
				prAdapter->rWifiVar.prP2pSpecificBssInfo
				[prBssInfo->u4PrivateData];
			uint8_t i = 0;

			/* AKM suite count */
			WLAN_SET_FIELD_16(cp,
				prP2pSpecBssInfo->u4KeyMgtSuiteCount);
			cp += 2;

			/* AKM suite */
			for (i = 0;
				i < prP2pSpecBssInfo->u4KeyMgtSuiteCount;
				i++) {
				DBGLOG(RSN, TRACE, "KeyMgtSuite 0x%04x\n",
					prP2pSpecBssInfo->au4KeyMgtSuite[i]);
				WLAN_SET_FIELD_32(cp,
					prP2pSpecBssInfo->au4KeyMgtSuite[i]);
				cp += 4;
			}

			RSN_IE(pucBuffer)->ucLength +=
				(prP2pSpecBssInfo->u4KeyMgtSuiteCount - 1) * 4;
		} else {
			WLAN_SET_FIELD_16(cp, 1);	/* AKM suite count */
			cp += 2;
			/* AKM suite */
			WLAN_SET_FIELD_32(cp, GET_BSS_INFO_BY_INDEX(prAdapter,
			    ucBssIndex)->u4RsnSelectedAKMSuite);
			cp += 4;
		}

		/* Capabilities */
		uint16_t rsn_cap = GET_BSS_INFO_BY_INDEX(prAdapter,
				  ucBssIndex)->u2RsnSelectedCapInfo;
		WLAN_SET_FIELD_16(cp, rsn_cap);
		DBGLOG(RSN, TRACE,
		       "Gen RSN IE = %x\n", GET_BSS_INFO_BY_INDEX(prAdapter,
				       ucBssIndex)->u2RsnSelectedCapInfo);
 #if CFG_SUPPORT_802_11W
		if (GET_BSS_INFO_BY_INDEX(prAdapter,
			ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS) {
			if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo,
				ucBssIndex) ==
			    RSN_AUTH_MFP_REQUIRED) {
				WLAN_SET_FIELD_16(cp,
					ELEM_WPA_CAP_MFPC | ELEM_WPA_CAP_MFPR | rsn_cap);
					/* Capabilities */
				DBGLOG(RSN, TRACE,
					"RSN_AUTH_MFP - MFPC & MFPR\n");
			} else if (kalGetRsnIeMfpCap(prAdapter->prGlueInfo,
				ucBssIndex) ==
				   RSN_AUTH_MFP_OPTIONAL) {
				WLAN_SET_FIELD_16(cp, ELEM_WPA_CAP_MFPC | (rsn_cap & ~ELEM_WPA_CAP_MFPR));
					/* Capabilities */
				DBGLOG(RSN, TRACE, "RSN_AUTH_MFP - MFPC\n");
			} else {
				DBGLOG(RSN, TRACE,
					"!RSN_AUTH_MFP - No MFPC!\n");
			}
		} else if ((GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->
				eNetworkType == NETWORK_TYPE_P2P) &&
			   (GET_BSS_INFO_BY_INDEX(prAdapter,
				ucBssIndex)->eCurrentOPMode ==
				(uint8_t) OP_MODE_ACCESS_POINT)) {
			/* AP PMF */
			/* for AP mode, keep origin RSN IE content w/o update */
		}
#endif
		cp += 2;

#if (CFG_SUPPORT_SUPPLICANT_SME == 1)
		prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
		ASSERT(prConnSettings);

		/*Fill PMKID and Group Management Cipher for AIS */
		if (GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->eNetworkType
				== NETWORK_TYPE_AIS) {
			if (prConnSettings->rRsnInfo.u2PmkidCnt > 0) {
				/* Fill PMKID Count field */
				WLAN_SET_FIELD_16(cp,
					prConnSettings->rRsnInfo.u2PmkidCnt);
				cp += 2;
				RSN_IE(pucBuffer)->ucLength += 2;
				/* Fill PMKID List field */
				kalMemCopy(cp,
					&prConnSettings->rRsnInfo.aucPmkidList,
					(prConnSettings->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN));
				DBGLOG(RSN, INFO,
					"Dump PMDID when gen rsn ie & len:%d\n",
					RSN_IE(pucBuffer)->ucLength);
				DBGLOG_MEM8(RSN, INFO, cp,
					(prConnSettings->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN));
				cp += (prConnSettings->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN);
				RSN_IE(pucBuffer)->ucLength +=
					(prConnSettings->rRsnInfo.u2PmkidCnt * RSN_PMKID_LEN);
			}
#if CFG_SUPPORT_802_11W
			else {
				/* Follow supplicant flow to
				 * fill PMKID Count field = 0 only when
				 * Group Management Cipher field
				 * need to be filled
				 */
				if (prAisSpecBssInfo->fgMgmtProtection) {
					WLAN_SET_FIELD_16(cp, 0);
					cp += 2;
					RSN_IE(pucBuffer)->ucLength += 2;
				}
			}

			/* Fill Group Management Cipher field */
			if (prAisSpecBssInfo->fgMgmtProtection) {
				u4GroupMgmt =
				prAdapter->prGlueInfo->rWpaInfo[ucBssIndex].u4CipherGroupMgmt;
				WLAN_SET_FIELD_32(cp, u4GroupMgmt);

				cp += 4;
				RSN_IE(pucBuffer)->ucLength += 4;
			}
#endif
		}
#else
		/* Fill PMKID and Group Management Cipher for AIS */
		if (GET_BSS_INFO_BY_INDEX(prAdapter,
				ucBssIndex)->eNetworkType == NETWORK_TYPE_AIS) {
			prStaRec = cnmGetStaRecByIndex(prAdapter,
						prMsduInfo->ucStaRecIndex);

			if (!prStaRec) {
				DBGLOG(RSN, ERROR, "prStaRec is NULL!");
			} else  {
				entry = rsnSearchPmkidEntry(prAdapter,
					prStaRec->aucMacAddr, ucBssIndex);
				if (prStaRec->ucAuthAlgNum ==
						AUTH_ALGORITHM_NUM_SAE) {
					DBGLOG(RSN, INFO,
						"Do not apply PMKID in RSNIE if auth type is SAE");
					entry = NULL;
				}
			}


			/* Fill PMKID Count and List field */
			if (entry) {
				uint8_t *pmk = entry->rBssidInfo.arPMKID;

				RSN_IE(pucBuffer)->ucLength = 38;
				/* Fill PMKID Count field */
				WLAN_SET_FIELD_16(cp, 1);
				cp += 2;
				DBGLOG(RSN, INFO, "BSSID " MACSTR
					"use PMKID " PMKSTR "\n",
					MAC2STR(entry->rBssidInfo.arBSSID),
					pmk[0], pmk[1], pmk[2], pmk[3], pmk[4],
					pmk[5], pmk[6], pmk[7],	pmk[8], pmk[9],
					pmk[10], pmk[11], pmk[12] + pmk[13],
					pmk[14], pmk[15]);
				/* Fill PMKID List field */
				kalMemCopy(cp, entry->rBssidInfo.arPMKID,
					IW_PMKID_LEN);
				cp += IW_PMKID_LEN;
			}
#if CFG_SUPPORT_802_11W
			else {
				/* Follow supplicant flow to
				 * fill PMKID Count field = 0 only when
				 * Group Management Cipher field
				 * need to be filled
				 */
				if (prAisSpecBssInfo
					->fgMgmtProtection) {
					WLAN_SET_FIELD_16(cp, 0)

					cp += 2;
					RSN_IE(pucBuffer)->ucLength += 2;
				}
			}

			/* Fill Group Management Cipher field */
			if (prAisSpecBssInfo->fgMgmtProtection) {
				u4GroupMgmt =
					kalGetRsnIeGroupMgmt(
						prAdapter->prGlueInfo,
						ucBssIndex);
				WLAN_SET_FIELD_32(cp, u4GroupMgmt);

				cp += 4;
				RSN_IE(pucBuffer)->ucLength += 4;
			}
#endif
		}
#endif
		prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	}

}				/* rsnGenerateRSNIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parse the given IE buffer and check if it is WFA IE and return Type
 *	and SubType for further process.
 *
 * \param[in] pucBuf             Pointer to buffer of WFA Information Element.
 * \param[out] pucOuiType        Pointer to the storage of OUI Type.
 * \param[out] pu2SubTypeVersion Pointer to the storage of OUI SubType
 *					and Version.
 * \retval TRUE  Parse IE ok
 * \retval FALSE Parse IE fail
 */
/*----------------------------------------------------------------------------*/
u_int8_t
rsnParseCheckForWFAInfoElem(IN struct ADAPTER *prAdapter,
			    IN uint8_t *pucBuf, OUT uint8_t *pucOuiType,
			    OUT uint16_t *pu2SubTypeVersion)
{
	uint8_t aucWfaOui[] = VENDOR_OUI_WFA;
	struct IE_WFA *prWfaIE;

	ASSERT(pucBuf);
	ASSERT(pucOuiType);
	ASSERT(pu2SubTypeVersion);
	prWfaIE = (struct IE_WFA *)pucBuf;

	do {
		if (IE_LEN(pucBuf) <= ELEM_MIN_LEN_WFA_OUI_TYPE_SUBTYPE) {
			break;
		} else if (prWfaIE->aucOui[0] != aucWfaOui[0] ||
			   prWfaIE->aucOui[1] != aucWfaOui[1]
			   || prWfaIE->aucOui[2] != aucWfaOui[2]) {
			break;
		}

		*pucOuiType = prWfaIE->ucOuiType;
		WLAN_GET_FIELD_16(&prWfaIE->aucOuiSubTypeVersion[0],
				  pu2SubTypeVersion);

		return TRUE;
	} while (FALSE);

	return FALSE;

}				/* end of rsnParseCheckForWFAInfoElem() */

#if CFG_SUPPORT_AAA
/*----------------------------------------------------------------------------*/
/*!
 * \brief Parse the given IE buffer and check if it is RSN IE with CCMP PSK
 *
 * \param[in] prAdapter             Pointer to Adapter
 * \param[in] prSwRfb               Pointer to the rx buffer
 * \param[in] pIE                   Pointer rthe buffer of Information Element.
 * \param[out] prStatusCode     Pointer to the return status code.

 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnParserCheckForRSNCCMPPSK(struct ADAPTER *prAdapter,
				 struct RSN_INFO_ELEM *prIe,
				 struct STA_RECORD *prStaRec,
				 uint16_t *pu2StatusCode)
{

	struct RSN_INFO rRsnIe;
	struct BSS_INFO *prBssInfo;
	uint8_t i;
	uint16_t statusCode;

	ASSERT(prAdapter);
	ASSERT(prIe);
	ASSERT(prStaRec);
	ASSERT(pu2StatusCode);

	*pu2StatusCode = STATUS_CODE_INVALID_INFO_ELEMENT;

	if (rsnParseRsnIE(prAdapter, prIe, &rRsnIe)) {
		if ((rRsnIe.u4PairwiseKeyCipherSuiteCount != 1)
		    || (rRsnIe.au4PairwiseKeyCipherSuite[0] !=
			RSN_CIPHER_SUITE_CCMP)) {
			*pu2StatusCode = STATUS_CODE_INVALID_PAIRWISE_CIPHER;
			return;
		}
		/* When softap's conf support both TKIP&CCMP,
		 * the Group Cipher Suite would be TKIP
		 * If we check the Group Cipher Suite == CCMP
		 * about peer's Asso Req
		 * The connection would be fail
		 * due to STATUS_CODE_INVALID_GROUP_CIPHER
		 */
		if (rRsnIe.u4GroupKeyCipherSuite != RSN_CIPHER_SUITE_CCMP &&
			!prAdapter->rWifiVar.fgReuseRSNIE) {
			*pu2StatusCode = STATUS_CODE_INVALID_GROUP_CIPHER;
			return;
		}

		if ((rRsnIe.u4AuthKeyMgtSuiteCount != 1)
			|| ((rRsnIe.au4AuthKeyMgtSuite[0] != RSN_AKM_SUITE_PSK)
#if CFG_SUPPORT_SOFTAP_WPA3
			&& (rRsnIe.au4AuthKeyMgtSuite[0] != RSN_AKM_SUITE_SAE)
#endif
			)) {
			DBGLOG(RSN, WARN, "RSN with invalid AKMP\n");
			*pu2StatusCode = STATUS_CODE_INVALID_AKMP;
			return;
		}

		DBGLOG(RSN, TRACE, "RSN with CCMP-PSK\n");
		*pu2StatusCode = WLAN_STATUS_SUCCESS;

#if CFG_SUPPORT_802_11W
		/* AP PMF */
		/* 1st check: if already PMF connection, reject assoc req:
		 * error 30 ASSOC_REJECTED_TEMPORARILY
		 */
		if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
			*pu2StatusCode = STATUS_CODE_ASSOC_REJECTED_TEMPORARILY;
			return;
		}

		/* if RSN capability not exist, just return */
		if (!rRsnIe.fgRsnCapPresent) {
			*pu2StatusCode = WLAN_STATUS_SUCCESS;
			return;
		}

		prStaRec->rPmfCfg.fgMfpc = (rRsnIe.u2RsnCap &
					    ELEM_WPA_CAP_MFPC) ? 1 : 0;
		prStaRec->rPmfCfg.fgMfpr = (rRsnIe.u2RsnCap &
					    ELEM_WPA_CAP_MFPR) ? 1 : 0;

		prStaRec->rPmfCfg.fgSaeRequireMfp = FALSE;

		for (i = 0; i < rRsnIe.u4AuthKeyMgtSuiteCount; i++) {
			if ((rRsnIe.au4AuthKeyMgtSuite[i] ==
			     RSN_AKM_SUITE_802_1X_SHA256) ||
			    (rRsnIe.au4AuthKeyMgtSuite[i] ==
			     RSN_AKM_SUITE_PSK_SHA256)) {
				DBGLOG(RSN, INFO, "STA SHA256 support\n");
				prStaRec->rPmfCfg.fgSha256 = TRUE;
				break;
			} else if (rRsnIe.au4AuthKeyMgtSuite[i] ==
				RSN_AKM_SUITE_SAE) {
				DBGLOG(RSN, INFO, "STA SAE support\n");
				prStaRec->rPmfCfg.fgSaeRequireMfp = TRUE;
				break;
			}
		}

		DBGLOG(RSN, INFO,
		       "STA Assoc req mfpc:%d, mfpr:%d, sha256:%d, bssIndex:%d, applyPmf:%d\n",
		       prStaRec->rPmfCfg.fgMfpc, prStaRec->rPmfCfg.fgMfpr,
		       prStaRec->rPmfCfg.fgSha256, prStaRec->ucBssIndex,
		       prStaRec->rPmfCfg.fgApplyPmf);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prStaRec->ucBssIndex);

		/* if PMF validation fail, return success as legacy association
		 */
		statusCode = rsnPmfCapableValidation(prAdapter, prBssInfo,
						     prStaRec);
		*pu2StatusCode = statusCode;
#endif
	}

}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate an authentication event to NDIS.
 *
 * \param[in] u4Flags Authentication event: \n
 *                     PARAM_AUTH_REQUEST_REAUTH 0x01 \n
 *                     PARAM_AUTH_REQUEST_KEYUPDATE 0x02 \n
 *                     PARAM_AUTH_REQUEST_PAIRWISE_ERROR 0x06 \n
 *                     PARAM_AUTH_REQUEST_GROUP_ERROR 0x0E \n
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void rsnGenMicErrorEvent(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prSta, IN u_int8_t fgFlags)
{
	struct PARAM_INDICATION_EVENT authEvent;
	struct BSS_INFO *prAisBssInfo;
	uint8_t ucBssIndex = 0;

	DEBUGFUNC("rsnGenMicErrorEvent");

	ucBssIndex = prSta->ucBssIndex;

	prAisBssInfo =
		aisGetAisBssInfo(prAdapter,
		ucBssIndex);

	/* Status type: Authentication Event */
	authEvent.rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

	/* Authentication request */
	authEvent.rAuthReq.u4Length = sizeof(struct PARAM_AUTH_REQUEST);
	COPY_MAC_ADDR(authEvent.rAuthReq.arBssid,
		prAisBssInfo->aucBSSID);
	if (fgFlags == TRUE)
		authEvent.rAuthReq.u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
	else
		authEvent.rAuthReq.u4Flags = PARAM_AUTH_REQUEST_PAIRWISE_ERROR;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (void *)&authEvent,
				     sizeof(struct PARAM_INDICATION_EVENT),
				     ucBssIndex);
}				/* rsnGenMicErrorEvent */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to handle TKIP MIC failures.
 *
 * \param[in] adapter_p Pointer to the adapter object data area.
 * \param[in] prSta Pointer to the STA which occur MIC Error
 * \param[in] fgErrorKeyType type of error key
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnTkipHandleMICFailure(IN struct ADAPTER *prAdapter,
			     IN struct STA_RECORD *prSta,
			     IN u_int8_t fgErrorKeyType)
{
	/* UINT_32               u4RsnaCurrentMICFailTime; */
	/* P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo; */

	DEBUGFUNC("rsnTkipHandleMICFailure");

	ASSERT(prAdapter);
#if 1
	rsnGenMicErrorEvent(prAdapter, prSta, fgErrorKeyType);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO,
	       "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);
#else
	ASSERT(prSta);

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter, prSta->ucBssIndex);

	/* Record the MIC error occur time. */
	GET_CURRENT_SYSTIME(&u4RsnaCurrentMICFailTime);

	/* Generate authentication request event. */
	DBGLOG(RSN, INFO,
	       "Generate TKIP MIC error event (type: 0%d)\n", fgErrorKeyType);

	/* If less than 60 seconds have passed since a previous TKIP MIC
	 * failure, disassociate from the AP and wait for 60 seconds
	 * before (re)associating with the same AP.
	 */
	if (prAisSpecBssInfo->u4RsnaLastMICFailTime != 0 &&
	    !CHECK_FOR_TIMEOUT(u4RsnaCurrentMICFailTime,
			       prAisSpecBssInfo->u4RsnaLastMICFailTime,
			       SEC_TO_SYSTIME(TKIP_COUNTERMEASURE_SEC))) {
		/* If less than 60 seconds expired since last MIC error,
		 * we have to block traffic.
		 */

		DBGLOG(RSN, INFO, "Start blocking traffic!\n");
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);

		secFsmEventStartCounterMeasure(prAdapter, prSta);
	} else {
		rsnGenMicErrorEvent(prAdapter, /* prSta, */ fgErrorKeyType);
		DBGLOG(RSN, INFO, "First TKIP MIC error!\n");
	}

	COPY_SYSTIME(prAisSpecBssInfo->u4RsnaLastMICFailTime,
		     u4RsnaCurrentMICFailTime);
#endif
}				/* rsnTkipHandleMICFailure */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to search the desired entry in
 *        PMKID cache according to the BSSID
 *
 * \param[in] pucBssid Pointer to the BSSID
 * \param[out] pu4EntryIndex Pointer to place the found entry index
 *
 * \retval TRUE, if found one entry for specified BSSID
 * \retval FALSE, if not found
 */
/*----------------------------------------------------------------------------*/
struct PMKID_ENTRY *rsnSearchPmkidEntry(IN struct ADAPTER *prAdapter,
			     IN uint8_t *pucBssid,
			     IN uint8_t ucBssIndex)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	ASSERT(pucBssid);

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter,
		ucBssIndex);
	cache = &prAisSpecBssInfo->rPmkidCache;

	LINK_FOR_EACH_ENTRY(entry, cache, rLinkEntry, struct PMKID_ENTRY) {
		if (EQUAL_MAC_ADDR(entry->rBssidInfo.arBSSID, pucBssid))
			return entry;
	}

	return NULL;
} /* rsnSearchPmkidEntry */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to check the BSS Desc at scan result
 *             with pre-auth cap at wpa2 mode. If there is no cache entry,
 *             notify the PMKID indication.
 *
 * \param[in] prBss The BSS Desc at scan result
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void rsnCheckPmkidCache(IN struct ADAPTER *prAdapter, IN struct BSS_DESC *prBss,
	IN uint8_t ucBssIndex)
{
	struct BSS_INFO *prAisBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct CONNECTION_SETTINGS *prConnSettings;

	if (!prBss)
		return;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	prAisBssInfo = aisGetAisBssInfo(prAdapter, ucBssIndex);
	prAisSpecBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);

	/* Generate pmkid candidate indications for other APs which are
	 * also belong to the same SSID with the current connected AP or
	 * beacon timeout AP but have no available pmkid.
	 */
	if ((prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED ||
	    (prAisBssInfo->eConnectionState == MEDIA_STATE_DISCONNECTED &&
		 aisFsmIsInProcessBeaconTimeout(prAdapter, ucBssIndex))) &&
	    prConnSettings->eAuthMode == AUTH_MODE_WPA2 &&
	    EQUAL_SSID(prBss->aucSSID, prBss->ucSSIDLen,
		prConnSettings->aucSSID, prConnSettings->ucSSIDLen) &&
	    UNEQUAL_MAC_ADDR(prBss->aucBSSID, prAisBssInfo->aucBSSID) &&
	    !rsnSearchPmkidEntry(prAdapter, prBss->aucBSSID,
	    ucBssIndex)) {
		struct PARAM_PMKID_CANDIDATE candidate;

		COPY_MAC_ADDR(candidate.arBSSID, prBss->aucBSSID);
		candidate.u4Flags = prBss->u2RsnCap & MASK_RSNIE_CAP_PREAUTH;
		rsnGeneratePmkidIndication(prAdapter, &candidate,
			ucBssIndex);

		DBGLOG(RSN, TRACE, "[%d] Generate " MACSTR
			" with preauth %d to pmkid candidate list\n",
			ucBssIndex,
			MAC2STR(prBss->aucBSSID), candidate.u4Flags);
	}
} /* rsnCheckPmkidCache */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to add/update pmkid.
 *
 * \param[in] prPmkid The new pmkid
 *
 * \return status
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnSetPmkid(IN struct ADAPTER *prAdapter,
		    IN struct PARAM_PMKID *prPmkid)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter,
		prPmkid->ucBssIdx);
	cache = &prAisSpecBssInfo->rPmkidCache;

	entry = rsnSearchPmkidEntry(prAdapter, prPmkid->arBSSID,
		prPmkid->ucBssIdx);
	if (!entry) {
		entry = kalMemAlloc(sizeof(struct PMKID_ENTRY), VIR_MEM_TYPE);
		if (!entry)
			return -ENOMEM;
		LINK_INSERT_TAIL(cache,	&entry->rLinkEntry);
	}

	DBGLOG(RSN, INFO,
		"[%d] Set " MACSTR ", total %d, PMKID " PMKSTR "\n",
		prPmkid->ucBssIdx,
		MAC2STR(prPmkid->arBSSID), cache->u4NumElem,
		prPmkid->arPMKID[0], prPmkid->arPMKID[1], prPmkid->arPMKID[2],
		prPmkid->arPMKID[3], prPmkid->arPMKID[4], prPmkid->arPMKID[5],
		prPmkid->arPMKID[6], prPmkid->arPMKID[7], prPmkid->arPMKID[8],
		prPmkid->arPMKID[9], prPmkid->arPMKID[10], prPmkid->arPMKID[11],
		prPmkid->arPMKID[12] + prPmkid->arPMKID[13],
		prPmkid->arPMKID[14], prPmkid->arPMKID[15]);

	kalMemCopy(&entry->rBssidInfo, prPmkid, sizeof(struct PARAM_PMKID));
	return WLAN_STATUS_SUCCESS;
} /* rsnSetPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to del pmkid.
 *
 * \param[in] prPmkid pmkid should be deleted
 *
 * \return status
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnDelPmkid(IN struct ADAPTER *prAdapter,
		    IN struct PARAM_PMKID *prPmkid)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	if (!prPmkid)
		return WLAN_STATUS_INVALID_DATA;

	DBGLOG(RSN, TRACE, "[%d] Del " MACSTR " pmkid\n",
		prPmkid->ucBssIdx,
		MAC2STR(prPmkid->arBSSID));

	prAisSpecBssInfo = aisGetAisSpecBssInfo(prAdapter,
		prPmkid->ucBssIdx);
	cache = &prAisSpecBssInfo->rPmkidCache;
	entry = rsnSearchPmkidEntry(prAdapter, prPmkid->arBSSID,
		prPmkid->ucBssIdx);
	if (entry) {
		if (kalMemCmp(prPmkid->arPMKID,
			entry->rBssidInfo.arPMKID, IW_PMKID_LEN)) {
			DBGLOG(RSN, WARN, "Del " MACSTR " pmkid but mismatch\n",
				MAC2STR(prPmkid->arBSSID));
		}
		LINK_REMOVE_KNOWN_ENTRY(cache, entry);
		kalMemFree(entry, VIR_MEM_TYPE, sizeof(struct PMKID_ENTRY));
	}

	return WLAN_STATUS_SUCCESS;
} /* rsnDelPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to delete all pmkid.
 *
 * \return status
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnFlushPmkid(IN struct ADAPTER *prAdapter, IN uint8_t ucBssIndex)
{
	struct AIS_SPECIFIC_BSS_INFO *prAisSpecBssInfo;
	struct PMKID_ENTRY *entry;
	struct LINK *cache;

	prAisSpecBssInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	cache = &prAisSpecBssInfo->rPmkidCache;

	DBGLOG(RSN, INFO, "[%d] Flush Pmkid total:%d\n",
		ucBssIndex,
		cache->u4NumElem);

	while (!LINK_IS_EMPTY(cache)) {
		LINK_REMOVE_HEAD(cache, entry, struct PMKID_ENTRY *);
		kalMemFree(entry, VIR_MEM_TYPE, sizeof(struct PMKID_ENTRY));
	}
	return WLAN_STATUS_SUCCESS;
} /* rsnDelPmkid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate an PMKID candidate list
 *        indication to NDIS.
 *
 * \param[in] prAdapter Pointer to the adapter object data area.
 * \param[in] u4Flags PMKID candidate list event:
 *                    PARAM_PMKID_CANDIDATE_PREAUTH_ENABLED 0x01
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void rsnGeneratePmkidIndication(IN struct ADAPTER *prAdapter,
				IN struct PARAM_PMKID_CANDIDATE *prCandi,
				IN uint8_t ucBssIndex)
{
	struct PARAM_INDICATION_EVENT pmkidEvent;

	DEBUGFUNC("rsnGeneratePmkidIndication");

	ASSERT(prAdapter);

	/* Status type: PMKID Candidatelist Event */
	pmkidEvent.rStatus.eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;
	kalMemCopy(&pmkidEvent.rCandi, prCandi,
		sizeof(struct PARAM_PMKID_CANDIDATE));

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (void *) &pmkidEvent,
				     sizeof(struct PARAM_INDICATION_EVENT),
				     ucBssIndex);
} /* rsnGeneratePmkidIndication */

#if CFG_SUPPORT_WPS2
/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate WSC IE for
 *        associate request frame.
 *
 * \param[in]  prCurrentBss     The Selected BSS description
 *
 * \retval The append WSC IE length
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateWSCIE(IN struct ADAPTER *prAdapter,
		      IN struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;
	struct CONNECTION_SETTINGS *prConnSettings = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	if (!IS_BSS_INDEX_AIS(prAdapter,
		prMsduInfo->ucBssIndex))
		return;

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	prConnSettings = aisGetConnSettings(prAdapter,
		prMsduInfo->ucBssIndex);

	/* ASSOC INFO IE ID: 221 :0xDD */
	if (prConnSettings->u2WSCAssocInfoIELen) {
		kalMemCopy(pucBuffer,
			   &prConnSettings->aucWSCAssocInfoIE,
			   prConnSettings->u2WSCAssocInfoIELen);
		prMsduInfo->u2FrameLength +=
		    prConnSettings->u2WSCAssocInfoIELen;
	}

}
#endif

#if CFG_SUPPORT_802_11W

/*----------------------------------------------------------------------------*/
/*!
 * \brief to check if the Bip Key installed or not
 *
 * \param[in]
 *           prAdapter
 *
 * \return
 *           TRUE
 *           FALSE
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnCheckBipKeyInstalled(IN struct ADAPTER
				 *prAdapter, IN struct STA_RECORD *prStaRec)
{
	/* caution: prStaRec might be null ! */
	if (prStaRec) {
		if (GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex)
		    ->eNetworkType == (uint8_t) NETWORK_TYPE_AIS) {
			return aisGetAisSpecBssInfo(prAdapter,
				prStaRec->ucBssIndex)
				->fgBipKeyInstalled;
		} else if ((GET_BSS_INFO_BY_INDEX(prAdapter,
				prStaRec->ucBssIndex)
				->eNetworkType == NETWORK_TYPE_P2P)
				&&
			(GET_BSS_INFO_BY_INDEX(prAdapter,
				prStaRec->ucBssIndex)
				->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
			if (prStaRec->rPmfCfg.fgApplyPmf)
				DBGLOG(RSN, INFO, "AP-STA PMF capable\n");
			return prStaRec->rPmfCfg.fgApplyPmf;
		} else {
			return FALSE;
		}
	} else
		return FALSE;

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to check the Sa query timeout.
 *
 *
 * \note
 *      Called by: AIS module, Handle by Sa Quert timeout
 */
/*----------------------------------------------------------------------------*/
uint8_t rsnCheckSaQueryTimeout(
	IN struct ADAPTER *prAdapter, IN uint8_t ucBssIdx)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct BSS_INFO *prAisBssInfo;
	uint32_t now;

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIdx);
	ASSERT(prBssSpecInfo);
	prAisBssInfo =
		aisGetAisBssInfo(prAdapter, ucBssIdx);

	GET_CURRENT_SYSTIME(&now);

	if (CHECK_FOR_TIMEOUT(now, prBssSpecInfo->u4SaQueryStart,
			      TU_TO_MSEC(1000))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		prBssSpecInfo->ucSaQueryTimedOut = 1;
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
		prBssSpecInfo->u4SaQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);
#if 1
		if (prAisBssInfo == NULL) {
			DBGLOG(RSN, ERROR, "prAisBssInfo is NULL");
		} else if (prAisBssInfo->eConnectionState ==
		    MEDIA_STATE_CONNECTED) {
			struct MSG_AIS_ABORT *prAisAbortMsg;

			prAisAbortMsg =
				(struct MSG_AIS_ABORT *) cnmMemAlloc(prAdapter,
						RAM_TYPE_MSG,
						sizeof(struct MSG_AIS_ABORT));
			if (!prAisAbortMsg)
				return 0;
			prAisAbortMsg->rMsgHdr.eMsgId = MID_SAA_AIS_FSM_ABORT;
			prAisAbortMsg->ucReasonOfDisconnect =
			    DISCONNECT_REASON_CODE_DISASSOCIATED;
			prAisAbortMsg->fgDelayIndication = FALSE;
			prAisAbortMsg->ucBssIndex = ucBssIdx;
			mboxSendMsg(prAdapter, MBOX_ID_0,
				    (struct MSG_HDR *) prAisAbortMsg,
				    MSG_SEND_METHOD_BUF);
		}
#else
		/* Re-connect */
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
#endif
		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query timer.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnStartSaQueryTimer(IN struct ADAPTER *prAdapter,
			  IN unsigned long ulParamPtr)
{
	struct BSS_INFO *prBssInfo;
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint16_t u2PayloadLen;
	uint8_t *pucTmp = NULL;
	uint8_t ucTransId[ACTION_SA_QUERY_TR_ID_LEN];
	uint8_t ucBssIndex = (uint8_t) ulParamPtr;

	prBssInfo = aisGetAisBssInfo(prAdapter,
		ucBssIndex);
	ASSERT(prBssInfo);

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	ASSERT(prBssSpecInfo);

	DBGLOG(RSN, INFO, "MFP: Start Sa Query\n");

	if (prBssInfo->prStaRecOfAP == NULL) {
		/* Add patch to resolve PMF 5.3.3.5 &
		 * PMF 5.4.3.1 test failure issue.
		 * Sa query count need to set to 0 if StaRec is removed
		 */
		if (prBssSpecInfo->u4SaQueryCount > 0)
			rsnStopSaQuery(prAdapter, ucBssIndex);
		DBGLOG(RSN, INFO, "MFP: unassociated AP!\n");
		return;
	}

	if (prBssSpecInfo->u4SaQueryCount > 0
	    && rsnCheckSaQueryTimeout(prAdapter,
	    ucBssIndex)) {
		DBGLOG(RSN, INFO, "MFP: u4SaQueryCount count =%d\n",
		       prBssSpecInfo->u4SaQueryCount);
		return;
	}

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_REQUEST;

	if (prBssSpecInfo->u4SaQueryCount == 0)
		GET_CURRENT_SYSTIME(&prBssSpecInfo->u4SaQueryStart);

	if (prBssSpecInfo->u4SaQueryCount) {
		pucTmp = kalMemAlloc(prBssSpecInfo->u4SaQueryCount *
				     ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
		if (!pucTmp) {
			DBGLOG(RSN, INFO,
			       "MFP: Fail to alloc tmp buffer for backup sa query id\n");
			cnmMgtPktFree(prAdapter, prMsduInfo);
			return;
		}
		kalMemCopy(pucTmp, prBssSpecInfo->pucSaQueryTransId,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
	}

	kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
		   prBssSpecInfo->u4SaQueryCount * ACTION_SA_QUERY_TR_ID_LEN);

	ucTransId[0] = (uint8_t) (kalRandomNumber() & 0xFF);
	ucTransId[1] = (uint8_t) (kalRandomNumber() & 0xFF);

	kalMemCopy(prTxFrame->ucTransId, ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	prBssSpecInfo->u4SaQueryCount++;

	prBssSpecInfo->pucSaQueryTransId =
	    kalMemAlloc(prBssSpecInfo->u4SaQueryCount *
			ACTION_SA_QUERY_TR_ID_LEN, VIR_MEM_TYPE);
	if (!prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(pucTmp, VIR_MEM_TYPE,
			   (prBssSpecInfo->u4SaQueryCount - 1) *
			   ACTION_SA_QUERY_TR_ID_LEN);
		DBGLOG(RSN, INFO,
		       "MFP: Fail to alloc buffer for sa query id list\n");
		cnmMgtPktFree(prAdapter, prMsduInfo);
		return;
	}

	if (pucTmp) {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, pucTmp,
			   (prBssSpecInfo->u4SaQueryCount - 1) *
			   ACTION_SA_QUERY_TR_ID_LEN);
		kalMemCopy(
			&prBssSpecInfo->pucSaQueryTransId[
				(prBssSpecInfo->u4SaQueryCount - 1)
				* ACTION_SA_QUERY_TR_ID_LEN],
				ucTransId, ACTION_SA_QUERY_TR_ID_LEN);
		kalMemFree(pucTmp, VIR_MEM_TYPE,
			   (prBssSpecInfo->u4SaQueryCount -
			    1) * ACTION_SA_QUERY_TR_ID_LEN);
	} else {
		kalMemCopy(prBssSpecInfo->pucSaQueryTransId, ucTransId,
			   ACTION_SA_QUERY_TR_ID_LEN);
	}

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "Set SA Query timer %d (%d Tu)",
	       prBssSpecInfo->u4SaQueryCount, 201);

	cnmTimerStartTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer,
			   TU_TO_MSEC(201));

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnStartSaQuery(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIdx)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIdx);
	ASSERT(prBssSpecInfo);
	DBGLOG(RSN, INFO, "prBssSpecInfo->u4SaQueryCount %d\n",
		prBssSpecInfo->u4SaQueryCount);

	if (prBssSpecInfo->u4SaQueryCount == 0)
		rsnStartSaQueryTimer(prAdapter, (unsigned long) ucBssIdx);
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to stop the 802.11w sa query.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnStopSaQuery(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIdx)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIdx);
	ASSERT(prBssSpecInfo);

	cnmTimerStopTimer(prAdapter, &prBssSpecInfo->rSaQueryTimer);

	if (prBssSpecInfo->pucSaQueryTransId) {
		kalMemFree(prBssSpecInfo->pucSaQueryTransId, VIR_MEM_TYPE,
			   prBssSpecInfo->u4SaQueryCount
				* ACTION_SA_QUERY_TR_ID_LEN);
		prBssSpecInfo->pucSaQueryTransId = NULL;
	}

	prBssSpecInfo->u4SaQueryCount = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnSaQueryRequest(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame = NULL;
	uint16_t u2PayloadLen;
	struct STA_RECORD *prStaRec;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint8_t ucBssIndex = secGetBssIdxByRfb(prAdapter,
		prSwRfb);

	prBssInfo = aisGetAisBssInfo(prAdapter,
		ucBssIndex);
	ASSERT(prBssInfo);

	if (!prSwRfb)
		return;

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)		/* Todo:: for not AIS check */
		return;

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Request from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo,
		ucBssIndex) ==
	    MEDIA_STATE_DISCONNECTED) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Ignore SA Query Request from unassociated STA "
		       MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Sending SA Query Response to " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prBssInfo->prStaRecOfAP))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId,
		   ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prBssInfo->prStaRecOfAP->ucBssIndex,
		     prBssInfo->prStaRecOfAP->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

#if 0
	/* 4 Update information of MSDU_INFO_T */
	/* Management frame */
	prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;
	prMsduInfo->ucStaRecIndex = prBssInfo->prStaRecOfAP->ucIndex;
	prMsduInfo->ucNetworkType = prBssInfo->ucNetTypeIndex;
	prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
	prMsduInfo->fgIs802_1x = FALSE;
	prMsduInfo->fgIs802_11 = TRUE;
	prMsduInfo->u2FrameLength = WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen;
	prMsduInfo->ucPID = nicAssignPID(prAdapter);
	prMsduInfo->pfTxDoneHandler = NULL;
	prMsduInfo->fgIsBasicRate = FALSE;
#endif
	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AIS module, Handle Rx mgmt request
 */
/*----------------------------------------------------------------------------*/
void rsnSaQueryAction(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct AIS_SPECIFIC_BSS_INFO *prBssSpecInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	uint32_t i;
	uint8_t ucBssIndex = secGetBssIdxByRfb(prAdapter,
		prSwRfb);

	prBssSpecInfo =
		aisGetAisSpecBssInfo(prAdapter, ucBssIndex);
	ASSERT(prBssSpecInfo);

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (!prStaRec)
		return;

	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
		       (unsigned long)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Unexpected SA Query Action %d\n",
		       prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Response from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */

	for (i = 0; i < prBssSpecInfo->u4SaQueryCount; i++) {
		if (kalMemCmp(prBssSpecInfo->pucSaQueryTransId +
			      i * ACTION_SA_QUERY_TR_ID_LEN,
			      prRxFrame->ucTransId,
			      ACTION_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= prBssSpecInfo->u4SaQueryCount) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: No matching SA Query transaction identifier found\n");
		return;
	}

	DBGLOG(RSN, INFO, "Reply to pending SA Query received\n");

	rsnStopSaQuery(prAdapter, ucBssIndex);
}
#endif

static u_int8_t rsnCheckWpaRsnInfo(struct BSS_INFO *prBss,
				   struct RSN_INFO *prWpaRsnInfo)
{
	uint32_t i = 0;

	if (prWpaRsnInfo->u4GroupKeyCipherSuite !=
	    prBss->u4RsnSelectedGroupCipher) {
		DBGLOG(RSN, INFO,
		       "GroupCipherSuite change, old=0x%04x, new=0x%04x\n",
		       prBss->u4RsnSelectedGroupCipher,
		       prWpaRsnInfo->u4GroupKeyCipherSuite);
		return TRUE;
	}
	for (; i < prWpaRsnInfo->u4AuthKeyMgtSuiteCount; i++)
		if (prBss->u4RsnSelectedAKMSuite ==
		    prWpaRsnInfo->au4AuthKeyMgtSuite[i])
			break;
	if (i == prWpaRsnInfo->u4AuthKeyMgtSuiteCount) {
		DBGLOG(RSN, INFO,
		       "KeyMgmt change, not find 0x%04x in new beacon\n",
		       prBss->u4RsnSelectedAKMSuite);
		return TRUE;
	}

	for (i = 0; i < prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount; i++)
		if (prBss->u4RsnSelectedPairwiseCipher ==
		    prWpaRsnInfo->au4PairwiseKeyCipherSuite[i])
			break;
	if (i == prWpaRsnInfo->u4PairwiseKeyCipherSuiteCount) {
		DBGLOG(RSN, INFO,
		       "Pairwise Cipher change, not find 0x%04x in new beacon\n",
		       prBss->u4RsnSelectedPairwiseCipher);
		return TRUE;
	}

	return FALSE;
}

u_int8_t rsnCheckSecurityModeChanged(
			struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo,
			struct BSS_DESC *prBssDesc)
{
	uint8_t ucBssIdx = 0;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	struct GL_WPA_INFO *prWpaInfo;

	if (!prBssInfo) {
		DBGLOG(RSN, ERROR, "Empty prBssInfo\n");
		return FALSE;
	}
	ucBssIdx = prBssInfo->ucBssIndex;
	eAuthMode = aisGetAuthMode(prAdapter, ucBssIdx);
	prWpaInfo = aisGetWpaInfo(prAdapter, ucBssIdx);

	switch (eAuthMode) {
	case AUTH_MODE_OPEN: /* original is open system */
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) &&
		    !prWpaInfo->fgPrivacyInvoke &&
		    !secIsWepBss(prAdapter, prBssInfo)
#if CFG_SUPPORT_WPS2
		    /* Don't check while WPS is in process */
		    && !aisGetConnSettings(prAdapter, ucBssIdx)->fgWpsActive
#endif
		) {
			DBGLOG(RSN, INFO, "security change, open->privacy\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_SHARED:	/* original is WEP */
	case AUTH_MODE_AUTO_SWITCH:
		if ((prBssDesc->u2CapInfo & CAP_INFO_PRIVACY) == 0) {
			DBGLOG(RSN, INFO, "security change, WEP->open\n");
			return TRUE;
		} else if (prBssDesc->fgIERSN || prBssDesc->fgIEWPA) {
			DBGLOG(RSN, INFO, "security change, WEP->WPA/WPA2\n");
			return TRUE;
		}
		break;
	case AUTH_MODE_WPA:	/*original is WPA */
	case AUTH_MODE_WPA_PSK:
	case AUTH_MODE_WPA_NONE:
		if (prBssDesc->fgIEWPA)
			return rsnCheckWpaRsnInfo(prBssInfo,
				&prBssDesc->rWPAInfo);
		DBGLOG(RSN, INFO, "security change, WPA->%s\n",
		       prBssDesc->fgIERSN ? "WPA2" :
		       (prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ?
				"WEP" : "OPEN"));
		return TRUE;
	case AUTH_MODE_WPA2:	/*original is WPA2 */
	case AUTH_MODE_WPA2_PSK:
	case AUTH_MODE_WPA2_FT:
	case AUTH_MODE_WPA2_FT_PSK:
	case AUTH_MODE_WPA3_SAE:
		if (prBssDesc->fgIERSN)
			return rsnCheckWpaRsnInfo(prBssInfo,
				&prBssDesc->rRSNInfo);
		DBGLOG(RSN, INFO, "security change, WPA2->%s\n",
		       prBssDesc->fgIEWPA ? "WPA" :
		       (prBssDesc->u2CapInfo & CAP_INFO_PRIVACY ?
				"WEP" : "OPEN"));
		return TRUE;
	default:
		DBGLOG(RSN, WARN, "unknowned eAuthMode=%d\n", eAuthMode);
		break;
	}
	return FALSE;
}

#if CFG_SUPPORT_AAA
#define WPS_DEV_OUI_WFA                 0x0050f204
#define ATTR_RESPONSE_TYPE              0x103b

#define ATTR_VERSION                    0x104a
#define ATTR_VENDOR_EXT                 0x1049
#define WPS_VENDOR_ID_WFA               14122

void rsnGenerateWSCIEForAssocRsp(struct ADAPTER *prAdapter,
				 struct MSDU_INFO *prMsduInfo)
{
	struct WIFI_VAR *prWifiVar = NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *)NULL;
	uint16_t u2IELen = 0;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);
	ASSERT(IS_NET_ACTIVE(prAdapter, prMsduInfo->ucBssIndex));

	prWifiVar = &(prAdapter->rWifiVar);
	ASSERT(prWifiVar);

	DBGLOG(RSN, TRACE, "WPS: Building WPS IE for (Re)Association Response");
	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);

	if (prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	u2IELen = kalP2PCalWSC_IELen(prAdapter->prGlueInfo, 3,
				     (uint8_t) prP2pBssInfo->u4PrivateData);

	kalP2PGenWSC_IE(prAdapter->prGlueInfo,
			3,
			(uint8_t *) ((unsigned long) prMsduInfo->prPacket +
				  (unsigned long) prMsduInfo->u2FrameLength),
			(uint8_t) prP2pBssInfo->u4PrivateData);
	prMsduInfo->u2FrameLength += (uint16_t) kalP2PCalWSC_IELen(
					prAdapter->prGlueInfo, 3,
					(uint8_t) prP2pBssInfo->u4PrivateData);
}

#endif

#if CFG_SUPPORT_802_11W
/* AP PMF */
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to validate setting if PMF connection capable
 *    or not. If AP MFPC=1, and STA MFPC=1, we let this as PMF connection
 *
 *
 * \return status code
 */
/*----------------------------------------------------------------------------*/
uint16_t rsnPmfCapableValidation(IN struct ADAPTER
				 *prAdapter, IN struct BSS_INFO *prBssInfo,
				 IN struct STA_RECORD *prStaRec)
{
	u_int8_t selfMfpc, selfMfpr, peerMfpc, peerMfpr;

	selfMfpc = prBssInfo->rApPmfCfg.fgMfpc;
	selfMfpr = prBssInfo->rApPmfCfg.fgMfpr;
	peerMfpc = prStaRec->rPmfCfg.fgMfpc;
	peerMfpr = prStaRec->rPmfCfg.fgMfpr;

	DBGLOG(RSN, INFO,
	       "AP mfpc:%d, mfpr:%d / STA mfpc:%d, mfpr:%d\n",
	       selfMfpc, selfMfpr, peerMfpc, peerMfpr);

	if ((selfMfpc == TRUE) && (peerMfpc == FALSE)) {
		if ((selfMfpr == TRUE) && (peerMfpr == FALSE)) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case 4\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}

		if (peerMfpr == TRUE) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case 7\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}

		if ((prBssInfo->u4RsnSelectedAKMSuite ==
			RSN_AKM_SUITE_SAE) &&
			prStaRec->rPmfCfg.fgSaeRequireMfp) {
			DBGLOG(RSN, ERROR,
				"PMF policy violation for case sae_require_mfp\n");
			return STATUS_CODE_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}
	}

	if ((selfMfpc == TRUE) && (peerMfpc == TRUE)) {
		DBGLOG(RSN, ERROR, "PMF Connection\n");
		prStaRec->rPmfCfg.fgApplyPmf = TRUE;
	}

	return STATUS_CODE_SUCCESSFUL;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to generate TIMEOUT INTERVAL IE
 *     for association resp.
 *     Add Timeout interval IE (56) when PMF invalid association.
 *
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void rsnPmfGenerateTimeoutIE(struct ADAPTER *prAdapter,
			     struct MSDU_INFO *prMsduInfo)
{
	struct IE_TIMEOUT_INTERVAL *prTimeout;
	struct STA_RECORD *prStaRec = NULL;

	ASSERT(prAdapter);
	ASSERT(prMsduInfo);

	prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	if (!prStaRec)
		return;

	prTimeout = (struct IE_TIMEOUT_INTERVAL *)
	    (((uint8_t *) prMsduInfo->prPacket) + prMsduInfo->u2FrameLength);

	/* only when PMF connection, and association error code is 30 */
	if ((rsnCheckBipKeyInstalled(prAdapter, prStaRec) == TRUE)
	    &&
	    (prStaRec->u2StatusCode ==
	     STATUS_CODE_ASSOC_REJECTED_TEMPORARILY)) {

		DBGLOG(RSN, INFO, "rsnPmfGenerateTimeoutIE TRUE\n");
		prTimeout->ucId = ELEM_ID_TIMEOUT_INTERVAL;
		prTimeout->ucLength = ELEM_MAX_LEN_TIMEOUT_IE;
		prTimeout->ucType = IE_TIMEOUT_INTERVAL_TYPE_ASSOC_COMEBACK;
		prTimeout->u4Value = 1 << 10;
		prMsduInfo->u2FrameLength += IE_SIZE(prTimeout);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to check the Sa query timeout.
 * check if total retry time is greater than 1000ms
 *
 * \retval 1: retry max timeout. 0: not timeout
 * \note
 *      Called by: AAA module, Handle by Sa Query timeout
 */
/*----------------------------------------------------------------------------*/
uint8_t rsnApCheckSaQueryTimeout(IN struct ADAPTER
				 *prAdapter, IN struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prBssInfo;
	uint32_t now;

	GET_CURRENT_SYSTIME(&now);

	if (CHECK_FOR_TIMEOUT(now, prStaRec->rPmfCfg.u4SAQueryStart,
			      TU_TO_MSEC(1000))) {
		DBGLOG(RSN, INFO, "association SA Query timed out\n");

		/* XXX PMF TODO how to report STA REC disconnect?? */
		/* when SAQ retry count timeout, clear this STA */
		prStaRec->rPmfCfg.ucSAQueryTimedOut = 1;
		prStaRec->rPmfCfg.u2TransactionID = 0;
		prStaRec->rPmfCfg.u4SAQueryCount = 0;
		cnmTimerStopTimer(prAdapter, &prStaRec->rPmfCfg.rSAQueryTimer);

		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prStaRec->ucBssIndex);

		/* refer to p2pRoleFsmRunEventRxDeauthentication */
		if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
			if (bssRemoveClient(prAdapter, prBssInfo, prStaRec)) {
				/* Indicate disconnect to Host. */
				p2pFuncDisconnect(prAdapter, prBssInfo,
					prStaRec, FALSE, 0);
				/* Deactive BSS if PWR is IDLE and no peer */
				if (IS_NET_PWR_STATE_IDLE(prAdapter,
					prBssInfo->ucBssIndex)
				    &&
				    (bssGetClientCount(prAdapter, prBssInfo)
					== 0)) {
					/* All Peer disconnected !!
					 * Stop BSS now!!
					 */
					p2pFuncStopComplete(prAdapter,
						prBssInfo);
				}
			}
		}

		return 1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w sa query timer.
 *    This routine is triggered every 201ms, and every time enter function,
 *    check max timeout
 *
 * \note
 *      Called by: AAA module, Handle TX SAQ request
 */
/*----------------------------------------------------------------------------*/
void rsnApStartSaQueryTimer(IN struct ADAPTER *prAdapter,
			    IN unsigned long ulParamPtr)
{
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) ulParamPtr;
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;
	uint16_t u2PayloadLen;

	ASSERT(prStaRec);

	DBGLOG(RSN, INFO, "MFP: AP Start Sa Query timer\n");

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

	if (prStaRec->rPmfCfg.u4SAQueryCount > 0
	    && rsnApCheckSaQueryTimeout(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO,
		       "MFP: retry max timeout, u4SaQueryCount count =%d\n",
		       prStaRec->rPmfCfg.u4SAQueryCount);
		return;
	}

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec))
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_REQUEST;

	if (prStaRec->rPmfCfg.u4SAQueryCount == 0)
		GET_CURRENT_SYSTIME(&prStaRec->rPmfCfg.u4SAQueryStart);

	/* if retry, transcation id ++ */
	if (prStaRec->rPmfCfg.u4SAQueryCount) {
		prStaRec->rPmfCfg.u2TransactionID++;
	} else {
		/* if first SAQ request, random pick transaction id */
		prStaRec->rPmfCfg.u2TransactionID =
		    (uint16_t) (kalRandomNumber() & 0xFFFF);
	}

	DBGLOG(RSN, INFO, "SAQ transaction id:%d\n",
	       prStaRec->rPmfCfg.u2TransactionID);

	/* trnsform U16 to U8 array */
	prTxFrame->ucTransId[0] = ((prStaRec->rPmfCfg.u2TransactionID
				& 0xff00) >> 8);
	prTxFrame->ucTransId[1] = ((prStaRec->rPmfCfg.u2TransactionID
				& 0x00ff) >> 0);

	prStaRec->rPmfCfg.u4SAQueryCount++;

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

	DBGLOG(RSN, INFO, "AP Set SA Query timer %d (%d Tu)\n",
	       prStaRec->rPmfCfg.u4SAQueryCount, 201);

	cnmTimerStartTimer(prAdapter,
			   &prStaRec->rPmfCfg.rSAQueryTimer, TU_TO_MSEC(201));

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to start the 802.11w TX SA query.
 *
 *
 * \note
 *      Called by: AAA module, Handle Tx action frame request
 */
/*----------------------------------------------------------------------------*/
void rsnApStartSaQuery(IN struct ADAPTER *prAdapter,
		       IN struct STA_RECORD *prStaRec)
{
	ASSERT(prStaRec);

	DBGLOG(RSN, INFO, "rsnApStartSaQuery\n");

	if (prStaRec) {
		cnmTimerStopTimer(prAdapter,
				  &prStaRec->rPmfCfg.rSAQueryTimer);
		cnmTimerInitTimer(prAdapter,
			  &prStaRec->rPmfCfg.rSAQueryTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)rsnApStartSaQueryTimer,
			  (unsigned long) prStaRec);

		if (prStaRec->rPmfCfg.u4SAQueryCount == 0)
			rsnApStartSaQueryTimer(prAdapter,
					       (unsigned long)prStaRec);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to stop the 802.11w SA query.
 *
 *
 * \note
 *      Called by: AAA module, stop TX SAQ if receive correct SAQ response
 */
/*----------------------------------------------------------------------------*/
void rsnApStopSaQuery(IN struct ADAPTER *prAdapter,
		      IN struct STA_RECORD *prStaRec)
{
	ASSERT(prStaRec);

	cnmTimerStopTimer(prAdapter, &prStaRec->rPmfCfg.rSAQueryTimer);
	prStaRec->rPmfCfg.u2TransactionID = 0;
	prStaRec->rPmfCfg.u4SAQueryCount = 0;
	prStaRec->rPmfCfg.ucSAQueryTimedOut = 0;
}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AAA module, Handle Rx action request
 */
/*----------------------------------------------------------------------------*/
void rsnApSaQueryRequest(IN struct ADAPTER *prAdapter,
			 IN struct SW_RFB *prSwRfb)
{
	struct BSS_INFO *prBssInfo;
	struct MSDU_INFO *prMsduInfo;
	struct ACTION_SA_QUERY_FRAME *prRxFrame = NULL;
	uint16_t u2PayloadLen;
	struct STA_RECORD *prStaRec;
	struct ACTION_SA_QUERY_FRAME *prTxFrame;

	if (!prSwRfb)
		return;

	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);
	if (!prStaRec)		/* Todo:: for not AIS check */
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);
	ASSERT(prBssInfo);

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	if (!prRxFrame)
		return;

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: AP Received SA Query Request from " MACSTR
	       "\n", MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	if (!rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: AP Ignore SA Query Request non-PMF STA "
		       MACSTR "\n", MAC2STR(prStaRec->aucMacAddr));
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Sending SA Query Response to " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	prMsduInfo = (struct MSDU_INFO *)cnmMgtPktAlloc(prAdapter,
							MAC_TX_RESERVED_FIELD +
							PUBLIC_ACTION_MAX_LEN);

	if (!prMsduInfo)
		return;

	/* drop cipher mismatch or unprotected UC */
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		if (IS_INCORRECT_SEC_RX_FRAME(prSwRfb,
			prRxFrame->aucDestAddr,
			prRxFrame->u2FrameCtrl) ||
			prSwRfb->fgIsCipherLenMS) {
			/* if cipher mismatch, or incorrect encrypt,
			 * just drop
			 */
			DBGLOG(RSN, ERROR, "drop SAQ req CM/CLM=1\n");
			return;
		}
	}

	prTxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    ((unsigned long)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

	prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
	if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
		prTxFrame->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
		DBGLOG(RSN, INFO, "AP SAQ resp set FC PF bit\n");
	}
	COPY_MAC_ADDR(prTxFrame->aucDestAddr, prStaRec->aucMacAddr);
	COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucBSSID);
	COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

	prTxFrame->ucCategory = CATEGORY_SA_QUERY_ACTION;
	prTxFrame->ucAction = ACTION_SA_QUERY_RESPONSE;

	kalMemCopy(prTxFrame->ucTransId, prRxFrame->ucTransId,
		   ACTION_SA_QUERY_TR_ID_LEN);

	u2PayloadLen = 2 + ACTION_SA_QUERY_TR_ID_LEN;

	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(prAdapter,
		     prMsduInfo,
		     prStaRec->ucBssIndex,
		     prStaRec->ucIndex,
		     WLAN_MAC_MGMT_HEADER_LEN,
		     WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen, NULL,
		     MSDU_RATE_MODE_AUTO);

	/* 4 Enqueue the frame to send this action frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to process the 802.11w sa query action frame.
 *
 *
 * \note
 *      Called by: AAA module, Handle Rx action request
 */
/*----------------------------------------------------------------------------*/
void rsnApSaQueryAction(IN struct ADAPTER *prAdapter, IN struct SW_RFB *prSwRfb)
{
	struct ACTION_SA_QUERY_FRAME *prRxFrame;
	struct STA_RECORD *prStaRec;
	uint16_t u2SwapTrID;

	prRxFrame = (struct ACTION_SA_QUERY_FRAME *)
	    prSwRfb->pvHeader;
	prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (prStaRec == NULL) {
		DBGLOG(RSN, INFO, "rsnApSaQueryAction: prStaRec is NULL");
		return;
	}

	DBGLOG(RSN, TRACE,
	       "AP PMF SAQ action enter from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));
	if (prSwRfb->u2PacketLen < ACTION_SA_QUERY_TR_ID_LEN) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Too short SA Query Action frame (len=%lu)\n",
		       (unsigned long)prSwRfb->u2PacketLen);
		return;
	}

	if (prRxFrame->ucAction == ACTION_SA_QUERY_REQUEST) {
		rsnApSaQueryRequest(prAdapter, prSwRfb);
		return;
	}

	if (prRxFrame->ucAction != ACTION_SA_QUERY_RESPONSE) {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: Unexpected SA Query Action %d\n",
		       prRxFrame->ucAction);
		return;
	}

	DBGLOG(RSN, INFO,
	       "IEEE 802.11: Received SA Query Response from " MACSTR "\n",
	       MAC2STR(prStaRec->aucMacAddr));

	DBGLOG_MEM8(RSN, INFO, prRxFrame->ucTransId, ACTION_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */
	/* transform to network byte order */
	u2SwapTrID = htons(prStaRec->rPmfCfg.u2TransactionID);
	if (kalMemCmp((uint8_t *) &u2SwapTrID, prRxFrame->ucTransId,
		      ACTION_SA_QUERY_TR_ID_LEN) == 0) {
		DBGLOG(RSN, INFO, "AP Reply to SA Query received\n");
		rsnApStopSaQuery(prAdapter, prStaRec);
	} else {
		DBGLOG(RSN, INFO,
		       "IEEE 802.11: AP No matching SA Query transaction identifier found\n");
	}

}

#endif /* CFG_SUPPORT_802_11W */

#if CFG_SUPPORT_PASSPOINT
u_int8_t rsnParseOsenIE(struct ADAPTER *prAdapter,
			struct IE_WFA_OSEN *prInfoElem,
			struct RSN_INFO *prOsenInfo)
{
	uint32_t i;
	int32_t u4RemainRsnIeLen;
	uint16_t u2Version = 0;
	uint16_t u2Cap = 0;
	uint32_t u4GroupSuite = RSN_CIPHER_SUITE_CCMP;
	uint16_t u2PairSuiteCount = 0;
	uint16_t u2AuthSuiteCount = 0;
	uint8_t *pucPairSuite = NULL;
	uint8_t *pucAuthSuite = NULL;
	uint8_t *cp;

	ASSERT(prOsenInfo);

	cp = ((uint8_t *) prInfoElem) + 6;
	u4RemainRsnIeLen = (int32_t) prInfoElem->ucLength - 4;
	do {
		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Group Key Cipher Suite field. */
		if (u4RemainRsnIeLen < 4) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in group cipher suite (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_32(cp, &u4GroupSuite);
		cp += 4;
		u4RemainRsnIeLen -= 4;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Pairwise Key Cipher Suite Count field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in pairwise cipher suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2PairSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Pairwise Key Cipher Suite List field. */
		i = (uint32_t) u2PairSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in pairwise cipher suite list (IE len: %d, Remain %u, Cnt %d GS %x)\n",
			       prInfoElem->ucLength, u4RemainRsnIeLen,
			       u2PairSuiteCount, u4GroupSuite);
			return FALSE;
		}

		pucPairSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the Authentication and Key Management Cipher Suite
		 * Count field.
		 */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in auth & key mgt suite count (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2AuthSuiteCount);
		cp += 2;
		u4RemainRsnIeLen -= 2;

		/* Parse the Authentication and Key Management Cipher Suite
		 * List field.
		 */
		i = (uint32_t) u2AuthSuiteCount * 4;
		if (u4RemainRsnIeLen < (int32_t) i) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in auth & key mgt suite list (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		pucAuthSuite = cp;

		cp += i;
		u4RemainRsnIeLen -= (int32_t) i;

		if (u4RemainRsnIeLen == 0)
			break;

		/* Parse the RSN u2Capabilities field. */
		if (u4RemainRsnIeLen < 2) {
			DBGLOG(RSN, WARN,
			       "Fail to parse RSN IE in RSN capabilities (IE len: %d)\n",
			       prInfoElem->ucLength);
			return FALSE;
		}

		WLAN_GET_FIELD_16(cp, &u2Cap);
	} while (FALSE);

	/* Save the RSN information for the BSS. */
	prOsenInfo->ucElemId = ELEM_ID_VENDOR;

	prOsenInfo->u2Version = 0;

	prOsenInfo->u4GroupKeyCipherSuite = u4GroupSuite;

	DBGLOG(RSN, TRACE,
	       "RSN: version %d, group key cipher suite 0x%x\n",
	       u2Version, SWAP32(u4GroupSuite));

	if (pucPairSuite) {
		/* The information about the pairwise key cipher suites
		 * is present.
		 */
		if (u2PairSuiteCount > MAX_NUM_SUPPORTED_CIPHER_SUITES)
			u2PairSuiteCount = MAX_NUM_SUPPORTED_CIPHER_SUITES;

		prOsenInfo->u4PairwiseKeyCipherSuiteCount =
		    (uint32_t) u2PairSuiteCount;

		for (i = 0; i < (uint32_t) u2PairSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucPairSuite,
				&prOsenInfo->au4PairwiseKeyCipherSuite[i]);
			pucPairSuite += 4;

			DBGLOG(RSN, TRACE,
			  "RSN: pairwise key cipher suite [%d]: 0x%x\n", i,
			  SWAP32(prOsenInfo->au4PairwiseKeyCipherSuite[i]));
		}
	} else {
		/* The information about the pairwise key cipher suites
		 * is not present. Use the default chipher suite for RSN: CCMP
		 */

		prOsenInfo->u4PairwiseKeyCipherSuiteCount = 1;
		prOsenInfo->au4PairwiseKeyCipherSuite[0] =
		    RSN_CIPHER_SUITE_CCMP;

		DBGLOG(RSN, WARN,
		       "No Pairwise Cipher Suite found, using default (CCMP)\n");
	}

	if (pucAuthSuite) {
		/* The information about the authentication
		 * and key management suites is present.
		 */

		if (u2AuthSuiteCount > MAX_NUM_SUPPORTED_AKM_SUITES)
			u2AuthSuiteCount = MAX_NUM_SUPPORTED_AKM_SUITES;

		prOsenInfo->u4AuthKeyMgtSuiteCount = (uint32_t)
		    u2AuthSuiteCount;

		for (i = 0; i < (uint32_t) u2AuthSuiteCount; i++) {
			WLAN_GET_FIELD_32(pucAuthSuite,
					  &prOsenInfo->au4AuthKeyMgtSuite[i]);
			pucAuthSuite += 4;

			DBGLOG(RSN, TRACE, "RSN: AKM suite [%d]: 0x%x\n", i
				SWAP32(prOsenInfo->au4AuthKeyMgtSuite[i]));
		}
	} else {
		/* The information about the authentication and
		 * key management suites is not present.
		 * Use the default AKM suite for RSN.
		 */
		prOsenInfo->u4AuthKeyMgtSuiteCount = 1;
		prOsenInfo->au4AuthKeyMgtSuite[0] = RSN_AKM_SUITE_802_1X;

		DBGLOG(RSN, WARN, "No AKM found, using default (802.1X)\n");
	}


	DBGLOG(RSN, ERROR,
	       "[RSN-GEN] group=%08x pairwise=%08x akm=%08x rsn_len=%u",
	       GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u4RsnSelectedGroupCipher,
	       GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u4RsnSelectedPairwiseCipher,
	       GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex)->u4RsnSelectedAKMSuite,
	       RSN_IE(pucBuffer)->ucLength);

	prOsenInfo->u2RsnCap = u2Cap;
#if CFG_SUPPORT_802_11W
	prOsenInfo->fgRsnCapPresent = TRUE;
#endif
	DBGLOG(RSN, TRACE, "RSN cap: 0x%04x\n", prOsenInfo->u2RsnCap);

	return TRUE;
}
#endif /* CFG_SUPPORT_PASSPOINT */

uint32_t rsnCalculateFTIELen(struct ADAPTER *prAdapter, uint8_t ucBssIdx,
			     struct STA_RECORD *prStaRec)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx);

	eAuthMode = aisGetAuthMode(prAdapter,
		ucBssIdx);

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prFTIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				 eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return 0;
	return IE_SIZE(prFtIEs->prFTIE);
}

void rsnGenerateFTIE(IN struct ADAPTER *prAdapter,
		     IN OUT struct MSDU_INFO *prMsduInfo)
{
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	uint8_t *pucBuffer =
		(uint8_t *)prMsduInfo->prPacket + prMsduInfo->u2FrameLength;
	uint32_t ucFtIeSize = 0;
	uint8_t ucBssIdx = prMsduInfo->ucBssIndex;
	struct FT_IES *prFtIEs = aisGetFtIe(prAdapter, ucBssIdx);

	eAuthMode = aisGetAuthMode(prAdapter,
		ucBssIdx);

	if (!IS_BSS_INDEX_VALID(ucBssIdx) ||
	    !IS_BSS_AIS(GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx)) ||
	    !prFtIEs->prFTIE || (eAuthMode != AUTH_MODE_WPA2_FT &&
				 eAuthMode != AUTH_MODE_WPA2_FT_PSK))
		return;
	ucFtIeSize = IE_SIZE(prFtIEs->prFTIE);
	prMsduInfo->u2FrameLength += ucFtIeSize;
	kalMemCopy(pucBuffer, prFtIEs->prFTIE, ucFtIeSize);
}
#if CFG_SUPPORT_OWE
/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate OWE IE for
 *        associate request frame.
 *
 * \param[in]  prAdapter	The Selected BSS description
 * \param[in]  prMsduInfo	MSDU packet buffer
 *
 * \retval N/A
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
void rsnGenerateOWEIE(IN struct ADAPTER *prAdapter,
		      IN OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;
	uint8_t ucLength;
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings =
		aisGetConnSettings(prAdapter, prMsduInfo->ucBssIndex);

	ucLength = prConnSettings->rOweInfo.ucLength + 2;

	DBGLOG(RSN, INFO, "rsnGenerateOWEIE\n");

	if (prConnSettings->rOweInfo.ucLength == 0)
		return;

	ASSERT(prMsduInfo);

	pucBuffer = (uint8_t *) ((unsigned long)
				 prMsduInfo->prPacket + (unsigned long)
				 prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);


	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */

	kalMemCopy(pucBuffer, &(prConnSettings->rOweInfo),
		   ucLength);
	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);

	DBGLOG_MEM8(RSN, INFO, pucBuffer, IE_SIZE(pucBuffer));

	return;

}

/*----------------------------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to calculate OWE IE length for
 *        associate request frame.
 *
 * \param[in]  prAdapter	Major data structure for driver operation
 * \param[in]  ucBssIndex	unused for this function
 * \param[in]  prStaRec		unused for this function
 *
 * \retval The append WPA IE length
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*----------------------------------------------------------------------------*/
uint32_t rsnCalOweIELen(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex, struct STA_RECORD *prStaRec)
{
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	if (prConnSettings->rOweInfo.ucLength != 0)
		return prConnSettings->rOweInfo.ucLength + 2;

	return 0;
}
#endif
#if CFG_SUPPORT_WPA3_H2E
/*-----------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to generate RSNXE for
 *        associate request frame.
 *
 * \param[in]  prAdapter	The Selected BSS description
 * \param[in]  prMsduInfo	MSDU packet buffer
 *
 * \retval N/A
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*--------------------------------------------------------*/
void rsnGenerateRSNXE(IN struct ADAPTER *prAdapter,
	IN OUT struct MSDU_INFO *prMsduInfo)
{
	uint8_t *pucBuffer;
	uint8_t ucLength;
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings =
		aisGetConnSettings(prAdapter, prMsduInfo->ucBssIndex);

	ucLength = prConnSettings->rRsnXE.ucLength + 2;

	DBGLOG(RSN, INFO, "rsnGenerateRSNXE\n");

	if (prConnSettings->rRsnXE.ucLength == 0)
		return;

	ASSERT(prMsduInfo);

	pucBuffer = (uint8_t *) ((unsigned long)
		prMsduInfo->prPacket + (unsigned long)
		prMsduInfo->u2FrameLength);

	ASSERT(pucBuffer);
	/* if (eNetworkId != NETWORK_TYPE_AIS_INDEX) */
	/* return; */
	kalMemCopy(pucBuffer, &(prConnSettings->rRsnXE),
		ucLength);
	prMsduInfo->u2FrameLength += IE_SIZE(pucBuffer);
	DBGLOG_MEM8(RSN, INFO, pucBuffer, IE_SIZE(pucBuffer));
	return;
}

/*-----------------------------------------------------------*/
/*!
 *
 * \brief This routine is called to calculate RSNXE length for
 *        associate request frame.
 *
 * \param[in]  prAdapter	Major data structure for driver operation
 * \param[in]  ucBssIndex	unused for this function
 * \param[in]  prStaRec		unused for this function
 *
 * \retval The append WPA IE length
 *
 * \note
 *      Called by: AIS module, Associate request
 */
/*-----------------------------------------------------------*/
uint32_t rsnCalRSNXELen(IN struct ADAPTER *prAdapter,
	IN uint8_t ucBssIndex, struct STA_RECORD *prStaRec)
{
	struct CONNECTION_SETTINGS *prConnSettings;

	prConnSettings = aisGetConnSettings(prAdapter, ucBssIndex);
	if (prConnSettings->rRsnXE.ucLength != 0)
		return prConnSettings->rRsnXE.ucLength + 2;

	return 0;
}
#endif
