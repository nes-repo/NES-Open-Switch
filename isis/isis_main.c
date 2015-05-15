/*
 *  Copyright (c) 2008-2015
 *      NES Dev <nes.open.switch@gmail.com>
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

#ifndef __ISIS_MAIN_C__
#	define __ISIS_MAIN_C__


#include "isis_ext.h"
#include "isis_defines.h"
#include "switch_ext.h"

#include "lib/thread.h"


static xThreadInfo_t oIsisThread =
{
	.u32Index = XTHREAD_ID (ModuleId_isis_c, 0),
	.u8SchedPolicy = SCHED_RR,
	.u8Priority = 1,
	.poStart = &isis_start,
};


void *
isis_main (
	void *pvArgv)
{
	if (xThread_create (&oIsisThread) == NULL)
	{
		Isis_log (xLog_err_c, "xThread_create() failed\n");
		return NULL;
	}
	
	return NULL;
}

void *
isis_start (
	void *pvArgv)
{
	while (1)
	{
		xThread_sleep (1);
	}
	return NULL;
}


#endif	// __ISIS_MAIN_C__
