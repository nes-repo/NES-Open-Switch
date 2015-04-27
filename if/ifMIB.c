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
#include "ifUtils.h"
#include "ifMIB.h"

#include "system_ext.h"

#include "lib/number.h"
#include "lib/bitmap.h"
#include "lib/binaryTree.h"
#include "lib/buffer.h"
#include "lib/sync.h"
#include "lib/snmp.h"

#include <stdbool.h>

#define ROLLBACK_BUFFER "ROLLBACK_BUFFER"



static oid interfaces_oid[] = {1,3,6,1,2,1,2};
static oid ifMIB_oid[] = {1,3,6,1,2,1,31};
static oid neIfMIB_oid[] = {1,3,6,1,4,1,36969,61};

static oid ifMIBObjects_oid[] = {1,3,6,1,2,1,31,1};

static oid ifTable_oid[] = {1,3,6,1,2,1,2,2};
static oid ifXTable_oid[] = {1,3,6,1,2,1,31,1,1};
static oid ifStackTable_oid[] = {1,3,6,1,2,1,31,1,2};
static oid ifRcvAddressTable_oid[] = {1,3,6,1,2,1,31,1,4};
static oid neIfTable_oid[] = {1,3,6,1,4,1,36969,61,1,1};

static oid snmptrap_oid[] = {1,3,6,1,6,3,1,1,4,1,0};

static oid linkDown_oid[] = {1,3,6,1,6,3,1,1,5,3};
static oid linkUp_oid[] = {1,3,6,1,6,3,1,1,5,4};



/**
 *	initialize ifMIB group mapper
 */
void
ifMIB_init (void)
{
	extern oid interfaces_oid[];
	extern oid ifMIB_oid[];
	extern oid neIfMIB_oid[];
	extern oid ifMIBObjects_oid[];
	
	DEBUGMSGTL (("ifMIB", "Initializing\n"));
	
	/* register interfaces scalar mapper */
	netsnmp_register_scalar_group (
		netsnmp_create_handler_registration (
			"interfaces_mapper", &interfaces_mapper,
			interfaces_oid, OID_LENGTH (interfaces_oid),
			HANDLER_CAN_RONLY
		),
		IFNUMBER,
		IFNUMBER
	);
	
	/* register ifMIBObjects scalar mapper */
	netsnmp_register_scalar_group (
		netsnmp_create_handler_registration (
			"ifMIBObjects_mapper", &ifMIBObjects_mapper,
			ifMIBObjects_oid, OID_LENGTH (ifMIBObjects_oid),
			HANDLER_CAN_RONLY
		),
		IFTABLELASTCHANGE,
		IFSTACKLASTCHANGE
	);
	
	
	/* register ifMIB group table mappers */
	ifTable_init ();
	ifXTable_init ();
	ifStackTable_init ();
	ifRcvAddressTable_init ();
	neIfTable_init ();
	
	/* register ifMIB modules */
	sysORTable_createRegister ("interfaces", interfaces_oid, OID_LENGTH (interfaces_oid));
	sysORTable_createRegister ("ifMIB", ifMIB_oid, OID_LENGTH (ifMIB_oid));
	sysORTable_createRegister ("neIfMIB", neIfMIB_oid, OID_LENGTH (neIfMIB_oid));
}


/**
 *	scalar mapper(s)
 */
interfaces_t oInterfaces =
{
	.oIfLock = xRwLock_initInline (),
};

/** interfaces scalar mapper **/
int
interfaces_mapper (netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	extern oid interfaces_oid[];
	netsnmp_request_info *request;
	/* We are never called for a GETNEXT if it's registered as a
	   "group instance", as it's "magically" handled for us. */
	
	switch (reqinfo->mode)
	{
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			switch (request->requestvb->name[OID_LENGTH (interfaces_oid) - 1])
			{
			case IFNUMBER:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, oInterfaces.i32IfNumber);
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

ifMIBObjects_t oIfMIBObjects;

/** ifMIBObjects scalar mapper **/
int
ifMIBObjects_mapper (netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info   *reqinfo,
	netsnmp_request_info         *requests)
{
	extern oid ifMIBObjects_oid[];
	netsnmp_request_info *request;
	/* We are never called for a GETNEXT if it's registered as a
	   "group instance", as it's "magically" handled for us. */
	
	switch (reqinfo->mode)
	{
	case MODE_GET:
		for (request = requests; request != NULL; request = request->next)
		{
			switch (request->requestvb->name[OID_LENGTH (ifMIBObjects_oid) - 1])
			{
			case IFTABLELASTCHANGE:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, oIfMIBObjects.u32TableLastChange);
				break;
			case IFSTACKLASTCHANGE:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, oIfMIBObjects.u32StackLastChange);
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
static int8_t
ifData_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ifData_t *pEntry1 = xBTree_entry (pNode1, ifData_t, oBTreeNode);
	register ifData_t *pEntry2 = xBTree_entry (pNode2, ifData_t, oBTreeNode);
	
	return
		(pEntry1->u32Index < pEntry2->u32Index) ? -1:
		(pEntry1->u32Index == pEntry2->u32Index) ? 0: 1;
}

static xBTree_t oIfData_BTree = xBTree_initInline (&ifData_BTreeNodeCmp);

ifData_t *
ifData_createEntry (
	uint32_t u32Index)
{
	register ifData_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Index = u32Index;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIfData_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	xRwLock_init (&poEntry->oLock, NULL);
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIfData_BTree);
	return poEntry;
}

ifData_t *
ifData_getByIndex (
	uint32_t u32Index)
{
	register ifData_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIfData_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifData_t, oBTreeNode);
}

ifData_t *
ifData_getNextIndex (
	uint32_t u32Index)
{
	register ifData_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIfData_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifData_t, oBTreeNode);
}

void
ifData_removeEntry (ifData_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIfData_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIfData_BTree);
	xRwLock_destroy (&poEntry->oLock);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

bool
ifData_getByIndexExt (
	uint32_t u32Index, bool bWrLock,
	ifData_t **ppoIfData)
{
	register ifData_t *poIfData = NULL;
	
	ifTable_rdLock ();
	
	if ((poIfData = ifData_getByIndex (u32Index)) == NULL)
	{
		goto ifData_getByIndexExt_cleanup;
	}
	
	if (ppoIfData != NULL)
	{
		bWrLock ? ifData_wrLock (poIfData): ifData_rdLock (poIfData);
		*ppoIfData = poIfData;
	}
	
ifData_getByIndexExt_cleanup:
	
	ifTable_unLock ();
	return poIfData != NULL;
}

bool
ifData_createReference (
	uint32_t u32IfIndex,
	int32_t i32Type,
	int32_t i32AdminStatus,
	bool bCreate, bool bReference, bool bActivate,
	ifData_t **ppoIfData)
{
	register ifData_t *poIfData = NULL;
	
	if (u32IfIndex == ifIndex_zero_c &&
		(i32Type == 0 || !bCreate || ppoIfData == NULL))
	{
		return false;
	}
	
	
	bCreate ? ifTable_wrLock (): ifTable_rdLock ();
	
	if (u32IfIndex != ifIndex_zero_c && (poIfData = ifData_getByIndex (u32IfIndex)) != NULL)
	{
		if (i32Type != 0 && poIfData->oIf.i32Type != 0 && poIfData->oIf.i32Type != i32Type)
		{
			goto ifData_createReference_ifUnlock;
		}
	}
	else if (bCreate)
	{
		register neIfEntry_t *poNeIfEntry = NULL;
		
		if ((poNeIfEntry = neIfTable_createExt (u32IfIndex)) == NULL)
		{
			goto ifData_createReference_ifUnlock;
		}
		
		poIfData = ifData_getByNeEntry (poNeIfEntry);
	}
	else
	{
		goto ifData_createReference_ifUnlock;
	}
	
	ifData_wrLock (poIfData);
	
ifData_createReference_ifUnlock:
	ifTable_unLock ();
	if (poIfData == NULL)
	{
		goto ifData_createReference_cleanup;
	}
	
	i32Type != 0 ? (poIfData->oNe.i32Type = i32Type): false;
	i32AdminStatus != 0 ? (poIfData->oIf.i32AdminStatus = i32AdminStatus): false;
	
	if (bReference)
	{
		poIfData->u32NumReferences++;
	}
	if (bActivate && !neIfRowStatus_handler (&poIfData->oNe, xRowStatus_active_c))
	{
		goto ifData_createReference_cleanup;
	}
	
	if (ppoIfData == NULL)
	{
		ifData_unLock (poIfData);
	}
	else
	{
		*ppoIfData = poIfData;
	}
	return true;
	
	
ifData_createReference_cleanup:
	
	poIfData != NULL ? ifData_unLock (poIfData): false;
	if (u32IfIndex != ifIndex_zero_c)
	{
		ifData_removeReference (u32IfIndex, bCreate, bReference, bActivate);
	}
	return false;
}

bool
ifData_removeReference (
	uint32_t u32IfIndex,
	bool bCreate, bool bReference, bool bActivate)
{
	register ifData_t *poIfData = NULL;
	
	bCreate ? ifTable_wrLock (): ifTable_rdLock ();
	
	if ((poIfData = ifData_getByIndex (u32IfIndex)) == NULL)
	{
		goto ifData_removeReference_success;
	}
	ifData_wrLock (poIfData);
	
	if (bActivate && !neIfRowStatus_handler (&poIfData->oNe, xRowStatus_destroy_c))
	{
		goto ifData_removeReference_cleanup;
	}
	if (bReference && poIfData->u32NumReferences > 0)
	{
		poIfData->u32NumReferences--;
	}
	if (bCreate && poIfData->u32NumReferences == 0)
	{
		xBTree_nodeRemove (&poIfData->oBTreeNode, &oIfData_BTree);
		ifData_unLock (poIfData);
		
		register ifData_t *poTmpIfData = poIfData;
		
		poIfData = NULL;
		if (!neIfTable_removeExt (&poTmpIfData->oNe))
		{
			goto ifData_removeReference_cleanup;
		}
	}
	
ifData_removeReference_success:
	
	poIfData != NULL ? ifData_unLock (poIfData): false;
	ifTable_unLock ();
	return true;
	
	
ifData_removeReference_cleanup:
	
	poIfData != NULL ? ifData_unLock (poIfData): false;
	ifTable_unLock ();
	return false;
}

/** initialize ifTable table mapper **/
void
ifTable_init (void)
{
	extern oid ifTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ifTable", &ifTable_mapper,
		ifTable_oid, OID_LENGTH (ifTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifIndex */,
		0);
	table_info->min_column = IFINDEX;
	table_info->max_column = IFOUTERRORS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ifTable_getFirst;
	iinfo->get_next_data_point = &ifTable_getNext;
	iinfo->get_data_point = &ifTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
ifEntry_t *
ifTable_createEntry (
	uint32_t u32Index)
{
	register ifEntry_t *poEntry = NULL;
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getByIndex (u32Index)) == NULL ||
		xBitmap_getBit (poIfData->au8Flags, ifFlags_ifCreated_c))
	{
		return NULL;
	}
	poEntry = &poIfData->oIf;
	
	poEntry->i32AdminStatus = ifAdminStatus_down_c;
	poEntry->i32OperStatus = xOperStatus_notPresent_c;
	
	xBitmap_setBit (poIfData->au8Flags, ifFlags_ifCreated_c, 1);
	return poEntry;
}

ifEntry_t *
ifTable_getByIndex (
	uint32_t u32Index)
{
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getByIndex (u32Index)) == NULL ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifCreated_c))
	{
		return NULL;
	}
	
	return &poIfData->oIf;
}

ifEntry_t *
ifTable_getNextIndex (
	uint32_t u32Index)
{
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getNextIndex (u32Index)) == NULL ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifCreated_c))
	{
		return NULL;
	}
	
	return &poIfData->oIf;
}

/* remove a row from the table */
void
ifTable_removeEntry (ifEntry_t *poEntry)
{
	if (poEntry == NULL)
	{
		return;
	}
	
	register ifData_t *poIfData = ifData_getByIfEntry (poEntry);
	
	xBitmap_setBit (poIfData->au8Flags, ifFlags_ifCreated_c, 0);
	return;
}

ifEntry_t *
ifTable_createExt (
	uint32_t u32Index)
{
	ifEntry_t *poEntry = NULL;
	
	poEntry = ifTable_createEntry (
		u32Index);
	if (poEntry == NULL)
	{
		goto ifTable_createExt_cleanup;
	}
	
	if (!ifTable_createHier (poEntry))
	{
		ifTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ifTable_createExt_cleanup;
	}
	
	oInterfaces.i32IfNumber++;
	oIfMIBObjects.u32TableLastChange++;	/* TODO */
	
	
ifTable_createExt_cleanup:
	return poEntry;
}

bool
ifTable_removeExt (ifEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ifTable_removeHier (poEntry))
	{
		goto ifTable_removeExt_cleanup;
	}
	ifTable_removeEntry (poEntry);
	bRetCode = true;
	
	oInterfaces.i32IfNumber--;
	oIfMIBObjects.u32TableLastChange--;	/* TODO */
	
	
ifTable_removeExt_cleanup:
	return bRetCode;
}

bool
ifTable_createHier (
	ifEntry_t *poEntry)
{
	register ifData_t *poIfData = ifData_getByIfEntry (poEntry);
	
	if (ifXTable_getByIndex (poIfData->u32Index) == NULL &&
		ifXTable_createEntry (poIfData->u32Index) == NULL)
	{
		goto ifTable_createHier_cleanup;
	}
	
	{
		register ifStackEntry_t *poLowerStackEntry = NULL;
		
		if ((poLowerStackEntry = ifStackTable_getByIndex (poIfData->u32Index, 0)) == NULL &&
			(poLowerStackEntry = ifStackTable_getNextIndex (poIfData->u32Index, 0)) != NULL &&
			poLowerStackEntry->u32HigherLayer != poIfData->u32Index)
		{
			if ((poLowerStackEntry = ifStackTable_createExt (poIfData->u32Index, 0)) == NULL)
			{
				goto ifTable_createHier_cleanup;
			}
			
			poLowerStackEntry->u8Status = ifStackStatus_active_c;
		}
	}
	
	{
		register ifStackEntry_t *poUpperStackEntry = NULL;
		
		if ((poUpperStackEntry = ifStackTable_getByIndex (0, poIfData->u32Index)) == NULL &&
			(poUpperStackEntry = ifStackTable_LToH_getNextIndex (0, poIfData->u32Index)) != NULL &&
			poUpperStackEntry->u32LowerLayer != poIfData->u32Index)
		{
			if ((poUpperStackEntry = ifStackTable_createExt (0, poIfData->u32Index)) == NULL)
			{
				goto ifTable_createHier_cleanup;
			}
			
			poUpperStackEntry->u8Status = ifStackStatus_active_c;
		}
	}
	
	{
		register ifRcvAddressEntry_t *poIfRcvAddressEntry = NULL;
		uint8_t au8Address[sizeof (poIfRcvAddressEntry->au8Address)] = {0};
		size_t u16Address_len = 0;
		
		while (
			(poIfRcvAddressEntry = ifRcvAddressTable_getNextIndex (poIfData->u32Index, au8Address, u16Address_len)) != NULL &&
			poIfRcvAddressEntry->u32Index == poIfData->u32Index)
		{
			memcpy (au8Address, poIfRcvAddressEntry->au8Address, sizeof (au8Address));
			u16Address_len = poIfRcvAddressEntry->u16Address_len;
			
			ifRcvAddressTable_removeEntry (poIfRcvAddressEntry);
		}
	}
	
	return true;
	
	
ifTable_createHier_cleanup:
	
	ifTable_removeHier (poEntry);
	return false;
}

bool
ifTable_removeHier (
	ifEntry_t *poEntry)
{
	register uint32_t u32Index = 0;
	register ifData_t *poIfData = ifData_getByIfEntry (poEntry);
	register ifXEntry_t *poIfXEntry = NULL;
	register ifStackEntry_t *poLowerStackEntry = NULL;
	register ifStackEntry_t *poUpperStackEntry = NULL;
	
	if ((poUpperStackEntry = ifStackTable_getByIndex (0, poIfData->u32Index)) != NULL)
	{
		ifStackTable_removeExt (poUpperStackEntry);
	}
	u32Index = 0;
	while (
		(poUpperStackEntry = ifStackTable_LToH_getNextIndex (u32Index, poIfData->u32Index)) != NULL &&
		poUpperStackEntry->u32LowerLayer == poIfData->u32Index)
	{
		u32Index = poUpperStackEntry->u32HigherLayer;
		ifStackTable_removeExt (poUpperStackEntry);
	}
	
	if ((poLowerStackEntry = ifStackTable_getByIndex (poIfData->u32Index, 0)) != NULL)
	{
		ifStackTable_removeExt (poLowerStackEntry);
	}
	u32Index = 0;
	while (
		(poLowerStackEntry = ifStackTable_getNextIndex (poIfData->u32Index, u32Index)) != NULL &&
		poLowerStackEntry->u32HigherLayer == poIfData->u32Index)
	{
		u32Index = poLowerStackEntry->u32LowerLayer;
		ifStackTable_removeExt (poLowerStackEntry);
	}
	
	if ((poIfXEntry = ifXTable_getByIndex (poIfData->u32Index)) != NULL)
	{
		ifXTable_removeEntry (poIfXEntry);
	}
	
	return true;
}

bool
ifAdminStatus_handler (
	ifEntry_t *poEntry,
	int32_t i32AdminStatus, bool bForce)
{
	register ifData_t *poIfData = ifData_getByIfEntry (poEntry);
	
	if (!xBitmap_getBit (poIfData->au8Flags, ifFlags_neCreated_c) ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifXCreated_c))
	{
		goto ifAdminStatus_handler_cleanup;
	}
	
	if (poEntry->i32AdminStatus == (i32AdminStatus & xAdminStatus_mask_c) && !bForce)
	{
		goto ifAdminStatus_handler_success;
	}
	if (poIfData->oNe.u8RowStatus != xRowStatus_active_c && poIfData->oNe.u8RowStatus != xRowStatus_notReady_c &&
		(i32AdminStatus & ~xAdminStatus_mask_c))
	{
		poEntry->i32AdminStatus = i32AdminStatus;
		goto ifAdminStatus_handler_success;
	}
	
	
	switch (i32AdminStatus & xAdminStatus_mask_c)
	{
	case xAdminStatus_up_c:
		poEntry->i32AdminStatus = xAdminStatus_up_c;
		
		if (!neIfEnable_modify (poIfData, i32AdminStatus & xAdminStatus_mask_c))
		{
			goto ifAdminStatus_handler_cleanup;
		}
		break;
		
	case xAdminStatus_down_c:
		if (!neIfStatus_modify (poIfData->u32Index, xOperStatus_down_c, false, false))
		{
			goto ifAdminStatus_handler_cleanup;
		}
		
		i32AdminStatus & xAdminStatus_fromParent_c ? false: (poEntry->i32AdminStatus = xAdminStatus_down_c);
		
		if (!neIfEnable_modify (poIfData, i32AdminStatus & xAdminStatus_mask_c))
		{
			goto ifAdminStatus_handler_cleanup;
		}
		break;
		
	case xAdminStatus_testing_c:
		if (!neIfStatus_modify (poIfData->u32Index, xOperStatus_testing_c, false, false))
		{
			goto ifAdminStatus_handler_cleanup;
		}
		
		poEntry->i32AdminStatus = xAdminStatus_testing_c;
		
		if (!neIfEnable_modify (poIfData, i32AdminStatus & xAdminStatus_mask_c))
		{
			goto ifAdminStatus_handler_cleanup;
		}
		break;
	}
	
ifAdminStatus_handler_success:
	
	return true;
	
	
ifAdminStatus_handler_cleanup:
	
	return false;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ifTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIfData_BTree);
	return ifTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ifTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifData_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ifData_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) &poEntry->oIf;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIfData_BTree);
	return put_index_data;
}

bool
ifTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ifTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ifTable table mapper */
int
ifTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ifEntry_t *table_entry;
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
			table_entry = (ifEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			register ifData_t *poIfData = ifData_getByIfEntry (table_entry);
			
			switch (table_info->colnum)
			{
			case IFINDEX:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, poIfData->u32Index);
				break;
			case IFDESCR:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Descr, table_entry->u16Descr_len);
				break;
			case IFTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Type);
				break;
			case IFMTU:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Mtu);
				break;
			case IFSPEED:
				snmp_set_var_typed_integer (request->requestvb, ASN_GAUGE, table_entry->u32Speed);
				break;
			case IFPHYSADDRESS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PhysAddress, table_entry->u16PhysAddress_len);
				break;
			case IFADMINSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32AdminStatus);
				break;
			case IFOPERSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32OperStatus);
				break;
			case IFLASTCHANGE:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, table_entry->u32LastChange);
				break;
			case IFINOCTETS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InOctets);
				break;
			case IFINUCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InUcastPkts);
				break;
			case IFINDISCARDS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InDiscards);
				break;
			case IFINERRORS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InErrors);
				break;
			case IFINUNKNOWNPROTOS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InUnknownProtos);
				break;
			case IFOUTOCTETS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32OutOctets);
				break;
			case IFOUTUCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32OutUcastPkts);
				break;
			case IFOUTDISCARDS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32OutDiscards);
				break;
			case IFOUTERRORS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32OutErrors);
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
			table_entry = (ifEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFADMINSTATUS:
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
			table_entry = (ifEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ifEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFADMINSTATUS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32AdminStatus))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32AdminStatus, sizeof (table_entry->i32AdminStatus));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				if (!ifAdminStatus_handler (table_entry, *request->requestvb->val.integer, false))
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_GENERR);
					return SNMP_ERR_NOERROR;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ifEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFADMINSTATUS:
				memcpy (&table_entry->i32AdminStatus, pvOldDdata, sizeof (table_entry->i32AdminStatus));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ifXTable table mapper **/
void
ifXTable_init (void)
{
	extern oid ifXTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ifXTable", &ifXTable_mapper,
		ifXTable_oid, OID_LENGTH (ifXTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifIndex */,
		0);
	table_info->min_column = IFNAME;
	table_info->max_column = IFCOUNTERDISCONTINUITYTIME;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ifXTable_getFirst;
	iinfo->get_next_data_point = &ifXTable_getNext;
	iinfo->get_data_point = &ifXTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
ifXEntry_t *
ifXTable_createEntry (
	uint32_t u32Index)
{
	register ifXEntry_t *poEntry = NULL;
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getByIndex (u32Index)) == NULL ||
		xBitmap_getBit (poIfData->au8Flags, ifFlags_ifXCreated_c))
	{
		return NULL;
	}
	poEntry = &poIfData->oIfX;
	
	poEntry->i32LinkUpDownTrapEnable = ifLinkUpDownTrapEnable_disabled_c;
	poEntry->i32PromiscuousMode = ifPromiscuousMode_false_c;
	poEntry->i32ConnectorPresent = ifConnectorPresent_false_c;
	
	xBitmap_setBit (poIfData->au8Flags, ifFlags_ifXCreated_c, 1);
	return poEntry;
}

ifXEntry_t *
ifXTable_getByIndex (
	uint32_t u32Index)
{
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getByIndex (u32Index)) == NULL ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifXCreated_c))
	{
		return NULL;
	}
	
	return &poIfData->oIfX;
}

ifXEntry_t *
ifXTable_getNextIndex (
	uint32_t u32Index)
{
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getNextIndex (u32Index)) == NULL ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifXCreated_c))
	{
		return NULL;
	}
	
	return &poIfData->oIfX;
}

/* remove a row from the table */
void
ifXTable_removeEntry (ifXEntry_t *poEntry)
{
	if (poEntry == NULL)
	{
		return;
	}
	
	register ifData_t *poIfData = ifData_getByIfXEntry (poEntry);
	
	xBitmap_setBit (poIfData->au8Flags, ifFlags_ifXCreated_c, 0);
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ifXTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIfData_BTree);
	return ifXTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ifXTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifData_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ifData_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) &poEntry->oIfX;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIfData_BTree);
	return put_index_data;
}

bool
ifXTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifXEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = ifXTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ifXTable table mapper */
int
ifXTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ifXEntry_t *table_entry;
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
			table_entry = (ifXEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFNAME:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Name, table_entry->u16Name_len);
				break;
			case IFINMULTICASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InMulticastPkts);
				break;
			case IFINBROADCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32InBroadcastPkts);
				break;
			case IFOUTMULTICASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32OutMulticastPkts);
				break;
			case IFOUTBROADCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER, table_entry->u32OutBroadcastPkts);
				break;
			case IFHCINOCTETS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCInOctets);
				break;
			case IFHCINUCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCInUcastPkts);
				break;
			case IFHCINMULTICASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCInMulticastPkts);
				break;
			case IFHCINBROADCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCInBroadcastPkts);
				break;
			case IFHCOUTOCTETS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCOutOctets);
				break;
			case IFHCOUTUCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCOutUcastPkts);
				break;
			case IFHCOUTMULTICASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCOutMulticastPkts);
				break;
			case IFHCOUTBROADCASTPKTS:
				snmp_set_var_typed_integer (request->requestvb, ASN_COUNTER64, table_entry->u64HCOutBroadcastPkts);
				break;
			case IFLINKUPDOWNTRAPENABLE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32LinkUpDownTrapEnable);
				break;
			case IFHIGHSPEED:
				snmp_set_var_typed_integer (request->requestvb, ASN_GAUGE, table_entry->u32HighSpeed);
				break;
			case IFPROMISCUOUSMODE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32PromiscuousMode);
				break;
			case IFCONNECTORPRESENT:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32ConnectorPresent);
				break;
			case IFALIAS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Alias, table_entry->u16Alias_len);
				break;
			case IFCOUNTERDISCONTINUITYTIME:
				snmp_set_var_typed_integer (request->requestvb, ASN_TIMETICKS, table_entry->u32CounterDiscontinuityTime);
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
			table_entry = (ifXEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFLINKUPDOWNTRAPENABLE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IFPROMISCUOUSMODE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IFALIAS:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8Alias));
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
			table_entry = (ifXEntry_t*) netsnmp_extract_iterator_context (request);
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
			table_entry = (ifXEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFLINKUPDOWNTRAPENABLE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32LinkUpDownTrapEnable))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32LinkUpDownTrapEnable, sizeof (table_entry->i32LinkUpDownTrapEnable));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32LinkUpDownTrapEnable = *request->requestvb->val.integer;
				break;
			case IFPROMISCUOUSMODE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32PromiscuousMode))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32PromiscuousMode, sizeof (table_entry->i32PromiscuousMode));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32PromiscuousMode = *request->requestvb->val.integer;
				break;
			case IFALIAS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8Alias))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16Alias_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8Alias, sizeof (table_entry->au8Alias));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8Alias, 0, sizeof (table_entry->au8Alias));
				memcpy (table_entry->au8Alias, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16Alias_len = request->requestvb->val_len;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			table_entry = (ifXEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFLINKUPDOWNTRAPENABLE:
				memcpy (&table_entry->i32LinkUpDownTrapEnable, pvOldDdata, sizeof (table_entry->i32LinkUpDownTrapEnable));
				break;
			case IFPROMISCUOUSMODE:
				memcpy (&table_entry->i32PromiscuousMode, pvOldDdata, sizeof (table_entry->i32PromiscuousMode));
				break;
			case IFALIAS:
				memcpy (table_entry->au8Alias, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16Alias_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ifStackTable table mapper **/
void
ifStackTable_init (void)
{
	extern oid ifStackTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ifStackTable", &ifStackTable_mapper,
		ifStackTable_oid, OID_LENGTH (ifStackTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifStackHigherLayer */,
		ASN_INTEGER /* index: ifStackLowerLayer */,
		0);
	table_info->min_column = IFSTACKSTATUS;
	table_info->max_column = IFSTACKSTATUS;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ifStackTable_getFirst;
	iinfo->get_next_data_point = &ifStackTable_getNext;
	iinfo->get_data_point = &ifStackTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ifStackTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ifStackEntry_t *pEntry1 = xBTree_entry (pNode1, ifStackEntry_t, oBTreeNode);
	register ifStackEntry_t *pEntry2 = xBTree_entry (pNode2, ifStackEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32HigherLayer < pEntry2->u32HigherLayer) ||
		(pEntry1->u32HigherLayer == pEntry2->u32HigherLayer && pEntry1->u32LowerLayer < pEntry2->u32LowerLayer) ? -1:
		(pEntry1->u32HigherLayer == pEntry2->u32HigherLayer && pEntry1->u32LowerLayer == pEntry2->u32LowerLayer) ? 0: 1;
}

static int8_t
ifStackTable_LToH_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ifStackEntry_t *pEntry1 = xBTree_entry (pNode1, ifStackEntry_t, oLToH_BTreeNode);
	register ifStackEntry_t *pEntry2 = xBTree_entry (pNode2, ifStackEntry_t, oLToH_BTreeNode);
	
	return
		(pEntry1->u32LowerLayer < pEntry2->u32LowerLayer) ||
		(pEntry1->u32LowerLayer == pEntry2->u32LowerLayer && pEntry1->u32HigherLayer < pEntry2->u32HigherLayer) ? -1:
		(pEntry1->u32LowerLayer == pEntry2->u32LowerLayer && pEntry1->u32HigherLayer == pEntry2->u32HigherLayer) ? 0: 1;
}

xBTree_t oIfStackTable_BTree = xBTree_initInline (&ifStackTable_BTreeNodeCmp);
xBTree_t oIfStackTable_LToH_BTree = xBTree_initInline (&ifStackTable_LToH_BTreeNodeCmp);

/* create a new row in the table */
ifStackEntry_t *
ifStackTable_createEntry (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	register ifStackEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32HigherLayer = u32HigherLayer;
	poEntry->u32LowerLayer = u32LowerLayer;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIfStackTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->u8Status = xRowStatus_notInService_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIfStackTable_BTree);
	xBTree_nodeAdd (&poEntry->oLToH_BTreeNode, &oIfStackTable_LToH_BTree);
	return poEntry;
}

ifStackEntry_t *
ifStackTable_getByIndex (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	register ifStackEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32HigherLayer = u32HigherLayer;
	poTmpEntry->u32LowerLayer = u32LowerLayer;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIfStackTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifStackEntry_t, oBTreeNode);
}

ifStackEntry_t *
ifStackTable_getNextIndex (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	register ifStackEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32HigherLayer = u32HigherLayer;
	poTmpEntry->u32LowerLayer = u32LowerLayer;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIfStackTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifStackEntry_t, oBTreeNode);
}

ifStackEntry_t *
ifStackTable_LToH_getNextIndex (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	register ifStackEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (ifStackEntry_t))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32HigherLayer = u32HigherLayer;
	poTmpEntry->u32LowerLayer = u32LowerLayer;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oLToH_BTreeNode, &oIfStackTable_LToH_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifStackEntry_t, oLToH_BTreeNode);
}

/* remove a row from the table */
void
ifStackTable_removeEntry (ifStackEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIfStackTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIfStackTable_BTree);
	xBTree_nodeRemove (&poEntry->oLToH_BTreeNode, &oIfStackTable_LToH_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

bool
ifStackTable_createRegister (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	register bool bRetCode = false;
	register bool bIfLocked = false;
	register ifStackEntry_t *poEntry = NULL;
	
	if (u32HigherLayer == 0 || u32LowerLayer == 0)
	{
		return false;
	}
	
	ifStack_wrLock ();
	
	if ((poEntry = ifStackTable_getByIndex (u32HigherLayer, u32LowerLayer)) == NULL &&
		(poEntry = ifStackTable_createExt (u32HigherLayer, u32LowerLayer)) == NULL)
	{
		goto ifStackTable_createRegister_cleanup;
	}
	
	ifTable_rdLock ();
	bIfLocked = true;
	
	if (!ifStackStatus_handler (poEntry, xRowStatus_active_c))
	{
		goto ifStackTable_createRegister_cleanup;
	}
	
	bRetCode = true;
	
ifStackTable_createRegister_cleanup:
	
	bIfLocked ? ifTable_unLock (): false;
	ifStack_unLock ();
	
	return bRetCode;
}

bool
ifStackTable_removeRegister (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	register bool bRetCode = false;
	register bool bIfLocked = false;
	register ifStackEntry_t *poEntry = NULL;
	
	if (u32HigherLayer == 0 || u32LowerLayer == 0)
	{
		return false;
	}
	
	ifStack_wrLock ();
	
	if ((poEntry = ifStackTable_getByIndex (u32HigherLayer, u32LowerLayer)) == NULL)
	{
		goto ifStackTable_removeRegister_cleanup;
	}
	
	ifTable_rdLock ();
	bIfLocked = true;
	
	if (!ifStackStatus_handler (poEntry, xRowStatus_destroy_c))
	{
		goto ifStackTable_removeRegister_cleanup;
	}
	
	ifTable_unLock ();
	bIfLocked = false;
	
	if (!ifStackTable_removeExt (poEntry))
	{
		goto ifStackTable_removeRegister_cleanup;
	}
	
	bRetCode = true;
	
ifStackTable_removeRegister_cleanup:
	
	bIfLocked ? ifTable_unLock (): false;
	ifStack_unLock ();
	
	return bRetCode;
}

ifStackEntry_t *
ifStackTable_createExt (
	uint32_t u32HigherLayer,
	uint32_t u32LowerLayer)
{
	ifStackEntry_t *poEntry = NULL;
	
	poEntry = ifStackTable_createEntry (
		u32HigherLayer,
		u32LowerLayer);
	if (poEntry == NULL)
	{
		goto ifStackTable_createExt_cleanup;
	}
	
	if (!ifStackTable_createHier (poEntry))
	{
		ifStackTable_removeEntry (poEntry);
		poEntry = NULL;
		goto ifStackTable_createExt_cleanup;
	}
	
	oIfMIBObjects.u32StackLastChange++;	/* TODO */
	
	
ifStackTable_createExt_cleanup:
	
	return poEntry;
}

bool
ifStackTable_removeExt (ifStackEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!ifStackTable_removeHier (poEntry))
	{
		goto ifStackTable_removeExt_cleanup;
	}
	ifStackTable_removeEntry (poEntry);
	bRetCode = true;
	
	oIfMIBObjects.u32StackLastChange--;	/* TODO */
	
	
ifStackTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
ifStackTable_createHier (
	ifStackEntry_t *poEntry)
{
	register ifStackEntry_t *poLowerStackEntry = NULL;
	register ifStackEntry_t *poUpperStackEntry = NULL;
	
	if (poEntry->u32HigherLayer == 0 || poEntry->u32LowerLayer == 0)
	{
		return true;
	}
	
	if ((poUpperStackEntry = ifStackTable_getByIndex (poEntry->u32HigherLayer, 0)) != NULL)
	{
		ifStackTable_removeEntry (poUpperStackEntry);
	}
	
	if ((poLowerStackEntry = ifStackTable_getByIndex (0, poEntry->u32LowerLayer)) != NULL)
	{
		ifStackTable_removeEntry (poLowerStackEntry);
	}
	
	return true;
}

bool
ifStackTable_removeHier (
	ifStackEntry_t *poEntry)
{
	register ifStackEntry_t *poLowerStackEntry = NULL;
	register ifStackEntry_t *poUpperStackEntry = NULL;
	
	if (poEntry->u32HigherLayer == 0 || poEntry->u32LowerLayer == 0)
	{
		return true;
	}
	
	if ((poLowerStackEntry = ifStackTable_LToH_getNextIndex (poEntry->u32LowerLayer, 0)) == NULL ||
		poLowerStackEntry->u32LowerLayer != poEntry->u32LowerLayer)
	{
		ifStackTable_createEntry (0, poEntry->u32LowerLayer);
	}
	
	if ((poUpperStackEntry = ifStackTable_getNextIndex (poEntry->u32HigherLayer, 0)) == NULL ||
		poUpperStackEntry->u32HigherLayer != poEntry->u32HigherLayer)
	{
		ifStackTable_createEntry (poEntry->u32HigherLayer, 0);
	}
	
	return true;
}

bool
ifStackStatus_handler (
	ifStackEntry_t *poEntry,
	uint8_t u8Status)
{
	oIfMIBObjects.u32StackLastChange += 0;	/* TODO */
	
	return false;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ifStackTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIfStackTable_BTree);
	return ifStackTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ifStackTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifStackEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ifStackEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32HigherLayer);
	idx = idx->next_variable;
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32LowerLayer);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIfStackTable_BTree);
	return put_index_data;
}

bool
ifStackTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifStackEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ifStackTable_getByIndex (
		*idx1->val.integer,
		*idx2->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ifStackTable table mapper */
int
ifStackTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ifStackEntry_t *table_entry;
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8Status);
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			register netsnmp_variable_list *idx2 = idx1->next_variable;
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					if (*idx1->val.integer == 0 || *idx2->val.integer == 0)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = ifStackTable_createExt (
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ifStackTable_removeExt (table_entry);
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!ifStackStatus_handler (table_entry, *request->requestvb->val.integer))
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ifStackTable_removeExt (table_entry);
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
			table_entry = (ifStackEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFSTACKSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					ifStackTable_removeExt (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize ifRcvAddressTable table mapper **/
void
ifRcvAddressTable_init (void)
{
	extern oid ifRcvAddressTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"ifRcvAddressTable", &ifRcvAddressTable_mapper,
		ifRcvAddressTable_oid, OID_LENGTH (ifRcvAddressTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifIndex */,
		ASN_OCTET_STR /* index: ifRcvAddressAddress */,
		0);
	table_info->min_column = IFRCVADDRESSSTATUS;
	table_info->max_column = IFRCVADDRESSTYPE;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &ifRcvAddressTable_getFirst;
	iinfo->get_next_data_point = &ifRcvAddressTable_getNext;
	iinfo->get_data_point = &ifRcvAddressTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

static int8_t
ifRcvAddressTable_BTreeNodeCmp (
	xBTree_Node_t *pNode1, xBTree_Node_t *pNode2, xBTree_t *pBTree)
{
	register ifRcvAddressEntry_t *pEntry1 = xBTree_entry (pNode1, ifRcvAddressEntry_t, oBTreeNode);
	register ifRcvAddressEntry_t *pEntry2 = xBTree_entry (pNode2, ifRcvAddressEntry_t, oBTreeNode);
	
	return
		(pEntry1->u32Index < pEntry2->u32Index) ||
		(pEntry1->u32Index == pEntry2->u32Index && xBinCmp (pEntry1->au8Address, pEntry2->au8Address, pEntry1->u16Address_len, pEntry2->u16Address_len) == -1) ? -1:
		(pEntry1->u32Index == pEntry2->u32Index && xBinCmp (pEntry1->au8Address, pEntry2->au8Address, pEntry1->u16Address_len, pEntry2->u16Address_len) == 0) ? 0: 1;
}

xBTree_t oIfRcvAddressTable_BTree = xBTree_initInline (&ifRcvAddressTable_BTreeNodeCmp);

/* create a new row in the table */
ifRcvAddressEntry_t *
ifRcvAddressTable_createEntry (
	uint32_t u32Index,
	uint8_t *pau8Address, size_t u16Address_len)
{
	register ifRcvAddressEntry_t *poEntry = NULL;
	
	if ((poEntry = xBuffer_cAlloc (sizeof (*poEntry))) == NULL)
	{
		return NULL;
	}
	
	poEntry->u32Index = u32Index;
	memcpy (poEntry->au8Address, pau8Address, u16Address_len);
	poEntry->u16Address_len = u16Address_len;
	if (xBTree_nodeFind (&poEntry->oBTreeNode, &oIfRcvAddressTable_BTree) != NULL)
	{
		xBuffer_free (poEntry);
		return NULL;
	}
	
	poEntry->u8Status = xRowStatus_notInService_c;
	poEntry->i32Type = ifRcvAddressType_volatile_c;
	
	xBTree_nodeAdd (&poEntry->oBTreeNode, &oIfRcvAddressTable_BTree);
	return poEntry;
}

ifRcvAddressEntry_t *
ifRcvAddressTable_getByIndex (
	uint32_t u32Index,
	uint8_t *pau8Address, size_t u16Address_len)
{
	register ifRcvAddressEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	memcpy (poTmpEntry->au8Address, pau8Address, u16Address_len);
	poTmpEntry->u16Address_len = u16Address_len;
	if ((poNode = xBTree_nodeFind (&poTmpEntry->oBTreeNode, &oIfRcvAddressTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifRcvAddressEntry_t, oBTreeNode);
}

ifRcvAddressEntry_t *
ifRcvAddressTable_getNextIndex (
	uint32_t u32Index,
	uint8_t *pau8Address, size_t u16Address_len)
{
	register ifRcvAddressEntry_t *poTmpEntry = NULL;
	register xBTree_Node_t *poNode = NULL;
	
	if ((poTmpEntry = xBuffer_cAlloc (sizeof (*poTmpEntry))) == NULL)
	{
		return NULL;
	}
	
	poTmpEntry->u32Index = u32Index;
	memcpy (poTmpEntry->au8Address, pau8Address, u16Address_len);
	poTmpEntry->u16Address_len = u16Address_len;
	if ((poNode = xBTree_nodeFindNext (&poTmpEntry->oBTreeNode, &oIfRcvAddressTable_BTree)) == NULL)
	{
		xBuffer_free (poTmpEntry);
		return NULL;
	}
	
	xBuffer_free (poTmpEntry);
	return xBTree_entry (poNode, ifRcvAddressEntry_t, oBTreeNode);
}

/* remove a row from the table */
void
ifRcvAddressTable_removeEntry (ifRcvAddressEntry_t *poEntry)
{
	if (poEntry == NULL ||
		xBTree_nodeFind (&poEntry->oBTreeNode, &oIfRcvAddressTable_BTree) == NULL)
	{
		return;    /* Nothing to remove */
	}
	
	xBTree_nodeRemove (&poEntry->oBTreeNode, &oIfRcvAddressTable_BTree);
	xBuffer_free (poEntry);   /* XXX - release any other internal resources */
	return;
}

bool
ifRcvAddressTable_createRegister (
	uint32_t u32Index,
	uint8_t *pau8Address, size_t u16Address_len)
{
	register bool bRetCode = false;
	register ifRcvAddressEntry_t *poEntry = NULL;
	
	if (u32Index == ifIndex_zero_c ||
		pau8Address == NULL || u16Address_len == 0)
	{
		return false;
	}
	
	ifTable_rdLock ();
	
	if (ifData_getByIndex (u32Index) != NULL)
	{
		goto ifRcvAddressTable_createRegister_cleanup;
	}
	
	if ((poEntry = ifRcvAddressTable_getByIndex (u32Index, pau8Address, u16Address_len)) == NULL &&
		(poEntry = ifRcvAddressTable_createEntry (u32Index, pau8Address, u16Address_len)) == NULL)
	{
		goto ifRcvAddressTable_createRegister_cleanup;
	}
	poEntry->u8Status = xRowStatus_active_c;
	
	poEntry->u32NumReferences++;
	bRetCode = true;
	
ifRcvAddressTable_createRegister_cleanup:
	
	ifTable_unLock ();
	return bRetCode;
}

bool
ifRcvAddressTable_removeRegister (
	uint32_t u32Index,
	uint8_t *pau8Address, size_t u16Address_len)
{
	register bool bRetCode = false;
	register ifRcvAddressEntry_t *poEntry = NULL;
	
	ifTable_rdLock ();
	
	if ((poEntry = ifRcvAddressTable_getByIndex (u32Index, pau8Address, u16Address_len)) == NULL)
	{
		goto ifRcvAddressTable_removeRegister_cleanup;
	}
	
	if (poEntry->u32NumReferences > 0)
	{
		poEntry->u32NumReferences--;
	}
	if (poEntry->u32NumReferences == 0)
	{
		ifRcvAddressTable_removeEntry (poEntry);
	}
	
	bRetCode = true;
	
ifRcvAddressTable_removeRegister_cleanup:
	
	ifTable_unLock ();
	return bRetCode;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
ifRcvAddressTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIfRcvAddressTable_BTree);
	return ifRcvAddressTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
ifRcvAddressTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifRcvAddressEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ifRcvAddressEntry_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	idx = idx->next_variable;
	snmp_set_var_value (idx, poEntry->au8Address, poEntry->u16Address_len);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIfRcvAddressTable_BTree);
	return put_index_data;
}

bool
ifRcvAddressTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifRcvAddressEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	
	poEntry = ifRcvAddressTable_getByIndex (
		*idx1->val.integer,
		(void*) idx2->val.string, idx2->val_len);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* ifRcvAddressTable table mapper */
int
ifRcvAddressTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	ifRcvAddressEntry_t *table_entry;
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
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8Status);
				break;
			case IFRCVADDRESSTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Type);
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
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case IFRCVADDRESSTYPE:
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
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			register netsnmp_variable_list *idx2 = idx1->next_variable;
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = ifRcvAddressTable_createEntry (
						*idx1->val.integer,
						(void*) idx2->val.string, idx2->val_len);
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
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ifRcvAddressTable_removeEntry (table_entry);
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
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSTYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Type))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Type, sizeof (table_entry->i32Type));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Type = *request->requestvb->val.integer;
				break;
			}
		}
		/* Check the internal consistency of an active row */
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_CREATEANDGO:
					if (/* TODO : int ifRcvAddressTable_dep (...) */ TOBE_REPLACED != TOBE_REPLACED)
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
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					ifRcvAddressTable_removeEntry (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			case IFRCVADDRESSTYPE:
				memcpy (&table_entry->i32Type, pvOldDdata, sizeof (table_entry->i32Type));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (ifRcvAddressEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case IFRCVADDRESSSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				case RS_ACTIVE:
					table_entry->u8Status = RS_ACTIVE;
					break;
					
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				case RS_NOTINSERVICE:
					table_entry->u8Status = RS_NOTINSERVICE;
					break;
					
				case RS_DESTROY:
					ifRcvAddressTable_removeEntry (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}

/** initialize neIfTable table mapper **/
void
neIfTable_init (void)
{
	extern oid neIfTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"neIfTable", &neIfTable_mapper,
		neIfTable_oid, OID_LENGTH (neIfTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_INTEGER /* index: ifIndex */,
		0);
	table_info->min_column = NEIFNAME;
	table_info->max_column = NEIFSTORAGETYPE;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &neIfTable_getFirst;
	iinfo->get_next_data_point = &neIfTable_getNext;
	iinfo->get_data_point = &neIfTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
neIfEntry_t *
neIfTable_createEntry (
	uint32_t u32IfIndex)
{
	register neIfEntry_t *poEntry = NULL;
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_createEntry (u32IfIndex)) == NULL)
	{
		return NULL;
	}
	poEntry = &poIfData->oNe;
	
	/*poEntry->au8Name = ""*/;
	/*poEntry->au8Descr = ""*/;
	/*poEntry->au8PhysAddress = 0*/;
	xBitmap_setBitsRev (poEntry->au8AdminFlags, 4, 1, neIfAdminFlags_speed100Mbps_c, neIfAdminFlags_autoNeg_c, neIfAdminFlags_loopDetect_c, neIfAdminFlags_macFwd_c);
	poEntry->u8RowStatus = xRowStatus_notInService_c;
	poEntry->u8StorageType = neIfStorageType_volatile_c;
	
	xBitmap_setBit (poIfData->au8Flags, ifFlags_neCreated_c, 1); 
	return poEntry;
}

neIfEntry_t *
neIfTable_getByIndex (
	uint32_t u32IfIndex)
{
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getByIndex (u32IfIndex)) == NULL ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_neCreated_c))
	{
		return NULL;
	}
	
	return &poIfData->oNe;
}

neIfEntry_t *
neIfTable_getNextIndex (
	uint32_t u32IfIndex)
{
	register ifData_t *poIfData = NULL;
	
	if ((poIfData = ifData_getNextIndex (u32IfIndex)) == NULL ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_neCreated_c))
	{
		return NULL;
	}
	
	return &poIfData->oNe;
}

/* remove a row from the table */
void
neIfTable_removeEntry (neIfEntry_t *poEntry)
{
	if (poEntry == NULL)
	{
		return;
	}
	
	ifData_removeEntry (ifData_getByNeEntry (poEntry));
	return;
}

neIfEntry_t *
neIfTable_createExt (
	uint32_t u32IfIndex)
{
	neIfEntry_t *poEntry = NULL;
	
	poEntry = neIfTable_createEntry (
		u32IfIndex);
	if (poEntry == NULL)
	{
		goto neIfTable_createExt_cleanup;
	}
	
	if (!neIfTable_createHier (poEntry))
	{
		neIfTable_removeEntry (poEntry);
		poEntry = NULL;
		goto neIfTable_createExt_cleanup;
	}
	
	
neIfTable_createExt_cleanup:
	
	return poEntry;
}

bool
neIfTable_removeExt (neIfEntry_t *poEntry)
{
	register bool bRetCode = false;
	
	if (!neIfTable_removeHier (poEntry))
	{
		goto neIfTable_removeExt_cleanup;
	}
	neIfTable_removeEntry (poEntry);
	bRetCode = true;
	
	
neIfTable_removeExt_cleanup:
	
	return bRetCode;
}

bool
neIfTable_createHier (
	neIfEntry_t *poEntry)
{
	register ifData_t *poIfData = ifData_getByNeEntry (poEntry);
	
	if (ifTable_getByIndex (poIfData->u32Index) == NULL &&
		ifTable_createExt (poIfData->u32Index) == NULL)
	{
		goto neIfTable_createHier_cleanup;
	}
	
	return true;
	
	
neIfTable_createHier_cleanup:
	
	neIfTable_removeHier (poEntry);
	return false;
}

bool
neIfTable_removeHier (
	neIfEntry_t *poEntry)
{
	register ifEntry_t *poIfEntry = NULL;
	register ifData_t *poIfData = ifData_getByNeEntry (poEntry);
	
	if (poIfData->u32NumReferences == 0 &&
		(poIfEntry = ifTable_getByIndex (poIfData->u32Index)) != NULL)
	{
		ifTable_removeExt (poIfEntry);
	}
	
	return true;
}

bool
neIfRowStatus_handler (
	neIfEntry_t *poEntry,
	uint8_t u8RowStatus)
{
	register ifData_t *poIfData = ifData_getByNeEntry (poEntry);
	
	if (!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifCreated_c) ||
		!xBitmap_getBit (poIfData->au8Flags, ifFlags_ifXCreated_c))
	{
		goto neIfRowStatus_handler_cleanup;
	}
	
	if (poEntry->u8RowStatus == u8RowStatus)
	{
		goto neIfRowStatus_handler_success;
	}
	
	switch (u8RowStatus & xRowStatus_mask_c)
	{
	case xRowStatus_active_c:
	{
		if (poEntry->i32Type == 0)
		{
			goto neIfRowStatus_handler_cleanup;
		}
		
		poIfData->oIf.i32Type = poEntry->i32Type;
		poIfData->oIf.i32Mtu = poEntry->i32Mtu;
		poIfData->oIf.u32Speed = 0;
		xNumber_toUint32 (poEntry->au8Speed, sizeof (poEntry->au8Speed), 4, 7, &poIfData->oIf.u32Speed);
		poIfData->oIfX.u32HighSpeed = 0;
		xNumber_toUint32 (poEntry->au8Speed, sizeof (poEntry->au8Speed), 0, 3, &poIfData->oIfX.u32HighSpeed);
		
		/* TODO */
		poEntry->u8RowStatus = xRowStatus_active_c;
		
		if (!ifAdminStatus_handler (&poIfData->oIf, poIfData->oIf.i32AdminStatus, true))
		{
			goto neIfRowStatus_handler_cleanup;
		}
		break;
	}
	
	case xRowStatus_notInService_c:
		if (!ifAdminStatus_handler (&poIfData->oIf, xAdminStatus_down_c | xAdminStatus_fromParent_c, false))
		{
			goto neIfRowStatus_handler_cleanup;
		}
		
		/* TODO */
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_createAndGo_c:
		poEntry->u8RowStatus = xRowStatus_notReady_c;
		break;
		
	case xRowStatus_createAndWait_c:
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
		
	case xRowStatus_destroy_c:
		if (!ifAdminStatus_handler (&poIfData->oIf, xAdminStatus_down_c | xAdminStatus_fromParent_c, false))
		{
			goto neIfRowStatus_handler_cleanup;
		}
		
		/* TODO */
		poEntry->u8RowStatus = xRowStatus_notInService_c;
		break;
	}
	
neIfRowStatus_handler_success:
	
	return true;
	
	
neIfRowStatus_handler_cleanup:
	
	return false;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
neIfTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oIfData_BTree);
	return neIfTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
neIfTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	ifData_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, ifData_t, oBTreeNode);
	
	snmp_set_var_typed_integer (idx, ASN_INTEGER, poEntry->u32Index);
	*my_data_context = (void*) &poEntry->oNe;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oIfData_BTree);
	return put_index_data;
}

bool
neIfTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	neIfEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	
	poEntry = neIfTable_getByIndex (
		*idx1->val.integer);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* neIfTable table mapper */
int
neIfTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	neIfEntry_t *table_entry;
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			
			switch (table_info->colnum)
			{
			case NEIFNAME:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Name, table_entry->u16Name_len);
				break;
			case NEIFDESCR:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Descr, table_entry->u16Descr_len);
				break;
			case NEIFTYPE:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Type);
				break;
			case NEIFMTU:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->i32Mtu);
				break;
			case NEIFSPEED:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8Speed, table_entry->u16Speed_len);
				break;
			case NEIFPHYSADDRESS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8PhysAddress, table_entry->u16PhysAddress_len);
				break;
			case NEIFADMINFLAGS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8AdminFlags, table_entry->u16AdminFlags_len);
				break;
			case NEIFOPERFLAGS:
				snmp_set_var_typed_value (request->requestvb, ASN_OCTET_STR, (u_char*) table_entry->au8OperFlags, table_entry->u16OperFlags_len);
				break;
			case NEIFROWSTATUS:
				snmp_set_var_typed_integer (request->requestvb, ASN_INTEGER, table_entry->u8RowStatus);
				break;
			case NEIFSTORAGETYPE:
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case NEIFNAME:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8Name));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFDESCR:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8Descr));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFTYPE:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFMTU:
				ret = netsnmp_check_vb_type (requests->requestvb, ASN_INTEGER);
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFSPEED:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8Speed));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFPHYSADDRESS:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8PhysAddress));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFADMINFLAGS:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OCTET_STR, sizeof (table_entry->au8AdminFlags));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFROWSTATUS:
				ret = netsnmp_check_vb_rowstatus (request->requestvb, (table_entry ? RS_ACTIVE : RS_NONEXISTENT));
				if (ret != SNMP_ERR_NOERROR)
				{
					netsnmp_set_request_error (reqinfo, request, ret);
					return SNMP_ERR_NOERROR;
				}
				break;
			case NEIFSTORAGETYPE:
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			register netsnmp_variable_list *idx1 = table_info->indexes;
			
			switch (table_info->colnum)
			{
			case NEIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					if (/* TODO */ TOBE_REPLACED != TOBE_REPLACED)
					{
						netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
						return SNMP_ERR_NOERROR;
					}
					
					table_entry = neIfTable_createExt (
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
			case NEIFNAME:
			case NEIFDESCR:
			case NEIFTYPE:
			case NEIFMTU:
			case NEIFSPEED:
			case NEIFPHYSADDRESS:
			case NEIFADMINFLAGS:
			case NEIFSTORAGETYPE:
				if (table_entry->u8RowStatus == xRowStatus_active_c || table_entry->u8RowStatus == xRowStatus_notReady_c)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_INCONSISTENTVALUE);
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case NEIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					neIfTable_removeExt (table_entry);
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case NEIFNAME:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8Name))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16Name_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8Name, sizeof (table_entry->au8Name));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8Name, 0, sizeof (table_entry->au8Name));
				memcpy (table_entry->au8Name, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16Name_len = request->requestvb->val_len;
				break;
			case NEIFDESCR:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8Descr))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16Descr_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8Descr, sizeof (table_entry->au8Descr));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8Descr, 0, sizeof (table_entry->au8Descr));
				memcpy (table_entry->au8Descr, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16Descr_len = request->requestvb->val_len;
				break;
			case NEIFTYPE:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Type))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Type, sizeof (table_entry->i32Type));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Type = *request->requestvb->val.integer;
				break;
			case NEIFMTU:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (table_entry->i32Mtu))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					memcpy (pvOldDdata, &table_entry->i32Mtu, sizeof (table_entry->i32Mtu));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				table_entry->i32Mtu = *request->requestvb->val.integer;
				break;
			case NEIFSPEED:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8Speed))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16Speed_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8Speed, sizeof (table_entry->au8Speed));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8Speed, 0, sizeof (table_entry->au8Speed));
				memcpy (table_entry->au8Speed, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16Speed_len = request->requestvb->val_len;
				break;
			case NEIFPHYSADDRESS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8PhysAddress))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16PhysAddress_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8PhysAddress, sizeof (table_entry->au8PhysAddress));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8PhysAddress, 0, sizeof (table_entry->au8PhysAddress));
				memcpy (table_entry->au8PhysAddress, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16PhysAddress_len = request->requestvb->val_len;
				break;
			case NEIFADMINFLAGS:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->au8AdminFlags))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16AdminFlags_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->au8AdminFlags, sizeof (table_entry->au8AdminFlags));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->au8AdminFlags, 0, sizeof (table_entry->au8AdminFlags));
				memcpy (table_entry->au8AdminFlags, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16AdminFlags_len = request->requestvb->val_len;
				break;
			case NEIFSTORAGETYPE:
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case NEIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_ACTIVE:
				case RS_NOTINSERVICE:
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
				case RS_DESTROY:
					if (!neIfRowStatus_handler (table_entry, *request->requestvb->val.integer))
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
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (table_entry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			
			switch (table_info->colnum)
			{
			case NEIFNAME:
				memcpy (table_entry->au8Name, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16Name_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case NEIFDESCR:
				memcpy (table_entry->au8Descr, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16Descr_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case NEIFTYPE:
				memcpy (&table_entry->i32Type, pvOldDdata, sizeof (table_entry->i32Type));
				break;
			case NEIFMTU:
				memcpy (&table_entry->i32Mtu, pvOldDdata, sizeof (table_entry->i32Mtu));
				break;
			case NEIFSPEED:
				memcpy (table_entry->au8Speed, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16Speed_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case NEIFPHYSADDRESS:
				memcpy (table_entry->au8PhysAddress, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16PhysAddress_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case NEIFADMINFLAGS:
				memcpy (table_entry->au8AdminFlags, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
				table_entry->u16AdminFlags_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				break;
			case NEIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					neIfTable_removeExt (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
				}
				break;
			case NEIFSTORAGETYPE:
				memcpy (&table_entry->u8StorageType, pvOldDdata, sizeof (table_entry->u8StorageType));
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		for (request = requests; request != NULL; request = request->next)
		{
			table_entry = (neIfEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			
			switch (table_info->colnum)
			{
			case NEIFROWSTATUS:
				switch (*request->requestvb->val.integer)
				{
				case RS_CREATEANDGO:
				case RS_CREATEANDWAIT:
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
					break;
					
				case RS_DESTROY:
					neIfTable_removeExt (table_entry);
					break;
				}
			}
		}
		break;
	}
	
	return SNMP_ERR_NOERROR;
}


/**
 *	notification mapper(s)
 */
int
linkDown_trap (void)
{
	extern oid snmptrap_oid[];
	extern oid linkDown_oid[];
	netsnmp_variable_list *var_list = NULL;
	oid ifIndex_oid[] = {1,3,6,1,2,1,2,2,1,1, /* insert index here */};
	oid ifAdminStatus_oid[] = {1,3,6,1,2,1,2,2,1,7, /* insert index here */};
	oid ifOperStatus_oid[] = {1,3,6,1,2,1,2,2,1,8, /* insert index here */};
	
	/*
	 * Set the snmpTrapOid.0 value
	 */
	snmp_varlist_add_variable (&var_list,
		snmptrap_oid, OID_LENGTH (snmptrap_oid),
		ASN_OBJECT_ID,
		(const u_char*) linkDown_oid, sizeof (linkDown_oid));
		
	/*
	 * Add any objects from the trap definition
	 */
	snmp_varlist_add_variable (&var_list,
		ifIndex_oid, OID_LENGTH (ifIndex_oid),
		ASN_INTEGER,
		/* Set an appropriate value for ifIndex */
		NULL, 0);
	snmp_varlist_add_variable (&var_list,
		ifAdminStatus_oid, OID_LENGTH (ifAdminStatus_oid),
		ASN_INTEGER,
		/* Set an appropriate value for ifAdminStatus */
		NULL, 0);
	snmp_varlist_add_variable (&var_list,
		ifOperStatus_oid, OID_LENGTH (ifOperStatus_oid),
		ASN_INTEGER,
		/* Set an appropriate value for ifOperStatus */
		NULL, 0);
		
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
linkUp_trap (void)
{
	extern oid snmptrap_oid[];
	extern oid linkUp_oid[];
	netsnmp_variable_list *var_list = NULL;
	oid ifIndex_oid[] = {1,3,6,1,2,1,2,2,1,1, /* insert index here */};
	oid ifAdminStatus_oid[] = {1,3,6,1,2,1,2,2,1,7, /* insert index here */};
	oid ifOperStatus_oid[] = {1,3,6,1,2,1,2,2,1,8, /* insert index here */};
	
	/*
	 * Set the snmpTrapOid.0 value
	 */
	snmp_varlist_add_variable (&var_list,
		snmptrap_oid, OID_LENGTH (snmptrap_oid),
		ASN_OBJECT_ID,
		(const u_char*) linkUp_oid, sizeof (linkUp_oid));
		
	/*
	 * Add any objects from the trap definition
	 */
	snmp_varlist_add_variable (&var_list,
		ifIndex_oid, OID_LENGTH (ifIndex_oid),
		ASN_INTEGER,
		/* Set an appropriate value for ifIndex */
		NULL, 0);
	snmp_varlist_add_variable (&var_list,
		ifAdminStatus_oid, OID_LENGTH (ifAdminStatus_oid),
		ASN_INTEGER,
		/* Set an appropriate value for ifAdminStatus */
		NULL, 0);
	snmp_varlist_add_variable (&var_list,
		ifOperStatus_oid, OID_LENGTH (ifOperStatus_oid),
		ASN_INTEGER,
		/* Set an appropriate value for ifOperStatus */
		NULL, 0);
		
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
