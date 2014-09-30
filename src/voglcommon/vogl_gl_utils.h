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

// File: vogl_gl_utils.h
#ifndef VOGL_GL_UTILS_H
#define VOGL_GL_UTILS_H

// Loki
#include "TypeTraits.h"

#include "vogl_ctypes.h"

#include "vogl_hash_map.h"
#include "vogl_command_line_params.h"
#include "vogl_map.h"
#include "vogl_json.h"
#include "vogl_value.h"
#include "vogl_growable_array.h"
#include "vogl_matrix.h"

//----------------------------------------------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------------------------------------------
namespace vogl
{
    class json_value;
}

class vogl_context_info;
class vogl_blob_manager;
class vogl_context_desc;

typedef vogl::map<GLenum, int> GLenum_to_int_map;

//----------------------------------------------------------------------------------------------------------------------
// cast_val_to_uint64
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
static inline uint64_t cast_val_to_uint64(const T &value)
{
    int64_t res = 0;
    memcpy(&res, &value, VOGL_MIN(sizeof(uint64_t), sizeof(T)));
    return res;
}

//----------------------------------------------------------------------------------------------------------------------
// type/format/texture helpers
//----------------------------------------------------------------------------------------------------------------------
uint32_t vogl_get_gl_type_size(uint32_t ogl_type);
uint32_t vogl_get_image_format_channels(GLenum format);
void vogl_get_image_format_info(GLenum format, GLenum type, uint32_t &num_channels, uint32_t &bits_per_element, uint32_t &bits_per_pixel);

// vogl_get_image_size() reads the current pixel unpack state!
size_t vogl_get_image_size(GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth);
size_t vogl_get_tex_target_image_size(GLenum target, GLint level, GLenum format, GLenum type);
uint32_t vogl_get_image_format_size_in_bytes(GLenum format, GLenum type);

size_t vogl_determine_glMap1_size(GLenum target, GLint stride, GLint order);
size_t vogl_determine_glMap2_size(GLenum target, GLint ustride, GLint uorder, GLint vstride, GLint vorder);

//----------------------------------------------------------------------------------------------------------------------
// struct vogl_enum_desc
//----------------------------------------------------------------------------------------------------------------------
struct vogl_enum_desc
{
    inline vogl_enum_desc()
    {
    }

    inline vogl_enum_desc(uint64_t value, const char *pPrefix, const char *pSpec_type, const char *pGL_type, const char *pMacro_name)
        : m_value(value), m_prefix(pPrefix), m_spec_type(pSpec_type), m_gl_type(pGL_type), m_macro_name(pMacro_name)
    {
        VOGL_FUNC_TRACER
    }

    uint64_t m_value;
    vogl::dynamic_string m_prefix;
    vogl::dynamic_string m_spec_type;
    vogl::dynamic_string m_gl_type;
    vogl::dynamic_string m_macro_name;

    // Priorities are only used when trying to determine the best enum string to return given a value.
    // TODO: Move this to voglgen to avoid the runtime init cost.
    enum sort_priority_t
    {
        cSCNone = 0,
        cSCARB,
        cSCEXT,
        cSCVendor,
        cSCOES
    };

    sort_priority_t m_sort_priority;

    void init_sort_priority();

    bool operator<(const vogl_enum_desc &rhs) const;
};

typedef vogl::vector<vogl_enum_desc> vogl_enum_desc_vec;

//----------------------------------------------------------------------------------------------------------------------
// vogl_spec_type_value_enum_key
//----------------------------------------------------------------------------------------------------------------------
struct vogl_spec_type_and_value_enum_key
{
    inline vogl_spec_type_and_value_enum_key()
    {
    }

    inline vogl_spec_type_and_value_enum_key(const char *pSpec_type, uint64_t value)
        : m_spec_type(pSpec_type), m_value(value)
    {
        VOGL_FUNC_TRACER
    }

    vogl::dynamic_string m_spec_type;
    uint64_t m_value;

    inline bool operator==(const vogl_spec_type_and_value_enum_key &rhs) const
    {
        return (m_spec_type == rhs.m_spec_type) && (m_value == rhs.m_value);
    }

    inline operator size_t() const
    {
        return vogl::fast_hash_obj(m_value) ^ static_cast<size_t>(m_spec_type.get_hash());
    }
};

//----------------------------------------------------------------------------------------------------------------------
// enum vogl_state_type
// TODO: Rename this to vogl_pname_type or something
//----------------------------------------------------------------------------------------------------------------------
enum vogl_state_type
{
    // These enums MUST me consistent with the types in pname_defs.h
    cSTInvalid = 'X',
    cSTGLboolean = 'B',
    cSTGLenum = 'E',
    cSTInt32 = 'I',
    cSTUInt32 = 'U',
    cSTInt64 = 'i',
    cSTUInt64 = 'u',
    cSTFloat = 'F',
    cSTDouble = 'D',
    cSTPointer = 'P',
    cTotalStateTypes = 9
};

//----------------------------------------------------------------------------------------------------------------------
// enum helpers
//----------------------------------------------------------------------------------------------------------------------
class gl_enums
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(gl_enums);

public:
    gl_enums();

    // pPreferred_prefix should usually be "gl", or it can be "glx", "wgl", etc.
    // TODO: Add a helper that takes a GL entrypoint and param index from vogl_trace_packet::ctype_to_json_value()
    const char *find_name(uint64_t gl_enum, const char *pPreferred_prefix = NULL) const;

    const char *find_name(const char *pSpec_type, uint64_t gl_enum, const char *pPreferred_prefix = NULL) const;

    const char *find_name(uint64_t gl_enum, gl_entrypoint_id_t entrypoint_id, int param_index) const;

    const char *find_gl_name(uint64_t gl_enum) const
    {
        return find_name(gl_enum, "gl");
    }
    const char *find_glx_name(uint64_t gl_enum) const
    {
        return find_name(gl_enum, "glx");
    }
    const char *find_gl_error_name(GLenum gl_enum) const
    {
        return find_name("ErrorCode", gl_enum);
    }

    // Finds the name of a GL internal/base image format, if this fails then it just calls find_gl_name().
    const char *find_gl_image_format_name(GLenum gl_enum) const;

    // Returns -1 on failure
    enum
    {
        cUnknownEnum = 0xFFFFFFFFFFFFFFF0ULL
    };
    uint64_t find_enum(const dynamic_string &str) const;

    // This is driven via the pname table, but it may also call GL!
    int get_pname_count(uint64_t gl_enum) const;

    // This is driven via the pname table, but it may also call GL!
    int get_pname_type(uint64_t gl_enum) const;

    // Maps a GL enum to a pname definition index, or returns cInvalidIndex on failure.
    int find_pname_def_index(uint64_t gl_enum) const;

private:
    uint16_t m_gl_enum_to_pname_def_index[0x10000];

    typedef vogl::hash_map<dynamic_string, uint64_t> gl_enum_name_hash_map;
    gl_enum_name_hash_map m_enum_name_hash_map;

    typedef vogl::hash_map<uint64_t, vogl_enum_desc_vec> gl_enum_value_to_enum_desc_hash_map;
    gl_enum_value_to_enum_desc_hash_map m_enum_value_hash_map;

    typedef vogl::hash_map<vogl_spec_type_and_value_enum_key, vogl_enum_desc_vec> gl_spec_type_and_value_to_enum_desc_hash_map_t;
    gl_spec_type_and_value_to_enum_desc_hash_map_t m_spec_type_and_enum_value_hash_map;

    typedef vogl::hash_map<GLenum, const char *> vogl_image_format_hashmap_t;
    vogl_image_format_hashmap_t m_image_formats;

    void init_enum_descs();
    void add_enum(const char *pPrefix, const char *pSpec_type, const char *pGL_type, const char *pName, uint64_t value);
    void init_image_formats();
};

gl_enums &get_gl_enums();

GLenum vogl_get_json_value_as_enum(const json_node &node, const char *pKey, GLenum def = 0);
GLenum vogl_get_json_value_as_enum(const json_value &val, GLenum def = 0);

//----------------------------------------------------------------------------------------------------------------------
// Client side array tables
//----------------------------------------------------------------------------------------------------------------------
struct vogl_client_side_array_desc_t
{
    gl_entrypoint_id_t m_entrypoint;
    uint32_t m_is_enabled;  // glIsEnabled
    uint32_t m_get_binding; // glGetIntegerv - this gets the snapshotted binding
    uint32_t m_get_pointer; // glGetPointerv
    uint32_t m_get_size;    // will be 0 if there is no associated glGet
    uint32_t m_get_stride;  // glGetIntegerv
    uint32_t m_get_type;    // glGetIntegerv
};

#define VOGL_CLIENT_SIDE_ARRAY_DESC(id, entrypoint, is_enabled, get_binding, get_pointer, get_size, get_stride, get_type) id,
enum vogl_client_side_array_desc_id_t
{
#include "vogl_client_side_array_descs.inc"
    VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS
};
#undef VOGL_CLIENT_SIDE_ARRAY_DESC

extern const vogl_client_side_array_desc_t g_vogl_client_side_array_descs[];

//----------------------------------------------------------------------------------------------------------------------
// Misc GL helpers/wrappers
//----------------------------------------------------------------------------------------------------------------------
GLuint vogl_get_bound_gl_buffer(GLenum target);

// glGet() wrappers
GLint vogl_get_gl_integer(GLenum pname);
vec4I vogl_get_gl_vec4I(GLenum pname);
GLint vogl_get_gl_integer_indexed(GLenum pname, uint32_t index);
GLint64 vogl_get_gl_integer64(GLenum pname);
GLint64 vogl_get_gl_integer64_indexed(GLenum pname, uint32_t index);
GLboolean vogl_get_gl_boolean(GLenum pname);
GLboolean vogl_get_gl_boolean_indexed(GLenum pname, uint32_t index);
GLfloat vogl_get_gl_float(GLenum pname);
vec4F vogl_get_gl_vec4F(GLenum pname);
matrix44F vogl_get_gl_matrix44F(GLenum pname);
GLdouble vogl_get_gl_double(GLenum pname);
vec4D vogl_get_gl_vec4D(GLenum pname);
matrix44D vogl_get_gl_matrix44D(GLenum pname);

void vogl_reset_pixel_store_states();
void vogl_reset_pixel_transfer_states(const vogl_context_info &context_info);

bool vogl_copy_buffer_to_image(void *pDst, uint32_t dst_size, uint32_t width, uint32_t height, GLuint format = GL_RGB, GLuint type = GL_UNSIGNED_BYTE, bool flip_image = false, GLuint framebuffer = 0, GLuint read_buffer = GL_BACK, GLuint pixel_pack_buffer = 0);

//----------------------------------------------------------------------------------------------------------------------
// Entrypoint helpers
//----------------------------------------------------------------------------------------------------------------------

inline bool vogl_is_make_current_entrypoint(gl_entrypoint_id_t id)
{
    return (id == VOGL_ENTRYPOINT_glXMakeCurrent)
        || (id == VOGL_ENTRYPOINT_wglMakeCurrent)
        || (id == VOGL_ENTRYPOINT_wglMakeContextCurrentARB)
        || (id == VOGL_ENTRYPOINT_wglMakeContextCurrentEXT);
}

inline bool vogl_is_swap_buffers_entrypoint(gl_entrypoint_id_t id)
{
    return (id == VOGL_ENTRYPOINT_glXSwapBuffers) || (id == VOGL_ENTRYPOINT_wglSwapBuffers);
}

bool vogl_is_draw_entrypoint(gl_entrypoint_id_t id);
bool vogl_is_clear_entrypoint(gl_entrypoint_id_t id);
bool vogl_is_frame_buffer_write_entrypoint(gl_entrypoint_id_t id);
bool vogl_is_start_nested_entrypoint(gl_entrypoint_id_t id);
bool vogl_is_end_nested_entrypoint(gl_entrypoint_id_t id);
bool vogl_is_marker_push_entrypoint(gl_entrypoint_id_t id);
bool vogl_is_marker_pop_entrypoint(gl_entrypoint_id_t id);

//----------------------------------------------------------------------------------------------------------------------
// Error helpers
//----------------------------------------------------------------------------------------------------------------------
extern bool g_gl_get_error_enabled;

// Returns *true* if there was an error.
bool vogl_check_gl_error_internal(bool suppress_error_message = false, const char *pFile = "", uint32_t line = 0, const char *pFunc = "");
#define vogl_check_gl_error() (g_gl_get_error_enabled ? vogl_check_gl_error_internal(false, __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR) : false)
#define vogl_check_gl_error_suppress_message() (g_gl_get_error_enabled ? vogl_check_gl_error_internal(true, __FILE__, __LINE__, VOGL_FUNCTION_INFO_CSTR) : false)

#define VOGL_CHECK_GL_ERROR                          \
    do                                               \
    {                                                \
        bool _prev_gl_error = vogl_check_gl_error(); \
        VOGL_ASSERT(!_prev_gl_error);                \
        VOGL_NOTE_UNUSED(_prev_gl_error);            \
    } while (0)

void vogl_enable_gl_get_error();
void vogl_disable_gl_get_error();
bool vogl_is_gl_get_error_enabled();

//----------------------------------------------------------------------------------------------------------------------
// Binding helpers
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_binding_from_target(GLenum target);
GLenum vogl_get_object_category_from_binding(GLenum binding);
GLenum vogl_get_object_category_from_target(GLenum target);
GLenum vogl_get_target_from_binding(GLenum binding);

void vogl_bind_object(GLenum target, GLuint handle);
GLuint vogl_get_bound_object(GLenum target);

void vogl_debug_message_control(const vogl_context_info &context_info, GLenum err_code, bool enabled);

// NOTE: Don't use this for anything other than debugging!
GLenum vogl_determine_texture_target(const vogl_context_info &context_info, GLuint handle);

int vogl_gl_get_uniform_size_in_GLints(GLenum type);
int vogl_gl_get_uniform_size_in_bytes(GLenum type);
int vogl_gl_get_uniform_base_type(GLenum type);

//----------------------------------------------------------------------------------------------------------------------
// class vogl_binding_state
//----------------------------------------------------------------------------------------------------------------------
class vogl_binding_state
{
    struct saved_binding
    {
        saved_binding()
        {
        }
        saved_binding(GLenum target, GLenum handle)
            : m_target(target), m_handle(handle)
        {
        }
        void set(GLenum target, GLenum handle)
        {
            m_target = target;
            m_handle = handle;
        }

        GLenum m_target;
        GLuint m_handle;
    };

    vogl::growable_array<saved_binding, 16> m_bindings;

public:
    vogl_binding_state()
    {
        VOGL_FUNC_TRACER
    }
    ~vogl_binding_state()
    {
        VOGL_FUNC_TRACER
    }

    void clear()
    {
        VOGL_FUNC_TRACER m_bindings.clear();
    }

    void save(GLenum target)
    {
        VOGL_FUNC_TRACER

        GLuint handle = vogl_get_bound_object(target);
        m_bindings.enlarge(1)->set(target, handle);
    }

    // Defining this function in vogl_gl_utils.cpp due to its use
    //   of vogl_context_info to avoid dependency errors
    void save_textures(const vogl_context_info *context_info);

    void save_buffers()
    {
        VOGL_FUNC_TRACER

        static const GLenum s_buffer_targets[] =
            {
                GL_ARRAY_BUFFER, GL_ATOMIC_COUNTER_BUFFER, GL_COPY_READ_BUFFER,
                GL_COPY_WRITE_BUFFER, GL_DRAW_INDIRECT_BUFFER, GL_DISPATCH_INDIRECT_BUFFER,
                GL_ELEMENT_ARRAY_BUFFER, GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER,
                GL_SHADER_STORAGE_BUFFER, GL_TEXTURE_BUFFER,
                GL_TRANSFORM_FEEDBACK_BUFFER, GL_UNIFORM_BUFFER
                // TODO: GL_QUERY_BUFFER
            };

        m_bindings.reserve(VOGL_ARRAY_SIZE(s_buffer_targets) + 8);
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_buffer_targets); i++)
            save(s_buffer_targets[i]);
    }

    void restore()
    {
        VOGL_FUNC_TRACER

        for (uint32_t i = 0; i < m_bindings.size(); i++)
            vogl_bind_object(m_bindings[i].m_target, m_bindings[i].m_handle);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_scoped_binding_state
//----------------------------------------------------------------------------------------------------------------------
class vogl_scoped_binding_state
{
    vogl_binding_state m_saved_state;

public:
    vogl_scoped_binding_state()
    {
        VOGL_FUNC_TRACER
    }
    vogl_scoped_binding_state(GLenum target)
    {
        VOGL_FUNC_TRACER m_saved_state.save(target);
    }
    vogl_scoped_binding_state(GLenum target0, GLenum target1)
    {
        VOGL_FUNC_TRACER m_saved_state.save(target0);
        m_saved_state.save(target1);
    }
    vogl_scoped_binding_state(GLenum target0, GLenum target1, GLenum target2)
    {
        VOGL_FUNC_TRACER m_saved_state.save(target0);
        m_saved_state.save(target1);
        m_saved_state.save(target2);
    }
    vogl_scoped_binding_state(GLenum target0, GLenum target1, GLenum target2, GLenum target3)
    {
        VOGL_FUNC_TRACER m_saved_state.save(target0);
        m_saved_state.save(target1);
        m_saved_state.save(target2);
        m_saved_state.save(target3);
    }

    ~vogl_scoped_binding_state()
    {
        VOGL_FUNC_TRACER restore();
    }

    void clear()
    {
        VOGL_FUNC_TRACER m_saved_state.clear();
    }

    void save(GLenum target)
    {
        VOGL_FUNC_TRACER m_saved_state.save(target);
    }

    void save_textures(const vogl_context_info *context_info)
    {
        VOGL_FUNC_TRACER m_saved_state.save_textures(context_info);
    }
    void save_buffers()
    {
        VOGL_FUNC_TRACER m_saved_state.save_buffers();
    }

    void restore()
    {
        VOGL_FUNC_TRACER m_saved_state.restore();
        m_saved_state.clear();
    }
};

//----------------------------------------------------------------------------------------------------------------------
// enum vogl_generic_state_types_t
//----------------------------------------------------------------------------------------------------------------------
enum vogl_generic_state_type
{
    cGSTPixelStore,    // pixel pack, unpack
    cGSTPixelTransfer, // GL_INDEX_OFFSET, RGBA scale/biases, etc.
    cGSTReadBuffer,    // this is a per-framebuffer state!
    cGSTDrawBuffer,    // this is a per-framebuffer state!
    cGSTActiveTexture,
    cGSTClientActiveTexture,
    cGSTMatrixMode,
    cGSTARBVertexProgram,
    cGSTARBFragmentProgram,
    cGSTTotalTypes
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_state_saver
//----------------------------------------------------------------------------------------------------------------------
class vogl_state_saver
{
    struct saved_state
    {
        saved_state()
        {
            VOGL_FUNC_TRACER
        }

        template <typename T>
        saved_state(vogl_generic_state_type state_type, GLenum pname, T data)
            : m_state_type(state_type), m_pname(pname), m_value(data)
        {
            VOGL_FUNC_TRACER
        }

        template <typename T>
        void set(vogl_generic_state_type state_type, GLenum pname, T data)
        {
            VOGL_FUNC_TRACER m_state_type = state_type;
            m_pname = pname;
            m_value = data;
        }

        vogl_generic_state_type m_state_type;

        GLenum m_pname;
        vogl::value m_value;
    };

public:
    vogl_state_saver()
    {
        VOGL_FUNC_TRACER
    }

    void clear()
    {
        VOGL_FUNC_TRACER m_states.clear();
        m_draw_buffers.clear();
    }

    void save(vogl_generic_state_type type, const vogl_context_info *context_info = NULL);
    void restore();

private:
    vogl::growable_array<saved_state, 64> m_states;
    vogl::growable_array<GLenum, 32> m_draw_buffers;
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_scoped_state
//----------------------------------------------------------------------------------------------------------------------
class vogl_scoped_state_saver
{
    vogl_state_saver m_state;

public:
    vogl_scoped_state_saver()
    {
        VOGL_FUNC_TRACER
    }
    vogl_scoped_state_saver(vogl_generic_state_type type)
    {
        VOGL_FUNC_TRACER save(type);
    }
    vogl_scoped_state_saver(vogl_generic_state_type type0, vogl_generic_state_type type1)
    {
        VOGL_FUNC_TRACER save(type0);
        save(type1);
    }
    vogl_scoped_state_saver(vogl_generic_state_type type0, vogl_generic_state_type type1, vogl_generic_state_type type2)
    {
        VOGL_FUNC_TRACER save(type0);
        save(type1);
        save(type2);
    }
    vogl_scoped_state_saver(vogl_generic_state_type type0, vogl_generic_state_type type1, vogl_generic_state_type type2, vogl_generic_state_type type3)
    {
        VOGL_FUNC_TRACER save(type0);
        save(type1);
        save(type2);
        save(type3);
    }

    ~vogl_scoped_state_saver()
    {
        VOGL_FUNC_TRACER restore();
    }

    void clear()
    {
        VOGL_FUNC_TRACER m_state.clear();
    }

    void save(vogl_generic_state_type type)
    {
        VOGL_FUNC_TRACER m_state.save(type);
    }

    void save(const vogl_context_info &context_info, vogl_generic_state_type type)
    {
        VOGL_FUNC_TRACER m_state.save(type, &context_info);
    }
    void restore()
    {
        VOGL_FUNC_TRACER m_state.restore();
        clear();
    }
};

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_serialize_vec
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
inline bool vogl_json_serialize_vec(json_node &node, vogl_blob_manager &blob_manager, const char *pKey, const T &vec)
{
    VOGL_FUNC_TRACER

    json_node &array_node = node.add_array(pKey);
    for (uint32_t i = 0; i < vec.size(); i++)
    {
        json_node &obj_node = array_node.add_object();
        if (!vec[i].serialize(obj_node, blob_manager))
            return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_serialize_ptr_vec
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
inline bool vogl_json_serialize_ptr_vec(json_node &node, vogl_blob_manager &blob_manager, const char *pKey, const T &vec)
{
    VOGL_FUNC_TRACER

    json_node &array_node = node.add_array(pKey);
    for (uint32_t i = 0; i < vec.size(); i++)
    {
        json_node &obj_node = array_node.add_object();
        if (vec[i])
        {
            if (!vec[i]->serialize(obj_node, blob_manager))
                return false;
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_serialize_ptr_vec
//----------------------------------------------------------------------------------------------------------------------
template <typename T, typename U>
inline bool vogl_json_serialize_ptr_vec(json_node &node, vogl_blob_manager &blob_manager, const char *pKey, const T &vec, const U &p0)
{
    VOGL_FUNC_TRACER

    json_node &array_node = node.add_array(pKey);
    for (uint32_t i = 0; i < vec.size(); i++)
    {
        json_node &obj_node = array_node.add_object();
        if (vec[i])
        {
            if (!vec[i]->serialize(obj_node, blob_manager, p0))
                return false;
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_deserialize_vec
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
inline bool vogl_json_deserialize_vec(const json_node &node, const vogl_blob_manager &blob_manager, const char *pKey, T &vec)
{
    VOGL_FUNC_TRACER

    const json_node *pArray_node = node.find_child_array(pKey);
    if (!pArray_node)
    {
        vec.resize(0);
        return false;
    }

    vec.resize(pArray_node->size());

    for (uint32_t i = 0; i < pArray_node->size(); i++)
    {
        const json_node *pObj_node = pArray_node->get_value_as_object(i);
        if (!pObj_node)
            return false;
        if (!vec[i].deserialize(*pObj_node, blob_manager))
            return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_deserialize_ptr_vec
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
inline bool vogl_json_deserialize_ptr_vec(const json_node &node, const vogl_blob_manager &blob_manager, const char *pKey, T &vec)
{
    VOGL_FUNC_TRACER

    const json_node *pArray_node = node.find_child_array(pKey);
    if (!pArray_node)
    {
        vec.resize(0);
        return false;
    }

    vec.resize(0);

    for (uint32_t i = 0; i < pArray_node->size(); i++)
    {
        const json_node *pObj_node = pArray_node->get_value_as_object(i);
        if (!pObj_node)
            return false;

        typename T::value_type pObj = vogl_new(typename Loki::TypeTraits<typename T::value_type>::PointeeType);

        if (!pObj->deserialize(*pObj_node, blob_manager))
        {
            vogl_delete(pObj);
            return false;
        }

        vec.push_back(pObj);
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_deserialize_ptr_vec
//----------------------------------------------------------------------------------------------------------------------
template <typename T, typename U>
inline bool vogl_json_deserialize_ptr_vec(const json_node &node, const vogl_blob_manager &blob_manager, const char *pKey, T &vec, const U &p0)
{
    VOGL_FUNC_TRACER

    const json_node *pArray_node = node.find_child_array(pKey);
    if (!pArray_node)
    {
        vec.resize(0);
        return false;
    }

    vec.resize(0);

    for (uint32_t i = 0; i < pArray_node->size(); i++)
    {
        const json_node *pObj_node = pArray_node->get_value_as_object(i);
        if (!pObj_node)
            return false;

        typename T::value_type pObj = vogl_new(typename Loki::TypeTraits<typename T::value_type>::PointeeType);

        if (!pObj->deserialize(*pObj_node, blob_manager, p0))
        {
            vogl_delete(pObj);
            return false;
        }

        vec.push_back(pObj);
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_deserialize_obj
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
bool vogl_json_deserialize_obj(const json_node &node, const vogl_blob_manager &blob_manager, const char *pKey, T &obj)
{
    VOGL_FUNC_TRACER

    const json_node *pNode = node.find_child_object(pKey);
    if (!pNode)
        return false;
    return obj.deserialize(*pNode, blob_manager);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_deserialize_array
//----------------------------------------------------------------------------------------------------------------------
template <typename T>
bool vogl_json_deserialize_array(const json_node &node, const vogl_blob_manager &blob_manager, const char *pKey, T &obj)
{
    VOGL_FUNC_TRACER

    const json_node *pNode = node.find_child_array(pKey);
    if (!pNode)
        return false;
    return obj.deserialize(*pNode, blob_manager);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_serialize_vec4F
// TODO: Add roundtrip checking, and/or move this to common code somewhere
//----------------------------------------------------------------------------------------------------------------------
inline bool vogl_json_serialize_vec4F(json_node &dst_node, const vec4F &vec)
{
    VOGL_FUNC_TRACER

    dst_node.init_array();
    for (uint32_t j = 0; j < 4; j++)
        dst_node.add_value(vec[j]);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_json_deserialize_vec4
// TODO: Add roundtrip checking, and/or move this to common code somewhere
//----------------------------------------------------------------------------------------------------------------------
inline bool vogl_json_deserialize_vec4F(const json_node &src_node, vec4F &vec)
{
    VOGL_FUNC_TRACER

    if ((!src_node.is_array()) || (src_node.size() != 4))
        return false;

    for (uint32_t j = 0; j < 4; j++)
    {
        if (src_node[j].is_object_or_array())
            return false;
        vec[j] = src_node[j].as_float();
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_format_debug_output_arb
//----------------------------------------------------------------------------------------------------------------------
void vogl_format_debug_output_arb(char out_str[], size_t out_str_size, GLenum source, GLenum type, GLuint id, GLenum severity, const char *msg);
void GLAPIENTRY vogl_generic_arb_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param);
void vogl_enable_generic_context_debug_messages();

//----------------------------------------------------------------------------------------------------------------------
// Context helpers
//----------------------------------------------------------------------------------------------------------------------
#if (VOGL_PLATFORM_HAS_GLX)
typedef GLXContext vogl_gl_context;
typedef Display *vogl_gl_display;
typedef GLXDrawable vogl_gl_drawable;
typedef GLXFBConfig vogl_gl_fb_config;
#elif(VOGL_PLATFORM_HAS_WGL)
typedef HGLRC vogl_gl_context;
typedef HDC vogl_gl_display;
typedef HWND vogl_gl_drawable;
typedef int vogl_gl_fb_config;
#else
// TODO
typedef void *vogl_gl_context;
typedef void *vogl_gl_display;
typedef int vogl_gl_drawable;
typedef int vogl_gl_fb_config;
#endif

enum
{
    eCHCDebugContextFlag = 1,
    cCHCCoreProfileFlag = 2,
    cCHCCompatProfileFlag = 4,
};

vogl_gl_context vogl_create_context(vogl_gl_display display, vogl_gl_fb_config fb_config, vogl_gl_context share_context, uint32_t major_ver, uint32_t minor_ver, uint32_t flags, vogl_context_desc *pDesc = NULL);
vogl_gl_context vogl_get_current_context();
vogl_gl_display vogl_get_current_display();
vogl_gl_drawable vogl_get_current_drawable();

// vogl_get_current_fb_config() assumes a context is current.
vogl_gl_fb_config vogl_get_current_fb_config(uint32_t screen = 0);

void vogl_make_current(vogl_gl_display display, vogl_gl_drawable drawable, vogl_gl_context context);

void vogl_destroy_context(vogl_gl_display display, vogl_gl_context context);

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_max_supported_mip_levels
//----------------------------------------------------------------------------------------------------------------------
int vogl_get_max_supported_mip_levels();

//----------------------------------------------------------------------------------------------------------------------
// pretty_print_param_val
// entrypoint_id and param_index are to assist the GL enum to string conversion only, set param_index to -1 == return param.
// Set entrypoint_id to VOGL_ENTRYPOINT_INVALID if the value is not a call parameter.
//----------------------------------------------------------------------------------------------------------------------
bool pretty_print_param_val(dynamic_string &str, const vogl_ctype_desc_t &ctype_desc, uint64_t val_data, gl_entrypoint_id_t entrypoint_id, int param_index);

#endif // VOGL_GL_UTILS_H

