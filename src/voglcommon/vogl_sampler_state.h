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

// File: vogl_sampler_state.h
#ifndef VOGL_SAMPLER_STATE_H
#define VOGL_SAMPLER_STATE_H

#include "vogl_core.h"
#include "vogl_ktx_texture.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"
#include "vogl_vec.h"

class vogl_sampler_state : public vogl_gl_object_state
{
public:
    vogl_sampler_state();
    virtual ~vogl_sampler_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTSampler;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_SAMPLERS;
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

    const vogl_state_vector &get_params() const
    {
        return m_params;
    }

    virtual bool is_valid() const
    {
        return m_is_valid;
    }

    // Content comparison, ignores handles.
    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;

private:
    GLuint m_snapshot_handle;
    vogl_state_vector m_params;

    bool m_is_valid;

    bool set_sampler_parameter(GLuint handle, GLenum pname) const;
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_sampler_state);
}

#endif // VOGL_TEXTURE_SAMPLER_H
