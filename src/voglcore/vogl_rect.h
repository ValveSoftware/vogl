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

// File: vogl_rect.h
#pragma once

#include "vogl_core.h"
#include "vogl_vec.h"
#include "vogl_hash.h"

namespace vogl
{
    class rect
    {
    public:
        inline rect()
        {
        }

        inline rect(eClear)
        {
            clear();
        }

        // up to, but not including right/bottom
        inline rect(int left, int top, int right, int bottom)
        {
            set(left, top, right, bottom);
        }

        inline rect(const vec2I &lo, const vec2I &hi)
        {
            m_corner[0] = lo;
            m_corner[1] = hi;
        }

        inline rect(const vec2I &point)
        {
            m_corner[0] = point;
            m_corner[1].set(point[0] + 1, point[1] + 1);
        }

        inline bool operator==(const rect &r) const
        {
            return (m_corner[0] == r.m_corner[0]) && (m_corner[1] == r.m_corner[1]);
        }

        inline bool operator<(const rect &r) const
        {
            for (uint32_t i = 0; i < 2; i++)
            {
                if (m_corner[i] < r.m_corner[i])
                    return true;
                else if (!(m_corner[i] == r.m_corner[i]))
                    return false;
            }

            return false;
        }

        inline void clear()
        {
            m_corner[0].clear();
            m_corner[1].clear();
        }

        inline void set(int left, int top, int right, int bottom)
        {
            m_corner[0].set(left, top);
            m_corner[1].set(right, bottom);
        }

        inline void set(const vec2I &lo, const vec2I &hi)
        {
            m_corner[0] = lo;
            m_corner[1] = hi;
        }

        inline void set(const vec2I &point)
        {
            m_corner[0] = point;
            m_corner[1].set(point[0] + 1, point[1] + 1);
        }

        inline uint32_t get_width() const
        {
            return m_corner[1][0] - m_corner[0][0];
        }
        inline uint32_t get_height() const
        {
            return m_corner[1][1] - m_corner[0][1];
        }

        inline int get_left() const
        {
            return m_corner[0][0];
        }
        inline int get_top() const
        {
            return m_corner[0][1];
        }
        inline int get_right() const
        {
            return m_corner[1][0];
        }
        inline int get_bottom() const
        {
            return m_corner[1][1];
        }

        inline bool is_empty() const
        {
            return (m_corner[1][0] <= m_corner[0][0]) || (m_corner[1][1] <= m_corner[0][1]);
        }

        inline uint32_t get_dimension(uint32_t axis) const
        {
            return m_corner[1][axis] - m_corner[0][axis];
        }
        inline uint32_t get_area() const
        {
            return get_dimension(0) * get_dimension(1);
        }

        inline const vec2I &operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < 2);
            return m_corner[i];
        }
        inline vec2I &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < 2);
            return m_corner[i];
        }

        inline rect &translate(int x_ofs, int y_ofs)
        {
            m_corner[0][0] += x_ofs;
            m_corner[0][1] += y_ofs;
            m_corner[1][0] += x_ofs;
            m_corner[1][1] += y_ofs;
            return *this;
        }

        inline rect &init_expand()
        {
            m_corner[0].set(INT_MAX);
            m_corner[1].set(INT_MIN);
            return *this;
        }

        inline rect &expand(int x, int y)
        {
            m_corner[0][0] = math::minimum(m_corner[0][0], x);
            m_corner[0][1] = math::minimum(m_corner[0][1], y);
            m_corner[1][0] = math::maximum(m_corner[1][0], x + 1);
            m_corner[1][1] = math::maximum(m_corner[1][1], y + 1);
            return *this;
        }

        inline rect &expand(const rect &r)
        {
            m_corner[0][0] = math::minimum(m_corner[0][0], r[0][0]);
            m_corner[0][1] = math::minimum(m_corner[0][1], r[0][1]);
            m_corner[1][0] = math::maximum(m_corner[1][0], r[1][0]);
            m_corner[1][1] = math::maximum(m_corner[1][1], r[1][1]);
            return *this;
        }

        inline bool touches(const rect &r) const
        {
            for (uint32_t i = 0; i < 2; i++)
            {
                if (r[1][i] <= m_corner[0][i])
                    return false;
                else if (r[0][i] >= m_corner[1][i])
                    return false;
            }

            return true;
        }

        inline bool fully_within(const rect &r) const
        {
            for (uint32_t i = 0; i < 2; i++)
            {
                if (m_corner[0][i] < r[0][i])
                    return false;
                else if (m_corner[1][i] > r[1][i])
                    return false;
            }

            return true;
        }

        inline bool intersect(const rect &r)
        {
            if (!touches(r))
            {
                clear();
                return false;
            }

            for (uint32_t i = 0; i < 2; i++)
            {
                m_corner[0][i] = math::maximum<int>(m_corner[0][i], r[0][i]);
                m_corner[1][i] = math::minimum<int>(m_corner[1][i], r[1][i]);
            }

            return true;
        }

        inline bool contains(int x, int y) const
        {
            return (x >= m_corner[0][0]) && (x < m_corner[1][0]) &&
                   (y >= m_corner[0][1]) && (y < m_corner[1][1]);
        }

        inline bool contains(const vec2I &p) const
        {
            return contains(p[0], p[1]);
        }

    private:
        vec2I m_corner[2];
    };

    inline rect make_rect(uint32_t width, uint32_t height)
    {
        return rect(0, 0, width, height);
    }

} // namespace vogl
