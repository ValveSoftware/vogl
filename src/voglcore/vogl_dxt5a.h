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

// File: vogl_dxt5a.h
#pragma once

#include "vogl_core.h"
#include "vogl_dxt.h"

namespace vogl
{
    class dxt5_endpoint_optimizer
    {
    public:
        dxt5_endpoint_optimizer();

        struct params
        {
            params()
                : m_block_index(0),
                  m_pPixels(NULL),
                  m_num_pixels(0),
                  m_comp_index(3),
                  m_quality(cCRNDXTQualityUber),
                  m_use_both_block_types(true)
            {
            }

            uint32_t m_block_index;

            const color_quad_u8 *m_pPixels;
            uint32_t m_num_pixels;
            uint32_t m_comp_index;

            vogl_dxt_quality m_quality;

            bool m_use_both_block_types;
        };

        struct results
        {
            uint8_t *m_pSelectors;

            uint64_t m_error;

            uint8_t m_first_endpoint;
            uint8_t m_second_endpoint;

            uint8_t m_block_type; // 1 if 6-alpha, otherwise 8-alpha
        };

        bool compute(const params &p, results &r);

    private:
        const params *m_pParams;
        results *m_pResults;

        vogl::vector<uint8_t> m_unique_values;
        vogl::vector<uint32_t> m_unique_value_weights;

        vogl::vector<uint8_t> m_trial_selectors;
        vogl::vector<uint8_t> m_best_selectors;
        int m_unique_value_map[256];

        sparse_bit_array m_flags;

        void evaluate_solution(uint32_t low_endpoint, uint32_t high_endpoint);
    };

} // namespace vogl
