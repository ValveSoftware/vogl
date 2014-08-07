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

// File: vogl_shader_state.h
#ifndef VOGL_SHADER_STATE_H
#define VOGL_SHADER_STATE_H

#include "vogl_common.h"
#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"

#include "vogl_common.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"
#include "vogl_fs_preprocessor.h"

class vogl_shader_state : public vogl_gl_object_state
{
public:
    vogl_shader_state();
    vogl_shader_state(const vogl_shader_state &other);
    virtual ~vogl_shader_state();

    vogl_shader_state &operator=(const vogl_shader_state &rhs);

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTShader;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_SHADERS;
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

    virtual bool get_marked_for_deletion() const
    {
        return m_marked_for_deletion;
    }

    GLenum get_shader_type() const
    {
        return m_shader_type;
    }

    const dynamic_string &get_info_log() const
    {
        return m_info_log;
    }
    const dynamic_string &get_source() const
    {
        return m_source;
    }

    void set_source(const char* source)
    {
       m_source.set(source);
    }

    bool get_compile_status() const
    {
        return m_compile_status;
    }

    bool get_restore_compile_status() const
    {
        return m_restore_compile_status;
    }

    bool compare_full_state(const vogl_shader_state &rhs) const;

    void set_snapshot_handle(GLuint64 handle)
    {
        m_snapshot_handle = static_cast<uint32_t>(handle);
        VOGL_ASSERT(handle == m_snapshot_handle);
    }

    // Content comparison, ignores handle or anything else that can't be saved/restored to GL.
    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;

private:
    GLuint m_snapshot_handle;
    GLenum m_shader_type;

    dynamic_string m_info_log;
    dynamic_string m_source;
    mutable dynamic_string m_source_blob_id; // only use for informational purposes
    mutable bool m_restore_compile_status;
    vogl_fs_preprocessor* m_fs_pp;

    bool m_marked_for_deletion;
    bool m_compile_status;

    bool m_is_valid;
};

typedef vogl::vector<vogl_shader_state> vogl_shader_state_vec;

#endif // VOGL_SHADER_STATE_H
