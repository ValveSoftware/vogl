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

// File: vogl_query_state.cpp
#include "vogl_query_state.h"

vogl_query_state::vogl_query_state()
    : m_snapshot_handle(0),
      m_target(GL_NONE),
      m_prev_result(0),
      m_has_been_begun(false),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_query_state::~vogl_query_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_query_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_CHECK_GL_ERROR;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);
    m_target = target;
    m_has_been_begun = GL_ENTRYPOINT(glIsQuery)(static_cast<GLuint>(handle)) != 0;

    if (target != GL_NONE)
    {
        if (context_info.supports_extension("GL_ARB_timer_query") && GL_ENTRYPOINT(glGetQueryObjecti64v))
        {
            GLint64 result = 0;
            GL_ENTRYPOINT(glGetQueryObjecti64v)(m_snapshot_handle, GL_QUERY_RESULT, &result);
            m_prev_result = result;
        }
        else
        {
            GLuint prev_result32;
            GL_ENTRYPOINT(glGetQueryObjectuiv)(m_snapshot_handle, GL_QUERY_RESULT, &prev_result32);
            m_prev_result = prev_result32;
        }
    }

    m_get_result_status = vogl_check_gl_error();

    m_is_valid = true;

    return true;
}

bool vogl_query_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    if (!m_is_valid)
        return false;

    bool created_handle = false;

    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenQueries)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;
        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_QUERIES, m_snapshot_handle, handle, m_target);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_QUERIES, m_snapshot_handle) == handle);

        created_handle = true;
    }

    // m_target will be GL_NONE if the query has been genned but not begun up to this point.
    if ((m_target != GL_NONE) && (m_has_been_begun))
    {
        GLuint prev_query = 0;
        GL_ENTRYPOINT(glGetQueryiv)(m_target, GL_CURRENT_QUERY, reinterpret_cast<GLint *>(&prev_query));
        VOGL_CHECK_GL_ERROR;

        VOGL_ASSERT(handle <= cUINT32_MAX);

        // Begin end the restore query, so it becomes a valid name.
        GL_ENTRYPOINT(glBeginQuery)(m_target, static_cast<GLuint>(handle));
        if (vogl_check_gl_error())
            goto handle_error;

        GL_ENTRYPOINT(glEndQuery)(m_target);
        if (vogl_check_gl_error())
            goto handle_error;

        if (prev_query)
        {
            // Now begin/end the original query so it's active again. The query API sucks.
            GL_ENTRYPOINT(glBeginQuery)(m_target, prev_query);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glEndQuery)(m_target);
            VOGL_CHECK_GL_ERROR;
        }
    }

    return true;

handle_error:
    if ((handle) && (created_handle))
    {
        remapper.delete_handle_and_object(VOGL_NAMESPACE_QUERIES, m_snapshot_handle, handle);

        //GLuint handle32 = static_cast<GLuint>(handle);
        //GL_ENTRYPOINT(glDeleteQueries)(1, &handle32);
        //VOGL_CHECK_GL_ERROR;

        handle = 0;
    }

    return false;
}

bool vogl_query_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_QUERIES, m_snapshot_handle));

    return true;
}

void vogl_query_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_target = GL_NONE;
    m_prev_result = 0;
    m_get_result_status = GL_NONE;
    m_has_been_begun = false;
    m_is_valid = false;
}

bool vogl_query_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
        return false;

    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("target", get_gl_enums().find_name(m_target, "gl"));
    node.add_key_value("prev_result", m_prev_result);
    node.add_key_value("has_been_begun", m_has_been_begun);

    return true;
}

bool vogl_query_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    m_snapshot_handle = node.value_as_uint32("handle");
    if (!m_snapshot_handle)
    {
        return false;
    }

    m_target = vogl_get_json_value_as_enum(node, "target");
    if (!utils::is_in_set(static_cast<int>(m_target), GL_NONE, GL_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED, GL_PRIMITIVES_GENERATED, GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_TIME_ELAPSED))
    {
        clear();
        return false;
    }

    m_prev_result = node.value_as_int64("prev_result");
    m_has_been_begun = node.value_as_bool("has_been_begun");

    m_is_valid = true;

    return true;
}

bool vogl_query_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTQuery)
        return false;

    const vogl_query_state &rhs = static_cast<const vogl_query_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    // We can't restore the prev result, so don't bother comparing it, so there's not much to do here.
    return (m_target == rhs.m_target) && (m_has_been_begun == rhs.m_has_been_begun);
}
