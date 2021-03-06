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
#include "mplsLsrStdMIB.h"
#include "mplsLsrExtStdMIB.h"

#include "system_ext.h"

#include "lib/binaryTree.h"
#include "lib/buffer.h"
#include "lib/snmp.h"

#include <stdbool.h>
#include <stdint.h>

#define ROLLBACK_BUFFER "ROLLBACK_BUFFER"



static oid mplsLsrExtStdMIB_oid[] = {1,3,6,1,2,1,10,166,19};

static oid mplsXCExtTable_oid[] = {1,3,6,1,2,1,10,166,19,1,1};



/**
 *	initialize mplsLsrExtStdMIB group mapper
 */
void
mplsLsrExtStdMIB_init (void)
{
	extern oid mplsLsrExtStdMIB_oid[];
	
	DEBUGMSGTL (("mplsLsrExtStdMIB", "Initializing\n"));
	
	
	/* register mplsLsrExtStdMIB group table mappers */
	mplsXCExtTable_init ();
	
	/* register mplsLsrExtStdMIB modules */
	sysORTable_createRegister ("mplsLsrExtStdMIB", mplsLsrExtStdMIB_oid, OID_LENGTH (mplsLsrExtStdMIB_oid));
}


/**
 *	table mapper(s) & helper(s)
 */
/** initialize mplsXCExtTable table mapper **/
void
mplsXCExtTable_init (void)
{
	extern oid mplsXCExtTable_oid[];
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;
	netsnmp_table_registration_info *table_info;
	
	reg = netsnmp_create_handler_registration (
		"mplsXCExtTable", &mplsXCExtTable_mapper,
		mplsXCExtTable_oid, OID_LENGTH (mplsXCExtTable_oid),
		HANDLER_CAN_RWRITE
		);
		
	table_info = xBuffer_cAlloc (sizeof (netsnmp_table_registration_info));
	netsnmp_table_helper_add_indexes (table_info,
		ASN_OCTET_STR /* index: mplsXCIndex */,
		ASN_OCTET_STR /* index: mplsXCInSegmentIndex */,
		ASN_OCTET_STR /* index: mplsXCOutSegmentIndex */,
		0);
	table_info->min_column = MPLSXCEXTTUNNELPOINTER;
	table_info->max_column = MPLSXCEXTOPPOSITEDIRXCPTR;
	
	iinfo = xBuffer_cAlloc (sizeof (netsnmp_iterator_info));
	iinfo->get_first_data_point = &mplsXCExtTable_getFirst;
	iinfo->get_next_data_point = &mplsXCExtTable_getNext;
	iinfo->get_data_point = &mplsXCExtTable_get;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	
	netsnmp_register_table_iterator (reg, iinfo);
	
	/* Initialise the contents of the table here */
}

/* create a new row in the table */
mplsXCExtEntry_t *
mplsXCExtTable_createEntry (
	uint8_t *pau8Index, size_t u16Index_len,
	uint8_t *pau8InSegmentIndex, size_t u16InSegmentIndex_len,
	uint8_t *pau8OutSegmentIndex, size_t u16OutSegmentIndex_len)
{
	register mplsXCExtEntry_t *poEntry = NULL;
	register mplsXCEntry_t *poXC = NULL;
	
	if ((poXC = mplsXCTable_getByIndex (pau8Index, u16Index_len, pau8InSegmentIndex, u16InSegmentIndex_len, pau8OutSegmentIndex, u16OutSegmentIndex_len)) == NULL)
	{
		return NULL;
	}
	poEntry = &poXC->oX;
	
	return poEntry;
}

mplsXCExtEntry_t *
mplsXCExtTable_getByIndex (
	uint8_t *pau8Index, size_t u16Index_len,
	uint8_t *pau8InSegmentIndex, size_t u16InSegmentIndex_len,
	uint8_t *pau8OutSegmentIndex, size_t u16OutSegmentIndex_len)
{
	register mplsXCEntry_t *poXC = NULL;
	
	if ((poXC = mplsXCTable_getByIndex (pau8Index, u16Index_len, pau8InSegmentIndex, u16InSegmentIndex_len, pau8OutSegmentIndex, u16OutSegmentIndex_len)) == NULL)
	{
		return NULL;
	}
	
	return &poXC->oX;
}

mplsXCExtEntry_t *
mplsXCExtTable_getNextIndex (
	uint8_t *pau8Index, size_t u16Index_len,
	uint8_t *pau8InSegmentIndex, size_t u16InSegmentIndex_len,
	uint8_t *pau8OutSegmentIndex, size_t u16OutSegmentIndex_len)
{
	register mplsXCEntry_t *poXC = NULL;
	
	if ((poXC = mplsXCTable_getNextIndex (pau8Index, u16Index_len, pau8InSegmentIndex, u16InSegmentIndex_len, pau8OutSegmentIndex, u16OutSegmentIndex_len)) == NULL)
	{
		return NULL;
	}
	
	return &poXC->oX;
}

/* remove a row from the table */
void
mplsXCExtTable_removeEntry (mplsXCExtEntry_t *poEntry)
{
	return;
}

/* example iterator hook routines - using 'getNext' to do most of the work */
netsnmp_variable_list *
mplsXCExtTable_getFirst (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	*my_loop_context = xBTree_nodeGetFirst (&oMplsXCTable_BTree);
	return mplsXCExtTable_getNext (my_loop_context, my_data_context, put_index_data, mydata);
}

netsnmp_variable_list *
mplsXCExtTable_getNext (
	void **my_loop_context, void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	mplsXCEntry_t *poEntry = NULL;
	netsnmp_variable_list *idx = put_index_data;
	
	if (*my_loop_context == NULL)
	{
		return NULL;
	}
	poEntry = xBTree_entry (*my_loop_context, mplsXCEntry_t, oBTreeNode);
	
	snmp_set_var_value (idx, poEntry->au8Index, poEntry->u16Index_len);
	idx = idx->next_variable;
	snmp_set_var_value (idx, poEntry->au8InSegmentIndex, poEntry->u16InSegmentIndex_len);
	idx = idx->next_variable;
	snmp_set_var_value (idx, poEntry->au8OutSegmentIndex, poEntry->u16OutSegmentIndex_len);
	*my_data_context = (void*) poEntry;
	*my_loop_context = (void*) xBTree_nodeGetNext (&poEntry->oBTreeNode, &oMplsXCTable_BTree);
	return put_index_data;
}

bool
mplsXCExtTable_get (
	void **my_data_context,
	netsnmp_variable_list *put_index_data, netsnmp_iterator_info *mydata)
{
	mplsXCEntry_t *poEntry = NULL;
	register netsnmp_variable_list *idx1 = put_index_data;
	register netsnmp_variable_list *idx2 = idx1->next_variable;
	register netsnmp_variable_list *idx3 = idx2->next_variable;
	
	poEntry = mplsXCTable_getByIndex (
		(void*) idx1->val.string, idx1->val_len,
		(void*) idx2->val.string, idx2->val_len,
		(void*) idx3->val.string, idx3->val_len);
	if (poEntry == NULL)
	{
		return false;
	}
	
	*my_data_context = (void*) poEntry;
	return true;
}

/* mplsXCExtTable table mapper */
int
mplsXCExtTable_mapper (
	netsnmp_mib_handler *handler,
	netsnmp_handler_registration *reginfo,
	netsnmp_agent_request_info *reqinfo,
	netsnmp_request_info *requests)
{
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	mplsXCExtEntry_t *table_entry;
	register mplsXCEntry_t *poEntry = NULL;
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
			poEntry = (mplsXCEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case MPLSXCEXTTUNNELPOINTER:
				snmp_set_var_typed_value (request->requestvb, ASN_OBJECT_ID, (u_char*) table_entry->aoTunnelPointer, table_entry->u16TunnelPointer_len);
				break;
			case MPLSXCEXTOPPOSITEDIRXCPTR:
				snmp_set_var_typed_value (request->requestvb, ASN_OBJECT_ID, (u_char*) table_entry->aoOppositeDirXCPtr, table_entry->u16OppositeDirXCPtr_len);
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
			poEntry = (mplsXCEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case MPLSXCEXTOPPOSITEDIRXCPTR:
				ret = netsnmp_check_vb_type_and_max_size (request->requestvb, ASN_OBJECT_ID, sizeof (table_entry->aoOppositeDirXCPtr));
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
			poEntry = (mplsXCEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL)
			{
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case MPLSXCEXTOPPOSITEDIRXCPTR:
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
			poEntry = (mplsXCEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case MPLSXCEXTOPPOSITEDIRXCPTR:
				if (pvOldDdata == NULL && (pvOldDdata = xBuffer_cAlloc (sizeof (xOctetString_t) + sizeof (table_entry->aoOppositeDirXCPtr))) == NULL)
				{
					netsnmp_set_request_error (reqinfo, request, SNMP_ERR_RESOURCEUNAVAILABLE);
					return SNMP_ERR_NOERROR;
				}
				else if (pvOldDdata != table_entry)
				{
					((xOctetString_t*) pvOldDdata)->pData = pvOldDdata + sizeof (xOctetString_t);
					((xOctetString_t*) pvOldDdata)->u16Len = table_entry->u16OppositeDirXCPtr_len;
					memcpy (((xOctetString_t*) pvOldDdata)->pData, table_entry->aoOppositeDirXCPtr, sizeof (table_entry->aoOppositeDirXCPtr));
					netsnmp_request_add_list_data (request, netsnmp_create_data_list (ROLLBACK_BUFFER, pvOldDdata, &xBuffer_free));
				}
				
				memset (table_entry->aoOppositeDirXCPtr, 0, sizeof (table_entry->aoOppositeDirXCPtr));
				memcpy (table_entry->aoOppositeDirXCPtr, request->requestvb->val.string, request->requestvb->val_len);
				table_entry->u16OppositeDirXCPtr_len = request->requestvb->val_len;
				break;
			}
		}
		break;
		
	case MODE_SET_UNDO:
		for (request = requests; request != NULL; request = request->next)
		{
			pvOldDdata = netsnmp_request_get_list_data (request, ROLLBACK_BUFFER);
			poEntry = (mplsXCEntry_t*) netsnmp_extract_iterator_context (request);
			table_info = netsnmp_extract_table_info (request);
			if (poEntry == NULL || pvOldDdata == NULL)
			{
				continue;
			}
			table_entry = &poEntry->oX;
			
			switch (table_info->colnum)
			{
			case MPLSXCEXTOPPOSITEDIRXCPTR:
				if (pvOldDdata == table_entry)
				{
					mplsXCExtTable_removeEntry (table_entry);
					netsnmp_request_remove_list_entry (request, ROLLBACK_BUFFER);
				}
				else
				{
					memcpy (table_entry->aoOppositeDirXCPtr, ((xOctetString_t*) pvOldDdata)->pData, ((xOctetString_t*) pvOldDdata)->u16Len);
					table_entry->u16OppositeDirXCPtr_len = ((xOctetString_t*) pvOldDdata)->u16Len;
				}
				break;
			}
		}
		break;
		
	case MODE_SET_COMMIT:
		break;
	}
	
	return SNMP_ERR_NOERROR;
}
