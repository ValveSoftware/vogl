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

// File: vogl_default_framebuffer_state.cpp
#include "vogl_default_framebuffer_state.h"

#include "vogl_console.h"
#include "vogl_data_stream_serializer.h"
#include "vogl_dynamic_stream.h"

#include "vogl_common.h"
#include "vogl_texture_format.h"

static const GLenum g_def_framebuffer_enums[] =
{
    GL_FRONT_LEFT,
    GL_BACK_LEFT,
    GL_FRONT_RIGHT,
    GL_BACK_RIGHT,
    0
};

bool vogl_get_default_framebuffer_attribs(vogl_default_framebuffer_attribs &attribs, uint32_t screen)
{
    #if (VOGL_PLATFORM_HAS_GLX)
        GLXDrawable pDrawable = GL_ENTRYPOINT(glXGetCurrentDrawable)();
        Display *pDisplay = GL_ENTRYPOINT(glXGetCurrentDisplay)();
        if ((!pDrawable) || (!pDisplay))
            return false;

        GLint red_size = 8, green_size = 8, blue_size = 8, alpha_size = 8;
        GLint doublebuffer = 1, stereo = 0, depth_size = 24, stencil_size = 8;
        GLint drawable_type = GLX_WINDOW_BIT;
        GLint render_type = GLX_RGBA_BIT;
        GLint samples = 0, sample_buffers = 0;

        GL_ENTRYPOINT(glGetIntegerv)(GL_RED_BITS, &red_size);
        GL_ENTRYPOINT(glGetIntegerv)(GL_GREEN_BITS, &green_size);
        GL_ENTRYPOINT(glGetIntegerv)(GL_BLUE_BITS, &blue_size);
        GL_ENTRYPOINT(glGetIntegerv)(GL_ALPHA_BITS, &alpha_size);
        GL_ENTRYPOINT(glGetIntegerv)(GL_DOUBLEBUFFER, &doublebuffer);
        GL_ENTRYPOINT(glGetIntegerv)(GL_STEREO, &stereo);
        GL_ENTRYPOINT(glGetIntegerv)(GL_DEPTH_BITS, &depth_size);
        GL_ENTRYPOINT(glGetIntegerv)(GL_STENCIL_BITS, &stencil_size);
        GL_ENTRYPOINT(glGetIntegerv)(GL_SAMPLES, &samples);
        GL_ENTRYPOINT(glGetIntegerv)(GL_SAMPLE_BUFFERS, &sample_buffers);

        attribs.m_r_size = red_size;
        attribs.m_g_size = green_size;
        attribs.m_b_size = blue_size;
        attribs.m_a_size = alpha_size;
        attribs.m_depth_size = depth_size;
        attribs.m_stencil_size = stencil_size;
        attribs.m_samples = samples;
        attribs.m_double_buffered = (doublebuffer != 0);

        //GL_ENTRYPOINT(glXQueryDrawable)(pDisplay, pDrawable, GLX_WIDTH, &attribs.m_width);
        //GL_ENTRYPOINT(glXQueryDrawable)(pDisplay, pDrawable, GLX_HEIGHT, &attribs.m_height);
        XWindowAttributes attrs;
        XGetWindowAttributes(pDisplay, pDrawable, &attrs);
        attribs.m_width = attrs.width;
        attribs.m_height = attrs.height;

        return true;

	#elif (VOGL_PLATFORM_HAS_CGL)
        VOGL_ASSERT(!"UNIMPLEMENTED vogl_get_default_framebuffer_attribs");
		return false;

	#elif (VOGL_PLATFORM_HAS_WGL)
        HDC hDeviceContext = GL_ENTRYPOINT(wglGetCurrentDC)();
        if (hDeviceContext == NULL)
            return false;

        HWND hWindow = WindowFromDC(hDeviceContext);
        if (hWindow == NULL)
        {
            VOGL_ASSERT(!"No window available");
            return false;
        }

        int pixelFormatIndex = GL_ENTRYPOINT(wglGetPixelFormat)(hDeviceContext);
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
        int result = GL_ENTRYPOINT(wglDescribePixelFormat)(hDeviceContext, pixelFormatIndex, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
        if (result == 0)
        {
            VOGL_ASSERT(!"wglDescribePixelFormat failed");
            return false;
        }

        attribs.m_r_size = pfd.cRedBits;
        attribs.m_g_size = pfd.cGreenBits;
        attribs.m_b_size = pfd.cBlueBits;
        attribs.m_a_size = pfd.cAlphaBits;
        attribs.m_depth_size = pfd.cDepthBits;
        attribs.m_stencil_size = pfd.cStencilBits;
        attribs.m_samples = 0;
        attribs.m_double_buffered = (pfd.dwFlags & PFD_DOUBLEBUFFER) == PFD_DOUBLEBUFFER;

        RECT clientRect;
        if (GetClientRect(hWindow, &clientRect))
        {
            attribs.m_width = clientRect.right - clientRect.left;
            attribs.m_height = clientRect.bottom - clientRect.top;
        }

        return true;
    #else
		#error "Implement vogl_get_default_framebuffer_attribs for this platform."
        return(false);
    #endif
}

vogl_default_framebuffer_state::vogl_default_framebuffer_state() :
    m_valid(false)
{
}

vogl_default_framebuffer_state::~vogl_default_framebuffer_state()
{
}

bool vogl_default_framebuffer_state::snapshot(const vogl_context_info &context_info, const vogl_default_framebuffer_attribs &fb_attribs)
{
    VOGL_NOTE_UNUSED(context_info);

    clear();

    m_fb_attribs = fb_attribs;

    // Create compatible GL texture
    // Attach this texture to an FBO
    // Blit default framebuffer to this FBO
    // Capture this texture's state

    vogl_scoped_state_saver framebuffer_state_saver(cGSTReadBuffer, cGSTDrawBuffer);

    vogl_scoped_binding_state orig_framebuffers(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER, GL_TEXTURE_2D, GL_TEXTURE_2D_MULTISAMPLE);

    GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    vogl_scoped_binding_state orig_bindings(GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER);

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_PACK_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_UNPACK_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    vogl_scoped_state_saver pixelstore_state_saver(cGSTPixelStore);

    vogl_scoped_state_saver pixeltransfer_state_saver;
    if (!context_info.is_core_profile())
        pixeltransfer_state_saver.save(cGSTPixelTransfer);

    vogl_reset_pixel_store_states();
    if (!context_info.is_core_profile())
        vogl_reset_pixel_transfer_states(context_info);

    // TODO: Test multisampled default framebuffers
    const GLenum tex_target = (fb_attribs.m_samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    for (uint32_t i = 0; i < cDefFramebufferTotal; i++)
    {
        GLenum internal_fmt, pixel_fmt, pixel_type;

        // TODO: This uses fixed pixel formats, and assumes there's always a depth/stencil buffer.
        if (i == cDefFramebufferDepthStencil)
        {
            if ((fb_attribs.m_depth_size + fb_attribs.m_stencil_size) == 0)
                continue;

            GL_ENTRYPOINT(glReadBuffer)(fb_attribs.m_double_buffered ? GL_BACK_LEFT : GL_FRONT_LEFT);

            internal_fmt = GL_DEPTH_STENCIL;
            pixel_fmt = GL_DEPTH_STENCIL;
            pixel_type = GL_UNSIGNED_INT_24_8;
        }
        else
        {
            if ((fb_attribs.m_r_size + fb_attribs.m_g_size + fb_attribs.m_b_size + fb_attribs.m_a_size) == 0)
                continue;

            GL_ENTRYPOINT(glReadBuffer)(g_def_framebuffer_enums[i]);

            internal_fmt = GL_RGBA;
            pixel_fmt = GL_RGBA;
            pixel_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        }

        if (vogl_check_gl_error_internal(true))
            continue;

        // Create texture
        GLuint tex_handle = 0;
        GL_ENTRYPOINT(glGenTextures)(1, &tex_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindTexture)(tex_target, tex_handle);
        VOGL_CHECK_GL_ERROR;

        if (fb_attribs.m_samples > 1)
        {
            GL_ENTRYPOINT(glTexImage2DMultisample)(tex_target,
                fb_attribs.m_samples,
                internal_fmt,
                fb_attribs.m_width,
                fb_attribs.m_height,
                GL_TRUE);
        }
        else
        {
            GL_ENTRYPOINT(glTexImage2D)(tex_target,
                0,
                internal_fmt,
                fb_attribs.m_width,
                fb_attribs.m_height,
                0,
                pixel_fmt,
                pixel_type,
                NULL);
        }

        if (vogl_check_gl_error_internal())
        {
            GL_ENTRYPOINT(glBindTexture)(tex_target, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDeleteTextures)(1, &tex_handle);
            VOGL_CHECK_GL_ERROR;

            continue;
        }

        GL_ENTRYPOINT(glTexParameteri)(tex_target, GL_TEXTURE_MAX_LEVEL, 0);
        VOGL_CHECK_GL_ERROR;

        // Create FBO
        GLuint fbo_handle = 0;
        GL_ENTRYPOINT(glGenFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, (i == cDefFramebufferDepthStencil) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_COLOR_ATTACHMENT0, tex_target, tex_handle, 0);
        VOGL_CHECK_GL_ERROR;

        GLenum draw_buf = (i == cDefFramebufferDepthStencil) ? GL_NONE : GL_COLOR_ATTACHMENT0;
        GL_ENTRYPOINT(glDrawBuffers)(1, &draw_buf);
        VOGL_CHECK_GL_ERROR;

        GLenum cur_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
        VOGL_CHECK_GL_ERROR;

        bool status = true;

        if (cur_status == GL_FRAMEBUFFER_COMPLETE)
        {
            GL_ENTRYPOINT(glBlitFramebuffer)(
                0, 0, fb_attribs.m_width, fb_attribs.m_height,
                0, 0, fb_attribs.m_width, fb_attribs.m_height,
                (i == cDefFramebufferDepthStencil) ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : GL_COLOR_BUFFER_BIT,
                GL_NEAREST);

            if (vogl_check_gl_error_internal())
            {
                status = false;
            }
        }

        if (status)
        {
            vogl_handle_remapper def_handle_remapper;
            status = m_textures[i].snapshot(context_info, def_handle_remapper, tex_handle, tex_target);

            if (!status)
            {
                vogl_error_printf("Failed snapshotting texture for default framebuffer %s\n", get_gl_enums().find_gl_name(g_def_framebuffer_enums[i]));
            }
        }
        else
        {
            vogl_warning_printf("Failed blitting framebuffer %u\n", i);
        }

        // Delete FBO
        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDrawBuffer)(GL_FRONT_LEFT);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindTexture)(tex_target, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteTextures)(1, &tex_handle);
        VOGL_CHECK_GL_ERROR;
    }

    m_valid = true;

    return true;
}

bool vogl_default_framebuffer_state::restore(const vogl_context_info &context_info, bool restore_front_buffer) const
{
    VOGL_NOTE_UNUSED(context_info);

    if (!m_valid)
        return false;

    // TODO: Test multisampled default framebuffers
    // TODO: Check to ensure the stored fb is compatible with the current framebuffer
    const GLenum tex_target = (m_fb_attribs.m_samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    vogl_scoped_state_saver framebuffer_state_saver(cGSTReadBuffer, cGSTDrawBuffer);

    vogl_scoped_binding_state orig_framebuffers(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER, GL_TEXTURE_2D, GL_TEXTURE_2D_MULTISAMPLE);

    GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    for (uint32_t i = 0; i < cDefFramebufferTotal; i++)
    {
        if ((!restore_front_buffer) && ((i == cDefFramebufferFrontLeft) || (i == cDefFramebufferFrontRight)))
            continue;

        if (!m_textures[i].is_valid())
            continue;

        GL_ENTRYPOINT(glDrawBuffer)((i == cDefFramebufferDepthStencil) ? GL_NONE : g_def_framebuffer_enums[i]);
        if (vogl_check_gl_error_internal(true))
            continue;

        GLuint64 tex_handle64 = 0;

        vogl_handle_remapper def_handle_remapper;
        if (!m_textures[i].restore(context_info, def_handle_remapper, tex_handle64))
        {
            vogl_error_printf("Failed restoring texture %u\n", i);
            continue;
        }

        GLuint tex_handle = static_cast<GLuint>(tex_handle64);

        // Create FBO
        GLuint fbo_handle = 0;
        GL_ENTRYPOINT(glGenFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glFramebufferTexture2D)(GL_READ_FRAMEBUFFER, (i == cDefFramebufferDepthStencil) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_COLOR_ATTACHMENT0, tex_target, tex_handle, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glReadBuffer)((i == cDefFramebufferDepthStencil) ? GL_NONE : GL_COLOR_ATTACHMENT0);
        VOGL_CHECK_GL_ERROR;

        GLenum cur_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_READ_FRAMEBUFFER);
        VOGL_CHECK_GL_ERROR;

        bool status = true;

        if (cur_status == GL_FRAMEBUFFER_COMPLETE)
        {
            GL_ENTRYPOINT(glBlitFramebuffer)(
                0, 0, m_fb_attribs.m_width, m_fb_attribs.m_height,
                0, 0, m_fb_attribs.m_width, m_fb_attribs.m_height,
                (i == cDefFramebufferDepthStencil) ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : GL_COLOR_BUFFER_BIT,
                GL_NEAREST);

            if (vogl_check_gl_error_internal())
            {
                status = false;
            }
        }

        if (!status)
        {
            vogl_warning_printf("Failed blitting framebuffer %u\n", i);
        }

        // Delete FBO
        GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteFramebuffers)(1, &fbo_handle);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glReadBuffer)(GL_FRONT_LEFT);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindTexture)(tex_target, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteTextures)(1, &tex_handle);
        VOGL_CHECK_GL_ERROR;
    }

    return true;
}

void vogl_default_framebuffer_state::clear()
{
    m_fb_attribs.clear();

    for (uint32_t i = 0; i < cDefFramebufferTotal; i++)
        m_textures[i].clear();

    m_valid = false;
}

bool vogl_default_framebuffer_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    if (!m_valid)
        return false;

    if (!m_fb_attribs.serialize(node.add_object("attribs")))
        return false;

    json_node &framebuffers_array = node.add_array("framebuffers");
    for (uint32_t i = 0; i < cDefFramebufferTotal; i++)
    {
        json_node &object_node = framebuffers_array.add_object();
        if (m_textures[i].is_valid())
        {
            if (!m_textures[i].serialize(object_node, blob_manager))
                return false;
        }
    }

    return true;
}

bool vogl_default_framebuffer_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    clear();

    if (!node.has_object("attribs"))
        return false;

    if (!m_fb_attribs.deserialize(*node.find_child_object("attribs")))
        return false;

    const json_node *pFramebuffers_array = node.find_child_array("framebuffers");
    if (pFramebuffers_array)
    {
        for (uint32_t i = 0; i < math::minimum<uint32_t>(cDefFramebufferTotal, pFramebuffers_array->size()); i++)
        {
            if ((pFramebuffers_array->is_child_object(i)) && (pFramebuffers_array->get_child(i)->size()))
            {
                if (!m_textures[i].deserialize(*pFramebuffers_array->get_child(i), blob_manager))
                    return false;
            }
        }
    }

    m_valid = true;

    return true;
}

