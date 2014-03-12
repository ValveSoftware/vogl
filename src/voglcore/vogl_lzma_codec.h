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

// File: vogl_lzma_codec.h
#pragma once

#include "vogl_core.h"
#include "vogl_packed_uint.h"

namespace vogl
{
    class lzma_codec
    {
    public:
        lzma_codec();
        ~lzma_codec();

        // Always available, because we're statically linking in lzmalib now vs. dynamically loading the DLL.
        bool is_initialized() const
        {
            return true;
        }

        bool pack(const void *p, uint n, vogl::vector<uint8> &buf);

        bool unpack(const void *p, uint n, vogl::vector<uint8> &buf);

    private:
        typedef int(VOGL_STDCALL *LzmaCompressFuncPtr)(unsigned char *dest, size_t *destLen, const unsigned char *src, size_t srcLen,
                                                         unsigned char *outProps, size_t *outPropsSize, /* *outPropsSize must be = 5 */
                                                         int level,                                     /* 0 <= level <= 9, default = 5 */
                                                         unsigned dictSize,                             /* default = (1 << 24) */
                                                         int lc,                                        /* 0 <= lc <= 8, default = 3  */
                                                         int lp,                                        /* 0 <= lp <= 4, default = 0  */
                                                         int pb,                                        /* 0 <= pb <= 4, default = 2  */
                                                         int fb,                                        /* 5 <= fb <= 273, default = 32 */
                                                         int numThreads                                 /* 1 or 2, default = 2 */
                                                         );

        typedef int(VOGL_STDCALL *LzmaUncompressFuncPtr)(unsigned char *dest, size_t *destLen, const unsigned char *src, size_t *srcLen,
                                                           const unsigned char *props, size_t propsSize);

        LzmaCompressFuncPtr m_pCompress;
        LzmaUncompressFuncPtr m_pUncompress;

        enum
        {
            cLZMAPropsSize = 5
        };

#pragma pack(push)
#pragma pack(1)
        struct header
        {
            enum
            {
                cSig = 'L' | ('0' << 8),
                cChecksumSkipBytes = 3
            };
            packed_uint<2> m_sig;
            uint8 m_checksum;

            uint8 m_lzma_props[cLZMAPropsSize];

            packed_uint<4> m_comp_size;
            packed_uint<4> m_uncomp_size;

            packed_uint<4> m_adler32;
        };
#pragma pack(pop)
    };

} // namespace vogl
