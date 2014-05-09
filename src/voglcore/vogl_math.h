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

// File: vogl_math.h
#pragma once

#include "vogl_core.h"

#if defined(_M_IX86) && defined(COMPILER_MSVC)
#include <intrin.h>
#pragma intrinsic(__emulu)
unsigned __int64 __emulu(unsigned int a, unsigned int b);
//#elif defined(COMPILER_GCCLIKE)
//#include <xmmintrin.h>
#endif

namespace vogl
{
    namespace math
    {
        const float cPi = 3.1415926535f; // 180
        const double cPiD = 3.14159265358979323846;
        const float cHalfPi = 3.1415926535f * .5f;  // 90
        const float cTwoPi = 3.1415926535f * 2.0f;  // 360
        const float cFourPi = 3.1415926535f * 4.0f; // 720
        const float cNearlyInfinite = 1.0e+37f;
        const float cTinyEpsilon = 0.00000762939453125f; // .5*(2^-16)
        const float cE = (float)2.71828182845904523536;
        const double cED = 2.71828182845904523536;

        const float cDegToRad = 0.01745329252f;
        const float cRadToDeg = 57.29577951f;

        const uint32_t cFloatExpBits = 8;
        const uint32_t cFloatExpMask = (1 << cFloatExpBits) - 1;
        const uint32_t cFloatFractBits = 23;
        const uint32_t cFloatFractMask = (1 << cFloatFractBits) - 1;
        const uint32_t cFloatExpShift = 23;
        const uint32_t cFloatExpShiftedMask = cFloatExpMask << cFloatExpShift;
        const int32_t cFloatExpBias = 127;
        const uint32_t cFloatSignMask = 0x80000000U;

        const uint32_t cDoubleExpBits = 11;
        const uint64_t cDoubleExpMask = (1ULL << cDoubleExpBits) - 1ULL;
        const uint32_t cDoubleFractBits = 52;
        const uint64_t cDoubleFractMask = (1ULL << cDoubleFractBits) - 1ULL;
        const uint32_t cDoubleExpShift = 52;
        const uint64_t cDoubleExpShiftedMask = cDoubleExpMask << cDoubleExpShift;
        const int32_t cDoubleExpBias = 1023;
        const uint64_t cDoubleSignMask = 0x8000000000000000ULL;

        extern uint32_t g_bitmasks[32];

        template <typename T>
        inline bool is_within_open_range(T a, T l, T h)
        {
            return (a >= l) && (a < h);
        }
        template <typename T>
        inline T open_range_check(T a, T h)
        {
            (void)h;
            VOGL_ASSERT(a < h);
            return a;
        }
        template <typename T>
        inline T open_range_check(T a, T l, T h)
        {
            (void)l;
            (void)h;
            VOGL_ASSERT((a >= l) && (a < h));
            return a;
        }

        template <typename T>
        inline bool is_within_closed_range(T a, T l, T h)
        {
            return (a >= l) && (a <= h);
        }
        template <typename T>
        inline T closed_range_check(T a, T h)
        {
            (void)h;
            VOGL_ASSERT(a <= h);
            return a;
        }
        template <typename T>
        inline T closed_range_check(T a, T l, T h)
        {
            (void)l;
            (void)h;
            VOGL_ASSERT((a >= l) && (a <= h));
            return a;
        }

        // Yes I know these should probably be pass by ref, not val:
        // http://www.stepanovpapers.com/notes.pdf
        // Just don't use them on non-simple (non built-in) types!
        template <typename T>
        inline T minimum(T a, T b)
        {
            return (a < b) ? a : b;
        }
        template <typename T>
        inline T minimum(T a, T b, T c)
        {
            return minimum(minimum(a, b), c);
        }
        template <typename T>
        inline T minimum(T a, T b, T c, T d)
        {
            return minimum(minimum(minimum(a, b), c), d);
        }

        template <typename T>
        inline T maximum(T a, T b)
        {
            return (a > b) ? a : b;
        }
        template <typename T>
        inline T maximum(T a, T b, T c)
        {
            return maximum(maximum(a, b), c);
        }
        template <typename T>
        inline T maximum(T a, T b, T c, T d)
        {
            return maximum(maximum(maximum(a, b), c), d);
        }

        template <typename T, typename U>
        inline T lerp(T a, T b, U c)
        {
            return a + (b - a) * c;
        }
        template <typename T, typename U>
        T cubic_lerp(const T &a, const T &b, U s)
        {
            if (s < 0.0f)
                s = 0.0f;
            else if (s > 1.0f)
                s = 1.0f;

            return (a * ((2.0f * (s * s * s)) - (3.0f * (s * s)) + 1.0f)) +
                   (b * ((3.0f * (s * s)) - (2.0f * (s * s * s))));
        }

        template <typename T>
        inline T clamp_low(T value, T low)
        {
            return (value < low) ? low : value;
        }

        template <typename T>
        inline T clamp_high(T value, T high)
        {
            return (value > high) ? high : value;
        }
        template <typename T>
        inline T clamp(T value, T low, T high)
        {
            return (value < low) ? low : ((value > high) ? high : value);
        }

        template <typename T>
        inline T saturate(T value)
        {
            return (value < 0.0f) ? 0.0f : ((value > 1.0f) ? 1.0f : value);
        }

        template <typename T>
        inline T frac(T value)
        {
            T abs_value = fabs(value);
            return abs_value - floor(abs_value);
        }

        // truncation
        inline int float_to_int(float f)
        {
            return static_cast<int>(f);
        }
        inline int float_to_int(double f)
        {
            return static_cast<int>(f);
        }
        inline uint32_t float_to_uint(float f)
        {
            return static_cast<uint32_t>(f);
        }
        inline uint32_t float_to_uint(double f)
        {
            return static_cast<uint32_t>(f);
        }

        // round to nearest
        inline int float_to_int_round(float f)
        {
            return static_cast<int>(f + ((f < 0.0f) ? -.5f : .5f));
        }
        inline int64_t float_to_int64_round(float f)
        {
            return static_cast<int64_t>(f + ((f < 0.0f) ? -.5f : .5f));
        }
        inline int double_to_int_round(double f)
        {
            return static_cast<int>(f + ((f < 0.0f) ? -.5f : .5f));
        }
        inline int64_t double_to_int64_round(double f)
        {
            return static_cast<int64_t>(f + ((f < 0.0f) ? -.5f : .5f));
        }

        inline uint32_t float_to_uint_round(float f)
        {
            return static_cast<uint32_t>((f < 0.0f) ? 0.0f : (f + .5f));
        }
        inline uint64_t float_to_uint64_round(float f)
        {
            return static_cast<uint64_t>((f < 0.0f) ? 0.0f : (f + .5f));
        }
        inline uint32_t double_to_uint_round(double f)
        {
            return static_cast<uint32_t>((f < 0.0f) ? 0.0f : (f + .5f));
        }
        inline uint64_t double_to_uint64_round(double f)
        {
            return static_cast<uint64_t>((f < 0.0f) ? 0.0f : (f + .5f));
        }

        inline int float_to_int_nearest(float f)
        {
            //return _mm_cvtss_si32(_mm_load_ss(&f));
            return float_to_int_round(f);
        }

        template <typename T>
        inline int sign(T value)
        {
            return (value < 0) ? -1 : ((value > 0) ? 1 : 0);
        }

        template <typename T>
        inline T square(T value)
        {
            return value * value;
        }

        inline bool is_power_of_2(uint32_t x)
        {
            return x && ((x & (x - 1U)) == 0U);
        }
        inline bool is_power_of_2(uint64_t x)
        {
            return x && ((x & (x - 1U)) == 0U);
        }

        template <typename T>
        inline bool is_pointer_aligned(T p, uint32_t alignment)
        {
            VOGL_ASSERT(is_power_of_2(alignment));
            return (reinterpret_cast<intptr_t>(p) & (alignment - 1)) == 0;
        }

        template <typename T>
        inline T align_up_pointer(T p, uint32_t alignment)
        {
            VOGL_ASSERT(is_power_of_2(alignment));
            intptr_t q = reinterpret_cast<intptr_t>(p);
            q = (q + alignment - 1) & (~(alignment - 1));
            return reinterpret_cast<T>(q);
        }

        template <typename T>
        inline uint32_t get_bytes_to_align_up_pointer(T p, uint32_t alignment)
        {
            return reinterpret_cast<uint8_t *>(align_up_pointer(p, alignment)) - reinterpret_cast<uint8_t *>(p);
        }

        template <typename T>
        inline T align_up_value(T x, uint32_t alignment)
        {
            VOGL_ASSERT(is_power_of_2(alignment));
            uint64_t q = static_cast<uint64_t>(x);
            q = (q + alignment - 1U) & (~static_cast<uint64_t>(alignment - 1U));
            return static_cast<T>(q);
        }

        template <typename T>
        inline T align_down_value(T x, uint32_t alignment)
        {
            VOGL_ASSERT(is_power_of_2(alignment));
            uint64_t q = static_cast<uint64_t>(x);
            q = q & (~static_cast<uint64_t>(alignment - 1U));
            return static_cast<T>(q);
        }

        template <typename T>
        inline T get_align_up_value_delta(T x, uint32_t alignment)
        {
            return align_up_value(x, alignment) - x;
        }

        template <class T>
        inline T prev_wrap(T i, T n)
        {
            T temp = i - 1;
            if (temp < 0)
                temp = n - 1;
            return temp;
        }

        template <class T>
        inline T next_wrap(T i, T n)
        {
            T temp = i + 1;
            if (temp >= n)
                temp = 0;
            return temp;
        }

        inline float deg_to_rad(float f)
        {
            return f * cDegToRad;
        };

        inline float rad_to_deg(float f)
        {
            return f * cRadToDeg;
        };

        // (x mod y) with special handling for negative x values.
        static inline int posmod(int x, int y)
        {
            if (x >= 0)
                return (x < y) ? x : (x % y);

            int m = (-x) % y;
            if (m != 0)
                m = y - m;
            return m;
        }

        inline float posfmod(float x, float y)
        {
            VOGL_ASSERT(y > 0.0f);

            if (x >= 0.0f)
                return fmod(x, y);
            float m = fmod(-x, y);
            if (m != 0.0f)
                m = y - m;
            return m;
        }

        // From "Hackers Delight"
        inline uint32_t next_pow2(uint32_t val)
        {
            val--;
            val |= val >> 16;
            val |= val >> 8;
            val |= val >> 4;
            val |= val >> 2;
            val |= val >> 1;
            return val + 1;
        }

        inline uint64_t next_pow2(uint64_t val)
        {
            val--;
            val |= val >> 32;
            val |= val >> 16;
            val |= val >> 8;
            val |= val >> 4;
            val |= val >> 2;
            val |= val >> 1;
            return val + 1;
        }

        inline uint32_t floor_log2i(uint32_t v)
        {
            uint32_t l = 0;
            while (v > 1U)
            {
                v >>= 1;
                l++;
            }
            return l;
        }

        inline uint32_t ceil_log2i(uint32_t v)
        {
            uint32_t l = floor_log2i(v);
            if ((l != cIntBits) && (v > (1U << l)))
                l++;
            return l;
        }

        // Returns the total number of bits needed to encode v.
        inline uint32_t total_bits(uint32_t v)
        {
            uint32_t l = 0;
            while (v > 0U)
            {
                v >>= 1;
                l++;
            }
            return l;
        }

        // Actually counts the number of set bits, but hey
        inline uint32_t bitmask_size(uint32_t mask)
        {
            uint32_t size = 0;
            while (mask)
            {
                mask &= (mask - 1U);
                size++;
            }
            return size;
        }

        inline uint32_t bitmask_ofs(uint32_t mask)
        {
            if (!mask)
                return 0;
            uint32_t ofs = 0;
            while ((mask & 1U) == 0)
            {
                mask >>= 1U;
                ofs++;
            }
            return ofs;
        }

        // true if n is prime. Umm, not fast.
        bool is_prime(uint32_t n);

        // Find the smallest prime >= n.
        uint32_t get_prime(uint32_t n);

        // See Bit Twiddling Hacks (public domain)
        // http://www-graphics.stanford.edu/~seander/bithacks.html
        inline uint32_t count_trailing_zero_bits(uint32_t v)
        {
            uint32_t c = 32; // c will be the number of zero bits on the right

            static const unsigned int B[] = { 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF };
            static const unsigned int S[] = { 1, 2, 4, 8, 16 }; // Our Magic Binary Numbers

            for (int i = 4; i >= 0; --i) // unroll for more speed
            {
                if (v & B[i])
                {
                    v <<= S[i];
                    c -= S[i];
                }
            }

            if (v)
            {
                c--;
            }

            return c;
        }

        inline uint32_t count_leading_zero_bits(uint32_t v)
        {
            #if defined(COMPILER_MSVC)
                // BitScanReverse doesn't return the count of leading 0s, it returns the LSB-based index of the first 1 bit found. 
                // Since we want the number of 0s, we subtract bitcount(uint32_t) - 1 - _BitScanReverse value.
                // When fed a 0, BSR will return an 0 (and in that case we return that there are 32 leading 0s). 
                unsigned long lz = 0;
                if (_BitScanReverse(&lz, v)) {
                    return 31 - lz;
                }

                return 32;
            #elif defined(COMPILER_GCCLIKE)
                return v ? __builtin_clz(v) : 32;
            #else
                uint32_t temp;
                uint32_t result = 32U;

                temp = (v >> 16U);
                if (temp)
                {
                    result -= 16U;
                    v = temp;
                }
                temp = (v >> 8U);
                if (temp)
                {
                    result -= 8U;
                    v = temp;
                }
                temp = (v >> 4U);
                if (temp)
                {
                    result -= 4U;
                    v = temp;
                }
                temp = (v >> 2U);
                if (temp)
                {
                    result -= 2U;
                    v = temp;
                }
                temp = (v >> 1U);
                if (temp)
                {
                    result -= 1U;
                    v = temp;
                }

                if (v & 1U)
                    result--;

                return result;
            #endif
        }

        inline uint32_t count_leading_zero_bits64(uint64_t v)
        {
            // BitScanReverse64 doesn't return the count of leading 0s, it returns the LSB-based index of the first 1 bit found. 
            // Since we want the number of 0s, we subtract bitcount(uint32_t) - 1 - _BitScanReverse value.
            // When fed a 0, BSR will return an 0 (and in that case we return that there are 32 leading 0s). 
            #if defined(PLATFORM_64BITS) && defined(COMPILER_MSVC)
                unsigned long lz = 64;
                if (_BitScanReverse64(&lz, v)) {
                    return uint64(lz);
                }

                return 64;
            #elif defined(COMPILER_GCCLIKE)
                return v ? __builtin_clzll(v) : 64;
            #else
                uint64_t temp;
                uint32_t result = 64U;

                temp = (v >> 32U);
                if (temp)
                {
                    result -= 32U;
                    v = temp;
                }
                temp = (v >> 16U);
                if (temp)
                {
                    result -= 16U;
                    v = temp;
                }
                temp = (v >> 8U);
                if (temp)
                {
                    result -= 8U;
                    v = temp;
                }
                temp = (v >> 4U);
                if (temp)
                {
                    result -= 4U;
                    v = temp;
                }
                temp = (v >> 2U);
                if (temp)
                {
                    result -= 2U;
                    v = temp;
                }
                temp = (v >> 1U);
                if (temp)
                {
                    result -= 1U;
                    v = temp;
                }

                if (v & 1U)
                    result--;

                return result;
            #endif
        }

        // Returns 64-bit result of a * b
        inline uint64_t emulu(uint32_t a, uint32_t b)
        {
            #if defined(_M_IX86) && defined(COMPILER_MSVC)
                return __emulu(a, b);
            #else
                return static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
            #endif
        }

        double compute_entropy(const uint8_t *p, uint32_t n);

        void compute_lower_pow2_dim(int &width, int &height);
        void compute_upper_pow2_dim(int &width, int &height);

        inline bool equal_tol(float a, float b, float t)
        {
            return fabs(a - b) <= ((maximum(fabs(a), fabs(b)) + 1.0f) * t);
        }

        inline bool equal_tol(double a, double b, double t)
        {
            return fabs(a - b) <= ((maximum(fabs(a), fabs(b)) + 1.0f) * t);
        }

        inline uint32_t mul255(uint32_t a, uint32_t b)
        {
            uint32_t t = a * b + 128;
            return (t + (t >> 8)) >> 8;
        }

        inline uint32_t clamp255(uint32_t a)
        {
            if (a & 0xFFFFFF00U)
                a = (~(static_cast<int>(a) >> 31)) & 0xFF;
            return a;
        }

        inline uint32_t get_float_bits(float f)
        {
            union
            {
                float f;
                uint32_t u;
            } x;
            x.f = f;
            return x.u;
        }

        inline uint64_t get_double_bits(double d)
        {
            union
            {
                double d;
                uint64_t u;
            } x;
            x.d = d;
            return x.u;
        }

        inline uint32_t get_float_mantissa(float f)
        {
            const uint32_t u = get_float_bits(f);
            return u & cFloatFractMask;
        }

        inline uint64_t get_double_mantissa(double d)
        {
            const uint64_t u = get_double_bits(d);
            return u & cDoubleFractMask;
        }

        inline int get_float_exponent(float f)
        {
            const uint32_t u = get_float_bits(f);
            const int exp = (u >> cFloatExpShift) & cFloatExpMask;
            return exp - cFloatExpBias;
        }

        inline int get_double_exponent(double d)
        {
            const uint64_t u = get_double_bits(d);
            const int exp = (u >> cDoubleExpShift) & cDoubleExpMask;
            return exp - cDoubleExpBias;
        }

        inline bool is_denormal(double d)
        {
            const uint64_t u = get_double_bits(d);
            const uint64_t exp = (u >> cDoubleExpShift) & cDoubleExpMask;
            const uint64_t mantissa = u & cDoubleFractMask;
            return (exp == 0) && (mantissa != 0);
        }

        inline bool is_nan_or_inf(double d)
        {
            const uint64_t u = get_double_bits(d);
            const uint64_t exp = (u >> cDoubleExpShift) & cDoubleExpMask;
            return exp == cDoubleExpMask;
        }

        inline bool is_denormal(float f)
        {
            const uint32_t u = get_float_bits(f);
            const uint32_t exp = (u >> cFloatExpShift) & cFloatExpMask;
            const uint32_t mantissa = u & cFloatFractMask;
            return (exp == 0) && (mantissa != 0);
        }

        inline bool is_nan_or_inf(float f)
        {
            uint32_t u = get_float_bits(f);
            return ((u >> cFloatExpShift) & cFloatExpMask) == cFloatExpMask;
        }

        inline bool is_float_signed(float f)
        {
            return (get_float_bits(f) & cFloatSignMask) != 0;
        }

        inline bool is_double_signed(double d)
        {
            return (get_double_bits(d) & cDoubleSignMask) != 0;
        }

        inline uint64_t combine_two_uint32s(uint32_t l, uint32_t h)
        {
            return static_cast<uint64_t>(l) | (static_cast<uint64_t>(h) << 32U);
        }

        float gauss(int x, int y, float sigma_sqr);

        enum
        {
            cComputeGaussianFlagNormalize = 1,
            cComputeGaussianFlagPrint = 2,
            cComputeGaussianFlagNormalizeCenterToOne = 4
        };

        void compute_gaussian_kernel(float *pDst, int size_x, int size_y, float sigma_sqr, uint32_t flags);
    }

} // namespace vogl
