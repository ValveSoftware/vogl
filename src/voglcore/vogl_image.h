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

// File: vogl_image.h
#pragma once

#include "vogl_core.h"
#include "vogl_color.h"
#include "vogl_vec.h"
#include "vogl_pixel_format.h"
#include "vogl_rect.h"

namespace vogl
{
    enum
    {
        BLIT_FLAG_SWAP_RGB = 1,
        BLIT_FLAG_MULTIPLY_BY_ALPHA = 2,
        BLIT_FLAG_UNPREMULTIPLY_BY_ALPHA = 4
    };

    template <typename color_type>
    class image
    {
    public:
        typedef color_type color_t;
        typedef typename color_type::component_t component_t;

        typedef vogl::vector<color_type> pixel_buf_t;

        image()
            : m_width(0),
              m_height(0),
              m_pitch(0),
              m_total(0),
              m_comp_flags(pixel_format_helpers::cDefaultCompFlags),
              m_pPixels(NULL)
        {
        }

        // pitch is in PIXELS, not bytes.
        image(uint32_t width, uint32_t height, uint32_t pitch = UINT_MAX, const color_type &background = color_type::make_black(), uint32_t flags = pixel_format_helpers::cDefaultCompFlags)
            : m_comp_flags(flags)
        {
            VOGL_ASSERT((width > 0) && (height > 0));
            if (pitch == UINT_MAX)
                pitch = width;

            m_pixel_buf.resize(pitch * height);

            m_width = width;
            m_height = height;
            m_pitch = pitch;
            m_total = m_pitch * m_height;

            m_pPixels = &m_pixel_buf.front();

            set_all(background);
        }

        // pitch is in PIXELS, not bytes.
        image(color_type *pPixels, uint32_t width, uint32_t height, uint32_t pitch = UINT_MAX, uint32_t flags = pixel_format_helpers::cDefaultCompFlags)
        {
            alias(pPixels, width, height, pitch, flags);
        }

        image &operator=(const image &other)
        {
            if (this == &other)
                return *this;

            if (other.m_pixel_buf.is_empty())
            {
                // This doesn't look very safe - let's make a new instance.
                //m_pixel_buf.clear();
                //m_pPixels = other.m_pPixels;

                const uint32_t total_pixels = other.m_pitch * other.m_height;
                if ((total_pixels) && (other.m_pPixels))
                {
                    m_pixel_buf.resize(total_pixels);
                    m_pixel_buf.insert(0, other.m_pPixels, m_pixel_buf.size());
                    m_pPixels = &m_pixel_buf.front();
                }
                else
                {
                    m_pixel_buf.clear();
                    m_pPixels = NULL;
                }
            }
            else
            {
                m_pixel_buf = other.m_pixel_buf;
                m_pPixels = &m_pixel_buf.front();
            }

            m_width = other.m_width;
            m_height = other.m_height;
            m_pitch = other.m_pitch;
            m_total = other.m_total;
            m_comp_flags = other.m_comp_flags;

            return *this;
        }

        image(const image &other)
            : m_width(0), m_height(0), m_pitch(0), m_total(0), m_comp_flags(pixel_format_helpers::cDefaultCompFlags), m_pPixels(NULL)
        {
            *this = other;
        }

        // pitch is in PIXELS, not bytes.
        void alias(color_type *pPixels, uint32_t width, uint32_t height, uint32_t pitch = UINT_MAX, uint32_t flags = pixel_format_helpers::cDefaultCompFlags)
        {
            m_pixel_buf.clear();

            m_pPixels = pPixels;

            m_width = width;
            m_height = height;
            m_pitch = (pitch == UINT_MAX) ? width : pitch;
            m_total = m_pitch * m_height;
            m_comp_flags = flags;
        }

        // pitch is in PIXELS, not bytes.
        bool grant_ownership(color_type *pPixels, uint32_t width, uint32_t height, uint32_t pitch = UINT_MAX, uint32_t flags = pixel_format_helpers::cDefaultCompFlags)
        {
            if (pitch == UINT_MAX)
                pitch = width;

            if ((!pPixels) || (!width) || (!height) || (pitch < width))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (pPixels == get_ptr())
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            clear();

            if (!m_pixel_buf.grant_ownership(pPixels, height * pitch, height * pitch))
                return false;

            m_pPixels = pPixels;

            m_width = width;
            m_height = height;
            m_pitch = pitch;
            m_total = pitch * height;
            m_comp_flags = flags;

            return true;
        }

        void clear()
        {
            m_pPixels = NULL;
            m_pixel_buf.clear();
            m_width = 0;
            m_height = 0;
            m_pitch = 0;
            m_total = 0;
            m_comp_flags = pixel_format_helpers::cDefaultCompFlags;
        }

        inline bool is_valid() const
        {
            return m_total > 0;
        }

        inline pixel_format_helpers::component_flags get_comp_flags() const
        {
            return static_cast<pixel_format_helpers::component_flags>(m_comp_flags);
        }
        inline void set_comp_flags(pixel_format_helpers::component_flags new_flags)
        {
            m_comp_flags = new_flags;
        }
        inline void reset_comp_flags()
        {
            m_comp_flags = pixel_format_helpers::cDefaultCompFlags;
        }

        inline bool is_component_valid(uint32_t index) const
        {
            VOGL_ASSERT(index < 4U);
            return utils::is_flag_set(m_comp_flags, index);
        }
        inline void set_component_valid(uint32_t index, bool state)
        {
            VOGL_ASSERT(index < 4U);
            utils::set_flag(m_comp_flags, index, state);
        }
        inline void set_valid_components(bool rgb, bool alpha)
        {
            set_comp_flags(static_cast<pixel_format_helpers::component_flags>((rgb ? pixel_format_helpers::cCompFlagsRGBAValid : 0) | (alpha ? pixel_format_helpers::cCompFlagAValid : 0)));
        }

        inline bool has_rgb() const
        {
            return is_component_valid(0) || is_component_valid(1) || is_component_valid(2);
        }
        inline bool has_alpha() const
        {
            return is_component_valid(3);
        }

        inline bool is_grayscale() const
        {
            return utils::is_bit_set(m_comp_flags, pixel_format_helpers::cCompFlagGrayscale);
        }
        inline void set_grayscale(bool state)
        {
            utils::set_bit(m_comp_flags, pixel_format_helpers::cCompFlagGrayscale, state);
        }

        void set_all(const color_type &c)
        {
            for (uint32_t i = 0; i < m_total; i++)
                m_pPixels[i] = c;
        }

        void flip_x()
        {
            const uint32_t half_width = m_width / 2;
            for (uint32_t y = 0; y < m_height; y++)
            {
                for (uint32_t x = 0; x < half_width; x++)
                {
                    color_type c((*this)(x, y));
                    (*this)(x, y) = (*this)(m_width - 1 - x, y);
                    (*this)(m_width - 1 - x, y) = c;
                }
            }
        }

        void flip_y()
        {
            const uint32_t half_height = m_height / 2;
            for (uint32_t y = 0; y < half_height; y++)
            {
                for (uint32_t x = 0; x < m_width; x++)
                {
                    color_type c((*this)(x, y));
                    (*this)(x, y) = (*this)(x, m_height - 1 - y);
                    (*this)(x, m_height - 1 - y) = c;
                }
            }
        }

        void convert_to_grayscale()
        {
            for (uint32_t y = 0; y < m_height; y++)
                for (uint32_t x = 0; x < m_width; x++)
                {
                    color_type c((*this)(x, y));
                    typename color_type::component_t l = static_cast<typename color_type::component_t>(c.get_luma());
                    c.r = l;
                    c.g = l;
                    c.b = l;
                    (*this)(x, y) = c;
                }

            set_grayscale(true);
        }

        void swizzle(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
        {
            for (uint32_t y = 0; y < m_height; y++)
                for (uint32_t x = 0; x < m_width; x++)
                {
                    const color_type &c = (*this)(x, y);

                    (*this)(x, y) = color_type(c[r], c[g], c[b], c[a]);
                }
        }

        void set_alpha_to_luma()
        {
            for (uint32_t y = 0; y < m_height; y++)
                for (uint32_t x = 0; x < m_width; x++)
                {
                    color_type c((*this)(x, y));
                    typename color_type::component_t l = static_cast<typename color_type::component_t>(c.get_luma());
                    c.a = l;
                    (*this)(x, y) = c;
                }

            set_component_valid(3, true);
        }

        bool extract_block_clamped(color_type *pDst, uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool flip_xy = false) const
        {
            if ((x >= m_width) || (y >= m_height))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (flip_xy)
            {
                for (uint32_t y_ofs = 0; y_ofs < h; y_ofs++)
                    for (uint32_t x_ofs = 0; x_ofs < w; x_ofs++)
                        pDst[x_ofs * h + y_ofs] = get_clamped(x_ofs + x, y_ofs + y); // 5/4/12 - this was incorrectly x_ofs * 4
            }
            else if (((x + w) > m_width) || ((y + h) > m_height))
            {
                for (uint32_t y_ofs = 0; y_ofs < h; y_ofs++)
                    for (uint32_t x_ofs = 0; x_ofs < w; x_ofs++)
                        *pDst++ = get_clamped(x_ofs + x, y_ofs + y);
            }
            else
            {
                const color_type *pSrc = get_scanline(y) + x;

                for (uint32_t i = h; i; i--)
                {
                    memcpy(pDst, pSrc, w * sizeof(color_type));
                    pDst += w;

                    pSrc += m_pitch;
                }
            }

            return true;
        }

        bool extract_block_wrapped(color_type *pDst, int x, int y, uint32_t w, uint32_t h, bool flip_xy = false) const
        {
            if (flip_xy)
            {
                for (uint32_t y_ofs = 0; y_ofs < h; y_ofs++)
                    for (uint32_t x_ofs = 0; x_ofs < w; x_ofs++)
                        pDst[x_ofs * h + y_ofs] = get_wrapped(x_ofs + x, y_ofs + y);
            }
            else
            {
                for (uint32_t y_ofs = 0; y_ofs < h; y_ofs++)
                    for (uint32_t x_ofs = 0; x_ofs < w; x_ofs++)
                        *pDst++ = get_wrapped(x_ofs + x, y_ofs + y);
            }

            return true;
        }

        // No clipping!
        void unclipped_fill_box(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const color_type &c)
        {
            if (((x + w) > m_width) || ((y + h) > m_height))
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            color_type *p = get_scanline(y) + x;

            for (uint32_t i = h; i; i--)
            {
                color_type *q = p;
                for (uint32_t j = w; j; j--)
                    *q++ = c;
                p += m_pitch;
            }
        }

        void draw_rect(int x, int y, uint32_t width, uint32_t height, const color_type &c)
        {
            draw_line(x, y, x + width - 1, y, c);
            draw_line(x, y, x, y + height - 1, c);
            draw_line(x + width - 1, y, x + width - 1, y + height - 1, c);
            draw_line(x, y + height - 1, x + width - 1, y + height - 1, c);
        }

        // No clipping!
        bool unclipped_blit(uint32_t src_x, uint32_t src_y, uint32_t src_w, uint32_t src_h, uint32_t dst_x, uint32_t dst_y, const image &src, uint32_t blit_flags = 0)
        {
            if ((!is_valid()) || (!src.is_valid()))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (((src_x + src_w) > src.get_width()) || ((src_y + src_h) > src.get_height()))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (((dst_x + src_w) > get_width()) || ((dst_y + src_h) > get_height()))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            const color_type *VOGL_RESTRICT pS = &src(src_x, src_y);
            color_type *VOGL_RESTRICT pD = &(*this)(dst_x, dst_y);

            const uint32_t bytes_to_copy_per_scanline = src_w * sizeof(color_type);
            const uint32_t src_pitch_minus_src_w = src.get_pitch() - src_w;
            const uint32_t dst_pitch_minus_src_w = get_pitch() - src_w;

            if (blit_flags & BLIT_FLAG_UNPREMULTIPLY_BY_ALPHA)
            {
                if (vogl_is_little_endian() && (!color_t::component_traits::cSigned) && (sizeof(component_t) == sizeof(uint8_t)))
                {
                    VOGL_ASSERT(sizeof(color_t) == sizeof(uint32_t));
                    for (uint32_t i = src_h; i; i--)
                    {
                        color_quad_u8 *pD_end = pD + src_w;
                        if (blit_flags & BLIT_FLAG_SWAP_RGB)
                        {
                            do
                            {
                                uint32_t s = *reinterpret_cast<const uint32_t *>(pS);
                                pS++;

                                uint32_t a = s >> 24, b = s & 0xFF, g = (s & 0xFF00) >> 8, r = (s & 0xFF0000) >> 16;
                                if ((a != 255) && (a))
                                {
                                    uint32_t round = a / 2;
                                    r = math::clamp255((r * 255 + round) / a);
                                    g = math::clamp255((g * 255 + round) / a);
                                    b = math::clamp255((b * 255 + round) / a);
                                }

                                *reinterpret_cast<uint32_t *>(pD) = ((a << 24) | (b << 16) | (g << 8) | r);
                                pD++;

                            } while (pD != pD_end);
                        }
                        else
                        {
                            do
                            {
                                uint32_t s = *reinterpret_cast<const uint32_t *>(pS);
                                pS++;

                                uint32_t a = s >> 24;
                                if ((a != 255) && (a))
                                {
                                    uint32_t r = s & 0xFF, g = (s & 0xFF00) >> 8, b = (s & 0xFF0000) >> 16;
                                    uint32_t round = a / 2;
                                    r = math::clamp255((r * 255 + round) / a);
                                    g = math::clamp255((g * 255 + round) / a);
                                    b = math::clamp255((b * 255 + round) / a);
                                    *reinterpret_cast<uint32_t *>(pD) = ((a << 24) | (b << 16) | (g << 8) | r);
                                }
                                else
                                {
                                    *reinterpret_cast<uint32_t *>(pD) = s;
                                }

                                pD++;

                            } while (pD != pD_end);
                        }

                        pS += src_pitch_minus_src_w;
                        pD += dst_pitch_minus_src_w;
                    }
                }
                else
                {
                    for (uint32_t i = src_h; i; i--)
                    {
                        color_quad_u8 *pD_end = pD + src_w;
                        do
                        {
                            color_type s(*pS++);

                            if (blit_flags & BLIT_FLAG_SWAP_RGB)
                                std::swap(s.r, s.b);

                            float flScale = 0.0f;
                            if (s.a != 0)
                            {
                                if (color_t::component_traits::cFloat)
                                    flScale = 1.0f / s.a;
                                else
                                    flScale = static_cast<float>(color_t::component_traits::cMax) / s.a;
                            }
                            s.scale_rgb_by_float(flScale);

                            *pD++ = s;
                        } while (pD != pD_end);

                        pS += src_pitch_minus_src_w;
                        pD += dst_pitch_minus_src_w;
                    }
                }
            }
            else if (blit_flags & BLIT_FLAG_MULTIPLY_BY_ALPHA)
            {
                if (vogl_is_little_endian() && (!color_t::component_traits::cSigned) && (sizeof(component_t) == sizeof(uint8_t)))
                {
                    VOGL_ASSERT(sizeof(color_t) == sizeof(uint32_t));
                    for (uint32_t i = src_h; i; i--)
                    {
                        color_quad_u8 *pD_end = pD + src_w;
                        if (blit_flags & BLIT_FLAG_SWAP_RGB)
                        {
                            do
                            {
                                uint32_t a = pS->a;
                                uint32_t b = math::mul255(pS->r, a);
                                uint32_t g = math::mul255(pS->g, a);
                                uint32_t r = math::mul255(pS->b, a);
                                pS++;

                                *reinterpret_cast<uint32_t *>(pD) = ((a << 24) | (b << 16) | (g << 8) | r);
                                pD++;
                            } while (pD != pD_end);
                        }
                        else
                        {
                            do
                            {
                                uint32_t a = pS->a;
                                uint32_t r = math::mul255(pS->r, a);
                                uint32_t g = math::mul255(pS->g, a);
                                uint32_t b = math::mul255(pS->b, a);
                                pS++;

                                *reinterpret_cast<uint32_t *>(pD) = ((a << 24) | (b << 16) | (g << 8) | r);
                                pD++;
                            } while (pD != pD_end);
                        }

                        pS += src_pitch_minus_src_w;
                        pD += dst_pitch_minus_src_w;
                    }
                }
                else
                {
                    for (uint32_t i = src_h; i; i--)
                    {
                        color_quad_u8 *pD_end = pD + src_w;
                        do
                        {
                            color_type s(*pS++);

                            if (blit_flags & BLIT_FLAG_SWAP_RGB)
                                std::swap(s.r, s.b);

                            s.scale_rgb_by_component(s.a);

                            *pD++ = s;
                        } while (pD != pD_end);

                        pS += src_pitch_minus_src_w;
                        pD += dst_pitch_minus_src_w;
                    }
                }
            }
            else if (blit_flags & BLIT_FLAG_SWAP_RGB)
            {
                for (uint32_t i = src_h; i; i--)
                {
                    color_quad_u8 *pD_end = pD + src_w;
                    if (vogl_is_little_endian() && (sizeof(component_t) == sizeof(uint8_t)))
                    {
                        VOGL_ASSERT(sizeof(color_t) == sizeof(uint32_t));
                        do
                        {
                            uint32_t c = *reinterpret_cast<const uint32_t *>(pS);
                            pS++;

                            c = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c & 0xFF0000) >> 16);

                            *reinterpret_cast<uint32_t *>(pD) = c;
                            pD++;
                        } while (pD != pD_end);
                    }
                    else
                    {
                        do
                        {
                            color_type s(*pS++);
                            std::swap(s.r, s.b);
                            *pD++ = s;
                        } while (pD != pD_end);
                    }

                    pS += src_pitch_minus_src_w;
                    pD += dst_pitch_minus_src_w;
                }
            }
            else
            {
                if ((bytes_to_copy_per_scanline == src.get_pitch_in_bytes()) && (bytes_to_copy_per_scanline == get_pitch_in_bytes()))
                {
                    memcpy(pD, pS, bytes_to_copy_per_scanline * src_h);
                }
                else
                {
                    for (uint32_t i = src_h; i; i--)
                    {
                        memcpy(pD, pS, bytes_to_copy_per_scanline);

                        pS += src.get_pitch();
                        pD += get_pitch();
                    }
                }
            }

            return true;
        }

        // With clipping.
        bool blit(int dst_x, int dst_y, const image &src, uint32_t blit_flags = 0)
        {
            if ((!is_valid()) || (!src.is_valid()))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            int src_x = 0;
            int src_y = 0;

            if (dst_x < 0)
            {
                src_x = -dst_x;
                if (src_x >= static_cast<int>(src.get_width()))
                    return false;
                dst_x = 0;
            }

            if (dst_y < 0)
            {
                src_y = -dst_y;
                if (src_y >= static_cast<int>(src.get_height()))
                    return false;
                dst_y = 0;
            }

            if ((dst_x >= (int)m_width) || (dst_y >= (int)m_height))
                return false;

            uint32_t width = math::minimum(m_width - dst_x, src.get_width() - src_x);
            uint32_t height = math::minimum(m_height - dst_y, src.get_height() - src_y);

            bool success = unclipped_blit(src_x, src_y, width, height, dst_x, dst_y, src, blit_flags);
            (void)success;
            VOGL_ASSERT(success);

            return true;
        }

        // With clipping.
        bool blit(int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, const image &src, uint32_t blit_flags = 0)
        {
            if ((!is_valid()) || (!src.is_valid()))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            rect src_rect(src_x, src_y, src_x + src_w, src_y + src_h);
            if (!src_rect.intersect(src.get_bounds()))
                return false;

            rect dst_rect(dst_x, dst_y, dst_x + src_rect.get_width(), dst_y + src_rect.get_height());
            if (!dst_rect.intersect(get_bounds()))
                return false;

            int src_x_ofs = dst_rect.get_left() - dst_x;
            if (src_x_ofs >= static_cast<int>(src_rect.get_width()))
                return false;
            src_rect[0][0] += src_x_ofs;

            int src_y_ofs = dst_rect.get_top() - dst_y;
            if (src_y_ofs >= static_cast<int>(src_rect.get_height()))
                return false;
            src_rect[0][1] += src_y_ofs;
            VOGL_ASSERT(src.is_valid());

            bool success = unclipped_blit(
                src_rect.get_left(), src_rect.get_top(),
                math::minimum(src_rect.get_width(), dst_rect.get_width()), math::minimum(src_rect.get_height(), dst_rect.get_height()),
                dst_rect.get_left(), dst_rect.get_top(), src, blit_flags);
            (void)success;
            VOGL_ASSERT(success);

            return true;
        }

        // In-place resize of image dimensions (cropping).
        bool crop(uint32_t new_width, uint32_t new_height, uint32_t new_pitch = UINT_MAX, const color_type background = color_type::make_black())
        {
            if (new_pitch == UINT_MAX)
                new_pitch = new_width;

            if ((new_width == m_width) && (new_height == m_height) && (new_pitch == m_pitch))
                return true;

            if ((!new_width) || (!new_height) || (!new_pitch))
            {
                clear();
                return false;
            }

            pixel_buf_t existing_pixels;
            existing_pixels.swap(m_pixel_buf);

            if (!m_pixel_buf.try_resize(new_height * new_pitch))
            {
                clear();
                return false;
            }

            for (uint32_t y = 0; y < new_height; y++)
            {
                for (uint32_t x = 0; x < new_width; x++)
                {
                    if ((x < m_width) && (y < m_height))
                        m_pixel_buf[x + y * new_pitch] = existing_pixels[x + y * m_pitch];
                    else
                        m_pixel_buf[x + y * new_pitch] = background;
                }
            }

            m_width = new_width;
            m_height = new_height;
            m_pitch = new_pitch;
            m_total = new_pitch * new_height;
            m_pPixels = &m_pixel_buf.front();

            return true;
        }

        // In-place resize of image dimensions (same as cropping).
        bool resize(uint32_t new_width, uint32_t new_height, uint32_t new_pitch = UINT_MAX, const color_type background = color_type::make_black())
        {
            return crop(new_width, new_height, new_pitch, background);
        }

        inline uint32_t get_width() const
        {
            return m_width;
        }
        inline uint32_t get_height() const
        {
            return m_height;
        }
        inline uint32_t get_total_pixels() const
        {
            return m_width * m_height;
        }

        inline rect get_bounds() const
        {
            return rect(0, 0, m_width, m_height);
        }

        inline uint32_t get_pitch() const
        {
            return m_pitch;
        }
        inline uint32_t get_pitch_in_bytes() const
        {
            return m_pitch * sizeof(color_type);
        }

        // Returns pitch * height, NOT width * height!
        inline uint32_t get_total() const
        {
            return m_total;
        }

        inline uint32_t get_block_width(uint32_t block_size) const
        {
            return (m_width + block_size - 1) / block_size;
        }
        inline uint32_t get_block_height(uint32_t block_size) const
        {
            return (m_height + block_size - 1) / block_size;
        }
        inline uint32_t get_total_blocks(uint32_t block_size) const
        {
            return get_block_width(block_size) * get_block_height(block_size);
        }

        inline uint32_t get_size_in_bytes() const
        {
            return sizeof(color_type) * m_total;
        }

        inline const color_type *get_pixels() const
        {
            return m_pPixels;
        }
        inline color_type *get_pixels()
        {
            return m_pPixels;
        }

        inline const color_type &operator()(uint32_t x, uint32_t y) const
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            return m_pPixels[x + y * m_pitch];
        }

        inline color_type &operator()(uint32_t x, uint32_t y)
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            return m_pPixels[x + y * m_pitch];
        }

        inline const color_type &get_unclamped(uint32_t x, uint32_t y) const
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            return m_pPixels[x + y * m_pitch];
        }

        inline color_type &get_unclamped(uint32_t x, uint32_t y)
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            return m_pPixels[x + y * m_pitch];
        }

        inline const color_type &get_clamped(int x, int y) const
        {
            x = math::clamp<int>(x, 0, m_width - 1);
            y = math::clamp<int>(y, 0, m_height - 1);
            return m_pPixels[x + y * m_pitch];
        }

        inline color_type &get_clamped(int x, int y)
        {
            x = math::clamp<int>(x, 0, m_width - 1);
            y = math::clamp<int>(y, 0, m_height - 1);
            return m_pPixels[x + y * m_pitch];
        }

        inline const color_type &get_wrapped(int x, int y) const
        {
            if (static_cast<uint32_t>(x) >= m_width)
                x = math::posmod(x, m_width);
            if (static_cast<uint32_t>(y) >= m_height)
                y = math::posmod(y, m_height);
            return m_pPixels[x + y * m_pitch];
        }

        inline color_type &get_wrapped(int x, int y)
        {
            if (static_cast<uint32_t>(x) >= m_width)
                x = math::posmod(x, m_width);
            if (static_cast<uint32_t>(y) >= m_height)
                y = math::posmod(y, m_height);
            return m_pPixels[x + y * m_pitch];
        }

        inline const color_type &get_clamped_or_wrapped(int x, int y, bool wrap_u, bool wrap_v) const
        {
            x = wrap_u ? math::posmod(x, m_width) : math::clamp<int>(x, 0, m_width - 1);
            y = wrap_v ? math::posmod(y, m_height) : math::clamp<int>(y, 0, m_height - 1);
            return m_pPixels[x + y * m_pitch];
        }

        inline color_type &get_clamped_or_wrapped(int x, int y, bool wrap_u, bool wrap_v)
        {
            x = wrap_u ? math::posmod(x, m_width) : math::clamp<int>(x, 0, m_width - 1);
            y = wrap_v ? math::posmod(y, m_height) : math::clamp<int>(y, 0, m_height - 1);
            return m_pPixels[x + y * m_pitch];
        }

        // Sample image with bilinear filtering.
        // (x,y) - Continuous coordinates, where pixel centers are at (.5,.5), valid image coords are [0,width] and [0,height].
        void get_filtered(float x, float y, color_type &result) const
        {
            x -= .5f;
            y -= .5f;

            int ix = (int)floor(x);
            int iy = (int)floor(y);
            float wx = x - ix;
            float wy = y - iy;

            color_type a(get_clamped(ix, iy));
            color_type b(get_clamped(ix + 1, iy));
            color_type c(get_clamped(ix, iy + 1));
            color_type d(get_clamped(ix + 1, iy + 1));

            for (uint32_t i = 0; i < 4; i++)
            {
                double top = math::lerp<double>(a[i], b[i], wx);
                double bot = math::lerp<double>(c[i], d[i], wx);
                double m = math::lerp<double>(top, bot, wy);

                if (!color_type::component_traits::cFloat)
                    m += .5f;

                result.set_component(i, static_cast<typename color_type::parameter_t>(m));
            }
        }

        void get_filtered(float x, float y, vec4F &result, bool wrap_u = false, bool wrap_v = false) const
        {
            x -= .5f;
            y -= .5f;

            int ix = (int)floor(x);
            int iy = (int)floor(y);
            float wx = x - ix;
            float wy = y - iy;

            color_type a(get_clamped_or_wrapped(ix, iy, wrap_u, wrap_v));
            color_type b(get_clamped_or_wrapped(ix + 1, iy, wrap_u, wrap_v));
            color_type c(get_clamped_or_wrapped(ix, iy + 1, wrap_u, wrap_v));
            color_type d(get_clamped_or_wrapped(ix + 1, iy + 1, wrap_u, wrap_v));

            for (uint32_t i = 0; i < 4; i++)
            {
                float top = math::lerp<float>(a[i], b[i], wx);
                float bot = math::lerp<float>(c[i], d[i], wx);
                float m = math::lerp<float>(top, bot, wy);

                result[i] = m;
            }
        }

        void get_unfiltered(float x, float y, vec4F &result, bool wrap_u = false, bool wrap_v = false) const
        {
            x -= .5f;
            y -= .5f;

            int ix = (int)floor(x);
            int iy = (int)floor(y);

            color_type c(get_clamped_or_wrapped(ix, iy, wrap_u, wrap_v));
            result.set(static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]), static_cast<float>(c[3]));
        }

        inline void set_pixel_unclipped(uint32_t x, uint32_t y, const color_type &c)
        {
            VOGL_ASSERT((x < m_width) && (y < m_height));
            m_pPixels[x + y * m_pitch] = c;
        }

        inline void set_pixel_clipped(int x, int y, const color_type &c)
        {
            if ((static_cast<uint32_t>(x) >= m_width) || (static_cast<uint32_t>(y) >= m_height))
                return;

            m_pPixels[x + y * m_pitch] = c;
        }

        inline const color_type *get_scanline(uint32_t y) const
        {
            VOGL_ASSERT(y < m_height);
            return &m_pPixels[y * m_pitch];
        }

        inline color_type *get_scanline(uint32_t y)
        {
            VOGL_ASSERT(y < m_height);
            return &m_pPixels[y * m_pitch];
        }

        inline const color_type *get_ptr() const
        {
            return m_pPixels;
        }

        inline color_type *get_ptr()
        {
            return m_pPixels;
        }

        inline void swap(image &other)
        {
            utils::swap(m_width, other.m_width);
            utils::swap(m_height, other.m_height);
            utils::swap(m_pitch, other.m_pitch);
            utils::swap(m_total, other.m_total);
            utils::swap(m_comp_flags, other.m_comp_flags);
            utils::swap(m_pPixels, other.m_pPixels);
            m_pixel_buf.swap(other.m_pixel_buf);
        }

        void draw_line(int xs, int ys, int xe, int ye, const color_type &color)
        {
            if (xs > xe)
            {
                utils::swap(xs, xe);
                utils::swap(ys, ye);
            }

            int dx = xe - xs, dy = ye - ys;
            if (!dx)
            {
                if (ys > ye)
                    utils::swap(ys, ye);
                for (int i = ys; i <= ye; i++)
                    set_pixel_clipped(xs, i, color);
            }
            else if (!dy)
            {
                for (int i = xs; i < xe; i++)
                    set_pixel_clipped(i, ys, color);
            }
            else if (dy > 0)
            {
                if (dy <= dx)
                {
                    int e = 2 * dy - dx, e_no_inc = 2 * dy, e_inc = 2 * (dy - dx);
                    rasterize_line(xs, ys, xe, ye, 0, 1, e, e_inc, e_no_inc, color);
                }
                else
                {
                    int e = 2 * dx - dy, e_no_inc = 2 * dx, e_inc = 2 * (dx - dy);
                    rasterize_line(xs, ys, xe, ye, 1, 1, e, e_inc, e_no_inc, color);
                }
            }
            else
            {
                dy = -dy;
                if (dy <= dx)
                {
                    int e = 2 * dy - dx, e_no_inc = 2 * dy, e_inc = 2 * (dy - dx);
                    rasterize_line(xs, ys, xe, ye, 0, -1, e, e_inc, e_no_inc, color);
                }
                else
                {
                    int e = 2 * dx - dy, e_no_inc = (2 * dx), e_inc = 2 * (dx - dy);
                    rasterize_line(xe, ye, xs, ys, 1, -1, e, e_inc, e_no_inc, color);
                }
            }
        }

        const pixel_buf_t &get_pixel_buf() const
        {
            return m_pixel_buf;
        }
        pixel_buf_t &get_pixel_buf()
        {
            return m_pixel_buf;
        }

    private:
        uint32_t m_width;
        uint32_t m_height;
        uint32_t m_pitch;
        uint32_t m_total;
        uint32_t m_comp_flags;

        color_type *m_pPixels;

        pixel_buf_t m_pixel_buf;

        void rasterize_line(int xs, int ys, int xe, int ye, int pred, int inc_dec, int e, int e_inc, int e_no_inc, const color_type &color)
        {
            int start, end, var;

            if (pred)
            {
                start = ys;
                end = ye;
                var = xs;
                for (int i = start; i <= end; i++)
                {
                    set_pixel_clipped(var, i, color);
                    if (e < 0)
                        e += e_no_inc;
                    else
                    {
                        var += inc_dec;
                        e += e_inc;
                    }
                }
            }
            else
            {
                start = xs;
                end = xe;
                var = ys;
                for (int i = start; i <= end; i++)
                {
                    set_pixel_clipped(i, var, color);
                    if (e < 0)
                        e += e_no_inc;
                    else
                    {
                        var += inc_dec;
                        e += e_inc;
                    }
                }
            }
        }
    };

    typedef image<color_quad_u8> image_u8;
    typedef image<color_quad_i16> image_i16;
    typedef image<color_quad_u16> image_u16;
    typedef image<color_quad_i32> image_i32;
    typedef image<color_quad_u32> image_u32;
    typedef image<color_quad_f> image_f;

    template <typename color_type>
    inline void swap(image<color_type> &a, image<color_type> &b)
    {
        a.swap(b);
    }

} // namespace vogl
