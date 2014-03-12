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

// File: vogl_ray.h
#pragma once

#include "vogl_core.h"
#include "vogl_vec.h"

namespace vogl
{
    template <typename vector_type>
    class ray
    {
    public:
        typedef vector_type vector_t;
        typedef typename vector_type::scalar_type scalar_type;

        inline ray()
        {
        }
        inline ray(eClear)
        {
            clear();
        }
        inline ray(const vector_type &origin, const vector_type &direction)
            : m_origin(origin), m_direction(direction)
        {
        }

        inline void clear()
        {
            m_origin.clear();
            m_direction.clear();
        }

        inline const vector_type &get_origin(void) const
        {
            return m_origin;
        }
        inline void set_origin(const vector_type &origin)
        {
            m_origin = origin;
        }

        inline const vector_type &get_direction(void) const
        {
            return m_direction;
        }
        inline void set_direction(const vector_type &direction)
        {
            m_direction = direction;
        }

        inline scalar_type set_endpoints(const vector_type &start, const vector_type &end, const vector_type &def)
        {
            m_origin = start;

            m_direction = end - start;
            return static_cast<scalar_type>(m_direction.normalize(&def));
        }

        inline vector_type eval(scalar_type t) const
        {
            return m_origin + m_direction * t;
        }

    private:
        vector_type m_origin;
        vector_type m_direction;
    };

    typedef ray<vec2F> ray2F;
    typedef ray<vec3F> ray3F;

} // namespace vogl
