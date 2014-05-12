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

// File: vogl_math.cpp
#include "vogl_core.h"

namespace vogl
{
    namespace math
    {
        uint32_t g_bitmasks[32] =
            {
                1U << 0U, 1U << 1U, 1U << 2U, 1U << 3U,
                1U << 4U, 1U << 5U, 1U << 6U, 1U << 7U,
                1U << 8U, 1U << 9U, 1U << 10U, 1U << 11U,
                1U << 12U, 1U << 13U, 1U << 14U, 1U << 15U,
                1U << 16U, 1U << 17U, 1U << 18U, 1U << 19U,
                1U << 20U, 1U << 21U, 1U << 22U, 1U << 23U,
                1U << 24U, 1U << 25U, 1U << 26U, 1U << 27U,
                1U << 28U, 1U << 29U, 1U << 30U, 1U << 31U
            };

        bool is_prime(uint32_t n)
        {
            if (n <= 2)
                return n == 2;

            if ((n & 1) == 0)
                return false;

            const uint32_t end = (uint32_t)sqrt(static_cast<double>(n));

            for (uint32_t i = 3; i <= end; i += 2)
                if ((n % i) == 0)
                    return false;

            return true;
        }

        // Find the smallest prime >= n.
        uint32_t get_prime(uint32_t n)
        {
            while (!is_prime(n))
                n++;
            return n;
        }

        double compute_entropy(const uint8_t *p, uint32_t n)
        {
            uint32_t hist[256];
            utils::zero_object(hist);

            for (uint32_t i = 0; i < n; i++)
                hist[*p++]++;

            double entropy = 0.0f;

            const double invln2 = 1.0f / log(2.0f);
            for (uint32_t i = 0; i < 256; i++)
            {
                if (!hist[i])
                    continue;

                double prob = static_cast<double>(hist[i]) / n;
                entropy += (-log(prob) * invln2) * hist[i];
            }

            return entropy;
        }

        void compute_lower_pow2_dim(int &width, int &height)
        {
            const int tex_width = width;
            const int tex_height = height;

            width = 1;
            for (;;)
            {
                if ((width * 2) > tex_width)
                    break;
                width *= 2;
            }

            height = 1;
            for (;;)
            {
                if ((height * 2) > tex_height)
                    break;
                height *= 2;
            }
        }

        void compute_upper_pow2_dim(int &width, int &height)
        {
            if (!math::is_power_of_2((uint32_t)width))
                width = math::next_pow2((uint32_t)width);

            if (!math::is_power_of_2((uint32_t)height))
                height = math::next_pow2((uint32_t)height);
        }

        float gauss(int x, int y, float sigma_sqr)
        {
            float pow = expf(-((x * x + y * y) / (2.0f * sigma_sqr)));
            float g = (1.0f / (sqrtf(2.0f * cPi * sigma_sqr))) * pow;
            return g;
        }

        // size_x/y should be odd
        void compute_gaussian_kernel(float *pDst, int size_x, int size_y, float sigma_sqr, uint32_t flags)
        {
            VOGL_ASSERT(size_x & size_y & 1);

            if (!(size_x | size_y))
                return;

            int mid_x = size_x / 2;
            int mid_y = size_y / 2;

            double sum = 0;
            for (int x = 0; x < size_x; x++)
            {
                for (int y = 0; y < size_y; y++)
                {
                    float g;
                    if ((x > mid_x) && (y < mid_y))
                        g = pDst[(size_x - x - 1) + y * size_x];
                    else if ((x < mid_x) && (y > mid_y))
                        g = pDst[x + (size_y - y - 1) * size_x];
                    else if ((x > mid_x) && (y > mid_y))
                        g = pDst[(size_x - x - 1) + (size_y - y - 1) * size_x];
                    else
                        g = gauss(x - mid_x, y - mid_y, sigma_sqr);

                    pDst[x + y * size_x] = g;
                    sum += g;
                }
            }

            if (flags & cComputeGaussianFlagNormalizeCenterToOne)
            {
                sum = pDst[mid_x + mid_y * size_x];
            }

            if (flags & (cComputeGaussianFlagNormalizeCenterToOne | cComputeGaussianFlagNormalize))
            {
                double one_over_sum = 1.0f / sum;
                for (int i = 0; i < size_x * size_y; i++)
                    pDst[i] = static_cast<float>(pDst[i] * one_over_sum);

                if (flags & cComputeGaussianFlagNormalizeCenterToOne)
                    pDst[mid_x + mid_y * size_x] = 1.0f;
            }

            if (flags & cComputeGaussianFlagPrint)
            {
                printf("{\n");
                for (int y = 0; y < size_y; y++)
                {
                    printf("  ");
                    for (int x = 0; x < size_x; x++)
                    {
                        printf("%f, ", pDst[x + y * size_x]);
                    }
                    printf("\n");
                }
                printf("}");
            }
        }

    } // namespace math
} // namespace vogl
