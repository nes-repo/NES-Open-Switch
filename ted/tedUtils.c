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
//set ts=4 sw=4

#ifndef __TEDUTILS_C__
#	define __TEDUTILS_C__



#include "tedUtils.h"
#include "if/ifUtils.h"
#include "if/ifMIB.h"

#include <stdbool.h>
#include <stdint.h>


static ifType_stackHandler_t teLinkTable_stackHandler;


bool tedUtilsInit (void)
{
	register bool bRetCode = false;
	ifTypeEntry_t *poIfTypeEntry = NULL;
	
	ifTable_wrLock ();
	
	if ((poIfTypeEntry = ifTypeTable_createExt (ifType_teLink_c)) == NULL)
	{
		goto tedUtilsInit_cleanup;
	}
	
	poIfTypeEntry->pfStackHandler = teLinkTable_stackHandler;
	
	bRetCode = true;
	
tedUtilsInit_cleanup:
	
	ifTable_unLock ();
	return bRetCode;
}

bool
teLinkTable_stackHandler (
	ifEntry_t *poHigherIfEntry, ifEntry_t *poLowerIfEntry,
	uint8_t u8Action, bool isLocked)
{
	return true;
}



#endif	// __TEDUTILS_C__
