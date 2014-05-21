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

// File: vogl_dxt_image.cpp
#include "vogl_core.h"
#include "vogl_dxt_image.h"
#if VOGL_SUPPORT_SQUISH
#include "squish\squish.h"
#endif
#include "vogl_ryg_dxt.hpp"
#include "vogl_dxt_fast.h"
#include "vogl_console.h"
#include "vogl_threading.h"

#if VOGL_SUPPORT_ATI_COMPRESS
#ifdef _DLL
#pragma comment(lib, "ATI_Compress_MT_DLL_VC8.lib")
#else
#pragma comment(lib, "ATI_Compress_MT_VC8.lib")
#endif
#include "..\ext\ATI_Compress\ATI_Compress.h"
#endif

#include "vogl_etc.h"

namespace vogl
{
    dxt_image::dxt_image()
        : m_pElements(NULL),
          m_width(0),
          m_height(0),
          m_blocks_x(0),
          m_blocks_y(0),
          m_total_blocks(0),
          m_total_elements(0),
          m_num_elements_per_block(0),
          m_bytes_per_block(0),
          m_format(cDXTInvalid)
    {
        utils::zero_object(m_element_type);
        utils::zero_object(m_element_component_index);
    }

    dxt_image::dxt_image(const dxt_image &other)
        : m_pElements(NULL)
    {
        *this = other;
    }

    dxt_image &dxt_image::operator=(const dxt_image &rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        m_width = rhs.m_width;
        m_height = rhs.m_height;
        m_blocks_x = rhs.m_blocks_x;
        m_blocks_y = rhs.m_blocks_y;
        m_num_elements_per_block = rhs.m_num_elements_per_block;
        m_bytes_per_block = rhs.m_bytes_per_block;
        m_format = rhs.m_format;
        m_total_blocks = rhs.m_total_blocks;
        m_total_elements = rhs.m_total_elements;
        m_pElements = NULL;
        memcpy(m_element_type, rhs.m_element_type, sizeof(m_element_type));
        memcpy(m_element_component_index, rhs.m_element_component_index, sizeof(m_element_component_index));

        if (rhs.m_pElements)
        {
            m_elements.resize(m_total_elements);
            memcpy(&m_elements[0], rhs.m_pElements, sizeof(element) * m_total_elements);
            m_pElements = &m_elements[0];
        }

        return *this;
    }

    void dxt_image::clear()
    {
        m_elements.clear();
        m_width = 0;
        m_height = 0;
        m_blocks_x = 0;
        m_blocks_y = 0;
        m_num_elements_per_block = 0;
        m_bytes_per_block = 0;
        m_format = cDXTInvalid;
        utils::zero_object(m_element_type);
        utils::zero_object(m_element_component_index);
        m_total_blocks = 0;
        m_total_elements = 0;
        m_pElements = NULL;
    }

    bool dxt_image::init_internal(dxt_format fmt, uint32_t width, uint32_t height)
    {
        VOGL_ASSERT((fmt != cDXTInvalid) && (width > 0) && (height > 0));

        clear();

        m_width = width;
        m_height = height;

        m_blocks_x = (m_width + 3) >> cDXTBlockShift;
        m_blocks_y = (m_height + 3) >> cDXTBlockShift;

        m_num_elements_per_block = 2;
        if ((fmt == cDXT1) || (fmt == cDXT1A) || (fmt == cDXT5A) || (fmt == cETC1))
            m_num_elements_per_block = 1;

        m_total_blocks = m_blocks_x * m_blocks_y;
        m_total_elements = m_total_blocks * m_num_elements_per_block;

        VOGL_ASSUME((uint32_t)cDXT1BytesPerBlock == (uint32_t)cETC1BytesPerBlock);
        m_bytes_per_block = cDXT1BytesPerBlock * m_num_elements_per_block;

        m_format = fmt;

        switch (m_format)
        {
            case cDXT1:
            case cDXT1A:
            {
                m_element_type[0] = cColorDXT1;
                m_element_component_index[0] = -1;
                break;
            }
            case cDXT3:
            {
                m_element_type[0] = cAlphaDXT3;
                m_element_type[1] = cColorDXT1;
                m_element_component_index[0] = 3;
                m_element_component_index[1] = -1;
                break;
            }
            case cDXT5:
            {
                m_element_type[0] = cAlphaDXT5;
                m_element_type[1] = cColorDXT1;
                m_element_component_index[0] = 3;
                m_element_component_index[1] = -1;
                break;
            }
            case cDXT5A:
            {
                m_element_type[0] = cAlphaDXT5;
                m_element_component_index[0] = 3;
                break;
            }
            case cDXN_XY:
            {
                m_element_type[0] = cAlphaDXT5;
                m_element_type[1] = cAlphaDXT5;
                m_element_component_index[0] = 0;
                m_element_component_index[1] = 1;
                break;
            }
            case cDXN_YX:
            {
                m_element_type[0] = cAlphaDXT5;
                m_element_type[1] = cAlphaDXT5;
                m_element_component_index[0] = 1;
                m_element_component_index[1] = 0;
                break;
            }
            case cETC1:
            {
                m_element_type[0] = cColorETC1;
                m_element_component_index[0] = -1;
                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                clear();
                return false;
            }
        }

        return true;
    }

    bool dxt_image::init(dxt_format fmt, uint32_t width, uint32_t height, bool clear_elements)
    {
        if (!init_internal(fmt, width, height))
            return false;

        m_elements.resize(m_total_elements);
        m_pElements = &m_elements[0];

        if (clear_elements)
            memset(m_pElements, 0, sizeof(element) * m_total_elements);

        return true;
    }

    bool dxt_image::init(dxt_format fmt, uint32_t width, uint32_t height, uint32_t num_elements, element *pElements, bool create_copy)
    {
        VOGL_ASSERT(num_elements && pElements);

        if (!init_internal(fmt, width, height))
            return false;

        if (num_elements != m_total_elements)
        {
            clear();
            return false;
        }

        if (create_copy)
        {
            m_elements.resize(m_total_elements);
            m_pElements = &m_elements[0];

            memcpy(m_pElements, pElements, m_total_elements * sizeof(element));
        }
        else
            m_pElements = pElements;

        return true;
    }

    struct init_task_params
    {
        dxt_format m_fmt;
        const image_u8 *m_pImg;
        const dxt_image::pack_params *m_pParams;
        vogl_thread_id_t m_main_thread;
        atomic32_t m_canceled;
    };

    void dxt_image::init_task(uint64_t data, void *pData_ptr)
    {
        const uint32_t thread_index = static_cast<uint32_t>(data);
        init_task_params *pInit_params = static_cast<init_task_params *>(pData_ptr);

        const image_u8 &img = *pInit_params->m_pImg;
        const pack_params &p = *pInit_params->m_pParams;
        const bool is_main_thread = (vogl_get_current_thread_id() == pInit_params->m_main_thread);

        uint32_t block_index = 0;

        set_block_pixels_context optimizer_context;
        int prev_progress_percentage = -1;

        for (uint32_t block_y = 0; block_y < m_blocks_y; block_y++)
        {
            const uint32_t pixel_ofs_y = block_y * cDXTBlockSize;

            for (uint32_t block_x = 0; block_x < m_blocks_x; block_x++, block_index++)
            {
                if (pInit_params->m_canceled)
                    return;

                if (p.m_pProgress_callback && is_main_thread && ((block_index & 63) == 63))
                {
                    const uint32_t progress_percentage = p.m_progress_start + ((block_index * p.m_progress_range + get_total_blocks() / 2) / get_total_blocks());
                    if ((int)progress_percentage != prev_progress_percentage)
                    {
                        prev_progress_percentage = progress_percentage;
                        if (!(p.m_pProgress_callback)(progress_percentage, p.m_pProgress_callback_user_data_ptr))
                        {
                            atomic_exchange32(&pInit_params->m_canceled, VOGL_TRUE);
                            return;
                        }
                    }
                }

                if (p.m_num_helper_threads)
                {
                    if ((block_index % (p.m_num_helper_threads + 1)) != thread_index)
                        continue;
                }

                color_quad_u8 pixels[cDXTBlockSize * cDXTBlockSize];

                const uint32_t pixel_ofs_x = block_x * cDXTBlockSize;

                for (uint32_t y = 0; y < cDXTBlockSize; y++)
                {
                    const uint32_t iy = math::minimum(pixel_ofs_y + y, img.get_height() - 1);

                    for (uint32_t x = 0; x < cDXTBlockSize; x++)
                    {
                        const uint32_t ix = math::minimum(pixel_ofs_x + x, img.get_width() - 1);

                        pixels[x + y * cDXTBlockSize] = img(ix, iy);
                    }
                }

                set_block_pixels(block_x, block_y, pixels, p, optimizer_context);
            }
        }
    }

#if VOGL_SUPPORT_ATI_COMPRESS
    bool dxt_image::init_ati_compress(dxt_format fmt, const image_u8 &img, const pack_params &p)
    {
        image_u8 tmp_img(img);
        for (uint32_t y = 0; y < img.get_height(); y++)
        {
            for (uint32_t x = 0; x < img.get_width(); x++)
            {
                color_quad_u8 c(img(x, y));
                std::swap(c.r, c.b);
                tmp_img(x, y) = c;
            }
        }

        ATI_TC_Texture src_tex;
        utils::zero_object(src_tex);
        src_tex.dwSize = sizeof(ATI_TC_Texture);
        src_tex.dwWidth = tmp_img.get_width();
        src_tex.dwHeight = tmp_img.get_height();
        src_tex.dwPitch = tmp_img.get_pitch_in_bytes();
        src_tex.format = ATI_TC_FORMAT_ARGB_8888;
        src_tex.dwDataSize = src_tex.dwPitch * tmp_img.get_height();
        src_tex.pData = (ATI_TC_BYTE *)tmp_img.get_ptr();

        ATI_TC_Texture dst_tex;
        utils::zero_object(dst_tex);
        dst_tex.dwSize = sizeof(ATI_TC_Texture);
        dst_tex.dwWidth = tmp_img.get_width();
        dst_tex.dwHeight = tmp_img.get_height();
        dst_tex.dwDataSize = get_size_in_bytes();
        dst_tex.pData = (ATI_TC_BYTE *)get_element_ptr();

        switch (fmt)
        {
            case cDXT1:
            case cDXT1A:
                dst_tex.format = ATI_TC_FORMAT_DXT1;
                break;
            case cDXT3:
                dst_tex.format = ATI_TC_FORMAT_DXT3;
                break;
            case cDXT5:
                dst_tex.format = ATI_TC_FORMAT_DXT5;
                break;
            case cDXT5A:
                dst_tex.format = ATI_TC_FORMAT_ATI1N;
                break;
            case cDXN_XY:
                dst_tex.format = ATI_TC_FORMAT_ATI2N_XY;
                break;
            case cDXN_YX:
                dst_tex.format = ATI_TC_FORMAT_ATI2N;
                break;
            default:
            {
                VOGL_ASSERT(false);
                return false;
            }
        }

        ATI_TC_CompressOptions options;
        utils::zero_object(options);
        options.dwSize = sizeof(ATI_TC_CompressOptions);

        if (fmt == cDXT1A)
        {
            options.bDXT1UseAlpha = true;
            options.nAlphaThreshold = (ATI_TC_BYTE)p.m_dxt1a_alpha_threshold;
        }
        options.bDisableMultiThreading = (p.m_num_helper_threads == 0);
        switch (p.m_quality)
        {
            case cCRNDXTQualityFast:
                options.nCompressionSpeed = ATI_TC_Speed_Fast;
                break;
            case cCRNDXTQualitySuperFast:
                options.nCompressionSpeed = ATI_TC_Speed_SuperFast;
                break;
            default:
                options.nCompressionSpeed = ATI_TC_Speed_Normal;
                break;
        }

        if (p.m_perceptual)
        {
            options.bUseChannelWeighting = true;
            options.fWeightingRed = .212671f;
            options.fWeightingGreen = .715160f;
            options.fWeightingBlue = .072169f;
        }

        ATI_TC_ERROR err = ATI_TC_ConvertTexture(&src_tex, &dst_tex, &options, NULL, NULL, NULL);
        return err == ATI_TC_OK;
    }
#endif

    bool dxt_image::init(dxt_format fmt, const image_u8 &img, const pack_params &p)
    {
        if (!init(fmt, img.get_width(), img.get_height(), false))
            return false;

#if VOGL_SUPPORT_ATI_COMPRESS
        if (p.m_compressor == cCRNDXTCompressorATI)
            return init_ati_compress(fmt, img, p);
#endif

        task_pool *pPool = p.m_pTask_pool;

        task_pool tmp_pool;
        if (!pPool)
        {
            if (!tmp_pool.init(p.m_num_helper_threads))
                return false;
            pPool = &tmp_pool;
        }

        init_task_params init_params;
        init_params.m_fmt = fmt;
        init_params.m_pImg = &img;
        init_params.m_pParams = &p;
        init_params.m_main_thread = vogl_get_current_thread_id();
        init_params.m_canceled = false;

        for (uint32_t i = 0; i <= p.m_num_helper_threads; i++)
            pPool->queue_object_task(this, &dxt_image::init_task, i, &init_params);

        pPool->join();

        if (init_params.m_canceled)
            return false;

        return true;
    }

    bool dxt_image::unpack(image_u8 &img) const
    {
        if (!m_total_elements)
            return false;

        img.crop(m_width, m_height);

        color_quad_u8 pixels[cDXTBlockSize * cDXTBlockSize];
        for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
            pixels[i].set(0, 0, 0, 255);

        bool all_blocks_valid = true;
        for (uint32_t block_y = 0; block_y < m_blocks_y; block_y++)
        {
            const uint32_t pixel_ofs_y = block_y * cDXTBlockSize;
            const uint32_t limit_y = math::minimum<uint32_t>(cDXTBlockSize, img.get_height() - pixel_ofs_y);

            for (uint32_t block_x = 0; block_x < m_blocks_x; block_x++)
            {
                if (!get_block_pixels(block_x, block_y, pixels))
                    all_blocks_valid = false;

                const uint32_t pixel_ofs_x = block_x * cDXTBlockSize;

                const uint32_t limit_x = math::minimum<uint32_t>(cDXTBlockSize, img.get_width() - pixel_ofs_x);

                for (uint32_t y = 0; y < limit_y; y++)
                {
                    const uint32_t iy = pixel_ofs_y + y;

                    for (uint32_t x = 0; x < limit_x; x++)
                    {
                        const uint32_t ix = pixel_ofs_x + x;

                        img(ix, iy) = pixels[x + (y << cDXTBlockShift)];
                    }
                }
            }
        }

        if (!all_blocks_valid)
            vogl_error_printf("dxt_image::unpack: One or more invalid blocks encountered!\n");

        img.reset_comp_flags();
        img.set_component_valid(0, false);
        img.set_component_valid(1, false);
        img.set_component_valid(2, false);
        for (uint32_t i = 0; i < m_num_elements_per_block; i++)
        {
            if (m_element_component_index[i] < 0)
            {
                img.set_component_valid(0, true);
                img.set_component_valid(1, true);
                img.set_component_valid(2, true);
            }
            else
                img.set_component_valid(m_element_component_index[i], true);
        }

        img.set_component_valid(3, get_dxt_format_has_alpha(m_format));

        return true;
    }

    void dxt_image::endian_swap()
    {
        utils::endian_switch_words(reinterpret_cast<uint16_t *>(m_elements.get_ptr()), m_elements.size_in_bytes() / sizeof(uint16_t));
    }

    const dxt_image::element &dxt_image::get_element(uint32_t block_x, uint32_t block_y, uint32_t element_index) const
    {
        VOGL_ASSERT((block_x < m_blocks_x) && (block_y < m_blocks_y) && (element_index < m_num_elements_per_block));
        return m_pElements[(block_x + block_y * m_blocks_x) * m_num_elements_per_block + element_index];
    }

    dxt_image::element &dxt_image::get_element(uint32_t block_x, uint32_t block_y, uint32_t element_index)
    {
        VOGL_ASSERT((block_x < m_blocks_x) && (block_y < m_blocks_y) && (element_index < m_num_elements_per_block));
        return m_pElements[(block_x + block_y * m_blocks_x) * m_num_elements_per_block + element_index];
    }

    bool dxt_image::has_alpha() const
    {
        switch (m_format)
        {
            case cDXT1:
            {
                for (uint32_t i = 0; i < m_total_elements; i++)
                {
                    const dxt1_block &blk = *(dxt1_block *)&m_pElements[i];

                    if (blk.get_low_color() <= blk.get_high_color())
                    {
                        for (uint32_t y = 0; y < cDXTBlockSize; y++)
                            for (uint32_t x = 0; x < cDXTBlockSize; x++)
                                if (blk.get_selector(x, y) == 3)
                                    return true;
                    }
                }

                break;
            }
            case cDXT1A:
            case cDXT3:
            case cDXT5:
            case cDXT5A:
                return true;
            default:
                break;
        }

        return false;
    }

    color_quad_u8 dxt_image::get_pixel(uint32_t x, uint32_t y) const
    {
        VOGL_ASSERT((x < m_width) && (y < m_height));

        const uint32_t block_x = x >> cDXTBlockShift;
        const uint32_t block_y = y >> cDXTBlockShift;

        const element *pElement = reinterpret_cast<const element *>(&get_element(block_x, block_y, 0));

        color_quad_u8 result(0, 0, 0, 255);

        for (uint32_t element_index = 0; element_index < m_num_elements_per_block; element_index++, pElement++)
        {
            switch (m_element_type[element_index])
            {
                case cColorETC1:
                {
                    const etc1_block &block = *reinterpret_cast<const etc1_block *>(&get_element(block_x, block_y, element_index));

                    const bool diff_flag = block.get_diff_bit();
                    const bool flip_flag = block.get_flip_bit();
                    const uint32_t table_index0 = block.get_inten_table(0);
                    const uint32_t table_index1 = block.get_inten_table(1);
                    color_quad_u8 subblock_colors0[4], subblock_colors1[4];

                    if (diff_flag)
                    {
                        const uint16_t base_color5 = block.get_base5_color();
                        const uint16_t delta_color3 = block.get_delta3_color();
                        etc1_block::get_diff_subblock_colors(subblock_colors0, base_color5, table_index0);
                        etc1_block::get_diff_subblock_colors(subblock_colors1, base_color5, delta_color3, table_index1);
                    }
                    else
                    {
                        const uint16_t base_color4_0 = block.get_base4_color(0);
                        etc1_block::get_abs_subblock_colors(subblock_colors0, base_color4_0, table_index0);
                        const uint16_t base_color4_1 = block.get_base4_color(1);
                        etc1_block::get_abs_subblock_colors(subblock_colors1, base_color4_1, table_index1);
                    }

                    const uint32_t bx = x & 3;
                    const uint32_t by = y & 3;

                    const uint32_t selector_index = block.get_selector(bx, by);
                    if (flip_flag)
                    {
                        if (by <= 2)
                            result = subblock_colors0[selector_index];
                        else
                            result = subblock_colors1[selector_index];
                    }
                    else
                    {
                        if (bx <= 2)
                            result = subblock_colors0[selector_index];
                        else
                            result = subblock_colors1[selector_index];
                    }

                    break;
                }
                case cColorDXT1:
                {
                    const dxt1_block *pBlock = reinterpret_cast<const dxt1_block *>(&get_element(block_x, block_y, element_index));

                    const uint32_t l = pBlock->get_low_color();
                    const uint32_t h = pBlock->get_high_color();

                    color_quad_u8 c0(dxt1_block::unpack_color(static_cast<uint16_t>(l), true));
                    color_quad_u8 c1(dxt1_block::unpack_color(static_cast<uint16_t>(h), true));

                    const uint32_t s = pBlock->get_selector(x & 3, y & 3);

                    if (l > h)
                    {
                        switch (s)
                        {
                            case 0:
                                result.set_noclamp_rgb(c0.r, c0.g, c0.b);
                                break;
                            case 1:
                                result.set_noclamp_rgb(c1.r, c1.g, c1.b);
                                break;
                            case 2:
                                result.set_noclamp_rgb((c0.r * 2 + c1.r) / 3, (c0.g * 2 + c1.g) / 3, (c0.b * 2 + c1.b) / 3);
                                break;
                            case 3:
                                result.set_noclamp_rgb((c1.r * 2 + c0.r) / 3, (c1.g * 2 + c0.g) / 3, (c1.b * 2 + c0.b) / 3);
                                break;
                        }
                    }
                    else
                    {
                        switch (s)
                        {
                            case 0:
                                result.set_noclamp_rgb(c0.r, c0.g, c0.b);
                                break;
                            case 1:
                                result.set_noclamp_rgb(c1.r, c1.g, c1.b);
                                break;
                            case 2:
                                result.set_noclamp_rgb((c0.r + c1.r) >> 1U, (c0.g + c1.g) >> 1U, (c0.b + c1.b) >> 1U);
                                break;
                            case 3:
                            {
                                if (m_format <= cDXT1A)
                                    result.set_noclamp_rgba(0, 0, 0, 0);
                                else
                                    result.set_noclamp_rgb(0, 0, 0);
                                break;
                            }
                        }
                    }

                    break;
                }
                case cAlphaDXT5:
                {
                    const int comp_index = m_element_component_index[element_index];

                    const dxt5_block *pBlock = reinterpret_cast<const dxt5_block *>(&get_element(block_x, block_y, element_index));

                    const uint32_t l = pBlock->get_low_alpha();
                    const uint32_t h = pBlock->get_high_alpha();

                    const uint32_t s = pBlock->get_selector(x & 3, y & 3);

                    if (l > h)
                    {
                        switch (s)
                        {
                            case 0:
                                result[comp_index] = static_cast<uint8_t>(l);
                                break;
                            case 1:
                                result[comp_index] = static_cast<uint8_t>(h);
                                break;
                            case 2:
                                result[comp_index] = static_cast<uint8_t>((l * 6 + h) / 7);
                                break;
                            case 3:
                                result[comp_index] = static_cast<uint8_t>((l * 5 + h * 2) / 7);
                                break;
                            case 4:
                                result[comp_index] = static_cast<uint8_t>((l * 4 + h * 3) / 7);
                                break;
                            case 5:
                                result[comp_index] = static_cast<uint8_t>((l * 3 + h * 4) / 7);
                                break;
                            case 6:
                                result[comp_index] = static_cast<uint8_t>((l * 2 + h * 5) / 7);
                                break;
                            case 7:
                                result[comp_index] = static_cast<uint8_t>((l + h * 6) / 7);
                                break;
                        }
                    }
                    else
                    {
                        switch (s)
                        {
                            case 0:
                                result[comp_index] = static_cast<uint8_t>(l);
                                break;
                            case 1:
                                result[comp_index] = static_cast<uint8_t>(h);
                                break;
                            case 2:
                                result[comp_index] = static_cast<uint8_t>((l * 4 + h) / 5);
                                break;
                            case 3:
                                result[comp_index] = static_cast<uint8_t>((l * 3 + h * 2) / 5);
                                break;
                            case 4:
                                result[comp_index] = static_cast<uint8_t>((l * 2 + h * 3) / 5);
                                break;
                            case 5:
                                result[comp_index] = static_cast<uint8_t>((l + h * 4) / 5);
                                break;
                            case 6:
                                result[comp_index] = 0;
                                break;
                            case 7:
                                result[comp_index] = 255;
                                break;
                        }
                    }

                    break;
                }
                case cAlphaDXT3:
                {
                    const int comp_index = m_element_component_index[element_index];

                    const dxt3_block *pBlock = reinterpret_cast<const dxt3_block *>(&get_element(block_x, block_y, element_index));

                    result[comp_index] = static_cast<uint8_t>(pBlock->get_alpha(x & 3, y & 3, true));

                    break;
                }
                default:
                    break;
            }
        }

        return result;
    }

    uint32_t dxt_image::get_pixel_alpha(uint32_t x, uint32_t y, uint32_t element_index) const
    {
        VOGL_ASSERT((x < m_width) && (y < m_height) && (element_index < m_num_elements_per_block));

        const uint32_t block_x = x >> cDXTBlockShift;
        const uint32_t block_y = y >> cDXTBlockShift;

        switch (m_element_type[element_index])
        {
            case cColorDXT1:
            {
                if (m_format <= cDXT1A)
                {
                    const dxt1_block *pBlock = reinterpret_cast<const dxt1_block *>(&get_element(block_x, block_y, element_index));

                    const uint32_t l = pBlock->get_low_color();
                    const uint32_t h = pBlock->get_high_color();

                    if (l <= h)
                    {
                        uint32_t s = pBlock->get_selector(x & 3, y & 3);

                        return (s == 3) ? 0 : 255;
                    }
                    else
                    {
                        return 255;
                    }
                }

                break;
            }
            case cAlphaDXT5:
            {
                const dxt5_block *pBlock = reinterpret_cast<const dxt5_block *>(&get_element(block_x, block_y, element_index));

                const uint32_t l = pBlock->get_low_alpha();
                const uint32_t h = pBlock->get_high_alpha();

                const uint32_t s = pBlock->get_selector(x & 3, y & 3);

                if (l > h)
                {
                    switch (s)
                    {
                        case 0:
                            return l;
                        case 1:
                            return h;
                        case 2:
                            return (l * 6 + h) / 7;
                        case 3:
                            return (l * 5 + h * 2) / 7;
                        case 4:
                            return (l * 4 + h * 3) / 7;
                        case 5:
                            return (l * 3 + h * 4) / 7;
                        case 6:
                            return (l * 2 + h * 5) / 7;
                        case 7:
                            return (l + h * 6) / 7;
                    }
                }
                else
                {
                    switch (s)
                    {
                        case 0:
                            return l;
                        case 1:
                            return h;
                        case 2:
                            return (l * 4 + h) / 5;
                        case 3:
                            return (l * 3 + h * 2) / 5;
                        case 4:
                            return (l * 2 + h * 3) / 5;
                        case 5:
                            return (l + h * 4) / 5;
                        case 6:
                            return 0;
                        case 7:
                            return 255;
                    }
                }
            }
            case cAlphaDXT3:
            {
                const dxt3_block *pBlock = reinterpret_cast<const dxt3_block *>(&get_element(block_x, block_y, element_index));

                return pBlock->get_alpha(x & 3, y & 3, true);
            }
            default:
                break;
        }

        return 255;
    }

    void dxt_image::set_pixel(uint32_t x, uint32_t y, const color_quad_u8 &c, bool perceptual)
    {
        VOGL_ASSERT((x < m_width) && (y < m_height));

        const uint32_t block_x = x >> cDXTBlockShift;
        const uint32_t block_y = y >> cDXTBlockShift;

        element *pElement = &get_element(block_x, block_y, 0);

        for (uint32_t element_index = 0; element_index < m_num_elements_per_block; element_index++, pElement++)
        {
            switch (m_element_type[element_index])
            {
                case cColorETC1:
                {
                    etc1_block &block = *reinterpret_cast<etc1_block *>(&get_element(block_x, block_y, element_index));

                    const bool diff_flag = block.get_diff_bit();
                    const bool flip_flag = block.get_flip_bit();
                    const uint32_t table_index0 = block.get_inten_table(0);
                    const uint32_t table_index1 = block.get_inten_table(1);
                    color_quad_u8 subblock_colors0[4], subblock_colors1[4];

                    if (diff_flag)
                    {
                        const uint16_t base_color5 = block.get_base5_color();
                        const uint16_t delta_color3 = block.get_delta3_color();
                        etc1_block::get_diff_subblock_colors(subblock_colors0, base_color5, table_index0);
                        etc1_block::get_diff_subblock_colors(subblock_colors1, base_color5, delta_color3, table_index1);
                    }
                    else
                    {
                        const uint16_t base_color4_0 = block.get_base4_color(0);
                        etc1_block::get_abs_subblock_colors(subblock_colors0, base_color4_0, table_index0);
                        const uint16_t base_color4_1 = block.get_base4_color(1);
                        etc1_block::get_abs_subblock_colors(subblock_colors1, base_color4_1, table_index1);
                    }

                    const uint32_t bx = x & 3;
                    const uint32_t by = y & 3;

                    color_quad_u8 *pColors = subblock_colors1;
                    if (flip_flag)
                    {
                        if (by <= 2)
                            pColors = subblock_colors0;
                    }
                    else
                    {
                        if (bx <= 2)
                            pColors = subblock_colors0;
                    }

                    uint32_t best_error = UINT_MAX;
                    uint32_t best_selector = 0;

                    for (uint32_t i = 0; i < 4; i++)
                    {
                        uint32_t error = color::color_distance(perceptual, pColors[i], c, false);
                        if (error < best_error)
                        {
                            best_error = error;
                            best_selector = i;
                        }
                    }

                    block.set_selector(bx, by, best_selector);
                    break;
                }
                case cColorDXT1:
                {
                    dxt1_block *pDXT1_block = reinterpret_cast<dxt1_block *>(pElement);

                    color_quad_u8 colors[cDXT1SelectorValues];
                    const uint32_t n = pDXT1_block->get_block_colors(colors, static_cast<uint16_t>(pDXT1_block->get_low_color()), static_cast<uint16_t>(pDXT1_block->get_high_color()));

                    if ((m_format == cDXT1A) && (c.a < 128))
                        pDXT1_block->set_selector(x & 3, y & 3, 3);
                    else
                    {
                        uint32_t best_error = UINT_MAX;
                        uint32_t best_selector = 0;

                        for (uint32_t i = 0; i < n; i++)
                        {
                            uint32_t error = color::color_distance(perceptual, colors[i], c, false);
                            if (error < best_error)
                            {
                                best_error = error;
                                best_selector = i;
                            }
                        }

                        pDXT1_block->set_selector(x & 3, y & 3, best_selector);
                    }

                    break;
                }
                case cAlphaDXT5:
                {
                    dxt5_block *pDXT5_block = reinterpret_cast<dxt5_block *>(pElement);

                    uint32_t values[cDXT5SelectorValues];
                    dxt5_block::get_block_values(values, pDXT5_block->get_low_alpha(), pDXT5_block->get_high_alpha());

                    const int comp_index = m_element_component_index[element_index];

                    uint32_t best_error = UINT_MAX;
                    uint32_t best_selector = 0;

                    for (uint32_t i = 0; i < cDXT5SelectorValues; i++)
                    {
                        uint32_t error = static_cast<uint32_t>(labs(values[i]) - c[comp_index]); // no need to square

                        if (error < best_error)
                        {
                            best_error = error;
                            best_selector = i;
                        }
                    }

                    pDXT5_block->set_selector(x & 3, y & 3, best_selector);

                    break;
                }
                case cAlphaDXT3:
                {
                    const int comp_index = m_element_component_index[element_index];

                    dxt3_block *pDXT3_block = reinterpret_cast<dxt3_block *>(pElement);

                    pDXT3_block->set_alpha(x & 3, y & 3, c[comp_index], true);

                    break;
                }
                default:
                    break;
            }
        } // element_index
    }

    bool dxt_image::get_block_pixels(uint32_t block_x, uint32_t block_y, color_quad_u8 *pPixels) const
    {
        bool success = true;
        const element *pElement = &get_element(block_x, block_y, 0);

        for (uint32_t element_index = 0; element_index < m_num_elements_per_block; element_index++, pElement++)
        {
            switch (m_element_type[element_index])
            {
                case cColorETC1:
                {
                    const etc1_block &block = *reinterpret_cast<const etc1_block *>(&get_element(block_x, block_y, element_index));
// Preserve alpha if the format is something weird (like ETC1 for color and DXT5A for alpha) - which isn't currently supported.
                    if (!rg_etc1::unpack_etc1_block(&block, (uint32_t *)pPixels, m_format != cETC1))
                        success = false;

                    break;
                }
                case cColorDXT1:
                {
                    const dxt1_block *pDXT1_block = reinterpret_cast<const dxt1_block *>(pElement);

                    color_quad_u8 colors[cDXT1SelectorValues];
                    pDXT1_block->get_block_colors(colors, static_cast<uint16_t>(pDXT1_block->get_low_color()), static_cast<uint16_t>(pDXT1_block->get_high_color()));

                    for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                    {
                        uint32_t s = pDXT1_block->get_selector(i & 3, i >> 2);

                        pPixels[i].r = colors[s].r;
                        pPixels[i].g = colors[s].g;
                        pPixels[i].b = colors[s].b;

                        if (m_format <= cDXT1A)
                            pPixels[i].a = colors[s].a;
                    }

                    break;
                }
                case cAlphaDXT5:
                {
                    const dxt5_block *pDXT5_block = reinterpret_cast<const dxt5_block *>(pElement);

                    uint32_t values[cDXT5SelectorValues];
                    dxt5_block::get_block_values(values, pDXT5_block->get_low_alpha(), pDXT5_block->get_high_alpha());

                    const int comp_index = m_element_component_index[element_index];

                    for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                    {
                        uint32_t s = pDXT5_block->get_selector(i & 3, i >> 2);

                        pPixels[i][comp_index] = static_cast<uint8_t>(values[s]);
                    }

                    break;
                }
                case cAlphaDXT3:
                {
                    const dxt3_block *pDXT3_block = reinterpret_cast<const dxt3_block *>(pElement);

                    const int comp_index = m_element_component_index[element_index];

                    for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                    {
                        uint32_t a = pDXT3_block->get_alpha(i & 3, i >> 2, true);

                        pPixels[i][comp_index] = static_cast<uint8_t>(a);
                    }

                    break;
                }
                default:
                    break;
            }
        } // element_index
        return success;
    }

    void dxt_image::set_block_pixels(uint32_t block_x, uint32_t block_y, const color_quad_u8 *pPixels, const pack_params &p)
    {
        set_block_pixels_context context;
        set_block_pixels(block_x, block_y, pPixels, p, context);
    }

    void dxt_image::set_block_pixels(
        uint32_t block_x, uint32_t block_y, const color_quad_u8 *pPixels, const pack_params &p,
        set_block_pixels_context &context)
    {
        element *pElement = &get_element(block_x, block_y, 0);

        if (m_format == cETC1)
        {
            etc1_block &dst_block = *reinterpret_cast<etc1_block *>(pElement);

            rg_etc1::etc1_quality etc_quality = rg_etc1::cHighQuality;
            if (p.m_quality <= cCRNDXTQualityFast)
                etc_quality = rg_etc1::cLowQuality;
            else if (p.m_quality <= cCRNDXTQualityNormal)
                etc_quality = rg_etc1::cMediumQuality;

            rg_etc1::etc1_pack_params pack_params;
            pack_params.m_dithering = p.m_dithering;
            //pack_params.m_perceptual = p.m_perceptual;
            pack_params.m_quality = etc_quality;
            rg_etc1::pack_etc1_block(&dst_block, (uint32_t *)pPixels, pack_params);
        }
        else
#if VOGL_SUPPORT_SQUISH
            if ((p.m_compressor == cCRNDXTCompressorSquish) && ((m_format == cDXT1) || (m_format == cDXT1A) || (m_format == cDXT3) || (m_format == cDXT5) || (m_format == cDXT5A)))
        {
            uint32_t squish_flags = 0;
            if ((m_format == cDXT1) || (m_format == cDXT1A))
                squish_flags = squish::kDxt1;
            else if (m_format == cDXT3)
                squish_flags = squish::kDxt3;
            else if (m_format == cDXT5A)
                squish_flags = squish::kDxt5A;
            else
                squish_flags = squish::kDxt5;

            if (p.m_perceptual)
                squish_flags |= squish::kColourMetricPerceptual;
            else
                squish_flags |= squish::kColourMetricUniform;

            if (p.m_quality >= cCRNDXTQualityBetter)
                squish_flags |= squish::kColourIterativeClusterFit;
            else if (p.m_quality == cCRNDXTQualitySuperFast)
                squish_flags |= squish::kColourRangeFit;

            color_quad_u8 pixels[cDXTBlockSize * cDXTBlockSize];

            memcpy(pixels, pPixels, sizeof(color_quad_u8) * cDXTBlockSize * cDXTBlockSize);

            if (m_format == cDXT1)
            {
                for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                    pixels[i].a = 255;
            }
            else if (m_format == cDXT1A)
            {
                for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                    if (pixels[i].a < p.m_dxt1a_alpha_threshold)
                        pixels[i].a = 0;
                    else
                        pixels[i].a = 255;
            }

            squish::Compress(reinterpret_cast<const squish::u8 *>(pixels), pElement, squish_flags);
        }
        else
#endif // VOGL_SUPPORT_SQUISH
            // RYG doesn't support DXT1A
            if ((p.m_compressor == cCRNDXTCompressorRYG) && ((m_format == cDXT1) || (m_format == cDXT5) || (m_format == cDXT5A)))
        {
            color_quad_u8 pixels[cDXTBlockSize * cDXTBlockSize];

            for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
            {
                pixels[i].r = pPixels[i].b;
                pixels[i].g = pPixels[i].g;
                pixels[i].b = pPixels[i].r;

                if (m_format == cDXT1)
                    pixels[i].a = 255;
                else
                    pixels[i].a = pPixels[i].a;
            }

            if (m_format == cDXT5A)
                ryg_dxt::sCompressDXT5ABlock((sU8 *)pElement, (const sU32 *)pixels, 0);
            else
                ryg_dxt::sCompressDXTBlock((sU8 *)pElement, (const sU32 *)pixels, m_format == cDXT5, 0);
        }
        else if ((p.m_compressor == cCRNDXTCompressorCRNF) && (m_format != cDXT1A))
        {
            for (uint32_t element_index = 0; element_index < m_num_elements_per_block; element_index++, pElement++)
            {
                switch (m_element_type[element_index])
                {
                    case cColorDXT1:
                    {
                        dxt1_block *pDXT1_block = reinterpret_cast<dxt1_block *>(pElement);
                        dxt_fast::compress_color_block(pDXT1_block, pPixels, p.m_quality >= cCRNDXTQualityNormal);

                        break;
                    }
                    case cAlphaDXT5:
                    {
                        dxt5_block *pDXT5_block = reinterpret_cast<dxt5_block *>(pElement);
                        dxt_fast::compress_alpha_block(pDXT5_block, pPixels, m_element_component_index[element_index]);

                        break;
                    }
                    case cAlphaDXT3:
                    {
                        const int comp_index = m_element_component_index[element_index];

                        dxt3_block *pDXT3_block = reinterpret_cast<dxt3_block *>(pElement);

                        for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                            pDXT3_block->set_alpha(i & 3, i >> 2, pPixels[i][comp_index], true);

                        break;
                    }
                    default:
                        break;
                }
            }
        }
        else
        {
            dxt1_endpoint_optimizer &dxt1_optimizer = context.m_dxt1_optimizer;
            dxt5_endpoint_optimizer &dxt5_optimizer = context.m_dxt5_optimizer;

            for (uint32_t element_index = 0; element_index < m_num_elements_per_block; element_index++, pElement++)
            {
                switch (m_element_type[element_index])
                {
                    case cColorDXT1:
                    {
                        dxt1_block *pDXT1_block = reinterpret_cast<dxt1_block *>(pElement);

                        bool pixels_have_alpha = false;
                        if (m_format == cDXT1A)
                        {
                            for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                                if (pPixels[i].a < p.m_dxt1a_alpha_threshold)
                                {
                                    pixels_have_alpha = true;
                                    break;
                                }
                        }

                        dxt1_endpoint_optimizer::results results;
                        uint8_t selectors[cDXTBlockSize * cDXTBlockSize];
                        results.m_pSelectors = selectors;

                        dxt1_endpoint_optimizer::params params;
                        params.m_block_index = block_x + block_y * m_blocks_x;
                        params.m_quality = p.m_quality;
                        params.m_perceptual = p.m_perceptual;
                        params.m_grayscale_sampling = p.m_grayscale_sampling;
                        params.m_pixels_have_alpha = pixels_have_alpha;
                        params.m_use_alpha_blocks = p.m_use_both_block_types;
                        params.m_use_transparent_indices_for_black = p.m_use_transparent_indices_for_black;
                        params.m_dxt1a_alpha_threshold = p.m_dxt1a_alpha_threshold;
                        params.m_pPixels = pPixels;
                        params.m_num_pixels = cDXTBlockSize * cDXTBlockSize;
                        params.m_endpoint_caching = p.m_endpoint_caching;
                        params.m_color_weights[0] = p.m_color_weights[0];
                        params.m_color_weights[1] = p.m_color_weights[1];
                        params.m_color_weights[2] = p.m_color_weights[2];

                        if ((m_format != cDXT1) && (m_format != cDXT1A))
                            params.m_use_alpha_blocks = false;

                        if (!dxt1_optimizer.compute(params, results))
                        {
                            VOGL_ASSERT_ALWAYS;
                            break;
                        }

                        pDXT1_block->set_low_color(results.m_low_color);
                        pDXT1_block->set_high_color(results.m_high_color);

                        for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                            pDXT1_block->set_selector(i & 3, i >> 2, selectors[i]);

                        break;
                    }
                    case cAlphaDXT5:
                    {
                        dxt5_block *pDXT5_block = reinterpret_cast<dxt5_block *>(pElement);

                        dxt5_endpoint_optimizer::results results;

                        uint8_t selectors[cDXTBlockSize * cDXTBlockSize];
                        results.m_pSelectors = selectors;

                        dxt5_endpoint_optimizer::params params;
                        params.m_block_index = block_x + block_y * m_blocks_x;
                        params.m_pPixels = pPixels;
                        params.m_num_pixels = cDXTBlockSize * cDXTBlockSize;
                        params.m_comp_index = m_element_component_index[element_index];
                        params.m_quality = p.m_quality;
                        params.m_use_both_block_types = p.m_use_both_block_types;

                        if (!dxt5_optimizer.compute(params, results))
                        {
                            VOGL_ASSERT_ALWAYS;
                            break;
                        }

                        pDXT5_block->set_low_alpha(results.m_first_endpoint);
                        pDXT5_block->set_high_alpha(results.m_second_endpoint);

                        for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                            pDXT5_block->set_selector(i & 3, i >> 2, selectors[i]);

                        break;
                    }
                    case cAlphaDXT3:
                    {
                        const int comp_index = m_element_component_index[element_index];

                        dxt3_block *pDXT3_block = reinterpret_cast<dxt3_block *>(pElement);

                        for (uint32_t i = 0; i < cDXTBlockSize * cDXTBlockSize; i++)
                            pDXT3_block->set_alpha(i & 3, i >> 2, pPixels[i][comp_index], true);

                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    void dxt_image::get_block_endpoints(uint32_t block_x, uint32_t block_y, uint32_t element_index, uint32_t &packed_low_endpoint, uint32_t &packed_high_endpoint) const
    {
        const element &block = get_element(block_x, block_y, element_index);

        switch (m_element_type[element_index])
        {
            case cColorETC1:
            {
                const etc1_block &src_block = *reinterpret_cast<const etc1_block *>(&block);
                if (src_block.get_diff_bit())
                {
                    packed_low_endpoint = src_block.get_base5_color();
                    packed_high_endpoint = src_block.get_delta3_color();
                }
                else
                {
                    packed_low_endpoint = src_block.get_base4_color(0);
                    packed_high_endpoint = src_block.get_base4_color(1);
                }

                break;
            }
            case cColorDXT1:
            {
                const dxt1_block &block1 = *reinterpret_cast<const dxt1_block *>(&block);

                packed_low_endpoint = block1.get_low_color();
                packed_high_endpoint = block1.get_high_color();

                break;
            }
            case cAlphaDXT5:
            {
                const dxt5_block &block5 = *reinterpret_cast<const dxt5_block *>(&block);

                packed_low_endpoint = block5.get_low_alpha();
                packed_high_endpoint = block5.get_high_alpha();

                break;
            }
            case cAlphaDXT3:
            {
                packed_low_endpoint = 0;
                packed_high_endpoint = 255;

                break;
            }
            default:
                break;
        }
    }

    int dxt_image::get_block_endpoints(uint32_t block_x, uint32_t block_y, uint32_t element_index, color_quad_u8 &low_endpoint, color_quad_u8 &high_endpoint, bool scaled) const
    {
        uint32_t l = 0, h = 0;
        get_block_endpoints(block_x, block_y, element_index, l, h);

        switch (m_element_type[element_index])
        {
            case cColorETC1:
            {
                const etc1_block &src_block = *reinterpret_cast<const etc1_block *>(&get_element(block_x, block_y, element_index));
                if (src_block.get_diff_bit())
                {
                    low_endpoint = etc1_block::unpack_color5(static_cast<uint16_t>(l), scaled);
                    etc1_block::unpack_color5(high_endpoint, static_cast<uint16_t>(l), static_cast<uint16_t>(h), scaled);
                }
                else
                {
                    low_endpoint = etc1_block::unpack_color4(static_cast<uint16_t>(l), scaled);
                    high_endpoint = etc1_block::unpack_color4(static_cast<uint16_t>(h), scaled);
                }

                return -1;
            }
            case cColorDXT1:
            {
                uint32_t r, g, b;

                dxt1_block::unpack_color(r, g, b, static_cast<uint16_t>(l), scaled);
                low_endpoint.r = static_cast<uint8_t>(r);
                low_endpoint.g = static_cast<uint8_t>(g);
                low_endpoint.b = static_cast<uint8_t>(b);

                dxt1_block::unpack_color(r, g, b, static_cast<uint16_t>(h), scaled);
                high_endpoint.r = static_cast<uint8_t>(r);
                high_endpoint.g = static_cast<uint8_t>(g);
                high_endpoint.b = static_cast<uint8_t>(b);

                return -1;
            }
            case cAlphaDXT5:
            {
                const int component = m_element_component_index[element_index];

                low_endpoint[component] = static_cast<uint8_t>(l);
                high_endpoint[component] = static_cast<uint8_t>(h);

                return component;
            }
            case cAlphaDXT3:
            {
                const int component = m_element_component_index[element_index];

                low_endpoint[component] = static_cast<uint8_t>(l);
                high_endpoint[component] = static_cast<uint8_t>(h);

                return component;
            }
            default:
                break;
        }

        return 0;
    }

    uint32_t dxt_image::get_block_colors(uint32_t block_x, uint32_t block_y, uint32_t element_index, color_quad_u8 *pColors, uint32_t subblock_index)
    {
        const element &block = get_element(block_x, block_y, element_index);

        switch (m_element_type[element_index])
        {
            case cColorETC1:
            {
                const etc1_block &src_block = *reinterpret_cast<const etc1_block *>(&get_element(block_x, block_y, element_index));
                const uint32_t table_index0 = src_block.get_inten_table(0);
                const uint32_t table_index1 = src_block.get_inten_table(1);
                if (src_block.get_diff_bit())
                {
                    const uint16_t base_color5 = src_block.get_base5_color();
                    const uint16_t delta_color3 = src_block.get_delta3_color();
                    if (subblock_index)
                        etc1_block::get_diff_subblock_colors(pColors, base_color5, delta_color3, table_index1);
                    else
                        etc1_block::get_diff_subblock_colors(pColors, base_color5, table_index0);
                }
                else
                {
                    if (subblock_index)
                    {
                        const uint16_t base_color4_1 = src_block.get_base4_color(1);
                        etc1_block::get_abs_subblock_colors(pColors, base_color4_1, table_index1);
                    }
                    else
                    {
                        const uint16_t base_color4_0 = src_block.get_base4_color(0);
                        etc1_block::get_abs_subblock_colors(pColors, base_color4_0, table_index0);
                    }
                }

                break;
            }
            case cColorDXT1:
            {
                const dxt1_block &block1 = *reinterpret_cast<const dxt1_block *>(&block);
                return dxt1_block::get_block_colors(pColors, static_cast<uint16_t>(block1.get_low_color()), static_cast<uint16_t>(block1.get_high_color()));
            }
            case cAlphaDXT5:
            {
                const dxt5_block &block5 = *reinterpret_cast<const dxt5_block *>(&block);

                uint32_t values[cDXT5SelectorValues];

                const uint32_t n = dxt5_block::get_block_values(values, block5.get_low_alpha(), block5.get_high_alpha());

                const int comp_index = m_element_component_index[element_index];
                for (uint32_t i = 0; i < n; i++)
                    pColors[i][comp_index] = static_cast<uint8_t>(values[i]);

                return n;
            }
            case cAlphaDXT3:
            {
                const int comp_index = m_element_component_index[element_index];
                for (uint32_t i = 0; i < 16; i++)
                    pColors[i][comp_index] = static_cast<uint8_t>((i << 4) | i);

                return 16;
            }
            default:
                break;
        }

        return 0;
    }

    uint32_t dxt_image::get_subblock_index(uint32_t x, uint32_t y, uint32_t element_index) const
    {
        if (m_element_type[element_index] != cColorETC1)
            return 0;

        const uint32_t block_x = x >> cDXTBlockShift;
        const uint32_t block_y = y >> cDXTBlockShift;

        const element &block = get_element(block_x, block_y, element_index);

        const etc1_block &src_block = *reinterpret_cast<const etc1_block *>(&block);
        if (src_block.get_flip_bit())
        {
            return ((y & 3) >= 2) ? 1 : 0;
        }
        else
        {
            return ((x & 3) >= 2) ? 1 : 0;
        }
    }

    uint32_t dxt_image::get_total_subblocks(uint32_t element_index) const
    {
        return (m_element_type[element_index] == cColorETC1) ? 2 : 0;
    }

    uint32_t dxt_image::get_selector(uint32_t x, uint32_t y, uint32_t element_index) const
    {
        VOGL_ASSERT((x < m_width) && (y < m_height));

        const uint32_t block_x = x >> cDXTBlockShift;
        const uint32_t block_y = y >> cDXTBlockShift;

        const element &block = get_element(block_x, block_y, element_index);

        switch (m_element_type[element_index])
        {
            case cColorETC1:
            {
                const etc1_block &src_block = *reinterpret_cast<const etc1_block *>(&block);
                return src_block.get_selector(x & 3, y & 3);
            }
            case cColorDXT1:
            {
                const dxt1_block &block1 = *reinterpret_cast<const dxt1_block *>(&block);
                return block1.get_selector(x & 3, y & 3);
            }
            case cAlphaDXT5:
            {
                const dxt5_block &block5 = *reinterpret_cast<const dxt5_block *>(&block);
                return block5.get_selector(x & 3, y & 3);
            }
            case cAlphaDXT3:
            {
                const dxt3_block &block3 = *reinterpret_cast<const dxt3_block *>(&block);
                return block3.get_alpha(x & 3, y & 3, false);
            }
            default:
                break;
        }

        return 0;
    }

    void dxt_image::change_dxt1_to_dxt1a()
    {
        if (m_format == cDXT1)
            m_format = cDXT1A;
    }

    void dxt_image::flip_col(uint32_t x)
    {
        const uint32_t other_x = (m_blocks_x - 1) - x;
        for (uint32_t y = 0; y < m_blocks_y; y++)
        {
            for (uint32_t e = 0; e < get_elements_per_block(); e++)
            {
                element tmp[2] = { get_element(x, y, e), get_element(other_x, y, e) };

                for (uint32_t i = 0; i < 2; i++)
                {
                    switch (get_element_type(e))
                    {
                        case cColorDXT1:
                            reinterpret_cast<dxt1_block *>(&tmp[i])->flip_x();
                            break;
                        case cAlphaDXT3:
                            reinterpret_cast<dxt3_block *>(&tmp[i])->flip_x();
                            break;
                        case cAlphaDXT5:
                            reinterpret_cast<dxt5_block *>(&tmp[i])->flip_x();
                            break;
                        default:
                            VOGL_ASSERT_ALWAYS;
                            break;
                    }
                }

                get_element(x, y, e) = tmp[1];
                get_element(other_x, y, e) = tmp[0];
            }
        }
    }

    void dxt_image::flip_row(uint32_t y)
    {
        const uint32_t other_y = (m_blocks_y - 1) - y;
        for (uint32_t x = 0; x < m_blocks_x; x++)
        {
            for (uint32_t e = 0; e < get_elements_per_block(); e++)
            {
                element tmp[2] = { get_element(x, y, e), get_element(x, other_y, e) };

                for (uint32_t i = 0; i < 2; i++)
                {
                    switch (get_element_type(e))
                    {
                        case cColorDXT1:
                            reinterpret_cast<dxt1_block *>(&tmp[i])->flip_y();
                            break;
                        case cAlphaDXT3:
                            reinterpret_cast<dxt3_block *>(&tmp[i])->flip_y();
                            break;
                        case cAlphaDXT5:
                            reinterpret_cast<dxt5_block *>(&tmp[i])->flip_y();
                            break;
                        default:
                            VOGL_ASSERT_ALWAYS;
                            break;
                    }
                }

                get_element(x, y, e) = tmp[1];
                get_element(x, other_y, e) = tmp[0];
            }
        }
    }

    bool dxt_image::can_flip(uint32_t axis_index)
    {
        if (m_format == cETC1)
        {
            // Can't reliably flip ETC1 textures (because of asymmetry in the 555/333 differential coding of subblock colors).
            return false;
        }

        uint32_t d;
        if (axis_index)
            d = m_height;
        else
            d = m_width;

        if (d & 3)
        {
            if (d > 4)
                return false;
        }

        return true;
    }

    bool dxt_image::flip_x()
    {
        if (m_format == cETC1)
        {
            // Can't reliably flip ETC1 textures (because of asymmetry in the 555/333 differential coding of subblock colors).
            return false;
        }

        if ((m_width & 3) && (m_width > 4))
            return false;

        if (m_width == 1)
            return true;

        const uint32_t mid_x = m_blocks_x / 2;

        for (uint32_t x = 0; x < mid_x; x++)
            flip_col(x);

        if (m_blocks_x & 1)
        {
            const uint32_t w = math::minimum(m_width, 4U);
            for (uint32_t y = 0; y < m_blocks_y; y++)
            {
                for (uint32_t e = 0; e < get_elements_per_block(); e++)
                {
                    element tmp(get_element(mid_x, y, e));
                    switch (get_element_type(e))
                    {
                        case cColorDXT1:
                            reinterpret_cast<dxt1_block *>(&tmp)->flip_x(w, 4);
                            break;
                        case cAlphaDXT3:
                            reinterpret_cast<dxt3_block *>(&tmp)->flip_x(w, 4);
                            break;
                        case cAlphaDXT5:
                            reinterpret_cast<dxt5_block *>(&tmp)->flip_x(w, 4);
                            break;
                        default:
                            VOGL_ASSERT_ALWAYS;
                            break;
                    }
                    get_element(mid_x, y, e) = tmp;
                }
            }
        }

        return true;
    }

    bool dxt_image::flip_y()
    {
        if (m_format == cETC1)
        {
            // Can't reliably flip ETC1 textures (because of asymmetry in the 555/333 differential coding of subblock colors).
            return false;
        }

        if ((m_height & 3) && (m_height > 4))
            return false;

        if (m_height == 1)
            return true;

        const uint32_t mid_y = m_blocks_y / 2;

        for (uint32_t y = 0; y < mid_y; y++)
            flip_row(y);

        if (m_blocks_y & 1)
        {
            const uint32_t h = math::minimum(m_height, 4U);
            for (uint32_t x = 0; x < m_blocks_x; x++)
            {
                for (uint32_t e = 0; e < get_elements_per_block(); e++)
                {
                    element tmp(get_element(x, mid_y, e));
                    switch (get_element_type(e))
                    {
                        case cColorDXT1:
                            reinterpret_cast<dxt1_block *>(&tmp)->flip_y(4, h);
                            break;
                        case cAlphaDXT3:
                            reinterpret_cast<dxt3_block *>(&tmp)->flip_y(4, h);
                            break;
                        case cAlphaDXT5:
                            reinterpret_cast<dxt5_block *>(&tmp)->flip_y(4, h);
                            break;
                        default:
                            VOGL_ASSERT_ALWAYS;
                            break;
                    }
                    get_element(x, mid_y, e) = tmp;
                }
            }
        }

        return true;
    }

} // namespace vogl
