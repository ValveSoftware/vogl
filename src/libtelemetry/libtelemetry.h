/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

/*
 * libtelemetry.h
 */
#ifndef _LIBTELEMETRY_H
#define _LIBTELEMETRY_H

#if defined( USE_TELEMETRY )

/* 
 * USE_TELEMETRY
 */
#include "../telemetry/include/telemetry.h"

#define SO_API_EXPORT extern "C" __attribute__((visibility ("default")))
#define SO_GLOBAL_EXPORT extern "C" __attribute__((visibility ("default")))

typedef struct telemetry_info_t
{
    HTELEMETRY ctx[8];
} telemetry_ctx_t;
SO_GLOBAL_EXPORT telemetry_info_t g_tminfo;
// Don't reference g_tminfo directly - use get_tminfo(). This works around
//  a bug where the application and so each had separate copies of g_tminfo
//  for some reason.
// http://netwinder.osuosl.org/users/p/patb/public_html/elf_relocs.html (see R_386_COPY here)
// http://www.technovelty.org/c/what-exactly-does-bsymblic-do.html (what does -Bsymbolic do)
// http://stackoverflow.com/questions/8623657/multiple-instances-of-singleton-across-shared-libraries-on-linux
// readelf --relocs and readelf --dynamic will help debug this.
// also LD_DEBUG=bindings
inline telemetry_info_t& get_tminfo() { return g_tminfo; }

#define TELEMETRY_LEVEL_MIN 0
#define TELEMETRY_LEVEL_MAX 3
#define TELEMETRY_LEVEL0 get_tminfo().ctx[0]
#define TELEMETRY_LEVEL1 get_tminfo().ctx[1]
#define TELEMETRY_LEVEL2 get_tminfo().ctx[2]
#define TELEMETRY_LEVEL3 get_tminfo().ctx[3]

SO_API_EXPORT void telemetry_tick();

SO_API_EXPORT void telemetry_set_servername(const char *servername);
SO_API_EXPORT const char *telemetry_get_servername();

SO_API_EXPORT void telemetry_set_appname(const char *appname);
SO_API_EXPORT const char *telemetry_get_appname();

SO_API_EXPORT void telemetry_set_level(int level);
SO_API_EXPORT int telemetry_get_level();

#else

/* 
 * !USE_TELEMETRY
 */
#define NTELEMETRY 1

#define TELEMETRY_LEVEL_MIN 0
#define TELEMETRY_LEVEL_MAX 3
#define TELEMETRY_LEVEL0 NULL
#define TELEMETRY_LEVEL1 NULL
#define TELEMETRY_LEVEL2 NULL
#define TELEMETRY_LEVEL3 NULL

#define telemetry_tick()

#define telemetry_set_servername(_servername)
#define telemetry_get_servername() ""

#define telemetry_set_appname(_appname)
#define telemetry_get_appname() ""

#define telemetry_set_level(_level)
#define telemetry_get_level() -1

#define TMERR_DISABLED 1
#define TMPRINTF_TOKEN_NONE 0

#define tmGetSessionName(...)
#define tmEndTryLock(...)
#define tmEndTryLockEx(...)
#define tmSetLockState(...)
#define tmSetLockStateEx(...)
#define tmSetLockStateMinTime(...) 0
#define tmSetLockStateMinTimeEx(...) 0
#define tmSignalLockCount(...)

#define tmCheckVersion(...) 0
#define tmGetCallStack(...) 0
#define tmSendCallStack( ... ) TMPRINTF_TOKEN_NONE
#define tmGetCallStackR(...) 0
#define tmSendCallStackR(...) TMPRINTF_TOKEN_NONE
#define tmSendCallStackWithSkipR(...) TMPRINTF_TOKEN_NONE

#define tmGetVersion(...) 0
#define tmStartup(...)  TMERR_DISABLED
#define tmGetPlatformInformation(...) TMERR_DISABLED
#define tmInitializeContext(...) TMERR_DISABLED
#define tmShutdown(...) TMERR_DISABLED

#define tmEnter(...)
#define tmEnterEx(...)
#define tmZone(...) 
#define tmZoneFiltered(...)
#define tmLeaveEx(...)

#define tmBeginTimeSpan(...)
#define tmEndTimeSpan(...)

#define tmEmitAccumulationZone(...)

#define tmGetStati(...) 0

#define tmSetVariable(...)

#define tmBlob(...)
#define tmDisjointBlob(...)
#define tmSetTimelineSectionName(...)
#define tmThreadName(...)
#define tmLockName(...)
#define tmMessage(...)
#define tmAlloc(...)
#define tmAllocEx(...)

#define tmTryLock(...)
#define tmTryLockEx(...)
    
#define tmPlot(...)
#define tmPlotF32(...)
#define tmPlotF64(...)
#define tmPlotI32(...)
#define tmPlotU32(...)
#define tmPlotS32(...)
#define tmPlotI64(...)
#define tmPlotU64(...)
#define tmPlotS64(...)

#define tmPPUGetListener(...) TMERR_DISABLED
#define tmPPURegisterSPUProgram(...) TMERR_DISABLED
#define tmSPUBindContextToListener(...) 
#define tmSPUUpdateTime(...)
#define tmSPUFlushImage(...)

#define TM_CONTEXT_LITE(val) ((char*)(val))
#define TM_CONTEXT_FULL(val) ((char*)(val))

typedef char *HTELEMETRY;

#endif // !USE_TELEMETRY

#endif // _LIBTELEMETRY_H
