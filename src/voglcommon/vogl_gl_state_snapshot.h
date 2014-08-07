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

// File: vogl_gl_state_snapshot.h
#ifndef VOGL_GL_STATE_SNAPSHOT_H
#define VOGL_GL_STATE_SNAPSHOT_H

#include "vogl_core.h"
#include "vogl_sparse_vector.h"
#include "vogl_md5.h"

#include "vogl_common.h"
#include "vogl_general_context_state.h"
#include "vogl_texture_state.h"
#include "vogl_buffer_state.h"
#include "vogl_fbo_state.h"
#include "vogl_sampler_state.h"
#include "vogl_shader_state.h"
#include "vogl_program_state.h"
#include "vogl_renderbuffer_state.h"
#include "vogl_query_state.h"
#include "vogl_vao_state.h"
#include "vogl_texenv_state.h"
#include "vogl_light_state.h"
#include "vogl_material_state.h"
#include "vogl_display_list_state.h"
#include "vogl_matrix_state.h"
#include "vogl_current_vertex_attrib_state.h"
#include "vogl_arb_program_state.h"
#include "vogl_handle_tracker.h"
#include "vogl_default_framebuffer_state.h"
#include "vogl_sso_state.h"

//----------------------------------------------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------------------------------------------
typedef vogl::hash_map<GLuint, GLenum> vogl_handle_hash_map;
typedef vogl::hash_map<GLuint> vogl_handle_hash_set;
typedef vogl::hash_map<uint64_t, empty_type> vogl_sync_hash_set;

//----------------------------------------------------------------------------------------------------------------------
// vogl_handle_to_sync
//----------------------------------------------------------------------------------------------------------------------
inline GLsync vogl_handle_to_sync(uint64_t handle)
{
    GLsync sync = 0;
    memcpy(&sync, &handle, math::minimum<uint32_t>(sizeof(GLsync), sizeof(uint64_t)));
    return sync;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_sync_to_handle
//----------------------------------------------------------------------------------------------------------------------
inline uint64_t vogl_sync_to_handle(GLsync sync)
{
    uint64_t handle = 0;
    memcpy(&handle, &sync, math::minimum<uint32_t>(sizeof(GLsync), sizeof(uint64_t)));
    return handle;
}

//----------------------------------------------------------------------------------------------------------------------
// struct vogl_mapped_buffer_desc
//----------------------------------------------------------------------------------------------------------------------
struct vogl_mapped_buffer_desc
{
    // Handles and ptrs here are in the replay/GL domain
    GLuint m_buffer;
    GLenum m_target;
    vogl_trace_ptr_value m_offset;
    vogl_trace_ptr_value m_length;
    GLbitfield m_access;
    bool m_range;
    void *m_pPtr;

    vogl_mapped_buffer_desc()
    {
        clear();
    }

    void clear()
    {
        memset(this, 0, sizeof(*this));
    }
};

typedef vogl::vector<vogl_mapped_buffer_desc> vogl_mapped_buffer_desc_vec;

//----------------------------------------------------------------------------------------------------------------------
// class vogl_capture_context_params
// TODO: Rename this to vogl_context_state_shadow?
//----------------------------------------------------------------------------------------------------------------------
class vogl_capture_context_params
{
public:
    vogl_capture_context_params()
        : m_rbos(VOGL_NAMESPACE_RENDER_BUFFERS),
          m_textures(VOGL_NAMESPACE_TEXTURES),
          m_objs(VOGL_NAMESPACE_PROGRAMS),
          m_program_pipelines(VOGL_NAMESPACE_PIPELINES),
          m_filter_program_handles(false)
    {
        VOGL_FUNC_TRACER
    }

    void clear()
    {
        VOGL_FUNC_TRACER

        m_query_targets.clear();
        m_buffer_targets.clear();
        m_samplers.clear();
        m_vaos.clear();
        m_syncs.clear();
        m_framebuffers.clear();
        m_display_lists.clear();
        m_arb_program_targets.clear();

        m_rbos.clear();
        m_textures.clear();
        m_objs.clear();
        m_program_pipelines.clear();

        m_linked_programs.clear();

        m_program_handles_filter.clear();
        m_filter_program_handles = false;
    }

    // During tracing: All handles live in the tracing GL namespace (there is no replay namespace).
    // During replay: Any handles here live in the actual GL namespace (i.e. the "replay" or native GL namespace, NOT the trace namespace).
    vogl_handle_hash_map m_query_targets;
    vogl_handle_hash_map m_buffer_targets;
    vogl_handle_hash_set m_samplers;
    vogl_handle_hash_set m_vaos;
    vogl_sync_hash_set m_syncs;
    vogl_handle_hash_set m_framebuffers;
    vogl_handle_hash_map m_arb_program_targets;

    // During replay: trace domain
    vogl_display_list_state m_display_lists;

    // During replay: replay domain
    vogl_linked_program_state m_linked_programs;
    vogl_mapped_buffer_desc_vec m_mapped_buffers;

    // During replay: These objects map from trace (non-inv) to replay (inv).
    // TODO: Transition ALL the above hash sets/maps to instances of vogl_handle_tracker.
    vogl_handle_tracker m_rbos;
    vogl_handle_tracker m_textures;
    vogl_handle_tracker m_objs;
    vogl_handle_tracker m_program_pipelines;

    vogl_handle_hash_set m_program_handles_filter;
    bool m_filter_program_handles;
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_state_snapshot
//----------------------------------------------------------------------------------------------------------------------
class vogl_context_snapshot
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_context_snapshot);

public:
    vogl_context_snapshot();
    ~vogl_context_snapshot();

    void clear();

    bool capture(const vogl_context_desc &desc, const vogl_context_info &info, const vogl_capture_context_params &capture_params, vogl_handle_remapper &remapper);

    bool is_valid() const
    {
        return m_is_valid;
    }

    const vogl_context_desc &get_context_desc() const
    {
        return m_context_desc;
    }
    const vogl_context_info &get_context_info() const
    {
        return m_context_info;
    }

    bool remap_handles(vogl_handle_remapper &remapper);

    const vogl_general_context_state &get_general_state() const
    {
        return m_general_state;
    }
    vogl_general_context_state &get_general_state()
    {
        return m_general_state;
    }

    const vogl_texenv_state &get_texenv_state() const
    {
        return m_texenv_state;
    }
    vogl_texenv_state &get_texenv_state()
    {
        return m_texenv_state;
    }

    const vogl_light_state &get_light_state() const
    {
        return m_light_state;
    }
    vogl_light_state &get_light_state()
    {
        return m_light_state;
    }

    const vogl_material_state &get_material_state() const
    {
        return m_material_state;
    }
    vogl_material_state &get_material_state()
    {
        return m_material_state;
    }

    const vogl_display_list_state &get_display_list_state() const
    {
        return m_display_list_state;
    }
    vogl_display_list_state &get_display_list_state()
    {
        return m_display_list_state;
    }

    const vogl_matrix_state &get_matrix_state() const
    {
        return m_matrix_state;
    }
    vogl_matrix_state &get_matrix_state()
    {
        return m_matrix_state;
    }

    const vogl_polygon_stipple_state &get_polygon_stipple_state() const
    {
        return m_polygon_stipple_state;
    }
    vogl_polygon_stipple_state &get_polygon_stipple_state()
    {
        return m_polygon_stipple_state;
    }

    const vogl_gl_object_state_ptr_vec &get_objects() const
    {
        return m_object_ptrs;
    }
    vogl_gl_object_state_ptr_vec &get_objects()
    {
        return m_object_ptrs;
    }

    const vogl_current_vertex_attrib_state &get_current_vertex_attrib_state() const
    {
        return m_current_vertex_attrib_state;
    }
    vogl_current_vertex_attrib_state &get_current_vertex_attrib_state()
    {
        return m_current_vertex_attrib_state;
    }

    const vogl_arb_program_environment_state &get_arb_program_environment_state() const
    {
        return m_arb_program_environment_state;
    }
    vogl_arb_program_environment_state &get_arb_program_environment_state()
    {
        return m_arb_program_environment_state;
    }

    void get_all_objects_of_category(vogl_gl_object_state_type state_type, vogl_gl_object_state_ptr_vec &obj_ptr_vec) const;

    bool serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes);

private:
    vogl_context_desc m_context_desc;
    vogl_context_info m_context_info;

    vogl_general_context_state m_general_state;
    vogl_texenv_state m_texenv_state;
    vogl_light_state m_light_state;
    vogl_material_state m_material_state;
    vogl_display_list_state m_display_list_state;
    vogl_matrix_state m_matrix_state;
    vogl_polygon_stipple_state m_polygon_stipple_state;
    vogl_current_vertex_attrib_state m_current_vertex_attrib_state;
    vogl_arb_program_environment_state m_arb_program_environment_state;

    vogl_gl_object_state_ptr_vec m_object_ptrs;

    bool capture_objects(vogl_gl_object_state_type state_type, const vogl_capture_context_params &capture_params, vogl_handle_remapper &remapper);

    bool m_is_valid;

    void destroy_objects();
};

typedef vogl::vector<vogl_context_snapshot *> vogl_context_snapshot_ptr_vec;

//----------------------------------------------------------------------------------------------------------------------
// struct vogl_client_side_array_desc
//----------------------------------------------------------------------------------------------------------------------
struct vogl_client_side_array_desc
{
    vogl_client_side_array_desc()
    {
    }
    vogl_client_side_array_desc(vogl_trace_ptr_value ptr, uint32_t size)
        : m_ptr(ptr), m_size(size)
    {
    }

    void init(vogl_trace_ptr_value ptr, uint32_t size)
    {
        m_ptr = ptr;
        m_size = size;
    }

    vogl_trace_ptr_value m_ptr;
    uint32_t m_size;

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(blob_manager);

        node.add_key_value("ptr", m_ptr);
        node.add_key_value("size", m_size);
        return true;
    }

    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(blob_manager);

        m_ptr = node.value_as_uint64("ptr");
        m_size = node.value_as_uint32("size");
        return true;
    }
};

typedef vogl::vector<vogl_client_side_array_desc> vogl_client_side_array_desc_vec;

//----------------------------------------------------------------------------------------------------------------------
// class vogl_gl_state_snapshot
//----------------------------------------------------------------------------------------------------------------------
class vogl_gl_state_snapshot
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_gl_state_snapshot);

public:
    vogl_gl_state_snapshot();
    ~vogl_gl_state_snapshot();

    void clear();

    // frame_index indicates the beginning of frame X, at a swap boundary
    bool begin_capture(uint32_t window_width, uint32_t window_height, vogl_trace_ptr_value cur_context, uint32_t frame_index, int64_t gl_call_counter, bool at_frame_boundary);

    void add_client_side_array_ptrs(const vogl_client_side_array_desc_vec &client_side_vertex_attrib_ptrs, const vogl_client_side_array_desc_vec &client_side_array_ptrs, const vogl_client_side_array_desc_vec &client_side_texcoord_ptrs);

    bool capture_context(
        const vogl_context_desc &desc, const vogl_context_info &info, vogl_handle_remapper &remapper,
        const vogl_capture_context_params &capture_params);

    bool end_capture();

    bool is_valid() const
    {
        return m_is_valid;
    }

    bool serialize(json_node &node, vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager, const vogl_ctypes *pCtypes);

    md5_hash get_uuid() const
    {
        return m_uuid;
    }
    uint32_t get_window_width() const
    {
        return m_window_width;
    }
    uint32_t get_window_height() const
    {
        return m_window_height;
    }
    vogl_trace_ptr_value get_cur_trace_context() const
    {
        return m_cur_trace_context;
    }

    void set_frame_index(uint32_t frame_index)
    {
        m_frame_index = frame_index;
    }
    uint32_t get_frame_index() const
    {
        return m_frame_index;
    }
    int64_t get_gl_call_counter() const
    {
        return m_gl_call_counter;
    }
    bool get_at_frame_boundary() const
    {
        return m_at_frame_boundary;
    }

    bool get_is_restorable() const
    {
        return m_is_restorable;
    }
    void set_is_restorable(bool is_restorable)
    {
        m_is_restorable = is_restorable;
    }

    const vogl_client_side_array_desc_vec &get_client_side_vertex_attrib_ptrs() const
    {
        return m_client_side_vertex_attrib_ptrs;
    }
    const vogl_client_side_array_desc_vec &get_client_side_array_ptrs() const
    {
        return m_client_side_array_ptrs;
    }
    const vogl_client_side_array_desc_vec &get_client_side_texcoord_ptrs() const
    {
        return m_client_side_texcoord_ptrs;
    }

    const vogl_context_snapshot_ptr_vec &get_contexts() const
    {
        return m_context_ptrs;
    }
    vogl_context_snapshot_ptr_vec &get_contexts()
    {
        return m_context_ptrs;
    }

    const vogl_default_framebuffer_state &get_default_framebuffer() const
    {
        return m_default_framebuffer;
    }
    vogl_default_framebuffer_state &get_default_framebuffer()
    {
        return m_default_framebuffer;
    }
    
    vogl_context_snapshot* get_context(vogl_trace_ptr_value contextHandle) const
    {
        for (vogl_context_snapshot_ptr_vec::const_iterator iter = m_context_ptrs.begin(); iter != m_context_ptrs.end(); iter++)
        {
            if ((*iter)->get_context_desc().get_trace_context() == contextHandle)
            {
                return (*iter);
            }
        }
        return NULL;
    }

private:
    md5_hash m_uuid;
    uint32_t m_window_width;
    uint32_t m_window_height;
    vogl_trace_ptr_value m_cur_trace_context;
    uint32_t m_frame_index;
    int64_t m_gl_call_counter;
    bool m_at_frame_boundary;
    bool m_is_restorable;

    vogl_client_side_array_desc_vec m_client_side_vertex_attrib_ptrs;
    vogl_client_side_array_desc_vec m_client_side_array_ptrs;
    vogl_client_side_array_desc_vec m_client_side_texcoord_ptrs;

    vogl_context_snapshot_ptr_vec m_context_ptrs;

    vogl_default_framebuffer_state m_default_framebuffer;

    bool m_captured_default_framebuffer;
    bool m_is_valid;

    void destroy_contexts()
    {
        for (uint32_t i = 0; i < m_context_ptrs.size(); i++)
            vogl_delete(m_context_ptrs[i]);
        m_context_ptrs.clear();
    }
};

namespace vogl
{
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_context_snapshot);
    VOGL_DEFINE_BITWISE_MOVABLE(vogl_gl_state_snapshot);
}

#endif // VOGL_GL_STATE_SNAPSHOT_H
