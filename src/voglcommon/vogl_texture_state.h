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

// File: vogl_texture_state.h
#ifndef VOGL_TEXTURE_STATE_H
#define VOGL_TEXTURE_STATE_H

#include "vogl_core.h"
#include "vogl_ktx_texture.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"
#include "vogl_vec.h"

class vogl_texture_state : public vogl_gl_object_state
{
public:
    vogl_texture_state();
    virtual ~vogl_texture_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTTexture;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_TEXTURES;
    }

    // Creates snapshot of a texture handle
    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target);

    // Creates and restores a texture
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

    GLenum get_target() const
    {
        return m_target;
    }

    // Buffer textures will have a GL_TEXTURE_INTERNAL_FORMAT defined in m_params, but nothing else.
    GLuint get_buffer() const
    {
        return m_buffer;
    }

    // Note: Multisampling textures may only have 1 sample!
    uint32_t get_num_samples() const
    {
        return m_num_samples;
    }

    vogl::ktx_texture &get_texture(uint32_t sample_index = 0)
    {
        return m_textures[sample_index];
    }

    const vogl::ktx_texture &get_texture(uint32_t sample_index = 0) const
    {
        return m_textures[sample_index];
    }

    const vogl_state_vector &get_params() const
    {
        return m_params;
    }

    uint32_t get_num_levels() const
    {
        return m_level_params[0].size();
    }

    const vogl_state_vector &get_level_params(uint32_t face, uint32_t level) const
    {
        return m_level_params[face][level];
    }

    // Content comparison, ignores handle.
    virtual bool compare_restorable_state(const vogl_gl_object_state &rhs_obj) const;

private:
    GLuint m_snapshot_handle;
    GLenum m_target;
    GLuint m_buffer;

    uint32_t m_num_samples;
    enum { cMaxSamples = 32 };
    vogl::ktx_texture m_textures[cMaxSamples];

    vogl_state_vector m_params;

    typedef vogl::vector<vogl_state_vector> vogl_state_vector_array;

    enum
    {
        cCubeMapFaces = 6
    };

    // For MSAA textures: m_level_params are the params of the original MSAA texture
    vogl_state_vector_array m_level_params[cCubeMapFaces];

    bool m_is_unquerable;
    bool m_is_valid;

    bool set_tex_parameter(GLenum pname) const;
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_texture_state);
}

#endif // VOGL_TEXTURE_STATE_H
