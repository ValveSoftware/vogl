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

// File: vogl_context_info.h
#pragma once

#include "vogl_common.h"
#include "vogl_dynamic_string.h"

#include "vogl_json.h"
#include "vogl_blob_manager.h"

enum vogl_gl_version_t
{
    VOGL_GL_VERSION_UNKNOWN = 0x000000,
    VOGL_GL_VERSION_1_0 = 0x010000,
    VOGL_GL_VERSION_1_1 = 0x010100,
    VOGL_GL_VERSION_1_2 = 0x010200,
    VOGL_GL_VERSION_1_2_1 = 0x010201,
    VOGL_GL_VERSION_1_3 = 0x010300,
    VOGL_GL_VERSION_1_4 = 0x010400,
    VOGL_GL_VERSION_1_5 = 0x010500,
    VOGL_GL_VERSION_2_0 = 0x020000,
    VOGL_GL_VERSION_2_1 = 0x020100,
    VOGL_GL_VERSION_3_0 = 0x030000,
    VOGL_GL_VERSION_3_1 = 0x030100,
    VOGL_GL_VERSION_3_2 = 0x030200,
    VOGL_GL_VERSION_3_3 = 0x030300,
    VOGL_GL_VERSION_4_0 = 0x040000,
    VOGL_GL_VERSION_4_1 = 0x040100,
    VOGL_GL_VERSION_4_2 = 0x040200,
    VOGL_GL_VERSION_4_3 = 0x040300,
    VOGL_GL_VERSION_4_4 = 0x040400,
    VOGL_GL_VERSION_4_5 = 0x040500,
    VOGL_GL_VERSION_4_6 = 0x040600,
    VOGL_GL_VERSION_4_7 = 0x040700,
    VOGL_GL_VERSION_4_8 = 0x040800,
    VOGL_GL_VERSION_4_9 = 0x040900,
    VOGL_GL_VERSION_5_0 = 0x050000,
    VOGL_GL_VERSION_5_1 = 0x050100,
    VOGL_GL_VERSION_5_2 = 0x050200,
    VOGL_GL_VERSION_5_3 = 0x050300,
    VOGL_GL_VERSION_5_4 = 0x050400,
    VOGL_GL_VERSION_5_5 = 0x050500
};

#define VOGL_CREATE_GL_VERSION(major, minor, patch) static_cast<vogl_gl_version_t>(((major) << 16) | ((minor) << 8) | (patch))

#define VOGL_GL_VERSION_GET_MAJOR(x) (((x) >> 16) & 0xFF)
#define VOGL_GL_VERSION_GET_MINOR(x) (((x) >> 8) & 0xFF)
#define VOGL_GL_VERSION_GET_PATCH(x) ((x) & 0xFF)

int vogl_determine_attrib_list_array_size(const int *attrib_list);

class vogl_context_attribs
{
public:
    typedef vogl::vector<int> attrib_vec;

    vogl_context_attribs();
    vogl_context_attribs(const int *pAttribs);
    vogl_context_attribs(const int *pAttribs, uint32_t num_attribs);
    vogl_context_attribs(const attrib_vec &vec);

    void init(const int *pAttribs);
    void init(const int *pAttribs, uint32_t num_attribs);
    void init(const attrib_vec &vec);

    void clear()
    {
        m_attribs.clear();
    }

    bool serialize(vogl::json_node &node) const;
    bool deserialize(const vogl::json_node &node);

    const attrib_vec &get_vec() const
    {
        return m_attribs;
    }
    attrib_vec &get_vec()
    {
        return m_attribs;
    }

    int *get_ptr()
    {
        return m_attribs.get_ptr();
    }
    const int *get_ptr() const
    {
        return m_attribs.get_ptr();
    }

    uint32_t size() const
    {
        return m_attribs.size();
    }
    int operator[](uint32_t i) const
    {
        return m_attribs[i];
    }
    int &operator[](uint32_t i)
    {
        return m_attribs[i];
    }

    // Returns offset of value following key, or -1 if not found (or if the key was found but it was at the very end of the int array).
    int find_value_ofs(int key_to_find) const;
    bool has_key(int key_to_find) const
    {
        return find_value_ofs(key_to_find) >= 0;
    }
    bool get_value(int key_to_find, int &val) const;
    int get_value_or_default(int key_to_find, int def = 0) const
    {
        int val = 0;
        if (!get_value(key_to_find, val))
            return def;
        return val;
    }
    void add_key(int key, int value);

    bool check() const;

private:
    attrib_vec m_attribs;
};

// TODO: Add replay context handle
class vogl_context_desc
{
public:
    vogl_context_desc()
    {
        VOGL_FUNC_TRACER

        clear();
    }

    vogl_context_desc(gl_entrypoint_id_t creation_func, GLboolean direct, vogl_trace_ptr_value trace_context, vogl_trace_ptr_value trace_share_context, const vogl_context_attribs &attribs)
    {
        VOGL_FUNC_TRACER

        clear();

        init(creation_func, direct, trace_context, trace_share_context, attribs);
    }

    void init(gl_entrypoint_id_t creation_func, GLboolean direct, vogl_trace_ptr_value trace_context, vogl_trace_ptr_value trace_share_context, const vogl_context_attribs &attribs)
    {
        VOGL_FUNC_TRACER

        m_creation_func = creation_func;
        m_direct = direct;
        m_trace_context = trace_context;
        m_trace_share_context = trace_share_context;
        m_attribs = attribs;
    }

    void clear()
    {
        VOGL_FUNC_TRACER

        m_creation_func = VOGL_ENTRYPOINT_INVALID;
        m_direct = false;
        m_trace_context = 0;
        m_trace_share_context = 0;
        m_attribs.clear();
    }

    bool serialize(json_node &node, const vogl_blob_manager &blob_manager) const
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(blob_manager);

        node.add_key_value("creation_func", (m_creation_func != VOGL_ENTRYPOINT_INVALID) ? g_vogl_entrypoint_descs[m_creation_func].m_pName : "");
        node.add_key_value("direct", m_direct);
        node.add_key_value("trace_context", static_cast<uint64_t>(m_trace_context));
        node.add_key_value("trace_share_handle", static_cast<uint64_t>(m_trace_share_context));

        return m_attribs.serialize(node);
    }

    bool deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
    {
        VOGL_FUNC_TRACER

        VOGL_NOTE_UNUSED(blob_manager);

        m_creation_func = vogl_find_entrypoint(node.value_as_string("creation_func"));
        m_direct = node.value_as_bool("direct");
        m_trace_context = static_cast<vogl_trace_ptr_value>(node.value_as_uint64("trace_context"));
        m_trace_share_context = static_cast<vogl_trace_ptr_value>(node.value_as_uint64("trace_share_handle"));

        return m_attribs.deserialize(node);
    }

    gl_entrypoint_id_t get_creation_func() const
    {
        return m_creation_func;
    }
    GLboolean get_direct() const
    {
        return m_direct;
    }
    vogl_trace_ptr_value get_trace_context() const
    {
        return m_trace_context;
    }
    vogl_trace_ptr_value get_trace_share_context() const
    {
        return m_trace_share_context;
    }

    const vogl_context_attribs &get_attribs() const
    {
        return m_attribs;
    }
    vogl_context_attribs &get_attribs()
    {
        return m_attribs;
    }

private:
    gl_entrypoint_id_t m_creation_func;
    GLboolean m_direct;
    vogl_trace_ptr_value m_trace_context;
    vogl_trace_ptr_value m_trace_share_context;

    vogl_context_attribs m_attribs;
};

class vogl_context_info
{
public:
    vogl_context_info();

    // Inits from the current GL context
    vogl_context_info(const vogl_context_desc &desc);

    // Inits from the current GL context
    bool init(const vogl_context_desc &desc);

    void clear();

    inline bool is_valid() const
    {
        return m_is_valid;
    }

    bool serialize(vogl::json_node &node, const vogl_blob_manager &blob_manager) const;
    bool deserialize(const vogl::json_node &node, const vogl_blob_manager &blob_manager);

    const vogl_context_desc &get_desc() const
    {
        return m_desc;
    }

    inline vogl_gl_version_t get_version() const
    {
        return m_version;
    }
    inline vogl_gl_version_t get_glsl_version() const
    {
        return m_glsl_version;
    }

    inline bool is_forward_compatible() const
    {
        return m_forward_compatible;
    }
    inline bool is_core_profile() const
    {
        return m_core_profile;
    }
    inline bool is_compatibility_profile() const
    {
        return !m_core_profile;
    }
    inline bool is_debug_context() const
    {
        return m_debug_context;
    }

    inline const vogl::dynamic_string &get_vendor_str() const
    {
        return m_vendor_str;
    }
    inline const vogl::dynamic_string &get_renderer_str() const
    {
        return m_renderer_str;
    }
    inline const vogl::dynamic_string &get_version_str() const
    {
        return m_version_str;
    }
    inline const vogl::dynamic_string &get_glsl_version_str() const
    {
        return m_glsl_version_str;
    }

    inline const vogl::dynamic_string_array &get_extensions() const
    {
        return m_extensions;
    }

    // Not case sensitive. Currently uses a binary search of strings, so it's not terribly slow but it's not super fast either.
    bool supports_extension(const char *pExt) const;

    uint32_t get_max_vertex_attribs() const
    {
        return m_max_vertex_attribs;
    }

    // compat only - will be 0 in core
    uint32_t get_max_texture_coords() const
    {
        return m_max_texture_coords;
    }
    // compat only - will be 0 in core
    uint32_t get_max_texture_units() const
    {
        return m_max_texture_units;
    }
    uint32_t get_max_texture_image_units() const
    {
        return m_max_texture_image_units;
    }
    uint32_t get_max_combined_texture_coords() const
    {
        return m_max_combined_texture_coords;
    }
    uint32_t get_max_draw_buffers() const
    {
        return m_max_draw_buffers;
    }
    uint32_t get_max_lights() const
    {
        return m_max_lights;
    }
    uint32_t get_max_uniform_buffer_bindings() const
    {
        return m_max_uniform_buffer_bindings;
    }
    uint32_t get_max_arb_program_matrices() const
    {
        return m_max_arb_program_matrices;
    }
    uint32_t get_max_arb_vertex_program_env_params() const
    {
        return m_max_arb_vertex_program_env_params;
    }
    uint32_t get_max_arb_fragment_program_env_params() const
    {
        return m_max_arb_fragment_program_env_params;
    }
    uint32_t get_max_transform_feedback_separate_attribs() const
    {
        return m_max_transform_feedback_separate_attribs;
    }

private:
    vogl_context_desc m_desc;

    vogl_gl_version_t m_version;
    vogl_gl_version_t m_glsl_version;

    bool m_forward_compatible;
    bool m_core_profile;
    bool m_debug_context;

    vogl::dynamic_string m_version_str;
    vogl::dynamic_string m_glsl_version_str;
    vogl::dynamic_string m_vendor_str;
    vogl::dynamic_string m_renderer_str;

    // Sorted driver reported extensions.
    vogl::dynamic_string_array m_extensions;
    // "Core" extensions are those extensions we know this context must support, independent of whether or not it actually appears in extensions.
    vogl::dynamic_string_array m_core_extensions;

    uint32_t m_max_vertex_attribs;
    uint32_t m_max_texture_coords;
    uint32_t m_max_texture_units;
    uint32_t m_max_texture_image_units;
    uint32_t m_max_combined_texture_coords;
    uint32_t m_max_draw_buffers;
    uint32_t m_max_lights;
    uint32_t m_max_uniform_buffer_bindings;
    uint32_t m_max_arb_program_matrices;
    uint32_t m_max_arb_vertex_program_env_params;
    uint32_t m_max_arb_fragment_program_env_params;
    uint32_t m_max_transform_feedback_separate_attribs;

    bool m_is_valid;

    void query_string(vogl::dynamic_string &str, GLenum name);
    vogl_gl_version_t parse_version_string(const vogl::dynamic_string &str);
    bool query_extensions();
    void init_context_limits();
    void determine_core_extensions();
};
