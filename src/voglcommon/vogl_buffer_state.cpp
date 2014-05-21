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

// File: vogl_buffer_state.cpp
#include "vogl_common.h"
#include "vogl_buffer_state.h"
#include "vogl_gl_state_snapshot.h"

vogl_buffer_state::vogl_buffer_state()
    : m_snapshot_handle(0),
      m_target(GL_NONE),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_buffer_state::~vogl_buffer_state()
{
    VOGL_FUNC_TRACER
}

// TODO: GL3/4 buffer types, add GL_QUERY_BUFFER
bool vogl_buffer_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);
    m_target = target;

    if (m_target != GL_NONE)
    {
        vogl_scoped_binding_state orig_bindings(target);

        GL_ENTRYPOINT(glBindBuffer)(target, m_snapshot_handle);
        VOGL_CHECK_GL_ERROR;

        bool any_gl_errors = false;

#define GET_INT(pname)                                                  \
    do                                                                  \
    {                                                                   \
        int value = 0;                                                  \
        GL_ENTRYPOINT(glGetBufferParameteriv)(m_target, pname, &value); \
        if (vogl_check_gl_error())                                       \
            any_gl_errors = true;                                       \
        m_params.insert(pname, 0, &value, sizeof(value));               \
    } while (0)

        GET_INT(GL_BUFFER_ACCESS);
        GET_INT(GL_BUFFER_MAPPED);
        GET_INT(GL_BUFFER_SIZE);
        GET_INT(GL_BUFFER_USAGE);

#undef GET_INT

        GLvoid *pSnapshot_map_ptr;
        GL_ENTRYPOINT(glGetBufferPointerv)(m_target, GL_BUFFER_MAP_POINTER, &pSnapshot_map_ptr);
        if (vogl_check_gl_error())
            any_gl_errors = true;

        if (any_gl_errors)
        {
            vogl_error_printf("GL error while retrieving buffer %" PRIu64 " target %s's params\n",
                             (uint64_t)handle, get_gl_enums().find_gl_name(target));
            clear();
            return false;
        }

        int buf_size = m_params.get_value<int>(GL_BUFFER_SIZE);
        if (buf_size < 0)
        {
            vogl_error_printf("Invalid buffer size, buffer %" PRIu64 " target %s size %i\n", (uint64_t)handle, get_gl_enums().find_gl_name(target), buf_size);
            clear();
            return false;
        }

        if (m_params.get_value<int>(GL_BUFFER_MAPPED) != 0)
        {
            vogl_error_printf("Can't snapshot buffer %" PRIu64 " target %s while it's currently mapped\n",
                             (uint64_t)handle, get_gl_enums().find_gl_name(target));
            clear();
            return false;
        }

        if (buf_size)
        {
            if (!m_buffer_data.try_resize(buf_size))
            {
                vogl_error_printf("Out of memory while trying to allocate buffer, buffer %" PRIu64 " target %s size %i\n", (uint64_t)handle, get_gl_enums().find_gl_name(target), buf_size);
                clear();
                return false;
            }

            // This will fail if the buffer is currently mapped.
            GL_ENTRYPOINT(glGetBufferSubData)(target, 0, buf_size, m_buffer_data.get_ptr());

            if (vogl_check_gl_error())
            {
                vogl_warning_printf("GL error while retrieving buffer data, buffer %" PRIu64 " target %s size %i\n", (uint64_t)handle, get_gl_enums().find_gl_name(target), buf_size);
            }
        }
    }

    m_is_valid = true;

    return true;
}

void vogl_buffer_state::set_mapped_buffer_snapshot_state(const vogl_mapped_buffer_desc &map_desc)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(map_desc.m_buffer == m_snapshot_handle);

    m_is_mapped = true;

    m_map_ofs = map_desc.m_offset;
    m_map_size = map_desc.m_length;
    m_map_access = map_desc.m_access;
    m_map_range = map_desc.m_range;
}

bool vogl_buffer_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    int buf_usage = 0, buf_size = 0;

    if (!m_is_valid)
        return false;

    VOGL_CHECK_GL_ERROR;

    bool created_handle = false;

    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenBuffers)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;

        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_BUFFERS, m_snapshot_handle, handle, m_target);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_snapshot_handle) == handle);

        created_handle = true;
    }

    if (m_target != GL_NONE)
    {
        vogl_scoped_binding_state orig_bindings(m_target);

        GL_ENTRYPOINT(glBindBuffer)(m_target, static_cast<GLuint>(handle));
        if (vogl_check_gl_error())
            goto handle_failure;

        buf_usage = m_params.get_value<int>(GL_BUFFER_USAGE);
        buf_size = m_params.get_value<int>(GL_BUFFER_SIZE);

        if (buf_size != static_cast<int>(m_buffer_data.size()))
        {
            VOGL_ASSERT_ALWAYS;
            goto handle_failure;
        }

        GL_ENTRYPOINT(glBufferData)(m_target, buf_size, m_buffer_data.get_ptr(), buf_usage);
        if (vogl_check_gl_error())
            goto handle_failure;
    }

    return true;

handle_failure:
    vogl_error_printf("Failed restoring trace buffer %u target %s size %u\n", m_snapshot_handle, get_gl_enums().find_gl_name(m_target), buf_size);

    GL_ENTRYPOINT(glBindBuffer)(m_target, 0);
    VOGL_CHECK_GL_ERROR;

    if ((handle) && (created_handle))
    {
        remapper.delete_handle_and_object(VOGL_NAMESPACE_BUFFERS, m_snapshot_handle, handle);

        //GLuint handle32 = static_cast<GLuint>(handle);
        //GL_ENTRYPOINT(glDeleteBuffers)(1, &handle32);
        //VOGL_CHECK_GL_ERROR;

        handle = 0;
    }

    return false;
}

bool vogl_buffer_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_snapshot_handle));

    return true;
}

void vogl_buffer_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_target = GL_NONE;
    m_buffer_data.clear();
    m_params.clear();
    m_is_valid = false;

    m_map_ofs = 0;
    m_map_size = 0;
    m_map_access = 0;
    m_map_range = false;
    m_is_mapped = false;
}

bool vogl_buffer_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    dynamic_string blob_id;

    if (m_buffer_data.size())
    {
        const char *pBuf_type = utils::map_value(static_cast<int>(m_target), "buf",
                                                 GL_ARRAY_BUFFER, "buf_vertex",
                                                 GL_ELEMENT_ARRAY_BUFFER, "buf_index",
                                                 GL_UNIFORM_BUFFER, "buf_uniform");

        dynamic_string prefix;
        prefix.format("%s_0x%04X", pBuf_type, m_params.get_value<int>(GL_BUFFER_USAGE));

        blob_id = blob_manager.add_buf_compute_unique_id(m_buffer_data.get_ptr(), m_buffer_data.size(), prefix.get_ptr(), "raw");
        if (blob_id.is_empty())
            return false;
    }

    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("target", get_gl_enums().find_gl_name(m_target));
    node.add_key_value("buffer_data_blob_id", blob_id);

    node.add_key_value("map_ofs", m_map_ofs);
    node.add_key_value("map_size", m_map_size);
    node.add_key_value("map_access", m_map_access);
    node.add_key_value("map_range", m_map_range);
    node.add_key_value("is_mapped", m_is_mapped);

    if (!m_params.serialize(node.add_object("params"), blob_manager))
        return false;

    return true;
}

bool vogl_buffer_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    m_snapshot_handle = node.value_as_uint32("handle");
    m_target = vogl_get_json_value_as_enum(node, "target");
    if (m_target != GL_NONE)
    {
        const json_node *pParams_obj = node.find_child_object("params");
        if (!pParams_obj)
        {
            clear();
            return false;
        }

        if (!m_params.deserialize(*pParams_obj, blob_manager))
        {
            clear();
            return false;
        }

        int buf_size = m_params.get_value<int>(GL_BUFFER_SIZE);

        if (buf_size)
        {
            dynamic_string blob_id(node.value_as_string_ptr("buffer_data_blob_id"));
            if (blob_id.is_empty())
            {
                clear();
                return false;
            }

            if (!blob_manager.get(blob_id, m_buffer_data))
            {
                clear();
                return false;
            }
        }

        if (buf_size != static_cast<int>(m_buffer_data.size()))
        {
            clear();
            return false;
        }
    }

    m_map_ofs = node.value_as_uint64("map_ofs");
    m_map_size = node.value_as_uint64("map_size");
    m_map_access = node.value_as_uint32("map_access");
    m_map_range = node.value_as_bool("map_range");
    m_is_mapped = node.value_as_bool("is_mapped");

    m_is_valid = true;

    return true;
}

// Content comparison, ignores handle.
bool vogl_buffer_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTBuffer)
        return false;

    const vogl_buffer_state &rhs = static_cast<const vogl_buffer_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    if (m_target != rhs.m_target)
        return false;

    if (m_buffer_data != rhs.m_buffer_data)
        return false;

    if (m_params != rhs.m_params)
        return false;

    return true;
}

























