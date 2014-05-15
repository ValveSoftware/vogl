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

// File: vogl_dxt5a.cpp
#include "vogl_core.h"
#include "vogl_dxt5a.h"
#include "vogl_ryg_dxt.hpp"
#include "vogl_dxt_fast.h"
#include "vogl_intersect.h"

namespace vogl
{
    dxt5_endpoint_optimizer::dxt5_endpoint_optimizer()
        : m_pParams(NULL),
          m_pResults(NULL)
    {
        m_unique_values.reserve(16);
        m_unique_value_weights.reserve(16);
    }

    bool dxt5_endpoint_optimizer::compute(const params &p, results &r)
    {
        m_pParams = &p;
        m_pResults = &r;

        if ((!p.m_num_pixels) || (!p.m_pPixels))
            return false;

        m_unique_values.resize(0);
        m_unique_value_weights.resize(0);

        for (uint32_t i = 0; i < 256; i++)
            m_unique_value_map[i] = -1;

        for (uint32_t i = 0; i < p.m_num_pixels; i++)
        {
            uint32_t alpha = p.m_pPixels[i][p.m_comp_index];

            int index = m_unique_value_map[alpha];

            if (index == -1)
            {
                index = m_unique_values.size();

                m_unique_value_map[alpha] = index;

                m_unique_values.push_back(static_cast<uint8_t>(alpha));
                m_unique_value_weights.push_back(0);
            }

            m_unique_value_weights[index]++;
        }

        if (m_unique_values.size() == 1)
        {
            r.m_block_type = 0;
            r.m_error = 0;
            r.m_first_endpoint = m_unique_values[0];
            r.m_second_endpoint = m_unique_values[0];
            memset(r.m_pSelectors, 0, p.m_num_pixels);
            return true;
        }

        m_trial_selectors.resize(m_unique_values.size());
        m_best_selectors.resize(m_unique_values.size());

        r.m_error = cUINT64_MAX;

        for (uint32_t i = 0; i < m_unique_values.size() - 1; i++)
        {
            const uint32_t low_endpoint = m_unique_values[i];

            for (uint32_t j = i + 1; j < m_unique_values.size(); j++)
            {
                const uint32_t high_endpoint = m_unique_values[j];

                evaluate_solution(low_endpoint, high_endpoint);
            }
        }

        if ((m_pParams->m_quality >= cCRNDXTQualityBetter) && (m_pResults->m_error))
        {
            m_flags.resize(256 * 256);
            m_flags.clear_all_bits();

            const int cProbeAmount = (m_pParams->m_quality == cCRNDXTQualityUber) ? 16 : 8;

            for (int l_delta = -cProbeAmount; l_delta <= cProbeAmount; l_delta++)
            {
                const int l = m_pResults->m_first_endpoint + l_delta;
                if (l < 0)
                    continue;
                else if (l > 255)
                    break;

                const uint32_t bit_index = l * 256;

                for (int h_delta = -cProbeAmount; h_delta <= cProbeAmount; h_delta++)
                {
                    const int h = m_pResults->m_second_endpoint + h_delta;
                    if (h < 0)
                        continue;
                    else if (h > 255)
                        break;

                    //if (m_flags.get_bit(bit_index + h))
                    //   continue;
                    if ((m_flags.get_bit(bit_index + h)) || (m_flags.get_bit(h * 256 + l)))
                        continue;
                    m_flags.set_bit(bit_index + h);

                    evaluate_solution(static_cast<uint32_t>(l), static_cast<uint32_t>(h));
                }
            }
        }

        if (m_pResults->m_first_endpoint == m_pResults->m_second_endpoint)
        {
            for (uint32_t i = 0; i < m_best_selectors.size(); i++)
                m_best_selectors[i] = 0;
        }
        else if (m_pResults->m_block_type)
        {
            //if (l > h)
            //   eight alpha
            // else
            //   six alpha

            if (m_pResults->m_first_endpoint > m_pResults->m_second_endpoint)
            {
                utils::swap(m_pResults->m_first_endpoint, m_pResults->m_second_endpoint);
                for (uint32_t i = 0; i < m_best_selectors.size(); i++)
                    m_best_selectors[i] = g_six_alpha_invert_table[m_best_selectors[i]];
            }
        }
        else if (!(m_pResults->m_first_endpoint > m_pResults->m_second_endpoint))
        {
            utils::swap(m_pResults->m_first_endpoint, m_pResults->m_second_endpoint);
            for (uint32_t i = 0; i < m_best_selectors.size(); i++)
                m_best_selectors[i] = g_eight_alpha_invert_table[m_best_selectors[i]];
        }

        for (uint32_t i = 0; i < m_pParams->m_num_pixels; i++)
        {
            uint32_t alpha = m_pParams->m_pPixels[i][m_pParams->m_comp_index];

            int index = m_unique_value_map[alpha];

            m_pResults->m_pSelectors[i] = m_best_selectors[index];
        }

        return true;
    }

    void dxt5_endpoint_optimizer::evaluate_solution(uint32_t low_endpoint, uint32_t high_endpoint)
    {
        for (uint32_t block_type = 0; block_type < (m_pParams->m_use_both_block_types ? 2U : 1U); block_type++)
        {
            uint32_t selector_values[8];

            if (!block_type)
                dxt5_block::get_block_values8(selector_values, low_endpoint, high_endpoint);
            else
                dxt5_block::get_block_values6(selector_values, low_endpoint, high_endpoint);

            uint64_t trial_error = 0;

            for (uint32_t i = 0; i < m_unique_values.size(); i++)
            {
                const uint32_t val = m_unique_values[i];
                const uint32_t weight = m_unique_value_weights[i];

                uint32_t best_selector_error = UINT_MAX;
                uint32_t best_selector = 0;

                for (uint32_t j = 0; j < 8; j++)
                {
                    int selector_error = val - selector_values[j];
                    selector_error = selector_error * selector_error * (int)weight;

                    if (static_cast<uint32_t>(selector_error) < best_selector_error)
                    {
                        best_selector_error = selector_error;
                        best_selector = j;
                        if (!best_selector_error)
                            break;
                    }
                }

                m_trial_selectors[i] = static_cast<uint8_t>(best_selector);
                trial_error += best_selector_error;

                if (trial_error > m_pResults->m_error)
                    break;
            }

            if (trial_error < m_pResults->m_error)
            {
                m_pResults->m_error = trial_error;
                m_pResults->m_first_endpoint = static_cast<uint8_t>(low_endpoint);
                m_pResults->m_second_endpoint = static_cast<uint8_t>(high_endpoint);
                m_pResults->m_block_type = static_cast<uint8_t>(block_type);
                m_best_selectors.swap(m_trial_selectors);

                if (!trial_error)
                    break;
            }
        }
    }

} // namespace vogl
