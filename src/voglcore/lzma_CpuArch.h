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

/* CpuArch.h
2008-08-05
Igor Pavlov
Public domain */

#ifndef __CPUARCH_H
#define __CPUARCH_H

/*
LITTLE_ENDIAN_UNALIGN means:
  1) CPU is LITTLE_ENDIAN
  2) it's allowed to make unaligned memory accesses
if LITTLE_ENDIAN_UNALIGN is not defined, it means that we don't know
about these properties of platform.
*/

#if defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) || defined(__i386__) || defined(__x86_64__)
#define LITTLE_ENDIAN_UNALIGN
#endif

#ifdef LITTLE_ENDIAN_UNALIGN

#define GetUi16(p) (*(const UInt16 *)(p))
#define GetUi32(p) (*(const UInt32 *)(p))
#define GetUi64(p) (*(const UInt64 *)(p))
#define SetUi32(p, d) *(UInt32 *)(p) = (d);

#else

#define GetUi16(p) (((const Byte *)(p))[0] | ((UInt16)((const Byte *)(p))[1] << 8))

#define GetUi32(p) (                         \
    ((const Byte *)(p))[0] |                 \
    ((UInt32)((const Byte *)(p))[1] << 8) |  \
    ((UInt32)((const Byte *)(p))[2] << 16) | \
    ((UInt32)((const Byte *)(p))[3] << 24))

#define GetUi64(p) (GetUi32(p) | ((UInt64)GetUi32(((const Byte *)(p)) + 4) << 32))

#define SetUi32(p, d)                         \
    {                                         \
        UInt32 _x_ = (d);                     \
        ((Byte *)(p))[0] = (Byte)_x_;         \
        ((Byte *)(p))[1] = (Byte)(_x_ >> 8);  \
        ((Byte *)(p))[2] = (Byte)(_x_ >> 16); \
        ((Byte *)(p))[3] = (Byte)(_x_ >> 24); \
    }

#endif

#if defined(LITTLE_ENDIAN_UNALIGN) && defined(_WIN64) && (_MSC_VER >= 1300)

#pragma intrinsic(_byteswap_ulong)
#pragma intrinsic(_byteswap_uint64)
#define GetBe32(p) _byteswap_ulong(*(const UInt32 *)(const Byte *)(p))
#define GetBe64(p) _byteswap_uint64(*(const UInt64 *)(const Byte *)(p))

#else

#define GetBe32(p) (                         \
    ((UInt32)((const Byte *)(p))[0] << 24) | \
    ((UInt32)((const Byte *)(p))[1] << 16) | \
    ((UInt32)((const Byte *)(p))[2] << 8) |  \
    ((const Byte *)(p))[3])

#define GetBe64(p) (((UInt64)GetBe32(p) << 32) | GetBe32(((const Byte *)(p)) + 4))

#endif

#define GetBe16(p) (((UInt16)((const Byte *)(p))[0] << 8) | ((const Byte *)(p))[1])

#endif
