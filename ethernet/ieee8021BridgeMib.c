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

#define SNMP_SRC

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "system/systemMIB.h"
#include "ethernetUtils.h"
#include "ieee8021BridgeMib.h"
#include "if/ifMIB.h"

#include "lib/binaryTree.h"
#include "lib/buffer.h"
#include "lib/sync.h"
#include "lib/snmp.h"

#include <stdbool.h>

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
}


ieee8021Bridge_t oBridge =
{
	.oComponentLock = xRwLock_initInline (),
	.oPhyPortLock = xRwLock_initInline (),
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

xBTree_t oIeee8021BridgeBaseTable_BTree = xBTree_initInline (&ieee8021BridgeBaseTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
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
	
	poEntry->i32TrafficClassesEnabled = ieee8021BridgeBaseTrafficClassesEnabled_true_c;
	poEntry->i32MmrpEnabledStatus = ieee8021BridgeBaseMmrpEnabledStatus_true_c;
	poEntry->u8RowStatus = xRowStatus_notInService_c;
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
	xRwLock_destroy (&poEntry->oLock);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

ieee8021BridgeBaseEntry_t *
ieee8021BridgeBaseTable_createExt (
	uint32_t u32ComponentId)
{
	ieee8021BridgeBaseEntry_t *poEntry = NULL;
	
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
	return true;
}

bool
ieee8021BridgeBaseTable_removeHier (
	ieee8021BridgeBaseEntry_t *poEntry)
{
	return true;
}

bool ieee8021BridgeBaseTrafficClassesEnabled_handler (
	ieee8021BridgeBaseEntry_t *poEntry, int32_t i32TrafficClassesEnabled, bool bForce)
{
	if (poEntry->i32TrafficClassesEnabled == i32TrafficClassesEnabled && !bForce)
	{
		goto ieee8021BridgeBaseTrafficClassesEnabled_handler_success;
	}
	
	switch (i32TrafficClassesEnabled)
	{
	default:
		goto ieee8021BridgeBaseTrafficClassesEnabled_handler_cleanup;
		
	case ieee8021BridgeBaseTrafficClassesEnabled_true_c:
		/* TODO */
		
		!bForce ? (poEntry->i32TrafficClassesEnabled = ieee8021BridgeBaseTrafficClassesEnabled_true_c): false;
		break;
		
	case ieee8021BridgeBaseTrafficClassesEnabled_false_c:
		/* TODO */
		
		!bForce ? (poEntry->i32TrafficClassesEnabled = ieee8021BridgeBaseTrafficClassesEnabled_false_c): false;
		break;
	}
	
ieee8021BridgeBaseTrafficClassesEnabled_handler_success:
	
	return true;
	
	
ieee8021BridgeBaseTrafficClassesEnabled_handler_cleanup:
	
	return false;
}

bool ieee8021BridgeBaseMmrpEnabledStatus_handler (
	ieee8021BridgeBaseEntry_t *poEntry, int32_t i32MmrpEnabledStatus, bool bForce)
{
	if (poEntry->i32MmrpEnabledStatus == i32MmrpEnabledStatus && !bForce)
	{
		goto ieee8021BridgeBaseMmrpEnabledStatus_handler_success;
	}
	
	switch (i32MmrpEnabledStatus)
	{
	default:
		goto ieee8021BridgeBaseMmrpEnabledStatus_handler_cleanup;
		
	case ieee8021BridgeBaseMmrpEnabledStatus_true_c:
		/* TODO */
		
		!bForce ? (poEntry->i32MmrpEnabledStatus = ieee8021BridgeBaseMmrpEnabledStatus_true_c): false;
		break;
		
	case ieee8021BridgeBaseMmrpEnabledStatus_false_c:
		/* TODO */
		
		!bForce ? (poEntry->i32MmrpEnabledStatus = ieee8021BridgeBaseMmrpEnabledStatus_false_c): false;
		break;
	}
	
ieee8021BridgeBaseMmrpEnabledStatus_handler_success:
	
	return true;
	
	
ieee8021BridgeBaseMmrpEnabledStatus_handler_cleanup:
	
	return false;
}

bool
ieee8021BridgeBaseRowStatus_handler (
	ieee8021BridgeBaseEntry_t *poEntry, uint8_t u8RowStatus)
{
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto ieee8021BridgeBaseRowStatus_handler_success;
	}
	
	switch (u8RowStatus)
	{
	case xRowStatus_active_c:
		/* TODO */
		
		if (!ieee8021BridgeBaseTrafficClassesEnabled_handler (poEntry, poEntry->i32TrafficClassesEnabled, true))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		if (!ieee8021BridgeBaseMmrpEnabledStatus_handler (poEntry, poEntry->i32MmrpEnabledStatus, true))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		{
			register uint32_t u32BasePort = 0;
			register ieee8021BridgeDot1dPortEntry_t *poIeee8021BridgeDot1dPortEntry = NULL;
			
			while (
				(poIeee8021BridgeDot1dPortEntry = ieee8021BridgeDot1dPortTable_getNextIndex (poEntry->u32ComponentId, u32BasePort)) != NULL &&
				poIeee8021BridgeDot1dPortEntry->u32BasePortComponentId == poEntry->u32ComponentId)
			{
				u32BasePort = poIeee8021BridgeDot1dPortEntry->u32BasePort;
				
				if (!ieee8021BridgeDot1dPortRowStatus_handler (poIeee8021BridgeDot1dPortEntry, u8RowStatus | xRowStatus_fromParent_c))
				{
					goto ieee8021BridgeBaseRowStatus_handler_cleanup;
				}
			}
		}
		
		if (!ieee8021BridgeBaseRowStatus_update (poEntry, u8RowStatus))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_active_c;
		break;
		
	case xRowStatus_notInService_c:
		{
			register uint32_t u32BasePort = 0;
			register ieee8021BridgeDot1dPortEntry_t *poIeee8021BridgeDot1dPortEntry = NULL;
			
			while (
				(poIeee8021BridgeDot1dPortEntry = ieee8021BridgeDot1dPortTable_getNextIndex (poEntry->u32ComponentId, u32BasePort)) != NULL &&
				poIeee8021BridgeDot1dPortEntry->u32BasePortComponentId == poEntry->u32ComponentId)
			{
				u32BasePort = poIeee8021BridgeDot1dPortEntry->u32BasePort;
				
				if (!ieee8021BridgeDot1dPortRowStatus_handler (poIeee8021BridgeDot1dPortEntry, u8RowStatus | xRowStatus_fromParent_c))
				{
					goto ieee8021BridgeBaseRowStatus_handler_cleanup;
				}
			}
		}
		
		if (!ieee8021BridgeBaseRowStatus_update (poEntry, u8RowStatus))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_createAndGo_c:
		goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		{
			register uint32_t u32BasePort = 0;
			register ieee8021BridgeDot1dPortEntry_t *poIeee8021BridgeDot1dPortEntry = NULL;
			
			while (
				(poIeee8021BridgeDot1dPortEntry = ieee8021BridgeDot1dPortTable_getNextIndex (poEntry->u32ComponentId, u32BasePort)) != NULL &&
				poIeee8021BridgeDot1dPortEntry->u32BasePortComponentId == poEntry->u32ComponentId)
			{
				u32BasePort = poIeee8021BridgeDot1dPortEntry->u32BasePort;
				
				if (!ieee8021BridgeDot1dPortRowStatus_handler (poIeee8021BridgeDot1dPortEntry, u8RowStatus | xRowStatus_fromParent_c) ||
					!ieee8021BridgeDot1dPortTable_removeExt (poIeee8021BridgeDot1dPortEntry))
				{
					goto ieee8021BridgeBaseRowStatus_handler_cleanup;
				}
			}
		}
		
		if (!ieee8021BridgeBaseRowStatus_update (poEntry, u8RowStatus))
		{
			goto ieee8021BridgeBaseRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
ieee8021BridgeBaseRowStatus_handler_success:
	
	return true;
	
	
ieee8021BridgeBaseRowStatus_handler_cleanup:
	
	return false;
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
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32TrafficClassesEnabled);
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32MmrpEnabledStatus);
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
			case IEEE8021BRIDGEBASEBRIDGEADDRESS:
			case IEEE8021BRIDGEBASECOMPONENTTYPE:
			case IEEE8021BRIDGEBASEDEVICECAPABILITIES:
				if (table_entry->u8RowStatus == RS_ACTIVE || table_entry->u8RowStatus == RS_NOTREADY)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEBASEROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
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
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32TrafficClassesEnabled))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32TrafficClassesEnabled, sizeof (table_entry->i32TrafficClassesEnabled));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				if (!ieee8021BridgeBaseTrafficClassesEnabled_handler (table_entry, *request->requestvb->val.integer, false))
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32MmrpEnabledStatus))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32MmrpEnabledStatus, sizeof (table_entry->i32MmrpEnabledStatus));
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
				memcpy (&table_entry->i32TrafficClassesEnabled, pvOldDdata, sizeof (table_entry->i32TrafficClassesEnabled));
				break;
			case IEEE8021BRIDGEBASEMMRPENABLEDSTATUS:
				memcpy (&table_entry->i32MmrpEnabledStatus, pvOldDdata, sizeof (table_entry->i32MmrpEnabledStatus));
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

/* create a new row in the (unsorted) table */
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
	
	poEntry->i32AdminPointToPoint = ieee8021BridgeBasePortAdminPointToPoint_forceFalse_c;
	
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

ieee8021BridgeBasePortEntry_t *
ieee8021BridgeBasePortTable_createExt (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	ieee8021BridgeBasePortEntry_t *poEntry = NULL;
	
	poEntry = ieee8021BridgeBasePortTable_createEntry (
		u32ComponentId,
		u32Port);
	if (poEntry == NULL)
	{
		return NULL;
	}
	
	return poEntry;
}

bool
ieee8021BridgeBasePortTable_removeExt (ieee8021BridgeBasePortEntry_t *poEntry)
{
	ieee8021BridgeBasePortTable_removeEntry (poEntry);
	
	return true;
}

bool
ieee8021BridgeBasePortIfIndex_handler (ieee8021BridgeBasePortEntry_t *poEntry)
{
	ieee8021BridgePhyPortInfo_t oIeee8021BridgePhyPortInfo = ieee8021BridgePhyPortInfo_initInline (ieee8021BridgePhyPortInfo_all_c);
	
	
	if (poEntry->pOldEntry != NULL && poEntry->u32IfIndex == poEntry->pOldEntry->u32IfIndex)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_success;
	}
	
	
	if (poEntry->pOldEntry == NULL || poEntry->pOldEntry->u32IfIndex == 0)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_newIfIndex;
	}
	
	/* TODO */
	
	if (!ieee8021BridgePhyPortInfo_getByIfIndex (poEntry->pOldEntry->u32IfIndex, &oIeee8021BridgePhyPortInfo))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	oIeee8021BridgePhyPortInfo.poIfToPortEntry->u32IndexComponentId = 0;
	oIeee8021BridgePhyPortInfo.poIfToPortEntry->u32IndexPort = 0;
	if (oIeee8021BridgePhyPortInfo.poPhyPortEntry != NULL)
	{
		oIeee8021BridgePhyPortInfo.poPhyPortEntry->u32ToComponentId = 0;
		oIeee8021BridgePhyPortInfo.poPhyPortEntry->u32ToInternalPort = 0;
	}
	poEntry->i32External = ieee8021BridgeBasePortExternal_false_c;
	
	if (!ifData_removeReference (poEntry->pOldEntry->u32IfIndex, false, true, false))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	
ieee8021BridgeBasePortIfIndex_handler_newIfIndex:
	
	if (poEntry->u32IfIndex == 0)
	{
		goto ieee8021BridgeBasePortIfIndex_handler_success;
	}
	
	if (!ieee8021BridgePhyPortInfo_getByIfIndex (poEntry->u32IfIndex, &oIeee8021BridgePhyPortInfo))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	oIeee8021BridgePhyPortInfo.poIfToPortEntry->u32IndexComponentId = poEntry->u32ComponentId;
	oIeee8021BridgePhyPortInfo.poIfToPortEntry->u32IndexPort = poEntry->u32Port;
	if (oIeee8021BridgePhyPortInfo.poPhyPortEntry != NULL)
	{
		oIeee8021BridgePhyPortInfo.poPhyPortEntry->u32ToComponentId = poEntry->u32ComponentId;
		oIeee8021BridgePhyPortInfo.poPhyPortEntry->u32ToInternalPort = poEntry->u32Port;
	}
	poEntry->i32External = oIeee8021BridgePhyPortInfo.poPhyPortEntry == NULL ? ieee8021BridgeBasePortExternal_false_c: ieee8021BridgeBasePortExternal_true_c;
	
	if (!ifData_createReference (poEntry->u32IfIndex, 0, false, true, false, NULL))
	{
		goto ieee8021BridgeBasePortIfIndex_handler_cleanup;
	}
	
	/* TODO */
	
ieee8021BridgeBasePortIfIndex_handler_success:
	
	return true;
	
	
ieee8021BridgeBasePortIfIndex_handler_cleanup:
	
	return false;
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
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32External);
				break;
			case IEEE8021BRIDGEBASEPORTADMINPOINTTOPOINT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32AdminPointToPoint);
				break;
			case IEEE8021BRIDGEBASEPORTOPERPOINTTOPOINT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32OperPointToPoint);
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

static int8_t
ieee8021BridgeBaseIfToPortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgeBaseIfToPortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgeBaseIfToPortEntry_t, oBTreeNode);
	register ieee8021BridgeBaseIfToPortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgeBaseIfToPortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32IfIndex < pEntry2->u32IfIndex) ? -1:
		(pEntry1->u32IfIndex == pEntry2->u32IfIndex) ? 0: 1;
}

xBTree_t oIeee8021BridgeBaseIfToPortTable_BTree = xBTree_initInline (&ieee8021BridgeBaseIfToPortTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgeBaseIfToPortEntry_t *
ieee8021BridgeBaseIfToPortTable_createEntry (
	uint32_t u32IfIndex)
{
	register ieee8021BridgeBaseIfToPortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32IfIndex = u32IfIndex;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree);
	return poEntry;
}

ieee8021BridgeBaseIfToPortEntry_t *
ieee8021BridgeBaseIfToPortTable_getByIndex (
	uint32_t u32IfIndex)
{
	register ieee8021BridgeBaseIfToPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32IfIndex = u32IfIndex;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBaseIfToPortEntry_t, oBTreeNode);
}

ieee8021BridgeBaseIfToPortEntry_t *
ieee8021BridgeBaseIfToPortTable_getNextIndex (
	uint32_t u32IfIndex)
{
	register ieee8021BridgeBaseIfToPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32IfIndex = u32IfIndex;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgeBaseIfToPortEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgeBaseIfToPortTable_removeEntry (ieee8021BridgeBaseIfToPortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

ieee8021BridgeBaseIfToPortEntry_t *
ieee8021BridgeBaseIfToPortTable_createExt (
	uint32_t u32IfIndex)
{
	ieee8021BridgeBaseIfToPortEntry_t *poEntry = NULL;
	
	poEntry = ieee8021BridgeBaseIfToPortTable_createEntry (
		u32IfIndex);
	if (poEntry == NULL)
	{
		goto ieee8021BridgeBaseIfToPortTable_createExt_cleanup;
	}
	
	if (!ieee8021BridgeBaseIfToPortTable_createHier (poEntry))
	{
		ieee8021BridgeBaseIfToPortTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ieee8021BridgeBaseIfToPortTable_createExt_cleanup;
	}
	
	
ieee8021BridgeBaseIfToPortTable_createExt_cleanup:
	
	return poEntry;
}

bool
ieee8021BridgeBaseIfToPortTable_removeExt (ieee8021BridgeBaseIfToPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ieee8021BridgeBaseIfToPortTable_removeHier (poEntry))
	{
		goto ieee8021BridgeBaseIfToPortTable_removeExt_cleanup;
	}
	
	ieee8021BridgeBaseIfToPortTable_removeEntry (poEntry);
	bRetCode = true;
	
	
ieee8021BridgeBaseIfToPortTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
ieee8021BridgeBaseIfToPortTable_createHier (
	ieee8021BridgeBaseIfToPortEntry_t *poEntry)
{
	if (!ifData_createReference (poEntry->u32IfIndex, 0, false, true, false, NULL))
	{
		goto ieee8021BridgeBaseIfToPortTable_createHier_cleanup;
	}
	
	return true;
	
	
ieee8021BridgeBaseIfToPortTable_createHier_cleanup:
	
	ieee8021BridgeBaseIfToPortTable_removeHier (poEntry);
	return false;
}

bool
ieee8021BridgeBaseIfToPortTable_removeHier (
	ieee8021BridgeBaseIfToPortEntry_t *poEntry)
{
	if (!ifData_removeReference (poEntry->u32IfIndex, false, true, false))
	{
		goto ieee8021BridgeBaseIfToPortTable_removeHier_cleanup;
	}
	
	return true;
	
	
ieee8021BridgeBaseIfToPortTable_removeHier_cleanup:
	
	return false;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgeBaseIfToPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgeBaseIfToPortTable_BTree);
	return ieee8021BridgeBaseIfToPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgeBaseIfToPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBaseIfToPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeBaseIfToPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32IfIndex);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeBaseIfToPortTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgeBaseIfToPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgeBaseIfToPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ieee8021BridgeBaseIfToPortTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
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
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32IndexComponentId);
				break;
			case IEEE8021BRIDGEBASEIFINDEXPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32IndexPort);
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

xBTree_t oIeee8021BridgePhyPortTable_BTree = xBTree_initInline (&ieee8021BridgePhyPortTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_createEntry (
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Port = u32Port;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree);
	return poEntry;
}

ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_getByIndex (
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePhyPortEntry_t, oBTreeNode);
}

ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_getNextIndex (
	uint32_t u32Port)
{
	register ieee8021BridgePhyPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePhyPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePhyPortEntry_t, oBTreeNode);
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
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

bool
ieee8021BridgePhyPortInfo_getByIfIndex (
	uint32_t u32IfIndex,
	ieee8021BridgePhyPortInfo_t *poPhyPortInfo)
{
	register ieee8021BridgeBaseIfToPortEntry_t *poIeee8021BridgeBaseIfToPortEntry = NULL;
	
	if ((poIeee8021BridgeBaseIfToPortEntry = ieee8021BridgeBaseIfToPortTable_getByIndex (u32IfIndex)) == NULL)
	{
		goto ieee8021BridgePhyPortInfo_getByIfIndex_cleanup;
	}
	
	poPhyPortInfo->poIfToPortEntry = poIeee8021BridgeBaseIfToPortEntry;
	
	if (poPhyPortInfo->u8Flags & ieee8021BridgePhyPortInfo_phyPortEntry_c && poIeee8021BridgeBaseIfToPortEntry->u32Port != 0 &&
		(poPhyPortInfo->poPhyPortEntry = ieee8021BridgePhyPortTable_getByIndex (poIeee8021BridgeBaseIfToPortEntry->u32Port)) == NULL)
	{
		goto ieee8021BridgePhyPortInfo_getByIfIndex_cleanup;
	}
	
ieee8021BridgePhyPortInfo_getByIfIndex_cleanup:
	
	return (poIeee8021BridgeBaseIfToPortEntry == NULL ||
			(poPhyPortInfo->u8Flags & ieee8021BridgePhyPortInfo_phyPortEntry_c &&
			 poIeee8021BridgeBaseIfToPortEntry->u32Port != 0 && poPhyPortInfo->poPhyPortEntry == NULL)) != true;
}

ieee8021BridgePhyPortEntry_t *
ieee8021BridgePhyPortTable_createExt (
	uint32_t u32Port,
	uint32_t u32IfIndex)
{
	ieee8021BridgePhyPortEntry_t *poEntry = NULL;
	
	if (u32IfIndex == 0)
	{
		return NULL;
	}
	
	poEntry = ieee8021BridgePhyPortTable_createEntry (
		u32IfIndex);
	if (poEntry == NULL)
	{
		goto ieee8021BridgePhyPortTable_createExt_cleanup;
	}
	
	poEntry->u32IfIndex = u32IfIndex;
	
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
	register ieee8021BridgeBaseIfToPortEntry_t *poIeee8021BridgeBaseIfToPortEntry = NULL;
	
	if ((poIeee8021BridgeBaseIfToPortEntry = ieee8021BridgeBaseIfToPortTable_getByIndex (poEntry->u32IfIndex)) == NULL &&
		(poIeee8021BridgeBaseIfToPortEntry = ieee8021BridgeBaseIfToPortTable_createExt (poEntry->u32IfIndex)) == NULL)
	{
		goto ieee8021BridgePhyPortTable_createHier_cleanup;
	}
	poIeee8021BridgeBaseIfToPortEntry->u32Port = poEntry->u32Port;
	
	return true;
	
	
ieee8021BridgePhyPortTable_createHier_cleanup:
	
	ieee8021BridgePhyPortTable_removeHier (poEntry);
	return false;
}

bool
ieee8021BridgePhyPortTable_removeHier (
	ieee8021BridgePhyPortEntry_t *poEntry)
{
	register ieee8021BridgeBaseIfToPortEntry_t *poIeee8021BridgeBaseIfToPortEntry = NULL;
	
	if ((poIeee8021BridgeBaseIfToPortEntry = ieee8021BridgeBaseIfToPortTable_getByIndex (poEntry->u32IfIndex)) != NULL)
	{
		ieee8021BridgeBaseIfToPortTable_removeExt (poIeee8021BridgeBaseIfToPortEntry);
	}
	
	return true;
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
		*idx1->val.integer);
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
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32ToComponentId);
				break;
			case IEEE8021BRIDGEPHYPORTTOINTERNALPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32ToInternalPort);
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

/* create a new row in the (unsorted) table */
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

ieee8021BridgeTpPortEntry_t *
ieee8021BridgeTpPortTable_createExt (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	ieee8021BridgeTpPortEntry_t *poEntry = NULL;
	
	poEntry = ieee8021BridgeTpPortTable_createEntry (
		u32ComponentId,
		u32Port);
	if (poEntry == NULL)
	{
		return NULL;
	}
	
	if (!ieee8021BridgeTpPortTable_createHier (poEntry))
	{
		ieee8021BridgeTpPortTable_removeEntry (poEntry);
		return NULL;
	}
	
	return poEntry;
}

bool
ieee8021BridgeTpPortTable_removeExt (ieee8021BridgeTpPortEntry_t *poEntry)
{
	if (!ieee8021BridgeTpPortTable_removeHier (poEntry))
	{
		return false;
	}
	ieee8021BridgeTpPortTable_removeEntry (poEntry);
	
	return true;
}

bool
ieee8021BridgeTpPortTable_createHier (
	ieee8021BridgeTpPortEntry_t *poEntry)
{
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poIeee8021BridgeBasePortEntry = NULL;
	register ieee8021BridgeDot1dPortEntry_t *poIeee8021BridgeDot1dPortEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32ComponentId)) == NULL ||
		(poIeee8021BridgeBasePortEntry = ieee8021BridgeBasePortTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port)) == NULL ||
		(poIeee8021BridgeDot1dPortEntry = ieee8021BridgeDot1dPortTable_getByIndex (poEntry->u32ComponentId, poEntry->u32Port)) == NULL ||
		poIeee8021BridgeBaseEntry->u8RowStatus != xRowStatus_active_c || poIeee8021BridgeDot1dPortEntry->u8RowStatus != xRowStatus_active_c ||
		poIeee8021BridgeBasePortEntry->u32IfIndex == 0)
	{
		goto ieee8021BridgeTpPortTable_createHier_cleanup;
	}
	
	if (poIeee8021BridgeBaseEntry->u32TpIfIndex == 0)
	{
		register ifStackEntry_t *poIfStackEntry = NULL;
		ifData_t *poIfData = NULL;
		
		if (!ifData_createReference (ifIndex_zero_c, ifType_bridge_c, true, true, true, &poIfData))
		{
			goto ieee8021BridgeTpPortTable_createHier_cleanup;
		}
		poIeee8021BridgeBaseEntry->u32TpIfIndex = poIfData->u32Index;
		
		if (poIeee8021BridgeBaseEntry->u8RowStatus == xRowStatus_active_c &&
			poIeee8021BridgeDot1dPortEntry->u8RowStatus == xRowStatus_active_c &&
			!ifAdminStatus_handler (&poIfData->oIf, xAdminStatus_up_c, false))
		{
			goto ieee8021BridgeTpPortTable_createHier_cleanup;
		}
		
		if ((poIfStackEntry = ifStackTable_createExt (poIeee8021BridgeBaseEntry->u32TpIfIndex, poIeee8021BridgeBasePortEntry->u32IfIndex)) == NULL ||
			!ifStackStatus_handler (poIfStackEntry, xRowStatus_active_c))
		{
			goto ieee8021BridgeTpPortTable_createHier_cleanup;
		}
	}
	poIeee8021BridgeBaseEntry->u32NumTpPorts++;
	
	return true;
	
	
ieee8021BridgeTpPortTable_createHier_cleanup:
	
	ieee8021BridgeTpPortTable_removeHier (poEntry);
	return false;
}

bool
ieee8021BridgeTpPortTable_removeHier (
	ieee8021BridgeTpPortEntry_t *poEntry)
{
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32ComponentId)) == NULL)
	{
		goto ieee8021BridgeTpPortTable_removeHier_cleanup;
	}
	
	if (poIeee8021BridgeBaseEntry->u32NumTpPorts > 0)
	{
		poIeee8021BridgeBaseEntry->u32NumTpPorts--;
	}
	
	if (poIeee8021BridgeBaseEntry->u32NumTpPorts == 0 && poIeee8021BridgeBaseEntry->u32TpIfIndex != 0)
	{
		if (!ifData_removeReference (poIeee8021BridgeBaseEntry->u32TpIfIndex, true, true, true))
		{
			goto ieee8021BridgeTpPortTable_removeHier_cleanup;
		}
		poIeee8021BridgeBaseEntry->u32TpIfIndex = 0;
	}
	
	return true;
	
	
ieee8021BridgeTpPortTable_removeHier_cleanup:
	
	return false;
}

bool
ieee8021BridgeTpPortTable_handler (
	uint32_t u32IfIndex, bool bEnable)
{
	register ieee8021BridgeBaseIfToPortEntry_t *poIeee8021BridgeBaseIfToPortEntry = NULL;
	register ieee8021BridgeTpPortEntry_t *poIeee8021BridgeTpPortEntry = NULL;
	
	
	if (u32IfIndex == 0)
	{
		goto ieee8021BridgeTpPortTable_handler_cleanup;
	}
	
	if ((poIeee8021BridgeBaseIfToPortEntry = ieee8021BridgeBaseIfToPortTable_getByIndex (u32IfIndex)) == NULL)
	{
		goto ieee8021BridgeTpPortTable_handler_success;
	}
	
	poIeee8021BridgeTpPortEntry = ieee8021BridgeTpPortTable_getByIndex (poIeee8021BridgeBaseIfToPortEntry->u32IndexComponentId, poIeee8021BridgeBaseIfToPortEntry->u32IndexPort);
	
	if (bEnable ^ (poIeee8021BridgeTpPortEntry == NULL))
	{
		goto ieee8021BridgeTpPortTable_handler_success;
	}
	
	
	if (bEnable)
	{
		if (ieee8021BridgeTpPortTable_createExt (poIeee8021BridgeBaseIfToPortEntry->u32IndexComponentId, poIeee8021BridgeBaseIfToPortEntry->u32IndexPort) == NULL)
		{
			goto ieee8021BridgeTpPortTable_handler_cleanup;
		}
		
		/* TODO: HAL */
	}
	else
	{
		/* TODO: HAL */
		
		if (!ieee8021BridgeTpPortTable_removeExt (poIeee8021BridgeTpPortEntry))
		{
			goto ieee8021BridgeTpPortTable_handler_cleanup;
		}
	}
	
ieee8021BridgeTpPortTable_handler_success:
	
	return true;
	
	
ieee8021BridgeTpPortTable_handler_cleanup:
	
	return false;
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

/* create a new row in the (unsorted) table */
ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortPriorityEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->i32PortRequireDropEncoding = ieee8021BridgePortRequireDropEncoding_false_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree);
	return poEntry;
}

ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortPriorityEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortPriorityEntry_t, oBTreeNode);
}

ieee8021BridgePortPriorityEntry_t *
ieee8021BridgePortPriorityTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortPriorityEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortPriorityEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgePortPriorityTable_removeEntry (ieee8021BridgePortPriorityEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
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
	poEntry->i32PortNumTrafficClasses = 8;
	poEntry->i32CodePointSelection = ieee8021BridgePortPriorityCodePointSelection_codePoint8p0d_c;
	poEntry->i32PortUseDEI = ieee8021BridgePortUseDEI_false_c;
	poEntry->i32PortServiceAccessPrioritySelection = ieee8021BridgePortServiceAccessPrioritySelection_true_c;
	
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
	register ieee8021BridgeUserPriorityRegenEntry_t *poIeee8021BridgeUserPriorityRegenEntry = NULL;
	register ieee8021BridgeTrafficClassEntry_t *poIeee8021BridgeTrafficClassEntry = NULL;
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *poIeee8021BridgePortOutboundAccessPriorityEntry = NULL;
	register ieee8021BridgePortDecodingEntry_t *poIeee8021BridgePortDecodingEntry = NULL;
	register ieee8021BridgePortEncodingEntry_t *poIeee8021BridgePortEncodingEntry = NULL;
	register ieee8021BridgeServiceAccessPriorityEntry_t *poIeee8021BridgeServiceAccessPriorityEntry = NULL;
	
	for (register uint8_t u8Priority = 0; u8Priority <= 7; u8Priority++)
	{
		if ((poIeee8021BridgeServiceAccessPriorityEntry = ieee8021BridgeServiceAccessPriorityTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort, u8Priority)) != NULL)
		{
			ieee8021BridgeServiceAccessPriorityTable_removeEntry (poIeee8021BridgeServiceAccessPriorityEntry);
		}
	}
	
	for (register uint8_t u8Pcp = 1; u8Pcp <= 4; u8Pcp++)
	{
		for (register uint8_t u8Priority = 0; u8Priority <= 7; u8Priority++)
		{
			for (register uint8_t u8Dei = 1; u8Dei <= 2; u8Dei++)
			{
				if ((poIeee8021BridgePortEncodingEntry = ieee8021BridgePortEncodingTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort, u8Pcp, u8Priority, u8Dei)) != NULL)
				{
					ieee8021BridgePortEncodingTable_removeEntry (poIeee8021BridgePortEncodingEntry);
				}
			}
		}
	}
	
	for (register uint8_t u8Pcp = 1; u8Pcp <= 4; u8Pcp++)
	{
		for (register uint8_t u8Priority = 0; u8Priority <= 7; u8Priority++)
		{
			if ((poIeee8021BridgePortDecodingEntry = ieee8021BridgePortDecodingTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort, u8Pcp, u8Priority)) != NULL)
			{
				ieee8021BridgePortDecodingTable_removeEntry (poIeee8021BridgePortDecodingEntry);
			}
		}
	}
	
	for (register uint8_t u8Priority = 0; u8Priority <= 7; u8Priority++)
	{
		if ((poIeee8021BridgePortOutboundAccessPriorityEntry = ieee8021BridgePortOutboundAccessPriorityTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort, u8Priority)) != NULL)
		{
			ieee8021BridgePortOutboundAccessPriorityTable_removeEntry (poIeee8021BridgePortOutboundAccessPriorityEntry);
		}
	}
	
	for (register uint8_t u8Priority = 0; u8Priority <= 7; u8Priority++)
	{
		if ((poIeee8021BridgeTrafficClassEntry = ieee8021BridgeTrafficClassTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort, u8Priority)) != NULL)
		{
			ieee8021BridgeTrafficClassTable_removeEntry (poIeee8021BridgeTrafficClassEntry);
		}
	}
	
	for (register uint8_t u8Priority = 0; u8Priority <= 7; u8Priority++)
	{
		if ((poIeee8021BridgeUserPriorityRegenEntry = ieee8021BridgeUserPriorityRegenTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort, u8Priority)) != NULL)
		{
			ieee8021BridgeUserPriorityRegenTable_removeEntry (poIeee8021BridgeUserPriorityRegenEntry);
		}
	}
	
	return true;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortPriorityTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePortPriorityTable_BTree);
	return ieee8021BridgePortPriorityTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortPriorityTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortPriorityEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortPriorityEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortPriorityTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePortPriorityTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortPriorityEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgePortPriorityTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
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
			table_entry = (ieee8021BridgePortPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32PortDefaultUserPriority);
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PortNumTrafficClasses);
				break;
			case IEEE8021BRIDGEPORTPRIORITYCODEPOINTSELECTION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32CodePointSelection);
				break;
			case IEEE8021BRIDGEPORTUSEDEI:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PortUseDEI);
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PortRequireDropEncoding);
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PortServiceAccessPrioritySelection);
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
			table_entry = (ieee8021BridgePortPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
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
			table_entry = (ieee8021BridgePortPriorityEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ieee8021BridgePortPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32PortDefaultUserPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32PortDefaultUserPriority, sizeof (table_entry->u32PortDefaultUserPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32PortDefaultUserPriority = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PortNumTrafficClasses))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PortNumTrafficClasses, sizeof (table_entry->i32PortNumTrafficClasses));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PortNumTrafficClasses = *request->requestvb->val.integer;
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
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PortUseDEI))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PortUseDEI, sizeof (table_entry->i32PortUseDEI));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PortUseDEI = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PortRequireDropEncoding))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PortRequireDropEncoding, sizeof (table_entry->i32PortRequireDropEncoding));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PortRequireDropEncoding = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PortServiceAccessPrioritySelection))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PortServiceAccessPrioritySelection, sizeof (table_entry->i32PortServiceAccessPrioritySelection));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PortServiceAccessPrioritySelection = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgePortPriorityEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDEFAULTUSERPRIORITY:
				memcpy (&table_entry->u32PortDefaultUserPriority, pvOldDdata, sizeof (table_entry->u32PortDefaultUserPriority));
				break;
			case IEEE8021BRIDGEPORTNUMTRAFFICCLASSES:
				memcpy (&table_entry->i32PortNumTrafficClasses, pvOldDdata, sizeof (table_entry->i32PortNumTrafficClasses));
				break;
			case IEEE8021BRIDGEPORTPRIORITYCODEPOINTSELECTION:
				memcpy (&table_entry->i32CodePointSelection, pvOldDdata, sizeof (table_entry->i32CodePointSelection));
				break;
			case IEEE8021BRIDGEPORTUSEDEI:
				memcpy (&table_entry->i32PortUseDEI, pvOldDdata, sizeof (table_entry->i32PortUseDEI));
				break;
			case IEEE8021BRIDGEPORTREQUIREDROPENCODING:
				memcpy (&table_entry->i32PortRequireDropEncoding, pvOldDdata, sizeof (table_entry->i32PortRequireDropEncoding));
				break;
			case IEEE8021BRIDGEPORTSERVICEACCESSPRIORITYSELECTION:
				memcpy (&table_entry->i32PortServiceAccessPrioritySelection, pvOldDdata, sizeof (table_entry->i32PortServiceAccessPrioritySelection));
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
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort && pEntry1->u32UserPriority < pEntry2->u32UserPriority) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort && pEntry1->u32UserPriority == pEntry2->u32UserPriority) ? 0: 1;
}

xBTree_t oIeee8021BridgeUserPriorityRegenTable_BTree = xBTree_initInline (&ieee8021BridgeUserPriorityRegenTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgeUserPriorityRegenEntry_t *
ieee8021BridgeUserPriorityRegenTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32UserPriority)
{
	register ieee8021BridgeUserPriorityRegenEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	poEntry->u32UserPriority = u32UserPriority;
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
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	poTmpEntry->u32UserPriority = u32UserPriority;
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
	poTmpEntry->u32UserPriority = u32UserPriority;
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
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeUserPriorityRegenEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32UserPriority);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeUserPriorityRegenTable_BTree);
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
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32RegenUserPriority);
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
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32RegenUserPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32RegenUserPriority, sizeof (table_entry->u32RegenUserPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32RegenUserPriority = *request->requestvb->val.integer;
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
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEREGENUSERPRIORITY:
				memcpy (&table_entry->u32RegenUserPriority, pvOldDdata, sizeof (table_entry->u32RegenUserPriority));
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
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort && pEntry1->u32Priority < pEntry2->u32Priority) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort && pEntry1->u32Priority == pEntry2->u32Priority) ? 0: 1;
}

xBTree_t oIeee8021BridgeTrafficClassTable_BTree = xBTree_initInline (&ieee8021BridgeTrafficClassTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgeTrafficClassEntry_t *
ieee8021BridgeTrafficClassTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32Priority)
{
	register ieee8021BridgeTrafficClassEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	poEntry->u32Priority = u32Priority;
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
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	poTmpEntry->u32Priority = u32Priority;
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
	poTmpEntry->u32Priority = u32Priority;
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
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeTrafficClassEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Priority);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeTrafficClassTable_BTree);
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
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Class);
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
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Class))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Class, sizeof (table_entry->i32Class));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Class = *request->requestvb->val.integer;
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
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGETRAFFICCLASS:
				memcpy (&table_entry->i32Class, pvOldDdata, sizeof (table_entry->i32Class));
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
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort && pEntry1->u32RegenUserPriority < pEntry2->u32RegenUserPriority) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort && pEntry1->u32RegenUserPriority == pEntry2->u32RegenUserPriority) ? 0: 1;
}

xBTree_t oIeee8021BridgePortOutboundAccessPriorityTable_BTree = xBTree_initInline (&ieee8021BridgePortOutboundAccessPriorityTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgePortOutboundAccessPriorityEntry_t *
ieee8021BridgePortOutboundAccessPriorityTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort,
	uint32_t u32RegenUserPriority)
{
	register ieee8021BridgePortOutboundAccessPriorityEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	poEntry->u32RegenUserPriority = u32RegenUserPriority;
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
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	poTmpEntry->u32RegenUserPriority = u32RegenUserPriority;
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
	poTmpEntry->u32RegenUserPriority = u32RegenUserPriority;
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
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortOutboundAccessPriorityEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32RegenUserPriority);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortOutboundAccessPriorityTable_BTree);
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
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTOUTBOUNDACCESSPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32Priority);
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
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow < pEntry2->i32PriorityCodePointRow) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow && pEntry1->i32PriorityCodePoint < pEntry2->i32PriorityCodePoint) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow && pEntry1->i32PriorityCodePoint == pEntry2->i32PriorityCodePoint) ? 0: 1;
}

xBTree_t oIeee8021BridgePortDecodingTable_BTree = xBTree_initInline (&ieee8021BridgePortDecodingTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgePortDecodingEntry_t *
ieee8021BridgePortDecodingTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint)
{
	register ieee8021BridgePortDecodingEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32PortNum = u32PortNum;
	poEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
	poEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
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
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
	poTmpEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
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
	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
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
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortDecodingEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32PortNum);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32PriorityCodePointRow);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32PriorityCodePoint);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortDecodingTable_BTree);
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
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32Priority);
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32DropEligible);
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
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32Priority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32Priority, sizeof (table_entry->u32Priority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32Priority = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32DropEligible))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32DropEligible, sizeof (table_entry->i32DropEligible));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32DropEligible = *request->requestvb->val.integer;
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
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTDECODINGPRIORITY:
				memcpy (&table_entry->u32Priority, pvOldDdata, sizeof (table_entry->u32Priority));
				break;
			case IEEE8021BRIDGEPORTDECODINGDROPELIGIBLE:
				memcpy (&table_entry->i32DropEligible, pvOldDdata, sizeof (table_entry->i32DropEligible));
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
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow < pEntry2->i32PriorityCodePointRow) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow && pEntry1->i32PriorityCodePoint < pEntry2->i32PriorityCodePoint) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow && pEntry1->i32PriorityCodePoint == pEntry2->i32PriorityCodePoint && pEntry1->i32DropEligible < pEntry2->i32DropEligible) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->i32PriorityCodePointRow == pEntry2->i32PriorityCodePointRow && pEntry1->i32PriorityCodePoint == pEntry2->i32PriorityCodePoint && pEntry1->i32DropEligible == pEntry2->i32DropEligible) ? 0: 1;
}

xBTree_t oIeee8021BridgePortEncodingTable_BTree = xBTree_initInline (&ieee8021BridgePortEncodingTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgePortEncodingEntry_t *
ieee8021BridgePortEncodingTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	int32_t i32PriorityCodePointRow,
	int32_t i32PriorityCodePoint,
	int32_t i32DropEligible)
{
	register ieee8021BridgePortEncodingEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32PortNum = u32PortNum;
	poEntry->i32PriorityCodePointRow = i32PriorityCodePointRow;
	poEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	poEntry->i32DropEligible = i32DropEligible;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
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
	int32_t i32DropEligible)
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
	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	poTmpEntry->i32DropEligible = i32DropEligible;
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
	int32_t i32DropEligible)
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
	poTmpEntry->i32PriorityCodePoint = i32PriorityCodePoint;
	poTmpEntry->i32DropEligible = i32DropEligible;
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
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortEncodingEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32PortNum);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32PriorityCodePointRow);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32PriorityCodePoint);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->i32DropEligible);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortEncodingTable_BTree);
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
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32Priority);
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
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32Priority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32Priority, sizeof (table_entry->u32Priority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32Priority = *request->requestvb->val.integer;
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
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTENCODINGPRIORITY:
				memcpy (&table_entry->u32Priority, pvOldDdata, sizeof (table_entry->u32Priority));
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
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum < pEntry2->u32PortNum) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->u32Received < pEntry2->u32Received) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32PortNum == pEntry2->u32PortNum && pEntry1->u32Received == pEntry2->u32Received) ? 0: 1;
}

xBTree_t oIeee8021BridgeServiceAccessPriorityTable_BTree = xBTree_initInline (&ieee8021BridgeServiceAccessPriorityTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgeServiceAccessPriorityEntry_t *
ieee8021BridgeServiceAccessPriorityTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32PortNum,
	uint32_t u32Received)
{
	register ieee8021BridgeServiceAccessPriorityEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32PortNum = u32PortNum;
	poEntry->u32Received = u32Received;
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
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32PortNum = u32PortNum;
	poTmpEntry->u32Received = u32Received;
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
	poTmpEntry->u32Received = u32Received;
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
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgeServiceAccessPriorityEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32PortNum);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Received);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgeServiceAccessPriorityTable_BTree);
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
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32Value);
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
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32Value))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32Value, sizeof (table_entry->u32Value));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32Value = *request->requestvb->val.integer;
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
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGESERVICEACCESSPRIORITYVALUE:
				memcpy (&table_entry->u32Value, pvOldDdata, sizeof (table_entry->u32Value));
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

static int8_t
ieee8021BridgePortMrpTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePortMrpEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePortMrpEntry_t, oBTreeNode);
	register ieee8021BridgePortMrpEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePortMrpEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgePortMrpTable_BTree = xBTree_initInline (&ieee8021BridgePortMrpTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgePortMrpEntry_t *
ieee8021BridgePortMrpTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMrpEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->i32JoinTime = 20;
	poEntry->i32LeaveTime = 60;
	poEntry->i32LeaveAllTime = 1000;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree);
	return poEntry;
}

ieee8021BridgePortMrpEntry_t *
ieee8021BridgePortMrpTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMrpEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortMrpEntry_t, oBTreeNode);
}

ieee8021BridgePortMrpEntry_t *
ieee8021BridgePortMrpTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMrpEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortMrpEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgePortMrpTable_removeEntry (ieee8021BridgePortMrpEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortMrpTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePortMrpTable_BTree);
	return ieee8021BridgePortMrpTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortMrpTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortMrpEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortMrpEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortMrpTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePortMrpTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortMrpEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgePortMrpTable_getByIndex (
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
			table_entry = (ieee8021BridgePortMrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
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
			table_entry = (ieee8021BridgePortMrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
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
			table_entry = (ieee8021BridgePortMrpEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ieee8021BridgePortMrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
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
			table_entry = (ieee8021BridgePortMrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
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

static int8_t
ieee8021BridgePortMmrpTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021BridgePortMmrpEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021BridgePortMmrpEntry_t, oBTreeNode);
	register ieee8021BridgePortMmrpEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021BridgePortMmrpEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32BasePortComponentId < pEntry2->u32BasePortComponentId) ||
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort < pEntry2->u32BasePort) ? -1:
		(pEntry1->u32BasePortComponentId == pEntry2->u32BasePortComponentId && pEntry1->u32BasePort == pEntry2->u32BasePort) ? 0: 1;
}

xBTree_t oIeee8021BridgePortMmrpTable_BTree = xBTree_initInline (&ieee8021BridgePortMmrpTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgePortMmrpEntry_t *
ieee8021BridgePortMmrpTable_createEntry (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMmrpEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32BasePortComponentId = u32BasePortComponentId;
	poEntry->u32BasePort = u32BasePort;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->i32EnabledStatus = ieee8021BridgePortMmrpEnabledStatus_true_c;
	poEntry->i32PortRestrictedGroupRegistration = ieee8021BridgePortRestrictedGroupRegistration_false_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree);
	return poEntry;
}

ieee8021BridgePortMmrpEntry_t *
ieee8021BridgePortMmrpTable_getByIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMmrpEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortMmrpEntry_t, oBTreeNode);
}

ieee8021BridgePortMmrpEntry_t *
ieee8021BridgePortMmrpTable_getNextIndex (
	uint32_t u32BasePortComponentId,
	uint32_t u32BasePort)
{
	register ieee8021BridgePortMmrpEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32BasePortComponentId = u32BasePortComponentId;
	poTmpEntry->u32BasePort = u32BasePort;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021BridgePortMmrpEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021BridgePortMmrpTable_removeEntry (ieee8021BridgePortMmrpEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021BridgePortMmrpTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021BridgePortMmrpTable_BTree);
	return ieee8021BridgePortMmrpTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021BridgePortMmrpTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortMmrpEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021BridgePortMmrpEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePortComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32BasePort);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021BridgePortMmrpTable_BTree);
	return put_index_data;
}

bool
ieee8021BridgePortMmrpTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021BridgePortMmrpEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021BridgePortMmrpTable_getByIndex (
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
			table_entry = (ieee8021BridgePortMmrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32EnabledStatus);
				break;
			case IEEE8021BRIDGEPORTMMRPFAILEDREGISTRATIONS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64FailedRegistrations);
				break;
			case IEEE8021BRIDGEPORTMMRPLASTPDUORIGIN:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8LastPduOrigin, table_entry->u16LastPduOrigin_len);
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PortRestrictedGroupRegistration);
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
			table_entry = (ieee8021BridgePortMmrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
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
			table_entry = (ieee8021BridgePortMmrpEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ieee8021BridgePortMmrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32EnabledStatus))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32EnabledStatus, sizeof (table_entry->i32EnabledStatus));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32EnabledStatus = *request->requestvb->val.integer;
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PortRestrictedGroupRegistration))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PortRestrictedGroupRegistration, sizeof (table_entry->i32PortRestrictedGroupRegistration));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PortRestrictedGroupRegistration = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021BridgePortMmrpEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021BRIDGEPORTMMRPENABLEDSTATUS:
				memcpy (&table_entry->i32EnabledStatus, pvOldDdata, sizeof (table_entry->i32EnabledStatus));
				break;
			case IEEE8021BRIDGEPORTRESTRICTEDGROUPREGISTRATION:
				memcpy (&table_entry->i32PortRestrictedGroupRegistration, pvOldDdata, sizeof (table_entry->i32PortRestrictedGroupRegistration));
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
		(pEntry1->u32IfIndex < pEntry2->u32IfIndex) ? -1:
		(pEntry1->u32IfIndex == pEntry2->u32IfIndex) ? 0: 1;
}

xBTree_t oIeee8021BridgeILanIfTable_BTree = xBTree_initInline (&ieee8021BridgeILanIfTable_BTreeNodeCmp);

/* create a new row in the (unsorted) table */
ieee8021BridgeILanIfEntry_t *
ieee8021BridgeILanIfTable_createEntry (
	uint32_t u32IfIndex)
{
	register ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32IfIndex = u32IfIndex;
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
	uint32_t u32IfIndex)
{
	register ieee8021BridgeILanIfEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32IfIndex = u32IfIndex;
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
	uint32_t u32IfIndex)
{
	register ieee8021BridgeILanIfEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32IfIndex = u32IfIndex;
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
ieee8021BridgeILanIfTable_createExt (
	uint32_t u32IfIndex)
{
	ieee8021BridgeILanIfEntry_t *poEntry = NULL;
	
	poEntry = ieee8021BridgeILanIfTable_createEntry (
		u32IfIndex);
	if (poEntry == NULL)
	{
		return NULL;
	}
	
	if (!ieee8021BridgeILanIfTable_createHier (poEntry))
	{
		ieee8021BridgeILanIfTable_removeEntry (poEntry);
		return NULL;
	}
	
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
	if (!ifData_createReference (poEntry->u32IfIndex, ifType_ilan_c, true, true, true, NULL))
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
	return ifData_removeReference (poEntry->u32IfIndex, true, true, true);
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
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32IfIndex);
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
				case RS_CREATEANDGO:
					if (/* TODO : int ieee8021BridgeILanIfTable_dep (...) */ TOBE_REPLACED != TOBE_REPLACED)
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
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				case RS_ACTIVE:
					table_entry->u8RowStatus = RS_ACTIVE;
					break;
					
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				case RS_NOTINSERVICE:
					table_entry->u8RowStatus = RS_NOTINSERVICE;
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

/* create a new row in the (unsorted) table */
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
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32BasePortComponentId)) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	
	if (ieee8021BridgeBasePortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL &&
		ieee8021BridgeBasePortTable_createExt (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	if (ieee8021BridgeTpPortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL &&
		ieee8021BridgeTpPortTable_createExt (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	if (ieee8021BridgePortPriorityTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL &&
		ieee8021BridgePortPriorityTable_createExt (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	if (ieee8021BridgePortMrpTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL &&
		ieee8021BridgePortMrpTable_createEntry (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	if (ieee8021BridgePortMmrpTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL &&
		ieee8021BridgePortMmrpTable_createEntry (poEntry->u32BasePortComponentId, poEntry->u32BasePort) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_createHier_cleanup;
	}
	
	poIeee8021BridgeBaseEntry->i32NumPorts++;
	return true;
	
	
ieee8021BridgeDot1dPortTable_createHier_cleanup:
	
	ieee8021BridgeDot1dPortTable_removeHier (poEntry);
	return false;
}

bool
ieee8021BridgeDot1dPortTable_removeHier (
	ieee8021BridgeDot1dPortEntry_t *poEntry)
{
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poIeee8021BridgeBasePortEntry = NULL;
	register ieee8021BridgeTpPortEntry_t *poIeee8021BridgeTpPortEntry = NULL;
	register ieee8021BridgePortPriorityEntry_t *poIeee8021BridgePortPriorityEntry = NULL;
	register ieee8021BridgePortMrpEntry_t *poIeee8021BridgePortMrpEntry = NULL;
	register ieee8021BridgePortMmrpEntry_t *poIeee8021BridgePortMmrpEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32BasePortComponentId)) == NULL)
	{
		goto ieee8021BridgeDot1dPortTable_removeHier_success;
	}
	
	
	if ((poIeee8021BridgePortMmrpEntry = ieee8021BridgePortMmrpTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) != NULL)
	{
		ieee8021BridgePortMmrpTable_removeEntry (poIeee8021BridgePortMmrpEntry);
	}
	
	if ((poIeee8021BridgePortMrpEntry = ieee8021BridgePortMrpTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) != NULL)
	{
		ieee8021BridgePortMrpTable_removeEntry (poIeee8021BridgePortMrpEntry);
	}
	
	if ((poIeee8021BridgePortPriorityEntry = ieee8021BridgePortPriorityTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) != NULL)
	{
		ieee8021BridgePortPriorityTable_removeExt (poIeee8021BridgePortPriorityEntry);
	}
	
	if ((poIeee8021BridgeTpPortEntry = ieee8021BridgeTpPortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) != NULL)
	{
		ieee8021BridgeTpPortTable_removeExt (poIeee8021BridgeTpPortEntry);
	}
	
	if ((poIeee8021BridgeBasePortEntry = ieee8021BridgeBasePortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) != NULL)
	{
		ieee8021BridgeBasePortTable_removeExt (poIeee8021BridgeBasePortEntry);
	}
	
	poIeee8021BridgeBaseEntry->i32NumPorts--;
	
ieee8021BridgeDot1dPortTable_removeHier_success:
	
	return true;
}

bool
ieee8021BridgeDot1dPortRowStatus_handler (
	ieee8021BridgeDot1dPortEntry_t *poEntry, uint8_t u8RowStatus)
{
	register ieee8021BridgeBaseEntry_t *poIeee8021BridgeBaseEntry = NULL;
	register ieee8021BridgeBasePortEntry_t *poIeee8021BridgeBasePortEntry = NULL;
	
	if ((poIeee8021BridgeBaseEntry = ieee8021BridgeBaseTable_getByIndex (poEntry->u32BasePortComponentId)) == NULL ||
		(poIeee8021BridgeBasePortEntry = ieee8021BridgeBasePortTable_getByIndex (poEntry->u32BasePortComponentId, poEntry->u32BasePort)) == NULL)
	{
		goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
	}
	
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto ieee8021BridgeDot1dPortRowStatus_handler_success;
	}
	if (u8RowStatus & xRowStatus_fromParent_c &&
		(((u8RowStatus & xRowStatus_mask_c) == xRowStatus_active_c && poEntry->u8RowStatus != xRowStatus_notReady_c) ||
		 ((u8RowStatus & xRowStatus_mask_c) == xRowStatus_notInService_c && poEntry->u8RowStatus != xRowStatus_active_c)))
	{
		goto ieee8021BridgeDot1dPortRowStatus_handler_success;
	}
	
	
	switch (u8RowStatus & xRowStatus_mask_c)
	{
	case xRowStatus_active_c:
		if (poEntry->u8RowStatus == u8RowStatus)
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_success;
		}
		
		if (!(u8RowStatus & xRowStatus_fromParent_c) && poIeee8021BridgeBaseEntry->u8RowStatus != xRowStatus_active_c)
		{
			poEntry->u8RowStatus = xRowStatus_notReady_c;
			goto ieee8021BridgeDot1dPortRowStatus_handler_success;
		}
		
		if (poIeee8021BridgeBasePortEntry->u32IfIndex == 0 ||
			!ieee8021BridgeBasePortIfIndex_handler (poIeee8021BridgeBasePortEntry))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		if (!ieee8021BridgeDot1dPortRowStatus_update (poEntry, u8RowStatus & xRowStatus_mask_c))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_active_c;
		
		if (poIeee8021BridgeBasePortEntry->pOldEntry != NULL)
		{
			xBuffer_free (poIeee8021BridgeBasePortEntry->pOldEntry);
			poIeee8021BridgeBasePortEntry->pOldEntry = NULL;
		}
		break;
		
	case xRowStatus_notInService_c:
		if (!(u8RowStatus & xRowStatus_fromParent_c) && poIeee8021BridgeBaseEntry->u8RowStatus != xRowStatus_active_c)
		{
			poEntry->u8RowStatus = u8RowStatus;
			goto ieee8021BridgeDot1dPortRowStatus_handler_success;
		}
		
		if (poIeee8021BridgeBasePortEntry->pOldEntry == NULL &&
			(poIeee8021BridgeBasePortEntry->pOldEntry = xBuffer_alloc (sizeof (*poIeee8021BridgeBasePortEntry->pOldEntry))) == NULL)
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		if (!ieee8021BridgeDot1dPortRowStatus_update (poEntry, u8RowStatus & xRowStatus_mask_c))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		memcpy (poIeee8021BridgeBasePortEntry->pOldEntry, poIeee8021BridgeBasePortEntry, sizeof (*poIeee8021BridgeBasePortEntry->pOldEntry));
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_createAndGo_c:
		goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!ieee8021BridgeDot1dPortRowStatus_update (poEntry, u8RowStatus & xRowStatus_mask_c))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		poIeee8021BridgeBasePortEntry->u32IfIndex = 0;
		if (!ieee8021BridgeBasePortIfIndex_handler (poIeee8021BridgeBasePortEntry))
		{
			goto ieee8021BridgeDot1dPortRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		if (poIeee8021BridgeBasePortEntry->pOldEntry != NULL)
		{
			xBuffer_free (poIeee8021BridgeBasePortEntry->pOldEntry);
			poIeee8021BridgeBasePortEntry->pOldEntry = NULL;
		}
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
ieee8021BridgeDot1dPortRowStatus_handler_success:
	
	return true;
	
	
ieee8021BridgeDot1dPortRowStatus_handler_cleanup:
	
	return u8RowStatus & xRowStatus_fromParent_c;
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
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				case RS_ACTIVE:
					table_entry->u8RowStatus = RS_ACTIVE;
					break;
					
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				case RS_NOTINSERVICE:
					table_entry->u8RowStatus = RS_NOTINSERVICE;
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