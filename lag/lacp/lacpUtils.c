/*
 *  Copyright (c) 2013, 2014
 *      NES <nes.open.switch@gmail.com>
 *
 *  All rights reserved. This source file is the sole property of NES, and
 *  contain proprietary and confidential information related to NES.
 *
 *  Licensed under the NES RED License, Version 1.0 (the "License"); you may
 *  not use this file except in compliance with the License. You may obtain a
 *  copy of the License bundled along with this file. Any kind of reproduction
 *  or duplication of any part of this file which conflicts with the License
 *  without prior written consent from NES is strictly prohibited.
 *
 *  Unless required by applicable law and agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 */
//set ts=4 sw=4

#ifndef __LACP_UTILS_C__
#	define __LACP_UTILS_C__



#include "lacp_ext.h"
#include "lacpUtils.h"
#include "lacpProtoConsts.h"
#include "lacpProtoObjects.h"
#include "lag/lagMIB.h"
#include "if/ifMIB.h"

#include "lib/bitmap.h"
#include "lib/buffer.h"

#include <stdbool.h>
#include <stdint.h>


static bool
	dot3adAggPortLacp_init (dot3adAggPortData_t *poEntry);
static bool
	dot3adAggPortLacp_reset (dot3adAggPortData_t *poEntry);
static bool
	dot3adAggPortLacp_setCurrentTimer (
		dot3adAggPortData_t *poEntry, bool bDefault);
static bool
	dot3adAggPortLacp_setWaitTimer (dot3adAggPortData_t *poEntry);

static bool
	dot3adAggPortLacp_detachAggregator (dot3adAggPortData_t *poEntry);
static bool
	dot3adAggPortLacp_disableColx (dot3adAggPortData_t *poEntry);
static bool
	dot3adAggPortLacp_disableDisx (dot3adAggPortData_t *poEntry);

bool
	dot3adAggPortLacp_detach (dot3adAggPortData_t *poEntry);
bool
	dot3adAggPortLacp_lacpPduTx (dot3adAggPortData_t *poEntry);
bool
	dot3adAggPortLacp_rxInit (dot3adAggPortData_t *poEntry);
static bool
	dot3adAggPortLacp_lacpPduRx (
		dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu);

static bool
	dot3adAggPortLacp_checkPartnerInfoPdu (
		dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu);
static void
	dot3adAggPortLacp_updatePartnerInfo (
		dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu);

static bool
	dot3adAggPortLacp_setDefaults (dot3adAggPortData_t *poEntry);
static bool
	dot3adAggPortLacp_checkPortSelected (
		dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu);
static bool
	dot3adAggPortLacp_checkDefaultSelected (dot3adAggPortData_t *poEntry);


bool
dot3adAggLacpStatus_update (
	dot3adAggData_t *poEntry,
	uint8_t u8RowStatus)
{
	register bool bRetCode = false;
	
	switch (u8RowStatus)
	{
	case xRowStatus_active_c:
		/* TODO */
		break;
		
	case xRowStatus_notInService_c:
	case xRowStatus_destroy_c:
		/* TODO */
		break;
	}
	
	bRetCode = true;
	
// neAggRowStatus_update_cleanup:
	
	return bRetCode;
}

bool
dot3adAggPortLacpStatus_update (
	dot3adAggPortData_t *poEntry,
	uint8_t u8RowStatus)
{
	register bool bRetCode = false;
	
	switch (u8RowStatus)
	{
	case xRowStatus_active_c:
		if (!dot3adAggPortLacp_init (poEntry))
		{
			goto dot3adAggPortLacpStatus_update_cleanup;
		}
		
		/* TODO */
		
		if (xBitmap_getBit (poEntry->oNe.au8Flags, neAggPortFlags_lacp_c))
		{
			if (!ifRcvAddressTable_createRegister (poEntry->u32Index, poEntry->oPortX.au8ProtocolDA, poEntry->oPortX.u16ProtocolDA_len))
			{
				goto dot3adAggPortLacpStatus_update_cleanup;
			}
			
			/* TODO */
		}
		break;
		
	case xRowStatus_notInService_c:
	case xRowStatus_destroy_c:
		if (!dot3adAggPortLacp_reset (poEntry))
		{
			goto dot3adAggPortLacpStatus_update_cleanup;
		}
		
		if (xBitmap_getBit (poEntry->oNe.au8Flags, neAggPortFlags_lacp_c))
		{
			/* TODO */
			
			if (!ifRcvAddressTable_removeRegister (poEntry->u32Index, poEntry->oPortX.au8ProtocolDA, poEntry->oPortX.u16ProtocolDA_len))
			{
				goto dot3adAggPortLacpStatus_update_cleanup;
			}
		}
		
		/* TODO */
		break;
	}
	
	bRetCode = true;
	
dot3adAggPortLacpStatus_update_cleanup:
	
	return bRetCode;
}


bool
dot3adAggPortLacp_init (dot3adAggPortData_t *poEntry)
{
	dot3adAggPortLacp_setDefaults (poEntry);
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_expired_c, 0);
	
	poEntry->u8Selection = dot3adAggPortSelection_none_c;
	
	return true;
}

bool
dot3adAggPortLacp_reset (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	
	dot3adAggPortData_zeroOperState (poEntry);
	return true;
}

bool
dot3adAggPortLacp_stateUpdate (
	dot3adAggPortData_t *poEntry, bool bForce)
{
	register bool bRetCode = false;
	
	xBitmap_setBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_synchronization_c, 0);
	
	if (poEntry->u8OperStatus != xOperStatus_up_c || !poEntry->bFullDuplex)
	{
		dot3adAggPortLacp_setDefaults (poEntry);
		
		xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_expired_c, 0);
		xBitmap_setBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_aggregation_c, 0);
		
		poEntry->u8Selection = dot3adAggPortSelection_none_c;
	}
	else if (!bForce)
	{
		if (!dot3adAggPortLacp_rxInit (poEntry))
		{
			goto dot3adAggPortLacp_stateUpdate_cleanup;
		}
	}
	
	bRetCode = true;
	
dot3adAggPortLacp_stateUpdate_cleanup:
	
	return bRetCode;
}

bool
dot3adAggPortLacp_setCurrentTimer (
	dot3adAggPortData_t *poEntry, bool bDefault)
{
	/* TODO */
	return false;
}

bool
dot3adAggPortLacp_setWaitTimer (dot3adAggPortData_t *poEntry)
{
	poEntry->u8AggState = dot3adAggPortAggState_waiting_c;
	
	/* TODO */
	return false;
}

bool
dot3adAggPortLacp_detach (dot3adAggPortData_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!dot3adAggPortLacp_detachAggregator (poEntry))
	{
		goto dot3adAggPortLacp_detach_cleanup;
	}
	
	if (xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_collecting_c) &&
		!dot3adAggPortLacp_disableColx (poEntry))
	{
		goto dot3adAggPortLacp_detach_cleanup;
	}
	
	if (xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_distributing_c) &&
		!dot3adAggPortLacp_disableDisx (poEntry))
	{
		goto dot3adAggPortLacp_detach_cleanup;
	}
	
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_synchronization_c, 0);
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_distributing_c, 0);
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_collecting_c, 0);
	poEntry->u8AggState = dot3adAggPortAggState_detached_c;
	
	if (!dot3adAggPortLacp_lacpPduTx (poEntry))
	{
		goto dot3adAggPortLacp_detach_cleanup;
	}
	
	if ((poEntry->u8Selection == dot3adAggPortSelection_active_c ||
		 poEntry->u8Selection == dot3adAggPortSelection_standby_c) &&
		!dot3adAggPortLacp_setWaitTimer (poEntry))
	{
		goto dot3adAggPortLacp_detach_cleanup;
	}
	
	bRetCode = true;
	
dot3adAggPortLacp_detach_cleanup:
	
	return bRetCode;
}

bool
dot3adAggPortLacp_detachAggregator (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}

bool
dot3adAggPortLacp_attachAggregator (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}

bool
dot3adAggPortLacp_disableDisxColx (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}

bool
dot3adAggPortLacp_disableColx (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}

bool
dot3adAggPortLacp_enableColx (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}

bool
dot3adAggPortLacp_disableDisx (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}

bool
dot3adAggPortLacp_enableDisx (dot3adAggPortData_t *poEntry)
{
	/* TODO */
	return true;
}
bool
dot3adAggPortLacp_lacpPduTx (dot3adAggPortData_t *poEntry)
{
	register bool bRetCode = false;
	register LacpPdu_Lacp_t *poPdu = NULL;
	register void *pvBuffer = NULL;
	
	if ((poPdu = xBuffer_cAlloc (sizeof (*poPdu))) == NULL)
	{
		goto dot3adAggPortLacp_lacpPduTx_cleanup;
	}
	if ((pvBuffer = xBuffer_cAlloc (LacpPdu_Lacp_size_c)) == NULL)
	{
		goto dot3adAggPortLacp_lacpPduTx_cleanup;
	}
	
	register void *pvBufferOffset = pvBuffer;
	
	poPdu->oHeader.u8Type = IeeeSlowProtocolsType_lacp_c;
	poPdu->oHeader.u8Version = Lacp_Version1_c;
	LacpPduHeader_serialize (pvBufferOffset, &poPdu->oActor);
	pvBufferOffset += LacpPduHeader_size_c;
	
	poPdu->oActor.oHeader.u8Type = LacpTlv_Actor_c;
	poPdu->oActor.oHeader.u8Length = LacpTlv_Actor_size_c;
	poPdu->oActor.u16SystemPriority = poEntry->oPort.i32ActorSystemPriority;
	memcpy (poPdu->oActor.oSystemAddress, poEntry->oPort.au8ActorSystemID, sizeof (poPdu->oActor.oSystemAddress));
	poPdu->oActor.u16Key = poEntry->oPort.i32ActorOperKey;
	poPdu->oActor.u16PortPriority = poEntry->oPort.i32ActorPortPriority;
	poPdu->oActor.u16PortNumber = poEntry->oPort.i32ActorPort;
	xBitmap_copyFromRev (poPdu->oActor.au8State, poEntry->oPort.au8ActorOperState, dot3adAggPortState_bitMin, dot3adAggPortState_bitMax_c);
	LacpTlv_Actor_serialize (pvBufferOffset, &poPdu->oActor);
	pvBufferOffset += LacpTlv_Actor_size_c;
	
	poPdu->oPartner.oHeader.u8Type = LacpTlv_Partner_c;
	poPdu->oPartner.oHeader.u8Length = LacpTlv_Partner_size_c;
	poPdu->oPartner.u16SystemPriority = poEntry->oPort.i32PartnerOperSystemPriority;
	memcpy (poPdu->oPartner.oSystemAddress, poEntry->oPort.au8PartnerOperSystemID, sizeof (poPdu->oPartner.oSystemAddress));
	poPdu->oPartner.u16Key = poEntry->oPort.i32PartnerOperKey;
	poPdu->oPartner.u16PortPriority = poEntry->oPort.i32PartnerOperPortPriority;
	poPdu->oPartner.u16PortNumber = poEntry->oPort.i32PartnerOperPort;
	xBitmap_copyFromRev (poPdu->oPartner.au8State, poEntry->oPort.au8PartnerOperState, dot3adAggPortState_bitMin, dot3adAggPortState_bitMax_c);
	LacpTlv_Partner_serialize (pvBufferOffset, &poPdu->oPartner);
	pvBufferOffset += LacpTlv_Partner_size_c;
	
	poPdu->oCollector.oHeader.u8Type = LacpTlv_Collector_c;
	poPdu->oCollector.oHeader.u8Length = LacpTlv_Collector_size_c;
	poPdu->oCollector.u16MaxDelay = poEntry->i32CollectorMaxDelay;
	LacpTlv_Collector_serialize (pvBufferOffset, &poPdu->oCollector);
	pvBufferOffset += LacpTlv_Collector_size_c;
	
	poPdu->oTerminator.oHeader.u8Type = LacpTlv_Terminator_c;
	poPdu->oTerminator.oHeader.u8Length = LacpTlv_Terminator_size_c;
	LacpTlv_Terminator_serialize (pvBufferOffset, &poPdu->oTerminator);
	pvBufferOffset += LacpTlv_Terminator_size_c;
	
	pvBufferOffset += LacpPduTrailer_Lacp_size_c;
	
// 	if (!ethernet_portTx (pvBuffer, poEntry->u32Index))
// 	{
// 		goto dot3adAggPortLacp_lacpPduTx_cleanup;
// 	}
	pvBuffer = NULL;
	
	bRetCode = true;
	
dot3adAggPortLacp_lacpPduTx_cleanup:
	
	if (poPdu != NULL)
	{
		xBuffer_free (poPdu);
	}
	return bRetCode;
}

bool
dot3adAggPortLacp_rxInit (dot3adAggPortData_t *poEntry)
{
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_expired_c, 1);
	xBitmap_setBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_synchronization_c, 0);
	xBitmap_setBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_lacpTimeout_c, 0);
	
	if (!dot3adAggPortLacp_setCurrentTimer (poEntry, true))
	{
		return false;
	}
	
	return true;
}

void
dot3adAggPortLacp_processPduRx (lacpMessage_Pdu_t *pMessage)
{
	register dot3adAggPortData_t *poDot3adAggPortData = NULL;
	
	if (pMessage == NULL ||
		(pMessage->u8Type != IeeeSlowProtocolsType_lacp_c && pMessage->u8Type != IeeeSlowProtocolsType_marker_c))
	{
		goto dot3adAggPortLacp_processPduRx_cleanup;
	}
	
	if ((poDot3adAggPortData = dot3adAggPortData_getByIndex (pMessage->u32IfIndex)) == NULL)
	{
		goto dot3adAggPortLacp_processPduRx_cleanup;
	}
	
	if (poDot3adAggPortData->u8OperStatus != xOperStatus_up_c ||
		!poDot3adAggPortData->bFullDuplex)
	{
		goto dot3adAggPortLacp_processPduRx_cleanup;
	}
	
	
	switch (pMessage->u8Type)
	{
	default:
		poDot3adAggPortData->oStats.u32UnknownRx++;
		goto dot3adAggPortLacp_processPduRx_cleanup;
		
	case IeeeSlowProtocolsType_lacp_c:
		if (!dot3adAggPortLacp_lacpPduRx (poDot3adAggPortData, pMessage->pvData))
		{
			goto dot3adAggPortLacp_processPduRx_cleanup;
		}
		break;
		
	case IeeeSlowProtocolsType_marker_c:
		/* TODO */
		poDot3adAggPortData->oStats.u32MarkerPDUsRx++;
// 		poDot3adAggPortData->oStats.u32MarkerResponsePDUsRx++;
		break;
	}
	
dot3adAggPortLacp_processPduRx_cleanup:
	
	if (pMessage->pvData != NULL)
	{
		free (pMessage->pvData);
	}
	return;
}

bool
dot3adAggPortLacp_lacpPduRx (
	dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu)
{
	bool bRetCode = false;
	
	if (!dot3adAggPortLacp_checkPortSelected (poEntry, poPdu))
	{
		poEntry->u8Selection = dot3adAggPortSelection_none_c;
	}
	
	if (!dot3adAggPortLacp_checkPartnerInfoPdu (poEntry, poPdu) &&
		!dot3adAggPortLacp_lacpPduTx (poEntry))
	{
		goto dot3adAggPortLacp_lacpPduRx_cleanup;
	}
	
	dot3adAggPortLacp_updatePartnerInfo (poEntry, poPdu);
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_expired_c, 0);
	
	if (!dot3adAggPortLacp_setCurrentTimer (poEntry, false))
	{
		goto dot3adAggPortLacp_lacpPduRx_cleanup;
	}
	
	/* TODO */
	
	bRetCode = true;
	
dot3adAggPortLacp_lacpPduRx_cleanup:
	
	bRetCode ? poEntry->oStats.u32LACPDUsRx++: poEntry->oStats.u32IllegalRx++;
	return bRetCode;
}

bool
dot3adAggPortLacp_checkPartnerInfoPdu (
	dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu)
{
	return
		poPdu->oPartner.u16PortNumber == poEntry->oPort.i32ActorPort &&
		poPdu->oPartner.u16PortPriority == poEntry->oPort.i32ActorPortPriority &&
		memcmp (poPdu->oPartner.oSystemAddress, poEntry->oPort.au8ActorSystemID, sizeof (poPdu->oPartner.oSystemAddress)) == 0 &&
		poPdu->oPartner.u16SystemPriority == poEntry->oPort.i32ActorSystemPriority &&
		poPdu->oPartner.u16Key == poEntry->oPort.i32ActorOperKey &&
		(xBitmap_getBit (poPdu->oPartner.au8State, dot3adAggPortState_lacpActivity_c) != 0) ==
			(xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_lacpActivity_c) != 0) &&
		(xBitmap_getBit (poPdu->oPartner.au8State, dot3adAggPortState_lacpTimeout_c) != 0) ==
			(xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_lacpTimeout_c) != 0) &&
		(xBitmap_getBit (poPdu->oPartner.au8State, dot3adAggPortState_aggregation_c) != 0) ==
			(xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_aggregation_c) != 0) &&
		(xBitmap_getBit (poPdu->oPartner.au8State, dot3adAggPortState_synchronization_c) != 0) ==
			(xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_synchronization_c) != 0);
}

void
dot3adAggPortLacp_updatePartnerInfo (
	dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu)
{
	register bool bPartnerSynchronization = false;
	
	poEntry->oPort.i32PartnerOperPort = poPdu->oActor.u16PortNumber;
	poEntry->oPort.i32PartnerOperPortPriority = poPdu->oActor.u16PortPriority;
	memcpy (poEntry->oPort.au8PartnerOperSystemID, poPdu->oActor.oSystemAddress, sizeof (poEntry->oPort.au8PartnerOperSystemID));
	poEntry->oPort.i32PartnerOperSystemPriority = poPdu->oActor.u16SystemPriority;
	poEntry->oPort.i32PartnerOperKey = poPdu->oActor.u16Key;
	xBitmap_copyToRev (poEntry->oPort.au8PartnerOperState, poPdu->oActor.au8State, dot3adAggPortState_bitMin, dot3adAggPortState_bitMax_c);
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_defaulted_c, 0);
	
	if (poPdu->oPartner.u16PortNumber == poEntry->oPort.i32ActorPort &&
		poPdu->oPartner.u16PortPriority == poEntry->oPort.i32ActorPortPriority &&
		memcmp (poPdu->oPartner.oSystemAddress, poEntry->oPort.au8ActorSystemID, sizeof (poPdu->oPartner.oSystemAddress)) == 0 &&
		poPdu->oPartner.u16SystemPriority == poEntry->oPort.i32ActorSystemPriority &&
		poPdu->oPartner.u16Key == poEntry->oPort.i32ActorOperKey)
	{
		register bool bPduActorSynchronization = xBitmap_getBit (poPdu->oActor.au8State, dot3adAggPortState_synchronization_c) != 0;
		register bool bPduActorAggregation = xBitmap_getBit (poPdu->oActor.au8State, dot3adAggPortState_aggregation_c) != 0;
		register bool bPduPartnerAggregation = xBitmap_getBit (poPdu->oPartner.au8State, dot3adAggPortState_aggregation_c) != 0;
		register bool bActorAggregation = xBitmap_getBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_aggregation_c) != 0;
		
		if ((!bPduActorAggregation && bPduActorSynchronization) || (bPduPartnerAggregation == bActorAggregation))
		{
			bPartnerSynchronization = true;
		}
	}
	
	xBitmap_setBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_synchronization_c, bPartnerSynchronization);
	
	return;
}

void
dot3adAggPortLacp_expirePartnerInfo (dot3adAggPortData_t *poEntry)
{
	if (!dot3adAggPortLacp_checkDefaultSelected (poEntry))
	{
		poEntry->u8Selection = dot3adAggPortSelection_none_c;
	}
	
	dot3adAggPortLacp_setDefaults (poEntry);
	xBitmap_setBitRev (poEntry->oPort.au8ActorOperState, dot3adAggPortState_expired_c, 0);
	return;
}

bool
dot3adAggPortLacp_setDefaults (dot3adAggPortData_t *poEntry)
{
	poEntry->oPort.i32PartnerOperSystemPriority = poEntry->oPort.i32PartnerAdminSystemPriority;
	memcpy (poEntry->oPort.au8PartnerOperSystemID, poEntry->oPort.au8PartnerAdminSystemID, sizeof (poEntry->oPort.au8PartnerOperSystemID));
	poEntry->oPort.i32PartnerOperKey = poEntry->oPort.i32PartnerAdminKey;
	poEntry->oPort.i32PartnerOperPortPriority = poEntry->oPort.i32PartnerAdminPortPriority;
	poEntry->oPort.i32PartnerOperPort = poEntry->oPort.i32PartnerAdminPort;
	memcpy (poEntry->oPort.au8PartnerOperState, poEntry->oPort.au8PartnerAdminState, sizeof (poEntry->oPort.au8PartnerOperState));
	
	xBitmap_setBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortActorOperState_defaulted_c, 1);
	return true;
}

bool
dot3adAggPortLacp_checkPortSelected (
	dot3adAggPortData_t *poEntry, LacpPdu_Lacp_t *poPdu)
{
	return
		poPdu->oActor.u16PortNumber == poEntry->oPort.i32PartnerOperPort &&
		poPdu->oActor.u16PortPriority == poEntry->oPort.i32PartnerOperPortPriority &&
		memcmp (poPdu->oActor.oSystemAddress, poEntry->oPort.au8PartnerOperSystemID, sizeof (poPdu->oActor.oSystemAddress)) == 0 &&
		poPdu->oActor.u16SystemPriority == poEntry->oPort.i32PartnerOperSystemPriority &&
		poPdu->oActor.u16Key == poEntry->oPort.i32PartnerOperKey &&
		(xBitmap_getBit (poPdu->oActor.au8State, dot3adAggPortState_aggregation_c) != 0) ==
			(xBitmap_getBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_aggregation_c) != 0);
}

bool
dot3adAggPortLacp_checkDefaultSelected (dot3adAggPortData_t *poEntry)
{
	return
		poEntry->oPort.i32PartnerAdminPort == poEntry->oPort.i32PartnerOperPort &&
		poEntry->oPort.i32PartnerAdminPortPriority == poEntry->oPort.i32PartnerOperPortPriority &&
		memcmp (poEntry->oPort.au8PartnerAdminSystemID, poEntry->oPort.au8PartnerOperSystemID, sizeof (poEntry->oPort.au8PartnerAdminSystemID)) == 0 &&
		poEntry->oPort.i32PartnerAdminSystemPriority == poEntry->oPort.i32PartnerOperSystemPriority &&
		poEntry->oPort.i32PartnerAdminKey == poEntry->oPort.i32PartnerOperKey &&
		(xBitmap_getBitRev (poEntry->oPort.au8PartnerAdminState, dot3adAggPortState_aggregation_c) != 0) ==
			(xBitmap_getBitRev (poEntry->oPort.au8PartnerOperState, dot3adAggPortState_aggregation_c) != 0);
}



#endif	// __LACP_UTILS_C__