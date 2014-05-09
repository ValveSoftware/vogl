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

// File: vogl_arb_program_state.h
#ifndef VOGL_ARB_PROGRAM_STATE_H
#define VOGL_ARB_PROGRAM_STATE_H

#include "vogl_common.h"
#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"

#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"

typedef vogl::vector<vec4F> vec4F_vec;

class vogl_arb_program_state : public vogl_gl_object_state
{
public:
    vogl_arb_program_state();
    virtual ~vogl_arb_program_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTARBProgram;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_PROGRAM_ARB;
    }

    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target);

    virtual bool restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const;

    virtual bool remap_handles(vogl_handle_remapper &remapper);

    virtual void clear();

    virtual bool is_valid() const
    {
        return m_is_valid;
    }

    virtual bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    virtual bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    virtual GLuint64 get_snapshot_handle() const
    {
        return m_snapshot_handle;
    }

    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs) const;

    GLenum get_target() const
    {
        return m_target;
    }

    int get_error_position() const
    {
        return m_error_position;
    }
    const dynamic_string &get_error_string() const
    {
        return m_error_string;
    }
    bool is_native() const
    {
        return m_is_native;
    }

    GLenum get_program_format() const
    {
        return m_program_format;
    }

    const uint8_vec &get_program_string() const
    {
        return m_program_string;
    }
    bool set_program_string(const char* program_string, int program_length)
    {
        if ((program_length < 0) || (!m_program_string.try_resize(program_length)))
        {
            return false;
        }

        memcpy(m_program_string.get_ptr(), program_string, program_length);

        return true;
    }

    uint32_t get_program_string_size() const
    {
        return m_program_string.size();
    }

    const vec4F_vec &get_program_local_params() const
    {
        return m_local_params;
    }
    GLint get_num_instructions() const
    {
        return m_num_instructions;
    }

private:
    GLuint m_snapshot_handle;
    GLenum m_target;

    // only valid on restore, we can't query this on snapshotting unless we're willing to resubmit the string (which could have other consequences)
    mutable GLint m_error_position; // -1 if program compiled successfuly
    mutable dynamic_string m_error_string;
    mutable bool m_is_native;

    GLint m_num_instructions;

    GLenum m_program_format;
    uint8_vec m_program_string;
    vec4F_vec m_local_params;

    bool m_is_valid;

    GLint get_program_int(GLenum pname) const;
};

class vogl_arb_program_environment_state
{
public:
    vogl_arb_program_environment_state();
    ~vogl_arb_program_environment_state();

    bool snapshot(const vogl_context_info &context_info);

    bool restore(const vogl_context_info &context_info, vogl_handle_remapper &trace_to_replay_remapper) const;

    bool remap_handles(vogl_handle_remapper &remapper);

    void clear();

    bool is_valid() const
    {
        return m_is_valid;
    }

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    enum
    {
        cVertexTarget,
        cFragmentTarget,
        cNumTargets
    };

    GLuint get_cur_program(uint32_t index) const
    {
        return m_cur_programs[index];
    }

    const vec4F_vec &get_env_params(uint32_t index) const
    {
        return m_env_params[index];
    }

    bool compare_restorable_state(const vogl_arb_program_environment_state &rhs) const;

    static GLenum get_target_enum(uint32_t index)
    {
        VOGL_ASSERT(index < cNumTargets);
        return (index == cVertexTarget) ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB;
    }

private:
    GLuint m_cur_programs[cNumTargets];

    vec4F_vec m_env_params[cNumTargets];
    bool m_is_valid;

    static const char *get_target_index_name(uint32_t index)
    {
        VOGL_ASSERT(index < cNumTargets);
        return (index == cVertexTarget) ? "vertex" : "fragment";
    }
};

#endif // VOGL_ARB_PROGRAM_STATE_H
