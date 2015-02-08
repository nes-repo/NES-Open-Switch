/*
 *  Copyright (c) 2008-2015
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
#include "ieee8021SpanningTreeMib.h"

#include "lib/binaryTree.h"
#include "lib/buffer.h"
#include "lib/snmp.h"

#define ROLLBACK_BUFFER "ROLLBACK_BUFFER"



static oid ieee8021SpanningTreeMib_oid[] = {1,3,111,2,802,1,1,3};

static oid ieee8021SpanningTreeTable_oid[] = {1,3,111,2,802,1,1,3,1,1};
static oid ieee8021SpanningTreePortTable_oid[] = {1,3,111,2,802,1,1,3,1,2};

static oid snmptrap_oid[] = {1,3,6,1,6,3,1,1,4,1,0};

static oid ieee8021SpanningTreeNewRoot_oid[] = {1,3,111,2,802,1,1,3,0,1};
static oid ieee8021SpanningTreeTopologyChange_oid[] = {1,3,111,2,802,1,1,3,0,2};



/**
 *	initialize ieee8021SpanningTreeMib group mapper
 */
void
ieee8021SpanningTreeMib_init (void)
{
	extern oid ieee8021SpanningTreeMib_oid[];
	
	DEBUGMSGTL (("ieee8021SpanningTreeMib", "Initializing\n"));
	
	
	/* register ieee8021SpanningTreeMib group table mappers */
	ieee8021SpanningTreeTable_init ();
	ieee8021SpanningTreePortTable_init ();
	
	/* register ieee8021SpanningTreeMib modules */
	sysORTable_createRegister ("ieee8021SpanningTreeMib", ieee8021SpanningTreeMib_oid, OID_LENGTH (ieee8021SpanningTreeMib_oid));
}


/**
 *	table mapper(s) & helper(s)
 */
/** initialize ieee8021SpanningTreeTable table mapper **/
void
ieee8021SpanningTreeTable_init (void)
{
	extern oid ieee8021SpanningTreeTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021SpanningTreeTable", &ieee8021SpanningTreeTable_mapper,
		ieee8021SpanningTreeTable_oid, OID_LENGTH (ieee8021SpanningTreeTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021SpanningTreeComponentId */,
		0);
	table_info->min_column = IEEE8021SPANNINGTREEPROTOCOLSPECIFICATION;
	table_info->max_column = IEEE8021SPANNINGTREERSTPTXHOLDCOUNT;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021SpanningTreeTable_getFirst;
	iinfo->get_next_data_point = &ieee8021SpanningTreeTable_getNext;
	iinfo->get_data_point = &ieee8021SpanningTreeTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021SpanningTreeTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021SpanningTreeEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021SpanningTreeEntry_t, oBTreeNode);
	register ieee8021SpanningTreeEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021SpanningTreeEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId) ? 0: 1;
}

xBTree_t oIeee8021SpanningTreeTable_BTree = xBTree_initInline (&ieee8021SpanningTreeTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021SpanningTreeEntry_t *
ieee8021SpanningTreeTable_createEntry (
	uint32_t u32ComponentId)
{
	ieee8021SpanningTreeEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (ieee8021SpanningTreeEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->i32RstpTxHoldCount = 3;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree);
	return poEntry;
}

ieee8021SpanningTreeEntry_t *
ieee8021SpanningTreeTable_getByIndex (
	uint32_t u32ComponentId)
{
	register ieee8021SpanningTreeEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (ieee8021SpanningTreeEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021SpanningTreeEntry_t, oBTreeNode);
}

ieee8021SpanningTreeEntry_t *
ieee8021SpanningTreeTable_getNextIndex (
	uint32_t u32ComponentId)
{
	register ieee8021SpanningTreeEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (ieee8021SpanningTreeEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021SpanningTreeEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021SpanningTreeTable_removeEntry (ieee8021SpanningTreeEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021SpanningTreeTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021SpanningTreeTable_BTree);
	return ieee8021SpanningTreeTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021SpanningTreeTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021SpanningTreeEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021SpanningTreeEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021SpanningTreeTable_BTree);
	return put_index_data;
}

bool
ieee8021SpanningTreeTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021SpanningTreeEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ieee8021SpanningTreeTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021SpanningTreeTable table mapper */
int
ieee8021SpanningTreeTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021SpanningTreeEntry_t *table_entry;
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
			table_entry = (ieee8021SpanningTreeEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPROTOCOLSPECIFICATION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ProtocolSpecification);
				break;
			case IEEE8021SPANNINGTREEPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Priority);
				break;
			case IEEE8021SPANNINGTREETIMESINCETOPOLOGYCHANGE:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, table_entry->u32TimeSinceTopologyChange);
				break;
			case IEEE8021SPANNINGTREETOPCHANGES:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64TopChanges);
				break;
			case IEEE8021SPANNINGTREEDESIGNATEDROOT:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8DesignatedRoot, table_entry->u16DesignatedRoot_len);
				break;
			case IEEE8021SPANNINGTREEROOTCOST:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RootCost);
				break;
			case IEEE8021SPANNINGTREEROOTPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32RootPort);
				break;
			case IEEE8021SPANNINGTREEMAXAGE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32MaxAge);
				break;
			case IEEE8021SPANNINGTREEHELLOTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32HelloTime);
				break;
			case IEEE8021SPANNINGTREEHOLDTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32HoldTime);
				break;
			case IEEE8021SPANNINGTREEFORWARDDELAY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ForwardDelay);
				break;
			case IEEE8021SPANNINGTREEBRIDGEMAXAGE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32BridgeMaxAge);
				break;
			case IEEE8021SPANNINGTREEBRIDGEHELLOTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32BridgeHelloTime);
				break;
			case IEEE8021SPANNINGTREEBRIDGEFORWARDDELAY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32BridgeForwardDelay);
				break;
			case IEEE8021SPANNINGTREEVERSION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Version);
				break;
			case IEEE8021SPANNINGTREERSTPTXHOLDCOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RstpTxHoldCount);
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
			table_entry = (ieee8021SpanningTreeEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREEBRIDGEMAXAGE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREEBRIDGEHELLOTIME:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREEBRIDGEFORWARDDELAY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREEVERSION:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREERSTPTXHOLDCOUNT:
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
			table_entry = (ieee8021SpanningTreeEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ieee8021SpanningTreeEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Priority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Priority, sizeof (table_entry->i32Priority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Priority = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREEBRIDGEMAXAGE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32BridgeMaxAge))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32BridgeMaxAge, sizeof (table_entry->i32BridgeMaxAge));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32BridgeMaxAge = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREEBRIDGEHELLOTIME:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32BridgeHelloTime))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32BridgeHelloTime, sizeof (table_entry->i32BridgeHelloTime));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32BridgeHelloTime = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREEBRIDGEFORWARDDELAY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32BridgeForwardDelay))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32BridgeForwardDelay, sizeof (table_entry->i32BridgeForwardDelay));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32BridgeForwardDelay = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREEVERSION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Version))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Version, sizeof (table_entry->i32Version));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Version = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREERSTPTXHOLDCOUNT:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32RstpTxHoldCount))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32RstpTxHoldCount, sizeof (table_entry->i32RstpTxHoldCount));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32RstpTxHoldCount = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021SpanningTreeEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPRIORITY:
				memcpy (&table_entry->i32Priority, pvOldDdata, sizeof (table_entry->i32Priority));
				break;
			case IEEE8021SPANNINGTREEBRIDGEMAXAGE:
				memcpy (&table_entry->i32BridgeMaxAge, pvOldDdata, sizeof (table_entry->i32BridgeMaxAge));
				break;
			case IEEE8021SPANNINGTREEBRIDGEHELLOTIME:
				memcpy (&table_entry->i32BridgeHelloTime, pvOldDdata, sizeof (table_entry->i32BridgeHelloTime));
				break;
			case IEEE8021SPANNINGTREEBRIDGEFORWARDDELAY:
				memcpy (&table_entry->i32BridgeForwardDelay, pvOldDdata, sizeof (table_entry->i32BridgeForwardDelay));
				break;
			case IEEE8021SPANNINGTREEVERSION:
				memcpy (&table_entry->i32Version, pvOldDdata, sizeof (table_entry->i32Version));
				break;
			case IEEE8021SPANNINGTREERSTPTXHOLDCOUNT:
				memcpy (&table_entry->i32RstpTxHoldCount, pvOldDdata, sizeof (table_entry->i32RstpTxHoldCount));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ieee8021SpanningTreePortTable table mapper **/
void
ieee8021SpanningTreePortTable_init (void)
{
	extern oid ieee8021SpanningTreePortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ieee8021SpanningTreePortTable", &ieee8021SpanningTreePortTable_mapper,
		ieee8021SpanningTreePortTable_oid, OID_LENGTH (ieee8021SpanningTreePortTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_UNSIGNED /* index: ieee8021SpanningTreePortComponentId */,
		ASN_UNSIGNED /* index: ieee8021SpanningTreePort */,
		0);
	table_info->min_column = IEEE8021SPANNINGTREEPORTPRIORITY;
	table_info->max_column = IEEE8021SPANNINGTREERSTPPORTADMINPATHCOST;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ieee8021SpanningTreePortTable_getFirst;
	iinfo->get_next_data_point = &ieee8021SpanningTreePortTable_getNext;
	iinfo->get_data_point = &ieee8021SpanningTreePortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ieee8021SpanningTreePortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ieee8021SpanningTreePortEntry_t *pEntry1 = xBTree_entry (pNode1, ieee8021SpanningTreePortEntry_t, oBTreeNode);
	register ieee8021SpanningTreePortEntry_t *pEntry2 = xBTree_entry (pNode2, ieee8021SpanningTreePortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32ComponentId < pEntry2->u32ComponentId) ||
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32Port < pEntry2->u32Port) ? -1:
		(pEntry1->u32ComponentId == pEntry2->u32ComponentId && pEntry1->u32Port == pEntry2->u32Port) ? 0: 1;
}

xBTree_t oIeee8021SpanningTreePortTable_BTree = xBTree_initInline (&ieee8021SpanningTreePortTable_BTreeNodeCmp);

/* create a new row in the table */
ieee8021SpanningTreePortEntry_t *
ieee8021SpanningTreePortTable_createEntry (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	ieee8021SpanningTreePortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (ieee8021SpanningTreePortEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32ComponentId = u32ComponentId;
	poEntry->u32Port = u32Port;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree);
	return poEntry;
}

ieee8021SpanningTreePortEntry_t *
ieee8021SpanningTreePortTable_getByIndex (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021SpanningTreePortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (ieee8021SpanningTreePortEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021SpanningTreePortEntry_t, oBTreeNode);
}

ieee8021SpanningTreePortEntry_t *
ieee8021SpanningTreePortTable_getNextIndex (
	uint32_t u32ComponentId,
	uint32_t u32Port)
{
	register ieee8021SpanningTreePortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (ieee8021SpanningTreePortEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32ComponentId = u32ComponentId;
	poTmpEntry->u32Port = u32Port;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ieee8021SpanningTreePortEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ieee8021SpanningTreePortTable_removeEntry (ieee8021SpanningTreePortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ieee8021SpanningTreePortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIeee8021SpanningTreePortTable_BTree);
	return ieee8021SpanningTreePortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ieee8021SpanningTreePortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021SpanningTreePortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ieee8021SpanningTreePortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32ComponentId);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_UNSIGNED, poEntry->u32Port);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIeee8021SpanningTreePortTable_BTree);
	return put_index_data;
}

bool
ieee8021SpanningTreePortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ieee8021SpanningTreePortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ieee8021SpanningTreePortTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ieee8021SpanningTreePortTable table mapper */
int
ieee8021SpanningTreePortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ieee8021SpanningTreePortEntry_t *table_entry;
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
			table_entry = (ieee8021SpanningTreePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPORTPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Priority);
				break;
			case IEEE8021SPANNINGTREEPORTSTATE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32State);
				break;
			case IEEE8021SPANNINGTREEPORTENABLED:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Enabled);
				break;
			case IEEE8021SPANNINGTREEPORTPATHCOST:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PathCost);
				break;
			case IEEE8021SPANNINGTREEPORTDESIGNATEDROOT:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8DesignatedRoot, table_entry->u16DesignatedRoot_len);
				break;
			case IEEE8021SPANNINGTREEPORTDESIGNATEDCOST:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32DesignatedCost);
				break;
			case IEEE8021SPANNINGTREEPORTDESIGNATEDBRIDGE:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8DesignatedBridge, table_entry->u16DesignatedBridge_len);
				break;
			case IEEE8021SPANNINGTREEPORTDESIGNATEDPORT:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8DesignatedPort, table_entry->u16DesignatedPort_len);
				break;
			case IEEE8021SPANNINGTREEPORTFORWARDTRANSITIONS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64ForwardTransitions);
				break;
			case IEEE8021SPANNINGTREERSTPPORTPROTOCOLMIGRATION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RstpPortProtocolMigration);
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINEDGEPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RstpPortAdminEdgePort);
				break;
			case IEEE8021SPANNINGTREERSTPPORTOPEREDGEPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RstpPortOperEdgePort);
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINPATHCOST:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RstpPortAdminPathCost);
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
			table_entry = (ieee8021SpanningTreePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPORTPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREEPORTENABLED:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREEPORTPATHCOST:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREERSTPPORTPROTOCOLMIGRATION:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINEDGEPORT:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINPATHCOST:
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
			table_entry = (ieee8021SpanningTreePortEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ieee8021SpanningTreePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPORTPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Priority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Priority, sizeof (table_entry->i32Priority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Priority = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREEPORTENABLED:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Enabled))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Enabled, sizeof (table_entry->i32Enabled));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Enabled = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREEPORTPATHCOST:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PathCost))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PathCost, sizeof (table_entry->i32PathCost));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PathCost = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREERSTPPORTPROTOCOLMIGRATION:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32RstpPortProtocolMigration))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32RstpPortProtocolMigration, sizeof (table_entry->i32RstpPortProtocolMigration));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32RstpPortProtocolMigration = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINEDGEPORT:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32RstpPortAdminEdgePort))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32RstpPortAdminEdgePort, sizeof (table_entry->i32RstpPortAdminEdgePort));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32RstpPortAdminEdgePort = *request->requestvb->val.integer;
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINPATHCOST:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32RstpPortAdminPathCost))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32RstpPortAdminPathCost, sizeof (table_entry->i32RstpPortAdminPathCost));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32RstpPortAdminPathCost = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ieee8021SpanningTreePortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IEEE8021SPANNINGTREEPORTPRIORITY:
				memcpy (&table_entry->i32Priority, pvOldDdata, sizeof (table_entry->i32Priority));
				break;
			case IEEE8021SPANNINGTREEPORTENABLED:
				memcpy (&table_entry->i32Enabled, pvOldDdata, sizeof (table_entry->i32Enabled));
				break;
			case IEEE8021SPANNINGTREEPORTPATHCOST:
				memcpy (&table_entry->i32PathCost, pvOldDdata, sizeof (table_entry->i32PathCost));
				break;
			case IEEE8021SPANNINGTREERSTPPORTPROTOCOLMIGRATION:
				memcpy (&table_entry->i32RstpPortProtocolMigration, pvOldDdata, sizeof (table_entry->i32RstpPortProtocolMigration));
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINEDGEPORT:
				memcpy (&table_entry->i32RstpPortAdminEdgePort, pvOldDdata, sizeof (table_entry->i32RstpPortAdminEdgePort));
				break;
			case IEEE8021SPANNINGTREERSTPPORTADMINPATHCOST:
				memcpy (&table_entry->i32RstpPortAdminPathCost, pvOldDdata, sizeof (table_entry->i32RstpPortAdminPathCost));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}


/**
 *	notification mapper(s)
 */
int
ieee8021SpanningTreeNewRoot_trap (void)
{
	extern oid snmptrap_oid[];
	extern oid ieee8021SpanningTreeNewRoot_oid[];
	netsnmp_variable_list *var_list = NULL;
	
	/*
	 * Set the snmpTrapOid.0 value
	 */
	snmp_varlist_add_variable (&var_list,
		snmptrap_oid, OID_LENGTH (snmptrap_oid),
		ASN_OBJECT_ID,
		(const u_char*) ieee8021SpanningTreeNewRoot_oid, sizeof (ieee8021SpanningTreeNewRoot_oid));
		
		
	/*
	 * Add any extra (optional) objects here
	 */
	
	/*
	 * Send the trap to the list of configured destinations
	 *  and clean up
	 */
	send_v2trap (var_list);
	snmp_free_varbind (var_list);
	
	return SNMP_ERR_NOERROR;
}

int
ieee8021SpanningTreeTopologyChange_trap (void)
{
	extern oid snmptrap_oid[];
	extern oid ieee8021SpanningTreeTopologyChange_oid[];
	netsnmp_variable_list *var_list = NULL;
	
	/*
	 * Set the snmpTrapOid.0 value
	 */
	snmp_varlist_add_variable (&var_list,
		snmptrap_oid, OID_LENGTH (snmptrap_oid),
		ASN_OBJECT_ID,
		(const u_char*) ieee8021SpanningTreeTopologyChange_oid, sizeof (ieee8021SpanningTreeTopologyChange_oid));
		
		
	/*
	 * Add any extra (optional) objects here
	 */
	
	/*
	 * Send the trap to the list of configured destinations
	 *  and clean up
	 */
	send_v2trap (var_list);
	snmp_free_varbind (var_list);
	
	return SNMP_ERR_NOERROR;
}
