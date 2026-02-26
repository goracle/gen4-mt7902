/* SPDX-License-Identifier: GPL-2.0 */
/* 
* Copyright (c) 2021 MediaTek Inc.
*/

/* \file   nic_uni_cmd_event.c
 *    \brief  Callback functions for Command packets.
 *
 *	Various Event packet handlers which will be setup in the callback
 *  function of a command packet.
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
 * \brief command packet generation utility
 *
 * \param[in] prAdapter          Pointer to the Adapter structure.
 * \param[in] ucCID              Command ID
 * \param[in] fgSetQuery         Set or Query
 * \param[in] fgNeedResp         Need for response
 * \param[in] pfCmdDoneHandler   Function pointer when command is done
 * \param[in] u4SetQueryInfoLen  The length of the set/query buffer
 * \param[in] pucInfoBuffer      Pointer to set/query buffer
 *
 *
 * \retval WLAN_STATUS_PENDING
 * \retval WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*!
* \brief command packet generation utility
*
* \param[in] prAdapter	   Pointer to the Adapter structure.
* \param[in] ucCID		   Command ID
* \param[in] fgSetQuery	   Set or Query
* \param[in] fgNeedResp	   Need for response
* \param[in] pfCmdDoneHandler   Function pointer when command is done
* \param[in] u4SetQueryInfoLen  The length of the set/query buffer
* \param[in] pucInfoBuffer	   Pointer to set/query buffer
*
*
* \retval WLAN_STATUS_PENDING
* \retval WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/









