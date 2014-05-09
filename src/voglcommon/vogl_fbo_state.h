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

// File: vogl_fbo_state.h
#ifndef VOGL_FBO_STATE_H
#define VOGL_FBO_STATE_H

#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"

#include "vogl_common.h"
#include "vogl_general_context_state.h"

class vogl_framebuffer_attachment
{
public:
    vogl_framebuffer_attachment();
    ~vogl_framebuffer_attachment();

    bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLenum attachment, GLenum type);

    bool remap_handles(vogl_handle_remapper &remapper);

    void clear();

    bool serialize(json_node &node) const;
    bool deserialize(const json_node &node);

    inline GLenum get_attachment() const
    {
        return m_attachment;
    }

    inline GLenum get_type() const
    {
        return m_type;
    }

    inline const GLenum_to_int_map &get_params() const
    {
        return m_params;
    }

    inline int get_param(GLenum pname, GLenum def = GL_NONE) const
    {
        return m_params.value(pname, def);
    }

    inline GLuint get_handle() const
    {
        return get_param(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, 0);
    }

    bool operator==(const vogl_framebuffer_attachment &rhs) const;

private:
    // For default framebuffers, m_attachment is GL_FRONT_LEFT, etc.
    // Otherwise, it's GL_COLOR_ATTACHMENTi, GL_DEPTH_ATTACHMENT, etc.
    GLenum m_attachment;

    // GL_NONE, GL_FRAMEBUFFER_DEFAULT, GL_RENDERBUFFER, or GL_TEXTURE
    GLenum m_type;

    // For all types: GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME
    // For default: GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, etc.
    // For textures: GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, etc.
    GLenum_to_int_map m_params;
};

class vogl_framebuffer_state : public vogl_gl_object_state
{
public:
    vogl_framebuffer_state();
    virtual ~vogl_framebuffer_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTFramebuffer;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_FRAMEBUFFERS;
    }

    // Creates snapshot of FBO handle, or 0 for the default framebuffer.
    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target);

    // Creates and restores an FBO, assumes textures/RBO's have been already restored.
    virtual bool restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const;

    virtual bool remap_handles(vogl_handle_remapper &remapper);

    virtual void clear();

    virtual bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    virtual bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    virtual GLuint64 get_snapshot_handle() const
    {
        return m_snapshot_handle;
    }

    virtual bool is_valid() const
    {
        return m_is_valid;
    }

    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;

    typedef vogl::map<GLenum, vogl_framebuffer_attachment> GLenum_to_attachment_map;
    const GLenum_to_attachment_map &get_attachments() const
    {
        return m_attachments;
    }

    // GL_FRAMEBUFFER_COMPLETE, etc.
    GLenum get_status() const
    {
        return m_status;
    }

    uint32_t get_read_buffer() const
    {
        return m_read_buffer;
    }

    const uint_vec &get_draw_buffers() const
    {
        return m_draw_buffers;
    }

private:
    GLuint m_snapshot_handle;
    bool m_has_been_bound;

    GLenum_to_attachment_map m_attachments;

    uint_vec m_draw_buffers;
    uint32_t m_read_buffer;

    GLenum m_status;

    bool m_is_valid;
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_framebuffer_attachment);
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_framebuffer_state);
}

#endif // #ifndef VOGL_FBO_STATE_H
