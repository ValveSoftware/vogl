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

/* 7zBuf.h -- Byte Buffer
2008-10-04 : Igor Pavlov : Public domain */

#ifndef __7Z_BUF_H
#define __7Z_BUF_H

#include "lzma_Types.h"

namespace vogl
{

    typedef struct
    {
        Byte *data;
        size_t size;
    } CBuf;

    void Buf_Init(CBuf *p);
    int Buf_Create(CBuf *p, size_t size, ISzAlloc *alloc);
    void Buf_Free(CBuf *p, ISzAlloc *alloc);

    typedef struct
    {
        Byte *data;
        size_t size;
        size_t pos;
    } CDynBuf;

    void DynBuf_Construct(CDynBuf *p);
    void DynBuf_SeekToBeg(CDynBuf *p);
    int DynBuf_Write(CDynBuf *p, const Byte *buf, size_t size, ISzAlloc *alloc);
    void DynBuf_Free(CDynBuf *p, ISzAlloc *alloc);
}

#endif
