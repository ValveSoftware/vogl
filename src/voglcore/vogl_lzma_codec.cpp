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

// File: vogl_lzma_codec.cpp
#include "vogl_core.h"
#include "vogl_lzma_codec.h"
#include "vogl_strutils.h"
#include "vogl_checksum.h"
#include "lzma_LzmaLib.h"
#include "vogl_threading.h"

namespace vogl
{
    lzma_codec::lzma_codec()
        : m_pCompress(LzmaCompress),
          m_pUncompress(LzmaUncompress)
    {
        VOGL_ASSUME(cLZMAPropsSize == LZMA_PROPS_SIZE);
    }

    lzma_codec::~lzma_codec()
    {
    }

    bool lzma_codec::pack(const void *p, uint n, vogl::vector<uint8> &buf)
    {
        if (n > 1024U * 1024U * 1024U)
            return false;

        uint max_comp_size = n + math::maximum<uint>(128, n >> 8);
        buf.resize(sizeof(header) + max_comp_size);

        header *pHDR = reinterpret_cast<header *>(&buf[0]);
        uint8 *pComp_data = &buf[sizeof(header)];

        utils::zero_object(*pHDR);

        pHDR->m_uncomp_size = n;
        pHDR->m_adler32 = adler32(p, n);

        if (n)
        {
            size_t destLen = 0;
            size_t outPropsSize = 0;
            int status = SZ_ERROR_INPUT_EOF;

            for (uint trial = 0; trial < 3; trial++)
            {
                destLen = max_comp_size;
                outPropsSize = cLZMAPropsSize;

                status = (*m_pCompress)(pComp_data, &destLen, reinterpret_cast<const unsigned char *>(p), n,
                                        pHDR->m_lzma_props, &outPropsSize,
                                        -1, /* 0 <= level <= 9, default = 5 */
                                        0,  /* default = (1 << 24) */
                                        -1, /* 0 <= lc <= 8, default = 3  */
                                        -1, /* 0 <= lp <= 4, default = 0  */
                                        -1, /* 0 <= pb <= 4, default = 2  */
                                        -1, /* 5 <= fb <= 273, default = 32 */
#if defined(PLATFORM_WINDOWS)
                                        (g_number_of_processors > 1) ? 2 : 1
#else
                                        1
#endif
                                        );

                if (status != SZ_ERROR_OUTPUT_EOF)
                    break;

                max_comp_size += ((n + 1) / 2);
                buf.resize(sizeof(header) + max_comp_size);
                pHDR = reinterpret_cast<header *>(&buf[0]);
                pComp_data = &buf[sizeof(header)];
            }

            if (status != SZ_OK)
            {
                buf.clear();
                return false;
            }

            pHDR->m_comp_size = static_cast<uint>(destLen);

            buf.resize(VOGL_SIZEOF_U32(header) + static_cast<uint32>(destLen));
        }

        pHDR->m_sig = header::cSig;
        pHDR->m_checksum = static_cast<uint8>(adler32((uint8 *)pHDR + header::cChecksumSkipBytes, sizeof(header) - header::cChecksumSkipBytes));

        return true;
    }

    bool lzma_codec::unpack(const void *p, uint n, vogl::vector<uint8> &buf)
    {
        buf.resize(0);

        if (n < sizeof(header))
            return false;

        const header &hdr = *static_cast<const header *>(p);
        if (hdr.m_sig != header::cSig)
            return false;

        if (static_cast<uint8>(adler32((const uint8 *)&hdr + header::cChecksumSkipBytes, sizeof(hdr) - header::cChecksumSkipBytes)) != hdr.m_checksum)
            return false;

        if (!hdr.m_uncomp_size)
            return true;

        if (!hdr.m_comp_size)
            return false;

        if (hdr.m_uncomp_size > 1024U * 1024U * 1024U)
            return false;

        if (!buf.try_resize(hdr.m_uncomp_size))
            return false;

        const uint8 *pComp_data = static_cast<const uint8 *>(p) + sizeof(header);
        size_t srcLen = n - sizeof(header);
        if (srcLen < hdr.m_comp_size)
            return false;

        size_t destLen = hdr.m_uncomp_size;

        int status = (*m_pUncompress)(&buf[0], &destLen, pComp_data, &srcLen,
                                      hdr.m_lzma_props, cLZMAPropsSize);

        if ((status != SZ_OK) || (destLen != hdr.m_uncomp_size))
        {
            buf.clear();
            return false;
        }

        if (adler32(&buf[0], buf.size()) != hdr.m_adler32)
        {
            buf.clear();
            return false;
        }

        return true;
    }

} // namespace vogl
