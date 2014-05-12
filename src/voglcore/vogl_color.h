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

// File: vogl_color.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename component_type>
    struct color_quad_component_traits
    {
        enum
        {
            cSigned = false,
            cFloat = false,
            cMin = cUINT8_MIN,
            cMax = cUINT8_MAX
        };
    };

    template <>
    struct color_quad_component_traits<int8_t>
    {
        enum
        {
            cSigned = true,
            cFloat = false,
            cMin = cINT8_MIN,
            cMax = cINT8_MAX
        };
    };

    template <>
    struct color_quad_component_traits<int16_t>
    {
        enum
        {
            cSigned = true,
            cFloat = false,
            cMin = cINT16_MIN,
            cMax = cINT16_MAX
        };
    };

    template <>
    struct color_quad_component_traits<uint16_t>
    {
        enum
        {
            cSigned = false,
            cFloat = false,
            cMin = cUINT16_MIN,
            cMax = cUINT16_MAX
        };
    };

    template <>
    struct color_quad_component_traits<int32_t>
    {
        enum
        {
            cSigned = true,
            cFloat = false,
            cMin = cINT32_MIN,
            cMax = cINT32_MAX
        };
    };

    template <>
    struct color_quad_component_traits<uint32_t>
    {
        enum
        {
            cSigned = false,
            cFloat = false,
            cMin = cUINT32_MIN,
            cMax = cUINT32_MAX
        };
    };

    template <>
    struct color_quad_component_traits<float>
    {
        enum
        {
            cSigned = false,
            cFloat = true,
            cMin = cINT32_MIN,
            cMax = cINT32_MAX
        };
    };

    template <>
    struct color_quad_component_traits<double>
    {
        enum
        {
            cSigned = false,
            cFloat = true,
            cMin = cINT32_MIN,
            cMax = cINT32_MAX
        };
    };

    template <typename component_type, typename parameter_type>
    class color_quad : public helpers::rel_ops<color_quad<component_type, parameter_type> >
    {
        template <typename T>
        static inline parameter_type clamp(T v)
        {
            parameter_type result = static_cast<parameter_type>(v);
            if (!component_traits::cFloat)
            {
                if (v < component_traits::cMin)
                    result = static_cast<parameter_type>(component_traits::cMin);
                else if (v > component_traits::cMax)
                    result = static_cast<parameter_type>(component_traits::cMax);
            }
            return result;
        }

#ifdef COMPILER_MSVC
        template <>
        static inline parameter_type clamp(int v)
        {
            if (!component_traits::cFloat)
            {
                if ((!component_traits::cSigned) && (component_traits::cMin == 0) && (component_traits::cMax == 0xFF))
                {
                    if (v & 0xFFFFFF00U)
                        v = (~(static_cast<int>(v) >> 31)) & 0xFF;
                }
                else
                {
                    if (v < component_traits::cMin)
                        v = component_traits::cMin;
                    else if (v > component_traits::cMax)
                        v = component_traits::cMax;
                }
            }
            return static_cast<parameter_type>(v);
        }
#endif

    public:
        typedef component_type component_t;
        typedef parameter_type parameter_t;
        typedef color_quad_component_traits<component_type> component_traits;

        enum
        {
            cNumComps = 4
        };

        union
        {
            struct
            {
                component_type r;
                component_type g;
                component_type b;
                component_type a;
            };

            component_type c[cNumComps];

            uint32_t m_u32;
        };

        inline color_quad()
        {
        }

        inline color_quad(eClear)
            : r(0), g(0), b(0), a(0)
        {
        }

        inline color_quad(const color_quad &other)
            : r(other.r), g(other.g), b(other.b), a(other.a)
        {
        }

        explicit inline color_quad(parameter_type y, parameter_type alpha = component_traits::cMax)
        {
            set(y, alpha);
        }

        inline color_quad(parameter_type red, parameter_type green, parameter_type blue, parameter_type alpha = component_traits::cMax)
        {
            set(red, green, blue, alpha);
        }

        explicit inline color_quad(eNoClamp, parameter_type y, parameter_type alpha = component_traits::cMax)
        {
            set_noclamp_y_alpha(y, alpha);
        }

        inline color_quad(eNoClamp, parameter_type red, parameter_type green, parameter_type blue, parameter_type alpha = component_traits::cMax)
        {
            set_noclamp_rgba(red, green, blue, alpha);
        }

        template <typename other_component_type, typename other_parameter_type>
        inline color_quad(const color_quad<other_component_type, other_parameter_type> &other)
            : r(static_cast<component_type>(clamp(other.r))), g(static_cast<component_type>(clamp(other.g))), b(static_cast<component_type>(clamp(other.b))), a(static_cast<component_type>(clamp(other.a)))
        {
        }

        inline void clear()
        {
            r = 0;
            g = 0;
            b = 0;
            a = 0;
        }

        inline color_quad &operator=(const color_quad &other)
        {
            r = other.r;
            g = other.g;
            b = other.b;
            a = other.a;
            return *this;
        }

        inline color_quad &set_rgb(const color_quad &other)
        {
            r = other.r;
            g = other.g;
            b = other.b;
            return *this;
        }

        template <typename other_component_type, typename other_parameter_type>
        inline color_quad &operator=(const color_quad<other_component_type, other_parameter_type> &other)
        {
            r = static_cast<component_type>(clamp(other.r));
            g = static_cast<component_type>(clamp(other.g));
            b = static_cast<component_type>(clamp(other.b));
            a = static_cast<component_type>(clamp(other.a));
            return *this;
        }

        inline color_quad &operator=(parameter_type y)
        {
            set(y, component_traits::cMax);
            return *this;
        }

        inline color_quad &set(parameter_type y, parameter_type alpha = component_traits::cMax)
        {
            y = clamp(y);
            alpha = clamp(alpha);
            r = static_cast<component_type>(y);
            g = static_cast<component_type>(y);
            b = static_cast<component_type>(y);
            a = static_cast<component_type>(alpha);
            return *this;
        }

        inline color_quad &set_alpha(parameter_type alpha)
        {
            a = static_cast<component_type>(clamp(alpha));
            return *this;
        }

        inline color_quad &set_noclamp_y_alpha(parameter_type y, parameter_type alpha = component_traits::cMax)
        {
            VOGL_ASSERT((y >= component_traits::cMin) && (y <= component_traits::cMax));
            VOGL_ASSERT((alpha >= component_traits::cMin) && (alpha <= component_traits::cMax));

            r = static_cast<component_type>(y);
            g = static_cast<component_type>(y);
            b = static_cast<component_type>(y);
            a = static_cast<component_type>(alpha);
            return *this;
        }

        inline color_quad &set(parameter_type red, parameter_type green, parameter_type blue, parameter_type alpha = component_traits::cMax)
        {
            r = static_cast<component_type>(clamp(red));
            g = static_cast<component_type>(clamp(green));
            b = static_cast<component_type>(clamp(blue));
            a = static_cast<component_type>(clamp(alpha));
            return *this;
        }

        inline color_quad &set_noclamp_rgba(parameter_type red, parameter_type green, parameter_type blue, parameter_type alpha)
        {
            VOGL_ASSERT((red >= component_traits::cMin) && (red <= component_traits::cMax));
            VOGL_ASSERT((green >= component_traits::cMin) && (green <= component_traits::cMax));
            VOGL_ASSERT((blue >= component_traits::cMin) && (blue <= component_traits::cMax));
            VOGL_ASSERT((alpha >= component_traits::cMin) && (alpha <= component_traits::cMax));

            r = static_cast<component_type>(red);
            g = static_cast<component_type>(green);
            b = static_cast<component_type>(blue);
            a = static_cast<component_type>(alpha);
            return *this;
        }

        inline color_quad &set_noclamp_rgb(parameter_type red, parameter_type green, parameter_type blue)
        {
            VOGL_ASSERT((red >= component_traits::cMin) && (red <= component_traits::cMax));
            VOGL_ASSERT((green >= component_traits::cMin) && (green <= component_traits::cMax));
            VOGL_ASSERT((blue >= component_traits::cMin) && (blue <= component_traits::cMax));

            r = static_cast<component_type>(red);
            g = static_cast<component_type>(green);
            b = static_cast<component_type>(blue);
            return *this;
        }

        static inline parameter_type get_min_comp()
        {
            return component_traits::cMin;
        }
        static inline parameter_type get_max_comp()
        {
            return component_traits::cMax;
        }
        static inline bool get_comps_are_signed()
        {
            return component_traits::cSigned;
        }

        inline component_type operator[](uint32_t i) const
        {
            VOGL_ASSERT(i < cNumComps);
            return c[i];
        }
        inline component_type &operator[](uint32_t i)
        {
            VOGL_ASSERT(i < cNumComps);
            return c[i];
        }

        inline color_quad &set_component(uint32_t i, parameter_type f)
        {
            VOGL_ASSERT(i < cNumComps);

            c[i] = static_cast<component_type>(clamp(f));

            return *this;
        }

        inline color_quad &set_grayscale(parameter_t l)
        {
            component_t x = static_cast<component_t>(clamp(l));
            c[0] = x;
            c[1] = x;
            c[2] = x;
            return *this;
        }

        inline color_quad &clamp(const color_quad &l, const color_quad &h)
        {
            for (uint32_t i = 0; i < cNumComps; i++)
                c[i] = static_cast<component_type>(math::clamp<parameter_type>(c[i], l[i], h[i]));
            return *this;
        }

        inline color_quad &clamp(parameter_type l, parameter_type h)
        {
            for (uint32_t i = 0; i < cNumComps; i++)
                c[i] = static_cast<component_type>(math::clamp<parameter_type>(c[i], l, h));
            return *this;
        }

        // Returns CCIR 601 luma (consistent with color_utils::RGB_To_Y).
        inline parameter_type get_luma() const
        {
            return static_cast<parameter_type>((19595U * r + 38470U * g + 7471U * b + 32768U) >> 16U);
        }

        // Returns REC 709 luma.
        inline parameter_type get_luma_rec709() const
        {
            return static_cast<parameter_type>((13938U * r + 46869U * g + 4729U * b + 32768U) >> 16U);
        }

        // Beware of endianness!
        inline uint32_t get_uint32() const
        {
            VOGL_ASSERT(sizeof(*this) == sizeof(uint32_t));
            return *reinterpret_cast<const uint32_t *>(this);
        }

        // Beware of endianness!
        inline uint64_t get_uint64() const
        {
            VOGL_ASSERT(sizeof(*this) == sizeof(uint64_t));
            return *reinterpret_cast<const uint64_t *>(this);
        }

        inline uint32_t squared_distance(const color_quad &c, bool alpha = true) const
        {
            return math::square(r - c.r) + math::square(g - c.g) + math::square(b - c.b) + (alpha ? math::square(a - c.a) : 0);
        }

        inline bool rgb_equals(const color_quad &rhs) const
        {
            return (r == rhs.r) && (g == rhs.g) && (b == rhs.b);
        }

        inline bool operator==(const color_quad &rhs) const
        {
            if (sizeof(color_quad) == sizeof(uint32_t))
                return m_u32 == rhs.m_u32;
            else
                return (r == rhs.r) && (g == rhs.g) && (b == rhs.b) && (a == rhs.a);
        }

        inline bool operator<(const color_quad &rhs) const
        {
            for (uint32_t i = 0; i < cNumComps; i++)
            {
                if (c[i] < rhs.c[i])
                    return true;
                else if (!(c[i] == rhs.c[i]))
                    return false;
            }
            return false;
        }

        color_quad &operator+=(const color_quad &other)
        {
            for (uint32_t i = 0; i < 4; i++)
                c[i] = static_cast<component_type>(clamp(c[i] + other.c[i]));
            return *this;
        }

        color_quad &operator-=(const color_quad &other)
        {
            for (uint32_t i = 0; i < 4; i++)
                c[i] = static_cast<component_type>(clamp(c[i] - other.c[i]));
            return *this;
        }

        color_quad &operator*=(parameter_type v)
        {
            for (uint32_t i = 0; i < 4; i++)
                c[i] = static_cast<component_type>(clamp(c[i] * v));
            return *this;
        }

        color_quad &operator/=(parameter_type v)
        {
            for (uint32_t i = 0; i < 4; i++)
                c[i] = static_cast<component_type>(c[i] / v);
            return *this;
        }

        color_quad get_swizzled(uint32_t x, uint32_t y, uint32_t z, uint32_t w) const
        {
            VOGL_ASSERT((x | y | z | w) < 4);
            return color_quad(c[x], c[y], c[z], c[w]);
        }

        friend color_quad operator+(const color_quad &lhs, const color_quad &rhs)
        {
            color_quad result(lhs);
            result += rhs;
            return result;
        }

        friend color_quad operator-(const color_quad &lhs, const color_quad &rhs)
        {
            color_quad result(lhs);
            result -= rhs;
            return result;
        }

        friend color_quad operator*(const color_quad &lhs, parameter_type v)
        {
            color_quad result(lhs);
            result *= v;
            return result;
        }

        friend color_quad operator/(const color_quad &lhs, parameter_type v)
        {
            color_quad result(lhs);
            result /= v;
            return result;
        }

        friend color_quad operator*(parameter_type v, const color_quad &rhs)
        {
            color_quad result(rhs);
            result *= v;
            return result;
        }

        inline bool is_grayscale() const
        {
            return (c[0] == c[1]) && (c[1] == c[2]);
        }

        uint32_t get_min_component_index(bool alpha = true) const
        {
            uint32_t index = 0;
            uint32_t limit = alpha ? cNumComps : (cNumComps - 1);
            for (uint32_t i = 1; i < limit; i++)
                if (c[i] < c[index])
                    index = i;
            return index;
        }

        uint32_t get_max_component_index(bool alpha = true) const
        {
            uint32_t index = 0;
            uint32_t limit = alpha ? cNumComps : (cNumComps - 1);
            for (uint32_t i = 1; i < limit; i++)
                if (c[i] > c[index])
                    index = i;
            return index;
        }

        operator size_t() const
        {
            return (size_t)fast_hash(this, sizeof(*this));
        }

        void get_float4(float *pDst)
        {
            for (uint32_t i = 0; i < 4; i++)
                pDst[i] = ((*this)[i] - component_traits::cMin) / float(component_traits::cMax - component_traits::cMin);
        }

        void get_float3(float *pDst)
        {
            for (uint32_t i = 0; i < 3; i++)
                pDst[i] = ((*this)[i] - component_traits::cMin) / float(component_traits::cMax - component_traits::cMin);
        }

        void scale_rgb_by_float(float flScale)
        {
            for (uint32_t i = 0; i < 3; i++)
                set_component(i, static_cast<parameter_t>(c[i] * flScale + (component_traits::cFloat ? 0.0f : .5f)));
        }

        void scale_rgb_by_component(parameter_t v)
        {
            float flScale;
            if (component_traits::cFloat)
                flScale = static_cast<float>(v);
            else
                flScale = math::maximum<float>(0.0f, v / static_cast<float>(component_traits::cMax));
            scale_rgb_by_float(flScale);
        }

        static color_quad component_min(const color_quad &a, const color_quad &b)
        {
            color_quad result;
            for (uint32_t i = 0; i < 4; i++)
                result[i] = static_cast<component_type>(math::minimum(a[i], b[i]));
            return result;
        }

        static color_quad component_max(const color_quad &a, const color_quad &b)
        {
            color_quad result;
            for (uint32_t i = 0; i < 4; i++)
                result[i] = static_cast<component_type>(math::maximum(a[i], b[i]));
            return result;
        }

        static color_quad make_black()
        {
            return color_quad(0, 0, 0, static_cast<color_quad::component_t>(component_traits::cMax));
        }

        static color_quad make_white()
        {
            return color_quad(static_cast<color_quad::component_t>(component_traits::cMax), static_cast<color_quad::component_t>(component_traits::cMax), static_cast<color_quad::component_t>(component_traits::cMax), static_cast<color_quad::component_t>(component_traits::cMax));
        }
    }; // class color_quad

    template <typename c, typename q>
    struct scalar_type<color_quad<c, q> >
    {
        enum
        {
            cFlag = true
        };
        static inline void construct(color_quad<c, q> *p)
        {
            VOGL_NOTE_UNUSED(p);
        }
        static inline void construct(color_quad<c, q> *p, const color_quad<c, q> &init)
        {
            memcpy(p, &init, sizeof(color_quad<c, q>));
        }
        static inline void construct_array(color_quad<c, q> *p, uint32_t n)
        {
            VOGL_NOTE_UNUSED(p), VOGL_NOTE_UNUSED(n);
        }
        static inline void destruct(color_quad<c, q> *p)
        {
            VOGL_NOTE_UNUSED(p);
        }
        static inline void destruct_array(color_quad<c, q> *p, uint32_t n)
        {
            VOGL_NOTE_UNUSED(p), VOGL_NOTE_UNUSED(n);
        }
    };

    typedef color_quad<uint8_t, int> color_quad_u8;
    typedef color_quad<int8_t, int> color_quad_i8;
    typedef color_quad<int16_t, int> color_quad_i16;
    typedef color_quad<uint16_t, int> color_quad_u16;
    typedef color_quad<int32_t, int> color_quad_i32;
    typedef color_quad<uint32_t, uint32_t> color_quad_u32;
    typedef color_quad<float, float> color_quad_f;
    typedef color_quad<double, double> color_quad_d;

    namespace color
    {
        inline uint32_t elucidian_distance(uint32_t r0, uint32_t g0, uint32_t b0, uint32_t r1, uint32_t g1, uint32_t b1)
        {
            int dr = (int)r0 - (int)r1;
            int dg = (int)g0 - (int)g1;
            int db = (int)b0 - (int)b1;

            return static_cast<uint32_t>(dr * dr + dg * dg + db * db);
        }

        inline uint32_t elucidian_distance(uint32_t r0, uint32_t g0, uint32_t b0, uint32_t a0, uint32_t r1, uint32_t g1, uint32_t b1, uint32_t a1)
        {
            int dr = (int)r0 - (int)r1;
            int dg = (int)g0 - (int)g1;
            int db = (int)b0 - (int)b1;
            int da = (int)a0 - (int)a1;

            return static_cast<uint32_t>(dr * dr + dg * dg + db * db + da * da);
        }

        inline uint32_t elucidian_distance(const color_quad_u8 &c0, const color_quad_u8 &c1, bool alpha)
        {
            if (alpha)
                return elucidian_distance(c0.r, c0.g, c0.b, c0.a, c1.r, c1.g, c1.b, c1.a);
            else
                return elucidian_distance(c0.r, c0.g, c0.b, c1.r, c1.g, c1.b);
        }

        inline uint32_t weighted_elucidian_distance(uint32_t r0, uint32_t g0, uint32_t b0, uint32_t r1, uint32_t g1, uint32_t b1, uint32_t wr, uint32_t wg, uint32_t wb)
        {
            int dr = (int)r0 - (int)r1;
            int dg = (int)g0 - (int)g1;
            int db = (int)b0 - (int)b1;

            return static_cast<uint32_t>((wr * dr * dr) + (wg * dg * dg) + (wb * db * db));
        }

        inline uint32_t weighted_elucidian_distance(
            uint32_t r0, uint32_t g0, uint32_t b0, uint32_t a0,
            uint32_t r1, uint32_t g1, uint32_t b1, uint32_t a1,
            uint32_t wr, uint32_t wg, uint32_t wb, uint32_t wa)
        {
            int dr = (int)r0 - (int)r1;
            int dg = (int)g0 - (int)g1;
            int db = (int)b0 - (int)b1;
            int da = (int)a0 - (int)a1;

            return static_cast<uint32_t>((wr * dr * dr) + (wg * dg * dg) + (wb * db * db) + (wa * da * da));
        }

        inline uint32_t weighted_elucidian_distance(const color_quad_u8 &c0, const color_quad_u8 &c1, uint32_t wr, uint32_t wg, uint32_t wb, uint32_t wa)
        {
            return weighted_elucidian_distance(c0.r, c0.g, c0.b, c0.a, c1.r, c1.g, c1.b, c1.a, wr, wg, wb, wa);
        }

        //const uint32_t cRWeight = 8;//24;
        //const uint32_t cGWeight = 24;//73;
        //const uint32_t cBWeight = 1;//3;

        const uint32_t cRWeight = 8;  //24;
        const uint32_t cGWeight = 25; //73;
        const uint32_t cBWeight = 1;  //3;

        inline uint32_t color_distance(bool perceptual, const color_quad_u8 &e1, const color_quad_u8 &e2, bool alpha)
        {
            if (perceptual)
            {
                if (alpha)
                    return weighted_elucidian_distance(e1, e2, cRWeight, cGWeight, cBWeight, cRWeight + cGWeight + cBWeight);
                else
                    return weighted_elucidian_distance(e1, e2, cRWeight, cGWeight, cBWeight, 0);
            }
            else
                return elucidian_distance(e1, e2, alpha);
        }

        inline uint32_t peak_color_error(const color_quad_u8 &e1, const color_quad_u8 &e2)
        {
            return math::maximum<uint32_t>(static_cast<uint32_t>(labs(e1[0] - e2[0])), static_cast<uint32_t>(labs(e1[1] - e2[1])), static_cast<uint32_t>(labs(e1[2] - e2[2])));
            //return math::square<int>(e1[0] - e2[0]) + math::square<int>(e1[1] - e2[1]) + math::square<int>(e1[2] - e2[2]);
        }

        // y - [0,255]
        // co - [-127,127]
        // cg - [-126,127]
        inline void RGB_to_YCoCg(int r, int g, int b, int &y, int &co, int &cg)
        {
            y = (r >> 2) + (g >> 1) + (b >> 2);
            co = (r >> 1) - (b >> 1);
            cg = -(r >> 2) + (g >> 1) - (b >> 2);
        }

        inline void YCoCg_to_RGB(int y, int co, int cg, int &r, int &g, int &b)
        {
            int tmp = y - cg;
            g = y + cg;
            r = tmp + co;
            b = tmp - co;
        }

        static inline uint8_t clamp_component(int i)
        {
            if (static_cast<uint32_t>(i) > 255U)
            {
                if (i < 0)
                    i = 0;
                else if (i > 255)
                    i = 255;
            }
            return static_cast<uint8_t>(i);
        }

        // RGB->YCbCr constants, scaled by 2^16
        const int YR = 19595, YG = 38470, YB = 7471, CB_R = -11059, CB_G = -21709, CB_B = 32768, CR_R = 32768, CR_G = -27439, CR_B = -5329;
        // YCbCr->RGB constants, scaled by 2^16
        const int R_CR = 91881, B_CB = 116130, G_CR = -46802, G_CB = -22554;

        inline int RGB_to_Y(const color_quad_u8 &rgb)
        {
            const int r = rgb[0], g = rgb[1], b = rgb[2];
            return (r * YR + g * YG + b * YB + 32768) >> 16;
        }

        // RGB to YCbCr (same as JFIF JPEG).
        // Odd default biases account for 565 endpoint packing.
        inline void RGB_to_YCC(color_quad_u8 &ycc, const color_quad_u8 &rgb, int cb_bias = 123, int cr_bias = 125)
        {
            const int r = rgb[0], g = rgb[1], b = rgb[2];
            ycc.a = static_cast<uint8_t>((r * YR + g * YG + b * YB + 32768) >> 16);
            ycc.r = clamp_component(cb_bias + ((r * CB_R + g * CB_G + b * CB_B + 32768) >> 16));
            ycc.g = clamp_component(cr_bias + ((r * CR_R + g * CR_G + b * CR_B + 32768) >> 16));
            ycc.b = 0;
        }

        // YCbCr to RGB.
        // Odd biases account for 565 endpoint packing.
        inline void YCC_to_RGB(color_quad_u8 &rgb, const color_quad_u8 &ycc, int cb_bias = 123, int cr_bias = 125)
        {
            const int y = ycc.a;
            const int cb = ycc.r - cb_bias;
            const int cr = ycc.g - cr_bias;
            rgb.r = clamp_component(y + ((R_CR * cr + 32768) >> 16));
            rgb.g = clamp_component(y + ((G_CR * cr + G_CB * cb + 32768) >> 16));
            rgb.b = clamp_component(y + ((B_CB * cb + 32768) >> 16));
            rgb.a = 255;
        }

        // Float RGB->YCbCr constants
        const float S = 1.0f / 65536.0f;
        const float F_YR = S * YR, F_YG = S * YG, F_YB = S * YB, F_CB_R = S * CB_R, F_CB_G = S * CB_G, F_CB_B = S * CB_B, F_CR_R = S * CR_R, F_CR_G = S * CR_G, F_CR_B = S * CR_B;
        // Float YCbCr->RGB constants
        const float F_R_CR = S * R_CR, F_B_CB = S * B_CB, F_G_CR = S * G_CR, F_G_CB = S * G_CB;

        inline void RGB_to_YCC_float(color_quad_f &ycc, const color_quad_u8 &rgb)
        {
            const int r = rgb[0], g = rgb[1], b = rgb[2];
            ycc.a = r * F_YR + g * F_YG + b * F_YB;
            ycc.r = r * F_CB_R + g * F_CB_G + b * F_CB_B;
            ycc.g = r * F_CR_R + g * F_CR_G + b * F_CR_B;
            ycc.b = 0;
        }

        inline void YCC_float_to_RGB(color_quad_u8 &rgb, const color_quad_f &ycc)
        {
            float y = ycc.a, cb = ycc.r, cr = ycc.g;
            rgb.r = color::clamp_component(static_cast<int>(.5f + y + F_R_CR * cr));
            rgb.g = color::clamp_component(static_cast<int>(.5f + y + F_G_CR * cr + F_G_CB * cb));
            rgb.b = color::clamp_component(static_cast<int>(.5f + y + F_B_CB * cb));
            rgb.a = 255;
        }

    } // namespace color

    // This class purposely trades off speed for flexibility. It can handle any component swizzle, any pixel type from 1-4 components and 1-32 bits/component,
    // any pixel size between 1-16 bytes/pixel, any pixel stride, any color_quad data type (signed/unsigned/float 8/16/32 bits/component), and scaled/non-scaled components.
    // On the downside, it's freaking slow.
    class pixel_packer
    {
    public:
        pixel_packer()
        {
            clear();
        }

        pixel_packer(uint32_t num_comps, uint32_t bits_per_comp, int pixel_stride = -1, bool reversed = false)
        {
            init(num_comps, bits_per_comp, pixel_stride, reversed);
        }

        pixel_packer(const char *pComp_map, int pixel_stride = -1, int force_comp_size = -1)
        {
            init(pComp_map, pixel_stride, force_comp_size);
        }

        void clear()
        {
            utils::zero_this(this);
        }

        inline bool is_valid() const
        {
            return m_pixel_stride > 0;
        }

        inline uint32_t get_pixel_stride() const
        {
            return m_pixel_stride;
        }
        void set_pixel_stride(uint32_t n)
        {
            m_pixel_stride = n;
        }

        uint32_t get_num_comps() const
        {
            return m_num_comps;
        }
        uint32_t get_comp_size(uint32_t index) const
        {
            VOGL_ASSERT(index < 4);
            return m_comp_size[index];
        }
        uint32_t get_comp_ofs(uint32_t index) const
        {
            VOGL_ASSERT(index < 4);
            return m_comp_ofs[index];
        }
        uint32_t get_comp_max(uint32_t index) const
        {
            VOGL_ASSERT(index < 4);
            return m_comp_max[index];
        }
        bool get_rgb_is_luma() const
        {
            return m_rgb_is_luma;
        }

        template <typename color_quad_type>
        const void *unpack(const void *p, color_quad_type &color, bool rescale = true) const
        {
            const uint8_t *pSrc = static_cast<const uint8_t *>(p);

            for (uint32_t i = 0; i < 4; i++)
            {
                const uint32_t comp_size = m_comp_size[i];
                if (!comp_size)
                {
                    if (color_quad_type::component_traits::cFloat)
                        color[i] = static_cast<typename color_quad_type::parameter_t>((i == 3) ? 1 : 0);
                    else
                        color[i] = static_cast<typename color_quad_type::parameter_t>((i == 3) ? color_quad_type::component_traits::cMax : 0);
                    continue;
                }

                uint32_t n = 0, dst_bit_ofs = 0;
                uint32_t src_bit_ofs = m_comp_ofs[i];
                while (dst_bit_ofs < comp_size)
                {
                    const uint32_t byte_bit_ofs = src_bit_ofs & 7;
                    n |= ((pSrc[src_bit_ofs >> 3] >> byte_bit_ofs) << dst_bit_ofs);

                    const uint32_t bits_read = 8 - byte_bit_ofs;
                    src_bit_ofs += bits_read;
                    dst_bit_ofs += bits_read;
                }

                const uint32_t mx = m_comp_max[i];
                n &= mx;

                const uint32_t h = static_cast<uint32_t>(color_quad_type::component_traits::cMax);

                if (color_quad_type::component_traits::cFloat)
                    color.set_component(i, static_cast<typename color_quad_type::parameter_t>(n));
                else if (rescale)
                    color.set_component(i, static_cast<typename color_quad_type::parameter_t>((static_cast<uint64_t>(n) * h + (mx >> 1U)) / mx));
                else if (color_quad_type::component_traits::cSigned)
                    color.set_component(i, static_cast<typename color_quad_type::parameter_t>(math::minimum<uint32_t>(n, h)));
                else
                    color.set_component(i, static_cast<typename color_quad_type::parameter_t>(n));
            }

            if (m_rgb_is_luma)
            {
                color[0] = color[1];
                color[2] = color[1];
            }

            return pSrc + m_pixel_stride;
        }

        template <typename color_quad_type>
        void *pack(const color_quad_type &color, void *p, bool rescale = true) const
        {
            uint8_t *pDst = static_cast<uint8_t *>(p);

            for (uint32_t i = 0; i < 4; i++)
            {
                const uint32_t comp_size = m_comp_size[i];
                if (!comp_size)
                    continue;

                uint32_t mx = m_comp_max[i];

                uint32_t n;
                if (color_quad_type::component_traits::cFloat)
                {
                    typename color_quad_type::parameter_t t = color[i];
                    if (t < 0.0f)
                        n = 0;
                    else if (t > static_cast<typename color_quad_type::parameter_t>(mx))
                        n = mx;
                    else
                        n = math::minimum<uint32_t>(static_cast<uint32_t>(floor(t + .5f)), mx);
                }
                else if (rescale)
                {
                    if (color_quad_type::component_traits::cSigned)
                        n = math::maximum<int>(static_cast<int>(color[i]), 0);
                    else
                        n = static_cast<uint32_t>(color[i]);

                    const uint32_t h = static_cast<uint32_t>(color_quad_type::component_traits::cMax);
                    n = static_cast<uint32_t>((static_cast<uint64_t>(n) * mx + (h >> 1)) / h);
                }
                else
                {
                    if (color_quad_type::component_traits::cSigned)
                        n = math::minimum<uint32_t>(static_cast<uint32_t>(math::maximum<int>(static_cast<int>(color[i]), 0)), mx);
                    else
                        n = math::minimum<uint32_t>(static_cast<uint32_t>(color[i]), mx);
                }

                uint32_t src_bit_ofs = 0;
                uint32_t dst_bit_ofs = m_comp_ofs[i];
                while (src_bit_ofs < comp_size)
                {
                    const uint32_t cur_byte_bit_ofs = (dst_bit_ofs & 7);
                    const uint32_t cur_byte_bits = 8 - cur_byte_bit_ofs;

                    uint32_t byte_val = pDst[dst_bit_ofs >> 3];
                    uint32_t bit_mask = (mx << cur_byte_bit_ofs) & 0xFF;
                    byte_val &= ~bit_mask;
                    byte_val |= (n << cur_byte_bit_ofs);
                    pDst[dst_bit_ofs >> 3] = static_cast<uint8_t>(byte_val);

                    mx >>= cur_byte_bits;
                    n >>= cur_byte_bits;

                    dst_bit_ofs += cur_byte_bits;
                    src_bit_ofs += cur_byte_bits;
                }
            }

            return pDst + m_pixel_stride;
        }

        bool init(uint32_t num_comps, uint32_t bits_per_comp, int pixel_stride = -1, bool reversed = false)
        {
            clear();

            if ((num_comps < 1) || (num_comps > 4) || (bits_per_comp < 1) || (bits_per_comp > 32))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            for (uint32_t i = 0; i < num_comps; i++)
            {
                m_comp_size[i] = bits_per_comp;
                m_comp_ofs[i] = i * bits_per_comp;
                if (reversed)
                    m_comp_ofs[i] = ((num_comps - 1) * bits_per_comp) - m_comp_ofs[i];
            }

            for (uint32_t i = 0; i < 4; i++)
                m_comp_max[i] = static_cast<uint32_t>((1ULL << m_comp_size[i]) - 1ULL);

            m_pixel_stride = (pixel_stride >= 0) ? pixel_stride : (num_comps * bits_per_comp + 7) / 8;

            return true;
        }

        // Format examples:
        // R16G16B16
        // B5G6R5
        // B5G5R5x1
        // Y8A8
        // A8R8G8B8
        // First component is at LSB in memory. Assumes unsigned integer components, 1-32bits each.
        bool init(const char *pComp_map, int pixel_stride = -1, int force_comp_size = -1)
        {
            clear();

            uint32_t cur_bit_ofs = 0;

            while (*pComp_map)
            {
                char c = *pComp_map++;

                int comp_index = -1;
                if (c == 'R')
                    comp_index = 0;
                else if (c == 'G')
                    comp_index = 1;
                else if (c == 'B')
                    comp_index = 2;
                else if (c == 'A')
                    comp_index = 3;
                else if (c == 'Y')
                    comp_index = 4;
                else if (c != 'x')
                    return false;

                uint32_t comp_size = 0;

                uint32_t n = *pComp_map;
                if ((n >= '0') && (n <= '9'))
                {
                    comp_size = n - '0';
                    pComp_map++;

                    n = *pComp_map;
                    if ((n >= '0') && (n <= '9'))
                    {
                        comp_size = (comp_size * 10) + (n - '0');
                        pComp_map++;
                    }
                }

                if (force_comp_size != -1)
                    comp_size = force_comp_size;

                if ((!comp_size) || (comp_size > 32))
                    return false;

                if (comp_index == 4)
                {
                    if (m_comp_size[0] || m_comp_size[1] || m_comp_size[2])
                        return false;

                    //m_comp_ofs[0] = m_comp_ofs[1] = m_comp_ofs[2] = cur_bit_ofs;
                    //m_comp_size[0] = m_comp_size[1] = m_comp_size[2] = comp_size;
                    m_comp_ofs[1] = cur_bit_ofs;
                    m_comp_size[1] = comp_size;
                    m_rgb_is_luma = true;
                    m_num_comps++;
                }
                else if (comp_index >= 0)
                {
                    if (m_comp_size[comp_index])
                        return false;

                    m_comp_ofs[comp_index] = cur_bit_ofs;
                    m_comp_size[comp_index] = comp_size;
                    m_num_comps++;
                }

                cur_bit_ofs += comp_size;
            }

            for (uint32_t i = 0; i < 4; i++)
                m_comp_max[i] = static_cast<uint32_t>((1ULL << m_comp_size[i]) - 1ULL);

            if (pixel_stride >= 0)
                m_pixel_stride = pixel_stride;
            else
                m_pixel_stride = (cur_bit_ofs + 7) / 8;
            return true;
        }

    private:
        uint32_t m_pixel_stride;
        uint32_t m_num_comps;
        uint32_t m_comp_size[4];
        uint32_t m_comp_ofs[4];
        uint32_t m_comp_max[4];
        bool m_rgb_is_luma;
    };

} // namespace vogl
