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

// File: vogl_renderbuffer_state.cpp
#include "vogl_common.h"
#include "vogl_renderbuffer_state.h"
#include "vogl_texture_format.h"

vogl_renderbuffer_desc::vogl_renderbuffer_desc()
{
    VOGL_FUNC_TRACER

    clear();
}

vogl_renderbuffer_desc::~vogl_renderbuffer_desc()
{
    VOGL_FUNC_TRACER
}

void vogl_renderbuffer_desc::clear()
{
    VOGL_FUNC_TRACER
    m_width = 0;
    m_height = 0;
    m_samples = 0;
    m_internal_format = 0;
    m_red_size = 0;
    m_green_size = 0;
    m_blue_size = 0;
    m_alpha_size = 0;
    m_depth_size = 0;
    m_stencil_size = 0;
}

bool vogl_renderbuffer_desc::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(context_info.get_version() >= VOGL_GL_VERSION_3_0);
    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

#define GET_STATE(e, x)                                                                               \
    do                                                                                                \
    {                                                                                                 \
        GL_ENTRYPOINT(glGetRenderbufferParameteriv)(GL_RENDERBUFFER, e, reinterpret_cast<int *>(&x)); \
        VOGL_CHECK_GL_ERROR;                                                                           \
    } while (0)
    GET_STATE(GL_RENDERBUFFER_WIDTH, m_width);
    GET_STATE(GL_RENDERBUFFER_HEIGHT, m_height);
    GET_STATE(GL_RENDERBUFFER_SAMPLES, m_samples);
    GET_STATE(GL_RENDERBUFFER_INTERNAL_FORMAT, m_internal_format);
    GET_STATE(GL_RENDERBUFFER_RED_SIZE, m_red_size);
    GET_STATE(GL_RENDERBUFFER_GREEN_SIZE, m_green_size);
    GET_STATE(GL_RENDERBUFFER_BLUE_SIZE, m_blue_size);
    GET_STATE(GL_RENDERBUFFER_ALPHA_SIZE, m_alpha_size);
    GET_STATE(GL_RENDERBUFFER_DEPTH_SIZE, m_depth_size);
    GET_STATE(GL_RENDERBUFFER_STENCIL_SIZE, m_stencil_size);
#undef GET_STATE

    return true;
}

bool vogl_renderbuffer_desc::restore(const vogl_context_info &context_info) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    if (!m_width)
        return false;

    VOGL_ASSERT(context_info.get_version() >= VOGL_GL_VERSION_3_0);

    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glRenderbufferStorageMultisample)(GL_RENDERBUFFER, m_samples, m_internal_format, m_width, m_height);

    bool prev_gl_error = vogl_check_gl_error();
    VOGL_ASSERT(!prev_gl_error);

    return !prev_gl_error;
}

bool vogl_renderbuffer_desc::serialize(json_node &node) const
{
    VOGL_FUNC_TRACER

    node.add_key_value("width", m_width);
    node.add_key_value("height", m_height);
    node.add_key_value("internal_format", get_gl_enums().find_name(m_internal_format, "gl"));
    node.add_key_value("samples", m_samples);
    node.add_key_value("red_size", m_red_size);
    node.add_key_value("green_size", m_green_size);
    node.add_key_value("blue_size", m_blue_size);
    node.add_key_value("alpha_size", m_alpha_size);
    node.add_key_value("depth_size", m_depth_size);
    node.add_key_value("stencil_size", m_stencil_size);

    return true;
}

bool vogl_renderbuffer_desc::deserialize(const json_node &node)
{
    VOGL_FUNC_TRACER

    clear();

    m_width = node.value_as_int("width");
    m_height = node.value_as_int("height");
    m_samples = node.value_as_int("samples");
    m_red_size = node.value_as_int("red_size");
    m_green_size = node.value_as_int("green_size");
    m_blue_size = node.value_as_int("blue_size");
    m_alpha_size = node.value_as_int("alpha_size");
    m_depth_size = node.value_as_int("depth_size");
    m_stencil_size = node.value_as_int("stencil_size");

    const char *pFmt = node.value_as_string_ptr("internal_format");
    if (!pFmt)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    uint64_t enum_val = get_gl_enums().find_enum(pFmt);
    VOGL_ASSERT(enum_val != gl_enums::cUnknownEnum);
    if (enum_val == gl_enums::cUnknownEnum)
        return false;
    if (enum_val > cUINT32_MAX)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }
    m_internal_format = static_cast<GLenum>(enum_val);

    return true;
}

bool vogl_renderbuffer_desc::operator==(const vogl_renderbuffer_desc &rhs) const
{
    VOGL_FUNC_TRACER

#define COMP(x)     \
    if (x != rhs.x) \
        return false;
    COMP(m_width);
    COMP(m_height);
    COMP(m_samples);
    COMP(m_internal_format);
    COMP(m_red_size);
    COMP(m_green_size);
    COMP(m_blue_size);
    COMP(m_alpha_size);
    COMP(m_depth_size);
    COMP(m_stencil_size);

#undef COMP
    return true;
}

bool vogl_renderbuffer_desc::get_int(GLenum enum_val, int *pVals, uint32_t n) const
{
    VOGL_FUNC_TRACER

    int result = 0;

    if (n < 1)
        return false;

    switch (enum_val)
    {
        case GL_RENDERBUFFER_WIDTH:
        {
            result = m_width;
            break;
        }
        case GL_RENDERBUFFER_HEIGHT:
        {
            result = m_height;
            break;
        }
        case GL_RENDERBUFFER_SAMPLES:
        {
            result = m_samples;
            break;
        }
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
        {
            result = m_internal_format;
            break;
        }
        case GL_RENDERBUFFER_RED_SIZE:
        {
            result = m_red_size;
            break;
        }
        case GL_RENDERBUFFER_GREEN_SIZE:
        {
            result = m_green_size;
            break;
        }
        case GL_RENDERBUFFER_BLUE_SIZE:
        {
            result = m_blue_size;
            break;
        }
        case GL_RENDERBUFFER_ALPHA_SIZE:
        {
            result = m_alpha_size;
            break;
        }
        case GL_RENDERBUFFER_DEPTH_SIZE:
        {
            result = m_depth_size;
            break;
        }
        case GL_RENDERBUFFER_STENCIL_SIZE:
        {
            result = m_stencil_size;
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            *pVals = 0;
            return false;
        }
    }

    *pVals = result;
    return true;
}

vogl_renderbuffer_state::vogl_renderbuffer_state()
    : m_snapshot_handle(0),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_renderbuffer_state::~vogl_renderbuffer_state()
{
    VOGL_FUNC_TRACER

    clear();
}

bool vogl_renderbuffer_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_ASSERT(context_info.get_version() >= VOGL_GL_VERSION_3_0);
    VOGL_CHECK_GL_ERROR;
    VOGL_NOTE_UNUSED(target);

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);

    vogl_scoped_binding_state orig_renderbuffer(GL_RENDERBUFFER);

    GL_ENTRYPOINT(glBindRenderbuffer)(GL_RENDERBUFFER, m_snapshot_handle);
    VOGL_CHECK_GL_ERROR;

    if (!m_desc.snapshot(context_info))
        return false;

    if ((!m_desc.m_width) || (!m_desc.m_height) || (!m_desc.m_internal_format))
    {
        // Renderbuffer was only genned - no need to spit out warning
        //vogl_warning_printf("Unable to retrieve description renderbuffer %" PRIu64 "\n", static_cast<uint64_t>(handle));
    }
    else
    {
        vogl_scoped_state_saver framebuffer_state_saver(cGSTReadBuffer, cGSTDrawBuffer);
        vogl_scoped_binding_state orig_framebuffers(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER, GL_TEXTURE_2D, GL_TEXTURE_2D_MULTISAMPLE);

        const GLenum tex_target = (m_desc.m_samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

        bool capture_status = false;

        GLenum internal_fmt = m_desc.m_internal_format;
        const vogl_internal_tex_format *pInternal_tex_fmt = vogl_find_internal_texture_format(internal_fmt);
        if ((pInternal_tex_fmt) && (pInternal_tex_fmt->m_optimum_get_image_fmt != GL_NONE) && (pInternal_tex_fmt->m_optimum_get_image_type != GL_NONE))
        {
            // Create texture
            GLuint tex_handle = 0;
            GL_ENTRYPOINT(glGenTextures)(1, &tex_handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glBindTexture)(tex_target, tex_handle);
            VOGL_CHECK_GL_ERROR;

            if (m_desc.m_samples > 1)
            {
                GL_ENTRYPOINT(glTexImage2DMultisample)(tex_target,
                    m_desc.m_samples,
                    internal_fmt,
                    m_desc.m_width,
                    m_desc.m_height,
                    GL_TRUE);
            }
            else
            {
                GL_ENTRYPOINT(glTexImage2D)(tex_target,
                    0,
                    internal_fmt,
                    m_desc.m_width,
                    m_desc.m_height,
                    0,
                    pInternal_tex_fmt->m_optimum_get_image_fmt,
                    pInternal_tex_fmt->m_optimum_get_image_type,
                    NULL);
            }

            if (!vogl_check_gl_error_internal())
            {
                GL_ENTRYPOINT(glTexParameteri)(tex_target, GL_TEXTURE_MAX_LEVEL, 0);
                VOGL_CHECK_GL_ERROR;

                GLenum attachment = GL_COLOR_ATTACHMENT0;
                GLenum draw_and_read_buf = GL_COLOR_ATTACHMENT0;
                GLenum blit_type = GL_COLOR_BUFFER_BIT;

                if ((m_desc.m_depth_size) && (m_desc.m_stencil_size))
                {
                    attachment = GL_DEPTH_STENCIL_ATTACHMENT;
                    draw_and_read_buf = GL_NONE;
                    blit_type = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
                }
                else if (m_desc.m_depth_size)
                {
                    attachment = GL_DEPTH_ATTACHMENT;
                    draw_and_read_buf = GL_NONE;
                    blit_type = GL_DEPTH_BUFFER_BIT;
                }
                else if (m_desc.m_stencil_size)
                {
                    attachment = GL_STENCIL_ATTACHMENT;
                    draw_and_read_buf = GL_NONE;
                    blit_type = GL_STENCIL_BUFFER_BIT;
                }

                GLuint src_fbo_handle = 0, dst_fbo_handle = 0;

                // Source FBO
                GL_ENTRYPOINT(glGenFramebuffers)(1, &src_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, src_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glFramebufferRenderbuffer)(GL_READ_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_snapshot_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glReadBuffer)(draw_and_read_buf);
                VOGL_CHECK_GL_ERROR;

                // Dest FBO
                GL_ENTRYPOINT(glGenFramebuffers)(1, &dst_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, dst_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, attachment, tex_target, tex_handle, 0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDrawBuffers)(1, &draw_and_read_buf);
                VOGL_CHECK_GL_ERROR;

                GLenum read_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_READ_FRAMEBUFFER);
                VOGL_CHECK_GL_ERROR;

                GLenum draw_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
                VOGL_CHECK_GL_ERROR;

                if ((read_status == GL_FRAMEBUFFER_COMPLETE) && (draw_status == GL_FRAMEBUFFER_COMPLETE))
                {
                    GL_ENTRYPOINT(glBlitFramebuffer)(
                        0, 0, m_desc.m_width, m_desc.m_height,
                        0, 0, m_desc.m_width, m_desc.m_height,
                        blit_type,
                        GL_NEAREST);

                    if (!vogl_check_gl_error_internal())
                    {
                        vogl_handle_remapper def_handle_remapper;
                        if (m_texture.snapshot(context_info, def_handle_remapper, tex_handle, tex_target))
                            capture_status = true;
                    }
                }

                // Delete FBO
                GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDeleteFramebuffers)(1, &dst_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, 0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDeleteFramebuffers)(1, &src_fbo_handle);
                VOGL_CHECK_GL_ERROR;
            }

            GL_ENTRYPOINT(glBindTexture)(tex_target, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDeleteTextures)(1, &tex_handle);
            VOGL_CHECK_GL_ERROR;
        }

        if (!capture_status)
        {
            vogl_error_printf("Failed blitting renderbuffer data to texture for renderbuffer %" PRIu64 "\n", static_cast<uint64_t>(handle));
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_renderbuffer_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(context_info.get_version() >= VOGL_GL_VERSION_3_0);

    if (!m_is_valid)
        return false;

    vogl_scoped_binding_state orig_renderbuffer(GL_RENDERBUFFER);

    bool created_handle = false;

    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenRenderbuffers)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;
        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_RENDER_BUFFERS, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_RENDER_BUFFERS, m_snapshot_handle) == handle);

        created_handle = true;
    }

    GL_ENTRYPOINT(glBindRenderbuffer)(GL_RENDERBUFFER, static_cast<GLuint>(handle));
    if (vogl_check_gl_error())
        goto handle_error;

    if ((m_desc.m_width) && (m_desc.m_height) && (m_desc.m_internal_format))
    {
        if (!m_desc.restore(context_info))
            goto handle_error;

        if (m_texture.is_valid())
        {
            GLenum attachment = GL_COLOR_ATTACHMENT0;
            GLenum draw_and_read_buf = GL_COLOR_ATTACHMENT0;
            GLenum blit_type = GL_COLOR_BUFFER_BIT;

            if ((m_desc.m_depth_size) && (m_desc.m_stencil_size))
            {
                attachment = GL_DEPTH_STENCIL_ATTACHMENT;
                draw_and_read_buf = GL_NONE;
                blit_type = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
            }
            else if (m_desc.m_depth_size)
            {
                attachment = GL_DEPTH_ATTACHMENT;
                draw_and_read_buf = GL_NONE;
                blit_type = GL_DEPTH_BUFFER_BIT;
            }
            else if (m_desc.m_stencil_size)
            {
                attachment = GL_STENCIL_ATTACHMENT;
                draw_and_read_buf = GL_NONE;
                blit_type = GL_STENCIL_BUFFER_BIT;
            }

            bool restore_status = false;

            GLuint64 tex_handle64 = 0;
            vogl_handle_remapper def_handle_remapper;
            if (m_texture.restore(context_info, def_handle_remapper, tex_handle64))
            {
                GLuint tex_handle = static_cast<GLuint>(tex_handle64);

                const GLenum tex_target = (m_desc.m_samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

                GLuint src_fbo_handle = 0, dst_fbo_handle = 0;

                // Source FBO
                GL_ENTRYPOINT(glGenFramebuffers)(1, &src_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, src_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glFramebufferTexture2D)(GL_READ_FRAMEBUFFER, attachment, tex_target, tex_handle, 0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glReadBuffer)(draw_and_read_buf);
                VOGL_CHECK_GL_ERROR;

                // Dest FBO
                GL_ENTRYPOINT(glGenFramebuffers)(1, &dst_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, dst_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glFramebufferRenderbuffer)(GL_DRAW_FRAMEBUFFER, attachment, GL_RENDERBUFFER, static_cast<GLuint>(handle));
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDrawBuffers)(1, &draw_and_read_buf);
                VOGL_CHECK_GL_ERROR;

                GLenum read_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_READ_FRAMEBUFFER);
                VOGL_CHECK_GL_ERROR;

                GLenum draw_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
                VOGL_CHECK_GL_ERROR;

                if ((read_status = GL_FRAMEBUFFER_COMPLETE) && (draw_status == GL_FRAMEBUFFER_COMPLETE))
                {
    #if 0
                    // HACK HACK HACK
                    if (m_texture.get_num_samples() > 1)
                    {
                        uint32_t base_level = m_texture.get_params().get_value<GLenum>(GL_TEXTURE_BASE_LEVEL);

                        if (base_level < m_texture.get_num_levels())
                        {
                            const vogl_state_vector &state_vec = m_texture.get_level_params(0, base_level);

                            uint32_t clear_mask = 0;
                            if (state_vec.get_value<GLenum>(GL_TEXTURE_DEPTH_SIZE))
                            {
                                clear_mask |= GL_DEPTH_BUFFER_BIT;
                            }
                            if (state_vec.get_value<GLenum>(GL_TEXTURE_STENCIL_SIZE))
                            {
                                clear_mask |= GL_STENCIL_BUFFER_BIT;
                            }
                            if (state_vec.get_value<GLenum>(GL_TEXTURE_RED_SIZE) + state_vec.get_value<GLenum>(GL_TEXTURE_GREEN_SIZE) + state_vec.get_value<GLenum>(GL_TEXTURE_BLUE_SIZE) + state_vec.get_value<GLenum>(GL_TEXTURE_ALPHA_SIZE) +
                                state_vec.get_value<GLenum>(GL_TEXTURE_INTENSITY_SIZE) + state_vec.get_value<GLenum>(GL_TEXTURE_LUMINANCE_SIZE))
                            {
                                clear_mask |= GL_COLOR_BUFFER_BIT;
                            }

                            GL_ENTRYPOINT(glClearColor)(1.0f, 0.0f, 1.0f, 1.0f);
                            GL_ENTRYPOINT(glClearDepth)(.5f);
                            GL_ENTRYPOINT(glClearStencil)(128);
                            GL_ENTRYPOINT(glClear)(clear_mask);

                            VOGL_CHECK_GL_ERROR;
                        }
                    }
                    else
    #endif
                    {
                        GL_ENTRYPOINT(glBlitFramebuffer)(
                            0, 0, m_desc.m_width, m_desc.m_height,
                            0, 0, m_desc.m_width, m_desc.m_height,
                            blit_type,
                            GL_NEAREST);

                        if (!vogl_check_gl_error_internal())
                        {
                            restore_status = true;
                        }
                    }
                }

                // Delete FBO
                GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDeleteFramebuffers)(1, &dst_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, 0);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDeleteFramebuffers)(1, &src_fbo_handle);
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDeleteTextures)(1, &tex_handle);
                VOGL_CHECK_GL_ERROR;
            }

            if (!restore_status)
            {
                vogl_error_printf("Failed restoring contents of renderbuffer %u\n", static_cast<GLuint>(handle));
            }
        }
    }

    return true;

handle_error:
    if (created_handle)
    {
        GL_ENTRYPOINT(glBindRenderbuffer)(GL_RENDERBUFFER, 0);
        VOGL_CHECK_GL_ERROR;

        remapper.delete_handle_and_object(VOGL_NAMESPACE_RENDER_BUFFERS, m_snapshot_handle, handle);

        //GLuint handle32 = static_cast<GLuint>(handle);
        //GL_ENTRYPOINT(glDeleteRenderbuffers)(1, &handle32);
        //VOGL_CHECK_GL_ERROR;

        handle = 0;
    }

    return false;
}

bool vogl_renderbuffer_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_RENDER_BUFFERS, m_snapshot_handle));

    return true;
}

void vogl_renderbuffer_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;

    m_desc.clear();
    m_texture.clear();

    m_is_valid = false;
}

bool vogl_renderbuffer_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
        return false;

    node.add_key_value("handle", m_snapshot_handle);

    if (!m_desc.serialize(node))
        return false;

    if (m_texture.is_valid())
    {
        if (!m_texture.serialize(node.add_object("texture"), blob_manager))
            return false;
    }

    return true;
}

bool vogl_renderbuffer_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    m_snapshot_handle = node.value_as_int("handle");

    if (!m_desc.deserialize(node))
        return false;

    if (node.has_object("texture"))
    {
        if (!m_texture.deserialize(*node.find_child_object("texture"), blob_manager))
            return false;
    }

    m_is_valid = true;

    return true;
}

bool vogl_renderbuffer_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTRenderbuffer)
        return false;

    const vogl_renderbuffer_state &rhs = static_cast<const vogl_renderbuffer_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    return (m_desc == rhs.m_desc) && (m_texture.compare_restorable_state(rhs.m_texture));
}

