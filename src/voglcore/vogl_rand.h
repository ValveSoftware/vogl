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

namespace vogl
{
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
        inline fast_random(uint32_t i)
        {
            seed(i);
        }
        inline fast_random &operator=(const fast_random &other)
        {
            jsr = other.jsr;
            jcong = other.jcong;
            return *this;
        }

        inline void seed(uint32_t i);
        inline void set_default_seed()
        {
            jsr = 0xABCD917A;
            jcong = 0x17F3DEAD;
        }

        inline uint32_t urand32();
        inline uint64_t urand64();
        inline uint32_t get_bit();

        int irand(int l, int h);
        uint32_t urand(uint32_t l, uint32_t h);

        double drand(double l, double h);

        float frand(float l, float h);

    private:
        uint32_t jsr;
        uint32_t jcong;
    };

    class random
    {
    public:
        random();
        random(uint32_t i);

        void seed();
        void seed(uint32_t i);

        uint8_t urand8()
        {
            return static_cast<uint8_t>(urand32());
        }
        uint16_t urand16()
        {
            return static_cast<uint16_t>(urand32());
        }
        uint32_t urand32();
        uint64_t urand64();

        uint32_t get_bit();

        // Returns random double between [0, 1)
        double drand(double l, double h);

        float frand(float l, float h);

        // Returns random signed int between [l, h)
        int irand(int l, int h);

        // Returns random signed int between [l, h]
        int irand_inclusive(int l, int h);

        // Returns random unsigned int between [l, h)
        uint32_t urand(uint32_t l, uint32_t h);

        // Returns random unsigned int between [l, h]
        uint32_t urand_inclusive(uint32_t l, uint32_t h);

        // Returns random 64-bit unsigned int between [l, h)
        uint64_t urand64(uint64_t l, uint64_t h);

        // Returns random 64-bit unsigned int between [l, h]
        uint64_t urand64_inclusive(uint64_t l, uint64_t h);

        double gaussian(double mean, double stddev);

    private:
        fast_random m_frand;
    };

    // inlines

#define VOGL_SHR3 (jsr ^= (jsr << 17), jsr ^= (jsr >> 13), jsr ^= (jsr << 5))
#define VOGL_CONG (jcong = 69069 * jcong + 1234567)
    inline void fast_random::seed(uint32_t i)
    {
        jsr = i;
        VOGL_SHR3;
        jcong = (~i) ^ 0xDEADBEEF;
    }

    inline uint32_t fast_random::urand32()
    {
        return VOGL_SHR3 ^ VOGL_CONG;
    }

    inline uint32_t fast_random::get_bit()
    {
        uint32_t k = urand32();
        return ((k >> 31U) ^ (k >> 29U)) & 1;
    }

    inline uint64_t fast_random::urand64()
    {
        uint64_t result = urand32();
        result <<= 32ULL;
        result |= urand32();
        return result;
    }

    bool rand_test();

} // namespace vogl
