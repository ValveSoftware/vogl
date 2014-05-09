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

// File: vogl_vao_state.h
#ifndef vogl_vao_state_H
#define vogl_vao_state_H

#include "vogl_common.h"
#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"

#include "vogl_common.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"

struct vogl_vertex_attrib_desc
{
    vogl_vertex_attrib_desc()
    {
        utils::zero_object(*this);
    }

    vogl_trace_ptr_value m_pointer;
    GLuint m_array_binding;
    GLint m_size;
    GLenum m_type;
    GLsizei m_stride;
    GLint m_integer;
    GLuint m_divisor;
    bool m_enabled;
    bool m_normalized;

    bool operator==(const vogl_vertex_attrib_desc &rhs) const;
};

class vogl_vao_state : public vogl_gl_object_state
{
public:
    vogl_vao_state();
    virtual ~vogl_vao_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTVertexArray;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_VERTEX_ARRAYS;
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

    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;

    bool compare_all_state(const vogl_vao_state &rhs) const;

    unsigned int get_vertex_attrib_count() const
    {
        return m_vertex_attribs.size();
    }

    const vogl_vertex_attrib_desc &get_vertex_attrib_desc(uint32_t index) const
    {
        return m_vertex_attribs[index];
    }

    uint32_t get_element_array_binding() const
    {
        return m_element_array_binding;
    }

private:
    GLuint m_snapshot_handle;
    GLuint m_element_array_binding;

    vogl::vector<vogl_vertex_attrib_desc> m_vertex_attribs;

    bool m_is_valid;
    bool m_has_been_bound;
};

#endif // VOGL_VAO_STATE_H
