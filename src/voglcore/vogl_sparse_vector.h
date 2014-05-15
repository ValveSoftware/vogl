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

// File: vogl_sparse_vector.h
#pragma once

#include "vogl_core.h"
#include "vogl_rand.h"
#include "vogl_strutils.h"
#include <vector>

namespace vogl
{
    template <typename T, uint32_t Log2N>
    class sparse_vector_traits
    {
    public:
        static inline void *alloc_space(uint32_t size)
        {
            return vogl_malloc(size);
        }

        static inline void free_space(void *p)
        {
            vogl_free(p);
        }

        static inline void construct_group(T *p)
        {
            scalar_type<T>::construct_array(p, 1U << Log2N);
        }

        static inline void destruct_group(T *p)
        {
            scalar_type<T>::destruct_array(p, 1U << Log2N);
        }

        static inline void construct_element(T *p)
        {
            scalar_type<T>::construct(p);
        }

        static inline void destruct_element(T *p)
        {
            scalar_type<T>::destruct(p);
        }

        static inline void copy_group(T *pDst, const T *pSrc)
        {
            for (uint32_t j = 0; j < (1U << Log2N); j++)
                pDst[j] = pSrc[j];
        }
    };

    template <typename T, uint32_t Log2N, template <typename, uint32_t> class Traits = sparse_vector_traits>
    class sparse_vector : public Traits<T, Log2N>
    {
    public:
        typedef Traits<T, Log2N> base;

        enum
        {
            N = 1U << Log2N
        };
        typedef vogl::vector<T *> group_ptr_vec;

        inline sparse_vector()
            : m_size(0), m_num_active_groups(0)
        {
            init_default();
        }

        inline sparse_vector(uint32_t size)
            : m_size(0), m_num_active_groups(0)
        {
            init_default();

            resize(size);
        }

        inline sparse_vector(const sparse_vector &other)
            : m_size(0), m_num_active_groups(0)
        {
            init_default();

            *this = other;
        }

        template <uint32_t OtherLogN, template <typename, uint32_t> class OtherTraits>
        inline sparse_vector(const sparse_vector<T, OtherLogN, OtherTraits> &other)
            : m_size(0), m_num_active_groups(0)
        {
            init_default();

            default_element() = other.default_element();

            *this = other;
        }

        inline sparse_vector(const vogl::vector<T> &other)
            : m_size(0), m_num_active_groups(0)
        {
            init_default();

            *this = other;
        }

        inline ~sparse_vector()
        {
            for (uint32_t i = 0; (i < m_groups.size()) && m_num_active_groups; i++)
                free_group(m_groups[i]);

            deinit_default();
        }

        // Get/set the default object returned by operator [] on non-present elements.
        inline const T &default_element() const
        {
            return *reinterpret_cast<const T *>(m_default);
        }
        inline T &default_element()
        {
            return *reinterpret_cast<T *>(m_default);
        }

        inline bool assign(const sparse_vector &other)
        {
            if (this == &other)
                return true;

            if (!try_resize(other.size()))
                return false;

            for (uint32_t i = 0; i < other.m_groups.size(); i++)
            {
                const T *p = other.m_groups[i];

                T *q = m_groups[i];

                if (p)
                {
                    if (!q)
                    {
                        q = alloc_group(true);
                        if (!q)
                            return false;

                        m_groups[i] = q;
                    }

                    base::copy_group(q, p);
                }
                else if (q)
                {
                    free_group(q);
                    m_groups[i] = NULL;
                }
            }

            return true;
        }

        sparse_vector &operator=(const sparse_vector &other)
        {
            if (!assign(other))
            {
                VOGL_FAIL("Out of memory");
            }

            return *this;
        }

        template <uint32_t OtherLogN, template <typename, uint32_t> class OtherTraits>
        inline bool assign(const sparse_vector<T, OtherLogN, OtherTraits> &other)
        {
            if ((void *)this == (void *)&other)
                return true;

            if (!try_resize(other.size()))
                return true;

            for (uint32_t i = 0; i < other.size(); ++i)
                if (other.is_present(i))
                    set(i, *other.get_ptr(i));

            return true;
        }

        template <uint32_t OtherLogN, template <typename, uint32_t> class OtherTraits>
        inline sparse_vector &operator=(const sparse_vector<T, OtherLogN, OtherTraits> &other)
        {
            if (!assign(other))
            {
                VOGL_FAIL("Out of memory");
            }

            return *this;
        }

        inline bool assign(const vogl::vector<T> &other, bool default_element_check)
        {
            if (!try_resize(other.size()))
                return true;

            if (default_element_check)
            {
                allocate_all_groups();

                const T *pSrc = other.get_ptr();
                const uint32_t n = other.size();
                for (uint32_t i = 0; i < n; ++i)
                    m_groups[i >> Log2N][i & (N - 1)] = *pSrc++;
            }
            else
            {
                for (uint32_t i = 0; i < other.size(); ++i)
                    if (other[i] != default_element())
                        set(i, other[i]);
            }

            return true;
        }

        inline bool operator=(const vogl::vector<T> &other)
        {
            if (!assign(other, true))
            {
                VOGL_FAIL("Out of memory");
            }
            return true;
        }

        bool operator==(const sparse_vector &other) const
        {
            if (this == &other)
                return true;

            if (m_size != other.m_size)
                return false;

            for (uint32_t i = 0; i < m_size; i++)
                if (!((*this)[i] == other[i]))
                    return false;

            return true;
        }

        bool operator!=(const sparse_vector &other) const
        {
            return !(*this == other);
        }

        bool operator<(const sparse_vector &rhs) const
        {
            const uint32_t min_size = math::minimum(m_size, rhs.m_size);

            uint32_t i;
            for (i = 0; i < min_size; i++)
                if (!((*this)[i] == rhs[i]))
                    break;

            if (i < min_size)
                return (*this)[i] < rhs[i];

            return m_size < rhs.m_size;
        }

        void clear()
        {
            if (m_groups.size())
            {
                for (uint32_t i = 0; (i < m_groups.size()) && m_num_active_groups; i++)
                    free_group(m_groups[i]);

                m_groups.clear();
            }

            m_size = 0;

            VOGL_ASSERT(!m_num_active_groups);
        }

        bool get_vector(vogl::vector<T> &dst) const
        {
            if (!dst.try_resize(size()))
                return false;

            const T *const *ppGroups = m_groups.get_ptr();
            const uint32_t num_groups = m_groups.size();
            for (uint32_t group_iter = 0; group_iter < num_groups; ++group_iter)
            {
                const T *pGroup = ppGroups[group_iter];
                if (!pGroup)
                    continue;

                const uint32_t n = get_num_valid_elements_in_group(group_iter);
                T *pDst = &dst[group_iter << Log2N];

                for (uint32_t i = 0; i < n; ++i)
                    *pDst++ = *pGroup++;
            }

            return true;
        }

        bool try_resize(uint32_t new_size)
        {
            if (m_size == new_size)
                return true;

            uint32_t new_last_group_index = 0;
            uint32_t prev_num_valid_elements_in_new_last_group = 0;
            bool shrinking = ((new_size) && (new_size < m_size));
            if (shrinking)
            {
                new_last_group_index = (new_size - 1) >> Log2N;
                prev_num_valid_elements_in_new_last_group = get_num_valid_elements_in_group(new_last_group_index);
                VOGL_ASSERT(prev_num_valid_elements_in_new_last_group);
            }

            const uint32_t new_num_groups = (new_size + N - 1) >> Log2N;
            if (new_num_groups != m_groups.size())
            {
                for (uint32_t i = new_num_groups; i < m_groups.size(); i++)
                    free_group(m_groups[i]);

                if (!m_groups.try_resize(new_num_groups))
                    return false;
            }

            m_size = new_size;

            if (shrinking)
            {
                T *pLast_group = m_groups[new_last_group_index];
                if (pLast_group)
                {
                    uint32_t cur_num_valid_elements_in_new_last_group = get_num_valid_elements_in_group(new_last_group_index);
                    VOGL_ASSERT(cur_num_valid_elements_in_new_last_group);

                    if (cur_num_valid_elements_in_new_last_group < prev_num_valid_elements_in_new_last_group)
                    {
                        for (uint32_t i = cur_num_valid_elements_in_new_last_group; i < prev_num_valid_elements_in_new_last_group; i++)
                            pLast_group[i] = default_element();
                    }
                }
            }

            return true;
        }

        void resize(uint32_t size)
        {
            if (!try_resize(size))
            {
                VOGL_FAIL("Out of memory");
            }
        }

        inline uint32_t size() const
        {
            return m_size;
        }
        inline bool is_empty() const
        {
            return 0 == m_size;
        }

        inline uint32_t capacity() const
        {
            return m_groups.size() << Log2N;
        }
        inline uint32_t actual_capacity() const
        {
            return m_num_active_groups << Log2N;
        }

        // Returns the default object if the element does not yet exist.
        // at() will not force elements to be allocated if they don't exist, preventing accidents.
        inline const T &at(uint32_t i) const
        {
            VOGL_ASSERT(i < m_size);
            const T *p = m_groups[i >> Log2N];
            return p ? p[i & (N - 1)] : *reinterpret_cast<const T *>(m_default);
        }

        // Returns the default object if the element does not yet exist.
        // operator [] is read only, i.e. will not force elements to be allocated if they don't exist, preventing accidents.
        inline const T &operator[](uint32_t i) const
        {
            return at(i);
        }

        class sparse_vector_object_accessor
        {
            sparse_vector &m_array;
            uint32_t m_index;

        public:
            inline sparse_vector_object_accessor(sparse_vector &array, uint32_t index)
                : m_array(array), m_index(index)
            {
            }
            inline sparse_vector_object_accessor(const sparse_vector_object_accessor &other)
                : m_array(other.m_array), m_index(other.m_index)
            {
            }

            // Returns const ref to element, or the default element if it's not allocated.
            inline operator const T &() const
            {
                return m_array.get_element(m_index);
            }

            // Taking the address always forces the element to be allocated.
            inline T *operator&() const
            {
                T *p = m_array.get_ptr(m_index);
                if (!p)
                    p = m_array.ensure_present(m_index);
                return p;
            }

            // Assigns element, forcing it to be allocated if it doesn't exist.
            inline T &operator=(const T &other) const
            {
                T *p = m_array.get_ptr(m_index);
                if (!p)
                    p = m_array.ensure_present(m_index);
                *p = other;
                return *p;
            }
        };

        // Yes this is ugly, but it works around the fact that we can't overload operator "." in C++.
        // So operator[] and at() are read-only (and never force elements to be allocated, preventing silly accidents), and operator () is read/write (it will write on operator =).
        // I'm torn about this guy, it's probably too confusing vs. just calling get_or_create_element(), get_ptr(), etc.
        inline const sparse_vector_object_accessor operator()(uint32_t i)
        {
            VOGL_ASSERT(i < m_size);
            return sparse_vector_object_accessor(*this, i);
        }

        //	Returns a ref to the element (allocating it if necessary).
        inline T &get_or_create_element(uint32_t i)
        {
            VOGL_ASSERT(i < m_size);

            T *p = m_groups[i >> Log2N];
            if (!p)
                return *ensure_present(i);

            return p[i & (N - 1)];
        }

        // Returns NULL if element does not yet exist.
        inline const T *get_ptr(uint32_t i) const
        {
            VOGL_ASSERT(i < m_size);
            const T *p = m_groups[i >> Log2N];
            return p ? &p[i & (N - 1)] : NULL;
        }

        // Returns NULL if element does not yet exist.
        inline T *get_ptr(uint32_t i)
        {
            VOGL_ASSERT(i < m_size);
            T *p = m_groups[i >> Log2N];
            return p ? &p[i & (N - 1)] : NULL;
        }

        // Returns true if the element exists.
        inline bool is_present(uint32_t i) const
        {
            VOGL_ASSERT(i < m_size);
            return m_groups[i >> Log2N] != NULL;
        }

        // Ensures element is allocated/present (enlarging the entire array if needed).
        inline T *ensure_present(uint32_t index)
        {
            const uint32_t group_index = index >> Log2N;
            if (group_index >= m_groups.size())
            {
                if (!try_resize(index + 1))
                    return NULL;
            }

            T *p = m_groups[group_index];
            if (!p)
            {
                p = alloc_group(true);
                if (!p)
                    return NULL;

                m_groups[group_index] = p;
            }

            m_size = math::maximum(index + 1, m_size);

            return p + (index & (N - 1));
        }

        inline bool set(uint32_t index, const T &obj)
        {
            T *p = ensure_present(index);
            if (!p)
                return false;

            *p = obj;

            return true;
        }

        inline void push_back(const T &obj)
        {
            if (!set(m_size, obj))
            {
                VOGL_FAIL("Out of memory");
            }
        }

        inline bool try_push_back(const T &obj)
        {
            return set(m_size, obj);
        }

        inline T *enlarge(uint32_t n)
        {
            uint32_t orig_size = m_size;
            for (uint32_t i = 0; i < n; i++)
            {
                if (!ensure_present(m_size))
                {
                    VOGL_FAIL("Out of memory");
                }
            }
            return get_ptr(orig_size);
        }

        inline void pop_back()
        {
            VOGL_ASSERT(m_size);
            if (m_size)
                resize(m_size - 1);
        }

        // Sets range [start, start + num) to the default object, freeing any groups that it can
        inline void invalidate_range(uint32_t start, uint32_t num)
        {
            if (!num)
                return;

            if ((start + num) > m_size)
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            const uint32_t num_to_skip = math::minimum(math::get_align_up_value_delta(start, N), num);

            for (uint32_t i = 0; i < num_to_skip; i++)
            {
                if (is_present(start + i))
                    set(start + i, default_element());
            }

            start += num_to_skip;
            num -= num_to_skip;

            const uint32_t first_group_to_free = start >> Log2N;
            const uint32_t num_groups_to_free = num >> Log2N;

            free_groups(first_group_to_free, num_groups_to_free);

            const uint32_t total_elements_freed = num_groups_to_free << Log2N;
            start += total_elements_freed;
            num -= total_elements_freed;

            for (uint32_t i = 0; i < num; i++)
            {
                if (is_present(start + i))
                    set(start + i, default_element());
            }
        }

        inline void invalidate_all()
        {
            for (uint32_t i = 0; i < m_groups.size(); i++)
            {
                T *p = m_groups[i];
                if (p)
                {
                    free_group(p);
                    m_groups[i] = NULL;
                }
            }
        }

        inline void swap(sparse_vector &other)
        {
            utils::swap(m_size, other.m_size);
            m_groups.swap(other.m_groups);
            utils::swap(m_num_active_groups, other.m_num_active_groups);
        }

        inline bool set_all(const T &val)
        {
            if (is_empty())
                return true;

            const bool is_default = (val == default_element());

            for (uint32_t group_iter = 0; group_iter < m_groups.size(); ++group_iter)
            {
                const T *pGroup = m_groups[group_iter];
                if (!pGroup)
                {
                    if (is_default)
                        continue;

                    pGroup = alloc_group(true);
                    if (!pGroup)
                        return false;

                    m_groups[group_iter] = pGroup;
                }

                const uint32_t n = get_num_valid_elements_in_group(group_iter);
                for (uint32_t i = 0; i < n; i++)
                    pGroup[i] = val;
            }

            return true;
        }

        inline int find(const T &key) const
        {
            if (is_empty())
                return cInvalidIndex;

            const bool is_default = (key == default_element());

            for (uint32_t group_iter = 0; group_iter < m_groups.size(); ++group_iter)
            {
                const T *pGroup = m_groups[group_iter];
                if (!pGroup)
                {
                    if (is_default)
                        return group_iter << Log2N;
                    continue;
                }

                const uint32_t n = get_num_valid_elements_in_group(group_iter);
                for (uint32_t i = 0; i < n; i++)
                    if (pGroup[i] == key)
                        return (group_iter << Log2N) + i;
            }

            return cInvalidIndex;
        }

        inline uint32_t allocate_all_groups()
        {
            if (m_num_active_groups == m_groups.size())
                return 0;

            uint32_t num_groups_allocated = 0;

            for (uint32_t group_iter = 0; group_iter < m_groups.size(); ++group_iter)
            {
                T *pGroup = m_groups[group_iter];
                if (pGroup)
                    continue;

                m_groups[group_iter] = alloc_group();
                num_groups_allocated++;
            }

            return num_groups_allocated;
        }

        inline uint32_t optimize_groups()
        {
            if (!m_num_active_groups)
                return 0;

            uint32_t num_groups_freed = 0;

            for (uint32_t group_iter = 0; group_iter < m_groups.size(); ++group_iter)
            {
                T *pGroup = m_groups[group_iter];
                if (!pGroup)
                    continue;

                const uint32_t n = get_num_valid_elements_in_group(group_iter);

                uint32_t i;
                for (i = 0; i < n; i++)
                    if (!(pGroup[i] == default_element()))
                        break;

                if (i == n)
                {
                    free_group(pGroup);
                    m_groups[group_iter] = NULL;

                    ++num_groups_freed;
                }
            }

            return num_groups_freed;
        }

        inline bool check() const
        {
            if (m_groups.size() != ((m_size + (N - 1)) >> Log2N))
                return false;

            vogl::vector<T *> group_ptrs;
            uint32_t total_active_groups = 0;
            for (uint32_t group_iter = 0; group_iter < m_groups.size(); ++group_iter)
            {
                T *pGroup = m_groups[group_iter];
                if (reinterpret_cast<intptr_t>(pGroup) & (VOGL_MIN_ALLOC_ALIGNMENT - 1))
                    return false;

                if (pGroup)
                {
                    total_active_groups++;
                    group_ptrs.push_back(pGroup);
                }
            }

            if (total_active_groups != m_num_active_groups)
                return false;

            group_ptrs.sort();
            group_ptrs.unique();
            if (group_ptrs.size() != total_active_groups)
                return false;

            if (m_groups.size())
            {
                uint32_t t = 0;
                for (uint32_t i = 0; i < (m_groups.size() - 1); i++)
                {
                    uint32_t n = get_num_valid_elements_in_group(i);
                    if (n != N)
                        return false;
                    t += n;
                }

                uint32_t num_valid_in_last_group = get_num_valid_elements_in_group(m_groups.size() - 1);
                t += num_valid_in_last_group;
                if (t != m_size)
                    return false;

                if (!num_valid_in_last_group)
                    return false;
                if (num_valid_in_last_group > N)
                    return false;

                const T *pLast_group = m_groups.back();
                if (pLast_group)
                {
                    for (uint32_t i = num_valid_in_last_group; i < N; i++)
                    {
                        if (!(pLast_group[i] == default_element()))
                            return false;
                    }
                }
            }

            return true;
        }

        inline void copy_element(uint32_t dst_index, uint32_t src_index)
        {
            if (!is_present(src_index))
            {
                if (is_present(dst_index))
                    set(dst_index, default_element());
            }
            else
            {
                set(dst_index, *get_ptr(src_index));
            }
        }

        inline void erase(uint32_t start, uint32_t n)
        {
            VOGL_ASSERT((start + n) <= m_size);
            if ((start + n) > m_size)
                return;

            if (!n)
                return;

            const uint32_t orig_num_groups = m_groups.size();

            uint32_t num_to_move = m_size - (start + n);
            uint32_t dst_index = start;
            uint32_t src_index = start + n;

            while ((num_to_move) && ((dst_index & (N - 1)) != 0))
            {
                copy_element(dst_index, src_index);

                ++src_index;
                ++dst_index;
                --num_to_move;
            }

            if (num_to_move)
            {
                // dst_index must be aligned to the beginning of a group here.
                VOGL_ASSERT((dst_index & (N - 1)) == 0);

                const uint32_t move_gap_size = src_index - dst_index;

                // If the move gap is >= the group size, then we can just free all the groups in the middle of the move gap that are overridden (and bulk erase the group ptrs array).
                if (move_gap_size >= N)
                {
                    uint32_t first_full_group, num_full_groups;
                    get_full_group_range(dst_index, math::minimum(num_to_move, move_gap_size), first_full_group, num_full_groups);

                    if (num_full_groups)
                    {
                        const uint32_t total_elements_to_free = num_full_groups << Log2N;

                        VOGL_ASSERT(total_elements_to_free <= num_to_move);
                        VOGL_ASSERT(dst_index <= (first_full_group << Log2N));
                        VOGL_ASSERT(src_index >= total_elements_to_free);

                        free_groups(first_full_group, num_full_groups);

                        m_groups.erase(first_full_group, num_full_groups);
                        m_groups.resize(orig_num_groups);

                        //VOGL_ASSERT(check());

                        src_index -= total_elements_to_free;
                    }
                }

                while (num_to_move)
                {
                    if (!is_present(src_index))
                    {
                        if ((src_index & (N - 1)) == 0)
                        {
                            uint32_t num_not_present = math::minimum<uint32_t>(num_to_move, N);

                            // We know up to the next N src elements are not present, so there's no need to check them.
                            VOGL_ASSERT(!is_present(src_index + num_not_present - 1));

                            src_index += num_not_present;
                            num_to_move -= num_not_present;

                            while (num_not_present)
                            {
                                if (is_present(dst_index))
                                    set(dst_index, default_element());

                                ++dst_index;
                                --num_not_present;
                            }
                            continue;
                        }

                        if (is_present(dst_index))
                            set(dst_index, default_element());
                    }
                    else
                    {
                        set(dst_index, *get_ptr(src_index));
                    }

                    ++src_index;
                    ++dst_index;
                    --num_to_move;
                }
            }

            resize(m_size - n);
        }

        inline void insert(uint32_t index, const T *p, uint32_t n, bool check_for_default_elements_while_copying = true)
        {
            VOGL_ASSERT(index <= m_size);
            if (!n)
                return;

            const uint32_t orig_size = m_size;
            resize(m_size + n);

            uint32_t num_to_copy = orig_size - index;

            uint32_t src_index = orig_size - 1;
            uint32_t dst_index = src_index + n;

            while (num_to_copy)
            {
                copy_element(dst_index, src_index);

                --src_index;
                --dst_index;
                --num_to_copy;
            }

            dst_index = index;

            if (check_for_default_elements_while_copying)
            {
                while (n)
                {
                    if (!(*p == default_element()))
                        set(dst_index, *p);

                    ++dst_index;
                    ++p;
                    --n;
                }
            }
            else
            {
                while (n)
                {
                    set(dst_index++, *p++);
                    --n;
                }
            }
        }

        inline void insert(uint32_t index, const T &obj)
        {
            insert(index, &obj, 1);
        }

        inline void push_front(const T &obj)
        {
            insert(0, &obj, 1);
        }

        sparse_vector &append(const sparse_vector &other)
        {
            VOGL_ASSERT(this != &other);

            if (other.m_size)
                insert(m_size, &other[0], other.m_size);
            return *this;
        }

        sparse_vector &append(const T *p, uint32_t n)
        {
            insert(m_size, p, n);
            return *this;
        }

        // low-level element group access
        inline uint32_t get_num_groups() const
        {
            return m_groups.size();
        }
        inline uint32_t get_num_active_groups() const
        {
            return m_num_active_groups;
        }
        inline uint32_t get_group_size() const
        {
            return N;
        }

        inline bool is_last_group(uint32_t group_index) const
        {
            return (static_cast<int>(group_index) == (static_cast<int>(m_groups.size()) - 1));
        }

        inline uint32_t get_group_index(uint32_t element_index)
        {
            return element_index >> Log2N;
        }
        inline uint32_t get_group_offset(uint32_t element_index)
        {
            return element_index & (N - 1);
        }

        inline const T *get_group_ptr(uint32_t group_index) const
        {
            return m_groups[group_index];
        }
        inline T *get_group_ptr(uint32_t group_index)
        {
            return m_groups[group_index];
        }

        const group_ptr_vec &get_group_ptr_vec() const
        {
            return m_groups;
        }
        group_ptr_vec &get_group_ptr_vec()
        {
            return m_groups;
        }

        inline bool is_group_present(uint32_t group_index) const
        {
            return get_group_ptr(group_index) != NULL;
        }

        inline uint32_t get_num_valid_elements_in_group(uint32_t group_index) const
        {
            VOGL_ASSERT(group_index < m_groups.size());

            if (!m_groups.size())
                return 0;

            uint32_t n = 0;
            if (static_cast<int>(group_index) == (static_cast<int>(m_groups.size()) - 1))
                n = m_size & (N - 1);

            return n ? n : static_cast<uint32_t>(N);
        }

    private:
        uint32_t m_size;
        uint32_t m_num_active_groups;

        group_ptr_vec m_groups;

        uint64_t m_default[(sizeof(T) + sizeof(uint64_t) - 1) / sizeof(uint64_t)];

        inline T *alloc_group(bool nofail = false)
        {
            T *p = static_cast<T *>(base::alloc_space(N * sizeof(T)));

            if (!p)
            {
                if (nofail)
                    return NULL;

                VOGL_FAIL("Out of memory");
            }

            base::construct_group(p);

            m_num_active_groups++;

            return p;
        }

        inline void free_group(T *p)
        {
            if (p)
            {
                VOGL_ASSERT(m_num_active_groups);
                m_num_active_groups--;

                base::destruct_group(p);

                base::free_space(p);
            }
        }

        inline void free_groups(uint32_t first_group_to_free, uint32_t num_groups_to_free)
        {
            for (uint32_t i = 0; i < num_groups_to_free; i++)
            {
                T *p = m_groups[first_group_to_free + i];
                if (p)
                {
                    free_group(p);
                    m_groups[first_group_to_free + i] = NULL;
                }
            }
        }

        // assumes [start, start + num) is a valid group range
        inline void get_full_group_range(uint32_t start, uint32_t num, uint32_t &first_full_group, uint32_t &num_full_groups)
        {
            const uint32_t num_to_skip = math::minimum(math::get_align_up_value_delta(start, N), num);

            start += num_to_skip;
            num -= num_to_skip;

            first_full_group = start >> Log2N;
            num_full_groups = num >> Log2N;
        }

        inline void init_default()
        {
            base::construct_element(reinterpret_cast<T *>(m_default));
        }

        inline void deinit_default()
        {
            base::destruct_element(reinterpret_cast<T *>(m_default));
        }
    };

    extern uint32_t g_sparse_vector_test_counters[2];

    template <uint32_t counter>
    class sparse_vector_test_obj
    {
    public:
        sparse_vector_test_obj()
        {
            g_sparse_vector_test_counters[counter]++;
        }

        sparse_vector_test_obj(const sparse_vector_test_obj &)
        {
            g_sparse_vector_test_counters[counter]--;
        }

        ~sparse_vector_test_obj()
        {
            VOGL_ASSERT(g_sparse_vector_test_counters[counter]);
            g_sparse_vector_test_counters[counter]--;
        }

        bool operator==(const sparse_vector_test_obj &other) const
        {
            return this == &other;
        }
    };

    inline bool sparse_vector_test(uint32_t seed = 15)
    {
        uint32_t num_failures = 0;
        random r;
        r.seed(seed);

#define VOGL_SPARSE_VECTOR_CHECK(x)          \
    do                                       \
    {                                        \
        if (!(x))                            \
        {                                    \
            num_failures++;                  \
            vogl_debug_break_if_debugging(); \
        }                                    \
    } while (0)

        printf("\n");
        for (uint32_t t = 0; t < 10; t++)
        {
            {
                printf("std::vector: ");
                timed_scope stopwatch;
                {
                    std::vector<dynamic_string> t1;
                    //t1.reserve(100000);
                    for (uint32_t i = 0; i < 100000; i++)
                    {
                        char buf[256];
                        vogl_sprintf_s(buf, sizeof(buf), "%u", i);
                        t1.push_back(buf);
                    }
                }
            }

            {
                printf("std::vector with reserve: ");
                timed_scope stopwatch;
                {
                    std::vector<dynamic_string> t1;
                    t1.reserve(100000);
                    for (uint32_t i = 0; i < 100000; i++)
                    {
                        char buf[256];
                        vogl_sprintf_s(buf, sizeof(buf), "%u", i);
                        t1.push_back(buf);
                    }
                }
            }

            {
                printf("vogl::vector: ");
                timed_scope stopwatch;
                {
                    vogl::vector<dynamic_string> t1;
                    //t1.reserve(100000);
                    for (uint32_t i = 0; i < 100000; i++)
                    {
                        char buf[256];
                        vogl_sprintf_s(buf, sizeof(buf), "%u", i);
                        t1.push_back(buf);
                    }
                }
            }

            {
                printf("vogl::vector with reserve: ");
                timed_scope stopwatch;
                {
                    vogl::vector<dynamic_string> t1;
                    t1.reserve(100000);
                    for (uint32_t i = 0; i < 100000; i++)
                    {
                        char buf[256];
                        vogl_sprintf_s(buf, sizeof(buf), "%u", i);
                        t1.push_back(buf);
                    }
                }
            }

#define SPARSE_VECTOR_PUSH_BACK_TEST(x)                                                                         \
    {                                                                                                           \
        printf("vogl::sparse_vector %u, group size %u bytes: ", x, (1U << x) * (uint32_t)sizeof(dynamic_string)); \
        timed_scope stopwatch;                                                                                  \
        {                                                                                                       \
            sparse_vector<dynamic_string, x> t2;                                                                \
            for (uint32_t i = 0; i < 100000; i++)                                                                   \
            {                                                                                                   \
                char buf[256];                                                                                  \
                vogl_sprintf_s(buf, sizeof(buf), "%u", i);                                                       \
                t2.push_back(buf);                                                                              \
            }                                                                                                   \
        }                                                                                                       \
    }

            SPARSE_VECTOR_PUSH_BACK_TEST(0);
            SPARSE_VECTOR_PUSH_BACK_TEST(1);
            SPARSE_VECTOR_PUSH_BACK_TEST(2);
            SPARSE_VECTOR_PUSH_BACK_TEST(3);
            SPARSE_VECTOR_PUSH_BACK_TEST(4);
            SPARSE_VECTOR_PUSH_BACK_TEST(5);
            SPARSE_VECTOR_PUSH_BACK_TEST(6);
            SPARSE_VECTOR_PUSH_BACK_TEST(7);
            SPARSE_VECTOR_PUSH_BACK_TEST(8);
            SPARSE_VECTOR_PUSH_BACK_TEST(9);
            SPARSE_VECTOR_PUSH_BACK_TEST(10);
            SPARSE_VECTOR_PUSH_BACK_TEST(11);
            SPARSE_VECTOR_PUSH_BACK_TEST(12);
            SPARSE_VECTOR_PUSH_BACK_TEST(13);
            SPARSE_VECTOR_PUSH_BACK_TEST(14);
#undef SPARSE_VECTOR_PUSH_BACK_TEST
        }

        {
            sparse_vector<uint32_t, 2> a1;

            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            for (uint32_t i = 0; i < 16; i++)
                a1.push_back(i);

            const uint32_t *q = &a1[0];
            VOGL_NOTE_UNUSED(q);
            uint32_t *p = &a1(0);
            VOGL_NOTE_UNUSED(p);
            const uint32_t &x1 = a1[0];
            VOGL_NOTE_UNUSED(x1);
            uint32_t &x2 = a1.get_or_create_element(0);
            VOGL_NOTE_UNUSED(x2);

            for (uint32_t i = 0; i < 16; i++)
                a1(i) = i;

            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            VOGL_SPARSE_VECTOR_CHECK(a1.size() == 16);
            VOGL_SPARSE_VECTOR_CHECK(a1.get_num_active_groups() == 4);

            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1[i] == i);

            const sparse_vector<uint32_t, 2> a1_const(a1);
            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1_const[i] == i);
            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1_const.at(i) == i);

            a1.invalidate_range(1, 9);
            VOGL_SPARSE_VECTOR_CHECK(a1.get_num_active_groups() == 3);
            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            for (uint32_t i = 0; i < 16; i++)
            {
                if ((i >= 1) && (i <= 9))
                    VOGL_SPARSE_VECTOR_CHECK(a1[i] == a1.default_element());
                else
                    VOGL_SPARSE_VECTOR_CHECK(a1[i] == i);
            }

            a1.invalidate_all();
            VOGL_SPARSE_VECTOR_CHECK(a1.get_num_active_groups() == 0);
            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1[i] == a1.default_element());
        }

        {
            sparse_vector<uint32_t, 0> a1;

            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            for (uint32_t i = 0; i < 16; i++)
                a1.push_back(i);

            const uint32_t *q = &a1[0];
            VOGL_NOTE_UNUSED(q);
            uint32_t *p = &a1(0);
            VOGL_NOTE_UNUSED(p);
            const uint32_t &x1 = a1[0];
            VOGL_NOTE_UNUSED(x1);
            uint32_t &x2 = a1.get_or_create_element(0);
            VOGL_NOTE_UNUSED(x2);

            for (uint32_t i = 0; i < 16; i++)
                a1(i) = i;

            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            VOGL_SPARSE_VECTOR_CHECK(a1.size() == 16);
            VOGL_SPARSE_VECTOR_CHECK(a1.get_num_active_groups() == 16);

            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1[i] == i);

            const sparse_vector<uint32_t, 0> a1_const(a1);
            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1_const[i] == i);
            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1_const.at(i) == i);

            a1.invalidate_range(1, 9);
            VOGL_SPARSE_VECTOR_CHECK(a1.get_num_active_groups() == 7);
            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            for (uint32_t i = 0; i < 16; i++)
            {
                if ((i >= 1) && (i <= 9))
                    VOGL_SPARSE_VECTOR_CHECK(a1[i] == a1.default_element());
                else
                    VOGL_SPARSE_VECTOR_CHECK(a1[i] == i);
            }

            a1.invalidate_all();
            VOGL_SPARSE_VECTOR_CHECK(a1.get_num_active_groups() == 0);
            VOGL_SPARSE_VECTOR_CHECK(a1.check());

            for (uint32_t i = 0; i < 16; i++)
                VOGL_SPARSE_VECTOR_CHECK(a1[i] == a1.default_element());
        }

        {
            sparse_vector<sparse_vector_test_obj<0>, 2> blah;

            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 1);

            blah.resize(16);
            for (uint32_t i = 0; i < 16; i++)
                blah.set(i, sparse_vector_test_obj<0>());

            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 17);

            blah.erase(4, 4);

            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 13);

            for (uint32_t i = 4; i < 8; i++)
                blah.set(i, blah.default_element());
            blah.optimize_groups();
            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 13);

            blah.insert(4, sparse_vector_test_obj<0>());
            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 17); // 17, because the sparse_vector had to add a new group to the end and groups are always fully constructed

            blah.clear();

            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 1);

            blah.resize(256);
            blah.allocate_all_groups();

            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 257);

            blah.erase(0, blah.size());

            VOGL_SPARSE_VECTOR_CHECK(g_sparse_vector_test_counters[0] == 1);
        }

        for (uint32_t iter = 0; iter < 10000; iter++)
        {
            uint32_t n = r.irand_inclusive(0, 100000);
            uint32_t j = r.irand_inclusive(0, math::clamp<uint32_t>(n / 30, 1, n));

            vogl::vector<uint32_t> a1(n);
            sparse_vector<uint32_t, 6> a2;
            sparse_vector<uint32_t, 6> a4;

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            for (uint32_t m = 0; m < j; m++)
            {
                uint32_t pos = r.irand(0, n);
                a1[pos] = m;
                a2.set(pos, m);
            }

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            a2.resize(n);

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            a2.optimize_groups();

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            a2.allocate_all_groups();

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            a2.optimize_groups();

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            a4 = a2;

            vector<uint32_t> k4;
            a2.get_vector(k4);
            sparse_vector<uint32_t, 8> k5(k4);
            VOGL_SPARSE_VECTOR_CHECK(k5 == a4);
            k5.clear();
            k5.assign(k4, true);
            VOGL_SPARSE_VECTOR_CHECK(k5 == a4);

            sparse_vector<uint32_t, 6> a3(a2);

            VOGL_SPARSE_VECTOR_CHECK(a2 == a3);
            VOGL_SPARSE_VECTOR_CHECK(!(a2 < a3));
            VOGL_SPARSE_VECTOR_CHECK(!(a3 < a2));

            for (uint32_t m = 0; m < n; m++)
            {
                VOGL_SPARSE_VECTOR_CHECK(a1[m] == a2[m]);
            }

            for (uint32_t m = 0; m < n; m++)
            {
                VOGL_SPARSE_VECTOR_CHECK(a1[a1.size() - 1] == a2[a2.size() - 1]);
                a1.pop_back();
                a2.pop_back();

                if (m == n / 2)
                {
                    VOGL_SPARSE_VECTOR_CHECK(a2.check());

                    a3.allocate_all_groups();
                    a3 = a2;

                    VOGL_SPARSE_VECTOR_CHECK(a2 == a3);
                    VOGL_SPARSE_VECTOR_CHECK(!(a2 < a3));
                    VOGL_SPARSE_VECTOR_CHECK(!(a3 < a2));

                    VOGL_SPARSE_VECTOR_CHECK(a3.check());
                }
            }

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            a2.optimize_groups();

            VOGL_SPARSE_VECTOR_CHECK(a2.check());

            {
                vogl::vector<uint32_t> shadow(a4.size());
                for (uint32_t i = 0; i < a4.size(); i++)
                    shadow[i] = a4[i];

                for (uint32_t m = 0; m < a4.size(); m++)
                {
                    VOGL_SPARSE_VECTOR_CHECK(a4[m] == shadow[m]);
                }

                vogl::vector<uint32_t> vals;
                vals.reserve(n);

                uint32_t k = r.irand(0, math::clamp<uint32_t>(n / 4, 1, 16));
                while ((k) && (a4.size()))
                {
                    uint32_t s = r.irand(0, a4.size());
                    uint32_t l = r.irand_inclusive(1, a4.size() - s);

                    if (r.get_bit())
                    {
                        shadow.erase(s, l);
                        a4.erase(s, l);
                    }
                    else
                    {
                        vals.resize(l);
                        for (uint32_t i = 0; i < l; i++)
                            vals[i] = r.urand32();

                        shadow.insert(s, vals.get_ptr(), l);
                        a4.insert(s, vals.get_ptr(), l);
                    }

                    VOGL_SPARSE_VECTOR_CHECK(a4.check());
                    VOGL_SPARSE_VECTOR_CHECK(shadow.size() == a4.size());

                    for (uint32_t m = 0; m < a4.size(); m++)
                    {
                        VOGL_SPARSE_VECTOR_CHECK(a4[m] == shadow[m]);
                    }

                    k--;
                }
            }

            {
                sparse_vector<uint32_t, 0> a5(a4.size());
                a5 = a4;

                vogl::vector<uint32_t> shadow(a5.size());
                for (uint32_t i = 0; i < a5.size(); i++)
                    shadow[i] = a5[i];

                for (uint32_t m = 0; m < a5.size(); m++)
                {
                    VOGL_SPARSE_VECTOR_CHECK(a5[m] == shadow[m]);
                }

                vogl::vector<uint32_t> vals;
                vals.reserve(n);

                uint32_t k = r.irand(0, math::clamp<uint32_t>(n / 4, 1, 16));
                while ((k) && (a5.size()))
                {
                    uint32_t s = r.irand(0, a5.size());
                    uint32_t l = r.irand_inclusive(1, a5.size() - s);

                    if (r.get_bit())
                    {
                        shadow.erase(s, l);
                        a5.erase(s, l);
                    }
                    else
                    {
                        vals.resize(l);
                        for (uint32_t i = 0; i < l; i++)
                            vals[i] = r.urand32();

                        shadow.insert(s, vals.get_ptr(), l);
                        a5.insert(s, vals.get_ptr(), l);
                    }

                    VOGL_SPARSE_VECTOR_CHECK(a5.check());
                    VOGL_SPARSE_VECTOR_CHECK(shadow.size() == a5.size());

                    for (uint32_t m = 0; m < a5.size(); m++)
                    {
                        VOGL_SPARSE_VECTOR_CHECK(a5[m] == shadow[m]);
                    }

                    k--;
                }
            }

            uint32_t k = 2;
            sparse_vector<uint32_t *, 2> z1;
            z1.push_back(&k);
            *z1[0] = 5;
            z1(0) = &k;
            VOGL_SPARSE_VECTOR_CHECK(*z1[0] == 5);

            sparse_vector<uint32_t *, 4> z2(z1);
            sparse_vector<uint32_t *, 4> z3;
            z3 = z1;
            z3 = z2;

            sparse_vector<dynamic_string, 4> j1;
            j1.push_back("blah");
            const sparse_vector<dynamic_string, 4> &j2 = j1;
            j1.at(0).get_ptr();
            j1(0) = dynamic_string("cool");
            j1[0].get_ptr();
            j2[0].get_ptr();

            if ((iter & 255) == 0)
                printf("%u\n", iter);
        }

#undef VOGL_SPARSE_VECTOR_CHECK

        return !num_failures;
    }

} // namespace vogl
