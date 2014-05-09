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

// File: vogl_packed_uint
#pragma once

#include "vogl_core.h"

namespace vogl
{
    // N must range from [1,8]
    template <uint32_t N, bool big_endian = true>
    struct packed_uint
    {
        typedef typename template_if<(N > sizeof(uint32_t)), uint64_t, uint32_t>::result param_type;

        inline packed_uint()
        {
            VOGL_ASSUME((N >= 1) && (N <= 8));
        }

        inline packed_uint(param_type val)
        {
            VOGL_ASSUME((N >= 1) && (N <= 8));
            *this = val;
        }

        inline packed_uint(const packed_uint &other)
        {
            VOGL_ASSUME((N >= 1) && (N <= 8));
            *this = other;
        }

        inline packed_uint &operator=(const packed_uint &rhs)
        {
            if (this != &rhs)
                memcpy(m_buf, rhs.m_buf, sizeof(m_buf));
            return *this;
        }

        inline packed_uint &operator=(param_type val)
        {
            if (big_endian)
            {
                for (uint32_t i = 0; i < N; ++i, val >>= 8U)
                    m_buf[(N - 1) - i] = static_cast<uint8_t>(val);
            }
            else
            {
                for (uint32_t i = 0; i < N; ++i, val >>= 8U)
                    m_buf[i] = static_cast<uint8_t>(val);
            }

            VOGL_ASSERT(!val);

            return *this;
        }

        inline uint64_t get_uint64() const
        {
            uint64_t val = 0;
            for (uint32_t i = 0; i < N; ++i)
            {
                val <<= 8U;
                val |= m_buf[big_endian ? i : ((N - 1) - i)];
            }
            return val;
        }

        inline uint32_t get_uint32() const
        {
            uint32_t val = 0;
            for (uint32_t i = 0; i < N; ++i)
            {
                val <<= 8U;
                val |= m_buf[big_endian ? i : ((N - 1) - i)];
            }
            return val;
        }

        inline uint16_t get_uint16() const
        {
            uint16_t val = 0;
            for (uint32_t i = 0; i < N; ++i)
            {
                val <<= 8U;
                val |= m_buf[big_endian ? i : ((N - 1) - i)];
            }
            return val;
        }

        inline operator param_type() const
        {
            return static_cast<param_type>((sizeof(param_type) <= sizeof(uint32_t)) ? get_uint32() : get_uint64());
        }

        inline packed_uint &byte_swap()
        {
            for (uint32_t i = 0; i < (N / 2); i++)
                std::swap(m_buf[i], m_buf[(N - 1) - i]);
            return *this;
        }

        uint8_t m_buf[N];
    };

    inline void packed_uint_test()
    {
        packed_uint<8, true> q0;
        q0 = 0x123456789ABCDEF0ULL;
        uint64_t k0 = q0;
        VOGL_VERIFY(k0 == 0x123456789ABCDEF0ULL);
        VOGL_VERIFY(q0.m_buf[0] == 0x12);

        packed_uint<8, false> q1;
        q1 = 0x123456789ABCDEF0ULL;
        uint64_t k1 = q1;
        VOGL_VERIFY(k1 == 0x123456789ABCDEF0ULL);
        VOGL_VERIFY(q1.m_buf[0] == 0xF0);

        packed_uint<4, true> q2;
        q2 = 0xDEADBEEF;
        uint32_t k2 = q2;
        VOGL_VERIFY(k2 == 0xDEADBEEF);
        VOGL_VERIFY(q2.m_buf[0] == 0xDE);

        packed_uint<4, false> q3;
        q3 = 0xDEADBEEF;
        uint32_t k3 = q3;
        VOGL_VERIFY(k3 == 0xDEADBEEF);
        VOGL_VERIFY(q3.m_buf[0] == 0xEF);

        packed_uint<3, true> q8;
        q8 = 0xDEADBE;
        uint32_t k8 = q8;
        VOGL_VERIFY(k8 == 0xDEADBE);
        VOGL_VERIFY(q8.m_buf[0] == 0xDE);

        packed_uint<3, false> q9;
        q9 = 0xDEADBE;
        uint32_t k9 = q9;
        VOGL_VERIFY(k9 == 0xDEADBE);
        VOGL_VERIFY(q9.m_buf[0] == 0xBE);

        packed_uint<2, true> q4;
        q4 = 0xDEAD;
        uint32_t k4 = q4;
        VOGL_VERIFY(k4 == 0xDEAD);
        VOGL_VERIFY(q4.m_buf[0] == 0xDE);

        packed_uint<2, false> q5;
        q5 = 0xDEAD;
        uint32_t k5 = q5;
        VOGL_VERIFY(k5 == 0xDEAD);
        VOGL_VERIFY(q5.m_buf[0] == 0xAD);

        packed_uint<1, true> q6;
        q6 = 0xDE;
        uint32_t k6 = q6;
        VOGL_VERIFY(k6 == 0xDE);
        VOGL_VERIFY(q6.m_buf[0] == 0xDE);

        packed_uint<1, false> q7;
        q7 = 0xDE;
        uint32_t k7 = q7;
        VOGL_VERIFY(k7 == 0xDE);
        VOGL_VERIFY(q7.m_buf[0] == 0xDE);

#if 0
	for (uint64_t i = 0; i <= 0xFFFFFFFFU; i++)
	{
		packed_uint<4, true> b(static_cast<uint32_t>(i));
		packed_uint<4, true> l(static_cast<uint32_t>(i));
		VOGL_VERIFY((b == i) && (l == i));
		if ((i & 4095) == 4095) printf("0x%04X\n", i);
	}
#endif
    }

} // namespace vogl
