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

// File: vogl_mergesort.h
// This isn't very fast, and requires extra storage, but it's a reasonably fast stable sort with no degenerate cases.
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename T>
    struct mergesort_use_swaps_during_merge
    {
        enum
        {
            cFlag = false
        };
    };

#define VOGL_MERGESORT_USE_SWAPS_DURING_MERGE(T)         \
    template <> struct mergesort_use_swaps_during_merge<T> \
    {                                                      \
        enum { cFlag = true };                             \
    };

    VOGL_MERGESORT_USE_SWAPS_DURING_MERGE(dynamic_string);

    namespace detail
    {

        template <typename T, typename Comparator>
        inline void BottomUpMerge(vogl::vector<T> &A, uint32_t iLeft, uint32_t iRight, uint32_t iEnd, vogl::vector<T> &B, Comparator comp)
        {
            uint32_t i0 = iLeft;
            uint32_t i1 = iRight;

#if 0
	/* While there are elements in the left or right lists */
	for (uint32_t j = iLeft; j < iEnd; j++)
	{
		/* If left list head exists and is <= existing right list head */
		if ((i0 < iRight) && ((i1 >= iEnd) || (!comp(A[i1], A[i0]))))
		{
			B[j] = A[i0];
			++i0;
		}
		else
		{
			B[j] = A[i1];
			++i1;
		}
	}
#else
            uint32_t j = iLeft;

            if ((i0 < iRight) && (i1 < iEnd))
            {
                while (j < iEnd)
                {
                    if (!comp(A[i1], A[i0]))
                    {
                        B[j++] = A[i0++];
                        if (i0 >= iRight)
                            goto special;
                    }
                    else
                    {
                        B[j++] = A[i1++];
                        if (i1 >= iEnd)
                            goto special;
                    }
                }
                return;
            }

        special:
            while (j < iEnd)
            {
                if ((i0 < iRight) && ((i1 >= iEnd) || (!comp(A[i1], A[i0]))))
                    B[j++] = A[i0++];
                else
                    B[j++] = A[i1++];
            }
#endif
        }

        template <typename T, typename Comparator>
        inline void BottomUpMergeSwap(vogl::vector<T> &A, uint32_t iLeft, uint32_t iRight, uint32_t iEnd, vogl::vector<T> &B, Comparator comp)
        {
            uint32_t i0 = iLeft;
            uint32_t i1 = iRight;

#if 0
	/* While there are elements in the left or right lists */
	for (uint32_t j = iLeft; j < iEnd; j++)
	{
		/* If left list head exists and is <= existing right list head */
		if ((i0 < iRight) && ((i1 >= iEnd) || (!comp(A[i1], A[i0]))))
		{
			B[j] = A[i0];
			++i0;
		}
		else
		{
			B[j] = A[i1];
			++i1;
		}
	}
#else
            uint32_t j = iLeft;

            if ((i0 < iRight) && (i1 < iEnd))
            {
                while (j < iEnd)
                {
                    if (!comp(A[i1], A[i0]))
                    {
                        std::swap(B[j++], A[i0++]);
                        if (i0 >= iRight)
                            goto special;
                    }
                    else
                    {
                        std::swap(B[j++], A[i1++]);
                        if (i1 >= iEnd)
                            goto special;
                    }
                }
                return;
            }

        special:
            while (j < iEnd)
            {
                if ((i0 < iRight) && ((i1 >= iEnd) || (!comp(A[i1], A[i0]))))
                    std::swap(B[j++], A[i0++]);
                else
                    std::swap(B[j++], A[i1++]);
            }
#endif
        }

        /* array A[] has the items to sort; array B[] is a work array */
        template <typename T, typename Comparator>
        inline void BottomUpSort(vogl::vector<T> &A, Comparator comp)
        {
            const uint32_t n = A.size();

            vogl::vector<T> B(n);

            /* Each 1-element run in A is already "sorted". */

            /* Make successively longer sorted runs of length 2, 4, 8, 16... until whole array is sorted. */
            for (uint32_t width = 1; width < n; width = 2 * width)
            {
                /* Array A is full of runs of length width. */
                for (uint32_t i = 0; i < n; i = i + 2 * width)
                {
                    /* Merge two runs: A[i:i+width-1] and A[i+width:i+2*width-1] to B[] */
                    /* or copy A[i:n-1] to B[] ( if(i+width >= n) ) */
                    if (mergesort_use_swaps_during_merge<T>::cFlag)
                        BottomUpMergeSwap(A, i, math::minimum(i + width, n), math::minimum(i + 2 * width, n), B, comp);
                    else
                        BottomUpMerge(A, i, math::minimum(i + width, n), math::minimum(i + 2 * width, n), B, comp);
                }

                /* Now work array B is full of runs of length 2*width. */
                /* Swap array B to array A for next iteration. */
                A.swap(B);
                /* Now array A is full of runs of length 2*width. */
            }
        }

    } // namespace detail

    template <typename T, typename Comparator>
    inline void mergesort(vogl::vector<T> &elems, Comparator comp)
    {
        detail::BottomUpSort(elems, comp);
    }

    template <typename T>
    inline void mergesort(vogl::vector<T> &elems)
    {
        mergesort(elems, std::less<T>());
    }

} // namespace vogl

