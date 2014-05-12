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

// File: vogl_pixel_format.h
#pragma once

#include "vogl_core.h"
#include "vogl_dxt.h"
#include "vogl.h"
#include "dds_defs.h"

namespace vogl
{
    namespace pixel_format_helpers
    {
        uint32_t get_num_formats();
        pixel_format get_pixel_format_by_index(uint32_t index);

        const char *get_pixel_format_string(pixel_format fmt);

        const char *get_vogl_format_string(vogl_format fmt);

        inline bool is_grayscale(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_L8:
                case PIXEL_FMT_A8L8:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_dxt1(pixel_format fmt)
        {
            return (fmt == PIXEL_FMT_DXT1) || (fmt == PIXEL_FMT_DXT1A);
        }

        // has_alpha() should probably be called "has_opacity()" - it indicates if the format encodes opacity
        // because some swizzled DXT5 formats do not encode opacity.
        inline bool has_alpha(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_DXT1A:
                case PIXEL_FMT_DXT2:
                case PIXEL_FMT_DXT3:
                case PIXEL_FMT_DXT4:
                case PIXEL_FMT_DXT5:
                case PIXEL_FMT_DXT5A:
                case PIXEL_FMT_A8R8G8B8:
                case PIXEL_FMT_A8:
                case PIXEL_FMT_A8L8:
                case PIXEL_FMT_DXT5_AGBR:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_alpha_only(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_A8:
                case PIXEL_FMT_DXT5A:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_normal_map(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_3DC:
                case PIXEL_FMT_DXN:
                case PIXEL_FMT_DXT5_xGBR:
                case PIXEL_FMT_DXT5_xGxR:
                case PIXEL_FMT_DXT5_AGBR:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline int is_dxt(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_DXT1:
                case PIXEL_FMT_DXT1A:
                case PIXEL_FMT_DXT2:
                case PIXEL_FMT_DXT3:
                case PIXEL_FMT_DXT4:
                case PIXEL_FMT_DXT5:
                case PIXEL_FMT_3DC:
                case PIXEL_FMT_DXT5A:
                case PIXEL_FMT_DXN:
                case PIXEL_FMT_DXT5_CCxY:
                case PIXEL_FMT_DXT5_xGxR:
                case PIXEL_FMT_DXT5_xGBR:
                case PIXEL_FMT_DXT5_AGBR:
                case PIXEL_FMT_ETC1:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline int get_fundamental_format(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_DXT1A:
                    return PIXEL_FMT_DXT1;
                case PIXEL_FMT_DXT5_CCxY:
                case PIXEL_FMT_DXT5_xGxR:
                case PIXEL_FMT_DXT5_xGBR:
                case PIXEL_FMT_DXT5_AGBR:
                    return PIXEL_FMT_DXT5;
                default:
                    break;
            }
            return fmt;
        }

        inline dxt_format get_dxt_format(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_DXT1:
                    return cDXT1;
                case PIXEL_FMT_DXT1A:
                    return cDXT1A;
                case PIXEL_FMT_DXT2:
                    return cDXT3;
                case PIXEL_FMT_DXT3:
                    return cDXT3;
                case PIXEL_FMT_DXT4:
                    return cDXT5;
                case PIXEL_FMT_DXT5:
                    return cDXT5;
                case PIXEL_FMT_3DC:
                    return cDXN_YX;
                case PIXEL_FMT_DXT5A:
                    return cDXT5A;
                case PIXEL_FMT_DXN:
                    return cDXN_XY;
                case PIXEL_FMT_DXT5_CCxY:
                    return cDXT5;
                case PIXEL_FMT_DXT5_xGxR:
                    return cDXT5;
                case PIXEL_FMT_DXT5_xGBR:
                    return cDXT5;
                case PIXEL_FMT_DXT5_AGBR:
                    return cDXT5;
                case PIXEL_FMT_ETC1:
                    return cETC1;
                default:
                    break;
            }
            return cDXTInvalid;
        }

        inline pixel_format from_dxt_format(dxt_format dxt_fmt)
        {
            switch (dxt_fmt)
            {
                case cDXT1:
                    return PIXEL_FMT_DXT1;
                case cDXT1A:
                    return PIXEL_FMT_DXT1A;
                case cDXT3:
                    return PIXEL_FMT_DXT3;
                case cDXT5:
                    return PIXEL_FMT_DXT5;
                case cDXN_XY:
                    return PIXEL_FMT_DXN;
                case cDXN_YX:
                    return PIXEL_FMT_3DC;
                case cDXT5A:
                    return PIXEL_FMT_DXT5A;
                case cETC1:
                    return PIXEL_FMT_ETC1;
                default:
                    break;
            }
            VOGL_ASSERT(false);
            return PIXEL_FMT_INVALID;
        }

        inline bool is_pixel_format_non_srgb(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_3DC:
                case PIXEL_FMT_DXN:
                case PIXEL_FMT_DXT5A:
                case PIXEL_FMT_DXT5_CCxY:
                case PIXEL_FMT_DXT5_xGxR:
                case PIXEL_FMT_DXT5_xGBR:
                case PIXEL_FMT_DXT5_AGBR:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_vogl_format_non_srgb(vogl_format fmt)
        {
            switch (fmt)
            {
                case cCRNFmtDXN_XY:
                case cCRNFmtDXN_YX:
                case cCRNFmtDXT5A:
                case cCRNFmtDXT5_CCxY:
                case cCRNFmtDXT5_xGxR:
                case cCRNFmtDXT5_xGBR:
                case cCRNFmtDXT5_AGBR:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline uint32_t get_bpp(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_DXT1:
                    return 4;
                case PIXEL_FMT_DXT1A:
                    return 4;
                case PIXEL_FMT_ETC1:
                    return 4;
                case PIXEL_FMT_DXT2:
                    return 8;
                case PIXEL_FMT_DXT3:
                    return 8;
                case PIXEL_FMT_DXT4:
                    return 8;
                case PIXEL_FMT_DXT5:
                    return 8;
                case PIXEL_FMT_3DC:
                    return 8;
                case PIXEL_FMT_DXT5A:
                    return 4;
                case PIXEL_FMT_R8G8B8:
                    return 24;
                case PIXEL_FMT_A8R8G8B8:
                    return 32;
                case PIXEL_FMT_A8:
                    return 8;
                case PIXEL_FMT_L8:
                    return 8;
                case PIXEL_FMT_A8L8:
                    return 16;
                case PIXEL_FMT_DXN:
                    return 8;
                case PIXEL_FMT_DXT5_CCxY:
                    return 8;
                case PIXEL_FMT_DXT5_xGxR:
                    return 8;
                case PIXEL_FMT_DXT5_xGBR:
                    return 8;
                case PIXEL_FMT_DXT5_AGBR:
                    return 8;
                default:
                    break;
            }
            VOGL_ASSERT(false);
            return 0;
        };

        inline uint32_t get_dxt_bytes_per_block(pixel_format fmt)
        {
            switch (fmt)
            {
                case PIXEL_FMT_DXT1:
                    return 8;
                case PIXEL_FMT_DXT1A:
                    return 8;
                case PIXEL_FMT_DXT5A:
                    return 8;
                case PIXEL_FMT_ETC1:
                    return 8;
                case PIXEL_FMT_DXT2:
                    return 16;
                case PIXEL_FMT_DXT3:
                    return 16;
                case PIXEL_FMT_DXT4:
                    return 16;
                case PIXEL_FMT_DXT5:
                    return 16;
                case PIXEL_FMT_3DC:
                    return 16;
                case PIXEL_FMT_DXN:
                    return 16;
                case PIXEL_FMT_DXT5_CCxY:
                    return 16;
                case PIXEL_FMT_DXT5_xGxR:
                    return 16;
                case PIXEL_FMT_DXT5_xGBR:
                    return 16;
                case PIXEL_FMT_DXT5_AGBR:
                    return 16;
                default:
                    break;
            }
            VOGL_ASSERT(false);
            return 0;
        }

        enum component_flags
        {
            cCompFlagRValid = 1,
            cCompFlagGValid = 2,
            cCompFlagBValid = 4,
            cCompFlagAValid = 8,
            cCompFlagGrayscale = 16,
            cCompFlagNormalMap = 32,
            cCompFlagLumaChroma = 64,
            cCompFlagsRGBValid = cCompFlagRValid | cCompFlagGValid | cCompFlagBValid,
            cCompFlagsRGBAValid = cCompFlagRValid | cCompFlagGValid | cCompFlagBValid | cCompFlagAValid,
            cDefaultCompFlags = cCompFlagsRGBAValid
        };

        component_flags get_component_flags(pixel_format fmt);

        vogl_format convert_pixel_format_to_best_vogl_format(pixel_format vogl_fmt);

        pixel_format convert_vogl_format_to_pixel_format(vogl_format fmt);

    } // namespace pixel_format_helpers

} // namespace vogl
