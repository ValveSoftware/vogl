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

// File: vogl_program_state.h
#ifndef VOGL_PROGRAM_STATE_H
#define VOGL_PROGRAM_STATE_H

#include "vogl_common.h"
#include "vogl_dynamic_string.h"
#include "vogl_json.h"
#include "vogl_map.h"
#include "vogl_unique_ptr.h"

#include "vogl_general_context_state.h"
#include "vogl_blob_manager.h"
#include "vogl_shader_state.h"

struct vogl_program_attrib_state
{
    GLint m_size;
    GLenum m_type;
    dynamic_string m_name;
    GLint m_bound_location;

    void clear()
    {
        m_size = 0;
        m_type = GL_NONE;
        m_name.clear();
        m_bound_location = 0;
    }

    inline bool operator==(const vogl_program_attrib_state &rhs) const
    {
        return (m_size == rhs.m_size) && (m_type == rhs.m_type) && (!m_name.compare(rhs.m_name, true)) && (m_bound_location == rhs.m_bound_location);
    }
};

typedef vogl::vector<vogl_program_attrib_state> vogl_attrib_state_vec;

struct vogl_program_uniform_state
{
    GLint m_size;
    GLenum m_type;
    dynamic_string m_name;
    GLint m_base_location;
    growable_array<uint8_t, 16> m_data;

    void clear()
    {
        m_size = 0;
        m_type = GL_NONE;
        m_name.clear();
        m_base_location = 0;
        m_data.clear();
    }

    inline bool operator==(const vogl_program_uniform_state &rhs) const
    {
        return (m_size == rhs.m_size) && (m_type == rhs.m_type) && (!m_name.compare(rhs.m_name, true)) && (m_base_location == rhs.m_base_location) && (m_data == rhs.m_data);
    }
};

typedef vogl::vector<vogl_program_uniform_state> vogl_uniform_state_vec;

enum vogl_referenced_by_flags {
    cBufferReferencedByVertex = 1,
    cBufferReferencedByTesselationControl = 2,
    cBufferReferencedByTesselationEvaluation = 4,
    cBufferReferencedByGeometry = 8,
    cBufferReferencedByFragment = 16,
    cBufferReferencedByCompute = 32,
};

struct vogl_program_uniform_block_state
{
    GLuint m_uniform_block_index;
    dynamic_string m_name;
    GLint m_uniform_block_binding_point;
    GLint m_uniform_block_data_size;
    GLint m_uniform_block_active_uniforms;
    int m_referenced_by;

    void clear()
    {
        m_uniform_block_index = 0;
        m_name.clear();
        m_uniform_block_binding_point = 0;
        m_uniform_block_data_size = 0;
        m_uniform_block_active_uniforms = 0;
        m_referenced_by = 0;
    }

    inline bool operator==(const vogl_program_uniform_block_state &rhs) const
    {
#define CMP(x)      \
    if (x != rhs.x) \
        return false;
        CMP(m_uniform_block_index)
        CMP(m_name)
        CMP(m_uniform_block_binding_point)
        CMP(m_uniform_block_data_size)
        CMP(m_uniform_block_active_uniforms)
        CMP(m_referenced_by)
#undef CMP
        return true;
    }
};

typedef vogl::vector<vogl_program_uniform_block_state> vogl_uniform_block_state_vec;

struct vogl_program_output_state
{
    dynamic_string m_name;
    GLint m_location;
    GLint m_location_index;
    GLenum m_type;
    GLint m_array_size;
    bool m_is_per_patch;
    //GLint m_location_component; // TODO

    void clear()
    {
        utils::zero_object(*this);
    }

    inline bool operator==(const vogl_program_output_state &rhs) const
    {
#define CMP(x)      \
    if (x != rhs.x) \
        return false;
        CMP(m_name)
        CMP(m_location);
        CMP(m_location_index);
        CMP(m_type);
        CMP(m_array_size);
        CMP(m_is_per_patch);
//CMP(m_location_component);
#undef CMP
        return true;
    }
};

typedef vogl::vector<vogl_program_output_state> vogl_program_output_state_vec;

struct vogl_program_transform_feedback_varying
{
    GLint m_index;
    dynamic_string m_name;
    GLsizei m_size;
    GLenum m_type;

    void clear()
    {
        m_index = 0;
        m_name.clear();
        m_size = 0;
        m_type = GL_NONE;
    }

    inline bool operator==(const vogl_program_transform_feedback_varying &rhs) const
    {
#define CMP(x)      \
    if (x != rhs.x) \
        return false;
        CMP(m_index)
        CMP(m_name);
        CMP(m_size);
        CMP(m_type);
#undef CMP
        return true;
    }
};

typedef vogl::vector<vogl_program_transform_feedback_varying> vogl_program_transform_feedback_varying_vec;

class vogl_program_state : public vogl_gl_object_state
{
public:
    vogl_program_state();
    vogl_program_state(const vogl_program_state &other);
    virtual ~vogl_program_state();

    vogl_program_state &operator=(const vogl_program_state &rhs);

    virtual vogl_gl_object_state_type get_type() const
    {
        return cGLSTProgram;
    }
    virtual vogl_namespace_t get_handle_namespace() const
    {
        return VOGL_NAMESPACE_PROGRAMS;
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

    virtual bool get_marked_for_deletion() const
    {
        return m_marked_for_deletion;
    }

    const dynamic_string &get_info_log() const
    {
        return m_info_log;
    }

    bool link_snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, gl_entrypoint_id_t link_entrypoint, GLuint handle, const void *pBinary = NULL, uint32_t binary_size = 0, GLenum binary_format = GL_NONE, GLenum type = GL_NONE, GLsizei count = 0, GLchar *const *strings = NULL);

    void set_link_time_snapshot(vogl_unique_ptr<vogl_program_state> &pSnapshot);

    const vogl_unique_ptr<vogl_program_state> &get_link_time_snapshot() const
    {
        return m_pLink_time_snapshot;
    }
    vogl_unique_ptr<vogl_program_state> &get_link_time_snapshot()
    {
        return m_pLink_time_snapshot;
    }

    bool has_link_time_snapshot() const
    {
        return m_pLink_time_snapshot.get() != NULL;
    }

    bool is_link_snapshot() const
    {
        return m_link_snapshot;
    }

    bool get_link_status() const
    {
        return m_link_status;
    }
    bool get_separable() const
    {
        return m_separable;
    }
    bool get_verify_status() const
    {
        return m_verify_status;
    }

    uint32_t get_num_active_attribs() const
    {
        return m_num_active_attribs;
    }
    uint32_t get_num_active_uniforms() const
    {
        return m_num_active_uniforms;
    }
    uint32_t get_num_active_uniform_blocks() const
    {
        return m_num_active_uniform_blocks;
    }

    typedef vogl::vector<GLuint> attached_shader_vec;
    const attached_shader_vec &get_attached_shaders() const
    {
        return m_attached_shaders;
    }

    const vogl_attrib_state_vec &get_attrib_state_vec() const
    {
        return m_attribs;
    }

    const vogl_uniform_state_vec &get_uniform_state_vec() const
    {
        return m_uniforms;
    }

    const vogl_uniform_block_state_vec &get_uniform_block_state_vec()
    {
        return m_uniform_blocks;
    }

    const uint8_vec &get_program_binary() const
    {
        return m_program_binary;
    }
    GLenum get_program_binary_format() const
    {
        return m_program_binary_format;
    }

    uint32_t get_program_binary_size() const
    {
        return m_program_binary.size();
    }

    const vogl_shader_state_vec &get_shaders() const
    {
        return m_shaders;
    }

    vogl_shader_state_vec &get_shaders()
    {
        return m_shaders;
    }

    const vogl_program_output_state_vec &get_outputs() const
    {
        return m_outputs;
    }

    // The LATEST transform feedback mode.
    GLenum get_transform_feedback_mode() const
    {
        return m_transform_feedback_mode;
    }

    // These are the varyings as of the last link, NOT the very latest varyings.
    uint32_t get_transform_feedback_num_varyings() const
    {
        return m_transform_feedback_num_varyings;
    }

    // These are the varyings as of the last link, NOT the very latest varyings.
    const vogl_program_transform_feedback_varying_vec &get_varyings() const
    {
        return m_varyings;
    }

    bool compare_full_state(const vogl_program_state &rhs) const;

private:
    gl_entrypoint_id_t m_link_entrypoint;
    GLuint m_snapshot_handle;

    uint8_vec m_program_binary;
    GLenum m_program_binary_format;

    attached_shader_vec m_attached_shaders;

    dynamic_string m_info_log;

    uint32_t m_num_active_attribs;
    uint32_t m_num_active_uniforms;
    uint32_t m_num_active_uniform_blocks;

    vogl_attrib_state_vec m_attribs;
    vogl_uniform_state_vec m_uniforms;
    vogl_uniform_block_state_vec m_uniform_blocks;

    vogl_shader_state_vec m_shaders;

    vogl_program_output_state_vec m_outputs;

    vogl_unique_ptr<vogl_program_state> m_pLink_time_snapshot;

    GLenum m_transform_feedback_mode;
    uint32_t m_transform_feedback_num_varyings;
    vogl_program_transform_feedback_varying_vec m_varyings;

    GLenum m_create_shader_program_type;
    dynamic_string_array m_create_shader_program_strings;

    bool m_marked_for_deletion;
    bool m_link_status;
    bool m_separable;
    bool m_verify_status;
    bool m_link_snapshot;

    bool m_is_valid;

    bool snapshot_uniforms(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_uniform_blocks(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_active_attribs(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_attached_shaders(const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool linked_using_binary);
    bool snapshot_shader_objects(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_info_log(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_program_binary(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_basic_info(const vogl_context_info &context_info, vogl_handle_remapper &remapper);
    bool snapshot_outputs(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint program);
    bool snapshot_transform_feedback(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint program);
    bool restore_uniforms(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const;
    bool restore_uniform_blocks(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const;
    bool restore_active_attribs(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const;
    bool restore_outputs(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const;
    bool restore_transform_feedback(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors) const;
    bool restore_link_snapshot(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors, bool &link_succeeded) const;
    bool link_program(uint32_t handle32, const vogl_context_info &context_info, vogl_handle_remapper &remapper, bool &any_restore_warnings, bool &any_gl_errors, bool &link_succeeded) const;
};

typedef vogl::map<GLuint, vogl_program_state> vogl_program_state_map;

class vogl_linked_program_state
{
public:
    vogl_linked_program_state();
    vogl_linked_program_state(const vogl_linked_program_state &other);
    ~vogl_linked_program_state();

    vogl_linked_program_state &operator=(const vogl_linked_program_state &rhs);

    void clear();

    bool add_snapshot(GLuint handle, const vogl_program_state &prog);
    bool add_snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, gl_entrypoint_id_t link_entrypoint, GLuint handle, GLenum binary_format = GL_NONE, const void *pBinary = NULL, uint32_t binary_size = 0);
    bool add_snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, gl_entrypoint_id_t link_entrypoint, GLuint handle, GLenum type, GLsizei count, GLchar *const *strings);
    bool remove_snapshot(GLuint handle);

    const vogl_program_state *find_snapshot(GLuint handle) const;
    vogl_program_state *find_snapshot(GLuint handle);

    bool serialize(json_node &node, vogl_blob_manager &blob_manager) const;
    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager);

    bool remap_handles(vogl_handle_remapper &remapper);

    const vogl_program_state_map &get_linked_program_map() const
    {
        return m_linked_programs;
    }

    vogl_program_state_map &get_linked_program_map()
    {
        return m_linked_programs;
    }

private:
    vogl_program_state_map m_linked_programs;
};

#endif // VOGL_PROGRAM_STATE_H
