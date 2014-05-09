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

// File: vogl_mipmapped_texture.h
#pragma once

#include "vogl_core.h"
#include "vogl_dxt_image.h"
#include "dds_defs.h"
#include "vogl_pixel_format.h"
#include "vogl_image.h"
#include "vogl_resampler.h"
#include "vogl_data_stream_serializer.h"
#include "vogl_texture_file_types.h"
#include "vogl_image_utils.h"

namespace vogl
{
    class ktx_texture;

    enum orientation_flags_t
    {
        cOrientationFlagXFlipped = 1,
        cOrientationFlagYFlipped = 2,
        cDefaultOrientationFlags = 0
    };

    enum unpack_flags_t
    {
        cUnpackFlagUncook = 1,
        cUnpackFlagUnflip = 2
    };

    class mip_level
    {
        friend class mipmapped_texture;

    public:
        mip_level();
        ~mip_level();

        mip_level(const mip_level &other);
        mip_level &operator=(const mip_level &rhs);

        // Assumes ownership.
        void assign(image_u8 *p, pixel_format fmt = PIXEL_FMT_INVALID, orientation_flags_t orient_flags = cDefaultOrientationFlags);
        void assign(dxt_image *p, pixel_format fmt = PIXEL_FMT_INVALID, orientation_flags_t orient_flags = cDefaultOrientationFlags);

        void clear();

        inline uint32_t get_width() const
        {
            return m_width;
        }
        inline uint32_t get_height() const
        {
            return m_height;
        }
        inline uint32_t get_total_pixels() const
        {
            return m_width * m_height;
        }

        orientation_flags_t get_orientation_flags() const
        {
            return m_orient_flags;
        }
        void set_orientation_flags(orientation_flags_t flags)
        {
            m_orient_flags = flags;
        }

        inline image_u8 *get_image() const
        {
            return m_pImage;
        }
        inline dxt_image *get_dxt_image() const
        {
            return m_pDXTImage;
        }

        image_u8 *get_unpacked_image(image_u8 &tmp, uint32_t unpack_flags) const;

        inline bool is_packed() const
        {
            return m_pDXTImage != NULL;
        }

        inline bool is_valid() const
        {
            return (m_pImage != NULL) || (m_pDXTImage != NULL);
        }

        inline pixel_format_helpers::component_flags get_comp_flags() const
        {
            return m_comp_flags;
        }
        inline void set_comp_flags(pixel_format_helpers::component_flags comp_flags)
        {
            m_comp_flags = comp_flags;
        }

        inline pixel_format get_format() const
        {
            return m_format;
        }
        inline void set_format(pixel_format fmt)
        {
            m_format = fmt;
        }

        bool convert(pixel_format fmt, bool cook, const dxt_image::pack_params &p);

        bool pack_to_dxt(const image_u8 &img, pixel_format fmt, bool cook, const dxt_image::pack_params &p, orientation_flags_t orient_flags = cDefaultOrientationFlags);
        bool pack_to_dxt(pixel_format fmt, bool cook, const dxt_image::pack_params &p);

        bool unpack_from_dxt(bool uncook = true);

        // Returns true if flipped on either axis.
        bool is_flipped() const;

        bool is_x_flipped() const;
        bool is_y_flipped() const;

        bool can_unflip_without_unpacking() const;

        // Returns true if unflipped on either axis.
        // Will try to flip packed (DXT/ETC) data in-place, if this isn't possible it'll unpack/uncook the mip level then unflip.
        bool unflip(bool allow_unpacking_to_flip, bool uncook_during_unpack);

        bool set_alpha_to_luma();
        bool convert(image_utils::conversion_type conv_type);

        bool flip_x();
        bool flip_y();

    private:
        uint32_t m_width;
        uint32_t m_height;

        pixel_format_helpers::component_flags m_comp_flags;
        pixel_format m_format;

        image_u8 *m_pImage;
        dxt_image *m_pDXTImage;

        orientation_flags_t m_orient_flags;

        void cook_image(image_u8 &img) const;
        void uncook_image(image_u8 &img) const;
    };

    // A face is an array of mip_level ptr's.
    typedef vogl::vector<mip_level *> mip_ptr_vec;

    // And an array of one, six, or N faces make up a texture.
    typedef vogl::vector<mip_ptr_vec> face_vec;

    // And a set of textures make up a texture array
    typedef vogl::vector<face_vec> face_array_vec;

    class mipmapped_texture
    {
    public:
        // Construction/destruction
        mipmapped_texture();
        ~mipmapped_texture();

        mipmapped_texture(const mipmapped_texture &other);
        mipmapped_texture &operator=(const mipmapped_texture &rhs);

        void clear();

        void init(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, uint32_t faces, uint32_t array_size, pixel_format fmt, const char *pName, orientation_flags_t orient_flags);

        // Assumes ownership.
        void assign(uint32_t array_index, face_vec &faces);
        void assign(uint32_t array_index, mip_level *pLevel);
        void assign(uint32_t array_index, image_u8 *p, pixel_format fmt = PIXEL_FMT_INVALID, orientation_flags_t orient_flags = cDefaultOrientationFlags);
        void assign(uint32_t array_index, dxt_image *p, pixel_format fmt = PIXEL_FMT_INVALID, orientation_flags_t orient_flags = cDefaultOrientationFlags);

        void set(texture_file_types::format source_file_type, const mipmapped_texture &mipmapped_texture);

        // Accessors
        image_u8 *get_level_image(uint32_t array_index, uint32_t face, uint32_t level, image_u8 &img, uint32_t unpack_flags = cUnpackFlagUncook | cUnpackFlagUnflip) const;

        inline bool is_valid() const
        {
            return m_face_array.size() > 0;
        }

        const dynamic_string &get_name() const
        {
            return m_name;
        }
        void set_name(const dynamic_string &name)
        {
            m_name = name;
        }

        const dynamic_string &get_source_filename() const
        {
            return get_name();
        }
        texture_file_types::format get_source_file_type() const
        {
            return m_source_file_type;
        }

        inline uint32_t get_width() const
        {
            return m_width;
        }
        inline uint32_t get_height() const
        {
            return m_height;
        }
        inline uint32_t get_depth() const
        {
            return m_depth;
        }
        inline uint32_t get_total_pixels() const
        {
            return m_width * m_height;
        }
        uint32_t get_total_pixels_in_all_faces_and_mips() const;

        inline uint32_t get_array_size() const
        {
            return m_array_size;
        }
        inline uint32_t get_num_faces() const
        {
            return m_faces;
        }
        inline uint32_t get_num_levels() const
        {
            if (m_face_array.is_empty())
                return 0;
            else if (m_face_array[0].is_empty())
                return 0;
            else
                return m_face_array[0][0].size();
        }

        inline pixel_format_helpers::component_flags get_comp_flags() const
        {
            return m_comp_flags;
        }
        inline pixel_format get_format() const
        {
            return m_format;
        }

        inline bool is_unpacked() const
        {
            if (get_num_faces())
            {
                return get_level(0, 0, 0)->get_image() != NULL;
            }
            return false;
        }

        inline const mip_ptr_vec &get_face(uint32_t array_index, uint32_t face) const
        {
            return m_face_array[array_index][face];
        }
        inline mip_ptr_vec &get_face(uint32_t array_index, uint32_t face)
        {
            return m_face_array[array_index][face];
        }

        inline const mip_level *get_level(uint32_t array_index, uint32_t face, uint32_t mip) const
        {
            return m_face_array[array_index][face][mip];
        }
        inline mip_level *get_level(uint32_t array_index, uint32_t face, uint32_t mip)
        {
            return m_face_array[array_index][face][mip];
        }

        bool has_alpha() const;
        bool is_normal_map() const;
        bool is_vertical_cross() const;
        bool is_cubemap() const;
        bool is_packed() const;
        texture_type determine_texture_type() const;

        const dynamic_string &get_last_error() const
        {
            return m_last_error;
        }
        void clear_last_error()
        {
            m_last_error.clear();
        }

        // Reading/writing
        bool read_dds(data_stream_serializer &serializer);
        bool write_dds(data_stream_serializer &serializer) const;

        bool read_ktx(data_stream_serializer &serializer);
        bool read_ktx(const vogl::ktx_texture &ktx);
        bool write_ktx(data_stream_serializer &serializer) const;

        // If file_format is texture_file_types::cFormatInvalid, the format will be determined from the filename's extension.
        bool read_from_file(const char *pFilename, texture_file_types::format file_format = texture_file_types::cFormatInvalid);
        bool read_from_stream(data_stream_serializer &serializer, texture_file_types::format file_format = texture_file_types::cFormatInvalid);

        bool write_to_file(
            const char *pFilename,
            texture_file_types::format file_format = texture_file_types::cFormatInvalid,
            vogl_comp_params *pComp_params = NULL,
            uint32_t *pActual_quality_level = NULL, float *pActual_bitrate = NULL,
            uint32_t image_write_flags = 0);

        // Conversion
        bool convert(pixel_format fmt, bool cook, const dxt_image::pack_params &p);
        bool convert(pixel_format fmt, const dxt_image::pack_params &p);
        bool convert(image_utils::conversion_type conv_type);

        bool unpack_from_dxt(bool uncook = true);

        bool set_alpha_to_luma();

        void discard_mipmaps();

        void discard_mips();

        struct resample_params
        {
            resample_params()
                : m_pFilter("kaiser"),
                  m_wrapping(false),
                  m_srgb(false),
                  m_renormalize(false),
                  m_filter_scale(.9f),
                  m_gamma(1.75f), // or 2.2f
                  m_multithreaded(true)
            {
            }

            const char *m_pFilter;
            bool m_wrapping;
            bool m_srgb;
            bool m_renormalize;
            float m_filter_scale;
            float m_gamma;
            bool m_multithreaded;
        };

        bool resize(uint32_t new_width, uint32_t new_height, const resample_params &params);

        struct generate_mipmap_params : public resample_params
        {
            generate_mipmap_params()
                : resample_params(),
                  m_min_mip_size(1),
                  m_max_mips(0)
            {
            }

            uint32_t m_min_mip_size;
            uint32_t m_max_mips; // actually the max # of total levels
        };

        bool generate_mipmaps(const generate_mipmap_params &params, bool force);

        bool crop(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

        bool vertical_cross_to_cubemap();

        void swap(mipmapped_texture &img);

        bool check() const;

        void set_orientation_flags(orientation_flags_t flags);

        // Returns true if any face/miplevel is flipped.
        bool is_flipped() const;
        bool is_x_flipped() const;
        bool is_y_flipped() const;
        bool can_unflip_without_unpacking() const;
        bool unflip(bool allow_unpacking_to_flip, bool uncook_if_necessary_to_unpack);

        bool flip_y(bool update_orientation_flags);

    private:
        dynamic_string m_name;

        uint32_t m_width;
        uint32_t m_height;
        uint32_t m_depth;
        uint32_t m_faces;
        uint32_t m_array_size;

        pixel_format_helpers::component_flags m_comp_flags;
        pixel_format m_format;

        face_array_vec m_face_array;

        texture_file_types::format m_source_file_type;

        mutable dynamic_string m_last_error;

        inline void clear_last_error() const
        {
            m_last_error.clear();
        }
        inline void set_last_error(const char *p) const
        {
            m_last_error = p;
        }

        void free_all_mips();
        bool read_regular_image(data_stream_serializer &serializer, texture_file_types::format file_format);
        bool write_regular_image(const char *pFilename, uint32_t image_write_flags);
        bool read_dds_internal(data_stream_serializer &serializer);
        void change_dxt1_to_dxt1a();
        bool flip_y_helper();
    };

    inline void swap(mipmapped_texture &a, mipmapped_texture &b)
    {
        a.swap(b);
    }

} // namespace vogl
