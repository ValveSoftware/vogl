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

// File: vogl_geranl_context_state.cpp
#include "vogl_common.h"
#include "vogl_general_context_state.h"
#include "vogl_console.h"

#include "gl_pname_defs.h"

// TODO: Indexed versions of glGet's
// TODO: Add GL4 types
// TODO: pixel maps
// TODO: current list mode?

//#define DEBUG_CHECK_FOR_UNPROCESSED_STATES

//----------------------------------------------------------------------------------------------------------------------
// GL get related tables
//----------------------------------------------------------------------------------------------------------------------
struct gl_get_def
{
    GLenum m_enum_val;
    uint32_t m_min_ver;
    uint32_t m_max_ver;
    const char *m_pExtension;
    GLenum m_enum_val_max;
};

static const gl_get_def g_vogl_get_defs[] =
    {
// versions are encoded in bytes: 0xMMmm MM=major mm=minor
#define DEFINE_GL_GET(sym, min_ver, max_ver) \
    {                                        \
        sym, min_ver, max_ver, NULL, GL_NONE \
    }                                        \
    ,
#define DEFINE_GL_GET_EXT(sym, min_ver, max_ver, ext) \
    {                                                 \
        sym, min_ver, max_ver, ext, GL_NONE           \
    }                                                 \
    ,
#define DEFINE_GL_INDEXED_GET(sym, min_ver, max_ver, max_sym) \
    {                                                         \
        sym, min_ver, max_ver, NULL, max_sym                  \
    }                                                         \
    ,
#define DEFINE_GL_INDEXED_GET_EXT(sym, min_ver, max_ver, max_sym, ext) \
    {                                                                  \
        sym, min_ver, max_ver, ext, max_sym                            \
    }                                                                  \
    ,
#include "gl_gets.inc"
#undef DEFINE_GL_GET
    };

#define VOGL_GET_DEF_MAX_SUPPORTED_VERSION VOGL_CREATE_GL_VERSION(4, 3, 0)
#define VOGL_GET_DEF_MAX_VERSION VOGL_CREATE_GL_VERSION(9, 9, 0)

//----------------------------------------------------------------------------------------------------------------------
// client active texture and active texture dependent enums
// From 2.1 docs:
// "When the ARB_multitexture extension is supported, or the GL version is 1.3 or
// greater, the following parameters return the associated value for the active
// texture unit: GL_CURRENT_RASTER_TEXTURE_COORDS, GL_TEXTURE_1D,
// GL_TEXTURE_BINDING_1D, GL_TEXTURE_2D, GL_TEXTURE_BINDING_2D, GL_TEXTURE_3D,
// GL_TEXTURE_BINDING_3D, GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T, GL_TEXTURE_GEN_R,
// GL_TEXTURE_GEN_Q, GL_TEXTURE_MATRIX, and GL_TEXTURE_STACK_DEPTH.
// Likewise, the following parameters return the associated value for the active
// client texture unit:
// GL_TEXTURE_COORD_ARRAY, GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING,
// GL_TEXTURE_COORD_ARRAY_SIZE, GL_TEXTURE_COORD_ARRAY_STRIDE,
// GL_TEXTURE_COORD_ARRAY_TYPE.
//----------------------------------------------------------------------------------------------------------------------
static const GLenum g_vogl_client_active_texture_dependent_enums[] =
    {
        GL_TEXTURE_COORD_ARRAY,
        GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING,
        GL_TEXTURE_COORD_ARRAY_SIZE,
        GL_TEXTURE_COORD_ARRAY_STRIDE,
        GL_TEXTURE_COORD_ARRAY_TYPE,
        GL_TEXTURE_COORD_ARRAY_POINTER
    };
//----------------------------------------------------------------------------------------------------------------------
static const GLenum g_vogl_active_texture_dependent_enums[] =
    {
        // These are limited to the max # of combined texture units
        GL_TEXTURE_1D,
        GL_TEXTURE_BINDING_1D,
        GL_TEXTURE_2D,
        GL_TEXTURE_BINDING_2D,
        GL_TEXTURE_3D,
        GL_TEXTURE_BINDING_3D,
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_BINDING_CUBE_MAP,
        GL_TEXTURE_CUBE_MAP_ARRAY,
        GL_TEXTURE_BINDING_CUBE_MAP_ARRAY,
        GL_TEXTURE_RECTANGLE,
        GL_TEXTURE_BINDING_RECTANGLE,
        GL_TEXTURE_BUFFER,
        GL_TEXTURE_BINDING_BUFFER,
        GL_TEXTURE_1D_ARRAY,
        GL_TEXTURE_BINDING_1D_ARRAY,
        GL_TEXTURE_2D_ARRAY,
        GL_TEXTURE_BINDING_2D_ARRAY,
        GL_TEXTURE_2D_MULTISAMPLE,
        GL_TEXTURE_BINDING_2D_MULTISAMPLE,
        GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
        GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY,
        GL_SAMPLER_BINDING, // needed here for the gets, but not the sets

        // These are limited to the max # of texture units
        GL_CURRENT_RASTER_TEXTURE_COORDS,
        GL_TEXTURE_GEN_S,
        GL_TEXTURE_GEN_T,
        GL_TEXTURE_GEN_R,
        GL_TEXTURE_GEN_Q,
        GL_TEXTURE_MATRIX,
        GL_TRANSPOSE_TEXTURE_MATRIX,
        GL_CURRENT_TEXTURE_COORDS,
        GL_TEXTURE_STACK_DEPTH
    };

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_enum_is_dependent_on_active_texture
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_enum_is_dependent_on_active_texture(GLenum enum_val)
{
    VOGL_FUNC_TRACER

    return (utils::find_value_in_array(enum_val, g_vogl_active_texture_dependent_enums) >= 0);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_enum_is_dependent_on_client_active_texture
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_enum_is_dependent_on_client_active_texture(GLenum enum_val)
{
    VOGL_FUNC_TRACER

    return (utils::find_value_in_array(enum_val, g_vogl_client_active_texture_dependent_enums) >= 0);
}

//----------------------------------------------------------------------------------------------------------------------
// inverse table
//----------------------------------------------------------------------------------------------------------------------
class gl_get_desc
{
public:
    gl_get_desc()
    {
        VOGL_FUNC_TRACER

        init();
    }

    void init()
    {
        VOGL_FUNC_TRACER

        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(g_vogl_get_defs); i++)
            m_gl_enum_to_get_def_index.insert(g_vogl_get_defs[i].m_enum_val, i);
    }

    inline uint32_t get_total() const
    {
        return VOGL_ARRAY_SIZE(g_vogl_get_defs);
    }

    inline GLenum get_enum_val(uint32_t index) const
    {
        return get_desc(index).m_enum_val;
    }
    inline uint32_t get_min_vers(uint32_t index) const
    {
        uint32_t packed_ver = get_desc(index).m_min_ver;
        VOGL_ASSERT(packed_ver <= 0xFF);
        return VOGL_CREATE_GL_VERSION(packed_ver >> 4, packed_ver & 0xF, 0);
    }
    inline uint32_t get_max_vers(uint32_t index) const
    {
        uint32_t packed_ver = get_desc(index).m_max_ver;
        VOGL_ASSERT(packed_ver <= 0xFF);
        return VOGL_CREATE_GL_VERSION(packed_ver >> 4, packed_ver & 0xF, 0);
    }
    inline const char *get_extension(uint32_t index)
    {
        return get_desc(index).m_pExtension;
    }
    inline uint32_t get_enum_val_max(uint32_t index) const
    {
        return get_desc(index).m_enum_val_max;
    }

    inline int find_gl_enum(GLenum enum_val) const
    {
        VOGL_FUNC_TRACER

        if (enum_val > cUINT32_MAX)
            return vogl::cInvalidIndex;

        gl_enum_to_get_def_index_hash_map::const_iterator it(m_gl_enum_to_get_def_index.find(static_cast<uint32_t>(enum_val)));
        if (it == m_gl_enum_to_get_def_index.end())
            return vogl::cInvalidIndex;

        return it->second;
    }

    inline bool is_gl_enum_indexed_by_client_active_texture(GLenum val) const
    {
        VOGL_FUNC_TRACER

        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(g_vogl_client_active_texture_dependent_enums); i++)
            if (g_vogl_client_active_texture_dependent_enums[i] == val)
                return true;
        return false;
    }

private:
    typedef vogl::hash_map<GLenum, uint32_t> gl_enum_to_get_def_index_hash_map;
    gl_enum_to_get_def_index_hash_map m_gl_enum_to_get_def_index;

    const gl_get_def &get_desc(uint32_t index) const
    {
        return g_vogl_get_defs[vogl::math::open_range_check<uint32_t>(index, get_total())];
    }
};

static gl_get_desc& get_gl_get_desc()
{
    static gl_get_desc s_get_desc;
    return s_get_desc;
}

//----------------------------------------------------------------------------------------------------------------------
// struct snapshot_context_info
//----------------------------------------------------------------------------------------------------------------------
class vogl_snapshot_context_info
{
public:
    vogl_snapshot_context_info()
    {
    }

    vogl_snapshot_context_info(const vogl_context_info &context_info)
    {
        init(context_info);
    }

    void init(const vogl_context_info &context_info)
    {
        VOGL_FUNC_TRACER

        m_can_use_glGetBooleani_v = (GL_ENTRYPOINT(glGetBooleani_v) != NULL) && (context_info.get_version() >= VOGL_GL_VERSION_3_0);
        m_can_use_glGetIntegeri_v = (GL_ENTRYPOINT(glGetIntegeri_v) != NULL) && (context_info.get_version() >= VOGL_GL_VERSION_3_0);
        m_can_use_glGetInteger64v = (GL_ENTRYPOINT(glGetInteger64v) != NULL) && ((context_info.get_version() >= VOGL_GL_VERSION_3_2) || context_info.supports_extension("GL_ARB_sync"));
        m_can_use_glGetInteger64i_v = (GL_ENTRYPOINT(glGetInteger64i_v) != NULL) && (context_info.get_version() >= VOGL_GL_VERSION_3_2);
        m_can_use_glGetFloati_v = (GL_ENTRYPOINT(glGetFloati_v) != NULL) && (context_info.get_version() >= VOGL_GL_VERSION_4_1);
        m_can_use_glGetDoublei_v = (GL_ENTRYPOINT(glGetDoublei_v) != NULL) && ((context_info.get_version() >= VOGL_GL_VERSION_4_3) || context_info.supports_extension("GL_ARB_viewport_array"));
        m_can_use_glGetPointerv = (GL_ENTRYPOINT(glGetPointerv) != NULL) && !context_info.is_core_profile();

        m_max_texture_units = 0;
        m_max_texture_coords = 0;

        if (!context_info.is_core_profile())
        {
            GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_TEXTURE_UNITS, &m_max_texture_units);
            GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_TEXTURE_COORDS, &m_max_texture_coords);
        }

        m_max_combined_texture_coords = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_max_combined_texture_coords);

        m_max_draw_buffers = 0;
        if (context_info.get_version() >= VOGL_GL_VERSION_2_0)
        {
            GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_DRAW_BUFFERS, &m_max_draw_buffers);
        }

        m_max_vertex_attribs = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_VERTEX_ATTRIBS, &m_max_vertex_attribs);

        VOGL_CHECK_GL_ERROR;
    }

    bool m_can_use_glGetBooleani_v;
    bool m_can_use_glGetIntegeri_v;
    bool m_can_use_glGetInteger64v;
    bool m_can_use_glGetInteger64i_v;
    bool m_can_use_glGetFloati_v;
    bool m_can_use_glGetDoublei_v;
    bool m_can_use_glGetPointerv;

    GLint m_max_texture_units;
    GLint m_max_texture_coords;
    GLint m_max_combined_texture_coords;
    GLint m_max_draw_buffers;
    GLint m_max_vertex_attribs;
};

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::vogl_general_context_state
//----------------------------------------------------------------------------------------------------------------------
vogl_general_context_state::vogl_general_context_state()
    : vogl_state_vector()
{
    VOGL_FUNC_TRACER
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::restore_buffer_binding
//----------------------------------------------------------------------------------------------------------------------
bool vogl_general_context_state::restore_buffer_binding(GLenum binding_enum, GLenum set_enum, vogl_handle_remapper &remapper) const
{
    VOGL_FUNC_TRACER

    uint32_t buffer = 0;
    if (get(binding_enum, 0, &buffer, 1, false))
    {
        buffer = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, buffer));

        GL_ENTRYPOINT(glBindBuffer)(set_enum, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glBindBuffer)(set_enum, buffer);
        VOGL_CHECK_GL_ERROR;

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::restore_buffer_binding_range
//----------------------------------------------------------------------------------------------------------------------
bool vogl_general_context_state::restore_buffer_binding_range(GLenum binding_enum, GLenum start_enum, GLenum size_enum, GLenum set_enum, uint32_t index, bool indexed_variant, vogl_handle_remapper &remapper) const
{
    VOGL_FUNC_TRACER

    uint64_t start, size = 0;
    uint32_t buffer = 0;
    if (get(binding_enum, index, &buffer, 1, indexed_variant) &&
        get(start_enum, index, &start, 1, indexed_variant) &&
        get(size_enum, index, &size, 1, indexed_variant))
    {
        if (buffer)
        {
            buffer = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, buffer));

            if ((!start) && (!size))
            {
                GL_ENTRYPOINT(glBindBufferBase)(set_enum, index, buffer);
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glBindBufferRange)(set_enum, index, buffer, (GLintptr)start, (GLsizeiptr)size);
                VOGL_CHECK_GL_ERROR;
            }
        }

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::can_snapshot_state
//----------------------------------------------------------------------------------------------------------------------
bool vogl_general_context_state::can_snapshot_state(const vogl_context_info &context_info, vogl_snapshot_context_info &context, uint32_t get_desc_index)
{
    VOGL_FUNC_TRACER

    gl_get_desc& gl_desc = get_gl_get_desc();
    const GLenum enum_val = gl_desc.get_enum_val(get_desc_index);
    const uint32_t min_vers = gl_desc.get_min_vers(get_desc_index);
    const uint32_t max_vers = gl_desc.get_max_vers(get_desc_index);
    const char *pExtension = gl_desc.get_extension(get_desc_index);
    if (pExtension)
    {
        if (!context_info.supports_extension(pExtension))
            return false;
        return true; // If extension is supported, snapshot regardless of GL ver
    }

    VOGL_ASSERT(min_vers >= VOGL_CREATE_GL_VERSION(1, 0, 0));
    VOGL_ASSERT(min_vers < VOGL_CREATE_GL_VERSION(9, 9, 0));
    VOGL_ASSERT(max_vers >= VOGL_CREATE_GL_VERSION(1, 0, 0));
    VOGL_ASSERT(max_vers <= VOGL_CREATE_GL_VERSION(9, 9, 0));

    if (uint32_t(context_info.get_version()) < min_vers)
        return false;
    else if ((context_info.is_core_profile()) && (max_vers < VOGL_GET_DEF_MAX_VERSION))
    {
        if (uint32_t(context_info.get_version()) > max_vers)
            return false;
    }

    if ((enum_val >= GL_DRAW_BUFFER0) && (enum_val <= GL_DRAW_BUFFER15))
    {
        if (enum_val >= static_cast<GLenum>(GL_DRAW_BUFFER0 + context.m_max_draw_buffers))
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::snapshot_state
//----------------------------------------------------------------------------------------------------------------------
bool vogl_general_context_state::snapshot_state(const vogl_context_info &context_info, vogl_snapshot_context_info &context, uint32_t get_desc_index, uint32_t index, bool indexed_variant)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    const GLenum enum_val = get_gl_get_desc().get_enum_val(get_desc_index);

    const char *pName = get_gl_enums().find_name(enum_val);

    int pname_def_index = get_gl_enums().find_pname_def_index(enum_val);
    if (pname_def_index < 0)
    {
        vogl_warning_printf("Unable to find pname def for GL enum %s\n", pName);
        return false;
    }

    const gl_pname_def_t &pname_def = g_gl_pname_defs[pname_def_index];

    VOGL_ASSERT(pname_def.m_gl_enum == enum_val);

    bool has_get = false, has_geti = false;
    if (pname_def.m_pFuncs[0])
    {
        if (!vogl_strcmp(pname_def.m_pFuncs, "glGet"))
            has_get = true;
        else if (!vogl_strcmp(pname_def.m_pFuncs, "glGetI"))
            has_geti = true;
        else if (!vogl_strcmp(pname_def.m_pFuncs, "glGet,glGetI"))
        {
            has_get = true;
            has_geti = true;
        }
        else
        {
            vogl_error_printf("Unrecognized pname func \"%s\" in pname table\n", pname_def.m_pFuncs);
        }
    }

    if (!has_get && !has_geti)
    {
        //printf("! %s\n", pname_def.m_pName);
        return false;
    }

    if ((!indexed_variant) && (!has_get))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((indexed_variant) && (!has_geti))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    vogl_state_data trial_state_data;

    vogl_state_type state_type = static_cast<vogl_state_type>(pname_def.m_type);

    uint32_t n = get_gl_enums().get_pname_count(enum_val);
    if (!n)
        return false;

    switch (state_type)
    {
        case 'B':
        {
            GLboolean *p = trial_state_data.init_and_get_data_ptr<GLboolean>(enum_val, index, n, state_type, indexed_variant);
            if (indexed_variant)
            {
                GL_ENTRYPOINT(glGetBooleani_v)(enum_val, index, p);
            }
            else
            {
                GL_ENTRYPOINT(glGetBooleanv)(enum_val, p);
            }

            break;
        }
        case 'E':
        {
            GLenum *p = trial_state_data.init_and_get_data_ptr<GLenum>(enum_val, index, n, state_type, indexed_variant);
            if (indexed_variant)
            {
                GL_ENTRYPOINT(glGetIntegeri_v)(enum_val, index, reinterpret_cast<GLint *>(p));
            }
            else
            {
                GL_ENTRYPOINT(glGetIntegerv)(enum_val, reinterpret_cast<GLint *>(p));
            }

            if (enum_val == GL_CULL_FACE_MODE)
            {
                switch (*p)
                {
                    case GL_FRONT:
                    case GL_BACK:
                    case GL_FRONT_AND_BACK:
                        break;
                    default:
                    {
                        vogl_error_printf("GL_CULL_FACE_MODE is 0x%X, which is not valid! Forcing it to GL_BACK.\n", *p);
                        *p = GL_BACK;
                        break;
                    }
                }
            }

            break;
        }
        case 'I':
        case 'U':
        {
            int32_t *p = trial_state_data.init_and_get_data_ptr<int32_t>(enum_val, index, n, state_type, indexed_variant);
            VOGL_ASSERT(!p[0]);

            if (indexed_variant)
            {
                GL_ENTRYPOINT(glGetIntegeri_v)(enum_val, index, p);
            }
            else
            {
                GL_ENTRYPOINT(glGetIntegerv)(enum_val, p);
            }

            // Sanity check to find GL_DRAW_INDIRECT_BUFFER_BINDING remapping problem.
            switch (enum_val)
            {
                case GL_ARRAY_BUFFER_BINDING:
                case GL_ELEMENT_ARRAY_BUFFER_BINDING:
                case GL_PIXEL_PACK_BUFFER_BINDING:
                case GL_PIXEL_UNPACK_BUFFER_BINDING:
                case GL_COPY_READ_BUFFER_BINDING:
                case GL_COPY_WRITE_BUFFER_BINDING:
                case GL_DRAW_INDIRECT_BUFFER_BINDING:
                case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
                case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
                case GL_UNIFORM_BUFFER_BINDING:
                case GL_ATOMIC_COUNTER_BUFFER_BINDING:
                case GL_SHADER_STORAGE_BUFFER_BINDING:
                case GL_VERTEX_ARRAY_BUFFER_BINDING:
                case GL_COLOR_ARRAY_BUFFER_BINDING:
                case GL_INDEX_ARRAY_BUFFER_BINDING:
                case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING:
                case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
                case GL_FOG_COORD_ARRAY_BUFFER_BINDING:
                case GL_NORMAL_ARRAY_BUFFER_BINDING:
                case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING:
                case GL_SAMPLER_BINDING:
                case GL_CURRENT_QUERY:
                case GL_CURRENT_PROGRAM:
                case GL_PROGRAM_PIPELINE_BINDING:
                case GL_RENDERBUFFER_BINDING:
                case GL_READ_FRAMEBUFFER_BINDING:
                case GL_DRAW_FRAMEBUFFER_BINDING:
                case GL_VERTEX_ARRAY_BINDING:
                case GL_TEXTURE_BINDING_BUFFER:
                case GL_TEXTURE_BINDING_RECTANGLE:
                case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:
                case GL_TEXTURE_BINDING_CUBE_MAP:
                case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
                case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
                case GL_TEXTURE_BINDING_2D_ARRAY:
                case GL_TEXTURE_BINDING_1D_ARRAY:
                case GL_TEXTURE_BINDING_1D:
                case GL_TEXTURE_BINDING_2D:
                case GL_TEXTURE_BINDING_3D:
                case GL_TEXTURE_BUFFER: // Should eventually be changed to GL_TEXTURE_BUFFER_BINDING, see http://www.khronos.org/bugzilla/show_bug.cgi?id=844
                {
                    int handle = *p;
                    if (handle < 0)
                    {
                        vogl_error_printf("Driver has returned a negative handle (%i) for state %s, slamming it to 0!\n", handle, pname_def.m_pName);
                        *p = 0;
                    }
                    else if (handle > 0xFFFF)
                    {
                        // We know this pname is unreliable on AMD right now.
                        if (enum_val == GL_DRAW_INDIRECT_BUFFER_BINDING)
                        {
                            vogl_error_printf("Driver has returned a probably bogus handle (%i) for state %s, slamming it to 0!\n", handle, pname_def.m_pName);
                            *p = 0;
                        }
                        else
                        {
                            vogl_warning_printf("Driver has returned a potentially bogus handle (%i) for state %s!\n", handle, pname_def.m_pName);
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            break;
        }
        case 'i':
        case 'u':
        {
            if (!context.m_can_use_glGetInteger64v)
                return false;

            GLint64 *p = trial_state_data.init_and_get_data_ptr<GLint64>(enum_val, index, n, state_type, indexed_variant);
            if (indexed_variant)
            {
                GL_ENTRYPOINT(glGetInteger64i_v)(enum_val, index, p);
            }
            else
            {
                GL_ENTRYPOINT(glGetInteger64v)(enum_val, p);
            }

            break;
        }
        case 'F':
        {
            float *p = trial_state_data.init_and_get_data_ptr<float>(enum_val, index, n, state_type, indexed_variant);
            if (indexed_variant)
            {
                GL_ENTRYPOINT(glGetFloati_v)(enum_val, index, p);
            }
            else
            {
                GL_ENTRYPOINT(glGetFloatv)(enum_val, p);
            }
            break;
        }
        case 'D':
        {
            double *p = trial_state_data.init_and_get_data_ptr<double>(enum_val, index, n, state_type, indexed_variant);
            if (indexed_variant)
            {
                GL_ENTRYPOINT(glGetDoublei_v)(enum_val, index, p);
            }
            else
            {
                GL_ENTRYPOINT(glGetDoublev)(enum_val, p);
            }
            break;
        }
        case 'P':
        {
            if (!context.m_can_use_glGetPointerv)
                return false;

            if (indexed_variant)
            {
                // This is a legacy/compat API.
                VOGL_ASSERT_ALWAYS;

                indexed_variant = false;
            }

            if (n > 16)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            void *p[17];
            p[16] = (void *)0xCFCFCFCF;

            GL_ENTRYPOINT(glGetPointerv)(enum_val, p);

            VOGL_ASSERT(p[16] == (void *)0xCFCFCFCF);

            uint64_t *q = trial_state_data.init_and_get_data_ptr<uint64_t>(enum_val, index, n, state_type, indexed_variant);
            for (uint32_t i = 0; i < n; i++)
                q[i] = reinterpret_cast<uint64_t>(p[i]);

            break;
        }
        default:
        {
            vogl_warning_printf("Unknown pname type for GL enum %s\n", pName);
            return false;
        }
    }

    bool gl_get_failed = vogl_check_gl_error();
    if (gl_get_failed)
    {
        vogl_warning_printf("Failed retrieving GL state for GL enum %s\n", pName);
        return false;
    }

    trial_state_data.debug_check();

    if (!insert(trial_state_data))
        vogl_warning_printf("vogl_state_vector::deserialize: Ignoring duplicate state 0x%X index %u\n", trial_state_data.get_enum_val(), trial_state_data.get_index());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::snapshot_active_queries
//----------------------------------------------------------------------------------------------------------------------
static const GLenum g_query_targets[] = { GL_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED, GL_TIME_ELAPSED, GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_PRIMITIVES_GENERATED };

bool vogl_general_context_state::snapshot_active_queries(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    if (context_info.get_version() < VOGL_GL_VERSION_2_0)
        return true;

    //TODO: Add GL4 types

    GLint handle = 0;

    GL_ENTRYPOINT(glGetQueryiv)(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &handle);
    VOGL_CHECK_GL_ERROR;
    if (!insert(vogl_state_data(GL_CURRENT_QUERY, GL_SAMPLES_PASSED, &handle, sizeof(handle), false)))
        return false;

    if (context_info.supports_extension("GL_ARB_occlusion_query2") || (context_info.get_version() >= VOGL_GL_VERSION_3_3))
    {
        handle = 0;
        GL_ENTRYPOINT(glGetQueryiv)(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &handle);
        VOGL_CHECK_GL_ERROR;

        if (!insert(vogl_state_data(GL_CURRENT_QUERY, GL_ANY_SAMPLES_PASSED, &handle, sizeof(handle), false)))
            return false;
    }

    if (context_info.supports_extension("GL_NV_transform_feedback") || (context_info.get_version() >= VOGL_GL_VERSION_3_3))
    {
        handle = 0;
        GL_ENTRYPOINT(glGetQueryiv)(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &handle);
        VOGL_CHECK_GL_ERROR;

        if (!insert(vogl_state_data(GL_CURRENT_QUERY, GL_PRIMITIVES_GENERATED, &handle, sizeof(handle), false)))
            return false;

        handle = 0;
        GL_ENTRYPOINT(glGetQueryiv)(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_CURRENT_QUERY, &handle);
        VOGL_CHECK_GL_ERROR;

        if (!insert(vogl_state_data(GL_CURRENT_QUERY, GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, &handle, sizeof(handle), false)))
            return false;
    }

    if (context_info.supports_extension("GL_ARB_timer_query") || (context_info.get_version() >= VOGL_GL_VERSION_3_3))
    {
        handle = 0;
        GL_ENTRYPOINT(glGetQueryiv)(GL_TIME_ELAPSED, GL_CURRENT_QUERY, &handle);
        VOGL_CHECK_GL_ERROR;

        if (!insert(vogl_state_data(GL_CURRENT_QUERY, GL_TIME_ELAPSED, &handle, sizeof(handle), false)))
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_general_context_state::snapshot
//----------------------------------------------------------------------------------------------------------------------
bool vogl_general_context_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(context_info.is_valid());

    bool prev_gl_error = vogl_check_gl_error();
    VOGL_ASSERT(!prev_gl_error);

    vogl_snapshot_context_info snapshot_context_info(context_info);

    prev_gl_error = vogl_check_gl_error();
    VOGL_ASSERT(!prev_gl_error);

    m_states.clear();

    gl_get_desc& gl_desc = get_gl_get_desc();

    for (uint32_t get_desc_index = 0; get_desc_index < gl_desc.get_total(); get_desc_index++)
    {
        const GLenum enum_val = gl_desc.get_enum_val(get_desc_index);

        const bool is_dependent_on_client_active_texture = vogl_gl_enum_is_dependent_on_client_active_texture(enum_val);
        const bool is_dependent_on_active_texture = vogl_gl_enum_is_dependent_on_active_texture(enum_val);
        if ((is_dependent_on_client_active_texture) || (is_dependent_on_active_texture))
            continue;

        if (!can_snapshot_state(context_info, snapshot_context_info, get_desc_index))
            continue;

        GLenum enum_val_max = gl_desc.get_enum_val_max(get_desc_index);
        if (enum_val_max == GL_NONE)
            snapshot_state(context_info, snapshot_context_info, get_desc_index, 0, false);
        else
        {
            uint32_t max_indices = vogl_get_gl_integer(enum_val_max);
            VOGL_CHECK_GL_ERROR;

            for (uint32_t i = 0; i < max_indices; i++)
                snapshot_state(context_info, snapshot_context_info, get_desc_index, i, true);
        }
    }

    if (!context_info.is_core_profile())
    {
        // client active texture dependent glGet's
        GLint prev_client_active_texture = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &prev_client_active_texture);

        prev_gl_error = vogl_check_gl_error();
        VOGL_ASSERT(!prev_gl_error);

        const uint32_t max_client_texture_coords = snapshot_context_info.m_max_texture_coords;
        VOGL_ASSERT(max_client_texture_coords);

        for (uint32_t texcoord_index = 0; texcoord_index < max_client_texture_coords; texcoord_index++)
        {
            GL_ENTRYPOINT(glClientActiveTexture)(GL_TEXTURE0 + texcoord_index);

            prev_gl_error = vogl_check_gl_error();
            VOGL_ASSERT(!prev_gl_error);

            for (uint32_t get_desc_index = 0; get_desc_index < gl_desc.get_total(); get_desc_index++)
            {
                const GLenum enum_val = gl_desc.get_enum_val(get_desc_index);

                const bool is_dependent_on_client_active_texture = vogl_gl_enum_is_dependent_on_client_active_texture(enum_val);
                if (!is_dependent_on_client_active_texture)
                    continue;

                if (can_snapshot_state(context_info, snapshot_context_info, get_desc_index))
                    snapshot_state(context_info, snapshot_context_info, get_desc_index, texcoord_index, false);
            }
        }

        GL_ENTRYPOINT(glClientActiveTexture)(prev_client_active_texture);

        prev_gl_error = vogl_check_gl_error();
        VOGL_ASSERT(!prev_gl_error);
    }

    // active texture dependent glGet's
    GLint prev_active_texture = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_ACTIVE_TEXTURE, &prev_active_texture);

    prev_gl_error = vogl_check_gl_error();
    VOGL_ASSERT(!prev_gl_error);

    // FIXME: Test on core profiles (that'll be fun)
    const uint32_t max_texture_coords = math::maximum<uint32_t>(snapshot_context_info.m_max_texture_coords, snapshot_context_info.m_max_combined_texture_coords);
    VOGL_ASSERT(max_texture_coords);

    for (uint32_t texcoord_index = 0; texcoord_index < max_texture_coords; texcoord_index++)
    {
        GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0 + texcoord_index);

        prev_gl_error = vogl_check_gl_error();
        VOGL_ASSERT(!prev_gl_error);

        for (uint32_t get_desc_index = 0; get_desc_index < gl_desc.get_total(); get_desc_index++)
        {
            const GLenum enum_val = gl_desc.get_enum_val(get_desc_index);

            const bool is_dependent_on_active_texture = vogl_gl_enum_is_dependent_on_active_texture(enum_val);
            if (!is_dependent_on_active_texture)
                continue;

            // skip the stuff that's limited by the max texture coords
            if ((enum_val == GL_CURRENT_RASTER_TEXTURE_COORDS) ||
                (enum_val == GL_CURRENT_TEXTURE_COORDS) ||
                (enum_val == GL_TEXTURE_GEN_S) ||
                (enum_val == GL_TEXTURE_GEN_T) ||
                (enum_val == GL_TEXTURE_GEN_Q) ||
                (enum_val == GL_TEXTURE_GEN_R) ||
                (enum_val == GL_TEXTURE_MATRIX) ||
                (enum_val == GL_TRANSPOSE_TEXTURE_MATRIX) ||
                (enum_val == GL_TEXTURE_STACK_DEPTH))
            {
                if (static_cast<int>(texcoord_index) >= snapshot_context_info.m_max_texture_coords)
                    continue;
            }

            if (utils::is_in_set<GLenum, GLenum>(enum_val, GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP))
            {
                if (static_cast<int>(texcoord_index) >= snapshot_context_info.m_max_texture_units)
                    continue;
            }

            if (can_snapshot_state(context_info, snapshot_context_info, get_desc_index))
                snapshot_state(context_info, snapshot_context_info, get_desc_index, texcoord_index, false);
        }
    }

    GL_ENTRYPOINT(glActiveTexture)(prev_active_texture);

    snapshot_active_queries(context_info);

    prev_gl_error = vogl_check_gl_error();
    VOGL_ASSERT(!prev_gl_error);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::restore
// TODO: Holy methods of doom, split this up!
//----------------------------------------------------------------------------------------------------------------------
bool vogl_general_context_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, vogl_persistent_restore_state &persistent_state) const
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(context_info.is_valid());

    VOGL_NOTE_UNUSED(context_info);

#ifdef DEBUG_CHECK_FOR_UNPROCESSED_STATES
    vogl::vector<vogl_state_id> processed_states;
    processed_states.reserve(m_states.size());
#define ADD_PROCESSED_STATE(e, i) processed_states.push_back(vogl_state_id(e, i, false));
#define ADD_PROCESSED_STATE_INDEXED_VARIANT(e, i) processed_states.push_back(vogl_state_id(e, i, true));
#else
#define ADD_PROCESSED_STATE(e, i) \
    do                            \
    {                             \
    } while (0)
#define ADD_PROCESSED_STATE_INDEXED_VARIANT(e, i) \
    do                                            \
    {                                             \
    } while (0)
#endif

    GLint prev_client_active_texture = 0;
    if (!context_info.is_core_profile())
    {
        GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &prev_client_active_texture);
    }

    GLint prev_active_texture = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_ACTIVE_TEXTURE, &prev_active_texture);

    GLint prev_array_buffer_binding = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &prev_array_buffer_binding);

    VOGL_CHECK_GL_ERROR;

    for (const_iterator it = begin(); it != end(); ++it)
    {
        const vogl_state_data &state = it->second;

        const GLenum enum_val = state.get_enum_val();
        const uint32_t index = state.get_index();

        if (enum_val == GL_CLIENT_ACTIVE_TEXTURE)
        {
            prev_client_active_texture = state.get_element<int>(0);
            ADD_PROCESSED_STATE(enum_val, index);
            continue;
        }
        else if (enum_val == GL_ACTIVE_TEXTURE)
        {
            prev_active_texture = state.get_element<int>(0);
            ADD_PROCESSED_STATE(enum_val, index);
            continue;
        }
        else if (enum_val == GL_ARRAY_BUFFER_BINDING)
        {
            prev_array_buffer_binding = state.get_element<uint32_t>(0);
            prev_array_buffer_binding = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, prev_array_buffer_binding));
            ADD_PROCESSED_STATE(enum_val, index);
            continue;
        }
        else if (enum_val == GL_SAMPLER_BINDING)
        {
            // sampler objects - put this here to avoid the active texture set
            GLuint sampler_object = state.get_element<GLuint>(0);
            sampler_object = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_SAMPLERS, sampler_object));

            GL_ENTRYPOINT(glBindSampler)(index, sampler_object);
            VOGL_CHECK_GL_ERROR;

            ADD_PROCESSED_STATE(enum_val, index);
            continue;
        }

        if (vogl_gl_enum_is_dependent_on_active_texture(enum_val))
        {
            GL_ENTRYPOINT(glActiveTexture)(GL_TEXTURE0 + index);
            VOGL_CHECK_GL_ERROR;

            ADD_PROCESSED_STATE(enum_val, index);
        }
        else if (vogl_gl_enum_is_dependent_on_client_active_texture(enum_val))
        {
            GL_ENTRYPOINT(glClientActiveTexture)(GL_TEXTURE0 + index);
            VOGL_CHECK_GL_ERROR;

            ADD_PROCESSED_STATE(enum_val, index);
        }

        if ((state.get_data_type() == cSTGLboolean) && (state.get_num_elements() == 1))
        {
            // glEnableClientState/DisableClientState or glEnable/glDisable
            GLboolean boolean_val = state.get_element<GLboolean>(0);

            switch (enum_val)
            {
                case GL_COLOR_ARRAY:
                case GL_EDGE_FLAG_ARRAY:
                case GL_FOG_COORD_ARRAY:
                case GL_INDEX_ARRAY:
                case GL_NORMAL_ARRAY:
                case GL_SECONDARY_COLOR_ARRAY:
                case GL_TEXTURE_COORD_ARRAY:
                case GL_VERTEX_ARRAY:
                {
                    if (boolean_val)
                        GL_ENTRYPOINT(glEnableClientState)(enum_val);
                    else
                        GL_ENTRYPOINT(glDisableClientState)(enum_val);
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                // isolate imaging state
                case GL_COLOR_TABLE:
                case GL_CONVOLUTION_1D:
                case GL_CONVOLUTION_2D:
                case GL_SEPARABLE_2D:
                case GL_HISTOGRAM:
                case GL_MINMAX:
                case GL_POST_COLOR_MATRIX_COLOR_TABLE:
                case GL_POST_CONVOLUTION_COLOR_TABLE:
                {
                    if (context_info.supports_extension("GL_ARB_imaging"))
                    {
                        if (boolean_val)
                            GL_ENTRYPOINT(glEnable)(enum_val);
                        else
                            GL_ENTRYPOINT(glDisable)(enum_val);
                        ADD_PROCESSED_STATE(enum_val, index);
                    }
                    break;
                }
                case GL_ALPHA_TEST:
                case GL_AUTO_NORMAL:
                case GL_BLEND:
                case GL_CLIP_DISTANCE0: // same as CLIP_PLANE0, etc.
                case GL_CLIP_DISTANCE1:
                case GL_CLIP_DISTANCE2:
                case GL_CLIP_DISTANCE3:
                case GL_CLIP_DISTANCE4:
                case GL_CLIP_DISTANCE5:
                case GL_CLIP_DISTANCE6:
                case GL_CLIP_DISTANCE7:
                case GL_COLOR_LOGIC_OP:
                case GL_COLOR_MATERIAL:
                case GL_COLOR_SUM:
                case GL_CULL_FACE:
                case GL_DEBUG_OUTPUT:
                case GL_DEBUG_OUTPUT_SYNCHRONOUS:
                case GL_DEPTH_CLAMP:
                case GL_DEPTH_TEST:
                case GL_DITHER:
                case GL_FOG:
                case GL_FRAMEBUFFER_SRGB:
                case GL_INDEX_LOGIC_OP:
                case GL_LIGHT0:
                case GL_LIGHT1:
                case GL_LIGHT2:
                case GL_LIGHT3:
                case GL_LIGHT4:
                case GL_LIGHT5:
                case GL_LIGHT6:
                case GL_LIGHT7:
                case GL_LIGHTING:
                case GL_LINE_SMOOTH:
                case GL_LINE_STIPPLE:
                case GL_MAP1_COLOR_4:
                case GL_MAP1_INDEX:
                case GL_MAP1_NORMAL:
                case GL_MAP1_TEXTURE_COORD_1:
                case GL_MAP1_TEXTURE_COORD_2:
                case GL_MAP1_TEXTURE_COORD_3:
                case GL_MAP1_TEXTURE_COORD_4:
                case GL_MAP1_VERTEX_3:
                case GL_MAP1_VERTEX_4:
                case GL_MAP2_COLOR_4:
                case GL_MAP2_INDEX:
                case GL_MAP2_NORMAL:
                case GL_MAP2_TEXTURE_COORD_1:
                case GL_MAP2_TEXTURE_COORD_2:
                case GL_MAP2_TEXTURE_COORD_3:
                case GL_MAP2_TEXTURE_COORD_4:
                case GL_MAP2_VERTEX_3:
                case GL_MAP2_VERTEX_4:
                case GL_MULTISAMPLE:
                case GL_NORMALIZE:
                case GL_POINT_SMOOTH:
                case GL_POINT_SPRITE:
                case GL_POLYGON_OFFSET_FILL:
                case GL_POLYGON_OFFSET_LINE:
                case GL_POLYGON_OFFSET_POINT:
                case GL_POLYGON_SMOOTH:
                case GL_POLYGON_STIPPLE:
                case GL_PRIMITIVE_RESTART:
                case GL_PRIMITIVE_RESTART_FIXED_INDEX:
                case GL_PROGRAM_POINT_SIZE: // same as GL_VERTEX_PROGRAM_POINT_SIZE
                case GL_RASTERIZER_DISCARD:
                case GL_RESCALE_NORMAL:
                case GL_SAMPLE_ALPHA_TO_COVERAGE:
                case GL_SAMPLE_ALPHA_TO_ONE:
                case GL_SAMPLE_COVERAGE:
                case GL_SAMPLE_MASK:
                case GL_SAMPLE_SHADING:
                case GL_SCISSOR_TEST:
                case GL_STENCIL_TEST:
                case GL_TEXTURE_1D:
                case GL_TEXTURE_2D:
                case GL_TEXTURE_3D:
                case GL_TEXTURE_CUBE_MAP:
                case GL_TEXTURE_CUBE_MAP_SEAMLESS:
                case GL_TEXTURE_GEN_Q:
                case GL_TEXTURE_GEN_R:
                case GL_TEXTURE_GEN_S:
                case GL_TEXTURE_GEN_T:
                case GL_VERTEX_PROGRAM_TWO_SIDE:
                case GL_VERTEX_PROGRAM_ARB:
                case GL_FRAGMENT_PROGRAM_ARB:
                {
                    if (boolean_val)
                        GL_ENTRYPOINT(glEnable)(enum_val);
                    else
                        GL_ENTRYPOINT(glDisable)(enum_val);
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_EDGE_FLAG:
                {
                    GL_ENTRYPOINT(glEdgeFlag)(boolean_val);
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_LIGHT_MODEL_LOCAL_VIEWER:
                case GL_LIGHT_MODEL_TWO_SIDE:
                {
                    GL_ENTRYPOINT(glLightModeli)(enum_val, boolean_val);
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_MAP_COLOR:
                case GL_MAP_STENCIL:
                {
                    GL_ENTRYPOINT(glPixelTransferi)(enum_val, boolean_val);
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_PACK_LSB_FIRST:
                case GL_PACK_SWAP_BYTES:
                case GL_UNPACK_LSB_FIRST:
                case GL_UNPACK_SWAP_BYTES:
                {
                    GL_ENTRYPOINT(glPixelStorei)(enum_val, boolean_val);
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_COLOR_WRITEMASK:
                case GL_DEPTH_WRITEMASK:
                case GL_SAMPLE_COVERAGE_INVERT:
                case GL_IMAGE_BINDING_LAYERED:
                {
                    // handled below
                    break;
                }
                case GL_INDEX_MODE:
                case GL_RGBA_MODE:
                case GL_DOUBLEBUFFER:
                case GL_STEREO:
                case GL_SHADER_COMPILER:
                {
                    // Not a user changable state
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                default:
                {
                    vogl_debug_printf("FIXME: Don't know how to hande boolean GLenum 0x%04X %s\n", enum_val, get_gl_enums().find_name(enum_val));
                    break;
                }
            }

            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            switch (enum_val)
            {
                case GL_VERTEX_ARRAY_BUFFER_BINDING:
                case GL_COLOR_ARRAY_BUFFER_BINDING:
                case GL_INDEX_ARRAY_BUFFER_BINDING:
                case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING:
                case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
                case GL_FOG_COORD_ARRAY_BUFFER_BINDING:
                case GL_NORMAL_ARRAY_BUFFER_BINDING:
                case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING:
                {
                    // Client side arrays
                    // TODO: Unfortunately, all this crap is tied to VAO state, except for client side arrays.

                    uint32_t client_side_array_index;
                    for (client_side_array_index = 0; client_side_array_index < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS; client_side_array_index++)
                        if (g_vogl_client_side_array_descs[client_side_array_index].m_get_binding == enum_val)
                            break;

                    VOGL_VERIFY(client_side_array_index < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS);

                    const vogl_client_side_array_desc_t &array_desc = g_vogl_client_side_array_descs[client_side_array_index];

                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, binding));

                    int size = 0, stride = 0;
                    GLenum type = 0;
                    void *pPtr = NULL;

                    bool success = get(array_desc.m_get_pointer, index, &pPtr);
                    if ((success) && (!binding))
                    {
                        pPtr = reinterpret_cast<void *>(remapper.remap_vertex_array_ptr(static_cast<vogl_client_side_array_desc_id_t>(client_side_array_index), index, reinterpret_cast<vogl_trace_ptr_value>(pPtr)));
                        ADD_PROCESSED_STATE(array_desc.m_get_pointer, index);
                    }

                    if (array_desc.m_get_size)
                    {
                        if (!get(array_desc.m_get_size, index, &size))
                            success = false;
                        else
                            ADD_PROCESSED_STATE(array_desc.m_get_size, index);
                    }

                    if (!get(array_desc.m_get_stride, index, &stride))
                        success = false;
                    else
                        ADD_PROCESSED_STATE(array_desc.m_get_stride, index);

                    if (array_desc.m_get_type)
                    {
                        if (!get(array_desc.m_get_type, index, &type))
                            success = false;
                        else
                            ADD_PROCESSED_STATE(array_desc.m_get_type, index);
                    }

                    if (success)
                    {
                        GL_ENTRYPOINT(glBindBuffer)(GL_ARRAY_BUFFER, binding);

                        switch (enum_val)
                        {
                            case GL_VERTEX_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glVertexPointer)(size, type, stride, pPtr);
                                break;
                            case GL_COLOR_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glColorPointer)(size, type, stride, pPtr);
                                break;
                            case GL_INDEX_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glIndexPointer)(type, stride, pPtr);
                                break;
                            case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glSecondaryColorPointer)(size, type, stride, pPtr);
                                break;
                            case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glTexCoordPointer)(size, type, stride, pPtr);
                                break;
                            case GL_FOG_COORD_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glFogCoordPointer)(type, stride, pPtr);
                                break;
                            case GL_NORMAL_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glNormalPointer)(type, stride, pPtr);
                                break;
                            case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING:
                                GL_ENTRYPOINT(glEdgeFlagPointer)(stride, pPtr);
                                break;
                        }
                    }

                    VOGL_CHECK_GL_ERROR;

                    ADD_PROCESSED_STATE(enum_val, index);

                    break;
                }
                case GL_TEXTURE_BINDING_1D:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_1D, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_2D:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_3D:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_3D, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_1D_ARRAY:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_1D_ARRAY, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_2D_ARRAY:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D_ARRAY, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D_MULTISAMPLE, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_CUBE_MAP:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_CUBE_MAP, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_CUBE_MAP_ARRAY, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_RECTANGLE:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_RECTANGLE, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
                case GL_TEXTURE_BINDING_BUFFER:
                {
                    uint32_t binding = state.get_element<uint32_t>(0);
                    binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, binding));
                    GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_BUFFER, binding);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, index);
                    break;
                }
            } // switch (enum_val)

        } // if (state.m_data_type == cSTGLboolean)

    } // state_index

    // vao binding
    GLuint vao_binding;
    if (get(GL_VERTEX_ARRAY_BINDING, 0, &vao_binding))
    {
        vao_binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_VERTEX_ARRAYS, vao_binding));
        GL_ENTRYPOINT(glBindVertexArray)(vao_binding);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_VERTEX_ARRAY_BINDING, 0);
    }

    // blend func separate
    GLenum src_rgb, src_alpha;
    GLenum dst_rgb, dst_alpha;
    if (get(GL_BLEND_SRC_RGB, 0, &src_rgb) && get(GL_BLEND_SRC_ALPHA, 0, &src_alpha) && get(GL_BLEND_DST_RGB, 0, &dst_rgb) && get(GL_BLEND_DST_ALPHA, 0, &dst_alpha))
    {
        GL_ENTRYPOINT(glBlendFuncSeparate)(src_rgb, dst_rgb, src_alpha, dst_alpha);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_BLEND_SRC, 0);
        ADD_PROCESSED_STATE(GL_BLEND_DST, 0);
        ADD_PROCESSED_STATE(GL_BLEND_SRC_RGB, 0);
        ADD_PROCESSED_STATE(GL_BLEND_SRC_ALPHA, 0);
        ADD_PROCESSED_STATE(GL_BLEND_DST_RGB, 0);
        ADD_PROCESSED_STATE(GL_BLEND_DST_ALPHA, 0);
    }

    // blend color
    float blend_color[4];
    if (get(GL_BLEND_COLOR, 0, blend_color, 4))
    {
        GL_ENTRYPOINT(glBlendColor)(blend_color[0], blend_color[1], blend_color[2], blend_color[3]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_BLEND_COLOR, 0);
    }

    // blend equation separate
    GLenum blend_equation_rgb = 0, blend_equation_alpha = 0;
    if (get(GL_BLEND_EQUATION_RGB, 0, &blend_equation_rgb) && get(GL_BLEND_EQUATION_ALPHA, 0, &blend_equation_alpha))
    {
        GL_ENTRYPOINT(glBlendEquationSeparate)(blend_equation_rgb, blend_equation_alpha);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_BLEND_EQUATION_RGB, 0);
        ADD_PROCESSED_STATE(GL_BLEND_EQUATION_ALPHA, 0);
    }

    // clear color
    float clear_color[4];
    if (get(GL_COLOR_CLEAR_VALUE, 0, clear_color, 4))
    {
        GL_ENTRYPOINT(glClearColor)(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_COLOR_CLEAR_VALUE, 0);
    }

    // logic op
    GLenum logic_op_mode;
    if (get(GL_LOGIC_OP_MODE, 0, &logic_op_mode))
    {
        GL_ENTRYPOINT(glLogicOp)(logic_op_mode);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_LOGIC_OP_MODE, 0);
    }

    // color mask
    bool color_mask[4];
    if (get(GL_COLOR_WRITEMASK, 0, color_mask, 4))
    {
        GL_ENTRYPOINT(glColorMask)(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_COLOR_WRITEMASK, 0);
    }

    // Restore indexed color masks (we've already restored the global state, which sets all the indexed states)
    for (uint32_t i = 0; i < context_info.get_max_draw_buffers(); i++)
    {
        if (get(GL_COLOR_WRITEMASK, i, color_mask, 4, true))
        {
            GL_ENTRYPOINT(glColorMaski)(i, color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
            VOGL_CHECK_GL_ERROR;
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_COLOR_WRITEMASK, i);
        }
    }

    // cull face mode
    GLenum cull_face_mode;
    if (get(GL_CULL_FACE_MODE, 0, &cull_face_mode))
    {
        GL_ENTRYPOINT(glCullFace)(cull_face_mode);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_CULL_FACE_MODE, 0);
    }

    // front face
    GLenum front_face;
    if (get(GL_FRONT_FACE, 0, &front_face))
    {
        GL_ENTRYPOINT(glFrontFace)(front_face);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_FRONT_FACE, 0);
    }

    // depth clear value
    double depth_clear_val;
    if (get(GL_DEPTH_CLEAR_VALUE, 0, &depth_clear_val))
    {
        GL_ENTRYPOINT(glClearDepth)(depth_clear_val);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_DEPTH_CLEAR_VALUE, 0);
    }

    // min sample shading
    float min_sample_shading;
    if (get(GL_MIN_SAMPLE_SHADING_VALUE, 0, &min_sample_shading))
    {
        GL_ENTRYPOINT(glMinSampleShading)(min_sample_shading);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_MIN_SAMPLE_SHADING_VALUE, 0);
    }

    // depth func
    GLenum depth_func;
    if (get(GL_DEPTH_FUNC, 0, &depth_func))
    {
        GL_ENTRYPOINT(glDepthFunc)(depth_func);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_DEPTH_FUNC, 0);
    }

    // depth range
    double depth_range[2];
    if (get(GL_DEPTH_RANGE, 0, depth_range, 2))
    {
        GL_ENTRYPOINT(glDepthRange)(depth_range[0], depth_range[1]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_DEPTH_RANGE, 0);
    }

    // depth mask
    bool depth_mask;
    if (get(GL_DEPTH_WRITEMASK, 0, &depth_mask))
    {
        GL_ENTRYPOINT(glDepthMask)(depth_mask != 0);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_DEPTH_WRITEMASK, 0);
    }

    // draw framebuffer binding
    uint32_t draw_framebuffer_binding = 0;
    if (get(GL_DRAW_FRAMEBUFFER_BINDING, 0, &draw_framebuffer_binding))
    {
        draw_framebuffer_binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_FRAMEBUFFERS, draw_framebuffer_binding));

        GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, draw_framebuffer_binding);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_DRAW_FRAMEBUFFER_BINDING, 0);
    }

    // read framebuffer binding
    uint32_t read_framebuffer_binding = 0;
    if (get(GL_READ_FRAMEBUFFER_BINDING, 0, &read_framebuffer_binding))
    {
        read_framebuffer_binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_FRAMEBUFFERS, read_framebuffer_binding));

        GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, read_framebuffer_binding);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_READ_FRAMEBUFFER_BINDING, 0);
    }

    // draw buffer
    GLenum draw_buffer;
    if (get(GL_DRAW_BUFFER, 0, &draw_buffer))
    {
        GL_ENTRYPOINT(glDrawBuffer)(draw_buffer);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_DRAW_BUFFER, 0);
    }

    // draw buffers
    const uint32_t MAX_DRAW_BUFFERS = 16;

    GLenum draw_buffers[MAX_DRAW_BUFFERS];
    utils::zero_object(draw_buffers);

    uint32_t draw_buffer_index;
    for (draw_buffer_index = 0; draw_buffer_index < MAX_DRAW_BUFFERS; draw_buffer_index++)
    {
        if (!get(GL_DRAW_BUFFER0 + draw_buffer_index, 0, &draw_buffers[draw_buffer_index]))
            break;

        ADD_PROCESSED_STATE(GL_DRAW_BUFFER0 + draw_buffer_index, 0);
    }

    if (draw_buffer_index)
    {
        int num_actual_draw_buffers;
        for (num_actual_draw_buffers = MAX_DRAW_BUFFERS - 1; num_actual_draw_buffers >= 0; num_actual_draw_buffers--)
            if (draw_buffers[num_actual_draw_buffers])
                break;

        VOGL_ASSERT(num_actual_draw_buffers >= 0);
        num_actual_draw_buffers++;

        if (num_actual_draw_buffers == 1)
        {
            GL_ENTRYPOINT(glDrawBuffer)(draw_buffers[0]);
            VOGL_CHECK_GL_ERROR;
        }
        else
        {
            for (int i = 0; i < num_actual_draw_buffers; i++)
            {
                VOGL_ASSERT((utils::is_not_in_set<GLenum, GLenum>(draw_buffers[i], GL_FRONT, GL_BACK, GL_LEFT, GL_RIGHT, GL_FRONT_AND_BACK)));
            }

            GL_ENTRYPOINT(glDrawBuffers)(num_actual_draw_buffers, draw_buffers);
            VOGL_CHECK_GL_ERROR;
        }
    }

    // read buffer
    GLenum read_buffer = 0;
    if (get(GL_READ_BUFFER, 0, &read_buffer))
    {
        GL_ENTRYPOINT(glReadBuffer)(read_buffer);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_READ_BUFFER, 0);
    }

    // renderbuffer binding
    uint32_t renderbuffer_binding = 0;
    if (get(GL_RENDERBUFFER_BINDING, 0, &renderbuffer_binding))
    {
        renderbuffer_binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_RENDER_BUFFERS, renderbuffer_binding));

        GL_ENTRYPOINT(glBindRenderbuffer)(GL_RENDERBUFFER, renderbuffer_binding);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_RENDERBUFFER_BINDING, 0);
    }

    // line width
    float line_width;
    if (get(GL_LINE_WIDTH, 0, &line_width))
    {
        GL_ENTRYPOINT(glLineWidth)(line_width);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_LINE_WIDTH, 0);
    }

    // point fade threshold
    float point_fade_threshold_size;
    if (get(GL_POINT_FADE_THRESHOLD_SIZE, 0, &point_fade_threshold_size))
    {
        GL_ENTRYPOINT(glPointParameterf)(GL_POINT_FADE_THRESHOLD_SIZE, point_fade_threshold_size);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POINT_FADE_THRESHOLD_SIZE, 0);
    }

    // point dist atten
    float point_distance_atten[3];
    if (get(GL_POINT_DISTANCE_ATTENUATION, 0, point_distance_atten, 3))
    {
        GL_ENTRYPOINT(glPointParameterfv)(GL_POINT_DISTANCE_ATTENUATION, point_distance_atten);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POINT_DISTANCE_ATTENUATION, 0);
    }

    // point sprite coord origin
    GLenum point_sprite_coord_origin = 0;
    if (get(GL_POINT_SPRITE_COORD_ORIGIN, 0, &point_sprite_coord_origin))
    {
        GL_ENTRYPOINT(glPointParameteri)(GL_POINT_SPRITE_COORD_ORIGIN, point_sprite_coord_origin);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POINT_SPRITE_COORD_ORIGIN, 0);
    }

    // point size min
    float point_size_min;
    if (get(GL_POINT_SIZE_MIN, 0, &point_size_min))
    {
        GL_ENTRYPOINT(glPointParameterf)(GL_POINT_SIZE_MIN, point_size_min);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POINT_SIZE_MIN, 0);
    }

    // point size max
    float point_size_max;
    if (get(GL_POINT_SIZE_MAX, 0, &point_size_max))
    {
        GL_ENTRYPOINT(glPointParameterf)(GL_POINT_SIZE_MAX, point_size_max);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POINT_SIZE_MAX, 0);
    }

    // provoking vertex
    GLenum provoking_vertex = 0;
    if (get(GL_PROVOKING_VERTEX, 0, &provoking_vertex))
    {
        GL_ENTRYPOINT(glProvokingVertex)(provoking_vertex);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_PROVOKING_VERTEX, 0);
    }

    // point size
    float point_size;
    if (get(GL_POINT_SIZE, 0, &point_size))
    {
        GL_ENTRYPOINT(glPointSize)(point_size);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POINT_SIZE, 0);
    }

    // polygon offset
    float polygon_offset_factor, polygon_offset_units;
    if (get(GL_POLYGON_OFFSET_FACTOR, 0, &polygon_offset_factor) && get(GL_POLYGON_OFFSET_UNITS, 0, &polygon_offset_units))
    {
        GL_ENTRYPOINT(glPolygonOffset)(polygon_offset_factor, polygon_offset_units);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POLYGON_OFFSET_FACTOR, 0);
        ADD_PROCESSED_STATE(GL_POLYGON_OFFSET_UNITS, 0);
    }

    // polygon mode
    GLenum polygon_mode[2] = { 0, 0 };
    if (get(GL_POLYGON_MODE, 0, polygon_mode, 2))
    {
        GL_ENTRYPOINT(glPolygonMode)(GL_FRONT, polygon_mode[0]);
        GL_ENTRYPOINT(glPolygonMode)(GL_BACK, polygon_mode[1]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_POLYGON_MODE, 0);
    }

    // sample coverage
    float sample_coverage;
    bool sample_invert;
    if (get(GL_SAMPLE_COVERAGE_VALUE, 0, &sample_coverage) && get(GL_SAMPLE_COVERAGE_INVERT, 0, &sample_invert))
    {
        GL_ENTRYPOINT(glSampleCoverage)(sample_coverage, sample_invert);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_SAMPLE_COVERAGE_VALUE, 0);
        ADD_PROCESSED_STATE(GL_SAMPLE_COVERAGE_INVERT, 0);
    }

    // viewport
    int viewport[4] = { 0, 0, 0, 0 };
    if (get(GL_VIEWPORT, 0, viewport, 4))
    {
        GL_ENTRYPOINT(glViewport)(viewport[0], viewport[1], viewport[2], viewport[3]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_VIEWPORT, 0);
    }

    // scissor
    int scissor[4] = { 0, 0, 0, 0 };
    if (get(GL_SCISSOR_BOX, 0, scissor, 4))
    {
        GL_ENTRYPOINT(glScissor)(scissor[0], scissor[1], scissor[2], scissor[3]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_SCISSOR_BOX, 0);
    }

    // stencil op separate
    GLenum stencil_fail = 0, stencil_dp_pass = 0, stencil_dp_fail = 0;
    if (get(GL_STENCIL_FAIL, 0, &stencil_fail) && get(GL_STENCIL_PASS_DEPTH_PASS, 0, &stencil_dp_pass) && get(GL_STENCIL_PASS_DEPTH_FAIL, 0, &stencil_dp_fail))
    {
        GL_ENTRYPOINT(glStencilOpSeparate)(GL_FRONT, stencil_fail, stencil_dp_fail, stencil_dp_pass);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_FAIL, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_PASS_DEPTH_PASS, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_PASS_DEPTH_FAIL, 0);
    }

    if (get(GL_STENCIL_BACK_FAIL, 0, &stencil_fail) && get(GL_STENCIL_BACK_PASS_DEPTH_PASS, 0, &stencil_dp_pass) && get(GL_STENCIL_BACK_PASS_DEPTH_FAIL, 0, &stencil_dp_fail))
    {
        GL_ENTRYPOINT(glStencilOpSeparate)(GL_BACK, stencil_fail, stencil_dp_fail, stencil_dp_pass);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_FAIL, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_PASS_DEPTH_PASS, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_PASS_DEPTH_FAIL, 0);
    }

    // stencil func separate
    GLenum stencil_func = 0;
    GLint stencil_ref = 0;
    GLuint stencil_mask = 0;

    if (get(GL_STENCIL_FUNC, 0, &stencil_func) && get(GL_STENCIL_REF, 0, &stencil_ref) && get(GL_STENCIL_VALUE_MASK, 0, &stencil_mask))
    {
        GL_ENTRYPOINT(glStencilFuncSeparate)(GL_FRONT, stencil_func, stencil_ref, stencil_mask);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_FUNC, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_REF, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_VALUE_MASK, 0);
    }

    if (get(GL_STENCIL_BACK_FUNC, 0, &stencil_func) && get(GL_STENCIL_BACK_REF, 0, &stencil_ref) && get(GL_STENCIL_BACK_VALUE_MASK, 0, &stencil_mask))
    {
        GL_ENTRYPOINT(glStencilFuncSeparate)(GL_BACK, stencil_func, stencil_ref, stencil_mask);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_FUNC, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_REF, 0);
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_VALUE_MASK, 0);
    }

    // stencil writemask separate
    GLuint stencil_writemask = 0;
    if (get(GL_STENCIL_WRITEMASK, 0, &stencil_writemask))
    {
        GL_ENTRYPOINT(glStencilMaskSeparate)(GL_FRONT, stencil_writemask);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_WRITEMASK, 0);
    }

    if (get(GL_STENCIL_BACK_WRITEMASK, 0, &stencil_writemask))
    {
        GL_ENTRYPOINT(glStencilMaskSeparate)(GL_BACK, stencil_writemask);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_BACK_WRITEMASK, 0);
    }

    GLenum color_material_face, color_material_parameter;
    if (get(GL_COLOR_MATERIAL_FACE, 0, &color_material_face) && get(GL_COLOR_MATERIAL_PARAMETER, 0, &color_material_parameter))
    {
        GL_ENTRYPOINT(glColorMaterial)(color_material_face, color_material_parameter);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_COLOR_MATERIAL_FACE, 0);
        ADD_PROCESSED_STATE(GL_COLOR_MATERIAL_PARAMETER, 0);
    }

    // buffer bindings
    restore_buffer_binding(GL_COPY_READ_BUFFER_BINDING, GL_COPY_READ_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_COPY_READ_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_COPY_WRITE_BUFFER_BINDING, GL_COPY_WRITE_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_COPY_WRITE_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_DRAW_INDIRECT_BUFFER_BINDING, GL_DRAW_INDIRECT_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_DRAW_INDIRECT_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_DISPATCH_INDIRECT_BUFFER_BINDING, GL_DISPATCH_INDIRECT_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_DISPATCH_INDIRECT_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_PIXEL_PACK_BUFFER_BINDING, GL_PIXEL_PACK_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_PIXEL_PACK_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_PIXEL_UNPACK_BUFFER_BINDING, GL_PIXEL_UNPACK_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_PIXEL_UNPACK_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_ELEMENT_ARRAY_BUFFER_BINDING, GL_ELEMENT_ARRAY_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_ELEMENT_ARRAY_BUFFER_BINDING, 0);

    // Not GL_TEXTURE_BUFFER_BINDING due to this bug: http://www.khronos.org/bugzilla/show_bug.cgi?id=844
    restore_buffer_binding(GL_TEXTURE_BUFFER, GL_TEXTURE_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_TEXTURE_BUFFER, 0);

    // restore transform feedback targets
    for (uint32_t i = 0; i < context_info.get_max_transform_feedback_separate_attribs(); i++)
    {
        restore_buffer_binding_range(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, GL_TRANSFORM_FEEDBACK_BUFFER_START, GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, GL_TRANSFORM_FEEDBACK_BUFFER, i, true, remapper);
        ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i);
        ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_TRANSFORM_FEEDBACK_BUFFER_START, i);
        ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i);
    }

    restore_buffer_binding(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, GL_TRANSFORM_FEEDBACK_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0);

    // restore uniform buffer binding target, and the indexed variants
    for (uint32_t i = 0; i < context_info.get_max_uniform_buffer_bindings(); i++)
    {
        restore_buffer_binding_range(GL_UNIFORM_BUFFER_BINDING, GL_UNIFORM_BUFFER_START, GL_UNIFORM_BUFFER_SIZE, GL_UNIFORM_BUFFER, i, true, remapper);
        ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_UNIFORM_BUFFER_BINDING, i);
        ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_UNIFORM_BUFFER_START, i);
        ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_UNIFORM_BUFFER_SIZE, i);
    }

    restore_buffer_binding(GL_UNIFORM_BUFFER_BINDING, GL_UNIFORM_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_UNIFORM_BUFFER_BINDING, 0);

    // Restore indexed blending state (we've already restored the global state, which sets all the indexed states)
    for (uint32_t i = 0; i < context_info.get_max_draw_buffers(); i++)
    {
        GLint enabled = 0;
        if (get(GL_BLEND, i, &enabled, 1, true))
        {
            if (enabled)
            {
                GL_ENTRYPOINT(glEnablei)(GL_BLEND, i);
                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                GL_ENTRYPOINT(glDisablei)(GL_BLEND, i);
                VOGL_CHECK_GL_ERROR;
            }
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_BLEND, i);
        }
    }

    // TODO: these GL4 guys have indexed and offset/size variants
    restore_buffer_binding(GL_ATOMIC_COUNTER_BUFFER_BINDING, GL_ATOMIC_COUNTER_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_ATOMIC_COUNTER_BUFFER_BINDING, 0);

    restore_buffer_binding(GL_SHADER_STORAGE_BUFFER_BINDING, GL_SHADER_STORAGE_BUFFER, remapper);
    ADD_PROCESSED_STATE(GL_SHADER_STORAGE_BUFFER_BINDING, 0);

    // pixel transfer settings
    static const GLenum s_pixel_transfer_pnames[] =
        {
            GL_INDEX_SHIFT, GL_INDEX_OFFSET, GL_RED_SCALE, GL_GREEN_SCALE, GL_BLUE_SCALE, GL_ALPHA_SCALE, GL_DEPTH_SCALE,
            GL_RED_BIAS, GL_GREEN_BIAS, GL_BLUE_BIAS, GL_ALPHA_BIAS, GL_DEPTH_BIAS, GL_POST_COLOR_MATRIX_RED_SCALE,
            GL_POST_COLOR_MATRIX_GREEN_SCALE, GL_POST_COLOR_MATRIX_BLUE_SCALE, GL_POST_COLOR_MATRIX_ALPHA_SCALE,
            GL_POST_COLOR_MATRIX_RED_BIAS, GL_POST_COLOR_MATRIX_GREEN_BIAS, GL_POST_COLOR_MATRIX_BLUE_BIAS,
            GL_POST_COLOR_MATRIX_ALPHA_BIAS, GL_POST_CONVOLUTION_RED_SCALE, GL_POST_CONVOLUTION_GREEN_SCALE,
            GL_POST_CONVOLUTION_BLUE_SCALE, GL_POST_CONVOLUTION_ALPHA_SCALE, GL_POST_CONVOLUTION_RED_BIAS,
            GL_POST_CONVOLUTION_GREEN_BIAS, GL_POST_CONVOLUTION_BLUE_BIAS, GL_POST_CONVOLUTION_ALPHA_BIAS
        };

    uint32_t array_size = VOGL_ARRAY_SIZE(s_pixel_transfer_pnames);
    
    if (!context_info.supports_extension("GL_ARB_imaging"))
        array_size -= 16;  // skip enums from GL_POST_COLOR_MATRIX_RED_SCALE

    for (uint32_t i = 0; i < array_size; i++)
    {
        GLenum enum_val = s_pixel_transfer_pnames[i];

        int pname_def_index = get_gl_enums().find_pname_def_index(enum_val);
        if (pname_def_index < 0)
        {
            vogl_warning_printf("Unable to find pname def for GL enum %s\n", get_gl_enums().find_name(enum_val));
            continue;
        }

        const gl_pname_def_t &pname_def = g_gl_pname_defs[pname_def_index];

        VOGL_VERIFY(pname_def.m_count == 1);

        if (pname_def.m_type == cSTInt32)
        {
            int val;
            if (get(enum_val, 0, &val))
            {
                GL_ENTRYPOINT(glPixelTransferi)(enum_val, val);
                VOGL_CHECK_GL_ERROR;
                ADD_PROCESSED_STATE(enum_val, 0);
            }
        }
        else if (pname_def.m_type == cSTFloat)
        {
            float val;
            if (get(enum_val, 0, &val))
            {
                GL_ENTRYPOINT(glPixelTransferf)(enum_val, val);
                VOGL_CHECK_GL_ERROR;
                ADD_PROCESSED_STATE(enum_val, 0);
            }
        }
        else
        {
            VOGL_VERIFY(0);
        }
    }

    // fog states
    static const GLenum s_fog_pnames[] = { GL_FOG_MODE, GL_FOG_DENSITY, GL_FOG_START, GL_FOG_END, GL_FOG_INDEX, GL_FOG_COLOR, GL_FOG_COORD_SRC };

    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_fog_pnames); i++)
    {
        GLenum enum_val = s_fog_pnames[i];

        int pname_def_index = get_gl_enums().find_pname_def_index(enum_val);
        if (pname_def_index < 0)
        {
            vogl_warning_printf("Unable to find pname def for GL enum %s\n", get_gl_enums().find_name(enum_val));
            continue;
        }

        const gl_pname_def_t &pname_def = g_gl_pname_defs[pname_def_index];

        switch (pname_def.m_type)
        {
            case cSTFloat:
            {
                VOGL_ASSERT(pname_def.m_count <= 4);
                float v[4];
                if (get(enum_val, 0, v, 4))
                {
                    if (pname_def.m_count > 1)
                        GL_ENTRYPOINT(glFogfv)(enum_val, v);
                    else
                        GL_ENTRYPOINT(glFogf)(enum_val, v[0]);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, 0);
                }
                break;
            }
            case cSTInt32:
            {
                VOGL_VERIFY(pname_def.m_count == 1);
                int v;
                if (get(enum_val, 0, &v))
                {
                    GL_ENTRYPOINT(glFogi)(enum_val, v);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, 0);
                }
                break;
            }
            case cSTGLenum:
            {
                VOGL_VERIFY(pname_def.m_count == 1);
                GLenum v;
                if (get(enum_val, 0, &v))
                {
                    GL_ENTRYPOINT(glFogi)(enum_val, v);
                    VOGL_CHECK_GL_ERROR;
                    ADD_PROCESSED_STATE(enum_val, 0);
                }
                break;
            }
            default:
            {
                VOGL_VERIFY(0);
                break;
            }
        }
    }

    // hints
    static GLenum s_hint_pnames[] = { GL_GENERATE_MIPMAP_HINT, GL_FOG_HINT, GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_LINE_SMOOTH_HINT, GL_POLYGON_SMOOTH_HINT, GL_TEXTURE_COMPRESSION_HINT, GL_PERSPECTIVE_CORRECTION_HINT, GL_POINT_SMOOTH_HINT };
    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_hint_pnames); i++)
    {
        GLenum enum_val = s_hint_pnames[i];
        GLenum val;
        if (get(enum_val, 0, &val))
        {
            GL_ENTRYPOINT(glHint)(enum_val, val);
            VOGL_CHECK_GL_ERROR;
            ADD_PROCESSED_STATE(enum_val, 0);
        }
    }

    // primitive restart index
    uint32_t prim_restart_index;
    if (get(GL_PRIMITIVE_RESTART_INDEX, 0, &prim_restart_index))
    {
        GL_ENTRYPOINT(glPrimitiveRestartIndex)(prim_restart_index);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_PRIMITIVE_RESTART_INDEX, 0);
    }

    // alpha func
    GLenum alpha_func;
    float alpha_ref;
    if (get(GL_ALPHA_TEST_FUNC, 0, &alpha_func) && get(GL_ALPHA_TEST_REF, 0, &alpha_ref))
    {
        GL_ENTRYPOINT(glAlphaFunc)(alpha_func, alpha_ref);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_ALPHA_TEST_FUNC, 0);
        ADD_PROCESSED_STATE(GL_ALPHA_TEST_REF, 0);
    }

    // clear index value
    int clear_index;
    if (get(GL_INDEX_CLEAR_VALUE, 0, &clear_index))
    {
        GL_ENTRYPOINT(glClearIndex)(static_cast<float>(clear_index));
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_INDEX_CLEAR_VALUE, 0);
    }

    // index writemask
    int index_writemask;
    if (get(GL_INDEX_WRITEMASK, 0, &index_writemask))
    {
        GL_ENTRYPOINT(glIndexMask)(index_writemask);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_INDEX_WRITEMASK, 0);
    }

    // line stipple
    int line_stipple_pattern, line_stipple_repeat;
    if (get(GL_LINE_STIPPLE_PATTERN, 0, &line_stipple_pattern) && get(GL_LINE_STIPPLE_REPEAT, 0, &line_stipple_repeat))
    {
        GL_ENTRYPOINT(glLineStipple)(line_stipple_repeat, line_stipple_pattern);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_LINE_STIPPLE_PATTERN, 0);
        ADD_PROCESSED_STATE(GL_LINE_STIPPLE_REPEAT, 0);
    }

    // list base
    int list_base;
    if (get(GL_LIST_BASE, 0, &list_base))
    {
        GL_ENTRYPOINT(glListBase)(list_base);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_LIST_BASE, 0);
    }

    // matrix mode
    GLenum matrix_mode;
    if (get(GL_MATRIX_MODE, 0, &matrix_mode))
    {
        GL_ENTRYPOINT(glMatrixMode)(matrix_mode);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_MATRIX_MODE, 0);
    }

    // shade model
    GLenum shade_model;
    if (get(GL_SHADE_MODEL, 0, &shade_model))
    {
        GL_ENTRYPOINT(glShadeModel)(shade_model);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_SHADE_MODEL, 0);
    }

    // pixel zoom
    float zoom_x, zoom_y;
    if (get(GL_ZOOM_X, 0, &zoom_x) && get(GL_ZOOM_Y, 0, &zoom_y))
    {
        GL_ENTRYPOINT(glPixelZoom)(zoom_x, zoom_y);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_ZOOM_X, 0);
        ADD_PROCESSED_STATE(GL_ZOOM_Y, 0);
    }

    // stencil clear value
    int stencil_clear_value;
    if (get(GL_STENCIL_CLEAR_VALUE, 0, &stencil_clear_value))
    {
        GL_ENTRYPOINT(glClearStencil)(stencil_clear_value);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_STENCIL_CLEAR_VALUE, 0);
    }

    // pixel store
    static const GLenum s_pixel_store_pnames[] =
        {
            GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_IMAGES,
            GL_PACK_ALIGNMENT, GL_PACK_IMAGE_HEIGHT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_IMAGES, GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_ROWS
        };

    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_pixel_store_pnames); i++)
    {
        GLenum enum_val = s_pixel_store_pnames[i];

        int pname_def_index = get_gl_enums().find_pname_def_index(enum_val);
        if (pname_def_index < 0)
        {
            vogl_warning_printf("Unable to find pname def for GL enum %s\n", get_gl_enums().find_name(enum_val));
            continue;
        }

        const gl_pname_def_t &pname_def = g_gl_pname_defs[pname_def_index];

        VOGL_VERIFY((pname_def.m_type == cSTInt32) && (pname_def.m_count == 1));

        int val;
        if (get(enum_val, 0, &val))
        {
            GL_ENTRYPOINT(glPixelStorei)(enum_val, val);
            VOGL_CHECK_GL_ERROR;
            ADD_PROCESSED_STATE(enum_val, 0);
        }
    }

    // program pipeline binding
    uint32_t program_pipeline_binding;
    if (get(GL_PROGRAM_PIPELINE_BINDING, 0, &program_pipeline_binding))
    {
        // is this correct?
        program_pipeline_binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_PIPELINES, program_pipeline_binding));
        GL_ENTRYPOINT(glBindProgramPipeline)(program_pipeline_binding);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_PROGRAM_PIPELINE_BINDING, 0);
    }

    // accum clear value
    float accum_clear_value[4];
    if (get(GL_ACCUM_CLEAR_VALUE, 0, accum_clear_value, 4))
    {
        GL_ENTRYPOINT(glClearAccum)(accum_clear_value[0], accum_clear_value[1], accum_clear_value[2], accum_clear_value[3]);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_ACCUM_CLEAR_VALUE, 0);
    }

    float light_model_ambient[4];
    if (get(GL_LIGHT_MODEL_AMBIENT, 0, light_model_ambient, 4))
    {
        GL_ENTRYPOINT(glLightModelfv)(GL_LIGHT_MODEL_AMBIENT, light_model_ambient);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_LIGHT_MODEL_AMBIENT, 0);
    }

    GLenum light_model_color_control;
    if (get(GL_LIGHT_MODEL_COLOR_CONTROL, 0, &light_model_color_control))
    {
        GL_ENTRYPOINT(glLightModeli)(GL_LIGHT_MODEL_COLOR_CONTROL, light_model_color_control);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_LIGHT_MODEL_COLOR_CONTROL, 0);
    }

    // current program
    uint32_t cur_trace_program;
    if (get(GL_CURRENT_PROGRAM, 0, &cur_trace_program))
    {
        uint32_t cur_program = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_PROGRAMS, cur_trace_program));
        GL_ENTRYPOINT(glUseProgram)(cur_program);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_CURRENT_PROGRAM, 0);
    }

    int attrib_stack_depth;
    if (get(GL_ATTRIB_STACK_DEPTH, 0, &attrib_stack_depth) && attrib_stack_depth)
    {
        vogl_warning_printf("Attribute stack is not empty and cannot be restored\n");
        ADD_PROCESSED_STATE(GL_ATTRIB_STACK_DEPTH, 0);
    }

    int client_attrib_stack_depth;
    if (get(GL_CLIENT_ATTRIB_STACK_DEPTH, 0, &client_attrib_stack_depth) && client_attrib_stack_depth)
    {
        vogl_warning_printf("Client attribute stack is not empty and cannot be restored\n");
        ADD_PROCESSED_STATE(GL_CLIENT_ATTRIB_STACK_DEPTH, 0);
    }

    int selection_buffer_size = 0;
    if (get(GL_SELECTION_BUFFER_SIZE, 0, &selection_buffer_size) && (selection_buffer_size))
    {
        if ((!persistent_state.m_pSelect_buffer) || (!persistent_state.m_pSelect_buffer->try_resize(selection_buffer_size)))
        {
            vogl_warning_printf("Unable to restore selection buffer\n");
        }
        else
        {
            GL_ENTRYPOINT(glSelectBuffer)(selection_buffer_size, persistent_state.m_pSelect_buffer->get_ptr());
            VOGL_CHECK_GL_ERROR;
        }
    }

    int name_stack_depth;
    if (get(GL_NAME_STACK_DEPTH, 0, &name_stack_depth) && name_stack_depth)
    {
        GL_ENTRYPOINT(glRenderMode)(GL_SELECT);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glInitNames)();
        VOGL_CHECK_GL_ERROR;

        for (int i = 0; i < name_stack_depth; i++)
            GL_ENTRYPOINT(glPushName)(i + 1); // push anything

        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glRenderMode)(GL_RENDER);
        VOGL_CHECK_GL_ERROR;

        ADD_PROCESSED_STATE(GL_NAME_STACK_DEPTH, 0);
    }

    // render mode
    GLenum render_mode;
    if (get(GL_RENDER_MODE, 0, &render_mode))
    {
        GL_ENTRYPOINT(glRenderMode)(render_mode);
        VOGL_CHECK_GL_ERROR;
        ADD_PROCESSED_STATE(GL_RENDER_MODE, 0);
    }

    // current color
    vec4F cur_color(0.0f);
    if (get(GL_CURRENT_COLOR, 0, cur_color.get_ptr(), 4))
    {
        GL_ENTRYPOINT(glColor4f)(cur_color[0], cur_color[1], cur_color[2], cur_color[3]);
        ADD_PROCESSED_STATE(GL_CURRENT_COLOR, 0);
    }

    vec3F cur_normal(0.0f);
    if (get(GL_CURRENT_NORMAL, 0, cur_normal.get_ptr(), 3))
    {
        GL_ENTRYPOINT(glNormal3f)(cur_normal[0], cur_normal[1], cur_normal[2]);
        ADD_PROCESSED_STATE(GL_CURRENT_NORMAL, 0);
    }

    float cur_index = 0;
    if (get(GL_CURRENT_INDEX, 0, &cur_index, 1))
    {
        GL_ENTRYPOINT(glIndexf)(cur_index);
        ADD_PROCESSED_STATE(GL_CURRENT_INDEX, 0);
    }

    float cur_fog_coord = 0;
    if (get(GL_CURRENT_FOG_COORD, 0, &cur_fog_coord, 1))
    {
        GL_ENTRYPOINT(glFogCoordf)(cur_fog_coord);
        ADD_PROCESSED_STATE(GL_CURRENT_FOG_COORD, 0);
    }

    vec4F cur_secondary_color(0.0f);
    if (get(GL_CURRENT_SECONDARY_COLOR, 0, cur_secondary_color.get_ptr(), 4))
    {
        GL_ENTRYPOINT(glSecondaryColor3f)(cur_secondary_color[0], cur_secondary_color[1], cur_secondary_color[2]);
        ADD_PROCESSED_STATE(GL_CURRENT_SECONDARY_COLOR, 0);
    }

    vec4F cur_raster_position(0.0f);
    if (get(GL_CURRENT_RASTER_POSITION, 0, cur_raster_position.get_ptr(), 4))
    {
        GL_ENTRYPOINT(glRasterPos4f)(cur_raster_position[0], cur_raster_position[1], cur_raster_position[2], cur_raster_position[3]);
        ADD_PROCESSED_STATE(GL_CURRENT_RASTER_POSITION, 0);
    }

    // I don't know what we can do about these, or if we even care.
    ADD_PROCESSED_STATE(GL_CURRENT_RASTER_POSITION_VALID, 0);
    ADD_PROCESSED_STATE(GL_CURRENT_RASTER_COLOR, 0);
    ADD_PROCESSED_STATE(GL_CURRENT_RASTER_DISTANCE, 0);
    ADD_PROCESSED_STATE(GL_CURRENT_RASTER_INDEX, 0);
    //ADD_PROCESSED_STATE(GL_CURRENT_RASTER_SECONDARY_COLOR, 0); // can't retrieve on AMD's v13 drivers

    // Image bindings
    if (context_info.supports_extension("GL_ARB_shader_image_load_store"))
    {
        GLint max_image_units = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_MAX_IMAGE_UNITS, &max_image_units);
        VOGL_CHECK_GL_ERROR;

        for (int i = 0; i < max_image_units; i++)
        {
            GLuint trace_binding = 0;
            if (!get(GL_IMAGE_BINDING_NAME, i, &trace_binding, 1, true))
                continue;

            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_IMAGE_BINDING_NAME, i);

            GLuint replay_binding = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, trace_binding));

            int level = 0;
            get(GL_IMAGE_BINDING_LEVEL, i, &level, 1, true);
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_IMAGE_BINDING_LEVEL, i);

            bool layered = false;
            get(GL_IMAGE_BINDING_LAYERED, i, &layered, 1, true);
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_IMAGE_BINDING_LAYERED, i);

            GLint layer = 0;
            get(GL_IMAGE_BINDING_LAYER, i, &layer, 1, true);
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_IMAGE_BINDING_LAYER, i);

            GLenum access = 0;
            get(GL_IMAGE_BINDING_ACCESS, i, &access, 1, true);
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_IMAGE_BINDING_ACCESS, i);

            GLenum format = GL_NONE;
            get(GL_IMAGE_BINDING_FORMAT, i, &format, 1, true);
            ADD_PROCESSED_STATE_INDEXED_VARIANT(GL_IMAGE_BINDING_FORMAT, i);

            GL_ENTRYPOINT(glBindImageTexture)(i, replay_binding, level, layered, layer, access, format);
            VOGL_CHECK_GL_ERROR;
        }
    }
    // TODO: pixel maps?

    //----------------------------------------

    // begin any active queries

    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(g_query_targets); i++)
    {
        const GLenum target = g_query_targets[i];

        GLuint query_handle = 0;
        if (get(GL_CURRENT_QUERY, target, &query_handle))
        {
            ADD_PROCESSED_STATE(GL_CURRENT_QUERY, target);

            if (query_handle)
            {
                query_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_QUERIES, query_handle));

                if (query_handle)
                {
                    GL_ENTRYPOINT(glBeginQuery)(target, query_handle);
                    VOGL_CHECK_GL_ERROR;
                }
            }
        }
    }

    //----------------------------------------

    GL_ENTRYPOINT(glBindBuffer)(GL_ARRAY_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_ARRAY_BUFFER, prev_array_buffer_binding);
    VOGL_CHECK_GL_ERROR;

    if (!context_info.is_core_profile())
    {
        GL_ENTRYPOINT(glClientActiveTexture)(prev_client_active_texture);
        VOGL_CHECK_GL_ERROR;
    }

    GL_ENTRYPOINT(glActiveTexture)(prev_active_texture);
    VOGL_CHECK_GL_ERROR;

#ifdef DEBUG_CHECK_FOR_UNPROCESSED_STATES

#define NOTE_UNUSED(x) ADD_PROCESSED_STATE(x, 0);

    NOTE_UNUSED(GL_SUBPIXEL_BITS)
    NOTE_UNUSED(GL_VIEWPORT_SUBPIXEL_BITS)
    NOTE_UNUSED(GL_RED_BITS)
    NOTE_UNUSED(GL_INDEX_BITS)
    NOTE_UNUSED(GL_GREEN_BITS)
    NOTE_UNUSED(GL_DEPTH_BITS)
    NOTE_UNUSED(GL_BLUE_BITS)
    NOTE_UNUSED(GL_ALPHA_BITS)
    NOTE_UNUSED(GL_ACCUM_ALPHA_BITS)
    NOTE_UNUSED(GL_ACCUM_BLUE_BITS)
    NOTE_UNUSED(GL_ACCUM_GREEN_BITS)
    NOTE_UNUSED(GL_ACCUM_RED_BITS)
    NOTE_UNUSED(GL_STENCIL_BITS)
    NOTE_UNUSED(GL_ATTRIB_STACK_DEPTH)
    NOTE_UNUSED(GL_CLIENT_ATTRIB_STACK_DEPTH)
    //NOTE_UNUSED(GL_COLOR_MATRIX_STACK_DEPTH) // can't retrieve on AMD v13 drivers
    NOTE_UNUSED(GL_DEBUG_GROUP_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_ATTRIB_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_COLOR_MATRIX_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_DEBUG_GROUP_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_MODELVIEW_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_NAME_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_PROJECTION_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_TEXTURE_STACK_DEPTH)
    NOTE_UNUSED(GL_MAX_3D_TEXTURE_SIZE)
    NOTE_UNUSED(GL_MAX_ARRAY_TEXTURE_LAYERS)
    NOTE_UNUSED(GL_MAX_CLIP_PLANES)
    NOTE_UNUSED(GL_MAX_COLOR_ATTACHMENTS)
    NOTE_UNUSED(GL_MAX_COLOR_TEXTURE_SAMPLES)
    NOTE_UNUSED(GL_MAX_COMBINED_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
    NOTE_UNUSED(GL_MAX_COMBINED_UNIFORM_BLOCKS)
    NOTE_UNUSED(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_COMPUTE_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS)
    NOTE_UNUSED(GL_MAX_COMPUTE_LOCAL_INVOCATIONS)
    NOTE_UNUSED(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
    NOTE_UNUSED(GL_MAX_COMPUTE_UNIFORM_BLOCKS)
    NOTE_UNUSED(GL_MAX_COMPUTE_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_CUBE_MAP_TEXTURE_SIZE)
    NOTE_UNUSED(GL_MAX_DEPTH_TEXTURE_SAMPLES)
    NOTE_UNUSED(GL_MAX_DRAW_BUFFERS)
    NOTE_UNUSED(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS)
    NOTE_UNUSED(GL_MAX_ELEMENTS_INDICES)
    NOTE_UNUSED(GL_MAX_ELEMENTS_VERTICES)
    NOTE_UNUSED(GL_MAX_ELEMENT_INDEX)
    NOTE_UNUSED(GL_MAX_EVAL_ORDER)
    NOTE_UNUSED(GL_MAX_FRAGMENT_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_FRAGMENT_INPUT_COMPONENTS)
    NOTE_UNUSED(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_FRAGMENT_UNIFORM_BLOCKS)
    NOTE_UNUSED(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_FRAGMENT_UNIFORM_VECTORS)
    NOTE_UNUSED(GL_MAX_FRAMEBUFFER_HEIGHT)
    NOTE_UNUSED(GL_MAX_FRAMEBUFFER_LAYERS)
    NOTE_UNUSED(GL_MAX_FRAMEBUFFER_SAMPLES)
    NOTE_UNUSED(GL_MAX_FRAMEBUFFER_WIDTH)
    NOTE_UNUSED(GL_MAX_GEOMETRY_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_INPUT_COMPONENTS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_OUTPUT_VERTICES)
    NOTE_UNUSED(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_UNIFORM_BLOCKS)
    NOTE_UNUSED(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_INTEGER_SAMPLES)
    NOTE_UNUSED(GL_MAX_LABEL_LENGTH)
    NOTE_UNUSED(GL_MAX_LIGHTS)
    NOTE_UNUSED(GL_MAX_LIST_NESTING)
    NOTE_UNUSED(GL_MAX_PIXEL_MAP_TABLE)
    NOTE_UNUSED(GL_MAX_PROGRAM_TEXEL_OFFSET)
    NOTE_UNUSED(GL_MAX_RECTANGLE_TEXTURE_SIZE)
    NOTE_UNUSED(GL_MAX_RENDERBUFFER_SIZE)
    NOTE_UNUSED(GL_MAX_SAMPLE_MASK_WORDS)
    NOTE_UNUSED(GL_MAX_SERVER_WAIT_TIMEOUT)
    NOTE_UNUSED(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS)
    NOTE_UNUSED(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_TEXTURE_BUFFER_SIZE)
    NOTE_UNUSED(GL_MAX_TEXTURE_COORDS)
    NOTE_UNUSED(GL_MAX_TEXTURE_IMAGE_UNITS)
    NOTE_UNUSED(GL_MAX_TEXTURE_LOD_BIAS)
    NOTE_UNUSED(GL_MAX_TEXTURE_SIZE)
    NOTE_UNUSED(GL_MAX_TEXTURE_UNITS)
    NOTE_UNUSED(GL_MAX_UNIFORM_BLOCK_SIZE)
    NOTE_UNUSED(GL_MAX_UNIFORM_BUFFER_BINDINGS)
    NOTE_UNUSED(GL_MAX_UNIFORM_LOCATIONS)
    NOTE_UNUSED(GL_MAX_VARYING_FLOATS)
    NOTE_UNUSED(GL_MAX_VARYING_VECTORS)
    NOTE_UNUSED(GL_MAX_VERTEX_ATOMIC_COUNTERS)
    NOTE_UNUSED(GL_MAX_VERTEX_ATTRIBS)
    NOTE_UNUSED(GL_MAX_VERTEX_ATTRIB_BINDINGS)
    NOTE_UNUSED(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET)
    NOTE_UNUSED(GL_MAX_VERTEX_OUTPUT_COMPONENTS)
    NOTE_UNUSED(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS)
    NOTE_UNUSED(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
    NOTE_UNUSED(GL_MAX_VERTEX_UNIFORM_BLOCKS)
    NOTE_UNUSED(GL_MAX_VERTEX_UNIFORM_COMPONENTS)
    NOTE_UNUSED(GL_MAX_VERTEX_UNIFORM_VECTORS)
    NOTE_UNUSED(GL_MAX_VIEWPORTS)
    NOTE_UNUSED(GL_MAX_VIEWPORT_DIMS)
    NOTE_UNUSED(GL_MODELVIEW_STACK_DEPTH)
    NOTE_UNUSED(GL_NAME_STACK_DEPTH)
    NOTE_UNUSED(GL_PROJECTION_STACK_DEPTH)
    NOTE_UNUSED(GL_COLOR_MATRIX)
    NOTE_UNUSED(GL_AUX_BUFFERS)
    NOTE_UNUSED(GL_COMPRESSED_TEXTURE_FORMATS)
    NOTE_UNUSED(GL_CONTEXT_FLAGS)
    NOTE_UNUSED(GL_FEEDBACK_BUFFER_POINTER)
    NOTE_UNUSED(GL_FEEDBACK_BUFFER_SIZE)
    NOTE_UNUSED(GL_FEEDBACK_BUFFER_TYPE)
    NOTE_UNUSED(GL_IMPLEMENTATION_COLOR_READ_FORMAT)
    NOTE_UNUSED(GL_IMPLEMENTATION_COLOR_READ_TYPE)
    NOTE_UNUSED(GL_LAYER_PROVOKING_VERTEX)
    NOTE_UNUSED(GL_MIN_MAP_BUFFER_ALIGNMENT)
    NOTE_UNUSED(GL_MIN_PROGRAM_TEXEL_OFFSET)
    NOTE_UNUSED(GL_MINOR_VERSION)
    NOTE_UNUSED(GL_MAJOR_VERSION)
    NOTE_UNUSED(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT)
    NOTE_UNUSED(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT)
    NOTE_UNUSED(GL_ALIASED_LINE_WIDTH_RANGE)
    NOTE_UNUSED(GL_ALIASED_POINT_SIZE_RANGE)
    NOTE_UNUSED(GL_LINE_WIDTH_GRANULARITY)
    NOTE_UNUSED(GL_LINE_WIDTH_RANGE)
    NOTE_UNUSED(GL_MAP1_GRID_DOMAIN)
    NOTE_UNUSED(GL_MAP1_GRID_SEGMENTS)
    NOTE_UNUSED(GL_MAP2_GRID_DOMAIN)
    NOTE_UNUSED(GL_MAP2_GRID_SEGMENTS)
    NOTE_UNUSED(GL_MODELVIEW_MATRIX)
    NOTE_UNUSED(GL_NUM_COMPRESSED_TEXTURE_FORMATS)
    NOTE_UNUSED(GL_NUM_EXTENSIONS)
    NOTE_UNUSED(GL_NUM_PROGRAM_BINARY_FORMATS)
    NOTE_UNUSED(GL_NUM_SHADER_BINARY_FORMATS)
    NOTE_UNUSED(GL_POINT_SIZE_GRANULARITY)
    NOTE_UNUSED(GL_POINT_SIZE_RANGE)
    NOTE_UNUSED(GL_POST_COLOR_MATRIX_COLOR_TABLE)
    NOTE_UNUSED(GL_PROGRAM_BINARY_FORMATS)
    NOTE_UNUSED(GL_PROJECTION_MATRIX)
    NOTE_UNUSED(GL_SAMPLES)
    NOTE_UNUSED(GL_SAMPLE_BUFFERS)
    NOTE_UNUSED(GL_SELECTION_BUFFER_POINTER)
    //NOTE_UNUSED(GL_SELECTION_BUFFER_SIZE)
    NOTE_UNUSED(GL_TIMESTAMP)
    NOTE_UNUSED(GL_TRANSPOSE_COLOR_MATRIX)
    NOTE_UNUSED(GL_TRANSPOSE_MODELVIEW_MATRIX)
    NOTE_UNUSED(GL_TRANSPOSE_PROJECTION_MATRIX)
    NOTE_UNUSED(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT)
    NOTE_UNUSED(GL_VIEWPORT_BOUNDS_RANGE)
    NOTE_UNUSED(GL_VIEWPORT_INDEX_PROVOKING_VERTEX)
#undef NOTE_UNUSED

    for (const_iterator it = begin(); it != end(); ++it)
    {
        const vogl_state_data &state = it->second;

        uint32_t i;
        for (i = 0; i < processed_states.size(); i++)
            if (state.get_id() == processed_states[i])
                break;

        if (i == processed_states.size())
        {
            vogl_debug_printf("Didn't process state: %s index: %u indexed_variant: %u\n", get_gl_enums().find_name(state.get_enum_val()), state.get_index(), state.get_indexed_variant());
        }
    }
#endif

    return true;
}

bool vogl_general_context_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    for (state_map::iterator it = m_states.begin(); it != m_states.end(); ++it)
    {
        vogl_state_data &state = it->second;

        const GLenum enum_val = state.get_enum_val();
        const uint32_t index = state.get_index();
        VOGL_NOTE_UNUSED(index);

        vogl_namespace_t handle_namespace = VOGL_NAMESPACE_INVALID;

        switch (enum_val)
        {
            case GL_ARRAY_BUFFER_BINDING:
            case GL_ELEMENT_ARRAY_BUFFER_BINDING:
            case GL_PIXEL_PACK_BUFFER_BINDING:
            case GL_PIXEL_UNPACK_BUFFER_BINDING:
            case GL_COPY_READ_BUFFER_BINDING:
            case GL_COPY_WRITE_BUFFER_BINDING:
            case GL_DRAW_INDIRECT_BUFFER_BINDING:
            case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
            case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            case GL_UNIFORM_BUFFER_BINDING:
            case GL_ATOMIC_COUNTER_BUFFER_BINDING:
            case GL_SHADER_STORAGE_BUFFER_BINDING:
            case GL_TEXTURE_BUFFER: // should eventually be changed to GL_TEXTURE_BUFFER_BINDING, see http://www.khronos.org/bugzilla/show_bug.cgi?id=844

            case GL_VERTEX_ARRAY_BUFFER_BINDING:
            case GL_COLOR_ARRAY_BUFFER_BINDING:
            case GL_INDEX_ARRAY_BUFFER_BINDING:
            case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING:
            case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            case GL_FOG_COORD_ARRAY_BUFFER_BINDING:
            case GL_NORMAL_ARRAY_BUFFER_BINDING:
            case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING:

                handle_namespace = VOGL_NAMESPACE_BUFFERS;
                break;

            case GL_SAMPLER_BINDING:
                handle_namespace = VOGL_NAMESPACE_SAMPLERS;
                break;

            case GL_CURRENT_QUERY:
                handle_namespace = VOGL_NAMESPACE_QUERIES;
                break;

            case GL_CURRENT_PROGRAM:
                handle_namespace = VOGL_NAMESPACE_PROGRAMS;
                break;

            case GL_PROGRAM_PIPELINE_BINDING:
                handle_namespace = VOGL_NAMESPACE_PIPELINES;
                break;

            case GL_RENDERBUFFER_BINDING:
                handle_namespace = VOGL_NAMESPACE_RENDER_BUFFERS;
                break;

            case GL_READ_FRAMEBUFFER_BINDING:
            case GL_DRAW_FRAMEBUFFER_BINDING:
                handle_namespace = VOGL_NAMESPACE_FRAMEBUFFERS;
                break;

            case GL_VERTEX_ARRAY_BINDING:
                handle_namespace = VOGL_NAMESPACE_VERTEX_ARRAYS;
                break;

            case GL_TEXTURE_BINDING_BUFFER:
            case GL_TEXTURE_BINDING_RECTANGLE:
            case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:
            case GL_TEXTURE_BINDING_CUBE_MAP:
            case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
            case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
            case GL_TEXTURE_BINDING_2D_ARRAY:
            case GL_TEXTURE_BINDING_1D_ARRAY:
            case GL_TEXTURE_BINDING_1D:
            case GL_TEXTURE_BINDING_2D:
            case GL_TEXTURE_BINDING_3D:
            case GL_IMAGE_BINDING_NAME:
                handle_namespace = VOGL_NAMESPACE_TEXTURES;
                break;
        }

        if (handle_namespace == VOGL_NAMESPACE_INVALID)
            continue;

        if ((state.get_num_elements() != 1) || ((state.get_data_type() != cSTInt32) && (state.get_data_type() != cSTUInt32)))
        {
            VOGL_ASSERT_ALWAYS;
            continue;
        }

        GLuint handle;
        state.get_uint(&handle);

        if (handle)
        {
            handle = static_cast<GLuint>(remapper.remap_handle(handle_namespace, handle));

            state.get_element<GLuint>(0) = handle;
        }
    }

    return true;
}

// TODO: Move this to separate file
vogl_polygon_stipple_state::vogl_polygon_stipple_state()
    : m_valid(false)
{
    VOGL_FUNC_TRACER

    utils::zero_object(m_pattern);
}

vogl_polygon_stipple_state::~vogl_polygon_stipple_state()
{
    VOGL_FUNC_TRACER
}

bool vogl_polygon_stipple_state::snapshot(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    VOGL_CHECK_GL_ERROR;

    clear();

    vogl_scoped_state_saver pixelstore_state_saver(cGSTPixelStore);
    vogl_reset_pixel_store_states();

    GL_ENTRYPOINT(glGetPolygonStipple)(m_pattern);
    VOGL_CHECK_GL_ERROR;

    m_valid = true;

    return true;
}

bool vogl_polygon_stipple_state::restore(const vogl_context_info &context_info) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(context_info);

    if (!m_valid)
        return false;

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_state_saver pixelstore_state_saver(cGSTPixelStore);
    vogl_reset_pixel_store_states();

    GL_ENTRYPOINT(glPolygonStipple)(m_pattern);
    VOGL_CHECK_GL_ERROR;

    return true;
}

void vogl_polygon_stipple_state::clear()
{
    VOGL_FUNC_TRACER

    m_valid = false;
    utils::zero_object(m_pattern);
}

bool vogl_polygon_stipple_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    if (!m_valid)
        return false;

    json_node &arr_node = node.add_array("pattern");
    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(m_pattern); i++)
        arr_node.add_value(m_pattern[i]);

    return true;
}

bool vogl_polygon_stipple_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    const json_node *pArr_node = node.find_child_array("pattern");
    if (!pArr_node)
        return false;

    if (!pArr_node->are_all_children_values())
        return false;

    // An earlier version wrote the wrong size, so ignore that data.
    if (pArr_node->size() == VOGL_ARRAY_SIZE(m_pattern))
    {
        for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(m_pattern); i++)
            m_pattern[i] = static_cast<uint8_t>(pArr_node->value_as_uint32(i));
    }
    else
    {
        vogl_warning_printf("Polygon stipple data is not valid in this older trace file so it's being ignored - please recapture (sorry)\n");
    }

    m_valid = true;

    return true;
}

uint32_t vogl_polygon_stipple_state::get_num_pattern_rows() const
{
    return 32;
}

uint32_t vogl_polygon_stipple_state::get_pattern_row(uint32_t row_index) const
{
    VOGL_ASSERT(row_index < 32);

    return m_pattern[4 * row_index] | (m_pattern[4 * row_index + 1] << 8U) | (m_pattern[4 * row_index + 2] << 16U) | (m_pattern[4 * row_index + 3] << 24U);
}
