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

/* LzmaLib.c -- LZMA library wrapper
2008-08-05
Igor Pavlov
Public domain */
#include "vogl_core.h"
#include "lzma_LzmaEnc.h"
#include "lzma_LzmaDec.h"
#include "lzma_Alloc.h"
#include "lzma_LzmaLib.h"

namespace vogl
{

    static void *SzAlloc(void *p, size_t size)
    {
        (void)p;
        return MyAlloc(size);
    }
    static void SzFree(void *p, void *address)
    {
        (void)p;
        MyFree(address);
    }
    static ISzAlloc g_Alloc = { SzAlloc, SzFree };

    MY_STDAPI LzmaCompress(unsigned char *dest, size_t *destLen, const unsigned char *src, size_t srcLen,
                           unsigned char *outProps, size_t *outPropsSize,
                           int level,         /* 0 <= level <= 9, default = 5 */
                           unsigned dictSize, /* use (1 << N) or (3 << N). 4 KB < dictSize <= 128 MB */
                           int lc,            /* 0 <= lc <= 8, default = 3  */
                           int lp,            /* 0 <= lp <= 4, default = 0  */
                           int pb,            /* 0 <= pb <= 4, default = 2  */
                           int fb,            /* 5 <= fb <= 273, default = 32 */
                           int numThreads     /* 1 or 2, default = 2 */
                           )
    {
        CLzmaEncProps props;
        LzmaEncProps_Init(&props);
        props.level = level;
        props.dictSize = dictSize;
        props.lc = lc;
        props.lp = lp;
        props.pb = pb;
        props.fb = fb;
        props.numThreads = numThreads;

        return LzmaEncode(dest, destLen, src, srcLen, &props, outProps, outPropsSize, 0,
                          NULL, &g_Alloc, &g_Alloc);
    }

    MY_STDAPI LzmaUncompress(unsigned char *dest, size_t *destLen, const unsigned char *src, size_t *srcLen,
                             const unsigned char *props, size_t propsSize)
    {
        ELzmaStatus status;
        return LzmaDecode(dest, destLen, src, srcLen, props, (unsigned)propsSize, LZMA_FINISH_ANY, &status, &g_Alloc);
    }
}
