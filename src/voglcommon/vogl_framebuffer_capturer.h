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

// File: vogl_framebuffer_capturer.h
#ifndef VOGL_FRAMEBUFFER_CAPTURER_H
#define VOGL_FRAMEBUFFER_CAPTURER_H

#include "vogl_common.h"
#include "vogl_dynamic_string.h"

typedef bool (*vogl_write_image_callback_func_ptr)(uint32_t width, uint32_t height, uint32_t pitch, size_t size, GLenum pixel_format, GLenum pixel_type, const void *pImage, void *pOpaque, uint64_t frame_index);

class vogl_framebuffer_capturer
{
public:
    enum
    {
        cMaxBufs = 4
    };

    vogl_framebuffer_capturer();

    ~vogl_framebuffer_capturer();

    bool init(uint32_t num_buffers, vogl_write_image_callback_func_ptr pWrite_func, void *pWrite_opaque, GLenum pixel_format, GLenum pixel_type);
    void deinit(bool ok_to_make_gl_calls);

    bool is_initialized() const
    {
        return m_initialized;
    }

    uint32_t get_num_buffers() const
    {
        return m_num_pbos;
    }
    uint32_t get_num_busy_buffers() const
    {
        return m_num_busy_pbos;
    }

    bool did_any_write_fail() const
    {
        return m_did_any_write_fail;
    }
    void clear_did_any_write_fail_flag()
    {
        m_did_any_write_fail = false;
    }

    bool flush();

    bool capture(uint32_t width, uint32_t height, GLuint framebuffer = 0, GLuint read_buffer = GL_BACK, uint64_t frame_index = 0);

    vogl::vector<GLuint> get_buffer_handles() const;

private:
    bool m_initialized;
    bool m_did_any_write_fail;

    vogl_write_image_callback_func_ptr m_pWrite_func;
    void *m_pWrite_opaque;

    GLenum m_pixel_format;
    GLenum m_pixel_type;

    struct pbo
    {
        pbo()
            : m_width(0), m_height(0), m_pitch(0), m_buffer(0), m_busy(false)
        {
        }

        void clear()
        {
            utils::zero_object(*this);
        }

        uint64_t m_frame_index;
        uint32_t m_width;
        uint32_t m_height;
        uint32_t m_pitch;
        size_t m_size;
        GLuint m_buffer;
        bool m_busy;
    };

    uint32_t m_num_pbos;
    uint32_t m_pbo_head;
    uint32_t m_pbo_tail;
    uint32_t m_num_busy_pbos;
    pbo m_pbos[cMaxBufs];

    uint32_t m_cur_width;
    uint32_t m_cur_height;

    bool flush_pbo(pbo &buf);
    void delete_all_bufs();
    bool recreate_buffers(uint32_t new_width, uint32_t new_height);
};

#endif // VOGL_FRAMEBUFFER_CAPTURER_H
