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
#include "lagMIB.h"
#include "lagUtils.h"
#include "if/ifMIB.h"

#include "system_ext.h"

#include "lib/ieee802.h"
#include "lib/bitmap.h"
#include "lib/binaryTree.h"
#include "lib/buffer.h"
#include "lib/snmp.h"
#include "lib/time.h"

#include <stdbool.h>
#include <stdint.h>

#define ROLLBACK_BUFFER "ROLLBACK_BUFFER"



static oid lagMIB_oid[] = {1,2,840,10006,300,43};
static oid neLagMIB_oid[] = {1,3,6,1,4,1,36969,73};

static oid lagMIBObjects_oid[] = {1,2,840,10006,300,43,1};

static oid dot3adAggTable_oid[] = {1,2,840,10006,300,43,1,1,1};
static oid dot3adAggPortListTable_oid[] = {1,2,840,10006,300,43,1,1,2};
static oid dot3adAggPortTable_oid[] = {1,2,840,10006,300,43,1,2,1};
static oid dot3adAggPortStatsTable_oid[] = {1,2,840,10006,300,43,1,2,2};
static oid dot3adAggPortDebugTable_oid[] = {1,2,840,10006,300,43,1,2,3};
static oid dot3adAggPortXTable_oid[] = {1,2,840,10006,300,43,1,2,4};
static oid neAggTable_oid[] = {1,3,6,1,4,1,36969,73,1,1};
static oid neAggPortListTable_oid[] = {1,3,6,1,4,1,36969,73,1,2};
static oid neAggPortTable_oid[] = {1,3,6,1,4,1,36969,73,1,3};



/**
 *	initialize lagMIB group mapper
 */
void
lagMIB_init (void)
{
	extern oid lagMIB_oid[];
	extern oid neLagMIB_oid[];
	extern oid lagMIBObjects_oid[];
	
	DEBUGMSGTL (("lagMIB", "Initializing\n"));
	
	/* register lagMIBObjects scalar mapper */
	netsnmp_register_scalar_group (
		netsnmp_create_handler_registration (
			"lagMIBObjects_mapper", &lagMIBObjects_mapper,
			lagMIBObjects_oid, OID_LENGTH (lagMIBObjects_oid),
			HANDLER_CAN_RONLY
		),
		DOT3ADTABLESLASTCHANGED,
		DOT3ADTABLESLASTCHANGED
	);
	
	
	/* register lagMIB group table mappers */
	dot3adAggTable_init ();
	dot3adAggPortListTable_init ();
	dot3adAggPortTable_init ();
	dot3adAggPortStatsTable_init ();
	dot3adAggPortDebugTable_init ();
	dot3adAggPortXTable_init ();
	neAggTable_init ();
	neAggPortListTable_init ();
	neAggPortTable_init ();
	
	/* register lagMIB modules */
	sysORTable_createRegister ("lagMIB", lagMIB_oid, OID_LENGTH (lagMIB_oid));
	sysORTable_createRegister ("neLagMIB", neLagMIB_oid, OID_LENGTH (neLagMIB_oid));
}


/**
 *	scalar mapper(s)
 */
lagMIBObjects_t oLagMIBObjects;

/** lagMIBObjects scalar mapper **/
int
lagMIBObjects_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	extern oid lagMIBObjects_oid[];
	netsnmp_request_info *request;
	/* We are never called for a GETNEXT if it's registered as a
	   "group instance", as it's "magically" handled for us. */
	
	switch (reqinfo->mode)
	{
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			switch (request->requestvb->name[OID_LENGTH (lagMIBObjects_oid)])
			{
			case DOT3ADTABLESLASTCHANGED:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, (uint32_t) (xTime_centiTime (xTime_typeMono_c) - oLagMIBObjects.u32Dot3adTablesLastChanged));
				break;
				
			default:
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				continue;
			}
		}
		break;
		
		
	default:
		/* we should never get here, so this is a really bad error */
		snmp_log (LOG_ERR, "unknown mode (%d) in handle_\n", reqinfo->mode);
		return SNMP_ERR_GENERR;
	}
	
	return SNMP_ERR_NOERROR;
}


/**
 *	table mapper(s) & helper(s)
 */
/** initialize dot3adAggTable table mapper **/
void
dot3adAggTable_init (void)
{
	extern oid dot3adAggTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"dot3adAggTable", &dot3adAggTable_mapper,
		dot3adAggTable_oid, OID_LENGTH (dot3adAggTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggIndex */,
		0);
	table_info->min_column = DOT3ADAGGMACADDRESS;
	table_info->max_column = DOT3ADAGGCOLLECTORMAXDELAY;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &dot3adAggTable_getFirst;
	iinfo->get_next_data_point = &dot3adAggTable_getNext;
	iinfo->get_data_point = &dot3adAggTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
dot3adAggTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register dot3adAggEntry_t *pEntry1 = xBTree_entry (pNode1, dot3adAggEntry_t, oBTreeNode);
	register dot3adAggEntry_t *pEntry2 = xBTree_entry (pNode2, dot3adAggEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

static int8_t
dot3adAggTable_Group_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register dot3adAggEntry_t *pEntry1 = xBTree_entry (pNode1, dot3adAggEntry_t, oBTreeNode);
	register dot3adAggEntry_t *pEntry2 = xBTree_entry (pNode2, dot3adAggEntry_t, oBTreeNode);
	
	return
		(pEntry1->oK.i32GroupType < pEntry2->oK.i32GroupType) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex < pEntry2->oK.u32GroupIndex) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

static int8_t
dot3adAggTable_Key_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register dot3adAggEntry_t *pEntry1 = xBTree_entry (pNode1, dot3adAggEntry_t, oBTreeNode);
	register dot3adAggEntry_t *pEntry2 = xBTree_entry (pNode2, dot3adAggEntry_t, oBTreeNode);
	
	return
		(pEntry1->oK.i32GroupType < pEntry2->oK.i32GroupType) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex < pEntry2->oK.u32GroupIndex) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->oK.i32ActorAdminKey < pEntry2->oK.i32ActorAdminKey) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->oK.i32ActorAdminKey == pEntry2->oK.i32ActorAdminKey && pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->oK.i32ActorAdminKey == pEntry2->oK.i32ActorAdminKey && pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

xBTree_t oDot3adAggTable_BTree = xBTree_initInline (&dot3adAggTable_BTreeNodeCmp);
xBTree_t oDot3adAggTable_Group_BTree = xBTree_initInline (&dot3adAggTable_Group_BTreeNodeCmp);
xBTree_t oDot3adAggTable_Key_BTree = xBTree_initInline (&dot3adAggTable_Key_BTreeNodeCmp);

/* create a new row in the table */
dot3adAggEntry_t *
dot3adAggTable_createEntry (
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Index = u32Index;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oDot3adAggTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oDot3adAggTable_BTree);
	return poEntry;
}

dot3adAggEntry_t *
dot3adAggTable_getByIndex (
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oDot3adAggTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggEntry_t, oBTreeNode);
}

dot3adAggEntry_t *
dot3adAggTable_Group_getByIndex (
	int32_t i32GroupType,
	uint32_t u32GroupIndex,
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->oK.i32GroupType = i32GroupType;
	poTmpEntry->oK.u32GroupIndex = u32GroupIndex;
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oGroup_BTreeNode, &oDot3adAggTable_Group_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggEntry_t, oBTreeNode);
}

dot3adAggEntry_t *
dot3adAggTable_getNextIndex (
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oDot3adAggTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggEntry_t, oBTreeNode);
}

dot3adAggEntry_t *
dot3adAggTable_Group_getNextIndex (
	int32_t i32GroupType,
	uint32_t u32GroupIndex,
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->oK.i32GroupType = i32GroupType;
	poTmpEntry->oK.u32GroupIndex = u32GroupIndex;
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oGroup_BTreeNode, &oDot3adAggTable_Group_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
dot3adAggTable_removeEntry (dot3adAggEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oDot3adAggTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oGroup_BTreeNode, &oDot3adAggTable_Group_BTree);
	xBTree_nodeRemove (&poEntry->oKey_BTreeNode, &oDot3adAggTable_Key_BTree);
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oDot3adAggTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

dot3adAggEntry_t *
dot3adAggTable_createExt (
	uint32_t u32Index)
{
	dot3adAggEntry_t *poEntry = NULL;
	
	poEntry = dot3adAggTable_createEntry (
		u32Index);
	if (poEntry == NULL)
	{
		goto dot3adAggTable_createExt_cleanup;
	}
	
	if (!dot3adAggTable_createHier (poEntry))
	{
		dot3adAggTable_removeEntry (poEntry);
		poEntry = NULL;
		goto dot3adAggTable_createExt_cleanup;
	}
	
	oLagMIBObjects.u32Dot3adTablesLastChanged = xTime_centiTime (xTime_typeMono_c);
	
dot3adAggTable_createExt_cleanup:
	
	return poEntry;
}

bool
dot3adAggTable_removeExt (dot3adAggEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!dot3adAggTable_removeHier (poEntry))
	{
		goto dot3adAggTable_removeExt_cleanup;
	}
	dot3adAggTable_removeEntry (poEntry);
	bRetCode = true;
	
	oLagMIBObjects.u32Dot3adTablesLastChanged = xTime_centiTime (xTime_typeMono_c);
	
dot3adAggTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
dot3adAggTable_createHier (
	dot3adAggEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ifTable_createReference (poEntry->u32Index, ifType_ieee8023adLag_c, 0, true, true, true, NULL))
	{
		goto dot3adAggTable_createHier_cleanup;
	}
	
	if (dot3adAggPortListTable_createEntry (poEntry->u32Index) == NULL)
	{
		goto dot3adAggTable_createHier_cleanup;
	}
	
	bRetCode = true;
	
dot3adAggTable_createHier_cleanup:
	
	!bRetCode ? dot3adAggTable_removeHier (poEntry): false;
	return bRetCode;
}

bool
dot3adAggTable_removeHier (
	dot3adAggEntry_t *poEntry)
{
	register bool bRetCode = false;
	register dot3adAggPortListEntry_t *poDot3adAggPortListEntry = NULL;
	
	if ((poDot3adAggPortListEntry = dot3adAggPortListTable_getByIndex (poEntry->u32Index)) != NULL)
	{
		dot3adAggPortListTable_removeEntry (poDot3adAggPortListEntry);
	}
	
	if (!ifTable_removeReference (poEntry->u32Index, true, true, true))
	{
		goto dot3adAggTable_removeHier_cleanup;
	}
	
	bRetCode = true;
	
dot3adAggTable_removeHier_cleanup:
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
dot3adAggTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggTable_BTree);
	return dot3adAggTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
dot3adAggTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggTable_BTree);
	return put_index_data;
}

bool
dot3adAggTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* dot3adAggTable table mapper */
int
dot3adAggTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	dot3adAggEntry_t *table_entry;
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
			table_entry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGMACADDRESS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8MACAddress, sizeof (table_entry->au8MACAddress));
				break;
			case DOT3ADAGGACTORSYSTEMPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorSystemPriority);
				break;
			case DOT3ADAGGACTORSYSTEMID:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8ActorSystemID, sizeof (table_entry->au8ActorSystemID));
				break;
			case DOT3ADAGGAGGREGATEORINDIVIDUAL:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8AggregateOrIndividual);
				break;
			case DOT3ADAGGACTORADMINKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorAdminKey);
				break;
			case DOT3ADAGGACTOROPERKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorOperKey);
				break;
			case DOT3ADAGGPARTNERSYSTEMID:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PartnerSystemID, sizeof (table_entry->au8PartnerSystemID));
				break;
			case DOT3ADAGGPARTNERSYSTEMPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerSystemPriority);
				break;
			case DOT3ADAGGPARTNEROPERKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerOperKey);
				break;
			case DOT3ADAGGCOLLECTORMAXDELAY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32CollectorMaxDelay);
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
			table_entry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGACTORSYSTEMPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGACTORADMINKEY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGCOLLECTORMAXDELAY:
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
			table_entry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGACTORSYSTEMPRIORITY:
			case DOT3ADAGGACTORADMINKEY:
			case DOT3ADAGGCOLLECTORMAXDELAY:
				if (table_entry->oNe.u8RowStatus == xRowStatus_active_c || table_entry->oNe.u8RowStatus == xRowStatus_notReady_c)
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
			table_entry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGACTORSYSTEMPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ActorSystemPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ActorSystemPriority, sizeof (table_entry->i32ActorSystemPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ActorSystemPriority = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGACTORADMINKEY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ActorAdminKey))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ActorAdminKey, sizeof (table_entry->i32ActorAdminKey));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ActorAdminKey = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGCOLLECTORMAXDELAY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32CollectorMaxDelay))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32CollectorMaxDelay, sizeof (table_entry->i32CollectorMaxDelay));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32CollectorMaxDelay = *request->requestvb->val.integer;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGACTORSYSTEMPRIORITY:
				memcpy (&table_entry->i32ActorSystemPriority, pvOldDdata, sizeof (table_entry->i32ActorSystemPriority));
				break;
			case DOT3ADAGGACTORADMINKEY:
				memcpy (&table_entry->i32ActorAdminKey, pvOldDdata, sizeof (table_entry->i32ActorAdminKey));
				break;
			case DOT3ADAGGCOLLECTORMAXDELAY:
				memcpy (&table_entry->i32CollectorMaxDelay, pvOldDdata, sizeof (table_entry->i32CollectorMaxDelay));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize dot3adAggPortListTable table mapper **/
void
dot3adAggPortListTable_init (void)
{
	extern oid dot3adAggPortListTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"dot3adAggPortListTable", &dot3adAggPortListTable_mapper,
		dot3adAggPortListTable_oid, OID_LENGTH (dot3adAggPortListTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggIndex */,
		0);
	table_info->min_column = DOT3ADAGGPORTLISTPORTS;
	table_info->max_column = DOT3ADAGGPORTLISTPORTS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &dot3adAggPortListTable_getFirst;
	iinfo->get_next_data_point = &dot3adAggPortListTable_getNext;
	iinfo->get_data_point = &dot3adAggPortListTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
dot3adAggPortListEntry_t *
dot3adAggPortListTable_createEntry (
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poAgg = NULL;
	
	if ((poAgg = dot3adAggTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAgg->oPortList;
}

dot3adAggPortListEntry_t *
dot3adAggPortListTable_getByIndex (
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poAgg = NULL;
	
	if ((poAgg = dot3adAggTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAgg->oPortList;
}

dot3adAggPortListEntry_t *
dot3adAggPortListTable_getNextIndex (
	uint32_t u32Index)
{
	register dot3adAggEntry_t *poAgg = NULL;
	
	if ((poAgg = dot3adAggTable_getNextIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAgg->oPortList;
}

/* remove a row from the table */
void
dot3adAggPortListTable_removeEntry (dot3adAggPortListEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
dot3adAggPortListTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggTable_BTree);
	return dot3adAggPortListTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
dot3adAggPortListTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggTable_BTree);
	return put_index_data;
}

bool
dot3adAggPortListTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* dot3adAggPortListTable table mapper */
int
dot3adAggPortListTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	dot3adAggPortListEntry_t *table_entry;
	register dot3adAggEntry_t *poEntry = NULL;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oPortList;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTLISTPORTS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Ports, table_entry->u16Ports_len);
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

/** initialize dot3adAggPortTable table mapper **/
void
dot3adAggPortTable_init (void)
{
	extern oid dot3adAggPortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"dot3adAggPortTable", &dot3adAggPortTable_mapper,
		dot3adAggPortTable_oid, OID_LENGTH (dot3adAggPortTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggPortIndex */,
		0);
	table_info->min_column = DOT3ADAGGPORTACTORSYSTEMPRIORITY;
	table_info->max_column = DOT3ADAGGPORTAGGREGATEORINDIVIDUAL;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &dot3adAggPortTable_getFirst;
	iinfo->get_next_data_point = &dot3adAggPortTable_getNext;
	iinfo->get_data_point = &dot3adAggPortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
dot3adAggPortTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register dot3adAggPortEntry_t *pEntry1 = xBTree_entry (pNode1, dot3adAggPortEntry_t, oBTreeNode);
	register dot3adAggPortEntry_t *pEntry2 = xBTree_entry (pNode2, dot3adAggPortEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

static int8_t
dot3adAggPortTable_Group_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register dot3adAggPortEntry_t *pEntry1 = xBTree_entry (pNode1, dot3adAggPortEntry_t, oGroup_BTreeNode);
	register dot3adAggPortEntry_t *pEntry2 = xBTree_entry (pNode2, dot3adAggPortEntry_t, oGroup_BTreeNode);
	
	return
		(pEntry1->oK.i32GroupType < pEntry2->oK.i32GroupType) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex < pEntry2->oK.u32GroupIndex) ||
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->oK.i32GroupType == pEntry2->oK.i32GroupType && pEntry1->oK.u32GroupIndex == pEntry2->oK.u32GroupIndex && pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

xBTree_t oDot3adAggPortTable_BTree = xBTree_initInline (&dot3adAggPortTable_BTreeNodeCmp);
xBTree_t oDot3adAggPortTable_Group_BTree = xBTree_initInline (&dot3adAggPortTable_Group_BTreeNodeCmp);

/* create a new row in the table */
dot3adAggPortEntry_t *
dot3adAggPortTable_createEntry (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Index = u32Index;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->u8OperStatus = xOperStatus_notPresent_c;
	poEntry->u8Selection = dot3adAggPortSelection_none_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	return poEntry;
}

dot3adAggPortEntry_t *
dot3adAggPortTable_getByIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oDot3adAggPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggPortEntry_t, oBTreeNode);
}

dot3adAggPortEntry_t *
dot3adAggPortTable_Group_getByIndex (
	int32_t i32GroupType,
	uint32_t u32GroupIndex,
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->oK.i32GroupType = i32GroupType;
	poTmpEntry->oK.u32GroupIndex = u32GroupIndex;
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oGroup_BTreeNode, &oDot3adAggPortTable_Group_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggPortEntry_t, oGroup_BTreeNode);
}

dot3adAggPortEntry_t *
dot3adAggPortTable_getNextIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oDot3adAggPortTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggPortEntry_t, oBTreeNode);
}

dot3adAggPortEntry_t *
dot3adAggPortTable_Group_getNextIndex (
	int32_t i32GroupType,
	uint32_t u32GroupIndex,
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->oK.i32GroupType = i32GroupType;
	poTmpEntry->oK.u32GroupIndex = u32GroupIndex;
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oGroup_BTreeNode, &oDot3adAggPortTable_Group_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, dot3adAggPortEntry_t, oGroup_BTreeNode);
}

/* remove a row from the table */
void
dot3adAggPortTable_removeEntry (dot3adAggPortEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oGroup_BTreeNode, &oDot3adAggPortTable_Group_BTree);
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

dot3adAggPortEntry_t *
dot3adAggPortTable_createExt (
	uint32_t u32Index)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	
	poEntry = dot3adAggPortTable_createEntry (
		u32Index);
	if (poEntry == NULL)
	{
		goto dot3adAggPortTable_createExt_cleanup;
	}
	
	if (!dot3adAggPortTable_createHier (poEntry))
	{
		dot3adAggPortTable_removeEntry (poEntry);
		poEntry = NULL;
		goto dot3adAggPortTable_createExt_cleanup;
	}
	
	oLagMIBObjects.u32Dot3adTablesLastChanged = xTime_centiTime (xTime_typeMono_c);
	
dot3adAggPortTable_createExt_cleanup:
	
	return poEntry;
}

bool
dot3adAggPortTable_removeExt (dot3adAggPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!dot3adAggPortTable_removeHier (poEntry))
	{
		goto dot3adAggPortTable_removeExt_cleanup;
	}
	dot3adAggPortTable_removeEntry (poEntry);
	bRetCode = true;
	
	oLagMIBObjects.u32Dot3adTablesLastChanged = xTime_centiTime (xTime_typeMono_c);
	
dot3adAggPortTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
dot3adAggPortTable_createHier (
	dot3adAggPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ifTable_createReference (poEntry->u32Index, 0, 0, false, true, false, NULL))
	{
		goto dot3adAggPortTable_createHier_cleanup;
	}
	
	if (dot3adAggPortStatsTable_createEntry (poEntry->u32Index) == NULL)
	{
		goto dot3adAggPortTable_createHier_cleanup;
	}
	
	if (dot3adAggPortDebugTable_createEntry (poEntry->u32Index) == NULL)
	{
		goto dot3adAggPortTable_createHier_cleanup;
	}
	
	if (dot3adAggPortXTable_createEntry (poEntry->u32Index) == NULL)
	{
		goto dot3adAggPortTable_createHier_cleanup;
	}
	
	bRetCode = true;
	
dot3adAggPortTable_createHier_cleanup:
	
	!bRetCode ? dot3adAggPortTable_removeHier (poEntry): false;
	return bRetCode;
}

bool
dot3adAggPortTable_removeHier (
	dot3adAggPortEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	{
		register dot3adAggPortXEntry_t *poDot3adAggPortXEntry = NULL;
		
		if ((poDot3adAggPortXEntry = dot3adAggPortXTable_getByIndex (poEntry->u32Index)) != NULL)
		{
			dot3adAggPortXTable_removeEntry (poDot3adAggPortXEntry);
		}
	}
	
	{
		register dot3adAggPortDebugEntry_t *poDot3adAggPortDebugEntry = NULL;
		
		if ((poDot3adAggPortDebugEntry = dot3adAggPortDebugTable_getByIndex (poEntry->u32Index)) != NULL)
		{
			dot3adAggPortDebugTable_removeEntry (poDot3adAggPortDebugEntry);
		}
	}
	
	{
		register dot3adAggPortStatsEntry_t *poDot3adAggPortStatsEntry = NULL;
		
		if ((poDot3adAggPortStatsEntry = dot3adAggPortStatsTable_getByIndex (poEntry->u32Index)) != NULL)
		{
			dot3adAggPortStatsTable_removeEntry (poDot3adAggPortStatsEntry);
		}
	}
	
	if (!ifTable_removeReference (poEntry->u32Index, false, true, false))
	{
		goto dot3adAggPortTable_removeHier_cleanup;
	}
	
	bRetCode = true;
	
dot3adAggPortTable_removeHier_cleanup:
	
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
dot3adAggPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggPortTable_BTree);
	return dot3adAggPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
dot3adAggPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	return put_index_data;
}

bool
dot3adAggPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggPortTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* dot3adAggPortTable table mapper */
int
dot3adAggPortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	dot3adAggPortEntry_t *table_entry;
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
			table_entry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTACTORSYSTEMPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorSystemPriority);
				break;
			case DOT3ADAGGPORTACTORSYSTEMID:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8ActorSystemID, sizeof (table_entry->au8ActorSystemID));
				break;
			case DOT3ADAGGPORTACTORADMINKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorAdminKey);
				break;
			case DOT3ADAGGPORTACTOROPERKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorOperKey);
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerAdminSystemPriority);
				break;
			case DOT3ADAGGPORTPARTNEROPERSYSTEMPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerOperSystemPriority);
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMID:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PartnerAdminSystemID, sizeof (table_entry->au8PartnerAdminSystemID));
				break;
			case DOT3ADAGGPORTPARTNEROPERSYSTEMID:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PartnerOperSystemID, sizeof (table_entry->au8PartnerOperSystemID));
				break;
			case DOT3ADAGGPORTPARTNERADMINKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerAdminKey);
				break;
			case DOT3ADAGGPORTPARTNEROPERKEY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerOperKey);
				break;
			case DOT3ADAGGPORTSELECTEDAGGID:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u32SelectedAggID);
				break;
			case DOT3ADAGGPORTATTACHEDAGGID:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u32AttachedAggID);
				break;
			case DOT3ADAGGPORTACTORPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorPort);
				break;
			case DOT3ADAGGPORTACTORPORTPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorPortPriority);
				break;
			case DOT3ADAGGPORTPARTNERADMINPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerAdminPort);
				break;
			case DOT3ADAGGPORTPARTNEROPERPORT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerOperPort);
				break;
			case DOT3ADAGGPORTPARTNERADMINPORTPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerAdminPortPriority);
				break;
			case DOT3ADAGGPORTPARTNEROPERPORTPRIORITY:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerOperPortPriority);
				break;
			case DOT3ADAGGPORTACTORADMINSTATE:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8ActorAdminState, sizeof (table_entry->au8ActorAdminState));
				break;
			case DOT3ADAGGPORTACTOROPERSTATE:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8ActorOperState, sizeof (table_entry->au8ActorOperState));
				break;
			case DOT3ADAGGPORTPARTNERADMINSTATE:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PartnerAdminState, sizeof (table_entry->au8PartnerAdminState));
				break;
			case DOT3ADAGGPORTPARTNEROPERSTATE:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PartnerOperState, sizeof (table_entry->au8PartnerOperState));
				break;
			case DOT3ADAGGPORTAGGREGATEORINDIVIDUAL:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8AggregateOrIndividual);
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
			table_entry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTACTORSYSTEMPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTACTORADMINKEY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTACTOROPERKEY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMID:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8PartnerAdminSystemID));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTPARTNERADMINKEY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTACTORPORTPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTPARTNERADMINPORT:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTPARTNERADMINPORTPRIORITY:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTACTORADMINSTATE:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8ActorAdminState));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case DOT3ADAGGPORTPARTNERADMINSTATE:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8PartnerAdminState));
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
			table_entry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTACTORSYSTEMPRIORITY:
			case DOT3ADAGGPORTACTORADMINKEY:
			case DOT3ADAGGPORTACTOROPERKEY:
			case DOT3ADAGGPORTPARTNERADMINSYSTEMPRIORITY:
			case DOT3ADAGGPORTPARTNERADMINSYSTEMID:
			case DOT3ADAGGPORTPARTNERADMINKEY:
			case DOT3ADAGGPORTACTORPORTPRIORITY:
			case DOT3ADAGGPORTPARTNERADMINPORT:
			case DOT3ADAGGPORTPARTNERADMINPORTPRIORITY:
			case DOT3ADAGGPORTACTORADMINSTATE:
			case DOT3ADAGGPORTPARTNERADMINSTATE:
				if (table_entry->oNe.u8RowStatus == xRowStatus_active_c || table_entry->oNe.u8RowStatus == xRowStatus_notReady_c)
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
			table_entry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTACTORSYSTEMPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ActorSystemPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ActorSystemPriority, sizeof (table_entry->i32ActorSystemPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ActorSystemPriority = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTACTORADMINKEY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ActorAdminKey))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ActorAdminKey, sizeof (table_entry->i32ActorAdminKey));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ActorAdminKey = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTACTOROPERKEY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ActorOperKey))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ActorOperKey, sizeof (table_entry->i32ActorOperKey));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ActorOperKey = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PartnerAdminSystemPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PartnerAdminSystemPriority, sizeof (table_entry->i32PartnerAdminSystemPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PartnerAdminSystemPriority = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMID:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8PartnerAdminSystemID))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, table_entry->au8PartnerAdminSystemID, sizeof (table_entry->au8PartnerAdminSystemID));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8PartnerAdminSystemID, 0, sizeof (table_entry->au8PartnerAdminSystemID));
				memcpy (table_entry->au8PartnerAdminSystemID, request->requestvb->val.string, request->requestvb->val_len);
				break;
			case DOT3ADAGGPORTPARTNERADMINKEY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PartnerAdminKey))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PartnerAdminKey, sizeof (table_entry->i32PartnerAdminKey));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PartnerAdminKey = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTACTORPORTPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32ActorPortPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32ActorPortPriority, sizeof (table_entry->i32ActorPortPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32ActorPortPriority = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTPARTNERADMINPORT:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PartnerAdminPort))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PartnerAdminPort, sizeof (table_entry->i32PartnerAdminPort));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PartnerAdminPort = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTPARTNERADMINPORTPRIORITY:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PartnerAdminPortPriority))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PartnerAdminPortPriority, sizeof (table_entry->i32PartnerAdminPortPriority));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PartnerAdminPortPriority = *request->requestvb->val.integer;
				break;
			case DOT3ADAGGPORTACTORADMINSTATE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8ActorAdminState))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, table_entry->au8ActorAdminState, sizeof (table_entry->au8ActorAdminState));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8ActorAdminState, 0, sizeof (table_entry->au8ActorAdminState));
				memcpy (table_entry->au8ActorAdminState, request->requestvb->val.string, request->requestvb->val_len);
				break;
			case DOT3ADAGGPORTPARTNERADMINSTATE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8PartnerAdminState))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, table_entry->au8PartnerAdminState, sizeof (table_entry->au8PartnerAdminState));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8PartnerAdminState, 0, sizeof (table_entry->au8PartnerAdminState));
				memcpy (table_entry->au8PartnerAdminState, request->requestvb->val.string, request->requestvb->val_len);
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTACTORSYSTEMPRIORITY:
				memcpy (&table_entry->i32ActorSystemPriority, pvOldDdata, sizeof (table_entry->i32ActorSystemPriority));
				break;
			case DOT3ADAGGPORTACTORADMINKEY:
				memcpy (&table_entry->i32ActorAdminKey, pvOldDdata, sizeof (table_entry->i32ActorAdminKey));
				break;
			case DOT3ADAGGPORTACTOROPERKEY:
				memcpy (&table_entry->i32ActorOperKey, pvOldDdata, sizeof (table_entry->i32ActorOperKey));
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMPRIORITY:
				memcpy (&table_entry->i32PartnerAdminSystemPriority, pvOldDdata, sizeof (table_entry->i32PartnerAdminSystemPriority));
				break;
			case DOT3ADAGGPORTPARTNERADMINSYSTEMID:
				memcpy (table_entry->au8PartnerAdminSystemID, pvOldDdata, sizeof (table_entry->au8PartnerAdminSystemID));
				break;
			case DOT3ADAGGPORTPARTNERADMINKEY:
				memcpy (&table_entry->i32PartnerAdminKey, pvOldDdata, sizeof (table_entry->i32PartnerAdminKey));
				break;
			case DOT3ADAGGPORTACTORPORTPRIORITY:
				memcpy (&table_entry->i32ActorPortPriority, pvOldDdata, sizeof (table_entry->i32ActorPortPriority));
				break;
			case DOT3ADAGGPORTPARTNERADMINPORT:
				memcpy (&table_entry->i32PartnerAdminPort, pvOldDdata, sizeof (table_entry->i32PartnerAdminPort));
				break;
			case DOT3ADAGGPORTPARTNERADMINPORTPRIORITY:
				memcpy (&table_entry->i32PartnerAdminPortPriority, pvOldDdata, sizeof (table_entry->i32PartnerAdminPortPriority));
				break;
			case DOT3ADAGGPORTACTORADMINSTATE:
				memcpy (table_entry->au8ActorAdminState, pvOldDdata, sizeof (table_entry->au8ActorAdminState));
				break;
			case DOT3ADAGGPORTPARTNERADMINSTATE:
				memcpy (table_entry->au8PartnerAdminState, pvOldDdata, sizeof (table_entry->au8PartnerAdminState));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize dot3adAggPortStatsTable table mapper **/
void
dot3adAggPortStatsTable_init (void)
{
	extern oid dot3adAggPortStatsTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"dot3adAggPortStatsTable", &dot3adAggPortStatsTable_mapper,
		dot3adAggPortStatsTable_oid, OID_LENGTH (dot3adAggPortStatsTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggPortIndex */,
		0);
	table_info->min_column = DOT3ADAGGPORTSTATSLACPDUSRX;
	table_info->max_column = DOT3ADAGGPORTSTATSMARKERRESPONSEPDUSTX;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &dot3adAggPortStatsTable_getFirst;
	iinfo->get_next_data_point = &dot3adAggPortStatsTable_getNext;
	iinfo->get_data_point = &dot3adAggPortStatsTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
dot3adAggPortStatsEntry_t *
dot3adAggPortStatsTable_createEntry (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oStats;
}

dot3adAggPortStatsEntry_t *
dot3adAggPortStatsTable_getByIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oStats;
}

dot3adAggPortStatsEntry_t *
dot3adAggPortStatsTable_getNextIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getNextIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oStats;
}

/* remove a row from the table */
void
dot3adAggPortStatsTable_removeEntry (dot3adAggPortStatsEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
dot3adAggPortStatsTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggPortTable_BTree);
	return dot3adAggPortStatsTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
dot3adAggPortStatsTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	return put_index_data;
}

bool
dot3adAggPortStatsTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggPortTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* dot3adAggPortStatsTable table mapper */
int
dot3adAggPortStatsTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	dot3adAggPortStatsEntry_t *table_entry;
	register dot3adAggPortEntry_t *poEntry = NULL;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oStats;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTSTATSLACPDUSRX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32LACPDUsRx);
				break;
			case DOT3ADAGGPORTSTATSMARKERPDUSRX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32MarkerPDUsRx);
				break;
			case DOT3ADAGGPORTSTATSMARKERRESPONSEPDUSRX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32MarkerResponsePDUsRx);
				break;
			case DOT3ADAGGPORTSTATSUNKNOWNRX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32UnknownRx);
				break;
			case DOT3ADAGGPORTSTATSILLEGALRX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32IllegalRx);
				break;
			case DOT3ADAGGPORTSTATSLACPDUSTX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32LACPDUsTx);
				break;
			case DOT3ADAGGPORTSTATSMARKERPDUSTX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32MarkerPDUsTx);
				break;
			case DOT3ADAGGPORTSTATSMARKERRESPONSEPDUSTX:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32MarkerResponsePDUsTx);
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

/** initialize dot3adAggPortDebugTable table mapper **/
void
dot3adAggPortDebugTable_init (void)
{
	extern oid dot3adAggPortDebugTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"dot3adAggPortDebugTable", &dot3adAggPortDebugTable_mapper,
		dot3adAggPortDebugTable_oid, OID_LENGTH (dot3adAggPortDebugTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggPortIndex */,
		0);
	table_info->min_column = DOT3ADAGGPORTDEBUGRXSTATE;
	table_info->max_column = DOT3ADAGGPORTDEBUGPARTNERCHANGECOUNT;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &dot3adAggPortDebugTable_getFirst;
	iinfo->get_next_data_point = &dot3adAggPortDebugTable_getNext;
	iinfo->get_data_point = &dot3adAggPortDebugTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
dot3adAggPortDebugEntry_t *
dot3adAggPortDebugTable_createEntry (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oDebug;
}

dot3adAggPortDebugEntry_t *
dot3adAggPortDebugTable_getByIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oDebug;
}

dot3adAggPortDebugEntry_t *
dot3adAggPortDebugTable_getNextIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getNextIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oDebug;
}

/* remove a row from the table */
void
dot3adAggPortDebugTable_removeEntry (dot3adAggPortDebugEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
dot3adAggPortDebugTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggPortTable_BTree);
	return dot3adAggPortDebugTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
dot3adAggPortDebugTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	return put_index_data;
}

bool
dot3adAggPortDebugTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggPortTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* dot3adAggPortDebugTable table mapper */
int
dot3adAggPortDebugTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	dot3adAggPortDebugEntry_t *table_entry;
	register dot3adAggPortEntry_t *poEntry = NULL;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oDebug;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTDEBUGRXSTATE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32RxState);
				break;
			case DOT3ADAGGPORTDEBUGLASTRXTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, table_entry->u32LastRxTime);
				break;
			case DOT3ADAGGPORTDEBUGMUXSTATE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32MuxState);
				break;
			case DOT3ADAGGPORTDEBUGMUXREASON:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8MuxReason, table_entry->u16MuxReason_len);
				break;
			case DOT3ADAGGPORTDEBUGACTORCHURNSTATE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ActorChurnState);
				break;
			case DOT3ADAGGPORTDEBUGPARTNERCHURNSTATE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PartnerChurnState);
				break;
			case DOT3ADAGGPORTDEBUGACTORCHURNCOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32ActorChurnCount);
				break;
			case DOT3ADAGGPORTDEBUGPARTNERCHURNCOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32PartnerChurnCount);
				break;
			case DOT3ADAGGPORTDEBUGACTORSYNCTRANSITIONCOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32ActorSyncTransitionCount);
				break;
			case DOT3ADAGGPORTDEBUGPARTNERSYNCTRANSITIONCOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32PartnerSyncTransitionCount);
				break;
			case DOT3ADAGGPORTDEBUGACTORCHANGECOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32ActorChangeCount);
				break;
			case DOT3ADAGGPORTDEBUGPARTNERCHANGECOUNT:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32PartnerChangeCount);
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

/** initialize dot3adAggPortXTable table mapper **/
void
dot3adAggPortXTable_init (void)
{
	extern oid dot3adAggPortXTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"dot3adAggPortXTable", &dot3adAggPortXTable_mapper,
		dot3adAggPortXTable_oid, OID_LENGTH (dot3adAggPortXTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggPortIndex */,
		0);
	table_info->min_column = DOT3ADAGGPORTPROTOCOLDA;
	table_info->max_column = DOT3ADAGGPORTPROTOCOLDA;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &dot3adAggPortXTable_getFirst;
	iinfo->get_next_data_point = &dot3adAggPortXTable_getNext;
	iinfo->get_data_point = &dot3adAggPortXTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
dot3adAggPortXEntry_t *
dot3adAggPortXTable_createEntry (
	uint32_t u32Index)
{
	register dot3adAggPortXEntry_t *poEntry = NULL;
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	poEntry = &poAggPort->oX;
	
	/*poEntry->au8ProtocolDA = 1652522221570*/;
	memcpy (poEntry->au8ProtocolDA, IeeeEui_slowProtocolsMulticast, sizeof (poEntry->au8ProtocolDA));
	
	return poEntry;
}

dot3adAggPortXEntry_t *
dot3adAggPortXTable_getByIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oX;
}

dot3adAggPortXEntry_t *
dot3adAggPortXTable_getNextIndex (
	uint32_t u32Index)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getNextIndex (u32Index)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oX;
}

/* remove a row from the table */
void
dot3adAggPortXTable_removeEntry (dot3adAggPortXEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
dot3adAggPortXTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggPortTable_BTree);
	return dot3adAggPortXTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
dot3adAggPortXTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	return put_index_data;
}

bool
dot3adAggPortXTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggPortTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* dot3adAggPortXTable table mapper */
int
dot3adAggPortXTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	dot3adAggPortXEntry_t *table_entry;
	register dot3adAggPortEntry_t *poEntry = NULL;
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTPROTOCOLDA:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8ProtocolDA, sizeof (table_entry->au8ProtocolDA));
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTPROTOCOLDA:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8ProtocolDA));
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oX;
			
			register dot3adAggPortEntry_t *poAggPort = dot3adAggPortTable_getByPortXEntry (table_entry);
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTPROTOCOLDA:
				if (poAggPort->oNe.u8RowStatus == xRowStatus_active_c || poAggPort->oNe.u8RowStatus == xRowStatus_notReady_c)
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTPROTOCOLDA:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8ProtocolDA))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, table_entry->au8ProtocolDA, sizeof (table_entry->au8ProtocolDA));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8ProtocolDA, 0, sizeof (table_entry->au8ProtocolDA));
				memcpy (table_entry->au8ProtocolDA, request->requestvb->val.string, request->requestvb->val_len);
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case DOT3ADAGGPORTPROTOCOLDA:
				memcpy (table_entry->au8ProtocolDA, pvOldDdata, sizeof (table_entry->au8ProtocolDA));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize neAggTable table mapper **/
void
neAggTable_init (void)
{
	extern oid neAggTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"neAggTable", &neAggTable_mapper,
		neAggTable_oid, OID_LENGTH (neAggTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggIndex */,
		0);
	table_info->min_column = NEAGGGROUPTYPE;
	table_info->max_column = NEAGGSTORAGETYPE;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &neAggTable_getFirst;
	iinfo->get_next_data_point = &neAggTable_getNext;
	iinfo->get_data_point = &neAggTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
neAggEntry_t *
neAggTable_createEntry (
	uint32_t u32Dot3adAggIndex)
{
	register neAggEntry_t *poEntry = NULL;
	register dot3adAggEntry_t *poAgg = NULL;
	
	if ((poAgg = dot3adAggTable_createExt (u32Dot3adAggIndex)) == NULL)
	{
		return NULL;
	}
	poEntry = &poAgg->oNe;
	
	poEntry->i32GroupType = neAggGroupType_internal_c;
	poEntry->u32GroupIndex = 0;
	/*poEntry->au8BandwidthMax = 0*/;
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	poEntry->u8StorageType = neAggStorageType_nonVolatile_c;
	
	return poEntry;
}

neAggEntry_t *
neAggTable_getByIndex (
	uint32_t u32Dot3adAggIndex)
{
	register dot3adAggEntry_t *poAgg = NULL;
	
	if ((poAgg = dot3adAggTable_getByIndex (u32Dot3adAggIndex)) == NULL)
	{
		return NULL;
	}
	
	return &poAgg->oNe;
}

neAggEntry_t *
neAggTable_getNextIndex (
	uint32_t u32Dot3adAggIndex)
{
	register dot3adAggEntry_t *poAgg = NULL;
	
	if ((poAgg = dot3adAggTable_getNextIndex (u32Dot3adAggIndex)) == NULL)
	{
		return NULL;
	}
	
	return &poAgg->oNe;
}

/* remove a row from the table */
void
neAggTable_removeEntry (neAggEntry_t *poEntry)
{
	if (poEntry == NULL)
	{
		return;
	}
	
	dot3adAggTable_removeExt (dot3adAggTable_getByNeEntry (poEntry));
	return;
}

bool
neAggRowStatus_handler (
	neAggEntry_t *poEntry, uint8_t u8RowStatus)
{
	register dot3adAggEntry_t *poAgg = dot3adAggTable_getByNeEntry (poEntry);
	
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto neAggRowStatus_handler_success;
	}
	
	switch (u8RowStatus)
	{
	case xRowStatus_active_c:
		if (poEntry->i32GroupType == neAggGroupType_internal_c && poEntry->u32GroupIndex == 0)
		{
			poEntry->u32GroupIndex = poAgg->u32Index;
		}
		if (poAgg->i32ActorAdminKey == 0)
		{
			poAgg->i32ActorAdminKey = (poEntry->u32GroupIndex & 0xFFFF) ^ (poEntry->u32GroupIndex >> 16) ^ poEntry->i32GroupType;
		}
		
		poAgg->oK.i32GroupType = poEntry->i32GroupType;
		poAgg->oK.u32GroupIndex = poEntry->u32GroupIndex;
		poAgg->oK.i32ActorAdminKey = poAgg->i32ActorAdminKey;
		
		xBTree_nodeAdd (&poAgg->oGroup_BTreeNode, &oDot3adAggTable_Group_BTree);
		xBTree_nodeAdd (&poAgg->oKey_BTreeNode, &oDot3adAggTable_Key_BTree);
		
		/* TODO */
		
		if (!neAggRowStatus_update (poAgg, u8RowStatus))
		{
			goto neAggRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_active_c;
		break;
		
	case xRowStatus_notInService_c:
		if (!neAggRowStatus_update (poAgg, u8RowStatus))
		{
			goto neAggRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_createAndGo_c:
		goto neAggRowStatus_handler_cleanup;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!neAggRowStatus_update (poAgg, u8RowStatus))
		{
			goto neAggRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
neAggRowStatus_handler_success:
	
	return true;
	
	
neAggRowStatus_handler_cleanup:
	
	return false;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
neAggTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggTable_BTree);
	return neAggTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
neAggTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggTable_BTree);
	return put_index_data;
}

bool
neAggTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = dot3adAggTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* neAggTable table mapper */
int
neAggTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	neAggEntry_t *table_entry;
	register dot3adAggEntry_t *poEntry = NULL;
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
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGGROUPTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32GroupType);
				break;
			case NEAGGGROUPINDEX:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32GroupIndex);
				break;
			case NEAGGBANDWIDTHMAX:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8BandwidthMax, sizeof (table_entry->au8BandwidthMax));
				break;
			case NEAGGROWSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RowStatus);
				break;
			case NEAGGSTORAGETYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8StorageType);
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
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGGROUPTYPE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGGROUPINDEX:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGBANDWIDTHMAX:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8BandwidthMax));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGROWSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGSTORAGETYPE:
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
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = neAggTable_createEntry (
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
			case NEAGGGROUPTYPE:
			case NEAGGGROUPINDEX:
			case NEAGGBANDWIDTHMAX:
			case NEAGGSTORAGETYPE:
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
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					neAggTable_removeEntry (table_entry);
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
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGGROUPTYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32GroupType))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32GroupType, sizeof (table_entry->i32GroupType));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32GroupType = *request->requestvb->val.integer;
				break;
			case NEAGGGROUPINDEX:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32GroupIndex))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32GroupIndex, sizeof (table_entry->u32GroupIndex));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32GroupIndex = *request->requestvb->val.integer;
				break;
			case NEAGGBANDWIDTHMAX:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8BandwidthMax))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, table_entry->au8BandwidthMax, sizeof (table_entry->au8BandwidthMax));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8BandwidthMax, 0, sizeof (table_entry->au8BandwidthMax));
				memcpy (table_entry->au8BandwidthMax, request->requestvb->val.string, request->requestvb->val_len);
				break;
			case NEAGGSTORAGETYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8StorageType))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8StorageType, sizeof (table_entry->u8StorageType));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8StorageType = *request->requestvb->val.integer;
				break;
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!neAggRowStatus_handler (table_entry, *request->requestvb->val.integer))
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
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGGROUPTYPE:
				memcpy (&table_entry->i32GroupType, pvOldDdata, sizeof (table_entry->i32GroupType));
				break;
			case NEAGGGROUPINDEX:
				memcpy (&table_entry->u32GroupIndex, pvOldDdata, sizeof (table_entry->u32GroupIndex));
				break;
			case NEAGGBANDWIDTHMAX:
				memcpy (table_entry->au8BandwidthMax, pvOldDdata, sizeof (table_entry->au8BandwidthMax));
				break;
			case NEAGGROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					neAggTable_removeEntry (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			case NEAGGSTORAGETYPE:
				memcpy (&table_entry->u8StorageType, pvOldDdata, sizeof (table_entry->u8StorageType));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					neAggTable_removeEntry (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize neAggPortListTable table mapper **/
void
neAggPortListTable_init (void)
{
	extern oid neAggPortListTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"neAggPortListTable", &neAggPortListTable_mapper,
		neAggPortListTable_oid, OID_LENGTH (neAggPortListTable_oid),
		HANDLER_CAN_RONLY
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggIndex */,
		ASN_INTEGER /* index: dot3adAggPortIndex */,
		0);
	table_info->min_column = NEAGGPORTSELECTION;
	table_info->max_column = NEAGGPORTSELECTION;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &neAggPortListTable_getFirst;
	iinfo->get_next_data_point = &neAggPortListTable_getNext;
	iinfo->get_data_point = &neAggPortListTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
neAggPortListTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register neAggPortListEntry_t *pEntry1 = xBTree_entry (pNode1, neAggPortListEntry_t, oBTreeNode);
	register neAggPortListEntry_t *pEntry2 = xBTree_entry (pNode2, neAggPortListEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32Dot3adAggIndex < pEntry2->u32Dot3adAggIndex) ||
		(pEntry1->u32Dot3adAggIndex == pEntry2->u32Dot3adAggIndex && pEntry1->u32Dot3adAggPortIndex < pEntry2->u32Dot3adAggPortIndex) ? -1:
		(pEntry1->u32Dot3adAggIndex == pEntry2->u32Dot3adAggIndex && pEntry1->u32Dot3adAggPortIndex == pEntry2->u32Dot3adAggPortIndex) ? 0: 1;
}

xBTree_t oNeAggPortListTable_BTree = xBTree_initInline (&neAggPortListTable_BTreeNodeCmp);

/* create a new row in the table */
neAggPortListEntry_t *
neAggPortListTable_createEntry (
	uint32_t u32Dot3adAggIndex,
	uint32_t u32Dot3adAggPortIndex)
{
	register neAggPortListEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Dot3adAggIndex = u32Dot3adAggIndex;
	poEntry->u32Dot3adAggPortIndex = u32Dot3adAggPortIndex;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oNeAggPortListTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oNeAggPortListTable_BTree);
	return poEntry;
}

neAggPortListEntry_t *
neAggPortListTable_getByIndex (
	uint32_t u32Dot3adAggIndex,
	uint32_t u32Dot3adAggPortIndex)
{
	register neAggPortListEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Dot3adAggIndex = u32Dot3adAggIndex;
	poTmpEntry->u32Dot3adAggPortIndex = u32Dot3adAggPortIndex;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oNeAggPortListTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, neAggPortListEntry_t, oBTreeNode);
}

neAggPortListEntry_t *
neAggPortListTable_getNextIndex (
	uint32_t u32Dot3adAggIndex,
	uint32_t u32Dot3adAggPortIndex)
{
	register neAggPortListEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Dot3adAggIndex = u32Dot3adAggIndex;
	poTmpEntry->u32Dot3adAggPortIndex = u32Dot3adAggPortIndex;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oNeAggPortListTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, neAggPortListEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
neAggPortListTable_removeEntry (neAggPortListEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oNeAggPortListTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oNeAggPortListTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
neAggPortListTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oNeAggPortListTable_BTree);
	return neAggPortListTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
neAggPortListTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	neAggPortListEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, neAggPortListEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Dot3adAggIndex);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Dot3adAggPortIndex);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oNeAggPortListTable_BTree);
	return put_index_data;
}

bool
neAggPortListTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	neAggPortListEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = neAggPortListTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* neAggPortListTable table mapper */
int
neAggPortListTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	neAggPortListEntry_t *table_entry;
	
	switch (reqinfo->mode)
	{
	/*
	 * Read-support (also covers GetNext requests)
	 */
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (neAggPortListEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case NEAGGPORTSELECTION:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Selection);
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

/** initialize neAggPortTable table mapper **/
void
neAggPortTable_init (void)
{
	extern oid neAggPortTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"neAggPortTable", &neAggPortTable_mapper,
		neAggPortTable_oid, OID_LENGTH (neAggPortTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: dot3adAggPortIndex */,
		0);
	table_info->min_column = NEAGGPORTGROUPTYPE;
	table_info->max_column = NEAGGPORTSTORAGETYPE;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &neAggPortTable_getFirst;
	iinfo->get_next_data_point = &neAggPortTable_getNext;
	iinfo->get_data_point = &neAggPortTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
neAggPortEntry_t *
neAggPortTable_createEntry (
	uint32_t u32Dot3adAggPortIndex)
{
	register neAggPortEntry_t *poEntry = NULL;
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_createExt (u32Dot3adAggPortIndex)) == NULL)
	{
		return NULL;
	}
	poEntry = &poAggPort->oNe;
	
	poEntry->i32GroupType = neAggPortGroupType_internal_c;
	poEntry->u32GroupIndex = 0;
	xBitmap_setBitsRev (poEntry->au8Flags, 2, 1, neAggPortFlags_lacp_c, neAggPortFlags_lacpActive_c);
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	poEntry->u8StorageType = neAggPortStorageType_nonVolatile_c;
	
	return poEntry;
}

neAggPortEntry_t *
neAggPortTable_getByIndex (
	uint32_t u32Dot3adAggPortIndex)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getByIndex (u32Dot3adAggPortIndex)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oNe;
}

neAggPortEntry_t *
neAggPortTable_getNextIndex (
	uint32_t u32Dot3adAggPortIndex)
{
	register dot3adAggPortEntry_t *poAggPort = NULL;
	
	if ((poAggPort = dot3adAggPortTable_getNextIndex (u32Dot3adAggPortIndex)) == NULL)
	{
		return NULL;
	}
	
	return &poAggPort->oNe;
}

/* remove a row from the table */
void
neAggPortTable_removeEntry (neAggPortEntry_t *poEntry)
{
	if (poEntry == NULL)
	{
		return;
	}
	
	dot3adAggPortTable_removeExt (dot3adAggPortTable_getByNeEntry (poEntry));
	return;
}

bool
neAggPortRowStatus_handler (
	neAggPortEntry_t *poEntry, uint8_t u8RowStatus)
{
	register dot3adAggPortEntry_t *poAggPort = dot3adAggPortTable_getByNeEntry (poEntry);
	
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto neAggPortRowStatus_handler_success;
	}
	
	switch (u8RowStatus)
	{
	case xRowStatus_active_c:
		/* TODO */
		
		if (!neAggPortRowStatus_update (poAggPort, u8RowStatus))
		{
			goto neAggPortRowStatus_handler_cleanup;
		}
		
		poEntry->u8RowStatus = xRowStatus_active_c;
		break;
		
	case xRowStatus_notInService_c:
		if (!neAggPortRowStatus_update (poAggPort, u8RowStatus))
		{
			goto neAggPortRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_createAndGo_c:
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_active_c;
		break;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!neAggPortRowStatus_update (poAggPort, u8RowStatus))
		{
			goto neAggPortRowStatus_handler_cleanup;
		}
		
		/* TODO */
		
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
neAggPortRowStatus_handler_success:
	
	return true;
	
	
neAggPortRowStatus_handler_cleanup:
	
	return false;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
neAggPortTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oDot3adAggPortTable_BTree);
	return neAggPortTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
neAggPortTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	dot3adAggPortEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, dot3adAggPortEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oDot3adAggPortTable_BTree);
	return put_index_data;
}

bool
neAggPortTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	neAggPortEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = neAggPortTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* neAggPortTable table mapper */
int
neAggPortTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	neAggPortEntry_t *table_entry;
	register dot3adAggPortEntry_t *poEntry = NULL;
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTGROUPTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32GroupType);
				break;
			case NEAGGPORTGROUPINDEX:
				snmp_set_var_typed_integer (request->requestvb, ASN_UNSIGNED, table_entry->u32GroupIndex);
				break;
			case NEAGGPORTFLAGS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Flags, sizeof (table_entry->au8Flags));
				break;
			case NEAGGPORTROWSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RowStatus);
				break;
			case NEAGGPORTSTORAGETYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8StorageType);
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTGROUPTYPE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGPORTGROUPINDEX:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_UNSIGNED);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGPORTFLAGS:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8Flags));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGPORTROWSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEAGGPORTSTORAGETYPE:
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = neAggPortTable_createEntry (
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
			case NEAGGPORTGROUPTYPE:
			case NEAGGPORTGROUPINDEX:
			case NEAGGPORTFLAGS:
			case NEAGGPORTSTORAGETYPE:
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					neAggPortTable_removeEntry (table_entry);
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTGROUPTYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32GroupType))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32GroupType, sizeof (table_entry->i32GroupType));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32GroupType = *request->requestvb->val.integer;
				break;
			case NEAGGPORTGROUPINDEX:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u32GroupIndex))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u32GroupIndex, sizeof (table_entry->u32GroupIndex));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u32GroupIndex = *request->requestvb->val.integer;
				break;
			case NEAGGPORTFLAGS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->au8Flags))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, table_entry->au8Flags, sizeof (table_entry->au8Flags));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8Flags, 0, sizeof (table_entry->au8Flags));
				memcpy (table_entry->au8Flags, request->requestvb->val.string, request->requestvb->val_len);
				break;
			case NEAGGPORTSTORAGETYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->u8StorageType))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->u8StorageType, sizeof (table_entry->u8StorageType));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->u8StorageType = *request->requestvb->val.integer;
				break;
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!neAggPortRowStatus_handler (table_entry, *request->requestvb->val.integer))
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
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTGROUPTYPE:
				memcpy (&table_entry->i32GroupType, pvOldDdata, sizeof (table_entry->i32GroupType));
				break;
			case NEAGGPORTGROUPINDEX:
				memcpy (&table_entry->u32GroupIndex, pvOldDdata, sizeof (table_entry->u32GroupIndex));
				break;
			case NEAGGPORTFLAGS:
				memcpy (table_entry->au8Flags, pvOldDdata, sizeof (table_entry->au8Flags));
				break;
			case NEAGGPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					neAggPortTable_removeEntry (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			case NEAGGPORTSTORAGETYPE:
				memcpy (&table_entry->u8StorageType, pvOldDdata, sizeof (table_entry->u8StorageType));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			poEntry = (dot3adAggPortEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oNe;
			
			switch (table_info->colnum)
			{
			case NEAGGPORTROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					neAggPortTable_removeEntry (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}
