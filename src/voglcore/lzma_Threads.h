/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
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

/* Threads.h -- multithreading library
2008-11-22 : Igor Pavlov : Public domain */

#ifndef __7Z_THRESDS_H
#define __7Z_THRESDS_H

#include "lzma_Types.h"

namespace vogl
{

    typedef struct _CThread
    {
        HANDLE handle;
    } CThread;

#define Thread_Construct(thread) (thread)->handle = NULL
#define Thread_WasCreated(thread) ((thread)->handle != NULL)

    typedef unsigned THREAD_FUNC_RET_TYPE;
#define THREAD_FUNC_CALL_TYPE MY_STD_CALL
#define THREAD_FUNC_DECL THREAD_FUNC_RET_TYPE THREAD_FUNC_CALL_TYPE

    WRes Thread_Create(CThread *thread, THREAD_FUNC_RET_TYPE(THREAD_FUNC_CALL_TYPE *startAddress)(void *), LPVOID parameter);
    WRes Thread_Wait(CThread *thread);
    WRes Thread_Close(CThread *thread);

    typedef struct _CEvent
    {
        HANDLE handle;
    } CEvent;

    typedef CEvent CAutoResetEvent;
    typedef CEvent CManualResetEvent;

#define Event_Construct(event) (event)->handle = NULL
#define Event_IsCreated(event) ((event)->handle != NULL)

    WRes ManualResetEvent_Create(CManualResetEvent *event, int initialSignaled);
    WRes ManualResetEvent_CreateNotSignaled(CManualResetEvent *event);
    WRes AutoResetEvent_Create(CAutoResetEvent *event, int initialSignaled);
    WRes AutoResetEvent_CreateNotSignaled(CAutoResetEvent *event);
    WRes Event_Set(CEvent *event);
    WRes Event_Reset(CEvent *event);
    WRes Event_Wait(CEvent *event);
    WRes Event_Close(CEvent *event);

    typedef struct _CSemaphore
    {
        HANDLE handle;
    } CSemaphore;

#define Semaphore_Construct(p) (p)->handle = NULL

    WRes Semaphore_Create(CSemaphore *p, UInt32 initiallyCount, UInt32 maxCount);
    WRes Semaphore_ReleaseN(CSemaphore *p, UInt32 num);
    WRes Semaphore_Release1(CSemaphore *p);
    WRes Semaphore_Wait(CSemaphore *p);
    WRes Semaphore_Close(CSemaphore *p);

    typedef CRITICAL_SECTION CCriticalSection;

    WRes CriticalSection_Init(CCriticalSection *p);
#define CriticalSection_Delete(p) DeleteCriticalSection(p)
#define CriticalSection_Enter(p) EnterCriticalSection(p)
#define CriticalSection_Leave(p) LeaveCriticalSection(p)
}

#endif
