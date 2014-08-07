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

// File: vogl_sso_state.cpp
#include "vogl_common.h"
#include "vogl_sso_state.h"

vogl_sso_state::vogl_sso_state()
    : m_snapshot_handle(0),
      m_has_been_bound(false),
      m_is_valid(false)      
{
    VOGL_FUNC_TRACER
    
    utils::zero_object(m_shader_objs);
}

vogl_sso_state::~vogl_sso_state()
{
    VOGL_FUNC_TRACER

    clear();
}

static int vogl_get_program_pipeline(uint32_t index, GLenum pname)
{
    VOGL_FUNC_TRACER

    int value = 0;
    GL_ENTRYPOINT(glGetProgramPipelineiv)(index, pname, &value);
    VOGL_CHECK_GL_ERROR;
    return value;
}

GLint vogl_sso_state::get_shader_int(GLenum pname) const
{
    VOGL_FUNC_TRACER

    GLint val = 0;
    GL_ENTRYPOINT(glGetProgramivARB)(m_snapshot_handle, pname, &val);
    VOGL_CHECK_GL_ERROR;
    return val;
}

bool vogl_sso_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);
    VOGL_ASSERT(context_info.get_version() >= VOGL_GL_VERSION_3_0);
    VOGL_CHECK_GL_ERROR;
    VOGL_NOTE_UNUSED(target);

    VOGL_ASSERT(handle <= cUINT32_MAX);

    clear();
    m_snapshot_handle = static_cast<GLuint>(handle);
    m_has_been_bound = GL_ENTRYPOINT(glIsProgramPipeline)(m_snapshot_handle) != 0;

    if (m_has_been_bound)
    {
        // TODO : This needs to be cleaned up. Right now it's "magic" that this order
        //   matches up to enum ordering, & that enum and this array are same size
        const GLenum s_sso_attachments[] =
            {
                GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER
            };
        uint32_t num_attachments = VOGL_ARRAY_SIZE(s_sso_attachments);
        
        for (uint32_t i = 0; i < num_attachments; i++)
            m_shader_objs[i] = vogl_get_program_pipeline(m_snapshot_handle, s_sso_attachments[i]);
        
        // TODO : Could also query INFO_LOG_LENGTH, VALIDATE_STATUS, ACTIVE_PROGRAM & INFO_LOG
    }

    m_is_valid = true;

    return true;
}

bool vogl_sso_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

    VOGL_CHECK_GL_ERROR;

    if (!m_is_valid)
        return false;

    if (!m_snapshot_handle)
    {
        // Can't restore the default sso
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if (!handle)
    {
        // TODO : Don't handle this case yet
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    VOGL_ASSERT(handle <= cUINT32_MAX);

    if (m_has_been_bound)
    {
        GL_ENTRYPOINT(glBindProgramPipeline)(static_cast<GLuint>(handle));
        if (vogl_check_gl_error())
            return false;
        
        // TODO : I believe this actually needs to handle re-binding program objs etc. to recreate the SSO                  
    }

    return true;
}

void vogl_sso_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_has_been_bound = false;

    for (uint32_t i = 0; i < cNumShaders; i++)
        m_shader_objs[i] = 0;
    
    m_is_valid = false;
}

bool vogl_sso_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    for (uint32_t target = 0; target < cNumShaders; target++)
    {
        uint32_t replay_handle = m_shader_objs[target];
        if (replay_handle)
        {
            uint32_t trace_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_PIPELINES, replay_handle));

            m_shader_objs[target] = trace_handle;
        }
    }

    return true;
}

bool vogl_sso_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
        return false;

    for (uint32_t type = 0; type < cNumShaders; type++)
    {
        json_node &state_node = node.add_object(get_shader_index_name(type));
        
        state_node.add_key_value("shader_obj", m_shader_objs[type]);

        // TODO : Probably need to serialize more here, see comment in snapshot code
    }
    
    return true;
}

bool vogl_sso_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    if (!node.is_object())
        return false;

    for (uint32_t type = 0; type < cNumShaders; type++)
    {
        const json_node *pState = node.find_child_object(get_shader_index_name(type));
        if (!pState)
            continue;

        m_shader_objs[type] = pState->value_as_uint32("shader_obj");
    }

    m_is_valid = true;

    return true;
}

bool vogl_sso_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTProgramPipeline)
        return false;

    const vogl_sso_state &rhs = static_cast<const vogl_sso_state &>(rhs_obj);
    
    for (uint32_t i = 0; i < cNumShaders; i++)
    {
        if (m_shader_objs[i] != rhs.m_shader_objs[i])
            return false;
    }

    return true;
}
