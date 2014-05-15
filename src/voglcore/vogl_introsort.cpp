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

// File: vogl_introsort.cpp
#include "vogl_core.h"
#include "vogl_introsort.h"
#include "vogl_sort.h"
#include "vogl_mergesort.h"
#include "vogl_radix_sort.h"

namespace vogl
{
    struct sort_test_struct
    {
        uint32_t i;
        uint32_t j;
        bool operator<(const sort_test_struct &rhs) const
        {
            return i < rhs.i;
        }
    };

    bool introsort_test()
    {
        random rnd;
        vogl::vector<int> x;
        vogl::vector<int> y;

        rnd.seed(1000);

        double std_sort_total_time = 0;
        double introsort_total_time = 0;
        double heapsort_total_time = 0;
        double shellsort_total_time = 0;
        double mergesort_total_time = 0;
        double radixsort_total_time = 0;

        double string_std_sort_total_time = 0;
        double string_introsort_total_time = 0;
        double string_mergesort_total_time = 0;

        for (uint32_t t = 0; t < 1000; t++)
        {
            uint32_t n = rnd.irand_inclusive(0, 100000);

            x.resize(n);

            uint32_t k = static_cast<uint32_t>((1ULL << rnd.irand_inclusive(0, 31)) - 1UL);
            for (uint32_t i = 0; i < n; i++)
                x[i] = rnd.irand_inclusive(0, k);

            if (rnd.irand(0, 30) == 0)
            {
                x.sort();
                if (rnd.get_bit())
                    x.reverse();
            }

            printf("------------------------------\n");
            printf("iter: %u, n: %u:\n", t, n);

            y = x;
            {
                vogl::timed_scope tm("introsort");
                introsort(y.begin(), y.end());
                introsort_total_time += tm.get_elapsed_secs();
            }
            if (!y.is_sorted())
                return false;

            y = x;
            {
                vogl::timed_scope tm("std::sort");
                std::sort(y.begin(), y.end());
                std_sort_total_time += tm.get_elapsed_secs();
            }
            if (!y.is_sorted())
                return false;

            y = x;
            {
                vogl::timed_scope tm("heap_sort");
                heap_sort(y.size(), y.get_ptr());
                heapsort_total_time += tm.get_elapsed_secs();
            }
            if (!y.is_sorted())
                return false;

            y = x;
            {
                vogl::timed_scope tm("shell_sort");
                shell_sort(y.size(), y.get_ptr());
                shellsort_total_time += tm.get_elapsed_secs();
            }
            if (!y.is_sorted())
                return false;

            y = x;
            {
                vogl::timed_scope tm("merge_sort");
                vogl::mergesort(y);
                mergesort_total_time += tm.get_elapsed_secs();
            }
            if (!y.is_sorted())
                return false;

            y = x;
            {
                vogl::timed_scope tm("radix_sort");

                vogl::vector<int> z(n);
                int *pSorted = radix_sort(n, y.get_ptr(), z.get_ptr(), 0, sizeof(int));

                for (uint32_t i = 1; i < n; i++)
                    if (pSorted[i] < pSorted[i - 1])
                        return false;

                radixsort_total_time += tm.get_elapsed_secs();
            }

            printf("%u, std::sort: %f introsort: %f, heapsort: %f, shellsort: %f, mergesort: %f, radixsort: %f\n", n, std_sort_total_time, introsort_total_time, heapsort_total_time, shellsort_total_time, mergesort_total_time, radixsort_total_time);

            // mergesort stability test
            // TODO: This doesn't below here
            vogl::vector<sort_test_struct> xx(n);
            for (uint32_t i = 0; i < n; i++)
            {
                xx[i].i = x[i] & 0xFFFF;
                xx[i].j = i;
            }

            vogl::mergesort(xx);

            for (uint32_t i = 1; i < n; i++)
            {
                if (xx[i - 1].i > xx[i].i)
                    return false;
                if (xx[i - 1].i == xx[i].i)
                {
                    if (xx[i - 1].j >= xx[i].j)
                        return false;
                }
            }

            // string test
            dynamic_string_array sx(n);
            for (uint32_t i = 0; i < n; i++)
                sx[i].format("%u", x[i]);

            dynamic_string_array sy;

            sy = sx;
            {
                vogl::timed_scope tm("introsort string");
                introsort(sy.begin(), sy.end());
                string_introsort_total_time += tm.get_elapsed_secs();
            }
            if (!y.is_sorted())
                return false;

            sy = sx;
            {
                vogl::timed_scope tm("std::sort string");
                std::sort(sy.begin(), sy.end());
                string_std_sort_total_time += tm.get_elapsed_secs();
            }
            if (!sy.is_sorted())
                return false;

            sy = sx;
            {
                vogl::timed_scope tm("merge_sort string");
                vogl::mergesort(sy);
                string_mergesort_total_time += tm.get_elapsed_secs();
            }
            if (!sy.is_sorted())
                return false;

            printf("%u, string std::sort: %f introsort: %f mergesort: %f\n", n, string_std_sort_total_time, string_introsort_total_time, string_mergesort_total_time);
        }

        return true;
    }

} // namespace vogl
