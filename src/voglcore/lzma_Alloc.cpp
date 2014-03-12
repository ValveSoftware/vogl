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

/* Alloc.c -- Memory allocation functions
2008-09-24
Igor Pavlov
Public domain */
#include "vogl_core.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>

#include "lzma_Alloc.h"

namespace vogl
{

/* #define _SZ_ALLOC_DEBUG */

/* use _SZ_ALLOC_DEBUG to debug alloc/free operations */
#ifdef _SZ_ALLOC_DEBUG
#include <stdio.h>
    int g_allocCount = 0;
    int g_allocCountMid = 0;
    int g_allocCountBig = 0;
#endif

    void *MyAlloc(size_t size)
    {
        if (size == 0)
            return 0;
#ifdef _SZ_ALLOC_DEBUG
        {
            void *p = vogl_malloc(size);
            fprintf(stderr, "\nAlloc %10d bytes, count = %10d,  addr = %8X", size, g_allocCount++, (unsigned)p);
            return p;
        }
#else
        return vogl_malloc(size);
#endif
    }

    void MyFree(void *address)
    {
#ifdef _SZ_ALLOC_DEBUG
        if (address != 0)
            fprintf(stderr, "\nFree; count = %10d,  addr = %8X", --g_allocCount, (unsigned)address);
#endif
        vogl_free(address);
    }

#ifdef _WIN32

    void *MidAlloc(size_t size)
    {
        if (size == 0)
            return 0;
#ifdef _SZ_ALLOC_DEBUG
        fprintf(stderr, "\nAlloc_Mid %10d bytes;  count = %10d", size, g_allocCountMid++);
#endif
        return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
    }

    void MidFree(void *address)
    {
#ifdef _SZ_ALLOC_DEBUG
        if (address != 0)
            fprintf(stderr, "\nFree_Mid; count = %10d", --g_allocCountMid);
#endif
        if (address == 0)
            return;
        VirtualFree(address, 0, MEM_RELEASE);
    }

#ifndef MEM_LARGE_PAGES
#undef _7ZIP_LARGE_PAGES
#endif

#ifdef _7ZIP_LARGE_PAGES
    SIZE_T g_LargePageSize = 0;
    typedef SIZE_T(WINAPI *GetLargePageMinimumP)();
#endif

    void SetLargePageSize()
    {
#ifdef _7ZIP_LARGE_PAGES
        SIZE_T size = 0;
        GetLargePageMinimumP largePageMinimum = (GetLargePageMinimumP)
            GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetLargePageMinimum");
        if (largePageMinimum == 0)
            return;
        size = largePageMinimum();
        if (size == 0 || (size & (size - 1)) != 0)
            return;
        g_LargePageSize = size;
#endif
    }

    void *BigAlloc(size_t size)
    {
        if (size == 0)
            return 0;
#ifdef _SZ_ALLOC_DEBUG
        fprintf(stderr, "\nAlloc_Big %10d bytes;  count = %10d", size, g_allocCountBig++);
#endif

#ifdef _7ZIP_LARGE_PAGES
        if (g_LargePageSize != 0 && g_LargePageSize <= (1 << 30) && size >= (1 << 18))
        {
            void *res = VirtualAlloc(0, (size + g_LargePageSize - 1) & (~(g_LargePageSize - 1)),
                                     MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
            if (res != 0)
                return res;
        }
#endif
        return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
    }

    void BigFree(void *address)
    {
#ifdef _SZ_ALLOC_DEBUG
        if (address != 0)
            fprintf(stderr, "\nFree_Big; count = %10d", --g_allocCountBig);
#endif

        if (address == 0)
            return;
        VirtualFree(address, 0, MEM_RELEASE);
    }

#endif
}
