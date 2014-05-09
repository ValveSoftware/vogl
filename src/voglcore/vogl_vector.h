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

// File: vogl_vector.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    struct elemental_vector
    {
        void *m_p;
        uint32_t m_size;
        uint32_t m_capacity;

        typedef void (*object_mover)(void *pDst, void *pSrc, uint32_t num);

        bool increase_capacity(uint32_t min_new_capacity, bool grow_hint, uint32_t element_size, object_mover pRelocate, bool nofail);
    };

    template <typename T>
    class vector : public helpers::rel_ops<vector<T> >
    {
    public:
        typedef T *iterator;
        typedef const T *const_iterator;
        typedef T value_type;
        typedef T &reference;
        typedef const T &const_reference;
        typedef T *pointer;
        typedef const T *const_pointer;

        inline vector()
            : m_p(NULL),
              m_size(0),
              m_capacity(0)
        {
        }

        inline vector(uint32_t n, const T &init)
            : m_p(NULL),
              m_size(0),
              m_capacity(0)
        {
            increase_capacity(n, false);
            helpers::construct_array(m_p, n, init);
            m_size = n;
        }

        inline vector(const vector &other)
            : m_p(NULL),
              m_size(0),
              m_capacity(0)
        {
            increase_capacity(other.m_size, false);

            m_size = other.m_size;

            if (type_is_bitwise_copyable())
                memcpy(reinterpret_cast<void *>(m_p), reinterpret_cast<const void *>(other.m_p), m_size * sizeof(T));
            else
            {
                T *pDst = m_p;
                const T *pSrc = other.m_p;
                for (uint32_t i = m_size; i > 0; i--)
                    helpers::construct(pDst++, *pSrc++);
            }
        }

        inline explicit vector(uint32_t size)
            : m_p(NULL),
              m_size(0),
              m_capacity(0)
        {
            resize(size);
        }

        inline ~vector()
        {
            if (m_p)
            {
                scalar_type<T>::destruct_array(m_p, m_size);
                vogl_free(m_p);
            }
        }

        inline vector &operator=(const vector &other)
        {
            if (this == &other)
                return *this;

            if (m_capacity >= other.m_size)
                resize(0);
            else
            {
                clear();
                increase_capacity(other.m_size, false);
            }

            if (type_is_bitwise_copyable())
                memcpy(reinterpret_cast<void *>(m_p), reinterpret_cast<const void *>(other.m_p), other.m_size * sizeof(T));
            else
            {
                T *pDst = m_p;
                const T *pSrc = other.m_p;
                for (uint32_t i = other.m_size; i > 0; i--)
                    helpers::construct(pDst++, *pSrc++);
            }

            m_size = other.m_size;

            return *this;
        }

        inline const T *begin() const
        {
            return m_p;
        }
        T *begin()
        {
            return m_p;
        }

        inline const T *end() const
        {
            return m_p + m_size;
        }
        T *end()
        {
            return m_p + m_size;
        }

        inline bool is_empty() const
        {
            return !m_size;
        }
        inline uint32_t size() const
        {
            return m_size;
        }
        inline uint32_t size_in_bytes() const
        {
            return m_size * sizeof(T);
        }
        inline uint32_t capacity() const
        {
            return m_capacity;
        }

        // operator[] will assert on out of range indices, but in final builds there is (and will never be) any range checking on this method.
        inline const T &operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < m_size);
            return m_p[i];
        }
        inline T &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < m_size);
            return m_p[i];
        }

        // at() always includes range checking, even in final builds, unlike operator [].
        // The first element is returned if the index is out of range.
        inline const T &at(uint32_t i) const
        {
            VOGL_ASSERT(i < m_size);
            return (i >= m_size) ? m_p[0] : m_p[i];
        }
        inline T &at(uint32_t i)
        {
            VOGL_ASSERT(i < m_size);
            return (i >= m_size) ? m_p[0] : m_p[i];
        }

        inline const T &front() const
        {
            VOGL_ASSERT(m_size);
            return m_p[0];
        }
        inline T &front()
        {
            VOGL_ASSERT(m_size);
            return m_p[0];
        }

        inline const T &back() const
        {
            VOGL_ASSERT(m_size);
            return m_p[m_size - 1];
        }
        inline T &back()
        {
            VOGL_ASSERT(m_size);
            return m_p[m_size - 1];
        }

        inline const T *get_ptr() const
        {
            return m_p;
        }
        inline T *get_ptr()
        {
            return m_p;
        }

        inline const T *get_const_ptr() const
        {
            return m_p;
        }

        // clear() sets the container to empty, then frees the allocated block.
        inline void clear()
        {
            if (m_p)
            {
                scalar_type<T>::destruct_array(m_p, m_size);
                vogl_free(m_p);
                m_p = NULL;
                m_size = 0;
                m_capacity = 0;
            }
        }

        inline void clear_no_destruction()
        {
            if (m_p)
            {
                vogl_free(m_p);
                m_p = NULL;
                m_size = 0;
                m_capacity = 0;
            }
        }

        inline void reserve(uint32_t new_capacity)
        {
            if (new_capacity > m_capacity)
                increase_capacity(new_capacity, false);
            else if (new_capacity < m_capacity)
            {
                // Must work around the lack of a "decrease_capacity()" method.
                // This case is rare enough in practice that it's probably not worth implementing an optimized in-place resize.
                vector tmp;
                tmp.increase_capacity(math::maximum(m_size, new_capacity), false);
                tmp = *this;
                swap(tmp);
            }
        }

        inline bool try_reserve(uint32_t new_capacity)
        {
            return increase_capacity(new_capacity, true, true);
        }

        // resize() will never shrink the array's capacity, only enlarge it.
        // resize(0) empties the container, but does not free the allocated block.
        // If grow_hint is true, the capacity will at least double on resizes (it will also double if you're only enlarging by 1 element, assuming you're push_back'ing).
        inline void resize(uint32_t new_size, bool grow_hint = false)
        {
            if (m_size != new_size)
            {
                if (new_size < m_size)
                    scalar_type<T>::destruct_array(m_p + new_size, m_size - new_size);
                else
                {
                    if (new_size > m_capacity)
                        increase_capacity(new_size, (new_size == (m_size + 1)) || grow_hint);

                    scalar_type<T>::construct_array(m_p + m_size, new_size - m_size);
                }

                m_size = new_size;
            }
        }

        inline bool try_resize(uint32_t new_size, bool grow_hint = false)
        {
            if (m_size != new_size)
            {
                if (new_size < m_size)
                    scalar_type<T>::destruct_array(m_p + new_size, m_size - new_size);
                else
                {
                    if (new_size > m_capacity)
                    {
                        if (!increase_capacity(new_size, (new_size == (m_size + 1)) || grow_hint, true))
                            return false;
                    }

                    scalar_type<T>::construct_array(m_p + m_size, new_size - m_size);
                }

                m_size = new_size;
            }

            return true;
        }

        // Ensures array is large enough for element at index to be accessed.
        inline void ensure_element_is_valid(uint32_t index)
        {
            if (index >= m_size)
                resize(index + 1, true);
        }

        // If size >= capacity/2, reset() sets the container's size to 0 but doesn't free the allocated block (because the container may be similarly loaded in the future).
        // Otherwise it blows away the allocated block. See http://www.codercorner.com/blog/?p=494
        inline void reset()
        {
            if (m_size >= (m_capacity >> 1))
                resize(0);
            else
                clear();
        }

        inline T *enlarge(uint32_t i)
        {
            uint32_t cur_size = m_size;
            resize(cur_size + i, true);
            return get_ptr() + cur_size;
        }

        inline T *try_enlarge(uint32_t i)
        {
            uint32_t cur_size = m_size;
            if (!try_resize(cur_size + i, true))
                return NULL;
            return get_ptr() + cur_size;
        }

        inline void push_back(const T &obj)
        {
            VOGL_ASSERT(!m_p || (&obj < m_p) || (&obj >= (m_p + m_size)));

            if (m_size >= m_capacity)
                increase_capacity(m_size + 1, true);

            scalar_type<T>::construct(m_p + m_size, obj);
            m_size++;
        }

        inline bool try_push_back(const T &obj)
        {
            VOGL_ASSERT(!m_p || (&obj < m_p) || (&obj >= (m_p + m_size)));

            if (m_size >= m_capacity)
            {
                if (!increase_capacity(m_size + 1, true, true))
                    return false;
            }

            scalar_type<T>::construct(m_p + m_size, obj);
            m_size++;

            return true;
        }

        inline void push_back_value(T obj)
        {
            if (m_size >= m_capacity)
                increase_capacity(m_size + 1, true);

            scalar_type<T>::construct(m_p + m_size, obj);
            m_size++;
        }

        inline void pop_back()
        {
            VOGL_ASSERT(m_size);

            if (m_size)
            {
                m_size--;
                scalar_type<T>::destruct(&m_p[m_size]);
            }
        }

        inline void insert(uint32_t index, const T *p, uint32_t n)
        {
            VOGL_ASSERT(index <= m_size);
            if (!n)
                return;

            const uint32_t orig_size = m_size;
            resize(m_size + n, true);

            const uint32_t num_to_move = orig_size - index;

            if (type_is_bitwise_copyable())
            {
                // This overwrites the destination object bits, but bitwise copyable means we don't need to worry about destruction.
                memmove(reinterpret_cast<void *>(m_p + index + n), reinterpret_cast<const void *>(m_p + index), sizeof(T) * num_to_move);
            }
            else
            {
                const T *pSrc = m_p + orig_size - 1;
                T *pDst = const_cast<T *>(pSrc) + n;

                for (uint32_t i = 0; i < num_to_move; i++)
                {
                    VOGL_ASSERT((pDst - m_p) < (int)m_size);
                    *pDst-- = *pSrc--;
                }
            }

            T *pDst = m_p + index;

            if (type_is_bitwise_copyable())
            {
                // This copies in the new bits, overwriting the existing objects, which is OK for copyable types that don't need destruction.
                memcpy(reinterpret_cast<void *>(pDst), reinterpret_cast<const void *>(p), sizeof(T) * n);
            }
            else
            {
                for (uint32_t i = 0; i < n; i++)
                {
                    VOGL_ASSERT((pDst - m_p) < (int)m_size);
                    *pDst++ = *p++;
                }
            }
        }

        inline void insert(uint32_t index, const T &obj)
        {
            VOGL_ASSERT(!m_p || (&obj < m_p) || (&obj >= (m_p + m_size)));
            insert(index, &obj, 1);
        }

        // push_front() isn't going to be very fast - it's only here for usability.
        inline void push_front(const T &obj)
        {
            insert(0, &obj, 1);
        }

        vector &append(const vector &other)
        {
            VOGL_ASSERT(this != &other);
            if (other.m_size)
                insert(m_size, &other[0], other.m_size);
            return *this;
        }

        vector &append(const T *p, uint32_t n)
        {
            if (n)
                insert(m_size, p, n);
            return *this;
        }

        inline void erase(uint32_t start, uint32_t n)
        {
            VOGL_ASSERT((start + n) <= m_size);
            if ((start + n) > m_size)
                return;

            if (!n)
                return;

            const uint32_t num_to_move = m_size - (start + n);

            T *pDst = m_p + start;

            const T *pSrc = m_p + start + n;

            if (type_is_bitwise_copyable_or_movable())
            {
                // This test is overly cautious.
                if ((!type_is_bitwise_copyable()) || type_has_destructor())
                {
                    // Type has been marked explictly as bitwise movable, which means we can move them around but they may need to be destructed.
                    // First destroy the erased objects.
                    scalar_type<T>::destruct_array(pDst, n);
                }

                // Copy "down" the objects to preserve, filling in the empty slots.
                memmove(pDst, pSrc, num_to_move * sizeof(T));
            }
            else
            {
                // Type is not bitwise copyable or movable.
                // Move them down one at a time by using the equals operator, and destroying anything that's left over at the end.
                T *pDst_end = pDst + num_to_move;
                while (pDst != pDst_end)
                    *pDst++ = *pSrc++;

                scalar_type<T>::destruct_array(pDst_end, n);
            }

            m_size -= n;
        }

        inline void erase(uint32_t index)
        {
            erase(index, 1);
        }

        inline void erase(T *p)
        {
            VOGL_ASSERT((p >= m_p) && (p < (m_p + m_size)));
            erase(static_cast<uint32_t>(p - m_p));
        }

        void erase_unordered(uint32_t index)
        {
            VOGL_ASSERT(index < m_size);

            if ((index + 1) < m_size)
                (*this)[index] = back();

            pop_back();
        }

        inline bool operator==(const vector &rhs) const
        {
            if (this == &rhs)
                return true;

            if (m_size != rhs.m_size)
                return false;
            else if (m_size)
            {
                if (scalar_type<T>::cFlag)
                    return memcmp(m_p, rhs.m_p, sizeof(T) * m_size) == 0;
                else
                {
                    const T *pSrc = m_p;
                    const T *pDst = rhs.m_p;
                    for (uint32_t i = m_size; i; i--)
                        if (!(*pSrc++ == *pDst++))
                            return false;
                }
            }

            return true;
        }

        // lexicographical order
        inline bool operator<(const vector &rhs) const
        {
            const uint32_t min_size = math::minimum(m_size, rhs.m_size);

            const T *pSrc = m_p;
            const T *pSrc_end = m_p + min_size;
            const T *pDst = rhs.m_p;

            while ((pSrc < pSrc_end) && (*pSrc == *pDst))
            {
                pSrc++;
                pDst++;
            }

            if (pSrc < pSrc_end)
                return *pSrc < *pDst;

            return m_size < rhs.m_size;
        }

        inline void swap(vector &other)
        {
            utils::swap(m_p, other.m_p);
            utils::swap(m_size, other.m_size);
            utils::swap(m_capacity, other.m_capacity);
        }

        inline void sort()
        {
            introsort(begin(), end());
        }

        template <typename Comp>
        inline void sort(Comp compare_obj)
        {
            introsort(begin(), end(), compare_obj);
        }

        inline bool is_sorted() const
        {
            for (uint32_t i = 1; i < m_size; ++i)
                if (m_p[i] < m_p[i - 1])
                    return false;
            return true;
        }

        inline void unique()
        {
            if (!is_empty())
            {
                sort();

                resize(static_cast<uint32_t>(std::unique(begin(), end()) - begin()));
            }
        }

        template <typename EqComp>
        inline void unique(EqComp comp)
        {
            if (!is_empty())
            {
                sort();

                resize(static_cast<uint32_t>(std::unique(begin(), end(), comp) - begin()));
            }
        }

        inline void reverse()
        {
            uint32_t j = m_size >> 1;
            for (uint32_t i = 0; i < j; i++)
                utils::swap(m_p[i], m_p[m_size - 1 - i]);
        }

        inline int find(const T &key) const
        {
            VOGL_ASSERT(m_size <= static_cast<uint32_t>(cINT32_MAX));

            const T *p = m_p;
            const T *p_end = m_p + m_size;

            uint32_t index = 0;

            while (p != p_end)
            {
                if (key == *p)
                    return index;

                p++;
                index++;
            }

            return cInvalidIndex;
        }

        template <typename Q>
        inline int find(const T &key, Q equal_to) const
        {
            VOGL_ASSERT(m_size <= static_cast<uint32_t>(cINT32_MAX));

            const T *p = m_p;
            const T *p_end = m_p + m_size;

            uint32_t index = 0;

            while (p != p_end)
            {
                if (equal_to(key, *p))
                    return index;

                p++;
                index++;
            }

            return cInvalidIndex;
        }

        inline int find_last(const T &key) const
        {
            VOGL_ASSERT(m_size <= static_cast<uint32_t>(cINT32_MAX));

            for (int i = m_size - 1; i >= 0; --i)
                if (m_p[i] == key)
                    return i;

            return cInvalidIndex;
        }

        template <typename Q>
        inline int find_last(const T &key, Q equal_to) const
        {
            VOGL_ASSERT(m_size <= static_cast<uint32_t>(cINT32_MAX));

            for (int i = m_size - 1; i >= 0; --i)
                if (equal_to(m_p[i], key))
                    return i;

            return cInvalidIndex;
        }

        inline int find_sorted(const T &key) const
        {
            VOGL_ASSERT(m_size <= static_cast<uint32_t>(cINT32_MAX));

            if (m_size)
            {
                // Uniform binary search - Knuth Algorithm 6.2.1 U, unrolled twice.
                // TODO: OK, is this worth it vs the usual algorithm?
                int i = ((m_size + 1) >> 1) - 1;
                int m = m_size;

                for (;;)
                {
                    VOGL_ASSERT_OPEN_RANGE(i, 0, (int)m_size);
                    const T *pKey_i = m_p + i;
                    int cmp = key < *pKey_i;
#ifdef VOGL_BUILD_DEBUG
                    int cmp2 = *pKey_i < key;
                    VOGL_ASSERT((cmp != cmp2) || (key == *pKey_i));
#endif
                    if ((!cmp) && (key == *pKey_i))
                        return i;
                    m >>= 1;
                    if (!m)
                        break;
                    cmp = -cmp;
                    i += (((m + 1) >> 1) ^ cmp) - cmp;
                    if (i < 0)
                        break;

                    VOGL_ASSERT_OPEN_RANGE(i, 0, (int)m_size);
                    pKey_i = m_p + i;
                    cmp = key < *pKey_i;
#ifdef VOGL_BUILD_DEBUG
                    cmp2 = *pKey_i < key;
                    VOGL_ASSERT((cmp != cmp2) || (key == *pKey_i));
#endif
                    if ((!cmp) && (key == *pKey_i))
                        return i;
                    m >>= 1;
                    if (!m)
                        break;
                    cmp = -cmp;
                    i += (((m + 1) >> 1) ^ cmp) - cmp;
                    if (i < 0)
                        break;
                }
            }

            return cInvalidIndex;
        }

        template <typename Q>
        inline int find_sorted(const T &key, Q less_than) const
        {
            if (m_size)
            {
                // Uniform binary search - Knuth Algorithm 6.2.1 U, unrolled twice.
                // TODO: OK, is this worth it vs the usual algorithm?
                int i = ((m_size + 1) >> 1) - 1;
                int m = m_size;

                for (;;)
                {
                    VOGL_ASSERT_OPEN_RANGE(i, 0, (int)m_size);
                    const T *pKey_i = m_p + i;
                    int cmp = less_than(key, *pKey_i);
#ifdef VOGL_BUILD_DEBUG
                    int cmp2 = less_than(*pKey_i, key);
                    VOGL_ASSERT((cmp != cmp2) || (key == *pKey_i));
#endif
                    if ((!cmp) && (!less_than(*pKey_i, key)))
                        return i;
                    m >>= 1;
                    if (!m)
                        break;
                    cmp = -cmp;
                    i += (((m + 1) >> 1) ^ cmp) - cmp;
                    if (i < 0)
                        break;

                    VOGL_ASSERT_OPEN_RANGE(i, 0, (int)m_size);
                    pKey_i = m_p + i;
                    cmp = less_than(key, *pKey_i);
#ifdef VOGL_BUILD_DEBUG
                    cmp2 = less_than(*pKey_i, key);
                    VOGL_ASSERT((cmp != cmp2) || (key == *pKey_i));
#endif
                    if ((!cmp) && (!less_than(*pKey_i, key)))
                        return i;
                    m >>= 1;
                    if (!m)
                        break;
                    cmp = -cmp;
                    i += (((m + 1) >> 1) ^ cmp) - cmp;
                    if (i < 0)
                        break;
                }
            }

            return cInvalidIndex;
        }

        inline uint32_t count_occurences(const T &key) const
        {
            uint32_t c = 0;

            const T *p = m_p;
            const T *p_end = m_p + m_size;

            while (p != p_end)
            {
                if (key == *p)
                    c++;

                p++;
            }

            return c;
        }

        inline void set_all(const T &o)
        {
            if ((sizeof(T) == 1) && (scalar_type<T>::cFlag))
                memset(m_p, *reinterpret_cast<const uint8_t *>(&o), m_size);
            else
            {
                T *pDst = m_p;
                T *pDst_end = pDst + m_size;
                while (pDst != pDst_end)
                    *pDst++ = o;
            }
        }

        // Caller assumes ownership of the heap block associated with the container. Container is cleared.
        inline void *assume_ownership()
        {
            T *p = m_p;
            m_p = NULL;
            m_size = 0;
            m_capacity = 0;
            return p;
        }

        // Caller is granting ownership of the indicated heap block.
        // Block must have size constructed elements, and have enough room for capacity elements.
        inline bool grant_ownership(T *p, uint32_t size, uint32_t capacity)
        {
            // To to prevent the caller from obviously shooting themselves in the foot.
            if (((p + capacity) > m_p) && (p < (m_p + m_capacity)))
            {
                // Can grant ownership of a block inside the container itself!
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (size > capacity)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (!p)
            {
                if (capacity)
                {
                    VOGL_ASSERT_ALWAYS;
                    return false;
                }
            }
            else if (!capacity)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            clear();
            m_p = p;
            m_size = size;
            m_capacity = capacity;
            return true;
        }

        // Fisher–Yates shuffle
        template <typename R>
        inline void shuffle(R &rm)
        {
            VOGL_ASSERT(m_size < static_cast<uint32_t>(cINT32_MAX));

            if ((m_size <= 1U) || (m_size >= static_cast<uint32_t>(cINT32_MAX)))
                return;

            for (uint32_t i = m_size - 1; i >= 1; --i)
            {
                uint32_t j = rm.irand_inclusive(0, i);
                std::swap(m_p[j], m_p[i]);
            }
        }

    private:
        T *m_p;
        uint32_t m_size;
        uint32_t m_capacity;

        template <typename Q>
        struct is_vector
        {
            enum
            {
                cFlag = false
            };
        };
        template <typename Q>
        struct is_vector<vector<Q> >
        {
            enum
            {
                cFlag = true
            };
        };

        static void object_mover(void *pDst_void, void *pSrc_void, uint32_t num)
        {
            T *pSrc = static_cast<T *>(pSrc_void);
            T *const pSrc_end = pSrc + num;
            T *pDst = static_cast<T *>(pDst_void);

            while (pSrc != pSrc_end)
            {
                // placement new
                new (static_cast<void *>(pDst)) T(*pSrc);
                pSrc->~T();
                ++pSrc;
                ++pDst;
            }
        }

        inline bool increase_capacity(uint32_t min_new_capacity, bool grow_hint, bool nofail = false)
        {
            return reinterpret_cast<elemental_vector *>(this)->increase_capacity(
                min_new_capacity, grow_hint, sizeof(T),
                (type_is_bitwise_copyable_or_movable() || (is_vector<T>::cFlag)) ? NULL : object_mover, nofail);
        }

        static inline bool type_is_bitwise_copyable()
        {
            return VOGL_IS_BITWISE_COPYABLE(T);
        }

        static inline bool type_is_bitwise_copyable_or_movable()
        {
            return VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T);
        }

        static inline bool type_has_destructor()
        {
            return VOGL_HAS_DESTRUCTOR(T);
        }
    };

    typedef vogl::vector<uint8_t> uint8_vec;
    typedef vogl::vector<char> char_vec;
    typedef vogl::vector<int> int_vec;
    typedef vogl::vector<uint32_t> uint_vec;
    typedef vogl::vector<int32_t> int32_vec;
    typedef vogl::vector<uint32_t> uint32_vec;
    typedef vogl::vector<int64_t> int64_vec;
    typedef vogl::vector<uint64_t> uint64_vec;
    typedef vogl::vector<float> float_vec;
    typedef vogl::vector<double> double_vec;
    typedef vogl::vector<void *> void_ptr_vec;
    typedef vogl::vector<const void *> const_void_ptr_vec;

    template <typename T>
    struct bitwise_movable<vector<T> >
    {
        enum
        {
            cFlag = true
        };
    };

    template <typename T>
    inline void swap(vector<T> &a, vector<T> &b)
    {
        a.swap(b);
    }

#if 0
inline void vector_test()
{
	vogl::random r;
	for (uint32_t i = 0; i < 100000; i++)
	{
		uint32_t n = r.irand(0, 1000);
		vogl::vector<int> xx(n);

		for (int i = 0; i < n; i++)
			xx[i] = i * 2;
		VOGL_VERIFY(xx.find_sorted(-1) < 0);
		for (uint32_t i = 0; i < n; i++)
		{
			VOGL_VERIFY(xx.find_sorted(2 * i) == i);
			VOGL_VERIFY(xx.find_sorted(2 * i + 1) < 0);
		}
		VOGL_VERIFY(xx.find_sorted(n * 2) < 0);
		VOGL_VERIFY(xx.find_sorted(99999) < 0);
	}
}
#endif

} // namespace vogl

namespace std
{
    template <typename T>
    inline void swap(vogl::vector<T> &a, vogl::vector<T> &b)
    {
        a.swap(b);
    }
}
