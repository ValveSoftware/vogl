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
#include "vogl_core.h"
#include "vogl_rand.h"
#include "vogl_bigint128.h"
#include "vogl_port.h"

#include "vogl_image_utils.h"

namespace vogl
{
    static const double cNorm32 = 1.0 / (double)0x100000000ULL;

    random::random()
    {
        seed(12345);
    }

    random::random(uint32_t i)
    {
        seed(i);
    }

    void random::seed()
    {
        seed(plat_rand());
    }

    void random::seed(uint32_t i)
    {
        m_frand.seed(i);
    }

    uint32_t random::urand32()
    {
        return m_frand.urand32();
    }

    uint64_t random::urand64()
    {
        uint64_t result = urand32();
        result <<= 32ULL;
        result |= urand32();
        return result;
    }

    uint32_t random::get_bit()
    {
        uint32_t k = urand32();
        return (k ^ (k >> 6) ^ (k >> 10) ^ (k >> 30)) & 1;
    }

    double random::drand(double l, double h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        double result = l;

        for (uint32_t i = 0; i < 4; i++)
        {
            double trial_result = (urand32() * (cNorm32 * cNorm32)) + (urand32() * cNorm32);

            if ((trial_result >= l) && (trial_result < h))
            {
                result = trial_result;
                break;
            }
        }

        return result;
    }

    float random::frand(float l, float h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        float result = l;

        for (uint32_t i = 0; i < 4; i++)
        {
            float trial_result = static_cast<float>(l + (h - l) * (urand32() * cNorm32));

            if ((trial_result >= l) && (trial_result < h))
            {
                result = trial_result;
                break;
            }
        }

        return result;
    }

    static inline uint32_t umul32_return_high(uint32_t range, uint32_t rnd)
    {
#if defined(PLATFORM_32BIT) && defined(COMPILER_MSVC)
        //uint32_t rnd_range = static_cast<uint32_t>(__emulu(range, rnd) >> 32U);
        uint32_t x[2];
        *reinterpret_cast<uint64_t *>(x) = __emulu(range, rnd);
        uint32_t rnd_range = x[1];
#else
        uint32_t rnd_range = static_cast<uint32_t>((range * static_cast<uint64_t>(rnd)) >> 32U);
#endif

        return rnd_range;
    }

    int random::irand(int l, int h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32_t range = static_cast<uint32_t>(h - l);

        uint32_t rnd = urand32();

        uint32_t rnd_range = umul32_return_high(range, rnd);

        int result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    int random::irand_inclusive(int l, int h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        uint32_t range = static_cast<uint32_t>(h - l);

        uint32_t rnd = urand32();

        int result = static_cast<int>(rnd);
        if (range != cUINT32_MAX)
        {
            ++range;
            uint32_t rnd_range = umul32_return_high(range, rnd);
            result = l + rnd_range;
        }

        VOGL_ASSERT((result >= l) && (result <= h));
        return result;
    }

    uint32_t random::urand(uint32_t l, uint32_t h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32_t range = h - l;

        uint32_t rnd = urand32();

        uint32_t rnd_range = umul32_return_high(range, rnd);

        uint32_t result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    uint32_t random::urand_inclusive(uint32_t l, uint32_t h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        uint32_t range = h - l;

        uint32_t rnd = urand32();

        uint32_t result = rnd;
        if (range != cUINT32_MAX)
        {
            ++range;
            uint32_t rnd_range = umul32_return_high(range, rnd);
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

    int fast_random::irand(int l, int h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32_t range = static_cast<uint32_t>(h - l);

        uint32_t rnd = urand32();

        uint32_t rnd_range = umul32_return_high(range, rnd);

        int result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    uint32_t fast_random::urand(uint32_t l, uint32_t h)
    {
        VOGL_ASSERT(l < h);
        if (l >= h)
            return l;

        uint32_t range = h - l;

        uint32_t rnd = urand32();

        uint32_t rnd_range = umul32_return_high(range, rnd);

        uint32_t result = l + rnd_range;
        VOGL_ASSERT((result >= l) && (result < h));
        return result;
    }

    double fast_random::drand(double l, double h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        double result = l;

        for (uint32_t i = 0; i < 4; i++)
        {
            double trial_result = (urand32() * (cNorm32 * cNorm32)) + (urand32() * cNorm32);

            if ((trial_result >= l) && (trial_result < h))
            {
                result = trial_result;
                break;
            }
        }

        return result;
    }

    float fast_random::frand(float l, float h)
    {
        VOGL_ASSERT(l <= h);
        if (l >= h)
            return l;

        float result = l;

        for (uint32_t i = 0; i < 4; i++)
        {
            float trial_result = static_cast<float>(l + (h - l) * (urand32() * cNorm32));

            if ((trial_result >= l) && (trial_result < h))
            {
                result = trial_result;
                break;
            }
        }

        return result;
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
            printf("%s:\n", VOGL_FUNCTION_INFO_CSTR);
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
            printf("%s:\n", VOGL_FUNCTION_INFO_CSTR);
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
            printf("%s:\n", VOGL_FUNCTION_INFO_CSTR);
        }
        int operator()(int limit)
        {
            uint32_t b = 0;
            for (uint32_t i = 0; i < 32; i++)
                b = (b << 1) | m_r.get_bit();
            return static_cast<int>((static_cast<double>(b) * limit) / (static_cast<double>(cUINT32_MAX) + 1.0f));
        }
        random &m_r;
    };

    struct dbl_rand_test_obj
    {
        dbl_rand_test_obj(random &r)
            : m_r(r)
        {
            printf("%s:\n", VOGL_FUNCTION_INFO_CSTR);
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
        uint32_t seed = static_cast<uint32_t>(seconds);
        frm.seed(seed);
        rm.seed(seed);

        dynamic_string tmpdir = plat_gettmpdir();

        image_u8 eye_test_img(1024, 1024);
        for (uint32_t t = 0; t < 500; t++)
        {
            for (uint32_t i = 0; i < 40000; i++)
            {
                int x, y;
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

                if ((x < 0) || (x >= static_cast<int>(eye_test_img.get_width())))
                    return false;

                if ((y < 0) || (y >= static_cast<int>(eye_test_img.get_height())))
                    return false;

                uint32_t l = rm.irand(1, 16);
                if ((l < 1) || (l >= 16))
                    return false;

                color_quad_u8 c(eye_test_img(x, y));
                int k = math::minimum<int>(c[0] + l, 255);
                c.set(k);
                eye_test_img(x, y) = c;
            }

            dynamic_string filename(cVarArg, "%s/rand_eye_test_%02i.png", tmpdir.c_str(), t);
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

        for (uint32_t i = 0; i < 5; i++)
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
        const uint32_t N = 1000000000;
        printf("Running cycle test, %u tries:\n", N);
        for (uint32_t i = 0; i < N; i++)
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
