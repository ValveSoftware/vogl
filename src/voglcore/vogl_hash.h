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

// File: vogl_hash.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    extern uint64_t g_crc64_table[256];

    const uint64_t CRC64_INIT = 0;
    void crc64_init();
    uint64_t calc_crc64(uint64_t crc, const uint8_t *buf, size_t size);

    uint64_t calc_sum64(const uint8_t *buf, size_t size, uint32_t shift_amount = 0);

    uint32_t fast_hash(const void *p, int len);

    template <typename T>
    inline uint32_t fast_hash_obj(const T &obj)
    {
        return fast_hash(&obj, sizeof(obj));
    }

    // 4-byte integer hash, full avalanche
    inline uint32_t bitmix32c(uint32_t a)
    {
        a = (a + 0x7ed55d16) + (a << 12);
        a = (a ^ 0xc761c23c) ^ (a >> 19);
        a = (a + 0x165667b1) + (a << 5);
        a = (a + 0xd3a2646c) ^ (a << 9);
        a = (a + 0xfd7046c5) + (a << 3);
        a = (a ^ 0xb55a4f09) ^ (a >> 16);
        return a;
    }

    // 4-byte integer hash, full avalanche, no constants
    inline uint32_t bitmix32(uint32_t a)
    {
        a -= (a << 6);
        a ^= (a >> 17);
        a -= (a << 9);
        a ^= (a << 4);
        a -= (a << 3);
        a ^= (a << 10);
        a ^= (a >> 15);
        return a;
    }

    // Derived from: http://www.altdevblogaday.com/2011/10/27/quasi-compile-time-string-hashing/
    // Also see: http://www.isthe.com/chongo/tech/comp/fnv/#xor-fold
    class string_hash
    {
    public:
        inline string_hash()
            : m_hash(0)
        {
        }
        inline string_hash(uint32_t hash)
            : m_hash(hash)
        {
        }
        inline string_hash(const string_hash &other)
            : m_hash(other.m_hash)
        {
        }
        inline string_hash &operator=(const string_hash &rhs)
        {
            m_hash = rhs.m_hash;
            return *this;
        }

        template <uint32_t N>
        inline string_hash(const char (&str)[N])
            : m_hash(fnv_hash<N, N>::hash(str))
        {
        }

        template <uint32_t N>
        inline bool operator==(const char (&str)[N]) const
        {
            return m_hash == fnv_hash<N, N>::hash(str);
        }

        template <uint32_t N>
        inline bool operator!=(const char (&str)[N]) const
        {
            return m_hash != fnv_hash<N, N>::hash(str);
        }

        inline bool operator==(const string_hash &other) const
        {
            return m_hash == other.m_hash;
        }
        inline bool operator!=(const string_hash &other) const
        {
            return m_hash != other.m_hash;
        }

        struct const_char_ptr
        {
            inline const_char_ptr(const char *pStr)
                : m_pStr(pStr)
            {
            }
            const char *m_pStr;
        };
        inline string_hash(const const_char_ptr &str)
        {
            init();
            finalize(str.m_pStr);
        }
        inline string_hash(const const_char_ptr &str, size_t len)
        {
            init();
            finalize(str.m_pStr, len);
        }

        inline string_hash(char *pStr)
        {
            init();
            finalize((const char *)pStr);
        }
        inline string_hash(char *pStr, size_t len)
        {
            init();
            finalize((const char *)pStr, len);
        }

        inline uint32_t get_hash() const
        {
            return m_hash;
        }
        inline void set_hash(uint32_t hash)
        {
            m_hash = hash;
        }

        inline operator size_t() const
        {
            return get_hash();
        }

        inline uint32_t set(const char *pStr, size_t len)
        {
            init();
            return finalize(pStr, len);
        }

        inline uint32_t set(const char *pStr)
        {
            init();
            return finalize(pStr);
        }

        inline void init()
        {
            m_hash = 2166136261U;
        }

        inline uint32_t finalize(const char *pStr, size_t len)
        {
            update(pStr, len);
            m_hash *= 16777619u;
            return m_hash;
        }

        inline uint32_t finalize(const char *pStr)
        {
            return finalize(pStr, strlen(pStr));
        }

        inline uint32_t update(const char *pStr, size_t len)
        {
            uint32_t hash = m_hash;
            for (const char *pEnd = pStr + len; pStr != pEnd; ++pStr)
            {
                hash ^= *pStr;
                hash *= 16777619u;
            }
            m_hash = hash;
            return hash;
        }

        inline uint32_t update(const char *pStr)
        {
            return update(pStr, strlen(pStr));
        }

    private:
        uint32_t m_hash;

        template <uint32_t N, uint32_t I>
        struct fnv_hash
        {
            inline static uint32_t hash(const char (&str)[N])
            {
                return (fnv_hash<N, I - 1>::hash(str) ^ str[I - 1]) * 16777619U;
            }
        };

        template <uint32_t N>
        struct fnv_hash<N, 1>
        {
            inline static uint32_t hash(const char (&str)[N])
            {
                return (2166136261U ^ str[0]) * 16777619U;
            }
        };
    };

    const char *find_well_known_string_hash(const string_hash &hash);

} // namespace vogl
