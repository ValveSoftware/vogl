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

// File: vogl_checksum.cpp
#include "vogl_core.h"

namespace vogl
{
    // From the public domain stb.h header.
    uint32_t adler32(const void *pBuf, size_t buflen, uint32_t adler32)
    {
        const uint8_t *buffer = static_cast<const uint8_t *>(pBuf);

        const unsigned long ADLER_MOD = 65521;
        unsigned long s1 = adler32 & 0xffff, s2 = adler32 >> 16;
        size_t blocklen;
        unsigned long i;

        blocklen = buflen % 5552;
        while (buflen)
        {
            for (i = 0; i + 7 < blocklen; i += 8)
            {
                s1 += buffer[0], s2 += s1;
                s1 += buffer[1], s2 += s1;
                s1 += buffer[2], s2 += s1;
                s1 += buffer[3], s2 += s1;
                s1 += buffer[4], s2 += s1;
                s1 += buffer[5], s2 += s1;
                s1 += buffer[6], s2 += s1;
                s1 += buffer[7], s2 += s1;

                buffer += 8;
            }

            for (; i < blocklen; ++i)
                s1 += *buffer++, s2 += s1;

            s1 %= ADLER_MOD, s2 %= ADLER_MOD;
            buflen -= blocklen;
            blocklen = 5552;
        }
        return static_cast<uint32_t>((s2 << 16) + s1);
    }

    uint16_t crc16(const void *pBuf, size_t len, uint16_t crc)
    {
        crc = ~crc;

        const uint8_t *p = reinterpret_cast<const uint8_t *>(pBuf);
        while (len)
        {
            const uint16_t q = *p++ ^ (crc >> 8);
            crc <<= 8U;
            uint16_t r = (q >> 4) ^ q;
            crc ^= r;
            r <<= 5U;
            crc ^= r;
            r <<= 7U;
            crc ^= r;
            len--;
        }

        return static_cast<uint16_t>(~crc);
    }

} // namespace vogl
