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

/* 7zCrc.c -- CRC32 calculation
2008-08-05
Igor Pavlov
Public domain */
#include "vogl_core.h"

#include "lzma_7zCrc.h"

namespace vogl
{

#define kCrcPoly 0xEDB88320
    UInt32 g_CrcTable[256];

    void MY_FAST_CALL CrcGenerateTable(void)
    {
        UInt32 i;
        for (i = 0; i < 256; i++)
        {
            UInt32 r = i;
            int j;
            for (j = 0; j < 8; j++)
                r = (r >> 1) ^ (kCrcPoly & ~((r & 1) - 1));
            g_CrcTable[i] = r;
        }
    }

    UInt32 MY_FAST_CALL CrcUpdate(UInt32 v, const void *data, size_t size)
    {
        const Byte *p = (const Byte *)data;
        for (; size > 0; size--, p++)
            v = CRC_UPDATE_BYTE(v, *p);
        return v;
    }

    UInt32 MY_FAST_CALL CrcCalc(const void *data, size_t size)
    {
        return CrcUpdate(CRC_INIT_VAL, data, size) ^ 0xFFFFFFFF;
    }
}
