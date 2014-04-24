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

/* LzHash.h -- HASH functions for LZ algorithms
2008-10-04 : Igor Pavlov : Public domain */

#pragma once

#define kHash2Size (1 << 10)
#define kHash3Size (1 << 16)
#define kHash4Size (1 << 20)

#define kFix3HashSize (kHash2Size)
#define kFix4HashSize (kHash2Size + kHash3Size)
#define kFix5HashSize (kHash2Size + kHash3Size + kHash4Size)

#define HASH2_CALC hashValue = cur[0] | ((UInt32)cur[1] << 8);

#define HASH3_CALC                                                \
    {                                                             \
        UInt32 temp = p->crc[cur[0]] ^ cur[1];                    \
        hash2Value = temp & (kHash2Size - 1);                     \
        hashValue = (temp ^ ((UInt32)cur[2] << 8)) & p->hashMask; \
    }

#define HASH4_CALC                                                                        \
    {                                                                                     \
        UInt32 temp = p->crc[cur[0]] ^ cur[1];                                            \
        hash2Value = temp & (kHash2Size - 1);                                             \
        hash3Value = (temp ^ ((UInt32)cur[2] << 8)) & (kHash3Size - 1);                   \
        hashValue = (temp ^ ((UInt32)cur[2] << 8) ^ (p->crc[cur[3]] << 5)) & p->hashMask; \
    }

#define HASH5_CALC                                                           \
    {                                                                        \
        UInt32 temp = p->crc[cur[0]] ^ cur[1];                               \
        hash2Value = temp & (kHash2Size - 1);                                \
        hash3Value = (temp ^ ((UInt32)cur[2] << 8)) & (kHash3Size - 1);      \
        hash4Value = (temp ^ ((UInt32)cur[2] << 8) ^ (p->crc[cur[3]] << 5)); \
        hashValue = (hash4Value ^ (p->crc[cur[4]] << 3)) & p->hashMask;      \
        hash4Value &= (kHash4Size - 1);                                      \
    }

/* #define HASH_ZIP_CALC hashValue = ((cur[0] | ((UInt32)cur[1] << 8)) ^ p->crc[cur[2]]) & 0xFFFF; */
#define HASH_ZIP_CALC hashValue = ((cur[2] | ((UInt32)cur[0] << 8)) ^ p->crc[cur[1]]) & 0xFFFF;

#define MT_HASH2_CALC \
    hash2Value = (p->crc[cur[0]] ^ cur[1]) & (kHash2Size - 1);

#define MT_HASH3_CALC                                                   \
    {                                                                   \
        UInt32 temp = p->crc[cur[0]] ^ cur[1];                          \
        hash2Value = temp & (kHash2Size - 1);                           \
        hash3Value = (temp ^ ((UInt32)cur[2] << 8)) & (kHash3Size - 1); \
    }

#define MT_HASH4_CALC                                                                           \
    {                                                                                           \
        UInt32 temp = p->crc[cur[0]] ^ cur[1];                                                  \
        hash2Value = temp & (kHash2Size - 1);                                                   \
        hash3Value = (temp ^ ((UInt32)cur[2] << 8)) & (kHash3Size - 1);                         \
        hash4Value = (temp ^ ((UInt32)cur[2] << 8) ^ (p->crc[cur[3]] << 5)) & (kHash4Size - 1); \
    }

