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

// File: vogl_vec.h
#pragma once

#include "vogl_core.h"
#include "vogl_rand.h"

namespace vogl
{
    template <uint32_t N, typename T>
    class vec : public helpers::rel_ops<vec<N, T> >
    {
    public:
        typedef T scalar_type;
        enum
        {
            num_elements = N
        };

        inline vec()
        {
        }

        inline vec(eClear)
        {
            clear();
        }

        inline vec(const vec &other)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] = other.m_s[i];
        }

        template <uint32_t O, typename U>
        inline vec(const vec<O, U> &other)
        {
            set(other);
        }

        template <uint32_t O, typename U>
        inline vec(const vec<O, U> &other, T w)
        {
            *this = other;
            m_s[N - 1] = w;
        }

        explicit inline vec(T val)
        {
            set(val);
        }

        inline vec(T val0, T val1)
        {
            set(val0, val1);
        }

        inline vec(T val0, T val1, T val2)
        {
            set(val0, val1, val2);
        }

        inline vec(T val0, T val1, T val2, T val3)
        {
            set(val0, val1, val2, val3);
        }

        inline vec(
            T val0, T val1, T val2, T val3,
            T val4, T val5, T val6, T val7,
            T val8, T val9, T val10, T val11,
            T val12, T val13, T val14, T val15)
        {
            set(val0, val1, val2, val3,
                val4, val5, val6, val7,
                val8, val9, val10, val11,
                val12, val13, val14, val15);
        }

        inline vec(
            T val0, T val1, T val2, T val3,
            T val4, T val5, T val6, T val7,
            T val8, T val9, T val10, T val11,
            T val12, T val13, T val14, T val15,
            T val16, T val17, T val18, T val19)
        {
            set(val0, val1, val2, val3,
                val4, val5, val6, val7,
                val8, val9, val10, val11,
                val12, val13, val14, val15,
                val16, val17, val18, val19);
        }

        inline vec(
            T val0, T val1, T val2, T val3,
            T val4, T val5, T val6, T val7,
            T val8, T val9, T val10, T val11,
            T val12, T val13, T val14, T val15,
            T val16, T val17, T val18, T val19,
            T val20, T val21, T val22, T val23,
            T val24)
        {
            set(val0, val1, val2, val3,
                val4, val5, val6, val7,
                val8, val9, val10, val11,
                val12, val13, val14, val15,
                val16, val17, val18, val19,
                val20, val21, val22, val23,
                val24);
        }

        inline void clear()
        {
            if (N > 4)
                memset(m_s, 0, sizeof(m_s));
            else
            {
                for (uint32_t i = 0; i < N; i++)
                    m_s[i] = 0;
            }
        }

        template <uint32_t ON, typename OT>
        inline vec &set(const vec<ON, OT> &other)
        {
            if ((void *)this == (void *)&other)
                return *this;
            const uint32_t m = math::minimum(N, ON);
            uint32_t i;
            for (i = 0; i < m; i++)
                m_s[i] = static_cast<T>(other[i]);
            for (; i < N; i++)
                m_s[i] = 0;
            return *this;
        }

        inline vec &set_component(uint32_t index, T val)
        {
            VOGL_ASSERT(index < N);
            m_s[index] = val;
            return *this;
        }

        inline vec &set(T val)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] = val;
            return *this;
        }

        inline vec &set(T val0, T val1)
        {
            m_s[0] = val0;
            if (N >= 2)
            {
                m_s[1] = val1;

                for (uint32_t i = 2; i < N; i++)
                    m_s[i] = 0;
            }
            return *this;
        }

        inline vec &set(T val0, T val1, T val2)
        {
            m_s[0] = val0;
            if (N >= 2)
            {
                m_s[1] = val1;

                if (N >= 3)
                {
                    m_s[2] = val2;

                    for (uint32_t i = 3; i < N; i++)
                        m_s[i] = 0;
                }
            }
            return *this;
        }

        inline vec &set(T val0, T val1, T val2, T val3)
        {
            m_s[0] = val0;
            if (N >= 2)
            {
                m_s[1] = val1;

                if (N >= 3)
                {
                    m_s[2] = val2;

                    if (N >= 4)
                    {
                        m_s[3] = val3;

                        for (uint32_t i = 4; i < N; i++)
                            m_s[i] = 0;
                    }
                }
            }
            return *this;
        }

        inline vec &set(
            T val0, T val1, T val2, T val3,
            T val4, T val5, T val6, T val7,
            T val8, T val9, T val10, T val11,
            T val12, T val13, T val14, T val15)
        {
            m_s[0] = val0;
            if (N >= 2)
                m_s[1] = val1;
            if (N >= 3)
                m_s[2] = val2;
            if (N >= 4)
                m_s[3] = val3;

            if (N >= 5)
                m_s[4] = val4;
            if (N >= 6)
                m_s[5] = val5;
            if (N >= 7)
                m_s[6] = val6;
            if (N >= 8)
                m_s[7] = val7;

            if (N >= 9)
                m_s[8] = val8;
            if (N >= 10)
                m_s[9] = val9;
            if (N >= 11)
                m_s[10] = val10;
            if (N >= 12)
                m_s[11] = val11;

            if (N >= 13)
                m_s[12] = val12;
            if (N >= 14)
                m_s[13] = val13;
            if (N >= 15)
                m_s[14] = val14;
            if (N >= 16)
                m_s[15] = val15;

            for (uint32_t i = 16; i < N; i++)
                m_s[i] = 0;

            return *this;
        }

        inline vec &set(
            T val0, T val1, T val2, T val3,
            T val4, T val5, T val6, T val7,
            T val8, T val9, T val10, T val11,
            T val12, T val13, T val14, T val15,
            T val16, T val17, T val18, T val19)
        {
            m_s[0] = val0;
            if (N >= 2)
                m_s[1] = val1;
            if (N >= 3)
                m_s[2] = val2;
            if (N >= 4)
                m_s[3] = val3;

            if (N >= 5)
                m_s[4] = val4;
            if (N >= 6)
                m_s[5] = val5;
            if (N >= 7)
                m_s[6] = val6;
            if (N >= 8)
                m_s[7] = val7;

            if (N >= 9)
                m_s[8] = val8;
            if (N >= 10)
                m_s[9] = val9;
            if (N >= 11)
                m_s[10] = val10;
            if (N >= 12)
                m_s[11] = val11;

            if (N >= 13)
                m_s[12] = val12;
            if (N >= 14)
                m_s[13] = val13;
            if (N >= 15)
                m_s[14] = val14;
            if (N >= 16)
                m_s[15] = val15;

            if (N >= 17)
                m_s[16] = val16;
            if (N >= 18)
                m_s[17] = val17;
            if (N >= 19)
                m_s[18] = val18;
            if (N >= 20)
                m_s[19] = val19;

            for (uint32_t i = 20; i < N; i++)
                m_s[i] = 0;

            return *this;
        }

        inline vec &set(
            T val0, T val1, T val2, T val3,
            T val4, T val5, T val6, T val7,
            T val8, T val9, T val10, T val11,
            T val12, T val13, T val14, T val15,
            T val16, T val17, T val18, T val19,
            T val20, T val21, T val22, T val23,
            T val24)
        {
            m_s[0] = val0;
            if (N >= 2)
                m_s[1] = val1;
            if (N >= 3)
                m_s[2] = val2;
            if (N >= 4)
                m_s[3] = val3;

            if (N >= 5)
                m_s[4] = val4;
            if (N >= 6)
                m_s[5] = val5;
            if (N >= 7)
                m_s[6] = val6;
            if (N >= 8)
                m_s[7] = val7;

            if (N >= 9)
                m_s[8] = val8;
            if (N >= 10)
                m_s[9] = val9;
            if (N >= 11)
                m_s[10] = val10;
            if (N >= 12)
                m_s[11] = val11;

            if (N >= 13)
                m_s[12] = val12;
            if (N >= 14)
                m_s[13] = val13;
            if (N >= 15)
                m_s[14] = val14;
            if (N >= 16)
                m_s[15] = val15;

            if (N >= 17)
                m_s[16] = val16;
            if (N >= 18)
                m_s[17] = val17;
            if (N >= 19)
                m_s[18] = val18;
            if (N >= 20)
                m_s[19] = val19;

            if (N >= 21)
                m_s[20] = val20;
            if (N >= 22)
                m_s[21] = val21;
            if (N >= 23)
                m_s[22] = val22;
            if (N >= 24)
                m_s[23] = val23;

            if (N >= 25)
                m_s[24] = val24;

            for (uint32_t i = 25; i < N; i++)
                m_s[i] = 0;

            return *this;
        }

        inline vec &set(const T *pValues)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] = pValues[i];
            return *this;
        }

        template <uint32_t ON, typename OT>
        inline vec &swizzle_set(const vec<ON, OT> &other, uint32_t i)
        {
            return set(static_cast<T>(other[i]));
        }

        template <uint32_t ON, typename OT>
        inline vec &swizzle_set(const vec<ON, OT> &other, uint32_t i, uint32_t j)
        {
            return set(static_cast<T>(other[i]), static_cast<T>(other[j]));
        }

        template <uint32_t ON, typename OT>
        inline vec &swizzle_set(const vec<ON, OT> &other, uint32_t i, uint32_t j, uint32_t k)
        {
            return set(static_cast<T>(other[i]), static_cast<T>(other[j]), static_cast<T>(other[k]));
        }

        template <uint32_t ON, typename OT>
        inline vec &swizzle_set(const vec<ON, OT> &other, uint32_t i, uint32_t j, uint32_t k, uint32_t l)
        {
            return set(static_cast<T>(other[i]), static_cast<T>(other[j]), static_cast<T>(other[k]), static_cast<T>(other[l]));
        }

        inline vec &operator=(const vec &rhs)
        {
            if (this != &rhs)
            {
                for (uint32_t i = 0; i < N; i++)
                    m_s[i] = rhs.m_s[i];
            }
            return *this;
        }

        template <uint32_t O, typename U>
        inline vec &operator=(const vec<O, U> &other)
        {
            if ((void *)this == (void *)&other)
                return *this;

            uint32_t s = math::minimum(N, O);

            uint32_t i;
            for (i = 0; i < s; i++)
                m_s[i] = static_cast<T>(other[i]);

            for (; i < N; i++)
                m_s[i] = 0;

            return *this;
        }

        inline bool operator==(const vec &rhs) const
        {
            for (uint32_t i = 0; i < N; i++)
                if (!(m_s[i] == rhs.m_s[i]))
                    return false;
            return true;
        }

        inline bool operator<(const vec &rhs) const
        {
            for (uint32_t i = 0; i < N; i++)
            {
                if (m_s[i] < rhs.m_s[i])
                    return true;
                else if (!(m_s[i] == rhs.m_s[i]))
                    return false;
            }

            return false;
        }

        inline T operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < N);
            return m_s[i];
        }

        inline T &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < N);
            return m_s[i];
        }

        template <uint32_t index>
        inline uint64_t get_component_as_uint() const
        {
            VOGL_ASSUME(index < N);
            if (sizeof(T) == sizeof(float))
                return *reinterpret_cast<const uint32_t *>(&m_s[index]);
            else
                return *reinterpret_cast<const uint64_t *>(&m_s[index]);
        }

        inline T get_x(void) const
        {
            return m_s[0];
        }
        inline T get_y(void) const
        {
            VOGL_ASSUME(N >= 2);
            return m_s[1];
        }
        inline T get_z(void) const
        {
            VOGL_ASSUME(N >= 3);
            return m_s[2];
        }
        inline T get_w(void) const
        {
            VOGL_ASSUME(N >= 4);
            return m_s[3];
        }

        inline vec get_x_vector() const
        {
            return broadcast<0>();
        }
        inline vec get_y_vector() const
        {
            return broadcast<1>();
        }
        inline vec get_z_vector() const
        {
            return broadcast<2>();
        }
        inline vec get_w_vector() const
        {
            return broadcast<3>();
        }

        inline T get_component(uint32_t i) const
        {
            return (*this)[i];
        }

        inline vec &set_x(T v)
        {
            m_s[0] = v;
            return *this;
        }
        inline vec &set_y(T v)
        {
            VOGL_ASSUME(N >= 2);
            m_s[1] = v;
            return *this;
        }
        inline vec &set_z(T v)
        {
            VOGL_ASSUME(N >= 3);
            m_s[2] = v;
            return *this;
        }
        inline vec &set_w(T v)
        {
            VOGL_ASSUME(N >= 4);
            m_s[3] = v;
            return *this;
        }

        inline const T *get_ptr() const
        {
            return reinterpret_cast<const T *>(&m_s[0]);
        }
        inline T *get_ptr()
        {
            return reinterpret_cast<T *>(&m_s[0]);
        }

        inline vec as_point() const
        {
            vec result(*this);
            result[N - 1] = 1;
            return result;
        }

        inline vec as_dir() const
        {
            vec result(*this);
            result[N - 1] = 0;
            return result;
        }

        inline vec<2, T> select2(uint32_t i, uint32_t j) const
        {
            VOGL_ASSERT((i < N) && (j < N));
            return vec<2, T>(m_s[i], m_s[j]);
        }

        inline vec<3, T> select3(uint32_t i, uint32_t j, uint32_t k) const
        {
            VOGL_ASSERT((i < N) && (j < N) && (k < N));
            return vec<3, T>(m_s[i], m_s[j], m_s[k]);
        }

        inline vec<4, T> select4(uint32_t i, uint32_t j, uint32_t k, uint32_t l) const
        {
            VOGL_ASSERT((i < N) && (j < N) && (k < N) && (l < N));
            return vec<4, T>(m_s[i], m_s[j], m_s[k], m_s[l]);
        }

        inline bool is_dir() const
        {
            return m_s[N - 1] == 0;
        }
        inline bool is_vector() const
        {
            return is_dir();
        }
        inline bool is_point() const
        {
            return m_s[N - 1] == 1;
        }

        inline vec project() const
        {
            vec result(*this);
            if (result[N - 1])
                result /= result[N - 1];
            return result;
        }

        inline vec broadcast(unsigned i) const
        {
            return vec((*this)[i]);
        }

        template <uint32_t i>
        inline vec broadcast() const
        {
            return vec((*this)[i]);
        }

        inline vec swizzle(uint32_t i, uint32_t j) const
        {
            return vec((*this)[i], (*this)[j]);
        }

        inline vec swizzle(uint32_t i, uint32_t j, uint32_t k) const
        {
            return vec((*this)[i], (*this)[j], (*this)[k]);
        }

        inline vec swizzle(uint32_t i, uint32_t j, uint32_t k, uint32_t l) const
        {
            return vec((*this)[i], (*this)[j], (*this)[k], (*this)[l]);
        }

        inline vec operator-() const
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = -m_s[i];
            return result;
        }

        inline vec operator+() const
        {
            return *this;
        }

        inline vec &operator+=(const vec &other)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] += other.m_s[i];
            return *this;
        }

        inline vec &operator-=(const vec &other)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] -= other.m_s[i];
            return *this;
        }

        inline vec &operator*=(const vec &other)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] *= other.m_s[i];
            return *this;
        }

        inline vec &operator/=(const vec &other)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] /= other.m_s[i];
            return *this;
        }

        inline vec &operator*=(T s)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] *= s;
            return *this;
        }

        inline vec &operator/=(T s)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] /= s;
            return *this;
        }

        // component-wise multiply (not a dot product like in previous versions)
        friend inline vec operator*(const vec &lhs, const vec &rhs)
        {
            return vec::mul_components(lhs, rhs);
        }

        friend inline vec operator*(const vec &lhs, T val)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = lhs.m_s[i] * val;
            return result;
        }

        friend inline vec operator*(T val, const vec &rhs)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = val * rhs.m_s[i];
            return result;
        }

        friend inline vec operator/(const vec &lhs, const vec &rhs)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = lhs.m_s[i] / rhs.m_s[i];
            return result;
        }

        friend inline vec operator/(const vec &lhs, T val)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = lhs.m_s[i] / val;
            return result;
        }

        friend inline vec operator+(const vec &lhs, const vec &rhs)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = lhs.m_s[i] + rhs.m_s[i];
            return result;
        }

        friend inline vec operator-(const vec &lhs, const vec &rhs)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result.m_s[i] = lhs.m_s[i] - rhs.m_s[i];
            return result;
        }

        static inline vec<3, T> cross2(const vec &a, const vec &b)
        {
            VOGL_ASSUME(N >= 2);
            return vec<3, T>(0, 0, a[0] * b[1] - a[1] * b[0]);
        }

        inline vec<3, T> cross2(const vec &b) const
        {
            return cross2(*this, b);
        }

        static inline vec<3, T> cross3(const vec &a, const vec &b)
        {
            VOGL_ASSUME(N >= 3);
            return vec<3, T>(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
        }

        inline vec<3, T> cross3(const vec &b) const
        {
            return cross3(*this, b);
        }

        static inline vec<3, T> cross(const vec &a, const vec &b)
        {
            VOGL_ASSUME(N >= 2);

            if (N == 2)
                return cross2(a, b);
            else
                return cross3(a, b);
        }

        inline vec<3, T> cross(const vec &b) const
        {
            VOGL_ASSUME(N >= 2);
            return cross(*this, b);
        }

        inline T dot(const vec &rhs) const
        {
            return dot(*this, rhs);
        }

        inline vec dot_vector(const vec &rhs) const
        {
            return vec(dot(*this, rhs));
        }

        static inline T dot(const vec &lhs, const vec &rhs)
        {
            T result = lhs.m_s[0] * rhs.m_s[0];
            for (uint32_t i = 1; i < N; i++)
                result += lhs.m_s[i] * rhs.m_s[i];
            return result;
        }

        inline T dot2(const vec &rhs) const
        {
            VOGL_ASSUME(N >= 2);
            return m_s[0] * rhs.m_s[0] + m_s[1] * rhs.m_s[1];
        }

        inline T dot3(const vec &rhs) const
        {
            VOGL_ASSUME(N >= 3);
            return m_s[0] * rhs.m_s[0] + m_s[1] * rhs.m_s[1] + m_s[2] * rhs.m_s[2];
        }

        inline T dot4(const vec &rhs) const
        {
            VOGL_ASSUME(N >= 4);
            return m_s[0] * rhs.m_s[0] + m_s[1] * rhs.m_s[1] + m_s[2] * rhs.m_s[2] + m_s[3] * rhs.m_s[3];
        }

        inline T norm(void) const
        {
            T sum = m_s[0] * m_s[0];
            for (uint32_t i = 1; i < N; i++)
                sum += m_s[i] * m_s[i];
            return sum;
        }

        inline T length(void) const
        {
            return sqrt(norm());
        }

        inline T squared_distance(const vec &rhs) const
        {
            T dist2 = 0;
            for (uint32_t i = 0; i < N; i++)
            {
                T d = m_s[i] - rhs.m_s[i];
                dist2 += d * d;
            }
            return dist2;
        }

        inline T squared_distance(const vec &rhs, T early_out) const
        {
            T dist2 = 0;
            for (uint32_t i = 0; i < N; i++)
            {
                T d = m_s[i] - rhs.m_s[i];
                dist2 += d * d;
                if (dist2 > early_out)
                    break;
            }
            return dist2;
        }

        inline T distance(const vec &rhs) const
        {
            T dist2 = 0;
            for (uint32_t i = 0; i < N; i++)
            {
                T d = m_s[i] - rhs.m_s[i];
                dist2 += d * d;
            }
            return sqrt(dist2);
        }

        inline vec inverse() const
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = m_s[i] ? (1.0f / m_s[i]) : 0;
            return result;
        }

        // returns squared length (norm)
        inline double normalize(const vec *pDefaultVec = NULL)
        {
            double n = m_s[0] * m_s[0];
            for (uint32_t i = 1; i < N; i++)
                n += m_s[i] * m_s[i];

            if (n != 0)
                *this *= static_cast<T>(1.0f / sqrt(n));
            else if (pDefaultVec)
                *this = *pDefaultVec;
            return n;
        }

        inline double normalize3(const vec *pDefaultVec = NULL)
        {
            VOGL_ASSUME(N >= 3);

            double n = m_s[0] * m_s[0] + m_s[1] * m_s[1] + m_s[2] * m_s[2];

            if (n != 0)
                *this *= static_cast<T>((1.0f / sqrt(n)));
            else if (pDefaultVec)
                *this = *pDefaultVec;
            return n;
        }

        inline vec &normalize_in_place(const vec *pDefaultVec = NULL)
        {
            normalize(pDefaultVec);
            return *this;
        }

        inline vec &normalize3_in_place(const vec *pDefaultVec = NULL)
        {
            normalize3(pDefaultVec);
            return *this;
        }

        inline vec get_normalized(const vec *pDefaultVec = NULL) const
        {
            vec result(*this);
            result.normalize(pDefaultVec);
            return result;
        }

        inline vec get_normalized3(const vec *pDefaultVec = NULL) const
        {
            vec result(*this);
            result.normalize3(pDefaultVec);
            return result;
        }

        inline vec &clamp(T l, T h)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] = static_cast<T>(math::clamp(m_s[i], l, h));
            return *this;
        }

        inline vec &clamp(const vec &l, const vec &h)
        {
            for (uint32_t i = 0; i < N; i++)
                m_s[i] = static_cast<T>(math::clamp(m_s[i], l[i], h[i]));
            return *this;
        }

        inline bool is_within_bounds(const vec &l, const vec &h) const
        {
            for (uint32_t i = 0; i < N; i++)
                if ((m_s[i] < l[i]) || (m_s[i] > h[i]))
                    return false;

            return true;
        }

        inline bool is_within_bounds(T l, T h) const
        {
            for (uint32_t i = 0; i < N; i++)
                if ((m_s[i] < l) || (m_s[i] > h))
                    return false;

            return true;
        }

        inline uint32_t get_major_axis(void) const
        {
            T m = fabs(m_s[0]);
            uint32_t r = 0;
            for (uint32_t i = 1; i < N; i++)
            {
                const T c = fabs(m_s[i]);
                if (c > m)
                {
                    m = c;
                    r = i;
                }
            }
            return r;
        }

        inline uint32_t get_minor_axis(void) const
        {
            T m = fabs(m_s[0]);
            uint32_t r = 0;
            for (uint32_t i = 1; i < N; i++)
            {
                const T c = fabs(m_s[i]);
                if (c < m)
                {
                    m = c;
                    r = i;
                }
            }
            return r;
        }

        inline void get_projection_axes(uint32_t &u, uint32_t &v) const
        {
            const int axis = get_major_axis();
            if (m_s[axis] < 0.0f)
            {
                v = math::next_wrap<uint32_t>(axis, N);
                u = math::next_wrap<uint32_t>(v, N);
            }
            else
            {
                u = math::next_wrap<uint32_t>(axis, N);
                v = math::next_wrap<uint32_t>(u, N);
            }
        }

        inline T get_absolute_minimum(void) const
        {
            T result = fabs(m_s[0]);
            for (uint32_t i = 1; i < N; i++)
                result = math::minimum(result, fabs(m_s[i]));
            return result;
        }

        inline T get_absolute_maximum(void) const
        {
            T result = fabs(m_s[0]);
            for (uint32_t i = 1; i < N; i++)
                result = math::maximum(result, fabs(m_s[i]));
            return result;
        }

        inline T get_minimum(void) const
        {
            T result = m_s[0];
            for (uint32_t i = 1; i < N; i++)
                result = math::minimum(result, m_s[i]);
            return result;
        }

        inline T get_maximum(void) const
        {
            T result = m_s[0];
            for (uint32_t i = 1; i < N; i++)
                result = math::maximum(result, m_s[i]);
            return result;
        }

        inline vec &remove_unit_direction(const vec &dir)
        {
            *this -= (dot(dir) * dir);
            return *this;
        }

        inline vec get_remove_unit_direction(const vec &dir) const
        {
            return *this - (dot(dir) * dir);
        }

        inline bool all_less(const vec &b) const
        {
            for (uint32_t i = 0; i < N; i++)
                if (m_s[i] >= b.m_s[i])
                    return false;
            return true;
        }

        inline bool all_less_equal(const vec &b) const
        {
            for (uint32_t i = 0; i < N; i++)
                if (m_s[i] > b.m_s[i])
                    return false;
            return true;
        }

        inline bool all_greater(const vec &b) const
        {
            for (uint32_t i = 0; i < N; i++)
                if (m_s[i] <= b.m_s[i])
                    return false;
            return true;
        }

        inline bool all_greater_equal(const vec &b) const
        {
            for (uint32_t i = 0; i < N; i++)
                if (m_s[i] < b.m_s[i])
                    return false;
            return true;
        }

        inline vec negate_xyz() const
        {
            vec ret;

            ret[0] = -m_s[0];
            if (N >= 2)
                ret[1] = -m_s[1];
            if (N >= 3)
                ret[2] = -m_s[2];

            for (uint32_t i = 3; i < N; i++)
                ret[i] = m_s[i];

            return ret;
        }

        inline vec &invert()
        {
            for (uint32_t i = 0; i < N; i++)
                if (m_s[i] != 0.0f)
                    m_s[i] = 1.0f / m_s[i];
            return *this;
        }

        inline scalar_type perp_dot(const vec &b) const
        {
            VOGL_ASSUME(N == 2);
            return m_s[0] * b.m_s[1] - m_s[1] * b.m_s[0];
        }

        inline vec perp() const
        {
            VOGL_ASSUME(N == 2);
            return vec(-m_s[1], m_s[0]);
        }

        inline vec get_floor() const
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = floor(m_s[i]);
            return result;
        }

        inline vec get_ceil() const
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = ceil(m_s[i]);
            return result;
        }

        // static helper methods

        static inline vec mul_components(const vec &lhs, const vec &rhs)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = lhs.m_s[i] * rhs.m_s[i];
            return result;
        }

        static inline vec mul_add_components(const vec &a, const vec &b, const vec &c)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = a.m_s[i] * b.m_s[i] + c.m_s[i];
            return result;
        }

        static inline vec make_axis(uint32_t i)
        {
            vec result;
            result.clear();
            result[i] = 1;
            return result;
        }

        static inline vec component_max(const vec &a, const vec &b)
        {
            vec ret;
            for (uint32_t i = 0; i < N; i++)
                ret.m_s[i] = math::maximum(a.m_s[i], b.m_s[i]);
            return ret;
        }

        static inline vec component_min(const vec &a, const vec &b)
        {
            vec ret;
            for (uint32_t i = 0; i < N; i++)
                ret.m_s[i] = math::minimum(a.m_s[i], b.m_s[i]);
            return ret;
        }

        static inline vec lerp(const vec &a, const vec &b, float t)
        {
            vec ret;
            for (uint32_t i = 0; i < N; i++)
                ret.m_s[i] = a.m_s[i] + (b.m_s[i] - a.m_s[i]) * t;
            return ret;
        }

        static inline bool equal_tol(const vec &a, const vec &b, float t)
        {
            for (uint32_t i = 0; i < N; i++)
                if (!math::equal_tol(a.m_s[i], b.m_s[i], t))
                    return false;
            return true;
        }

        inline bool equal_tol(const vec &b, float t) const
        {
            return equal_tol(*this, b, t);
        }

        static inline vec make_random(random &r, float l, float h)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = r.frand(l, h);
            return result;
        }

        static inline vec make_random(fast_random &r, float l, float h)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = r.frand(l, h);
            return result;
        }

        static inline vec make_random(random &r, const vec &l, const vec &h)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = r.frand(l[i], h[i]);
            return result;
        }

        static inline vec make_random(fast_random &r, const vec &l, const vec &h)
        {
            vec result;
            for (uint32_t i = 0; i < N; i++)
                result[i] = r.frand(l[i], h[i]);
            return result;
        }

    protected:
        T m_s[N];
    };

    typedef vec<1, double> vec1D;
    typedef vec<2, double> vec2D;
    typedef vec<3, double> vec3D;
    typedef vec<4, double> vec4D;

    typedef vec<1, float> vec1F;

    typedef vec<2, float> vec2F;
    typedef vogl::vector<vec2F> vec2F_array;

    typedef vec<3, float> vec3F;
    typedef vogl::vector<vec3F> vec3F_array;

    typedef vec<4, float> vec4F;
    typedef vogl::vector<vec4F> vec4F_array;

    typedef vec<2, uint32_t> vec2U;
    typedef vec<2, int> vec2I;
    typedef vec<3, int> vec3I;
    typedef vec<4, int> vec4I;

    typedef vec<2, int16_t> vec2I16;
    typedef vec<3, int16_t> vec3I16;

    template <uint32_t N, typename T>
    struct scalar_type<vec<N, T> >
    {
        enum
        {
            cFlag = true
        };
        static inline void construct(vec<N, T> *p)
        {
            VOGL_NOTE_UNUSED(p);
        }
        static inline void construct(vec<N, T> *p, const vec<N, T> &init)
        {
            memcpy(p, &init, sizeof(vec<N, T>));
        }
        static inline void construct_array(vec<N, T> *p, uint32_t n)
        {
            VOGL_NOTE_UNUSED(p), VOGL_NOTE_UNUSED(n);
        }
        static inline void destruct(vec<N, T> *p)
        {
            VOGL_NOTE_UNUSED(p);
        }
        static inline void destruct_array(vec<N, T> *p, uint32_t n)
        {
            VOGL_NOTE_UNUSED(p), VOGL_NOTE_UNUSED(n);
        }
    };

} // namespace vogl
