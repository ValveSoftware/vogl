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

// File: vogl_image_utils.h
#pragma once

#include "vogl_core.h"
#include "vogl_image.h"
#include "vogl_data_stream_serializer.h"
#include "vogl_matrix.h"

namespace vogl
{
    enum pixel_format;

    namespace image_utils
    {
        enum read_flags_t
        {
            cReadFlagForceSTB = 1,
            cReadFlagsAllFlags = 1
        };

        bool read_from_stream_stb(data_stream_serializer &serializer, image_u8 &img);
        bool read_from_stream_jpgd(data_stream_serializer &serializer, image_u8 &img);
        bool read_from_stream(image_u8 &dest, data_stream_serializer &serializer, uint32_t read_flags = 0);
        bool read_from_file(image_u8 &dest, const char *pFilename, uint32_t read_flags = 0);

        // Reads texture from memory, results returned stb_image.c style.
        // *pActual_comps is set to 1, 3, or 4. req_comps must range from 1-4.
        uint8_t *read_from_memory(const uint8_t *pImage, int nSize, int *pWidth, int *pHeight, int *pActualComps, int req_comps, const char *pFilename);

        enum
        {
            cWriteFlagIgnoreAlpha = 0x00000001,
            cWriteFlagGrayscale = 0x00000002,
            cWriteFlagJPEGH1V1 = 0x00010000,
            cWriteFlagJPEGH2V1 = 0x00020000,
            cWriteFlagJPEGH2V2 = 0x00040000,
            cWriteFlagJPEGTwoPass = 0x00080000,
            cWriteFlagJPEGNoChromaDiscrim = 0x00100000,
            cWriteFlagJPEGQualityLevelMask = 0xFF000000,
            cWriteFlagJPEGQualityLevelShift = 24,
        };

        enum
        {
            cJPEGMinQualityLevel = 1,
            cJPEGMaxQualityLevel = 100
        };

        inline uint32_t create_jpeg_write_flags(uint32_t base_flags, uint32_t quality_level)
        {
            VOGL_ASSERT(quality_level <= 100);
            return base_flags | ((quality_level << cWriteFlagJPEGQualityLevelShift) & cWriteFlagJPEGQualityLevelMask);
        }

        const int cLumaComponentIndex = -1;

        bool write_to_file(const char *pFilename, const image_u8 &img, uint32_t write_flags = 0, int grayscale_comp_index = cLumaComponentIndex);
        bool write_to_file(const char *pFilename, const image_f &img, const vec4F &l, const vec4F &h, uint32_t write_flags = 0, int grayscale_comp_index = cLumaComponentIndex);

        bool has_alpha(const image_u8 &img);
        bool is_normal_map(const image_u8 &img, const char *pFilename = NULL);
        void renorm_normal_map(image_u8 &img);

        struct resample_params
        {
            resample_params()
                : m_dst_width(0),
                  m_dst_height(0),
                  m_pFilter("lanczos4"),
                  m_filter_scale(1.0f),
                  m_srgb(true),
                  m_wrapping(false),
                  m_first_comp(0),
                  m_num_comps(4),
                  m_source_gamma(2.2f), // 1.75f
                  m_multithreaded(true),
                  m_x_ofs(0.0f),
                  m_y_ofs(0.0f)
            {
            }

            uint32_t m_dst_width;
            uint32_t m_dst_height;
            const char *m_pFilter;
            float m_filter_scale;
            bool m_srgb;
            bool m_wrapping;
            uint32_t m_first_comp;
            uint32_t m_num_comps;
            float m_source_gamma;
            bool m_multithreaded;
            float m_x_ofs;
            float m_y_ofs;
        };

        bool resample_single_thread(const image_u8 &src, image_u8 &dst, const resample_params &params);
        bool resample_multithreaded(const image_u8 &src, image_u8 &dst, const resample_params &params);
        bool resample(const image_u8 &src, image_u8 &dst, const resample_params &params);
        bool resample(const image_f &src, image_f &dst, const resample_params &params);

        bool compute_delta(image_u8 &dest, const image_u8 &a, const image_u8 &b, uint32_t scale = 2);

        class error_metrics
        {
        public:
            error_metrics()
            {
                utils::zero_this(this);
            }

            void print(const char *pName) const;

            // If num_channels==0, luma error is computed.
            // If pHist != NULL, it must point to a 256 entry array.
            bool compute(const image_u8 &a, const image_u8 &b, uint32_t first_channel, uint32_t num_channels, bool average_component_error = true);

            uint32_t mMax;
            double mMean;
            double mMeanSquared;
            double mRootMeanSquared;
            double mPeakSNR;

            double mSSIM;

            inline bool operator==(const error_metrics &other) const
            {
                return mPeakSNR == other.mPeakSNR;
            }

            inline bool operator<(const error_metrics &other) const
            {
                return mPeakSNR < other.mPeakSNR;
            }

            inline bool operator>(const error_metrics &other) const
            {
                return mPeakSNR > other.mPeakSNR;
            }
        };

        void print_image_metrics(const image_u8 &src_img, const image_u8 &dst_img);

        double compute_block_ssim(uint32_t n, const uint8_t *pX, const uint8_t *pY);
        double compute_ssim(const image_u8 &a, const image_u8 &b, int channel_index);
        void print_ssim(const image_u8 &src_img, const image_u8 &dst_img);

        enum conversion_type
        {
            cConversion_Invalid = -1,
            cConversion_To_CCxY,
            cConversion_From_CCxY,
            cConversion_To_xGxR,
            cConversion_From_xGxR,
            cConversion_To_xGBR,
            cConversion_From_xGBR,
            cConversion_To_AGBR,
            cConversion_From_AGBR,
            cConversion_XY_to_XYZ,
            cConversion_Y_To_A,
            cConversion_A_To_RGBA,
            cConversion_Y_To_RGB,
            cConversion_To_Y,
            cConversionTotal
        };

        void convert_image(image_u8 &img, conversion_type conv_type);

        template <typename image_type>
        inline uint8_t *pack_image(const image_type &img, const pixel_packer &packer, uint32_t &n)
        {
            n = 0;

            if (!packer.is_valid())
                return NULL;

            const uint32_t width = img.get_width(), height = img.get_height();
            uint32_t dst_pixel_stride = packer.get_pixel_stride();
            uint32_t dst_pitch = width * dst_pixel_stride;

            n = dst_pitch * height;

            uint8_t *pImage = static_cast<uint8_t *>(vogl_malloc(n));

            uint8_t *pDst = pImage;
            for (uint32_t y = 0; y < height; y++)
            {
                const typename image_type::color_t *pSrc = img.get_scanline(y);
                for (uint32_t x = 0; x < width; x++)
                    pDst = (uint8_t *)packer.pack(*pSrc++, pDst);
            }

            return pImage;
        }

        image_utils::conversion_type get_conversion_type(bool cooking, pixel_format fmt);

        image_utils::conversion_type get_image_conversion_type_from_vogl_format(vogl_format fmt);

        double compute_std_dev(uint32_t n, const color_quad_u8 *pPixels, uint32_t first_channel, uint32_t num_channels);

        uint8_t *read_image_from_memory(const uint8_t *pImage, int nSize, int *pWidth, int *pHeight, int *pActualComps, int req_comps, const char *pFilename);

        template <uint32_t M>
        void create_normalized_convolution_matrix(matrix<M, M, float> &weights, const float *pSrc_weights)
        {
            if ((M & 1) == 0)
            {
                VOGL_ASSERT_ALWAYS;
                return;
            }

            const int HM = M / 2;
            VOGL_ASSERT(HM * 2 + 1 == M);

            float tw = 0.0f;
            for (int yd = -HM; yd <= HM; yd++)
            {
                float yw = pSrc_weights[yd + HM];
                for (int xd = -HM; xd <= HM; xd++)
                {
                    float xw = pSrc_weights[xd + HM];
                    float w = xw * yw;
                    weights[yd + HM][xd + HM] = w;
                    tw += w;
                }
            }

            VOGL_ASSERT(tw > 0.0f);
            weights /= tw;
        }

        inline void convolution_filter(image_u8 &dst, const image_u8 &src, const float *pWeights, uint32_t M, uint32_t N, bool wrapping);

        // M = num rows (Y), N = num cols (X)
        template <uint32_t M, uint32_t N>
        void convolution_filter(image_u8 &dst, const image_u8 &src, const matrix<M, N, float> &weights, bool wrapping)
        {
            convolution_filter(dst, src, &weights[0][0], M, N, wrapping);
        }

        void convolution_filter(image_f &dst, const image_f &src, const float *pWeights, uint32_t M, uint32_t N, bool wrapping);

        // M = num rows (Y), N = num cols (X)
        template <uint32_t M, uint32_t N>
        inline void convolution_filter(image_f &dst, const image_f &src, const matrix<M, N, float> &weights, bool wrapping)
        {
            convolution_filter(dst, src, &weights[0][0], M, N, wrapping);
        }

        bool forward_fourier_transform(const image_u8 &src, image_u8 &dst, uint32_t comp_mask = 0xF);

        void gaussian_filter(image_u8 &dst, const image_u8 &orig_img, uint32_t width_divisor, uint32_t height_divisor, uint32_t odd_filter_width, float sigma_sqr, bool wrapping);

    } // namespace image_utils

} // namespace vogl
