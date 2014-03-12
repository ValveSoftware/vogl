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

/* 7zBuf.c -- Byte Buffer
2008-03-28
Igor Pavlov
Public domain */
#include "vogl_core.h"

#include "lzma_7zBuf.h"

namespace vogl
{

    void Buf_Init(CBuf *p)
    {
        p->data = 0;
        p->size = 0;
    }

    int Buf_Create(CBuf *p, size_t size, ISzAlloc *alloc)
    {
        p->size = 0;
        if (size == 0)
        {
            p->data = 0;
            return 1;
        }
        p->data = (Byte *)alloc->Alloc(alloc, size);
        if (p->data != 0)
        {
            p->size = size;
            return 1;
        }
        return 0;
    }

    void Buf_Free(CBuf *p, ISzAlloc *alloc)
    {
        alloc->Free(alloc, p->data);
        p->data = 0;
        p->size = 0;
    }
}
