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

// File: vogl_dxt.h
#pragma once

#include "vogl_core.h"
#include "vogl.h"
#include "vogl_color.h"
#include "vogl_vec.h"
#include "vogl_rand.h"
#include "vogl_sparse_bit_array.h"
#include "vogl_hash_map.h"
#include <map>

#define VOGL_DXT_ALT_ROUNDING 1

namespace vogl
{
    enum dxt_constants
    {
        cDXT1BytesPerBlock = 8U,
        cDXT5NBytesPerBlock = 16U,
        cDXT5SelectorBits = 3U,
        cDXT5SelectorValues = 1U << cDXT5SelectorBits,
        cDXT5SelectorMask = cDXT5SelectorValues - 1U,
        cDXT1SelectorBits = 2U,
        cDXT1SelectorValues = 1U << cDXT1SelectorBits,
        cDXT1SelectorMask = cDXT1SelectorValues - 1U,
        cDXTBlockShift = 2U,
        cDXTBlockSize = 1U << cDXTBlockShift
    };

    enum dxt_format
    {
        cDXTInvalid = -1,

        // cDXT1/1A must appear first!
        cDXT1,
        cDXT1A,
        cDXT3,
        cDXT5,
        cDXT5A,
        cDXN_XY, // inverted relative to standard ATI2, 360's DXN
        cDXN_YX, // standard ATI2,
        cETC1    // Ericsson texture compression (color only, 4x4 blocks, 4bpp, 64-bits/block)
    };

    const float cDXT1MaxLinearValue = 3.0f;
    const float cDXT1InvMaxLinearValue = 1.0f / 3.0f;

    const float cDXT5MaxLinearValue = 7.0f;
    const float cDXT5InvMaxLinearValue = 1.0f / 7.0f;

    // Converts DXT1 raw color selector index to a linear value.
    extern const uint8_t g_dxt1_to_linear[cDXT1SelectorValues];

    // Converts DXT5 raw alpha selector index to a linear value.
    extern const uint8_t g_dxt5_to_linear[cDXT5SelectorValues];

    // Converts DXT1 linear color selector index to a raw value (inverse of g_dxt1_to_linear).
    extern const uint8_t g_dxt1_from_linear[cDXT1SelectorValues];

    // Converts DXT5 linear alpha selector index to a raw value (inverse of g_dxt5_to_linear).
    extern const uint8_t g_dxt5_from_linear[cDXT5SelectorValues];

    extern const uint8_t g_dxt5_alpha6_to_linear[cDXT5SelectorValues];

    extern const uint8_t g_six_alpha_invert_table[cDXT5SelectorValues];
    extern const uint8_t g_eight_alpha_invert_table[cDXT5SelectorValues];

    const char *get_dxt_format_string(dxt_format fmt);
    uint32_t get_dxt_format_bits_per_pixel(dxt_format fmt);
    bool get_dxt_format_has_alpha(dxt_format fmt);

    const char *get_dxt_quality_string(vogl_dxt_quality q);

    const char *get_dxt_compressor_name(vogl_dxt_compressor_type c);

    struct dxt1_block
    {
        uint8_t m_low_color[2];
        uint8_t m_high_color[2];

        enum
        {
            cNumSelectorBytes = 4
        };
        uint8_t m_selectors[cNumSelectorBytes];

        inline void clear()
        {
            utils::zero_this(this);
        }

        // These methods assume the in-memory rep is in LE byte order.
        inline uint32_t get_low_color() const
        {
            return m_low_color[0] | (m_low_color[1] << 8U);
        }

        inline uint32_t get_high_color() const
        {
            return m_high_color[0] | (m_high_color[1] << 8U);
        }

        inline void set_low_color(uint16_t c)
        {
            m_low_color[0] = static_cast<uint8_t>(c & 0xFF);
            m_low_color[1] = static_cast<uint8_t>((c >> 8) & 0xFF);
        }

        inline void set_high_color(uint16_t c)
        {
            m_high_color[0] = static_cast<uint8_t>(c & 0xFF);
            m_high_color[1] = static_cast<uint8_t>((c >> 8) & 0xFF);
        }

        inline bool is_constant_color_block() const
        {
            return get_low_color() == get_high_color();
        }
        inline bool is_alpha_block() const
        {
            return get_low_color() <= get_high_color();
        }
        inline bool is_non_alpha_block() const
        {
            return !is_alpha_block();
        }

        inline uint32_t get_selector(uint32_t x, uint32_t y) const
        {
            VOGL_ASSERT((x < 4U) && (y < 4U));
            return (m_selectors[y] >> (x * cDXT1SelectorBits)) & cDXT1SelectorMask;
        }

        inline void set_selector(uint32_t x, uint32_t y, uint32_t val)
        {
            VOGL_ASSERT((x < 4U) && (y < 4U) && (val < 4U));

            m_selectors[y] &= (~(cDXT1SelectorMask << (x * cDXT1SelectorBits)));
            m_selectors[y] |= (val << (x * cDXT1SelectorBits));
        }

        inline void flip_x(uint32_t w = 4, uint32_t h = 4)
        {
            for (uint32_t x = 0; x < (w / 2); x++)
            {
                for (uint32_t y = 0; y < h; y++)
                {
                    const uint32_t c = get_selector(x, y);
                    set_selector(x, y, get_selector((w - 1) - x, y));
                    set_selector((w - 1) - x, y, c);
                }
            }
        }

        inline void flip_y(uint32_t w = 4, uint32_t h = 4)
        {
            for (uint32_t y = 0; y < (h / 2); y++)
            {
                for (uint32_t x = 0; x < w; x++)
                {
                    const uint32_t c = get_selector(x, y);
                    set_selector(x, y, get_selector(x, (h - 1) - y));
                    set_selector(x, (h - 1) - y, c);
                }
            }
        }

        static uint16_t pack_color(const color_quad_u8 &color, bool scaled, uint32_t bias = 127U);
        static uint16_t pack_color(uint32_t r, uint32_t g, uint32_t b, bool scaled, uint32_t bias = 127U);

        static color_quad_u8 unpack_color(uint16_t packed_color, bool scaled, uint32_t alpha = 255U);
        static void unpack_color(uint32_t &r, uint32_t &g, uint32_t &b, uint16_t packed_color, bool scaled);

        static uint32_t get_block_colors3(color_quad_u8 *pDst, uint16_t color0, uint16_t color1);
        static uint32_t get_block_colors3_round(color_quad_u8 *pDst, uint16_t color0, uint16_t color1);

        static uint32_t get_block_colors4(color_quad_u8 *pDst, uint16_t color0, uint16_t color1);
        static uint32_t get_block_colors4_round(color_quad_u8 *pDst, uint16_t color0, uint16_t color1);

        // pDst must point to an array at least cDXT1SelectorValues long.
        static uint32_t get_block_colors(color_quad_u8 *pDst, uint16_t color0, uint16_t color1);

        static uint32_t get_block_colors_round(color_quad_u8 *pDst, uint16_t color0, uint16_t color1);

        static color_quad_u8 unpack_endpoint(uint32_t endpoints, uint32_t index, bool scaled, uint32_t alpha = 255U);
        static uint32_t pack_endpoints(uint32_t lo, uint32_t hi);

        static void get_block_colors_NV5x(color_quad_u8 *pDst, uint16_t packed_col0, uint16_t packed_col1, bool color4);
    };

    VOGL_DEFINE_BITWISE_COPYABLE(dxt1_block);

    struct dxt3_block
    {
        enum
        {
            cNumAlphaBytes = 8
        };
        uint8_t m_alpha[cNumAlphaBytes];

        void set_alpha(uint32_t x, uint32_t y, uint32_t value, bool scaled);
        uint32_t get_alpha(uint32_t x, uint32_t y, bool scaled) const;

        inline void flip_x(uint32_t w = 4, uint32_t h = 4)
        {
            for (uint32_t x = 0; x < (w / 2); x++)
            {
                for (uint32_t y = 0; y < h; y++)
                {
                    const uint32_t c = get_alpha(x, y, false);
                    set_alpha(x, y, get_alpha((w - 1) - x, y, false), false);
                    set_alpha((w - 1) - x, y, c, false);
                }
            }
        }

        inline void flip_y(uint32_t w = 4, uint32_t h = 4)
        {
            for (uint32_t y = 0; y < (h / 2); y++)
            {
                for (uint32_t x = 0; x < w; x++)
                {
                    const uint32_t c = get_alpha(x, y, false);
                    set_alpha(x, y, get_alpha(x, (h - 1) - y, false), false);
                    set_alpha(x, (h - 1) - y, c, false);
                }
            }
        }
    };

    VOGL_DEFINE_BITWISE_COPYABLE(dxt3_block);

    struct dxt5_block
    {
        uint8_t m_endpoints[2];

        enum
        {
            cNumSelectorBytes = 6
        };
        uint8_t m_selectors[cNumSelectorBytes];

        inline void clear()
        {
            utils::zero_this(this);
        }

        inline uint32_t get_low_alpha() const
        {
            return m_endpoints[0];
        }

        inline uint32_t get_high_alpha() const
        {
            return m_endpoints[1];
        }

        inline void set_low_alpha(uint32_t i)
        {
            VOGL_ASSERT(i <= cUINT8_MAX);
            m_endpoints[0] = static_cast<uint8_t>(i);
        }

        inline void set_high_alpha(uint32_t i)
        {
            VOGL_ASSERT(i <= cUINT8_MAX);
            m_endpoints[1] = static_cast<uint8_t>(i);
        }

        inline bool is_alpha6_block() const
        {
            return get_low_alpha() <= get_high_alpha();
        }

        uint32_t get_endpoints_as_word() const
        {
            return m_endpoints[0] | (m_endpoints[1] << 8);
        }
        uint32_t get_selectors_as_word(uint32_t index)
        {
            VOGL_ASSERT(index < 3);
            return m_selectors[index * 2] | (m_selectors[index * 2 + 1] << 8);
        }

        inline uint32_t get_selector(uint32_t x, uint32_t y) const
        {
            VOGL_ASSERT((x < 4U) && (y < 4U));

            uint32_t selector_index = (y * 4) + x;
            uint32_t bit_index = selector_index * cDXT5SelectorBits;

            uint32_t byte_index = bit_index >> 3;
            uint32_t bit_ofs = bit_index & 7;

            uint32_t v = m_selectors[byte_index];
            if (byte_index < (cNumSelectorBytes - 1))
                v |= (m_selectors[byte_index + 1] << 8);

            return (v >> bit_ofs) & 7;
        }

        inline void set_selector(uint32_t x, uint32_t y, uint32_t val)
        {
            VOGL_ASSERT((x < 4U) && (y < 4U) && (val < 8U));

            uint32_t selector_index = (y * 4) + x;
            uint32_t bit_index = selector_index * cDXT5SelectorBits;

            uint32_t byte_index = bit_index >> 3;
            uint32_t bit_ofs = bit_index & 7;

            uint32_t v = m_selectors[byte_index];
            if (byte_index < (cNumSelectorBytes - 1))
                v |= (m_selectors[byte_index + 1] << 8);

            v &= (~(7 << bit_ofs));
            v |= (val << bit_ofs);

            m_selectors[byte_index] = static_cast<uint8_t>(v);
            if (byte_index < (cNumSelectorBytes - 1))
                m_selectors[byte_index + 1] = static_cast<uint8_t>(v >> 8);
        }

        inline void flip_x(uint32_t w = 4, uint32_t h = 4)
        {
            for (uint32_t x = 0; x < (w / 2); x++)
            {
                for (uint32_t y = 0; y < h; y++)
                {
                    const uint32_t c = get_selector(x, y);
                    set_selector(x, y, get_selector((w - 1) - x, y));
                    set_selector((w - 1) - x, y, c);
                }
            }
        }

        inline void flip_y(uint32_t w = 4, uint32_t h = 4)
        {
            for (uint32_t y = 0; y < (h / 2); y++)
            {
                for (uint32_t x = 0; x < w; x++)
                {
                    const uint32_t c = get_selector(x, y);
                    set_selector(x, y, get_selector(x, (h - 1) - y));
                    set_selector(x, (h - 1) - y, c);
                }
            }
        }

        enum
        {
            cMaxSelectorValues = 8
        };

        // Results written to alpha channel.
        static uint32_t get_block_values6(color_quad_u8 *pDst, uint32_t l, uint32_t h);
        static uint32_t get_block_values8(color_quad_u8 *pDst, uint32_t l, uint32_t h);
        static uint32_t get_block_values(color_quad_u8 *pDst, uint32_t l, uint32_t h);

        static uint32_t get_block_values6(uint32_t *pDst, uint32_t l, uint32_t h);
        static uint32_t get_block_values8(uint32_t *pDst, uint32_t l, uint32_t h);
        // pDst must point to an array at least cDXT5SelectorValues long.
        static uint32_t get_block_values(uint32_t *pDst, uint32_t l, uint32_t h);

        static uint32_t unpack_endpoint(uint32_t packed, uint32_t index);
        static uint32_t pack_endpoints(uint32_t lo, uint32_t hi);
    };

    VOGL_DEFINE_BITWISE_COPYABLE(dxt5_block);

    struct dxt_pixel_block
    {
        color_quad_u8 m_pixels[cDXTBlockSize][cDXTBlockSize]; // [y][x]

        inline void clear()
        {
            utils::zero_object(*this);
        }
    };

    VOGL_DEFINE_BITWISE_COPYABLE(dxt_pixel_block);

} // namespace vogl
