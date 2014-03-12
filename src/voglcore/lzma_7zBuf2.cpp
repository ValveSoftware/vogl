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

/* 7zBuf2.c -- Byte Buffer
2008-10-04 : Igor Pavlov : Public domain */

#include "vogl_core.h"
#include <string.h>
#include "lzma_7zBuf.h"

namespace vogl
{

    void DynBuf_Construct(CDynBuf *p)
    {
        p->data = 0;
        p->size = 0;
        p->pos = 0;
    }

    void DynBuf_SeekToBeg(CDynBuf *p)
    {
        p->pos = 0;
    }

    int DynBuf_Write(CDynBuf *p, const Byte *buf, size_t size, ISzAlloc *alloc)
    {
        if (size > p->size - p->pos)
        {
            size_t newSize = p->pos + size;
            Byte *data;
            newSize += newSize / 4;
            data = (Byte *)alloc->Alloc(alloc, newSize);
            if (data == 0)
                return 0;
            p->size = newSize;
            memcpy(data, p->data, p->pos);
            alloc->Free(alloc, p->data);
            p->data = data;
        }
        memcpy(p->data + p->pos, buf, size);
        p->pos += size;
        return 1;
    }

    void DynBuf_Free(CDynBuf *p, ISzAlloc *alloc)
    {
        alloc->Free(alloc, p->data);
        p->data = 0;
        p->size = 0;
        p->pos = 0;
    }
}
