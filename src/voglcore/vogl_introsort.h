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

// File: vogl_introsort.h
// Rewritten for competitive performance vs. glibc's std::sort by Rich Geldreich.
// Original author was Keith Schwarz (htiek@cs.stanford.edu), appeared on his website like this:
// "If you're interested in using any of this code in your applications, feel free to
// do so! You don't need to cite me or this website as a source, though I would appreciate
// it if you did."
//
// Needed for independence from any particular RTL, and because glibc's
// std::sort is now optimized for std::move. When not compiling for
// C++0x11 std::sort doesn't actually use std::swap, and all my stuff overloads
// std::swap for perf. So screw glibc.
// I've optimized this to compete against glib'c introsort, and
// eliminated the dependence on std::make_heap/sort_heap.
// Most importantly, the original pivot code was subtly flawed (way too many swaps).
// It's within +- ~1% of glibc's std::sort on int arrays.
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename RandomIterator>
    void introsort(RandomIterator begin, RandomIterator end);

    template <typename RandomIterator, typename Comparator>
    void introsort(RandomIterator begin, RandomIterator end, Comparator comp);

    namespace introsort_detail
    {
        template <class ForwardIt1, class ForwardIt2>
        inline void introsort_iter_swap(ForwardIt1 a, ForwardIt2 b)
        {
            using std::swap;
            swap(*a, *b);
        }

        template <typename T, typename F>
        inline void heapsort_helper(size_t size, T *a, F compare_func)
        {
            if (size <= 1U)
                return;

            size_t start = (size - 1 - 1) >> 1;
            for (;;)
            {
                size_t root = start;

                for (;;)
                {
                    size_t child = (root << 1) + 1;
                    if (child >= size)
                        break;

                    if (((child + 1U) < size) && (compare_func(a[child], a[child + 1])))
                        child++;

                    if (compare_func(a[root], a[child]))
                    {
                        std::swap(a[root], a[child]);
                        root = child;
                    }
                    else
                        break;
                }

                if (!start)
                    break;

                start--;
            }

            size_t end = size - 1;
            for (;;)
            {
                std::swap(a[end], a[0]);

                size_t root = 0;

                for (;;)
                {
                    size_t child = (root << 1) + 1;
                    if (child >= end)
                        break;

                    if (((child + 1U) < end) && (compare_func(a[child], a[child + 1])))
                        child++;

                    if (compare_func(a[root], a[child]))
                    {
                        std::swap(a[root], a[child]);
                        root = child;
                    }
                    else
                        break;
                }

                if (end <= 1U)
                    break;
                end--;
            }
        }

        template <typename RandomIterator, typename Comparator>
        inline void heapsort(RandomIterator begin, RandomIterator end, Comparator comp, size_t num_elems)
        {
            VOGL_NOTE_UNUSED(end);
            heapsort_helper(num_elems, begin, comp);
        }

        template <typename RandomIterator, typename Comparator>
        inline void heapsort(RandomIterator begin, RandomIterator end, Comparator comp)
        {
            heapsort(begin, end, comp, std::distance(begin, end));
        }

        template <typename RandomIterator>
        inline void heapsort(RandomIterator begin, RandomIterator end)
        {
            heapsort(begin, end, std::less<typename std::iterator_traits<RandomIterator>::value_type>());
        }

        // Important: Don't mess with this guy or you can subtly regress perf!
        template <typename RandomIterator, typename Comparator>
        inline RandomIterator partition(RandomIterator begin, RandomIterator end, Comparator comp)
        {
            RandomIterator lhs = begin + 1;
            RandomIterator rhs = end;
            const typename std::iterator_traits<RandomIterator>::value_type &pivot = *begin;

            for (;;)
            {
                // This is guaranteed to terminate within range because pivot is the median of 3 elements from the partition, so there must be an element >= pivot within the partition.
                while (comp(*lhs, pivot))
                    ++lhs;

                --rhs;

                // This is guaranteed to terminate within range because when rhs is begin (the pivot location) comp(pivot, pivot) is guaranteed to return false.
                while (comp(pivot, *rhs))
                    --rhs;

                if (!(lhs < rhs))
                    return lhs;

                introsort_iter_swap(lhs, rhs);
                ++lhs;
            }
        }

        // Swaps the median element to one.
        template <typename RandomIterator, typename Comparator>
        inline void median_of_three(RandomIterator one, RandomIterator two, RandomIterator three, Comparator comp)
        {
            const bool comp12 = comp(*one, *two);
            const bool comp23 = comp(*two, *three);

            if (comp12 && comp23)
            {
                // 1  < 2  < 3
                introsort_iter_swap(two, one);
                return;
            }

            const bool comp13 = comp(*one, *three);

            if (comp12 && !comp23 && comp13)
            {
                // 1  < 3 <= 2
                introsort_iter_swap(three, one);
                return;
            }

            if (!comp12 && comp13)
            {
                // 2 <= 1  < 3
                return;
            }

            if (!comp12 && !comp13 && comp23)
            {
                // 2 <  3 <= 1
                introsort_iter_swap(three, one);
                return;
            }

            if (comp12 && !comp13)
            {
                // 3 <= 1  < 2
                return;
            }

            // 3 <= 2 <= 1
            introsort_iter_swap(two, one);
        }

        template <typename RandomIterator, typename Comparator>
        inline void insertion_sort(RandomIterator begin, RandomIterator end, Comparator comp)
        {
            if (begin == end)
                return;

            for (RandomIterator itr = begin + 1; itr != end; ++itr)
            {
                for (RandomIterator test = itr; test != begin && comp(*test, *(test - 1)); --test)
                    introsort_iter_swap(test, test - 1);
            }
        }

        template <typename RandomIterator, typename Comparator>
        void introsort_rec(RandomIterator begin, RandomIterator end, size_t depth, Comparator comp)
        {
            const size_t kBlockSize = 16;

            for (;;)
            {
                const size_t numElems = size_t(end - begin);

                if (numElems < kBlockSize)
                {
                    // Seems a tiny bit faster to immediately do the insertion on the partition.
                    // Also avoids compares between partitions, which we know are already sorted
                    // relative to eachother.
                    insertion_sort(begin, end, comp);
                    return;
                }

                if (depth == 0)
                {
                    heapsort(begin, end, comp, numElems);
                    return;
                }

                median_of_three(begin, begin + (numElems >> 1), end - 1, comp);

                --depth;

                RandomIterator partition_point = introsort_detail::partition(begin, end, comp);

                // This is consistently slower on cheap objects (int)'s - dunno why yet.
                RandomIterator pivot_iter = partition_point - 1;

                // Move the pivot element into its final sorted position.
                if (pivot_iter != begin)
                    introsort_iter_swap(begin, pivot_iter);

// Now all elements < partition_point are <= the pivot element,
// and all elements >= partition_point are >= the pivot element,
// and the element right before partition_point (the pivot element)
// is in its final sorted position so we can exclude it from
// further sorting.
#if 0
		for (RandomIterator it = partition_point; it < end; ++it)
			VOGL_ASSERT(utils::compare_ge(*it, *pivot_iter, comp));

		for (RandomIterator it = begin; it < partition_point; ++it)
			VOGL_ASSERT(utils::compare_le(*it, *pivot_iter, comp));
#endif

                // Recurse into the smaller subpartition, tail call to larger.
                // Not a win on random int arrays, but a small win on complex objects.
                if ((pivot_iter - begin) < (end - (pivot_iter + 1)))
                {
                    introsort_rec(begin, pivot_iter, depth, comp);
                    begin = pivot_iter + 1;
                }
                else
                {
                    introsort_rec(pivot_iter + 1, end, depth, comp);
                    end = pivot_iter;
                }
            }
        }

        template <typename RandomIterator>
        inline size_t introsort_depth(RandomIterator begin, RandomIterator end)
        {
            size_t numElems = size_t(end - begin);

            size_t lg2 = 0;
            for (; numElems != 0; numElems >>= 1, ++lg2)
                ;

            return lg2 * 2;
        }

    } // namespace introsort_detail

    template <typename RandomIterator, typename Comparator>
    inline void introsort(RandomIterator begin, RandomIterator end, Comparator comp)
    {
        introsort_detail::introsort_rec(begin, end, introsort_detail::introsort_depth(begin, end), comp);

//introsort_detail::insertion_sort(begin, end, comp);

#ifdef VOGL_BUILD_DEBUG
        // I'm paranoid, there are way too many ways of subtly fucking this up.
        if (begin != end)
        {
            for (RandomIterator it = begin + 1; it != end; ++it)
                VOGL_ASSERT(utils::compare_le(*(it - 1), *it, comp));
        }
#endif
    }

    template <typename RandomIterator>
    inline void introsort(RandomIterator begin, RandomIterator end)
    {
        introsort(begin, end, std::less<typename std::iterator_traits<RandomIterator>::value_type>());
    }

    bool introsort_test();

} // namespace vogl

