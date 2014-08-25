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

// These global mapping tables are built to correspond to shader type enum that's
//  declared in the header and included here in comment form for convenience
//  enum
//      {
//          cVertexShader,
//          cFragmentShader,
//          cGeometryShader,
//          cTessControlShader,
//          cTessEvalShader,
//          cNumShaders
//      };
static const GLenum gl_shader_type_mapping_data[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER};
const GLenum* vogl_sso_state::gl_shader_type_mapping = gl_shader_type_mapping_data;
static const GLbitfield gl_shader_bitmask_mapping[] = {0x01, 0x02, 0x04, 0x08, 0x10};

vogl_sso_state::vogl_sso_state()
    : m_snapshot_handle(0),
      m_has_been_bound(false),
      m_active_program(0),
      m_info_log_length(0),
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
        // Re-Bind PPO before querying state
        GL_ENTRYPOINT(glBindProgramPipeline)(m_snapshot_handle);
        // If GS not supported then only process VS & FS (this also skips FS types, which is not ideal but works for Intel MESA)
        const uint32_t num_shader_types = context_info.supports_extension("GL_ARB_geometry_shader4") ? cNumShaders : 2;
        for (uint32_t i = 0; i < num_shader_types; i++)
            m_shader_objs[i] = vogl_get_program_pipeline(m_snapshot_handle, gl_shader_type_mapping[i]);
        
        m_info_log_length = vogl_get_program_pipeline(m_snapshot_handle, GL_INFO_LOG_LENGTH);
        m_active_program = vogl_get_program_pipeline(m_snapshot_handle, GL_ACTIVE_PROGRAM);
        // TODO : Could also query VALIDATE_STATUS & INFO_LOG
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

    bool created_handle = false;
    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenProgramPipelines)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            goto handle_error;
        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_PIPELINES, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_PIPELINES, m_snapshot_handle) == handle);

        created_handle = true;
    }

    VOGL_ASSERT(handle <= cUINT32_MAX);

    if (m_has_been_bound)
    {
        GL_ENTRYPOINT(glBindProgramPipeline)(static_cast<GLuint>(handle));
        if (vogl_check_gl_error())
            goto handle_error;
        
        // If GS not supported then only process VS & FS (this also skips FS types, which is not ideal but works for Intel MESA)
        const uint32_t num_shader_types = context_info.supports_extension("GL_ARB_geometry_shader4") ? cNumShaders : 2;
        // Use appropriate programs for this pipeline
        for (uint32_t type = 0; type < num_shader_types; type++)
        {
            if (0 != m_shader_objs[type])
                GL_ENTRYPOINT(glUseProgramStages)(m_snapshot_handle, gl_shader_bitmask_mapping[type], m_shader_objs[type]);
        }
        if (0 != m_active_program)
            GL_ENTRYPOINT(glActiveShaderProgram)(m_snapshot_handle, m_active_program);
    }

    return true;
handle_error:
    vogl_error_printf("Failed restoring trace program pipeline %i, GL program pipeline %" PRIu64 "\n", m_snapshot_handle, (uint64_t)handle);

    GL_ENTRYPOINT(glBindProgramPipeline)(0);
    VOGL_CHECK_GL_ERROR;

    if (created_handle)
    {
        remapper.delete_handle_and_object(VOGL_NAMESPACE_PIPELINES, m_snapshot_handle, handle);
        handle = 0;
    }

    return false;
}

void vogl_sso_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_has_been_bound = false;

    for (uint32_t i = 0; i < cNumShaders; i++)
        m_shader_objs[i] = 0;
    m_active_program = 0;
    m_info_log_length = 0;
    m_is_valid = false;
}

bool vogl_sso_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    // Remap Pipeline handle
    uint32_t replay_handle = m_snapshot_handle;
    uint32_t trace_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_PIPELINES, m_snapshot_handle));
    m_snapshot_handle = trace_handle;
    // Make sure that we also have proper mapping for programs attached to pipeline
    for (uint32_t target = 0; target < cNumShaders; target++)
    {
        replay_handle = m_shader_objs[target];
        if (replay_handle)
        {
            trace_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_PROGRAMS, replay_handle));
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

    node.add_key_value("handle", m_snapshot_handle);
    for (uint32_t type = 0; type < cNumShaders; type++)
    {
        json_node &state_node = node.add_object(get_shader_index_name(type));
        
        state_node.add_key_value("shader_obj", m_shader_objs[type]);

        // TODO : Probably need to serialize more here, see comment in snapshot code
    }
    node.add_key_value("active_program", m_active_program);
    node.add_key_value("info_log_length", m_info_log_length);
    
    return true;
}

bool vogl_sso_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    if (!node.is_object())
        return false;

    m_snapshot_handle = node.value_as_uint32("handle");
    for (uint32_t type = 0; type < cNumShaders; type++)
    {
        const json_node *pState = node.find_child_object(get_shader_index_name(type));
        if (!pState)
            continue;

        m_shader_objs[type] = pState->value_as_uint32("shader_obj");
    }
    m_active_program = node.value_as_uint32("active_program");
    m_info_log_length = node.value_as_uint32("info_log_length");

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
