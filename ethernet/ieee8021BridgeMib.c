/*
 *  Copyright (c) 2008-2016
 *      NES Repo <nes.repo@gmail.com>
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

#define SNMP_SRC

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "ethernetUtils.h"
#include "neIeee8021BridgeMIB.h"
#include "ieee8021BridgeMib.h"
#include "if/ifMIB.h"

#include "system_ext.h"
#include "ethernet_ext.h"

#include "lib/bitmap.h"
#include "lib/freeRange.h"
#include "lib/binaryTree.h"
#include "lib/buffer.h"
#include "lib/sync.h"
#include "lib/snmp.h"

#include <stdbool.h>
#include <stdint.h>

#define ROLLBACK_BUFFER "ROLLBACK_BUFFER"



static oid ieee8021BridgeMib_oid[] = {1,3,111,2,802,1,1,2};

static oid ieee8021BridgeBaseTable_oid[] = {1,3,111,2,802,1,1,2,1,1,1};
static oid ieee8021BridgeBasePortTable_oid[] = {1,3,111,2,802,1,1,2,1,1,4};
static oid ieee8021BridgeBaseIfToPortTable_oid[] = {1,3,111,2,802,1,1,2,1,1,5};
static oid ieee8021BridgePhyPortTable_oid[] = {1,3,111,2,802,1,1,2,1,1,6};
static oid ieee8021BridgeTpPortTable_oid[] = {1,3,111,2,802,1,1,2,1,2,1};
static oid ieee8021BridgePortPriorityTable_oid[] = {1,3,111,2,802,1,1,2,1,3,1};
static oid ieee8021BridgeUserPriorityRegenTable_oid[] = {1,3,111,2,802,1,1,2,1,3,2};
static oid ieee8021BridgeTrafficClassTable_oid[] = {1,3,111,2,802,1,1,2,1,3,3};
static oid ieee8021BridgePortOutboundAccessPriorityTable_oid[] = {1,3,111,2,802,1,1,2,1,3,4};
static oid ieee8021BridgePortDecodingTable_oid[] = {1,3,111,2,802,1,1,2,1,3,5};
static oid ieee8021BridgePortEncodingTable_oid[] = {1,3,111,2,802,1,1,2,1,3,6};
static oid ieee8021BridgeServiceAccessPriorityTable_oid[] = {1,3,111,2,802,1,1,2,1,3,7};
static oid ieee8021BridgePortMrpTable_oid[] = {1,3,111,2,802,1,1,2,1,4,1};
static oid ieee8021BridgePortMmrpTable_oid[] = {1,3,111,2,802,1,1,2,1,5,1};
static oid ieee8021BridgeILanIfTable_oid[] = {1,3,111,2,802,1,1,2,1,6,1};
static oid ieee8021BridgeDot1dPortTable_oid[] = {1,3,111,2,802,1,1,2,1,7,1};



/**
 *	initialize ieee8021BridgeMib group mapper
 */
void
ieee8021BridgeMib_init (void)
{
	extern oid ieee8021BridgeMib_oid[];
	
	DEBUGMSGTL (("ieee8021BridgeMib", "Initializing\n"));
	
	
	/* register ieee8021BridgeMib group table mappers */
	ieee8021BridgeBaseTable_init ();
	ieee8021BridgeBasePortTable_init ();
	ieee8021BridgeBaseIfToPortTable_init ();
	ieee8021BridgePhyPortTable_init ();
	ieee8021BridgeTpPortTable_init ();
	ieee8021BridgePortPriorityTable_init ();
	ieee8021BridgeUserPriorityRegenTable_init ();
	ieee8021BridgeTrafficClassTable_init ();
	ieee8021BridgePortOutboundAccessPriorityTable_init ();
	ieee8021BridgePortDecodingTable_init ();
	ieee8021BridgePortEncodingTable_init ();
	ieee8021BridgeServiceAccessPriorityTable_init ();
	ieee8021BridgePortMrpTable_init ();
	ieee8021BridgePortMmrpTable_init ();
	ieee8021BridgeILanIfTable_init ();
	ieee8021BridgeDot1dPortTable_init ();
	
	/* register ieee8021BridgeMib modules */
	sysORTable_createRegister ("ieee8021BridgeMib", ieee8021BridgeMib_oid, OID_LENGTH (ieee8021BridgeMib_oid));
	
	xFreeRange_createRange (&oBridge.oComponent_FreeRange, ieee8021BridgeBaseComponent_start_c, ieee8021BridgeBaseComponent_end_c);
}


ieee8021Bridge_t oBridge =
{
	.oComponent_FreeRange = xFreeRange_initInline (),
	.oComponentLock = xRwLock_initInline (),
	.oPhyPortLock = xRwLock_initInline (),
};

const uint8_t ieee8021BridgePcpEncodingTable [ieee8021BridgePriorityCodePoint_count_c][ieee8021BridgePriority_count_c][ieee8021BridgeDEI_count_c] = {
	{{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}},
	{{0, 0}, {1, 1}, {2, 2}, {3, 3}, {5, 4}, {5, 4}, {6, 6}, {7, 7}},
	{{0, 0}, {1, 1}, {3, 2}, {3, 2}, {5, 4}, {5, 4}, {6, 6}, {7, 7}},
	{{1, 0}, {1, 0}, {3, 2}, {3, 2}, {5, 4}, {5, 4}, {6, 6}, {7, 7}},
};
const ieee8021BridgePcp_t ieee8021BridgePcpDecodingTable [ieee8021BridgePriorityCodePoint_count_c][ieee8021BridgePriority_count_c] = {
	{{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}},
	{{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 4}, {0, 4}, {0, 6}, {0, 7}},
	{{0, 0}, {0, 1}, {1, 2}, {0, 2}, {1, 4}, {0, 4}, {0, 6}, {0, 7}},
	{{1, 0}, {0, 0}, {1, 2}, {0, 2}, {1, 4}, {0, 4}, {0, 6}, {0, 7}},
};


/**
 *	table mapper(s) & helper(s)
 */
/** initialize ieee8021BridgeBaseTable table mapper **/
void
ieee8021BridgeBaseTable_init (void)
{
	extern oid ieee8021BridgeBaseTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeBaseTable", &ieee8021BridgeBaseTable_mapper,
		ieee8021BridgeBaseTable_oid, OID_LENGTH (ieee8021BridgeBaseTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBaseComponentId */,
		0);
	table_info->min_column = IEEE8021BRIDGEBASEBRIDGEADDRESS;
	table_info->max_column = IEEE8021BRIDGEBASEROWSTATUS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeBaseTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeBaseTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeBaseTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeBaseTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeBaseEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeBaseEntry_t, oBTreeNode);
	register ieee8021BridgeBaseEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeBaseEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId) ? 0: 1;
}

static int8_t
ieee8021BridgeBaseTable_Chassis_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeBaseEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeBaseEntry_t, oChassis_BTreeNode);
	register ieee8021BridgeBaseEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeBaseEntry_t, oChassis_BTreeNode);
	
	return
		(pEntry1->u32ChassisId < pEntry2->u32ChassisId) ||
		(pEntry1->u32ChassisId == pEntry2->u32ChassisId && pEntry1->u32ComponentId < pEntry2->u32ComponentId) ? -1:
		(pEntry1->u32ChassisId == pEntry2->u32ChassisId && pEntry1->u32ComponentId == pEntry2->u32ComponentId) ? 0: 1;
}

xBTree_t oIeee8021BridgeBaseTable_BTree = xBTree_initInline (&ieee8021BridgeBaseTable_BTreeNodeCmp);
xBTree_t oIeee8021BridgeBaseTable_Chassis_BTree = xBTree_initInline (&ieee8021BridgeBaseTable_Chassis_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeBaseEntry_t *
ieee8021BridgeBaseTable_createEntry (
	uint32_t u32ComponentId)
{
	register ieee8021BridgeBaseEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->u8TrafficClassesEnabled = ieee8021BridgeBaseTrafficClassesEnabled_true_c;
	poEntry->u8MmrpEnabledStatus = ieee8021BridgeBaseMmrpEnabledStatus_true_c;
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	xFreeRange_init (&poEntry->oPort_FreeRange);
	xFreeRange_createRange (&poEntry->oPort_FreeRange, ieee8021BridgeBasePort_start_c, ieee8021BridgeBasePort_end_c);
	xRwLock_init (&poEntry->oLock, NULL);
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree);
	return poEntry;
}

ieee8021BridgeBaseEntry_t *
ieee8021BridgeBaseTable_getByIndex (
	uint32_t u32ComponentId)
{
	register ieee8021BridgeBaseEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBaseEntry_t, oBTreeNode);
}

ieee8021BridgeBaseEntry_t *
ieee8021BridgeBaseTable_getNextIndex (
	uint32_t u32ComponentId)
{
	register ieee8021BridgeBaseEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBaseEntry_t, oBTreeNode);
}

ieee8021BridgeBaseEntry_t *
ieee8021BridgeBaseTable_Chassis_getNextIndex (
	uint32_t u32ChassisId,
	uint32_t u32ComponentId)
{
	register ieee8021BridgeBaseEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ChassisId = u32ChassisId;
	poTmpEntry->u32ComponentId = u32ComponentId;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oChassis_BTreeNode, &oIeee8021BridgeBaseTable_Chassis_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBaseEntry_t, oChassis_BTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeBaseTable_removeEntry (ieee8021BridgeBaseEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree);
	xBTree_nodeRemove (&poEntry->oChassis_BTreeNode, &oIeee8021BridgeBaseTable_Chassis_BTree);
	xRwLock_destroy (&poEntry->oLock);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

ieee8021BridgeBaseEntry_t *
ieee8021BridgeBaseTable_createExt (
	uint32_t u32ComponentId)
{
	ieee8021BridgeBaseEntry_t *poEntry = NULL;
	
	if (u32ComponentId == ieee8021BridgeBaseComponent_zero_c &&
		!xFreeRange_getFreeIndex (&oBridge.oComponent_FreeRange, false, 0, 0, &u32ComponentId))
	{
		goto ieee8021BridgeBaseTable_createExt_cleanup;
	}
	
	poEntry = ieee8021BridgeBaseTable_createEntry (
		u32ComponentId);
	if (poEntry == NULL)
	{
		goto ieee8021BridgeBaseTable_createExt_cleanup;
	}
	
	if (!ieee8021BridgeBaseTable_createHier (poEntry))
	{
		ieee8021BridgeBaseTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ieee8021BridgeBaseTable_createExt_cleanup;
	}
	
ieee8021BridgeBaseTable_createExt_cleanup:
	
	return poEntry;
}

bool
ieee8021BridgeBaseTable_removeExt (ieee8021BridgeBaseEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ieee8021BridgeBaseTable_removeHier (poEntry))
	{
		goto ieee8021BridgeBaseTable_removeExt_cleanup;
	}
	ieee8021BridgeBaseTable_removeEntry (poEntry);
	
	bRetCode = true;
	
ieee8021BridgeBaseTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseTable_createHier (
	ieee8021BridgeBaseEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!xFreeRange_allocateIndex (&oBridge.oComponent_FreeRange, poEntry->u32ComponentId))
	{
		goto ieee8021BridgeBaseTable_createHier_cleanup;
	}
	
	if ((neIeee8021BridgeBaseTable_createEntry (poEntry->u32ComponentId)) == NULL)
	{
		goto ieee8021BridgeBaseTable_createHier_cleanup;
	}
	
	if (!ieee8021BridgeBaseTable_hierUpdate (poEntry, xRowStatus_active_c))
	{
		goto ieee8021BridgeBaseTable_createHier_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgeBaseTable_createHier_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseTable_removeHier (
	ieee8021BridgeBaseEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ieee8021BridgeBaseTable_hierUpdate (poEntry, xRowStatus_destroy_c))
	{
		goto ieee8021BridgeBaseTable_removeHier_cleanup;
	}
	
	neIeee8021BridgeBaseTable_removeEntry (&poEntry->oNe);
	
	if (!xFreeRange_removeIndex (&oBridge.oComponent_FreeRange, poEntry->u32ComponentId))
	{
		goto ieee8021BridgeBaseTable_removeHier_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgeBaseTable_removeHier_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseEntry_init (
	ieee8021BridgeBaseEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (poEntry->oNe.u32ChassisId == 0 ||
		(poEntry->u32ChassisId != 0 && poEntry->u32ChassisId != poEntry->oNe.u32ChassisId))
	{
		goto ieee8021BridgeBaseEntry_init_cleanup;
	}
	
	register uint16_t u16Ports_len = xBitmap_maskCount (poEntry->oNe.u32NumPortsMax);
	
	if (poEntry->oNe.u32NumPortsMax == 0 ||
		(poEntry->oNe.u16Ports_len != 0 && poEntry->oNe.u16Ports_len < u16Ports_len))
	{
		goto ieee8021BridgeBaseEntry_init_cleanup;
	}
	
	if (poEntry->oNe.u16Ports_len == 0)
	{
		if ((poEntry->oNe.pu8Ports = xBuffer_cAlloc (u16Ports_len)) == NULL)
		{
			goto ieee8021BridgeBaseEntry_init_cleanup;
		}
		poEntry->oNe.u16Ports_len = u16Ports_len;
	}
	
	if (poEntry->u32ChassisId == 0)
	{
		poEntry->u32ChassisId = poEntry->oNe.u32ChassisId;
		xBTree_nodeAdd (&poEntry->oChassis_BTreeNode, &oIeee8021BridgeBaseTable_Chassis_BTree);
	}
	
	bRetCode = true;
	
ieee8021BridgeBaseEntry_init_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseTrafficClassesEnabled_handler (
	ieee8021BridgeBaseEntry_t *poEntry, uint8_t u8TrafficClassesEnabled, bool bForce)
{
	register bool bRetCode = false;
	
	if (poEntry->u8TrafficClassesEnabled == u8TrafficClassesEnabled && !bForce)
	{
		goto ieee8021BridgeBaseTrafficClassesEnabled_handler_success;
	}
	
	switch (u8TrafficClassesEnabled)
	{
	default:
		goto ieee8021BridgeBaseTrafficClassesEnabled_handler_cleanup;
		
	case ieee8021BridgeBaseTrafficClassesEnabled_false_c:
	case ieee8021BridgeBaseTrafficClassesEnabled_true_c:
		if (!ieee8021BridgeBaseTrafficClassesEnabled_update (poEntry, u8TrafficClassesEnabled))
		{
			goto ieee8021BridgeBaseTrafficClassesEnabled_handler_cleanup;
		}
		break;
	}
	
	!bForce ? (poEntry->u8TrafficClassesEnabled = u8TrafficClassesEnabled): false;
	
ieee8021BridgeBaseTrafficClassesEnabled_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeBaseTrafficClassesEnabled_handler_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseMmrpEnabledStatus_handler (
	ieee8021BridgeBaseEntry_t *poEntry, uint8_t u8MmrpEnabledStatus, bool bForce)
{
	register bool bRetCode = false;
	
	if (poEntry->u8MmrpEnabledStatus == u8MmrpEnabledStatus && !bForce)
	{
		goto ieee8021BridgeBaseMmrpEnabledStatus_handler_success;
	}
	
	switch (u8MmrpEnabledStatus)
	{
	default:
		goto ieee8021BridgeBaseMmrpEnabledStatus_handler_cleanup;
		
	case ieee8021BridgeBaseMmrpEnabledStatus_false_c:
	case ieee8021BridgeBaseMmrpEnabledStatus_true_c:
		if (!ieee8021BridgeBaseMmrpEnabledStatus_update (poEntry, u8MmrpEnabledStatus))
		{
			goto ieee8021BridgeBaseMmrpEnabledStatus_handler_cleanup;
		}
		break;
	}
	
	!bForce ? (poEntry->u8MmrpEnabledStatus = u8MmrpEnabledStatus): false;
	
ieee8021BridgeBaseMmrpEnabledStatus_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeBaseMmrpEnabledStatus_handler_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseRowStatus_handler (
	ieee8021BridgeBaseEntry_t *poEntry, uint8_t u8RowStatus)
{
	register bool bRetCode = false;
	
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto ieee8021BridgeBaseRowStatus_handler_success;
	}
	
	switch (u8RowStatus)
	{
	case xRowStatus_active_c:
		if (!ieee8021BridgeBaseEntry_init (poEntry) ||
			!ieee8021BridgeBaseRowStatus_update (poEntry, u8RowStatus))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = u8RowStatus;
		break;
		
	case xRowStatus_notInService_c:
		if (!ieee8021BridgeBaseRowStatus_update (poEntry, u8RowStatus))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = u8RowStatus;
		break;
		
	case xRowStatus_createAndGo_c:
		goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!ieee8021BridgeBaseRowStatus_update (poEntry, u8RowStatus))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
ieee8021BridgeBaseRowStatus_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeBaseRowStatus_handler_cleanup:
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeBaseTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeBaseTable_BTree);
	return ieee8021BridgeBaseTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeBaseTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBaseEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeBaseEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgeBaseTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBaseEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ieee8021BridgeBaseTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeBaseTable table mapper */
int
ieee8021BridgeBaseTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeBaseEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEBRIDGEADDRESS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8BridgeAddress, table_entry->u16BridgeAddress_len);
				break;
			case IEEE8021BRIDGEBASENUMPORTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32NumPorts);
				break;
			case IEEE8021BRIDGEBASECOMPONENTTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ComponentType);
				break;
			case IEEE8021BRIDGEBASEDEVICECAPABILITIES:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8DeviceCapabilities, table_entry->u16DeviceCapabilities_len);
				break;
			case IEEE8021BRIDGEBASETRAFFICCLASSESENABLED:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8TrafficClassesEnabled);
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8MmrpEnabledStatus);
				break;
			case IEEE8021BRIDGEBASEROWSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RowStatus);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEBRIDGEADDRESS:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8BridgeAddress));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASECOMPONENTTYPE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASEDEVICECAPABILITIES:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8DeviceCapabilities));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASETRAFFICCLASSESENABLED:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASEROWSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_WRONGVALUE);
					return SNMP_ERR_NOERROR;
					
				case RS_CREATEANDWAIT:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = ieee8021BridgeBaseTable_createExt (
						*idx1->val.integer);
					if (table_entry != NULL)
					{
						netsnmp_insert_iterator_context (request, table_entry);
						netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, table_entry, &xBuffer_free));
					}
					else
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
						return SNMP_ERR_NOERROR;
					}
					break;
					
				case RS_DESTROY:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					break;
				}
			default:
				if (table_entry == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				}
				break;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEBRIDGEADDRESS:
			case IEEE8021BRIDGEBASECOMPONENTTYPE:
			case IEEE8021BRIDGEBASEDEVICECAPABILITIES:
				if (table_entry->u8RowStatus == xRowStatus_active_c || table_entry->u8RowStatus == xRowStatus_notReady_c)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_FREE:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ieee8021BridgeBaseTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
			}
		}
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEBRIDGEADDRESS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8BridgeAddress))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16BridgeAddress_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8BridgeAddress, sizeof (table_entry->au8BridgeAddress));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8BridgeAddress, 0, sizeof (table_entry->au8BridgeAddress));
				memcpy (table_entry->au8BridgeAddress, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16BridgeAddress_len = request->requestvb->val_len;
				break;
			case IEEE8021BRIDGEBASECOMPONENTTYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ComponentType))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ComponentType, sizeof (table_entry->i32ComponentType));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ComponentType = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEBASEDEVICECAPABILITIES:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8DeviceCapabilities))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16DeviceCapabilities_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8DeviceCapabilities, sizeof (table_entry->au8DeviceCapabilities));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8DeviceCapabilities, 0, sizeof (table_entry->au8DeviceCapabilities));
				memcpy (table_entry->au8DeviceCapabilities, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16DeviceCapabilities_len = request->requestvb->val_len;
				break;
			case IEEE8021BRIDGEBASETRAFFICCLASSESENABLED:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8TrafficClassesEnabled))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8TrafficClassesEnabled, sizeof (table_entry->u8TrafficClassesEnabled));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				if (!ieee8021BridgeBaseTrafficClassesEnabled_handler (table_entry, *request->requestvb->val.integer, false))
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8MmrpEnabledStatus))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8MmrpEnabledStatus, sizeof (table_entry->u8MmrpEnabledStatus));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				if (!ieee8021BridgeBaseMmrpEnabledStatus_handler (table_entry, *request->requestvb->val.integer, false))
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!ieee8021BridgeBaseRowStatus_handler (table_entry, *request->requestvb->val.integer))
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					break;
				}
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEBRIDGEADDRESS:
				memcpy (table_entry->au8BridgeAddress, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16BridgeAddress_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case IEEE8021BRIDGEBASECOMPONENTTYPE:
				memcpy (&table_entry->i32ComponentType, pvOldDdata, sizeof (table_entry->i32ComponentType));
				break;
			case IEEE8021BRIDGEBASEDEVICECAPABILITIES:
				memcpy (table_entry->au8DeviceCapabilities, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16DeviceCapabilities_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case IEEE8021BRIDGEBASETRAFFICCLASSESENABLED:
				memcpy (&table_entry->u8TrafficClassesEnabled, pvOldDdata, sizeof (table_entry->u8TrafficClassesEnabled));
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				memcpy (&table_entry->u8MmrpEnabledStatus, pvOldDdata, sizeof (table_entry->u8MmrpEnabledStatus));
				break;
			case IEEE8021BRIDGEBASEROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ieee8021BridgeBaseTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBaseEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					ieee8021BridgeBaseTable_removeExt (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeBasePortTable table mapper **/
void
ieee8021BridgeBasePortTable_init (void)
{
	extern oid ieee8021BridgeBasePortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeBasePortTable", &ieee8021BridgeBasePortTable_mapper,
		ieee8021BridgeBasePortTable_oid, OID_LENGTH (ieee8021BridgeBasePortTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		0);
	table_info->min_column = IEEE8021BRIDGEBASEPORTIFINDEX;
	table_info->max_column = IEEE8021BRIDGEBASEPORTNAME;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeBasePortTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeBasePortTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeBasePortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeBasePortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeBasePortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeBasePortEntry_t, oBTreeNode);
	register ieee8021BridgeBasePortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeBasePortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32Port < pEntry2->u32Port) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32Port == pEntry2->u32Port) ? 0: 1;
}

xBTree_t oIeee8021BridgeBasePortTable_BTree = xBTree_initInline (&ieee8021BridgeBasePortTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeBasePortEntry_t *
ieee8021BridgeBasePortTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32Port = u32Port;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->i32Type = ieee8021BridgeBasePortType_none_c;
	poEntry->u8External = ieee8021BridgeBasePortExternal_false_c;
	poEntry->i32AdminPointToPoint = ieee8021BridgeBasePortAdminPointToPoint_forceFalse_c;
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree);
	return poEntry;
}

ieee8021BridgeBasePortEntry_t *
ieee8021BridgeBasePortTable_getByIndex (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021BridgeBasePortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBasePortEntry_t, oBTreeNode);
}

ieee8021BridgeBasePortEntry_t *
ieee8021BridgeBasePortTable_getNextIndex (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021BridgeBasePortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBasePortEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeBasePortTable_removeEntry (ieee8021BridgeBasePortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

bool
ieee8021BridgeBasePortTable_allocateIndex (
	ieee8021BridgeBaseEntry_t *poComponent,
	uint32_t *pu32Port)
{
	register bool bRetCode = false;
	uint32_t u32Port = *pu32Port;
	
	if (u32Port == ieee8021BridgeBasePort_zero_c &&
		!xFreeRange_getFreeIndex (&poComponent->oPort_FreeRange, false, 0, 0, &u32Port))
	{
		goto ieee8021BridgeBasePortTable_allocateIndex_cleanup;
	}
	
	if (!xFreeRange_allocateIndex (&poComponent->oPort_FreeRange, u32Port))
	{
		goto ieee8021BridgeBasePortTable_allocateIndex_cleanup;
	}
	xBitmap_setBitRev (poComponent->oNe.pu8Ports, u32Port - 1, 1);
	
	*pu32Port = u32Port;
	bRetCode = true;
	
ieee8021BridgeBasePortTable_allocateIndex_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBasePortTable_removeIndex (
	ieee8021BridgeBaseEntry_t *poComponent,
	uint32_t u32Port)
{
	register bool bRetCode = false;
	
	xBitmap_setBitRev (poComponent->oNe.pu8Ports, u32Port - 1, 0);
	if (!xFreeRange_removeIndex (&poComponent->oPort_FreeRange, u32Port))
	{
		goto ieee8021BridgeBasePortTable_removeIndex_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgeBasePortTable_removeIndex_cleanup:
	
	return bRetCode;
}

ieee8021BridgeBasePortEntry_t *
ieee8021BridgeBasePortTable_createExt (
	ieee8021BridgeBaseEntry_t *poComponent,
	uint32_t u32Port)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	
	if (u32Port == ieee8021BridgeBasePort_zero_c &&
		!xFreeRange_getFreeIndex (&poComponent->oPort_FreeRange, false, 0, 0, &u32Port))
	{
		goto ieee8021BridgeBasePortTable_createExt_cleanup;
	}
	
	poEntry = ieee8021BridgeBasePortTable_createEntry (
		poComponent->u32ComponentId,
		u32Port);
	if (poEntry == NULL)
	{
		goto ieee8021BridgeBasePortTable_createExt_cleanup;
	}
	
	if (!ieee8021BridgeBasePortTable_createHier (poComponent, poEntry))
	{
		ieee8021BridgeBasePortTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ieee8021BridgeBasePortTable_createExt_cleanup;
	}
	
ieee8021BridgeBasePortTable_createExt_cleanup:
	
	return poEntry;
}

bool
ieee8021BridgeBasePortTable_removeExt (ieee8021BridgeBaseEntry_t *poComponent, ieee8021BridgeBasePortEntry_t *poEntry)
{
	if (!ieee8021BridgeBasePortTable_removeHier (poComponent, poEntry))
	{
		return false;
	}
	ieee8021BridgeBasePortTable_removeEntry (poEntry);
	
	return true;
}

bool
ieee8021BridgeBasePortTable_createHier (ieee8021BridgeBaseEntry_t *poComponent, ieee8021BridgeBasePortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!xFreeRange_allocateIndex (&poComponent->oPort_FreeRange, poEntry->u32Port))
	{
		goto ieee8021BridgeBasePortTable_createHier_cleanup;
	}
	xBitmap_setBitRev (poComponent->oNe.pu8Ports, poEntry->u32Port - 1, 1);
	
	
	if ((neIeee8021BridgeBasePortTable_createEntry (poEntry->u32ComponentId, poEntry->u32Port)) == NULL)
	{
		goto ieee8021BridgeBasePortTable_createHier_cleanup;
	}
	
// 	if (ieee8021BridgeTpPortTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port) == NULL &&
// 		ieee8021BridgeTpPortTable_createExt (poEntry->u32ComponentId, poEntry->u32Port) == NULL)
// 	{
// 		goto ieee8021BridgeBasePortTable_createHier_cleanup;
// 	}
	
	if (ieee8021BridgePortPriorityTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port) == NULL &&
		ieee8021BridgePortPriorityTable_createExt (poEntry->u32ComponentId, poEntry->u32Port) == NULL)
	{
		goto ieee8021BridgeBasePortTable_createHier_cleanup;
	}
	
	if (ieee8021BridgePortMrpTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port) == NULL &&
		ieee8021BridgePortMrpTable_createEntry (poEntry->u32ComponentId, poEntry->u32Port) == NULL)
	{
		goto ieee8021BridgeBasePortTable_createHier_cleanup;
	}
	
	if (ieee8021BridgePortMmrpTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port) == NULL &&
		ieee8021BridgePortMmrpTable_createEntry (poEntry->u32ComponentId, poEntry->u32Port) == NULL)
	{
		goto ieee8021BridgeBasePortTable_createHier_cleanup;
	}
	
	if (!ieee8021BridgeBasePortTable_hierUpdate (poEntry, xRowStatus_active_c))
	{
		goto ieee8021BridgeBasePortTable_createHier_cleanup;
	}
	
	poComponent->i32NumPorts++;
	bRetCode = true;
	
ieee8021BridgeBasePortTable_createHier_cleanup:
	
	!bRetCode ? ieee8021BridgeBasePortTable_removeHier (poComponent, poEntry): false;
	return bRetCode;
}

bool
ieee8021BridgeBasePortTable_removeHier (ieee8021BridgeBaseEntry_t *poComponent, ieee8021BridgeBasePortEntry_t *poEntry)
{
	register bool bRetCode = false;
	register ieee8021BridgeTpPortEntry_t *poIeee8021BridgeTpPortEntry = NULL;
	register ieee8021BridgePortPriorityEntry_t *poIeee8021BridgePortPriorityEntry = NULL;
	register ieee8021BridgePortMrpEntry_t *poIeee8021BridgePortMrpEntry = NULL;
	register ieee8021BridgePortMmrpEntry_t *poIeee8021BridgePortMmrpEntry = NULL;
	
	
	if (!ieee8021BridgeBasePortTable_hierUpdate (poEntry, xRowStatus_destroy_c))
	{
		goto ieee8021BridgeBasePortTable_removeHier_cleanup;
	}
	
	neIeee8021BridgeBasePortTable_removeEntry (&poEntry->oNe);
	
	if ((poIeee8021BridgePortMmrpEntry = ieee8021BridgePortMmrpTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port)) != NULL)
	{
		ieee8021BridgePortMmrpTable_removeEntry (poIeee8021BridgePortMmrpEntry);
	}
	
	if ((poIeee8021BridgePortMrpEntry = ieee8021BridgePortMrpTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port)) != NULL)
	{
		ieee8021BridgePortMrpTable_removeEntry (poIeee8021BridgePortMrpEntry);
	}
	
	if ((poIeee8021BridgePortPriorityEntry = ieee8021BridgePortPriorityTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port)) != NULL)
	{
		ieee8021BridgePortPriorityTable_removeExt (poIeee8021BridgePortPriorityEntry);
	}
	
	if ((poIeee8021BridgeTpPortEntry = ieee8021BridgeTpPortTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port)) != NULL)
	{
		ieee8021BridgeTpPortTable_removeEntry (poIeee8021BridgeTpPortEntry);
	}
	
	
	xBitmap_setBitRev (poComponent->oNe.pu8Ports, poEntry->u32Port - 1, 0);
	if (!xFreeRange_removeIndex (&poComponent->oPort_FreeRange, poEntry->u32Port))
	{
		goto ieee8021BridgeBasePortTable_removeHier_cleanup;
	}
	
	poComponent->i32NumPorts--;
	bRetCode = true;
	
ieee8021BridgeBasePortTable_removeHier_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBasePortIfIndex_handler (
	ieee8021BridgeBaseEntry_t *poComponent,
	ieee8021BridgeBasePortEntry_t *poEntry)
{
	register bool bRetCode = false;
	register bool bLocked = false;
	register ieee8021BridgePhyPortEntry_t *poPhy = NULL;
	
	if (poEntry->pOldEntry != NULL && poEntry->u32IfIndex == poEntry->pOldEntry->u32IfIndex)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_success;
	}
	
	if (poEntry->u32IfIndex != 0 || (poEntry->pOldEntry != NULL && poEntry->pOldEntry->u32IfIndex != 0))
	{
		ieee8021BridgePhyPortTable_wrLock ();
		bLocked = true;
	}
	
	
	if (poEntry->pOldEntry == NULL || poEntry->pOldEntry->u32IfIndex == 0)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_newIfIndex;
	}
	
	if ((poPhy = ieee8021BridgePhyPortTable_getByIndex (poEntry->pOldEntry->u32IfIndex, 0)) == NULL)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	if (!ieee8021BridgeTpPortTable_handler (poComponent, poPhy, false, false))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	if (!ieee8021BridgePhyPortTable_detachComponent (poEntry, poPhy))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	if (!ifTable_removeReference (poEntry->pOldEntry->u32IfIndex, false, true, false))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	poEntry->pOldEntry->u32IfIndex = 0;
	
	
ieee8021BridgeBasePortIfIndex_handler_newIfIndex:
	
	if (poEntry->u32IfIndex == 0)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_success;
	}
	
	if ((poPhy = ieee8021BridgePhyPortTable_getByIndex (poEntry->u32IfIndex, 0)) == NULL)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	if (!xBitmap_getBitRev (poPhy->au8TypeCapabilities, poEntry->i32Type - 2) ||
		!ieee8021BridgePhyPortTable_attachComponent (poComponent, poEntry, poPhy))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	register bool bMacLearn = xBitmap_getBitRev (poPhy->au8AdminFlags, neIfAdminFlags_macLearn_c);
	register bool bMacFwd = xBitmap_getBitRev (poPhy->au8AdminFlags, neIfAdminFlags_macFwd_c);
	
	if (!ieee8021BridgeTpPortTable_handler (poComponent, poPhy, bMacLearn, bMacFwd))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	if (!ifTable_createReference (poEntry->u32IfIndex, 0, 0, false, true, false, NULL))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
ieee8021BridgeBasePortIfIndex_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeBasePortIfIndex_handler_cleanup:
	
	bLocked ? ieee8021BridgePhyPortTable_unLock (): false;
	return bRetCode;
}

bool
ieee8021BridgeBasePortRowStatus_handler (
	ieee8021BridgeBaseEntry_t *poComponent,
	ieee8021BridgeBasePortEntry_t *poEntry, uint8_t u8RowStatus)
{
	register bool bRetCode = false;
	register uint8_t u8RealStatus = u8RowStatus & xRowStatus_mask_c;
	
	if (poEntry->u8RowStatus == u8RealStatus)
	{
		goto ieee8021BridgeBasePortRowStatus_handler_success;
	}
	
	switch (u8RealStatus)
	{
	case xRowStatus_active_c:
	case xRowStatus_notReady_c:
		if (poEntry->i32Type < ieee8021BridgeBasePortType_min_c ||
			poEntry->i32Type > ieee8021BridgeBasePortType_max_c ||
			poEntry->u32IfIndex == 0)
		{
			goto ieee8021BridgeBasePortRowStatus_handler_cleanup;
		}
		
		if (!ieee8021BridgeBasePortRowStatus_update (poComponent, poEntry, u8RealStatus))
		{
			goto ieee8021BridgeBasePortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = u8RealStatus;
		break;
		
	case xRowStatus_notInService_c:
		if (!ieee8021BridgeBasePortRowStatus_update (poComponent, poEntry, u8RealStatus))
		{
			goto ieee8021BridgeBasePortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = u8RealStatus;
		break;
		
	case xRowStatus_createAndGo_c:
		goto ieee8021BridgeBasePortRowStatus_handler_cleanup;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!ieee8021BridgeBasePortRowStatus_update (poComponent, poEntry, u8RealStatus))
		{
			goto ieee8021BridgeBasePortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
ieee8021BridgeBasePortRowStatus_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeBasePortRowStatus_handler_cleanup:
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeBasePortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeBasePortTable_BTree);
	return ieee8021BridgeBasePortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeBasePortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeBasePortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgeBasePortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgeBasePortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeBasePortTable table mapper */
int
ieee8021BridgeBasePortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeBasePortEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEPORTIFINDEX:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u32IfIndex);
				break;
			case IEEE8021BRIDGEBASEPORTDELAYEXCEEDEDDISCARDS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64DelayExceededDiscards);
				break;
			case IEEE8021BRIDGEBASEPORTMTUEXCEEDEDDISCARDS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64MtuExceededDiscards);
				break;
			case IEEE8021BRIDGEBASEPORTCAPABILITIES:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Capabilities, table_entry->u16Capabilities_len);
				break;
			case IEEE8021BRIDGEBASEPORTTYPECAPABILITIES:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8TypeCapabilities, table_entry->u16TypeCapabilities_len);
				break;
			case IEEE8021BRIDGEBASEPORTTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Type);
				break;
			case IEEE8021BRIDGEBASEPORTEXTERNAL:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8External);
				break;
			case IEEE8021BRIDGEBASEPORTADMINPOINTTOPOINT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32AdminPointToPoint);
				break;
			case IEEE8021BRIDGEBASEPORTOPERPOINTTOPOINT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8OperPointToPoint);
				break;
			case IEEE8021BRIDGEBASEPORTNAME:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Name, table_entry->u16Name_len);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEPORTIFINDEX:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASEPORTADMINPOINTTOPOINT:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEPORTIFINDEX:
			case IEEE8021BRIDGEBASEPORTADMINPOINTTOPOINT:
				if (table_entry->u8RowStatus == xRowStatus_active_c || table_entry->u8RowStatus == xRowStatus_notReady_c)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEPORTIFINDEX:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32IfIndex))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32IfIndex, sizeof (table_entry->u32IfIndex));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32IfIndex = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEBASEPORTADMINPOINTTOPOINT:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32AdminPointToPoint))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32AdminPointToPoint, sizeof (table_entry->i32AdminPointToPoint));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32AdminPointToPoint = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEPORTIFINDEX:
				memcpy (&table_entry->u32IfIndex, pvOldDdata, sizeof (table_entry->u32IfIndex));
				break;
			case IEEE8021BRIDGEBASEPORTADMINPOINTTOPOINT:
				memcpy (&table_entry->i32AdminPointToPoint, pvOldDdata, sizeof (table_entry->i32AdminPointToPoint));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeChassisTable table mapper **/
bool
ieee8021BridgeChassis_createRegister (
	uint32_t u32ChassisId)
{
	register bool bRetCode = false;
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	
	ieee8021Bridge_wrLock ();
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_Chassis_getNextIndex (u32ChassisId, 0)) == NULL || poIeee8021BridgeBaseEntry->u32ChassisId != u32ChassisId)
	{
		if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_createExt (ieee8021BridgeBaseComponent_zero_c)) == NULL)
		{
			goto ieee8021BridgeChassis_createRegister_componentCleanup;
		}
		
		poIeee8021BridgeBaseEntry->oNe.u32ChassisId = u32ChassisId;
		poIeee8021BridgeBaseEntry->oNe.u32NumPortsMax = ETHERNET_PORT_MAP_SIZE;
		
		if (!ieee8021BridgeBaseEntry_init (poIeee8021BridgeBaseEntry))
		{
			goto ieee8021BridgeChassis_createRegister_componentCleanup;
		}
	}
	
	ieee8021BridgeBase_wrLock (poIeee8021BridgeBaseEntry);
	
ieee8021BridgeChassis_createRegister_componentCleanup:
	
	ieee8021Bridge_unLock ();
	if (poIeee8021BridgeBaseEntry == NULL)
	{
		goto ieee8021BridgeChassis_createRegister_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgeChassis_createRegister_cleanup:
	
	poIeee8021BridgeBaseEntry != NULL ? ieee8021BridgeBase_unLock (poIeee8021BridgeBaseEntry): false;
	return bRetCode;
}

bool
ieee8021BridgeChassis_removeRegister (
	uint32_t u32ChassisId)
{
	register bool bRetCode = false;
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	
	ieee8021Bridge_wrLock ();
	
	while ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_Chassis_getNextIndex (u32ChassisId, 0)) != NULL && poIeee8021BridgeBaseEntry->u32ChassisId != u32ChassisId)
	{
		ieee8021BridgeBase_wrLock (poIeee8021BridgeBaseEntry);
		
		if (!ieee8021BridgeBaseRowStatus_handler (poIeee8021BridgeBaseEntry, xRowStatus_destroy_c))
		{
			goto ieee8021BridgeChassis_removeRegister_cleanup;
		}
		
		xBTree_nodeRemove (&poIeee8021BridgeBaseEntry->oBTreeNode, &oIeee8021BridgeBaseTable_BTree);
		xBTree_nodeRemove (&poIeee8021BridgeBaseEntry->oChassis_BTreeNode, &oIeee8021BridgeBaseTable_Chassis_BTree);
		ieee8021BridgeBase_unLock (poIeee8021BridgeBaseEntry);
		
		if (!ieee8021BridgeBaseTable_removeExt (poIeee8021BridgeBaseEntry))
		{
			poIeee8021BridgeBaseEntry = NULL;
			goto ieee8021BridgeChassis_removeRegister_cleanup;
		}
	}
	
	bRetCode = true;
	
ieee8021BridgeChassis_removeRegister_cleanup:
	
	!bRetCode && poIeee8021BridgeBaseEntry != NULL ? ieee8021BridgeBase_unLock (poIeee8021BridgeBaseEntry): false;
	ieee8021Bridge_unLock ();
	
	return bRetCode;
}

/** initialize ieee8021BridgeBaseIfToPortTable table mapper **/
void
ieee8021BridgeBaseIfToPortTable_init (void)
{
	extern oid ieee8021BridgeBaseIfToPortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeBaseIfToPortTable", &ieee8021BridgeBaseIfToPortTable_mapper,
		ieee8021BridgeBaseIfToPortTable_oid, OID_LENGTH (ieee8021BridgeBaseIfToPortTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifIndex */,
		0);
	table_info->min_column = IEEE8021BRIDGEBASEIFINDEXCOMPONENTID;
	table_info->max_column = IEEE8021BRIDGEBASEIFINDEXPORT;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeBaseIfToPortTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeBaseIfToPortTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeBaseIfToPortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeBaseIfToPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePhyPortTable_If_BTree);
	return ieee8021BridgeBaseIfToPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeBaseIfToPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePhyPortEntry_t, oIf_BTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32IfIndex);
	*my_data_context = (void*) &poEntry->oIf;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oIf_BTreeNode, &oIeee8021BridgePhyPortTable_If_BTree);
	return put_index_data;
}

bool
ieee8021BridgeBaseIfToPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ieee8021BridgePhyPortTable_getByIndex (
		*idx1->val.integer, 0);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) &poEntry->oIf;
	return true;
}

/* ieee8021BridgeBaseIfToPortTable table mapper */
int
ieee8021BridgeBaseIfToPortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeBaseIfToPortEntry_t *table_entry;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeBaseIfToPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEIFINDEXCOMPONENTID:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32ComponentId);
				break;
			case IEEE8021BRIDGEBASEIFINDEXPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32Port);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePhyPortTable table mapper **/
void
ieee8021BridgePhyPortTable_init (void)
{
	extern oid ieee8021BridgePhyPortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePhyPortTable", &ieee8021BridgePhyPortTable_mapper,
		ieee8021BridgePhyPortTable_oid, OID_LENGTH (ieee8021BridgePhyPortTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgePhyPort */,
		0);
	table_info->min_column = IEEE8021BRIDGEPHYPORTIFINDEX;
	table_info->max_column = IEEE8021BRIDGEPHYPORTTOINTERNALPORT;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePhyPortTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePhyPortTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePhyPortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgePhyPortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePhyPortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePhyPortEntry_t, oBTreeNode);
	register ieee8021BridgePhyPortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePhyPortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32Port < pEntry2->u32Port) ? -1:
		(pEntry1->u32Port == pEntry2->u32Port) ? 0: 1;
}

static int8_t
ieee8021BridgePhyPortTable_If_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePhyPortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePhyPortEntry_t, oIf_BTreeNode);
	register ieee8021BridgePhyPortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePhyPortEntry_t, oIf_BTreeNode);
	
	return
		(pEntry1->u32IfIndex < pEntry2->u32IfIndex) ? -1:
		(pEntry1->u32IfIndex == pEntry2->u32IfIndex) ? 0: 1;
}

xBTree_t oIeee8021BridgePhyPortTable_BTree = xBTree_initInline (&ieee8021BridgePhyPortTable_BTreeNodeCmp);
xBTree_t oIeee8021BridgePhyPortTable_If_BTree = xBTree_initInline (&ieee8021BridgePhyPortTable_If_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_createEntry (
	uint32_t u32IfIndex,
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Port = u32Port;
	poEntry->u32IfIndex = u32IfIndex;
	if ((u32IfIndex != 0 && xBTree_nodeFind (&poEntry->oIf_BTreeNode, &oIeee8021BridgePhyPortTable_If_BTree) != NULL) ||
		(u32Port != 0 && xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree) != NULL))
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	if (u32IfIndex != 0) { xBTree_nodeAdd (&poEntry->oIf_BTreeNode, &oIeee8021BridgePhyPortTable_If_BTree); }
	if (u32Port != 0) { xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree); }
	
	return poEntry;
}

ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_getByIndex (
	uint32_t u32IfIndex,
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Port = u32Port;
	poTmpEntry->u32IfIndex = u32IfIndex;
	if ((u32IfIndex != 0 && (poNode = xBTree_nodeFind (&poTmpEntry->oIf_BTreeNode, &oIeee8021BridgePhyPortTable_If_BTree)) == NULL) ||
		(u32Port != 0 && (poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree)) == NULL))
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return
		u32IfIndex != 0 ? xBTree_entry (poNode, ieee8021BridgePhyPortEntry_t, oIf_BTreeNode):
		u32Port != 0 ? xBTree_entry (poNode, ieee8021BridgePhyPortEntry_t, oBTreeNode): NULL;
}

ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_getNextIndex (
	uint32_t u32IfIndex,
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Port = u32Port;
	poTmpEntry->u32IfIndex = u32IfIndex;
	if ((u32IfIndex != 0 && (poNode = xBTree_nodeFindNext (&poTmpEntry->oIf_BTreeNode, &oIeee8021BridgePhyPortTable_If_BTree)) == NULL) ||
		(u32Port != 0 && (poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree)) == NULL))
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return
		u32IfIndex != 0 ? xBTree_entry (poNode, ieee8021BridgePhyPortEntry_t, oIf_BTreeNode):
		u32Port != 0 ? xBTree_entry (poNode, ieee8021BridgePhyPortEntry_t, oBTreeNode): NULL;
}

/* remove a row from the table */
void
ieee8021BridgePhyPortTable_removeEntry (ieee8021BridgePhyPortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree);
	xBTree_nodeRemove (&poEntry->oIf_BTreeNode, &oIeee8021BridgePhyPortTable_If_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_createExt (
	uint32_t u32IfIndex,
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	
	if (u32IfIndex == 0)
	{
		goto ieee8021BridgePhyPortTable_createExt_cleanup;
	}
	
	poEntry = ieee8021BridgePhyPortTable_createEntry (
		u32IfIndex,
		u32Port);
	if (poEntry == NULL)
	{
		goto ieee8021BridgePhyPortTable_createExt_cleanup;
	}
	
	if (!ieee8021BridgePhyPortTable_createHier (poEntry))
	{
		ieee8021BridgePhyPortTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ieee8021BridgePhyPortTable_createExt_cleanup;
	}
	
ieee8021BridgePhyPortTable_createExt_cleanup:
	
	return poEntry;
}

bool
ieee8021BridgePhyPortTable_removeExt (ieee8021BridgePhyPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ieee8021BridgePhyPortTable_removeHier (poEntry))
	{
		goto ieee8021BridgePhyPortTable_removeExt_cleanup;
	}
	ieee8021BridgePhyPortTable_removeEntry (poEntry);
	
	bRetCode = true;
	
ieee8021BridgePhyPortTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgePhyPortTable_createHier (
	ieee8021BridgePhyPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ifTable_createReference (poEntry->u32IfIndex, 0, 0, false, true, false, NULL))
	{
		goto ieee8021BridgePhyPortTable_createHier_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgePhyPortTable_createHier_cleanup:
	
	!bRetCode ? ieee8021BridgePhyPortTable_removeHier (poEntry): false;
	return bRetCode;
}

bool
ieee8021BridgePhyPortTable_removeHier (
	ieee8021BridgePhyPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ifTable_removeReference (poEntry->u32IfIndex, false, true, false))
	{
		goto ieee8021BridgePhyPortTable_removeHier_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgePhyPortTable_removeHier_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgePhyPortTable_createRegister (
	uint32_t u32IfIndex,
	uint32_t u32Port,
	uint32_t u32ChassisId)
{
	register bool bRetCode = false;
	register ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	
	ieee8021BridgePhyPortTable_wrLock ();
	
	if ((poEntry = ieee8021BridgePhyPortTable_getByIndex (u32IfIndex, u32Port)) != NULL)
	{
		goto ieee8021BridgePhyPortTable_createRegister_success;
	}
	
	if ((poEntry = ieee8021BridgePhyPortTable_createExt (u32IfIndex, u32Port)) == NULL)
	{
		goto ieee8021BridgePhyPortTable_createRegister_cleanup;
	}
	
	poEntry->u32ChassisId = u32ChassisId;
	
ieee8021BridgePhyPortTable_createRegister_success:
	
	bRetCode = true;
	
ieee8021BridgePhyPortTable_createRegister_cleanup:
	
	ieee8021BridgePhyPortTable_unLock ();
	return bRetCode;
}

bool
ieee8021BridgePhyPortTable_removeRegister (
	uint32_t u32IfIndex,
	uint32_t u32Port)
{
	register bool bRetCode = false;
	register ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	register ieee8021BridgeBaseEntry_t *poComponent = NULL;
	
	
	ieee8021BridgePhyPortTable_wrLock ();
	
	if ((poEntry = ieee8021BridgePhyPortTable_getByIndex (u32IfIndex, u32Port)) == NULL)
	{
		goto ieee8021BridgePhyPortTable_removeRegister_success;
	}
	
	if (poEntry->oIf.u32ComponentId == 0)
	{
		goto ieee8021BridgePhyPortTable_removeRegister_remove;
	}
	
	
	ieee8021Bridge_wrLock ();
	
	if ((poComponent = ieee8021BridgeBaseTable_getByIndex (poEntry->oIf.u32ComponentId)) == NULL)
	{
		goto ieee8021BridgePhyPortTable_removeRegister_componentCleanup;
	}
	
	ieee8021BridgeBase_wrLock (poComponent);
	
ieee8021BridgePhyPortTable_removeRegister_componentCleanup:
	
	ieee8021Bridge_unLock ();
	if (poComponent == NULL)
	{
		goto ieee8021BridgePhyPortTable_removeRegister_cleanup;
	}
	
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (poEntry->oIf.u32ComponentId, poEntry->oIf.u32Port)) == NULL)
	{
		goto ieee8021BridgePhyPortTable_removeRegister_cleanup;
	}
	
	if (!ieee8021BridgePhyPortTable_detachComponent (poPort, poEntry))
	{
		goto ieee8021BridgePhyPortTable_removeRegister_cleanup;
	}
	
	
ieee8021BridgePhyPortTable_removeRegister_remove:
	
	if (!ieee8021BridgePhyPortTable_removeExt (poEntry))
	{
		goto ieee8021BridgePhyPortTable_removeRegister_cleanup;
	}
	
ieee8021BridgePhyPortTable_removeRegister_success:
	
	bRetCode = true;
	
ieee8021BridgePhyPortTable_removeRegister_cleanup:
	
	poComponent != NULL ? ieee8021BridgeBase_unLock (poComponent): false;
	ieee8021BridgePhyPortTable_unLock ();
	
	return bRetCode;
}

bool
ieee8021BridgePhyPortTable_attachComponent (
	ieee8021BridgeBaseEntry_t *poComponent, ieee8021BridgeBasePortEntry_t *poPort,
	ieee8021BridgePhyPortEntry_t *poPhy)
{
	register bool bRetCode = false;
	
	if (poComponent == NULL || poPort == NULL || poPhy == NULL)
	{
		goto ieee8021BridgePhyPortTable_attachComponent_cleanup;
	}
	
	if (poComponent->u32ChassisId != 0 && poPhy->u32ChassisId != 0 && poComponent->u32ChassisId != poPhy->u32ChassisId)
	{
		goto ieee8021BridgePhyPortTable_attachComponent_cleanup;
	}
	
	poPhy->oIf.u32ComponentId = poPort->u32ComponentId;
	poPhy->oIf.u32Port = poPort->u32Port;
	memcpy (poPort->au8Capabilities, poPhy->au8TypeCapabilities, sizeof (poPort->au8Capabilities));
	poPort->u32IfIndex = poPhy->u32IfIndex;
	poPort->u8External = poPhy->u32Port == 0 ? ieee8021BridgeBasePortExternal_false_c: ieee8021BridgeBasePortExternal_true_c;
	bRetCode = true;
	
ieee8021BridgePhyPortTable_attachComponent_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgePhyPortTable_detachComponent (
	ieee8021BridgeBasePortEntry_t *poPort,
	ieee8021BridgePhyPortEntry_t *poPhy)
{
	register bool bRetCode = false;
	
	if (poPort == NULL || poPhy == NULL)
	{
		goto ieee8021BridgePhyPortTable_detachComponent_cleanup;
	}
	
	poPhy->oIf.u32ComponentId = 0;
	poPhy->oIf.u32Port = 0;
	memset (poPort->au8Capabilities, 0, sizeof (poPort->au8Capabilities));
	poPort->u8External = ieee8021BridgeBasePortExternal_false_c;
	bRetCode = true;
	
ieee8021BridgePhyPortTable_detachComponent_cleanup:
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePhyPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePhyPortTable_BTree);
	return ieee8021BridgePhyPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePhyPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePhyPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePhyPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ieee8021BridgePhyPortTable_getByIndex (
		0, *idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgePhyPortTable table mapper */
int
ieee8021BridgePhyPortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePhyPortEntry_t *table_entry;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePhyPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPHYPORTIFINDEX:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u32IfIndex);
				break;
			case IEEE8021BRIDGEPHYMACADDRESS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8MacAddress, table_entry->u16MacAddress_len);
				break;
			case IEEE8021BRIDGEPHYPORTTOCOMPONENTID:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->oIf.u32ComponentId);
				break;
			case IEEE8021BRIDGEPHYPORTTOINTERNALPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->oIf.u32Port);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeTpPortTable table mapper **/
void
ieee8021BridgeTpPortTable_init (void)
{
	extern oid ieee8021BridgeTpPortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeTpPortTable", &ieee8021BridgeTpPortTable_mapper,
		ieee8021BridgeTpPortTable_oid, OID_LENGTH (ieee8021BridgeTpPortTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeTpPortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeTpPort */,
		0);
	table_info->min_column = IEEE8021BRIDGETPPORTMAXINFO;
	table_info->max_column = IEEE8021BRIDGETPPORTINDISCARDS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeTpPortTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeTpPortTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeTpPortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeTpPortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeTpPortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeTpPortEntry_t, oBTreeNode);
	register ieee8021BridgeTpPortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeTpPortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32Port < pEntry2->u32Port) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32Port == pEntry2->u32Port) ? 0: 1;
}

xBTree_t oIeee8021BridgeTpPortTable_BTree = xBTree_initInline (&ieee8021BridgeTpPortTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeTpPortEntry_t *
ieee8021BridgeTpPortTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021BridgeTpPortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32Port = u32Port;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree);
	return poEntry;
}

ieee8021BridgeTpPortEntry_t *
ieee8021BridgeTpPortTable_getByIndex (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021BridgeTpPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeTpPortEntry_t, oBTreeNode);
}

ieee8021BridgeTpPortEntry_t *
ieee8021BridgeTpPortTable_getNextIndex (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021BridgeTpPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeTpPortEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeTpPortTable_removeEntry (ieee8021BridgeTpPortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

bool
ieee8021BridgeTpPortTable_handler (
	ieee8021BridgeBaseEntry_t *poComponent,
	ieee8021BridgePhyPortEntry_t *poPhy, bool bMacLearn, bool bMacFwd)
{
	register bool bRetCode = false;
	register ieee8021BridgeTpPortEntry_t *poIeee8021BridgeTpPortEntry = NULL;
	
	poIeee8021BridgeTpPortEntry = ieee8021BridgeTpPortTable_getByIndex (poPhy->oIf.u32ComponentId, poPhy->oIf.u32Port);
	
	if (bMacFwd ^ (poIeee8021BridgeTpPortEntry != NULL))
	{
		goto ieee8021BridgeTpPortTable_handler_success;
	}
	
	
	if (!bMacFwd && !ieee8021BridgeTpPortStatus_update (poPhy, poIeee8021BridgeTpPortEntry, bMacLearn, bMacFwd))
	{
		goto ieee8021BridgeTpPortTable_handler_cleanup;
	}
	
	if (bMacFwd)
	{
		if ((poIeee8021BridgeTpPortEntry = ieee8021BridgeTpPortTable_createEntry (poPhy->oIf.u32ComponentId, poPhy->oIf.u32Port)) == NULL)
		{
			goto ieee8021BridgeTpPortTable_handler_cleanup;
		}
	}
	else
	{
		ieee8021BridgeTpPortTable_removeEntry (poIeee8021BridgeTpPortEntry);
	}
	
	if (bMacFwd && !ieee8021BridgeTpPortStatus_update (poPhy, poIeee8021BridgeTpPortEntry, bMacLearn, bMacFwd))
	{
		goto ieee8021BridgeTpPortTable_handler_cleanup;
	}
	
ieee8021BridgeTpPortTable_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeTpPortTable_handler_cleanup:
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeTpPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeTpPortTable_BTree);
	return ieee8021BridgeTpPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeTpPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeTpPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeTpPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeTpPortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgeTpPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeTpPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgeTpPortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeTpPortTable table mapper */
int
ieee8021BridgeTpPortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeTpPortEntry_t *table_entry;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeTpPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETPPORTMAXINFO:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32MaxInfo);
				break;
			case IEEE8021BRIDGETPPORTINFRAMES:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64InFrames);
				break;
			case IEEE8021BRIDGETPPORTOUTFRAMES:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64OutFrames);
				break;
			case IEEE8021BRIDGETPPORTINDISCARDS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64InDiscards);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePortPriorityTable table mapper **/
void
ieee8021BridgePortPriorityTable_init (void)
{
	extern oid ieee8021BridgePortPriorityTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePortPriorityTable", &ieee8021BridgePortPriorityTable_mapper,
		ieee8021BridgePortPriorityTable_oid, OID_LENGTH (ieee8021BridgePortPriorityTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		0);
	table_info->min_column = IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY;
	table_info->max_column = IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePortPriorityTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePortPriorityTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePortPriorityTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgePortPriorityTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePortPriorityEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePortPriorityEntry_t, oBTreeNode);
	register ieee8021BridgePortPriorityEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePortPriorityEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgePortPriorityTable_BTree = xBTree_initInline (&ieee8021BridgePortPriorityTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortPriorityEntry_t *poEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	poEntry = &poPort->oPriority;
	
	poEntry->i32NumTrafficClasses = 8;
	poEntry->i32CodePointSelection = ieee8021BridgePortPriorityCodePointSelection_codePoint8p0d_c;
	poEntry->u8UseDEI = ieee8021BridgePortUseDEI_false_c;
	poEntry->u8RequireDropEncoding = ieee8021BridgePortRequireDropEncoding_false_c;
	poEntry->u8ServiceAccessPrioritySelection = ieee8021BridgePortServiceAccessPrioritySelection_false_c;
	
	return poEntry;
}

ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	
	return &poPort->oPriority;
}

ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getNextIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	
	return &poPort->oPriority;
}

/* remove a row from the table */
void
ieee8021BridgePortPriorityTable_removeEntry (ieee8021BridgePortPriorityEntry_t *poEntry)
{
	return;
}

ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_createExt (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	ieee8021BridgePortPriorityEntry_t *poEntry = NULL;
	
	poEntry = ieee8021BridgePortPriorityTable_createEntry (
		u32BasePortComponentId,
		u32BasePort);
	if (poEntry == NULL)
	{
		return NULL;
	}
	
	if (!ieee8021BridgePortPriorityTable_createHier (poEntry))
	{
		ieee8021BridgePortPriorityTable_removeEntry (poEntry);
		return NULL;
	}
	
	return poEntry;
}

bool
ieee8021BridgePortPriorityTable_removeExt (ieee8021BridgePortPriorityEntry_t *poEntry)
{
	if (!ieee8021BridgePortPriorityTable_removeHier (poEntry))
	{
		return false;
	}
	ieee8021BridgePortPriorityTable_removeEntry (poEntry);
	
	return true;
}

bool
ieee8021BridgePortPriorityTable_createHier (
	ieee8021BridgePortPriorityEntry_t *poEntry)
{
	return true;
}

bool
ieee8021BridgePortPriorityTable_removeHier (
	ieee8021BridgePortPriorityEntry_t *poEntry)
{
	
	return true;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortPriorityTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeBasePortTable_BTree);
	return ieee8021BridgePortPriorityTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortPriorityTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeBasePortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) &poEntry->oPriority;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePortPriorityTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgeBasePortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) &poEntry->oPriority;
	return true;
}

/* ieee8021BridgePortPriorityTable table mapper */
int
ieee8021BridgePortPriorityTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePortPriorityEntry_t *table_entry;
	register ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oPriority;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32DefaultUserPriority);
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32NumTrafficClasses);
				break;
			case IEEE8021BRIDGEPORTPRIORITYCODEPOINTSELECTION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32CodePointSelection);
				break;
			case IEEE8021BRIDGEPORTUSEDEI:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8UseDEI);
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RequireDropEncoding);
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8ServiceAccessPrioritySelection);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oPriority;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTPRIORITYCODEPOINTSELECTION:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTUSEDEI:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oPriority;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32DefaultUserPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32DefaultUserPriority, sizeof (table_entry->u32DefaultUserPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32DefaultUserPriority = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32NumTrafficClasses))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32NumTrafficClasses, sizeof (table_entry->i32NumTrafficClasses));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32NumTrafficClasses = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTPRIORITYCODEPOINTSELECTION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32CodePointSelection))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32CodePointSelection, sizeof (table_entry->i32CodePointSelection));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32CodePointSelection = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTUSEDEI:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8UseDEI))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8UseDEI, sizeof (table_entry->u8UseDEI));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8UseDEI = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8RequireDropEncoding))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8RequireDropEncoding, sizeof (table_entry->u8RequireDropEncoding));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8RequireDropEncoding = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8ServiceAccessPrioritySelection))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8ServiceAccessPrioritySelection, sizeof (table_entry->u8ServiceAccessPrioritySelection));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8ServiceAccessPrioritySelection = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oPriority;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				memcpy (&table_entry->u32DefaultUserPriority, pvOldDdata, sizeof (table_entry->u32DefaultUserPriority));
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				memcpy (&table_entry->i32NumTrafficClasses, pvOldDdata, sizeof (table_entry->i32NumTrafficClasses));
				break;
			case IEEE8021BRIDGEPORTPRIORITYCODEPOINTSELECTION:
				memcpy (&table_entry->i32CodePointSelection, pvOldDdata, sizeof (table_entry->i32CodePointSelection));
				break;
			case IEEE8021BRIDGEPORTUSEDEI:
				memcpy (&table_entry->u8UseDEI, pvOldDdata, sizeof (table_entry->u8UseDEI));
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				memcpy (&table_entry->u8RequireDropEncoding, pvOldDdata, sizeof (table_entry->u8RequireDropEncoding));
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				memcpy (&table_entry->u8ServiceAccessPrioritySelection, pvOldDdata, sizeof (table_entry->u8ServiceAccessPrioritySelection));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeUserPriorityRegenTable table mapper **/
void
ieee8021BridgeUserPriorityRegenTable_init (void)
{
	extern oid ieee8021BridgeUserPriorityRegenTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeUserPriorityRegenTable", &ieee8021BridgeUserPriorityRegenTable_mapper,
		ieee8021BridgeUserPriorityRegenTable_oid, OID_LENGTH (ieee8021BridgeUserPriorityRegenTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		ASN_UNSIGNED /* index: ieee8021BridgeUserPriority */,
		0);
	table_info->min_column = IEEE8021BRIDGEREGENUSERPRIORITY;
	table_info->max_column = IEEE8021BRIDGEREGENUSERPRIORITY;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeUserPriorityRegenTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeUserPriorityRegenTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeUserPriorityRegenTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeUserPriorityRegenTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeUserPriorityRegenEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeUserPriorityRegenEntry_t, oBTreeNode);
	register ieee8021BridgeUserPriorityRegenEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeUserPriorityRegenEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgeUserPriorityRegenTable_BTree = xBTree_initInline (&ieee8021BridgeUserPriorityRegenTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeUserPriorityRegenEntry_t *
ieee8021BridgeUserPriorityRegenTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32UserPriority)
{
	register ieee8021BridgeUserPriorityRegenEntry_t *poEntry = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32UserPriority))
	{
		return NULL;
	}
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
// 	poEntry->u32UserPriority = u32UserPriority;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree);
	return poEntry;
}

ieee8021BridgeUserPriorityRegenEntry_t *
ieee8021BridgeUserPriorityRegenTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32UserPriority)
{
	register ieee8021BridgeUserPriorityRegenEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32UserPriority))
	{
		return NULL;
	}
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
// 	poTmpEntry->u32UserPriority = u32UserPriority;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeUserPriorityRegenEntry_t, oBTreeNode);
}

ieee8021BridgeUserPriorityRegenEntry_t *
ieee8021BridgeUserPriorityRegenTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32UserPriority)
{
	register ieee8021BridgeUserPriorityRegenEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
// 	poTmpEntry->u32UserPriority = u32UserPriority;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeUserPriorityRegenEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeUserPriorityRegenTable_removeEntry (ieee8021BridgeUserPriorityRegenEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeUserPriorityRegenTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeUserPriorityRegenTable_BTree);
	return ieee8021BridgeUserPriorityRegenTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeUserPriorityRegenTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeUserPriorityRegenEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	register netsnmp_variable_list *idx2 = put_index_data->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeUserPriorityRegenEntry_t, oBTreeNode);
	
	register uint32_t u32UserPriority =
		*idx2->val.integer != poEntry->u32BasePort ? ieee8021BridgePriority_min_c: ieee8021BridgePriority_getNext (*idx3->val.integer);
		
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, u32UserPriority);
	*my_data_context = (void*) poEntry;
	ieee8021BridgePriority_isLast (u32UserPriority) ? *my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree): false;
	return put_index_data;
}

bool
ieee8021BridgeUserPriorityRegenTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeUserPriorityRegenEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	poEntry = ieee8021BridgeUserPriorityRegenTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer,
		*idx3->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeUserPriorityRegenTable table mapper */
int
ieee8021BridgeUserPriorityRegenTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeUserPriorityRegenEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeUserPriorityRegenEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->au8RegenUserPriority[*idx3->val.integer]);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeUserPriorityRegenEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeUserPriorityRegenEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeUserPriorityRegenEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8RegenUserPriority[*idx3->val.integer]))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->au8RegenUserPriority[*idx3->val.integer], sizeof (table_entry->au8RegenUserPriority[*idx3->val.integer]));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->au8RegenUserPriority[*idx3->val.integer] = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeUserPriorityRegenEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				memcpy (&table_entry->au8RegenUserPriority[*idx3->val.integer], pvOldDdata, sizeof (table_entry->au8RegenUserPriority[*idx3->val.integer]));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeTrafficClassTable table mapper **/
void
ieee8021BridgeTrafficClassTable_init (void)
{
	extern oid ieee8021BridgeTrafficClassTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeTrafficClassTable", &ieee8021BridgeTrafficClassTable_mapper,
		ieee8021BridgeTrafficClassTable_oid, OID_LENGTH (ieee8021BridgeTrafficClassTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		ASN_UNSIGNED /* index: ieee8021BridgeTrafficClassPriority */,
		0);
	table_info->min_column = IEEE8021BRIDGETRAFFICCLASS;
	table_info->max_column = IEEE8021BRIDGETRAFFICCLASS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeTrafficClassTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeTrafficClassTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeTrafficClassTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeTrafficClassTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeTrafficClassEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeTrafficClassEntry_t, oBTreeNode);
	register ieee8021BridgeTrafficClassEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeTrafficClassEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgeTrafficClassTable_BTree = xBTree_initInline (&ieee8021BridgeTrafficClassTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeTrafficClassEntry_t *
ieee8021BridgeTrafficClassTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32Priority)
{
	register ieee8021BridgeTrafficClassEntry_t *poEntry = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32Priority))
	{
		return NULL;
	}
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
// 	poEntry->u32Priority = u32Priority;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree);
	return poEntry;
}

ieee8021BridgeTrafficClassEntry_t *
ieee8021BridgeTrafficClassTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32Priority)
{
	register ieee8021BridgeTrafficClassEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32Priority))
	{
		return NULL;
	}
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
// 	poTmpEntry->u32Priority = u32Priority;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeTrafficClassEntry_t, oBTreeNode);
}

ieee8021BridgeTrafficClassEntry_t *
ieee8021BridgeTrafficClassTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32Priority)
{
	register ieee8021BridgeTrafficClassEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
// 	poTmpEntry->u32Priority = u32Priority;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeTrafficClassEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeTrafficClassTable_removeEntry (ieee8021BridgeTrafficClassEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeTrafficClassTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeTrafficClassTable_BTree);
	return ieee8021BridgeTrafficClassTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeTrafficClassTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeTrafficClassEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	register netsnmp_variable_list *idx2 = put_index_data->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeTrafficClassEntry_t, oBTreeNode);
	
	register uint32_t u32Priority =
		*idx2->val.integer != poEntry->u32BasePort ? ieee8021BridgePriority_min_c: ieee8021BridgePriority_getNext (*idx3->val.integer);
		
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, u32Priority);
	*my_data_context = (void*) poEntry;
	ieee8021BridgePriority_isLast (u32Priority) ? *my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree): false;
	return put_index_data;
}

bool
ieee8021BridgeTrafficClassTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeTrafficClassEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	poEntry = ieee8021BridgeTrafficClassTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer,
		*idx3->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeTrafficClassTable table mapper */
int
ieee8021BridgeTrafficClassTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeTrafficClassEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeTrafficClassEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->au8Class[*idx3->val.integer]);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeTrafficClassEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeTrafficClassEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeTrafficClassEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8Class[*idx3->val.integer]))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->au8Class[*idx3->val.integer], sizeof (table_entry->au8Class[*idx3->val.integer]));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->au8Class[*idx3->val.integer] = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeTrafficClassEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				memcpy (&table_entry->au8Class[*idx3->val.integer], pvOldDdata, sizeof (table_entry->au8Class[*idx3->val.integer]));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePortOutboundAccessPriorityTable table mapper **/
void
ieee8021BridgePortOutboundAccessPriorityTable_init (void)
{
	extern oid ieee8021BridgePortOutboundAccessPriorityTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePortOutboundAccessPriorityTable", &ieee8021BridgePortOutboundAccessPriorityTable_mapper,
		ieee8021BridgePortOutboundAccessPriorityTable_oid, OID_LENGTH (ieee8021BridgePortOutboundAccessPriorityTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		ASN_UNSIGNED /* index: ieee8021BridgeRegenUserPriority */,
		0);
	table_info->min_column = IEEE8021BRIDGEPORTOUTBOUNDACCESSPRIORITY;
	table_info->max_column = IEEE8021BRIDGEPORTOUTBOUNDACCESSPRIORITY;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePortOutboundAccessPriorityTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePortOutboundAccessPriorityTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePortOutboundAccessPriorityTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgePortOutboundAccessPriorityTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePortOutboundAccessPriorityEntry_t, oBTreeNode);
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePortOutboundAccessPriorityEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgePortOutboundAccessPriorityTable_BTree = xBTree_initInline (&ieee8021BridgePortOutboundAccessPriorityTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgePortOutboundAccessPriorityEntry_t *
ieee8021BridgePortOutboundAccessPriorityTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32RegenUserPriority)
{
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *poEntry = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32RegenUserPriority))
	{
		return NULL;
	}
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
// 	poEntry->u32RegenUserPriority = u32RegenUserPriority;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree);
	return poEntry;
}

ieee8021BridgePortOutboundAccessPriorityEntry_t *
ieee8021BridgePortOutboundAccessPriorityTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32RegenUserPriority)
{
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32RegenUserPriority))
	{
		return NULL;
	}
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
// 	poTmpEntry->u32RegenUserPriority = u32RegenUserPriority;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortOutboundAccessPriorityEntry_t, oBTreeNode);
}

ieee8021BridgePortOutboundAccessPriorityEntry_t *
ieee8021BridgePortOutboundAccessPriorityTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32RegenUserPriority)
{
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
// 	poTmpEntry->u32RegenUserPriority = u32RegenUserPriority;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortOutboundAccessPriorityEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgePortOutboundAccessPriorityTable_removeEntry (ieee8021BridgePortOutboundAccessPriorityEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortOutboundAccessPriorityTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePortOutboundAccessPriorityTable_BTree);
	return ieee8021BridgePortOutboundAccessPriorityTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortOutboundAccessPriorityTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortOutboundAccessPriorityEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	register netsnmp_variable_list *idx2 = put_index_data->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortOutboundAccessPriorityEntry_t, oBTreeNode);
	
	register uint32_t u32RegenUserPriority =
		*idx2->val.integer != poEntry->u32BasePort ? ieee8021BridgePriority_min_c: ieee8021BridgePriority_getNext (*idx3->val.integer);
		
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, u32RegenUserPriority);
	*my_data_context = (void*) poEntry;
	ieee8021BridgePriority_isLast (u32RegenUserPriority) ? *my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree): false;
	return put_index_data;
}

bool
ieee8021BridgePortOutboundAccessPriorityTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortOutboundAccessPriorityEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	poEntry = ieee8021BridgePortOutboundAccessPriorityTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer,
		*idx3->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgePortOutboundAccessPriorityTable table mapper */
int
ieee8021BridgePortOutboundAccessPriorityTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePortOutboundAccessPriorityEntry_t *table_entry;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortOutboundAccessPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTOUTBOUNDACCESSPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->au8Priority[*idx3->val.integer]);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePortDecodingTable table mapper **/
void
ieee8021BridgePortDecodingTable_init (void)
{
	extern oid ieee8021BridgePortDecodingTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePortDecodingTable", &ieee8021BridgePortDecodingTable_mapper,
		ieee8021BridgePortDecodingTable_oid, OID_LENGTH (ieee8021BridgePortDecodingTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgePortDecodingComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgePortDecodingPortNum */,
		ASN_INTEGER /* index: ieee8021BridgePortDecodingPriorityCodePointRow */,
		ASN_INTEGER /* index: ieee8021BridgePortDecodingPriorityCodePoint */,
		0);
	table_info->min_column = IEEE8021BRIDGEPORTDECODINGPRIORITY;
	table_info->max_column = IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePortDecodingTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePortDecodingTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePortDecodingTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgePortDecodingTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePortDecodingEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePortDecodingEntry_t, oBTreeNode);
	register ieee8021BridgePortDecodingEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePortDecodingEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum < pEntry2->u32PortNum) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow < pEntry2->i32PriorityCodePointRow) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow) ? 0: 1;
}

xBTree_t oIeee8021BridgePortDecodingTable_BTree = xBTree_initInline (&ieee8021BridgePortDecodingTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgePortDecodingEntry_t *
ieee8021BridgePortDecodingTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint)
{
	register ieee8021BridgePortDecodingEntry_t *poEntry = NULL;
	
	if (!ieee8021BridgePriority_isValid (i32PriorityCodePoint))
	{
		return NULL;
	}
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32PortNum = u32PortNum;
	poEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
// 	poEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	for (i32PriorityCodePoint = ieee8021BridgePriority_min_c; i32PriorityCodePoint < ieee8021BridgePriority_max_c; i32PriorityCodePoint++)
	{
		poEntry->au8Priority[i32PriorityCodePoint] = ieee8021BridgePcpDecodingTable[i32PriorityCodePointRow][i32PriorityCodePoint].ubPcp;
		poEntry->au8DropEligible[i32PriorityCodePoint] = ieee8021BridgePcpDecodingTable[i32PriorityCodePointRow][i32PriorityCodePoint].ubDei;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree);
	return poEntry;
}

ieee8021BridgePortDecodingEntry_t *
ieee8021BridgePortDecodingTable_getByIndex (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint)
{
	register ieee8021BridgePortDecodingEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if (!ieee8021BridgePriority_isValid (i32PriorityCodePoint))
	{
		return NULL;
	}
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
	poTmpEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
// 	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortDecodingEntry_t, oBTreeNode);
}

ieee8021BridgePortDecodingEntry_t *
ieee8021BridgePortDecodingTable_getNextIndex (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint)
{
	register ieee8021BridgePortDecodingEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
	poTmpEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
// 	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortDecodingEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgePortDecodingTable_removeEntry (ieee8021BridgePortDecodingEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortDecodingTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePortDecodingTable_BTree);
	return ieee8021BridgePortDecodingTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortDecodingTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortDecodingEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	register netsnmp_variable_list *idx3 = put_index_data->next_variable->next_variable;
	register netsnmp_variable_list *idx4 = idx3->next_variable;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortDecodingEntry_t, oBTreeNode);
	
	register int32_t i32PriorityCodePoint =
		*idx3->val.integer != poEntry->i32PriorityCodePointRow ? ieee8021BridgePriority_min_c: ieee8021BridgePriority_getNext (*idx4->val.integer);
		
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32PortNum);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32PriorityCodePointRow);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, i32PriorityCodePoint);
	*my_data_context = (void*) poEntry;
	ieee8021BridgePriority_isLast (i32PriorityCodePoint) ? *my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree): false;
	return put_index_data;
}

bool
ieee8021BridgePortDecodingTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortDecodingEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	register netsnmp_variable_list *idx4 = idx3->next_variable;
	
	poEntry = ieee8021BridgePortDecodingTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer,
		*idx3->val.integer,
		*idx4->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgePortDecodingTable table mapper */
int
ieee8021BridgePortDecodingTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePortDecodingEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortDecodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx4 = table_info->indexes->next_variable->next_variable->next_variable;
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->au8Priority[*idx4->val.integer]);
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->au8DropEligible[*idx4->val.integer]);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortDecodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortDecodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgePortDecodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx4 = table_info->indexes->next_variable->next_variable->next_variable;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8Priority[*idx4->val.integer]))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->au8Priority[*idx4->val.integer], sizeof (table_entry->au8Priority[*idx4->val.integer]));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->au8Priority[*idx4->val.integer] = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8DropEligible[*idx4->val.integer]))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->au8DropEligible[*idx4->val.integer], sizeof (table_entry->au8DropEligible[*idx4->val.integer]));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->au8DropEligible[*idx4->val.integer] = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgePortDecodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx4 = table_info->indexes->next_variable->next_variable->next_variable;
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				memcpy (&table_entry->au8Priority[*idx4->val.integer], pvOldDdata, sizeof (table_entry->au8Priority[*idx4->val.integer]));
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				memcpy (&table_entry->au8DropEligible[*idx4->val.integer], pvOldDdata, sizeof (table_entry->au8DropEligible[*idx4->val.integer]));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePortEncodingTable table mapper **/
void
ieee8021BridgePortEncodingTable_init (void)
{
	extern oid ieee8021BridgePortEncodingTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePortEncodingTable", &ieee8021BridgePortEncodingTable_mapper,
		ieee8021BridgePortEncodingTable_oid, OID_LENGTH (ieee8021BridgePortEncodingTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgePortEncodingComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgePortEncodingPortNum */,
		ASN_INTEGER /* index: ieee8021BridgePortEncodingPriorityCodePointRow */,
		ASN_INTEGER /* index: ieee8021BridgePortEncodingPriorityCodePoint */,
		ASN_INTEGER /* index: ieee8021BridgePortEncodingDropEligible */,
		0);
	table_info->min_column = IEEE8021BRIDGEPORTENCODINGPRIORITY;
	table_info->max_column = IEEE8021BRIDGEPORTENCODINGPRIORITY;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePortEncodingTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePortEncodingTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePortEncodingTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgePortEncodingTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePortEncodingEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePortEncodingEntry_t, oBTreeNode);
	register ieee8021BridgePortEncodingEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePortEncodingEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum < pEntry2->u32PortNum) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow < pEntry2->i32PriorityCodePointRow) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow) ? 0: 1;
}

xBTree_t oIeee8021BridgePortEncodingTable_BTree = xBTree_initInline (&ieee8021BridgePortEncodingTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgePortEncodingEntry_t *
ieee8021BridgePortEncodingTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint,
	uint8_t u8DropEligible)
{
	register ieee8021BridgePortEncodingEntry_t *poEntry = NULL;
	
	if (!ieee8021BridgePriority_isValid (i32PriorityCodePoint) || !ieee8021BridgeDEI_isValid (u8DropEligible))
	{
		return NULL;
	}
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32PortNum = u32PortNum;
	poEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
// 	poEntry->i32PriorityCodePoint = i32PriorityCodePoint;
// 	poEntry->u8DropEligible = u8DropEligible;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	for (i32PriorityCodePoint = ieee8021BridgePriority_min_c; i32PriorityCodePoint < ieee8021BridgePriority_max_c; i32PriorityCodePoint++)
	{
		poEntry->au8Priority[i32PriorityCodePoint][0] = ieee8021BridgePcpEncodingTable[i32PriorityCodePointRow][i32PriorityCodePoint][0];
		poEntry->au8Priority[i32PriorityCodePoint][1] = ieee8021BridgePcpEncodingTable[i32PriorityCodePointRow][i32PriorityCodePoint][1];
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree);
	return poEntry;
}

ieee8021BridgePortEncodingEntry_t *
ieee8021BridgePortEncodingTable_getByIndex (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint,
	uint8_t u8DropEligible)
{
	register ieee8021BridgePortEncodingEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if (!ieee8021BridgePriority_isValid (i32PriorityCodePoint) || !ieee8021BridgeDEI_isValid (u8DropEligible))
	{
		return NULL;
	}
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
	poTmpEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
// 	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
// 	poTmpEntry->u8DropEligible = u8DropEligible;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortEncodingEntry_t, oBTreeNode);
}

ieee8021BridgePortEncodingEntry_t *
ieee8021BridgePortEncodingTable_getNextIndex (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint,
	uint8_t u8DropEligible)
{
	register ieee8021BridgePortEncodingEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
	poTmpEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
// 	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
// 	poTmpEntry->u8DropEligible = u8DropEligible;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortEncodingEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgePortEncodingTable_removeEntry (ieee8021BridgePortEncodingEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortEncodingTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePortEncodingTable_BTree);
	return ieee8021BridgePortEncodingTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortEncodingTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortEncodingEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	register netsnmp_variable_list *idx3 = put_index_data->next_variable->next_variable;
	register netsnmp_variable_list *idx4 = idx3->next_variable;
	register netsnmp_variable_list *idx5 = idx4->next_variable;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortEncodingEntry_t, oBTreeNode);
	
	register int32_t i32PriorityCodePoint =
		*idx3->val.integer != poEntry->i32PriorityCodePointRow ? ieee8021BridgePriority_min_c: ieee8021BridgePriority_getNext (*idx4->val.integer);
	register uint8_t u8DropEligible =
		*idx4->val.integer != i32PriorityCodePoint ? ieee8021BridgeDEI_min_c: ieee8021BridgeDEI_getNext (*idx5->val.integer);
		
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32PortNum);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32PriorityCodePointRow);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, i32PriorityCodePoint);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, u8DropEligible);
	*my_data_context = (void*) poEntry;
	ieee8021BridgePriority_isLast (i32PriorityCodePoint) && ieee8021BridgeDEI_isLast (u8DropEligible) ? *my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree): false;
	return put_index_data;
}

bool
ieee8021BridgePortEncodingTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortEncodingEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	register netsnmp_variable_list *idx4 = idx3->next_variable;
	register netsnmp_variable_list *idx5 = idx4->next_variable;
	
	poEntry = ieee8021BridgePortEncodingTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer,
		*idx3->val.integer,
		*idx4->val.integer,
		*idx5->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgePortEncodingTable table mapper */
int
ieee8021BridgePortEncodingTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePortEncodingEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortEncodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx4 = table_info->indexes->next_variable->next_variable->next_variable;
			register netsnmp_variable_list *idx5 = idx4->next_variable;
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c]);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortEncodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgePortEncodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgePortEncodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx4 = table_info->indexes->next_variable->next_variable->next_variable;
			register netsnmp_variable_list *idx5 = idx4->next_variable;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c]))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c], sizeof (table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c]));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c] = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgePortEncodingEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx4 = table_info->indexes->next_variable->next_variable->next_variable;
			register netsnmp_variable_list *idx5 = idx4->next_variable;
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				memcpy (&table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c], pvOldDdata, sizeof (table_entry->au8Priority[*idx4->val.integer][*idx5->val.integer - ieee8021BridgeDEI_min_c]));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeServiceAccessPriorityTable table mapper **/
void
ieee8021BridgeServiceAccessPriorityTable_init (void)
{
	extern oid ieee8021BridgeServiceAccessPriorityTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeServiceAccessPriorityTable", &ieee8021BridgeServiceAccessPriorityTable_mapper,
		ieee8021BridgeServiceAccessPriorityTable_oid, OID_LENGTH (ieee8021BridgeServiceAccessPriorityTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeServiceAccessPriorityComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeServiceAccessPriorityPortNum */,
		ASN_UNSIGNED /* index: ieee8021BridgeServiceAccessPriorityReceived */,
		0);
	table_info->min_column = IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE;
	table_info->max_column = IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeServiceAccessPriorityTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeServiceAccessPriorityTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeServiceAccessPriorityTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeServiceAccessPriorityTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeServiceAccessPriorityEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeServiceAccessPriorityEntry_t, oBTreeNode);
	register ieee8021BridgeServiceAccessPriorityEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeServiceAccessPriorityEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum < pEntry2->u32PortNum) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum) ? 0: 1;
}

xBTree_t oIeee8021BridgeServiceAccessPriorityTable_BTree = xBTree_initInline (&ieee8021BridgeServiceAccessPriorityTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeServiceAccessPriorityEntry_t *
ieee8021BridgeServiceAccessPriorityTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	uint32_t u32Received)
{
	register ieee8021BridgeServiceAccessPriorityEntry_t *poEntry = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32Received))
	{
		return NULL;
	}
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32PortNum = u32PortNum;
// 	poEntry->u32Received = u32Received;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree);
	return poEntry;
}

ieee8021BridgeServiceAccessPriorityEntry_t *
ieee8021BridgeServiceAccessPriorityTable_getByIndex (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	uint32_t u32Received)
{
	register ieee8021BridgeServiceAccessPriorityEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if (!ieee8021BridgePriority_isValid (u32Received))
	{
		return NULL;
	}
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
// 	poTmpEntry->u32Received = u32Received;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeServiceAccessPriorityEntry_t, oBTreeNode);
}

ieee8021BridgeServiceAccessPriorityEntry_t *
ieee8021BridgeServiceAccessPriorityTable_getNextIndex (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	uint32_t u32Received)
{
	register ieee8021BridgeServiceAccessPriorityEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
// 	poTmpEntry->u32Received = u32Received;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeServiceAccessPriorityEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeServiceAccessPriorityTable_removeEntry (ieee8021BridgeServiceAccessPriorityEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeServiceAccessPriorityTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeServiceAccessPriorityTable_BTree);
	return ieee8021BridgeServiceAccessPriorityTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeServiceAccessPriorityTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeServiceAccessPriorityEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	register netsnmp_variable_list *idx2 = put_index_data->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeServiceAccessPriorityEntry_t, oBTreeNode);
	
	register uint32_t u32Received =
		*idx2->val.integer != poEntry->u32PortNum ? ieee8021BridgePriority_min_c: ieee8021BridgePriority_getNext (*idx3->val.integer);
		
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32PortNum);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, u32Received);
	*my_data_context = (void*) poEntry;
	ieee8021BridgePriority_isLast (u32Received) ? *my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree): false;
	return put_index_data;
}

bool
ieee8021BridgeServiceAccessPriorityTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeServiceAccessPriorityEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	poEntry = ieee8021BridgeServiceAccessPriorityTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer,
		*idx3->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeServiceAccessPriorityTable table mapper */
int
ieee8021BridgeServiceAccessPriorityTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeServiceAccessPriorityEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeServiceAccessPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->au8Value[*idx3->val.integer]);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeServiceAccessPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeServiceAccessPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeServiceAccessPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8Value[*idx3->val.integer]))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->au8Value[*idx3->val.integer], sizeof (table_entry->au8Value[*idx3->val.integer]));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->au8Value[*idx3->val.integer] = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeServiceAccessPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx3 = table_info->indexes->next_variable->next_variable;
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				memcpy (&table_entry->au8Value[*idx3->val.integer], pvOldDdata, sizeof (table_entry->au8Value[*idx3->val.integer]));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePortMrpTable table mapper **/
void
ieee8021BridgePortMrpTable_init (void)
{
	extern oid ieee8021BridgePortMrpTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePortMrpTable", &ieee8021BridgePortMrpTable_mapper,
		ieee8021BridgePortMrpTable_oid, OID_LENGTH (ieee8021BridgePortMrpTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		0);
	table_info->min_column = IEEE8021BRIDGEPORTMRPJOINTIME;
	table_info->max_column = IEEE8021BRIDGEPORTMRPLEAVEALLTIME;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePortMrpTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePortMrpTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePortMrpTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
ieee8021BridgePortMrpEntry_t *
ieee8021BridgePortMrpTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMrpEntry_t *poEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	poEntry = &poPort->oMrp;
	
	poEntry->i32JoinTime = 20;
	poEntry->i32LeaveTime = 60;
	poEntry->i32LeaveAllTime = 1000;
	
	return poEntry;
}

ieee8021BridgePortMrpEntry_t *
ieee8021BridgePortMrpTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	
	return &poPort->oMrp;
}

ieee8021BridgePortMrpEntry_t *
ieee8021BridgePortMrpTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getNextIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	
	return &poPort->oMrp;
}

/* remove a row from the table */
void
ieee8021BridgePortMrpTable_removeEntry (ieee8021BridgePortMrpEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortMrpTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeBasePortTable_BTree);
	return ieee8021BridgePortMrpTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortMrpTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeBasePortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePortMrpTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgeBasePortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgePortMrpTable table mapper */
int
ieee8021BridgePortMrpTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePortMrpEntry_t *table_entry;
	register ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oMrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMRPJOINTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32JoinTime);
				break;
			case IEEE8021BRIDGEPORTMRPLEAVETIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32LeaveTime);
				break;
			case IEEE8021BRIDGEPORTMRPLEAVEALLTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32LeaveAllTime);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oMrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMRPJOINTIME:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTMRPLEAVETIME:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTMRPLEAVEALLTIME:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oMrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMRPJOINTIME:
			case IEEE8021BRIDGEPORTMRPLEAVETIME:
			case IEEE8021BRIDGEPORTMRPLEAVEALLTIME:
				if (poEntry->u8RowStatus == xRowStatus_active_c || poEntry->u8RowStatus == xRowStatus_notReady_c)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oMrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMRPJOINTIME:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32JoinTime))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32JoinTime, sizeof (table_entry->i32JoinTime));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32JoinTime = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTMRPLEAVETIME:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32LeaveTime))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32LeaveTime, sizeof (table_entry->i32LeaveTime));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32LeaveTime = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTMRPLEAVEALLTIME:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32LeaveAllTime))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32LeaveAllTime, sizeof (table_entry->i32LeaveAllTime));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32LeaveAllTime = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oMrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMRPJOINTIME:
				memcpy (&table_entry->i32JoinTime, pvOldDdata, sizeof (table_entry->i32JoinTime));
				break;
			case IEEE8021BRIDGEPORTMRPLEAVETIME:
				memcpy (&table_entry->i32LeaveTime, pvOldDdata, sizeof (table_entry->i32LeaveTime));
				break;
			case IEEE8021BRIDGEPORTMRPLEAVEALLTIME:
				memcpy (&table_entry->i32LeaveAllTime, pvOldDdata, sizeof (table_entry->i32LeaveAllTime));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgePortMmrpTable table mapper **/
void
ieee8021BridgePortMmrpTable_init (void)
{
	extern oid ieee8021BridgePortMmrpTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgePortMmrpTable", &ieee8021BridgePortMmrpTable_mapper,
		ieee8021BridgePortMmrpTable_oid, OID_LENGTH (ieee8021BridgePortMmrpTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		0);
	table_info->min_column = IEEE8021BRIDGEPORTMMRPENABLEDSTATUS;
	table_info->max_column = IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgePortMmrpTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgePortMmrpTable_getNext;
	iinfo->get_data_point = &ieee8021BridgePortMmrpTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
ieee8021BridgePortMmrpEntry_t *
ieee8021BridgePortMmrpTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMmrpEntry_t *poEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	poEntry = &poPort->oMmrp;
	
	poEntry->u8EnabledStatus = ieee8021BridgePortMmrpEnabledStatus_true_c;
	poEntry->u8RestrictedGroupRegistration = ieee8021BridgePortRestrictedGroupRegistration_false_c;
	
	return poEntry;
}

ieee8021BridgePortMmrpEntry_t *
ieee8021BridgePortMmrpTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getByIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	
	return &poPort->oMmrp;
}

ieee8021BridgePortMmrpEntry_t *
ieee8021BridgePortMmrpTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeBasePortEntry_t *poPort = NULL;
	
	if ((poPort = ieee8021BridgeBasePortTable_getNextIndex (u32BasePortComponentId, u32BasePort)) == NULL)
	{
		return NULL;
	}
	
	return &poPort->oMmrp;
}

/* remove a row from the table */
void
ieee8021BridgePortMmrpTable_removeEntry (ieee8021BridgePortMmrpEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortMmrpTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeBasePortTable_BTree);
	return ieee8021BridgePortMmrpTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortMmrpTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeBasePortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeBasePortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePortMmrpTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgeBasePortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgePortMmrpTable table mapper */
int
ieee8021BridgePortMmrpTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgePortMmrpEntry_t *table_entry;
	register ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oMmrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8EnabledStatus);
				break;
			case IEEE8021BRIDGEPORTMMRPFAILEDREGISTRATIONS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64FailedRegistrations);
				break;
			case IEEE8021BRIDGEPORTMMRPLASTPDUORIGIN:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8LastPduOrigin, table_entry->u16LastPduOrigin_len);
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RestrictedGroupRegistration);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oMmrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oMmrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				if (poEntry->u8RowStatus == xRowStatus_active_c || poEntry->u8RowStatus == xRowStatus_notReady_c)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_FREE:
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oMmrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8EnabledStatus))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8EnabledStatus, sizeof (table_entry->u8EnabledStatus));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8EnabledStatus = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8RestrictedGroupRegistration))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8RestrictedGroupRegistration, sizeof (table_entry->u8RestrictedGroupRegistration));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8RestrictedGroupRegistration = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (ieee8021BridgeBasePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oMmrp;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				memcpy (&table_entry->u8EnabledStatus, pvOldDdata, sizeof (table_entry->u8EnabledStatus));
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				memcpy (&table_entry->u8RestrictedGroupRegistration, pvOldDdata, sizeof (table_entry->u8RestrictedGroupRegistration));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeILanIfTable table mapper **/
void
ieee8021BridgeILanIfTable_init (void)
{
	extern oid ieee8021BridgeILanIfTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeILanIfTable", &ieee8021BridgeILanIfTable_mapper,
		ieee8021BridgeILanIfTable_oid, OID_LENGTH (ieee8021BridgeILanIfTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifIndex */,
		0);
	table_info->min_column = IEEE8021BRIDGEILANIFROWSTATUS;
	table_info->max_column = IEEE8021BRIDGEILANIFROWSTATUS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeILanIfTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeILanIfTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeILanIfTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeILanIfTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeILanIfEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeILanIfEntry_t, oBTreeNode);
	register ieee8021BridgeILanIfEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeILanIfEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

xBTree_t oIeee8021BridgeILanIfTable_BTree = xBTree_initInline (&ieee8021BridgeILanIfTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeILanIfEntry_t *
ieee8021BridgeILanIfTable_createEntry (
	uint32_t u32Index)
{
	register ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Index = u32Index;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree);
	return poEntry;
}

ieee8021BridgeILanIfEntry_t *
ieee8021BridgeILanIfTable_getByIndex (
	uint32_t u32Index)
{
	register ieee8021BridgeILanIfEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeILanIfEntry_t, oBTreeNode);
}

ieee8021BridgeILanIfEntry_t *
ieee8021BridgeILanIfTable_getNextIndex (
	uint32_t u32Index)
{
	register ieee8021BridgeILanIfEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeILanIfEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeILanIfTable_removeEntry (ieee8021BridgeILanIfEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

ieee8021BridgeILanIfEntry_t *
ieee8021BridgeILanIfTable_createRegister (
	uint32_t u32Index)
{
	bool bRetCode = false;
	register ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	
	ieee8021Bridge_wrLock ();
	
	if ((poEntry = ieee8021BridgeILanIfTable_createExt (u32Index)) == NULL)
	{
		goto ieee8021BridgeILanIfTable_createRegister_cleanup;
	}
	
	if (!ieee8021BridgeILanIfRowStatus_handler (poEntry, xRowStatus_active_c))
	{
		goto ieee8021BridgeILanIfTable_createRegister_cleanup;
	}
	
	bRetCode = true;
	
ieee8021BridgeILanIfTable_createRegister_cleanup:
	
	!bRetCode && poEntry != NULL ? ieee8021BridgeILanIfTable_removeExt (poEntry): false;
	ieee8021Bridge_unLock ();
	
	return bRetCode ? poEntry: NULL;
}

bool
ieee8021BridgeILanIfTable_removeRegister (
	uint32_t u32Index)
{
	bool bRetCode = false;
	register ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	
	ieee8021Bridge_wrLock ();
	
	if ((poEntry = ieee8021BridgeILanIfTable_getByIndex (u32Index)) == NULL)
	{
		goto ieee8021BridgeILanIfTable_removeRegister_success;
	}
	
	if (!ieee8021BridgeILanIfRowStatus_handler (poEntry, xRowStatus_destroy_c) ||
		!ieee8021BridgeILanIfTable_removeExt (poEntry))
	{
		goto ieee8021BridgeILanIfTable_removeRegister_cleanup;
	}
	
ieee8021BridgeILanIfTable_removeRegister_success:
	
	bRetCode = true;
	
ieee8021BridgeILanIfTable_removeRegister_cleanup:
	
	ieee8021Bridge_unLock ();
	
	return bRetCode;
}

ieee8021BridgeILanIfEntry_t *
ieee8021BridgeILanIfTable_createExt (
	uint32_t u32Index)
{
	register bool bIfReserved = u32Index != ifIndex_zero_c;
	register ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	
	if (!bIfReserved)
	{
		ifEntry_t *poILanIfEntry = NULL;
		
		if (!ifTable_createReference (u32Index, ifType_ilan_c, ifAdminStatus_up_c, true, true, false, &poILanIfEntry))
		{
			goto ieee8021BridgeILanIfTable_createExt_cleanup;
		}
		
		bIfReserved = true;
		u32Index = poILanIfEntry->u32Index;
		
		ifEntry_unLock (poILanIfEntry);
	}
	
	poEntry = ieee8021BridgeILanIfTable_createEntry (
		u32Index);
	if (poEntry == NULL)
	{
		goto ieee8021BridgeILanIfTable_createExt_cleanup;
	}
	
	if (!bIfReserved && !ieee8021BridgeILanIfTable_createHier (poEntry))
	{
		ieee8021BridgeILanIfTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ieee8021BridgeILanIfTable_createExt_cleanup;
	}
	
ieee8021BridgeILanIfTable_createExt_cleanup:
	
	return poEntry;
}

bool
ieee8021BridgeILanIfTable_removeExt (ieee8021BridgeILanIfEntry_t *poEntry)
{
	if (!ieee8021BridgeILanIfTable_removeHier (poEntry))
	{
		return false;
	}
	ieee8021BridgeILanIfTable_removeEntry (poEntry);
	
	return true;
}

bool
ieee8021BridgeILanIfTable_createHier (
	ieee8021BridgeILanIfEntry_t *poEntry)
{
	if (!ifTable_createReference (poEntry->u32Index, 0, ifAdminStatus_up_c, true, true, false, NULL))
	{
		goto ieee8021BridgeILanIfTable_createHier_cleanup;
	}
	
	return true;
	
	
ieee8021BridgeILanIfTable_createHier_cleanup:
	
	ieee8021BridgeILanIfTable_removeHier (poEntry);
	return false;
}

bool
ieee8021BridgeILanIfTable_removeHier (
	ieee8021BridgeILanIfEntry_t *poEntry)
{
	return ifTable_removeReference (poEntry->u32Index, true, true, true);
}

bool
ieee8021BridgeILanIfRowStatus_handler (
	ieee8021BridgeILanIfEntry_t *poEntry, uint8_t u8RowStatus)
{
	register bool bRetCode = false;
	ifEntry_t *poILanIfEntry = NULL;
	
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto ieee8021BridgeILanIfRowStatus_handler_success;
	}
	
	if (u8RowStatus == xRowStatus_createAndGo_c)
	{
		u8RowStatus = xRowStatus_active_c;
	}
	
	if (!ifTable_createReference (poEntry->u32Index, 0, 0, false, false, false, &poILanIfEntry) ||
		!neIfRowStatus_handler (&poILanIfEntry->oNe, u8RowStatus))
	{
		goto ieee8021BridgeILanIfRowStatus_handler_cleanup;
	}
	
ieee8021BridgeILanIfRowStatus_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeILanIfRowStatus_handler_cleanup:
	
	poILanIfEntry != NULL ? ifEntry_unLock (poILanIfEntry): false;
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeILanIfTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeILanIfTable_BTree);
	return ieee8021BridgeILanIfTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeILanIfTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeILanIfEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeILanIfTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgeILanIfTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ieee8021BridgeILanIfTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeILanIfTable table mapper */
int
ieee8021BridgeILanIfTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeILanIfEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RowStatus);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = ieee8021BridgeILanIfTable_createExt (
						*idx1->val.integer);
					if (table_entry != NULL)
					{
						netsnmp_insert_iterator_context (request, table_entry);
						netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, table_entry, &xBuffer_free));
					}
					else
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
						return SNMP_ERR_NOERROR;
					}
					break;
					
				case RS_DESTROY:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					break;
				}
			default:
				if (table_entry == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				}
				break;
			}
		}
		break;
		
	case MODE_SET_FREE:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ieee8021BridgeILanIfTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
			}
		}
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!ieee8021BridgeILanIfRowStatus_handler (table_entry, *request->requestvb->val.integer))
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					break;
				}
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ieee8021BridgeILanIfTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeILanIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEILANIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					ieee8021BridgeILanIfTable_removeExt (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021BridgeDot1dPortTable table mapper **/
void
ieee8021BridgeDot1dPortTable_init (void)
{
	extern oid ieee8021BridgeDot1dPortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021BridgeDot1dPortTable", &ieee8021BridgeDot1dPortTable_mapper,
		ieee8021BridgeDot1dPortTable_oid, OID_LENGTH (ieee8021BridgeDot1dPortTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021BridgeBasePort */,
		0);
	table_info->min_column = IEEE8021BRIDGEDOT1DPORTROWSTATUS;
	table_info->max_column = IEEE8021BRIDGEDOT1DPORTROWSTATUS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021BridgeDot1dPortTable_getFirst;
	iinfo->get_next_data_point = &ieee8021BridgeDot1dPortTable_getNext;
	iinfo->get_data_point = &ieee8021BridgeDot1dPortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021BridgeDot1dPortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeDot1dPortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeDot1dPortEntry_t, oBTreeNode);
	register ieee8021BridgeDot1dPortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeDot1dPortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgeDot1dPortTable_BTree = xBTree_initInline (&ieee8021BridgeDot1dPortTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021BridgeDot1dPortEntry_t *
ieee8021BridgeDot1dPortTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeDot1dPortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree);
	return poEntry;
}

ieee8021BridgeDot1dPortEntry_t *
ieee8021BridgeDot1dPortTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeDot1dPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeDot1dPortEntry_t, oBTreeNode);
}

ieee8021BridgeDot1dPortEntry_t *
ieee8021BridgeDot1dPortTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgeDot1dPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeDot1dPortEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeDot1dPortTable_removeEntry (ieee8021BridgeDot1dPortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

ieee8021BridgeDot1dPortEntry_t *
ieee8021BridgeDot1dPortTable_createExt (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	ieee8021BridgeDot1dPortEntry_t *poEntry = NULL;
	
	poEntry = ieee8021BridgeDot1dPortTable_createEntry (
		u32BasePortComponentId,
		u32BasePort);
	if (poEntry == NULL)
	{
		return NULL;
	}
	
	if (!ieee8021BridgeDot1dPortTable_createHier (poEntry))
	{
		ieee8021BridgeDot1dPortTable_removeEntry (poEntry);
		return NULL;
	}
	
	return poEntry;
}

bool
ieee8021BridgeDot1dPortTable_removeExt (ieee8021BridgeDot1dPortEntry_t *poEntry)
{
	if (!ieee8021BridgeDot1dPortTable_removeHier (poEntry))
	{
		return false;
	}
	ieee8021BridgeDot1dPortTable_removeEntry (poEntry);
	
	return true;
}

bool
ieee8021BridgeDot1dPortTable_createHier (
	ieee8021BridgeDot1dPortEntry_t *poEntry)
{
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32BasePortComponentId)) == NULL ||
		(poIeee8021BridgeBaseEntry->u8RowStatus == xRowStatus_active_c && poIeee8021BridgeBaseEntry->i32ComponentType != ieee8021BridgeBaseComponentType_dBridgeComponent_c))
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	register ieee8021BridgeBasePortEntry_t *poIeee8021BridgeBasePortEntry = NULL;
	
	if ((poIeee8021BridgeBasePortEntry = ieee8021BridgeBasePortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) == NULL &&
		(poIeee8021BridgeBasePortEntry = ieee8021BridgeBasePortTable_createExt (poIeee8021BridgeBaseEntry, poEntry->u32BasePort)) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	poIeee8021BridgeBasePortEntry->i32Type = ieee8021BridgeBasePortType_dBridgePort_c;
	
	return true;
	
	
ieee8021BridgeDot1dPortTable_createHier_cleanup:
	
	ieee8021BridgeDot1dPortTable_removeHier (poEntry);
	return false;
}

bool
ieee8021BridgeDot1dPortTable_removeHier (
	ieee8021BridgeDot1dPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poIeee8021BridgeBasePortEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32BasePortComponentId)) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_removeHier_success;
	}
	
	if ((poIeee8021BridgeBasePortEntry = ieee8021BridgeBasePortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) != NULL &&
		!ieee8021BridgeBasePortTable_removeExt (poIeee8021BridgeBaseEntry, poIeee8021BridgeBasePortEntry))
	{
		goto ieee8021BridgeDot1dPortTable_removeHier_cleanup;
	}
	
ieee8021BridgeDot1dPortTable_removeHier_success:
	
	bRetCode = true;
	
ieee8021BridgeDot1dPortTable_removeHier_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeDot1dPortRowStatus_handler (
	ieee8021BridgeDot1dPortEntry_t *poEntry, uint8_t u8RowStatus)
{
	register bool bRetCode = false;
	register uint8_t u8RealStatus = u8RowStatus & xRowStatus_mask_c;
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32BasePortComponentId)) == NULL)
	{
		goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
	}
	
	if (poEntry->u8RowStatus == u8RealStatus)
	{
		goto ieee8021BridgeDot1dPortRowStatus_handler_success;
	}
	if (u8RowStatus & xRowStatus_fromParent_c &&
		((u8RealStatus == xRowStatus_active_c && poEntry->u8RowStatus != xRowStatus_notReady_c) ||
		 (u8RealStatus == xRowStatus_notInService_c && poEntry->u8RowStatus != xRowStatus_active_c)))
	{
		goto ieee8021BridgeDot1dPortRowStatus_handler_success;
	}
	
	
	switch (u8RealStatus)
	{
	case xRowStatus_active_c:
		if (!(u8RowStatus & xRowStatus_fromParent_c) && poIeee8021BridgeBaseEntry->u8RowStatus != xRowStatus_active_c)
		{
			u8RealStatus = xRowStatus_notReady_c;
		}
		
		if (!ieee8021BridgeDot1dPortRowStatus_update (poIeee8021BridgeBaseEntry, poEntry, u8RealStatus))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = u8RealStatus;
		break;
		
	case xRowStatus_notInService_c:
		if (!ieee8021BridgeDot1dPortRowStatus_update (poIeee8021BridgeBaseEntry, poEntry, u8RealStatus))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus =
			poEntry->u8RowStatus == xRowStatus_active_c && (u8RowStatus & xRowStatus_fromParent_c) ? xRowStatus_notReady_c: xRowStatus_notInService_c;
		break;
		
	case xRowStatus_createAndGo_c:
		goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!ieee8021BridgeDot1dPortRowStatus_update (poIeee8021BridgeBaseEntry, poEntry, u8RealStatus))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
ieee8021BridgeDot1dPortRowStatus_handler_success:
	
	bRetCode = true;
	
ieee8021BridgeDot1dPortRowStatus_handler_cleanup:
	
	return bRetCode || (u8RowStatus & xRowStatus_fromParent_c);
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeDot1dPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeDot1dPortTable_BTree);
	return ieee8021BridgeDot1dPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeDot1dPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeDot1dPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeDot1dPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeDot1dPortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgeDot1dPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeDot1dPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgeDot1dPortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021BridgeDot1dPortTable table mapper */
int
ieee8021BridgeDot1dPortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021BridgeDot1dPortEntry_t *table_entry;
	void *pvOldDdata = NULL;
	int ret;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RowStatus);
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;
		
	/*
	 * Write-support
	 */
	case MODE_SET_RESERVE1:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_ERR_NOTWRITABLE);
				return SNMP_ERR_NOERROR;
			}
		}
		break;
		
	case MODE_SET_RESERVE2:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			register netsnmp_variable_list *idx2 = idx1->next_variable;
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_WRONGVALUE);
					return SNMP_ERR_NOERROR;
					
				case RS_CREATEANDWAIT:
					table_entry = ieee8021BridgeDot1dPortTable_createExt (
						*idx1->val.integer,
						*idx2->val.integer);
					if (table_entry != NULL)
					{
						netsnmp_insert_iterator_context (request, table_entry);
						netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, table_entry, &xBuffer_free));
					}
					else
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
						return SNMP_ERR_NOERROR;
					}
					break;
					
				case RS_DESTROY:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					break;
				}
			default:
				if (table_entry == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				}
				break;
			}
		}
		break;
		
	case MODE_SET_FREE:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ieee8021BridgeDot1dPortTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
			}
		}
		break;
		
	case MODE_SET_ACTION:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!ieee8021BridgeDot1dPortRowStatus_handler (table_entry, *request->requestvb->val.integer))
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					break;
				}
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ieee8021BridgeDot1dPortTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ieee8021BridgeDot1dPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEDOT1DPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					ieee8021BridgeDot1dPortTable_removeExt (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}
