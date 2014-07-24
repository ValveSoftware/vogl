/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
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

// File: vogl_ktx_texture.cpp
#include "vogl_core.h"
#include "vogl_ktx_texture.h"
#include "vogl_console.h"
#include "vogl_strutils.h"

// Set #if VOGL_KTX_PVRTEX_WORKAROUNDS to 1 to enable various workarounds for oddball KTX files written by PVRTexTool.
#define VOGL_KTX_PVRTEX_WORKAROUNDS 1

namespace vogl
{
    const uint8_t s_ktx_file_id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

    // true if the specified internal pixel format is compressed
    bool ktx_is_compressed_ogl_fmt(uint32_t ogl_fmt)
    {
        switch (ogl_fmt)
        {
            case KTX_COMPRESSED_RED_RGTC1:
            case KTX_COMPRESSED_SIGNED_RED_RGTC1_EXT:
            case KTX_COMPRESSED_LUMINANCE_LATC1_EXT:
            case KTX_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT:
            case KTX_ETC1_RGB8_OES:
            case KTX_RGB_S3TC:
            case KTX_RGB4_S3TC:
            case KTX_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_R11_EAC:
            case KTX_COMPRESSED_SIGNED_R11_EAC:
            case KTX_COMPRESSED_RGB8_ETC2:
            case KTX_COMPRESSED_SRGB8_ETC2:
            case KTX_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            case KTX_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            case KTX_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
            case KTX_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT:
            case KTX_COMPRESSED_RED_GREEN_RGTC2_EXT:
            case KTX_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
            case KTX_RGBA_S3TC:
            case KTX_RGBA4_S3TC:
            case KTX_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            case KTX_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            case KTX_RGBA_DXT5_S3TC:
            case KTX_RGBA4_DXT5_S3TC:
            case KTX_COMPRESSED_RGB_FXT1_3DFX:
            case KTX_COMPRESSED_RGBA_FXT1_3DFX:
            case KTX_COMPRESSED_RGBA8_ETC2_EAC:
            case KTX_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
            case KTX_COMPRESSED_RG11_EAC:
            case KTX_COMPRESSED_SIGNED_RG11_EAC:
            case KTX_COMPRESSED_RGBA_BPTC_UNORM_ARB:
            case KTX_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
            case KTX_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
            case KTX_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
                VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(ogl_fmt) != 0);
                return true;
            default:
                VOGL_ASSERT(ktx_get_ogl_compressed_base_internal_fmt(ogl_fmt) == 0);
                return false;
        }
    }

    bool ktx_is_packed_pixel_ogl_type(uint32_t ogl_type)
    {
        switch (ogl_type)
        {
            case KTX_UNSIGNED_BYTE_3_3_2:
            case KTX_UNSIGNED_BYTE_2_3_3_REV:
            case KTX_UNSIGNED_SHORT_5_6_5:
            case KTX_UNSIGNED_SHORT_5_6_5_REV:
            case KTX_UNSIGNED_SHORT_4_4_4_4:
            case KTX_UNSIGNED_SHORT_4_4_4_4_REV:
            case KTX_UNSIGNED_SHORT_5_5_5_1:
            case KTX_UNSIGNED_SHORT_1_5_5_5_REV:
            case KTX_UNSIGNED_INT_8_8_8_8:
            case KTX_UNSIGNED_INT_8_8_8_8_REV:
            case KTX_UNSIGNED_INT_10_10_10_2:
            case KTX_UNSIGNED_INT_2_10_10_10_REV:
            case KTX_UNSIGNED_INT_24_8:
            case KTX_UNSIGNED_INT_10F_11F_11F_REV:
            case KTX_UNSIGNED_INT_5_9_9_9_REV:
            case KTX_FLOAT_32_UNSIGNED_INT_24_8_REV:
                return true;
        }
        return false;
    }

    uint32_t ktx_get_ogl_type_size(uint32_t ogl_type)
    {
        switch (ogl_type)
        {
            case KTX_UNSIGNED_BYTE:
            case KTX_BYTE:
                return 1;
            case KTX_HALF_FLOAT:
            case KTX_UNSIGNED_SHORT:
            case KTX_SHORT:
                return 2;
            case KTX_FLOAT:
            case KTX_UNSIGNED_INT:
            case KTX_INT:
                return 4;
            case KTX_UNSIGNED_BYTE_3_3_2:
            case KTX_UNSIGNED_BYTE_2_3_3_REV:
                return 1;
            case KTX_UNSIGNED_SHORT_5_6_5:
            case KTX_UNSIGNED_SHORT_5_6_5_REV:
            case KTX_UNSIGNED_SHORT_4_4_4_4:
            case KTX_UNSIGNED_SHORT_4_4_4_4_REV:
            case KTX_UNSIGNED_SHORT_5_5_5_1:
            case KTX_UNSIGNED_SHORT_1_5_5_5_REV:
                return 2;
            case KTX_UNSIGNED_INT_8_8_8_8:
            case KTX_UNSIGNED_INT_8_8_8_8_REV:
            case KTX_UNSIGNED_INT_10_10_10_2:
            case KTX_UNSIGNED_INT_2_10_10_10_REV:
            case KTX_UNSIGNED_INT_24_8:
            case KTX_UNSIGNED_INT_10F_11F_11F_REV:
            case KTX_UNSIGNED_INT_5_9_9_9_REV:
            case KTX_FLOAT_32_UNSIGNED_INT_24_8_REV: // this was 8, probably a mistake
                return 4;
        }
        return 0;
    }

    uint32_t ktx_get_ogl_compressed_base_internal_fmt(uint32_t ogl_fmt)
    {
        switch (ogl_fmt)
        {
            case KTX_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
            case KTX_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT:
                // Should this be RG? I dunno.
                return KTX_LUMINANCE_ALPHA;

            case KTX_COMPRESSED_LUMINANCE_LATC1_EXT:
            case KTX_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT:
            case KTX_COMPRESSED_R11_EAC:
            case KTX_COMPRESSED_SIGNED_R11_EAC:
            case KTX_COMPRESSED_RED_RGTC1:
            case KTX_COMPRESSED_SIGNED_RED_RGTC1:
                return KTX_RED;

            case KTX_COMPRESSED_RG11_EAC:
            case KTX_COMPRESSED_SIGNED_RG11_EAC:
            case KTX_COMPRESSED_RG_RGTC2:
            case KTX_COMPRESSED_SIGNED_RG_RGTC2:
                return KTX_RG;

            case KTX_ETC1_RGB8_OES:
            case KTX_RGB_S3TC:
            case KTX_RGB4_S3TC:
            case KTX_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_RGB8_ETC2:
            case KTX_COMPRESSED_SRGB8_ETC2:
            case KTX_COMPRESSED_RGB_FXT1_3DFX:
            case KTX_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
            case KTX_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
                return KTX_RGB;

            case KTX_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            case KTX_RGBA_S3TC:
            case KTX_RGBA4_S3TC:
            case KTX_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            case KTX_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            case KTX_RGBA_DXT5_S3TC:
            case KTX_RGBA4_DXT5_S3TC:
            case KTX_COMPRESSED_RGBA_FXT1_3DFX:
            case KTX_COMPRESSED_RGBA_BPTC_UNORM_ARB:
            case KTX_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
            case KTX_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            case KTX_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            case KTX_COMPRESSED_RGBA8_ETC2_EAC:
            case KTX_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
                return KTX_RGBA;
        }
        return 0;
    }

    bool ktx_get_ogl_fmt_desc(uint32_t ogl_fmt, uint32_t ogl_type, uint32_t &block_dim_x, uint32_t &block_dim_y, uint32_t &bytes_per_block)
    {
        uint32_t ogl_type_size = ktx_get_ogl_type_size(ogl_type);

        block_dim_x = 0;
        block_dim_y = 0;
        bytes_per_block = 0;

        switch (ogl_fmt)
        {
            case KTX_COMPRESSED_RED_RGTC1:
            case KTX_COMPRESSED_SIGNED_RED_RGTC1_EXT:
            case KTX_COMPRESSED_LUMINANCE_LATC1_EXT:
            case KTX_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT:
            case KTX_ETC1_RGB8_OES:
            case KTX_RGB_S3TC:
            case KTX_RGB4_S3TC:
            case KTX_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            case KTX_COMPRESSED_R11_EAC:
            case KTX_COMPRESSED_SIGNED_R11_EAC:
            case KTX_COMPRESSED_RGB8_ETC2:
            case KTX_COMPRESSED_SRGB8_ETC2:
            case KTX_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            case KTX_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
            {
                block_dim_x = 4;
                block_dim_y = 4;
                bytes_per_block = 8;
                break;
            }
            case KTX_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
            case KTX_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT:
            case KTX_COMPRESSED_RED_GREEN_RGTC2_EXT:
            case KTX_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
            case KTX_RGBA_S3TC:
            case KTX_RGBA4_S3TC:
            case KTX_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
            case KTX_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            case KTX_RGBA_DXT5_S3TC:
            case KTX_RGBA4_DXT5_S3TC:
            case KTX_COMPRESSED_RGBA8_ETC2_EAC:
            case KTX_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
            case KTX_COMPRESSED_RG11_EAC:
            case KTX_COMPRESSED_SIGNED_RG11_EAC:
            case KTX_COMPRESSED_RGBA_BPTC_UNORM_ARB:
            case KTX_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
            case KTX_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
            case KTX_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
            {
                block_dim_x = 4;
                block_dim_y = 4;
                bytes_per_block = 16;
                break;
            }
            case KTX_COMPRESSED_RGB_FXT1_3DFX:
            case KTX_COMPRESSED_RGBA_FXT1_3DFX:
            {
                block_dim_x = 8;
                block_dim_y = 4;
                bytes_per_block = 16;
                break;               
            }
            case 1:
            case KTX_ALPHA:
            case KTX_RED:
            case KTX_GREEN:
            case KTX_BLUE:
            case KTX_RED_INTEGER:
            case KTX_GREEN_INTEGER:
            case KTX_BLUE_INTEGER:
            case KTX_ALPHA_INTEGER:
            case KTX_LUMINANCE:
            case KTX_DEPTH_COMPONENT:
            case KTX_LUMINANCE_INTEGER_EXT:
            {
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = ogl_type_size;
                break;
            }
            case KTX_R8:
            case KTX_R8UI:
            case KTX_ALPHA8:
            case KTX_LUMINANCE8:
            {
                VOGL_ASSERT(ogl_type_size == 1);
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = 1;
                break;
            }
            case 2:
            case KTX_RG:
            case KTX_RG_INTEGER:
            case KTX_LUMINANCE_ALPHA:
            case KTX_LUMINANCE_ALPHA_INTEGER_EXT:
            {
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = 2 * ogl_type_size;
                break;
            }
            case KTX_RG8:
            case KTX_LUMINANCE8_ALPHA8:
            {
                VOGL_ASSERT(ogl_type_size == 1);
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = 2;
                break;
            }
            case 3:
            case KTX_SRGB:
            case KTX_RGB:
            case KTX_BGR:
            case KTX_RGB_INTEGER:
            case KTX_BGR_INTEGER:
            {
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = ktx_is_packed_pixel_ogl_type(ogl_type) ? ogl_type_size : (3 * ogl_type_size);
                break;
            }
            case KTX_RGB8:
            case KTX_SRGB8:
            {
                VOGL_ASSERT(ogl_type_size == 1);
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = 3;
                break;
            }
            case 4:
            case KTX_RGBA:
            case KTX_BGRA:
            case KTX_RGBA_INTEGER:
            case KTX_BGRA_INTEGER:
            case KTX_SRGB_ALPHA:
            {
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = ktx_is_packed_pixel_ogl_type(ogl_type) ? ogl_type_size : (4 * ogl_type_size);
                break;
            }
            case KTX_SRGB8_ALPHA8:
            case KTX_RGBA8:
            {
                VOGL_ASSERT(ogl_type_size == 1);
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = 4;
                break;
            }
            case KTX_DEPTH_STENCIL:
            {
                block_dim_x = 1;
                block_dim_y = 1;
                bytes_per_block = ktx_is_packed_pixel_ogl_type(ogl_type) ? ogl_type_size : (2 * ogl_type_size);
                break;
            }
            default:
                return false;
        }
        return true;
    }

    bool ktx_texture::compute_pixel_info()
    {
        if ((!m_header.m_glType) || (!m_header.m_glFormat))
        {
            if (!ktx_is_compressed_ogl_fmt(m_header.m_glInternalFormat))
                return false;

            // Must be a compressed format.
            if ((m_header.m_glType) || (m_header.m_glFormat))
                return false;

            if (!ktx_get_ogl_fmt_desc(m_header.m_glInternalFormat, m_header.m_glType, m_block_dim_x, m_block_dim_y, m_bytes_per_block))
                return false;

            if (m_block_dim_x == 1 && m_block_dim_y == 1)
                return false;
        }
        else
        {
            // Must be an uncompressed format.
            if (ktx_is_compressed_ogl_fmt(m_header.m_glInternalFormat))
                return false;

            if (!ktx_get_ogl_fmt_desc(m_header.m_glFormat, m_header.m_glType, m_block_dim_x, m_block_dim_y, m_bytes_per_block))
                return false;

            if (m_block_dim_x > 1 || m_block_dim_y > 1)
                return false;
        }
        return true;
    }

    bool ktx_texture::read_from_stream(data_stream_serializer &serializer)
    {
        clear();

        // Read header
        if (serializer.read(&m_header, 1, sizeof(m_header)) != sizeof(ktx_header))
            return false;

        // Check header
        if (memcmp(s_ktx_file_id, m_header.m_identifier, sizeof(m_header.m_identifier)))
            return false;

        if ((m_header.m_endianness != KTX_OPPOSITE_ENDIAN) && (m_header.m_endianness != KTX_ENDIAN))
            return false;

        m_opposite_endianness = (m_header.m_endianness == KTX_OPPOSITE_ENDIAN);
        if (m_opposite_endianness)
        {
            m_header.endian_swap();

            if ((m_header.m_glTypeSize != sizeof(uint8_t)) && (m_header.m_glTypeSize != sizeof(uint16_t)) && (m_header.m_glTypeSize != sizeof(uint32_t)))
                return false;
        }

        if (!check_header())
            return false;

        if (!compute_pixel_info())
        {
#if VOGL_KTX_PVRTEX_WORKAROUNDS
            // rg [9/10/13] - moved this check into here, instead of in compute_pixel_info(), but need to retest it.
            if ((!m_header.m_glInternalFormat) && (!m_header.m_glType) && (!m_header.m_glTypeSize) && (!m_header.m_glBaseInternalFormat))
            {
                // PVRTexTool writes bogus headers when outputting ETC1.
                vogl_warning_printf("ktx_texture::compute_pixel_info: Header doesn't specify any format, assuming ETC1 and hoping for the best\n");
                m_header.m_glBaseInternalFormat = KTX_RGB;
                m_header.m_glInternalFormat = KTX_ETC1_RGB8_OES;
                m_header.m_glTypeSize = 1;
                m_block_dim_x = 4;
                m_block_dim_y = 4;
                m_bytes_per_block = 8;
            }
            else
#endif
                return false;
        }

        uint8_t pad_bytes[3];

        // Read the key value entries
        uint32_t num_key_value_bytes_remaining = m_header.m_bytesOfKeyValueData;
        while (num_key_value_bytes_remaining)
        {
            if (num_key_value_bytes_remaining < sizeof(uint32_t))
                return false;

            uint32_t key_value_byte_size;
            if (serializer.read(&key_value_byte_size, 1, sizeof(uint32_t)) != sizeof(uint32_t))
                return false;

            num_key_value_bytes_remaining -= sizeof(uint32_t);

            if (m_opposite_endianness)
                key_value_byte_size = utils::swap32(key_value_byte_size);

            if (key_value_byte_size > num_key_value_bytes_remaining)
                return false;

            uint8_vec key_value_data;
            if (key_value_byte_size)
            {
                key_value_data.resize(key_value_byte_size);
                if (serializer.read(&key_value_data[0], 1, key_value_byte_size) != key_value_byte_size)
                    return false;
            }

            m_key_values.push_back(key_value_data);

            uint32_t padding = 3 - ((key_value_byte_size + 3) % 4);
            if (padding)
            {
                if (serializer.read(pad_bytes, 1, padding) != padding)
                    return false;
            }

            num_key_value_bytes_remaining -= key_value_byte_size;
            if (num_key_value_bytes_remaining < padding)
                return false;
            num_key_value_bytes_remaining -= padding;
        }

        // Now read the mip levels
        uint32_t total_faces = get_num_mips() * get_array_size() * get_num_faces() * get_depth();
        if ((!total_faces) || (total_faces > 65535))
            return false;

// See Section 2.8 of KTX file format: No rounding to block sizes should be applied for block compressed textures.
// OK, I'm going to break that rule otherwise KTX can only store a subset of textures that DDS can handle for no good reason.
#if 0
	const uint32_t mip0_row_blocks = m_header.m_pixelWidth / m_block_dim_x;
	const uint32_t mip0_col_blocks = VOGL_MAX(1, m_header.m_pixelHeight) / m_block_dim_y;
#else
        const uint32_t mip0_row_blocks = (m_header.m_pixelWidth + m_block_dim_x - 1) / m_block_dim_x;
        const uint32_t mip0_col_blocks = (VOGL_MAX(1, m_header.m_pixelHeight) + m_block_dim_y - 1) / m_block_dim_y;
#endif
        if ((!mip0_row_blocks) || (!mip0_col_blocks))
            return false;

        const uint32_t mip0_depth = VOGL_MAX(1, m_header.m_pixelDepth);
        VOGL_NOTE_UNUSED(mip0_depth);

        bool has_valid_image_size_fields = true;
        bool disable_mip_and_cubemap_padding = false;

#if VOGL_KTX_PVRTEX_WORKAROUNDS
        {
            // PVRTexTool has a bogus KTX writer that doesn't write any imageSize fields. Nice.
            size_t expected_bytes_remaining = 0;
            for (uint32_t mip_level = 0; mip_level < get_num_mips(); mip_level++)
            {
                uint32_t mip_width, mip_height, mip_depth;
                get_mip_dim(mip_level, mip_width, mip_height, mip_depth);

                const uint32_t mip_row_blocks = (mip_width + m_block_dim_x - 1) / m_block_dim_x;
                const uint32_t mip_col_blocks = (mip_height + m_block_dim_y - 1) / m_block_dim_y;
                if ((!mip_row_blocks) || (!mip_col_blocks))
                    return false;

                expected_bytes_remaining += sizeof(uint32_t);

                if ((!m_header.m_numberOfArrayElements) && (get_num_faces() == 6))
                {
                    for (uint32_t face = 0; face < get_num_faces(); face++)
                    {
                        uint32_t slice_size = mip_row_blocks * mip_col_blocks * m_bytes_per_block;
                        expected_bytes_remaining += slice_size;

                        uint32_t num_cube_pad_bytes = 3 - ((slice_size + 3) % 4);
                        expected_bytes_remaining += num_cube_pad_bytes;
                    }
                }
                else
                {
                    uint32_t total_mip_size = 0;
                    for (uint32_t array_element = 0; array_element < get_array_size(); array_element++)
                    {
                        for (uint32_t face = 0; face < get_num_faces(); face++)
                        {
                            for (uint32_t zslice = 0; zslice < mip_depth; zslice++)
                            {
                                uint32_t slice_size = mip_row_blocks * mip_col_blocks * m_bytes_per_block;
                                total_mip_size += slice_size;
                            }
                        }
                    }
                    expected_bytes_remaining += total_mip_size;

                    uint32_t num_mip_pad_bytes = 3 - ((total_mip_size + 3) % 4);
                    expected_bytes_remaining += num_mip_pad_bytes;
                }
            }

            if (serializer.get_stream()->get_remaining() < expected_bytes_remaining)
            {
                has_valid_image_size_fields = false;
                disable_mip_and_cubemap_padding = true;
                vogl_warning_printf("ktx_texture::read_from_stream: KTX file size is smaller than expected - trying to read anyway without imageSize fields\n");
            }
        }
#endif

        for (uint32_t mip_level = 0; mip_level < get_num_mips(); mip_level++)
        {
            uint32_t mip_width, mip_height, mip_depth;
            get_mip_dim(mip_level, mip_width, mip_height, mip_depth);

            const uint32_t mip_row_blocks = (mip_width + m_block_dim_x - 1) / m_block_dim_x;
            const uint32_t mip_col_blocks = (mip_height + m_block_dim_y - 1) / m_block_dim_y;
            if ((!mip_row_blocks) || (!mip_col_blocks))
                return false;

            uint32_t image_size = 0;
            if (!has_valid_image_size_fields)
            {
                if ((!m_header.m_numberOfArrayElements) && (get_num_faces() == 6))
                {
                    // The KTX file format has an exception for plain cubemap textures, argh.
                    image_size = mip_row_blocks * mip_col_blocks * m_bytes_per_block;
                }
                else
                {
                    image_size = mip_depth * mip_row_blocks * mip_col_blocks * m_bytes_per_block * get_array_size() * get_num_faces();
                }
            }
            else
            {
                if (serializer.read(&image_size, 1, sizeof(image_size)) != sizeof(image_size))
                    return false;

                if (m_opposite_endianness)
                    image_size = utils::swap32(image_size);
            }

            if (!image_size)
                return false;

            uint32_t total_mip_size = 0;

            // The KTX file format has an exception for plain cubemap textures, argh.
            if ((!m_header.m_numberOfArrayElements) && (get_num_faces() == 6))
            {
                // plain non-array cubemap
                for (uint32_t face = 0; face < get_num_faces(); face++)
                {
                    VOGL_ASSERT(m_image_data.size() == get_image_index(mip_level, 0, face, 0));

                    m_image_data.push_back(uint8_vec());
                    uint8_vec &image_data = m_image_data.back();

                    image_data.resize(image_size);
                    if (serializer.read(&image_data[0], 1, image_size) != image_size)
                        return false;

                    if (m_opposite_endianness)
                        utils::endian_swap_mem(&image_data[0], image_size, m_header.m_glTypeSize);

                    uint32_t num_cube_pad_bytes = disable_mip_and_cubemap_padding ? 0 : (3 - ((image_size + 3) % 4));
                    if (serializer.read(pad_bytes, 1, num_cube_pad_bytes) != num_cube_pad_bytes)
                        return false;

                    total_mip_size += image_size + num_cube_pad_bytes;
                }
            }
            else
            {
                uint32_t num_image_bytes_remaining = image_size;

                // 1D, 2D, 3D (normal or array texture), or array cubemap
                for (uint32_t array_element = 0; array_element < get_array_size(); array_element++)
                {
                    for (uint32_t face = 0; face < get_num_faces(); face++)
                    {
                        for (uint32_t zslice = 0; zslice < mip_depth; zslice++)
                        {
                            uint32_t slice_size = mip_row_blocks * mip_col_blocks * m_bytes_per_block;
                            if ((!slice_size) || (slice_size > num_image_bytes_remaining))
                                return false;

                            uint32_t image_index = get_image_index(mip_level, array_element, face, zslice);
                            m_image_data.ensure_element_is_valid(image_index);

                            uint8_vec &image_data = m_image_data[image_index];

                            image_data.resize(slice_size);
                            if (serializer.read(&image_data[0], 1, slice_size) != slice_size)
                                return false;

                            if (m_opposite_endianness)
                                utils::endian_swap_mem(&image_data[0], slice_size, m_header.m_glTypeSize);

                            num_image_bytes_remaining -= slice_size;

                            total_mip_size += slice_size;
                        }
                    }
                }

                if (num_image_bytes_remaining)
                {
                    VOGL_ASSERT_ALWAYS;
                    return false;
                }
            }

            uint32_t num_mip_pad_bytes = disable_mip_and_cubemap_padding ? 0 : (3 - ((total_mip_size + 3) % 4));
            if (serializer.read(pad_bytes, 1, num_mip_pad_bytes) != num_mip_pad_bytes)
                return false;
        }
        return true;
    }

    bool ktx_texture::write_to_stream(data_stream_serializer &serializer, bool no_keyvalue_data) const
    {
        if (!consistency_check())
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        memcpy(m_header.m_identifier, s_ktx_file_id, sizeof(m_header.m_identifier));
        m_header.m_endianness = m_opposite_endianness ? KTX_OPPOSITE_ENDIAN : KTX_ENDIAN;

        if (m_block_dim_x == 1 && m_block_dim_y == 1)
        {
            m_header.m_glTypeSize = ktx_get_ogl_type_size(m_header.m_glType);
            m_header.m_glBaseInternalFormat = m_header.m_glFormat;
        }
        else
        {
            m_header.m_glBaseInternalFormat = ktx_get_ogl_compressed_base_internal_fmt(m_header.m_glInternalFormat);
        }

        m_header.m_bytesOfKeyValueData = 0;
        if (!no_keyvalue_data)
        {
            for (uint32_t i = 0; i < m_key_values.size(); i++)
                m_header.m_bytesOfKeyValueData += sizeof(uint32_t) + ((m_key_values[i].size() + 3) & ~3);
        }

        if (m_opposite_endianness)
            m_header.endian_swap();

        bool success = (serializer.write(&m_header, sizeof(m_header), 1) == 1);

        if (m_opposite_endianness)
            m_header.endian_swap();

        if (!success)
            return success;

        uint32_t total_key_value_bytes = 0;
        const uint8_t padding[3] = { 0, 0, 0 };

        if (!no_keyvalue_data)
        {
            for (uint32_t i = 0; i < m_key_values.size(); i++)
            {
                uint32_t key_value_size = m_key_values[i].size();

                if (m_opposite_endianness)
                    key_value_size = utils::swap32(key_value_size);

                success = (serializer.write(&key_value_size, sizeof(key_value_size), 1) == 1);
                total_key_value_bytes += sizeof(key_value_size);

                if (m_opposite_endianness)
                    key_value_size = utils::swap32(key_value_size);

                if (!success)
                    return false;

                if (key_value_size)
                {
                    if (serializer.write(&m_key_values[i][0], key_value_size, 1) != 1)
                        return false;
                    total_key_value_bytes += key_value_size;

                    uint32_t num_padding = 3 - ((key_value_size + 3) % 4);
                    if ((num_padding) && (serializer.write(padding, num_padding, 1) != 1))
                        return false;
                    total_key_value_bytes += num_padding;
                }
            }
            (void)total_key_value_bytes;
        }

        VOGL_ASSERT(total_key_value_bytes == m_header.m_bytesOfKeyValueData);

        for (uint32_t mip_level = 0; mip_level < get_num_mips(); mip_level++)
        {
            uint32_t mip_width, mip_height, mip_depth;
            get_mip_dim(mip_level, mip_width, mip_height, mip_depth);

            const uint32_t mip_row_blocks = (mip_width + m_block_dim_x - 1) / m_block_dim_x;
            const uint32_t mip_col_blocks = (mip_height + m_block_dim_y - 1) / m_block_dim_y;
            if ((!mip_row_blocks) || (!mip_col_blocks))
                return false;

            uint32_t image_size = mip_row_blocks * mip_col_blocks * m_bytes_per_block;
            if ((m_header.m_numberOfArrayElements) || (get_num_faces() == 1))
                image_size *= (get_array_size() * get_num_faces() * mip_depth);

            if (!image_size)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (m_opposite_endianness)
                image_size = utils::swap32(image_size);

            success = (serializer.write(&image_size, sizeof(image_size), 1) == 1);

            if (m_opposite_endianness)
                image_size = utils::swap32(image_size);

            if (!success)
                return false;

            uint32_t total_mip_size = 0;
            uint32_t total_image_data_size = 0;

            if ((!m_header.m_numberOfArrayElements) && (get_num_faces() == 6))
            {
                // plain non-array cubemap
                for (uint32_t face = 0; face < get_num_faces(); face++)
                {
                    const uint8_vec &image_data = get_image_data(get_image_index(mip_level, 0, face, 0));
                    if ((!image_data.size()) || (image_data.size() != image_size))
                        return false;

                    if (m_opposite_endianness)
                    {
                        uint8_vec tmp_image_data(image_data);
                        utils::endian_swap_mem(&tmp_image_data[0], tmp_image_data.size(), m_header.m_glTypeSize);
                        if (serializer.write(&tmp_image_data[0], tmp_image_data.size(), 1) != 1)
                            return false;
                    }
                    else if (serializer.write(&image_data[0], image_data.size(), 1) != 1)
                        return false;

                    // Not +=, but =, because of the silly image_size plain cubemap exception in the KTX file format
                    total_image_data_size = image_data.size();

                    uint32_t num_cube_pad_bytes = 3 - ((image_data.size() + 3) % 4);
                    if ((num_cube_pad_bytes) && (serializer.write(padding, num_cube_pad_bytes, 1) != 1))
                        return false;

                    total_mip_size += image_size + num_cube_pad_bytes;
                }
            }
            else
            {
                // 1D, 2D, 3D (normal or array texture), or array cubemap
                for (uint32_t array_element = 0; array_element < get_array_size(); array_element++)
                {
                    for (uint32_t face = 0; face < get_num_faces(); face++)
                    {
                        for (uint32_t zslice = 0; zslice < mip_depth; zslice++)
                        {
                            const uint8_vec &image_data = get_image_data(get_image_index(mip_level, array_element, face, zslice));
                            if (!image_data.size())
                                return false;

                            if (m_opposite_endianness)
                            {
                                uint8_vec tmp_image_data(image_data);
                                utils::endian_swap_mem(&tmp_image_data[0], tmp_image_data.size(), m_header.m_glTypeSize);
                                if (serializer.write(&tmp_image_data[0], tmp_image_data.size(), 1) != 1)
                                    return false;
                            }
                            else if (serializer.write(&image_data[0], image_data.size(), 1) != 1)
                                return false;

                            total_image_data_size += image_data.size();

                            total_mip_size += image_data.size();
                        }
                    }
                }

                uint32_t num_mip_pad_bytes = 3 - ((total_mip_size + 3) % 4);
                if ((num_mip_pad_bytes) && (serializer.write(padding, num_mip_pad_bytes, 1) != 1))
                    return false;
                total_mip_size += num_mip_pad_bytes;
            }

            VOGL_ASSERT((total_mip_size & 3) == 0);
            VOGL_ASSERT(total_image_data_size == image_size);
        }

        return true;
    }

    bool ktx_texture::init_1D(uint32_t width, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = width;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 1;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::init_1D_array(uint32_t width, uint32_t num_mips, uint32_t array_size, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = width;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_numberOfArrayElements = array_size;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 1;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::init_2D(uint32_t width, uint32_t height, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = width;
        m_header.m_pixelHeight = height;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 1;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::init_2D_array(uint32_t width, uint32_t height, uint32_t num_mips, uint32_t array_size, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = width;
        m_header.m_pixelHeight = height;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_numberOfArrayElements = array_size;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 1;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::init_3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = width;
        m_header.m_pixelHeight = height;
        m_header.m_pixelDepth = depth;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 1;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::init_cubemap(uint32_t dim, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = dim;
        m_header.m_pixelHeight = dim;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 6;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::init_cubemap_array(uint32_t dim, uint32_t num_mips, uint32_t array_size, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type)
    {
        clear();

        m_header.m_pixelWidth = dim;
        m_header.m_pixelHeight = dim;
        m_header.m_numberOfMipmapLevels = num_mips;
        m_header.m_numberOfArrayElements = array_size;
        m_header.m_glInternalFormat = ogl_internal_fmt;
        m_header.m_glFormat = ogl_fmt;
        m_header.m_glType = ogl_type;
        m_header.m_numberOfFaces = 6;

        if (!compute_pixel_info())
            return false;

        return true;
    }

    bool ktx_texture::check_header() const
    {
        if (((get_num_faces() != 1) && (get_num_faces() != 6)) || (!m_header.m_pixelWidth))
            return false;

        if ((!m_header.m_pixelHeight) && (m_header.m_pixelDepth))
            return false;

        if ((get_num_faces() == 6) && ((m_header.m_pixelDepth) || (!m_header.m_pixelHeight)))
            return false;

#if 0
        if (m_header.m_numberOfMipmapLevels)
        {
            const uint32_t max_mipmap_dimension = 1U << (m_header.m_numberOfMipmapLevels - 1U);
            if (max_mipmap_dimension > (VOGL_MAX(VOGL_MAX(m_header.m_pixelWidth, m_header.m_pixelHeight), m_header.m_pixelDepth)))
                return false;
        }
#endif

        return true;
    }

    uint32_t ktx_texture::get_expected_image_size(uint32_t mip_level) const
    {
        uint32_t mip_width, mip_height, mip_depth;
        get_mip_dim(mip_level, mip_width, mip_height, mip_depth);

        const uint32_t mip_row_blocks = (mip_width + m_block_dim_x - 1) / m_block_dim_x;
        const uint32_t mip_col_blocks = (mip_height + m_block_dim_y - 1) / m_block_dim_y;
        if ((!mip_row_blocks) || (!mip_col_blocks))
        {
            VOGL_ASSERT_ALWAYS;
            return 0;
        }

        return mip_row_blocks * mip_col_blocks * m_bytes_per_block;
    }

    uint32_t ktx_texture::get_total_images() const
    {
        if (!is_valid() || !get_num_mips())
            return 0;

        // bogus:
        //return get_num_mips() * (get_depth() * get_num_faces() * get_array_size());

        // Naive algorithm, could just compute based off the # of mips
        uint32_t max_index = 0;
        for (uint32_t mip_level = 0; mip_level < get_num_mips(); mip_level++)
        {
            uint32_t total_zslices = math::maximum<uint32_t>(get_depth() >> mip_level, 1U);
            uint32_t index = get_image_index(mip_level, get_array_size() - 1, get_num_faces() - 1, total_zslices - 1);
            max_index = math::maximum<uint32_t>(max_index, index);
        }

        return max_index + 1;
    }

    bool ktx_texture::consistency_check() const
    {
        if (!check_header())
            return false;

        uint32_t block_dim_x = 0, block_dim_y = 0, bytes_per_block = 0;
        if ((!m_header.m_glType) || (!m_header.m_glFormat))
        {
            if ((m_header.m_glType) || (m_header.m_glFormat))
                return false;
            if (!ktx_get_ogl_fmt_desc(m_header.m_glInternalFormat, m_header.m_glType, block_dim_x, block_dim_y, bytes_per_block))
                return false;
            if (block_dim_x == 1 && block_dim_y == 1)
                return false;
            //if ((get_width() % block_dim) || (get_height() % block_dim))
            //   return false;
        }
        else
        {
            if (!ktx_get_ogl_fmt_desc(m_header.m_glFormat, m_header.m_glType, block_dim_x, block_dim_y, bytes_per_block))
                return false;
            if (block_dim_x > 1 || block_dim_y > 1)
                return false;
        }
        if ((m_block_dim_x != block_dim_x) || (m_block_dim_y != block_dim_y) || (m_bytes_per_block != bytes_per_block))
            return false;

        uint32_t total_expected_images = get_total_images();
        if (m_image_data.size() != total_expected_images)
            return false;

        for (uint32_t mip_level = 0; mip_level < get_num_mips(); mip_level++)
        {
            uint32_t mip_width, mip_height, mip_depth;
            get_mip_dim(mip_level, mip_width, mip_height, mip_depth);

            const uint32_t mip_row_blocks = (mip_width + m_block_dim_x - 1) / m_block_dim_x;
            const uint32_t mip_col_blocks = (mip_height + m_block_dim_y - 1) / m_block_dim_y;
            if ((!mip_row_blocks) || (!mip_col_blocks))
                return false;

            for (uint32_t array_element = 0; array_element < get_array_size(); array_element++)
            {
                for (uint32_t face = 0; face < get_num_faces(); face++)
                {
                    for (uint32_t zslice = 0; zslice < mip_depth; zslice++)
                    {
                        const uint8_vec &image_data = get_image_data(get_image_index(mip_level, array_element, face, zslice));

                        uint32_t expected_image_size = mip_row_blocks * mip_col_blocks * m_bytes_per_block;
                        if (image_data.size() != expected_image_size)
                            return false;
                    }
                }
            }
        }

        return true;
    }

    void ktx_texture::get_keys(dynamic_string_array &keys) const
    {
        keys.resize(0);
        keys.reserve(m_key_values.size());

        for (uint32_t i = 0; i < m_key_values.size(); i++)
        {
            const uint8_vec &v = m_key_values[i];

            keys.enlarge(1)->set(reinterpret_cast<const char *>(v.get_ptr()));
        }
    }

    const uint8_vec *ktx_texture::find_key(const char *pKey) const
    {
        const uint32_t n = vogl_strlen(pKey) + 1;
        for (uint32_t i = 0; i < m_key_values.size(); i++)
        {
            const uint8_vec &v = m_key_values[i];
            if ((v.size() >= n) && (!memcmp(&v[0], pKey, n)))
                return &v;
        }

        return NULL;
    }

    bool ktx_texture::get_key_value_data(const char *pKey, uint8_vec &data) const
    {
        const uint8_vec *p = find_key(pKey);
        if (!p)
        {
            data.resize(0);
            return false;
        }

        const uint32_t ofs = vogl_strlen(pKey) + 1;
        const uint8_t *pValue = p->get_ptr() + ofs;
        const uint32_t n = p->size() - ofs;

        data.resize(n);
        if (n)
            memcpy(data.get_ptr(), pValue, n);
        return true;
    }

    bool ktx_texture::get_key_value_as_string(const char *pKey, dynamic_string &str) const
    {
        const uint8_vec *p = find_key(pKey);
        if (!p)
        {
            str.clear();
            return false;
        }

        const uint32_t ofs = vogl_strlen(pKey) + 1;
        const uint8_t *pValue = p->get_ptr() + ofs;
        const uint32_t n = p->size() - ofs;

        uint32_t i;
        for (i = 0; i < n; i++)
            if (!pValue[i])
                break;

        str.set_from_buf(pValue, i);
        return true;
    }

    uint32_t ktx_texture::add_key_value(const char *pKey, const void *pVal, uint32_t val_size)
    {
        const uint32_t idx = m_key_values.size();
        m_key_values.resize(idx + 1);
        uint8_vec &v = m_key_values.back();
        v.append(reinterpret_cast<const uint8_t *>(pKey), vogl_strlen(pKey) + 1);
        v.append(static_cast<const uint8_t *>(pVal), val_size);
        return idx;
    }

    bool ktx_texture::operator==(const ktx_texture &rhs) const
    {
        if (this == &rhs)
            return true;

// This is not super deep because I want to avoid poking around into internal state (such as the header)

#define CMP(x)      \
    if (x != rhs.x) \
        return false;
        CMP(get_ogl_internal_fmt());
        CMP(get_width());
        CMP(get_height());
        CMP(get_depth());
        CMP(get_num_mips());
        CMP(get_array_size());
        CMP(get_num_faces());
        CMP(is_compressed());
        CMP(get_block_dim_x());
        CMP(get_block_dim_y());

        // The image fmt/type shouldn't matter with compressed textures.
        if (!is_compressed())
        {
            CMP(get_ogl_fmt());
            CMP(get_ogl_type());
        }

        CMP(get_total_images());

        CMP(get_opposite_endianness());

        // Do an order insensitive key/value comparison.
        dynamic_string_array lhs_keys;
        get_keys(lhs_keys);

        dynamic_string_array rhs_keys;
        rhs.get_keys(rhs_keys);

        if (lhs_keys.size() != rhs_keys.size())
            return false;

        lhs_keys.sort(dynamic_string_less_than_case_sensitive());
        rhs_keys.sort(dynamic_string_less_than_case_sensitive());

        for (uint32_t i = 0; i < lhs_keys.size(); i++)
            if (lhs_keys[i].compare(rhs_keys[i], true) != 0)
                return false;

        for (uint32_t i = 0; i < lhs_keys.size(); i++)
        {
            uint8_vec lhs_data, rhs_data;
            if (!get_key_value_data(lhs_keys[i].get_ptr(), lhs_data))
                return false;
            if (!get_key_value_data(lhs_keys[i].get_ptr(), rhs_data))
                return false;
            if (lhs_data != rhs_data)
                return false;
        }

        // Compare images.
        for (uint32_t l = 0; l < get_num_mips(); l++)
        {
            for (uint32_t a = 0; a < get_array_size(); a++)
            {
                for (uint32_t f = 0; f < get_num_faces(); f++)
                {
                    for (uint32_t z = 0; z < get_depth(); z++)
                    {
                        const uint8_vec &lhs_img = get_image_data(l, a, f, z);
                        const uint8_vec &rhs_img = rhs.get_image_data(l, a, f, z);

                        if (lhs_img != rhs_img)
                            return false;
                    }
                }
            }
        }
#undef CMP
        return true;
    }

} // namespace vogl
