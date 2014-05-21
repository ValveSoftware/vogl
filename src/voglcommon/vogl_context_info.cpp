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

// File: vogl_context_info.cpp
#include "vogl_context_info.h"
#include "vogl_entrypoints.h"

#include "vogl_console.h"

#if VOGL_PLATFORM_SUPPORTS_BTRACE
    #include "btrace.h"
#endif

int vogl_determine_attrib_list_array_size(const int *attrib_list)
{
    VOGL_FUNC_TRACER

    if (!attrib_list)
        return 0;

    int n = 0;

    if (attrib_list)
    {
        for (;;)
        {
            int key = attrib_list[n];
            n++;
            if (!key)
                break;

            int val = attrib_list[n];
            VOGL_NOTE_UNUSED(val);
            n++;
        }
    }

    return n;
}

vogl_context_attribs::vogl_context_attribs()
{
    VOGL_FUNC_TRACER
}

vogl_context_attribs::vogl_context_attribs(const int *pAttribs)
{
    VOGL_FUNC_TRACER

    init(pAttribs);
}

vogl_context_attribs::vogl_context_attribs(const int *pAttribs, uint32_t num_attribs)
{
    VOGL_FUNC_TRACER

    init(pAttribs, num_attribs);
}

vogl_context_attribs::vogl_context_attribs(const attrib_vec &vec)
{
    VOGL_FUNC_TRACER

    init(vec.get_ptr(), vec.size());
}

void vogl_context_attribs::init(const int *pAttribs)
{
    VOGL_FUNC_TRACER

    init(pAttribs, vogl_determine_attrib_list_array_size(pAttribs));
}

void vogl_context_attribs::init(const int *pAttribs, uint32_t num_attribs)
{
    VOGL_FUNC_TRACER

    m_attribs.clear();

    if ((pAttribs) && (num_attribs))
    {
        m_attribs.append(pAttribs, num_attribs);
    }
}

void vogl_context_attribs::init(const attrib_vec &vec)
{
    VOGL_FUNC_TRACER

    init(vec.get_ptr(), vec.size());
}

int vogl_context_attribs::find_value_ofs(int key_to_find) const
{
    VOGL_FUNC_TRACER

    uint32_t ofs = 0;
    while (ofs < m_attribs.size())
    {
        int key = m_attribs[ofs];
        if (!key)
            break;

        if (++ofs >= m_attribs.size())
            break;

        if (key == key_to_find)
            return ofs;

        ofs++;
    }

    return -1;
}

bool vogl_context_attribs::get_value(int key_to_find, int &val) const
{
    VOGL_FUNC_TRACER

    int ofs = find_value_ofs(key_to_find);
    if (ofs < 0)
    {
        val = 0;
        return false;
    }

    val = m_attribs[ofs];
    return true;
}

bool vogl_context_attribs::check() const
{
    VOGL_FUNC_TRACER

    if (!m_attribs.size())
        return true;

    if (m_attribs.back() != 0)
        return false;

    uint32_t ofs = 0;
    do
    {
        int key = m_attribs[ofs];
        if (!key)
            return true;
        ofs++;

        if (ofs >= m_attribs.size())
            return false;

        ofs++;
    } while (ofs < m_attribs.size());

    return false;
}

bool vogl_context_attribs::serialize(vogl::json_node &node) const
{
    VOGL_FUNC_TRACER

    json_node &attribs_array = node.add_array("attribs");
    for (uint32_t i = 0; i < m_attribs.size(); i++)
        attribs_array.add_value(m_attribs[i]);

    return true;
}

bool vogl_context_attribs::deserialize(const vogl::json_node &node)
{
    VOGL_FUNC_TRACER

    const json_node *pAttribs_array = node.find_child_array("attribs");
    if (!pAttribs_array)
        return false;

    m_attribs.resize(pAttribs_array->size());
    for (uint32_t i = 0; i < pAttribs_array->size(); i++)
        m_attribs[i] = pAttribs_array->value_as_int(i);

    return true;
}

void vogl_context_attribs::add_key(int key, int value)
{
    VOGL_FUNC_TRACER

    if (m_attribs.size())
    {
        VOGL_ASSERT(m_attribs.back() == 0);
        if (m_attribs.back() == 0)
            m_attribs.resize(m_attribs.size() - 1);
    }

    m_attribs.push_back(key);
    m_attribs.push_back(value);
    m_attribs.push_back(0);
}

vogl_context_info::vogl_context_info()
    : m_version(VOGL_GL_VERSION_UNKNOWN),
      m_forward_compatible(false),
      m_core_profile(false),
      m_debug_context(false),
      m_max_vertex_attribs(0),
      m_max_texture_coords(0),
      m_max_texture_units(0),
      m_max_texture_image_units(0),
      m_max_combined_texture_coords(0),
      m_max_draw_buffers(0),
      m_max_lights(0),
      m_max_uniform_buffer_bindings(0),
      m_max_arb_program_matrices(0),
      m_max_arb_vertex_program_env_params(0),
      m_max_arb_fragment_program_env_params(0),
      m_max_transform_feedback_separate_attribs(0),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_context_info::vogl_context_info(const vogl_context_desc &desc)
    : m_version(VOGL_GL_VERSION_UNKNOWN),
      m_forward_compatible(false),
      m_core_profile(false),
      m_debug_context(false),
      m_max_vertex_attribs(0),
      m_max_texture_coords(0),
      m_max_texture_units(0),
      m_max_texture_image_units(0),
      m_max_combined_texture_coords(0),
      m_max_draw_buffers(0),
      m_max_lights(0),
      m_max_uniform_buffer_bindings(0),
      m_max_arb_program_matrices(0),
      m_max_arb_vertex_program_env_params(0),
      m_max_arb_fragment_program_env_params(0),
      m_max_transform_feedback_separate_attribs(0),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER

    init(desc);
}

void vogl_context_info::query_string(vogl::dynamic_string &str, GLenum name)
{
    VOGL_FUNC_TRACER

    const char *pStr = reinterpret_cast<const char *>(GL_ENTRYPOINT(glGetString)(name));
    str.set(pStr ? pStr : "");

    vogl_check_gl_error();
}

vogl_gl_version_t vogl_context_info::parse_version_string(const vogl::dynamic_string &str)
{
    VOGL_FUNC_TRACER

    if (str.size() < 3)
        return VOGL_GL_VERSION_UNKNOWN;

    // major_number.minor_number major_number.minor_number.release_number
    // release_number can be very large (tens of thousands)
    // optionally followed by vendor/driver info

    int major = 0, minor = 0;
    uint32_t n = sscanf(str.get_ptr(), "%d.%d", &major, &minor);
    if ((n < 2) || (major < 1) || (minor < 0) || (minor > 255))
    {
        vogl_error_printf("Failed parsing GL/GLSL version string \"%s\"!\n", str.get_ptr());

        return VOGL_GL_VERSION_UNKNOWN;
    }

    return VOGL_CREATE_GL_VERSION(major, minor, 0);
}

// See http://www.opengl.org/registry/specs/ARB/glx_create_context.txt
bool vogl_context_info::init(const vogl_context_desc &desc)
{
    VOGL_FUNC_TRACER

    clear();

    if (!GL_ENTRYPOINT(glGetString))
        return false;

    m_desc = desc;

    vogl_check_gl_error();

    query_string(m_version_str, GL_VERSION);
    query_string(m_glsl_version_str, GL_SHADING_LANGUAGE_VERSION);
    query_string(m_vendor_str, GL_VENDOR);
    query_string(m_renderer_str, GL_RENDERER);

    // Update btrace with our gl info.
    #if VOGL_PLATFORM_SUPPORTS_BTRACE
        btrace_get_glinfo().version_str = m_version_str;
        btrace_get_glinfo().glsl_version_str = m_glsl_version_str;
        btrace_get_glinfo().vendor_str = m_vendor_str;
        btrace_get_glinfo().renderer_str = m_renderer_str;
    #endif

    m_version = parse_version_string(m_version_str);
    m_glsl_version = parse_version_string(m_glsl_version_str);

    if (vogl_check_gl_error())
    {
        vogl_warning_printf("GL error querying context vendor/renderer/version strings!\n");
    }

    m_forward_compatible = false;
    m_core_profile = false;
    m_debug_context = false;

    // glGetInteger is GL >= 3.0 (even if glGetIntegerv can still be non-null on lower version contexts)
    if ((GL_ENTRYPOINT(glGetIntegerv)) && (m_version >= VOGL_GL_VERSION_3_0))
    {
        GLint major_vers = -1, minor_vers = -1;
        GL_ENTRYPOINT(glGetIntegerv)(GL_MAJOR_VERSION, &major_vers);
        GL_ENTRYPOINT(glGetIntegerv)(GL_MINOR_VERSION, &minor_vers);

        if (!vogl_check_gl_error())
        {
            if ((major_vers > 0) && (minor_vers >= 0))
            {
                vogl_gl_version_t context_version = VOGL_CREATE_GL_VERSION(major_vers, minor_vers, 0);
                VOGL_ASSERT(m_version == context_version);
                m_version = context_version;
            }
        }

        GLint context_flags = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_CONTEXT_FLAGS, &context_flags);
        if (!vogl_check_gl_error())
        {
            if (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
                m_debug_context = true;

            // forward compat is GL >= 3.0
            if (context_flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)
                m_forward_compatible = true;
        }

        // GL_CONTEXT_PROFILE_MASK is GL 3.2 or greater - (the core profile bit is ignored for context versions requested <v3.2)
        if (m_version >= VOGL_GL_VERSION_3_2)
        {
            GLint profile_mask = 0;
            GL_ENTRYPOINT(glGetIntegerv)(GL_CONTEXT_PROFILE_MASK, &profile_mask);

            if (!vogl_check_gl_error())
            {
                if (profile_mask & GL_CONTEXT_CORE_PROFILE_BIT)
                {
                    VOGL_ASSERT((profile_mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) == 0);
                    m_core_profile = true;
                }
            }
        }
        else if (m_version == VOGL_GL_VERSION_3_1 && m_forward_compatible)
        {
            m_core_profile = true;
        }
    }

    determine_core_extensions();

    bool success = query_extensions();

    m_is_valid = success && (m_version >= VOGL_GL_VERSION_1_0);

    init_context_limits();

    vogl_check_gl_error();

    return m_is_valid;
}

void vogl_context_info::init_context_limits()
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;

    m_max_vertex_attribs = vogl_get_gl_integer(GL_MAX_VERTEX_ATTRIBS);
    VOGL_CHECK_GL_ERROR;

    m_max_texture_coords = is_core_profile() ? 0 : vogl_get_gl_integer(GL_MAX_TEXTURE_COORDS);
    VOGL_CHECK_GL_ERROR;

    m_max_texture_units = is_core_profile() ? 0 : vogl_get_gl_integer(GL_MAX_TEXTURE_UNITS);
    VOGL_CHECK_GL_ERROR;

    m_max_texture_image_units = vogl_get_gl_integer(GL_MAX_TEXTURE_IMAGE_UNITS);
    VOGL_CHECK_GL_ERROR;

    m_max_combined_texture_coords = vogl_get_gl_integer(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    VOGL_CHECK_GL_ERROR;

    m_max_draw_buffers = (get_version() >= VOGL_GL_VERSION_2_0) ? vogl_get_gl_integer(GL_MAX_DRAW_BUFFERS) : 0;
    VOGL_CHECK_GL_ERROR;

    m_max_lights = is_core_profile() ? 0 : vogl_get_gl_integer(GL_MAX_LIGHTS);
    VOGL_CHECK_GL_ERROR;

    m_max_uniform_buffer_bindings = vogl_get_gl_integer(GL_MAX_UNIFORM_BUFFER_BINDINGS);
    VOGL_CHECK_GL_ERROR;

    m_max_transform_feedback_separate_attribs = (get_version() >= VOGL_GL_VERSION_3_0) ? vogl_get_gl_integer(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS) : 0;
    VOGL_CHECK_GL_ERROR;

    if (!is_core_profile() && supports_extension("GL_ARB_vertex_program") && GL_ENTRYPOINT(glGetProgramivARB))
    {
        m_max_arb_program_matrices = vogl_get_gl_integer(GL_MAX_PROGRAM_MATRICES_ARB);

        GL_ENTRYPOINT(glGetProgramivARB)(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, reinterpret_cast<GLint *>(&m_max_arb_vertex_program_env_params));
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGetProgramivARB)(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, reinterpret_cast<GLint *>(&m_max_arb_fragment_program_env_params));
        VOGL_CHECK_GL_ERROR;
    }
}

bool vogl_context_info::query_extensions()
{
    VOGL_FUNC_TRACER

    // glGetStringi is available in OpenGL3.0 and newer
    if (GL_ENTRYPOINT(glGetStringi))
    {
        GLint num_extensions = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_NUM_EXTENSIONS, &num_extensions);

        m_extensions.reserve(num_extensions + 200);

        for (int i = 0; i < num_extensions; i++)
        {
            const GLubyte *pGLExt = reinterpret_cast<const GLubyte *>(GL_ENTRYPOINT(glGetStringi)(GL_EXTENSIONS, i));
            if (pGLExt)
            {
                m_extensions.enlarge(1)->set(reinterpret_cast<const char *>(pGLExt));
                m_extensions.back().tolower();
            }
        }
    }

    // glGetString(GL_EXTENSIONS) is only supported in OpenGL3.0 and older.
    // It was deprecrated in 3.1, so it is available in a compatibility profile (GL_ARB_compatibility).
    if (get_version() <= VOGL_GL_VERSION_3_0 || is_compatibility_profile())
    {
        const char *pExtensions = reinterpret_cast<const char *>(GL_ENTRYPOINT(glGetString)(GL_EXTENSIONS));
        if (pExtensions)
        {
            dynamic_string extensions(pExtensions);
            dynamic_string_array tokens;
            extensions.tokenize(" \t", tokens, true);
            m_extensions.append(tokens);
        }
    }

    if (GL_ENTRYPOINT(glXQueryExtensionsString))
    {
        const char *pExtensions = reinterpret_cast<const char *>(GL_ENTRYPOINT(glXQueryExtensionsString)(GL_ENTRYPOINT(glXGetCurrentDisplay)(), 0));
        if (pExtensions)
        {
            dynamic_string extensions(pExtensions);
            dynamic_string_array tokens;
            extensions.tokenize(" \t", tokens, true);
            m_extensions.append(tokens);
        }
    }

    m_extensions.unique();

    return true;
}

void vogl_context_info::clear()
{
    VOGL_FUNC_TRACER

    m_desc.clear();

    m_version = VOGL_GL_VERSION_UNKNOWN;
    m_glsl_version = VOGL_GL_VERSION_UNKNOWN;

    m_forward_compatible = false;
    m_core_profile = false;
    m_debug_context = false;

    m_version_str.clear();
    m_glsl_version_str.clear();
    m_vendor_str.clear();
    m_renderer_str.clear();
    m_extensions.clear();
    m_core_extensions.clear();

    m_max_vertex_attribs = 0;
    m_max_texture_coords = 0;
    m_max_texture_units = 0;
    m_max_texture_image_units = 0;
    m_max_combined_texture_coords = 0;
    m_max_draw_buffers = 0;
    m_max_lights = 0;
    m_max_uniform_buffer_bindings = 0;
    m_max_arb_program_matrices = 0;
    m_max_arb_vertex_program_env_params = 0;
    m_max_arb_fragment_program_env_params = 0;
    m_max_transform_feedback_separate_attribs = 0;

    m_is_valid = false;
}

// I've seen some engines do this, they must have a reason.
// Some newer drivers don't report back extensions added to core GL. So make a separate list that contains those extensions.
// TODO: I'm not so sure about this whole idea.
// TODO: Make sure all this makes sense in core profiles.
void vogl_context_info::determine_core_extensions()
{
    VOGL_FUNC_TRACER

#define ADD_EXT(x)                       \
    do                                   \
    {                                    \
        m_core_extensions.push_back(#x); \
    } while (0)
    if (m_version >= VOGL_GL_VERSION_1_2)
    {
        ADD_EXT(GL_EXT_texture3D);
        ADD_EXT(GL_EXT_bgra);
        ADD_EXT(GL_EXT_packed_pixels);
        ADD_EXT(GL_EXT_rescale_normal);
        ADD_EXT(GL_EXT_separate_specular_color);
        ADD_EXT(GL_SGIS_texture_edge_clamp);
        ADD_EXT(GL_SGIS_texture_lod);
        ADD_EXT(GL_EXT_draw_range_elements);
    }

    if (m_version >= VOGL_GL_VERSION_1_3)
    {
        ADD_EXT(GL_ARB_texture_compression);
        ADD_EXT(GL_ARB_texture_cube_map);
        ADD_EXT(GL_ARB_multisample);
        ADD_EXT(GL_ARB_multitexture);
        ADD_EXT(GL_ARB_transpose_matrix);
        ADD_EXT(GL_ARB_texture_env_add);
        ADD_EXT(GL_ARB_texture_env_combine);
        ADD_EXT(GL_ARB_texture_env_dot3);
        ADD_EXT(GL_ARB_texture_border_clamp);
    }

    if (m_version >= VOGL_GL_VERSION_1_4)
    {
        ADD_EXT(GL_EXT_blend_color);
        ADD_EXT(GL_EXT_blend_minmax);
        ADD_EXT(GL_EXT_blend_subtract);
        ADD_EXT(GL_SGIS_generate_mipmap);
        ADD_EXT(GL_NV_blend_square);
        ADD_EXT(GL_ARB_depth_texture);
        ADD_EXT(GL_ARB_shadow);
        ADD_EXT(GL_EXT_fog_coord);
        ADD_EXT(GL_EXT_multi_draw_arrays);
        ADD_EXT(GL_ARB_point_parameters);
        ADD_EXT(GL_EXT_secondary_color);
        ADD_EXT(GL_EXT_blend_func_separate);
        ADD_EXT(GL_EXT_stencil_wrap);
        ADD_EXT(GL_ARB_texture_env_crossbar);
        ADD_EXT(GL_EXT_texture_lod_bias);
        ADD_EXT(GL_ARB_texture_mirrored_repeat);
        ADD_EXT(GL_ARB_window_pos);
    }

    if (m_version >= VOGL_GL_VERSION_1_5)
    {
        ADD_EXT(GL_ARB_vertex_buffer_object);
        ADD_EXT(GL_ARB_occlusion_query);
        ADD_EXT(GL_EXT_shadow_funcs);
    }

    if (m_version >= VOGL_GL_VERSION_2_0)
    {
        ADD_EXT(GL_ARB_shader_objects);
        ADD_EXT(GL_ARB_vertex_shader);
        ADD_EXT(GL_ARB_fragment_shader);
        ADD_EXT(GL_ARB_shading_language_100);
        ADD_EXT(GL_ARB_draw_buffers);
        ADD_EXT(GL_ARB_texture_non_power_of_two);
        ADD_EXT(GL_ARB_point_sprite);
        ADD_EXT(GL_EXT_blend_equation_separate);
    }

    if (m_version >= VOGL_GL_VERSION_2_1)
    {
        ADD_EXT(GL_EXT_texture_sRGB);
        ADD_EXT(GL_ARB_pixel_buffer_object);
    }

    if (m_version >= VOGL_GL_VERSION_3_0)
    {
        ADD_EXT(GL_EXT_framebuffer_sRGB);
        ADD_EXT(GL_EXT_gpu_shader4);
        ADD_EXT(GL_NV_conditional_render);
        ADD_EXT(GL_ARB_color_buffer_float);
        ADD_EXT(GL_ARB_depth_buffer_float);
        ADD_EXT(GL_ARB_texture_float);
        ADD_EXT(GL_EXT_packed_float);
        ADD_EXT(GL_EXT_texture_shared_exponent);
        ADD_EXT(GL_EXT_framebuffer_object);
        ADD_EXT(GL_NV_half_float);
        ADD_EXT(GL_EXT_texture_integer);
        ADD_EXT(GL_EXT_texture_array);
        ADD_EXT(GL_EXT_packed_depth_stencil);
        ADD_EXT(GL_EXT_draw_buffers2);
        ADD_EXT(GL_EXT_texture_compression_rgtc);
        ADD_EXT(GL_EXT_transform_feedback);
        ADD_EXT(GL_APPLE_vertex_array_object);
        ADD_EXT(GL_ARB_half_float_pixel);
        ADD_EXT(GL_EXT_framebuffer_multisample);
        ADD_EXT(GL_EXT_framebuffer_blit);
    }

    if (m_version >= VOGL_GL_VERSION_3_2)
    {
        ADD_EXT(GL_ARB_sync);
    }
#undef ADD_EXT

    m_core_extensions.unique();
}

bool vogl_context_info::serialize(vogl::json_node &node, const vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if (!m_desc.serialize(node, blob_manager))
        return false;

    node.add_key_value("version_major", VOGL_GL_VERSION_GET_MAJOR(m_version));
    node.add_key_value("version_minor", VOGL_GL_VERSION_GET_MINOR(m_version));
    node.add_key_value("version_patch", VOGL_GL_VERSION_GET_PATCH(m_version));

    node.add_key_value("glsl_version_major", VOGL_GL_VERSION_GET_MAJOR(m_glsl_version));
    node.add_key_value("glsl_version_minor", VOGL_GL_VERSION_GET_MINOR(m_glsl_version));

    node.add_key_value("forward_compatible", m_forward_compatible);
    node.add_key_value("core_profile", m_core_profile);
    node.add_key_value("debug_context", m_debug_context);

    node.add_key_value("version_string", m_version_str);
    node.add_key_value("glsl_version_string", m_glsl_version_str);
    node.add_key_value("vendor_string", m_vendor_str);
    node.add_key_value("renderer_string", m_renderer_str);

    node.add_key_value("max_vertex_attribs", m_max_vertex_attribs);
    node.add_key_value("max_texture_coords", m_max_texture_coords);
    node.add_key_value("max_texture_units", m_max_texture_units);
    node.add_key_value("max_texture_image_units", m_max_texture_image_units);
    node.add_key_value("max_combined_texture_coords", m_max_combined_texture_coords);
    node.add_key_value("max_draw_buffers", m_max_draw_buffers);
    node.add_key_value("max_lights", m_max_lights);
    node.add_key_value("max_uniform_buffer_bindings", m_max_uniform_buffer_bindings);
    node.add_key_value("max_arb_program_matrices", m_max_arb_program_matrices);
    node.add_key_value("max_arb_vertex_program_env_params", m_max_arb_vertex_program_env_params);
    node.add_key_value("max_arb_fragment_program_env_params", m_max_arb_fragment_program_env_params);
    node.add_key_value("max_transform_feedback_separate_attribs", m_max_transform_feedback_separate_attribs);

    json_node &extensions_array = node.add_array("extensions");
    for (uint32_t i = 0; i < m_extensions.size(); i++)
        extensions_array.add_value(m_extensions[i]);

    return true;
}

bool vogl_context_info::deserialize(const vogl::json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    if (!node.is_object())
        return false;

    bool success = node.get_value_as_string("version_string", m_version_str);

    if (!m_desc.deserialize(node, blob_manager))
        return false;

    int version_major = 0, version_minor = 0, version_patch = 0;
    version_major = node.value_as_int("version_major");
    version_minor = node.value_as_int("version_minor");
    version_patch = node.value_as_int("version_patch");
    m_version = VOGL_CREATE_GL_VERSION(version_major, version_minor, version_patch);

    int glsl_version_major = 0, glsl_version_minor = 0;
    glsl_version_major = node.value_as_int("glsl_version_major");
    glsl_version_minor = node.value_as_int("glsl_version_minor");
    m_glsl_version = VOGL_CREATE_GL_VERSION(glsl_version_major, glsl_version_minor, 0);

    m_forward_compatible = node.value_as_bool("forward_compatible");
    m_core_profile = node.value_as_bool("core_profile");
    m_debug_context = node.value_as_bool("debug_context");
    m_glsl_version_str = node.value_as_string("glsl_version_string");
    m_vendor_str = node.value_as_string("vendor_string");
    m_renderer_str = node.value_as_string("renderer_string");

    m_max_vertex_attribs = node.value_as_uint32("max_vertex_attribs");
    m_max_texture_coords = node.value_as_uint32("max_texture_coords");
    m_max_texture_units = node.value_as_uint32("max_texture_units");
    m_max_texture_image_units = node.value_as_uint32("max_texture_image_units");
    m_max_combined_texture_coords = node.value_as_uint32("max_combined_texture_coords");
    m_max_draw_buffers = node.value_as_uint32("max_draw_buffers");
    m_max_lights = node.value_as_uint32("max_lights");
    m_max_uniform_buffer_bindings = node.value_as_uint32("max_uniform_buffer_bindings");
    m_max_arb_program_matrices = node.value_as_uint32("max_arb_program_matrices");
    m_max_arb_vertex_program_env_params = node.value_as_uint32("max_arb_vertex_program_env_params");
    m_max_arb_fragment_program_env_params = node.value_as_uint32("max_arb_fragment_program_env_params");
    m_max_transform_feedback_separate_attribs = node.value_as_uint32("max_transform_feedback_separate_attribs");

    const json_node *pExtensions_array = node.find_child_array("extensions");
    if (pExtensions_array)
    {
        m_extensions.resize(pExtensions_array->size());

        for (uint32_t i = 0; i < pExtensions_array->size(); i++)
            m_extensions[i] = pExtensions_array->get_value(i).as_string();

        m_extensions.unique();
    }

    determine_core_extensions();

    VOGL_ASSERT(success);

    m_is_valid = success;

    return success;
}

bool vogl_context_info::supports_extension(const char *pExt) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    vogl::dynamic_string ext(pExt);

    if (!ext.begins_with("GL_", true))
    {
        vogl_warning_printf("Extension does not begin with \"GL_\" prefix, was this intended?\n");
    }

#if 0
	// Let's see if this is ever necessary. Also not sure how it interacts with core profiles, yet.
	if (m_core_extensions.find_sorted(ext) >= 0)
		return true;
#endif

    return m_extensions.find_sorted(ext) >= 0;
}
