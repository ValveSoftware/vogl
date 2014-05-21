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

// File: vogl_framebuffer_capturer.cpp
#include "vogl_framebuffer_capturer.h"

vogl_framebuffer_capturer::vogl_framebuffer_capturer()
    : m_initialized(false),
      m_did_any_write_fail(false),
      m_pWrite_func(NULL),
      m_pWrite_opaque(NULL),
      m_pixel_format(GL_NONE),
      m_pixel_type(GL_NONE),
      m_num_pbos(0),
      m_pbo_head(0),
      m_pbo_tail(0),
      m_num_busy_pbos(0),
      m_cur_width(0),
      m_cur_height(0)
{
    VOGL_FUNC_TRACER
}

vogl_framebuffer_capturer::~vogl_framebuffer_capturer()
{
    VOGL_FUNC_TRACER

    // Purposely DO NOT deinit, that's the user's problem, because deinit() requires a current context.
    //deinit(false);

    if (m_initialized)
    {
        vogl_warning_printf("vogl_framebuffer_capturer being destroyed while still initialized\n");
    }
}

bool vogl_framebuffer_capturer::init(uint32_t num_buffers, vogl_write_image_callback_func_ptr pWrite_func, void *pWrite_opaque, GLenum pixel_format, GLenum pixel_type)
{
    VOGL_FUNC_TRACER

    deinit(true);

    if (num_buffers > cMaxBufs)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    m_num_pbos = num_buffers;
    m_pWrite_func = pWrite_func;
    m_pWrite_opaque = pWrite_opaque;
    m_pixel_format = pixel_format;
    m_pixel_type = pixel_type;

    m_initialized = true;

    return true;
}

void vogl_framebuffer_capturer::deinit(bool ok_to_make_gl_calls)
{
    VOGL_FUNC_TRACER

    if (!m_initialized)
        return;

    if (ok_to_make_gl_calls)
    {
        flush();

        delete_all_bufs();
    }

    m_initialized = false;
    m_did_any_write_fail = false;

    m_pWrite_func = NULL;
    m_pWrite_opaque = NULL;

    m_pixel_format = GL_NONE;
    m_pixel_type = GL_NONE;

    m_num_pbos = 0;
    m_pbo_head = 0;
    m_pbo_tail = 0;
    m_num_busy_pbos = 0;
    for (uint32_t i = 0; i < cMaxBufs; i++)
        m_pbos[i].clear();

    m_cur_width = 0;
    m_cur_height = 0;
}

bool vogl_framebuffer_capturer::flush_pbo(pbo &buf)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(buf.m_buffer);

    if ((!buf.m_buffer) || (!buf.m_busy))
        return false;

    // Set it to not busy here, in case ctrl+c is hit while we're busy flushing the buffer
    // TODO: Make this atomic?
    buf.m_busy = false;

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_binding_state binding_saver(GL_PIXEL_PACK_BUFFER);

    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_PACK_BUFFER, buf.m_buffer);

    VOGL_CHECK_GL_ERROR;

    const void *pData = GL_ENTRYPOINT(glMapBuffer)(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

    VOGL_CHECK_GL_ERROR;

    if (!pData)
    {
        vogl_error_printf("Unable to map pixel pack buffer %u\n", buf.m_buffer);
    }
    else
    {
        if (m_pWrite_func)
        {
            bool success = (*m_pWrite_func)(buf.m_width, buf.m_height, buf.m_pitch, buf.m_size, m_pixel_format, m_pixel_type, pData, m_pWrite_opaque, buf.m_frame_index);
            if (!success)
                m_did_any_write_fail = true;
        }

        GL_ENTRYPOINT(glUnmapBuffer)(GL_PIXEL_PACK_BUFFER);

        VOGL_CHECK_GL_ERROR;
    }

    return true;
}

bool vogl_framebuffer_capturer::flush()
{
    VOGL_FUNC_TRACER

    if (!m_initialized)
        return false;

    while (m_num_busy_pbos)
    {
        pbo &tail = m_pbos[m_pbo_tail];
        if (!flush_pbo(tail))
            return false;

        m_pbo_tail = (m_pbo_tail + 1) % m_num_pbos;
        m_num_busy_pbos--;
    }

    VOGL_ASSERT(m_pbo_tail == m_pbo_head);
    return true;
}

void vogl_framebuffer_capturer::delete_all_bufs()
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;

    for (uint32_t i = 0; i < m_num_pbos; i++)
    {
        pbo &buf = m_pbos[i];
        if (!buf.m_buffer)
            continue;

        GL_ENTRYPOINT(glDeleteBuffers)(1, &buf.m_buffer);
        buf.clear();
    }

    m_num_busy_pbos = 0;

    VOGL_CHECK_GL_ERROR;
}

bool vogl_framebuffer_capturer::recreate_buffers(uint32_t new_width, uint32_t new_height)
{
    VOGL_FUNC_TRACER

    if (!flush())
        return false;

    VOGL_CHECK_GL_ERROR;

    delete_all_bufs();

    m_cur_width = new_width;
    m_cur_height = new_height;

    vogl_scoped_state_saver state_saver(cGSTPixelStore);
    vogl_scoped_binding_state binding_saver(GL_PIXEL_PACK_BUFFER);

    if (vogl_check_gl_error())
        return false;

    vogl_reset_pixel_store_states();

    size_t pitch = vogl_get_image_size(m_pixel_format, m_pixel_type, m_cur_width, 1, 1);
    size_t total_size = vogl_get_image_size(m_pixel_format, m_pixel_type, m_cur_width, m_cur_height, 1);

    if (vogl_check_gl_error())
        return false;

    for (uint32_t i = 0; i < m_num_pbos; i++)
    {
        pbo &buf = m_pbos[i];
        buf.m_width = m_cur_width;
        buf.m_height = m_cur_height;
        buf.m_pitch = (uint32_t)pitch;
        buf.m_size = total_size;
        buf.m_busy = false;

        GL_ENTRYPOINT(glGenBuffers)(1, &buf.m_buffer);
        if (vogl_check_gl_error())
            return false;

        GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_PACK_BUFFER, buf.m_buffer);
        if (vogl_check_gl_error())
            return false;

        GL_ENTRYPOINT(glBufferData)(GL_PIXEL_PACK_BUFFER, total_size, NULL, GL_STREAM_READ);
        if (vogl_check_gl_error())
            return false;
    }

    return true;
}

bool vogl_framebuffer_capturer::capture(uint32_t width, uint32_t height, GLuint framebuffer, GLuint read_buffer, uint64_t frame_index)
{
    VOGL_FUNC_TRACER

    if (!m_initialized)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((!width) || (!height))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    VOGL_CHECK_GL_ERROR;

    if ((width != m_cur_width) || (height != m_cur_height))
    {
        if (!recreate_buffers(width, height))
            return false;
    }

    if (m_num_busy_pbos == m_num_pbos)
    {
        pbo &buf_to_flush = m_pbos[m_pbo_tail];
        VOGL_ASSERT(buf_to_flush.m_busy);

        if (buf_to_flush.m_busy)
        {
            if (!flush_pbo(buf_to_flush))
                return false;
        }

        m_pbo_tail = (m_pbo_tail + 1) % m_num_pbos;
        m_num_busy_pbos--;
    }

    pbo &buf_to_read = m_pbos[m_pbo_head];
    VOGL_ASSERT(!buf_to_read.m_busy);

    if (!buf_to_read.m_busy)
    {
        if (!vogl_copy_buffer_to_image(NULL, 0, m_cur_width, m_cur_height, m_pixel_format, m_pixel_type, false, framebuffer, read_buffer, buf_to_read.m_buffer))
            return false;

        buf_to_read.m_frame_index = frame_index;
        buf_to_read.m_busy = true;

        m_pbo_head = (m_pbo_head + 1) % m_num_pbos;
        m_num_busy_pbos++;
    }

    return true;
}

vogl::vector<GLuint> vogl_framebuffer_capturer::get_buffer_handles() const
{
    VOGL_FUNC_TRACER

    vogl::vector<GLuint> res;

    if (m_initialized)
    {
        for (uint32_t i = 0; i < cMaxBufs; i++)
        {
            if (m_pbos[i].m_buffer)
                res.push_back(m_pbos[i].m_buffer);
        }
    }

    return res;
}
