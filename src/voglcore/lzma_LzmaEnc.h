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

/*  LzmaEnc.h -- LZMA Encoder
2008-10-04 : Igor Pavlov : Public domain */

#ifndef __LZMAENC_H
#define __LZMAENC_H

#include "lzma_Types.h"

namespace vogl
{

#define LZMA_PROPS_SIZE 5

    typedef struct _CLzmaEncProps
    {
        int level;             /*  0 <= level <= 9 */
        UInt32 dictSize;       /* (1 << 12) <= dictSize <= (1 << 27) for 32-bit version
                      (1 << 12) <= dictSize <= (1 << 30) for 64-bit version
                       default = (1 << 24) */
        int lc;                /* 0 <= lc <= 8, default = 3 */
        int lp;                /* 0 <= lp <= 4, default = 0 */
        int pb;                /* 0 <= pb <= 4, default = 2 */
        int algo;              /* 0 - fast, 1 - normal, default = 1 */
        int fb;                /* 5 <= fb <= 273, default = 32 */
        int btMode;            /* 0 - hashChain Mode, 1 - binTree mode - normal, default = 1 */
        int numHashBytes;      /* 2, 3 or 4, default = 4 */
        UInt32 mc;             /* 1 <= mc <= (1 << 30), default = 32 */
        unsigned writeEndMark; /* 0 - do not write EOPM, 1 - write EOPM, default = 0 */
        int numThreads;        /* 1 or 2, default = 2 */
    } CLzmaEncProps;

    void LzmaEncProps_Init(CLzmaEncProps *p);
    void LzmaEncProps_Normalize(CLzmaEncProps *p);
    UInt32 LzmaEncProps_GetDictSize(const CLzmaEncProps *props2);

    /* ---------- CLzmaEncHandle Interface ---------- */

    /* LzmaEnc_* functions can return the following exit codes:
Returns:
  SZ_OK           - OK
  SZ_ERROR_MEM    - Memory allocation error
  SZ_ERROR_PARAM  - Incorrect paramater in props
  SZ_ERROR_WRITE  - Write callback error.
  SZ_ERROR_PROGRESS - some break from progress callback
  SZ_ERROR_THREAD - errors in multithreading functions (only for Mt version)
*/

    typedef void *CLzmaEncHandle;

    CLzmaEncHandle LzmaEnc_Create(ISzAlloc *alloc);
    void LzmaEnc_Destroy(CLzmaEncHandle p, ISzAlloc *alloc, ISzAlloc *allocBig);
    SRes LzmaEnc_SetProps(CLzmaEncHandle p, const CLzmaEncProps *props);
    SRes LzmaEnc_WriteProperties(CLzmaEncHandle p, Byte *properties, SizeT *size);
    SRes LzmaEnc_Encode(CLzmaEncHandle p, ISeqOutStream *outStream, ISeqInStream *inStream,
                        ICompressProgress *progress, ISzAlloc *alloc, ISzAlloc *allocBig);
    SRes LzmaEnc_MemEncode(CLzmaEncHandle p, Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
                           int writeEndMark, ICompressProgress *progress, ISzAlloc *alloc, ISzAlloc *allocBig);

    /* ---------- One Call Interface ---------- */

    /* LzmaEncode
Return code:
  SZ_OK               - OK
  SZ_ERROR_MEM        - Memory allocation error
  SZ_ERROR_PARAM      - Incorrect paramater
  SZ_ERROR_OUTPUT_EOF - output buffer overflow
  SZ_ERROR_THREAD     - errors in multithreading functions (only for Mt version)
*/

    SRes LzmaEncode(Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen,
                    const CLzmaEncProps *props, Byte *propsEncoded, SizeT *propsSize, int writeEndMark,
                    ICompressProgress *progress, ISzAlloc *alloc, ISzAlloc *allocBig);
}

#endif
