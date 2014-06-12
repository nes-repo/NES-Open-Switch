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

#ifndef __LAGMIB_H__
#	define __LAGMIB_H__

#	ifdef __cplusplus
extern "C" {
#	endif



#include "lib/binaryTree.h"
#include "lib/snmp.h"

#define TOBE_REPLACED 1


/**
 *	agent MIB function
 */
void lagMIB_init (void);


/**
 *	scalar mapper(s)
 */
/** definitions for scalar(s) of lagMIBObjects **/
#define DOT3ADTABLESLASTCHANGED 3

typedef struct lagMIBObjects_t
{
	uint32_t u32Dot3adTablesLastChanged;
} lagMIBObjects_t;

extern lagMIBObjects_t oLagMIBObjects;

#ifdef SNMP_SRC
Netsnmp_Node_Handler lagMIBObjects_mapper;
#endif	/* SNMP_SRC */



/**
 *	table mapper(s)
 */
/**
 *	table dot3adAggTable definitions
 */
#define DOT3ADAGGINDEX 1
#define DOT3ADAGGMACADDRESS 2
#define DOT3ADAGGACTORSYSTEMPRIORITY 3
#define DOT3ADAGGACTORSYSTEMID 4
#define DOT3ADAGGAGGREGATEORINDIVIDUAL 5
#define DOT3ADAGGACTORADMINKEY 6
#define DOT3ADAGGACTOROPERKEY 7
#define DOT3ADAGGPARTNERSYSTEMID 8
#define DOT3ADAGGPARTNERSYSTEMPRIORITY 9
#define DOT3ADAGGPARTNEROPERKEY 10
#define DOT3ADAGGCOLLECTORMAXDELAY 11

enum
{
	/* enums for column dot3adAggAggregateOrIndividual */
	dot3adAggAggregateOrIndividual_true_c = 1,
	dot3adAggAggregateOrIndividual_false_c = 2,
};

/* table dot3adAggTable row entry data structure */
typedef struct dot3adAggEntry_t
{
	/* Index values */
	uint32_t u32Index;
	
	/* Column values */
	uint8_t au8MACAddress[6];
	size_t u16MACAddress_len;	/* # of uint8_t elements */
	int32_t i32ActorSystemPriority;
	uint8_t au8ActorSystemID[6];
	size_t u16ActorSystemID_len;	/* # of uint8_t elements */
	int32_t i32AggregateOrIndividual;
	int32_t i32ActorAdminKey;
	int32_t i32ActorOperKey;
	uint8_t au8PartnerSystemID[6];
	size_t u16PartnerSystemID_len;	/* # of uint8_t elements */
	int32_t i32PartnerSystemPriority;
	int32_t i32PartnerOperKey;
	int32_t i32CollectorMaxDelay;
	
	xBTree_Node_t oBTreeNode;
} dot3adAggEntry_t;

extern xBTree_t oDot3adAggTable_BTree;

/* dot3adAggTable table mapper */
void dot3adAggTable_init (void);
dot3adAggEntry_t * dot3adAggTable_createEntry (
	uint32_t u32Index);
dot3adAggEntry_t * dot3adAggTable_getByIndex (
	uint32_t u32Index);
dot3adAggEntry_t * dot3adAggTable_getNextIndex (
	uint32_t u32Index);
void dot3adAggTable_removeEntry (dot3adAggEntry_t *poEntry);
#ifdef SNMP_SRC
Netsnmp_First_Data_Point dot3adAggTable_getFirst;
Netsnmp_Next_Data_Point dot3adAggTable_getNext;
Netsnmp_Get_Data_Point dot3adAggTable_get;
Netsnmp_Node_Handler dot3adAggTable_mapper;
#endif	/* SNMP_SRC */


/**
 *	table dot3adAggPortListTable definitions
 */
#define DOT3ADAGGPORTLISTPORTS 1

/* table dot3adAggPortListTable row entry data structure */
typedef struct dot3adAggPortListEntry_t
{
	/* Index values */
	uint32_t u32Index;
	
	/* Column values */
	uint8_t au8Ports[/* TODO: PortList, PortList, "" */ TOBE_REPLACED];
	size_t u16Ports_len;	/* # of uint8_t elements */
	
	xBTree_Node_t oBTreeNode;
} dot3adAggPortListEntry_t;

extern xBTree_t oDot3adAggPortListTable_BTree;

/* dot3adAggPortListTable table mapper */
void dot3adAggPortListTable_init (void);
dot3adAggPortListEntry_t * dot3adAggPortListTable_createEntry (
	uint32_t u32Index);
dot3adAggPortListEntry_t * dot3adAggPortListTable_getByIndex (
	uint32_t u32Index);
dot3adAggPortListEntry_t * dot3adAggPortListTable_getNextIndex (
	uint32_t u32Index);
void dot3adAggPortListTable_removeEntry (dot3adAggPortListEntry_t *poEntry);
#ifdef SNMP_SRC
Netsnmp_First_Data_Point dot3adAggPortListTable_getFirst;
Netsnmp_Next_Data_Point dot3adAggPortListTable_getNext;
Netsnmp_Get_Data_Point dot3adAggPortListTable_get;
Netsnmp_Node_Handler dot3adAggPortListTable_mapper;
#endif	/* SNMP_SRC */


/**
 *	table dot3adAggPortTable definitions
 */
#define DOT3ADAGGPORTINDEX 1
#define DOT3ADAGGPORTACTORSYSTEMPRIORITY 2
#define DOT3ADAGGPORTACTORSYSTEMID 3
#define DOT3ADAGGPORTACTORADMINKEY 4
#define DOT3ADAGGPORTACTOROPERKEY 5
#define DOT3ADAGGPORTPARTNERADMINSYSTEMPRIORITY 6
#define DOT3ADAGGPORTPARTNEROPERSYSTEMPRIORITY 7
#define DOT3ADAGGPORTPARTNERADMINSYSTEMID 8
#define DOT3ADAGGPORTPARTNEROPERSYSTEMID 9
#define DOT3ADAGGPORTPARTNERADMINKEY 10
#define DOT3ADAGGPORTPARTNEROPERKEY 11
#define DOT3ADAGGPORTSELECTEDAGGID 12
#define DOT3ADAGGPORTATTACHEDAGGID 13
#define DOT3ADAGGPORTACTORPORT 14
#define DOT3ADAGGPORTACTORPORTPRIORITY 15
#define DOT3ADAGGPORTPARTNERADMINPORT 16
#define DOT3ADAGGPORTPARTNEROPERPORT 17
#define DOT3ADAGGPORTPARTNERADMINPORTPRIORITY 18
#define DOT3ADAGGPORTPARTNEROPERPORTPRIORITY 19
#define DOT3ADAGGPORTACTORADMINSTATE 20
#define DOT3ADAGGPORTACTOROPERSTATE 21
#define DOT3ADAGGPORTPARTNERADMINSTATE 22
#define DOT3ADAGGPORTPARTNEROPERSTATE 23
#define DOT3ADAGGPORTAGGREGATEORINDIVIDUAL 24

enum
{
	/* enums for column dot3adAggPortActorAdminState */
	dot3adAggPortActorAdminState_lacpActivity_c = 0,
	dot3adAggPortActorAdminState_lacpTimeout_c = 1,
	dot3adAggPortActorAdminState_aggregation_c = 2,
	dot3adAggPortActorAdminState_synchronization_c = 3,
	dot3adAggPortActorAdminState_collecting_c = 4,
	dot3adAggPortActorAdminState_distributing_c = 5,
	dot3adAggPortActorAdminState_defaulted_c = 6,
	dot3adAggPortActorAdminState_expired_c = 7,

	/* enums for column dot3adAggPortActorOperState */
	dot3adAggPortActorOperState_lacpActivity_c = 0,
	dot3adAggPortActorOperState_lacpTimeout_c = 1,
	dot3adAggPortActorOperState_aggregation_c = 2,
	dot3adAggPortActorOperState_synchronization_c = 3,
	dot3adAggPortActorOperState_collecting_c = 4,
	dot3adAggPortActorOperState_distributing_c = 5,
	dot3adAggPortActorOperState_defaulted_c = 6,
	dot3adAggPortActorOperState_expired_c = 7,

	/* enums for column dot3adAggPortPartnerAdminState */
	dot3adAggPortPartnerAdminState_lacpActivity_c = 0,
	dot3adAggPortPartnerAdminState_lacpTimeout_c = 1,
	dot3adAggPortPartnerAdminState_aggregation_c = 2,
	dot3adAggPortPartnerAdminState_synchronization_c = 3,
	dot3adAggPortPartnerAdminState_collecting_c = 4,
	dot3adAggPortPartnerAdminState_distributing_c = 5,
	dot3adAggPortPartnerAdminState_defaulted_c = 6,
	dot3adAggPortPartnerAdminState_expired_c = 7,

	/* enums for column dot3adAggPortPartnerOperState */
	dot3adAggPortPartnerOperState_lacpActivity_c = 0,
	dot3adAggPortPartnerOperState_lacpTimeout_c = 1,
	dot3adAggPortPartnerOperState_aggregation_c = 2,
	dot3adAggPortPartnerOperState_synchronization_c = 3,
	dot3adAggPortPartnerOperState_collecting_c = 4,
	dot3adAggPortPartnerOperState_distributing_c = 5,
	dot3adAggPortPartnerOperState_defaulted_c = 6,
	dot3adAggPortPartnerOperState_expired_c = 7,

	/* enums for column dot3adAggPortAggregateOrIndividual */
	dot3adAggPortAggregateOrIndividual_true_c = 1,
	dot3adAggPortAggregateOrIndividual_false_c = 2,
};

/* table dot3adAggPortTable row entry data structure */
typedef struct dot3adAggPortEntry_t
{
	/* Index values */
	uint32_t u32Index;
	
	/* Column values */
	int32_t i32ActorSystemPriority;
	uint8_t au8ActorSystemID[6];
	size_t u16ActorSystemID_len;	/* # of uint8_t elements */
	int32_t i32ActorAdminKey;
	int32_t i32ActorOperKey;
	int32_t i32PartnerAdminSystemPriority;
	int32_t i32PartnerOperSystemPriority;
	uint8_t au8PartnerAdminSystemID[6];
	size_t u16PartnerAdminSystemID_len;	/* # of uint8_t elements */
	uint8_t au8PartnerOperSystemID[6];
	size_t u16PartnerOperSystemID_len;	/* # of uint8_t elements */
	int32_t i32PartnerAdminKey;
	int32_t i32PartnerOperKey;
	uint32_t u32SelectedAggID;
	uint32_t u32AttachedAggID;
	int32_t i32ActorPort;
	int32_t i32ActorPortPriority;
	int32_t i32PartnerAdminPort;
	int32_t i32PartnerOperPort;
	int32_t i32PartnerAdminPortPriority;
	int32_t i32PartnerOperPortPriority;
	uint8_t au8ActorAdminState[1];
	size_t u16ActorAdminState_len;	/* # of uint8_t elements */
	uint8_t au8ActorOperState[1];
	size_t u16ActorOperState_len;	/* # of uint8_t elements */
	uint8_t au8PartnerAdminState[1];
	size_t u16PartnerAdminState_len;	/* # of uint8_t elements */
	uint8_t au8PartnerOperState[1];
	size_t u16PartnerOperState_len;	/* # of uint8_t elements */
	int32_t i32AggregateOrIndividual;
	
	xBTree_Node_t oBTreeNode;
} dot3adAggPortEntry_t;

extern xBTree_t oDot3adAggPortTable_BTree;

/* dot3adAggPortTable table mapper */
void dot3adAggPortTable_init (void);
dot3adAggPortEntry_t * dot3adAggPortTable_createEntry (
	uint32_t u32Index);
dot3adAggPortEntry_t * dot3adAggPortTable_getByIndex (
	uint32_t u32Index);
dot3adAggPortEntry_t * dot3adAggPortTable_getNextIndex (
	uint32_t u32Index);
void dot3adAggPortTable_removeEntry (dot3adAggPortEntry_t *poEntry);
#ifdef SNMP_SRC
Netsnmp_First_Data_Point dot3adAggPortTable_getFirst;
Netsnmp_Next_Data_Point dot3adAggPortTable_getNext;
Netsnmp_Get_Data_Point dot3adAggPortTable_get;
Netsnmp_Node_Handler dot3adAggPortTable_mapper;
#endif	/* SNMP_SRC */


/**
 *	table dot3adAggPortStatsTable definitions
 */
#define DOT3ADAGGPORTSTATSLACPDUSRX 1
#define DOT3ADAGGPORTSTATSMARKERPDUSRX 2
#define DOT3ADAGGPORTSTATSMARKERRESPONSEPDUSRX 3
#define DOT3ADAGGPORTSTATSUNKNOWNRX 4
#define DOT3ADAGGPORTSTATSILLEGALRX 5
#define DOT3ADAGGPORTSTATSLACPDUSTX 6
#define DOT3ADAGGPORTSTATSMARKERPDUSTX 7
#define DOT3ADAGGPORTSTATSMARKERRESPONSEPDUSTX 8

/* table dot3adAggPortStatsTable row entry data structure */
typedef struct dot3adAggPortStatsEntry_t
{
	/* Index values */
	uint32_t u32Index;
	
	/* Column values */
	uint32_t u32LACPDUsRx;
	uint32_t u32MarkerPDUsRx;
	uint32_t u32MarkerResponsePDUsRx;
	uint32_t u32UnknownRx;
	uint32_t u32IllegalRx;
	uint32_t u32LACPDUsTx;
	uint32_t u32MarkerPDUsTx;
	uint32_t u32MarkerResponsePDUsTx;
	
	xBTree_Node_t oBTreeNode;
} dot3adAggPortStatsEntry_t;

extern xBTree_t oDot3adAggPortStatsTable_BTree;

/* dot3adAggPortStatsTable table mapper */
void dot3adAggPortStatsTable_init (void);
dot3adAggPortStatsEntry_t * dot3adAggPortStatsTable_createEntry (
	uint32_t u32Index);
dot3adAggPortStatsEntry_t * dot3adAggPortStatsTable_getByIndex (
	uint32_t u32Index);
dot3adAggPortStatsEntry_t * dot3adAggPortStatsTable_getNextIndex (
	uint32_t u32Index);
void dot3adAggPortStatsTable_removeEntry (dot3adAggPortStatsEntry_t *poEntry);
#ifdef SNMP_SRC
Netsnmp_First_Data_Point dot3adAggPortStatsTable_getFirst;
Netsnmp_Next_Data_Point dot3adAggPortStatsTable_getNext;
Netsnmp_Get_Data_Point dot3adAggPortStatsTable_get;
Netsnmp_Node_Handler dot3adAggPortStatsTable_mapper;
#endif	/* SNMP_SRC */


/**
 *	table dot3adAggPortDebugTable definitions
 */
#define DOT3ADAGGPORTDEBUGRXSTATE 1
#define DOT3ADAGGPORTDEBUGLASTRXTIME 2
#define DOT3ADAGGPORTDEBUGMUXSTATE 3
#define DOT3ADAGGPORTDEBUGMUXREASON 4
#define DOT3ADAGGPORTDEBUGACTORCHURNSTATE 5
#define DOT3ADAGGPORTDEBUGPARTNERCHURNSTATE 6
#define DOT3ADAGGPORTDEBUGACTORCHURNCOUNT 7
#define DOT3ADAGGPORTDEBUGPARTNERCHURNCOUNT 8
#define DOT3ADAGGPORTDEBUGACTORSYNCTRANSITIONCOUNT 9
#define DOT3ADAGGPORTDEBUGPARTNERSYNCTRANSITIONCOUNT 10
#define DOT3ADAGGPORTDEBUGACTORCHANGECOUNT 11
#define DOT3ADAGGPORTDEBUGPARTNERCHANGECOUNT 12

enum
{
	/* enums for column dot3adAggPortDebugRxState */
	dot3adAggPortDebugRxState_currentRx_c = 1,
	dot3adAggPortDebugRxState_expired_c = 2,
	dot3adAggPortDebugRxState_defaulted_c = 3,
	dot3adAggPortDebugRxState_initialize_c = 4,
	dot3adAggPortDebugRxState_lacpDisabled_c = 5,
	dot3adAggPortDebugRxState_portDisabled_c = 6,

	/* enums for column dot3adAggPortDebugMuxState */
	dot3adAggPortDebugMuxState_detached_c = 1,
	dot3adAggPortDebugMuxState_waiting_c = 2,
	dot3adAggPortDebugMuxState_attached_c = 3,
	dot3adAggPortDebugMuxState_collecting_c = 4,
	dot3adAggPortDebugMuxState_distributing_c = 5,
	dot3adAggPortDebugMuxState_collectingDistributing_c = 6,

	/* enums for column dot3adAggPortDebugActorChurnState */
	dot3adAggPortDebugActorChurnState_noChurn_c = 1,
	dot3adAggPortDebugActorChurnState_churn_c = 2,
	dot3adAggPortDebugActorChurnState_churnMonitor_c = 3,

	/* enums for column dot3adAggPortDebugPartnerChurnState */
	dot3adAggPortDebugPartnerChurnState_noChurn_c = 1,
	dot3adAggPortDebugPartnerChurnState_churn_c = 2,
	dot3adAggPortDebugPartnerChurnState_churnMonitor_c = 3,
};

/* table dot3adAggPortDebugTable row entry data structure */
typedef struct dot3adAggPortDebugEntry_t
{
	/* Index values */
	uint32_t u32Index;
	
	/* Column values */
	int32_t i32RxState;
	uint32_t u32LastRxTime;
	int32_t i32MuxState;
	uint8_t au8MuxReason[255];
	size_t u16MuxReason_len;	/* # of uint8_t elements */
	int32_t i32ActorChurnState;
	int32_t i32PartnerChurnState;
	uint32_t u32ActorChurnCount;
	uint32_t u32PartnerChurnCount;
	uint32_t u32ActorSyncTransitionCount;
	uint32_t u32PartnerSyncTransitionCount;
	uint32_t u32ActorChangeCount;
	uint32_t u32PartnerChangeCount;
	
	xBTree_Node_t oBTreeNode;
} dot3adAggPortDebugEntry_t;

extern xBTree_t oDot3adAggPortDebugTable_BTree;

/* dot3adAggPortDebugTable table mapper */
void dot3adAggPortDebugTable_init (void);
dot3adAggPortDebugEntry_t * dot3adAggPortDebugTable_createEntry (
	uint32_t u32Index);
dot3adAggPortDebugEntry_t * dot3adAggPortDebugTable_getByIndex (
	uint32_t u32Index);
dot3adAggPortDebugEntry_t * dot3adAggPortDebugTable_getNextIndex (
	uint32_t u32Index);
void dot3adAggPortDebugTable_removeEntry (dot3adAggPortDebugEntry_t *poEntry);
#ifdef SNMP_SRC
Netsnmp_First_Data_Point dot3adAggPortDebugTable_getFirst;
Netsnmp_Next_Data_Point dot3adAggPortDebugTable_getNext;
Netsnmp_Get_Data_Point dot3adAggPortDebugTable_get;
Netsnmp_Node_Handler dot3adAggPortDebugTable_mapper;
#endif	/* SNMP_SRC */


/**
 *	table dot3adAggPortXTable definitions
 */
#define DOT3ADAGGPORTPROTOCOLDA 1

/* table dot3adAggPortXTable row entry data structure */
typedef struct dot3adAggPortXEntry_t
{
	/* Index values */
	uint32_t u32Index;
	
	/* Column values */
	uint8_t au8ProtocolDA[6];
	size_t u16ProtocolDA_len;	/* # of uint8_t elements */
	
	xBTree_Node_t oBTreeNode;
} dot3adAggPortXEntry_t;

extern xBTree_t oDot3adAggPortXTable_BTree;

/* dot3adAggPortXTable table mapper */
void dot3adAggPortXTable_init (void);
dot3adAggPortXEntry_t * dot3adAggPortXTable_createEntry (
	uint32_t u32Index);
dot3adAggPortXEntry_t * dot3adAggPortXTable_getByIndex (
	uint32_t u32Index);
dot3adAggPortXEntry_t * dot3adAggPortXTable_getNextIndex (
	uint32_t u32Index);
void dot3adAggPortXTable_removeEntry (dot3adAggPortXEntry_t *poEntry);
#ifdef SNMP_SRC
Netsnmp_First_Data_Point dot3adAggPortXTable_getFirst;
Netsnmp_Next_Data_Point dot3adAggPortXTable_getNext;
Netsnmp_Get_Data_Point dot3adAggPortXTable_get;
Netsnmp_Node_Handler dot3adAggPortXTable_mapper;
#endif	/* SNMP_SRC */



#	ifdef __cplusplus
}
#	endif

#endif /* __LAGMIB_H__ */
