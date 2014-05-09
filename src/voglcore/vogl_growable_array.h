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

// File: vogl_growable_array.h
//
// growable_array is backed by an embedded fixed array which can hold up to N elements, beyond this it automatically switches to a vogl::vector.
// This class is only a subset of vogl::vector, but its semantics closely match it.
// Objects in the embedded fixed array are not constructed until they are inserted/push_back'd, etc. They are guaranteed 8 byte alignment.
// If vogl::vector supports mixins I could add this functionality into it directly without having to duplicate code (and I could cut down on the #
// of member vars), but it would become much more complex.
// This is like llvm::SmallVector, see http://fahrenheit2539.blogspot.com/2012/06/introduction-in-depths-look-at.html
//
#ifndef VOGL_GROWABLE_ARRAY_H
#define VOGL_GROWABLE_ARRAY_H

#include "vogl_core.h"

namespace vogl
{
    template <typename T, uint32_t N>
    class growable_array
    {
    public:
        typedef T element_type;
        enum
        {
            cMaxFixedElements = N
        };

        inline growable_array()
            : m_fixed_size(0)
        {
            VOGL_ASSUME(N > 0);
        }

        inline growable_array(uint32_t initial_size)
            : m_fixed_size(0)
        {
            VOGL_ASSUME(N > 0);
            resize(initial_size);
        }

        inline growable_array(const growable_array &other)
            : m_fixed_size(0)
        {
            VOGL_ASSUME(N > 0);
            *this = other;
        }

        inline growable_array &operator=(const growable_array &rhs)
        {
            if (this == &rhs)
                return *this;

            clear();

            const uint32_t rhs_size = rhs.size();

            if (rhs_size > N)
                m_dynamic_elements = rhs.m_dynamic_elements;
            else
            {
                helpers::copy_array(&fixed_element(0), rhs.get_ptr(), rhs_size);
                m_fixed_size = rhs_size;
            }

            return *this;
        }

        inline ~growable_array()
        {
            VOGL_ASSERT((m_fixed_size && m_dynamic_elements.is_empty()) || (!m_fixed_size));
            VOGL_ASSERT((m_fixed_size && (m_dynamic_elements.get_ptr() == NULL)) || (!m_fixed_size));

            if (VOGL_HAS_DESTRUCTOR(T))
            {
                scalar_type<T>::destruct_array(&fixed_element(0), m_fixed_size);
            }
        }

        inline bool is_dynamic() const
        {
            return m_dynamic_elements.get_ptr() != NULL;
        }

        inline void clear()
        {
            VOGL_ASSERT((m_fixed_size && m_dynamic_elements.is_empty() && (m_dynamic_elements.get_ptr() == NULL)) || (!m_fixed_size));

            if (m_fixed_size)
            {
                if (VOGL_HAS_DESTRUCTOR(T))
                {
                    scalar_type<T>::destruct_array(&fixed_element(0), m_fixed_size);
                }
                m_fixed_size = 0;
            }
            else
            {
                m_dynamic_elements.clear();
            }
        }

        inline uint32_t size() const
        {
            return m_fixed_size ? m_fixed_size : m_dynamic_elements.size();
        }

        inline uint32_t size_in_bytes() const
        {
            return size() * sizeof(T);
        }

        inline bool is_empty() const
        {
            return !size();
        }

        inline uint32_t capacity() const
        {
            return is_dynamic() ? m_dynamic_elements.capacity() : N;
        }

        inline const T *get_ptr() const
        {
            return (m_dynamic_elements.get_ptr() ? m_dynamic_elements.get_ptr() : &fixed_element(0));
        }
        inline T *get_ptr()
        {
            return (m_dynamic_elements.get_ptr() ? m_dynamic_elements.get_ptr() : &fixed_element(0));
        }

        inline const T *begin() const
        {
            return get_ptr();
        }
        inline T *begin()
        {
            return get_ptr();
        }

        inline const T *end() const
        {
            return (m_dynamic_elements.get_ptr() ? m_dynamic_elements.end() : &fixed_element(m_fixed_size));
        }
        inline T *end()
        {
            return (m_dynamic_elements.get_ptr() ? m_dynamic_elements.end() : &fixed_element(m_fixed_size));
        }

        inline const T &front() const
        {
            VOGL_ASSERT(size());
            return begin()[0];
        }
        inline T &front()
        {
            VOGL_ASSERT(size());
            return begin()[0];
        }

        inline const T &back() const
        {
            VOGL_ASSERT(size());
            return end()[-1];
        }
        inline T &back()
        {
            VOGL_ASSERT(size());
            return end()[-1];
        }

        inline const T &operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < size());
            return get_ptr()[i];
        }

        inline T &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < size());
            return get_ptr()[i];
        }

        inline void set_all(const T &val)
        {
            if (VOGL_IS_BITWISE_COPYABLE(T) && (sizeof(T) == sizeof(uint8_t)))
                memset(get_ptr(), *reinterpret_cast<const uint8_t *>(&val), size());
            else
            {
                T *p = get_ptr();
                T *pEnd = p + size();
                while (p != pEnd)
                    *p++ = val;
            }
        }

        inline void resize(uint32_t n, bool grow_hint = false)
        {
            if (is_dynamic())
            {
                m_dynamic_elements.resize(n, grow_hint);
            }
            else if (n <= N)
            {
                if (n < m_fixed_size)
                    scalar_type<T>::destruct_array(&fixed_element(n), m_fixed_size - n);
                else if (n > m_fixed_size)
                    scalar_type<T>::construct_array(&fixed_element(m_fixed_size), n - m_fixed_size);
                m_fixed_size = n;
            }
            else
            {
                switch_to_dynamic(n);
                m_dynamic_elements.resize(n);
            }
        }

        inline void reserve(uint32_t n)
        {
            if ((n <= N) || (n <= size()))
                return;

            switch_to_dynamic(n);
        }

        inline void push_back(const T &obj)
        {
            if (is_dynamic())
                m_dynamic_elements.push_back(obj);
            else if ((m_fixed_size + 1) > N)
            {
                switch_to_dynamic(m_fixed_size + 1);
                m_dynamic_elements.push_back(obj);
            }
            else
            {
                scalar_type<T>::construct(&fixed_element(m_fixed_size), obj);
                ++m_fixed_size;
            }
        }

        inline T *enlarge(uint32_t n)
        {
            if ((m_fixed_size + n) > N)
                switch_to_dynamic(m_fixed_size + n);

            if (is_dynamic())
                return m_dynamic_elements.enlarge(n);

            T *pResult = &fixed_element(m_fixed_size);
            scalar_type<T>::construct_array(pResult, n);
            m_fixed_size += n;
            return pResult;
        }

        inline void pop_back()
        {
            if (is_dynamic())
                m_dynamic_elements.pop_back();
            else
            {
                VOGL_ASSERT(m_fixed_size);
                if (m_fixed_size)
                {
                    --m_fixed_size;
                    scalar_type<T>::destruct(&fixed_element(m_fixed_size));
                }
            }
        }

        inline void insert(uint32_t index, const T *p, uint32_t n)
        {
            VOGL_ASSERT(((p + n) <= &fixed_element(0)) || (p >= &fixed_element(m_fixed_size)));

            if ((m_fixed_size + n) > N)
                switch_to_dynamic(m_fixed_size + n);

            if (is_dynamic())
            {
                m_dynamic_elements.insert(index, p, n);
                return;
            }

            VOGL_ASSERT(index <= m_fixed_size);
            if (!n)
                return;

            const uint32_t orig_size = m_fixed_size;
            resize(m_fixed_size + n);

            VOGL_ASSERT(!is_dynamic());

            const uint32_t num_to_move = orig_size - index;

            if (VOGL_IS_BITWISE_COPYABLE(T))
            {
                // This overwrites the destination object bits, but bitwise copyable means we don't need to worry about destruction.
                memmove(&fixed_element(0) + index + n, &fixed_element(0) + index, sizeof(T) * num_to_move);
            }
            else
            {
                const T *pSrc = &fixed_element(0) + orig_size - 1;
                T *pDst = const_cast<T *>(pSrc) + n;

                for (uint32_t i = 0; i < num_to_move; i++)
                {
                    VOGL_ASSERT((pDst - &fixed_element(0)) < (int)m_fixed_size);
                    *pDst-- = *pSrc--;
                }
            }

            T *pDst = &fixed_element(0) + index;

            if (VOGL_IS_BITWISE_COPYABLE(T))
            {
                // This copies in the new bits, overwriting the existing objects, which is OK for copyable types that don't need destruction.
                memcpy(pDst, p, sizeof(T) * n);
            }
            else
            {
                for (uint32_t i = 0; i < n; i++)
                {
                    VOGL_ASSERT((pDst - &fixed_element(0)) < (int)m_fixed_size);
                    *pDst++ = *p++;
                }
            }
        }

        inline void insert(uint32_t index, const T &obj)
        {
            insert(index, &obj, 1);
        }

        // push_front() isn't going to be very fast - it's only here for usability.
        inline void push_front(const T &obj)
        {
            insert(0, &obj, 1);
        }

        inline growable_array &append(const growable_array &other)
        {
            VOGL_ASSERT(this != &other);
            if (other.size())
                insert(size(), other.get_ptr(), other.size());
            return *this;
        }

        inline growable_array &append(const T *p, uint32_t n)
        {
            if (n)
                insert(size(), p, n);
            return *this;
        }

        inline void erase(uint32_t start, uint32_t n)
        {
            if (is_dynamic())
            {
                m_dynamic_elements.erase(start, n);
                return;
            }

            VOGL_ASSERT((start + n) <= m_fixed_size);
            if ((start + n) > m_fixed_size)
                return;

            if (!n)
                return;

            const uint32_t num_to_move = m_fixed_size - (start + n);

            T *pDst = &fixed_element(0) + start;

            const T *pSrc = &fixed_element(0) + start + n;

            if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
            {
                // This test is overly cautious.
                if ((!VOGL_IS_BITWISE_COPYABLE(T)) || VOGL_HAS_DESTRUCTOR(T))
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

            m_fixed_size -= n;
        }

        inline void erase(uint32_t index)
        {
            erase(index, 1);
        }

        inline void erase_unordered(uint32_t index)
        {
            VOGL_ASSERT(index < size());

            if ((index + 1) < size())
                (*this)[index] = back();

            pop_back();
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
            for (uint32_t i = 1; i < size(); ++i)
                if ((*this)[i] < (*this)[i - 1])
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

        inline void reverse()
        {
            uint32_t cur_size = size();
            uint32_t j = cur_size >> 1;
            T *p = get_ptr();
            for (uint32_t i = 0; i < j; i++)
                utils::swap(p[i], p[cur_size - 1 - i]);
        }

        inline int find(const T &key) const
        {
            uint32_t cur_size;
            const T *p;
            if (is_dynamic())
            {
                cur_size = m_dynamic_elements.size();
                p = m_dynamic_elements.get_ptr();
            }
            else
            {
                cur_size = m_fixed_size;
                p = &fixed_element(0);
            }

            VOGL_ASSERT(cur_size <= cINT32_MAX);

            const T *p_end = p + cur_size;

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

        inline int find_last(const T &key) const
        {
            uint32_t cur_size;
            const T *p;
            if (is_dynamic())
            {
                cur_size = m_dynamic_elements.size();
                p = m_dynamic_elements.get_ptr();
            }
            else
            {
                cur_size = m_fixed_size;
                p = &fixed_element(0);
            }

            VOGL_ASSERT(cur_size <= cINT32_MAX);

            for (int i = cur_size - 1; i >= 0; --i)
                if (p[i] == key)
                    return i;

            return cInvalidIndex;
        }

        inline int find_sorted(const T &key) const
        {
            uint32_t cur_size;
            const T *p;
            if (is_dynamic())
            {
                cur_size = m_dynamic_elements.size();
                p = m_dynamic_elements.get_ptr();
            }
            else
            {
                cur_size = m_fixed_size;
                p = &fixed_element(0);
            }

            VOGL_ASSERT(cur_size <= cINT32_MAX);

            int begin = 0;
            int end = cur_size - 1;

            while (end >= begin)
            {
                int mid = begin + ((end - begin) >> 1);

                if (p[mid] < key)
                    begin = mid + 1;
                else if (key < p[mid])
                    end = mid - 1;
                else
                    return mid;
            }

            return cInvalidIndex;
        }

        void swap(growable_array &rhs)
        {
            growable_array &lhs = *this;

            if (&lhs == &rhs)
                return;

            if (lhs.is_dynamic() && !rhs.is_dynamic())
                helpers::move_array(&lhs.fixed_element(0), &rhs.fixed_element(0), rhs.m_fixed_size);
            else if (!lhs.is_dynamic() && rhs.is_dynamic())
                helpers::move_array(&rhs.fixed_element(0), &lhs.fixed_element(0), lhs.m_fixed_size);
            else if (!lhs.is_dynamic() && !rhs.is_dynamic())
            {
                uint32_t common_size = math::minimum(lhs.m_fixed_size, rhs.m_fixed_size);
                for (uint32_t i = 0; i < common_size; i++)
                    std::swap(lhs.fixed_element(i), rhs.fixed_element(i));

                if (lhs.m_fixed_size > rhs.m_fixed_size)
                    helpers::move_array(&rhs.fixed_element(rhs.m_fixed_size), &lhs.fixed_element(rhs.m_fixed_size), lhs.m_fixed_size - rhs.m_fixed_size);
                else if (rhs.m_fixed_size > lhs.m_fixed_size)
                    helpers::move_array(&fixed_element(lhs.m_fixed_size), &rhs.fixed_element(lhs.m_fixed_size), rhs.m_fixed_size - lhs.m_fixed_size);
            }

            lhs.m_dynamic_elements.swap(rhs.m_dynamic_elements);
            std::swap(lhs.m_fixed_size, rhs.m_fixed_size);
        }

        void switch_to_dynamic(uint32_t new_capacity)
        {
            if (is_dynamic())
            {
                m_dynamic_elements.reserve(new_capacity);
                return;
            }

            VOGL_ASSERT((new_capacity >= m_fixed_size) && (!m_dynamic_elements.size()));

            T *pBlock = static_cast<T *>(vogl_malloc(new_capacity * sizeof(T)));

            if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
            {
                memcpy(pBlock, &fixed_element(0), size_in_bytes());
            }
            else
            {
                T *pSrc = &fixed_element(0);
                T *pSrc_end = &fixed_element(m_fixed_size);
                T *pDst = pBlock;

                while (pSrc != pSrc_end)
                {
                    scalar_type<T>::construct(pDst++, *pSrc);
                    scalar_type<T>::destruct(pSrc++);
                }
            }

            m_dynamic_elements.grant_ownership(pBlock, m_fixed_size, new_capacity);
            m_fixed_size = 0;
        }

        void switch_to_embedded()
        {
            if (!is_dynamic() || (m_dynamic_elements.size() > N))
                return;

            uint32_t cur_size = m_dynamic_elements.size();
            T *pSrc = static_cast<T *>(m_dynamic_elements.assume_ownership());

            if (cur_size)
            {
                T *pDst = &fixed_element(0);

                if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
                    memcpy(pDst, pSrc, cur_size * sizeof(T));
                else
                {
                    T *pSrc_end = pSrc + cur_size;

                    while (pSrc != pSrc_end)
                    {
                        scalar_type<T>::construct(pDst++, *pSrc);
                        scalar_type<T>::destruct(pSrc++);
                    }
                }
            }

            m_fixed_size = cur_size;
            vogl_free(pSrc);
        }

        inline bool operator==(const growable_array &rhs) const
        {
            uint32_t n = size();
            if (n != rhs.size())
                return false;

            for (uint32_t i = 0; i < n; i++)
                if (!((*this)[i] == rhs[i]))
                    return false;

            return true;
        }

        inline bool operator!=(const growable_array &rhs) const
        {
            return !(*this == rhs);
        }

        template <typename OtherT>
        inline bool operator==(const OtherT &rhs) const
        {
            uint32_t n = size();
            if (n != rhs.size())
                return false;

            for (uint32_t i = 0; i < n; i++)
                if (!((*this)[i] == rhs[i]))
                    return false;

            return true;
        }

        template <typename OtherT>
        inline bool operator!=(const OtherT &rhs) const
        {
            return !(*this == rhs);
        }

    private:
        vogl::vector<T> m_dynamic_elements;
        uint32_t m_fixed_size;
        uint64_t m_fixed_elements[(N * sizeof(T) + sizeof(uint64_t) - 1) / sizeof(uint64_t)];

        inline const T &fixed_element(uint32_t i) const
        {
            return reinterpret_cast<const T *>(m_fixed_elements)[i];
        }
        inline T &fixed_element(uint32_t i)
        {
            return reinterpret_cast<T *>(m_fixed_elements)[i];
        }
    };

} // namespace vogl

#endif // VOGL_GROWABLE_ARRAY_H
