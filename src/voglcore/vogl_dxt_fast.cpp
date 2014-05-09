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

// File: vogl_dxt_fast.cpp
// Parts of this module are derived from RYG's excellent public domain DXTx compressor.
#include "vogl_core.h"
#include "vogl_dxt_fast.h"
#include "vogl_ryg_dxt.hpp"

namespace vogl
{
    namespace dxt_fast
    {
        static inline int mul_8bit(int a, int b)
        {
            int t = a * b + 128;
            return (t + (t >> 8)) >> 8;
        }

        static inline color_quad_u8 &unpack_color(color_quad_u8 &c, uint32_t v)
        {
            uint32_t rv = (v & 0xf800) >> 11;
            uint32_t gv = (v & 0x07e0) >> 5;
            uint32_t bv = (v & 0x001f) >> 0;

            c.r = ryg_dxt::Expand5[rv];
            c.g = ryg_dxt::Expand6[gv];
            c.b = ryg_dxt::Expand5[bv];
            c.a = 0;

            return c;
        }

        static inline uint32_t pack_color(const color_quad_u8 &c)
        {
            return (mul_8bit(c.r, 31) << 11) + (mul_8bit(c.g, 63) << 5) + mul_8bit(c.b, 31);
        }

#if 0
        static inline void lerp_color(color_quad_u8 &result, const color_quad_u8 &p1, const color_quad_u8 &p2, uint32_t f)
        {
            VOGL_ASSERT(f <= 255);

            result.r = static_cast<uint8_t>(p1.r + mul_8bit(p2.r - p1.r, f));
            result.g = static_cast<uint8_t>(p1.g + mul_8bit(p2.g - p1.g, f));
            result.b = static_cast<uint8_t>(p1.b + mul_8bit(p2.b - p1.b, f));
        }
#endif

        static inline void eval_colors(color_quad_u8 *pColors, uint32_t c0, uint32_t c1)
        {
            unpack_color(pColors[0], c0);
            unpack_color(pColors[1], c1);

#if 0
            lerp_color(pColors[2], pColors[0], pColors[1], 0x55);
            lerp_color(pColors[3], pColors[0], pColors[1], 0xAA);
#else
            pColors[2].r = (pColors[0].r * 2 + pColors[1].r) / 3;
            pColors[2].g = (pColors[0].g * 2 + pColors[1].g) / 3;
            pColors[2].b = (pColors[0].b * 2 + pColors[1].b) / 3;

            pColors[3].r = (pColors[1].r * 2 + pColors[0].r) / 3;
            pColors[3].g = (pColors[1].g * 2 + pColors[0].g) / 3;
            pColors[3].b = (pColors[1].b * 2 + pColors[0].b) / 3;
#endif
        }

        // false if all selectors equal
        static bool match_block_colors(uint32_t n, const color_quad_u8 *pBlock, const color_quad_u8 *pColors, uint8_t *pSelectors)
        {
            int dirr = pColors[0].r - pColors[1].r;
            int dirg = pColors[0].g - pColors[1].g;
            int dirb = pColors[0].b - pColors[1].b;

            int stops[4];
            for (int i = 0; i < 4; i++)
                stops[i] = pColors[i].r * dirr + pColors[i].g * dirg + pColors[i].b * dirb;

            // 0 2 3 1
            int c0Point = stops[1] + stops[3];
            int halfPoint = stops[3] + stops[2];
            int c3Point = stops[2] + stops[0];

            //dirr *= 2;
            //dirg *= 2;
            //dirb *= 2;
            c0Point >>= 1;
            halfPoint >>= 1;
            c3Point >>= 1;

            bool status = false;
            for (uint32_t i = 0; i < n; i++)
            {
                int dot = pBlock[i].r * dirr + pBlock[i].g * dirg + pBlock[i].b * dirb;

                uint8_t s;
                if (dot < halfPoint)
                    s = (dot < c0Point) ? 1 : 3;
                else
                    s = (dot < c3Point) ? 2 : 0;

                pSelectors[i] = s;

                if (s != pSelectors[0])
                    status = true;
            }

            return status;
        }

        static bool optimize_block_colors(uint32_t n, const color_quad_u8 *block, uint32_t &max16, uint32_t &min16, uint32_t ave_color[3], float axis[3])
        {
            int min[3], max[3];

            for (uint32_t ch = 0; ch < 3; ch++)
            {
                const uint8_t *bp = ((const uint8_t *)block) + ch;
                int minv, maxv;

                int64_t muv = bp[0];
                minv = maxv = bp[0];

                const uint32_t l = n << 2;
                for (uint32_t i = 4; i < l; i += 4)
                {
                    muv += bp[i];
                    minv = math::minimum<int>(minv, bp[i]);
                    maxv = math::maximum<int>(maxv, bp[i]);
                }

                ave_color[ch] = static_cast<int>((muv + (n / 2)) / n);
                min[ch] = minv;
                max[ch] = maxv;
            }

            if ((min[0] == max[0]) && (min[1] == max[1]) && (min[2] == max[2]))
                return false;

            // determine covariance matrix
            double cov[6];
            for (int i = 0; i < 6; i++)
                cov[i] = 0;

            for (uint32_t i = 0; i < n; i++)
            {
                double r = (int)block[i].r - (int)ave_color[0];
                double g = (int)block[i].g - (int)ave_color[1];
                double b = (int)block[i].b - (int)ave_color[2];

                cov[0] += r * r;
                cov[1] += r * g;
                cov[2] += r * b;
                cov[3] += g * g;
                cov[4] += g * b;
                cov[5] += b * b;
            }

            double covf[6], vfr, vfg, vfb;
            for (int i = 0; i < 6; i++)
                covf[i] = cov[i] * (1.0f / 255.0f);

            vfr = max[0] - min[0];
            vfg = max[1] - min[1];
            vfb = max[2] - min[2];

            static const uint32_t nIterPower = 4;
            for (uint32_t iter = 0; iter < nIterPower; iter++)
            {
                double r = vfr * covf[0] + vfg * covf[1] + vfb * covf[2];
                double g = vfr * covf[1] + vfg * covf[3] + vfb * covf[4];
                double b = vfr * covf[2] + vfg * covf[4] + vfb * covf[5];

                vfr = r;
                vfg = g;
                vfb = b;
            }

            double magn = math::maximum(math::maximum(fabs(vfr), fabs(vfg)), fabs(vfb));
            int v_r, v_g, v_b;

            if (magn < 4.0f) // too small, default to luminance
            {
                v_r = 148;
                v_g = 300;
                v_b = 58;

                axis[0] = (float)v_r;
                axis[1] = (float)v_g;
                axis[2] = (float)v_b;
            }
            else
            {
                magn = 512.0f / magn;
                vfr *= magn;
                vfg *= magn;
                vfb *= magn;
                v_r = static_cast<int>(vfr);
                v_g = static_cast<int>(vfg);
                v_b = static_cast<int>(vfb);

                axis[0] = (float)vfr;
                axis[1] = (float)vfg;
                axis[2] = (float)vfb;
            }

            int mind = block[0].r * v_r + block[0].g * v_g + block[0].b * v_b;
            int maxd = mind;
            color_quad_u8 minp(block[0]);
            color_quad_u8 maxp(block[0]);

            for (uint32_t i = 1; i < n; i++)
            {
                int dot = block[i].r * v_r + block[i].g * v_g + block[i].b * v_b;

                if (dot < mind)
                {
                    mind = dot;
                    minp = block[i];
                }

                if (dot > maxd)
                {
                    maxd = dot;
                    maxp = block[i];
                }
            }

            max16 = pack_color(maxp);
            min16 = pack_color(minp);

            return true;
        }

        // The refinement function. (Clever code, part 2)
        // Tries to optimize colors to suit block contents better.
        // (By solving a least squares system via normal equations+Cramer's rule)
        static bool refine_block(uint32_t n, const color_quad_u8 *block, uint32_t &max16, uint32_t &min16, const uint8_t *pSelectors)
        {
            static const int w1Tab[4] = { 3, 0, 2, 1 };

            static const int prods_0[4] = { 0x00, 0x00, 0x02, 0x02 };
            static const int prods_1[4] = { 0x00, 0x09, 0x01, 0x04 };
            static const int prods_2[4] = { 0x09, 0x00, 0x04, 0x01 };

            double akku_0 = 0;
            double akku_1 = 0;
            double akku_2 = 0;
            double At1_r, At1_g, At1_b;
            double At2_r, At2_g, At2_b;

            At1_r = At1_g = At1_b = 0;
            At2_r = At2_g = At2_b = 0;
            for (uint32_t i = 0; i < n; i++)
            {
                double r = block[i].r;
                double g = block[i].g;
                double b = block[i].b;
                int step = pSelectors[i];

                int w1 = w1Tab[step];

                akku_0 += prods_0[step];
                akku_1 += prods_1[step];
                akku_2 += prods_2[step];
                At1_r += w1 * r;
                At1_g += w1 * g;
                At1_b += w1 * b;
                At2_r += r;
                At2_g += g;
                At2_b += b;
            }

            At2_r = 3 * At2_r - At1_r;
            At2_g = 3 * At2_g - At1_g;
            At2_b = 3 * At2_b - At1_b;

            double xx = akku_2;
            double yy = akku_1;
            double xy = akku_0;

            double t = xx * yy - xy * xy;
            if (!yy || !xx || (fabs(t) < .0000125f))
                return false;

            double frb = (3.0f * 31.0f / 255.0f) / t;
            double fg = frb * (63.0f / 31.0f);

            uint32_t oldMin = min16;
            uint32_t oldMax = max16;

            // solve.
            max16 = math::clamp<int>(static_cast<int>((At1_r * yy - At2_r * xy) * frb + 0.5f), 0, 31) << 11;
            max16 |= math::clamp<int>(static_cast<int>((At1_g * yy - At2_g * xy) * fg + 0.5f), 0, 63) << 5;
            max16 |= math::clamp<int>(static_cast<int>((At1_b * yy - At2_b * xy) * frb + 0.5f), 0, 31) << 0;

            min16 = math::clamp<int>(static_cast<int>((At2_r * xx - At1_r * xy) * frb + 0.5f), 0, 31) << 11;
            min16 |= math::clamp<int>(static_cast<int>((At2_g * xx - At1_g * xy) * fg + 0.5f), 0, 63) << 5;
            min16 |= math::clamp<int>(static_cast<int>((At2_b * xx - At1_b * xy) * frb + 0.5f), 0, 31) << 0;

            return (oldMin != min16) || (oldMax != max16);
        }

        // false if all selectors equal
        static bool determine_selectors(uint32_t n, const color_quad_u8 *block, uint32_t min16, uint32_t max16, uint8_t *pSelectors)
        {
            color_quad_u8 color[4];

            if (max16 != min16)
            {
                eval_colors(color, min16, max16);

                return match_block_colors(n, block, color, pSelectors);
            }

            memset(pSelectors, 0, n);
            return false;
        }

        static uint64_t determine_error(uint32_t n, const color_quad_u8 *block, uint32_t min16, uint32_t max16, uint64_t early_out_error)
        {
            color_quad_u8 color[4];

            eval_colors(color, min16, max16);

            int dirr = color[0].r - color[1].r;
            int dirg = color[0].g - color[1].g;
            int dirb = color[0].b - color[1].b;

            int stops[4];
            for (int i = 0; i < 4; i++)
                stops[i] = color[i].r * dirr + color[i].g * dirg + color[i].b * dirb;

            // 0 2 3 1
            int c0Point = stops[1] + stops[3];
            int halfPoint = stops[3] + stops[2];
            int c3Point = stops[2] + stops[0];

            c0Point >>= 1;
            halfPoint >>= 1;
            c3Point >>= 1;

            uint64_t total_error = 0;

            for (uint32_t i = 0; i < n; i++)
            {
                const color_quad_u8 &a = block[i];

                uint32_t s = 0;
                if (min16 != max16)
                {
                    int dot = a.r * dirr + a.g * dirg + a.b * dirb;

                    if (dot < halfPoint)
                        s = (dot < c0Point) ? 1 : 3;
                    else
                        s = (dot < c3Point) ? 2 : 0;
                }

                const color_quad_u8 &b = color[s];

                int e = a[0] - b[0];
                total_error += e * e;

                e = a[1] - b[1];
                total_error += e * e;

                e = a[2] - b[2];
                total_error += e * e;

                if (total_error >= early_out_error)
                    break;
            }

            return total_error;
        }

        static bool refine_endpoints(uint32_t n, const color_quad_u8 *pBlock, uint32_t &low16, uint32_t &high16, uint8_t *pSelectors)
        {
            bool optimized = false;

            const int limits[3] = { 31, 63, 31 };

            for (uint32_t trial = 0; trial < 2; trial++)
            {
                color_quad_u8 color[4];
                eval_colors(color, low16, high16);

                uint64_t total_error[3] = { 0, 0, 0 };

                for (uint32_t i = 0; i < n; i++)
                {
                    const color_quad_u8 &a = pBlock[i];

                    const uint32_t s = pSelectors[i];
                    const color_quad_u8 &b = color[s];

                    int e = a[0] - b[0];
                    total_error[0] += e * e;

                    e = a[1] - b[1];
                    total_error[1] += e * e;

                    e = a[2] - b[2];
                    total_error[2] += e * e;
                }

                color_quad_u8 endpoints[2];
                endpoints[0] = dxt1_block::unpack_color((uint16_t)low16, false);
                endpoints[1] = dxt1_block::unpack_color((uint16_t)high16, false);

                color_quad_u8 expanded_endpoints[2];
                expanded_endpoints[0] = dxt1_block::unpack_color((uint16_t)low16, true);
                expanded_endpoints[1] = dxt1_block::unpack_color((uint16_t)high16, true);

                bool trial_optimized = false;

                for (uint32_t axis = 0; axis < 3; axis++)
                {
                    if (!total_error[axis])
                        continue;

                    const sU8 *const pExpand = (axis == 1) ? ryg_dxt::Expand6 : ryg_dxt::Expand5;

                    for (uint32_t e = 0; e < 2; e++)
                    {
                        uint32_t v[4];
                        v[e ^ 1] = expanded_endpoints[e ^ 1][axis];

                        for (int t = -1; t <= 1; t += 2)
                        {
                            int a = endpoints[e][axis] + t;
                            if ((a < 0) || (a > limits[axis]))
                                continue;

                            v[e] = pExpand[a];

                            //int delta = v[1] - v[0];
                            //v[2] = v[0] + mul_8bit(delta, 0x55);
                            //v[3] = v[0] + mul_8bit(delta, 0xAA);

                            v[2] = (v[0] * 2 + v[1]) / 3;
                            v[3] = (v[0] + v[1] * 2) / 3;

                            uint64_t axis_error = 0;

                            for (uint32_t i = 0; i < n; i++)
                            {
                                const color_quad_u8 &p = pBlock[i];

                                int e2 = v[pSelectors[i]] - p[axis];

                                axis_error += e2 * e2;

                                if (axis_error >= total_error[axis])
                                    break;
                            }

                            if (axis_error < total_error[axis])
                            {
                                //total_error[axis] = axis_error;

                                endpoints[e][axis] = (uint8_t)a;
                                expanded_endpoints[e][axis] = (uint8_t)v[e];

                                if (e)
                                    high16 = dxt1_block::pack_color(endpoints[1], false);
                                else
                                    low16 = dxt1_block::pack_color(endpoints[0], false);

                                determine_selectors(n, pBlock, low16, high16, pSelectors);

                                eval_colors(color, low16, high16);

                                utils::zero_object(total_error);

                                for (uint32_t i = 0; i < n; i++)
                                {
                                    const color_quad_u8 &a2 = pBlock[i];

                                    const uint32_t s = pSelectors[i];
                                    const color_quad_u8 &b = color[s];

                                    int e2 = a2[0] - b[0];
                                    total_error[0] += e2 * e2;

                                    e2 = a2[1] - b[1];
                                    total_error[1] += e2 * e2;

                                    e2 = a2[2] - b[2];
                                    total_error[2] += e2 * e2;
                                }

                                trial_optimized = true;
                            }

                        } // t

                    } // e
                }     // axis

                if (!trial_optimized)
                    break;

                optimized = true;

            } // for ( ; ; )

            return optimized;
        }

        static void refine_endpoints2(uint32_t n, const color_quad_u8 *pBlock, uint32_t &low16, uint32_t &high16, uint8_t *pSelectors, float axis[3])
        {
            uint64_t orig_error = determine_error(n, pBlock, low16, high16, cUINT64_MAX);
            if (!orig_error)
                return;

            float l = 1.0f / sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
            vec3F principle_axis(axis[0] * l, axis[1] * l, axis[2] * l);

            const float dist_per_trial = 0.027063293f;

            const uint32_t cMaxProbeRange = 8;
            uint32_t probe_low[cMaxProbeRange * 2 + 1];
            uint32_t probe_high[cMaxProbeRange * 2 + 1];

            int probe_range = 8;
            uint32_t num_iters = 4;

            const uint32_t num_trials = probe_range * 2 + 1;

            vec3F scaled_principle_axis(principle_axis * dist_per_trial);
            scaled_principle_axis[0] *= 31.0f;
            scaled_principle_axis[1] *= 63.0f;
            scaled_principle_axis[2] *= 31.0f;
            vec3F initial_ofs(scaled_principle_axis * (float)-probe_range);
            initial_ofs[0] += .5f;
            initial_ofs[1] += .5f;
            initial_ofs[2] += .5f;

            uint64_t cur_error = orig_error;

            for (uint32_t iter = 0; iter < num_iters; iter++)
            {
                color_quad_u8 endpoints[2];

                endpoints[0] = dxt1_block::unpack_color((uint16_t)low16, false);
                endpoints[1] = dxt1_block::unpack_color((uint16_t)high16, false);

                vec3F low_color(endpoints[0][0], endpoints[0][1], endpoints[0][2]);
                vec3F high_color(endpoints[1][0], endpoints[1][1], endpoints[1][2]);

                vec3F probe_low_color(low_color + initial_ofs);
                for (uint32_t i = 0; i < num_trials; i++)
                {
                    int r = math::clamp((int)floor(probe_low_color[0]), 0, 31);
                    int g = math::clamp((int)floor(probe_low_color[1]), 0, 63);
                    int b = math::clamp((int)floor(probe_low_color[2]), 0, 31);
                    probe_low[i] = b | (g << 5U) | (r << 11U);

                    probe_low_color += scaled_principle_axis;
                }

                vec3F probe_high_color(high_color + initial_ofs);
                for (uint32_t i = 0; i < num_trials; i++)
                {
                    int r = math::clamp((int)floor(probe_high_color[0]), 0, 31);
                    int g = math::clamp((int)floor(probe_high_color[1]), 0, 63);
                    int b = math::clamp((int)floor(probe_high_color[2]), 0, 31);
                    probe_high[i] = b | (g << 5U) | (r << 11U);

                    probe_high_color += scaled_principle_axis;
                }

                uint32_t best_l = low16;
                uint32_t best_h = high16;

                enum
                {
                    cMaxHash = 4
                };
                uint64_t hash[cMaxHash];
                for (uint32_t i = 0; i < cMaxHash; i++)
                    hash[i] = 0;

                uint32_t c = best_l | (best_h << 16);
                c = fast_hash(&c, sizeof(c));
                hash[(c >> 6) & 3] = 1ULL << (c & 63);

                for (uint32_t i = 0; i < num_trials; i++)
                {
                    for (uint32_t j = 0; j < num_trials; j++)
                    {
                        uint32_t l2 = probe_low[i];
                        uint32_t h = probe_high[j];
                        if (l2 < h)
                            utils::swap(l2, h);

                        uint32_t c2 = l2 | (h << 16);
                        c2 = fast_hash(&c2, sizeof(c2));
                        uint64_t mask = 1ULL << (c2 & 63);
                        uint32_t ofs = (c2 >> 6) & 3;
                        if (hash[ofs] & mask)
                            continue;

                        hash[ofs] |= mask;

                        uint64_t new_error = determine_error(n, pBlock, l2, h, cur_error);
                        if (new_error < cur_error)
                        {
                            best_l = l2;
                            best_h = h;
                            cur_error = new_error;
                        }
                    }
                }

                bool improved = false;

                if ((best_l != low16) || (best_h != high16))
                {
                    low16 = best_l;
                    high16 = best_h;

                    determine_selectors(n, pBlock, low16, high16, pSelectors);
                    improved = true;
                }

                if (refine_endpoints(n, pBlock, low16, high16, pSelectors))
                {
                    improved = true;

                    uint64_t cur_error2 = determine_error(n, pBlock, low16, high16, cUINT64_MAX);
                    if (!cur_error2)
                        return;
                }

                if (!improved)
                    break;

            } // iter

            //uint64_t end_error = determine_error(n, pBlock, low16, high16, UINT64_MAX);
            //if (end_error > orig_error) DebugBreak();
        }

        static void compress_solid_block(uint32_t n, uint32_t ave_color[3], uint32_t &low16, uint32_t &high16, uint8_t *pSelectors)
        {
            uint32_t r = ave_color[0];
            uint32_t g = ave_color[1];
            uint32_t b = ave_color[2];

            memset(pSelectors, 2, n);

            low16 = (ryg_dxt::OMatch5[r][0] << 11) | (ryg_dxt::OMatch6[g][0] << 5) | ryg_dxt::OMatch5[b][0];
            high16 = (ryg_dxt::OMatch5[r][1] << 11) | (ryg_dxt::OMatch6[g][1] << 5) | ryg_dxt::OMatch5[b][1];
        }

        void compress_color_block(uint32_t n, const color_quad_u8 *block, uint32_t &low16, uint32_t &high16, uint8_t *pSelectors, bool refine)
        {
            VOGL_ASSERT((n & 15) == 0);

            uint32_t ave_color[3];
            float axis[3];

            if (!optimize_block_colors(n, block, low16, high16, ave_color, axis))
            {
                compress_solid_block(n, ave_color, low16, high16, pSelectors);
            }
            else
            {
                if (!determine_selectors(n, block, low16, high16, pSelectors))
                    compress_solid_block(n, ave_color, low16, high16, pSelectors);
                else
                {
                    if (refine_block(n, block, low16, high16, pSelectors))
                        determine_selectors(n, block, low16, high16, pSelectors);

                    if (refine)
                        refine_endpoints2(n, block, low16, high16, pSelectors, axis);
                }
            }

            if (low16 < high16)
            {
                utils::swap(low16, high16);
                for (uint32_t i = 0; i < n; i++)
                    pSelectors[i] ^= 1;
            }
        }

        void compress_color_block(dxt1_block *pDXT1_block, const color_quad_u8 *pBlock, bool refine)
        {
            uint8_t color_selectors[16];
            uint32_t low16, high16;
            dxt_fast::compress_color_block(16, pBlock, low16, high16, color_selectors, refine);

            pDXT1_block->set_low_color(static_cast<uint16_t>(low16));
            pDXT1_block->set_high_color(static_cast<uint16_t>(high16));

            uint32_t mask = 0;
            for (int i = 15; i >= 0; i--)
            {
                mask <<= 2;
                mask |= color_selectors[i];
            }

            pDXT1_block->m_selectors[0] = (uint8_t)(mask & 0xFF);
            pDXT1_block->m_selectors[1] = (uint8_t)((mask >> 8) & 0xFF);
            pDXT1_block->m_selectors[2] = (uint8_t)((mask >> 16) & 0xFF);
            pDXT1_block->m_selectors[3] = (uint8_t)((mask >> 24) & 0xFF);
        }

        void compress_alpha_block(uint32_t n, const color_quad_u8 *block, uint32_t &low8, uint32_t &high8, uint8_t *pSelectors, uint32_t comp_index)
        {
            int min, max;
            min = max = block[0][comp_index];

            for (uint32_t i = 1; i < n; i++)
            {
                min = math::minimum<int>(min, block[i][comp_index]);
                max = math::maximum<int>(max, block[i][comp_index]);
            }

            low8 = max;
            high8 = min;

            int dist = max - min;
            int bias = min * 7 - (dist >> 1);
            int dist4 = dist * 4;
            int dist2 = dist * 2;

            for (uint32_t i = 0; i < n; i++)
            {
                int a = block[i][comp_index] * 7 - bias;
                int ind, t;

                t = (dist4 - a) >> 31;
                ind = t & 4;
                a -= dist4 & t;
                t = (dist2 - a) >> 31;
                ind += t & 2;
                a -= dist2 & t;
                t = (dist - a) >> 31;
                ind += t & 1;

                ind = -ind & 7;
                ind ^= (2 > ind);

                pSelectors[i] = static_cast<uint8_t>(ind);
            }
        }

        void compress_alpha_block(dxt5_block *pDXT5_block, const color_quad_u8 *pBlock, uint32_t comp_index)
        {
            uint8_t selectors[16];
            uint32_t low8, high8;

            compress_alpha_block(16, pBlock, low8, high8, selectors, comp_index);

            pDXT5_block->set_low_alpha(low8);
            pDXT5_block->set_high_alpha(high8);

            uint32_t mask = 0;
            uint32_t bits = 0;
            uint8_t *pDst = pDXT5_block->m_selectors;

            for (uint32_t i = 0; i < 16; i++)
            {
                mask |= (selectors[i] << bits);

                if ((bits += 3) >= 8)
                {
                    *pDst++ = static_cast<uint8_t>(mask);
                    mask >>= 8;
                    bits -= 8;
                }
            }
        }

        void find_representative_colors(uint32_t n, const color_quad_u8 *pBlock, color_quad_u8 &lo, color_quad_u8 &hi)
        {
            uint64_t ave64[3];
            ave64[0] = 0;
            ave64[1] = 0;
            ave64[2] = 0;

            for (uint32_t i = 0; i < n; i++)
            {
                ave64[0] += pBlock[i].r;
                ave64[1] += pBlock[i].g;
                ave64[2] += pBlock[i].b;
            }

            uint32_t ave[3];
            ave[0] = static_cast<uint32_t>((ave64[0] + (n / 2)) / n);
            ave[1] = static_cast<uint32_t>((ave64[1] + (n / 2)) / n);
            ave[2] = static_cast<uint32_t>((ave64[2] + (n / 2)) / n);

            int furthest_dist = -1;
            uint32_t furthest_index = 0;
            for (uint32_t i = 0; i < n; i++)
            {
                int r = pBlock[i].r - ave[0];
                int g = pBlock[i].g - ave[1];
                int b = pBlock[i].b - ave[2];
                int dist = r * r + g * g + b * b;
                if (dist > furthest_dist)
                {
                    furthest_dist = dist;
                    furthest_index = i;
                }
            }

            color_quad_u8 lo_color(pBlock[furthest_index]);

            int opp_dist = -1;
            uint32_t opp_index = 0;
            for (uint32_t i = 0; i < n; i++)
            {
                int r = pBlock[i].r - lo_color.r;
                int g = pBlock[i].g - lo_color.g;
                int b = pBlock[i].b - lo_color.b;
                int dist = r * r + g * g + b * b;
                if (dist > opp_dist)
                {
                    opp_dist = dist;
                    opp_index = i;
                }
            }

            color_quad_u8 hi_color(pBlock[opp_index]);

            for (uint32_t i = 0; i < 3; i++)
            {
                lo_color[i] = static_cast<uint8_t>((lo_color[i] + ave[i]) >> 1);
                hi_color[i] = static_cast<uint8_t>((hi_color[i] + ave[i]) >> 1);
            }

            const uint32_t cMaxIters = 4;
            for (uint32_t iter_index = 0; iter_index < cMaxIters; iter_index++)
            {
                if ((lo_color[0] == hi_color[0]) && (lo_color[1] == hi_color[1]) && (lo_color[2] == hi_color[2]))
                    break;

                uint64_t new_color[2][3];
                uint32_t weight[2];

                utils::zero_object(new_color);
                utils::zero_object(weight);

                int vec_r = hi_color[0] - lo_color[0];
                int vec_g = hi_color[1] - lo_color[1];
                int vec_b = hi_color[2] - lo_color[2];

                int lo_dot = vec_r * lo_color[0] + vec_g * lo_color[1] + vec_b * lo_color[2];
                int hi_dot = vec_r * hi_color[0] + vec_g * hi_color[1] + vec_b * hi_color[2];
                int mid_dot = lo_dot + hi_dot;

                vec_r *= 2;
                vec_g *= 2;
                vec_b *= 2;

                for (uint32_t i = 0; i < n; i++)
                {
                    const color_quad_u8 &c = pBlock[i];

                    const int dot = c[0] * vec_r + c[1] * vec_g + c[2] * vec_b;
                    const uint32_t match_index = (dot > mid_dot);

                    new_color[match_index][0] += c.r;
                    new_color[match_index][1] += c.g;
                    new_color[match_index][2] += c.b;
                    weight[match_index]++;
                }

                if ((!weight[0]) || (!weight[1]))
                    break;

                uint8_t new_color8[2][3];

                for (uint32_t j = 0; j < 2; j++)
                    for (uint32_t i = 0; i < 3; i++)
                        new_color8[j][i] = static_cast<uint8_t>((new_color[j][i] + (weight[j] / 2)) / weight[j]);

                if ((new_color8[0][0] == lo_color[0]) && (new_color8[0][1] == lo_color[1]) && (new_color8[0][2] == lo_color[2]) &&
                    (new_color8[1][0] == hi_color[0]) && (new_color8[1][1] == hi_color[1]) && (new_color8[1][2] == hi_color[2]))
                    break;

                for (uint32_t i = 0; i < 3; i++)
                {
                    lo_color[i] = new_color8[0][i];
                    hi_color[i] = new_color8[1][i];
                }
            }

            uint32_t energy[2] = { 0, 0 };
            for (uint32_t i = 0; i < 3; i++)
            {
                energy[0] += lo_color[i] * lo_color[i];
                energy[1] += hi_color[i] * hi_color[i];
            }

            if (energy[0] > energy[1])
                utils::swap(lo_color, hi_color);

            lo = lo_color;
            hi = hi_color;
        }

    } // namespace dxt_fast

} // namespace vogl
