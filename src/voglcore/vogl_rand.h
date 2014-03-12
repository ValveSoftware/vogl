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

// File: vogl_rand.h
#pragma once

#include "vogl_core.h"
#include "vogl_threading.h"

#define VOGL_SHR3 (jsr ^= (jsr << 17), jsr ^= (jsr >> 13), jsr ^= (jsr << 5))
#define VOGL_CONG (jcong = 69069 * jcong + 1234567)

namespace vogl
{
    class md5_hash;

    class kiss99
    {
    public:
        kiss99();

        void seed(uint32 i, uint32 j, uint32 k);

        inline uint32 next();

    private:
        uint32 x;
        uint32 y;
        uint32 z;
        uint32 c;
    };

    class well512
    {
    public:
        well512();

        enum
        {
            cStateSize = 16
        };
        void seed(uint32 seed[cStateSize]);
        void seed(uint32 seed);
        void seed(uint32 seed1, uint32 seed2, uint32 seed3);
        void seed(uint32 seed1, uint32 seed2, uint32 seed3, uint seed4);

        inline uint32 next();

    private:
        uint32 m_state[cStateSize];
        uint32 m_index;
    };

    class ranctx
    {
    public:
        ranctx()
        {
            seed(0xDE149737);
        }

        void seed(uint32 seed);

        inline uint32 next();

    private:
        uint32 a;
        uint32 b;
        uint32 c;
        uint32 d;
    };

    // Originally from Numerical Recipes in C
    // Interesting because the seed is just a 64-bit counter.
    class pdes
    {
    public:
        pdes()
            : m_ctr(0)
        {
        }

        void seed(uint64_t s)
        {
            m_ctr = s;
        }

        inline uint64_t get_seed() const
        {
            return m_ctr;
        }

        inline uint32 next32()
        {
            uint32 l = static_cast<uint32>(m_ctr);
            uint32 h = static_cast<uint32>(m_ctr >> 32U);
            psdes_hash_64(&h, &l);
            m_ctr++;
            return l;
        }

        inline uint64_t next64()
        {
            uint32 l = static_cast<uint32>(m_ctr);
            uint32 h = static_cast<uint32>(m_ctr >> 32U);
            psdes_hash_64(&h, &l);
            m_ctr++;
            return static_cast<uint64_t>(l) | (static_cast<uint64_t>(h) << 32U);
        }

    private:
        void psdes_hash_64(uint *pword0, uint *pword1)
        {
            const uint NITER = 4;
            uint i, iswap, ia, iah, ial, ib, ic;
            static const uint c1[NITER] = { 0xbaa96887, 0x1e17d32c, 0x03bcdc3c, 0x0f33d1b2 };
            static const uint c2[NITER] = { 0x4b0f3b58, 0xe874f0c3, 0x6955c5a6, 0x55a7ca46 };
            for (i = 0; i < NITER; i++)
            {
                iswap = *pword1;
                ia = iswap ^ c1[i];
                ial = ia & 0xffff;
                iah = ia >> 16;
                ib = ial * ial + ~(iah * iah);
                ic = (ib >> 16) | ((ib & 0xffff) << 16);
                *pword1 = (*pword0) ^ ((ic ^ c2[i]) + ial * iah);
                *pword0 = iswap;
            }
        }

        uint64_t m_ctr;
    };

    class random
    {
    public:
        random();
        random(uint32 i);

        void seed();
        void seed(uint32 i);
        void seed(uint32 i1, uint32 i2, uint32 i3);
        void seed(uint32 i1, uint32 i2, uint32 i3, uint32 i4);
        void seed(const md5_hash &h);
        void seed_from_urandom();

        uint8 urand8()
        {
            return static_cast<uint8>(urand32());
        }
        uint16 urand16()
        {
            return static_cast<uint16>(urand32());
        }
        uint32 urand32();
        uint64_t urand64();

        // "Fast" variant uses no multiplies.
        uint32 fast_urand32();

        uint32 get_bit();

        // Returns random double between [0, 1)
        double drand(double l, double h);

        float frand(float l, float h);

        // Returns random signed int between [l, h)
        int irand(int l, int h);

        // Returns random signed int between [l, h]
        int irand_inclusive(int l, int h);

        // Returns random unsigned int between [l, h)
        uint urand(uint l, uint h);

        // Returns random unsigned int between [l, h]
        uint urand_inclusive(uint l, uint h);

        // Returns random 64-bit unsigned int between [l, h)
        uint64_t urand64(uint64_t l, uint64_t h);

        // Returns random 64-bit unsigned int between [l, h]
        uint64_t urand64_inclusive(uint64_t l, uint64_t h);

        double gaussian(double mean, double stddev);

    private:
        ranctx m_ranctx;
        kiss99 m_kiss99;
        well512 m_well512;
        pdes m_pdes;
    };

    class thread_safe_random
    {
    public:
        thread_safe_random();
        thread_safe_random(uint32 i);

        void seed();
        void seed(uint32 i);
        void seed(uint32 i1, uint32 i2, uint32 i3);
        void seed(uint32 i1, uint32 i2, uint32 i3, uint32 i4);
        void seed(const md5_hash &h);
        void seed_from_urandom();

        uint8 urand8();
        uint16 urand16();
        uint32 urand32();
        uint64_t urand64();

        // "Fast" variant uses no multiplies.
        uint32 fast_urand32();

        uint32 get_bit();

        // Returns random double between [0, 1)
        double drand(double l, double h);

        float frand(float l, float h);

        // Returns random signed int between [l, h)
        int irand(int l, int h);

        // Returns random signed int between [l, h]
        int irand_inclusive(int l, int h);

        // Returns random unsigned int between [l, h)
        uint urand(uint l, uint h);

        // Returns random unsigned int between [l, h]
        uint urand_inclusive(uint l, uint h);

        // Returns random 64-bit unsigned int between [l, h)
        uint64_t urand64(uint64_t l, uint64_t h);

        // Returns random 64-bit unsigned int between [l, h]
        uint64_t urand64_inclusive(uint64_t l, uint64_t h);

        double gaussian(double mean, double stddev);

    private:
        random m_rm;
        spinlock m_spinlock;
    };

    // Simpler, minimal state PRNG - combines SHR3 and CONG.
    // This thing fails many tests (Gorilla, Collision, Birthday).
    class fast_random
    {
    public:
        inline fast_random()
            : jsr(0xABCD917A), jcong(0x17F3DEAD)
        {
        }
        inline fast_random(const fast_random &other)
            : jsr(other.jsr), jcong(other.jcong)
        {
        }
        inline fast_random(uint32 i)
        {
            seed(i);
        }
        inline fast_random &operator=(const fast_random &other)
        {
            jsr = other.jsr;
            jcong = other.jcong;
            return *this;
        }

        inline void seed(uint32 i);
        inline void set_default_seed()
        {
            jsr = 0xABCD917A;
            jcong = 0x17F3DEAD;
        }

        inline uint32 urand32();
        inline uint64_t urand64();
        inline uint32 get_bit();

        int irand(int l, int h);
        uint urand(uint l, uint h);

        double drand(double l, double h);

        float frand(float l, float h);

    private:
        uint32 jsr;
        uint32 jcong;
    };

    // inlines

    inline void fast_random::seed(uint32 i)
    {
        jsr = i;
        VOGL_SHR3;
        jcong = (~i) ^ 0xDEADBEEF;
    }

    inline uint32 fast_random::urand32()
    {
        return VOGL_SHR3 ^ VOGL_CONG;
    }

    inline uint32 fast_random::get_bit()
    {
        uint32 k = urand32();
        return ((k >> 31U) ^ (k >> 29U)) & 1;
    }

    inline uint64_t fast_random::urand64()
    {
        uint64_t result = urand32();
        result <<= 32ULL;
        result |= urand32();
        return result;
    }

    inline void memset_random(fast_random &context, void *pDst, uint n)
    {
        while (n >= 4)
        {
            *reinterpret_cast<uint32 *>(pDst) = context.urand32();
            pDst = reinterpret_cast<uint32 *>(pDst) + 1;
            n -= 4;
        }

        while (n)
        {
            *reinterpret_cast<uint8 *>(pDst) = static_cast<uint8>(context.urand32());
            pDst = reinterpret_cast<uint8 *>(pDst) + 1;
            --n;
        }
    }

    // non-thread safe global instances, for simplicity in single threaded tools/experiments
    extern random g_random;
    extern fast_random g_fast_random;
    extern thread_safe_random g_thread_safe_random;

    bool rand_test();

} // namespace vogl
