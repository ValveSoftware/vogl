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

// File: vogl_gl_utils.cpp
#include "vogl_common.h"
#include <stdarg.h>
#include "vogl_console.h"
#include "vogl_json.h"
#include "vogl_image.h"
#include "vogl_context_info.h"
#include "vogl_backtrace.h"

#define VOGL_DECLARE_PNAME_DEF_TABLE
#include "gl_pname_defs.h"

#define VOGL_NAMESPACES_IMPLEMENTATION
#include "vogl_namespaces.h"
#undef VOGL_NAMESPACES_IMPLEMENTATION

//----------------------------------------------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------------------------------------------
bool g_gl_get_error_enabled = true;

gl_enums &get_gl_enums()
{
    static gl_enums s_gl_enums;
    return s_gl_enums;
}

//----------------------------------------------------------------------------------------------------------------------
// client side array tables
//----------------------------------------------------------------------------------------------------------------------
const vogl_client_side_array_desc_t g_vogl_client_side_array_descs[] =
    {
#define VOGL_CLIENT_SIDE_ARRAY_DESC(id, entrypoint, is_enabled, get_binding, get_pointer, get_size, get_stride, get_type) \
    {                                                                                                                     \
        entrypoint, is_enabled, get_binding, get_pointer, get_size, get_stride, get_type                                  \
    }                                                                                                                     \
    ,
#include "vogl_client_side_array_descs.inc"
#undef VOGL_CLIENT_SIDE_ARRAY_DESC
    };

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_integer
//----------------------------------------------------------------------------------------------------------------------
GLint vogl_get_gl_integer(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLint values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetIntegerv)(pname, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_vec4I
//----------------------------------------------------------------------------------------------------------------------
vec4I vogl_get_gl_vec4I(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    vec4I result(0);
    GL_ENTRYPOINT(glGetIntegerv)(pname, &result[0]);
    return result;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_integer_indexed
//----------------------------------------------------------------------------------------------------------------------
GLint vogl_get_gl_integer_indexed(GLenum pname, uint32_t index)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLint values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetIntegeri_v)(pname, index, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_integer64
//----------------------------------------------------------------------------------------------------------------------
GLint64 vogl_get_gl_integer64(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLint64 values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetInteger64v)(pname, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_integer64_indexed
//----------------------------------------------------------------------------------------------------------------------
GLint64 vogl_get_gl_integer64_indexed(GLenum pname, uint32_t index)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLint64 values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetInteger64i_v)(pname, index, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_boolean
//----------------------------------------------------------------------------------------------------------------------
GLboolean vogl_get_gl_boolean(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLboolean values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetBooleanv)(pname, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_boolean_indexed
//----------------------------------------------------------------------------------------------------------------------
GLboolean vogl_get_gl_boolean_indexed(GLenum pname, uint32_t index)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLboolean values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetBooleani_v)(pname, index, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_float
//----------------------------------------------------------------------------------------------------------------------
GLfloat vogl_get_gl_float(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLfloat values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetFloatv)(pname, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_vec4F
//----------------------------------------------------------------------------------------------------------------------
vec4F vogl_get_gl_vec4F(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    vec4F values(0);
    GL_ENTRYPOINT(glGetFloatv)(pname, &values[0]);
    return values;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_matrix44F
//----------------------------------------------------------------------------------------------------------------------
matrix44F vogl_get_gl_matrix44F(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 16);

    matrix44F values(cClear);
    GL_ENTRYPOINT(glGetFloatv)(pname, &values[0][0]);
    return values;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_double
//----------------------------------------------------------------------------------------------------------------------
GLdouble vogl_get_gl_double(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    GLdouble values[4] = { 0, 0, 0, 0 };
    GL_ENTRYPOINT(glGetDoublev)(pname, values);
    return values[0];
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_vec4D
//----------------------------------------------------------------------------------------------------------------------
vec4D vogl_get_gl_vec4D(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 4);

    vec4D values(0);
    GL_ENTRYPOINT(glGetDoublev)(pname, &values[0]);
    return values;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_matrix44D
//----------------------------------------------------------------------------------------------------------------------
matrix44D vogl_get_gl_matrix44D(GLenum pname)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_gl_enums().get_pname_count(pname) <= 16);

    matrix44D values(cClear);
    GL_ENTRYPOINT(glGetDoublev)(pname, &values[0][0]);
    return values;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_image_format_size_in_bytes
//----------------------------------------------------------------------------------------------------------------------
uint32_t vogl_get_image_format_size_in_bytes(GLenum format, GLenum type)
{
    VOGL_FUNC_TRACER

    uint32_t num_channels, bits_per_element, bits_per_pixel;
    vogl_get_image_format_info(format, type, num_channels, bits_per_element, bits_per_pixel);
    return (bits_per_pixel + 7) >> 3;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_has_active_context
//----------------------------------------------------------------------------------------------------------------------
bool vogl_has_active_context()
{
    VOGL_FUNC_TRACER

#if (VOGL_PLATFORM_HAS_GLX)
    if (!GL_ENTRYPOINT(glXGetCurrentContext))
        return false;
    if (!GL_ENTRYPOINT(glXGetCurrentContext)())
        return false;

#elif(VOGL_PLATFORM_HAS_CGL)
    VOGL_ASSERT(!"UNIMPLEMENTED vogl_has_active_context");
    return false;

#elif(VOGL_PLATFORM_HAS_WGL)

    if (!GL_ENTRYPOINT(wglGetCurrentContext))
        return false;
    if (!GL_ENTRYPOINT(wglGetCurrentContext)())
        return false;
#else
#error "Implement vogl_has_active_context this platform."
#endif
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_tex_target_image_size
//----------------------------------------------------------------------------------------------------------------------
size_t vogl_get_tex_target_image_size(GLenum target, GLint level, GLenum format, GLenum type)
{
    VOGL_FUNC_TRACER

    GLint width = 0, height = 0, depth = 0;

    if (!vogl_has_active_context())
    {
        vogl_error_printf("vogl_get_tex_target_image_size() called without an active context!\n");
        return 0;
    }

    if (!GL_ENTRYPOINT(glGetTexLevelParameteriv))
    {
        vogl_error_printf("vogl_get_tex_target_image_size() called but the glGetTexLevelParameteriv function ptr is NULL!\n");
        return 0;
    }

    if (g_vogl_actual_gl_entrypoints.m_glGetTexLevelParameteriv)
    {
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_WIDTH, &width);
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_HEIGHT, &height);
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_DEPTH, &depth);
    }

    if ((!width) || (!height) || (!depth))
        return 0;

    return vogl_get_image_size(format, type, width, height, depth);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_enum_desc::init_sort_priority
//----------------------------------------------------------------------------------------------------------------------
void vogl_enum_desc::init_sort_priority()
{
    VOGL_FUNC_TRACER

    m_sort_priority = cSCNone;

    if (m_macro_name.ends_with("_EXT"))
        m_sort_priority = cSCEXT;
    else if (m_macro_name.ends_with("_ARB"))
        m_sort_priority = cSCARB;
    else if (m_macro_name.ends_with("_OES"))
        m_sort_priority = cSCOES;
    else
    {
        static const char *s_vendor_suffixes[] = { "_NV", "_AMD", "_INTEL", "_QCOM", "_ATI", "_SGIS", "_SGIX", "_ANGLE", "_APPLE", "_MESA", "_IBM" };
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_vendor_suffixes); i++)
        {
            if (m_macro_name.ends_with(s_vendor_suffixes[i]))
            {
                m_sort_priority = cSCVendor;
                break;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_enum_desc::operator <
//----------------------------------------------------------------------------------------------------------------------
bool vogl_enum_desc::operator<(const vogl_enum_desc &rhs) const
{
    VOGL_FUNC_TRACER

    if (m_sort_priority < rhs.m_sort_priority)
        return true;
    else if (m_sort_priority == rhs.m_sort_priority)
    {
        int comp_result = m_prefix.compare_using_length(rhs.m_prefix, true);
        if (comp_result < 0)
            return true;
        else if (comp_result == 0)
            return m_macro_name.compare_using_length(rhs.m_macro_name, true) < 0;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::gl_enums
//----------------------------------------------------------------------------------------------------------------------
gl_enums::gl_enums()
{
    VOGL_FUNC_TRACER

    memset(m_gl_enum_to_pname_def_index, 0xFF, sizeof(m_gl_enum_to_pname_def_index));

    for (uint32_t i = 0; i < GL_PNAME_DEFS_ARRAY_SIZE; i++)
    {
        if (g_gl_pname_defs[i].m_gl_enum < 0x10000)
        {
            if (m_gl_enum_to_pname_def_index[g_gl_pname_defs[i].m_gl_enum] != 0xFFFF)
            {
                vogl_debug_printf("duplicate GL enum in g_gl_pname_defs table: 0x%04X %s\n", i, g_gl_pname_defs[i].m_pName);
                continue;
            }

            m_gl_enum_to_pname_def_index[g_gl_pname_defs[i].m_gl_enum] = i;

            m_enum_name_hash_map.insert(g_gl_pname_defs[i].m_pName, g_gl_pname_defs[i].m_gl_enum);
        }
    }

    init_enum_descs();
    init_image_formats();
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::add_enum
//----------------------------------------------------------------------------------------------------------------------
void gl_enums::add_enum(const char *pPrefix, const char *pSpec_type, const char *pGL_type, const char *pName, uint64_t value)
{
    VOGL_FUNC_TRACER

    {
        gl_enum_value_to_enum_desc_hash_map::insert_result ins_result(m_enum_value_hash_map.insert(value, vogl_enum_desc_vec()));
        vogl_enum_desc_vec &vec = ins_result.first->second;
        vogl_enum_desc *pDesc = vec.enlarge(1);
        pDesc->m_value = value;
        pDesc->m_prefix = pPrefix;
        pDesc->m_spec_type = pSpec_type;
        pDesc->m_gl_type = pGL_type;
        pDesc->m_macro_name = pName;
        pDesc->init_sort_priority();
    }

    {
        gl_spec_type_and_value_to_enum_desc_hash_map_t::insert_result ins_result(m_spec_type_and_enum_value_hash_map.insert(vogl_spec_type_and_value_enum_key(pSpec_type, value), vogl_enum_desc_vec()));
        vogl_enum_desc_vec &vec = ins_result.first->second;

        vogl_enum_desc *pDesc = vec.enlarge(1);
        pDesc->m_value = value;
        pDesc->m_prefix = pPrefix;
        pDesc->m_spec_type = pSpec_type;
        pDesc->m_gl_type = pGL_type;
        pDesc->m_macro_name = pName;
        pDesc->init_sort_priority();
    }

    {
        gl_enum_name_hash_map::insert_result res(m_enum_name_hash_map.insert(pName, value));

        // Check for duplicate definitions with different values (compare apitrace's pname table vs. the spec)
        uint64_t value_in_map = res.first->second;
        VOGL_ASSERT(value_in_map == value);
        VOGL_NOTE_UNUSED(value_in_map);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::init_enum_descs
//----------------------------------------------------------------------------------------------------------------------
void gl_enums::init_enum_descs()
{
    VOGL_FUNC_TRACER

#define DEFINE_GL_ENUM_CATEGORY_BEGIN(spectype)
#define DEFINE_GL_DEFINE_MEMBER(prefix, spec_type, enum_name)
#define DEFINE_GL_ENUM_MEMBER(prefix, spec_type, gl_type, enum_name, value) add_enum(#prefix, #spec_type, gl_type, enum_name, value);
#define DEFINE_GL_ENUM_CATEGORY_END(x)

#include "gl_enum_desc.inc"
#include "glx_enum_desc.inc"

#undef DEFINE_GL_ENUM_CATEGORY_BEGIN
#undef DEFINE_GL_DEFINE_MEMBER
#undef DEFINE_GL_ENUM_MEMBER
#undef DEFINE_GL_ENUM_CATEGORY_END

    for (gl_enum_value_to_enum_desc_hash_map::iterator it = m_enum_value_hash_map.begin(); it != m_enum_value_hash_map.end(); ++it)
    {
        vogl_enum_desc_vec &vec = it->second;
        vec.sort();
    }

    for (gl_spec_type_and_value_to_enum_desc_hash_map_t::iterator it = m_spec_type_and_enum_value_hash_map.begin(); it != m_spec_type_and_enum_value_hash_map.end(); ++it)
    {
        vogl_enum_desc_vec &vec = it->second;
        vec.sort();
    }
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::get_name
//----------------------------------------------------------------------------------------------------------------------
const char *gl_enums::find_name(uint64_t gl_enum, const char *pPreferred_prefix) const
{
    VOGL_FUNC_TRACER

    gl_enum_value_to_enum_desc_hash_map::const_iterator it(m_enum_value_hash_map.find(gl_enum));
    if (it != m_enum_value_hash_map.end())
    {
        const vogl_enum_desc_vec &desc_vec = it->second;

        if (pPreferred_prefix)
        {
            for (uint32_t i = 0; i < desc_vec.size(); i++)
                if (!desc_vec[i].m_prefix.compare(pPreferred_prefix, false)) // purposely not case sensitive
                    return desc_vec[i].m_macro_name.get_ptr();
        }

        return desc_vec[0].m_macro_name.get_ptr();
    }

    // Try falling back to the pname table - some of the extension enums are not in the .spec files but are in apitrace's pname table.
    int pname_index = find_pname_def_index(gl_enum);
    if ((pname_index >= 0) && (g_gl_pname_defs[pname_index].m_pName))
        return g_gl_pname_defs[pname_index].m_pName;

    if (g_command_line_params().get_value_as_bool("debug"))
    {
        vogl_debug_printf("Failed finding GL enum 0x%08" PRIX64 ", API prefix \"%s\"\n", gl_enum, pPreferred_prefix ? pPreferred_prefix : "");
    }

    return NULL;
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::find_gl_image_format_name
//----------------------------------------------------------------------------------------------------------------------
const char *gl_enums::find_gl_image_format_name(GLenum gl_enum) const
{
    VOGL_FUNC_TRACER

    const char *const *ppName = m_image_formats.find_value(gl_enum);
    if (ppName)
        return *ppName;
    return find_gl_name(gl_enum);
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::find_name
//----------------------------------------------------------------------------------------------------------------------
const char *gl_enums::find_name(const char *pSpec_type, uint64_t gl_enum, const char *pPreferred_prefix) const
{
    VOGL_FUNC_TRACER

    // The spec type search is fuzzy but it does guide the search to saner enums in many common cases.
    // The spec types in the enum table don't always match up with the spec types in the GL API param desc's.
    // This isn't critical but it would be nice to try resolving this crap.
    if (pSpec_type)
    {
        gl_spec_type_and_value_to_enum_desc_hash_map_t::const_iterator it(m_spec_type_and_enum_value_hash_map.find(vogl_spec_type_and_value_enum_key(pSpec_type, gl_enum)));
        if (it != m_spec_type_and_enum_value_hash_map.end())
        {
            const vogl_enum_desc_vec &desc_vec = it->second;

            if (pPreferred_prefix)
            {
                for (uint32_t i = 0; i < desc_vec.size(); i++)
                    if (!desc_vec[i].m_prefix.compare(pPreferred_prefix, false)) // purposely not case sensitive
                        return desc_vec[i].m_macro_name.get_ptr();
            }

            return desc_vec[0].m_macro_name.get_ptr();
        }
        else
        {
            if (g_command_line_params().get_value_as_bool("debug"))
            {
                vogl_debug_printf("Failed finding GL enum with spec type %s, enum 0x%08" PRIX64 ", API prefix \"%s\", falling back to non-spec type search\n",
                                  pSpec_type, gl_enum, pPreferred_prefix ? pPreferred_prefix : "");
            }
        }
    }

    return find_name(gl_enum, pPreferred_prefix);
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::find_enum
//----------------------------------------------------------------------------------------------------------------------
const char *gl_enums::find_name(uint64_t gl_enum, gl_entrypoint_id_t entrypoint_id, int param_index) const
{
    VOGL_ASSERT(entrypoint_id < VOGL_NUM_ENTRYPOINTS);

    const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];

    const char *pSpec_type = NULL;
    if (param_index < 0)
    {
        pSpec_type = entrypoint_desc.m_pReturn_spec_type;
    }
    else
    {
        VOGL_ASSERT(param_index < static_cast<int>(entrypoint_desc.m_num_params));

        pSpec_type = g_vogl_entrypoint_param_descs[entrypoint_id][param_index].m_pSpec_type;
    }

    return find_name(pSpec_type, gl_enum, entrypoint_desc.m_pAPI_prefix);
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::find_enum
//----------------------------------------------------------------------------------------------------------------------
uint64_t gl_enums::find_enum(const dynamic_string &str) const
{
    VOGL_FUNC_TRACER

    gl_enum_name_hash_map::const_iterator it(m_enum_name_hash_map.find(str));
    return (it == m_enum_name_hash_map.end()) ? static_cast<uint64_t>(cUnknownEnum) : it->second;
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::get_pname_count
//----------------------------------------------------------------------------------------------------------------------
int gl_enums::get_pname_count(uint64_t gl_enum) const
{
    VOGL_FUNC_TRACER

    if (GL_ENTRYPOINT(glGetIntegerv))
    {
        if (gl_enum == GL_COMPRESSED_TEXTURE_FORMATS)
        {
            GLint value;
            GL_ENTRYPOINT(glGetIntegerv)(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &value);
            return value;
        }
        else if (gl_enum == GL_PROGRAM_BINARY_FORMATS)
        {
            GLint value;
            GL_ENTRYPOINT(glGetIntegerv)(GL_NUM_PROGRAM_BINARY_FORMATS, &value);
            return value;
        }
    }

    int pname_index = find_pname_def_index(gl_enum);
    if (pname_index >= 0)
        return g_gl_pname_defs[pname_index].m_count;

    vogl_warning_printf("Unknown GL enum: 0x%08" PRIX64 "\n", gl_enum);
    return -1;
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::get_pname_type
//----------------------------------------------------------------------------------------------------------------------
int gl_enums::get_pname_type(uint64_t gl_enum) const
{
    VOGL_FUNC_TRACER
    int pname_index = find_pname_def_index(gl_enum);
    if (pname_index >= 0)
        return g_gl_pname_defs[pname_index].m_type;

    vogl_warning_printf("Unknown GL enum: 0x%08" PRIX64 "\n", gl_enum);
    return cSTInt32;
}

//----------------------------------------------------------------------------------------------------------------------
// gl_enums::find_pname_def_index
//----------------------------------------------------------------------------------------------------------------------
int gl_enums::find_pname_def_index(uint64_t gl_enum) const
{
    VOGL_FUNC_TRACER

    if (gl_enum < 0x10000)
    {
        int pname_index = m_gl_enum_to_pname_def_index[static_cast<uint32_t>(gl_enum)];
        if (pname_index != 0xFFFF)
            return pname_index;
    }
    return vogl::cInvalidIndex;
}

//----------------------------------------------------------------------------------------------------------------------
// GL image format to enum helpers
//----------------------------------------------------------------------------------------------------------------------
void gl_enums::init_image_formats()
{
    VOGL_FUNC_TRACER

#define IMG_FMT(x) m_image_formats.insert(x, #x);
#include "vogl_image_formats.inc"
#undef IMG_FMT
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_bound_gl_buffer
//----------------------------------------------------------------------------------------------------------------------
GLuint vogl_get_bound_gl_buffer(GLenum target)
{
    VOGL_FUNC_TRACER

    GLenum t = 0;
    switch (target)
    {
        case GL_ARRAY_BUFFER:
            t = GL_ARRAY_BUFFER_BINDING;
            break;
        case GL_ATOMIC_COUNTER_BUFFER:
            t = GL_ATOMIC_COUNTER_BUFFER_BINDING;
            break;
        case GL_COPY_READ_BUFFER:
            t = GL_COPY_READ_BUFFER_BINDING;
            break;
        case GL_COPY_WRITE_BUFFER:
            t = GL_COPY_WRITE_BUFFER_BINDING;
            break;
        case GL_DRAW_INDIRECT_BUFFER:
            t = GL_DRAW_INDIRECT_BUFFER_BINDING;
            break;
        case GL_DISPATCH_INDIRECT_BUFFER:
            t = GL_DISPATCH_INDIRECT_BUFFER_BINDING;
            break;
        case GL_ELEMENT_ARRAY_BUFFER:
            t = GL_ELEMENT_ARRAY_BUFFER_BINDING;
            break;
        case GL_PIXEL_PACK_BUFFER:
            t = GL_PIXEL_PACK_BUFFER_BINDING;
            break;
        case GL_PIXEL_UNPACK_BUFFER:
            t = GL_PIXEL_UNPACK_BUFFER_BINDING;
            break;
        case GL_SHADER_STORAGE_BUFFER:
            t = GL_SHADER_STORAGE_BUFFER_BINDING;
            break;
        case GL_TRANSFORM_FEEDBACK_BUFFER:
            t = GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
            break;
        case GL_UNIFORM_BUFFER:
            t = GL_UNIFORM_BUFFER_BINDING;
            break;
        case GL_TEXTURE_BUFFER:
            // TODO:  Change to GL_TEXTURE_BUFFER_BINDING once it's in 4.4
            // http://www.khronos.org/bugzilla/show_bug.cgi?id=844
            t = GL_TEXTURE_BUFFER;
            break;
        default:
        {
            vogl_warning_printf("Unknown buffer GL enum 0x%08X\n", target);
            return 0;
        }
    }
    GLint id = 0;
    GL_ENTRYPOINT(glGetIntegerv)(t, &id);
    return id;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_reset_pixel_store_states
//----------------------------------------------------------------------------------------------------------------------
void vogl_reset_pixel_store_states()
{
    VOGL_FUNC_TRACER

    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_SWAP_BYTES, false);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_LSB_FIRST, false);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_ROW_LENGTH, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_IMAGE_HEIGHT, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_SKIP_ROWS, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_SKIP_PIXELS, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_SKIP_IMAGES, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_PACK_ALIGNMENT, 1);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_SWAP_BYTES, false);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_LSB_FIRST, false);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_ROW_LENGTH, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_IMAGE_HEIGHT, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_SKIP_ROWS, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_SKIP_PIXELS, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_SKIP_IMAGES, 0);
    GL_ENTRYPOINT(glPixelStorei)(GL_UNPACK_ALIGNMENT, 1);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_reset_pixel_transfer_states
//----------------------------------------------------------------------------------------------------------------------
void vogl_reset_pixel_transfer_states(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    GL_ENTRYPOINT(glPixelTransferi)(GL_MAP_COLOR, GL_FALSE);

    GL_ENTRYPOINT(glPixelTransferi)(GL_MAP_STENCIL, GL_FALSE);
    GL_ENTRYPOINT(glPixelTransferi)(GL_INDEX_SHIFT, 0);
    GL_ENTRYPOINT(glPixelTransferi)(GL_INDEX_OFFSET, 0);

    GL_ENTRYPOINT(glPixelTransferf)(GL_RED_SCALE, 1.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_GREEN_SCALE, 1.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_BLUE_SCALE, 1.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_ALPHA_SCALE, 1.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_DEPTH_SCALE, 1.0f);

    GL_ENTRYPOINT(glPixelTransferf)(GL_RED_BIAS, 0.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_GREEN_BIAS, 0.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_BLUE_BIAS, 0.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_ALPHA_BIAS, 0.0f);
    GL_ENTRYPOINT(glPixelTransferf)(GL_DEPTH_BIAS, 0.0f);
    if (context_info.supports_extension("GL_ARB_imaging"))
    {
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_RED_SCALE, 1.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_GREEN_SCALE, 1.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_BLUE_SCALE, 1.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_ALPHA_SCALE, 1.0f);

        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_RED_BIAS, 0.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_GREEN_BIAS, 0.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_BLUE_BIAS, 0.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_CONVOLUTION_ALPHA_BIAS, 0.0f);

        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_RED_SCALE, 1.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_GREEN_SCALE, 1.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_BLUE_SCALE, 1.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_ALPHA_SCALE, 1.0f);

        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_RED_BIAS, 0.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_GREEN_BIAS, 0.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_BLUE_BIAS, 0.0f);
        GL_ENTRYPOINT(glPixelTransferf)(GL_POST_COLOR_MATRIX_ALPHA_BIAS, 0.0f);
    }
}

#include "vogl_general_context_state.h"

//----------------------------------------------------------------------------------------------------------------------
// vogl_copy_buffer_to_image
//----------------------------------------------------------------------------------------------------------------------
bool vogl_copy_buffer_to_image(void *pDst, uint32_t dst_size, uint32_t width, uint32_t height, GLuint format, GLuint type, bool flip_image, GLuint framebuffer, GLuint read_buffer, GLuint pixel_pack_buffer)
{
    VOGL_FUNC_TRACER

    if ((!width) || (!height))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    // Forceably discard any errors up to here, even in benchmark mode, because glReadPixels() is going to be slow as hell anyway
    GL_ENTRYPOINT(glGetError)();

    // Save the state we'll be changing
    vogl_scoped_state_saver state_saver(cGSTPixelStore);
    vogl_scoped_binding_state orig_framebuffers(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER, GL_PIXEL_PACK_BUFFER);

    // Set the state we need to do a glReadPixels()
    vogl_reset_pixel_store_states();

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_PACK_BUFFER, pixel_pack_buffer);
    GL_ENTRYPOINT(glBindFramebuffer)(GL_FRAMEBUFFER, framebuffer);

    vogl_scoped_state_saver framebuffer_state_saver(cGSTReadBuffer, cGSTDrawBuffer);

    GL_ENTRYPOINT(glReadBuffer)(read_buffer);

    VOGL_CHECK_GL_ERROR;

    // Now read the pixels
    GLenum err = GL_INVALID_VALUE;

    // Ensure the destination buffer is big enough
    size_t image_size = vogl_get_image_size(format, type, width, height, 1);
    if ((pDst) && ((!image_size) || (dst_size < image_size)))
    {
        VOGL_ASSERT_ALWAYS;
    }
    else
    {
        GL_ENTRYPOINT(glReadPixels)(0, 0, width, height, format, type, pDst);

        err = GL_ENTRYPOINT(glGetError());
    }

    if (err)
    {
        vogl_warning_printf("GL error 0x%X while calling glReadPixels()!\n", err);
    }

    // Optionally flip the image
    if ((flip_image) && (err == GL_NO_ERROR) && (pDst))
    {
        size_t pitch = vogl_get_image_size(format, type, width, 1, 1);
        if (pitch > cUINT32_MAX)
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        if ((pitch) && ((image_size / pitch) == height))
        {
            vogl::vector<uint8_t> row_buf(static_cast<uint32_t>(pitch));

            for (uint32_t y = 0; y < (height / 2); y++)
            {
                uint8_t *pA = reinterpret_cast<uint8_t *>(pDst) + y * pitch;
                uint8_t *pB = reinterpret_cast<uint8_t *>(pDst) + (height - 1 - y) * pitch;
                memcpy(row_buf.get_ptr(), pA, pitch);
                memcpy(pA, pB, pitch);
                memcpy(pB, row_buf.get_ptr(), pitch);
            }
        }
        else
        {
            // Don't know how to flip it
            VOGL_ASSERT_ALWAYS;
        }
    }

    // Restore previous state - order is critical here!
    framebuffer_state_saver.restore();
    orig_framebuffers.restore();
    state_saver.restore();

    VOGL_CHECK_GL_ERROR;

    return (err == GL_NO_ERROR);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_check_gl_error_internal
// Returns *true* on error
//----------------------------------------------------------------------------------------------------------------------
bool vogl_check_gl_error_internal(bool suppress_error_message, const char *pFile, uint32_t line, const char *pFunc)
{
    VOGL_FUNC_TRACER

    bool status = false;
    for (;;)
    {
        // http://www.opengl.org/sdk/docs/man/html/glGetError.xhtml
        // "Thus, glGetError should always be called in a loop, until it returns GL_NO_ERROR, if all error flags are to be reset."
        GLenum gl_err = GL_ENTRYPOINT(glGetError)();
        if (gl_err == GL_NO_ERROR)
            break;

        if (!suppress_error_message)
        {
            vogl_error_printf("GL error: 0x%08X (%u): %s (Called From File: %s, line: %u, func: %s)\n", gl_err, gl_err, get_gl_enums().find_name("ErrorCode", gl_err), pFile ? pFile : "?", line, pFunc ? pFunc : "?");

#if 0
			dynamic_string_array backtrace;
			if (get_printable_backtrace(backtrace))
			{
				vogl_error_printf("Backtrace:\n");
				for (uint32_t i = 0; i < backtrace.size(); i++)
					vogl_error_printf("%s\n", backtrace[i].get_ptr());
			}
#endif
        }
        status = true;
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_enable_gl_get_errors
//----------------------------------------------------------------------------------------------------------------------
void vogl_enable_gl_get_error()
{
    g_gl_get_error_enabled = true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_disable_gl_get_errors
//----------------------------------------------------------------------------------------------------------------------
void vogl_disable_gl_get_error()
{
    g_gl_get_error_enabled = false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_gl_get_error_enabled
//----------------------------------------------------------------------------------------------------------------------
bool vogl_is_gl_get_error_enabled()
{
    return g_gl_get_error_enabled;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_draw_entrypoint
//----------------------------------------------------------------------------------------------------------------------
bool vogl_is_draw_entrypoint(gl_entrypoint_id_t id)
{
    VOGL_FUNC_TRACER

    switch (id)
    {
        case VOGL_ENTRYPOINT_glDrawRangeElements:
        case VOGL_ENTRYPOINT_glDrawRangeElementsBaseVertex:
        case VOGL_ENTRYPOINT_glDrawElements:
        case VOGL_ENTRYPOINT_glDrawArrays:
        case VOGL_ENTRYPOINT_glDrawArraysEXT:
        case VOGL_ENTRYPOINT_glDrawElementsBaseVertex:
        case VOGL_ENTRYPOINT_glDrawElementsInstanced:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedARB:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedBaseVertex:
        case VOGL_ENTRYPOINT_glDrawArraysIndirect:
        case VOGL_ENTRYPOINT_glDrawArraysInstanced:
        case VOGL_ENTRYPOINT_glDrawArraysInstancedARB:
        case VOGL_ENTRYPOINT_glDrawArraysInstancedBaseInstance:
        case VOGL_ENTRYPOINT_glDrawArraysInstancedEXT:
        case VOGL_ENTRYPOINT_glMultiDrawArrays:
        case VOGL_ENTRYPOINT_glMultiDrawArraysEXT:
        case VOGL_ENTRYPOINT_glMultiDrawElements:
        case VOGL_ENTRYPOINT_glMultiDrawElementsEXT:
        case VOGL_ENTRYPOINT_glMultiDrawElementsBaseVertex:
        case VOGL_ENTRYPOINT_glDrawElementArrayAPPLE:
        case VOGL_ENTRYPOINT_glDrawElementArrayATI:
        case VOGL_ENTRYPOINT_glDrawElementsIndirect:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedBaseInstance:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedBaseVertexBaseInstance:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedEXT:
        case VOGL_ENTRYPOINT_glDrawRangeElementArrayAPPLE:
        case VOGL_ENTRYPOINT_glDrawRangeElementArrayATI:
        case VOGL_ENTRYPOINT_glDrawRangeElementsEXT:
        case VOGL_ENTRYPOINT_glDrawTransformFeedback:
        case VOGL_ENTRYPOINT_glDrawTransformFeedbackInstanced:
        case VOGL_ENTRYPOINT_glDrawTransformFeedbackNV:
        case VOGL_ENTRYPOINT_glDrawTransformFeedbackStream:
        case VOGL_ENTRYPOINT_glDrawTransformFeedbackStreamInstanced:
        case VOGL_ENTRYPOINT_glMultiDrawArraysIndirect:
        case VOGL_ENTRYPOINT_glMultiDrawArraysIndirectAMD:
        case VOGL_ENTRYPOINT_glMultiDrawElementArrayAPPLE:
        case VOGL_ENTRYPOINT_glMultiDrawElementsIndirect:
        case VOGL_ENTRYPOINT_glMultiDrawElementsIndirectAMD:
        case VOGL_ENTRYPOINT_glMultiDrawRangeElementArrayAPPLE:
            return true;
        default:
            break;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_clear_entrypoint
//----------------------------------------------------------------------------------------------------------------------
bool vogl_is_clear_entrypoint(gl_entrypoint_id_t id)
{
    VOGL_FUNC_TRACER

    switch (id)
    {
        case VOGL_ENTRYPOINT_glClear:
        case VOGL_ENTRYPOINT_glClearBufferiv:
        case VOGL_ENTRYPOINT_glClearBufferfv:
        case VOGL_ENTRYPOINT_glClearBufferuiv:
        case VOGL_ENTRYPOINT_glClearBufferfi:
            return true;
        default:
            break;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_frame_buffer_write_entrypoint
//----------------------------------------------------------------------------------------------------------------------
bool vogl_is_frame_buffer_write_entrypoint(gl_entrypoint_id_t id)
{
    VOGL_FUNC_TRACER

    switch (id)
    {
        case VOGL_ENTRYPOINT_glEnd:
        case VOGL_ENTRYPOINT_glBitmap:
            return true;
        default:
            break;
    }
    return vogl_is_draw_entrypoint(id) || vogl_is_clear_entrypoint(id);
}

//------------------------------------------------------------------------------
// vogl_is_start_nested_entrypoint
//------------------------------------------------------------------------------
bool vogl_is_start_nested_entrypoint(gl_entrypoint_id_t id)
{
    switch (id)
    {
        case VOGL_ENTRYPOINT_glBegin:
        case VOGL_ENTRYPOINT_glNewList:
        case VOGL_ENTRYPOINT_glPushName:
        case VOGL_ENTRYPOINT_glPushMatrix:
        case VOGL_ENTRYPOINT_glPushAttrib:
        case VOGL_ENTRYPOINT_glPushClientAttrib:
            return true;
        default:
            break;
    }
    return false;
}

//------------------------------------------------------------------------------
// vogl_is_end_nested_entrypoint
//------------------------------------------------------------------------------
bool vogl_is_end_nested_entrypoint(gl_entrypoint_id_t id)
{
    switch (id)
    {
        case VOGL_ENTRYPOINT_glEnd:
        case VOGL_ENTRYPOINT_glEndList:
        case VOGL_ENTRYPOINT_glPopName:
        case VOGL_ENTRYPOINT_glPopMatrix:
        case VOGL_ENTRYPOINT_glPopAttrib:
        case VOGL_ENTRYPOINT_glPopClientAttrib:
            return true;
        default:
            break;
    }
    return false;
}

//------------------------------------------------------------------------------
// vogl_is_marker_push_entrypoint
//------------------------------------------------------------------------------
bool vogl_is_marker_push_entrypoint(gl_entrypoint_id_t id)
{
    switch (id)
    {
        case VOGL_ENTRYPOINT_glPushDebugGroup:
            //case VOGL_ENTRYPOINT_glPushGroupMarkerEXT:
            return true;
        default:
            break;
    }
    return false;
}

//------------------------------------------------------------------------------
// vogl_is_marker_pop_entrypoint
//------------------------------------------------------------------------------
bool vogl_is_marker_pop_entrypoint(gl_entrypoint_id_t id)
{
    switch (id)
    {
        case VOGL_ENTRYPOINT_glPopDebugGroup:
            //case VOGL_ENTRYPOINT_glPopGroupMarkerEXT:
            return true;
        default:
            break;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_json_value_as_enum
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_json_value_as_enum(const json_node &node, const char *pKey, GLenum def)
{
    VOGL_FUNC_TRACER

    uint64_t v = get_gl_enums().find_enum(node.value_as_string(pKey));
    if (v == gl_enums::cUnknownEnum)
        return def;
    VOGL_ASSERT(v <= cUINT32_MAX);
    return static_cast<GLenum>(v);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_json_value_as_enum
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_json_value_as_enum(const json_value &val, GLenum def)
{
    VOGL_FUNC_TRACER

    uint64_t v = get_gl_enums().find_enum(val.as_string());
    if (v == gl_enums::cUnknownEnum)
        return def;
    VOGL_ASSERT(v <= cUINT32_MAX);
    return static_cast<GLenum>(v);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_binding_from_target
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_binding_from_target(GLenum target)
{
    VOGL_FUNC_TRACER

    switch (target)
    {
#define DEFINE_BINDING(c, t, b) \
    case t:                     \
        return b;
#include "gl_buffer_bindings.inc"
#undef DEFINE_BINDING

        default:
        {
            vogl_warning_printf("Unknown target GL enum 0x%08X\n", target);
            return GL_NONE;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_object_category_from_binding
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_object_category_from_binding(GLenum binding)
{
    VOGL_FUNC_TRACER

    switch (binding)
    {
#define DEFINE_BINDING(c, t, b) \
    case b:                     \
        return c;
#include "gl_buffer_bindings.inc"
#undef DEFINE_BINDING

        default:
        {
            vogl_warning_printf("Unknown binding GL enum 0x%08X\n", binding);
            return GL_NONE;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_object_category_from_target
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_object_category_from_target(GLenum target)
{
    VOGL_FUNC_TRACER

    switch (target)
    {
#define DEFINE_BINDING(c, t, b) \
    case t:                     \
        return c;
#include "gl_buffer_bindings.inc"
#undef DEFINE_BINDING

        default:
        {
            vogl_warning_printf("Unknown target GL enum 0x%08X\n", target);
            return GL_NONE;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_target_from_binding
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_get_target_from_binding(GLenum binding)
{
    VOGL_FUNC_TRACER

    switch (binding)
    {
#define DEFINE_BINDING(c, t, b) \
    case b:                     \
        return t;
#include "gl_buffer_bindings.inc"
#undef DEFINE_BINDING

        default:
        {
            vogl_warning_printf("Unknown binding GL enum 0x%08X\n", binding);
            return GL_NONE;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_bind_object
//----------------------------------------------------------------------------------------------------------------------
void vogl_bind_object(GLenum target, GLuint handle)
{
    VOGL_FUNC_TRACER

    GLenum category = vogl_get_object_category_from_target(target);

    switch (category)
    {
        case GL_TEXTURE:
        {
            GL_ENTRYPOINT(glBindTexture)(target, handle);
            break;
        }
        case GL_BUFFER:
        {
            GL_ENTRYPOINT(glBindBuffer)(target, handle);
            break;
        }
        case GL_FRAMEBUFFER:
        {
            GL_ENTRYPOINT(glBindFramebuffer)(target, handle);
            break;
        }
        case GL_RENDERBUFFER:
        {
            GL_ENTRYPOINT(glBindRenderbuffer)(target, handle);
            break;
        }
        case GL_SAMPLER:
        {
            GL_ENTRYPOINT(glBindSampler)(target, handle);
            break;
        }
        case GL_VERTEX_ARRAY:
        {
            GL_ENTRYPOINT(glBindVertexArray)(handle);
            break;
        }
        case GL_ACTIVE_TEXTURE:
        {
            // not really a binding, exactly, but seems useful
            GL_ENTRYPOINT(glActiveTexture)(handle);
            break;
        }
        case GL_PROGRAM:
        {
            GL_ENTRYPOINT(glUseProgram)(handle);
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }

    VOGL_CHECK_GL_ERROR;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_bound_object
//----------------------------------------------------------------------------------------------------------------------
GLuint vogl_get_bound_object(GLenum target)
{
    VOGL_FUNC_TRACER

    GLenum binding = vogl_get_binding_from_target(target);
    if (binding == GL_NONE)
    {
        VOGL_ASSERT_ALWAYS;
        return 0;
    }

    GLint handle = 0;
    GL_ENTRYPOINT(glGetIntegerv)(binding, &handle);
    VOGL_CHECK_GL_ERROR;

    return handle;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_debug_message_control
//----------------------------------------------------------------------------------------------------------------------
void vogl_debug_message_control(const vogl_context_info &context_info, GLenum err_code, bool enabled)
{
    VOGL_FUNC_TRACER

    if (!context_info.is_debug_context())
        return;

    if (!context_info.supports_extension("GL_ARB_debug_output") || (!GL_ENTRYPOINT(glDebugMessageControlARB)))
        return;

    GL_ENTRYPOINT(glDebugMessageControlARB)(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_ERROR_ARB, GL_DONT_CARE, 1, &err_code, enabled);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_determine_texture_target
// NOTE: Don't use this for anything other than debugging!
//----------------------------------------------------------------------------------------------------------------------
GLenum vogl_determine_texture_target(const vogl_context_info &context_info, GLuint handle)
{
    VOGL_FUNC_TRACER

    GLboolean is_texture = GL_ENTRYPOINT(glIsTexture)(handle);
    if (!is_texture)
        return GL_NONE;

    vogl_scoped_binding_state orig_texture_state;
    orig_texture_state.save_textures(&context_info);

    VOGL_CHECK_GL_ERROR;

    // roughly sorted by most likely to least
    static const GLenum s_possible_targets[] =
        {
            GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP,
            GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_1D, GL_TEXTURE_1D_ARRAY,
            GL_TEXTURE_RECTANGLE, GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BUFFER, GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY
        };

    vogl_debug_message_control(context_info, GL_INVALID_OPERATION, false);

    uint32_t i;
    for (i = 0; i < VOGL_ARRAY_SIZE(s_possible_targets); i++)
    {
        GL_ENTRYPOINT(glBindTexture)(s_possible_targets[i], handle);
        if (!vogl_check_gl_error_suppress_message())
            break;
    }

    if (i == VOGL_ARRAY_SIZE(s_possible_targets))
        vogl_check_gl_error_suppress_message();

    vogl_debug_message_control(context_info, GL_INVALID_OPERATION, true);

    return (i < VOGL_ARRAY_SIZE(s_possible_targets)) ? s_possible_targets[i] : GL_NONE;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_saver::save
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_saver::save(vogl_generic_state_type type, const vogl_context_info *context_info)
{
    VOGL_FUNC_TRACER

#define SAVE_INT_STATE(type, pname)                                               \
    do                                                                            \
    {                                                                             \
        m_states.push_back(saved_state(type, pname, vogl_get_gl_integer(pname))); \
    } while (0)
#define SAVE_FLOAT_STATE(type, pname)                                           \
    do                                                                          \
    {                                                                           \
        m_states.push_back(saved_state(type, pname, vogl_get_gl_float(pname))); \
    } while (0)

    switch (type)
    {
        case cGSTPixelStore:
        {
            static const GLenum s_pixel_store_enums[] =
                {
                    GL_PACK_SWAP_BYTES, GL_PACK_LSB_FIRST, GL_PACK_ROW_LENGTH, GL_PACK_IMAGE_HEIGHT, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_IMAGES, GL_PACK_ALIGNMENT,
                    GL_UNPACK_SWAP_BYTES, GL_UNPACK_LSB_FIRST, GL_UNPACK_ROW_LENGTH, GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_IMAGES, GL_UNPACK_ALIGNMENT
                };

            for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_pixel_store_enums); i++)
                SAVE_INT_STATE(cGSTPixelStore, s_pixel_store_enums[i]);

            break;
        }
        case cGSTPixelTransfer:
        {
            static const GLenum s_pixel_transfer_int_enums[] =
                {
                    GL_MAP_COLOR, GL_MAP_STENCIL, GL_INDEX_SHIFT, GL_INDEX_OFFSET
                };

            for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_pixel_transfer_int_enums); i++)
                SAVE_INT_STATE(cGSTPixelTransfer, s_pixel_transfer_int_enums[i]);

            static const GLenum s_pixel_transfer_float_enums[] =
                {
                    GL_INDEX_OFFSET, GL_RED_SCALE, GL_GREEN_SCALE, GL_BLUE_SCALE, GL_ALPHA_SCALE, GL_DEPTH_SCALE, GL_RED_BIAS, GL_GREEN_BIAS,
                    GL_BLUE_BIAS, GL_ALPHA_BIAS, GL_DEPTH_BIAS, GL_POST_COLOR_MATRIX_RED_SCALE, GL_POST_COLOR_MATRIX_GREEN_SCALE, GL_POST_COLOR_MATRIX_BLUE_SCALE,
                    GL_POST_COLOR_MATRIX_ALPHA_SCALE, GL_POST_COLOR_MATRIX_RED_BIAS, GL_POST_COLOR_MATRIX_GREEN_BIAS, GL_POST_COLOR_MATRIX_BLUE_BIAS, GL_POST_COLOR_MATRIX_ALPHA_BIAS,
                    GL_POST_CONVOLUTION_RED_SCALE, GL_POST_CONVOLUTION_GREEN_SCALE, GL_POST_CONVOLUTION_BLUE_SCALE, GL_POST_CONVOLUTION_ALPHA_SCALE, GL_POST_CONVOLUTION_RED_BIAS,
                    GL_POST_CONVOLUTION_GREEN_BIAS, GL_POST_CONVOLUTION_BLUE_BIAS, GL_POST_CONVOLUTION_ALPHA_BIAS
                };
            uint32_t array_size = VOGL_ARRAY_SIZE(s_pixel_transfer_float_enums);
            if (!context_info || !context_info->supports_extension("GL_ARB_imaging"))
                array_size -= 16; // skip enums from GL_POST_COLOR_MATRIX_RED_SCALE
            for (uint32_t i = 0; i < array_size; i++)
                SAVE_FLOAT_STATE(cGSTPixelTransfer, s_pixel_transfer_float_enums[i]);

            break;
        }
        case cGSTReadBuffer:
        {
            SAVE_INT_STATE(type, GL_READ_BUFFER);
            break;
        }
        case cGSTDrawBuffer:
        {
            uint32_t max_draw_buffers = vogl_get_gl_integer(GL_MAX_DRAW_BUFFERS);
            m_draw_buffers.resize(max_draw_buffers);

            for (uint32_t i = 0; i < max_draw_buffers; i++)
                m_draw_buffers[i] = vogl_get_gl_integer(GL_DRAW_BUFFER0 + i);

            break;
        }
        case cGSTActiveTexture:
        {
            GLint active_texture = 0;
            GL_ENTRYPOINT(glGetIntegerv)(GL_ACTIVE_TEXTURE, &active_texture);
            m_states.push_back(saved_state(type, GL_ACTIVE_TEXTURE, active_texture));
            break;
        }
        case cGSTClientActiveTexture:
        {
            // FIXME: Don't think this is available in core (let the caller worry about it)
            GLint client_active_texture = 0;
            GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &client_active_texture);
            m_states.push_back(saved_state(type, GL_CLIENT_ACTIVE_TEXTURE, client_active_texture));
            break;
        }
        case cGSTMatrixMode:
        {
            SAVE_INT_STATE(type, GL_MATRIX_MODE);
            break;
        }
        case cGSTARBVertexProgram:
        {
            GLint cur_arb_vertex_program = 0;
            GL_ENTRYPOINT(glGetProgramivARB)(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &cur_arb_vertex_program);
            m_states.push_back(saved_state(type, GL_VERTEX_PROGRAM_ARB, cur_arb_vertex_program));
            break;
        }
        case cGSTARBFragmentProgram:
        {
            GLint cur_arb_fragment_program = 0;
            GL_ENTRYPOINT(glGetProgramivARB)(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &cur_arb_fragment_program);
            m_states.push_back(saved_state(type, GL_FRAGMENT_PROGRAM_ARB, cur_arb_fragment_program));
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }

#undef SAVE_INT_STATE
#undef SAVE_FLOAT_STATE

    VOGL_CHECK_GL_ERROR;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_saver::restore
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_saver::restore()
{
    VOGL_FUNC_TRACER

    if (m_draw_buffers.size())
    {
        int i;
        for (i = m_draw_buffers.size() - 1; i >= 0; --i)
            if (m_draw_buffers[i] != GL_NONE)
                break;

        // It's possible that every draw buffer is set to GL_NONE in which case, nothing to restore
        if (i >= 0)
        {
            // On NV, we can cvall glDrawBuffers() with a single GL_BACK, etc. and it's fine. On AMD it barfs.
            if (!i)
            {
                GL_ENTRYPOINT(glDrawBuffer)(m_draw_buffers[0]);
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glDrawBuffers)(i + 1, m_draw_buffers.get_ptr());
                VOGL_CHECK_GL_ERROR;
            }
        }
    }

    for (uint32_t i = 0; i < m_states.size(); i++)
    {
        const vogl::value_data_type value_type = m_states[i].m_value.get_data_type();

        switch (m_states[i].m_state_type)
        {
            case cGSTPixelStore:
            {
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glPixelStorei)(m_states[i].m_pname, m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            case cGSTPixelTransfer:
            {
                if (value_type == cDTFloat)
                {
                    GL_ENTRYPOINT(glPixelTransferf)(m_states[i].m_pname, m_states[i].m_value.get_float());
                    VOGL_CHECK_GL_ERROR;
                }
                else if (value_type == cDTInt)
                {
                    GL_ENTRYPOINT(glPixelTransferi)(m_states[i].m_pname, m_states[i].m_value.get_int());
                    VOGL_CHECK_GL_ERROR;
                }
                else
                {
                    VOGL_ASSERT_ALWAYS;
                    break;
                }
                break;
            }
            case cGSTReadBuffer:
            {
                VOGL_ASSERT(m_states[i].m_pname == GL_READ_BUFFER);
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glReadBuffer)(m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            case cGSTDrawBuffer:
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }
            case cGSTActiveTexture:
            {
                VOGL_ASSERT(m_states[i].m_pname == GL_ACTIVE_TEXTURE);
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glActiveTexture)(m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            case cGSTClientActiveTexture:
            {
                VOGL_ASSERT(m_states[i].m_pname == GL_CLIENT_ACTIVE_TEXTURE);
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glClientActiveTexture)(m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            case cGSTMatrixMode:
            {
                VOGL_ASSERT(m_states[i].m_pname == GL_MATRIX_MODE);
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glMatrixMode)(m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            case cGSTARBVertexProgram:
            {
                VOGL_ASSERT(m_states[i].m_pname == GL_VERTEX_PROGRAM_ARB);
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glBindProgramARB)(GL_VERTEX_PROGRAM_ARB, m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            case cGSTARBFragmentProgram:
            {
                VOGL_ASSERT(m_states[i].m_pname == GL_FRAGMENT_PROGRAM_ARB);
                VOGL_ASSERT(value_type == cDTInt);
                GL_ENTRYPOINT(glBindProgramARB)(GL_FRAGMENT_PROGRAM_ARB, m_states[i].m_value.get_int());
                VOGL_CHECK_GL_ERROR;
                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_get_uniform_size_in_GLints
//----------------------------------------------------------------------------------------------------------------------
int vogl_gl_get_uniform_size_in_GLints(GLenum type)
{
    VOGL_FUNC_TRACER

    switch (type)
    {
        case GL_FLOAT:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_BOOL:

        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_1D_ARRAY_SHADOW:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_SAMPLER_BUFFER:
        case GL_SAMPLER_2D_RECT:
        case GL_SAMPLER_2D_RECT_SHADOW:
        case GL_SAMPLER_CUBE_MAP_ARRAY:
        case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
        case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:

        case GL_INT_SAMPLER_1D:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_INT_SAMPLER_1D_ARRAY:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_INT_SAMPLER_BUFFER:
        case GL_INT_SAMPLER_2D_RECT:
        case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_BUFFER:
        case GL_UNSIGNED_INT_SAMPLER_2D_RECT:

        case GL_IMAGE_1D:
        case GL_IMAGE_2D:
        case GL_IMAGE_3D:
        case GL_IMAGE_2D_RECT:
        case GL_IMAGE_CUBE:
        case GL_IMAGE_BUFFER:
        case GL_IMAGE_1D_ARRAY:
        case GL_IMAGE_2D_ARRAY:
        case GL_IMAGE_2D_MULTISAMPLE:
        case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_INT_IMAGE_1D:
        case GL_INT_IMAGE_2D:
        case GL_INT_IMAGE_3D:
        case GL_INT_IMAGE_2D_RECT:
        case GL_INT_IMAGE_CUBE:
        case GL_INT_IMAGE_BUFFER:
        case GL_INT_IMAGE_1D_ARRAY:
        case GL_INT_IMAGE_2D_ARRAY:
        case GL_INT_IMAGE_2D_MULTISAMPLE:
        case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_1D:
        case GL_UNSIGNED_INT_IMAGE_2D:
        case GL_UNSIGNED_INT_IMAGE_3D:
        case GL_UNSIGNED_INT_IMAGE_2D_RECT:
        case GL_UNSIGNED_INT_IMAGE_CUBE:
        case GL_UNSIGNED_INT_IMAGE_BUFFER:
        case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:

        case GL_UNSIGNED_INT_ATOMIC_COUNTER:

            return 1;

        case GL_DOUBLE:
        case GL_FLOAT_VEC2:
        case GL_INT_VEC2:
        case GL_UNSIGNED_INT_VEC2:
        case GL_BOOL_VEC2:
            return 2;

        case GL_FLOAT_VEC3:
        case GL_INT_VEC3:
        case GL_UNSIGNED_INT_VEC3:
        case GL_BOOL_VEC3:
            return 3;

        case GL_DOUBLE_VEC2:
        case GL_FLOAT_VEC4:
        case GL_INT_VEC4:
        case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL_VEC4:
        case GL_FLOAT_MAT2:
            return 4;

        case GL_FLOAT_MAT3:
            return 9;

        case GL_FLOAT_MAT4:
            return 16;

        case GL_DOUBLE_VEC3:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT3x2:
            return 6;

        case GL_DOUBLE_VEC4:
        case GL_DOUBLE_MAT2:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT4x2:
            return 8;

        case GL_DOUBLE_MAT2x3:
        case GL_DOUBLE_MAT3x2:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x3:
            return 12;

        case GL_DOUBLE_MAT2x4:
        case GL_DOUBLE_MAT4x2:
            return 16;

        case GL_DOUBLE_MAT3:
            return 18;

        case GL_DOUBLE_MAT3x4:
        case GL_DOUBLE_MAT4x3:
            return 24;

        case GL_DOUBLE_MAT4:
            return 32;

        default:
        {
            VOGL_ASSERT_ALWAYS;
            vogl_warning_printf("Unknown uniform type 0x%04X\n", type);
            return 0;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_get_uniform_size_in_bytes
//----------------------------------------------------------------------------------------------------------------------
int vogl_gl_get_uniform_size_in_bytes(GLenum type)
{
    VOGL_FUNC_TRACER

    return vogl_gl_get_uniform_size_in_GLints(type) * sizeof(GLint);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_get_uniform_base_type
//----------------------------------------------------------------------------------------------------------------------
int vogl_gl_get_uniform_base_type(GLenum type)
{
    VOGL_FUNC_TRACER

    switch (type)
    {
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT3x2:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT4x2:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x3:
            return GL_FLOAT;

        case GL_DOUBLE:
        case GL_DOUBLE_VEC2:
        case GL_DOUBLE_VEC3:
        case GL_DOUBLE_VEC4:
        case GL_DOUBLE_MAT2:
        case GL_DOUBLE_MAT3:
        case GL_DOUBLE_MAT4:
        case GL_DOUBLE_MAT2x3:
        case GL_DOUBLE_MAT3x2:
        case GL_DOUBLE_MAT2x4:
        case GL_DOUBLE_MAT4x2:
        case GL_DOUBLE_MAT3x4:
        case GL_DOUBLE_MAT4x3:
            return GL_DOUBLE;

        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
            return GL_INT;

        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
        case GL_UNSIGNED_INT_ATOMIC_COUNTER:
            return GL_UNSIGNED_INT;

        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:
            return GL_BOOL;

        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_1D_SHADOW:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_1D_ARRAY_SHADOW:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_SAMPLER_BUFFER:
        case GL_SAMPLER_2D_RECT:
        case GL_SAMPLER_2D_RECT_SHADOW:
        case GL_SAMPLER_CUBE_MAP_ARRAY:
        case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
        case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
        case GL_INT_SAMPLER_1D:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_INT_SAMPLER_1D_ARRAY:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_INT_SAMPLER_BUFFER:
        case GL_INT_SAMPLER_2D_RECT:
        case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_BUFFER:
        case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
            return GL_SAMPLER;

        case GL_IMAGE_1D:
        case GL_IMAGE_2D:
        case GL_IMAGE_3D:
        case GL_IMAGE_2D_RECT:
        case GL_IMAGE_CUBE:
        case GL_IMAGE_BUFFER:
        case GL_IMAGE_1D_ARRAY:
        case GL_IMAGE_2D_ARRAY:
        case GL_IMAGE_2D_MULTISAMPLE:
        case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_INT_IMAGE_1D:
        case GL_INT_IMAGE_2D:
        case GL_INT_IMAGE_3D:
        case GL_INT_IMAGE_2D_RECT:
        case GL_INT_IMAGE_CUBE:
        case GL_INT_IMAGE_BUFFER:
        case GL_INT_IMAGE_1D_ARRAY:
        case GL_INT_IMAGE_2D_ARRAY:
        case GL_INT_IMAGE_2D_MULTISAMPLE:
        case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_1D:
        case GL_UNSIGNED_INT_IMAGE_2D:
        case GL_UNSIGNED_INT_IMAGE_3D:
        case GL_UNSIGNED_INT_IMAGE_2D_RECT:
        case GL_UNSIGNED_INT_IMAGE_CUBE:
        case GL_UNSIGNED_INT_IMAGE_BUFFER:
        case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
            return GL_IMAGE_1D; // what do we return??

        default:
        {
            VOGL_ASSERT_ALWAYS;
            vogl_warning_printf("Unknown uniform type 0x%04X\n", type);
            return GL_NONE;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_format_debug_output_arb
// Loosely derived from http://www.altdevblogaday.com/2011/06/23/improving-opengl-error-messages/
//----------------------------------------------------------------------------------------------------------------------
void vogl_format_debug_output_arb(char out_str[], size_t out_str_size, GLenum source, GLenum type, GLuint id, GLenum severity, const char *msg)
{
    VOGL_FUNC_TRACER

    char source_str[32];
    const char *source_fmt = "UNDEFINED(0x%04X)";

    switch (source)
    {
        case GL_DEBUG_SOURCE_API_ARB:
            source_fmt = "API";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
            source_fmt = "WINDOW_SYSTEM";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
            source_fmt = "SHADER_COMPILER";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
            source_fmt = "THIRD_PARTY";
            break;
        case GL_DEBUG_SOURCE_APPLICATION_ARB:
            source_fmt = "APPLICATION";
            break;
        case GL_DEBUG_SOURCE_OTHER_ARB:
            source_fmt = "OTHER";
            break;
        default:
            break;
    }

    vogl_sprintf_s(source_str, sizeof(source_str), source_fmt, source);

    char type_str[32];
    const char *type_fmt = "UNDEFINED(0x%04X)";
    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR_ARB:
            type_fmt = "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
            type_fmt = "DEPRECATED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
            type_fmt = "UNDEFINED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY_ARB:
            type_fmt = "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE_ARB:
            type_fmt = "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER_ARB:
            type_fmt = "OTHER";
            break;
        default:
            break;
    }
    vogl_sprintf_s(type_str, sizeof(type_str), type_fmt, type);

    char severity_str[32];
    const char *severity_fmt = "UNDEFINED";
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH_ARB:
            severity_fmt = "HIGH";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM_ARB:
            severity_fmt = "MEDIUM";
            break;
        case GL_DEBUG_SEVERITY_LOW_ARB:
            severity_fmt = "LOW";
            break;
        default:
            break;
    }

    vogl_sprintf_s(severity_str, sizeof(severity_str), severity_fmt, severity);

    vogl_sprintf_s(out_str, out_str_size, "OpenGL: %s [source=%s type=%s severity=%s id=%d]", msg, source_str, type_str, severity_str, id);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_generic_arb_debug_callback
//----------------------------------------------------------------------------------------------------------------------
void GLAPIENTRY vogl_generic_arb_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(length);
    VOGL_NOTE_UNUSED(pUser_param);

    char final_message[4096];

    vogl_format_debug_output_arb(final_message, sizeof(final_message), source, type, id, severity, reinterpret_cast<const char *>(message));

    vogl_warning_printf("%s\n", final_message);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_enable_generic_context_debug_messages
//----------------------------------------------------------------------------------------------------------------------
void vogl_enable_generic_context_debug_messages()
{
    GL_ENTRYPOINT(glDebugMessageCallbackARB)(vogl_generic_arb_debug_callback, NULL);
    GL_ENTRYPOINT(glEnable)(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
    VOGL_CHECK_GL_ERROR;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_create_context
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_context vogl_create_context(vogl_gl_display display, vogl_gl_fb_config fb_config, vogl_gl_context share_context, uint32_t major_ver, uint32_t minor_ver, uint32_t flags, vogl_context_desc *pDesc)
{
#if VOGL_PLATFORM_HAS_GLX
    vogl_context_attribs attribs;
    attribs.add_key(GLX_CONTEXT_MAJOR_VERSION_ARB, major_ver);
    attribs.add_key(GLX_CONTEXT_MINOR_VERSION_ARB, minor_ver);

    if (flags & (cCHCCoreProfileFlag | cCHCCompatProfileFlag))
    {
        uint32_t profile_mask = 0;
        if (flags & cCHCCoreProfileFlag)
            profile_mask |= GLX_CONTEXT_CORE_PROFILE_BIT_ARB;

        if (flags & cCHCCompatProfileFlag)
            profile_mask |= GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

        attribs.add_key(GLX_CONTEXT_PROFILE_MASK_ARB, profile_mask);
    }
    if (flags & eCHCDebugContextFlag)
    {
        attribs.add_key(GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB);
    }

    GLXContext context = GL_ENTRYPOINT(glXCreateContextAttribsARB)(display, fb_config, share_context, GL_TRUE, attribs.get_ptr());

    if (pDesc)
    {
        if (!context)
            pDesc->clear();
        else
            pDesc->init(VOGL_ENTRYPOINT_glXCreateContextAttribsARB, GL_TRUE, (vogl_trace_ptr_value)context, (vogl_trace_ptr_value)share_context, attribs);
    }

    return context;
#else
    VOGL_ASSERT(!"impl vogl_create_context");
    return 0;
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_current_context
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_context vogl_get_current_context()
{
#if (VOGL_PLATFORM_HAS_GLX)
    return GL_ENTRYPOINT(glXGetCurrentContext)();
#elif(VOGL_PLATFORM_HAS_WGL)
    return GL_ENTRYPOINT(wglGetCurrentContext)();
#else
    VOGL_ASSERT(!"impl vogl_get_current_context");
    return 0;
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_current_display
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_display vogl_get_current_display()
{
#if (VOGL_PLATFORM_HAS_GLX)
    return GL_ENTRYPOINT(glXGetCurrentDisplay)();

#elif (VOGL_PLATFORM_HAS_CGL)
    VOGL_ASSERT(!"UNIMPLEMENTED vogl_get_current_display");
    return(NULL);

#elif(VOGL_PLATFORM_HAS_WGL)
    // TODO: Not sure if this is the correct equivalent
    return GL_ENTRYPOINT(wglGetCurrentDC)();

#else
    VOGL_ASSERT(!"impl vogl_get_current_display");
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_current_drawable
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_drawable vogl_get_current_drawable()
{
#if (VOGL_PLATFORM_HAS_GLX)
    return GL_ENTRYPOINT(glXGetCurrentDrawable)();

#elif (VOGL_PLATFORM_HAS_CGL)
    VOGL_ASSERT(!"UNIMPLEMENTED vogl_get_current_drawable");
    return(NULL);

#elif(VOGL_PLATFORM_HAS_WGL)
    return WindowFromDC(GL_ENTRYPOINT(wglGetCurrentDC)());

#else
    VOGL_ASSERT(!"impl vogl_get_current_drawable");
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_current_fb_config
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_fb_config vogl_get_current_fb_config(uint32_t screen)
{
#if VOGL_PLATFORM_HAS_GLX
    GLXDrawable pDrawable = GL_ENTRYPOINT(glXGetCurrentDrawable)();
    Display *pDisplay = GL_ENTRYPOINT(glXGetCurrentDisplay)();
    if ((!pDrawable) || (!pDisplay))
        return 0;

    GLuint fbconfig_id = 0;
    GL_ENTRYPOINT(glXQueryDrawable)(pDisplay, pDrawable, GLX_FBCONFIG_ID, &fbconfig_id);

    GLint attrib_list[3] = { GLX_FBCONFIG_ID, static_cast<GLint>(fbconfig_id), None };
    GLint nelements = 0;
    GLXFBConfig *pConfigs = GL_ENTRYPOINT(glXChooseFBConfig)(pDisplay, screen, attrib_list, &nelements);

    if ((pConfigs) && (nelements > 0))
        return pConfigs[0];

    return 0;
#else
    VOGL_ASSERT(!"impl vogl_get_current_fb_config");
    return 0;
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_make_current
//----------------------------------------------------------------------------------------------------------------------
void vogl_make_current(vogl_gl_display display, vogl_gl_drawable drawable, vogl_gl_context context)
{
#if VOGL_PLATFORM_HAS_GLX
    GL_ENTRYPOINT(glXMakeCurrent)(display, drawable, context);
#elif VOGL_PLATFORM_HAS_WGL
    VOGL_NOTE_UNUSED(drawable);
    GL_ENTRYPOINT(wglMakeCurrent)(display, context);
#else
    VOGL_ASSERT(!"impl vogl_make_current");
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_destroy_context
//----------------------------------------------------------------------------------------------------------------------
void vogl_destroy_context(vogl_gl_display display, vogl_gl_context context)
{
#if VOGL_PLATFORM_HAS_GLX
    GL_ENTRYPOINT(glXDestroyContext)(display, context);
#elif(VOGL_PLATFORM_HAS_WGL)
    VOGL_NOTE_UNUSED(display);
    GL_ENTRYPOINT(wglDeleteContext)(context);
#else
    VOGL_ASSERT(!"impl vogl_destroy_context");
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_max_supported_mip_levels
//----------------------------------------------------------------------------------------------------------------------
int vogl_get_max_supported_mip_levels()
{
    int max_texture_size = vogl_get_gl_integer(GL_MAX_TEXTURE_SIZE);
    return math::floor_log2i(max_texture_size);
}

//----------------------------------------------------------------------------------------------------------------------
// pretty_print_param_val
// Assumes CPU is little endian.
//----------------------------------------------------------------------------------------------------------------------
bool pretty_print_param_val(dynamic_string &str, const vogl_ctype_desc_t &ctype_desc, uint64_t val_data, gl_entrypoint_id_t entrypoint_id, int param_index)
{
    bool handled = false;
    switch (ctype_desc.m_ctype)
    {
        case VOGL_BOOL:
        case VOGL_GLBOOLEAN:
        {
            if (val_data == 0)
            {
                str += "false";
                handled = true;
            }
            else if (val_data == 1)
            {
                str += "true";
                handled = true;
            }

            break;
        }
        case VOGL_GLENUM:
        {
            const char *pName;
            if (entrypoint_id != VOGL_ENTRYPOINT_INVALID)
                pName = get_gl_enums().find_name(val_data, entrypoint_id, param_index);
            else
                pName = get_gl_enums().find_name(val_data);

            if (pName)
            {
                VOGL_ASSERT(get_gl_enums().find_enum(pName) == val_data);

                str += pName;
                handled = true;
            }

            break;
        }
        case VOGL_FLOAT:
        case VOGL_GLFLOAT:
        case VOGL_GLCLAMPF:
        {
            str.format_append("%f", *reinterpret_cast<const float *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLDOUBLE:
        case VOGL_GLCLAMPD:
        {
            str.format_append("%f", *reinterpret_cast<const double *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLHANDLEARB:
        case VOGL_GLUINT:
        {
            str.format_append("%u", *reinterpret_cast<const uint32_t *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLBITFIELD:
        {
            if (val_data==0)
            {
                str.format_append("0");
            }else{
                bool first = true;
                for (int bit = 0; bit < sizeof(uint64_t)*8; bit++)
                {
                    uint64_t mask = (((uint64_t)1) << bit);
                    uint64_t flag = mask & val_data;
                    if (flag)
                    {
                        if (!first)
                            str.format_append(" | ");
                        first=false;

                        const char *pName;
                        if (entrypoint_id != VOGL_ENTRYPOINT_INVALID)
                            pName = get_gl_enums().find_name(flag, entrypoint_id, param_index);
                        else
                            pName = get_gl_enums().find_name(flag);

                        if (pName)
                        {
                            VOGL_ASSERT(get_gl_enums().find_enum(pName) == flag);

                            str += pName;
                        }else
                            str.format_append("%" PRIi64, flag);
                    }
                }
            }
            handled = true;
            break;
        }
        case VOGL_GLCHAR:
        {
            uint32_t v = *reinterpret_cast<const uint8_t *>(&val_data);
            if ((v >= 32) && (v < 128))
                str.format_append("'%c'", v);
            else
                str.format_append("%u", v);
            handled = true;
            break;
        }
        case VOGL_GLUBYTE:
        {
            str.format_append("%u", *reinterpret_cast<const uint8_t *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLUSHORT:
        {
            str.format_append("%u", *reinterpret_cast<const uint16_t *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLINT:
        case VOGL_INT:
        case VOGL_INT32T:
        case VOGL_GLSIZEI:
        case VOGL_GLFIXED:
        {
            str.format_append("%i", *reinterpret_cast<const int32_t *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLSHORT:
        {
            str.format_append("%i", *reinterpret_cast<const int16_t *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLBYTE:
        {
            str.format_append("%i", *reinterpret_cast<const int8_t *>(&val_data));
            handled = true;
            break;
        }
        case VOGL_GLINT64:
        case VOGL_GLINT64EXT:
        {
            str.format_append("%" PRIi64, val_data);
            handled = true;
            break;
        }
        default:
            break;
    }

    return handled;
}

/**************************************************************************
 *
 * Following functions derived from apitrace. Apitrace license:
 *
 * Copyright 2011 Jose Fonseca
 * Copyright 2008-2010 VMware, Inc.
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

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_gl_type_size (derived from apitrace)
//----------------------------------------------------------------------------------------------------------------------
uint32_t vogl_get_gl_type_size(uint32_t ogl_type)
{
    VOGL_FUNC_TRACER

    switch (ogl_type)
    {
        case GL_BOOL:
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return 1;
        case GL_HALF_FLOAT:
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_2_BYTES:
            return 2;
        case GL_3_BYTES:
            return 3;
        case GL_FLOAT:
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FIXED:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_24_8:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
        case GL_INT_2_10_10_10_REV:
        case GL_4_BYTES:
            return 4;
        case GL_DOUBLE:
            return 8;
        default:
            vogl_warning_printf("unknown GL type: 0x%04X\n", ogl_type);
            break;
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_image_format_channels (derived from apitrace)
//----------------------------------------------------------------------------------------------------------------------
uint32_t vogl_get_image_format_channels(GLenum format)
{
    VOGL_FUNC_TRACER

    switch (format)
    {
        case GL_COLOR_INDEX:
        case GL_RED:
        case GL_RED_INTEGER:
        case GL_GREEN:
        case GL_GREEN_INTEGER:
        case GL_BLUE:
        case GL_BLUE_INTEGER:
        case GL_ALPHA:
        case GL_ALPHA_INTEGER:
        case GL_INTENSITY:
        case GL_LUMINANCE:
        case GL_LUMINANCE_INTEGER_EXT:
        case GL_DEPTH_COMPONENT:
        case GL_STENCIL_INDEX:
            return 1;
        case GL_DEPTH_STENCIL:
        case GL_LUMINANCE_ALPHA:
        case GL_LUMINANCE_ALPHA_INTEGER_EXT:
        case GL_RG:
        case GL_RG_INTEGER:
        case GL_422_EXT:             // (luminance, chrominance)
        case GL_422_REV_EXT:         // (luminance, chrominance)
        case GL_422_AVERAGE_EXT:     // (luminance, chrominance)
        case GL_422_REV_AVERAGE_EXT: // (luminance, chrominance)
        case GL_HILO_NV:             // (hi, lo)
        case GL_DSDT_NV:             // (ds, dt)
        case GL_YCBCR_422_APPLE:     // (luminance, chroma)
        case GL_RGB_422_APPLE:       // (G, B) on even pixels, (G, R) on odd pixels
        case GL_YCRCB_422_SGIX:      // (Y, [Cb,Cr])
            return 2;
        case GL_RGB:
        case GL_RGB_INTEGER:
        case GL_BGR:
        case GL_BGR_INTEGER:
        case GL_DSDT_MAG_NV:    // (ds, dt, magnitude)
        case GL_YCRCB_444_SGIX: // (Cb, Y, Cr)
            return 3;
        case GL_RGBA:
        case GL_RGBA_INTEGER:
        case GL_BGRA:
        case GL_BGRA_INTEGER:
        case GL_ABGR_EXT:
        case GL_CMYK_EXT:
        case GL_DSDT_MAG_VIB_NV: // (ds, dt, magnitude, vibrance)
            return 4;
        case GL_CMYKA_EXT:
            return 5;
        case GL_FORMAT_SUBSAMPLE_24_24_OML:
        case GL_FORMAT_SUBSAMPLE_244_244_OML:
            // requires UNSIGNED_INT_10_10_10_2, so this value will be ignored
            return 0;
        default:
            vogl_warning_printf("unknown format 0x%04X\n", format);
            break;
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_image_format_info (derived from apitrace)
//----------------------------------------------------------------------------------------------------------------------
void vogl_get_image_format_info(GLenum format, GLenum type, uint32_t &num_channels, uint32_t &bits_per_element, uint32_t &bits_per_pixel)
{
    VOGL_FUNC_TRACER

    num_channels = vogl_get_image_format_channels(format);

    switch (type)
    {
        case GL_BITMAP:
            bits_per_pixel = bits_per_element = 1;
            break;
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            bits_per_element = 8;
            bits_per_pixel = bits_per_element * num_channels;
            break;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_HALF_FLOAT:
            bits_per_element = 16;
            bits_per_pixel = bits_per_element * num_channels;
            break;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            bits_per_element = 32;
            bits_per_pixel = bits_per_element * num_channels;
            break;
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            bits_per_pixel = bits_per_element = 8;
            break;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_8_8_MESA:
        case GL_UNSIGNED_SHORT_8_8_REV_MESA:
            bits_per_pixel = bits_per_element = 16;
            break;
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_24_8:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
        case GL_UNSIGNED_INT_S8_S8_8_8_NV:
        case GL_UNSIGNED_INT_8_8_S8_S8_REV_NV:
            bits_per_pixel = bits_per_element = 32;
            break;
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            bits_per_pixel = bits_per_element = 64;
            break;
        default:
            vogl_warning_printf("unknown type 0x%04X\n", type);
            bits_per_pixel = bits_per_element = 0;
            break;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_image_size (derived from apitrace)
//----------------------------------------------------------------------------------------------------------------------
size_t vogl_get_image_size(GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth)
{
    VOGL_FUNC_TRACER

    if (!vogl_has_active_context())
    {
        vogl_error_printf("vogl_get_image_size() called without an active context!\n");
        return 0;
    }

    uint32_t num_channels;
    uint32_t bits_per_element;
    uint32_t bits_per_pixel;
    vogl_get_image_format_info(format, type, num_channels, bits_per_element, bits_per_pixel);

    GLint alignment = vogl_get_gl_integer(GL_UNPACK_ALIGNMENT);
    GLint row_length = vogl_get_gl_integer(GL_UNPACK_ROW_LENGTH);
    GLint image_height = vogl_get_gl_integer(GL_UNPACK_IMAGE_HEIGHT);
    GLint skip_rows = vogl_get_gl_integer(GL_UNPACK_SKIP_ROWS);
    GLint skip_pixels = vogl_get_gl_integer(GL_UNPACK_SKIP_PIXELS);
    GLint skip_images = vogl_get_gl_integer(GL_UNPACK_SKIP_IMAGES);

    //printf("format: 0x%X type: 0x%X width: %i height: %i depth: %i alignment: %i row_length: %i image_height: %i skip_rows: %i skip_pixels: %i skip_images: %i\n",
    //       format, type, (int)width, (int)height, (int)depth, alignment, row_length, image_height, skip_rows, skip_pixels, skip_images);

    if (row_length <= 0)
    {
        row_length = width;
    }

    size_t row_stride = (row_length * bits_per_pixel + 7) / 8;

    if (((bits_per_element == 1 * 8) ||
         (bits_per_element == 2 * 8) ||
         (bits_per_element == 4 * 8) ||
         (bits_per_element == 8 * 8)) &&
        ((GLint)bits_per_element < alignment * 8))
    {
        row_stride = math::align_up_value(row_stride, alignment);
    }

    if (image_height <= 0)
    {
        image_height = height;
    }

    // GL_UNPACK_IMAGE_HEIGHT and GL_UNPACK_SKIP_IMAGES should probably not be considered for pixel rectangles

    size_t image_stride = image_height * row_stride;

    size_t size = depth * image_stride;

    size += (skip_pixels * bits_per_pixel + 7) / 8;
    size += skip_rows * row_stride;
    size += skip_images * image_stride;

    return size;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_determine_glMap1_size (originally from apitrace)
//----------------------------------------------------------------------------------------------------------------------
size_t vogl_determine_glMap1_size(GLenum target, GLint stride, GLint order)
{
    VOGL_FUNC_TRACER

    if (order < 1)
        return 0;

    GLint channels;
    switch (target)
    {
        case GL_MAP1_INDEX:
        case GL_MAP1_TEXTURE_COORD_1:
            channels = 1;
            break;
        case GL_MAP1_TEXTURE_COORD_2:
            channels = 2;
            break;
        case GL_MAP1_NORMAL:
        case GL_MAP1_TEXTURE_COORD_3:
        case GL_MAP1_VERTEX_3:
            channels = 3;
            break;
        case GL_MAP1_COLOR_4:
        case GL_MAP1_TEXTURE_COORD_4:
        case GL_MAP1_VERTEX_4:
            channels = 4;
            break;
        default:
            vogl_warning_printf("unknown GLenum 0x%04X\n", target);
            return 0;
    }

    if (stride < channels)
        return 0;

    return channels + stride * (order - 1);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_determine_glMap2_size (originally from apitrace)
//----------------------------------------------------------------------------------------------------------------------
size_t vogl_determine_glMap2_size(GLenum target, GLint ustride, GLint uorder, GLint vstride, GLint vorder)
{
    VOGL_FUNC_TRACER

    if (uorder < 1 || vorder < 1)
        return 0;

    GLint channels;
    switch (target)
    {
        case GL_MAP2_INDEX:
        case GL_MAP2_TEXTURE_COORD_1:
            channels = 1;
            break;
        case GL_MAP2_TEXTURE_COORD_2:
            channels = 2;
            break;
        case GL_MAP2_NORMAL:
        case GL_MAP2_TEXTURE_COORD_3:
        case GL_MAP2_VERTEX_3:
            channels = 3;
            break;
        case GL_MAP2_COLOR_4:
        case GL_MAP2_TEXTURE_COORD_4:
        case GL_MAP2_VERTEX_4:
            channels = 4;
            break;
        default:
            vogl_warning_printf("unknown GLenum 0x%04X\n", target);
            return 0;
    }

    if (ustride < channels || vstride < channels)
        return 0;

    return channels +
           ustride * (uorder - 1) +
           vstride * (vorder - 1);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_binding_state::save_textures
//   Defining this here rather than in vogl_gl_utils.h with the rest of the class due to
//   dependency issues caused by its use of vogl_context_info
//----------------------------------------------------------------------------------------------------------------------
void vogl_binding_state::save_textures(const vogl_context_info *context_info)
{
    VOGL_FUNC_TRACER

    // TODO: the valid stuff to save depends on the context version, argh
    // TODO: Add GL4 texture types!
    static const GLenum s_texture_targets_with_texbuffer[] =
        {
            GL_ACTIVE_TEXTURE,
            GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_1D_ARRAY,
            GL_TEXTURE_2D_ARRAY, GL_TEXTURE_RECTANGLE, GL_TEXTURE_CUBE_MAP,
            GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BUFFER, GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY
        };

    static const GLenum s_texture_targets[] =
        {
            GL_ACTIVE_TEXTURE,
            GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_1D_ARRAY,
            GL_TEXTURE_2D_ARRAY, GL_TEXTURE_RECTANGLE, GL_TEXTURE_CUBE_MAP,
            GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY
        };

    if (context_info && context_info->supports_extension("GL_EXT_texture_buffer_object"))
    {
        m_bindings.reserve(VOGL_ARRAY_SIZE(s_texture_targets_with_texbuffer) + 8);
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_texture_targets_with_texbuffer); i++)
            save(s_texture_targets_with_texbuffer[i]);
    }
    else
    {
        m_bindings.reserve(VOGL_ARRAY_SIZE(s_texture_targets) + 8);
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_texture_targets); i++)
            save(s_texture_targets[i]);
    }
}
