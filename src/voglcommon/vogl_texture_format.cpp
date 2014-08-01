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

// File: vogl_texture_format.cpp
#include "vogl_common.h"
#include "vogl_texture_format.h"
#include "vogl_ktx_texture.h"

//----------------------------------------------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------------------------------------------
static const vogl_internal_tex_format *get_vogl_internal_texture_formats(uint32_t *count)
{
    static const vogl_internal_tex_format s_vogl_internal_texture_formats[] =
    {
        // TODO: Test ALL of these format mappings on AMD and Intel Mesa.
        // TODO: This table mapping internal texture formats was composed by calling NVidia's driver.
        // We should also compose tables for other drivers, and a generic table.
        #include "vogl_internal_texture_formats.inc"
    };
    *count = VOGL_ARRAY_SIZE(s_vogl_internal_texture_formats);

    return s_vogl_internal_texture_formats;
}

typedef vogl::hash_map<GLenum, const vogl_internal_tex_format *> vogl_internal_texture_format_hash_map;

static vogl_internal_texture_format_hash_map& get_internal_format_hash_map()
{
    static vogl_internal_texture_format_hash_map s_internal_format_hash_map;
    return s_internal_format_hash_map;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_devel_dump_internal_texture_formats
// This func is only for testing various internal GL format related API's
// This func is used to generate vogl_internal_texture_formats.inc
//----------------------------------------------------------------------------------------------------------------------
void vogl_devel_dump_internal_texture_formats(const vogl_context_info &context_info)
{
    VOGL_FUNC_TRACER

    VOGL_CHECK_GL_ERROR;

    vogl_scoped_binding_state orig_texture;
    orig_texture.save_textures(&context_info);

    vogl_scoped_state_saver state_saver(cGSTPixelStore, cGSTPixelTransfer);

    vogl_reset_pixel_store_states();
    vogl_reset_pixel_transfer_states(context_info);

#if 0
	// silly experiment
	{
		GLuint handle;
		ACTUAL_GL_ENTRYPOINT(glGenTextures)(1, &handle);
		VOGL_CHECK_GL_ERROR;

		ACTUAL_GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, handle);
		VOGL_CHECK_GL_ERROR;

		for (uint32_t i = 0; i < 256; i++)
		{
			uint8_t vals[4] = { i, 0, 0, 0 };
			//ACTUAL_GL_ENTRYPOINT(glTexImage2D)(GL_TEXTURE_2D, 0, GL_R8_SNORM, 1, 1, 0, GL_RED, GL_BYTE, vals);

			//float vals[1] = { ( i - 128.0f) / 127.0f };
			//float vals[1] = { i / 255.0f };

			//ACTUAL_GL_ENTRYPOINT(glPixelTransferf)(GL_RED_SCALE, .5f);
			//ACTUAL_GL_ENTRYPOINT(glPixelTransferf)(GL_RED_BIAS, 0.5f);

			ACTUAL_GL_ENTRYPOINT(glTexImage2D)(GL_TEXTURE_2D, 0, GL_RGB8UI, 1, 1, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, vals);

			//ACTUAL_GL_ENTRYPOINT(glPixelTransferf)(GL_RED_SCALE, 1.0f);
			//ACTUAL_GL_ENTRYPOINT(glPixelTransferf)(GL_RED_BIAS, 0.0f);

			VOGL_CHECK_GL_ERROR;

			uint16_t gvals[4] = { 0, 0, 0, 0 };
			ACTUAL_GL_ENTRYPOINT(glGetTexImage)(GL_TEXTURE_2D, 0, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, gvals);
			VOGL_CHECK_GL_ERROR;

			printf("%u %u %u %u, %u %u %u %u\n", vals[0], vals[1], vals[2], vals[3],
			       gvals[0], gvals[1], gvals[2], gvals[3]);
		}


		ACTUAL_GL_ENTRYPOINT(glDeleteTextures)(1, &handle);
	}
#endif

    typedef vogl::map<GLenum, vogl_internal_tex_format> tex_format_map;
    tex_format_map internal_formats;

    // Iterate through the base internal fmts, which need some special handling (argh) because the actual internal fmt != the requested internal fmt
    GLenum base_internal_formats[] =
        {
            GL_DEPTH_COMPONENT,
            GL_DEPTH_STENCIL,
            GL_ALPHA,
            GL_RED,
            GL_RG,
            GL_RGB,
            GL_RGBA,
            GL_LUMINANCE,
            GL_LUMINANCE_ALPHA,
            GL_INTENSITY,
            GL_SLUMINANCE,
            GL_SLUMINANCE_ALPHA,
            GL_SRGB,
            GL_SRGB_ALPHA
        };

    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(base_internal_formats); i++)
    {
        printf("%s\n", get_gl_enums().find_gl_name(base_internal_formats[i]));

        GLuint handle;
        GL_ENTRYPOINT(glGenTextures)(1, &handle);
        VOGL_CHECK_GL_ERROR;

        GLenum target = GL_TEXTURE_2D;

        GL_ENTRYPOINT(glBindTexture)(target, handle);

        GLenum base_internal_fmt = base_internal_formats[i];

        vogl_internal_tex_format f;
        GL_ENTRYPOINT(glGetInternalformativ)(target, base_internal_fmt, GL_GET_TEXTURE_IMAGE_TYPE, sizeof(f.m_optimum_get_image_type), (GLint *)&f.m_optimum_get_image_type);
        GL_ENTRYPOINT(glGetInternalformativ)(target, base_internal_fmt, GL_GET_TEXTURE_IMAGE_FORMAT, sizeof(f.m_optimum_get_image_fmt), (GLint *)&f.m_optimum_get_image_fmt);
        VOGL_CHECK_GL_ERROR;

        GLenum &get_fmt = f.m_optimum_get_image_fmt;
        GLenum &get_type = f.m_optimum_get_image_type;

        // manual fixups, ARGH
        switch (base_internal_fmt)
        {
            case GL_DEPTH_COMPONENT:
            {
                get_fmt = GL_DEPTH_COMPONENT;
                get_type = GL_FLOAT;
                break;
            }
            case GL_RG:
            {
                get_fmt = GL_RG;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_RGB:
            {
                get_fmt = GL_RGB;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_RED:
            {
                get_fmt = GL_RED;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_COMPRESSED_LUMINANCE:
            {
                get_fmt = GL_LUMINANCE;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_COMPRESSED_LUMINANCE_ALPHA:
            {
                get_fmt = GL_LUMINANCE_ALPHA;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_COMPRESSED_RGB:
            {
                get_fmt = GL_RGBA;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_COMPRESSED_RGBA:
            {
                get_fmt = GL_RGBA;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_LUMINANCE_ALPHA:
            {
                get_fmt = GL_LUMINANCE_ALPHA;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_SLUMINANCE_ALPHA:
            {
                get_fmt = GL_LUMINANCE_ALPHA;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_SRGB:
            {
                get_fmt = GL_RGB;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            case GL_SRGB_ALPHA:
            {
                get_fmt = GL_RGBA;
                get_type = GL_UNSIGNED_BYTE;
                break;
            }
            default:
            {
                break;
            }
        }

        VOGL_VERIFY(get_fmt != GL_NONE);
        VOGL_VERIFY(get_type != GL_NONE);

        GL_ENTRYPOINT(glTexImage2D)(target, 0, base_internal_fmt, 32, 32, 0, get_fmt, get_type, NULL);
        VOGL_VERIFY(!vogl_check_gl_error());

//bool any_gl_errors = false;

#define GET_INT(dst, gl_enum)                                                  \
    do                                                                         \
    {                                                                          \
        int values[4];                                                         \
        utils::zero_object(values);                                            \
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, 0, (gl_enum), values); \
        (dst) = values[0];                                                     \
    } while (0)

#define GET_BOOL(dst, gl_enum)                                                 \
    do                                                                         \
    {                                                                          \
        int values[4];                                                         \
        utils::zero_object(values);                                            \
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, 0, (gl_enum), values); \
        (dst) = values[0] != 0;                                                \
    } while (0)


        GLenum actual_internal_fmt;
        GET_INT(actual_internal_fmt, GL_TEXTURE_INTERNAL_FORMAT);

        f.m_tex_image_flags = ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
        f.m_fmt = base_internal_fmt;
        f.m_actual_internal_fmt = actual_internal_fmt;
        f.m_name = get_gl_enums().find_name(base_internal_fmt, "gl");

        GET_INT(f.m_comp_sizes[cTCRed], GL_TEXTURE_RED_SIZE);
        GET_INT(f.m_comp_sizes[cTCGreen], GL_TEXTURE_GREEN_SIZE);
        GET_INT(f.m_comp_sizes[cTCBlue], GL_TEXTURE_BLUE_SIZE);
        GET_INT(f.m_comp_sizes[cTCAlpha], GL_TEXTURE_ALPHA_SIZE);
        GET_INT(f.m_comp_sizes[cTCStencil], GL_TEXTURE_STENCIL_SIZE);
        GET_INT(f.m_comp_sizes[cTCDepth], GL_TEXTURE_DEPTH_SIZE);
        GET_INT(f.m_comp_sizes[cTCIntensity], GL_TEXTURE_INTENSITY_SIZE);
        GET_INT(f.m_comp_sizes[cTCLuminance], GL_TEXTURE_LUMINANCE_SIZE);

        GET_INT(f.m_comp_types[cTCRed], GL_TEXTURE_RED_TYPE);
        GET_INT(f.m_comp_types[cTCGreen], GL_TEXTURE_GREEN_TYPE);
        GET_INT(f.m_comp_types[cTCBlue], GL_TEXTURE_BLUE_TYPE);
        GET_INT(f.m_comp_types[cTCAlpha], GL_TEXTURE_ALPHA_TYPE);
        GET_INT(f.m_comp_types[cTCDepth], GL_TEXTURE_DEPTH_TYPE);
        GET_INT(f.m_comp_types[cTCIntensity], GL_TEXTURE_INTENSITY_TYPE);
        GET_INT(f.m_comp_types[cTCLuminance], GL_TEXTURE_LUMINANCE_TYPE);

        GET_INT(f.m_shared_size, GL_TEXTURE_SHARED_SIZE);
        GET_BOOL(f.m_compressed, GL_TEXTURE_COMPRESSED);

        printf("base_internal_fmt: %s get_fmt: %s get_type: %s, actual_internal_fmt: %s compressed: %u\n",
               get_gl_enums().find_gl_name(base_internal_fmt), get_gl_enums().find_gl_name(get_fmt), get_gl_enums().find_gl_name(get_type),
               get_gl_enums().find_gl_name(actual_internal_fmt),
               f.m_compressed);
#undef GET_INT
#undef GET_BOOL

        //VOGL_ASSERT(!any_gl_errors);

        VOGL_ASSERT(f.m_actual_internal_fmt != GL_NONE);
        VOGL_ASSERT(f.m_optimum_get_image_fmt != GL_NONE);
        VOGL_ASSERT(f.m_optimum_get_image_type != GL_NONE);

        VOGL_ASSERT(!f.m_compressed);
        VOGL_ASSERT(!ktx_is_compressed_ogl_fmt(f.m_fmt) && !ktx_is_compressed_ogl_fmt(f.m_actual_internal_fmt));
        VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(f.m_fmt) == 0 && ktx_get_ogl_compressed_base_internal_fmt(f.m_actual_internal_fmt) == 0);

        if (!internal_formats.insert(base_internal_fmt, f).second)
        {
            internal_formats.find_value(base_internal_fmt)->m_tex_image_flags |= ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
        }

        GL_ENTRYPOINT(glBindTexture)(target, 0);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glDeleteTextures)(1, &handle);
        VOGL_CHECK_GL_ERROR;
    }

    for (uint32_t t = 0; t < 5; t++)
    {
        GLenum target = GL_NONE;
        switch (t)
        {
            case 0:
            {
                target = GL_TEXTURE_1D;
                break;
            }
            case 1:
            {
                target = GL_TEXTURE_2D;
                break;
            }
            case 2:
            {
                target = GL_TEXTURE_3D;
                break;
            }
            case 3:
            {
                target = GL_TEXTURE_2D_MULTISAMPLE;
                break;
            }
            case 4:
            {
                target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }
        }

        for (uint32_t fmt = 0; fmt <= 0xFFFF; fmt++)
        {
            GLuint handle;
            GL_ENTRYPOINT(glGenTextures)(1, &handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glBindTexture)(target, handle);

            vogl_debug_message_control(context_info, GL_INVALID_ENUM, false);
            vogl_debug_message_control(context_info, GL_INVALID_OPERATION, false);

            bool failed = false;

            switch (t)
            {
                case 0:
                {
                    GL_ENTRYPOINT(glTexStorage1D)(target, 1, fmt, 32);
                    failed = vogl_check_gl_error_suppress_message();
                    break;
                }
                case 1:
                {
                    GL_ENTRYPOINT(glTexStorage2D)(target, 1, fmt, 32, 32);
                    failed = vogl_check_gl_error_suppress_message();
                    break;
                }
                case 2:
                {
                    GL_ENTRYPOINT(glTexStorage3D)(target, 1, fmt, 32, 32, 32);
                    failed = vogl_check_gl_error_suppress_message();
                    break;
                }
                case 3:
                {
                    GL_ENTRYPOINT(glTexStorage2DMultisample)(target, 2, fmt, 32, 32, GL_TRUE);
                    failed = vogl_check_gl_error_suppress_message();
                    break;
                }
                case 4:
                {
                    GL_ENTRYPOINT(glTexStorage3DMultisample)(target, 2, fmt, 32, 32, 2, GL_TRUE);
                    failed = vogl_check_gl_error_suppress_message();
                    break;
                }
            }

            vogl_debug_message_control(context_info, GL_INVALID_ENUM, true);
            vogl_debug_message_control(context_info, GL_INVALID_OPERATION, true);

            if (failed)
                continue;

            bool any_gl_errors = false;
            VOGL_NOTE_UNUSED(any_gl_errors);

            vogl_internal_tex_format f;
            f.m_tex_image_flags = (1 << t);
            f.m_fmt = fmt;
            f.m_actual_internal_fmt = fmt; // this assumes the actual internal fmt will match here!
            f.m_name = get_gl_enums().find_name(fmt, "gl");

#define GET_INT(dst, gl_enum)                                                  \
    do                                                                         \
    {                                                                          \
        int values[4];                                                         \
        utils::zero_object(values);                                            \
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, 0, (gl_enum), values); \
        (dst) = values[0];                                                     \
    } while (0)

#define GET_BOOL(dst, gl_enum)                                                 \
    do                                                                         \
    {                                                                          \
        int values[4];                                                         \
        utils::zero_object(values);                                            \
        GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, 0, (gl_enum), values); \
        (dst) = values[0] != 0;                                                \
    } while (0)

            GLenum internal_fmt;
            GET_INT(internal_fmt, GL_TEXTURE_INTERNAL_FORMAT);
            VOGL_ASSERT(internal_fmt == fmt);
            GET_INT(f.m_comp_sizes[cTCRed], GL_TEXTURE_RED_SIZE);
            GET_INT(f.m_comp_sizes[cTCGreen], GL_TEXTURE_GREEN_SIZE);
            GET_INT(f.m_comp_sizes[cTCBlue], GL_TEXTURE_BLUE_SIZE);
            GET_INT(f.m_comp_sizes[cTCAlpha], GL_TEXTURE_ALPHA_SIZE);
            GET_INT(f.m_comp_sizes[cTCStencil], GL_TEXTURE_STENCIL_SIZE);
            GET_INT(f.m_comp_sizes[cTCDepth], GL_TEXTURE_DEPTH_SIZE);
            GET_INT(f.m_comp_sizes[cTCIntensity], GL_TEXTURE_INTENSITY_SIZE);
            GET_INT(f.m_comp_sizes[cTCLuminance], GL_TEXTURE_LUMINANCE_SIZE);

            GET_INT(f.m_comp_types[cTCRed], GL_TEXTURE_RED_TYPE);
            GET_INT(f.m_comp_types[cTCGreen], GL_TEXTURE_GREEN_TYPE);
            GET_INT(f.m_comp_types[cTCBlue], GL_TEXTURE_BLUE_TYPE);
            GET_INT(f.m_comp_types[cTCAlpha], GL_TEXTURE_ALPHA_TYPE);
            GET_INT(f.m_comp_types[cTCDepth], GL_TEXTURE_DEPTH_TYPE);
            GET_INT(f.m_comp_types[cTCIntensity], GL_TEXTURE_INTENSITY_TYPE);
            GET_INT(f.m_comp_types[cTCLuminance], GL_TEXTURE_LUMINANCE_TYPE);

            GET_INT(f.m_shared_size, GL_TEXTURE_SHARED_SIZE);
            GET_BOOL(f.m_compressed, GL_TEXTURE_COMPRESSED);
#undef GET_INT
#undef GET_BOOL

            VOGL_ASSERT(!any_gl_errors);

            GL_ENTRYPOINT(glGetInternalformativ)(target, fmt, GL_GET_TEXTURE_IMAGE_TYPE, sizeof(f.m_optimum_get_image_type), (GLint *)&f.m_optimum_get_image_type);
            GL_ENTRYPOINT(glGetInternalformativ)(target, fmt, GL_GET_TEXTURE_IMAGE_FORMAT, sizeof(f.m_optimum_get_image_fmt), (GLint *)&f.m_optimum_get_image_fmt);
            VOGL_CHECK_GL_ERROR;

            if (f.m_compressed)
            {
                f.m_optimum_get_image_fmt = GL_RGBA;
                f.m_optimum_get_image_type = GL_UNSIGNED_BYTE;
            }
            else
            {
#define HANDLE_FMT(gl_enum, fmt, type)     \
    case gl_enum:                          \
    {                                      \
        f.m_optimum_get_image_fmt = fmt;   \
        f.m_optimum_get_image_type = type; \
        break;                             \
    }
                bool unhandled = false;
                switch (fmt)
                {
                    HANDLE_FMT(GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV);
                    HANDLE_FMT(GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV);
                    HANDLE_FMT(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
                    HANDLE_FMT(GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
                    HANDLE_FMT(GL_INTENSITY32F_ARB, GL_RED, GL_FLOAT);

                    HANDLE_FMT(2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(3, GL_RGB, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_LUMINANCE4_ALPHA4, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_LUMINANCE6_ALPHA2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_LUMINANCE8_ALPHA8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_LUMINANCE12_ALPHA4, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_LUMINANCE12_ALPHA12, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_LUMINANCE16_ALPHA16, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT);

                    HANDLE_FMT(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_RGB8I, GL_RGB_INTEGER, GL_BYTE);
                    HANDLE_FMT(GL_RGB10, GL_RGB, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_RGB12, GL_RGB, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_RGBA12, GL_RGB, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_RG8, GL_RG, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_RG16, GL_RG, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_RG16F, GL_RG, GL_HALF_FLOAT);
                    HANDLE_FMT(GL_RG32F, GL_RG, GL_FLOAT);

                    HANDLE_FMT(GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_SLUMINANCE8_ALPHA8, GL_LUMINANCE_ALPHA, GL_BYTE);

                    HANDLE_FMT(GL_RGB32I, GL_RGB_INTEGER, GL_INT);
                    HANDLE_FMT(GL_RGB16I, GL_RGB_INTEGER, GL_SHORT);

                    HANDLE_FMT(GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT);
                    HANDLE_FMT(GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT);
                    HANDLE_FMT(GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_INT);
                    HANDLE_FMT(GL_SIGNED_RGBA8_NV, GL_RGBA, GL_BYTE);
                    HANDLE_FMT(GL_SIGNED_RGB8_NV, GL_RGB, GL_BYTE);
                    HANDLE_FMT(GL_SIGNED_LUMINANCE8_ALPHA8_NV, GL_LUMINANCE_ALPHA, GL_BYTE);
                    HANDLE_FMT(GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV, GL_RGBA, GL_BYTE);
                    HANDLE_FMT(GL_RG8_SNORM, GL_RG, GL_BYTE);
                    HANDLE_FMT(GL_RGB8_SNORM, GL_RGB, GL_BYTE);
                    HANDLE_FMT(GL_RG16_SNORM, GL_RG, GL_SHORT);
                    HANDLE_FMT(GL_RGB16_SNORM, GL_RGB, GL_SHORT);

                    HANDLE_FMT(GL_RGB32F, GL_RGB, GL_FLOAT);
                    HANDLE_FMT(GL_RGB16F, GL_RGB, GL_HALF_FLOAT);

                    // TODO: Research oddball formats
                    HANDLE_FMT(GL_PALETTE4_RGB8_OES, GL_RGB, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_PALETTE4_R5_G6_B5_OES, GL_RGB, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_PALETTE8_RGB8_OES, GL_RGB, GL_UNSIGNED_BYTE);
                    HANDLE_FMT(GL_PALETTE8_R5_G6_B5_OES, GL_RGB, GL_UNSIGNED_BYTE);

                    HANDLE_FMT(GL_HILO16_NV, GL_NONE, GL_NONE);
                    HANDLE_FMT(GL_SIGNED_HILO16_NV, GL_NONE, GL_NONE);
                    HANDLE_FMT(GL_DSDT8_MAG8_INTENSITY8_NV, GL_NONE, GL_NONE);
                    HANDLE_FMT(GL_HILO8_NV, GL_NONE, GL_NONE);
                    HANDLE_FMT(GL_SIGNED_HILO8_NV, GL_NONE, GL_NONE);
                    HANDLE_FMT(GL_DSDT8_NV, GL_NONE, GL_NONE);
                    HANDLE_FMT(GL_DSDT8_MAG8_NV, GL_NONE, GL_NONE);

                    default:
                        unhandled = true;
                        break;
                }

                if ((unhandled) && ((f.m_optimum_get_image_fmt == GL_NONE) || (f.m_optimum_get_image_type == GL_NONE)))
                {
                    printf("INVALID: %s %s %s\n", f.m_name.get_ptr(), get_gl_enums().find_name(f.m_optimum_get_image_fmt, "gl"), get_gl_enums().find_name(f.m_optimum_get_image_type, "gl"));
                }
            }

#undef HANDLE_FMT

            VOGL_ASSERT(f.m_actual_internal_fmt != GL_NONE);

            if ((f.m_optimum_get_image_fmt == GL_NONE) || (f.m_optimum_get_image_type == GL_NONE))
                vogl_warning_printf("Don't have an optimal get format/type for internal format %s\n", get_gl_enums().find_gl_name(fmt));

            VOGL_ASSERT(fmt != GL_LUMINANCE);

            VOGL_ASSERT(f.m_fmt == f.m_actual_internal_fmt);
            if (!f.m_compressed)
            {
                VOGL_ASSERT(!ktx_is_compressed_ogl_fmt(f.m_fmt) && !ktx_is_compressed_ogl_fmt(f.m_actual_internal_fmt));
                VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(f.m_fmt) == 0 && ktx_get_ogl_compressed_base_internal_fmt(f.m_actual_internal_fmt) == 0);
            }
            else
            {
                VOGL_ASSERT(ktx_is_compressed_ogl_fmt(f.m_actual_internal_fmt));
                VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(f.m_actual_internal_fmt) != 0);
            }

            if (!internal_formats.insert(fmt, f).second)
            {
                internal_formats.find_value(fmt)->m_tex_image_flags |= (1 << t);
            }

            GL_ENTRYPOINT(glBindTexture)(target, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDeleteTextures)(1, &handle);
            VOGL_CHECK_GL_ERROR;
        }
    }

    const char *pOutput_filename = "internal_texture_formats.inc";
    FILE *pFile = vogl_fopen(pOutput_filename, "w");
    VOGL_VERIFY(pFile);
    if (!pFile)
        return;

    for (tex_format_map::const_iterator it = internal_formats.begin(); it != internal_formats.end(); ++it)
    {
        vogl_internal_tex_format fmt(it->second);

        uint32_t actual_size = 0;

        if (!fmt.m_compressed)
        {
            VOGL_ASSERT(!ktx_is_compressed_ogl_fmt(fmt.m_fmt));
            VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(fmt.m_fmt) == 0);
        }
        else
        {
            VOGL_ASSERT(ktx_is_compressed_ogl_fmt(fmt.m_fmt));
            VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(fmt.m_fmt) != 0);
        }

        if ((!fmt.m_compressed) && (fmt.m_optimum_get_image_fmt != GL_NONE) && (fmt.m_optimum_get_image_type != GL_NONE))
        {
            GLuint handle;
            GL_ENTRYPOINT(glGenTextures)(1, &handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, handle);
            VOGL_CHECK_GL_ERROR;

            uint8_t vals[128];
            utils::zero_object(vals);
            vals[1] = 64;
            GL_ENTRYPOINT(glTexImage2D)(GL_TEXTURE_2D, 0, fmt.m_fmt, 1, 1, 0, fmt.m_optimum_get_image_fmt, fmt.m_optimum_get_image_type, vals);
            if (vogl_check_gl_error())
            {
                printf("glTexImage2D FAILED: %s %s %s\n", fmt.m_name.get_ptr(), get_gl_enums().find_name(fmt.m_optimum_get_image_fmt, "gl"), get_gl_enums().find_name(fmt.m_optimum_get_image_type, "gl"));
            }

            uint8_t gvals[128];
            memset(gvals, 0xCD, sizeof(gvals));
            GL_ENTRYPOINT(glGetTexImage)(GL_TEXTURE_2D, 0, fmt.m_optimum_get_image_fmt, fmt.m_optimum_get_image_type, gvals);

            uint32_t actual_size0 = 0;
            for (actual_size0 = 0; actual_size0 < sizeof(gvals); actual_size0++)
                if (gvals[actual_size0] == 0xCD)
                    break;

            memset(gvals, 0x12, sizeof(gvals));
            GL_ENTRYPOINT(glGetTexImage)(GL_TEXTURE_2D, 0, fmt.m_optimum_get_image_fmt, fmt.m_optimum_get_image_type, gvals);

            uint32_t actual_size1 = 0;
            for (actual_size1 = 0; actual_size1 < sizeof(gvals); actual_size1++)
                if (gvals[actual_size1] == 0x12)
                    break;

            VOGL_VERIFY(actual_size0 == actual_size1);

            //printf("glGetTexImage() wrote %u bytes\n", actual_size0);

            if (vogl_check_gl_error()) // || gvals[1] != vals[1])
            {
                printf("glGetTexImage() failed: %s %s %s\n", fmt.m_name.get_ptr(), get_gl_enums().find_name(fmt.m_optimum_get_image_fmt, "gl"), get_gl_enums().find_name(fmt.m_optimum_get_image_type, "gl"));
            }

            GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDeleteTextures)(1, &handle);

            actual_size = actual_size0;

            uint32_t s = vogl_get_image_format_size_in_bytes(fmt.m_optimum_get_image_fmt, fmt.m_optimum_get_image_type);
            VOGL_VERIFY(s);
            if (s != actual_size0)
            {
                VOGL_VERIFY(0);
            }

            vogl::ktx_texture ktx_tex;
            GLenum img_fmt;
            GLenum img_type;
            img_fmt = fmt.m_optimum_get_image_fmt;
            img_type = fmt.m_optimum_get_image_type;

            uint32_t block_dim_x, block_dim_y, bytes_per_block;
            bool success = ktx_get_ogl_fmt_desc(img_fmt, img_type, block_dim_x, block_dim_y, bytes_per_block);
            VOGL_VERIFY(success);
            VOGL_VERIFY(block_dim_x == 1);
            VOGL_VERIFY(block_dim_y == 1);
            VOGL_VERIFY(bytes_per_block == actual_size);

            if (!ktx_tex.init_2D(1, 1, 1, fmt.m_fmt, img_fmt, img_type))
            {
                printf("ktx_texture::init_2D() failed: %s %s %s\n", fmt.m_name.get_ptr(), get_gl_enums().find_name(fmt.m_optimum_get_image_fmt, "gl"), get_gl_enums().find_name(fmt.m_optimum_get_image_type, "gl"));
            }
        }
        else if (fmt.m_compressed)
        {
            GLuint handle;
            GL_ENTRYPOINT(glGenTextures)(1, &handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, handle);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glTexStorage2D)(GL_TEXTURE_2D, 1, fmt.m_fmt, 1, 1);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glGetTexLevelParameteriv)(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, (GLint *)&actual_size);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glBindTexture)(GL_TEXTURE_2D, 0);
            VOGL_CHECK_GL_ERROR;

            GL_ENTRYPOINT(glDeleteTextures)(1, &handle);
            VOGL_CHECK_GL_ERROR;

            uint32_t block_width = 0, block_height = 0, block_size = 0;
            GL_ENTRYPOINT(glGetInternalformativ)(GL_TEXTURE_2D, fmt.m_fmt, GL_TEXTURE_COMPRESSED_BLOCK_WIDTH, sizeof(int), reinterpret_cast<GLint *>(&block_width));
            GL_ENTRYPOINT(glGetInternalformativ)(GL_TEXTURE_2D, fmt.m_fmt, GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT, sizeof(int), reinterpret_cast<GLint *>(&block_height));
            GL_ENTRYPOINT(glGetInternalformativ)(GL_TEXTURE_2D, fmt.m_fmt, GL_TEXTURE_COMPRESSED_BLOCK_SIZE, sizeof(int), reinterpret_cast<GLint *>(&block_size));
            VOGL_CHECK_GL_ERROR;

            if (block_size == actual_size * 8U)
                block_size /= 8;

            uint32_t block_dim_x, block_dim_y, bytes_per_block;
            bool success = ktx_get_ogl_fmt_desc(fmt.m_fmt, GL_UNSIGNED_BYTE, block_dim_x, block_dim_y, bytes_per_block);
            if ((!success) || (block_dim_x != block_width) || (block_dim_y != block_height) || (bytes_per_block != actual_size) || (bytes_per_block != block_size))
            {
                printf("ktx_get_ogl_fmt_desc on compressed format failed: %s %s %s %u %i %i %i\n", fmt.m_name.get_ptr(), get_gl_enums().find_name(fmt.m_optimum_get_image_fmt, "gl"), get_gl_enums().find_name(fmt.m_optimum_get_image_type, "gl"), actual_size, block_width, block_height, block_size);
            }

            fmt.m_block_width = block_width;
            fmt.m_block_height = block_height;

            vogl::ktx_texture ktx_tex;
            if (!ktx_tex.init_2D(1, 1, 1, fmt.m_fmt, GL_NONE, GL_NONE))
            {
                printf("ktx_texture::init_2D() compressed failed: %s %s %s\n", fmt.m_name.get_ptr(), get_gl_enums().find_name(fmt.m_optimum_get_image_fmt, "gl"), get_gl_enums().find_name(fmt.m_optimum_get_image_type, "gl"));
            }
        }

        fmt.m_image_bytes_per_pixel_or_block = actual_size;

        fprintf(pFile, "   vogl_internal_tex_format(0x%04X, \"%s\", 0x%04X,\n", fmt.m_fmt, fmt.m_name.get_ptr(), fmt.m_actual_internal_fmt);

        fprintf(pFile, "      ");
        for (uint32_t i = 0; i < cTCTotalComponents; i++)
            fprintf(pFile, "%u, ", fmt.m_comp_sizes[i]);
        fprintf(pFile, "\n");

        fprintf(pFile, "      ");
        for (uint32_t i = 0; i < cTCTotalComponents; i++)
            fprintf(pFile, "%s, ", get_gl_enums().find_name(fmt.m_comp_types[i], "gl"));
        fprintf(pFile, "\n");

        fprintf(pFile, "      %u, 0x%02X, %u, \n", fmt.m_shared_size, fmt.m_tex_image_flags, fmt.m_compressed);
        fprintf(pFile, "      %s, %s, %u, %u, %u),\n",
                get_gl_enums().find_name(fmt.m_optimum_get_image_fmt, "gl"),
                get_gl_enums().find_name(fmt.m_optimum_get_image_type, "gl"),
                fmt.m_image_bytes_per_pixel_or_block,
                fmt.m_block_width, fmt.m_block_height);

#if 0
        uint32_t tex_formats_count;
        static const vogl_internal_tex_format *tex_formats = get_vogl_internal_texture_formats(&tex_formats_count);
		for (uint32_t q = 0; q < tex_formats_count; q++)
		{
			if (tex_formats[q].m_fmt == fmt.m_fmt)
			{
				if (!tex_formats[q].compare(fmt))
				{
					VOGL_ASSERT_ALWAYS;
				}
				break;
			}
		}
		if (q == tex_formats_count)
		{
			VOGL_ASSERT_ALWAYS;
		}
#endif
    }

    vogl_fclose(pFile);

    printf("Wrote file %s\n", pOutput_filename);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_internal_tex_format::vogl_internal_tex_format
//----------------------------------------------------------------------------------------------------------------------
vogl_internal_tex_format::vogl_internal_tex_format(
    GLenum fmt, const char *pName, GLenum actual_fmt,
    int s0, int s1, int s2, int s3, int s4, int s5, int s6, int s7,
    GLenum t0, GLenum t1, GLenum t2, GLenum t3, GLenum t4, GLenum t5, GLenum t6, GLenum t7,
    int shared_size, int tex_storage_type_flags, bool compressed, GLenum optimum_get_fmt, GLenum optimum_get_type,
    uint32_t image_bytes_per_pixel_or_block, uint32_t block_width, uint32_t block_height)
{
    VOGL_FUNC_TRACER

    //VOGL_VERIFY(optimum_get_fmt != GL_NONE);
    //VOGL_VERIFY(optimum_get_type != GL_NONE);

    m_fmt = fmt;
    m_name = pName;
    m_actual_internal_fmt = actual_fmt;
    m_comp_sizes[0] = s0;
    m_comp_sizes[1] = s1;
    m_comp_sizes[2] = s2;
    m_comp_sizes[3] = s3;
    m_comp_sizes[4] = s4;
    m_comp_sizes[5] = s5;
    m_comp_sizes[6] = s6;
    m_comp_sizes[7] = s7;
    m_comp_types[0] = t0;
    m_comp_types[1] = t1;
    m_comp_types[2] = t2;
    m_comp_types[3] = t3;
    m_comp_types[4] = t4;
    m_comp_types[5] = t5;
    m_comp_types[6] = t6;
    m_comp_types[7] = t7;
    m_shared_size = shared_size;
    m_tex_image_flags = tex_storage_type_flags;
    m_compressed = compressed;
    m_optimum_get_image_fmt = optimum_get_fmt;
    m_optimum_get_image_type = optimum_get_type;
    m_image_bytes_per_pixel_or_block = image_bytes_per_pixel_or_block;
    m_block_width = block_width;
    m_block_height = block_height;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_internal_tex_format::compare
//----------------------------------------------------------------------------------------------------------------------
bool vogl_internal_tex_format::compare(const vogl_internal_tex_format &rhs) const
{
    VOGL_FUNC_TRACER

#define CMP(x)      \
    if (x != rhs.x) \
        return false;
    CMP(m_fmt);
    CMP(m_name);
    CMP(m_actual_internal_fmt);
    for (uint32_t i = 0; i < cTCTotalComponents; i++)
    {
        CMP(m_comp_sizes[i]);
        CMP(m_comp_types[i]);
    }
    CMP(m_shared_size);
    CMP(m_tex_image_flags);
    CMP(m_optimum_get_image_fmt);
    CMP(m_optimum_get_image_type);
    CMP(m_image_bytes_per_pixel_or_block);
    CMP(m_block_width);
    CMP(m_block_height);
    CMP(m_compressed);
#undef CMP

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_texture_format_init
//----------------------------------------------------------------------------------------------------------------------
void vogl_texture_format_init()
{
    VOGL_FUNC_TRACER

    uint32_t tex_formats_count;
    static const vogl_internal_tex_format *tex_formats = get_vogl_internal_texture_formats(&tex_formats_count);

    VOGL_ASSERT(tex_formats_count && tex_formats[0].m_fmt);

    get_internal_format_hash_map().reserve(tex_formats_count);

    for (uint32_t i = 0; i < tex_formats_count; i++)
    {
        bool success = get_internal_format_hash_map().insert(tex_formats[i].m_fmt, &tex_formats[i]).second;
        VOGL_VERIFY(success);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_find_internal_texture_format
//----------------------------------------------------------------------------------------------------------------------
const vogl_internal_tex_format *vogl_find_internal_texture_format(GLenum internal_format)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(get_internal_format_hash_map().size());

    const vogl_internal_tex_format **ppFmt = get_internal_format_hash_map().find_value(internal_format);
    return ppFmt ? *ppFmt : NULL;
}
