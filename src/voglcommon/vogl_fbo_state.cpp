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

// File: vogl_fbo_state.cpp
#include "vogl_common.h"
#include "vogl_fbo_state.h"

vogl_framebuffer_attachment::vogl_framebuffer_attachment()
    : m_attachment(GL_NONE),
      m_type(GL_NONE)
{
    VOGL_FUNC_TRACER
}

vogl_framebuffer_attachment::~vogl_framebuffer_attachment()
{
    VOGL_FUNC_TRACER
}

bool vogl_framebuffer_attachment::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLenum attachment, GLenum type)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_NOTE_UNUSED(context_info);

    clear();

    m_attachment = attachment;

    m_type = type;

#define DO_QUERY(e)                                                                                \
    do                                                                                             \
    {                                                                                              \
        int val = 0;                                                                               \
        GL_ENTRYPOINT(glGetFramebufferAttachmentParameteriv)(GL_FRAMEBUFFER, attachment, e, &val); \
        VOGL_CHECK_GL_ERROR;                                                                        \
        m_params.insert(e, val);                                                                   \
    } while (0)

    // TODO: Is this query really valid on default framebuffer FBO's?
    DO_QUERY(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);

    static const GLenum s_common_queries[] =
        {
            GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
            GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING
        };

    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_common_queries); i++)
    {
        DO_QUERY(s_common_queries[i]);
    }

    if (m_type == GL_TEXTURE)
    {
        DO_QUERY(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
        DO_QUERY(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE);
        DO_QUERY(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
        // only need to query LAYERED if GS supported
        if (context_info.supports_extension("GL_ARB_geometry_shader4"))
            DO_QUERY(GL_FRAMEBUFFER_ATTACHMENT_LAYERED);
    }

#undef DO_QUERY

    return true;
}

bool vogl_framebuffer_attachment::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_params.contains(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME))
        return false;

    if (m_type == GL_RENDERBUFFER)
        m_params[GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME] = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_RENDER_BUFFERS, get_handle()));
    else if (m_type == GL_TEXTURE)
        m_params[GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME] = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, get_handle()));

    return true;
}

void vogl_framebuffer_attachment::clear()
{
    VOGL_FUNC_TRACER

    m_attachment = GL_NONE;
    m_type = GL_NONE;
    m_params.clear();
}

bool vogl_framebuffer_attachment::serialize(json_node &node) const
{
    VOGL_FUNC_TRACER

    node.add_key_value("attachment", get_gl_enums().find_name(m_attachment, "gl"));
    node.add_key_value("type", get_gl_enums().find_name(m_type, "gl"));

    for (GLenum_to_int_map::const_iterator it = m_params.begin(); it != m_params.end(); ++it)
    {
        const char *pEnum_name = get_gl_enums().find_name(it->first, "gl");

        if (get_gl_enums().get_pname_type(it->first) == cSTGLenum)
            node.add_key_value(pEnum_name, get_gl_enums().find_name(it->second, "gl"));
        else
            node.add_key_value(pEnum_name, it->second);
    }

    return true;
}

bool vogl_framebuffer_attachment::deserialize(const json_node &node)
{
    VOGL_FUNC_TRACER

    clear();

    m_attachment = vogl_get_json_value_as_enum(node, "attachment");
    m_type = vogl_get_json_value_as_enum(node, "type");

    for (uint32_t i = 0; i < node.size(); i++)
    {
        const dynamic_string &key = node.get_key(i);
        const json_value &val = node.get_value(i);

        if (key == "attachment")
            m_attachment = vogl_get_json_value_as_enum(val);
        else if (key == "type")
            m_type = vogl_get_json_value_as_enum(val);
        else
        {
            uint64_t enum_val = get_gl_enums().find_enum(key);
            if ((enum_val == gl_enums::cUnknownEnum) || (enum_val > cUINT32_MAX))
                vogl_warning_printf("Invalid enum \"%s\"\n", key.get_ptr());
            else
            {
                if (get_gl_enums().get_pname_type(enum_val) == cSTGLenum)
                    m_params.insert(static_cast<GLenum>(enum_val), vogl_get_json_value_as_enum(val));
                else
                    m_params.insert(static_cast<GLenum>(enum_val), val.as_int());
            }
        }
    }

    return true;
}

bool vogl_framebuffer_attachment::operator==(const vogl_framebuffer_attachment &rhs) const
{
    VOGL_FUNC_TRACER

    if (m_attachment != rhs.m_attachment)
        return false;
    if (m_type != rhs.m_type)
        return false;
    return m_params == rhs.m_params;
}

vogl_framebuffer_state::vogl_framebuffer_state()
    : m_snapshot_handle(0),
      m_has_been_bound(false),
      m_read_buffer(GL_NONE),
      m_status(GL_FRAMEBUFFER_COMPLETE),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_framebuffer_state::~vogl_framebuffer_state()
{
    VOGL_FUNC_TRACER

    clear();
}

bool vogl_framebuffer_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;
    VOGL_NOTE_UNUSED(target);

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);
    m_has_been_bound = GL_ENTRYPOINT(glIsFramebuffer)(m_snapshot_handle) != 0;

    if (m_has_been_bound)
    {
        vogl_scoped_binding_state orig_framebuffers(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER);

        GL_ENTRYPOINT(glBindFramebuffer)(GL_FRAMEBUFFER, m_snapshot_handle);
        VOGL_CHECK_GL_ERROR;

        // These are per-framebuffer states, not per-context, so save them.
        uint32_t max_draw_buffers = vogl_get_gl_integer(GL_MAX_DRAW_BUFFERS);
        VOGL_CHECK_GL_ERROR;

        m_draw_buffers.resize(max_draw_buffers);
        for (uint32_t i = 0; i < max_draw_buffers; i++)
        {
            m_draw_buffers[i] = vogl_get_gl_integer(GL_DRAW_BUFFER0 + i);
            VOGL_CHECK_GL_ERROR;
        }

        m_read_buffer = vogl_get_gl_integer(GL_READ_BUFFER);
        VOGL_CHECK_GL_ERROR;

        // TODO: Read GL_FRAMEBUFFER_DEFAULT_WIDTH, etc.

        m_status = GL_FRAMEBUFFER_COMPLETE;
        if (handle)
        {
            m_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
            VOGL_CHECK_GL_ERROR;
        }

        int max_color_attachments = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
        VOGL_CHECK_GL_ERROR;

        const GLenum s_default_attachments[] =
            {
                GL_FRONT_LEFT, GL_FRONT_RIGHT, GL_BACK_LEFT, GL_BACK_RIGHT, GL_DEPTH, GL_STENCIL
            };

        const GLenum s_framebuffer_attachments[] =
            {
                GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7,
                GL_COLOR_ATTACHMENT8, GL_COLOR_ATTACHMENT9, GL_COLOR_ATTACHMENT10, GL_COLOR_ATTACHMENT11, GL_COLOR_ATTACHMENT12, GL_COLOR_ATTACHMENT13, GL_COLOR_ATTACHMENT14, GL_COLOR_ATTACHMENT15,
                GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT
            };

        const GLenum *pAttachments = s_default_attachments;
        uint32_t num_attachments = VOGL_ARRAY_SIZE(s_default_attachments);

        if (handle)
        {
            pAttachments = s_framebuffer_attachments;
            num_attachments = VOGL_ARRAY_SIZE(s_framebuffer_attachments);
        }

        for (uint32_t i = 0; i < num_attachments; i++)
        {
            GLenum attachment = pAttachments[i];

            if ((attachment >= GL_COLOR_ATTACHMENT0) && (attachment <= GL_COLOR_ATTACHMENT15))
            {
                if (attachment > static_cast<uint32_t>((GL_COLOR_ATTACHMENT0 + max_color_attachments - 1)))
                    continue;
            }

            int obj_type = 0;
            GL_ENTRYPOINT(glGetFramebufferAttachmentParameteriv)(GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &obj_type);
            VOGL_CHECK_GL_ERROR;

            switch (obj_type)
            {
                case GL_NONE:
                {
                    break;
                }
                case GL_FRAMEBUFFER_DEFAULT:
                case GL_TEXTURE:
                case GL_RENDERBUFFER:
                {
                    if (!m_attachments[attachment].snapshot(context_info, remapper, attachment, obj_type))
                        return false;
                    break;
                }
                default:
                {
                    VOGL_ASSERT_ALWAYS;
                    break;
                }
            }
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_framebuffer_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

    if (!m_is_valid)
        return false;

    if (!m_snapshot_handle)
    {
        // Can't restore the default framebuffer
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    bool created_handle = false;

    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenFramebuffers)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;

        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_FRAMEBUFFERS, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_FRAMEBUFFERS, m_snapshot_handle) == handle);

        created_handle = true;
    }

    VOGL_ASSERT(handle <= cUINT32_MAX);

    if (m_has_been_bound)
    {
        // The order of these saves/restores matters because the draw and read buffers affect the currently bound framebuffer.
        vogl_scoped_binding_state binding_state(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER);

        GL_ENTRYPOINT(glBindFramebuffer)(GL_FRAMEBUFFER, static_cast<GLuint>(handle));
        if (vogl_check_gl_error())
            goto handle_error;

        for (GLenum_to_attachment_map::const_iterator it = m_attachments.begin(); it != m_attachments.end(); ++it)
        {
            const vogl_framebuffer_attachment &attachment_obj = it->second;
            const GLenum attachment_target = attachment_obj.get_attachment();
            const GLenum attachment_type = attachment_obj.get_type();

            switch (attachment_type)
            {
                case GL_FRAMEBUFFER_DEFAULT:
                {
                    goto handle_error;
                }
                case GL_RENDERBUFFER:
                {
                    GLuint trace_handle = attachment_obj.get_handle();
                    if (trace_handle)
                    {
                        GLuint rbo_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_RENDER_BUFFERS, trace_handle));
                        if (!rbo_handle) {
                            vogl_warning_printf("Failed mapping RenderBuffer handle %d\n", trace_handle);
                            break;
                            //goto handle_error;
                        }
                            

                        GL_ENTRYPOINT(glFramebufferRenderbuffer)(GL_DRAW_FRAMEBUFFER, attachment_target, GL_RENDERBUFFER, rbo_handle);
                        VOGL_CHECK_GL_ERROR;
                    }

                    break;
                }
                case GL_TEXTURE:
                {
                    GLuint trace_handle = attachment_obj.get_handle();
                    if (trace_handle)
                    {
                        //$ TODO WSHADOW: this handle shadows high level handle. This looks like a bug.
                        GLuint tex_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, trace_handle));
                        if (!tex_handle)
                            goto handle_error;

                        //const GLenum tex_target = vogl_determine_texture_target(context_info, tex_handle);
                        GLenum tex_target = GL_NONE;
                        if (!remapper.determine_to_object_target(VOGL_NAMESPACE_TEXTURES, tex_handle, tex_target))
                        {
                            vogl_error_printf("Failed determining FBO texture attachment's target, trace FBO handle %u GL handle %u, trace texture handle %u GL handle %u\n", m_snapshot_handle, static_cast<uint32_t>(handle), attachment_obj.get_handle(), tex_handle);
                            goto handle_error;
                        }

                        const int cube_map_face = attachment_obj.get_param(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, 0);
                        const int layer = attachment_obj.get_param(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, 0);
                        const int level = attachment_obj.get_param(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, 0);
                        const int layered = attachment_obj.get_param(GL_FRAMEBUFFER_ATTACHMENT_LAYERED, GL_FALSE);

                        if (layered)
                        {
                            // GL_FRAMEBUFFER_ATTACHMENT_LAYERED can only be true if glFramebufferTexture was used to attach.
                            GL_ENTRYPOINT(glFramebufferTexture)(GL_DRAW_FRAMEBUFFER, attachment_target, tex_handle, level);
                        }
                        else
                        {
                            switch (tex_target)
                            {
                                case GL_TEXTURE_1D:
                                {
                                    GL_ENTRYPOINT(glFramebufferTexture1D)(GL_DRAW_FRAMEBUFFER, attachment_target, tex_target, tex_handle, level);
                                    break;
                                }
                                case GL_TEXTURE_2D:
                                case GL_TEXTURE_2D_MULTISAMPLE:
                                case GL_TEXTURE_RECTANGLE:
                                {
                                    GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, attachment_target, tex_target, tex_handle, level);
                                    break;
                                }
                                case GL_TEXTURE_CUBE_MAP:
                                {
                                    VOGL_ASSERT((cube_map_face == GL_TEXTURE_CUBE_MAP_POSITIVE_X) || (cube_map_face == GL_TEXTURE_CUBE_MAP_POSITIVE_Y) || (cube_map_face == GL_TEXTURE_CUBE_MAP_POSITIVE_Z) ||
                                                  (cube_map_face == GL_TEXTURE_CUBE_MAP_NEGATIVE_X) || (cube_map_face == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y) || (cube_map_face == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));

                                    GL_ENTRYPOINT(glFramebufferTexture2D)(GL_DRAW_FRAMEBUFFER, attachment_target, cube_map_face, tex_handle, level);
                                    break;
                                }
                                case GL_TEXTURE_3D:
                                case GL_TEXTURE_1D_ARRAY:
                                case GL_TEXTURE_2D_ARRAY:
                                case GL_TEXTURE_CUBE_MAP_ARRAY:
                                case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
                                {
                                    GL_ENTRYPOINT(glFramebufferTextureLayer)(GL_DRAW_FRAMEBUFFER, attachment_target, tex_handle, level, layer);
                                    break;
                                }
                                default:
                                {
                                    vogl_error_printf("Don't know how to attach texture with target %s to FBO, trace FBO handle %u GL handle %u, trace texture handle %u GL handle %u\n", get_gl_enums().find_gl_name(tex_target), m_snapshot_handle, static_cast<uint32_t>(handle), attachment_obj.get_handle(), tex_handle);
                                    goto handle_error;
                                }
                            }
                        }

                        VOGL_CHECK_GL_ERROR;
                    }

                    break;
                }
                default:
                {
                    goto handle_error;
                }
            }
        }

        int last_valid_draw_buffer_idx;
        for (last_valid_draw_buffer_idx = m_draw_buffers.size() - 1; last_valid_draw_buffer_idx >= 0; last_valid_draw_buffer_idx--)
            if (m_draw_buffers[last_valid_draw_buffer_idx] != GL_NONE)
                break;

        uint32_t num_valid_draw_buffers = last_valid_draw_buffer_idx + 1;

        if (!num_valid_draw_buffers)
        {
            GL_ENTRYPOINT(glDrawBuffer)(GL_NONE);
            VOGL_CHECK_GL_ERROR;
        }
        else if (num_valid_draw_buffers == 1)
        {
            GL_ENTRYPOINT(glDrawBuffer)(m_draw_buffers[0]);
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            GL_ENTRYPOINT(glDrawBuffers)(m_draw_buffers.size(), m_draw_buffers.get_ptr());
            VOGL_CHECK_GL_ERROR;
        }

        GL_ENTRYPOINT(glReadBuffer)(m_read_buffer);
        VOGL_CHECK_GL_ERROR;

        GLenum cur_status;
        cur_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
        VOGL_CHECK_GL_ERROR;

        if (cur_status != m_status)
        {
            vogl_error_printf("Restored FBO's completeness (%s) is not the same as the trace's (%s), trace handle %u GL handle %u!\n", get_gl_enums().find_name(cur_status, "gl"), get_gl_enums().find_name(m_status, "gl"), m_snapshot_handle, static_cast<uint32_t>(handle));
        }
    }

    return true;

handle_error:
    if ((handle) && (created_handle))
    {
        remapper.delete_handle_and_object(VOGL_NAMESPACE_FRAMEBUFFERS, m_snapshot_handle, handle);

        //GLuint handle32 = static_cast<GLuint>(handle);
        //GL_ENTRYPOINT(glDeleteFramebuffers)(1, &handle32);
        //VOGL_CHECK_GL_ERROR;

        handle = 0;
    }

    return false;
}

void vogl_framebuffer_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_has_been_bound = false;

    m_attachments.clear();

    m_draw_buffers.clear();
    m_read_buffer = GL_NONE;

    m_status = GL_FRAMEBUFFER_COMPLETE;

    m_is_valid = false;
}

bool vogl_framebuffer_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_FRAMEBUFFERS, m_snapshot_handle));

    bool success = true;

    for (GLenum_to_attachment_map::iterator it = m_attachments.begin(); it != m_attachments.end(); ++it)
        if (!it->second.remap_handles(remapper))
            success = false;

    return success;
}

bool vogl_framebuffer_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
        return false;

    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("has_been_bound", m_has_been_bound);
    node.add_key_value("status", get_gl_enums().find_name(m_status, "gl"));

    node.add_key_value("read_buffer", m_read_buffer);

    json_node &draw_buffers_array = node.add_array("draw_buffers");
    for (uint32_t i = 0; i < m_draw_buffers.size(); i++)
        draw_buffers_array.add_value(m_draw_buffers[i]);

    json_node &attachments_array = node.add_array("attachments");
    for (GLenum_to_attachment_map::const_iterator it = m_attachments.begin(); it != m_attachments.end(); ++it)
        if (!it->second.serialize(attachments_array.add_object()))
            return false;

    return true;
}

bool vogl_framebuffer_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    m_snapshot_handle = node.value_as_int("handle");
    m_has_been_bound = node.value_as_bool("has_been_bound", true);
    m_status = vogl_get_json_value_as_enum(node, "status");

    m_read_buffer = node.value_as_uint32("read_buffer");

    const json_node *pDraw_buffers_array = node.find_child_array("draw_buffers");
    if ((pDraw_buffers_array) && (pDraw_buffers_array->are_all_children_values()))
    {
        m_draw_buffers.resize(pDraw_buffers_array->size());

        for (uint32_t i = 0; i < m_draw_buffers.size(); i++)
            m_draw_buffers[i] = pDraw_buffers_array->value_as_uint32(i);
    }

    const json_node *pAttachments_array = node.find_child_array("attachments");
    if (pAttachments_array)
    {
        for (uint32_t i = 0; i < pAttachments_array->size(); i++)
        {
            const json_node *pAttachment = pAttachments_array->get_child(i);
            if (pAttachment)
            {
                GLenum attachment = vogl_get_json_value_as_enum(*pAttachment, "attachment");
                if (attachment != GL_NONE)
                {
                    if (!m_attachments[attachment].deserialize(*pAttachment))
                        return false;
                }
            }
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_framebuffer_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTFramebuffer)
        return false;

    const vogl_framebuffer_state &rhs = static_cast<const vogl_framebuffer_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    if (m_status != rhs.m_status)
        return false;

    if (m_has_been_bound != rhs.m_has_been_bound)
        return false;

    if (m_read_buffer != rhs.m_read_buffer)
        return false;

    if (m_draw_buffers != rhs.m_draw_buffers)
        return false;

    return m_attachments == rhs.m_attachments;
}
