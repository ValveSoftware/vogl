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

// File: vogl_fixed_array.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename T, uint32_t N>
    class fixed_array
    {
    public:
        typedef T element_type;
        enum
        {
            cMaxElements = N
        };

        inline fixed_array()
        {
            VOGL_ASSUME(N > 0);
        }

        inline uint32_t size() const
        {
            return N;
        }

        inline uint32_t size_in_bytes() const
        {
            return sizeof(T) * N;
        }

        inline const T &operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < N);
            return m[i];
        }
        inline T &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < N);
            return m[i];
        }

        inline const T &front() const
        {
            return m[0];
        }
        inline T &front()
        {
            return m[0];
        }

        inline const T &back() const
        {
            return m[N - 1];
        }
        inline T &back()
        {
            return m[N - 1];
        }

        inline const T *begin() const
        {
            return m;
        }
        inline T *begin()
        {
            return m;
        }

        inline const T *end() const
        {
            return &m[N];
        }
        inline T *end()
        {
            return &m[N];
        }

        inline const T *get_ptr() const
        {
            return m;
        }
        inline T *get_ptr()
        {
            return m;
        }

        inline void set_all(const T &val)
        {
            for (uint32_t i = 0; i < N; i++)
                m[i] = val;
        }

        inline void bitwise_zero(void)
        {
            utils::zero_this(this);
        }

        inline void sort(void)
        {
            //std::sort(begin(), end());
            introsort(begin(), end());
        }

    private:
        T m[N];
    };

    template <typename T, uint32_t W, uint32_t H>
    class fixed_array2D
    {
    public:
        enum
        {
            total_elements = W * H,
            array_width = W,
            array_height = H
        };

        typedef T value_type;
        typedef T &reference;
        typedef const T &const_reference;
        typedef T *pointer;
        typedef const T *const_pointer;

        inline fixed_array2D()
        {
            set_all(T());
        }

        inline fixed_array2D(const fixed_array2D &other)
        {
            *this = other;
        }

        inline fixed_array2D &operator=(const fixed_array2D &rhs)
        {
            if (this == &rhs)
                return *this;

            for (uint32_t i = 0; i < total_elements; i++)
                m_vec[i] = rhs.m_vec[i];

            return *this;
        }

        inline void clear()
        {
            set_all(T());
        }

        void resize(uint32_t width, uint32_t height, bool preserve = true)
        {
            (void)width, (void)height, (void)preserve;
            // blindly ignoring resize for compat with vector2D
        }

        inline uint32_t width() const
        {
            return array_width;
        }
        inline uint32_t height() const
        {
            return array_height;
        }
        inline uint32_t size() const
        {
            return total_elements;
        }

        inline uint32_t size_in_bytes() const
        {
            return total_elements * sizeof(T);
        }

        inline const T *get_ptr() const
        {
            return m_vec;
        }
        inline T *get_ptr()
        {
            return m_vec;
        }

        inline const T &operator[](uint32_t i) const
        {
            return m_vec[math::open_range_check(i, total_elements)];
        }
        inline T &operator[](uint32_t i)
        {
            return m_vec[math::open_range_check(i, total_elements)];
        }

        inline const T &operator()(uint32_t x, uint32_t y) const
        {
            VOGL_ASSERT((x < array_width) && (y < array_height));
            return m_vec[x + y * array_width];
        }

        inline T &operator()(uint32_t x, uint32_t y)
        {
            VOGL_ASSERT((x < array_width) && (y < array_height));
            return m_vec[x + y * array_width];
        }

        inline const T &at(uint32_t x, uint32_t y) const
        {
            if ((x >= array_width) || (y >= array_height))
            {
                VOGL_ASSERT_ALWAYS;
                return m_vec[0];
            }
            return m_vec[x + y * array_width];
        }

        inline T &at(uint32_t x, uint32_t y)
        {
            if ((x >= array_width) || (y >= array_height))
            {
                VOGL_ASSERT_ALWAYS;
                return m_vec[0];
            }
            return m_vec[x + y * array_width];
        }

        inline const T &at_clamped(int x, int y) const
        {
            x = math::clamp<int>(x, 0, array_width - 1);
            y = math::clamp<int>(y, 0, array_height - 1);
            return m_vec[x + y * array_width];
        }

        inline T &at_clamped(int x, int y)
        {
            x = math::clamp<int>(x, 0, array_width - 1);
            y = math::clamp<int>(y, 0, array_height - 1);
            return m_vec[x + y * array_width];
        }

        inline const T &at_wrapped(int x, int y) const
        {
            x = math::posmod(x, array_width);
            y = math::posmod(y, array_height);
            return m_vec[x + y * array_width];
        }

        inline T &at_wrapped(int x, int y)
        {
            x = math::posmod(x, array_width);
            y = math::posmod(y, array_height);
            return m_vec[x + y * array_width];
        }

        inline void swap(fixed_array2D &other)
        {
            m_vec.swap(other.m_vec);
            std::swap(array_width, other.array_width);
            std::swap(array_height, other.array_height);
        }

        inline void set_all(const T &x)
        {
            for (uint32_t i = 0; i < total_elements; i++)
                m_vec[i] = x;
        }

        inline void bitwise_zero(void)
        {
            utils::zero_this(this);
        }

        inline void extract_block_clamped(fixed_array2D &dst, uint32_t dst_x, uint32_t dst_y, int src_x, int src_y, uint32_t w, uint32_t h) const
        {
            for (uint32_t y = 0; y < h; y++)
                for (uint32_t x = 0; x < w; x++)
                    dst.at(dst_x + x, dst_y + y) = at_clamped(src_x + x, src_y + y);
        }

        inline void extract_block_wrapped(fixed_array2D &dst, uint32_t dst_x, uint32_t dst_y, int src_x, int src_y, uint32_t w, uint32_t h) const
        {
            for (uint32_t y = 0; y < h; y++)
                for (uint32_t x = 0; x < w; x++)
                    dst.at(dst_x + x, dst_y + y) = at_wrapped(src_x + x, src_y + y);
        }

        inline bool operator==(const fixed_array2D &rhs) const
        {
            for (uint32_t i = 0; i < total_elements; i++)
                if (m_vec[i] != rhs.m_vec[i])
                    return false;
            return true;
        }

    private:
        T m_vec[W * H];
    };

} // namespace vogl
