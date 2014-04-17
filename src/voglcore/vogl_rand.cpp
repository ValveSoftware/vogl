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

// File: vogl_rand.cpp
// See:
// http://www.ciphersbyritter.com/NEWS4/RANDC.HTM
// http://burtleburtle.net/bob/rand/smallprng.html
// http://www.cs.ucl.ac.uk/staff/d.jones/GoodPracticeRNG.pdf
// See GPG7, page 120, or http://www.lomont.org/Math/Papers/2008/Lomont_PRNG_2008.pdf
// I've tested random::urand32() using the CISC test suite here: http://www.csis.hku.hk/cisc/download/idetect/
#include "vogl_core.h"
#include "vogl_rand.h"
#include "vogl_hash.h"
#include "vogl_md5.h"
#include "vogl_bigint128.h"

#include "vogl_image_utils.h"

// TODO: Linux specific, needed by the seed_from_urand() method
#if PLATFORM_LINUX
	#include <unistd.h>
	#include <sys/syscall.h>
#endif

#define znew (z = 36969 * (z & 65535) + (z >> 16))
#define wnew (w = 18000 * (w & 65535) + (w >> 16))
#define MWC ((znew << 16) + wnew)
#define FIB ((b = a + b), (a = b - a))
#define KISS ((MWC ^ VOGL_CONG) + VOGL_SHR3)
#define LFIB4 (c++, t[c] = t[c] + t[UC(c + 58)] + t[UC(c + 119)] + t[UC(c + 178)])
#define SWB (c++, bro = (x < y), t[c] = (x = t[UC(c + 34)]) - (y = t[UC(c + 19)] + bro))
#define UNI (KISS * 2.328306e-10)
#define VNI ((long)KISS) * 4.656613e-10
#define UC (unsigned char) /*a cast operation*/

//#define ROTL(x,k) (((x)<<(k))|((x)>>(32-(k))))
#define ROTL(x, k) VOGL_ROTATE_LEFT(x, k)

namespace vogl
{
    random g_random;
    fast_random g_fast_random;
    thread_safe_random g_thread_safe_random;

    static const double cNorm32 = 1.0 / (double)0x100000000ULL;

    kiss99::kiss99()
    {
        x = 123456789;
        y = 362436000;
        z = 521288629;
        c = 7654321;
    }

    void kiss99::seed(uint32 i, uint32 j, uint32 k)
    {
        x = i;
        y = j;
        z = k;
        c = 7654321;
    }

    inline uint32 kiss99::next()
    {
        x = 69069 * x + 12345;

        y ^= (y << 13);
        y ^= (y >> 17);
        y ^= (y << 5);

        uint64_t t = c;
        t += (698769069ULL * z);
        c = static_cast<uint32>(t >> 32);
        z = static_cast<uint32>(t);

        return (x + y + z);
    }

    inline uint32 ranctx::next()
    {
        uint32 e = a - ROTL(b, 27);
        a = b ^ ROTL(c, 17);
        b = c + d;
        c = d + e;
        d = e + a;
        return d;
    }

    void ranctx::seed(uint32 seed)
    {
        a = 0xf1ea5eed, b = c = d = seed;
        for (uint32 i = 0; i < 20; ++i)
            next();
    }

    well512::well512()
    {
        seed(0xDEADBE3F);
    }

    void well512::seed(uint32 seed[well512::cStateSize])
    {
        memcpy(m_state, seed, sizeof(m_state));
        m_index = 0;
    }

    void well512::seed(uint32 seed)
    {
        uint32 jsr = utils::swap32(seed) ^ 0xAAC29377;

        for (uint i = 0; i < cStateSize; i++)
        {
            VOGL_SHR3;
            seed = bitmix32c(seed);

            m_state[i] = seed ^ jsr;
        }
        m_index = 0;
    }

    void well512::seed(uint32 seed1, uint32 seed2, uint32 seed3)
    {
        uint32 jsr = seed2;
        uint32 jcong = seed3;

        for (uint i = 0; i < cStateSize; i++)
        {
            VOGL_SHR3;
            seed1 = bitmix32c(seed1);
            VOGL_CONG;

            m_state[i] = seed1 ^ jsr ^ jcong;
        }
        m_index = 0;
    }

    void well512::seed(uint32 seed1, uint32 seed2, uint32 seed3, uint seed4)
    {
        uint32 jsr = seed2;
        uint32 jcong = seed3;

        m_state[0] = seed1;
        m_state[1] = seed2;
        m_state[2] = seed3;
        m_state[3] = seed4;

        for (uint i = 4; i < cStateSize; i++)
        {
            VOGL_SHR3;
            VOGL_CONG;

            m_state[i] = jsr ^ jcong ^ m_state[(i - 4) & (cStateSize - 1)];
        }

        m_index = 0;
    }

    inline uint32 well512::next()
    {
        uint32 a, b, c, d;
        a = m_state[m_index];
        c = m_state[(m_index + 13) & 15];
        b = a ^ c ^ (a << 16) ^ (c << 15);
        c = m_state[(m_index + 9) & 15];
        c ^= (c >> 11);
        a = m_state[m_index] = b ^ c;
        d = a ^ ((a << 5) & 0xDA442D20UL);
        m_index = (m_index + 15) & 15;
        a = m_state[m_index];
        m_state[m_index] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);
        return m_state[m_index];
    }

    random::random()
    {
        seed(12345, 65435, 34221);
    }

    random::random(uint32 i)
    {
        seed(i);
    }

    void random::seed()
    {
        seed(12345, 65435, 34221);
    }

    void random::seed(uint32 i1, uint32 i2, uint32 i3)
    {
        m_ranctx.seed(i1 ^ i2 ^ i3);

        m_kiss99.seed(i1, i2, i3);

        m_well512.seed(i1, i2, i3);

        m_pdes.seed((static_cast<uint64_t>(i1) | (static_cast<uint64_t>(i2) << 32U)) ^ (static_cast<uint64_t>(i3) << 10));

        const uint warming_iters = 16;
        for (uint i = 0; i < warming_iters; i++)
            urand32();
    }

    void random::seed(uint32 i1, uint32 i2, uint32 i3, uint i4)
    {
        m_ranctx.seed(i1 ^ i2 ^ i3);

        m_kiss99.seed(i1, i2, i4);

        m_well512.seed(i1, i2, i3, i4);

        m_pdes.seed((static_cast<uint64_t>(i1) | (static_cast<uint64_t>(i4) << 32U)) ^ (static_cast<uint64_t>(i3) << 10));

        const uint warming_iters = 16;
        for (uint i = 0; i < warming_iters; i++)
            urand32();
    }

    void random::seed(const md5_hash &h)
    {
        seed(h[0], h[1], h[2], h[3]);
    }

    void random::seed(uint32 i)
    {
        md5_hash h(md5_hash_gen(&i, sizeof(i)).finalize());
        seed(h[0], h[1], h[2], h[3]);
    }

    void random::seed_from_urandom()
    {
        const uint N = 4;
        uint32 buf[N] = { 0xABCDEF, 0x12345678, 0xFFFECABC, 0xABCDDEF0 };
        FILE *fp = vogl_fopen("/dev/urandom", "rb");
        if (fp)
        {
            size_t n = fread(buf, 1, sizeof(buf), fp);
            VOGL_NOTE_UNUSED(n);
            vogl_fclose(fp);
        }

        // Hash thread, process ID's, etc. for good measure, or in case the call failed, or to make this easier to port. (we can just leave out the /dev/urandom read)
        pid_t tid = (pid_t)syscall(SYS_gettid);
        pid_t pid = getpid();
        pid_t ppid = getppid();

        buf[0] ^= static_cast<uint32>(time(NULL));
        buf[1] ^= static_cast<uint32>(utils::RDTSC()) ^ static_cast<uint32>(ppid);
        buf[2] ^= static_cast<uint32>(tid);
        buf[3] ^= static_cast<uint32>(pid);

        seed(buf[0], buf[1], buf[2], buf[3]);
    }

    uint32 random::urand32()
    {
        return m_kiss99.next() ^ m_ranctx.next() ^ m_well512.next() ^ m_pdes.next32();
    }

    uint64_t random::urand64()
    {
        uint64_t result = urand32();
        result <<= 32ULL;
        result |= urand32();
        return result;
    }

    uint32 random::fast_urand32()
    {
        return m_well512.next();
    }

    uint32 random::get_bit()
    {
        uint32 k = urand32();
        return (k ^ (k >> 6) ^ (k >> 10) ^ (k >> 30)) & 1;
    }

    double random::drand(double l, double h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        double fract = (urand32() * (cNorm32 * cNorm32)) + (urand32() * cNorm32);

        // clamp here shouldn't be necessary, but it ensures this function provably cannot return out of range results
        return math::clamp(l + (h - l) * fract, l, h);
    }

    float random::frand(float l, float h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        float r = static_cast<float>(l + (h - l) * (urand32() * cNorm32));

        // clamp here shouldn't be necessary, but it ensures this function provably cannot return out of range results
        return math::clamp<float>(r, l, h);
    }

    static inline uint umul32_return_high(uint32 range, uint32 rnd)
    {
#if defined(_M_IX86) && defined(_MSC_VER)
        //uint32 rnd_range = static_cast<uint32>(__emulu(range, rnd) >> 32U);
        uint32 x[2];
        *reinterpret_cast<uint64_t *>(x) = __emulu(range, rnd);
        uint32 rnd_range = x[1];
#else
        uint32 rnd_range = static_cast<uint32>((range * static_cast<uint64_t>(rnd)) >> 32U);
#endif

        return rnd_range;
    }

    int random::irand(int l, int h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32 range = static_cast<uint32>(h - l);

        uint32 rnd = urand32();

        uint32 rnd_range = umul32_return_high(range, rnd);

        int result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    int random::irand_inclusive(int l, int h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        uint32 range = static_cast<uint32>(h - l);

        uint32 rnd = urand32();

        int result = static_cast<int>(rnd);
        if (range != cUINT32_MAX)
        {
            ++range;
            uint32 rnd_range = umul32_return_high(range, rnd);
            result = l + rnd_range;
        }

        VOGL_ASSERT((result >= l) && (result <= h));
        return result;
    }

    uint random::urand(uint l, uint h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32 range = h - l;

        uint32 rnd = urand32();

        uint32 rnd_range = umul32_return_high(range, rnd);

        uint result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    uint random::urand_inclusive(uint l, uint h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        uint32 range = h - l;

        uint32 rnd = urand32();

        uint32 result = rnd;
        if (range != cUINT32_MAX)
        {
            ++range;
            uint32 rnd_range = umul32_return_high(range, rnd);
            result = l + rnd_range;
        }

        VOGL_ASSERT((result >= l) && (result <= h));
        return result;
    }

    uint64_t random::urand64(uint64_t l, uint64_t h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint64_t range = h - l;

        uint64_t rnd = urand64();

        bigint128 mul_result;
        bigint128::unsigned_multiply(range, rnd, mul_result);

        uint64_t rnd_range = mul_result.get_qword(1);

        uint64_t result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    uint64_t random::urand64_inclusive(uint64_t l, uint64_t h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        uint64_t range = h - l;

        uint64_t rnd = urand64();

        uint64_t result = rnd;
        if (range != cUINT64_MAX)
        {
            bigint128 mul_result;
            bigint128::unsigned_multiply(range + 1, rnd, mul_result);

            uint64_t rnd_range = mul_result.get_qword(1);

            result = l + rnd_range;
        }

        VOGL_ASSERT((result >= l) && (result <= h));
        return result;
    }

    /*
   ALGORITHM 712, COLLECTED ALGORITHMS FROM ACM.
   THIS WORK PUBLISHED IN TRANSACTIONS ON MATHEMATICAL SOFTWARE,
   VOL. 18, NO. 4, DECEMBER, 1992, PP. 434-435.
   The function returns a normally distributed pseudo-random number
   with a given mean and standard devaiation.  Calls are made to a
   function subprogram which must return independent random
   numbers uniform in the interval (0,1).
   The algorithm uses the ratio of uniforms method of A.J. Kinderman
   and J.F. Monahan augmented with quadratic bounding curves.
   */
    double random::gaussian(double mean, double stddev)
    {
        double q, u, v, x, y;

        /*
	Generate P = (u,v) uniform in rect. enclosing acceptance region
	Make sure that any random numbers <= 0 are rejected, since
	gaussian() requires uniforms > 0, but RandomUniform() delivers >= 0.
	*/
        do
        {
            u = drand(0, 1);
            v = drand(0, 1);
            if (u <= 0.0 || v <= 0.0)
            {
                u = 1.0;
                v = 1.0;
            }
            v = 1.7156 * (v - 0.5);

            /*  Evaluate the quadratic form */
            x = u - 0.449871;
            y = fabs(v) + 0.386595;
            q = x * x + y * (0.19600 * y - 0.25472 * x);

            /* Accept P if inside inner ellipse */
            if (q < 0.27597)
                break;

            /*  Reject P if outside outer ellipse, or outside acceptance region */
        } while ((q > 0.27846) || (v * v > -4.0 * log(u) * u * u));

        /*  Return ratio of P's coordinates as the normal deviate */
        return (mean + stddev * v / u);
    }

    thread_safe_random::thread_safe_random()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed();
    }

    thread_safe_random::thread_safe_random(uint32 i)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed(i);
    }

    void thread_safe_random::seed()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed();
    }

    void thread_safe_random::seed(uint32 i)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed(i);
    }

    void thread_safe_random::seed(uint32 i1, uint32 i2, uint32 i3)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed(i1, i2, i3);
    }

    void thread_safe_random::seed(uint32 i1, uint32 i2, uint32 i3, uint32 i4)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed(i1, i2, i3, i4);
    }

    void thread_safe_random::seed(const md5_hash &h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed(h);
    }

    void thread_safe_random::seed_from_urandom()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        m_rm.seed_from_urandom();
    }

    uint8 thread_safe_random::urand8()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand8();
    }

    uint16 thread_safe_random::urand16()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand16();
    }

    uint32 thread_safe_random::urand32()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand32();
    }

    uint64_t thread_safe_random::urand64()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand64();
    }

    uint32 thread_safe_random::fast_urand32()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.fast_urand32();
    }

    uint32 thread_safe_random::get_bit()
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.get_bit();
    }

    double thread_safe_random::drand(double l, double h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.drand(l, h);
    }

    float thread_safe_random::frand(float l, float h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.frand(l, h);
    }

    int thread_safe_random::irand(int l, int h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.irand(l, h);
    }

    int thread_safe_random::irand_inclusive(int l, int h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.irand_inclusive(l, h);
    }

    uint thread_safe_random::urand(uint l, uint h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand(l, h);
    }

    uint thread_safe_random::urand_inclusive(uint l, uint h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand_inclusive(l, h);
    }

    // Returns random 64-bit unsigned int between [l, h)
    uint64_t thread_safe_random::urand64(uint64_t l, uint64_t h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand64(l, h);
    }

    // Returns random 64-bit unsigned int between [l, h]
    uint64_t thread_safe_random::urand64_inclusive(uint64_t l, uint64_t h)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.urand64_inclusive(l, h);
    }

    double thread_safe_random::gaussian(double mean, double stddev)
    {
        scoped_spinlock scoped_lock(m_spinlock);
        return m_rm.gaussian(mean, stddev);
    }

    int fast_random::irand(int l, int h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32 range = static_cast<uint32>(h - l);

        uint32 rnd = urand32();

        uint32 rnd_range = umul32_return_high(range, rnd);

        int result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    uint fast_random::urand(uint l, uint h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32 range = h - l;

        uint32 rnd = urand32();

        uint32 rnd_range = umul32_return_high(range, rnd);

        uint result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    double fast_random::drand(double l, double h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        return math::clamp(l + (h - l) * (urand32() * cNorm32), l, h);
    }

    float fast_random::frand(float l, float h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        float r = static_cast<float>(l + (h - l) * (urand32() * cNorm32));

        return math::clamp<float>(r, l, h);
    }

    // Got this from http://www.physics.buffalo.edu/phy410-505-2009/topic2/lec-2-1.pdf
    template <typename T>
    double chi_sqr_test(T gen)
    {
        int tries = 10000000;
        const int BINS = 10000;
        printf("Binning %i tries in %i bins\n", tries, BINS);
        int bin[BINS];
        for (int b = 0; b < BINS; b++)
            bin[b] = 0;

        for (int i = 0; i < tries; i++)
        {
            int b = gen(BINS);
            VOGL_ASSERT(b < BINS);
            if (b == BINS)
                --b;
            ++bin[b];
        }
        double chiSqr = 0;
        double expect = tries / double(BINS);
        for (int b = 0; b < BINS; b++)
        {
            double diff = bin[b] - expect;
            chiSqr += diff * diff;
        }
        chiSqr /= expect;
        printf("chi-square: %f, chi-square/d.o.f. = %f\n", chiSqr, chiSqr / (BINS - 1));
        return chiSqr / (BINS - 1);
    }

    struct fast_rand_test_obj
    {
        fast_rand_test_obj(fast_random &r)
            : m_r(r)
        {
            printf("%s:\n", VOGL_METHOD_NAME);
        }
        int operator()(int limit)
        {
            return m_r.irand(0, limit);
        }
        fast_random &m_r;
    };

    struct rand_test_obj
    {
        rand_test_obj(random &r)
            : m_r(r)
        {
            printf("%s:\n", VOGL_METHOD_NAME);
        }
        int operator()(int limit)
        {
            return m_r.irand(0, limit);
        }
        random &m_r;
    };

    struct rand_test_obj2
    {
        rand_test_obj2(random &r)
            : m_r(r)
        {
            printf("%s:\n", VOGL_METHOD_NAME);
        }
        int operator()(int limit)
        {
            uint b = 0;
            for (uint i = 0; i < 32; i++)
                b = (b << 1) | m_r.get_bit();
            return (static_cast<double>(b) * limit) / (static_cast<double>(cUINT32_MAX) + 1.0f);
        }
        random &m_r;
    };

    struct dbl_rand_test_obj
    {
        dbl_rand_test_obj(random &r)
            : m_r(r)
        {
            printf("%s:\n", VOGL_METHOD_NAME);
        }
        int operator()(int limit)
        {
            return static_cast<int>(m_r.drand(0, 1.0f) * limit);
        }
        random &m_r;
    };

    template <typename T>
    double pi_estimate(const unsigned long points, T &func)
    {
        unsigned long hits = 0;
        for (unsigned long i = 0; i < points; i++)
        {
            double x = func.drand(0.0f, 1.0f);
            double y = func.drand(0.0f, 1.0f);
            if ((x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5) < 0.25)
                ++hits;
        }
        return 4.0f * double(hits) / double(points);
    }

    template <typename T>
    void measure_pi(
        const unsigned long trials,
        const unsigned long points,
        double &average,
        double &std_dev, T &func)
    {
        double sum = 0;
        double squared_sum = 0;
        for (unsigned long t = 0; t < trials; t++)
        {
            double pi = pi_estimate(points, func);
            sum += pi;
            squared_sum += pi * pi;
        }
        average = sum / trials;
        std_dev = math::maximum<double>(0.0f, squared_sum / trials - average * average);
        std_dev = sqrt(std_dev / (trials - 1));
    }

    // Very basic tests, but better than nothing.
    bool rand_test()
    {
        fast_random frm;
        random rm;

        time_t seconds = time(NULL);
        uint32 seed = static_cast<uint32>(seconds);
        frm.seed(seed);
        rm.seed(seed);

        image_u8 eye_test_img(1024, 1024);
        for (uint t = 0; t < 500; t++)
        {
            for (uint i = 0; i < 40000; i++)
            {
                uint x, y;
                if (i > 10000)
                {
                    x = static_cast<int>(rm.frand(0, 1.0f) * eye_test_img.get_width());
                    y = static_cast<int>(rm.frand(0, 1.0f) * eye_test_img.get_height());
                }
                else
                {
                    x = rm.irand(0, eye_test_img.get_width());
                    y = rm.irand(0, eye_test_img.get_height());
                }
                uint l = rm.irand(1, 16);
                color_quad_u8 c(eye_test_img(x, y));
                int k = math::minimum<int>(c[0] + l, 255);
                c.set(k);
                eye_test_img(x, y) = c;
            }

            dynamic_string filename(cVarArg, "rand_eye_test_%02i.png", t);
            image_utils::write_to_file(filename.get_ptr(), eye_test_img, image_utils::cWriteFlagIgnoreAlpha);
            printf("Wrote %s\n", filename.get_ptr());
        }

        chi_sqr_test(fast_rand_test_obj(frm));
        chi_sqr_test(fast_rand_test_obj(frm));

        chi_sqr_test(rand_test_obj(rm));
        chi_sqr_test(rand_test_obj(rm));
        chi_sqr_test(rand_test_obj2(rm));

        chi_sqr_test(dbl_rand_test_obj(rm));
        chi_sqr_test(dbl_rand_test_obj(rm));

        for (uint i = 0; i < 5; i++)
        {
            double avg, std_dev;
            measure_pi(10000, 10000, avg, std_dev, rm);
            printf("pi avg: %1.15f, std dev: %1.15f\n", avg, std_dev);

            // check for obviously bogus results
            if ((fabs(avg) - 3.14f) > .01f)
                return false;

            if (fabs(std_dev) < .0001f)
                return false;
        }

        random cur_rm(rm);
        random orig_rm(rm);
        const uint N = 1000000000;
        printf("Running cycle test, %u tries:\n", N);
        for (uint i = 0; i < N; i++)
        {
            cur_rm.urand32();
            if (memcmp(&cur_rm, &orig_rm, sizeof(cur_rm)) == 0)
            {
                printf("Cycle detected!\n");
                return false;
            }
            if ((i & 0xFFFFFF) == 0xFFFFFF)
                printf("%3.2f%%\n", (double)i / N * 100.0f);
        }
        printf("OK\n");

        return true;
    }

} // namespace vogl
