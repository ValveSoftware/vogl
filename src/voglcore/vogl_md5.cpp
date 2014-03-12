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

/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * (This is a heavily cut-down "BSD license".)
 *
 * This differs from Colin Plumb's older public domain implementation in that
 * no exactly 32-bit integer data type is required (any 32-bit or wider
 * unsigned integer data type will do), there's no compile-time endianness
 * configuration, and the function prototypes match OpenSSL's.  No code from
 * Colin Plumb's implementation has been reused; this comment merely compares
 * the properties of the two independent implementations.
 *
 * The primary goals of this implementation are portability and ease of use.
 * It is meant to be fast, but not as fast as possible.  Some known
 * optimizations are not included to reduce source code size and avoid
 * compile-time configuration.
 */

#include <string.h>

#include "vogl_md5.h"

namespace vogl
{

/*
 * The basic MD5 functions.
 *
 * F and G are optimized compared to their RFC 1321 definitions for
 * architectures that lack an AND-NOT instruction, just like in Colin Plumb's
 * implementation.
 */
#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))

/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s)                           \
    (a) += f((b), (c), (d)) + (x) + (t);                       \
    (a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
    (a) += (b);

/*
 * SET reads 4 input bytes in little-endian byte order and stores them
 * in a properly aligned word in host byte order.
 *
 * The check for little-endian architectures that tolerate unaligned
 * memory accesses is just an optimization.  Nothing will break if it
 * doesn't work.
 */
#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define SET(n) \
    (*(MD5_u32plus *)&ptr[(n) * 4])
#define GET(n) \
    SET(n)
#else
#define SET(n)                                   \
    (ctx->block[(n)] =                           \
         (MD5_u32plus)ptr[(n) * 4] |             \
         ((MD5_u32plus)ptr[(n) * 4 + 1] << 8) |  \
         ((MD5_u32plus)ptr[(n) * 4 + 2] << 16) | \
         ((MD5_u32plus)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
    (ctx->block[(n)])
#endif

    /*
 * This processes one or more 64-byte data blocks, but does NOT update
 * the bit counters.  There are no alignment requirements.
 */
    static const void *body(MD5_CTX *ctx, const void *data, unsigned long size)
    {
        const unsigned char *ptr;
        MD5_u32plus a, b, c, d;
        MD5_u32plus saved_a, saved_b, saved_c, saved_d;

        ptr = static_cast<const unsigned char *>(data);

        a = ctx->a;
        b = ctx->b;
        c = ctx->c;
        d = ctx->d;

        do
        {
            saved_a = a;
            saved_b = b;
            saved_c = c;
            saved_d = d;

            /* Round 1 */
            STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
            STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
            STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
            STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
            STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
            STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
            STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
            STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
            STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
            STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
            STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
            STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
            STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
            STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
            STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
            STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)

            /* Round 2 */
            STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
            STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
            STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
            STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
            STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
            STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
            STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
            STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
            STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
            STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
            STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
            STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
            STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
            STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
            STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
            STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)

            /* Round 3 */
            STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
            STEP(H, d, a, b, c, GET(8), 0x8771f681, 11)
            STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
            STEP(H, b, c, d, a, GET(14), 0xfde5380c, 23)
            STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
            STEP(H, d, a, b, c, GET(4), 0x4bdecfa9, 11)
            STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
            STEP(H, b, c, d, a, GET(10), 0xbebfbc70, 23)
            STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
            STEP(H, d, a, b, c, GET(0), 0xeaa127fa, 11)
            STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
            STEP(H, b, c, d, a, GET(6), 0x04881d05, 23)
            STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
            STEP(H, d, a, b, c, GET(12), 0xe6db99e5, 11)
            STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
            STEP(H, b, c, d, a, GET(2), 0xc4ac5665, 23)

            /* Round 4 */
            STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
            STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
            STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
            STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
            STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
            STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
            STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
            STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
            STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
            STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
            STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
            STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
            STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
            STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
            STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
            STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

            a += saved_a;
            b += saved_b;
            c += saved_c;
            d += saved_d;

            ptr += 64;
        } while (size -= 64);

        ctx->a = a;
        ctx->b = b;
        ctx->c = c;
        ctx->d = d;

        return ptr;
    }

    void MD5_Init(MD5_CTX *ctx)
    {
        ctx->a = 0x67452301;
        ctx->b = 0xefcdab89;
        ctx->c = 0x98badcfe;
        ctx->d = 0x10325476;

        ctx->lo = 0;
        ctx->hi = 0;
    }

    void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size)
    {
        MD5_u32plus saved_lo;
        unsigned long used, free;

        saved_lo = ctx->lo;
        if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo)
            ctx->hi++;
        ctx->hi += size >> 29;

        used = saved_lo & 0x3f;

        if (used)
        {
            free = 64 - used;

            if (size < free)
            {
                memcpy(&ctx->buffer[used], data, size);
                return;
            }

            memcpy(&ctx->buffer[used], data, free);
            data = (unsigned char *)data + free;
            size -= free;
            body(ctx, ctx->buffer, 64);
        }

        if (size >= 64)
        {
            data = body(ctx, data, size & ~(unsigned long)0x3f);
            size &= 0x3f;
        }

        memcpy(ctx->buffer, data, size);
    }

    void MD5_Final(unsigned char *result, MD5_CTX *ctx)
    {
        unsigned long used, free;

        used = ctx->lo & 0x3f;

        ctx->buffer[used++] = 0x80;

        free = 64 - used;

        if (free < 8)
        {
            memset(&ctx->buffer[used], 0, free);
            body(ctx, ctx->buffer, 64);
            used = 0;
            free = 64;
        }

        memset(&ctx->buffer[used], 0, free - 8);

        ctx->lo <<= 3;
        ctx->buffer[56] = ctx->lo;
        ctx->buffer[57] = ctx->lo >> 8;
        ctx->buffer[58] = ctx->lo >> 16;
        ctx->buffer[59] = ctx->lo >> 24;
        ctx->buffer[60] = ctx->hi;
        ctx->buffer[61] = ctx->hi >> 8;
        ctx->buffer[62] = ctx->hi >> 16;
        ctx->buffer[63] = ctx->hi >> 24;

        body(ctx, ctx->buffer, 64);

        result[0] = ctx->a;
        result[1] = ctx->a >> 8;
        result[2] = ctx->a >> 16;
        result[3] = ctx->a >> 24;
        result[4] = ctx->b;
        result[5] = ctx->b >> 8;
        result[6] = ctx->b >> 16;
        result[7] = ctx->b >> 24;
        result[8] = ctx->c;
        result[9] = ctx->c >> 8;
        result[10] = ctx->c >> 16;
        result[11] = ctx->c >> 24;
        result[12] = ctx->d;
        result[13] = ctx->d >> 8;
        result[14] = ctx->d >> 16;
        result[15] = ctx->d >> 24;

        memset(ctx, 0, sizeof(*ctx));
    }

    // Result needs to be directly extracted by the caller from ctx.a, ctx.b, etc. and the context is not cleared for perf.
    void MD5_Final(MD5_CTX *ctx)
    {
        unsigned long used, free;

        used = ctx->lo & 0x3f;

        ctx->buffer[used++] = 0x80;

        free = 64 - used;

        if (free < 8)
        {
            memset(&ctx->buffer[used], 0, free);
            body(ctx, ctx->buffer, 64);
            used = 0;
            free = 64;
        }

        memset(&ctx->buffer[used], 0, free - 8);

        ctx->lo <<= 3;
        ctx->buffer[56] = ctx->lo;
        ctx->buffer[57] = ctx->lo >> 8;
        ctx->buffer[58] = ctx->lo >> 16;
        ctx->buffer[59] = ctx->lo >> 24;
        ctx->buffer[60] = ctx->hi;
        ctx->buffer[61] = ctx->hi >> 8;
        ctx->buffer[62] = ctx->hi >> 16;
        ctx->buffer[63] = ctx->hi >> 24;

        body(ctx, ctx->buffer, 64);
    }

    bool md5_test()
    {
        md5_hash_gen gen;

        gen.update("The quick brown fox jumps over the lazy dog");
        if (gen.finalize() != md5_hash(0x9D7D109E, 0x82B62B37, 0x351DD86B, 0xD619A442))
            return false;

        gen.clear();
        gen.update("The quick brown fox jumps over the lazy dog.");
        if (gen.finalize() != md5_hash(0xC209D9E4, 0x1CFBD090, 0xADFF68A0, 0xD0CB22DF))
            return false;

        gen.clear();
        gen.update("This is a test");
        if (gen.finalize() != md5_hash("ce114e4501d2f4e2dcea3e17b546f339"))
            return false;

        gen.clear();
        gen.update("A");
        if (gen.finalize() != md5_hash("7fc56270e7a70fa81a5935b72eacbe29"))
            return false;
        gen.update("B");
        if (gen.finalize() != md5_hash("b86fc6b051f63d73de262d4c34e3a0a9"))
            return false;
        gen.update("C");
        if (gen.finalize() != md5_hash("902fbdd2b1df0c4f70b4a5d23525e932"))
            return false;

        dynamic_string str;
        gen.finalize().get_string(str);
        if (str != "902fbdd2b1df0c4f70b4a5d23525e932")
            return false;

        gen.clear();
        if (gen.finalize() != md5_hash("d41d8cd98f00b204e9800998ecf8427e"))
            return false;
        if (!(gen.finalize() == md5_hash("d41d8cd98f00b204e9800998ecf8427e")))
            return false;
        if (gen.finalize() < md5_hash("d41d8cd98f00b204e9800998ecf8427e"))
            return false;

        if (md5_hash_gen("fadsahfuidsyuifeywiurhwekjlrhblekwjhbrkjewqhrkjhewrf89ds7f987dsa897dsf9gyuierhlgkjhrekjthnrkejlthoruioguopdsifuapofsdj'fjdsaklhgkdjslfhgjkdfsgfdhjogiuyfviubygvvcx876bv87xcz6vcxyvcxyhkjv").finalize() != md5_hash("8897731974459964203eeee0851d1ef4"))
            return false;

        // purposely should fail
        if (!(md5_hash_gen("Xfadsahfuidsyuifeywiurhwekjlrhblekwjhbrkjewqhrkjhewrf89ds7f987dsa897dsf9gyuierhlgkjhrekjthnrkejlthoruioguopdsifuapofsdj'fjdsaklhgkdjslfhgjkdfsgfdhjogiuyfviubygvvcx876bv87xcz6vcxyvcxyhkjv").finalize() != md5_hash("8897731974459964203eeee0851d1ef4")))
            return false;

        if (md5_hash_gen("oidui09ds87f87cfudshufhdsfhkdjhfvu9icxyv789cx67v86dsf87ytiudshfiuhgkjrgwekjhreiour098dsu890f7d897f0ds7g8cydoivhckjhbvkjcvxh98v7by896fs785ds86f586tiy342ugr5iug432rdskhfdsiovioxcuyf890sd78fd78sayuhejhrjkehkjhsdjkfndsjvhd89cv7y8c7x96v78d68f76ds78f6ds78f89sd7f8ds7f98ds7087ds896guicxyviuyxhciuvhyxchvkjcxhkvjhcxuiysdfuifg").finalize() != md5_hash("9d641c30f6755515f6d98258cfc1b300"))
            return false;

        return true;
    }

} // namespace vogl
