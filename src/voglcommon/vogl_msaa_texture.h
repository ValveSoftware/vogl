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

// File: vogl_msaa_texture.h

#ifndef VOGL_MSAA_TEXTURE_H
#define VOGL_MSAA_TEXTURE_H

#include "vogl_core.h"
#include "vogl_ktx_texture.h"
#include "vogl_context_info.h"
#include "vogl_gl_utils.h"
#include "vogl_shader_utils.h"

class vogl_msaa_texture_splitter
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_msaa_texture_splitter);

public:
    vogl_msaa_texture_splitter();
    ~vogl_msaa_texture_splitter();

    bool init();
    void deinit();

    bool is_valid() const { return m_valid; }

    // Splits an MSAA texture to 1 or more non-MSAA textures.
    // One multisampled source, multiple non-multisampled destinations created by method.
    bool split(GLenum src_target, GLuint src_texture, vogl::vector<GLuint> &dest_textures);

    // Copies texels from a non-MSAA texture into a single sample of a MSAA texture.
    // Multiple sources, one multisampled destination. Caller creates destination.
    bool combine(GLuint src_texture, uint32_t dest_sample_index, GLenum dest_target, GLuint dest_texture);

    // Copies MSAA stencil samples from an MSAA 2D to 2D_ARRAY texture to a MSAA 2D or 2D_ARRAY color texture.
    bool copy_stencil_samples_to_color(GLenum target, GLuint src_texture, GLuint &dest_texture);

    // Copies MSAA color samples from an MSAA 2D to 2D_ARRAY texture to the stencil buffer of a MSAA 2D or 2D_ARRAY depth/stencil texture.
    bool copy_color_sample_to_stencil(GLuint src_texture, uint32_t dest_sample_index, GLenum dest_target, GLuint dest_texture);

private:
    vogl_context_info m_context_info;

    GLuint m_vao_handle;
    GLuint m_vertex_buffer;

    vogl_gl_context m_orig_context;
    vogl_gl_context m_work_context;
    vogl_gl_display m_cur_display;
    vogl_gl_fb_config m_cur_fb_config;
    vogl_gl_drawable m_cur_drawable;

    vogl_simple_gl_program m_read_color_program;
    vogl_simple_gl_program m_read_color_array_program;
    vogl_simple_gl_program m_read_depth_program;
    vogl_simple_gl_program m_read_depth_array_program;

    vogl_simple_gl_program m_write_color_program;
    vogl_simple_gl_program m_write_color_array_program;
    vogl_simple_gl_program m_write_depth_program;
    vogl_simple_gl_program m_write_depth_array_program;

    vogl_simple_gl_program m_const_color_program;

    vogl_simple_gl_program m_write_stencil_program;
    vogl_simple_gl_program m_write_stencil_array_program;

    bool m_valid;
};

#endif // VOGL_MSAA_TEXTURE_H
