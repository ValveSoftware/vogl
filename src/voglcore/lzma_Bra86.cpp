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

/* Bra86.c -- Converter for x86 code (BCJ)
2008-10-04 : Igor Pavlov : Public domain */
#include "vogl_core.h"
#include "lzma_Bra.h"

namespace vogl
{

#define Test86MSByte(b) ((b) == 0 || (b) == 0xFF)

    const Byte kMaskToAllowedStatus[8] = { 1, 1, 1, 0, 1, 0, 0, 0 };
    const Byte kMaskToBitNumber[8] = { 0, 1, 2, 2, 3, 3, 3, 3 };

    SizeT x86_Convert(Byte *data, SizeT size, UInt32 ip, UInt32 *state, int encoding)
    {
        SizeT bufferPos = 0, prevPosT;
        UInt32 prevMask = *state & 0x7;
        if (size < 5)
            return 0;
        ip += 5;
        prevPosT = (SizeT)0 - 1;

        for (;;)
        {
            Byte *p = data + bufferPos;
            Byte *limit = data + size - 4;
            for (; p < limit; p++)
                if ((*p & 0xFE) == 0xE8)
                    break;
            bufferPos = (SizeT)(p - data);
            if (p >= limit)
                break;
            prevPosT = bufferPos - prevPosT;
            if (prevPosT > 3)
                prevMask = 0;
            else
            {
                prevMask = (prevMask << ((int)prevPosT - 1)) & 0x7;
                if (prevMask != 0)
                {
                    Byte b = p[4 - kMaskToBitNumber[prevMask]];
                    if (!kMaskToAllowedStatus[prevMask] || Test86MSByte(b))
                    {
                        prevPosT = bufferPos;
                        prevMask = ((prevMask << 1) & 0x7) | 1;
                        bufferPos++;
                        continue;
                    }
                }
            }
            prevPosT = bufferPos;

            if (Test86MSByte(p[4]))
            {
                UInt32 src = ((UInt32)p[4] << 24) | ((UInt32)p[3] << 16) | ((UInt32)p[2] << 8) | ((UInt32)p[1]);
                UInt32 dest;
                for (;;)
                {
                    Byte b;
                    int index;
                    if (encoding)
                        dest = (ip + (UInt32)bufferPos) + src;
                    else
                        dest = src - (ip + (UInt32)bufferPos);
                    if (prevMask == 0)
                        break;
                    index = kMaskToBitNumber[prevMask] * 8;
                    b = (Byte)(dest >> (24 - index));
                    if (!Test86MSByte(b))
                        break;
                    src = dest ^ ((1 << (32 - index)) - 1);
                }
                p[4] = (Byte)(~(((dest >> 24) & 1) - 1));
                p[3] = (Byte)(dest >> 16);
                p[2] = (Byte)(dest >> 8);
                p[1] = (Byte)dest;
                bufferPos += 5;
            }
            else
            {
                prevMask = ((prevMask << 1) & 0x7) | 1;
                bufferPos++;
            }
        }
        prevPosT = bufferPos - prevPosT;
        *state = ((prevPosT > 3) ? 0 : ((prevMask << ((int)prevPosT - 1)) & 0x7));
        return bufferPos;
    }
}
