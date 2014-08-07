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

// File: vogl_image_utils.cpp
#include "vogl_core.h"
#include "vogl_image_utils.h"
#include "vogl_console.h"
#include "vogl_resampler.h"
#include "vogl_threaded_resampler.h"
#include "vogl_strutils.h"
#include "vogl_file_utils.h"
#include "vogl_threading.h"
#include "vogl_miniz.h"
#include "vogl_jpge.h"
#include "vogl_cfile_stream.h"
#include "vogl_buffer_stream.h"
#include "vogl_vector2d.h"

#include "vogl_stb_image.h"

#include "vogl_jpgd.h"

#include "vogl_pixel_format.h"

namespace vogl
{
    const float cInfinitePSNR = 999999.0f;
    const uint32_t VOGL_LARGEST_SUPPORTED_IMAGE_DIMENSION = 16384;

    namespace image_utils
    {
        bool read_from_stream_stb(data_stream_serializer &serializer, image_u8 &img)
        {
            uint8_vec buf;
            if (!serializer.read_entire_file(buf))
                return false;

            int x = 0, y = 0, n = 0;
            unsigned char *pData = stbi_load_from_memory(buf.get_ptr(), buf.size_in_bytes(), &x, &y, &n, 4);

            if (!pData)
                return false;

            if ((x > (int)VOGL_LARGEST_SUPPORTED_IMAGE_DIMENSION) || (y > (int)VOGL_LARGEST_SUPPORTED_IMAGE_DIMENSION))
            {
                stbi_image_free(pData);
                return false;
            }

            const bool has_alpha = ((n == 2) || (n == 4));

            img.crop(x, y);

            bool grayscale = true;

            for (int py = 0; py < y; py++)
            {
                const color_quad_u8 *pSrc = reinterpret_cast<const color_quad_u8 *>(pData) + (py * x);
                color_quad_u8 *pDst = img.get_scanline(py);
                color_quad_u8 *pDst_end = pDst + x;

                while (pDst != pDst_end)
                {
                    color_quad_u8 c(*pSrc++);
                    if (!has_alpha)
                        c.a = 255;

                    if (!c.is_grayscale())
                        grayscale = false;

                    *pDst++ = c;
                }
            }

            stbi_image_free(pData);

            img.reset_comp_flags();
            img.set_grayscale(grayscale);
            img.set_component_valid(3, has_alpha);

            return true;
        }

        bool read_from_stream_jpgd(data_stream_serializer &serializer, image_u8 &img)
        {
            uint8_vec buf;
            if (!serializer.read_entire_file(buf))
                return false;

            int width = 0, height = 0, actual_comps = 0;
            unsigned char *pSrc_img = jpgd::decompress_jpeg_image_from_memory(buf.get_ptr(), buf.size_in_bytes(), &width, &height, &actual_comps, 4);
            if (!pSrc_img)
                return false;

            if (math::maximum(width, height) > (int)VOGL_LARGEST_SUPPORTED_IMAGE_DIMENSION)
            {
                vogl_free(pSrc_img);
                return false;
            }

            if (!img.grant_ownership(reinterpret_cast<color_quad_u8 *>(pSrc_img), width, height))
            {
                vogl_free(pSrc_img);
                return false;
            }

            img.reset_comp_flags();
            img.set_grayscale(actual_comps == 1);
            img.set_component_valid(3, false);

            return true;
        }

        bool read_from_stream(image_u8 &dest, data_stream_serializer &serializer, uint32_t read_flags)
        {
            if (read_flags > cReadFlagsAllFlags)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (!serializer.get_stream())
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            dynamic_string ext(serializer.get_name());
            file_utils::get_extension(ext);

            if ((ext == "jpg") || (ext == "jpeg"))
            {
                // Use my jpeg decoder by default because it supports progressive jpeg's.
                if ((read_flags & cReadFlagForceSTB) == 0)
                {
                    return image_utils::read_from_stream_jpgd(serializer, dest);
                }
            }

            return image_utils::read_from_stream_stb(serializer, dest);
        }

        bool read_from_file(image_u8 &dest, const char *pFilename, uint32_t read_flags)
        {
            if (read_flags > cReadFlagsAllFlags)
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            cfile_stream file_stream;
            if (!file_stream.open(pFilename))
                return false;

            data_stream_serializer serializer(file_stream);
            return read_from_stream(dest, serializer, read_flags);
        }

        bool write_to_file(const char *pFilename, const image_u8 &img, uint32_t write_flags, int grayscale_comp_index)
        {
            if ((grayscale_comp_index < -1) || (grayscale_comp_index > 3))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            if (!img.get_width())
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            dynamic_string ext(pFilename);
            bool is_jpeg = false;
            if (file_utils::get_extension(ext))
            {
                is_jpeg = ((ext == "jpg") || (ext == "jpeg"));

                if ((ext != "png") && (ext != "bmp") && (ext != "tga") && (!is_jpeg))
                {
                    vogl_error_printf("vogl::image_utils::write_to_file: Can only write .BMP, .TGA, .PNG, or .JPG files!\n");
                    return false;
                }
            }

            vogl::vector<uint8_t> temp;
            uint32_t num_src_chans = 0;
            const void *pSrc_img = NULL;

            if (is_jpeg)
            {
                write_flags |= cWriteFlagIgnoreAlpha;
            }

            if ((img.get_comp_flags() & pixel_format_helpers::cCompFlagGrayscale) || (write_flags & image_utils::cWriteFlagGrayscale))
            {
                VOGL_ASSERT(grayscale_comp_index < 4);
                if (grayscale_comp_index > 3)
                    grayscale_comp_index = 3;

                temp.resize(img.get_total_pixels());

                for (uint32_t y = 0; y < img.get_height(); y++)
                {
                    const color_quad_u8 *pSrc = img.get_scanline(y);
                    const color_quad_u8 *pSrc_end = pSrc + img.get_width();
                    uint8_t *pDst = &temp[y * img.get_width()];

                    if (img.get_comp_flags() & pixel_format_helpers::cCompFlagGrayscale)
                    {
                        while (pSrc != pSrc_end)
                            *pDst++ = (*pSrc++)[1];
                    }
                    else if (grayscale_comp_index < 0)
                    {
                        while (pSrc != pSrc_end)
                            *pDst++ = static_cast<uint8_t>((*pSrc++).get_luma());
                    }
                    else
                    {
                        while (pSrc != pSrc_end)
                            *pDst++ = (*pSrc++)[grayscale_comp_index];
                    }
                }

                pSrc_img = &temp[0];
                num_src_chans = 1;
            }
            else if ((!img.is_component_valid(3)) || (write_flags & cWriteFlagIgnoreAlpha))
            {
                temp.resize(img.get_total_pixels() * 3);

                for (uint32_t y = 0; y < img.get_height(); y++)
                {
                    const color_quad_u8 *pSrc = img.get_scanline(y);
                    const color_quad_u8 *pSrc_end = pSrc + img.get_width();
                    uint8_t *pDst = &temp[y * img.get_width() * 3];

                    while (pSrc != pSrc_end)
                    {
                        const color_quad_u8 c(*pSrc++);

                        pDst[0] = c.r;
                        pDst[1] = c.g;
                        pDst[2] = c.b;

                        pDst += 3;
                    }
                }

                num_src_chans = 3;
                pSrc_img = &temp[0];
            }
            else
            {
                num_src_chans = 4;
                pSrc_img = img.get_ptr();
            }

            bool success = false;
            if (ext == "png")
            {
                size_t png_image_size = 0;
                void *pPNG_image_data = tdefl_write_image_to_png_file_in_memory_ex(pSrc_img, img.get_width(), img.get_height(), num_src_chans, &png_image_size, MZ_DEFAULT_LEVEL, MZ_FALSE);
                if (!pPNG_image_data)
                    return false;
                success = file_utils::write_buf_to_file(pFilename, pPNG_image_data, png_image_size);
                mz_free(pPNG_image_data);
            }
            else if (is_jpeg)
            {
                jpge::params params;
                if (write_flags & cWriteFlagJPEGQualityLevelMask)
                    params.m_quality = math::clamp<uint32_t>((write_flags & cWriteFlagJPEGQualityLevelMask) >> cWriteFlagJPEGQualityLevelShift, 1U, 100U);
                params.m_two_pass_flag = (write_flags & cWriteFlagJPEGTwoPass) != 0;
                params.m_no_chroma_discrim_flag = (write_flags & cWriteFlagJPEGNoChromaDiscrim) != 0;

                if (write_flags & cWriteFlagJPEGH1V1)
                    params.m_subsampling = jpge::H1V1;
                else if (write_flags & cWriteFlagJPEGH2V1)
                    params.m_subsampling = jpge::H2V1;
                else if (write_flags & cWriteFlagJPEGH2V2)
                    params.m_subsampling = jpge::H2V2;

                success = jpge::compress_image_to_jpeg_file(pFilename, img.get_width(), img.get_height(), num_src_chans, (const jpge::uint8_t *)pSrc_img, params);
            }
            else
            {
                success = ((ext == "bmp" ? stbi_write_bmp : stbi_write_tga)(pFilename, img.get_width(), img.get_height(), num_src_chans, pSrc_img) == VOGL_TRUE);
            }
            return success;
        }

        bool write_to_file(const char *pFilename, const image_f &img, const vec4F &l, const vec4F &h, uint32_t write_flags, int grayscale_comp_index)
        {
            image_u8 temp(img.get_width(), img.get_height());

            vec4F scale(h - l);
            for (uint32_t i = 0; i < 4; i++)
                scale[i] = (scale[i] != 0.0f) ? (255.0f / scale[i]) : 0;

            for (uint32_t y = 0; y < img.get_height(); y++)
            {
                for (uint32_t x = 0; x < img.get_width(); x++)
                {
                    const color_quad_f &o = img(x, y);

                    color_quad_u8 c;
                    for (uint32_t i = 0; i < 4; i++)
                        c[i] = static_cast<uint8_t>(math::clamp(math::float_to_int_nearest((o[i] - l[i]) * scale[i]), 0, 255));

                    temp(x, y) = c;
                }
            }

            return write_to_file(pFilename, temp, write_flags, grayscale_comp_index);
        }

        bool has_alpha(const image_u8 &img)
        {
            for (uint32_t y = 0; y < img.get_height(); y++)
                for (uint32_t x = 0; x < img.get_width(); x++)
                    if (img(x, y).a < 255)
                        return true;

            return false;
        }

        void renorm_normal_map(image_u8 &img)
        {
            for (uint32_t y = 0; y < img.get_height(); y++)
            {
                for (uint32_t x = 0; x < img.get_width(); x++)
                {
                    color_quad_u8 &c = img(x, y);
                    if ((c.r == 128) && (c.g == 128) && (c.b == 128))
                        continue;

                    vec3F v(c.r, c.g, c.b);
                    v *= 1.0f / 255.0f;
                    v *= 2.0f;
                    v -= vec3F(1.0f);
                    v.clamp(-1.0f, 1.0f);

                    float length = v.length();
                    if (length < .077f)
                        c.set(128, 128, 128, c.a);
                    else if (fabs(length - 1.0f) > .077f)
                    {
                        if (length)
                            v /= length;

                        for (uint32_t i = 0; i < 3; i++)
                            c[i] = static_cast<uint8_t>(math::clamp<float>(floor((v[i] + 1.0f) * .5f * 255.0f + .5f), 0.0f, 255.0f));

                        if ((c.r == 128) && (c.g == 128))
                        {
                            if (c.b < 128)
                                c.b = 0;
                            else
                                c.b = 255;
                        }
                    }
                }
            }
        }

        bool is_normal_map(const image_u8 &img, const char *pFilename)
        {
            float score = 0.0f;

            uint32_t num_invalid_pixels = 0;

            // TODO: Derive better score from pixel mean, eigenvecs/vals
            //vogl::vector<vec3F> pixels;

            for (uint32_t y = 0; y < img.get_height(); y++)
            {
                for (uint32_t x = 0; x < img.get_width(); x++)
                {
                    const color_quad_u8 &c = img(x, y);

                    if (c.b < 123)
                    {
                        num_invalid_pixels++;
                        continue;
                    }
                    else if ((c.r != 128) || (c.g != 128) || (c.b != 128))
                    {
                        vec3F v(c.r, c.g, c.b);
                        v -= vec3F(128.0f);
                        v /= vec3F(127.0f);
                        //pixels.push_back(v);
                        v.clamp(-1.0f, 1.0f);

                        float norm = v.norm();
                        if ((norm < 0.83f) || (norm > 1.29f))
                            num_invalid_pixels++;
                    }
                }
            }

            score -= math::clamp(float(num_invalid_pixels) / (img.get_width() * img.get_height()) - .026f, 0.0f, 1.0f) * 5.0f;

            if (pFilename)
            {
                dynamic_string str(pFilename);
                str.tolower();

                if (str.contains("normal") || str.contains("local") || str.contains("nmap"))
                    score += 1.0f;

                if (str.contains("diffuse") || str.contains("spec") || str.contains("gloss"))
                    score -= 1.0f;
            }

            return score >= 0.0f;
        }

        bool resample_single_thread(const image_u8 &src, image_u8 &dst, const resample_params &params)
        {
            const uint32_t src_width = src.get_width();
            const uint32_t src_height = src.get_height();

            if (math::maximum(src_width, src_height) > VOGL_RESAMPLER_MAX_DIMENSION)
            {
                printf("Image is too large!\n");
                return EXIT_FAILURE;
            }

            const int cMaxComponents = 4;
            if (((int)params.m_num_comps < 1) || ((int)params.m_num_comps > (int)cMaxComponents))
                return false;

            const uint32_t dst_width = params.m_dst_width;
            const uint32_t dst_height = params.m_dst_height;

            if ((math::minimum(dst_width, dst_height) < 1) || (math::maximum(dst_width, dst_height) > VOGL_RESAMPLER_MAX_DIMENSION))
            {
                printf("Image is too large!\n");
                return EXIT_FAILURE;
            }

            if ((src_width == dst_width) && (src_height == dst_height))
            {
                dst = src;
                return true;
            }

            dst.clear();
            dst.crop(params.m_dst_width, params.m_dst_height);

            // Partial gamma correction looks better on mips. Set to 1.0 to disable gamma correction.
            const float source_gamma = params.m_source_gamma; //1.75f;

            float srgb_to_linear[256];
            if (params.m_srgb)
            {
                for (int i = 0; i < 256; ++i)
                    srgb_to_linear[i] = (float)pow(i * 1.0f / 255.0f, source_gamma);
            }

            const int linear_to_srgb_table_size = 8192;
            unsigned char linear_to_srgb[linear_to_srgb_table_size];

            const float inv_linear_to_srgb_table_size = 1.0f / linear_to_srgb_table_size;
            const float inv_source_gamma = 1.0f / source_gamma;

            if (params.m_srgb)
            {
                for (int i = 0; i < linear_to_srgb_table_size; ++i)
                {
                    int k = (int)(255.0f * pow(i * inv_linear_to_srgb_table_size, inv_source_gamma) + .5f);
                    if (k < 0)
                        k = 0;
                    else if (k > 255)
                        k = 255;
                    linear_to_srgb[i] = (unsigned char)k;
                }
            }

            Resampler *resamplers[cMaxComponents];
            vogl::vector<float> samples[cMaxComponents];

            resamplers[0] = vogl_new(Resampler, src_width, src_height, dst_width, dst_height,
                                       params.m_wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 0.0f, 1.0f,
                                       params.m_pFilter, (Resampler::Contrib_List *)NULL, (Resampler::Contrib_List *)NULL, params.m_filter_scale, params.m_filter_scale, params.m_x_ofs, params.m_y_ofs);
            samples[0].resize(src_width);

            for (uint32_t i = 1; i < params.m_num_comps; i++)
            {
                resamplers[i] = vogl_new(Resampler, src_width, src_height, dst_width, dst_height,
                                           params.m_wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 0.0f, 1.0f,
                                           params.m_pFilter, resamplers[0]->get_clist_x(), resamplers[0]->get_clist_y(), params.m_filter_scale, params.m_filter_scale, params.m_x_ofs, params.m_y_ofs);
                samples[i].resize(src_width);
            }

            uint32_t dst_y = 0;

            for (uint32_t src_y = 0; src_y < src_height; src_y++)
            {
                const color_quad_u8 *pSrc = src.get_scanline(src_y);

                for (uint32_t x = 0; x < src_width; x++)
                {
                    for (uint32_t c = 0; c < params.m_num_comps; c++)
                    {
                        const uint32_t comp_index = params.m_first_comp + c;
                        const uint8_t v = (*pSrc)[comp_index];

                        if (!params.m_srgb || (comp_index == 3))
                            samples[c][x] = v * (1.0f / 255.0f);
                        else
                            samples[c][x] = srgb_to_linear[v];
                    }

                    pSrc++;
                }

                for (uint32_t c = 0; c < params.m_num_comps; c++)
                {
                    if (!resamplers[c]->put_line(&samples[c][0]))
                    {
                        for (uint32_t i = 0; i < params.m_num_comps; i++)
                            vogl_delete(resamplers[i]);
                        return false;
                    }
                }

                for (;;)
                {
                    uint32_t c;
                    for (c = 0; c < params.m_num_comps; c++)
                    {
                        const uint32_t comp_index = params.m_first_comp + c;

                        const float *pOutput_samples = resamplers[c]->get_line();
                        if (!pOutput_samples)
                            break;

                        const bool linear = !params.m_srgb || (comp_index == 3);
                        VOGL_ASSERT(dst_y < dst_height);
                        color_quad_u8 *pDst = dst.get_scanline(dst_y);

                        for (uint32_t x = 0; x < dst_width; x++)
                        {
                            if (linear)
                            {
                                int c2 = (int)(255.0f * pOutput_samples[x] + .5f);
                                if (c2 < 0)
                                    c2 = 0;
                                else if (c2 > 255)
                                    c2 = 255;
                                (*pDst)[comp_index] = (unsigned char)c2;
                            }
                            else
                            {
                                int j = (int)(linear_to_srgb_table_size * pOutput_samples[x] + .5f);
                                if (j < 0)
                                    j = 0;
                                else if (j >= linear_to_srgb_table_size)
                                    j = linear_to_srgb_table_size - 1;
                                (*pDst)[comp_index] = linear_to_srgb[j];
                            }

                            pDst++;
                        }
                    }
                    if (c < params.m_num_comps)
                        break;

                    dst_y++;
                }
            }

            for (uint32_t i = 0; i < params.m_num_comps; i++)
                vogl_delete(resamplers[i]);

            return true;
        }

        bool resample(const image_f &src, image_f &dst, const resample_params &params)
        {
            const uint32_t src_width = src.get_width();
            const uint32_t src_height = src.get_height();

            if (math::maximum(src_width, src_height) > VOGL_RESAMPLER_MAX_DIMENSION)
            {
                printf("Image is too large!\n");
                return EXIT_FAILURE;
            }

            const int cMaxComponents = 4;
            if (((int)params.m_num_comps < 1) || ((int)params.m_num_comps > (int)cMaxComponents))
                return false;

            const uint32_t dst_width = params.m_dst_width;
            const uint32_t dst_height = params.m_dst_height;

            if ((math::minimum(dst_width, dst_height) < 1) || (math::maximum(dst_width, dst_height) > VOGL_RESAMPLER_MAX_DIMENSION))
            {
                printf("Image is too large!\n");
                return EXIT_FAILURE;
            }

            if ((src_width == dst_width) && (src_height == dst_height))
            {
                dst = src;
                return true;
            }

            dst.clear();
            dst.crop(params.m_dst_width, params.m_dst_height);

            Resampler *resamplers[cMaxComponents];
            vogl::vector<float> samples[cMaxComponents];

            resamplers[0] = vogl_new(Resampler, src_width, src_height, dst_width, dst_height,
                                       params.m_wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 0.0f, 1.0f,
                                       params.m_pFilter, (Resampler::Contrib_List *)NULL, (Resampler::Contrib_List *)NULL, params.m_filter_scale, params.m_filter_scale, params.m_x_ofs, params.m_y_ofs);
            samples[0].resize(src_width);

            for (uint32_t i = 1; i < params.m_num_comps; i++)
            {
                resamplers[i] = vogl_new(Resampler, src_width, src_height, dst_width, dst_height,
                                           params.m_wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP, 0.0f, 1.0f,
                                           params.m_pFilter, resamplers[0]->get_clist_x(), resamplers[0]->get_clist_y(), params.m_filter_scale, params.m_filter_scale, params.m_x_ofs, params.m_y_ofs);
                samples[i].resize(src_width);
            }

            uint32_t dst_y = 0;

            for (uint32_t src_y = 0; src_y < src_height; src_y++)
            {
                const color_quad_f *pSrc = src.get_scanline(src_y);

                for (uint32_t x = 0; x < src_width; x++)
                {
                    for (uint32_t c = 0; c < params.m_num_comps; c++)
                    {
                        const uint32_t comp_index = params.m_first_comp + c;
                        samples[c][x] = (*pSrc)[comp_index];
                    }

                    pSrc++;
                }

                for (uint32_t c = 0; c < params.m_num_comps; c++)
                {
                    if (!resamplers[c]->put_line(&samples[c][0]))
                    {
                        for (uint32_t i = 0; i < params.m_num_comps; i++)
                            vogl_delete(resamplers[i]);
                        return false;
                    }
                }

                for (;;)
                {
                    uint32_t c;
                    for (c = 0; c < params.m_num_comps; c++)
                    {
                        const uint32_t comp_index = params.m_first_comp + c;

                        const float *pOutput_samples = resamplers[c]->get_line();
                        if (!pOutput_samples)
                            break;

                        VOGL_ASSERT(dst_y < dst_height);
                        color_quad_f *pDst = dst.get_scanline(dst_y);

                        for (uint32_t x = 0; x < dst_width; x++)
                        {
                            (*pDst)[comp_index] = pOutput_samples[x];
                            pDst++;
                        }
                    }

                    if (c < params.m_num_comps)
                        break;

                    dst_y++;
                }
            }

            for (uint32_t i = 0; i < params.m_num_comps; i++)
                vogl_delete(resamplers[i]);

            return true;
        }

        bool resample_multithreaded(const image_u8 &src, image_u8 &dst, const resample_params &params)
        {
            const uint32_t src_width = src.get_width();
            const uint32_t src_height = src.get_height();

            if (math::maximum(src_width, src_height) > VOGL_RESAMPLER_MAX_DIMENSION)
            {
                printf("Image is too large!\n");
                return EXIT_FAILURE;
            }

            const int cMaxComponents = 4;
            if (((int)params.m_num_comps < 1) || ((int)params.m_num_comps > (int)cMaxComponents))
                return false;

            const uint32_t dst_width = params.m_dst_width;
            const uint32_t dst_height = params.m_dst_height;

            if ((math::minimum(dst_width, dst_height) < 1) || (math::maximum(dst_width, dst_height) > VOGL_RESAMPLER_MAX_DIMENSION))
            {
                printf("Image is too large!\n");
                return EXIT_FAILURE;
            }

            if ((src_width == dst_width) && (src_height == dst_height))
            {
                dst = src;
                return true;
            }

            dst.clear();

            // Partial gamma correction looks better on mips. Set to 1.0 to disable gamma correction.
            const float source_gamma = params.m_source_gamma; //1.75f;

            float srgb_to_linear[256];
            if (params.m_srgb)
            {
                for (int i = 0; i < 256; ++i)
                    srgb_to_linear[i] = (float)pow(i * 1.0f / 255.0f, source_gamma);
            }

            const int linear_to_srgb_table_size = 8192;
            unsigned char linear_to_srgb[linear_to_srgb_table_size];

            const float inv_linear_to_srgb_table_size = 1.0f / linear_to_srgb_table_size;
            const float inv_source_gamma = 1.0f / source_gamma;

            if (params.m_srgb)
            {
                for (int i = 0; i < linear_to_srgb_table_size; ++i)
                {
                    int k = (int)(255.0f * pow(i * inv_linear_to_srgb_table_size, inv_source_gamma) + .5f);
                    if (k < 0)
                        k = 0;
                    else if (k > 255)
                        k = 255;
                    linear_to_srgb[i] = (unsigned char)k;
                }
            }

            task_pool tp;
            tp.init(g_number_of_processors - 1);

            threaded_resampler resampler(tp);
            threaded_resampler::params p;
            p.m_src_width = src_width;
            p.m_src_height = src_height;
            p.m_dst_width = dst_width;
            p.m_dst_height = dst_height;
            p.m_sample_low = 0.0f;
            p.m_sample_high = 1.0f;
            p.m_boundary_op = params.m_wrapping ? Resampler::BOUNDARY_WRAP : Resampler::BOUNDARY_CLAMP;
            p.m_Pfilter_name = params.m_pFilter;
            p.m_filter_x_scale = params.m_filter_scale;
            p.m_filter_y_scale = params.m_filter_scale;
            p.m_x_ofs = params.m_x_ofs;
            p.m_y_ofs = params.m_y_ofs;

            uint32_t resampler_comps = 4;
            if (params.m_num_comps == 1)
            {
                p.m_fmt = threaded_resampler::cPF_Y_F32;
                resampler_comps = 1;
            }
            else if (params.m_num_comps <= 3)
                p.m_fmt = threaded_resampler::cPF_RGBX_F32;
            else
                p.m_fmt = threaded_resampler::cPF_RGBA_F32;

            vogl::vector<float> src_samples;
            vogl::vector<float> dst_samples;

            if (!src_samples.try_resize(src_width * src_height * resampler_comps))
                return false;

            if (!dst_samples.try_resize(dst_width * dst_height * resampler_comps))
                return false;

            p.m_pSrc_pixels = src_samples.get_ptr();
            p.m_src_pitch = src_width * resampler_comps * sizeof(float);
            p.m_pDst_pixels = dst_samples.get_ptr();
            p.m_dst_pitch = dst_width * resampler_comps * sizeof(float);

            for (uint32_t src_y = 0; src_y < src_height; src_y++)
            {
                const color_quad_u8 *pSrc = src.get_scanline(src_y);
                float *pDst = src_samples.get_ptr() + src_width * resampler_comps * src_y;

                for (uint32_t x = 0; x < src_width; x++)
                {
                    for (uint32_t c = 0; c < params.m_num_comps; c++)
                    {
                        const uint32_t comp_index = params.m_first_comp + c;
                        const uint8_t v = (*pSrc)[comp_index];

                        if (!params.m_srgb || (comp_index == 3))
                            pDst[c] = v * (1.0f / 255.0f);
                        else
                            pDst[c] = srgb_to_linear[v];
                    }

                    pSrc++;
                    pDst += resampler_comps;
                }
            }

            if (!resampler.resample(p))
                return false;

            src_samples.clear();

            if (!dst.crop(params.m_dst_width, params.m_dst_height))
                return false;

            for (uint32_t dst_y = 0; dst_y < dst_height; dst_y++)
            {
                const float *pSrc = dst_samples.get_ptr() + dst_width * resampler_comps * dst_y;
                color_quad_u8 *pDst = dst.get_scanline(dst_y);

                for (uint32_t x = 0; x < dst_width; x++)
                {
                    color_quad_u8 dst2(0, 0, 0, 255);

                    for (uint32_t c = 0; c < params.m_num_comps; c++)
                    {
                        const uint32_t comp_index = params.m_first_comp + c;
                        const float v = pSrc[c];

                        if ((!params.m_srgb) || (comp_index == 3))
                        {
                            int c2 = static_cast<int>(255.0f * v + .5f);
                            if (c2 < 0)
                                c2 = 0;
                            else if (c2 > 255)
                                c2 = 255;
                            dst2[comp_index] = (unsigned char)c2;
                        }
                        else
                        {
                            int j = static_cast<int>(linear_to_srgb_table_size * v + .5f);
                            if (j < 0)
                                j = 0;
                            else if (j >= linear_to_srgb_table_size)
                                j = linear_to_srgb_table_size - 1;
                            dst2[comp_index] = linear_to_srgb[j];
                        }
                    }

                    *pDst++ = dst2;

                    pSrc += resampler_comps;
                }
            }

            return true;
        }

        bool resample(const image_u8 &src, image_u8 &dst, const resample_params &params)
        {
            if ((params.m_multithreaded) && (g_number_of_processors > 1))
                return resample_multithreaded(src, dst, params);
            else
                return resample_single_thread(src, dst, params);
        }

        bool compute_delta(image_u8 &dest, const image_u8 &a, const image_u8 &b, uint32_t scale)
        {
            if ((a.get_width() != b.get_width()) || (a.get_height() != b.get_height()))
                return false;

            dest.crop(a.get_width(), b.get_height());

            for (uint32_t y = 0; y < a.get_height(); y++)
            {
                for (uint32_t x = 0; x < a.get_width(); x++)
                {
                    const color_quad_u8 &ca = a(x, y);
                    const color_quad_u8 &cb = b(x, y);

                    color_quad_u8 cd;
                    for (uint32_t c = 0; c < 4; c++)
                    {
                        int d = (ca[c] - cb[c]) * scale + 128;
                        d = math::clamp(d, 0, 255);
                        cd[c] = static_cast<uint8_t>(d);
                    }

                    dest(x, y) = cd;
                }
            }

            return true;
        }

        // See page 605 of http://www.cns.nyu.edu/pub/eero/wang03-reprint.pdf
        // Image Quality Assessment: From Error Visibility to Structural Similarity
        double compute_block_ssim(uint32_t t, const uint8_t *pX, const uint8_t *pY)
        {
            double ave_x = 0.0f;
            double ave_y = 0.0f;
            for (uint32_t i = 0; i < t; i++)
            {
                ave_x += pX[i];
                ave_y += pY[i];
            }

            ave_x /= t;
            ave_y /= t;

            double var_x = 0.0f;
            double var_y = 0.0f;
            for (uint32_t i = 0; i < t; i++)
            {
                var_x += math::square(pX[i] - ave_x);
                var_y += math::square(pY[i] - ave_y);
            }

            var_x = sqrt(var_x / (t - 1));
            var_y = sqrt(var_y / (t - 1));

            double covar_xy = 0.0f;
            for (uint32_t i = 0; i < t; i++)
                covar_xy += (pX[i] - ave_x) * (pY[i] - ave_y);

            covar_xy /= (t - 1);

            const double c1 = 6.5025;  //(255*.01)^2
            const double c2 = 58.5225; //(255*.03)^2

            double n = (2.0f * ave_x * ave_y + c1) * (2.0f * covar_xy + c2);
            double d = (ave_x * ave_x + ave_y * ave_y + c1) * (var_x * var_x + var_y * var_y + c2);

            return n / d;
        }

        double compute_ssim(const image_u8 &a, const image_u8 &b, int channel_index)
        {
            const uint32_t N = 8;
            uint8_t sx[N * N], sy[N * N];

            double total_ssim = 0.0f;
            uint32_t total_blocks = 0;

            for (uint32_t y = 0; y < a.get_height(); y += N)
            {
                for (uint32_t x = 0; x < a.get_width(); x += N)
                {
                    for (uint32_t iy = 0; iy < N; iy++)
                    {
                        for (uint32_t ix = 0; ix < N; ix++)
                        {
                            if (channel_index < 0)
                                sx[ix + iy * N] = (uint8_t)a.get_clamped(x + ix, y + iy).get_luma();
                            else
                                sx[ix + iy * N] = (uint8_t)a.get_clamped(x + ix, y + iy)[channel_index];

                            if (channel_index < 0)
                                sy[ix + iy * N] = (uint8_t)b.get_clamped(x + ix, y + iy).get_luma();
                            else
                                sy[ix + iy * N] = (uint8_t)b.get_clamped(x + ix, y + iy)[channel_index];
                        }
                    }

                    double ssim = compute_block_ssim(N * N, sx, sy);
                    total_ssim += ssim;
                    total_blocks++;

                    //uint32_t ssim_c = (uint32_t)math::clamp<double>(ssim * 127.0f + 128.0f, 0, 255);
                    //yimg(x / N, y / N).set(ssim_c, ssim_c, ssim_c, 255);
                }
            }

            if (!total_blocks)
                return 0.0f;

            //save_to_file_stb_or_miniz("ssim.tga", yimg, cWriteFlagGrayscale);

            return total_ssim / total_blocks;
        }

        void print_ssim(const image_u8 &src_img, const image_u8 &dst_img)
        {
            double y_ssim = compute_ssim(src_img, dst_img, -1);
            vogl_printf("Luma MSSIM: %f, Scaled [.8,1]: %f", y_ssim, (y_ssim - .8f) / .2f);

            double r_ssim = compute_ssim(src_img, dst_img, 0);
            vogl_printf("   R MSSIM: %f", r_ssim);

            double g_ssim = compute_ssim(src_img, dst_img, 1);
            vogl_printf("   G MSSIM: %f", g_ssim);

            double b_ssim = compute_ssim(src_img, dst_img, 2);
            vogl_printf("   B MSSIM: %f", b_ssim);
        }

        void error_metrics::print(const char *pName) const
        {
            if (mPeakSNR >= cInfinitePSNR)
                vogl_printf("%s Error: Max: %3u, Mean: %3.3f, MSE: %3.3f, RMSE: %3.3f, PSNR: Infinite", pName, mMax, mMean, mMeanSquared, mRootMeanSquared);
            else
                vogl_printf("%s Error: Max: %3u, Mean: %3.3f, MSE: %3.3f, RMSE: %3.3f, PSNR: %3.3f, SSIM: %1.6f", pName, mMax, mMean, mMeanSquared, mRootMeanSquared, mPeakSNR, mSSIM);
        }

        bool error_metrics::compute(const image_u8 &a, const image_u8 &b, uint32_t first_channel, uint32_t num_channels, bool average_component_error)
        {
            //if ( (!a.get_width()) || (!b.get_height()) || (a.get_width() != b.get_width()) || (a.get_height() != b.get_height()) )
            //   return false;

            const uint32_t width = math::minimum(a.get_width(), b.get_width());
            const uint32_t height = math::minimum(a.get_height(), b.get_height());

            VOGL_ASSERT((first_channel < 4U) && (first_channel + num_channels <= 4U));

            // Histogram approach due to Charles Bloom.
            double hist[256];
            utils::zero_object(hist);

            for (uint32_t y = 0; y < height; y++)
            {
                for (uint32_t x = 0; x < width; x++)
                {
                    const color_quad_u8 &ca = a(x, y);
                    const color_quad_u8 &cb = b(x, y);

                    if (!num_channels)
                        hist[labs(ca.get_luma() - cb.get_luma())]++;
                    else
                    {
                        for (uint32_t c = 0; c < num_channels; c++)
                            hist[labs(ca[first_channel + c] - cb[first_channel + c])]++;
                    }
                }
            }

            mMax = 0;
            double sum = 0.0f, sum2 = 0.0f;
            for (uint32_t i = 0; i < 256; i++)
            {
                if (!hist[i])
                    continue;

                mMax = math::maximum(mMax, i);

                double x = i * hist[i];

                sum += x;
                sum2 += i * x;
            }

            // See http://bmrc.berkeley.edu/courseware/cs294/fall97/assignment/psnr.html
            double total_values = width * height;

            if (average_component_error)
                total_values *= math::clamp<uint32_t>(num_channels, 1, 4);

            mMean = math::clamp<double>(sum / total_values, 0.0f, 255.0f);
            mMeanSquared = math::clamp<double>(sum2 / total_values, 0.0f, 255.0f * 255.0f);

            mRootMeanSquared = sqrt(mMeanSquared);

            if (!mRootMeanSquared)
                mPeakSNR = cInfinitePSNR;
            else
                mPeakSNR = math::clamp<double>(log10(255.0f / mRootMeanSquared) * 20.0f, 0.0f, 500.0f);

            if (!a.has_rgb() || !b.has_rgb())
                mSSIM = 0;
            else
                mSSIM = compute_ssim(a, b, (num_channels != 1) ? -1 : first_channel);

            return true;
        }

        void print_image_metrics(const image_u8 &src_img, const image_u8 &dst_img)
        {
            if ((!src_img.get_width()) || (!dst_img.get_height()) || (src_img.get_width() != dst_img.get_width()) || (src_img.get_height() != dst_img.get_height()))
                vogl_printf("print_image_metrics: Image resolutions don't match exactly (%ux%u) vs. (%ux%u)", src_img.get_width(), src_img.get_height(), dst_img.get_width(), dst_img.get_height());

            image_utils::error_metrics error_metrics;

            if (src_img.has_rgb() || dst_img.has_rgb())
            {
                error_metrics.compute(src_img, dst_img, 0, 3, false);
                error_metrics.print("RGB Total  ");

                error_metrics.compute(src_img, dst_img, 0, 3, true);
                error_metrics.print("RGB Average");

                error_metrics.compute(src_img, dst_img, 0, 0);
                error_metrics.print("Luma       ");

                error_metrics.compute(src_img, dst_img, 0, 1);
                error_metrics.print("Red        ");

                error_metrics.compute(src_img, dst_img, 1, 1);
                error_metrics.print("Green      ");

                error_metrics.compute(src_img, dst_img, 2, 1);
                error_metrics.print("Blue       ");
            }

            if (src_img.has_alpha() || dst_img.has_alpha())
            {
                error_metrics.compute(src_img, dst_img, 3, 1);
                error_metrics.print("Alpha      ");
            }
        }

        static uint8_t regen_z(uint32_t x, uint32_t y)
        {
            float vx = math::clamp((x - 128.0f) * 1.0f / 127.0f, -1.0f, 1.0f);
            float vy = math::clamp((y - 128.0f) * 1.0f / 127.0f, -1.0f, 1.0f);
            float vz = sqrt(math::clamp(1.0f - vx * vx - vy * vy, 0.0f, 1.0f));

            vz = vz * 127.0f + 128.0f;

            if (vz < 128.0f)
                vz -= .5f;
            else
                vz += .5f;

            int ib = math::float_to_int(vz);

            return static_cast<uint8_t>(math::clamp(ib, 0, 255));
        }

        void convert_image(image_u8 &img, image_utils::conversion_type conv_type)
        {
            switch (conv_type)
            {
                case image_utils::cConversion_To_CCxY:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagAValid | pixel_format_helpers::cCompFlagLumaChroma));
                    break;
                }
                case image_utils::cConversion_From_CCxY:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid));
                    break;
                }
                case image_utils::cConversion_To_xGxR:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagAValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case image_utils::cConversion_From_xGxR:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case image_utils::cConversion_To_xGBR:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagAValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case image_utils::cConversion_To_AGBR:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagAValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case image_utils::cConversion_From_xGBR:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case image_utils::cConversion_From_AGBR:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagAValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case image_utils::cConversion_XY_to_XYZ:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagNormalMap));
                    break;
                }
                case cConversion_Y_To_A:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(img.get_comp_flags() | pixel_format_helpers::cCompFlagAValid));
                    break;
                }
                case cConversion_A_To_RGBA:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagAValid));
                    break;
                }
                case cConversion_Y_To_RGB:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(pixel_format_helpers::cCompFlagRValid | pixel_format_helpers::cCompFlagGValid | pixel_format_helpers::cCompFlagBValid | pixel_format_helpers::cCompFlagGrayscale | (img.has_alpha() ? pixel_format_helpers::cCompFlagAValid : 0)));
                    break;
                }
                case cConversion_To_Y:
                {
                    img.set_comp_flags(static_cast<pixel_format_helpers::component_flags>(img.get_comp_flags() | pixel_format_helpers::cCompFlagGrayscale));
                    break;
                }
                default:
                {
                    VOGL_ASSERT(false);
                    return;
                }
            }

            for (uint32_t y = 0; y < img.get_height(); y++)
            {
                for (uint32_t x = 0; x < img.get_width(); x++)
                {
                    color_quad_u8 src(img(x, y));
                    color_quad_u8 dst;

                    switch (conv_type)
                    {
                        case image_utils::cConversion_To_CCxY:
                        {
                            color::RGB_to_YCC(dst, src);
                            break;
                        }
                        case image_utils::cConversion_From_CCxY:
                        {
                            color::YCC_to_RGB(dst, src);
                            break;
                        }
                        case image_utils::cConversion_To_xGxR:
                        {
                            dst.r = 0;
                            dst.g = src.g;
                            dst.b = 0;
                            dst.a = src.r;
                            break;
                        }
                        case image_utils::cConversion_From_xGxR:
                        {
                            dst.r = src.a;
                            dst.g = src.g;
                            // This is kinda iffy, we're assuming the image is a normal map here.
                            dst.b = regen_z(src.a, src.g);
                            dst.a = 255;
                            break;
                        }
                        case image_utils::cConversion_To_xGBR:
                        {
                            dst.r = 0;
                            dst.g = src.g;
                            dst.b = src.b;
                            dst.a = src.r;
                            break;
                        }
                        case image_utils::cConversion_To_AGBR:
                        {
                            dst.r = src.a;
                            dst.g = src.g;
                            dst.b = src.b;
                            dst.a = src.r;
                            break;
                        }
                        case image_utils::cConversion_From_xGBR:
                        {
                            dst.r = src.a;
                            dst.g = src.g;
                            dst.b = src.b;
                            dst.a = 255;
                            break;
                        }
                        case image_utils::cConversion_From_AGBR:
                        {
                            dst.r = src.a;
                            dst.g = src.g;
                            dst.b = src.b;
                            dst.a = src.r;
                            break;
                        }
                        case image_utils::cConversion_XY_to_XYZ:
                        {
                            dst.r = src.r;
                            dst.g = src.g;
                            // This is kinda iffy, we're assuming the image is a normal map here.
                            dst.b = regen_z(src.r, src.g);
                            dst.a = 255;
                            break;
                        }
                        case image_utils::cConversion_Y_To_A:
                        {
                            dst.r = src.r;
                            dst.g = src.g;
                            dst.b = src.b;
                            dst.a = static_cast<uint8_t>(src.get_luma());
                            break;
                        }
                        case image_utils::cConversion_Y_To_RGB:
                        {
                            uint8_t y2 = static_cast<uint8_t>(src.get_luma());
                            dst.r = y2;
                            dst.g = y2;
                            dst.b = y2;
                            dst.a = src.a;
                            break;
                        }
                        case image_utils::cConversion_A_To_RGBA:
                        {
                            dst.r = src.a;
                            dst.g = src.a;
                            dst.b = src.a;
                            dst.a = src.a;
                            break;
                        }
                        case image_utils::cConversion_To_Y:
                        {
                            uint8_t y2 = static_cast<uint8_t>(src.get_luma());
                            dst.r = y2;
                            dst.g = y2;
                            dst.b = y2;
                            dst.a = src.a;
                            break;
                        }
                        default:
                        {
                            VOGL_ASSERT(false);
                            dst = src;
                            break;
                        }
                    }

                    img(x, y) = dst;
                }
            }
        }

        image_utils::conversion_type get_conversion_type(bool cooking, pixel_format fmt)
        {
            image_utils::conversion_type conv_type = image_utils::cConversion_Invalid;

            if (cooking)
            {
                switch (fmt)
                {
                    case PIXEL_FMT_DXT5_CCxY:
                    {
                        conv_type = image_utils::cConversion_To_CCxY;
                        break;
                    }
                    case PIXEL_FMT_DXT5_xGxR:
                    {
                        conv_type = image_utils::cConversion_To_xGxR;
                        break;
                    }
                    case PIXEL_FMT_DXT5_xGBR:
                    {
                        conv_type = image_utils::cConversion_To_xGBR;
                        break;
                    }
                    case PIXEL_FMT_DXT5_AGBR:
                    {
                        conv_type = image_utils::cConversion_To_AGBR;
                        break;
                    }
                    default:
                        break;
                }
            }
            else
            {
                switch (fmt)
                {
                    case PIXEL_FMT_3DC:
                    case PIXEL_FMT_DXN:
                    {
                        conv_type = image_utils::cConversion_XY_to_XYZ;
                        break;
                    }
                    case PIXEL_FMT_DXT5_CCxY:
                    {
                        conv_type = image_utils::cConversion_From_CCxY;
                        break;
                    }
                    case PIXEL_FMT_DXT5_xGxR:
                    {
                        conv_type = image_utils::cConversion_From_xGxR;
                        break;
                    }
                    case PIXEL_FMT_DXT5_xGBR:
                    {
                        conv_type = image_utils::cConversion_From_xGBR;
                        break;
                    }
                    case PIXEL_FMT_DXT5_AGBR:
                    {
                        conv_type = image_utils::cConversion_From_AGBR;
                        break;
                    }
                    default:
                        break;
                }
            }

            return conv_type;
        }

        image_utils::conversion_type get_image_conversion_type_from_vogl_format(vogl_format fmt)
        {
            switch (fmt)
            {
                case cCRNFmtDXT5_CCxY:
                    return image_utils::cConversion_To_CCxY;
                case cCRNFmtDXT5_xGxR:
                    return image_utils::cConversion_To_xGxR;
                case cCRNFmtDXT5_xGBR:
                    return image_utils::cConversion_To_xGBR;
                case cCRNFmtDXT5_AGBR:
                    return image_utils::cConversion_To_AGBR;
                default:
                    break;
            }
            return image_utils::cConversion_Invalid;
        }

        double compute_std_dev(uint32_t n, const color_quad_u8 *pPixels, uint32_t first_channel, uint32_t num_channels)
        {
            if (!n)
                return 0.0f;

            double sum = 0.0f;
            double sum2 = 0.0f;

            for (uint32_t i = 0; i < n; i++)
            {
                const color_quad_u8 &cp = pPixels[i];

                if (!num_channels)
                {
                    uint32_t l = cp.get_luma();
                    sum += l;
                    sum2 += l * l;
                }
                else
                {
                    for (uint32_t c = 0; c < num_channels; c++)
                    {
                        uint32_t l = cp[first_channel + c];
                        sum += l;
                        sum2 += l * l;
                    }
                }
            }

            double w = math::maximum(1U, num_channels) * n;
            sum /= w;
            sum2 /= w;

            double var = sum2 - sum * sum;
            var = math::maximum<double>(var, 0.0f);

            return sqrt(var);
        }

        // http://lodev.org/cgtutor/fourier.html
        // TODO: Rewrite this - the license is too restrictive
        // N=width, M=height
        // image in memory is: [x][y][c], C bytes per pixel
        template <uint32_t C>
        void FFT2D(int n, int m, bool inverse, const float *gRe, const float *gIm, float *GRe, float *GIm)
        {
            int l2n = 0, p = 1; //l2n will become log_2(n)
            while (p < n)
            {
                p *= 2;
                l2n++;
            }
            int l2m = 0;
            p = 1; //l2m will become log_2(m)
            while (p < m)
            {
                p *= 2;
                l2m++;
            }

            m = 1 << l2m;
            n = 1 << l2n; //Make sure m and n will be powers of 2, otherwise you'll get in an infinite loop

            //Erase all history of this array
            for (int x = 0; x < m; x++) //for each column
            {
                for (int y = 0; y < m; y++) //for each row
                {
                    for (int c = 0; c < static_cast<int>(C); c++) //for each color component
                    {
                        GRe[C * m * x + C * y + c] = gRe[C * m * x + C * y + c];
                        GIm[C * m * x + C * y + c] = gIm[C * m * x + C * y + c];
                    }
                }
            }

            //Bit reversal of each row
            int j;
            for (int y = 0; y < m; y++) //for each row
            {
                for (int c = 0; c < static_cast<int>(C); c++) //for each color component
                {
                    j = 0;
                    for (int i = 0; i < n - 1; i++)
                    {
                        GRe[C * m * i + C * y + c] = gRe[C * m * j + C * y + c];
                        GIm[C * m * i + C * y + c] = gIm[C * m * j + C * y + c];
                        int k = n / 2;
                        while (k <= j)
                        {
                            j -= k;
                            k /= 2;
                        }
                        j += k;
                    }
                }
            }

            //Bit reversal of each column
            float tx = 0, ty = 0;
            for (int x = 0; x < n; x++) //for each column
            {
                for (int c = 0; c < static_cast<int>(C); c++) //for each color component
                {
                    j = 0;
                    for (int i = 0; i < m - 1; i++)
                    {
                        if (i < j)
                        {
                            tx = GRe[C * m * x + C * i + c];
                            ty = GIm[C * m * x + C * i + c];
                            GRe[C * m * x + C * i + c] = GRe[C * m * x + C * j + c];
                            GIm[C * m * x + C * i + c] = GIm[C * m * x + C * j + c];
                            GRe[C * m * x + C * j + c] = tx;
                            GIm[C * m * x + C * j + c] = ty;
                        }
                        int k = m / 2;
                        while (k <= j)
                        {
                            j -= k;
                            k /= 2;
                        }
                        j += k;
                    }
                }
            }

            //Calculate the FFT of the columns
            for (int x = 0; x < n; x++) //for each column
            {
                for (int c = 0; c < static_cast<int>(C); c++) //for each color component
                {
                    //This is the 1D FFT:
                    float ca = -1.0;
                    float sa = 0.0;
                    int l1 = 1, l2 = 1;
                    for (int l = 0; l < l2n; l++)
                    {
                        l1 = l2;
                        l2 *= 2;
                        float u1 = 1.0;
                        float u2 = 0.0;
                        for (int j2 = 0; j2 < l1; j2++)
                        {
                            for (int i = j2; i < n; i += l2)
                            {
                                int i1 = i + l1;
                                float t1 = u1 * GRe[C * m * x + C * i1 + c] - u2 * GIm[C * m * x + C * i1 + c];
                                float t2 = u1 * GIm[C * m * x + C * i1 + c] + u2 * GRe[C * m * x + C * i1 + c];
                                GRe[C * m * x + C * i1 + c] = GRe[C * m * x + C * i + c] - t1;
                                GIm[C * m * x + C * i1 + c] = GIm[C * m * x + C * i + c] - t2;
                                GRe[C * m * x + C * i + c] += t1;
                                GIm[C * m * x + C * i + c] += t2;
                            }
                            float z = u1 * ca - u2 * sa;
                            u2 = u1 * sa + u2 * ca;
                            u1 = z;
                        }
                        sa = sqrtf((1.0f - ca) / 2.0f);
                        if (!inverse)
                            sa = -sa;
                        ca = sqrtf((1.0f + ca) / 2.0f);
                    }
                }
            }

            //Calculate the FFT of the rows
            for (int y = 0; y < m; y++) //for each row
            {
                for (int c = 0; c < static_cast<int>(C); c++) //for each color component
                {
                    //This is the 1D FFT:
                    float ca = -1.0;
                    float sa = 0.0;
                    int l1 = 1, l2 = 1;
                    for (int l = 0; l < l2m; l++)
                    {
                        l1 = l2;
                        l2 *= 2;
                        float u1 = 1.0;
                        float u2 = 0.0;
                        for (int j2 = 0; j2 < l1; j2++)
                        {
                            for (int i = j2; i < n; i += l2)
                            {
                                int i1 = i + l1;
                                float t1 = u1 * GRe[C * m * i1 + C * y + c] - u2 * GIm[C * m * i1 + C * y + c];
                                float t2 = u1 * GIm[C * m * i1 + C * y + c] + u2 * GRe[C * m * i1 + C * y + c];
                                GRe[C * m * i1 + C * y + c] = GRe[C * m * i + C * y + c] - t1;
                                GIm[C * m * i1 + C * y + c] = GIm[C * m * i + C * y + c] - t2;
                                GRe[C * m * i + C * y + c] += t1;
                                GIm[C * m * i + C * y + c] += t2;
                            }
                            float z = u1 * ca - u2 * sa;
                            u2 = u1 * sa + u2 * ca;
                            u1 = z;
                        }
                        sa = sqrtf((1.0f - ca) / 2.0f);
                        if (!inverse)
                            sa = -sa;
                        ca = sqrtf((1.0f + ca) / 2.0f);
                    }
                }
            }

            int d;
            if (inverse)
                d = n;
            else
                d = m;
            float inv_d = 1.0f / d;

            for (int x = 0; x < n; x++)
            {
                for (int y = 0; y < m; y++)
                {
                    for (int c = 0; c < static_cast<int>(C); c++) //for every value of the buffers
                    {
                        GRe[C * m * x + C * y + c] *= inv_d;
                        GIm[C * m * x + C * y + c] *= inv_d;
                    }
                }
            }
        }

        // N=width, M=height
        // image in memory is: [x][y][c], C bytes per pixel
        //void FFT2D(int n, int m, bool inverse, const float *gRe, const float *gIm, float *GRe, float *GIm)
        bool forward_fourier_transform(const image_u8 &src, image_u8 &dst, uint32_t comp_mask)
        {
            const uint32_t width = src.get_width();
            const uint32_t height = src.get_height();
            const uint32_t half_width = src.get_width() / 2;
            const uint32_t half_height = src.get_height() / 2;

            if (!math::is_power_of_2(width) || !math::is_power_of_2(height))
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            dst.resize(width * 2, height);

            vogl::vector2D<float> re0(height, width), im0(height, width);
            vogl::vector2D<float> re1(height, width), im1(height, width);

            for (uint32_t c = 0; c < 4; c++)
            {
                if ((comp_mask & (1 << c)) == 0)
                    continue;

                for (uint32_t y = 0; y < height; y++)
                    for (uint32_t x = 0; x < width; x++)
                        re0(y, x) = src(x, y)[c];

                FFT2D<1>(width, height, false, re0.get_ptr(), im0.get_ptr(), re1.get_ptr(), im1.get_ptr());

                for (uint32_t y = 0; y < height; y++)
                {
                    for (uint32_t x = 0; x < width; x++)
                    {
                        dst(x, y)[c] = static_cast<uint8_t>(math::clamp<float>(128.5f + re1.at_wrapped(y + half_height, x + half_width), 0.0f, 255.0f));
                        dst(x + width, y)[c] = static_cast<uint8_t>(math::clamp<float>(128.5f + im1.at_wrapped(y + half_height, x + half_width), 0.0f, 255.0f));
                    }
                }
            }

            return true;
        }

        void gaussian_filter(image_u8 &dst, const image_u8 &orig_img, uint32_t width_divisor, uint32_t height_divisor, uint32_t odd_filter_width, float sigma_sqr, bool wrapping)
        {
            VOGL_ASSERT(odd_filter_width && (odd_filter_width & 1));
            odd_filter_width |= 1;

            vector2D<float> kernel(odd_filter_width, odd_filter_width);
            math::compute_gaussian_kernel(kernel.get_ptr(), odd_filter_width, odd_filter_width, sigma_sqr, math::cComputeGaussianFlagNormalize);

            const int dst_width = orig_img.get_width() / width_divisor;
            const int dst_height = orig_img.get_height() / height_divisor;

            const int H = odd_filter_width / 2;
            const int L = -H;

            dst.crop(dst_width, dst_height);

            for (int oy = 0; oy < dst_height; oy++)
            {
                for (int ox = 0; ox < dst_width; ox++)
                {
                    vec4F c(0.0f);

                    for (int yd = L; yd <= H; yd++)
                    {
                        int y = oy * height_divisor + (height_divisor >> 1) + yd;

                        for (int xd = L; xd <= H; xd++)
                        {
                            int x = ox * width_divisor + (width_divisor >> 1) + xd;

                            const color_quad_u8 &p = orig_img.get_clamped_or_wrapped(x, y, wrapping, wrapping);

                            float w = kernel(xd + H, yd + H);
                            c[0] += p[0] * w;
                            c[1] += p[1] * w;
                            c[2] += p[2] * w;
                            c[3] += p[3] * w;
                        }
                    }

                    dst(ox, oy).set(math::float_to_int_nearest(c[0]), math::float_to_int_nearest(c[1]), math::float_to_int_nearest(c[2]), math::float_to_int_nearest(c[3]));
                }
            }
        }

        void convolution_filter(image_u8 &dst, const image_u8 &src, const float *pWeights, uint32_t M, uint32_t N, bool wrapping)
        {
            if (((M & 1) == 0) || ((N & 1) == 0))
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            const int width = src.get_width();
            const int height = src.get_height();

            dst.crop(width, height);

            const int HM = M / 2;
            const int HN = N / 2;

            VOGL_ASSERT((HM * 2 + 1) == static_cast<int>(M));
            VOGL_ASSERT((HN * 2 + 1) == static_cast<int>(N));

            for (int dst_y = 0; dst_y < height; dst_y++)
            {
                for (int dst_x = 0; dst_x < width; dst_x++)
                {
                    vec4F c(0.0f);

                    for (int yd = -HM; yd <= HM; yd++)
                    {
                        int src_y = wrapping ? math::posmod(dst_y + yd, height) : math::clamp<int>(dst_y + yd, 0, height - 1);

                        for (int xd = -HN; xd <= HN; xd++)
                        {
                            int src_x = wrapping ? math::posmod(dst_x + xd, width) : math::clamp<int>(dst_x + xd, 0, width - 1);
                            float w = pWeights[(yd + HM) * N + (xd + HN)];

                            const color_quad_u8 &src_c = src(src_x, src_y);
                            c[0] += src_c[0] * w;
                            c[1] += src_c[1] * w;
                            c[2] += src_c[2] * w;
                            c[3] += src_c[3] * w;
                        }
                    }

                    dst(dst_x, dst_y).set(static_cast<int>(c[0] + .5f), static_cast<int>(c[1] + .5f), static_cast<int>(c[2] + .5f), static_cast<int>(c[3] + .5f));
                }
            }
        }

        void convolution_filter(image_f &dst, const image_f &src, const float *pWeights, uint32_t M, uint32_t N, bool wrapping)
        {
            if (((M & 1) == 0) || ((N & 1) == 0))
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            const int width = src.get_width();
            const int height = src.get_height();

            dst.crop(width, height);

            const int HM = M / 2;
            const int HN = N / 2;

            VOGL_ASSERT((HM * 2 + 1) == static_cast<int>(M));
            VOGL_ASSERT((HN * 2 + 1) == static_cast<int>(N));

            for (int dst_y = 0; dst_y < height; dst_y++)
            {
                for (int dst_x = 0; dst_x < width; dst_x++)
                {
                    vec4F c(0.0f);

                    for (int yd = -HM; yd <= HM; yd++)
                    {
                        int src_y = wrapping ? math::posmod(dst_y + yd, height) : math::clamp<int>(dst_y + yd, 0, height - 1);

                        for (int xd = -HN; xd <= HN; xd++)
                        {
                            int src_x = wrapping ? math::posmod(dst_x + xd, width) : math::clamp<int>(dst_x + xd, 0, width - 1);
                            float w = pWeights[(yd + HM) * N + (xd + HN)];

                            const color_quad_u8 &src_c = src(src_x, src_y);
                            c[0] += src_c[0] * w;
                            c[1] += src_c[1] * w;
                            c[2] += src_c[2] * w;
                            c[3] += src_c[3] * w;
                        }
                    }

                    dst(dst_x, dst_y).set(c[0], c[1], c[2], c[3]);
                }
            }
        }

    } // namespace image_utils

} // namespace vogl
