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

// File: vogl_renderbuffer.h
#ifndef VOGL_RENDERBUFFER_H
#define VOGL_RENDERBUFFER_H

#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_texture_state.h"

class vogl_renderbuffer_desc
{
    friend class vogl_renderbuffer_state;

public:
    vogl_renderbuffer_desc();
    ~vogl_renderbuffer_desc();

    void clear();

    // Snapshots the currently bound renderbuffer.
    bool snapshot(const vogl_context_info &context_info);

    // Restores the snapshot to the currently bound renderbuffer.
    bool restore(const vogl_context_info &context_info) const;

    bool serialize(json_node &node) const;

    bool deserialize(const json_node &node);

    bool operator==(const vogl_renderbuffer_desc &rhs) const;

    bool get_int(GLenum enum_val, int *pVals, uint32_t n = 1) const;

    int get_int_or_default(GLenum enum_val, int def = 0) const
    {
        int result;
        if (!get_int(enum_val, &result, 1))
            return def;
        return result;
    }

private:
    GLsizei m_width;
    GLsizei m_height;
    GLsizei m_samples;
    GLenum m_internal_format;
    GLint m_red_size;
    GLint m_green_size;
    GLint m_blue_size;
    GLint m_alpha_size;
    GLint m_depth_size;
    GLint m_stencil_size;
};

class vogl_renderbuffer_state : public vogl_gl_object_state
{
public:
    vogl_renderbuffer_state();
    virtual ~vogl_renderbuffer_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTRenderbuffer;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_RENDER_BUFFERS;
    }

    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target);

    virtual bool restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const;

    virtual bool remap_handles(vogl_handle_remapper &remapper);

    virtual void clear();

    virtual bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    virtual bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    virtual GLuint64 get_snapshot_handle() const
    {
        return m_snapshot_handle;
    }

    const vogl_renderbuffer_desc &get_desc() const
    {
        return m_desc;
    }

    virtual bool is_valid() const
    {
        return m_is_valid;
    }

    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;

    const vogl_texture_state &get_texture() const { return m_texture; }
          vogl_texture_state &get_texture()       { return m_texture; }

private:
    GLuint m_snapshot_handle;
    vogl_renderbuffer_desc m_desc;
    vogl_texture_state m_texture;

    bool m_is_valid;
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_renderbuffer_desc);
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_renderbuffer_state);
}

#endif // VOGL_RENDERBUFFER_H
