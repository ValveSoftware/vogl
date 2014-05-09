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

// File: vogl_sparse_bit_array.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    class sparse_bit_array
    {
    public:
        sparse_bit_array();
        sparse_bit_array(uint32_t size);
        sparse_bit_array(sparse_bit_array &other);
        ~sparse_bit_array();

        sparse_bit_array &operator=(sparse_bit_array &other);

        void clear();

        inline uint32_t get_size()
        {
            return (m_num_groups << cBitsPerGroupShift);
        }

        void resize(uint32_t size);

        sparse_bit_array &operator&=(const sparse_bit_array &other);
        sparse_bit_array &operator|=(const sparse_bit_array &other);
        sparse_bit_array &and_not(const sparse_bit_array &other);

        void swap(sparse_bit_array &other);

        void optimize();

        void set_bit_range(uint32_t index, uint32_t num);
        void clear_bit_range(uint32_t index, uint32_t num);

        void clear_all_bits();

        inline void set_bit(uint32_t index)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            uint32_t bit_ofs = index & (cBitsPerGroup - 1);

            pGroup[bit_ofs >> 5] |= (1U << (bit_ofs & 31));
        }

        inline void clear_bit(uint32_t index)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            uint32_t bit_ofs = index & (cBitsPerGroup - 1);

            pGroup[bit_ofs >> 5] &= (~(1U << (bit_ofs & 31)));
        }

        inline void set(uint32_t index, bool value)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            uint32_t bit_ofs = index & (cBitsPerGroup - 1);

            uint32_t bit = (1U << (bit_ofs & 31));

            uint32_t c = pGroup[bit_ofs >> 5];
            uint32_t mask = (uint32_t)(-(int)value);

            pGroup[bit_ofs >> 5] = (c & ~bit) | (mask & bit);
        }

        inline bool get_bit(uint32_t index) const
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
                return 0;

            uint32_t bit_ofs = index & (cBitsPerGroup - 1);

            uint32_t bit = (1U << (bit_ofs & 31));

            return (pGroup[bit_ofs >> 5] & bit) != 0;
        }

        inline bool operator[](uint32_t index) const
        {
            return get_bit(index);
        }

        inline uint32_t get_uint32(uint32_t index) const
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
                return 0;

            uint32_t bit_ofs = index & (cBitsPerGroup - 1);

            return pGroup[bit_ofs >> 5];
        }

        inline void set_uint32(uint32_t index, uint32_t value) const
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            uint32_t bit_ofs = index & (cBitsPerGroup - 1);

            pGroup[bit_ofs >> 5] = value;
        }

        int find_first_set_bit(uint32_t index, uint32_t num) const;

        enum
        {
            cDWORDsPerGroupShift = 4U,
            cDWORDsPerGroup = 1U << cDWORDsPerGroupShift,
            cBitsPerGroupShift = cDWORDsPerGroupShift + 5,
            cBitsPerGroup = 1U << cBitsPerGroupShift,
            cBitsPerGroupMask = cBitsPerGroup - 1U,
            cBytesPerGroup = cDWORDsPerGroup * sizeof(uint32_t)
        };

        uint32_t get_num_groups() const
        {
            return m_num_groups;
        }
        uint32_t **get_groups()
        {
            return m_ppGroups;
        }

    private:
        uint32_t m_num_groups;
        uint32_t **m_ppGroups;

        static inline uint32_t *alloc_group(bool clear)
        {
            uint32_t *p = (uint32_t *)vogl_malloc(cBytesPerGroup);
            VOGL_VERIFY(p);
            if (clear)
                memset(p, 0, cBytesPerGroup);
            return p;
        }

        static inline void free_group(void *p)
        {
            if (p)
                vogl_free(p);
        }
    };

} // namespace vogl
