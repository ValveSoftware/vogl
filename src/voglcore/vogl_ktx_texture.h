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

// File: vogl_ktx_texture.h
#pragma once

#include "vogl_core.h"
#include "vogl_data_stream_serializer.h"

#define KTX_ENDIAN 0x04030201
#define KTX_OPPOSITE_ENDIAN 0x01020304

namespace vogl
{
    extern const uint8_t s_ktx_file_id[12];

    struct ktx_header
    {
        uint8_t m_identifier[12];
        uint32_t m_endianness;
        uint32_t m_glType;
        uint32_t m_glTypeSize;
        uint32_t m_glFormat;
        uint32_t m_glInternalFormat;
        uint32_t m_glBaseInternalFormat;
        uint32_t m_pixelWidth;
        uint32_t m_pixelHeight;
        uint32_t m_pixelDepth;
        uint32_t m_numberOfArrayElements;
        uint32_t m_numberOfFaces;
        uint32_t m_numberOfMipmapLevels;
        uint32_t m_bytesOfKeyValueData;

        void clear()
        {
            memset(this, 0, sizeof(*this));
        }

        void endian_swap()
        {
            utils::endian_swap_mem32(&m_endianness, (sizeof(*this) - sizeof(m_identifier)) / sizeof(uint32_t));
        }
    };

    typedef vogl::vector<uint8_vec> ktx_key_value_vec;
    typedef vogl::vector<uint8_vec> ktx_image_data_vec;

    // Compressed pixel data formats: ETC1, DXT1, DXT3, DXT5
    // These enums should match the OpenGL enums
    enum
    {
        KTX_ETC1_RGB8_OES = 0x8D64,
        KTX_RGB_S3TC = 0x83A0,
        KTX_RGB4_S3TC = 0x83A1,
        KTX_COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0,
        KTX_COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1,
        KTX_COMPRESSED_SRGB_S3TC_DXT1_EXT = 0x8C4C,
        KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT = 0x8C4D,
        KTX_COMPRESSED_RED_RGTC1 = 0x8DBB,
        KTX_COMPRESSED_SIGNED_RED_RGTC1 = 0x8DBC,
        KTX_COMPRESSED_RG_RGTC2 = 0x8DBD,
        KTX_COMPRESSED_SIGNED_RG_RGTC2 = 0x8DBE,
        KTX_RGBA_S3TC = 0x83A2,
        KTX_RGBA4_S3TC = 0x83A3,
        KTX_COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2,
        KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT = 0x8C4E,
        KTX_COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3,
        KTX_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT = 0x8C4F,
        KTX_RGBA_DXT5_S3TC = 0x83A4,
        KTX_RGBA4_DXT5_S3TC = 0x83A5,
        KTX_COMPRESSED_RGB_FXT1_3DFX = 0x86B0,
        KTX_COMPRESSED_RGBA_FXT1_3DFX = 0x86B1,
        KTX_COMPRESSED_SIGNED_RED_RGTC1_EXT = 0x8DBC,
        KTX_COMPRESSED_RED_GREEN_RGTC2_EXT = 0x8DBD,
        KTX_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT = 0x8DBE,
        KTX_COMPRESSED_LUMINANCE_LATC1_EXT = 0x8C70,
        KTX_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT = 0x8C71,
        KTX_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT = 0x8C72,
        KTX_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT = 0x8C73,
        KTX_COMPRESSED_RGBA_BPTC_UNORM_ARB = 0x8E8C,
        KTX_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB = 0x8E8D,
        KTX_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB = 0x8E8E,
        KTX_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB = 0x8E8F,
        KTX_COMPRESSED_R11_EAC = 0x9270,
        KTX_COMPRESSED_SIGNED_R11_EAC = 0x9271,
        KTX_COMPRESSED_RG11_EAC = 0x9272,
        KTX_COMPRESSED_SIGNED_RG11_EAC = 0x9273,
        KTX_COMPRESSED_RGB8_ETC2 = 0x9274,
        KTX_COMPRESSED_SRGB8_ETC2 = 0x9275,
        KTX_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9276,
        KTX_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9277,
        KTX_COMPRESSED_RGBA8_ETC2_EAC = 0x9278,
        KTX_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC = 0x9279
    };

    // Pixel formats (various internal, base, and base internal formats)
    enum
    {
        KTX_R8 = 0x8229,
        KTX_R8UI = 0x8232,
        KTX_RGB8 = 0x8051,
        KTX_SRGB8 = 0x8C41,
        KTX_SRGB = 0x8C40,
        KTX_SRGB_ALPHA = 0x8C42,
        KTX_SRGB8_ALPHA8 = 0x8C43,
        KTX_RGBA8 = 0x8058,
        KTX_STENCIL_INDEX = 0x1901,
        KTX_DEPTH_COMPONENT = 0x1902,
        KTX_DEPTH_STENCIL = 0x84F9,
        KTX_RED = 0x1903,
        KTX_GREEN = 0x1904,
        KTX_BLUE = 0x1905,
        KTX_ALPHA = 0x1906,
        KTX_RG = 0x8227,
        KTX_RGB = 0x1907,
        KTX_RGBA = 0x1908,
        KTX_BGR = 0x80E0,
        KTX_BGRA = 0x80E1,
        KTX_RED_INTEGER = 0x8D94,
        KTX_GREEN_INTEGER = 0x8D95,
        KTX_BLUE_INTEGER = 0x8D96,
        KTX_ALPHA_INTEGER = 0x8D97,
        KTX_LUMINANCE_INTEGER_EXT = 0x8D9C,
        KTX_LUMINANCE_ALPHA_INTEGER_EXT = 0x8D9D,
        KTX_RGB_INTEGER = 0x8D98,
        KTX_RGBA_INTEGER = 0x8D99,
        KTX_BGR_INTEGER = 0x8D9A,
        KTX_BGRA_INTEGER = 0x8D9B,
        KTX_LUMINANCE = 0x1909,
        KTX_LUMINANCE_ALPHA = 0x190A,
        KTX_RG_INTEGER = 0x8228,
        KTX_RG8 = 0x822B,
        KTX_ALPHA8 = 0x803C,
        KTX_LUMINANCE8 = 0x8040,
        KTX_LUMINANCE8_ALPHA8 = 0x8045,
        KTX_DEPTH_COMPONENT24 = 0x81A6
    };

    // Pixel data types
    enum
    {
        KTX_UNSIGNED_BYTE = 0x1401,
        KTX_BYTE = 0x1400,
        KTX_UNSIGNED_SHORT = 0x1403,
        KTX_SHORT = 0x1402,
        KTX_UNSIGNED_INT = 0x1405,
        KTX_INT = 0x1404,
        KTX_HALF_FLOAT = 0x140B,
        KTX_FLOAT = 0x1406,
        KTX_UNSIGNED_BYTE_3_3_2 = 0x8032,
        KTX_UNSIGNED_BYTE_2_3_3_REV = 0x8362,
        KTX_UNSIGNED_SHORT_5_6_5 = 0x8363,
        KTX_UNSIGNED_SHORT_5_6_5_REV = 0x8364,
        KTX_UNSIGNED_SHORT_4_4_4_4 = 0x8033,
        KTX_UNSIGNED_SHORT_4_4_4_4_REV = 0x8365,
        KTX_UNSIGNED_SHORT_5_5_5_1 = 0x8034,
        KTX_UNSIGNED_SHORT_1_5_5_5_REV = 0x8366,
        KTX_UNSIGNED_INT_8_8_8_8 = 0x8035,
        KTX_UNSIGNED_INT_8_8_8_8_REV = 0x8367,
        KTX_UNSIGNED_INT_10_10_10_2 = 0x8036,
        KTX_UNSIGNED_INT_2_10_10_10_REV = 0x8368,
        KTX_UNSIGNED_INT_24_8 = 0x84FA,
        KTX_UNSIGNED_INT_10F_11F_11F_REV = 0x8C3B,
        KTX_UNSIGNED_INT_5_9_9_9_REV = 0x8C3E,
        KTX_FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD
    };

    bool ktx_is_compressed_ogl_fmt(uint32_t ogl_fmt);
    bool ktx_is_packed_pixel_ogl_type(uint32_t ogl_type);
    uint32_t ktx_get_ogl_type_size(uint32_t ogl_type);
    bool ktx_get_ogl_fmt_desc(uint32_t ogl_fmt, uint32_t ogl_type, uint32_t &block_dim_x, uint32_t &block_dim_y, uint32_t &bytes_per_block);
    uint32_t ktx_get_ogl_compressed_base_internal_fmt(uint32_t ogl_fmt);

    class ktx_texture
    {
    public:
        ktx_texture()
        {
            clear();
        }

        ktx_texture(const ktx_texture &other)
        {
            *this = other;
        }

        ktx_texture &operator=(const ktx_texture &rhs)
        {
            if (this == &rhs)
                return *this;

            clear();

            m_header = rhs.m_header;
            m_key_values = rhs.m_key_values;
            m_image_data = rhs.m_image_data;
            m_block_dim_x = rhs.m_block_dim_x;
            m_block_dim_y = rhs.m_block_dim_y;
            m_bytes_per_block = rhs.m_bytes_per_block;
            m_opposite_endianness = rhs.m_opposite_endianness;

            return *this;
        }

        void clear()
        {
            m_header.clear();
            m_key_values.clear();
            m_image_data.clear();

            m_block_dim_x = 0;
            m_block_dim_y = 0;
            m_bytes_per_block = 0;

            m_opposite_endianness = false;
        }

        // High level methods
        bool read_from_stream(data_stream_serializer &serializer);
        bool write_to_stream(data_stream_serializer &serializer, bool no_keyvalue_data = false) const;

        // For compressed internal formats, set ogl_fmt and ogl_type to 0 (GL_NONE). The base internal format will be computed automatically.
        // Otherwise, if ogl_fmt/ogl_type are not 0 (GL_NONE) then the internal format must be uncompressed, and all fmt's must be valid.
        bool init_1D(uint32_t width, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);
        bool init_1D_array(uint32_t width, uint32_t num_mips, uint32_t array_size, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);
        bool init_2D(uint32_t width, uint32_t height, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);
        bool init_2D_array(uint32_t width, uint32_t height, uint32_t num_mips, uint32_t array_size, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);
        bool init_3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);
        bool init_cubemap(uint32_t dim, uint32_t num_mips, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);
        bool init_cubemap_array(uint32_t dim, uint32_t num_mips, uint32_t array_size, uint32_t ogl_internal_fmt, uint32_t ogl_fmt, uint32_t ogl_type);

        bool check_header() const;
        bool consistency_check() const;

        // General info

        bool is_valid() const
        {
            return (m_header.m_pixelWidth > 0) && (m_image_data.size() > 0);
        }

        uint32_t get_width() const
        {
            return m_header.m_pixelWidth;
        }
        uint32_t get_height() const
        {
            return VOGL_MAX(m_header.m_pixelHeight, 1);
        }
        uint32_t get_depth() const
        {
            return VOGL_MAX(m_header.m_pixelDepth, 1);
        }
        uint32_t get_num_mips() const
        {
            return VOGL_MAX(m_header.m_numberOfMipmapLevels, 1);
        }
        uint32_t get_array_size() const
        {
            return VOGL_MAX(m_header.m_numberOfArrayElements, 1);
        }
        uint32_t get_num_faces() const
        {
            return m_header.m_numberOfFaces;
        }

        uint32_t get_ogl_type() const
        {
            return m_header.m_glType;
        }
        uint32_t get_ogl_fmt() const
        {
            return m_header.m_glFormat;
        }
        uint32_t get_ogl_base_fmt() const
        {
            return m_header.m_glBaseInternalFormat;
        }
        uint32_t get_ogl_internal_fmt() const
        {
            return m_header.m_glInternalFormat;
        }

        uint32_t get_total_images() const;

        bool is_compressed() const
        {
            return (m_block_dim_x > 1) || (m_block_dim_y > 1);
        }
        bool is_uncompressed() const
        {
            return !is_compressed();
        }

        bool get_opposite_endianness() const
        {
            return m_opposite_endianness;
        }
        void set_opposite_endianness(bool flag)
        {
            m_opposite_endianness = flag;
        }

        uint32_t get_block_dim_x() const
        {
            return m_block_dim_x;
        }

        uint32_t get_block_dim_y() const
        {
            return m_block_dim_y;
        }
        uint32_t get_bytes_per_block() const
        {
            return m_bytes_per_block;
        }

        const ktx_header &get_header() const
        {
            return m_header;
        }

        // Key values
        const ktx_key_value_vec &get_key_value_vec() const
        {
            return m_key_values;
        }
        ktx_key_value_vec &get_key_value_vec()
        {
            return m_key_values;
        }

        void get_keys(dynamic_string_array &keys) const;

        const uint8_vec *find_key(const char *pKey) const;
        bool get_key_value_data(const char *pKey, uint8_vec &data) const;
        bool get_key_value_as_string(const char *pKey, dynamic_string &str) const;

        uint32_t add_key_value(const char *pKey, const void *pVal, uint32_t val_size);
        uint32_t add_key_value(const char *pKey, const char *pVal)
        {
            return add_key_value(pKey, pVal, static_cast<uint32_t>(strlen(pVal)) + 1);
        }

        // Image data
        uint32_t get_num_images() const
        {
            return m_image_data.size();
        }

        const uint8_vec &get_image_data(uint32_t image_index) const
        {
            return m_image_data[image_index];
        }
        uint8_vec &get_image_data(uint32_t image_index)
        {
            return m_image_data[image_index];
        }

        const uint8_vec &get_image_data(uint32_t mip_index, uint32_t array_index, uint32_t face_index, uint32_t zslice_index) const
        {
            return get_image_data(get_image_index(mip_index, array_index, face_index, zslice_index));
        }
        uint8_vec &get_image_data(uint32_t mip_index, uint32_t array_index, uint32_t face_index, uint32_t zslice_index)
        {
            return get_image_data(get_image_index(mip_index, array_index, face_index, zslice_index));
        }

        const ktx_image_data_vec &get_image_data_vec() const
        {
            return m_image_data;
        }
        ktx_image_data_vec &get_image_data_vec()
        {
            return m_image_data;
        }

        // Adds a single 2D image to the texture
        void add_image(uint32_t mip_index, uint32_t array_index, uint32_t face_index, uint32_t zslice_index, const void *pImage, uint32_t image_size)
        {
            const uint32_t image_index = get_image_index(mip_index, array_index, face_index, zslice_index);
            if (image_index >= m_image_data.size())
                m_image_data.resize(image_index + 1);
            if (image_size)
            {
                uint8_vec &v = m_image_data[image_index];
                v.resize(image_size);
                memcpy(&v[0], pImage, image_size);
            }
        }

        // Adds a single 2D image to the texture
        // Caller grants ownership of the indicated image data, on return img will be whatever previously in the target slot.
        void add_image_grant_ownership(uint32_t mip_index, uint32_t array_index, uint32_t face_index, uint32_t zslice_index, uint8_vec &img)
        {
            const uint32_t image_index = get_image_index(mip_index, array_index, face_index, zslice_index);
            if (image_index >= m_image_data.size())
                m_image_data.resize(image_index + 1);

            m_image_data[image_index].swap(img);
        }

        // Adds a single 2D image to the texture
        // FIXME: voglcore uses this simplified helper, the order of params is inconsistent with the other add_image()
        void add_image(uint32_t face_index, uint32_t mip_index, const void *pImage, uint32_t image_size)
        {
            add_image(mip_index, 0, face_index, 0, pImage, image_size);
        }

        uint32_t get_image_index(uint32_t mip_index, uint32_t array_index, uint32_t face_index, uint32_t zslice_index) const
        {
            VOGL_ASSERT((mip_index < get_num_mips()) && (array_index < get_array_size()) && (face_index < get_num_faces()) && (zslice_index < get_depth()));
            return zslice_index + (face_index * get_depth()) + (array_index * (get_depth() * get_num_faces())) + (mip_index * (get_depth() * get_num_faces() * get_array_size()));
        }

        void get_mip_dim(uint32_t mip_index, uint32_t &mip_width, uint32_t &mip_height) const
        {
            VOGL_ASSERT(mip_index < get_num_mips());
            mip_width = VOGL_MAX(get_width() >> mip_index, 1);
            mip_height = VOGL_MAX(get_height() >> mip_index, 1);
        }

        void get_mip_dim(uint32_t mip_index, uint32_t &mip_width, uint32_t &mip_height, uint32_t &mip_depth) const
        {
            VOGL_ASSERT(mip_index < get_num_mips());
            mip_width = VOGL_MAX(get_width() >> mip_index, 1);
            mip_height = VOGL_MAX(get_height() >> mip_index, 1);
            mip_depth = VOGL_MAX(get_depth() >> mip_index, 1);
        }

        // Returns the expected size of a single 2D image. (For 3D, multiply by the depth, etc.)
        uint32_t get_expected_image_size(uint32_t mip_index) const;

        bool operator==(const ktx_texture &rhs) const;
        bool operator!=(const ktx_texture &rhs) const
        {
            return !(*this == rhs);
        }

    private:
        mutable ktx_header m_header;

        ktx_key_value_vec m_key_values;
        ktx_image_data_vec m_image_data;

        uint32_t m_block_dim_x;
        uint32_t m_block_dim_y;
        uint32_t m_bytes_per_block;

        bool m_opposite_endianness;

        bool compute_pixel_info();
    };

} // namespace vogl
