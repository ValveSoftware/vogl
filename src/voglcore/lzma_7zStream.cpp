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

/* 7zStream.c -- 7z Stream functions
2008-11-23 : Igor Pavlov : Public domain */
#include "vogl_core.h"
#include <string.h>

#include "lzma_Types.h"

namespace vogl
{

    SRes SeqInStream_Read2(ISeqInStream *stream, void *buf, size_t size, SRes errorType)
    {
        while (size != 0)
        {
            size_t processed = size;
            RINOK(stream->Read(stream, buf, &processed));
            if (processed == 0)
                return errorType;
            buf = (void *)((Byte *)buf + processed);
            size -= processed;
        }
        return SZ_OK;
    }

    SRes SeqInStream_Read(ISeqInStream *stream, void *buf, size_t size)
    {
        return SeqInStream_Read2(stream, buf, size, SZ_ERROR_INPUT_EOF);
    }

    SRes SeqInStream_ReadByte(ISeqInStream *stream, Byte *buf)
    {
        size_t processed = 1;
        RINOK(stream->Read(stream, buf, &processed));
        return (processed == 1) ? SZ_OK : SZ_ERROR_INPUT_EOF;
    }

    SRes LookInStream_SeekTo(ILookInStream *stream, UInt64 offset)
    {
        Int64 t = offset;
        return stream->Seek(stream, &t, SZ_SEEK_SET);
    }

    SRes LookInStream_LookRead(ILookInStream *stream, void *buf, size_t *size)
    {
        void *lookBuf;
        if (*size == 0)
            return SZ_OK;
        RINOK(stream->Look(stream, &lookBuf, size));
        memcpy(buf, lookBuf, *size);
        return stream->Skip(stream, *size);
    }

    SRes LookInStream_Read2(ILookInStream *stream, void *buf, size_t size, SRes errorType)
    {
        while (size != 0)
        {
            size_t processed = size;
            RINOK(stream->Read(stream, buf, &processed));
            if (processed == 0)
                return errorType;
            buf = (void *)((Byte *)buf + processed);
            size -= processed;
        }
        return SZ_OK;
    }

    SRes LookInStream_Read(ILookInStream *stream, void *buf, size_t size)
    {
        return LookInStream_Read2(stream, buf, size, SZ_ERROR_INPUT_EOF);
    }

    static SRes LookToRead_Look_Lookahead(void *pp, void **buf, size_t *size)
    {
        SRes res = SZ_OK;
        CLookToRead *p = (CLookToRead *)pp;
        size_t size2 = p->size - p->pos;
        if (size2 == 0 && *size > 0)
        {
            p->pos = 0;
            size2 = LookToRead_BUF_SIZE;
            res = p->realStream->Read(p->realStream, p->buf, &size2);
            p->size = size2;
        }
        if (size2 < *size)
            *size = size2;
        *buf = p->buf + p->pos;
        return res;
    }

    static SRes LookToRead_Look_Exact(void *pp, void **buf, size_t *size)
    {
        SRes res = SZ_OK;
        CLookToRead *p = (CLookToRead *)pp;
        size_t size2 = p->size - p->pos;
        if (size2 == 0 && *size > 0)
        {
            p->pos = 0;
            if (*size > LookToRead_BUF_SIZE)
                *size = LookToRead_BUF_SIZE;
            res = p->realStream->Read(p->realStream, p->buf, size);
            size2 = p->size = *size;
        }
        if (size2 < *size)
            *size = size2;
        *buf = p->buf + p->pos;
        return res;
    }

    static SRes LookToRead_Skip(void *pp, size_t offset)
    {
        CLookToRead *p = (CLookToRead *)pp;
        p->pos += offset;
        return SZ_OK;
    }

    static SRes LookToRead_Read(void *pp, void *buf, size_t *size)
    {
        CLookToRead *p = (CLookToRead *)pp;
        size_t rem = p->size - p->pos;
        if (rem == 0)
            return p->realStream->Read(p->realStream, buf, size);
        if (rem > *size)
            rem = *size;
        memcpy(buf, p->buf + p->pos, rem);
        p->pos += rem;
        *size = rem;
        return SZ_OK;
    }

    static SRes LookToRead_Seek(void *pp, Int64 *pos, ESzSeek origin)
    {
        CLookToRead *p = (CLookToRead *)pp;
        p->pos = p->size = 0;
        return p->realStream->Seek(p->realStream, pos, origin);
    }

    void LookToRead_CreateVTable(CLookToRead *p, int lookahead)
    {
        p->s.Look = lookahead ? LookToRead_Look_Lookahead : LookToRead_Look_Exact;
        p->s.Skip = LookToRead_Skip;
        p->s.Read = LookToRead_Read;
        p->s.Seek = LookToRead_Seek;
    }

    void LookToRead_Init(CLookToRead *p)
    {
        p->pos = p->size = 0;
    }

    static SRes SecToLook_Read(void *pp, void *buf, size_t *size)
    {
        CSecToLook *p = (CSecToLook *)pp;
        return LookInStream_LookRead(p->realStream, buf, size);
    }

    void SecToLook_CreateVTable(CSecToLook *p)
    {
        p->s.Read = SecToLook_Read;
    }

    static SRes SecToRead_Read(void *pp, void *buf, size_t *size)
    {
        CSecToRead *p = (CSecToRead *)pp;
        return p->realStream->Read(p->realStream, buf, size);
    }

    void SecToRead_CreateVTable(CSecToRead *p)
    {
        p->s.Read = SecToRead_Read;
    }
}
