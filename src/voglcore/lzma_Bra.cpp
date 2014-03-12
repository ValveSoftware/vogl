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

/* Bra.c -- Converters for RISC code
2008-10-04 : Igor Pavlov : Public domain */
#include "vogl_core.h"
#include "lzma_Bra.h"

namespace vogl
{

    SizeT ARM_Convert(Byte *data, SizeT size, UInt32 ip, int encoding)
    {
        SizeT i;
        if (size < 4)
            return 0;
        size -= 4;
        ip += 8;
        for (i = 0; i <= size; i += 4)
        {
            if (data[i + 3] == 0xEB)
            {
                UInt32 dest;
                UInt32 src = ((UInt32)data[i + 2] << 16) | ((UInt32)data[i + 1] << 8) | (data[i + 0]);
                src <<= 2;
                if (encoding)
                    dest = ip + (UInt32)i + src;
                else
                    dest = src - (ip + (UInt32)i);
                dest >>= 2;
                data[i + 2] = (Byte)(dest >> 16);
                data[i + 1] = (Byte)(dest >> 8);
                data[i + 0] = (Byte)dest;
            }
        }
        return i;
    }

    SizeT ARMT_Convert(Byte *data, SizeT size, UInt32 ip, int encoding)
    {
        SizeT i;
        if (size < 4)
            return 0;
        size -= 4;
        ip += 4;
        for (i = 0; i <= size; i += 2)
        {
            if ((data[i + 1] & 0xF8) == 0xF0 &&
                (data[i + 3] & 0xF8) == 0xF8)
            {
                UInt32 dest;
                UInt32 src =
                    (((UInt32)data[i + 1] & 0x7) << 19) |
                    ((UInt32)data[i + 0] << 11) |
                    (((UInt32)data[i + 3] & 0x7) << 8) |
                    (data[i + 2]);

                src <<= 1;
                if (encoding)
                    dest = ip + (UInt32)i + src;
                else
                    dest = src - (ip + (UInt32)i);
                dest >>= 1;

                data[i + 1] = (Byte)(0xF0 | ((dest >> 19) & 0x7));
                data[i + 0] = (Byte)(dest >> 11);
                data[i + 3] = (Byte)(0xF8 | ((dest >> 8) & 0x7));
                data[i + 2] = (Byte)dest;
                i += 2;
            }
        }
        return i;
    }

    SizeT PPC_Convert(Byte *data, SizeT size, UInt32 ip, int encoding)
    {
        SizeT i;
        if (size < 4)
            return 0;
        size -= 4;
        for (i = 0; i <= size; i += 4)
        {
            if ((data[i] >> 2) == 0x12 && (data[i + 3] & 3) == 1)
            {
                UInt32 src = ((UInt32)(data[i + 0] & 3) << 24) |
                             ((UInt32)data[i + 1] << 16) |
                             ((UInt32)data[i + 2] << 8) |
                             ((UInt32)data[i + 3] & (~3));

                UInt32 dest;
                if (encoding)
                    dest = ip + (UInt32)i + src;
                else
                    dest = src - (ip + (UInt32)i);
                data[i + 0] = (Byte)(0x48 | ((dest >> 24) & 0x3));
                data[i + 1] = (Byte)(dest >> 16);
                data[i + 2] = (Byte)(dest >> 8);
                data[i + 3] &= 0x3;
                data[i + 3] |= dest;
            }
        }
        return i;
    }

    SizeT SPARC_Convert(Byte *data, SizeT size, UInt32 ip, int encoding)
    {
        UInt32 i;
        if (size < 4)
            return 0;
        size -= 4;
        for (i = 0; i <= size; i += 4)
        {
            if (((data[i] == 0x40) && ((data[i + 1] & 0xC0) == 0x00)) ||
                ((data[i] == 0x7F) && ((data[i + 1] & 0xC0) == 0xC0)))
            {
                UInt32 src =
                    ((UInt32)data[i + 0] << 24) |
                    ((UInt32)data[i + 1] << 16) |
                    ((UInt32)data[i + 2] << 8) |
                    ((UInt32)data[i + 3]);
                UInt32 dest;

                src <<= 2;
                if (encoding)
                    dest = ip + i + src;
                else
                    dest = src - (ip + i);
                dest >>= 2;

                dest = (((0 - ((dest >> 22) & 1)) << 22) & 0x3FFFFFFF) | (dest & 0x3FFFFF) | 0x40000000;

                data[i + 0] = (Byte)(dest >> 24);
                data[i + 1] = (Byte)(dest >> 16);
                data[i + 2] = (Byte)(dest >> 8);
                data[i + 3] = (Byte)dest;
            }
        }
        return i;
    }
}
