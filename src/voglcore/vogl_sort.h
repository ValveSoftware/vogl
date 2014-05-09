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

// File: vogl_sort.h
// FIXME: The comparison functions are non-standard (they take an extra void * for user data, which is both good and bad).
// Also, most of the sorts here are kinda useless in practice (either use vogl_introsort.h or vogl_mergesort.h), except maybe the index sorts.
#pragma once

#include "vogl_core.h"
#include "vogl_rand.h"

namespace vogl
{
    template <typename T>
    inline bool default_compare_func(const T &lhs, const T &rhs, const void *pData)
    {
        VOGL_NOTE_UNUSED(pData);
        return lhs < rhs;
    }

    template <typename T, typename F>
    inline void insertion_sort(int size, T *a, F compare_func, void *pCompare_func_data = NULL)
    {
        for (int i = 1; i < size; i++)
        {
            const T value(a[i]);

            int j = i - 1;
            while ((j >= 0) && (compare_func(value, a[j], pCompare_func_data)))
            {
                a[j + 1] = a[j];
                j--;
            }

            a[j + 1] = value;
        }
    }

    template <typename T>
    inline void insertion_sort(int size, T *a)
    {
        insertion_sort(size, a, default_compare_func<T>, NULL);
    }

    template <typename T, typename F>
    inline void shell_sort(int size, T *a, F compare_func, void *pCompare_func_data = NULL)
    {
        static const uint32_t sequence[] = { 1, 4, 10, 23, 57, 132, 301, 701, 1577, 3548 };
        const uint32_t cSequenceSize = sizeof(sequence) / sizeof(sequence[0]);

        int s = cSequenceSize - 1;

        do
        {
            int increment = sequence[s];

            for (int i = increment; i < size; i++)
            {
                const T temp(a[i]);

                int j = i;
                while ((j >= increment) && (compare_func(temp, a[j - increment], pCompare_func_data)))
                {
                    a[j] = a[j - increment];
                    j = j - increment;
                }

                a[j] = temp;
            }

        } while (--s >= 0);
    }

    template <typename T>
    inline void shell_sort(int size, T *a)
    {
        shell_sort(size, a, default_compare_func<T>, NULL);
    }

    template <typename T, typename F>
    inline void heap_sort(int size, T *a, F compare_func, void *pCompare_func_data = NULL)
    {
        int start = (size - 2) >> 1;
        while (start >= 0)
        {
            int root = start;

            for (;;)
            {
                int child = (root << 1) + 1;
                if (child >= size)
                    break;

                if (((child + 1) < size) && (compare_func(a[child], a[child + 1], pCompare_func_data)))
                    child++;

                if (compare_func(a[root], a[child], pCompare_func_data))
                {
                    utils::swap(a[root], a[child]);
                    root = child;
                }
                else
                    break;
            }

            start--;
        }

        int end = size - 1;
        while (end > 0)
        {
            utils::swap(a[end], a[0]);

            int root = 0;

            for (;;)
            {
                int child = (root << 1) + 1;
                if (child >= end)
                    break;

                if (((child + 1) < end) && (compare_func(a[child], a[child + 1], pCompare_func_data)))
                    child++;

                if (compare_func(a[root], a[child], pCompare_func_data))
                {
                    utils::swap(a[root], a[child]);
                    root = child;
                }
                else
                    break;
            }

            end--;
        }
    }

    template <typename T>
    inline void heap_sort(int size, T *a)
    {
        heap_sort(size, a, default_compare_func<T>, NULL);
    }

    template <typename T, typename F>
    inline void indexed_heap_sort(int size, const T *a, uint32_t *pIndices, F compare_func, void *pCompare_func_data = NULL)
    {
        int start = (size - 2) >> 1;
        while (start >= 0)
        {
            int root = start;

            for (;;)
            {
                int child = (root << 1) + 1;
                if (child >= size)
                    break;

                if (((child + 1) < size) && (compare_func(a[pIndices[child]], a[pIndices[child + 1]], pCompare_func_data)))
                    child++;

                if (compare_func(a[pIndices[root]], a[pIndices[child]], pCompare_func_data))
                {
                    utils::swap(pIndices[root], pIndices[child]);
                    root = child;
                }
                else
                    break;
            }

            start--;
        }

        int end = size - 1;
        while (end > 0)
        {
            utils::swap(pIndices[end], pIndices[0]);

            int root = 0;

            for (;;)
            {
                int child = (root << 1) + 1;
                if (child >= end)
                    break;

                if (((child + 1) < end) && (compare_func(a[pIndices[child]], a[pIndices[child + 1]], pCompare_func_data)))
                    child++;

                if (compare_func(a[pIndices[root]], a[pIndices[child]], pCompare_func_data))
                {
                    utils::swap(pIndices[root], pIndices[child]);
                    root = child;
                }
                else
                    break;
            }

            end--;
        }
    }

    template <typename T>
    inline void indexed_heap_sort(int size, const T *a, uint32_t *pIndices)
    {
        indexed_heap_sort(size, a, pIndices, default_compare_func<T>, NULL);
    }

    inline bool sort_test()
    {
        uint32_t num_failures = 0;

        vogl::random r;
        r.seed(2);
        for (uint32_t trial = 0; trial < 50000; trial++)
        {
            uint32_t sz = r.irand_inclusive(1, 8192);
            uint32_t z = r.irand_inclusive(2, 8192);
            printf("%u\n", sz);

            vogl::vector<uint32_t> x(sz);
            for (uint32_t i = 0; i < x.size(); i++)
                x[i] = r.irand(0, z);

            vogl::vector<uint32_t> indices(sz);
            for (uint32_t i = 0; i < sz; i++)
                indices[i] = i;

            bool used_indices = false;

            switch (r.irand_inclusive(0, 3))
            {
                case 0:
                    vogl::insertion_sort(x.size(), x.get_ptr());
                    break;
                case 1:
                    vogl::shell_sort(x.size(), x.get_ptr());
                    break;
                case 2:
                    vogl::heap_sort(x.size(), x.get_ptr());
                    break;
                case 3:
                    vogl::indexed_heap_sort(x.size(), x.get_ptr(), indices.get_ptr());
                    used_indices = true;
                    break;
            }

            if (used_indices)
            {
                for (uint32_t i = 1; i < x.size(); i++)
                {
                    if (x[indices[i - 1]] > x[indices[i]])
                    {
                        num_failures++;
                        break;
                    }
                }
            }
            else
            {
                for (uint32_t i = 1; i < x.size(); i++)
                {
                    if (x[i - 1] > x[i])
                    {
                        num_failures++;
                        break;
                    }
                }
            }
        }

        return num_failures == 0;
    }

} // namespace vogl
