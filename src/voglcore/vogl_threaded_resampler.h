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

// File: vogl_threaded_resampler.h
#pragma once

#include "vogl_core.h"
#include "vogl_resampler.h"
#include "vogl_vec.h"

namespace vogl
{
    class task_pool;
    class threaded_resampler
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(threaded_resampler);

    public:
        threaded_resampler(task_pool &tp);
        ~threaded_resampler();

        enum pixel_format
        {
            cPF_Y_F32,
            cPF_RGBX_F32,
            cPF_RGBA_F32,
            cPF_Total
        };

        struct params
        {
            params()
            {
                clear();
            }

            void clear()
            {
                utils::zero_object(*this);

                m_boundary_op = Resampler::BOUNDARY_CLAMP;
                m_sample_low = 0.0f;
                m_sample_high = 255.0f;
                m_Pfilter_name = VOGL_RESAMPLER_DEFAULT_FILTER;
                m_filter_x_scale = 1.0f;
                m_filter_y_scale = 1.0f;
                m_x_ofs = 0.0f;
                m_y_ofs = 0.0f;
            }

            pixel_format m_fmt;

            const void *m_pSrc_pixels;
            uint32_t m_src_width;
            uint32_t m_src_height;
            uint32_t m_src_pitch;

            void *m_pDst_pixels;
            uint32_t m_dst_width;
            uint32_t m_dst_height;
            uint32_t m_dst_pitch;

            Resampler::Boundary_Op m_boundary_op;

            float m_sample_low;
            float m_sample_high;

            const char *m_Pfilter_name;
            float m_filter_x_scale;
            float m_filter_y_scale;

            float m_x_ofs;
            float m_y_ofs;
        };

        bool resample(const params &p);

    private:
        task_pool *m_pTask_pool;

        const params *m_pParams;

        Resampler::Contrib_List *m_pX_contribs;
        Resampler::Contrib_List *m_pY_contribs;
        uint32_t m_bytes_per_pixel;

        vogl::vector<vec4F> m_tmp_img;

        void free_contrib_lists();

        void resample_x_task(uint64_t data, void *pData_ptr);
        void resample_y_task(uint64_t data, void *pData_ptr);
    };

} // namespace vogl
