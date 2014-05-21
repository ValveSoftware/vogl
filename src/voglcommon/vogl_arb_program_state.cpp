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

// File: vogl_arb_program_state.cpp
#include "vogl_arb_program_state.h"

static const char *vogl_get_arb_program_target_name(GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT((target == GL_VERTEX_PROGRAM_ARB) || (target == GL_FRAGMENT_PROGRAM_ARB));
    return (target == GL_VERTEX_PROGRAM_ARB) ? "vertex" : "fragment";
}

vogl_arb_program_state::vogl_arb_program_state()
    : m_snapshot_handle(0),
      m_target(GL_NONE),
      m_error_position(-1),
      m_is_native(false),
      m_num_instructions(0),
      m_program_format(GL_NONE),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_arb_program_state::~vogl_arb_program_state()
{
    VOGL_FUNC_TRACER
}

GLint vogl_arb_program_state::get_program_int(GLenum pname) const
{
    VOGL_FUNC_TRACER

    GLint val = 0;
    GL_ENTRYPOINT(glGetProgramivARB)(m_target, pname, &val);
    VOGL_CHECK_GL_ERROR;
    return val;
}

bool vogl_arb_program_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

    clear();

    VOGL_CHECK_GL_ERROR;

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);
    m_target = target;

    if (m_target != GL_NONE)
    {
        vogl_scoped_state_saver state_saver(cGSTARBVertexProgram, cGSTARBFragmentProgram);

        GL_ENTRYPOINT(glBindProgramARB)(m_target, m_snapshot_handle);
        VOGL_CHECK_GL_ERROR;

        m_program_format = get_program_int(GL_PROGRAM_FORMAT_ARB);

        // Note: there doesn't seem a way to query if this program was compiled sucessfully (without trying to recompile it)?
        m_num_instructions = get_program_int(GL_PROGRAM_INSTRUCTIONS_ARB);
        m_is_native = get_program_int(GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB) != 0;

        int program_len = get_program_int(GL_PROGRAM_LENGTH_ARB);
        if ((program_len < 0) || (!m_program_string.try_resize(program_len)))
        {
            clear();
            return false;
        }

        if (program_len)
        {
            GL_ENTRYPOINT(glGetProgramStringARB)(m_target, GL_PROGRAM_STRING_ARB, m_program_string.get_ptr());
            VOGL_CHECK_GL_ERROR;
        }

        int num_local_params = get_program_int(GL_PROGRAM_PARAMETERS_ARB);
        if ((num_local_params < 0) || (!m_local_params.try_resize(num_local_params)))
        {
            clear();
            return false;
        }

        for (int i = 0; i < num_local_params; i++)
        {
            GL_ENTRYPOINT(glGetProgramLocalParameterfvARB)(m_target, i, m_local_params[i].get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_arb_program_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

    if (!m_is_valid)
        return false;

    bool created_handle = false;
    VOGL_NOTE_UNUSED(created_handle);

    VOGL_ASSERT(static_cast<GLuint>(handle) == handle);

    GLuint handle32 = static_cast<GLuint>(handle);

    if (!handle32)
    {
        GL_ENTRYPOINT(glGenProgramsARB)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;

        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_PROGRAM_ARB, m_snapshot_handle, handle, m_target);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_PROGRAM_ARB, m_snapshot_handle) == handle);

        created_handle = true;
    }

    if (m_target != GL_NONE)
    {
        vogl_scoped_state_saver state_saver(cGSTARBVertexProgram, cGSTARBFragmentProgram);

        GL_ENTRYPOINT(glBindProgramARB)(m_target, handle32);
        VOGL_CHECK_GL_ERROR;

        if (m_program_string.size() && (m_program_format != GL_NONE))
        {
            GL_ENTRYPOINT(glProgramStringARB)(m_target, m_program_format, m_program_string.size(), m_program_string.get_ptr());

            bool failed = vogl_check_gl_error();

            m_error_position = vogl_get_gl_integer(GL_PROGRAM_ERROR_POSITION_ARB);
            m_is_native = get_program_int(GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB) != 0;

            if ((failed) || (m_error_position >= 0) || (!m_is_native))
            {
                const GLubyte *pError_string = NULL;
                pError_string = static_cast<const GLubyte *>(GL_ENTRYPOINT(glGetString)(GL_PROGRAM_ERROR_STRING_ARB));
                VOGL_CHECK_GL_ERROR;

                m_error_string.set(pError_string ? reinterpret_cast<const char *>(pError_string) : "");

                vogl_error_printf("Failed restoring ARB %s shader, GL handle %u, error position %i, is native: %u, error string: %s\n",
                                 vogl_get_arb_program_target_name(m_target), handle32, m_error_position, m_is_native, m_error_string.get_ptr());
            }
        }

        for (uint32_t i = 0; i < m_local_params.size(); i++)
        {
            GL_ENTRYPOINT(glProgramLocalParameter4fvARB)(m_target, i, m_local_params[i].get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
    }

    return true;
}

bool vogl_arb_program_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    uint32_t replay_handle = m_snapshot_handle;
    VOGL_NOTE_UNUSED(replay_handle);

    uint32_t trace_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_PROGRAM_ARB, m_snapshot_handle));

    m_snapshot_handle = trace_handle;

    return true;
}

void vogl_arb_program_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_target = GL_NONE;

    m_error_position = -1;
    m_error_string.clear();
    m_is_native = false;
    m_program_format = GL_NONE;
    m_num_instructions = 0;

    m_program_string.clear();
    m_local_params.clear();

    m_is_valid = false;
}

bool vogl_arb_program_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    node.add_key_value("snapshot_handle", m_snapshot_handle);
    node.add_key_value("target", get_gl_enums().find_gl_name(m_target));
    node.add_key_value("error_position", m_error_position);
    node.add_key_value("error_string", m_error_string);
    node.add_key_value("is_native", m_is_native);
    node.add_key_value("num_instructions", m_num_instructions);
    node.add_key_value("program_format", get_gl_enums().find_gl_name(m_program_format));

    if (m_program_string.size())
    {
        dynamic_string prefix(cVarArg, "arb_%s_program", vogl_get_arb_program_target_name(m_target));

        dynamic_string id(blob_manager.add_buf_compute_unique_id(m_program_string.get_ptr(), m_program_string.size(), prefix.get_ptr(), "txt"));
        if (id.is_empty())
            return false;

        node.add_key_value("program_string", id);
    }

    if (m_local_params.size())
    {
        json_node &param_array = node.add_array("local_params");
        for (uint32_t i = 0; i < m_local_params.size(); i++)
            if (!vogl_json_serialize_vec4F(param_array.add_array(), m_local_params[i]))
                return false;
    }

    return true;
}

bool vogl_arb_program_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    if (!node.is_object())
        return false;

    m_snapshot_handle = node.value_as_int("snapshot_handle");
    m_target = vogl_get_json_value_as_enum(node, "target");
    m_error_position = node.value_as_int("error_position");
    m_error_string = node.value_as_string("error_string");
    m_is_native = node.value_as_bool("is_native");
    m_num_instructions = node.value_as_int("num_instructions");
    m_program_format = vogl_get_json_value_as_enum(node, "program_format");

    if (node.has_key("program_string"))
    {
        if (!blob_manager.get(node.value_as_string("program_string"), m_program_string))
        {
            clear();
            return false;
        }
    }

    const json_node *pParams_node = node.find_child_array("local_params");
    if (pParams_node)
    {
        if (!pParams_node->are_all_children_arrays())
            return false;

        m_local_params.resize(pParams_node->size());
        for (uint32_t i = 0; i < pParams_node->size(); i++)
        {
            if (!vogl_json_deserialize_vec4F(*pParams_node->get_child(i), m_local_params[i]))
            {
                clear();
                return false;
            }
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_arb_program_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (get_type() != rhs_obj.get_type())
        return false;

    const vogl_arb_program_state &rhs = static_cast<const vogl_arb_program_state &>(rhs_obj);

    if (this == &rhs)
        return true;

#define CMP(x)      \
    if (x != rhs.x) \
        return false;
    CMP(m_target);
    CMP(m_program_format);
    CMP(m_program_string);
    CMP(m_local_params.size());
#undef CMP

    for (uint32_t i = 0; i < m_local_params.size(); i++)
        if (!m_local_params[i].equal_tol(rhs.m_local_params[i], .00125f))
            return false;

    return true;
}

vogl_arb_program_environment_state::vogl_arb_program_environment_state()
    : m_is_valid(false)
{
    VOGL_FUNC_TRACER

    VOGL_ASSUME(cNumTargets == 2);

    utils::zero_object(m_cur_programs);
}

vogl_arb_program_environment_state::~vogl_arb_program_environment_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_arb_program_environment_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    clear();

    VOGL_CHECK_GL_ERROR;

    for (uint32_t i = 0; i < cNumTargets; i++)
    {
        GLenum target = get_target_enum(i);

        GL_ENTRYPOINT(glGetProgramivARB)(target, GL_PROGRAM_BINDING_ARB, reinterpret_cast<GLint *>(&m_cur_programs[i]));
        VOGL_CHECK_GL_ERROR;

        GLuint num = (i == cVertexTarget) ? context_info.get_max_arb_vertex_program_env_params() : context_info.get_max_arb_fragment_program_env_params();

        m_env_params[i].resize(num);
        for (uint32_t j = 0; j < num; j++)
        {
            GL_ENTRYPOINT(glGetProgramEnvParameterfvARB)(target, j, m_env_params[i][j].get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_arb_program_environment_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &trace_to_replay_remapper) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    VOGL_CHECK_GL_ERROR;

    for (uint32_t i = 0; i < cNumTargets; i++)
    {
        GLenum target = get_target_enum(i);

        GLuint binding = static_cast<GLuint>(trace_to_replay_remapper.remap_handle(VOGL_NAMESPACE_PROGRAM_ARB, m_cur_programs[i]));

        GL_ENTRYPOINT(glBindProgramARB)(target, binding);
        VOGL_CHECK_GL_ERROR;

        GLuint num = (i == cVertexTarget) ? context_info.get_max_arb_vertex_program_env_params() : context_info.get_max_arb_fragment_program_env_params();

        if (m_env_params[i].size() > num)
            vogl_error_printf("Context only supports %u max ARB program env programs, but the snapshot has %u params\n", num, m_env_params[i].size());

        num = math::minimum<uint32_t>(num, m_env_params[i].size());

        for (uint32_t j = 0; j < num; j++)
        {
            GL_ENTRYPOINT(glProgramEnvParameter4fvARB)(target, j, m_env_params[i][j].get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
    }

    return true;
}

bool vogl_arb_program_environment_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    for (uint32_t target = 0; target < cNumTargets; target++)
    {
        uint32_t replay_handle = m_cur_programs[target];
        if (replay_handle)
        {
            uint32_t trace_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_PROGRAM_ARB, replay_handle));

            m_cur_programs[target] = trace_handle;
        }
    }

    return true;
}

void vogl_arb_program_environment_state::clear()
{
    VOGL_FUNC_TRACER

    utils::zero_object(m_cur_programs);

    m_is_valid = false;

    for (uint32_t i = 0; i < cNumTargets; i++)
        m_env_params[i].clear();
}

bool vogl_arb_program_environment_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    for (uint32_t target = 0; target < cNumTargets; target++)
    {
        json_node &state_node = node.add_object(get_target_index_name(target));

        state_node.add_key_value("cur_program", m_cur_programs[target]);

        json_node &env_params_node = state_node.add_array("env_params");

        for (uint32_t j = 0; j < m_env_params[target].size(); j++)
            if (!vogl_json_serialize_vec4F(env_params_node.add_array(), m_env_params[target][j]))
                return false;
    }

    return true;
}

bool vogl_arb_program_environment_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    if (!node.is_object())
        return false;

    for (uint32_t target = 0; target < cNumTargets; target++)
    {
        const json_node *pState = node.find_child_object(get_target_index_name(target));
        if (!pState)
            continue;

        m_cur_programs[target] = pState->value_as_uint32("cur_program");

        const json_node *pArray = pState->find_child_array("env_params");
        if (pArray)
        {
            m_env_params[target].resize(pArray->size());
            for (uint32_t i = 0; i < pArray->size(); i++)
            {
                if (!vogl_json_deserialize_vec4F(*pArray->get_value_as_array(i), m_env_params[target][i]))
                {
                    clear();
                    return false;
                }
            }
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_arb_program_environment_state::compare_restorable_state(const vogl_arb_program_environment_state &rhs) const
{
    VOGL_FUNC_TRACER

    if (m_is_valid != rhs.m_is_valid)
        return false;

    if (!m_is_valid)
        return true;

    for (uint32_t i = 0; i < cNumTargets; i++)
    {
        if (m_cur_programs[i] != rhs.m_cur_programs[i])
            return false;

        if (m_env_params[i].size() != rhs.m_env_params[i].size())
            return false;

        for (uint32_t j = 0; j < m_env_params[i].size(); j++)
            if (!m_env_params[i][j].equal_tol(rhs.m_env_params[i][j], .00125f))
                return false;
    }

    return true;
}
