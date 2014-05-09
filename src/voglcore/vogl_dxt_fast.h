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

// File: vogl_dxt_fast.h
#pragma once

#include "vogl_core.h"
#include "vogl_color.h"
#include "vogl_dxt.h"

namespace vogl
{
    namespace dxt_fast
    {
        void compress_color_block(uint32_t n, const color_quad_u8 *block, uint32_t &low16, uint32_t &high16, uint8_t *pSelectors, bool refine = false);
        void compress_color_block(dxt1_block *pDXT1_block, const color_quad_u8 *pBlock, bool refine = false);

        void compress_alpha_block(uint32_t n, const color_quad_u8 *block, uint32_t &low8, uint32_t &high8, uint8_t *pSelectors, uint32_t comp_index);
        void compress_alpha_block(dxt5_block *pDXT5_block, const color_quad_u8 *pBlock, uint32_t comp_index);

        void find_representative_colors(uint32_t n, const color_quad_u8 *pBlock, color_quad_u8 &lo, color_quad_u8 &hi);

    } // namespace dxt_fast

} // namespace vogl
