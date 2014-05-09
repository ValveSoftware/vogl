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

// File: vogl_hash.cpp
// See Paul Hsieh's page at: http://www.azillionmonkeys.com/qed/hash.html
// Also see http://www.concentric.net/~Ttwang/tech/inthash.htm,
// http://burtleburtle.net/bob/hash/integer.html
#include "vogl_core.h"

#undef get16bits
#if (defined(COMPILER_GCCLIKE) && defined(__i386__)) || defined(__WATCOMC__) || defined(COMPILER_MSVC) || defined(__BORLANDC__) || defined(__TURBOC__)
#define get16bits(d) (*((const uint16_t *)(d)))
#endif

#if !defined(get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8) + (uint32_t)(((const uint8_t *)(d))[0]))
#endif

namespace vogl
{
    struct well_known_hash_t
    {
        const char *m_pStr;
        string_hash m_hash;
    };

    const char *find_well_known_string_hash(const string_hash &hash)
    {
#define DEFINE_WELL_KNOWN_STRING_HASH(_x) { _x, string_hash(_x) }
        static const well_known_hash_t s_well_known_string_hashes[] =
        {
            DEFINE_WELL_KNOWN_STRING_HASH("width"),
            DEFINE_WELL_KNOWN_STRING_HASH("height"),
            DEFINE_WELL_KNOWN_STRING_HASH("indices"),
            DEFINE_WELL_KNOWN_STRING_HASH("win_width"),
            DEFINE_WELL_KNOWN_STRING_HASH("win_height"),
            DEFINE_WELL_KNOWN_STRING_HASH("active_attributes"),
            DEFINE_WELL_KNOWN_STRING_HASH("active_uniforms"),
            DEFINE_WELL_KNOWN_STRING_HASH("active_uniform_blocks"),
            DEFINE_WELL_KNOWN_STRING_HASH("link_status"),
            DEFINE_WELL_KNOWN_STRING_HASH("map_access"),
            DEFINE_WELL_KNOWN_STRING_HASH("map_range"),
            DEFINE_WELL_KNOWN_STRING_HASH("data"),
            DEFINE_WELL_KNOWN_STRING_HASH("flushed_ranges"),
            DEFINE_WELL_KNOWN_STRING_HASH("explicit_flush"),
            DEFINE_WELL_KNOWN_STRING_HASH("writable_map"),
            DEFINE_WELL_KNOWN_STRING_HASH("start"),
            DEFINE_WELL_KNOWN_STRING_HASH("end"),
            DEFINE_WELL_KNOWN_STRING_HASH("first_vertex_ofs"),
            DEFINE_WELL_KNOWN_STRING_HASH("viewport_x"),
            DEFINE_WELL_KNOWN_STRING_HASH("viewport_y"),
            DEFINE_WELL_KNOWN_STRING_HASH("viewport_width"),
            DEFINE_WELL_KNOWN_STRING_HASH("viewport_height"),
            DEFINE_WELL_KNOWN_STRING_HASH("func_id"),
        };
        const uint32_t s_num_well_known_string_hashes = VOGL_ARRAY_SIZE(s_well_known_string_hashes);

        for (uint32_t i = 0; i < s_num_well_known_string_hashes; i++)
            if (s_well_known_string_hashes[i].m_hash == hash)
                return s_well_known_string_hashes[i].m_pStr;
        return NULL;
    }

    uint32_t fast_hash(const void *p, int len)
    {
        const char *data = static_cast<const char *>(p);

        uint32_t hash = len, tmp;
        int rem;

        if (len <= 0 || data == NULL)
            return 0;

        rem = len & 3;
        len >>= 2;

        /* Main loop */
        for (; len > 0; len--)
        {
            hash += get16bits(data);
            tmp = (get16bits(data + 2) << 11) ^ hash;
            hash = (hash << 16) ^ tmp;
            data += 2 * sizeof(uint16_t);
            hash += hash >> 11;
        }

        /* Handle end cases */
        switch (rem)
        {
            case 3:
                hash += get16bits(data);
                hash ^= hash << 16;
                hash ^= data[sizeof(uint16_t)] << 18;
                hash += hash >> 11;
                break;
            case 2:
                hash += get16bits(data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
            case 1:
                hash += *data;
                hash ^= hash << 10;
                hash += hash >> 1;
        }

        /* Force "avalanching" of final 127 bits */
        hash ^= hash << 3;
        hash += hash >> 5;
        hash ^= hash << 4;
        hash += hash >> 17;
        hash ^= hash << 25;
        hash += hash >> 6;

        return hash;
    }

    // Public domain code originally from http://svn.r-project.org/R/trunk/src/extra/xz/check/crc64_small.c
    uint64_t g_crc64_table[256];

    void crc64_init()
    {
        static const uint64_t poly64 = 0xC96C5795D7870F42ULL;

        for (size_t b = 0; b < 256; ++b)
        {
            uint64_t r = b;
            for (size_t i = 0; i < 8; ++i)
            {
                if (r & 1)
                    r = (r >> 1) ^ poly64;
                else
                    r >>= 1;
            }

            g_crc64_table[b] = r;
        }

        return;
    }

    uint64_t calc_crc64(uint64_t crc, const uint8_t *buf, size_t size)
    {
        if (!g_crc64_table[0])
            crc64_init();

        crc = ~crc;

        while (size != 0)
        {
            crc = g_crc64_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
            --size;
        }

        return ~crc;
    }

    uint64_t calc_sum64(const uint8_t *buf, size_t size, uint32_t shift_amount)
    {
        uint64_t sum = 0;

        while (size != 0)
        {
            uint32_t c = *buf++;
            c >>= shift_amount;
            sum += c;
            --size;
        }

        return sum;
    }

} // namespace vogl
