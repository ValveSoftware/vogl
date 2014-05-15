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

// File: vogl_vec_interval.h
#pragma once

#include "vogl_core.h"
#include "vogl_vec.h"

namespace vogl
{
    enum eInitExpand
    {
        cInitExpand
    };

    template <typename T>
    class vec_interval
    {
    public:
        enum
        {
            N = T::num_elements
        };
        typedef typename T::scalar_type scalar_type;

        inline vec_interval()
        {
        }
        inline vec_interval(eInitExpand)
        {
            init_expand();
        }
        inline vec_interval(const T &v)
        {
            m_bounds[0] = v;
            m_bounds[1] = v;
        }
        inline vec_interval(const T &low, const T &high)
        {
            m_bounds[0] = low;
            m_bounds[1] = high;
        }

        inline void clear()
        {
            m_bounds[0].clear();
            m_bounds[1].clear();
        }

        inline const T &operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < 2);
            return m_bounds[i];
        }
        inline T &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < 2);
            return m_bounds[i];
        }

        inline T get_range() const
        {
            return m_bounds[1] - m_bounds[0];
        }
        inline scalar_type get_dimension(uint32_t axis) const
        {
            return m_bounds[1][axis] - m_bounds[0][axis];
        }

        inline void init_expand()
        {
            m_bounds[0].set(std::numeric_limits<scalar_type>::max());
            m_bounds[1].set(std::numeric_limits<scalar_type>::lowest());
        }

        inline void expand(const T &val)
        {
            m_bounds[0] = T::component_min(m_bounds[0], val);
            m_bounds[1] = T::component_max(m_bounds[1], val);
        }

        inline bool is_null() const
        {
            for (uint32_t i = 0; i < N; i++)
                if (m_bounds[0][i] > m_bounds[1][i])
                    return true;
            return false;
        }

    private:
        T m_bounds[2];
    };

    typedef vec_interval<vec1F> vec_interval1F;
    typedef vec_interval<vec2I> vec_interval2I;
    typedef vec_interval<vec2F> vec_interval2F;
    typedef vec_interval<vec3F> vec_interval3F;
    typedef vec_interval<vec4F> vec_interval4F;

    typedef vec_interval2F aabb2F;
    typedef vec_interval3F aabb3F;

    template <typename T>
    class scalar_interval
    {
    public:
        typedef T scalar_type;

        inline scalar_interval()
        {
        }
        inline scalar_interval(eInitExpand)
        {
            init_expand();
        }
        inline scalar_interval(const T v)
        {
            m_bounds[0] = v;
            m_bounds[1] = v;
        }
        inline scalar_interval(const T low, const T high)
        {
            m_bounds[0] = low;
            m_bounds[1] = high;
        }

        inline void clear()
        {
            m_bounds[0] = 0;
            m_bounds[1] = 0;
        }

        inline const T operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < 2);
            return m_bounds[i];
        }
        inline T &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < 2);
            return m_bounds[i];
        }

        inline T get_range() const
        {
            return m_bounds[1] - m_bounds[0];
        }
        inline T get_dimension() const
        {
            return get_range();
        }

        inline void init_expand()
        {
            m_bounds[0] = std::numeric_limits<scalar_type>::max();
            m_bounds[1] = std::numeric_limits<scalar_type>::min();
        }

        inline void expand(const T val)
        {
            m_bounds[0] = math::minimum(m_bounds[0], val);
            m_bounds[1] = math::maximum(m_bounds[1], val);
        }

        inline bool is_null() const
        {
            return m_bounds[0] > m_bounds[1];
        }

    private:
        T m_bounds[2];
    };

    typedef scalar_interval<float> scalar_intervalF;
    typedef scalar_interval<double> scalar_intervalD;

} // namespace vogl
