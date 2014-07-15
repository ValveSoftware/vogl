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

// File: vogl_texture_state.cpp
#include "vogl_texture_state.h"

#include "vogl_console.h"
#include "vogl_data_stream_serializer.h"
#include "vogl_dynamic_stream.h"

#include "vogl_common.h"
#include "vogl_texture_format.h"
#include "vogl_shader_utils.h"
#include "vogl_msaa_texture.h"

#define VOGL_SERIALIZED_TEXTURE_STATE_VERSION 0x101

vogl_texture_state::vogl_texture_state()
    : m_snapshot_handle(0),
      m_target(GL_NONE),
      m_buffer(0),
      m_num_samples(0),
      m_is_unquerable(false),
      m_is_valid(false)
{
    VOGL_FUNC_TRACER
}

vogl_texture_state::~vogl_texture_state()
{
    VOGL_FUNC_TRACER

    clear();
}

// TODO: Split this bad boy up into multiple methods
bool vogl_texture_state::snapshot(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 handle, GLenum target)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(remapper);

    const bool is_target_multisampled = ((target == GL_TEXTURE_2D_MULTISAMPLE) || (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY));

    // HACK HACK
    //vogl_devel_dump_internal_texture_formats(context_info); exit(0);

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_binding_state orig_bindings(GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER);
    orig_bindings.save_textures(&context_info);

    vogl_scoped_state_saver pixelstore_state_saver(cGSTPixelStore);

    vogl_scoped_state_saver pixeltransfer_state_saver;
    if (!context_info.is_core_profile())
        pixeltransfer_state_saver.save(context_info, cGSTPixelTransfer);

    vogl_reset_pixel_store_states();
    if (!context_info.is_core_profile())
        vogl_reset_pixel_transfer_states(context_info);

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_PACK_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_UNPACK_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    clear();

    VOGL_ASSERT(handle <= cUINT32_MAX);

    m_snapshot_handle = static_cast<uint32_t>(handle);
    m_target = target;

    if (m_target == GL_NONE)
    {
        // Texture was genned, but not bound to a target yet, so there's nothing more to do.
        m_is_valid = true;

        return true;
    }

    GL_ENTRYPOINT(glBindTexture)(target, m_snapshot_handle);
    VOGL_CHECK_GL_ERROR;

    if (m_target == GL_TEXTURE_BUFFER)
    {
        GLint handle2 = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_TEXTURE_BUFFER_DATA_STORE_BINDING_ARB, &handle2);
        VOGL_CHECK_GL_ERROR;

        m_buffer = handle2;

        GLint format = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_TEXTURE_BUFFER_FORMAT_ARB, &format);
        VOGL_CHECK_GL_ERROR;

        m_params.insert(GL_TEXTURE_INTERNAL_FORMAT, 0, &format, sizeof(format));

        m_is_valid = true;
        return true;
    }

    bool any_gl_errors = false;

#define GET_INT(pname)                                               \
    do                                                               \
    {                                                                \
        int values[4] = { 0, 0, 0, 0 };                              \
        GL_ENTRYPOINT(glGetTexParameteriv)(m_target, pname, values); \
        if (vogl_check_gl_error())                                    \
            any_gl_errors = true;                                    \
        m_params.insert(pname, 0, values, sizeof(values[0]));        \
    } while (0)
#define GET_FLOAT(pname)                                             \
    do                                                               \
    {                                                                \
        float values[4] = { 0, 0, 0, 0 };                            \
        GL_ENTRYPOINT(glGetTexParameterfv)(m_target, pname, values); \
        if (vogl_check_gl_error())                                    \
            any_gl_errors = true;                                    \
        m_params.insert(pname, 0, values, sizeof(values[0]));        \
    } while (0)

    GET_INT(GL_TEXTURE_BASE_LEVEL);
    GET_INT(GL_TEXTURE_MAX_LEVEL);

    if (!is_target_multisampled)
    {
        GET_FLOAT(GL_TEXTURE_BORDER_COLOR);
        GET_INT(GL_TEXTURE_COMPARE_MODE);
        GET_INT(GL_TEXTURE_COMPARE_FUNC);
        GET_FLOAT(GL_TEXTURE_LOD_BIAS);
        GET_INT(GL_TEXTURE_MIN_FILTER);
        GET_INT(GL_TEXTURE_MAG_FILTER);
        GET_FLOAT(GL_TEXTURE_MIN_LOD);
        GET_FLOAT(GL_TEXTURE_MAX_LOD);
        GET_INT(GL_TEXTURE_WRAP_S);
        GET_INT(GL_TEXTURE_WRAP_T);
        GET_INT(GL_TEXTURE_WRAP_R);
    }

    GET_INT(GL_TEXTURE_SWIZZLE_R);
    GET_INT(GL_TEXTURE_SWIZZLE_G);
    GET_INT(GL_TEXTURE_SWIZZLE_B);
    GET_INT(GL_TEXTURE_SWIZZLE_A);
    GET_INT(GL_TEXTURE_SWIZZLE_RGBA);

    if (!context_info.is_core_profile())
    {
        GET_INT(GL_GENERATE_MIPMAP);
    }

    GET_INT(GL_TEXTURE_IMMUTABLE_FORMAT);

    if (context_info.supports_extension("GL_EXT_texture_filter_anisotropic") && (!is_target_multisampled))
    {
        GET_FLOAT(GL_TEXTURE_MAX_ANISOTROPY_EXT);
    }

    if (context_info.supports_extension("GL_EXT_texture_sRGB_decode") && (!is_target_multisampled))
    {
        GET_INT(GL_TEXTURE_SRGB_DECODE_EXT);
    }

    if (!context_info.is_core_profile() && context_info.supports_extension("GL_ARB_shadow_ambient"))
    {
        GET_FLOAT(GL_TEXTURE_COMPARE_FAIL_VALUE_ARB);
    }

    if (!context_info.is_core_profile())
    {
        GET_INT(GL_DEPTH_TEXTURE_MODE);

        // TODO
        //GL_TEXTURE_PRIORITY
        //GL_TEXTURE_RESIDENT
    }
#undef GET_INT
#undef GET_FLOAT

    if (any_gl_errors)
    {
        vogl_error_printf("GL error while enumerating texture %" PRIu64 " target %s's params\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
        clear();
        return false;
    }

    int base_level = m_params.get_value<int>(GL_TEXTURE_BASE_LEVEL, 0, 0);
    if (base_level > 18)
    {
        vogl_error_printf("Unsupported base level %u while enumerating texture %" PRIu64 " target %s\n", base_level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
        clear();
        return false;
    }

    if (base_level > 0)
    {
        vogl_debug_printf("Base level is non-zero (%u) while enumerating texture %" PRIu64 " target %s\n", base_level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
    }

    int max_level = m_params.get_value<int>(GL_TEXTURE_MAX_LEVEL, 0, 1000);

    const GLenum target_to_query = ((m_target == GL_TEXTURE_CUBE_MAP) || (m_target == GL_TEXTURE_CUBE_MAP_ARRAY)) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : m_target;

    const int max_supported_mip_levels = vogl_get_max_supported_mip_levels();

    // Now try to find the highest actually defined mip level before we go any further.
    int trial_level;
    for (trial_level = max_supported_mip_levels - 1; trial_level >= 0; trial_level--)
    {
        GLenum level_internal_fmt = 0;
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, trial_level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&level_internal_fmt));

        // Super paranoid here because every driver seems to handle this crap slightly differently and apps lie
        bool is_valid = true;

        if (vogl_check_gl_error())
            is_valid = false;

        if (is_valid)
        {
            // Needed for ACTC's 3D textures, not sure why yet
            GLint level_width = 0;
            GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, trial_level, GL_TEXTURE_WIDTH, &level_width);
            if (vogl_check_gl_error())
                is_valid = false;
            else if (!level_width)
                is_valid = false;
        }

        if (is_valid)
            break;
    }

    if (trial_level < 0)
    {
        // Texture was genned and bound to a target yet, but we couldn't find any valid levels.
        m_is_unquerable = true;
        m_is_valid = true;

        return true;
    }

    uint32_t num_actual_mip_levels = trial_level + 1;

    // Try to query the base level
    if (num_actual_mip_levels != (static_cast<uint32_t>(max_level) + 1U))
    {
        vogl_warning_printf("Texture's GL_TEXTURE_MAX_LEVEL is %u, but the max defined mip level is %u. Note GL may not act consistently with this texture object, GL texture %" PRIu64 " target %s\n",
                              max_level, trial_level,
                              (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
    }

    GLenum internal_fmt = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, base_level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_fmt));
    if (vogl_check_gl_error())
    {
        vogl_error_printf("GL error while enumerating texture %" PRIu64 " target %s's base level GL_TEXTURE_INTERNAL_FORMAT\n", static_cast<uint64_t>(handle), get_gl_enums().find_gl_name(m_target));
        clear();
        return false;
    }

    const vogl_internal_tex_format *pInternal_tex_fmt = vogl_find_internal_texture_format(internal_fmt);
    if (!pInternal_tex_fmt)
    {
        vogl_error_printf("Unknown texture format 0x%04X (%s) while snapshotting texture %" PRIu64 " target %s\n",
                         internal_fmt, get_gl_enums().find_gl_image_format_name(internal_fmt), static_cast<uint64_t>(handle), get_gl_enums().find_gl_name(m_target));
        clear();
        return false;
    }

    if ((pInternal_tex_fmt->m_optimum_get_image_fmt == GL_NONE) || (pInternal_tex_fmt->m_optimum_get_image_type == GL_NONE))
    {
        vogl_warning_printf("Don't know how to retrieve texture data of internal format 0x%04X (%s) while snapshotting texture %" PRIu64 " target %s\n",
                           internal_fmt, get_gl_enums().find_gl_image_format_name(internal_fmt), static_cast<uint64_t>(handle), get_gl_enums().find_gl_name(m_target));
    }

    // Note: Mips below the base_level may or may not actually exist, AND the app can dynamically manipulate the base level so we need to try and save everything we can.
    int base_width = 0, base_height = 0, base_depth = 0;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, base_level, GL_TEXTURE_WIDTH, &base_width);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, base_level, GL_TEXTURE_HEIGHT, &base_height);
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, base_level, GL_TEXTURE_DEPTH, &base_depth);
    VOGL_CHECK_GL_ERROR;

    if ((base_width < 1) || (base_height < 1) || (base_depth < 1))
    {
        vogl_warning_printf("Couldn't retrieve base level's width, height and/or depth of GL texture %" PRIu64 " target %s internal format 0x%04X (%s)\n",
                           static_cast<uint64_t>(handle), get_gl_enums().find_gl_name(m_target), internal_fmt, get_gl_enums().find_gl_image_format_name(internal_fmt));

        // Texture has been deleted but it remained bound (but at least in source1 no shaders actually tried to read it), but we can't query anything about it on NV so we're kinda screwed.
        m_is_unquerable = true;
        m_is_valid = true;
        return true;
    }

    m_num_samples = 1;
    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, base_level, GL_TEXTURE_SAMPLES, reinterpret_cast<GLint *>(&m_num_samples));
    VOGL_CHECK_GL_ERROR;
    m_num_samples = math::maximum(m_num_samples, 1U);

    if (m_num_samples > 1)
    {
        if (m_num_samples > cMaxSamples)
        {
            vogl_error_printf("Unsupported number of samples (%u) while snapshotting texture %" PRIu64 " target %s\n",
                             m_num_samples, static_cast<uint64_t>(handle), get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
        else if ((target != GL_TEXTURE_2D_MULTISAMPLE) && (target != GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
        {
            vogl_error_printf("Unexpected number of samples (%u) while snapshotting texture %" PRIu64 " target %s\n",
                             m_num_samples, static_cast<uint64_t>(handle), get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
    }

    uint32_t width = base_width << base_level;
    uint32_t height = 1;
    uint32_t depth = 1;

    switch (target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        {
            height = base_height << base_level;
            break;
        }
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        {
            height = width;
            break;
        }
        case GL_TEXTURE_3D:
        {
            height = base_height << base_level;
            depth = base_depth << base_level;
            break;
        }
        case GL_TEXTURE_RECTANGLE:
        {
            VOGL_VERIFY(!base_level);
            height = base_height << base_level;
            break;
        }
        default:
            break;
    }

    //uint32_t max_possible_mip_levels = (m_target == GL_TEXTURE_RECTANGLE) ? 1 : utils::compute_max_mips(width, height, depth);

    GLenum image_fmt = pInternal_tex_fmt->m_optimum_get_image_fmt;
    GLenum image_type = pInternal_tex_fmt->m_optimum_get_image_type;

#if 0
    // OK, I can't retrive the default framebuffer's depth/stencil buffer on AMD - all I get is INVALID_OPERATION. Crap. This works fine on NV though.
    // This workaround didn't work.
    if (internal_fmt == GL_DEPTH_STENCIL)
    {
        if (GL_ENTRYPOINT(glGetInternalformativ) && context_info.supports_extension("GL_ARB_internalformat_query"))
        {
            GL_ENTRYPOINT(glGetInternalformativ)(GL_TEXTURE_2D, internal_fmt, GL_GET_TEXTURE_IMAGE_FORMAT, sizeof(image_fmt), (GLint *)&image_fmt);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glGetInternalformativ)(GL_TEXTURE_2D, internal_fmt, GL_GET_TEXTURE_IMAGE_TYPE, sizeof(image_type), (GLint *)&image_type);
            VOGL_CHECK_GL_ERROR;
        }
    }
#endif

    uint32_t num_faces = 1;

    GLenum ktx_image_fmt = GL_NONE, ktx_image_type = GL_NONE;
    if (!pInternal_tex_fmt->m_compressed)
    {
        ktx_image_fmt = image_fmt;
        ktx_image_type = image_type;
    }

    GLenum ktx_tex_target = m_target;

    if (m_target == GL_TEXTURE_1D)
    {
        if (!m_textures[0].init_1D(width, num_actual_mip_levels, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
    }
    else if ((m_target == GL_TEXTURE_2D) || (m_target == GL_TEXTURE_RECTANGLE))
    {
        if (!m_textures[0].init_2D(width, height, num_actual_mip_levels, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
    }
    else if (m_target == GL_TEXTURE_CUBE_MAP)
    {
        if (width != height)
        {
            vogl_error_printf("Unsupported cubemap dimensions (%ux%u) for texture %" PRIu64 " target %s\n", width, height, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }

        if (!m_textures[0].init_cubemap(width, num_actual_mip_levels, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }

        num_faces = cCubeMapFaces;
    }
    else if (m_target == GL_TEXTURE_CUBE_MAP_ARRAY)
    {
        if (width != height)
        {
            vogl_error_printf("Unsupported cubemap dimensions (%ux%u) for texture %" PRIu64 " target %s\n", width, height, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }

        if (!m_textures[0].init_cubemap_array(width, num_actual_mip_levels, base_depth, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }

        num_faces = cCubeMapFaces;
    }
    else if (m_target == GL_TEXTURE_3D)
    {
        if (!m_textures[0].init_3D(width, height, depth, num_actual_mip_levels, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
    }
    else if (m_target == GL_TEXTURE_1D_ARRAY)
    {
        if (!m_textures[0].init_1D_array(width, num_actual_mip_levels, base_height, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
    }
    else if (m_target == GL_TEXTURE_2D_ARRAY)
    {
        if (!m_textures[0].init_2D_array(width, height, num_actual_mip_levels, base_depth, internal_fmt, ktx_image_fmt, ktx_image_type))
        {
            vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }
    }
    else if (m_target == GL_TEXTURE_2D_MULTISAMPLE)
    {
        ktx_tex_target = GL_TEXTURE_2D;

        for (uint32_t sample_index = 0; sample_index < m_num_samples; sample_index++)
        {
            if (!m_textures[sample_index].init_2D(width, height, num_actual_mip_levels, internal_fmt, ktx_image_fmt, ktx_image_type))
            {
                vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                return false;
            }
        }
    }
    else if (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
        ktx_tex_target = GL_TEXTURE_2D_ARRAY;

        for (uint32_t sample_index = 0; sample_index < m_num_samples; sample_index++)
        {
            if (!m_textures[sample_index].init_2D_array(width, height, num_actual_mip_levels, base_depth, internal_fmt, ktx_image_fmt, ktx_image_type))
            {
                vogl_error_printf("Failed initializing KTX texture object for texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                return false;
            }
        }
    }
    else
    {
        vogl_error_printf("Unsupported target, texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
        clear();
        return false;
    }

    VOGL_VERIFY(m_textures[0].get_ogl_internal_fmt() == internal_fmt);
    if (!pInternal_tex_fmt->m_compressed)
    {
        VOGL_VERIFY(m_textures[0].get_ogl_fmt() == image_fmt);
        VOGL_VERIFY(m_textures[0].get_ogl_type() == image_type);
    }

    // We can't directly get the data of multisampled textures. Instead, copy the MSAA data into X separate non-MSAA textures.
    vogl::vector<GLuint> split_texture_handles;
    vogl::vector<GLuint> split_stencil_texture_handles;

#define VOGL_FREE_SPLIT_TEXTURES \
    if (split_texture_handles.size()) { GL_ENTRYPOINT(glDeleteTextures)(split_texture_handles.size(), split_texture_handles.get_ptr()); VOGL_CHECK_GL_ERROR; } \
    if (split_stencil_texture_handles.size()) { GL_ENTRYPOINT(glDeleteTextures)(split_stencil_texture_handles.size(), split_stencil_texture_handles.get_ptr()); VOGL_CHECK_GL_ERROR; }

    if ((m_target == GL_TEXTURE_2D_MULTISAMPLE) || (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
    {
        // Note: This temporarily switches GL contexts!
        vogl_msaa_texture_splitter splitter;

        if (!splitter.init())
        {
            vogl_error_printf("Failed initializing multisample texture splitter while snapshotting texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            clear();
            return false;
        }

        if ( (pInternal_tex_fmt->m_comp_sizes[cTCDepth]) ||
             (pInternal_tex_fmt->m_comp_sizes[cTCRed]) || (pInternal_tex_fmt->m_comp_sizes[cTCGreen]) || (pInternal_tex_fmt->m_comp_sizes[cTCBlue]) || (pInternal_tex_fmt->m_comp_sizes[cTCAlpha]) ||
             (pInternal_tex_fmt->m_comp_sizes[cTCIntensity]) || (pInternal_tex_fmt->m_comp_sizes[cTCLuminance]) )
        {
            if (!splitter.split(target, static_cast<GLuint>(handle), split_texture_handles))
            {
                vogl_error_printf("Failed splitting multisample texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                return false;
            }
        }

        if (pInternal_tex_fmt->m_comp_sizes[cTCStencil])
        {
            GLuint temp_msaa_color_tex = 0;
            if (!splitter.copy_stencil_samples_to_color(target, static_cast<GLuint>(handle), temp_msaa_color_tex))
            {
                vogl_error_printf("Failed splitting multisample texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                return false;
            }

            bool status = splitter.split(target, temp_msaa_color_tex, split_stencil_texture_handles);

            GL_ENTRYPOINT(glDeleteTextures)(1, &temp_msaa_color_tex);
            VOGL_CHECK_GL_ERROR;

            temp_msaa_color_tex = 0;

            if (!status)
            {
                vogl_error_printf("Failed splitting multisample texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                return false;
            }
        }
    }

    for (uint32_t face = 0; face < num_faces; face++)
    {
        GLenum face_target_to_query = m_target;
        if ((m_target == GL_TEXTURE_CUBE_MAP) || (m_target == GL_TEXTURE_CUBE_MAP_ARRAY))
            face_target_to_query = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

        m_level_params[face].resize(num_actual_mip_levels);

        for (uint32_t level = 0; level < num_actual_mip_levels; level++)
        {
            GLenum level_internal_fmt = 0;
            GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&level_internal_fmt));

            bool is_valid = true;
            if (vogl_check_gl_error())
                is_valid = false;
            else if (level_internal_fmt != internal_fmt)
                is_valid = false;

            if (is_valid)
            {
                // Needed for ACTC's 3D textures, not sure why yet
                GLint level_width = 0;
                GL_ENTRYPOINT(glGetTexLevelParameteriv)(target_to_query, level, GL_TEXTURE_WIDTH, &level_width);
                if (vogl_check_gl_error())
                    is_valid = false;
                else if (!level_width)
                    is_valid = false;
                else if (level_width != math::maximum<int>(width >> level, 1))
                {
                    VOGL_ASSERT_ALWAYS;
                    is_valid = false;
                }
            }

            if (!is_valid)
                continue;

            vogl_state_vector level_params;

// TODO: Check for core vs. compat profiles and not query the old stuff
#define GET_INT(gl_enum)                                                                        \
    do                                                                                          \
    {                                                                                           \
        int values[4] = { 0, 0, 0, 0 };                                                         \
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(face_target_to_query, level, gl_enum, values);  \
        if (vogl_check_gl_error())                                                              \
            any_gl_errors = true;                                                               \
        level_params.insert(gl_enum, 0, values, sizeof(values[0]));                             \
    } while (0)

            GET_INT(GL_TEXTURE_WIDTH);
            GET_INT(GL_TEXTURE_HEIGHT);
            GET_INT(GL_TEXTURE_DEPTH);
            GET_INT(GL_TEXTURE_INTERNAL_FORMAT);
            if (any_gl_errors)
            {
                vogl_error_printf("Failed retrieving face %u level %u's internal format and/or width while enumerating texture %" PRIu64 " target %s's level params\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                VOGL_FREE_SPLIT_TEXTURES
                return false;
            }

            GET_INT(GL_TEXTURE_SAMPLES);
            GET_INT(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS);

            GET_INT(GL_TEXTURE_RED_SIZE);
            GET_INT(GL_TEXTURE_GREEN_SIZE);
            GET_INT(GL_TEXTURE_BLUE_SIZE);
            GET_INT(GL_TEXTURE_ALPHA_SIZE);
            GET_INT(GL_TEXTURE_DEPTH_SIZE);
            GET_INT(GL_TEXTURE_STENCIL_SIZE);
            GET_INT(GL_TEXTURE_LUMINANCE_SIZE);
            GET_INT(GL_TEXTURE_INTENSITY_SIZE);
            GET_INT(GL_TEXTURE_SHARED_SIZE);
            GET_INT(GL_TEXTURE_COMPRESSED);

            if (context_info.supports_extension("GL_ARB_depth_texture"))
            {
                GET_INT(GL_TEXTURE_DEPTH_SIZE);
                GET_INT(GL_TEXTURE_DEPTH_TYPE);
            }

            if (context_info.supports_extension("GL_EXT_packed_depth_stencil"))
                GET_INT(GL_TEXTURE_STENCIL_SIZE_EXT);

            if (m_target == GL_TEXTURE_BUFFER)
            {
                GET_INT(GL_TEXTURE_BUFFER_DATA_STORE_BINDING);
                GET_INT(GL_TEXTURE_BUFFER_OFFSET);
                GET_INT(GL_TEXTURE_BUFFER_SIZE);
            }

            bool is_compressed = level_params.get_value<bool>(GL_TEXTURE_COMPRESSED);
            if (!is_compressed)
            {
                int value = 0;
                level_params.insert(GL_TEXTURE_COMPRESSED_IMAGE_SIZE, 0, &value, sizeof(value));
            }
            else
            {
                GET_INT(GL_TEXTURE_COMPRESSED_IMAGE_SIZE);
            }
#undef GET_INT

            if (any_gl_errors)
            {
                vogl_warning_printf("One or more GL errors while enumerating texture %" PRIu64 " target %s's level params\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
            }

            if ((level_params.get_value<int>(GL_TEXTURE_WIDTH) == 0) ||
                (level_params.get_value<int>(GL_TEXTURE_HEIGHT) == 0) ||
                (level_params.get_value<int>(GL_TEXTURE_DEPTH) == 0) ||
                (level_params.get_value<GLenum>(GL_TEXTURE_INTERNAL_FORMAT) != internal_fmt))
            {
                vogl_error_printf("Failed retrieving level %u's parameters while enumerating texture %" PRIu64 " target %s's level params\n", level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                clear();
                VOGL_FREE_SPLIT_TEXTURES
                return false;
            }

            size_t size_in_bytes = level_params.get_value<uint32_t>(GL_TEXTURE_COMPRESSED_IMAGE_SIZE);
            if (pInternal_tex_fmt->m_compressed)
            {
                if (!size_in_bytes)
                {
                    vogl_error_printf("Failed retrieving level %u's compressed size while enumerating texture %" PRIu64 " target %s's level params\n", level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    clear();
                    VOGL_FREE_SPLIT_TEXTURES
                    return false;
                }
            }
            else
            {
                if (size_in_bytes)
                {
                    vogl_error_printf("Unexpected compressed size on level %u's while enumerating texture %" PRIu64 " target %s's level params\n", level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    clear();
                    VOGL_FREE_SPLIT_TEXTURES
                    return false;
                }
            }

            m_level_params[face][level] = level_params;
        }
    }

    VOGL_CHECK_GL_ERROR;

    uint8_vec temp_img;

    // Now grab the data from each face, mipmap level, and slice/layer and supply it to the KTX texture object.
    for (uint32_t face = 0; face < num_faces; face++)
    {
        GLenum face_target_to_query = m_target;
        if ((m_target == GL_TEXTURE_CUBE_MAP) || (m_target == GL_TEXTURE_CUBE_MAP_ARRAY))
            face_target_to_query = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

        // Query available mip levels and add them to the texture.
        for (uint32_t level = 0; level < num_actual_mip_levels; level++)
        {
            const vogl_state_vector &level_params = m_level_params[face][level];

            // Insert placeholder images for the missing texture levels
            if (!level_params.find(GL_TEXTURE_WIDTH))
            {
                for (uint32_t sample_index = 0; sample_index < m_num_samples; sample_index++)
                {
                    int image_size = m_textures[sample_index].get_expected_image_size(level);
                    temp_img.resize(image_size);

                    if ((ktx_tex_target == GL_TEXTURE_1D_ARRAY) || (ktx_tex_target == GL_TEXTURE_2D_ARRAY) || (ktx_tex_target == GL_TEXTURE_CUBE_MAP_ARRAY))
                    {
                        VOGL_ASSERT(base_depth);
                        uint32_t num_array_elements = base_depth;

                        if (ktx_tex_target == GL_TEXTURE_1D_ARRAY)
                        {
                            VOGL_ASSERT(base_height);
                            num_array_elements = base_height;
                        }

                        for (uint32_t array_index = 0; array_index < num_array_elements; array_index++)
                        {
                            m_textures[sample_index].add_image(level, array_index, face, 0, temp_img.get_ptr(), image_size);
                        }
                    }
                    else
                    {
                        int level_depth = (target == GL_TEXTURE_3D) ? math::maximum<int>(1, depth >> level) : 1;

                        for (int zslice = 0; zslice < level_depth; zslice++)
                        {
                            m_textures[sample_index].add_image(level, 0, face, zslice, temp_img.get_ptr(), temp_img.size());
                        }
                    }
                } // sample_index

                continue;
            }

            int level_width = level_params.get_value<int>(GL_TEXTURE_WIDTH);
            int level_height = level_params.get_value<int>(GL_TEXTURE_HEIGHT);
            int level_depth = level_params.get_value<int>(GL_TEXTURE_DEPTH);

            int size_in_bytes = level_params.get_value<uint32_t>(GL_TEXTURE_COMPRESSED_IMAGE_SIZE);

            if (!pInternal_tex_fmt->m_compressed)
            {
                VOGL_ASSERT(!size_in_bytes);

                size_t size_in_bytes64 = vogl_get_image_size(image_fmt, image_type, level_width, level_height, level_depth);
                if (!size_in_bytes64)
                {
                    vogl_error_printf("Failed computing image size of face %u level %u, texture %" PRIu64 " target %s\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    clear();
                    VOGL_FREE_SPLIT_TEXTURES
                    return false;
                }

                if (size_in_bytes64 > static_cast<size_t>(cINT32_MAX))
                {
                    vogl_error_printf("Image size too large for face %u level %u, texture %" PRIu64 " target %s\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    clear();
                    VOGL_FREE_SPLIT_TEXTURES
                    return false;
                }

                size_in_bytes = static_cast<int>(size_in_bytes64);
            }

            for (uint32_t sample_index = 0; sample_index < m_num_samples; sample_index++)
            {
                GLenum get_target = face_target_to_query;

                if ((m_target == GL_TEXTURE_2D_MULTISAMPLE) || (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
                {
                    // For MSAA textures we read the contents of our temporary "split" textures, which contain the individual samples.
                    GL_ENTRYPOINT(glBindTexture)(ktx_tex_target, split_texture_handles[sample_index]);
                    VOGL_CHECK_GL_ERROR;

                    get_target = ktx_tex_target;
                }

                const uint32_t num_guard_bytes = 2;
                if (!temp_img.try_resize(size_in_bytes + num_guard_bytes))
                {
                    vogl_error_printf("Out of memory while trying to retrieve texture data, texture %" PRIu64 " target %s\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    clear();
                    VOGL_FREE_SPLIT_TEXTURES
                    return false;
                }

                // Write a pattern after the buffer to detect buffer size computation screwups.
                if (size_in_bytes >= 4)
                {
                    temp_img[size_in_bytes - 4] = 0x67;
                    temp_img[size_in_bytes - 3] = 0xCC;
                    temp_img[size_in_bytes - 2] = 0xD4;
                    temp_img[size_in_bytes - 1] = 0xF9;
                }

                temp_img[size_in_bytes] = 0xDE;
                temp_img[size_in_bytes + 1] = 0xAD;

                if (pInternal_tex_fmt->m_compressed)
                {
                    GL_ENTRYPOINT(glGetCompressedTexImage)(get_target, level, temp_img.get_ptr());
                }
                else
                {
                    GL_ENTRYPOINT(glGetTexImage)(get_target, level, image_fmt, image_type, temp_img.get_ptr());
                }

                if (vogl_check_gl_error())
                {
                    vogl_error_printf("Failed retrieving image data for face %u level %u, texture %" PRIu64 " target %s\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    clear();
                    VOGL_FREE_SPLIT_TEXTURES
                    return false;
                }

                if (size_in_bytes >= 4)
                {
                    if ((temp_img[size_in_bytes - 4] == 0x67) && (temp_img[size_in_bytes - 3] == 0xCC) &&
                        (temp_img[size_in_bytes - 2] == 0xD4) && (temp_img[size_in_bytes - 1] == 0xF9))
                    {
                        vogl_error_printf("Image data retrieval may have failed for face %u level %u, texture %" PRIu64 " target %s\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                    }
                }

                VOGL_VERIFY((temp_img[size_in_bytes] == 0xDE) && (temp_img[size_in_bytes + 1] == 0xAD));

                temp_img.try_resize(size_in_bytes);

                if (((m_target == GL_TEXTURE_2D_MULTISAMPLE) || (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)) && (split_stencil_texture_handles.size()))
                {
                    size_t split_color_size_in_bytes64 = vogl_get_image_size(GL_RGBA, GL_UNSIGNED_BYTE, level_width, level_height, level_depth);

                    if (!split_color_size_in_bytes64)
                    {
                        vogl_error_printf("Failed computing image size of face %u level %u, texture %" PRIu64 " target %s\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                        clear();
                        VOGL_FREE_SPLIT_TEXTURES
                        return false;
                    }

                    if (split_color_size_in_bytes64 > static_cast<size_t>(cINT32_MAX))
                    {
                        vogl_error_printf("Image size too large for face %u level %u, texture %" PRIu64 " target %s\n", face, level, (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
                        clear();
                        VOGL_FREE_SPLIT_TEXTURES
                        return false;
                    }

                    uint8_vec stencil_image_data(static_cast<uint32_t>(split_color_size_in_bytes64));

                    GL_ENTRYPOINT(glBindTexture)(ktx_tex_target, split_stencil_texture_handles[sample_index]);
                    VOGL_CHECK_GL_ERROR;

                    GL_ENTRYPOINT(glGetTexImage)(ktx_tex_target, level, GL_RGBA, GL_UNSIGNED_BYTE, stencil_image_data.get_ptr());
                    VOGL_CHECK_GL_ERROR;

                    switch (internal_fmt)
                    {
                        case GL_DEPTH_STENCIL:          // GL_UNSIGNED_INT_24_8
                        case GL_DEPTH24_STENCIL8:       // GL_UNSIGNED_INT_24_8
                        {
                            for (uint32_t y = 0; y < height; y++)
                            {
                                for (uint32_t x = 0; x < width; x++)
                                {
                                    uint32_t ofs = (x * sizeof(uint32_t)) + (y * width * sizeof(uint32_t));
                                    // I'm paranoid
                                    if ((ofs < stencil_image_data.size()) && (ofs < temp_img.size()))
                                    {
                                        uint8_t *pSrc = stencil_image_data.get_ptr() + ofs;
                                        uint8_t *pDest = temp_img.get_ptr() + ofs;

                                        pDest[0] = pSrc[0];
                                    }
                                }
                            }
                            break;
                        }
                        case GL_DEPTH32F_STENCIL8:      // GL_FLOAT_32_UNSIGNED_INT_24_8_REV
                        case GL_DEPTH32F_STENCIL8_NV:   // GL_FLOAT_32_UNSIGNED_INT_24_8_REV
                        {
                            for (uint32_t y = 0; y < height; y++)
                            {
                                for (uint32_t x = 0; x < width; x++)
                                {
                                    uint32_t ofs = (x * sizeof(uint32_t)) + (y * width * sizeof(uint32_t));
                                    // I'm paranoid
                                    if ((ofs < stencil_image_data.size()) && ((ofs + 3) < temp_img.size()))
                                    {
                                        uint8_t *pSrc = stencil_image_data.get_ptr() + ofs;
                                        uint8_t *pDest = temp_img.get_ptr() + ofs;

                                        pDest[3] = pSrc[0];
                                    }
                                }
                            }

                            break;
                        }
                        default:
                        {
                            vogl_warning_printf("Unable to set stencil data in texture %" PRIu64 "\n", (uint64_t)handle);
                            break;
                        }
                    }
                } // if ((m_target == GL_TEXTURE_2D_MULTISAMPLE) || (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY))

                if (ktx_tex_target == GL_TEXTURE_3D)
                {
                    uint32_t zslice_size = size_in_bytes;
                    if (level_depth > 1)
                    {
                        VOGL_ASSERT((size_in_bytes % level_depth) == 0);
                        zslice_size = size_in_bytes / level_depth;
                        VOGL_ASSERT(zslice_size);
                    }

                    VOGL_ASSERT((size_in_bytes % zslice_size) == 0);

                    uint32_t cur_ofs = 0;
                    for (int zslice = 0; zslice < level_depth; zslice++)
                    {
                        m_textures[sample_index].add_image(level, 0, face, zslice, temp_img.get_ptr() + cur_ofs, zslice_size);
                        cur_ofs += zslice_size;
                    }
                    VOGL_ASSERT(static_cast<int>(cur_ofs) == size_in_bytes);
                }
                else if ((ktx_tex_target == GL_TEXTURE_1D_ARRAY) || (ktx_tex_target == GL_TEXTURE_2D_ARRAY) || (ktx_tex_target == GL_TEXTURE_CUBE_MAP_ARRAY))
                {
                    VOGL_ASSERT(base_depth);
                    uint32_t num_array_elements = base_depth;

                    if (ktx_tex_target == GL_TEXTURE_1D_ARRAY)
                    {
                        num_array_elements = base_height;
                    }

                    VOGL_ASSERT((size_in_bytes % num_array_elements) == 0);
                    uint32_t element_size = size_in_bytes / num_array_elements;
                    VOGL_ASSERT(element_size);
                    VOGL_ASSERT((size_in_bytes % element_size) == 0);

                    uint32_t cur_ofs = 0;
                    for (uint32_t array_index = 0; array_index < num_array_elements; array_index++)
                    {
                        m_textures[sample_index].add_image(level, array_index, face, 0, temp_img.get_ptr() + cur_ofs, element_size);
                        cur_ofs += element_size;
                    }
                }
                else
                {
                    m_textures[sample_index].add_image_grant_ownership(level, 0, face, 0, temp_img);
                }

            } // sample_index

        } // level
    } // face

    GL_ENTRYPOINT(glBindTexture)(target, m_snapshot_handle);
    VOGL_CHECK_GL_ERROR;

    // TODO: Add more key/values?
    m_textures[0].add_key_value("VOGL_TARGET", dynamic_string(cVarArg, "%u", m_target).get_ptr());
    m_textures[0].add_key_value("VOGL_BASE_LEVEL", dynamic_string(cVarArg, "%u", m_params.get_value<int>(GL_TEXTURE_BASE_LEVEL)).get_ptr());
    m_textures[0].add_key_value("VOGL_MAX_LEVEL", dynamic_string(cVarArg, "%u", m_params.get_value<int>(GL_TEXTURE_MAX_LEVEL)).get_ptr());

    bool x_flipped = false, y_flipped = true;
    dynamic_string ktx_orient_str(cVarArg, "S=%c,T=%c", x_flipped ? 'l' : 'r', y_flipped ? 'u' : 'd');
    m_textures[0].add_key_value("KTXorientation", ktx_orient_str.get_ptr());

    if (!m_textures[0].consistency_check())
    {
        vogl_error_printf("Internal error: KTX texture failed internal consistency check, texture %" PRIu64 " target %s. This should not happen!\n", (uint64_t)handle, get_gl_enums().find_gl_name(m_target));
        clear();
        VOGL_FREE_SPLIT_TEXTURES
        return false;
    }

    VOGL_FREE_SPLIT_TEXTURES

    m_is_valid = true;

    return true;
}

bool vogl_texture_state::set_tex_parameter(GLenum pname) const
{
    VOGL_FUNC_TRACER

    const vogl_state_data *pData = m_params.find(pname);
    if (!pData)
    {
        // We only return false on a GL error.
        return true;
    }

    enum
    {
        cMaxElements = 16
    };

    if (pData->get_num_elements() > cMaxElements)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((pData->get_data_type() == cSTFloat) || (pData->get_data_type() == cSTDouble))
    {
        float fvals[16];
        pData->get_float(fvals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glTexParameterf)(m_target, pname, fvals[0]);
        else
            GL_ENTRYPOINT(glTexParameterfv)(m_target, pname, fvals);
    }
    else
    {
        int ivals[16];
        pData->get_int(ivals);
        if (pData->get_num_elements() == 1)
            GL_ENTRYPOINT(glTexParameteri)(m_target, pname, ivals[0]);
        else
            GL_ENTRYPOINT(glTexParameteriv)(m_target, pname, ivals);
    }

    return !vogl_check_gl_error();
}

// Note: We'll need the remapper for buffer textures.
bool vogl_texture_state::restore(const vogl_context_info &context_info, vogl_handle_remapper &remapper, GLuint64 &handle) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    VOGL_CHECK_GL_ERROR;

    vogl_msaa_texture_splitter splitter;

    vogl_scoped_binding_state orig_bindings(GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER);
    orig_bindings.save_textures(&context_info);

    vogl_scoped_state_saver pixeltransfer_state_saver(cGSTPixelStore);

    if (!context_info.is_core_profile())
        pixeltransfer_state_saver.save(cGSTPixelTransfer);

    vogl_reset_pixel_store_states();

    if (!context_info.is_core_profile())
        vogl_reset_pixel_transfer_states(context_info);

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_PACK_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    GL_ENTRYPOINT(glBindBuffer)(GL_PIXEL_UNPACK_BUFFER, 0);
    VOGL_CHECK_GL_ERROR;

    bool created_handle = false;

    if (!handle)
    {
        GLuint handle32 = 0;
        GL_ENTRYPOINT(glGenTextures)(1, &handle32);
        if ((vogl_check_gl_error()) || (!handle32))
            return false;
        handle = handle32;

        remapper.declare_handle(VOGL_NAMESPACE_TEXTURES, m_snapshot_handle, handle, m_target);
        if (!remapper.is_default_remapper())
        {
            VOGL_ASSERT(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, m_snapshot_handle) == handle);
        }

        created_handle = true;
    }

    if (m_target == GL_NONE)
    {
        // Texture has not been bound to a target yet, so we're done.
        return true;
    }

    uint32_t face = 0, level = 0;
    uint8_vec temp_img;

    uint32_t tex_width = 0, tex_height = 0, tex_depth = 0, total_actual_levels = 0, num_faces = 0;
    int base_level = 0, max_level = 0;
    VOGL_NOTE_UNUSED(base_level);
    VOGL_NOTE_UNUSED(max_level);
    bool is_immutable_format = false, is_compressed = false;
    GLenum internal_fmt = GL_NONE;

    const ktx_texture &tex0 = m_textures[0];

    GL_ENTRYPOINT(glBindTexture)(m_target, static_cast<uint32_t>(handle));
    if (vogl_check_gl_error())
        goto handle_error;

    if (m_target == GL_TEXTURE_BUFFER)
    {
        if (m_buffer)
        {
            GLuint buffer_handle = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_buffer));
            if (!buffer_handle)
            {
                vogl_error_printf("Failed remapping buffer handle for buffer texture trace handle %u GL handle %" PRIu64 ", trace buffer %u GL buffer %u\n", m_snapshot_handle, static_cast<uint64_t>(handle), m_buffer, buffer_handle);
                return false;
            }

            internal_fmt = m_params.get_value<int>(GL_TEXTURE_INTERNAL_FORMAT);

            if (!internal_fmt)
            {
                vogl_error_printf("Failed retrieving GL_TEXTURE_INTERNAL_FORMAT for buffer texture trace handle %u GL handle %" PRIu64 ", trace buffer %u GL buffer %u\n", m_snapshot_handle, static_cast<uint64_t>(handle), m_buffer, buffer_handle);
                return false;
            }

            GL_ENTRYPOINT(glTexBuffer)(m_target, internal_fmt, buffer_handle);
            VOGL_CHECK_GL_ERROR;
        }

        return true;
    }

    bool any_failures;
    any_failures = false;

#define SET_INT(pname)                 \
    do                                 \
    {                                  \
        if (!set_tex_parameter(pname)) \
            any_failures = true;       \
    } while (0)
#define SET_FLOAT(pname)               \
    do                                 \
    {                                  \
        if (!set_tex_parameter(pname)) \
            any_failures = true;       \
    } while (0)
    SET_INT(GL_TEXTURE_BASE_LEVEL);
    SET_INT(GL_TEXTURE_MAX_LEVEL);
    SET_FLOAT(GL_TEXTURE_BORDER_COLOR);
    SET_INT(GL_TEXTURE_COMPARE_MODE);
    SET_INT(GL_TEXTURE_COMPARE_FUNC);
    SET_INT(GL_TEXTURE_MIN_FILTER);
    SET_INT(GL_TEXTURE_MAG_FILTER);
    if (m_target != GL_TEXTURE_RECTANGLE)
    {
        SET_FLOAT(GL_TEXTURE_LOD_BIAS);
        SET_FLOAT(GL_TEXTURE_MIN_LOD);
        SET_FLOAT(GL_TEXTURE_MAX_LOD);
    }
    SET_INT(GL_TEXTURE_SWIZZLE_RGBA);
    SET_INT(GL_TEXTURE_WRAP_S);
    SET_INT(GL_TEXTURE_WRAP_T);
    SET_INT(GL_TEXTURE_WRAP_R);

    if (context_info.supports_extension("GL_EXT_texture_filter_anisotropic"))
    {
        SET_FLOAT(GL_TEXTURE_MAX_ANISOTROPY_EXT);
    }

    if (context_info.supports_extension("GL_EXT_texture_sRGB_decode"))
    {
        SET_INT(GL_TEXTURE_SRGB_DECODE_EXT);
    }

    if (!context_info.is_core_profile() && context_info.supports_extension("GL_ARB_shadow_ambient"))
    {
        SET_FLOAT(GL_TEXTURE_COMPARE_FAIL_VALUE_ARB);
    }

    if (!context_info.is_core_profile())
    {
        SET_INT(GL_DEPTH_TEXTURE_MODE);
    }

    // TODO?
    // GL_TEXTURE_PRIORITY
    // GL_TEXTURE_RESIDENT

    if ((m_is_unquerable) || (!m_textures[0].is_valid()))
    {
        if (m_params.get_value<int>(GL_GENERATE_MIPMAP))
        {
            GL_ENTRYPOINT(glGenerateMipmap)(m_target);
        }

        return true;
    }

    if (m_num_samples < 1)
        return false;

    tex_width = tex0.get_width();
    tex_height = tex0.get_height();
    tex_depth = tex0.get_depth();
    total_actual_levels = tex0.get_num_mips();
    base_level = m_params.get_value<int>(GL_TEXTURE_BASE_LEVEL);
    max_level = m_params.get_value<int>(GL_TEXTURE_MAX_LEVEL);

    is_immutable_format = m_params.get_value<int>(GL_TEXTURE_IMMUTABLE_FORMAT) != 0;
    is_compressed = tex0.is_compressed();
    internal_fmt = tex0.get_ogl_internal_fmt();

    num_faces = ((m_target == GL_TEXTURE_CUBE_MAP) || (m_target == GL_TEXTURE_CUBE_MAP_ARRAY)) ? cCubeMapFaces : 1;

    // Sanity checking
    for (uint32_t sample_index = 1; sample_index < m_num_samples; sample_index++)
    {
        const ktx_texture &cmp_tex = m_textures[sample_index];

        if ( (!cmp_tex.is_valid()) || (tex_width != cmp_tex.get_width()) || (tex_height != cmp_tex.get_height()) || (tex_depth != cmp_tex.get_depth()) ||
             (tex0.get_num_mips() != cmp_tex.get_num_mips()) || (tex0.get_array_size() != cmp_tex.get_array_size()) || (tex0.get_num_faces() != cmp_tex.get_num_faces()) ||
             (tex0.get_ogl_fmt() != cmp_tex.get_ogl_fmt()) || (tex0.get_ogl_type() != cmp_tex.get_ogl_type()) || (tex0.get_ogl_internal_fmt() != cmp_tex.get_ogl_internal_fmt()) )
        {
            vogl_error_printf("MSAA consistency error\n");

            VOGL_ASSERT_ALWAYS;
            goto handle_error;
        }
    }

    // TODO: Support immutable textures
    if (is_immutable_format)
    {
        vogl_warning_printf_once("TODO: Support immutable textures (texture will be created non-immutable)\n");
    }

    for (face = 0; face < num_faces; face++)
    {
        for (level = 0; level < total_actual_levels; level++)
        {
            const vogl_state_vector &level_params = m_level_params[face][level];

            if (!level_params.find(GL_TEXTURE_WIDTH))
                continue;

            int level_width = level_params.get_value<int>(GL_TEXTURE_WIDTH);
            int level_height = level_params.get_value<int>(GL_TEXTURE_HEIGHT);
            int level_depth = level_params.get_value<int>(GL_TEXTURE_DEPTH);
            int level_samples = level_params.get_value<int>(GL_TEXTURE_SAMPLES);

            GLenum level_internal_fmt = level_params.get_value<int>(GL_TEXTURE_INTERNAL_FORMAT);

            if ((level_width < 1) || (level_height < 1) || (level_depth < 1) || (level_internal_fmt != internal_fmt))
            {
                vogl_error_printf("Consistency error\n");
                VOGL_ASSERT_ALWAYS;
                goto handle_error;
            }

            if ((m_target == GL_TEXTURE_2D_MULTISAMPLE) || (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
            {
                if ((is_compressed) || (level_samples != static_cast<int>(m_num_samples)) || (level != 0))
                {
                    vogl_error_printf("Multisampled texture consistency error\n");
                    VOGL_ASSERT_ALWAYS;
                    goto handle_error;
                }

                if (m_target == GL_TEXTURE_2D_MULTISAMPLE)
                {
                    GL_ENTRYPOINT(glTexImage2DMultisample)(m_target, m_num_samples, internal_fmt, level_width, level_height, level_params.get_value<GLenum>(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS));
                    VOGL_CHECK_GL_ERROR;
                }
                else if (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
                {
                    GL_ENTRYPOINT(glTexImage3DMultisample)(m_target, m_num_samples, internal_fmt, level_width, level_height, tex0.get_array_size(), level_params.get_value<GLenum>(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS));
                    VOGL_CHECK_GL_ERROR;
                }
                else
                {
                    vogl_error_printf("Unexpected target error\n");
                    VOGL_ASSERT_ALWAYS;
                    goto handle_error;
                }

                // Note: This changes the active GL context to a work context!
                if (!splitter.init())
                {
                    vogl_error_printf("Failed initializing texture splitter object!\n");
                    VOGL_ASSERT_ALWAYS;
                    goto handle_error;
                }

                for (uint32_t sample_index = 0; sample_index < m_num_samples; sample_index++)
                {
                    const uint8_vec &src_img = m_textures[sample_index].get_image_data(level, 0, face, 0);
                    VOGL_ASSERT(src_img.size());

                    GLuint src_texture = 0;
                    GL_ENTRYPOINT(glGenTextures)(1, &src_texture);
                    VOGL_CHECK_GL_ERROR;

                    const GLenum src_target = (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

                    GL_ENTRYPOINT(glBindTexture)(src_target, src_texture);
                    VOGL_CHECK_GL_ERROR;

                    GL_ENTRYPOINT(glTexParameteri)(src_target, GL_TEXTURE_MAX_LEVEL, 0);
                    GL_ENTRYPOINT(glTexParameteri)(src_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    GL_ENTRYPOINT(glTexParameteri)(src_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    VOGL_CHECK_GL_ERROR;

                    if (src_target == GL_TEXTURE_2D_ARRAY)
                    {
                        uint32_t array_size = tex0.get_array_size();

                        temp_img.resize(0);
                        temp_img.reserve(src_img.size() * array_size);

                        for (uint32_t array_index = 0; array_index < array_size; array_index++)
                        {
                            temp_img.append(m_textures[sample_index].get_image_data(level, array_index, face, 0));
                        }

                        GL_ENTRYPOINT(glTexImage3D)(src_target, level, level_internal_fmt, level_width, level_height, array_size, 0, tex0.get_ogl_fmt(), tex0.get_ogl_type(), temp_img.get_ptr());
                        VOGL_CHECK_GL_ERROR;
                    }
                    else
                    {
                        GL_ENTRYPOINT(glTexImage2D)(src_target, level, level_internal_fmt, level_width, level_height, 0, tex0.get_ogl_fmt(), tex0.get_ogl_type(), src_img.get_ptr());
                        VOGL_CHECK_GL_ERROR;
                    }

                    bool status = splitter.combine(src_texture, sample_index, m_target, static_cast<uint32_t>(handle));

                    GL_ENTRYPOINT(glBindTexture)(src_target, 0);
                    VOGL_CHECK_GL_ERROR;

                    GL_ENTRYPOINT(glDeleteTextures)(1, &src_texture);
                    VOGL_CHECK_GL_ERROR;

                    if (!status)
                    {
                        splitter.deinit();

                        goto handle_error;
                    }

                    // Check if dest texture has stencil
                    bool has_stencil = utils::is_in_set<GLenum, GLenum>(internal_fmt, GL_DEPTH_STENCIL, GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH32F_STENCIL8_NV);
                    bool is_reversed_fmt = utils::is_in_set<GLenum, GLenum>(internal_fmt, GL_DEPTH32F_STENCIL8, GL_DEPTH32F_STENCIL8_NV);

                    if (has_stencil)
                    {
                        GLenum temp_color_format = GL_RGBA;
                        GLenum temp_color_type = is_reversed_fmt ? GL_UNSIGNED_INT_8_8_8_8 : GL_UNSIGNED_INT_8_8_8_8_REV;

                        GLuint temp_color_texture = 0;
                        GL_ENTRYPOINT(glGenTextures)(1, &temp_color_texture);
                        VOGL_CHECK_GL_ERROR;

                        const GLenum src_starget = (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

                        GL_ENTRYPOINT(glBindTexture)(src_starget, temp_color_texture);
                        VOGL_CHECK_GL_ERROR;

                        GL_ENTRYPOINT(glTexParameteri)(src_starget, GL_TEXTURE_MAX_LEVEL, 0);
                        GL_ENTRYPOINT(glTexParameteri)(src_starget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        GL_ENTRYPOINT(glTexParameteri)(src_starget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        VOGL_CHECK_GL_ERROR;

                        if (src_starget == GL_TEXTURE_2D_ARRAY)
                        {
                            uint32_t array_size = tex0.get_array_size();

                            temp_img.resize(0);
                            temp_img.reserve(src_img.size() * array_size);

                            for (uint32_t array_index = 0; array_index < array_size; array_index++)
                            {
                                temp_img.append(m_textures[sample_index].get_image_data(level, array_index, face, 0));
                            }

                            GL_ENTRYPOINT(glTexImage3D)(src_starget, level, GL_RGBA, level_width, level_height, array_size, 0, temp_color_format, temp_color_type, temp_img.get_ptr());
                            VOGL_CHECK_GL_ERROR;
                        }
                        else
                        {
                            GL_ENTRYPOINT(glTexImage2D)(src_starget, level, GL_RGBA, level_width, level_height, 0, temp_color_format, temp_color_type, src_img.get_ptr());
                            VOGL_CHECK_GL_ERROR;
                        }

                        status = splitter.copy_color_sample_to_stencil(temp_color_texture, sample_index, m_target, static_cast<uint32_t>(handle));

                        GL_ENTRYPOINT(glBindTexture)(src_starget, 0);
                        VOGL_CHECK_GL_ERROR;

                        GL_ENTRYPOINT(glDeleteTextures)(1, &temp_color_texture);
                        VOGL_CHECK_GL_ERROR;

                        if (!status)
                        {
                            splitter.deinit();

                            vogl_error_printf("Failed copying MSAA stencil samples from temp color texture to stencil\n");

                            goto handle_error;
                        }
                    }
                }

                splitter.deinit();
            }
            else // if ((m_target == GL_TEXTURE_2D_MULTISAMPLE) || (m_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
            {
                GLenum target_to_set = m_target;
                if ((m_target == GL_TEXTURE_CUBE_MAP) || (m_target == GL_TEXTURE_CUBE_MAP_ARRAY))
                    target_to_set = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

                if (level_samples > 1)
                {
                    vogl_error_printf("GL_TEXTURE_SAMPLES consistency error\n");
                    VOGL_ASSERT_ALWAYS;
                    goto handle_error;
                }

                const uint8_vec &src_img = tex0.get_image_data(level, 0, face, 0);
                VOGL_ASSERT(src_img.size());

                switch (m_target)
                {
                    case GL_TEXTURE_1D:
                    {
                        if (is_compressed)
                        {
                            GL_ENTRYPOINT(glCompressedTexImage1D)(target_to_set, level, level_internal_fmt, level_width, 0, src_img.size(), src_img.get_ptr());
                        }
                        else
                        {
                            GL_ENTRYPOINT(glTexImage1D)(target_to_set, level, level_internal_fmt, level_width, 0, tex0.get_ogl_fmt(), tex0.get_ogl_type(), src_img.get_ptr());
                        }

                        break;
                    }
                    case GL_TEXTURE_2D:
                    case GL_TEXTURE_RECTANGLE:
                    case GL_TEXTURE_CUBE_MAP:
                    case GL_TEXTURE_1D_ARRAY:
                    {
                        if (m_target == GL_TEXTURE_1D_ARRAY)
                        {
                            uint32_t array_size = tex0.get_array_size();
                            for (uint32_t array_index = 0; array_index < array_size; array_index++)
                            {
                                temp_img.append(tex0.get_image_data(level, array_index, face, 0));
                            }
                            level_height = array_size;
                        }
                        else
                        {
                            temp_img = src_img;
                        }

                        if (is_compressed)
                        {
                            GL_ENTRYPOINT(glCompressedTexImage2D)(target_to_set, level, level_internal_fmt, level_width, level_height, 0, src_img.size(), temp_img.get_ptr());
                        }
                        else
                        {
                            GL_ENTRYPOINT(glTexImage2D)(target_to_set, level, level_internal_fmt, level_width, level_height, 0, tex0.get_ogl_fmt(), tex0.get_ogl_type(), temp_img.get_ptr());
                        }

                        break;
                    }
                    case GL_TEXTURE_CUBE_MAP_ARRAY:
                    case GL_TEXTURE_2D_ARRAY:
                    case GL_TEXTURE_3D:
                    {
                        temp_img.resize(0);
                        temp_img.reserve(src_img.size() * level_depth);

                        if (m_target == GL_TEXTURE_3D)
                        {
                            for (int zslice = 0; zslice < level_depth; zslice++)
                            {
                                temp_img.append(tex0.get_image_data(level, 0, face, zslice));
                            }
                        }
                        else
                        {
                            // 2D_ARRAY or CUBE_MAP_ARRAY
                            uint32_t array_size = tex0.get_array_size();
                            for (uint32_t array_index = 0; array_index < array_size; array_index++)
                            {
                                temp_img.append(tex0.get_image_data(level, array_index, face, 0));
                            }
                            level_depth = array_size;
                        }

                        if (is_compressed)
                        {
                            GL_ENTRYPOINT(glCompressedTexImage3D)(target_to_set, level, level_internal_fmt, level_width, level_height, level_depth, 0, temp_img.size(), temp_img.get_ptr());
                        }
                        else
                        {
                            GL_ENTRYPOINT(glTexImage3D)(target_to_set, level, level_internal_fmt, level_width, level_height, level_depth, 0, tex0.get_ogl_fmt(), tex0.get_ogl_type(), temp_img.get_ptr());
                        }

                        break;
                    }
                    default:
                    {
                        VOGL_ASSERT_ALWAYS;
                        vogl_error_printf("Unsupported target type %d\n", m_target);
                        goto handle_error;
                    }
                }

                if (vogl_check_gl_error())
                {
                    vogl_error_printf("Failed creating texture image\n");
                    goto handle_error;
                }

            } // sample_index
        } // level
    } // face

    if (m_params.get_value<int>(GL_GENERATE_MIPMAP))
    {
        GL_ENTRYPOINT(glGenerateMipmap)(m_target);
        vogl_debug_printf("Generating mipmaps for texture, snapshot handle %u GL handle %u\n", m_snapshot_handle, (uint32_t)handle);
    }

#undef SET_INT
#undef SET_FLOAT

    if (any_failures)
    {
        vogl_warning_printf("One or more texture params could not be set on trace texture %u, GL texture %" PRIu64 " target %s, dimensions %ux%ux%u, internal format %s\n",
                           m_snapshot_handle, (uint64_t)handle, get_gl_enums().find_gl_name(m_target), tex_width, tex_height, tex_depth, get_gl_enums().find_gl_image_format_name(internal_fmt));
    }

    return true;

handle_error:
    vogl_error_printf("Failed restoring trace texture %u, GL texture %" PRIu64 " target %s, while processing face %u level %u, dimensions %ux%ux%u, internal format %s\n",
                     m_snapshot_handle, (uint64_t)handle, get_gl_enums().find_gl_name(m_target), face, level, tex_width, tex_height, tex_depth, get_gl_enums().find_gl_image_format_name(internal_fmt));

    GL_ENTRYPOINT(glBindTexture)(m_target, 0);
    VOGL_CHECK_GL_ERROR;

    if (created_handle)
    {
        remapper.delete_handle_and_object(VOGL_NAMESPACE_TEXTURES, m_snapshot_handle, handle);

        handle = 0;
    }

    return false;
}

bool vogl_texture_state::remap_handles(vogl_handle_remapper &remapper)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    m_snapshot_handle = static_cast<uint32_t>(remapper.remap_handle(VOGL_NAMESPACE_TEXTURES, m_snapshot_handle));

    if (m_buffer)
    {
        m_buffer = static_cast<GLuint>(remapper.remap_handle(VOGL_NAMESPACE_BUFFERS, m_buffer));
    }

    return true;
}

void vogl_texture_state::clear()
{
    VOGL_FUNC_TRACER

    m_snapshot_handle = 0;
    m_target = GL_NONE;
    m_buffer = 0;

    m_num_samples = 0;

    for (uint32_t i = 0; i < cMaxSamples; i++)
        m_textures[i].clear();

    m_params.clear();
    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(m_level_params); i++)
        m_level_params[i].clear();

    m_is_unquerable = false;
    m_is_valid = false;
}

bool vogl_texture_state::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
        return false;

    node.add_key_value("version", VOGL_SERIALIZED_TEXTURE_STATE_VERSION);
    node.add_key_value("handle", m_snapshot_handle);
    node.add_key_value("target", get_gl_enums().find_gl_name(m_target));
    node.add_key_value("is_unquerable", m_is_unquerable);
    node.add_key_value("buffer", m_buffer);
    node.add_key_value("samples", m_num_samples);

    if ((!m_is_unquerable) && (m_target != GL_NONE))
    {
        json_node &tex_params_obj = node.add_object("tex_params");
        if (!m_params.serialize(tex_params_obj, blob_manager))
            return false;

        if ((m_target != GL_TEXTURE_BUFFER) && (m_num_samples))
        {
            json_node &textures_array_node = node.add_array("textures");

            for (uint32_t sample_index = 0; sample_index < m_num_samples; sample_index++)
            {
                json_node &texture_node = textures_array_node.add_object();

                const ktx_texture &tex = m_textures[sample_index];

                const char *pTex_type = utils::map_value(static_cast<int>(m_target), "tex",
                                                         GL_TEXTURE_1D, "tex_1d",
                                                         GL_TEXTURE_2D, "tex_2d",
                                                         GL_TEXTURE_3D, "tex_3d",
                                                         GL_TEXTURE_CUBE_MAP, "tex_cube",
                                                         GL_TEXTURE_RECTANGLE, "tex_rect",
                                                         GL_TEXTURE_2D_ARRAY, "tex_2d_array",
                                                         GL_TEXTURE_1D_ARRAY, "tex_1d_array",
                                                         GL_TEXTURE_BUFFER, "tex_buffer",
                                                         GL_TEXTURE_2D_MULTISAMPLE, "tex_2d_multisample",
                                                         GL_TEXTURE_2D_MULTISAMPLE_ARRAY, "tex_2d_multisample_array",
                                                         GL_TEXTURE_CUBE_MAP_ARRAY, "tex_cube_array");

                uint32_t actual_mip_levels = m_params.get_value<GLint>(GL_TEXTURE_MAX_LEVEL) + 1;
                if (tex.is_valid())
                    actual_mip_levels = math::minimum(actual_mip_levels, tex.get_num_mips());

                dynamic_string prefix;
                switch (m_target)
                {
                    case GL_TEXTURE_1D:
                        prefix.format("%s_%u_levels_%u_%s", pTex_type, tex.get_width(), actual_mip_levels, get_gl_enums().find_gl_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_RECTANGLE:
                    case GL_TEXTURE_2D:
                        prefix.format("%s_%ux%u_levels_%u_%s", pTex_type, tex.get_width(), tex.get_height(), actual_mip_levels, get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_3D:
                        prefix.format("%s_%ux%ux%u_levels_%u_%s", pTex_type, tex.get_width(), tex.get_height(), tex.get_depth(), actual_mip_levels, get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_1D_ARRAY:
                        prefix.format("%s_%u_levels_%u_arraysize_%u_%s", pTex_type, tex.get_width(), actual_mip_levels, tex.get_array_size(), get_gl_enums().find_gl_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_2D_ARRAY:
                        prefix.format("%s_%ux%u_levels_%u_arraysize_%u_%s", pTex_type, tex.get_width(), tex.get_height(), actual_mip_levels, tex.get_array_size(), get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_2D_MULTISAMPLE:
                        prefix.format("%s_%ux%u_levels_%u_sample_%u_of_%u_%s", pTex_type, tex.get_width(), tex.get_height(), actual_mip_levels, sample_index, m_num_samples, get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
                        prefix.format("%s_%ux%u_levels_%u_sample_%u_of_%u_arraysize_%u_%s", pTex_type, tex.get_width(), tex.get_height(), actual_mip_levels, sample_index, m_num_samples, tex.get_array_size(), get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_CUBE_MAP:
                        prefix.format("%s_%ux%u_levels_%u_%s", pTex_type, tex.get_width(), tex.get_height(), actual_mip_levels, get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    case GL_TEXTURE_CUBE_MAP_ARRAY:
                        prefix.format("%s_%ux%u_levels_%u_arraysize_%u_%s", pTex_type, tex.get_width(), tex.get_height(), actual_mip_levels, tex.get_array_size(), get_gl_enums().find_gl_image_format_name(tex.get_ogl_internal_fmt()));
                        break;
                    default:
                        VOGL_ASSERT_ALWAYS;
                        return false;
                }

                dynamic_stream dyn_stream;
                data_stream_serializer serializer(dyn_stream);
                if (!tex.write_to_stream(serializer))
                    return false;

                dyn_stream.seek(0, false);

                dynamic_string blob_id(blob_manager.add_stream_compute_unique_id(dyn_stream, prefix.get_ptr(), "ktx"));
                if (blob_id.is_empty())
                    return false;

                dyn_stream.close();

                texture_node.add_key_value("texture_data_blob_id", blob_id);
            }

            json_node &level_params_array = node.add_array("level_params");
            for (uint32_t face = 0; face < m_textures[0].get_num_faces(); face++)
            {
                for (uint32_t level = 0; level < m_textures[0].get_num_mips(); level++)
                {
                    json_node &level_param_array_value = level_params_array.add_object();
                    if (m_target == GL_TEXTURE_CUBE_MAP)
                        level_param_array_value.add_key_value("face", face);
                    level_param_array_value.add_key_value("level", level);

                    json_node &params_node = level_param_array_value.add_object("params");
                    if (!m_level_params[face][level].serialize(params_node, blob_manager))
                        return false;
                }
            }
        }
    }

    return true;
}

bool vogl_texture_state::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    if ((!node.has_key("handle")) || (!node.has_key("target")))
        return false;

    m_snapshot_handle = node.value_as_uint32("handle");
    m_target = vogl_get_json_value_as_enum(node, "target");
    m_is_unquerable = node.value_as_bool("is_unquerable");
    m_buffer = node.value_as_uint32("buffer");
    m_num_samples = node.value_as_uint32("samples", 1);

    // Sanity check.
    if (!utils::is_in_set(static_cast<int>(m_target), GL_NONE, GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP_ARRAY,
        GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_RECTANGLE, GL_TEXTURE_BUFFER, GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY))
    {
        return false;
    }

    const json_node *pTex_params_obj = node.find_child_object("tex_params");
    if (pTex_params_obj)
    {
        if (!m_params.deserialize(*pTex_params_obj, blob_manager))
            return false;
    }

    if ((!m_is_unquerable) && (m_target != GL_NONE) && (m_target != GL_TEXTURE_BUFFER))
    {
        if (node.has_key("texture_data_blob_id"))
        {
            if (m_num_samples != 1)
                return false;

            dynamic_string blob_id(node.value_as_string_ptr("texture_data_blob_id"));
            if (blob_id.is_empty())
                return false;

            dynamic_stream tex_data;
            if (!blob_manager.get(blob_id, tex_data.get_buf()))
                return false;

            data_stream_serializer serializer(&tex_data);
            if (!m_textures[0].read_from_stream(serializer))
                return false;
        }
        else if (node.has_array("textures"))
        {
            const json_node *pTextures_array_node = node.find_child_array("textures");
            if ((!pTextures_array_node) || (!pTextures_array_node->size()) || (pTextures_array_node->size() > cMaxSamples))
                return false;

            for (uint32_t i = 0; i < pTextures_array_node->size(); i++)
            {
                const json_node *pTexture_node = pTextures_array_node->get_child(i);
                if (!pTexture_node)
                    return false;

                dynamic_string blob_id(pTexture_node->value_as_string_ptr("texture_data_blob_id"));
                if (blob_id.is_empty())
                    return false;

                dynamic_stream tex_data;
                if (!blob_manager.get(blob_id, tex_data.get_buf()))
                    return false;

                data_stream_serializer serializer(&tex_data);
                if (!m_textures[i].read_from_stream(serializer))
                    return false;
            }
        }
        else
        {
            return false;
        }

        const json_node *pLevel_params_array = node.find_child_array("level_params");
        if (pLevel_params_array)
        {
            for (uint32_t i = 0; i < pLevel_params_array->size(); i++)
            {
                const json_node *pLevel_params_node = pLevel_params_array->get_value_as_object(i);
                if (!pLevel_params_node)
                    return false;

                uint32_t face = pLevel_params_node->value_as_uint32("face");
                if (face)
                {
                    if ((m_target != GL_TEXTURE_CUBE_MAP) || (face > cCubeMapFaces))
                        return false;
                }

                uint32_t level = pLevel_params_node->value_as_uint32("level");
                // obviously crazy level
                if (level > 20)
                    return false;

                if (level >= m_level_params[face].size())
                    m_level_params[face].resize(level + 1);

                const json_node *pParams_node = pLevel_params_node->find_child_object("params");
                if (!pParams_node)
                    return false;

                if (!m_level_params[face][level].deserialize(*pParams_node, blob_manager))
                    return false;
            }
        }
    }

    m_is_valid = true;

    return true;
}

bool vogl_texture_state::compare_restorable_state(const vogl_gl_object_state &rhs_obj) const
{
    VOGL_FUNC_TRACER

    if ((!m_is_valid) || (!rhs_obj.is_valid()))
        return false;

    if (rhs_obj.get_type() != cGLSTTexture)
        return false;

    const vogl_texture_state &rhs = static_cast<const vogl_texture_state &>(rhs_obj);

    if (this == &rhs)
        return true;

#define CMP(x)      \
    if (x != rhs.x) \
        return false;
    CMP(m_is_unquerable);
    CMP(m_target);
    CMP(m_params);
    CMP(m_buffer);

    for (uint32_t i = 0; i < cCubeMapFaces; i++)
        CMP(m_level_params[i]);

    CMP(m_num_samples);

    for (uint32_t i = 0; i < m_num_samples; i++)
    {
        if (m_textures[i].is_valid() != rhs.m_textures[i].is_valid())
            return false;
        if (m_textures[i] != rhs.m_textures[i])
            return false;
    }
#undef CMP

    return true;
}
