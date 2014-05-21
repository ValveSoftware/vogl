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

// File: vogl_sync_object.cpp
#include "vogl_sync_object.h"
#include "vogl_gl_state_snapshot.h"

vogl_sync_state::vogl_sync_state()
    : m_snapshot_handle(0),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_sync_state::~vogl_sync_state()
{
    VOGL_FUNC_TRACER
}

// TODO: Requires GL >= 3.2 or extension GL_ARB_sync
bool vogl_sync_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(target);
    VOGL_NOTE_UNUSED(remapper);

    VOGL_CHECK_GL_ERROR;

    clear();

    m_snapshot_handle = handle;

    GLsync sync = vogl_handle_to_sync(handle);

    bool any_gl_errors = false;

#define GET_INT(pname)                                                    \
    do                                                                    \
    {                                                                     \
        GLint val = 0;                                                    \
        GLsizei len = 0;                                                  \
        GL_ENTRYPOINT(glGetSynciv)(sync, pname, sizeof(val), &len, &val); \
        if (vogl_check_gl_error())                                         \
            any_gl_errors = true;                                         \
        m_params.insert(pname, 0, &val, sizeof(val));                     \
    } while (0)

    GET_INT(GL_OBJECT_TYPE);
    GET_INT(GL_SYNC_STATUS);
    GET_INT(GL_SYNC_CONDITION);
    GET_INT(GL_SYNC_FLAGS);

#undef GET_INT

    if (any_gl_errors)
    {
        clear();

        vogl_error_printf("GL error while enumerating sync %" PRIu64 "'s' params\n", (uint64_t)handle);
        return false;
    }

    VOGL_ASSERT(m_params.get_value<GLenum>(GL_OBJECT_TYPE) == GL_SYNC_FENCE);
    VOGL_ASSERT(m_params.get_value<GLenum>(GL_SYNC_CONDITION) == GL_SYNC_GPU_COMMANDS_COMPLETE);
    VOGL_ASSERT(m_params.get_value<uint32_t>(GL_SYNC_FLAGS) == 0);

    m_is_valid = true;

    return true;
}

bool vogl_sync_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    if (!m_is_valid)
        return false;

    VOGL_CHECK_GL_ERROR;

    if (!handle)
    {
        GLsync sync = GL_ENTRYPOINT(glFenceSync)(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if ((vogl_check_gl_error()) || (!sync))
            return false;
        handle = vogl_sync_to_handle(sync);

        remapper.declare_handle(VOGL_NAMESPACE_SYNCS, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_SYNCS, m_snapshot_handle) == handle);
    }

    return true;
}

bool vogl_sync_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = remapper.remap_handle(VOGL_NAMESPACE_SYNCS, m_snapshot_handle);

    return true;
}

void vogl_sync_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_params.clear();
    m_is_valid = false;
}

bool vogl_sync_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    node.add_key_value("handle", m_snapshot_handle);

    return m_params.serialize(node.add_object("params"), blob_manager);
}

bool vogl_sync_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    m_snapshot_handle = node.value_as_uint32("handle");

    const json_node *pParams_node = node.find_child_object("params");
    if ((!pParams_node) || (!m_params.deserialize(*pParams_node, blob_manager)))
    {
        clear();
        return false;
    }

    m_is_valid = true;

    return true;
}

bool vogl_sync_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTSync)
        return false;

    const vogl_sync_state &rhs = static_cast<const vogl_sync_state &>(rhs_obj);

    if (this == &rhs)
        return true;

    return m_params == rhs.m_params;
}
