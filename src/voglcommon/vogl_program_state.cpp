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

// File: vogl_program_state.cpp
// TODO: Remap uniform block locations

#include "vogl_program_state.h"

#define VOGL_PROGRAM_VERSION 0x0101

static bool get_program_bool(GLuint handle, GLenum pname)
{
    VOGL_FUNC_TRACER

    GLint val = false;
    GL_ENTRYPOINT(glGetProgramiv)(handle, pname, &val);
    VOGL_CHECK_GL_ERROR;
    return val != 0;
}

static int get_program_int(GLuint handle, GLenum pname)
{
    VOGL_FUNC_TRACER

    GLint val = false;
    GL_ENTRYPOINT(glGetProgramiv)(handle, pname, &val);
    VOGL_CHECK_GL_ERROR;
    return val;
}

vogl_program_state::vogl_program_state()
    : m_link_entrypoint(VOGL_ENTRYPOINT_INVALID),
      m_snapshot_handle(0),
      m_program_binary_format(GL_NONE),
      m_num_active_attribs(0),
      m_num_active_uniforms(0),
      m_num_active_uniform_blocks(0),
      m_transform_feedback_mode(GL_NONE),
      m_transform_feedback_num_varyings(0),
      m_create_shader_program_type(GL_NONE),
      m_marked_for_deletion(false),
      m_link_status(false),
      m_separable(false),
      m_verify_status(false),
      m_link_snapshot(false),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_program_state::vogl_program_state(const vogl_program_state &other)
    : m_link_entrypoint(VOGL_ENTRYPOINT_INVALID),
      m_snapshot_handle(0),
      m_program_binary_format(GL_NONE),
      m_num_active_attribs(0),
      m_num_active_uniforms(0),
      m_num_active_uniform_blocks(0),
      m_transform_feedback_mode(GL_NONE),
      m_transform_feedback_num_varyings(0),
      m_create_shader_program_type(GL_NONE),
      m_marked_for_deletion(false),
      m_link_status(false),
      m_separable(false),
      m_verify_status(false),
      m_link_snapshot(false),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
    *this = other;
}

vogl_program_state::~vogl_program_state()
{
    VOGL_FUNC_TRACER
}

vogl_program_state &vogl_program_state::operator=(const vogl_program_state &rhs)
{
    VOGL_FUNC_TRACER

    if (this == &rhs)
        return *this;

    clear();

#define CPY(x) x = rhs.x;

    CPY(m_link_entrypoint);
    CPY(m_snapshot_handle);

    CPY(m_program_binary);
    CPY(m_program_binary_format);

    CPY(m_attached_shaders);

    CPY(m_info_log);

    CPY(m_num_active_attribs);
    CPY(m_num_active_uniforms);
    CPY(m_num_active_uniform_blocks);

    CPY(m_attribs);
    CPY(m_uniforms);
    CPY(m_uniform_blocks);

    CPY(m_shaders);
    CPY(m_outputs);

    CPY(m_transform_feedback_mode);
    CPY(m_transform_feedback_num_varyings);
    CPY(m_varyings);

    CPY(m_create_shader_program_type);
    CPY(m_create_shader_program_strings);

    CPY(m_marked_for_deletion);
    CPY(m_link_status);
    CPY(m_separable);
    CPY(m_verify_status);
    CPY(m_link_snapshot);

    CPY(m_is_valid);

#undef CPY

    if (rhs.m_pLink_time_snapshot.get())
    {
        m_pLink_time_snapshot.reset(vogl_new(vogl_program_state, *(rhs.m_pLink_time_snapshot.get())));
    }

    return *this;
}

bool vogl_program_state::snapshot_uniforms(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

    growable_array<GLchar, 4096> temp_buf;

    // Snapshot active uniforms
    GLint active_uniform_max_length = math::maximum(1024, get_program_int(m_snapshot_handle, GL_ACTIVE_UNIFORM_MAX_LENGTH));
    temp_buf.resize(active_uniform_max_length);
    m_uniforms.resize(m_num_active_uniforms);
    for (uint32_t uniform_iter = 0; uniform_iter < m_num_active_uniforms; uniform_iter++)
    {
        GLsizei actual_len = 0;
        GLint size = 0;
        GLenum type = GL_NONE;

        GL_ENTRYPOINT(glGetActiveUniform)(m_snapshot_handle, uniform_iter, active_uniform_max_length, &actual_len, &size, &type, temp_buf.get_ptr());
        VOGL_CHECK_GL_ERROR;

        vogl_program_uniform_state &uniform = m_uniforms[uniform_iter];
        uniform.m_size = size;
        uniform.m_type = type;
        uniform.m_name.set(reinterpret_cast<char *>(temp_buf.get_ptr()));
        uniform.m_base_location = -1;

        if (!uniform.m_name.begins_with("gl_", true))
        {
            uniform.m_base_location = GL_ENTRYPOINT(glGetUniformLocation)(m_snapshot_handle, temp_buf.get_ptr());
            VOGL_CHECK_GL_ERROR;
        }

        uint32_t type_size_in_bytes = vogl_gl_get_uniform_size_in_bytes(type);
        if (!type_size_in_bytes)
            continue;

        GLenum base_type = vogl_gl_get_uniform_base_type(type);
        if (base_type == GL_NONE)
            continue;

        uniform.m_data.resize(size * type_size_in_bytes);

        if (uniform.m_base_location != -1)
        {
            for (int element = 0; element < size; element++)
            {
                uint8_t *pDst = &uniform.m_data[element * type_size_in_bytes];

                if (base_type == GL_FLOAT)
                    GL_ENTRYPOINT(glGetUniformfv)(m_snapshot_handle, uniform.m_base_location + element, (GLfloat *)pDst);
                else if (base_type == GL_DOUBLE)
                    GL_ENTRYPOINT(glGetUniformdv)(m_snapshot_handle, uniform.m_base_location + element, (GLdouble *)pDst);
                else if (base_type == GL_UNSIGNED_INT)
                    GL_ENTRYPOINT(glGetUniformuiv)(m_snapshot_handle, uniform.m_base_location + element, (GLuint *)pDst);
                else
                    GL_ENTRYPOINT(glGetUniformiv)(m_snapshot_handle, uniform.m_base_location + element, (GLint *)pDst);

                VOGL_CHECK_GL_ERROR;
            }
        }
    }

    return true;
}

bool vogl_program_state::snapshot_uniform_blocks(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);

    // Snapshot active uniform blocks
    if ((context_info.get_version() >= VOGL_GL_VERSION_3_1) && GL_ENTRYPOINT(glGetActiveUniformBlockiv) && GL_ENTRYPOINT(glGetActiveUniformBlockName))
    {
        growable_array<char, 256> name_buf;

        m_uniform_blocks.reserve(m_num_active_uniform_blocks);

        for (uint32_t uniform_block_index = 0; uniform_block_index < m_num_active_uniform_blocks; uniform_block_index++)
        {
            GLint name_len = 0;
            GL_ENTRYPOINT(glGetActiveUniformBlockiv)(m_snapshot_handle, uniform_block_index, GL_UNIFORM_BLOCK_NAME_LENGTH, &name_len);
            VOGL_CHECK_GL_ERROR;

            vogl_program_uniform_block_state state;
            state.clear();
            state.m_uniform_block_index = uniform_block_index;

            name_buf.resize(name_len);

            GLsizei actual_len = 0;
            GL_ENTRYPOINT(glGetActiveUniformBlockName)(m_snapshot_handle, uniform_block_index, name_len, &actual_len, reinterpret_cast<GLchar *>(name_buf.get_ptr()));
            VOGL_CHECK_GL_ERROR;

            if (name_buf.size())
                state.m_name.set(name_buf.get_ptr());

            GL_ENTRYPOINT(glGetActiveUniformBlockiv)(m_snapshot_handle, uniform_block_index, GL_UNIFORM_BLOCK_BINDING, &state.m_uniform_block_binding_point);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glGetActiveUniformBlockiv)(m_snapshot_handle, uniform_block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &state.m_uniform_block_data_size);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glGetActiveUniformBlockiv)(m_snapshot_handle, uniform_block_index, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &state.m_uniform_block_active_uniforms);
            VOGL_CHECK_GL_ERROR;

#define GET_REFERENCED_BY(name, flag_name) do { \
        GLint referenced; \
        GL_ENTRYPOINT(glGetActiveUniformBlockiv)(m_snapshot_handle, uniform_block_index, GL_UNIFORM_BLOCK_REFERENCED_BY_ ## name ## _SHADER, &referenced); \
        VOGL_CHECK_GL_ERROR; \
        if (referenced) \
            state.m_referenced_by |= cBufferReferencedBy ## flag_name; \
    } while (0)

            GET_REFERENCED_BY(VERTEX, Vertex);
            GET_REFERENCED_BY(TESS_CONTROL, TesselationControl);
            GET_REFERENCED_BY(TESS_EVALUATION, TesselationEvaluation);
            GET_REFERENCED_BY(GEOMETRY, Geometry);
            GET_REFERENCED_BY(FRAGMENT, Fragment);
            if (context_info.get_version() >= VOGL_GL_VERSION_4_3)
                GET_REFERENCED_BY(COMPUTE, Compute);

#undef GET_REFERENCED_BY

            m_uniform_blocks.push_back(state);
        }
    }

    return true;
}

bool vogl_program_state::snapshot_active_attribs(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

    growable_array<GLchar, 4096> temp_buf;

    // Snapshot active attribs
    GLint active_attrib_max_length = math::maximum(1024, get_program_int(m_snapshot_handle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH));
    temp_buf.resize(active_attrib_max_length);

    m_attribs.resize(m_num_active_attribs);
    for (uint32_t attrib_iter = 0; attrib_iter < m_num_active_attribs; attrib_iter++)
    {
        GLint size = 0;
        GLenum type = GL_NONE;
        GLsizei actual_len = 0;

        GL_ENTRYPOINT(glGetActiveAttrib)(m_snapshot_handle, attrib_iter, active_attrib_max_length, &actual_len, &size, &type, temp_buf.get_ptr());
        VOGL_CHECK_GL_ERROR;

        vogl_program_attrib_state &attrib = m_attribs[attrib_iter];
        attrib.m_name.set(reinterpret_cast<char *>(temp_buf.get_ptr()));

        GLint loc = -1;
        if (!attrib.m_name.begins_with("gl_", true))
        {
            loc = GL_ENTRYPOINT(glGetAttribLocation)(m_snapshot_handle, temp_buf.get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
        attrib.m_bound_location = loc;

        attrib.m_size = size;
        attrib.m_type = type;
    }

    return true;
}

bool vogl_program_state::snapshot_attached_shaders(const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool linked_using_binary)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

    GLint num_attached_shaders = get_program_int(m_snapshot_handle, GL_ATTACHED_SHADERS);
    if (num_attached_shaders)
    {
        m_attached_shaders.resize(num_attached_shaders);

        GLsizei actual_count = 0;
        GL_ENTRYPOINT(glGetAttachedShaders)(m_snapshot_handle, num_attached_shaders, &actual_count, m_attached_shaders.get_ptr());
        VOGL_CHECK_GL_ERROR;

        VOGL_ASSERT(static_cast<uint32_t>(actual_count) == m_attached_shaders.size());

        // We can't trust glIsShader(), we must check our own shadow to determine if these are our handles or not.
        attached_shader_vec actual_attached_shaders;

        for (GLsizei i = 0; i < actual_count; i++)
        {
            if (remapper.is_valid_handle(VOGL_NAMESPACE_SHADERS, m_attached_shaders[i]))
                actual_attached_shaders.push_back(m_attached_shaders[i]);
            else if (!linked_using_binary)
                vogl_warning_printf("GL shader %u attached to GL program %u cannot be found in our object shadow\n", m_attached_shaders[i], m_snapshot_handle);
        }

        m_attached_shaders.swap(actual_attached_shaders);
        num_attached_shaders = m_attached_shaders.size();
    }

    return true;
}

bool vogl_program_state::snapshot_shader_objects(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    m_shaders.resize(m_attached_shaders.size());
    for (uint32_t i = 0; i < m_shaders.size(); i++)
    {
        GLuint handle = m_attached_shaders[i];

        if (!m_shaders[i].snapshot(context_info, remapper, handle, GL_NONE))
            return false;
    }

    return true;
}

bool vogl_program_state::snapshot_info_log(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

    GLint info_log_len = get_program_int(m_snapshot_handle, GL_INFO_LOG_LENGTH);

    growable_array<GLchar, 4096> temp_buf(info_log_len);

    // Snapshot program info log
    if (info_log_len)
    {
        GLsizei actual_len = 0;
        GL_ENTRYPOINT(glGetProgramInfoLog)(m_snapshot_handle, info_log_len, &actual_len, temp_buf.get_ptr());

        m_info_log.set(reinterpret_cast<char *>(temp_buf.get_ptr()));
    }

    return true;
}

bool vogl_program_state::snapshot_program_binary(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);

// HACK HACK - this crashes AMD's driver in Sam3 in interactive mode when we try to get the program binary's length, no idea why yet
#if 0
	if (m_link_status)
	{
		int retrievable_hint = get_program_int(m_snapshot_handle, GL_PROGRAM_BINARY_RETRIEVABLE_HINT);
		if (retrievable_hint)
		{
			int program_binary_length = get_program_int(m_snapshot_handle, GL_PROGRAM_BINARY_LENGTH);

			if (!vogl_check_gl_error() && program_binary_length)
			{
				m_program_binary.resize(program_binary_length);

				GLsizei actual_len = 0;
				GL_ENTRYPOINT(glGetProgramBinary)(m_snapshot_handle, program_binary_length, &actual_len, &m_program_binary_format, m_program_binary.get_ptr());

				if (!vogl_check_gl_error())
				{
					m_program_binary.resize(actual_len);
				}
				else
				{
					vogl_error_printf("Failed retrieving program binary for GL program %u!\n", m_snapshot_handle);

					m_program_binary.clear();
					m_program_binary_format = GL_NONE;
				}
			}
		}
	}
#endif

    return true;
}

bool vogl_program_state::snapshot_basic_info(const vogl_context_info &context_info, vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);

    m_link_status = get_program_bool(m_snapshot_handle, GL_LINK_STATUS);
    if (context_info.supports_extension("GL_ARB_separate_shader_objects"))
        m_separable = get_program_bool(m_snapshot_handle, GL_PROGRAM_SEPARABLE);
    m_marked_for_deletion = get_program_bool(m_snapshot_handle, GL_DELETE_STATUS);
    m_verify_status = get_program_bool(m_snapshot_handle, GL_VALIDATE_STATUS);
    m_num_active_attribs = get_program_int(m_snapshot_handle, GL_ACTIVE_ATTRIBUTES);
    m_num_active_uniforms = get_program_int(m_snapshot_handle, GL_ACTIVE_UNIFORMS);

    if ((context_info.get_version() >= VOGL_GL_VERSION_3_1) && GL_ENTRYPOINT(glGetActiveUniformBlockiv))
        m_num_active_uniform_blocks = get_program_int(m_snapshot_handle, GL_ACTIVE_UNIFORM_BLOCKS);

    return true;
}

bool vogl_program_state::snapshot_outputs(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint program)
{
    VOGL_FUNC_TRACER
    VOGL_NOTE_UNUSED(remapper);

    if ((!context_info.supports_extension("GL_ARB_program_interface_query")) ||
        (!GL_ENTRYPOINT(glGetProgramInterfaceiv)) || (!GL_ENTRYPOINT(glGetProgramResourceName)) || (!GL_ENTRYPOINT(glGetProgramResourceiv)))
    {
        return true;
    }

    GLint num_active_outputs = 0;
    GL_ENTRYPOINT(glGetProgramInterfaceiv)(program, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &num_active_outputs);
    VOGL_CHECK_GL_ERROR;

    m_outputs.resize(num_active_outputs);
    for (int i = 0; i < num_active_outputs; i++)
    {
        vogl_program_output_state &output = m_outputs[i];

        output.clear();

        GLchar name[256];
        GLsizei name_len = 0;
        GL_ENTRYPOINT(glGetProgramResourceName)(program, GL_PROGRAM_OUTPUT, i, sizeof(name), &name_len, name);
        VOGL_CHECK_GL_ERROR;

        const GLenum props_to_query[5] = { GL_LOCATION, GL_LOCATION_INDEX, GL_TYPE, GL_ARRAY_SIZE, GL_IS_PER_PATCH }; // TODO: GL_LOCATION_COMPONENT
        GLint props[5] = { 0, 0, 0, 0, 0 };
        GL_ENTRYPOINT(glGetProgramResourceiv)(program, GL_PROGRAM_OUTPUT, i, VOGL_ARRAY_SIZE(props_to_query), props_to_query, VOGL_ARRAY_SIZE(props), NULL, props);
        VOGL_CHECK_GL_ERROR;

        output.m_name = reinterpret_cast<const char *>(name);
        output.m_location = props[0];
        output.m_location_index = props[1];
        output.m_type = props[2];
        output.m_array_size = props[3];
        output.m_is_per_patch = props[4] != 0;
    }

    return true;
}

bool vogl_program_state::snapshot_transform_feedback(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint program)
{
    VOGL_FUNC_TRACER
    VOGL_NOTE_UNUSED(remapper);
    VOGL_NOTE_UNUSED(context_info);

    // The latest/current mode
    GL_ENTRYPOINT(glGetProgramiv)(program, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, reinterpret_cast<GLint *>(&m_transform_feedback_mode));
    VOGL_CHECK_GL_ERROR;

    // This is the linked state, NOT the current state
    GL_ENTRYPOINT(glGetProgramiv)(program, GL_TRANSFORM_FEEDBACK_VARYINGS, reinterpret_cast<GLint *>(&m_transform_feedback_num_varyings));
    VOGL_CHECK_GL_ERROR;

    m_varyings.resize(m_transform_feedback_num_varyings);

    for (uint32_t i = 0; i < m_transform_feedback_num_varyings; i++)
    {
        GLchar name[512] = { '\0' };
        GLsizei length = 0, size = 0;
        GLenum type = GL_NONE;

        // This is the linked state, NOT the current state
        GL_ENTRYPOINT(glGetTransformFeedbackVarying)(program, i, sizeof(name), &length, &size, &type, name);
        VOGL_CHECK_GL_ERROR;

        vogl_program_transform_feedback_varying &varying = m_varyings[i];
        varying.clear();

        varying.m_index = i;
        varying.m_name.set(reinterpret_cast<const char *>(name));
        varying.m_size = size;
        varying.m_type = type;
    }

    return true;
}

bool vogl_program_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(target);

    VOGL_CHECK_GL_ERROR;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<GLuint>(handle);

    if (!snapshot_basic_info(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_outputs(context_info, remapper, m_snapshot_handle))
    {
        clear();
        return false;
    }

    if (!snapshot_program_binary(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_info_log(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_attached_shaders(context_info, remapper, false))
    {
        clear();
        return false;
    }

    if (!snapshot_active_attribs(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_uniforms(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_uniform_blocks(context_info, remapper))
    {
        clear();
        return false;
    }

    // We'll snapshot this stuff, although some of it is linked state.
    if (!snapshot_transform_feedback(context_info, remapper, m_snapshot_handle))
    {
        clear();
        return false;
    }

    m_link_snapshot = false;
    m_is_valid = true;

    return true;
}

bool vogl_program_state::link_snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, gl_entrypoint_id_t link_entrypoint, GLuint handle, const void *pBinary, uint32_t binary_size, GLenum binary_format, GLenum type, GLsizei count, GLchar *const *strings)
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;

    clear();

    m_link_entrypoint = link_entrypoint;
    m_snapshot_handle = handle;

    m_create_shader_program_type = type;
    if (strings)
    {
        m_create_shader_program_strings.resize(count);
        for (int i = 0; i < count; i++)
        {
            m_create_shader_program_strings[i].set(strings[i] ? reinterpret_cast<const char *>(strings[i]) : "");
        }
    }

    bool linked_using_binary = pBinary && binary_size;

    if (!snapshot_basic_info(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_outputs(context_info, remapper, handle))
    {
        clear();
        return false;
    }

    if (linked_using_binary)
        m_program_binary.append(static_cast<const uint8_t *>(pBinary), binary_size);

    m_program_binary_format = binary_format;

    if (!snapshot_info_log(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_active_attribs(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!snapshot_attached_shaders(context_info, remapper, linked_using_binary))
    {
        clear();
        return false;
    }

    if (!snapshot_shader_objects(context_info, remapper))
    {
        clear();
        return false;
    }

    if (!linked_using_binary)
    {
        if (!snapshot_program_binary(context_info, remapper))
        {
            clear();
            return false;
        }
    }

    if (!snapshot_transform_feedback(context_info, remapper, handle))
    {
        clear();
        return false;
    }

    if ((m_link_status) && (!m_attached_shaders.size()) && (!linked_using_binary) && (type == GL_NONE))
    {
        vogl_error_printf("Program %u was successfully linked, but there are no attached shaders!\n", m_snapshot_handle);
    }

    // We don't care about the attached shader handles, we've now snapshotted and copied the ACTUAL shaders.
    m_attached_shaders.clear();

    m_link_snapshot = true;
    m_is_valid = true;

    return true;
}

void vogl_program_state::set_link_time_snapshot(vogl_unique_ptr<vogl_program_state> &pSnapshot)
{
    VOGL_FUNC_TRACER

    if (pSnapshot.get() && !pSnapshot->is_valid())
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    m_pLink_time_snapshot = pSnapshot;
}

bool vogl_program_state::restore_uniforms(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    const GLint num_active_uniforms = get_program_int(handle32, GL_ACTIVE_UNIFORMS);

    const GLint active_uniform_max_length = math::maximum(1024, get_program_int(handle32, GL_ACTIVE_UNIFORM_MAX_LENGTH));

    growable_array<GLchar, 1024> temp_buf(active_uniform_max_length);

    vogl_uniform_state_vec uniforms_to_restore(num_active_uniforms);

    // Restore active uniforms
    for (int uniform_iter = 0; uniform_iter < num_active_uniforms; uniform_iter++)
    {
        GLsizei actual_len = 0;
        GLint size = 0;
        GLenum type = GL_NONE;

        GL_ENTRYPOINT(glGetActiveUniform)(handle32, uniform_iter, temp_buf.size(), &actual_len, &size, &type, temp_buf.get_ptr());
        if (vogl_check_gl_error())
            any_gl_errors = true;

        VOGL_ASSERT(actual_len <= static_cast<GLsizei>(temp_buf.size()));

        vogl_program_uniform_state &uniform = uniforms_to_restore[uniform_iter];
        uniform.m_size = size;
        uniform.m_type = type;
        uniform.m_name.set(reinterpret_cast<char *>(temp_buf.get_ptr()));
        uniform.m_base_location = -1;

        if (!uniform.m_name.begins_with("gl_", true))
        {
            uniform.m_base_location = GL_ENTRYPOINT(glGetUniformLocation)(handle32, temp_buf.get_ptr());
            if (vogl_check_gl_error())
                any_gl_errors = true;
        }
    } // uniform_iter

    for (uint32_t uniform_iter = 0; uniform_iter < m_uniforms.size(); uniform_iter++)
    {
        const vogl_program_uniform_state &trace_uniform = m_uniforms[uniform_iter];
        if ((trace_uniform.m_name.begins_with("_gl", true)) || (trace_uniform.m_base_location < 0) || (!trace_uniform.m_data.size()))
            continue;

        uint32_t restore_uniform_index;
        for (restore_uniform_index = 0; restore_uniform_index < uniforms_to_restore.size(); restore_uniform_index++)
            if (uniforms_to_restore[restore_uniform_index].m_name.compare(trace_uniform.m_name, true) == 0)
                break;

        if (restore_uniform_index == uniforms_to_restore.size())
        {
            vogl_warning_printf("Failed finding trace uniform \"%s\", trace program %u GL program %u\n",
                               trace_uniform.m_name.get_ptr(), m_snapshot_handle, handle32);
            any_restore_warnings = true;
            continue;
        }

        const vogl_program_uniform_state &restore_uniform = uniforms_to_restore[restore_uniform_index];

        if (restore_uniform.m_base_location < 0)
        {
            vogl_warning_printf("Trace uniform \"%s\"'s type %s was found in the restored program but the restore handle is invalid, trace program %u GL program %u.\n",
                               trace_uniform.m_name.get_ptr(), get_gl_enums().find_gl_name(trace_uniform.m_type), m_snapshot_handle, handle32);
            any_restore_warnings = true;
            continue;
        }

        if (restore_uniform.m_type != trace_uniform.m_type)
        {
            vogl_warning_printf("Trace uniform \"%s\"'s type (%s) is different from restored trace program type (%s), trace program %u GL program %u. Uniform type conversion is not yet supported, skipping uniform!\n",
                               trace_uniform.m_name.get_ptr(), get_gl_enums().find_gl_name(trace_uniform.m_type), get_gl_enums().find_gl_name(restore_uniform.m_type),
                               m_snapshot_handle, handle32);
            any_restore_warnings = true;
            continue;
        }

        if (restore_uniform.m_size != trace_uniform.m_size)
        {
            vogl_warning_printf("Trace uniform \"%s\" type %s array size (%u) is different from restored program's array size (%u), trace program %u GL program %u. Will set as many array entries as possible and hope for the best.\n",
                               trace_uniform.m_name.get_ptr(), get_gl_enums().find_gl_name(trace_uniform.m_type), trace_uniform.m_size, restore_uniform.m_size,
                               m_snapshot_handle, handle32);
            any_restore_warnings = true;
        }

        const uint32_t array_size = math::minimum<uint32_t>(restore_uniform.m_size, trace_uniform.m_size);
        const void *pTrace_data = trace_uniform.m_data.get_ptr();
        const GLint restore_location = restore_uniform.m_base_location;

        for (uint32_t i = 0; i < array_size; i++)
            remapper.declare_location(m_snapshot_handle, handle32, trace_uniform.m_base_location + i, restore_location + i);

        if (array_size)
        {
            switch (restore_uniform.m_type)
            {
                case GL_DOUBLE:
                {
                    GL_ENTRYPOINT(glUniform1dv)(restore_location, array_size, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_VEC2:
                {
                    GL_ENTRYPOINT(glUniform2dv)(restore_location, array_size, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_VEC3:
                {
                    GL_ENTRYPOINT(glUniform3dv)(restore_location, array_size, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_VEC4:
                {
                    GL_ENTRYPOINT(glUniform4dv)(restore_location, array_size, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT2:
                {
                    GL_ENTRYPOINT(glUniformMatrix2dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT3:
                {
                    GL_ENTRYPOINT(glUniformMatrix3dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT4:
                {
                    GL_ENTRYPOINT(glUniformMatrix4dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT2x3:
                {
                    GL_ENTRYPOINT(glUniformMatrix2x3dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT3x2:
                {
                    GL_ENTRYPOINT(glUniformMatrix3x2dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT2x4:
                {
                    GL_ENTRYPOINT(glUniformMatrix2x4dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT4x2:
                {
                    GL_ENTRYPOINT(glUniformMatrix4x2dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT3x4:
                {
                    GL_ENTRYPOINT(glUniformMatrix3x4dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }
                case GL_DOUBLE_MAT4x3:
                {
                    GL_ENTRYPOINT(glUniformMatrix4x3dv)(restore_location, array_size, GL_FALSE, static_cast<const GLdouble *>(pTrace_data));
                    break;
                }

                case GL_FLOAT:
                {
                    GL_ENTRYPOINT(glUniform1fv)(restore_location, array_size, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_VEC2:
                {
                    GL_ENTRYPOINT(glUniform2fv)(restore_location, array_size, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_VEC3:
                {
                    GL_ENTRYPOINT(glUniform3fv)(restore_location, array_size, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_VEC4:
                {
                    GL_ENTRYPOINT(glUniform4fv)(restore_location, array_size, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT2:
                {
                    GL_ENTRYPOINT(glUniformMatrix2fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT3:
                {
                    GL_ENTRYPOINT(glUniformMatrix3fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT4:
                {
                    GL_ENTRYPOINT(glUniformMatrix4fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT2x3:
                {
                    GL_ENTRYPOINT(glUniformMatrix2x3fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT3x2:
                {
                    GL_ENTRYPOINT(glUniformMatrix3x2fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT2x4:
                {
                    GL_ENTRYPOINT(glUniformMatrix2x4fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT4x2:
                {
                    GL_ENTRYPOINT(glUniformMatrix4x2fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT3x4:
                {
                    GL_ENTRYPOINT(glUniformMatrix3x4fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }
                case GL_FLOAT_MAT4x3:
                {
                    GL_ENTRYPOINT(glUniformMatrix4x3fv)(restore_location, array_size, GL_FALSE, static_cast<const GLfloat *>(pTrace_data));
                    break;
                }

                case GL_SAMPLER_1D:
                case GL_SAMPLER_2D:
                case GL_SAMPLER_3D:
                case GL_SAMPLER_CUBE:
                case GL_SAMPLER_1D_SHADOW:
                case GL_SAMPLER_2D_SHADOW:
                case GL_SAMPLER_1D_ARRAY:
                case GL_SAMPLER_2D_ARRAY:
                case GL_SAMPLER_1D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_ARRAY_SHADOW:
                case GL_SAMPLER_2D_MULTISAMPLE:
                case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_SAMPLER_CUBE_SHADOW:
                case GL_SAMPLER_BUFFER:
                case GL_SAMPLER_2D_RECT:
                case GL_SAMPLER_2D_RECT_SHADOW:
                case GL_SAMPLER_CUBE_MAP_ARRAY:
                case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
                case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
                case GL_INT_SAMPLER_1D:
                case GL_INT_SAMPLER_2D:
                case GL_INT_SAMPLER_3D:
                case GL_INT_SAMPLER_CUBE:
                case GL_INT_SAMPLER_1D_ARRAY:
                case GL_INT_SAMPLER_2D_ARRAY:
                case GL_INT_SAMPLER_2D_MULTISAMPLE:
                case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_INT_SAMPLER_BUFFER:
                case GL_INT_SAMPLER_2D_RECT:
                case GL_UNSIGNED_INT_SAMPLER_1D:
                case GL_UNSIGNED_INT_SAMPLER_2D:
                case GL_UNSIGNED_INT_SAMPLER_3D:
                case GL_UNSIGNED_INT_SAMPLER_CUBE:
                case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
                case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                case GL_UNSIGNED_INT_SAMPLER_BUFFER:
                case GL_UNSIGNED_INT_SAMPLER_2D_RECT:

                case GL_IMAGE_1D:
                case GL_IMAGE_2D:
                case GL_IMAGE_3D:
                case GL_IMAGE_2D_RECT:
                case GL_IMAGE_CUBE:
                case GL_IMAGE_BUFFER:
                case GL_IMAGE_1D_ARRAY:
                case GL_IMAGE_2D_ARRAY:
                case GL_IMAGE_2D_MULTISAMPLE:
                case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
                case GL_INT_IMAGE_1D:
                case GL_INT_IMAGE_2D:
                case GL_INT_IMAGE_3D:
                case GL_INT_IMAGE_2D_RECT:
                case GL_INT_IMAGE_CUBE:
                case GL_INT_IMAGE_BUFFER:
                case GL_INT_IMAGE_1D_ARRAY:
                case GL_INT_IMAGE_2D_ARRAY:
                case GL_INT_IMAGE_2D_MULTISAMPLE:
                case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_1D:
                case GL_UNSIGNED_INT_IMAGE_2D:
                case GL_UNSIGNED_INT_IMAGE_3D:
                case GL_UNSIGNED_INT_IMAGE_2D_RECT:
                case GL_UNSIGNED_INT_IMAGE_CUBE:
                case GL_UNSIGNED_INT_IMAGE_BUFFER:
                case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
                case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
                case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:

                case GL_INT:
                case GL_BOOL:
                {
                    GL_ENTRYPOINT(glUniform1iv)(restore_location, array_size, static_cast<const GLint *>(pTrace_data));
                    break;
                }
                case GL_INT_VEC2:
                case GL_BOOL_VEC2:
                {
                    GL_ENTRYPOINT(glUniform2iv)(restore_location, array_size, static_cast<const GLint *>(pTrace_data));
                    break;
                }
                case GL_INT_VEC3:
                case GL_BOOL_VEC3:
                {
                    GL_ENTRYPOINT(glUniform3iv)(restore_location, array_size, static_cast<const GLint *>(pTrace_data));
                    break;
                }
                case GL_INT_VEC4:
                case GL_BOOL_VEC4:
                {
                    GL_ENTRYPOINT(glUniform4iv)(restore_location, array_size, static_cast<const GLint *>(pTrace_data));
                    break;
                }
                case GL_UNSIGNED_INT:
                case GL_UNSIGNED_INT_ATOMIC_COUNTER: // TODO: is this correct?
                {
                    GL_ENTRYPOINT(glUniform1uiv)(restore_location, array_size, static_cast<const GLuint *>(pTrace_data));
                    break;
                }
                case GL_UNSIGNED_INT_VEC2:
                {
                    GL_ENTRYPOINT(glUniform2uiv)(restore_location, array_size, static_cast<const GLuint *>(pTrace_data));
                    break;
                }
                case GL_UNSIGNED_INT_VEC3:
                {
                    GL_ENTRYPOINT(glUniform3uiv)(restore_location, array_size, static_cast<const GLuint *>(pTrace_data));
                    break;
                }
                case GL_UNSIGNED_INT_VEC4:
                {
                    GL_ENTRYPOINT(glUniform4uiv)(restore_location, array_size, static_cast<const GLuint *>(pTrace_data));
                    break;
                }
                default:
                {
                    VOGL_ASSERT_ALWAYS;
                    vogl_warning_printf("Unknown uniform type 0x%04X\n", restore_uniform.m_type);
                    any_restore_warnings = true;
                    continue;
                }
            }

            if (vogl_check_gl_error())
            {
                any_gl_errors = true;

                vogl_warning_printf("A GL error occurred while attempting to restore trace uniform \"%s\" type %s, trace program %u GL program %u.\n",
                                   trace_uniform.m_name.get_ptr(), get_gl_enums().find_gl_name(trace_uniform.m_type), m_snapshot_handle, handle32);
            }
        }
    } // uniform_iter

    return true;
}

bool vogl_program_state::restore_uniform_blocks(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);

    // Restore active uniform blocks
    if ((context_info.get_version() >= VOGL_GL_VERSION_3_1) && GL_ENTRYPOINT(glGetUniformBlockIndex) && GL_ENTRYPOINT(glUniformBlockBinding))
    {
        VOGL_CHECK_GL_ERROR;

        growable_array<char, 256> name_buf;

        for (uint32_t uniform_block_index = 0; uniform_block_index < m_num_active_uniform_blocks; uniform_block_index++)
        {
            const vogl_program_uniform_block_state &state = m_uniform_blocks[uniform_block_index];

            if (state.m_name.is_empty())
            {
                vogl_error_printf("Trace program %u GL program %u: Invalid uniform block name!\n", m_snapshot_handle, handle32);
                any_restore_warnings = true;
                continue;
            }

            GLuint restore_block_index = GL_ENTRYPOINT(glGetUniformBlockIndex)(handle32, reinterpret_cast<const GLchar *>(state.m_name.get_ptr()));
            bool gl_err = vogl_check_gl_error();
            if (gl_err)
                any_gl_errors = true;

            if ((gl_err) || (restore_block_index == GL_INVALID_INDEX))
            {
                vogl_error_printf("Trace program %u GL program %u: Failed finding uniform block \"%s\"\n", m_snapshot_handle, handle32, state.m_name.get_ptr());
                any_restore_warnings = true;
                continue;
            }

            if (restore_block_index != state.m_uniform_block_index)
            {
                vogl_error_printf("Trace program %u GL program %u: Uniform block \"%s\"'s restore block index (%u) differs from it's trace index (%u)! Uniform block indices are not currently remapped while replaying GL calls, so this trace may not be replayable!\n",
                                  (uint32_t)m_snapshot_handle, handle32, state.m_name.get_ptr(), state.m_uniform_block_index, restore_block_index);
                any_restore_warnings = true;
            }

            GL_ENTRYPOINT(glUniformBlockBinding)(handle32, restore_block_index, state.m_uniform_block_binding_point);
            if (vogl_check_gl_error())
            {
                any_gl_errors = true;

                vogl_error_printf("Trace program %u GL program %u: Failed restoring uniform block \"%s\"'s binding point %u\n", m_snapshot_handle, handle32, state.m_name.get_ptr(), state.m_uniform_block_binding_point);
                any_restore_warnings = true;
                continue;
            }

            GLint restore_block_data_size = 0;
            GL_ENTRYPOINT(glGetActiveUniformBlockiv)(handle32, restore_block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &restore_block_data_size);
            if (vogl_check_gl_error())
                any_gl_errors = true;

            if (state.m_uniform_block_data_size != restore_block_data_size)
            {
                vogl_warning_printf("Trace program %u GL program %u: Uniform block \"%s\"'s data size (%u) differs from trace's (%u)\n", m_snapshot_handle, handle32, state.m_name.get_ptr(), state.m_uniform_block_data_size, restore_block_data_size);
                any_restore_warnings = true;
            }

            GLint restore_block_active_uniforms = 0;
            GL_ENTRYPOINT(glGetActiveUniformBlockiv)(handle32, restore_block_index, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &restore_block_active_uniforms);
            if (vogl_check_gl_error())
                any_gl_errors = true;

            if (state.m_uniform_block_active_uniforms != restore_block_active_uniforms)
            {
                vogl_warning_printf("Trace program %u GL program %u: Uniform block \"%s\"'s number of active block uniforms (%u) differs from trace's (%i)\n",
                                    (uint32_t)m_snapshot_handle, handle32, state.m_name.get_ptr(), state.m_uniform_block_active_uniforms, restore_block_active_uniforms);
                any_restore_warnings = true;
            }
        }
    }
    else if (m_num_active_uniform_blocks)
    {
        vogl_error_printf("Trace program %u GL program %u has %u active uniform blocks, but the current context does not support uniform blocks!\n", m_snapshot_handle, handle32, m_num_active_uniform_blocks);
        any_restore_warnings = true;
    }

    return true;
}

bool vogl_program_state::restore_active_attribs(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);
    VOGL_NOTE_UNUSED(any_restore_warnings);

    // Restore active attribs
    for (uint32_t attrib_iter = 0; attrib_iter < m_num_active_attribs; attrib_iter++)
    {
        const vogl_program_attrib_state &attrib = m_attribs[attrib_iter];
        if ((attrib.m_name.begins_with("_gl", true)) || (attrib.m_bound_location < 0))
            continue;

        GL_ENTRYPOINT(glBindAttribLocation)(handle32, attrib.m_bound_location, reinterpret_cast<const GLchar *>(attrib.m_name.get_ptr()));

        if (vogl_check_gl_error())
        {
            any_gl_errors = true;

            vogl_warning_printf("GL error while binding attrib location %i name %s of trace program %u GL program %u\n", attrib.m_bound_location, attrib.m_name.get_ptr(), m_snapshot_handle, handle32);
        }
    }

    return true;
}

bool vogl_program_state::restore_outputs(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const
{
    VOGL_FUNC_TRACER
    VOGL_NOTE_UNUSED(remapper);

    for (uint32_t i = 0; i < m_outputs.size(); i++)
    {
        const vogl_program_output_state &output = m_outputs[i];

        if ((output.m_name.is_empty()) || (output.m_name.begins_with("gl_", true)))
            continue;

        int location = output.m_location;
        int location_index = output.m_location_index;

        if ((context_info.supports_extension("GL_ARB_blend_func_extended")) && (GL_ENTRYPOINT(glBindFragDataLocationIndexed)))
        {
            GL_ENTRYPOINT(glBindFragDataLocationIndexed)(handle32, location, location_index, reinterpret_cast<const GLchar *>(output.m_name.get_ptr()));
        }
        else
        {
            if (location_index)
            {
                // TODO: This is not a warning, but we'll try to soldier on anyway.
                any_restore_warnings = true;

                vogl_error_printf("GL_ARB_blend_func_extended is not supported, but GL program %u uses a non-zero location index\n", handle32);
            }

            GL_ENTRYPOINT(glBindFragDataLocation)(handle32, location, reinterpret_cast<const GLchar *>(output.m_name.get_ptr()));
        }

        if (vogl_check_gl_error())
        {
            any_gl_errors = true;

            vogl_error_printf("GL error while binding fragdata location %i location index %i name %s GL program %u\n", output.m_location, output.m_location_index, output.m_name.get_ptr(), handle32);
        }
    }

    return true;
}

bool vogl_program_state::restore_transform_feedback(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const
{
    VOGL_FUNC_TRACER;
    VOGL_NOTE_UNUSED(context_info);
    VOGL_NOTE_UNUSED(remapper);
    VOGL_NOTE_UNUSED(any_restore_warnings);

    if (!m_varyings.size())
        return true;

    dynamic_string_array names;

    for (uint32_t i = 0; i < m_varyings.size(); i++)
    {
        const vogl_program_transform_feedback_varying &varying = m_varyings[i];

        GLint index = varying.m_index;
        if (index < 0)
            continue;

        names.ensure_element_is_valid(index);
        names[index] = varying.m_name;
    }

    vogl::vector<GLchar *> varyings(names.size());
    for (uint32_t i = 0; i < names.size(); i++)
        varyings[i] = (GLchar *)(names[i].get_ptr());

    GL_ENTRYPOINT(glTransformFeedbackVaryings)(handle32, varyings.size(), varyings.get_ptr(), m_transform_feedback_mode);
    if (vogl_check_gl_error())
    {
        any_gl_errors = true;

        vogl_error_printf("GL error while setting transform feedback varyings, GL program %u\n", handle32);
    }

    return true;
}

bool vogl_program_state::restore_link_snapshot(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors, bool &link_succeeded) const
{
    VOGL_FUNC_TRACER

    if (vogl_check_gl_error())
        any_gl_errors = true;

    VOGL_ASSERT(m_link_snapshot);

    if (!restore_active_attribs(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
        return false;

    if (!restore_outputs(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
        return false;

    if (!restore_transform_feedback(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
        return false;

    if (m_separable)
        GL_ENTRYPOINT(glProgramParameteri)(handle32, GL_PROGRAM_SEPARABLE, GL_TRUE);

    uint_vec shader_handles;

    if (m_program_binary.size())
    {
        GL_ENTRYPOINT(glProgramBinary)(handle32, m_program_binary_format, m_program_binary.get_ptr(), m_program_binary.size());
        if (vogl_check_gl_error())
        {
            vogl_warning_printf("GL error while setting link-time snapshot program binary on trace program %u GL program %u\n", m_snapshot_handle, handle32);
            any_gl_errors = true;
        }
        else if (get_program_bool(handle32, GL_LINK_STATUS))
            link_succeeded = true;
    }

    if ((!link_succeeded) && (m_shaders.size()))
    {
        shader_handles.resize(m_shaders.size());

        for (uint32_t i = 0; i < m_shaders.size(); i++)
        {
            shader_handles[i] = GL_ENTRYPOINT(glCreateShader)(m_shaders[i].get_shader_type());
            if ((vogl_check_gl_error()) || (!shader_handles[i]))
                goto handle_error;

            GLuint64 handle = shader_handles[i];
            if (!m_shaders[i].restore(context_info, remapper, handle))
                goto handle_error;

            if (!m_shaders[i].get_restore_compile_status())
            {
                vogl_warning_printf("Failed compiling shadowed link-time shader while restoring program, trace program %u GL program %u\n", m_snapshot_handle, handle32);
                any_restore_warnings = true;
            }
        }

        for (uint32_t i = 0; i < m_shaders.size(); i++)
        {
            GL_ENTRYPOINT(glAttachShader)(handle32, shader_handles[i]);

            if (vogl_check_gl_error())
            {
                for (uint32_t j = 0; j < i; j++)
                    GL_ENTRYPOINT(glDetachShader)(handle32, shader_handles[j]);

                goto handle_error;
            }
        }

        GL_ENTRYPOINT(glLinkProgram)(handle32);

        if (vogl_check_gl_error())
        {
            vogl_warning_printf("GL error while linking link-time snapshot program on trace program %u GL program %u\n", m_snapshot_handle, handle32);
            any_gl_errors = true;
        }
        else if (get_program_bool(handle32, GL_LINK_STATUS))
            link_succeeded = true;

        for (uint32_t i = 0; i < shader_handles.size(); i++)
        {
            GL_ENTRYPOINT(glDetachShader)(handle32, shader_handles[i]);
            if (vogl_check_gl_error())
                any_gl_errors = true;

            GL_ENTRYPOINT(glDeleteShader)(shader_handles[i]);
            shader_handles[i] = 0;

            if (vogl_check_gl_error())
                any_gl_errors = true;
        }
    }

    return true;

handle_error:
    for (uint32_t i = 0; i < shader_handles.size(); i++)
        if (shader_handles[i])
            GL_ENTRYPOINT(glDeleteShader)(shader_handles[i]);

    if (vogl_check_gl_error())
        any_gl_errors = true;

    return false;
}

bool vogl_program_state::link_program(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors, bool &link_succeeded) const
{
    VOGL_FUNC_TRACER

    if (vogl_check_gl_error())
        any_gl_errors = true;

    if (m_link_snapshot)
        return restore_link_snapshot(handle32, context_info, remapper, any_restore_warnings, any_gl_errors, link_succeeded);

    // First restore the program's linked state.
    if (m_pLink_time_snapshot.get())
    {
        if (!m_pLink_time_snapshot->link_program(handle32, context_info, remapper, any_restore_warnings, any_gl_errors, link_succeeded))
            return false;
    }

    // Now restore the program's actual/current state.
    if (!restore_active_attribs(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
        return false;

    if (!restore_outputs(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
        return false;

    if (m_separable)
        GL_ENTRYPOINT(glProgramParameteri)(handle32, GL_PROGRAM_SEPARABLE, GL_TRUE);

    // TODO: Restore the CURRENT transform feedback state (not just the last linked state) - this will require deeper shadowing and considering how rarely transform feedback is used by games it's low priority.

    if (m_attached_shaders.size())
    {
        for (uint32_t i = 0; i < m_attached_shaders.size(); i++)
        {
            GLuint trace_handle = m_attached_shaders[i];
            GLuint replay_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_SHADERS, trace_handle));
            if (replay_handle)
            {
                GL_ENTRYPOINT(glAttachShader)(handle32, replay_handle);
                if (vogl_check_gl_error())
                {
                    vogl_error_printf("GL error while attaching shader %u to trace program %u GL program %u\n", replay_handle, m_snapshot_handle, handle32);
                    return false;
                }
            }
        }
    }

    return true;
}

bool vogl_program_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

    if (!m_is_valid)
        return false;

    vogl_scoped_binding_state orig_binding(GL_PROGRAM);

    bool created_handle = false;

    if (!handle)
    {
        if (m_link_entrypoint == VOGL_ENTRYPOINT_glCreateShaderProgramv)
        {
            vogl::vector<GLchar *> string_ptrs(m_create_shader_program_strings.size());

            for (uint32_t i = 0; i < m_create_shader_program_strings.size(); i++)
                string_ptrs[i] = (GLchar *)(m_create_shader_program_strings[i].get_ptr());

            handle = GL_ENTRYPOINT(glCreateShaderProgramv)(m_create_shader_program_type, m_create_shader_program_strings.size(), string_ptrs.get_ptr());
        }
        else
        {
            handle = GL_ENTRYPOINT(glCreateProgram)();
        }

        if ((vogl_check_gl_error()) || (!handle))
            return false;

        remapper.declare_handle(VOGL_NAMESPACE_PROGRAMS, m_snapshot_handle, handle, GL_NONE);
        VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_PROGRAMS, m_snapshot_handle) == handle);

        created_handle = true;
    }

    VOGL_ASSERT(handle <= cUINT32_MAX);
    GLuint handle32 = static_cast<GLuint>(handle);

    bool any_gl_errors = false;
    bool any_restore_warnings = false;

    bool link_succeeded = false;

    if (m_link_entrypoint == VOGL_ENTRYPOINT_glCreateShaderProgramv)
    {
        link_succeeded = get_program_bool(handle32, GL_LINK_STATUS);
    }
    else
    {
        if (!link_program(handle32, context_info, remapper, any_restore_warnings, any_gl_errors, link_succeeded))
            goto handle_error;
    }

    if ((m_pLink_time_snapshot.get()) && (link_succeeded != m_pLink_time_snapshot->m_link_status))
    {
        if (!link_succeeded)
        {
            if (m_pLink_time_snapshot->m_link_status)
            {
                vogl_error_printf("Failed linking trace program %u GL program %u, but this program was recorded as successfully linked in the trace\n", m_snapshot_handle, handle32);
                goto handle_error;
            }
            else
            {
                vogl_debug_printf("Failed linking trace program %u GL program %u, but this program was also recorded as not successfully linked in the trace\n", m_snapshot_handle, handle32);
            }
        }
        else if (!m_pLink_time_snapshot->m_link_status)
        {
            vogl_warning_printf("Succeeded linking trace program %u GL program %u, which is odd because this program was recorded as not successfully linked in the trace\n", m_snapshot_handle, handle32);
            any_restore_warnings = true;
        }
    }

    if (link_succeeded)
    {
        GL_ENTRYPOINT(glUseProgram)(handle32);
        if (vogl_check_gl_error())
        {
            any_gl_errors = true;
            goto handle_error;
        }

        if (!restore_uniforms(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
            goto handle_error;

        if (!restore_uniform_blocks(handle32, context_info, remapper, any_restore_warnings, any_gl_errors))
            goto handle_error;

    } // if (link_succeeded)

    if (any_gl_errors)
    {
        vogl_warning_printf("One or more GL errors occurred while attempting to restore trace program %u GL program %u. This program has been restored as much as possible, but the replay may diverge.\n", m_snapshot_handle, handle32);
    }

    if (any_restore_warnings)
    {
        vogl_warning_printf("One or more restore warnings occurred while attempting to restore trace program %u GL program %u. This program has been restored as much as possible, but the replay may diverge.\n", m_snapshot_handle, handle32);
    }

    return true;

handle_error:
    vogl_error_printf("Failed restoring trace program %u GL program %u\n", m_snapshot_handle, handle32);

    if ((handle) && (created_handle))
    {
        GL_ENTRYPOINT(glUseProgram)(0);
        VOGL_CHECK_GL_ERROR;

        remapper.delete_handle_and_object(VOGL_NAMESPACE_PROGRAMS, m_snapshot_handle, handle);

        handle = 0;
        handle32 = 0;
    }

    return false;
}

bool vogl_program_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    uint32_t replay_handle = m_snapshot_handle;

    uint32_t trace_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_PROGRAMS, m_snapshot_handle));

    m_snapshot_handle = trace_handle;

    if (!m_link_snapshot)
    {
        for (uint32_t i = 0; i < m_attached_shaders.size(); i++)
            m_attached_shaders[i] = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_SHADERS, m_attached_shaders[i]));
    }
    else
    {
        for (uint32_t i = 0; i < m_shaders.size(); i++)
        {
            // Try to remap the shader's handle, although it could be dead by now. This makes it easier to diff state captures.
            if (m_shaders[i].is_valid())
            {
                GLuint64 snapshot_handle = m_shaders[i].get_snapshot_handle();
                if (remapper.is_valid_handle(VOGL_NAMESPACE_SHADERS, snapshot_handle))
                    m_shaders[i].set_snapshot_handle(remapper.remap_handle(VOGL_NAMESPACE_SHADERS, snapshot_handle));
            }
        }
    }

    for (uint32_t i = 0; i < m_uniforms.size(); i++)
        if (m_uniforms[i].m_base_location >= 0)
            m_uniforms[i].m_base_location = remapper.remap_location(replay_handle, m_uniforms[i].m_base_location);

    if (m_pLink_time_snapshot.get())
    {
        if (!m_pLink_time_snapshot->remap_handles(remapper))
            return false;
    }

    return true;
}

void vogl_program_state::clear()
{
    VOGL_FUNC_TRACER

    m_link_entrypoint = VOGL_ENTRYPOINT_INVALID;

    m_snapshot_handle = 0;

    m_program_binary.clear();
    m_program_binary_format = GL_NONE;

    m_attached_shaders.clear();

    m_info_log.clear();

    m_num_active_attribs = 0;
    m_num_active_uniforms = 0;
    m_num_active_uniform_blocks = 0;

    m_attribs.clear();
    m_uniforms.clear();
    m_uniform_blocks.clear();

    m_shaders.clear();

    m_outputs.clear();

    m_pLink_time_snapshot.reset();

    m_transform_feedback_mode = GL_NONE;
    m_transform_feedback_num_varyings = 0;
    m_varyings.clear();

    m_create_shader_program_type = GL_NONE;
    m_create_shader_program_strings.clear();

    m_marked_for_deletion = false;
    m_link_status = false;
    m_separable = false;
    m_verify_status = false;

    m_link_snapshot = false;
    m_is_valid = false;
}

bool vogl_program_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_is_valid)
        return false;

    node.add_key_value("version", VOGL_PROGRAM_VERSION);
    node.add_key_value("link_entrypoint", m_link_entrypoint);
    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("link_snapshot", m_link_snapshot);
    node.add_key_value("link_status", m_link_status);
    node.add_key_value("separable", m_separable);
    node.add_key_value("verify_status", m_verify_status);
    node.add_key_value("marked_for_deletion", m_marked_for_deletion);
    node.add_key_value("num_active_attribs", m_num_active_attribs);
    node.add_key_value("num_active_uniforms", m_num_active_uniforms);
    node.add_key_value("num_active_uniform_blocks", m_num_active_uniform_blocks);
    node.add_key_value("info_log", m_info_log);

    node.add_key_value("create_shader_program_type", m_create_shader_program_type);
    node.add_vector("create_shader_program_strings", m_create_shader_program_strings);

    node.add_key_value("program_binary_format", m_program_binary_format);
    if (m_program_binary.size())
    {
        dynamic_string id(blob_manager.add_buf_compute_unique_id(m_program_binary.get_ptr(), m_program_binary.size(), "program_binary", "bin"));
        node.add_key_value("program_binary", id);
    }

    if (m_attached_shaders.size())
    {
        json_node &attached_shaders_array = node.add_array("attached_shaders");
        for (uint32_t i = 0; i < m_attached_shaders.size(); i++)
            attached_shaders_array.add_value(m_attached_shaders[i]);
    }

    if (m_shaders.size())
    {
        json_node &shader_objects_array = node.add_array("shader_objects");
        for (uint32_t i = 0; i < m_shaders.size(); i++)
            if (!m_shaders[i].serialize(shader_objects_array.add_object(), blob_manager))
                return false;
    }

    if (m_attribs.size())
    {
        json_node &active_attribs_array = node.add_array("active_attribs");
        for (uint32_t i = 0; i < m_attribs.size(); i++)
        {
            const vogl_program_attrib_state &attrib = m_attribs[i];

            json_node &attrib_node = active_attribs_array.add_object();
            attrib_node.add_key_value("name", attrib.m_name);
            attrib_node.add_key_value("type", get_gl_enums().find_gl_name(attrib.m_type));
            attrib_node.add_key_value("size", attrib.m_size);
            attrib_node.add_key_value("location", attrib.m_bound_location);
        }
    }

    if (m_uniforms.size())
    {
        json_node &active_uniforms_array = node.add_array("active_uniforms");

        vogl::vector<char> temp_buf;
        temp_buf.reserve(64);

        for (uint32_t i = 0; i < m_uniforms.size(); i++)
        {
            const vogl_program_uniform_state &uniform = m_uniforms[i];

            json_node &uniform_node = active_uniforms_array.add_object();
            uniform_node.add_key_value("name", uniform.m_name);
            uniform_node.add_key_value("type", get_gl_enums().find_gl_name(uniform.m_type));
            uniform_node.add_key_value("size", uniform.m_size);
            uniform_node.add_key_value("base_location", uniform.m_base_location);

            json_node &uniform_data = uniform_node.add_array("uniform_data");

            if (uniform.m_data.size())
            {
                const uint32_t type_size_in_GLints = vogl_gl_get_uniform_size_in_GLints(uniform.m_type);
                const uint32_t type_size = vogl_gl_get_uniform_size_in_bytes(uniform.m_type);
                const GLenum base_type = vogl_gl_get_uniform_base_type(uniform.m_type);

                for (int element = 0; element < uniform.m_size; element++)
                {
                    for (uint32_t element_ofs = 0; element_ofs < type_size_in_GLints; element_ofs++)
                    {
                        const uint32_t *pData = reinterpret_cast<const uint32_t *>(uniform.m_data.get_ptr() + type_size * element + sizeof(uint32_t) * element_ofs);

                        if (base_type == GL_FLOAT)
                        {
                            float f = *reinterpret_cast<const float *>(pData);

                            json_value v;
                            v.set_value(f);

                            temp_buf.resize(0);
                            v.serialize(temp_buf, false);

                            json_value dv;
                            bool failed = true;
                            if (dv.deserialize(temp_buf.get_ptr(), temp_buf.size()))
                            {
                                if (static_cast<float>(dv.as_double()) == f)
                                    failed = false;
                            }

                            if (failed)
                            {
                                dynamic_string str(cVarArg, "0x%08X", *pData);
                                uniform_data.add_value(str);
                            }
                            else
                            {
                                uniform_data.add_value(f);
                            }
                        }
                        else if (base_type == GL_DOUBLE)
                        {
                            double f = *reinterpret_cast<const double *>(pData);

                            json_value v;
                            v.set_value(f);

                            temp_buf.resize(0);
                            v.serialize(temp_buf, false);

                            json_value dv;

                            bool failed = true;
                            if (dv.deserialize(temp_buf.get_ptr(), temp_buf.size()))
                            {
                                if (dv.as_double() == f)
                                    failed = false;
                            }

                            if (failed)
                            {
                                uniform_data.add_value(*reinterpret_cast<const uint64_t *>(pData));
                            }
                            else
                            {
                                uniform_data.add_value(f);
                            }
                        }
                        else if (base_type == GL_BOOL)
                        {
                            uniform_data.add_value(*pData != 0);
                        }
                        else if (base_type == GL_UNSIGNED_INT)
                        {
                            uniform_data.add_value(*reinterpret_cast<const GLuint *>(pData));
                        }
                        else
                        {
                            uniform_data.add_value(*reinterpret_cast<const GLint *>(pData));
                        }
                    }
                }
            }
        }
    }

    if (m_uniform_blocks.size())
    {
        json_node &active_uniforms_blocks_array = node.add_array("active_uniform_blocks");
        for (uint32_t i = 0; i < m_uniform_blocks.size(); i++)
        {
            const vogl_program_uniform_block_state &state = m_uniform_blocks[i];

            json_node &uniform_block_node = active_uniforms_blocks_array.add_object();
            uniform_block_node.add_key_value("block_index", state.m_uniform_block_index);
            uniform_block_node.add_key_value("name", state.m_name);
            uniform_block_node.add_key_value("binding_point", state.m_uniform_block_binding_point);
            uniform_block_node.add_key_value("data_size", state.m_uniform_block_data_size);
            uniform_block_node.add_key_value("active_uniforms", state.m_uniform_block_active_uniforms);
        }
    }

    if (m_outputs.size())
    {
        json_node &outputs_array = node.add_array("outputs");
        for (uint32_t i = 0; i < m_outputs.size(); i++)
        {
            const vogl_program_output_state &output = m_outputs[i];

            json_node &outputs_node = outputs_array.add_object();
            outputs_node.add_key_value("index", i);
            outputs_node.add_key_value("name", output.m_name);
            outputs_node.add_key_value("location", output.m_location);
            outputs_node.add_key_value("location_index", output.m_location_index);
            outputs_node.add_key_value("type", get_gl_enums().find_gl_name(output.m_type));
            outputs_node.add_key_value("array_size", output.m_array_size);
            outputs_node.add_key_value("is_per_patch", output.m_is_per_patch);
        }
    }

    node.add_key_value("transform_feedback_mode", get_gl_enums().find_gl_name(m_transform_feedback_mode));
    node.add_key_value("transform_feedback_num_varyings", m_transform_feedback_num_varyings);

    if (m_transform_feedback_num_varyings)
    {
        json_node &varyings_array = node.add_array("transform_feedback_varyings");
        for (uint32_t i = 0; i < m_varyings.size(); i++)
        {
            const vogl_program_transform_feedback_varying &varying = m_varyings[i];

            json_node &node2 = varyings_array.add_object();
            node2.add_key_value("index", varying.m_index);
            node2.add_key_value("name", varying.m_name);
            node2.add_key_value("size", varying.m_size);
            node2.add_key_value("type", get_gl_enums().find_gl_name(varying.m_type));
        }
    }

    if (m_pLink_time_snapshot.get())
    {
        if (!m_pLink_time_snapshot->serialize(node.add_object("link_time_snapshot"), blob_manager))
            return false;
    }

    return true;
}

bool vogl_program_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    if (!node.is_object())
        return false;

    m_snapshot_handle = node.value_as_uint32("handle");
    m_link_entrypoint = static_cast<gl_entrypoint_id_t>(node.value_as_uint32("link_entrypoint"));
    m_link_snapshot = node.value_as_bool("link_snapshot");
    m_link_status = node.value_as_bool("link_status");
    m_separable = node.value_as_bool("separable");
    m_verify_status = node.value_as_bool("verify_status");
    m_marked_for_deletion = node.value_as_bool("marked_for_deletion");
    m_num_active_attribs = node.value_as_int("num_active_attribs");
    m_num_active_uniforms = node.value_as_int("num_active_uniforms");
    m_num_active_uniform_blocks = node.value_as_int("num_active_uniform_blocks");
    m_info_log = node.value_as_string("info_log");

    m_create_shader_program_type = node.value_as_uint32("create_shader_program_type");
    node.get_vector("create_shader_program_strings", m_create_shader_program_strings);

    m_program_binary_format = node.value_as_uint32("program_binary_format");
    if (node.has_key("program_binary"))
    {
        if (!blob_manager.get(node.value_as_string("program_binary"), m_program_binary))
        {
            vogl_warning_printf("Failed retreiving program_binary data blob from trace program %u\n", m_snapshot_handle);
        }
    }

    const json_node *pAttached_shaders_array = node.find_child_array("attached_shaders");
    if (pAttached_shaders_array)
    {
        m_attached_shaders.resize(pAttached_shaders_array->size());
        for (uint32_t i = 0; i < m_attached_shaders.size(); i++)
            m_attached_shaders[i] = pAttached_shaders_array->value_as_uint32(i);
    }

    const json_node *pShader_objects_array = node.find_child_array("shader_objects");
    if (pShader_objects_array)
    {
        if (!pShader_objects_array->are_all_children_objects())
            vogl_warning_printf("shader_objects node must contain all objects, trace program %u\n", m_snapshot_handle);
        else
        {
            m_shaders.resize(pShader_objects_array->size());
            for (uint32_t i = 0; i < m_shaders.size(); i++)
                if (!m_shaders[i].deserialize(*pShader_objects_array->get_value_as_object(i), blob_manager))
                    return false;
        }
    }

    const json_node *pActive_attribs_array = node.find_child_array("active_attribs");
    if (pActive_attribs_array)
    {
        m_attribs.resize(pActive_attribs_array->size());

        for (uint32_t i = 0; i < m_attribs.size(); i++)
        {
            const json_node *pAttrib_node = pActive_attribs_array->get_value_as_object(i);
            if (!pAttrib_node)
            {
                vogl_warning_printf("Missing attrib object in active_attribs array, trace program %u\n", m_snapshot_handle);
            }
            else
            {
                vogl_program_attrib_state &attrib = m_attribs[i];

                attrib.m_name = pAttrib_node->value_as_string("name");
                attrib.m_type = vogl_get_json_value_as_enum(*pAttrib_node, "type");
                attrib.m_size = pAttrib_node->value_as_int32("size");
                attrib.m_bound_location = pAttrib_node->value_as_int32("location");
            }
        }
    }

    const json_node *pActive_uniforms_array = node.find_child_array("active_uniforms");
    if (pActive_uniforms_array)
    {
        m_uniforms.resize(pActive_uniforms_array->size());

        for (uint32_t i = 0; i < m_uniforms.size(); i++)
        {
            const json_node *pUniform_node = pActive_uniforms_array->get_value_as_object(i);
            if (!pUniform_node)
            {
                vogl_warning_printf("Missing uniform object in active_uniforms array, trace program %u\n", m_snapshot_handle);
                continue;
            }

            vogl_program_uniform_state &uniform = m_uniforms[i];
            uniform.m_name = pUniform_node->value_as_string("name");
            uniform.m_type = vogl_get_json_value_as_enum(*pUniform_node, "type");
            uniform.m_size = pUniform_node->value_as_int32("size");
            uniform.m_base_location = pUniform_node->value_as_int32("base_location");

            const json_node *pUniform_data = pUniform_node->find_child_array("uniform_data");
            if (!pUniform_data)
            {
                vogl_warning_printf("Missing uniform_data child array in active_uniforms array, trace program %u\n", m_snapshot_handle);
                continue;
            }

            const uint32_t type_size_in_GLints = vogl_gl_get_uniform_size_in_GLints(uniform.m_type);
            const uint32_t type_size = vogl_gl_get_uniform_size_in_bytes(uniform.m_type);
            const GLenum base_type = vogl_gl_get_uniform_base_type(uniform.m_type);

            uint32_t uniform_data_size_in_bytes = type_size * uniform.m_size;
            if (uniform_data_size_in_bytes > 0)
            {
                uniform.m_data.resize(uniform_data_size_in_bytes);
            }

            if ((!type_size_in_GLints) || (!type_size))
            {
                vogl_warning_printf("Uniform type error while processing active_uniforms array, trace program %u\n", m_snapshot_handle);
                continue;
            }

            if (pUniform_data->size() != uniform.m_size * type_size_in_GLints)
            {
                vogl_warning_printf("uniform_data array's size is invalid, trace program %u\n", m_snapshot_handle);
                continue;
            }

            for (int element = 0; element < uniform.m_size; element++)
            {
                for (uint32_t element_ofs = 0; element_ofs < type_size_in_GLints; element_ofs++)
                {
                    const uint32_t array_index = element * type_size_in_GLints + element_ofs;

                    uint32_t *pData = reinterpret_cast<uint32_t *>(uniform.m_data.get_ptr() + type_size * element + sizeof(uint32_t) * element_ofs);
                    float *pFloat_data = reinterpret_cast<float *>(pData);
                    double *pDouble_data = reinterpret_cast<double *>(pData);

                    const json_value &json_val = pUniform_data->get_value(array_index);

                    if (json_val.is_string())
                    {
                        dynamic_string str(json_val.as_string());

                        str.trim();

                        bool is_hex = str.begins_with("0x", true);
                        bool is_negative = str.begins_with("-", true);

                        const char *pBuf = str.get_ptr();

                        uint64_t val64 = 0;
                        bool success = is_negative ? string_ptr_to_int64(pBuf, reinterpret_cast<int64_t &>(val64)) : string_ptr_to_uint64(pBuf, val64);

                        if (!success)
                            vogl_warning_printf("Failed converting uniform array element \"%s\" to integer, trace program %u\n", str.get_ptr(), m_snapshot_handle);
                        else if (base_type == GL_FLOAT)
                        {
                            if (is_hex)
                                *pFloat_data = *reinterpret_cast<const float *>(&val64);
                            else
                                *pFloat_data = static_cast<float>(val64);
                        }
                        else if (base_type == GL_DOUBLE)
                        {
                            if (is_hex)
                                *pDouble_data = *reinterpret_cast<const double *>(&val64);
                            else
                                *pDouble_data = static_cast<double>(val64);
                        }
                        else
                            *pData = static_cast<uint32_t>(val64);
                    }
                    else
                    {
                        if (base_type == GL_FLOAT)
                            *pFloat_data = json_val.as_float();
                        else if (base_type == GL_DOUBLE)
                            *pDouble_data = json_val.as_double();
                        else if (base_type == GL_UNSIGNED_INT)
                            *pData = json_val.as_uint32();
                        else
                            *pData = json_val.as_int32();
                    }
                }
            }
        }
    }

    const json_node *pActive_uniforms_blocks_array = node.find_child_array("active_uniform_blocks");
    if (pActive_uniforms_blocks_array)
    {
        if (!pActive_uniforms_blocks_array->are_all_children_objects())
        {
            vogl_warning_printf("active_uniform_blocks child array does not contain all child objects in trace program %u\n", m_snapshot_handle);
        }
        else
        {
            m_uniform_blocks.resize(pActive_uniforms_blocks_array->size());

            for (uint32_t i = 0; i < pActive_uniforms_blocks_array->size(); i++)
            {
                vogl_program_uniform_block_state &state = m_uniform_blocks[i];

                const json_node &uniform_block_node = *pActive_uniforms_blocks_array->get_value_as_object(i);

                state.m_uniform_block_index = uniform_block_node.value_as_uint32("block_index");
                state.m_name = uniform_block_node.value_as_string("name");
                state.m_uniform_block_binding_point = uniform_block_node.value_as_int("binding_point");
                state.m_uniform_block_data_size = uniform_block_node.value_as_int("data_size");
                state.m_uniform_block_active_uniforms = uniform_block_node.value_as_int("active_uniforms");
            }
        }
    }

    const json_node *pOutputs_array = node.find_child_array("outputs");
    if (pOutputs_array)
    {
        if (!pOutputs_array->are_all_children_objects())
        {
            vogl_warning_printf("outputs child array does not contain all child objects in trace program %u\n", m_snapshot_handle);
        }
        else
        {
            m_outputs.resize(pOutputs_array->size());
            for (uint32_t i = 0; i < pOutputs_array->size(); i++)
            {
                const json_node &output_node = *pOutputs_array->get_value_as_object(i);

                vogl_program_output_state &output = m_outputs[i];

                output.clear();

                output.m_name = output_node.value_as_string("name");
                output.m_location = output_node.value_as_int32("location");
                output.m_location_index = output_node.value_as_int32("location_index");
                output.m_type = vogl_get_json_value_as_enum(output_node, "type", GL_NONE);
                output.m_array_size = output_node.value_as_int32("array_size");
                output.m_is_per_patch = output_node.value_as_bool("is_per_patch");
            }
        }
    }

    const json_node *pLink_time_snapshot_node = node.find_child_object("link_time_snapshot");
    if (pLink_time_snapshot_node)
    {
        m_pLink_time_snapshot.reset(vogl_new(vogl_program_state));
        if (!m_pLink_time_snapshot->deserialize(*pLink_time_snapshot_node, blob_manager))
            return false;

        if (!m_pLink_time_snapshot->is_valid())
        {
            vogl_warning_printf("Deserialized link-time snapshot, but it wasn't valid! Trace program %u\n", m_snapshot_handle);

            m_pLink_time_snapshot.reset();
        }
    }

    m_transform_feedback_mode = vogl_get_json_value_as_enum(node, "transform_feedback_mode", GL_NONE);
    m_transform_feedback_num_varyings = node.value_as_uint32("transform_feedback_num_varyings");

    if (m_transform_feedback_num_varyings)
    {
        const json_node *pVaryings_array = node.find_child_array("transform_feedback_varyings");
        if (pVaryings_array)
        {
            m_varyings.resize(m_transform_feedback_num_varyings);

            for (uint32_t i = 0; i < m_transform_feedback_num_varyings; i++)
            {
                const json_node &varying_node = *pVaryings_array->get_value_as_object(i);

                vogl_program_transform_feedback_varying &varying = m_varyings[i];

                varying.clear();

                m_varyings[i].m_index = varying_node.value_as_uint32("index");
                m_varyings[i].m_name = varying_node.value_as_string("name");
                m_varyings[i].m_size = varying_node.value_as_int("size");
                m_varyings[i].m_type = vogl_get_json_value_as_enum(varying_node, "type");
            }
        }
        else
        {
            vogl_warning_printf("transform_feedback_num_varyings is %u, but the transform_feedback_varyings array wasn't found! Trace program %u\n", m_transform_feedback_num_varyings, m_snapshot_handle);

            m_varyings.clear();

            m_transform_feedback_mode = GL_NONE;
            m_transform_feedback_num_varyings = 0;
        }
    }

    m_is_valid = true;

    return true;
}

// Unused
bool vogl_program_state::compare_full_state(const vogl_program_state &rhs) const
{
    VOGL_ASSERT_ALWAYS;
    return false;
}

// Unused
bool vogl_program_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_ASSERT_ALWAYS;
    return false;
}

vogl_linked_program_state::vogl_linked_program_state()
{
    VOGL_FUNC_TRACER
}

vogl_linked_program_state::vogl_linked_program_state(const vogl_linked_program_state &other)
    : m_linked_programs(other.m_linked_programs)
{
    VOGL_FUNC_TRACER
}

vogl_linked_program_state::~vogl_linked_program_state()
{
    VOGL_FUNC_TRACER
}

vogl_linked_program_state &vogl_linked_program_state::operator=(const vogl_linked_program_state &rhs)
{
    VOGL_FUNC_TRACER

    if (this == &rhs)
        return *this;

    m_linked_programs = rhs.m_linked_programs;

    return *this;
}

void vogl_linked_program_state::clear()
{
    VOGL_FUNC_TRACER

    m_linked_programs.clear();
}

bool vogl_linked_program_state::add_snapshot(GLuint handle, const vogl_program_state &prog)
{
    VOGL_FUNC_TRACER

    m_linked_programs[handle] = prog;
    return true;
}

bool vogl_linked_program_state::add_snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, gl_entrypoint_id_t link_entrypoint, GLuint handle, GLenum binary_format, const void *pBinary, uint32_t binary_size)
{
    VOGL_FUNC_TRACER

    vogl_program_state_map::insert_result res(m_linked_programs.insert(handle));

    vogl_program_state &prog = (res.first)->second;

    if (!prog.link_snapshot(context_info, remapper, link_entrypoint, handle, pBinary, binary_size, binary_format, GL_NONE, 0, NULL))
    {
        m_linked_programs.erase(handle);
        return false;
    }

    return true;
}

bool vogl_linked_program_state::add_snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, gl_entrypoint_id_t link_entrypoint, GLuint handle, GLenum type, GLsizei count, GLchar *const *strings)
{
    VOGL_FUNC_TRACER

    vogl_program_state_map::insert_result res(m_linked_programs.insert(handle));

    vogl_program_state &prog = (res.first)->second;

    if (!prog.link_snapshot(context_info, remapper, link_entrypoint, handle, NULL, 0, GL_NONE, type, count, strings))
    {
        m_linked_programs.erase(handle);
        return false;
    }

    return true;
}

bool vogl_linked_program_state::remove_snapshot(GLuint handle)
{
    VOGL_FUNC_TRACER

    return m_linked_programs.erase(handle);
}

const vogl_program_state *vogl_linked_program_state::find_snapshot(GLuint handle) const
{
    VOGL_FUNC_TRACER

    return m_linked_programs.find_value(handle);
}

vogl_program_state *vogl_linked_program_state::find_snapshot(GLuint handle)
{
    VOGL_FUNC_TRACER

    return m_linked_programs.find_value(handle);
}

bool vogl_linked_program_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    node.init_array();

    for (vogl_program_state_map::const_iterator it = m_linked_programs.begin(); it != m_linked_programs.end(); ++it)
    {
        VOGL_ASSERT(it->first == it->second.get_snapshot_handle());

        if (!it->second.serialize(node.add_object(), blob_manager))
            return false;
    }

    return true;
}

bool vogl_linked_program_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    if (!node.is_array() || !node.are_all_children_objects())
    {
        VOGL_ASSERT_ALWAYS;

        return false;
    }

    vogl_program_state prog;

    for (uint32_t i = 0; i < node.size(); i++)
    {
        if (!prog.deserialize(*node.get_value_as_object(i), blob_manager))
        {
            VOGL_ASSERT_ALWAYS;

            clear();
            return false;
        }

        if (static_cast<uint32_t>(prog.get_snapshot_handle()) != prog.get_snapshot_handle())
        {
            VOGL_ASSERT_ALWAYS;

            clear();
            return false;
        }

        if (!m_linked_programs.insert(static_cast<uint32_t>(prog.get_snapshot_handle()), prog).second)
        {
            VOGL_ASSERT_ALWAYS;

            clear();
            return false;
        }
    }

    return true;
}

bool vogl_linked_program_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    vogl_program_state_map new_linked_programs;

    for (vogl_program_state_map::iterator it = m_linked_programs.begin(); it != m_linked_programs.end(); ++it)
    {
        VOGL_ASSERT(it->first == it->second.get_snapshot_handle());

        GLuint new_handle = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_PROGRAMS, it->first));

        vogl_program_state new_prog_state(it->second);
        if (!new_prog_state.remap_handles(remapper))
            return false;

        VOGL_ASSERT(new_handle == new_prog_state.get_snapshot_handle());

        if (!new_linked_programs.insert(new_handle, new_prog_state).second)
            return false;
    }

    m_linked_programs.swap(new_linked_programs);

    return true;
}
