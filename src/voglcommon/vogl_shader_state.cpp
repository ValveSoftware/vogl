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

// File: vogl_shader_state.cpp
#include "vogl_shader_state.h"
#include "vogl_growable_array.h"

vogl_shader_state::vogl_shader_state()
    : m_snapshot_handle(0),
      m_shader_type(GL_NONE),
      m_restore_compile_status(false),
      m_marked_for_deletion(false),
      m_compile_status(false),
      m_is_valid(false)
{
    m_fs_pp = vogl_fs_preprocessor::get_instance();
    VOGL_FUNC_TRACER
}

vogl_shader_state::vogl_shader_state(const vogl_shader_state &other)
    : m_snapshot_handle(0),
      m_shader_type(GL_NONE),
      m_restore_compile_status(false),
      m_marked_for_deletion(false),
      m_compile_status(false),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER

    *this = other;
}

vogl_shader_state &vogl_shader_state::operator=(const vogl_shader_state &rhs)
{
    VOGL_FUNC_TRACER

    if (this == &rhs)
        return *this;

    clear();

    m_snapshot_handle = rhs.m_snapshot_handle;
    m_shader_type = rhs.m_shader_type;
    m_info_log = rhs.m_info_log;
    m_source = rhs.m_source;
    m_source_blob_id = rhs.m_source_blob_id;
    m_restore_compile_status = rhs.m_restore_compile_status;
    m_marked_for_deletion = rhs.m_marked_for_deletion;
    m_compile_status = rhs.m_compile_status;
    m_is_valid = rhs.m_is_valid;

    return *this;
}

vogl_shader_state::~vogl_shader_state()
{
    VOGL_FUNC_TRACER
}

// TODO: This will only work in GL 2.0 or higher
bool vogl_shader_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(target);

    VOGL_CHECK_GL_ERROR;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);

    GL_ENTRYPOINT(glGetShaderiv)(m_snapshot_handle, GL_SHADER_TYPE, reinterpret_cast<GLint *>(&m_shader_type));
    if (vogl_check_gl_error())
    {
        clear();
        return false;
    }

    GLint val;

    GL_ENTRYPOINT(glGetShaderiv)(m_snapshot_handle, GL_DELETE_STATUS, &val);
    VOGL_CHECK_GL_ERROR;
    m_marked_for_deletion = (val != 0);

    GL_ENTRYPOINT(glGetShaderiv)(m_snapshot_handle, GL_COMPILE_STATUS, &val);
    VOGL_CHECK_GL_ERROR;
    m_compile_status = (val != 0);

    GLint info_log_length = 0;
    GL_ENTRYPOINT(glGetShaderiv)(m_snapshot_handle, GL_INFO_LOG_LENGTH, &info_log_length);
    VOGL_CHECK_GL_ERROR;

    growable_array<GLchar, 4096> temp_buf(info_log_length);

    if (info_log_length)
    {
        GLint actual_len = 0;
        GL_ENTRYPOINT(glGetShaderInfoLog)(m_snapshot_handle, info_log_length, &actual_len, temp_buf.get_ptr());
        VOGL_CHECK_GL_ERROR;

        m_info_log.set(reinterpret_cast<char *>(temp_buf.get_ptr()));
    }

    // length excluding the null terminator
    GLint shader_source_length = 0;
    GL_ENTRYPOINT(glGetShaderiv)(m_snapshot_handle, GL_SHADER_SOURCE_LENGTH, &shader_source_length);
    VOGL_CHECK_GL_ERROR;

    if (shader_source_length)
    {
        temp_buf.resize(shader_source_length);

        GLint actual_len = 0;
        GL_ENTRYPOINT(glGetShaderSource)(m_snapshot_handle, shader_source_length, &actual_len, temp_buf.get_ptr());
        VOGL_CHECK_GL_ERROR;

        set_source(reinterpret_cast<char *>(temp_buf.get_ptr()));
    }

    m_is_valid = true;

    return true;
}

bool vogl_shader_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

    m_restore_compile_status = false;

    if (!m_is_valid)
        return false;

    bool created_handle = false;

    if (!handle)
    {
        handle = GL_ENTRYPOINT(glCreateShader)(m_shader_type);
        if ((vogl_check_gl_error()) || (!handle))
            return false;

        remapper.declare_handle(VOGL_NAMESPACE_SHADERS, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_SHADERS, m_snapshot_handle) == handle);

        created_handle = true;
    }

    VOGL_ASSERT(handle <= cUINT32_MAX);

    if (m_source.get_len())
    {
        GLchar *const pStr = (GLchar *)(m_source.get_ptr());
        // Assuming that m_fs_pp class was instantiated when this class was and that it already has pp cmd & option info
        if ((GL_FRAGMENT_SHADER == m_shader_type) && m_fs_pp->enabled())
        {
            m_fs_pp->set_shader(m_snapshot_handle, pStr, m_source.get_len());
            if (!m_fs_pp->run())
                return false;

            GLchar *const pOutStr = (GLchar *)m_fs_pp->get_output_shader();
            vogl::vector<GLint> lengths(1);
            lengths[0] = m_fs_pp->get_length();
            GL_ENTRYPOINT(glShaderSource)(static_cast<GLuint>(handle), m_fs_pp->get_count(), &pOutStr, lengths.get_ptr());
        }
        else
        {
            GL_ENTRYPOINT(glShaderSource)(static_cast<GLuint>(handle), 1, &pStr, NULL);
        }

        if (vogl_check_gl_error())
            goto handle_error;

        // TODO: We're always trying to compile here when there's shader source, but the original object may not have been compiled. Is this important enough to shadow this?
        GL_ENTRYPOINT(glCompileShader)(static_cast<GLuint>(handle));
        if (vogl_check_gl_error())
            goto handle_error;

        // Immediately get the compile status now to avoid driver bugs I've seen in the wild.
        GLint val;
        GL_ENTRYPOINT(glGetShaderiv)(static_cast<GLuint>(handle), GL_COMPILE_STATUS, &val);
        VOGL_CHECK_GL_ERROR;

        m_restore_compile_status = (val != 0);

        if (!m_restore_compile_status)
        {
            GLint info_log_length = 0;
            GL_ENTRYPOINT(glGetShaderiv)(static_cast<GLuint>(handle), GL_INFO_LOG_LENGTH, &info_log_length);
            VOGL_CHECK_GL_ERROR;

            growable_array<GLchar, 4096> temp_buf(info_log_length);

            dynamic_string info_log;
            if (info_log_length)
            {
                GLint actual_len = 0;
                GL_ENTRYPOINT(glGetShaderInfoLog)(static_cast<GLuint>(handle), info_log_length, &actual_len, temp_buf.get_ptr());
                VOGL_CHECK_GL_ERROR;

                info_log.set(reinterpret_cast<char *>(temp_buf.get_ptr()));
            }

            vogl_warning_printf("Restored shader failed to compile: trace compile status %u, snapshot handle %u, restore handle %u, type: %s, blob id: %s, info log:\n\"%s\"\n",
                                m_compile_status, m_snapshot_handle, (uint32_t)handle, get_gl_enums().find_gl_name(m_shader_type), m_source_blob_id.get_ptr(), info_log.get_ptr());
        }
        else if (!m_compile_status)
        {
            vogl_warning_printf("Shader successfully compiled during restore, but was not compiled successfully during tracing: trace compile status %u, snapshot handle %u, restore handle %u, type: %s, blob id: %s\n",
                                m_compile_status, m_snapshot_handle, (uint32_t)handle, get_gl_enums().find_gl_name(m_shader_type), m_source_blob_id.get_ptr());
        }
    }

    return true;

handle_error:
    if (created_handle)
    {
        remapper.delete_handle_and_object(VOGL_NAMESPACE_SHADERS, m_snapshot_handle, handle);

        //GL_ENTRYPOINT(glDeleteShader)(static_cast<GLuint>(handle));
        //VOGL_CHECK_GL_ERROR;

        handle = 0;
    }
    return false;
}

bool vogl_shader_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_SHADERS, m_snapshot_handle));

    return true;
}

void vogl_shader_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_shader_type = GL_NONE;

    m_info_log.clear();
    m_source.clear();
    m_source_blob_id.clear();

    m_marked_for_deletion = false;
    m_compile_status = false;
    m_restore_compile_status = false;

    m_is_valid = false;
}

bool vogl_shader_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    dynamic_string source_blob_id;

    if (m_source.get_len())
    {
        const char *pPrefix = utils::map_value(static_cast<int>(m_shader_type), "shader",
                                               GL_VERTEX_SHADER, "vertex_shader",
                                               GL_COMPUTE_SHADER, "compute_shader",
                                               GL_TESS_CONTROL_SHADER, "tess_control_shader",
                                               GL_TESS_EVALUATION_SHADER, "tess_eval_shader",
                                               GL_GEOMETRY_SHADER, "geom_shader",
                                               GL_FRAGMENT_SHADER, "fragment_shader");

        dynamic_string prefix(cVarArg, "%s_%u", pPrefix, m_compile_status);

        source_blob_id = blob_manager.add_buf_compute_unique_id(m_source.get_ptr(), m_source.get_len(), prefix.get_ptr(), "txt");
        if (source_blob_id.is_empty())
            return false;

        m_source_blob_id = source_blob_id;
    }

    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("type", get_gl_enums().find_gl_name(m_shader_type));
    node.add_key_value("info_log", m_info_log);
    node.add_key_value("source_blob_id", source_blob_id);
    node.add_key_value("marked_for_deletion", m_marked_for_deletion);
    node.add_key_value("compile_status", m_compile_status);

    return true;
}

bool vogl_shader_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    dynamic_string source_blob_id(node.value_as_string("source_blob_id"));
    if (!source_blob_id.is_empty())
    {
        m_source_blob_id = source_blob_id;

        uint8_vec source_data;
        if (!blob_manager.get(source_blob_id, source_data))
            return false;

        // There can't be any null terminators in this buffer or dynamic_string will shit itself
        int first_zero_ofs = source_data.find(0);
        if (first_zero_ofs >= 0)
            source_data.resize(first_zero_ofs);

        if (!source_data.is_empty())
            m_source.set_from_buf(source_data.get_ptr(), source_data.size());
    }

    m_snapshot_handle = node.value_as_uint32("handle");
    VOGL_ASSERT(m_snapshot_handle);

    m_shader_type = vogl_get_json_value_as_enum(node, "type");
    if (!utils::is_in_set(static_cast<int>(m_shader_type), GL_VERTEX_SHADER, GL_COMPUTE_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER))
        return false;

    m_info_log = node.value_as_string("info_log");

    m_marked_for_deletion = node.value_as_bool("marked_for_deletion");
    m_compile_status = node.value_as_bool("compile_status");

    m_is_valid = true;

    return true;
}

bool vogl_shader_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTShader)
        return false;

    const vogl_shader_state &rhs = static_cast<const vogl_shader_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    // only compare the stuff we can save/restore
    if (m_shader_type != rhs.m_shader_type)
        return false;

    if (m_source.compare(rhs.m_source, true) != 0)
        return false;

    return true;
}

bool vogl_shader_state::compare_full_state(const vogl_shader_state &rhs) const
{
    VOGL_FUNC_TRACER

#define CMP(x)      \
    if (x != rhs.x) \
        return false;
    CMP(m_snapshot_handle);
    CMP(m_shader_type);
    CMP(m_info_log);
    CMP(m_source);
    CMP(m_marked_for_deletion);
    CMP(m_compile_status);
    CMP(m_is_valid);
#undef CMP

    return true;
}
