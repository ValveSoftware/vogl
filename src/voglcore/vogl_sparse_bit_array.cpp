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
#include "vogl_core.h"
#include "vogl_sparse_bit_array.h"

namespace vogl
{
    // TODO: This is for the sparse_array class.
    uint32_t g_sparse_vector_test_counters[2];

    sparse_bit_array::sparse_bit_array()
        : m_num_groups(0), m_ppGroups(NULL)
    {
    }

    sparse_bit_array::sparse_bit_array(uint32_t size)
        : m_num_groups(0), m_ppGroups(NULL)
    {
        resize(size);
    }

    sparse_bit_array::sparse_bit_array(sparse_bit_array &other)
    {
        m_num_groups = other.m_num_groups;
        m_ppGroups = (uint32_t **)vogl_malloc(m_num_groups * sizeof(uint32_t *));
        VOGL_VERIFY(m_ppGroups);

        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            if (other.m_ppGroups[i])
            {
                m_ppGroups[i] = alloc_group(false);
                memcpy(m_ppGroups[i], other.m_ppGroups[i], cBytesPerGroup);
            }
            else
                m_ppGroups[i] = NULL;
        }
    }

    sparse_bit_array::~sparse_bit_array()
    {
        clear();
    }

    sparse_bit_array &sparse_bit_array::operator=(sparse_bit_array &other)
    {
        if (this == &other)
            return *this;

        if (m_num_groups != other.m_num_groups)
        {
            clear();

            m_num_groups = other.m_num_groups;
            m_ppGroups = (uint32_t **)vogl_calloc(m_num_groups, sizeof(uint32_t *));
            VOGL_VERIFY(m_ppGroups);
        }

        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            if (other.m_ppGroups[i])
            {
                if (!m_ppGroups[i])
                    m_ppGroups[i] = alloc_group(false);
                memcpy(m_ppGroups[i], other.m_ppGroups[i], cBytesPerGroup);
            }
            else if (m_ppGroups[i])
            {
                free_group(m_ppGroups[i]);
                m_ppGroups[i] = NULL;
            }
        }

        return *this;
    }

    void sparse_bit_array::clear()
    {
        if (!m_num_groups)
            return;

        for (uint32_t i = 0; i < m_num_groups; i++)
            free_group(m_ppGroups[i]);

        vogl_free(m_ppGroups);
        m_ppGroups = NULL;

        m_num_groups = 0;
    }

    void sparse_bit_array::swap(sparse_bit_array &other)
    {
        utils::swap(m_ppGroups, other.m_ppGroups);
        utils::swap(m_num_groups, other.m_num_groups);
    }

    void sparse_bit_array::optimize()
    {
        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            uint32_t *s = m_ppGroups[i];
            if (s)
            {
                uint32_t j;
                for (j = 0; j < cDWORDsPerGroup; j++)
                    if (s[j])
                        break;
                if (j == cDWORDsPerGroup)
                {
                    free_group(s);
                    m_ppGroups[i] = NULL;
                }
            }
        }
    }

    void sparse_bit_array::set_bit_range(uint32_t index, uint32_t num)
    {
        VOGL_ASSERT((index + num) <= (m_num_groups << cBitsPerGroupShift));

        if (!num)
            return;
        else if (num == 1)
        {
            set_bit(index);
            return;
        }

        while ((index & cBitsPerGroupMask) || (num <= cBitsPerGroup))
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            const uint32_t group_bit_ofs = index & cBitsPerGroupMask;

            const uint32_t dword_bit_ofs = group_bit_ofs & 31;
            const uint32_t max_bits_to_set = 32 - dword_bit_ofs;

            const uint32_t bits_to_set = math::minimum(max_bits_to_set, num);
            const uint32_t msk = (0xFFFFFFFFU >> (32 - bits_to_set));

            pGroup[group_bit_ofs >> 5] |= (msk << dword_bit_ofs);

            num -= bits_to_set;
            if (!num)
                return;

            index += bits_to_set;
        }

        while (num >= cBitsPerGroup)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            memset(pGroup, 0xFF, sizeof(uint32_t) * cDWORDsPerGroup);

            num -= cBitsPerGroup;
            index += cBitsPerGroup;
        }

        while (num)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (!pGroup)
            {
                pGroup = alloc_group(true);
                m_ppGroups[group_index] = pGroup;
            }

            uint32_t group_bit_ofs = index & cBitsPerGroupMask;

            uint32_t bits_to_set = math::minimum(32U, num);
            uint32_t msk = (0xFFFFFFFFU >> (32 - bits_to_set));

            pGroup[group_bit_ofs >> 5] |= (msk << (group_bit_ofs & 31));

            num -= bits_to_set;
            index += bits_to_set;
        }
    }

    void sparse_bit_array::clear_all_bits()
    {
        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            uint32_t *pGroup = m_ppGroups[i];
            if (pGroup)
                memset(pGroup, 0, sizeof(uint32_t) * cDWORDsPerGroup);
        }
    }

    void sparse_bit_array::clear_bit_range(uint32_t index, uint32_t num)
    {
        VOGL_ASSERT((index + num) <= (m_num_groups << cBitsPerGroupShift));

        if (!num)
            return;
        else if (num == 1)
        {
            clear_bit(index);
            return;
        }

        while ((index & cBitsPerGroupMask) || (num <= cBitsPerGroup))
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            const uint32_t group_bit_ofs = index & cBitsPerGroupMask;

            const uint32_t dword_bit_ofs = group_bit_ofs & 31;
            const uint32_t max_bits_to_set = 32 - dword_bit_ofs;

            const uint32_t bits_to_set = math::minimum(max_bits_to_set, num);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (pGroup)
            {
                const uint32_t msk = (0xFFFFFFFFU >> (32 - bits_to_set));

                pGroup[group_bit_ofs >> 5] &= (~(msk << dword_bit_ofs));
            }

            num -= bits_to_set;
            if (!num)
                return;

            index += bits_to_set;
        }

        while (num >= cBitsPerGroup)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (pGroup)
            {
                free_group(pGroup);
                m_ppGroups[group_index] = NULL;
            }

            num -= cBitsPerGroup;
            index += cBitsPerGroup;
        }

        while (num)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t bits_to_set = math::minimum(32u, num);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (pGroup)
            {
                uint32_t group_bit_ofs = index & cBitsPerGroupMask;

                uint32_t msk = (0xFFFFFFFFU >> (32 - bits_to_set));

                pGroup[group_bit_ofs >> 5] &= (~(msk << (group_bit_ofs & 31)));
            }

            num -= bits_to_set;
            index += bits_to_set;
        }
    }

    void sparse_bit_array::resize(uint32_t size)
    {
        uint32_t num_groups = (size + cBitsPerGroup - 1) >> cBitsPerGroupShift;
        if (num_groups == m_num_groups)
            return;

        if (!num_groups)
        {
            clear();
            return;
        }

        sparse_bit_array temp;
        temp.swap(*this);

        m_num_groups = num_groups;
        m_ppGroups = (uint32_t **)vogl_calloc(m_num_groups, sizeof(uint32_t *));
        VOGL_VERIFY(m_ppGroups);

        uint32_t n = math::minimum(temp.m_num_groups, m_num_groups);
        for (uint32_t i = 0; i < n; i++)
        {
            uint32_t *p = temp.m_ppGroups[i];
            if (p)
            {
                m_ppGroups[i] = temp.m_ppGroups[i];
                temp.m_ppGroups[i] = NULL;
            }
        }
    }

    sparse_bit_array &sparse_bit_array::operator&=(const sparse_bit_array &other)
    {
        if (this == &other)
            return *this;

        VOGL_VERIFY(other.m_num_groups == m_num_groups);

        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            uint32_t *d = m_ppGroups[i];
            if (!d)
                continue;
            uint32_t *s = other.m_ppGroups[i];

            if (!s)
            {
                free_group(d);
                m_ppGroups[i] = NULL;
            }
            else
            {
                uint32_t oc = 0;
                for (uint32_t j = 0; j < cDWORDsPerGroup; j++)
                {
                    uint32_t c = d[j] & s[j];
                    d[j] = c;
                    oc |= c;
                }
                if (!oc)
                {
                    free_group(d);
                    m_ppGroups[i] = NULL;
                }
            }
        }

        return *this;
    }

    sparse_bit_array &sparse_bit_array::operator|=(const sparse_bit_array &other)
    {
        if (this == &other)
            return *this;

        VOGL_VERIFY(other.m_num_groups == m_num_groups);

        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            uint32_t *s = other.m_ppGroups[i];
            if (!s)
                continue;

            uint32_t *d = m_ppGroups[i];
            if (!d)
            {
                d = alloc_group(true);
                m_ppGroups[i] = d;
                memcpy(d, s, cBytesPerGroup);
            }
            else
            {
                uint32_t oc = 0;
                for (uint32_t j = 0; j < cDWORDsPerGroup; j++)
                {
                    uint32_t c = d[j] | s[j];
                    d[j] = c;
                    oc |= c;
                }
                if (!oc)
                {
                    free_group(d);
                    m_ppGroups[i] = NULL;
                }
            }
        }

        return *this;
    }

    sparse_bit_array &sparse_bit_array::and_not(const sparse_bit_array &other)
    {
        if (this == &other)
            return *this;

        VOGL_VERIFY(other.m_num_groups == m_num_groups);

        for (uint32_t i = 0; i < m_num_groups; i++)
        {
            uint32_t *d = m_ppGroups[i];
            if (!d)
                continue;
            uint32_t *s = other.m_ppGroups[i];
            if (!s)
                continue;

            uint32_t oc = 0;
            for (uint32_t j = 0; j < cDWORDsPerGroup; j++)
            {
                uint32_t c = d[j] & (~s[j]);
                d[j] = c;
                oc |= c;
            }
            if (!oc)
            {
                free_group(d);
                m_ppGroups[i] = NULL;
            }
        }

        return *this;
    }

    int sparse_bit_array::find_first_set_bit(uint32_t index, uint32_t num) const
    {
        VOGL_ASSERT((index + num) <= (m_num_groups << cBitsPerGroupShift));

        if (!num)
            return -1;

        while ((index & cBitsPerGroupMask) || (num <= cBitsPerGroup))
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            const uint32_t group_bit_ofs = index & cBitsPerGroupMask;
            const uint32_t dword_bit_ofs = group_bit_ofs & 31;

            const uint32_t max_bits_to_examine = 32 - dword_bit_ofs;
            const uint32_t bits_to_examine = math::minimum(max_bits_to_examine, num);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (pGroup)
            {
                const uint32_t msk = (0xFFFFFFFFU >> (32 - bits_to_examine));

                uint32_t bits = pGroup[group_bit_ofs >> 5] & (msk << dword_bit_ofs);
                if (bits)
                {
                    uint32_t num_trailing_zeros = math::count_trailing_zero_bits(bits);
                    int set_index = num_trailing_zeros + (index & ~31);
                    VOGL_ASSERT(get_bit(set_index));
                    return set_index;
                }
            }

            num -= bits_to_examine;
            if (!num)
                return -1;

            index += bits_to_examine;
        }

        while (num >= cBitsPerGroup)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (pGroup)
            {
                for (uint32_t i = 0; i < cDWORDsPerGroup; i++)
                {
                    uint32_t bits = pGroup[i];
                    if (bits)
                    {
                        uint32_t num_trailing_zeros = math::count_trailing_zero_bits(bits);

                        int set_index = num_trailing_zeros + index + (i << 5);
                        VOGL_ASSERT(get_bit(set_index));
                        return set_index;
                    }
                }
            }

            num -= cBitsPerGroup;
            index += cBitsPerGroup;
        }

        while (num)
        {
            uint32_t group_index = index >> cBitsPerGroupShift;
            VOGL_ASSERT(group_index < m_num_groups);

            uint32_t bits_to_examine = math::minimum(32U, num);

            uint32_t *pGroup = m_ppGroups[group_index];
            if (pGroup)
            {
                uint32_t group_bit_ofs = index & cBitsPerGroupMask;

                uint32_t msk = (0xFFFFFFFFU >> (32 - bits_to_examine));

                uint32_t bits = pGroup[group_bit_ofs >> 5] & (msk << (group_bit_ofs & 31));
                if (bits)
                {
                    uint32_t num_trailing_zeros = math::count_trailing_zero_bits(bits);

                    int set_index = num_trailing_zeros + (index & ~31);
                    VOGL_ASSERT(get_bit(set_index));
                    return set_index;
                }
            }

            num -= bits_to_examine;
            index += bits_to_examine;
        }

        return -1;
    }

} // namespace vogl
