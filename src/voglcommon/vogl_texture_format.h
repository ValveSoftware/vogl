/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
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

// File: vogl_texture_format.h
#ifndef VOGL_TEXTURE_FORMAT_H
#define VOGL_TEXTURE_FORMAT_H

// Corresponds to glTexImage1D/2D/3D/2DMultisample/3DMultisample API calls
enum vogl_tex_image_types
{
    cTIT1D,
    cTIT2D,
    cTIT3D,
    cTIT2DMultisample,
    cTIT3DMultisample,
    cTITTotalTypes
};

enum vogl_tex_components
{
    cTCRed,
    cTCGreen,
    cTCBlue,
    cTCAlpha,
    cTCDepth,
    cTCStencil,
    cTCIntensity,
    cTCLuminance,
    cTCTotalComponents
};

struct vogl_internal_tex_format
{
    GLenum m_fmt;
    GLenum m_actual_internal_fmt;
    dynamic_string m_name;
    int m_comp_sizes[cTCTotalComponents];
    GLenum m_comp_types[cTCTotalComponents];
    int m_shared_size;

    // Bitmask of vogl_tex_image_types's - indicates which glTexImage API's are valid to use with this internal format.
    uint32_t m_tex_image_flags;

    GLenum m_optimum_get_image_fmt;
    GLenum m_optimum_get_image_type;
    // 0 for compressed formats, otherwise this is the number of bytes per pixel written by the get_fmt/get_type (NOT the internal format)
    uint8_t m_image_bytes_per_pixel_or_block;
    uint8_t m_block_width;
    uint8_t m_block_height;
    bool m_compressed;

    vogl_internal_tex_format()
    {
        VOGL_FUNC_TRACER utils::zero_object(*this);
    }

    vogl_internal_tex_format(
        GLenum fmt, const char *pName, GLenum actual_fmt,
        int s0, int s1, int s2, int s3, int s4, int s5, int s6, int s7,
        GLenum t0, GLenum t1, GLenum t2, GLenum t3, GLenum t4, GLenum t5, GLenum t6, GLenum t7,
        int shared_size, int tex_storage_type_flags, bool compressed, GLenum optimum_get_fmt, GLenum optimum_get_type,
        uint32_t image_bytes_per_pixel_or_block, uint32_t block_width, uint32_t block_height);

    inline uint32_t get_max_comp_size() const
    {
        uint32_t n = 0;
        for (uint32_t i = 0; i < cTCTotalComponents; i++)
            n = math::maximum<uint32_t>(n, m_comp_sizes[i]);
        return n;
    }
    inline uint32_t get_total_comps() const
    {
        uint32_t n = 0;
        for (uint32_t i = 0; i < cTCTotalComponents; i++)
            if (m_comp_sizes[i])
                n++;
        return n;
    }
    inline GLenum get_first_comp_type() const
    {
        for (uint32_t i = 0; i < cTCTotalComponents; i++)
            if (m_comp_sizes[i])
                return m_comp_types[i];
        return GL_NONE;
    }

    // Deep equality comparison
    bool compare(const vogl_internal_tex_format &rhs) const;

    // Shallow sort comparisons
    inline bool operator==(const vogl_internal_tex_format &rhs) const
    {
        return m_name.compare(rhs.m_name, true) == 0;
    }
    inline bool operator<(const vogl_internal_tex_format &rhs) const
    {
        return m_name.compare(rhs.m_name, true) < 0;
    }
};

void vogl_devel_dump_internal_texture_formats(const vogl_context_info &context_info);

void vogl_texture_format_init();
const vogl_internal_tex_format *vogl_find_internal_texture_format(GLenum internal_format);

#endif // VOGL_TEXTURE_FORMAT_H
