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

// File: rg_etc1.h - Fast, high quality ETC1 block packer/unpacker - Rich Geldreich <richgel99@gmail.com>
// Please see ZLIB license at the end of this file.
#pragma once

#include "vogl_core.h"

namespace vogl
{

    namespace rg_etc1
    {
        // Unpacks an 8-byte ETC1 compressed block to a block of 4x4 32bpp RGBA pixels.
        // Returns false if the block is invalid. Invalid blocks will still be unpacked with clamping.
        // This function is thread safe, and does not dynamically allocate any memory.
        // If preserve_alpha is true, the alpha channel of the destination pixels will not be overwritten. Otherwise, alpha will be set to 255.
        bool unpack_etc1_block(const void *pETC1_block, unsigned int *pDst_pixels_rgba, bool preserve_alpha = false);

        // Quality setting = the higher the quality, the slower.
        // To pack large textures, it is highly recommended to call pack_etc1_block() in parallel, on different blocks, from multiple threads (particularly when using cHighQuality).
        enum etc1_quality
        {
            cLowQuality,
            cMediumQuality,
            cHighQuality,
        };

        struct etc1_pack_params
        {
            etc1_quality m_quality;
            bool m_dithering;

            inline etc1_pack_params()
            {
                clear();
            }

            void clear()
            {
                m_quality = cHighQuality;
                m_dithering = false;
            }
        };

        // Important: pack_etc1_block_init() must be called before calling pack_etc1_block().
        void pack_etc1_block_init();

        // Packs a 4x4 block of 32bpp RGBA pixels to an 8-byte ETC1 block.
        // 32-bit RGBA pixels must always be arranged as (R,G,B,A) (R first, A last) in memory, independent of platform endianness. A should always be 255.
        // Returns squared error of result.
        // This function is thread safe, and does not dynamically allocate any memory.
        // pack_etc1_block() does not currently support "perceptual" colorspace metrics - it primarily optimizes for RGB RMSE.
        unsigned int pack_etc1_block(void *pETC1_block, const unsigned int *pSrc_pixels_rgba, etc1_pack_params &pack_params);

    } // namespace rg_etc1

} // namespace vogl

//------------------------------------------------------------------------------
//
// rg_etc1 uses the ZLIB license:
// http://opensource.org/licenses/Zlib
//
// Copyright (c) 2012 Rich Geldreich
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software. If you use this software
// in a product, an acknowledgment in the product documentation would be
// appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
//------------------------------------------------------------------------------
