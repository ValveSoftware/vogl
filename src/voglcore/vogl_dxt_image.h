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

// File: vogl_dxt_image.h
#pragma once

#include "vogl_core.h"
#include "vogl_dxt1.h"
#include "vogl_dxt5a.h"
#include "vogl_image.h"
#include "vogl_rg_etc1.h"

#define VOGL_SUPPORT_ATI_COMPRESS 0

namespace vogl
{
    class task_pool;

    class dxt_image
    {
    public:
        dxt_image();
        dxt_image(const dxt_image &other);
        dxt_image &operator=(const dxt_image &rhs);

        void clear();

        inline bool is_valid() const
        {
            return m_blocks_x > 0;
        }

        uint32_t get_width() const
        {
            return m_width;
        }
        uint32_t get_height() const
        {
            return m_height;
        }

        uint32_t get_blocks_x() const
        {
            return m_blocks_x;
        }
        uint32_t get_blocks_y() const
        {
            return m_blocks_y;
        }
        uint32_t get_total_blocks() const
        {
            return m_blocks_x * m_blocks_y;
        }

        uint32_t get_elements_per_block() const
        {
            return m_num_elements_per_block;
        }
        uint32_t get_bytes_per_block() const
        {
            return m_bytes_per_block;
        }

        dxt_format get_format() const
        {
            return m_format;
        }

        bool has_color() const
        {
            return (m_format == cDXT1) || (m_format == cDXT1A) || (m_format == cDXT3) || (m_format == cDXT5) || (m_format == cETC1);
        }

        // Will be pretty slow if the image is DXT1, as this method scans for alpha blocks/selectors.
        bool has_alpha() const;

        enum element_type
        {
            cUnused = 0,
            cColorDXT1, // DXT1 color block
            cAlphaDXT3, // DXT3 alpha block (only)
            cAlphaDXT5, // DXT5 alpha block (only)
            cColorETC1, // ETC1 color block
        };

        element_type get_element_type(uint32_t element_index) const
        {
            VOGL_ASSERT(element_index < m_num_elements_per_block);
            return m_element_type[element_index];
        }

        //Returns -1 for RGB, or [0,3]
        int8_t get_element_component_index(uint32_t element_index) const
        {
            VOGL_ASSERT(element_index < m_num_elements_per_block);
            return m_element_component_index[element_index];
        }

        struct element
        {
            uint8_t m_bytes[8];

            uint32_t get_le_word(uint32_t index) const
            {
                VOGL_ASSERT(index < 4);
                return m_bytes[index * 2] | (m_bytes[index * 2 + 1] << 8);
            }
            uint32_t get_be_word(uint32_t index) const
            {
                VOGL_ASSERT(index < 4);
                return m_bytes[index * 2 + 1] | (m_bytes[index * 2] << 8);
            }

            void set_le_word(uint32_t index, uint32_t val)
            {
                VOGL_ASSERT((index < 4) && (val <= cUINT16_MAX));
                m_bytes[index * 2] = static_cast<uint8_t>(val & 0xFF);
                m_bytes[index * 2 + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
            }
            void set_be_word(uint32_t index, uint32_t val)
            {
                VOGL_ASSERT((index < 4) && (val <= cUINT16_MAX));
                m_bytes[index * 2 + 1] = static_cast<uint8_t>(val & 0xFF);
                m_bytes[index * 2] = static_cast<uint8_t>((val >> 8) & 0xFF);
            }

            void clear()
            {
                memset(this, 0, sizeof(*this));
            }
        };

        typedef vogl::vector<element> element_vec;

        bool init(dxt_format fmt, uint32_t width, uint32_t height, bool clear_elements);
        bool init(dxt_format fmt, uint32_t width, uint32_t height, uint32_t num_elements, element *pElements, bool create_copy);

        struct pack_params
        {
            pack_params()
            {
                clear();
            }

            void clear()
            {
                m_quality = cCRNDXTQualityUber;
                m_perceptual = true;
                m_dithering = false;
                m_grayscale_sampling = false;
                m_use_both_block_types = true;
                m_endpoint_caching = true;
                m_compressor = cCRNDXTCompressorCRN;
                m_pProgress_callback = NULL;
                m_pProgress_callback_user_data_ptr = NULL;
                m_dxt1a_alpha_threshold = 128;
                m_num_helper_threads = 0;
                m_progress_start = 0;
                m_progress_range = 100;
                m_use_transparent_indices_for_black = false;
                m_pTask_pool = NULL;
                m_color_weights[0] = 1;
                m_color_weights[1] = 1;
                m_color_weights[2] = 1;
            }

            void init(const vogl_comp_params &params)
            {
                m_perceptual = (params.m_flags & cCRNCompFlagPerceptual) != 0;
                m_num_helper_threads = params.m_num_helper_threads;
                m_use_both_block_types = (params.m_flags & cCRNCompFlagUseBothBlockTypes) != 0;
                m_use_transparent_indices_for_black = (params.m_flags & cCRNCompFlagUseTransparentIndicesForBlack) != 0;
                m_dxt1a_alpha_threshold = params.m_dxt1a_alpha_threshold;
                m_quality = params.m_dxt_quality;
                m_endpoint_caching = (params.m_flags & cCRNCompFlagDisableEndpointCaching) == 0;
                m_grayscale_sampling = (params.m_flags & cCRNCompFlagGrayscaleSampling) != 0;
                m_compressor = params.m_dxt_compressor_type;
            }

            uint32_t m_dxt1a_alpha_threshold;

            uint32_t m_num_helper_threads;

            vogl_dxt_quality m_quality;

            vogl_dxt_compressor_type m_compressor;

            bool m_perceptual;
            bool m_dithering;
            bool m_grayscale_sampling;
            bool m_use_both_block_types;
            bool m_endpoint_caching;
            bool m_use_transparent_indices_for_black;

            typedef bool (*progress_callback_func)(uint32_t percentage_complete, void *pUser_data_ptr);
            progress_callback_func m_pProgress_callback;
            void *m_pProgress_callback_user_data_ptr;

            uint32_t m_progress_start;
            uint32_t m_progress_range;

            task_pool *m_pTask_pool;

            int m_color_weights[3];
        };

        bool init(dxt_format fmt, const image_u8 &img, const pack_params &p = dxt_image::pack_params());

        bool unpack(image_u8 &img) const;

        void endian_swap();

        uint32_t get_total_elements() const
        {
            return m_elements.size();
        }

        const element_vec &get_element_vec() const
        {
            return m_elements;
        }
        element_vec &get_element_vec()
        {
            return m_elements;
        }

        const element &get_element(uint32_t block_x, uint32_t block_y, uint32_t element_index) const;
        element &get_element(uint32_t block_x, uint32_t block_y, uint32_t element_index);

        const element *get_element_ptr() const
        {
            return m_pElements;
        }
        element *get_element_ptr()
        {
            return m_pElements;
        }

        uint32_t get_size_in_bytes() const
        {
            return m_elements.size() * sizeof(element);
        }
        uint32_t get_row_pitch_in_bytes() const
        {
            return m_blocks_x * m_bytes_per_block;
        }

        color_quad_u8 get_pixel(uint32_t x, uint32_t y) const;
        uint32_t get_pixel_alpha(uint32_t x, uint32_t y, uint32_t element_index) const;

        void set_pixel(uint32_t x, uint32_t y, const color_quad_u8 &c, bool perceptual = true);

        // get_block_pixels() only sets those components stored in the image!
        bool get_block_pixels(uint32_t block_x, uint32_t block_y, color_quad_u8 *pPixels) const;

        struct set_block_pixels_context
        {
            dxt1_endpoint_optimizer m_dxt1_optimizer;
            dxt5_endpoint_optimizer m_dxt5_optimizer;
        };

        void set_block_pixels(uint32_t block_x, uint32_t block_y, const color_quad_u8 *pPixels, const pack_params &p, set_block_pixels_context &context);
        void set_block_pixels(uint32_t block_x, uint32_t block_y, const color_quad_u8 *pPixels, const pack_params &p);

        void get_block_endpoints(uint32_t block_x, uint32_t block_y, uint32_t element_index, uint32_t &packed_low_endpoint, uint32_t &packed_high_endpoint) const;

        // Returns a value representing the component(s) that where actually set, where -1 = RGB.
        // This method does not always set every component!
        int get_block_endpoints(uint32_t block_x, uint32_t block_y, uint32_t element_index, color_quad_u8 &low_endpoint, color_quad_u8 &high_endpoint, bool scaled = true) const;

        // pColors should point to a 16 entry array, to handle DXT3.
        // Returns the number of block colors: 3, 4, 6, 8, or 16.
        uint32_t get_block_colors(uint32_t block_x, uint32_t block_y, uint32_t element_index, color_quad_u8 *pColors, uint32_t subblock_index = 0);

        uint32_t get_subblock_index(uint32_t x, uint32_t y, uint32_t element_index) const;
        uint32_t get_total_subblocks(uint32_t element_index) const;

        uint32_t get_selector(uint32_t x, uint32_t y, uint32_t element_index) const;

        void change_dxt1_to_dxt1a();

        bool can_flip(uint32_t axis_index);

        // Returns true if the texture can actually be flipped.
        bool flip_x();
        bool flip_y();

    private:
        element_vec m_elements;
        element *m_pElements;

        uint32_t m_width;
        uint32_t m_height;

        uint32_t m_blocks_x;
        uint32_t m_blocks_y;
        uint32_t m_total_blocks;
        uint32_t m_total_elements;

        uint32_t m_num_elements_per_block; // 1 or 2
        uint32_t m_bytes_per_block;        // 8 or 16

        int8_t m_element_component_index[2];
        element_type m_element_type[2];

        dxt_format m_format; // DXT1, 1A, 3, 5, N/3DC, or 5A

        bool init_internal(dxt_format fmt, uint32_t width, uint32_t height);
        void init_task(uint64_t data, void *pData_ptr);

#if VOGL_SUPPORT_ATI_COMPRESS
        bool init_ati_compress(dxt_format fmt, const image_u8 &img, const pack_params &p);
#endif

        void flip_col(uint32_t x);
        void flip_row(uint32_t y);
    };

} // namespace vogl
