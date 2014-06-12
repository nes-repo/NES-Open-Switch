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

#ifndef __ETHERNET_DEFINES_H__
#	define __ETHERNET_DEFINES_H__

#	ifdef __cplusplus
extern "C" {
#	endif


#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "lib/common.h"

#define LOG_TIME_USED
#include "lib/log.h"

#define MOD_NAME "ETHERNET"
#define Ethernet_log(_pri, _frmt, _args ...) xLog_str (MOD_NAME, _pri, _frmt, ## _args)


#	ifdef __cplusplus
}
#	endif

#endif	// __ETHERNET_DEFINES_H__
