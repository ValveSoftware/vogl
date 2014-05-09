/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.
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
 * This file was originally authored by Ian Elliott (ian@lunarg.com).
 *
 **************************************************************************/

#ifndef PXFMT_H
#define PXFMT_H

#if defined(_MSC_VER) && (_MSC_VER < 1800)
// Looks like round() was added to Visual Studio 13:
//   http://msdn.microsoft.com/en-us/library/dn353646.aspx

/*****************************************************************************
 * Work around the fact that Microsoft didn't implement the round() function:
 *****************************************************************************/
#include <cmath>
inline float round(float x)
{
    if (x < 0.0) {
        return ceil(x - 0.5);
    } else {
        return floor(x + 0.5);
    }
}
/*****************************************************************************
 * End of work-around.
 *****************************************************************************/
#endif

// NOTE TO RICH: The following enum will eventually capture all of the
// combinations of "format" and "type" that OpenGL/KTX supports, plus any other
// ones that we want to support (e.g. for DirectX/DDS).  At this point, what's
// below is what's supported by the code.  There are also a couple of overlaps
// that I collapse into one (one is called out below).
enum pxfmt_sized_format
{
    PXFMT_INVALID = 0,

// GL_RED
    PXFMT_R8_UNORM,
    PXFMT_R8_SNORM,
    PXFMT_R16_UNORM,
    PXFMT_R16_SNORM,
    PXFMT_R32_UNORM,
    PXFMT_R32_SNORM,
    PXFMT_R32_FLOAT,

// GL_GREEN
    PXFMT_G8_UNORM,
    PXFMT_G8_SNORM,
    PXFMT_G16_UNORM,
    PXFMT_G16_SNORM,
    PXFMT_G32_UNORM,
    PXFMT_G32_SNORM,
    PXFMT_G32_FLOAT,

// GL_BLUE
    PXFMT_B8_UNORM,
    PXFMT_B8_SNORM,
    PXFMT_B16_UNORM,
    PXFMT_B16_SNORM,
    PXFMT_B32_UNORM,
    PXFMT_B32_SNORM,
    PXFMT_B32_FLOAT,

// GL_ALPHA
    PXFMT_A8_UNORM,
    PXFMT_A8_SNORM,
    PXFMT_A16_UNORM,
    PXFMT_A16_SNORM,
    PXFMT_A32_UNORM,
    PXFMT_A32_SNORM,
    PXFMT_A32_FLOAT,

// GL_RG
    PXFMT_RG8_UNORM,
    PXFMT_RG8_SNORM,
    PXFMT_RG16_UNORM,
    PXFMT_RG16_SNORM,
    PXFMT_RG32_UNORM,
    PXFMT_RG32_SNORM,
    PXFMT_RG32_FLOAT,

// GL_RGB
    PXFMT_RGB8_UNORM,
    PXFMT_RGB8_SNORM,
    PXFMT_RGB16_UNORM,
    PXFMT_RGB16_SNORM,
    PXFMT_RGB32_UNORM,
    PXFMT_RGB32_SNORM,
    PXFMT_RGB32_FLOAT,

    PXFMT_RGB332_UNORM,
    PXFMT_RGB233_UNORM,
    PXFMT_RGB565_UNORM,
    PXFMT_RGB565REV_UNORM,

// GL_RGBA
    // NOTE: The following two OpenGL format-type combinations are actually
    // laid out the same in memory:
    // - GL_RGBA, GL_UNSIGNED_BYTE
    // - GL_RGBA, GL_UNSIGNED_INT_8_8_8_8
    PXFMT_RGBA8_UNORM,
    PXFMT_RGBA8_SNORM,
    PXFMT_RGBA16_UNORM,
    PXFMT_RGBA16_SNORM,
    PXFMT_RGBA32_UNORM,
    PXFMT_RGBA32_SNORM,
    PXFMT_RGBA32_FLOAT,

    PXFMT_RGBA4_UNORM,
    PXFMT_RGBA4REV_UNORM,
    PXFMT_RGB5A1_UNORM,
    PXFMT_A1RGB5_UNORM,
    PXFMT_RGBA8REV_UNORM,
    PXFMT_RGB10A2_UNORM,
    PXFMT_A2RGB10_UNORM,

// GL_BGRA
    PXFMT_BGRA8_UNORM,
    PXFMT_BGRA8_SNORM,
    PXFMT_BGRA16_UNORM,
    PXFMT_BGRA16_SNORM,
    PXFMT_BGRA32_UNORM,
    PXFMT_BGRA32_SNORM,
    PXFMT_BGRA32_FLOAT,

    PXFMT_BGRA4_UNORM,
    PXFMT_BGRA4REV_UNORM,
    PXFMT_BGR5A1_UNORM,
    PXFMT_A1BGR5_UNORM,
    PXFMT_BGRA8REV_UNORM,
    PXFMT_BGR10A2_UNORM,
    PXFMT_A2BGR10_UNORM,

// GL_RED_INTEGER
    PXFMT_R8_UINT,
    PXFMT_R8_SINT,
    PXFMT_R16_UINT,
    PXFMT_R16_SINT,
    PXFMT_R32_UINT,
    PXFMT_R32_SINT,

// GL_GREEN_INTEGER
    PXFMT_G8_UINT,
    PXFMT_G8_SINT,
    PXFMT_G16_UINT,
    PXFMT_G16_SINT,
    PXFMT_G32_UINT,
    PXFMT_G32_SINT,

// GL_BLUE_INTEGER
    PXFMT_B8_UINT,
    PXFMT_B8_SINT,
    PXFMT_B16_UINT,
    PXFMT_B16_SINT,
    PXFMT_B32_UINT,
    PXFMT_B32_SINT,

// GL_ALPHA_INTEGER
    PXFMT_A8_UINT,
    PXFMT_A8_SINT,
    PXFMT_A16_UINT,
    PXFMT_A16_SINT,
    PXFMT_A32_UINT,
    PXFMT_A32_SINT,

// GL_RG_INTEGER
    PXFMT_RG8_UINT,
    PXFMT_RG8_SINT,
    PXFMT_RG16_UINT,
    PXFMT_RG16_SINT,
    PXFMT_RG32_UINT,
    PXFMT_RG32_SINT,

// GL_RGB_INTEGER
    PXFMT_RGB8_UINT,
    PXFMT_RGB8_SINT,
    PXFMT_RGB16_UINT,
    PXFMT_RGB16_SINT,
    PXFMT_RGB32_UINT,
    PXFMT_RGB32_SINT,

    PXFMT_RGB332_UINT,
    PXFMT_RGB233_UINT,
    PXFMT_RGB565_UINT,
    PXFMT_RGB565REV_UINT,

// GL_RGBA_INTEGER
    // NOTE: The following two OpenGL format-type combinations are actually
    // laid out the same in memory:
    // - GL_RGBA_INTEGER, GL_UNSIGNED_BYTE
    // - GL_RGBA_INTEGER, GL_UNSIGNED_INT_8_8_8_8
    PXFMT_RGBA8_UINT,
    PXFMT_RGBA8_SINT,
    PXFMT_RGBA16_UINT,
    PXFMT_RGBA16_SINT,
    PXFMT_RGBA32_UINT,
    PXFMT_RGBA32_SINT,

    PXFMT_RGBA4_UINT,
    PXFMT_RGBA4REV_UINT,
    PXFMT_RGB5A1_UINT,
    PXFMT_A1RGB5_UINT,
    PXFMT_RGBA8REV_UINT,
    PXFMT_RGB10A2_UINT,
    PXFMT_A2RGB10_UINT,

// GL_BGRA_INTEGER
    PXFMT_BGRA8_UINT,
    PXFMT_BGRA8_SINT,
    PXFMT_BGRA16_UINT,
    PXFMT_BGRA16_SINT,
    PXFMT_BGRA32_UINT,
    PXFMT_BGRA32_SINT,

    PXFMT_BGRA4_UINT,
    PXFMT_BGRA4REV_UINT,
    PXFMT_BGR5A1_UINT,
    PXFMT_A1BGR5_UINT,
    PXFMT_BGRA8REV_UINT,
    PXFMT_BGR10A2_UINT,
    PXFMT_A2BGR10_UINT,

// GL_DEPTH_COMPONENT
    PXFMT_D8_UNORM,
    PXFMT_D8_SNORM,
    PXFMT_D16_UNORM,
    PXFMT_D16_SNORM,
    PXFMT_D32_UNORM,
    PXFMT_D32_SNORM,
    PXFMT_D32_FLOAT,

// GL_STENCIL_INDEX
    PXFMT_S8_UINT,
    PXFMT_S8_SINT,
    PXFMT_S16_UINT,
    PXFMT_S16_SINT,
    PXFMT_S32_UINT,
    PXFMT_S32_SINT,
    PXFMT_S32_FLOAT,

// GL_DEPTH_STENCIL
    PXFMT_D24_UNORM_S8_UINT,
    PXFMT_D32_FLOAT_S8_UINT,
};


// This function is used to get back a pxfmt_sized_format enum value for a
// given OpenGL "format" and "type".  If the format-type combination isn't
// supported, PXFMT_INVALID is returned.
pxfmt_sized_format validate_format_type_combo(const GLenum format,
                                              const GLenum type);


// This function is used to convert a rectangular set of pixel data from one
// pxfmt_sized_format to another.  The implementation of this function is
// towards the bottom of the "pxfmt.cpp" file.
void pxfmt_convert_pixels(void *pDst, const void *pSrc,
                          const int width, const int height,
                          const pxfmt_sized_format src_fmt,
                          const pxfmt_sized_format dst_fmt);

#endif // PXFMT_H
