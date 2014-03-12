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

// File: vogl_vector2d.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename T>
    class vector2D
    {
    public:
        typedef vogl::vector<T> vector_type;
        typedef T value_type;
        typedef T &reference;
        typedef const T &const_reference;
        typedef T *pointer;
        typedef const T *const_pointer;

        inline vector2D(uint width = 0, uint height = 0, const T &def = T())
            : m_width(width),
              m_height(height),
              m_vec(width * height),
              m_def(def)
        {
        }

        inline vector2D(const vector2D &other)
            : m_width(other.m_width),
              m_height(other.m_height),
              m_vec(other.m_vec),
              m_def(other.m_def)
        {
        }

        inline vector2D &operator=(const vector2D &rhs)
        {
            if (this == &rhs)
                return *this;

            m_width = rhs.m_width;
            m_height = rhs.m_height;
            m_vec = rhs.m_vec;

            return *this;
        }

        bool try_resize(uint width, uint height, bool preserve = true)
        {
            if ((width == m_width) && (height == m_height))
                return true;

            vector_type new_vec;
            if (!new_vec.try_resize(width * height))
                return false;

            if (preserve)
            {
                const uint nx = math::minimum(width, m_width);
                const uint ny = math::minimum(height, m_height);

                for (uint y = 0; y < ny; y++)
                {
                    const T *pSrc = &m_vec[y * m_width];
                    T *pDst = &new_vec[y * width];
                    if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
                        memcpy(pDst, pSrc, nx * sizeof(T));
                    else
                    {
                        for (uint x = 0; x < nx; x++)
                            *pDst++ = *pSrc++;
                    }
                }
            }

            m_width = width;
            m_height = height;
            m_vec.swap(new_vec);

            return true;
        }

        void resize(uint width, uint height, bool preserve = true)
        {
            if (!try_resize(width, height, preserve))
            {
                VOGL_FAIL("vector2D::resize: Out of memory");
            }
        }

        inline void clear()
        {
            m_vec.clear();
            m_width = 0;
            m_height = 0;
        }

        inline uint width() const
        {
            return m_width;
        }
        inline uint height() const
        {
            return m_height;
        }
        inline uint size() const
        {
            return m_vec.size();
        }

        inline uint size_in_bytes() const
        {
            return m_vec.size() * sizeof(T);
        }

        const vector_type &get_vec() const
        {
            return m_vec;
        }
        vector_type &get_vec()
        {
            return m_vec;
        }

        inline const T *get_ptr() const
        {
            return m_vec.get_ptr();
        }
        inline T *get_ptr()
        {
            return m_vec.get_ptr();
        }

        inline const T &operator[](uint i) const
        {
            return m_vec[i];
        }
        inline T &operator[](uint i)
        {
            return m_vec[i];
        }

        inline const T &operator()(uint x, uint y) const
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            return m_vec[x + y * m_width];
        }

        inline T &operator()(uint x, uint y)
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            return m_vec[x + y * m_width];
        }

        inline const T &at(uint x, uint y) const
        {
            if ((x >= m_width) || (y >= m_height))
                return m_def;
            return m_vec[x + y * m_width];
        }

        inline T &at(uint x, uint y)
        {
            if ((x >= m_width) || (y >= m_height))
                return m_def;
            return m_vec[x + y * m_width];
        }

        inline const T &at_clamped(int x, int y) const
        {
            x = math::clamp<int>(x, 0, m_width - 1);
            y = math::clamp<int>(y, 0, m_height - 1);
            return m_vec[x + y * m_width];
        }

        inline T &at_clamped(int x, int y)
        {
            x = math::clamp<int>(x, 0, m_width - 1);
            y = math::clamp<int>(y, 0, m_height - 1);
            return m_vec[x + y * m_width];
        }

        inline const T &at_wrapped(int x, int y) const
        {
            x = math::posmod(x, m_width);
            y = math::posmod(y, m_height);
            return m_vec[x + y * m_width];
        }

        inline T &at_wrapped(int x, int y)
        {
            x = math::posmod(x, m_width);
            y = math::posmod(y, m_height);
            return m_vec[x + y * m_width];
        }

        inline void swap(vector2D &other)
        {
            m_vec.swap(other.m_vec);
            std::swap(m_width, other.m_width);
            std::swap(m_height, other.m_height);
            std::swap(m_def, other.m_def);
        }

        inline void set_all(const T &x)
        {
            m_vec.set_all(x);
        }

        inline void extract_block_clamped(vector2D &dst, uint dst_x, uint dst_y, int src_x, int src_y, uint w, uint h) const
        {
            for (uint y = 0; y < h; y++)
                for (uint x = 0; x < w; x++)
                    dst.at(dst_x + x, dst_y + y) = at_clamped(src_x + x, src_y + y);
        }

        inline void extract_block_wrapped(vector2D &dst, uint dst_x, uint dst_y, int src_x, int src_y, uint w, uint h) const
        {
            for (uint y = 0; y < h; y++)
                for (uint x = 0; x < w; x++)
                    dst.at(dst_x + x, dst_y + y) = at_wrapped(src_x + x, src_y + y);
        }

        // Doesn't compare the default object
        inline bool operator==(const vector2D &rhs) const
        {
            return (m_width == rhs.m_width) && (m_height == rhs.m_height) && (m_vec == rhs.m_vec);
        }

        inline bool operator!=(const vector2D &rhs) const
        {
            return !(*this == rhs);
        }

    private:
        uint m_width;
        uint m_height;
        vector_type m_vec;
        T m_def;
    };

} // namespace vogl
