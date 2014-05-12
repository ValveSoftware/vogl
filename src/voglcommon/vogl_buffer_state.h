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

// File: vogl_buffer_state.h
#ifndef VOGL_BUFFER_STATE_H
#define VOGL_BUFFER_STATE_H

#include "vogl_core.h"
#include "vogl_ktx_texture.h"
#include "vogl_context_info.h"
#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"
#include "vogl_vec.h"

struct vogl_mapped_buffer_desc;

class vogl_buffer_state : public vogl_gl_object_state
{
public:
    vogl_buffer_state();
    virtual ~vogl_buffer_state();

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTBuffer;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_BUFFERS;
    }

    virtual bool snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target);

    void set_mapped_buffer_snapshot_state(const vogl_mapped_buffer_desc &map_desc);

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

    GLenum get_target() const
    {
        return m_target;
    }

    const uint8_vec &get_buffer_data() const
    {
        return m_buffer_data;
    }
    const vogl_state_vector &get_params() const
    {
        return m_params;
    }

    // Whether or not the buffer was currently mapped when it was snapshotted. Note we don't actually support snapshotting the buffer
    // while it's mapped (we unmap it first), this data comes from the replayer's shadow. We also don't restore it in a mapped state, that's up to the caller.
    bool get_is_map_range() const { return m_map_range; }
    bool get_is_mapped() const { return m_is_mapped; }
    uint64_t get_map_ofs() const { return m_map_ofs; }
    uint64_t get_map_size() const { return m_map_size; }
    uint32_t get_map_access() const { return m_map_access; }

private:
    GLuint m_snapshot_handle;
    GLenum m_target;

    uint8_vec m_buffer_data;

    vogl_state_vector m_params;

    uint64_t m_map_ofs;
    uint64_t m_map_size;
    uint32_t m_map_access;
    bool m_map_range;
    bool m_is_mapped;

    bool m_is_valid;
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_buffer_state);
}

#endif // VOGL_BUFFER_STATE_H
