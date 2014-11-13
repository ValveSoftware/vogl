/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.  All Rights Reserved.
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

#if defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm> // For std::max()
#include <assert.h>

#include "pxfmt_gl.h"
#include "pxfmt.h"
#include <cmath>  // For std::floor() and pow()

#ifdef DECOMPRESS_DEBUG
#include <stdio.h>
#endif // DECOMPRESS_DEBUG

#include "pxfmt_internal.h"

// The internal data structures and functions are put into the following
// unnamed namespace, so that they aren't externally visible to this file:
namespace
{

/******************************************************************************
 *
 * Note: The following code creates a set of compile-time, templated struct's
 * that contain information needed to perform conversions to/from pixels in a
 * given pxfmt_sized_format.
 *
 * This allows the compiler to create, populate, and access something like a
 * table, using a pxfmt_sized_format as an index.  Yet, it's setup at
 * compile-time, not at run-time.  The use of the macro allows different data
 * types to be included in each struct, which are used in the functions that
 * come later in the file.
 *
 ******************************************************************************/

// The following struct contains the maximum value of (and as a result, a mask
// for) a certain number of bits:
template <bool is_signed, int nbits> struct max_values { };

#define MAX_VALUES(is_signed, nbits)                                    \
    template <> struct max_values<is_signed, nbits>                     \
    {                                                                   \
    public:                                                             \
        static const uint32 m_max =                                     \
            /* Calculate using a 64-bit integer, so that we can */      \
            /* shift left by 32 bits: */                                \
            (uint32) ((1ULL << ((!is_signed) ?                          \
                                /* The maximum value of unsigned */     \
                                /* integers is "(1 << nbits) - 1": */   \
                                (nbits) :                               \
                                /* The maximum value of signed */       \
                                /* integers is "(1 << (nbits-1)) - 1" */ \
                                /* unless nbits is 0 (which is only */  \
                                /* used at compile time): */            \
                                ((nbits) > 0) ? ((nbits) - 1) : 0)      \
                       /* Now, subtract one: */                         \
                       ) - 1);                                          \
    }


// Note: must define structs for "0" for compiling code below:
MAX_VALUES(false, 0);
MAX_VALUES(false, 1);
MAX_VALUES(false, 2);
MAX_VALUES(false, 3);
MAX_VALUES(false, 4);
MAX_VALUES(false, 5);
MAX_VALUES(false, 6);
MAX_VALUES(false, 8);
MAX_VALUES(false, 9);
MAX_VALUES(false, 10);
MAX_VALUES(false, 16);
MAX_VALUES(false, 24);
MAX_VALUES(false, 32);
MAX_VALUES(true,  0);
MAX_VALUES(true,  8);
MAX_VALUES(true,  16);
MAX_VALUES(true,  32);



/******************************************************************************
 *
 * Note: The following code creates a set of compile-time, templated struct's
 * that contain information needed to perform conversions between a double and
 * either a 10, 11, or 16-bit floating-point value.
 *
 ******************************************************************************/

enum pxfmt_small_fp
{
    NON_FP = 0,
    FP10 = 10,
    FP11 = 11,
    FP16 = 16,
};

template <pxfmt_small_fp nbits> struct small_fp { };

#define SMALL_FP(nbits, min, max, sign_shift,                           \
                 exp_mask, exp_shift, bias, man_mask, man_shift,        \
                 inf_exp_mask)                                          \
    template <> struct small_fp<nbits>                                  \
    {                                                                   \
    public:                                                             \
        static const int32 m_min = min;                                 \
        static const uint32 m_max = max;                                \
        static const uint32 m_sign_shift = sign_shift;                  \
        static const uint32 m_exp_mask = exp_mask;                      \
        static const uint32 m_exp_shift = exp_shift;                    \
        static const uint32 m_bias = bias;                              \
        static const uint32 m_man_mask = man_mask;                      \
        static const uint32 m_man_shift = man_shift;                    \
        static const uint32 m_inf_exp_mask = inf_exp_mask;              \
    };

// FIXME: SEEMS LIKE WE NEED THE FOLLOWING FOR INFINITY AND NAN:
//
// - bits to check a double's exponent for NAN case
// - a double's exponent for NAN/INFINITY (i.e. 128).  NOTE: This is bias + 1.
// - fp10-16's exponent for NAN/INFINITY (i.e. 16).    NOTE: This is bias + 1.
// - fp10-16's mantissa for NAN (e.g. 0x10 for fp10)

SMALL_FP(NON_FP,    0,     0,  0, 0x0000, 0,   0, 0x0000,  0,          0);
SMALL_FP(FP10,      0, 64512, 10, 0x03E0, 5,  15, 0x001f, 15, 0x7FF00000);
SMALL_FP(FP11,      0, 65024, 11, 0x07C0, 6,  15, 0x003f, 14, 0x7FF00000);
SMALL_FP(FP16, -65504, 65504, 15, 0x7C00, 10, 15, 0x03ff, 10, 0x7FF00000);


union double_conversion
{
    double d;
    struct
    {
        uint32 u[2];
    } i;
};


/******************************************************************************
 *
 * Note: The following code creates a set of compile-time, templated struct's
 * that contain information needed to perform conversions to/from pixels in a
 * given pxfmt_sized_format.
 *
 * This allows the compiler to create, populate, and access something like a
 * table, using a pxfmt_sized_format as an index.  Yet, it's setup at
 * compile-time, not at run-time.  The use of the macro allows different data
 * types to be included in each struct, which are used in the functions that
 * come later in the file.
 *
 ******************************************************************************/

// The following struct contains information used for the conversion of each
// pxfmt_sized_format (i.e. for each OpenGL format/type combination):
template <pxfmt_sized_format F> struct pxfmt_per_fmt_info { };

#define FMT_INFO_BASE(F, ogl_fmt, ftype, itype, ncomps, bypp,           \
                      needfp, norm, is_signed, pack,                    \
                      in0, in1, in2, in3,                               \
                      nbits0, nbits1, nbits2, nbits3,                   \
                      shift0, shift1, shift2, shift3,                   \
                      is_comp, bwidth, bheight, bsize)                  \
                                                                        \
    template <> struct pxfmt_per_fmt_info<F>                            \
    {                                                                   \
        static const pxfmt_sized_format m_fmt = F;                      \
        static const GLenum m_format = ogl_fmt;                         \
        /* The type to access components and/or pixels of formatted data: */ \
        typedef ftype m_formatted_type;                                 \
        /* The type of intermediate values (convert to/from): */        \
        typedef itype m_intermediate_type;                              \
        static const uint32 m_num_components = ncomps;                  \
        static const uint32 m_bytes_per_pixel = bypp;                   \
                                                                        \
        static const bool m_needs_fp_intermediate = needfp;             \
        static const bool m_is_normalized = norm;                       \
        static const bool m_is_signed = is_signed;                      \
        static const bool m_is_packed = pack;                           \
                                                                        \
        /* The m_index[] members identify which "component index" */    \
        /* to use for the n'th component dealt with.  For example, */   \
        /* for GL_RGBA, component 0 (the first component converted) */  \
        /* has an index of 0 (i.e. red), where for GL_BGRA, component */ \
        /* 0 is 2 (i.e. blue). */                                       \
        static const int32 m_index[4];                                 \
                                                                        \
        /* The m_max[] members contain the "maximum value" to use for */ \
        /* the n'th component dealt with, and is used for converting */ \
        /* between fixed-point normalized integer values and. */        \
        /* floating-point values. */                                    \
        static const uint32 m_max[4];                                   \
                                                                        \
        /* The m_mask[] members contain how many bits to shift between */ \
        /* the n'th component's bottom bit, and the bottom bit of a */  \
        /* a uint32.  This is only for packed types. */                 \
        static const uint32 m_shift[4];                                 \
                                                                        \
        /* The m_mask[] members identify which bits to use for the */   \
        /* n'th component dealt with.  This is only for packed types. */ \
        static const uint32 m_mask[4];                                  \
                                                                        \
        /* The m_small_fp[] member identifies which, if any,  */        \
        /* "small-floating-point" number is represented by a given */   \
        /* component. */                                                \
        static const pxfmt_small_fp m_small_fp[4];                      \
                                                                        \
        /* The m_is_compressed member is true for all compressed */     \
        /* textures, and false otherwise. */                            \
        static const bool m_is_compressed = is_comp;                    \
        /* The m_block_size member indicates the width (in texels) */   \
        /* of each compression block for all compressed textures, */    \
        /* and 0 otherwise. */                                          \
        static const uint32 m_block_width = bwidth;                     \
        /* The m_block_size member indicates the height (in texels) */  \
        /* of each compression block for all compressed textures, */    \
        /* and 0 otherwise. */                                          \
        static const uint32 m_block_height = bheight;                   \
        /* The m_block_size member indicates the number of bytes */     \
        /* per compression block for all compressed textures, and 0 */  \
        /* otherwise. */                                                \
        static const uint32 m_block_size = bsize;                       \
    };                                                                  \
    const int32 pxfmt_per_fmt_info<F>::m_index[] = {in0, in1, in2, in3}; \
    const uint32 pxfmt_per_fmt_info<F>::m_max[] =                       \
        {max_values<is_signed,nbits0>::m_max,                           \
         max_values<is_signed,nbits1>::m_max,                           \
         max_values<is_signed,nbits2>::m_max,                           \
         max_values<is_signed,nbits3>::m_max};                          \
    const uint32 pxfmt_per_fmt_info<F>::m_shift[] =                     \
        {shift0, shift1, shift2, shift3};                               \
    const uint32 pxfmt_per_fmt_info<F>::m_mask[] =                      \
        {(max_values<is_signed,nbits0>::m_max << shift0),               \
         (max_values<is_signed,nbits1>::m_max << shift1),               \
         (max_values<is_signed,nbits2>::m_max << shift2),               \
         (max_values<is_signed,nbits3>::m_max << shift3)};

    // This variation is used for most cases:
#define FMT_INFO(F, ogl_fmt, ftype, itype, ncomps, bypp,                \
                 needfp, norm, is_signed, pack,                         \
                 in0, in1, in2, in3,                                    \
                 nbits0, nbits1, nbits2, nbits3,                        \
                 shift0, shift1, shift2, shift3)                        \
                                                                        \
    FMT_INFO_BASE(F, ogl_fmt, ftype, itype, ncomps, bypp,               \
                  needfp, norm, is_signed, pack,                        \
                  in0, in1, in2, in3,                                   \
                  nbits0, nbits1, nbits2, nbits3,                       \
                  shift0, shift1, shift2, shift3,                       \
                  false, 0, 0, 1);                                      \
    const pxfmt_small_fp pxfmt_per_fmt_info<F>::m_small_fp[] =          \
        {NON_FP, NON_FP, NON_FP, NON_FP};

// This variation is used for the few "small-fp" cases:
#define FMT_INFO_SFP(F, ogl_fmt, ftype, itype, ncomps, bypp,            \
                     is_signed,                                         \
                     in0, in1, in2, in3,                                \
                     nbits0, nbits1, nbits2, nbits3,                    \
                     shift0, shift1, shift2, shift3,                    \
                     sfp0, sfp1, sfp2, sfp3)                            \
                                                                        \
    FMT_INFO_BASE(F, ogl_fmt, ftype, itype, ncomps, bypp,               \
                  true, false, is_signed, false,                        \
                  in0, in1, in2, in3,                                   \
                  nbits0, nbits1, nbits2, nbits3,                       \
                  shift0, shift1, shift2, shift3,                       \
                  false, 0, 0, 1);                                      \
    const pxfmt_small_fp pxfmt_per_fmt_info<F>::m_small_fp[] =          \
            {sfp0, sfp1, sfp2, sfp3};

    // This variation is used for compressed texture formats:
#define FMT_INFO_COMPRESSED(F, ogl_fmt, ncomps, is_signed,              \
                            bwidth, bheight, bsize)                     \
                                                                        \
    FMT_INFO_BASE(F, ogl_fmt, uint32, double,                           \
                  ncomps, /*don't care*/1,                              \
                  true, false, is_signed, /*don't care*/false,          \
                  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,             \
                  true, bwidth, bheight, bsize);                        \
    const pxfmt_small_fp pxfmt_per_fmt_info<F>::m_small_fp[] =          \
        {NON_FP, NON_FP, NON_FP, NON_FP};



// FIXME: use awk sometime to pretty this up, and align the columns of
// information:

// GL_RED
FMT_INFO(PXFMT_R8_UNORM,   GL_RED,    uint8,  double, 1, 1, true, true, false, false,       0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R8_SNORM,   GL_RED,    int8,   double, 1, 1, true, true, true, false,        0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R16_UNORM,  GL_RED,    uint16, double, 1, 2, true, true, false, false,       0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R16_SNORM,  GL_RED,    int16,  double, 1, 2, true, true, true, false,        0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R32_UNORM,  GL_RED,    uint32, double, 1, 4, true, true, false, false,       0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R32_SNORM,  GL_RED,    int32,  double, 1, 4, true, true, true, false,        0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_R16_FLOAT,GL_RED, uint16,  double, 1, 2,             true,               0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   FP16, NON_FP, NON_FP, NON_FP);
FMT_INFO(PXFMT_R32_FLOAT,  GL_RED,    float,  double, 1, 4, true, false, false, false,      0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_GREEN
FMT_INFO(PXFMT_G8_UNORM,   GL_GREEN,  uint8,  double, 1, 1, true, true, false, false,       1, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G8_SNORM,   GL_GREEN,  int8,   double, 1, 1, true, true, true, false,        1, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G16_UNORM,  GL_GREEN,  uint16, double, 1, 2, true, true, false, false,       1, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G16_SNORM,  GL_GREEN,  int16,  double, 1, 2, true, true, true, false,        1, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G32_UNORM,  GL_GREEN,  uint32, double, 1, 4, true, true, false, false,       1, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G32_SNORM,  GL_GREEN,  int32,  double, 1, 4, true, true, true, false,        1, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_G16_FLOAT,GL_GREEN,uint16, double,1, 2,              true,               1, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   NON_FP, FP16, NON_FP, NON_FP);
FMT_INFO(PXFMT_G32_FLOAT,  GL_GREEN,  float,  double, 1, 4, true, false, false, false,      1, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_BLUE
FMT_INFO(PXFMT_B8_UNORM,   GL_BLUE,   uint8,  double, 1, 1, true, true, false, false,       2, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B8_SNORM,   GL_BLUE,   int8,   double, 1, 1, true, true, true, false,        2, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B16_UNORM,  GL_BLUE,   uint16, double, 1, 2, true, true, false, false,       2, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B16_SNORM,  GL_BLUE,   int16,  double, 1, 2, true, true, true, false,        2, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B32_UNORM,  GL_BLUE,   uint32, double, 1, 4, true, true, false, false,       2, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B32_SNORM,  GL_BLUE,   int32,  double, 1, 4, true, true, true, false,        2, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_B16_FLOAT,GL_BLUE, uint16, double, 1, 2,             true,               2, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   NON_FP, NON_FP, FP16, NON_FP);
FMT_INFO(PXFMT_B32_FLOAT,  GL_BLUE,   float,  double, 1, 4, true, false, false, false,      2, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_ALPHA
FMT_INFO(PXFMT_A8_UNORM,          GL_ALPHA,  uint8,  double, 1, 1, true, true, false, false,    3, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A8_SNORM,          GL_ALPHA,  int8,   double, 1, 1, true, true, true, false,     3, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A16_UNORM,         GL_ALPHA,  uint16, double, 1, 2, true, true, false, false,    3, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A16_SNORM,         GL_ALPHA,  int16,  double, 1, 2, true, true, true, false,     3, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A32_UNORM,         GL_ALPHA,  uint32, double, 1, 4, true, true, false, false,    3, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A32_SNORM,         GL_ALPHA,  int32,  double, 1, 4, true, true, true, false,     3, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_A16_FLOAT,     GL_ALPHA,  uint16, double, 1, 2,             true,            3, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   NON_FP, NON_FP, NON_FP, FP16);
FMT_INFO(PXFMT_A32_FLOAT,         GL_ALPHA,  float,  double, 1, 4, true, false, false, false,   3, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_RG
FMT_INFO(PXFMT_RG8_UNORM,         GL_RG,     uint8,  double, 2, 2, true, true, false, false,    0, 1, -1, -1,     8,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG8_SNORM,         GL_RG,     int8,   double, 2, 2, true, true, true, false,     0, 1, -1, -1,     8,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG16_UNORM,        GL_RG,     uint16, double, 2, 4, true, true, false, false,    0, 1, -1, -1,    16, 16,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG16_SNORM,        GL_RG,     int16,  double, 2, 4, true, true, true, false,     0, 1, -1, -1,    16, 16,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG32_UNORM,        GL_RG,     uint32, double, 2, 8, true, true, false, false,    0, 1, -1, -1,    32, 32,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG32_SNORM,        GL_RG,     int32,  double, 2, 8, true, true, true, false,     0, 1, -1, -1,    32, 32,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_RG16_FLOAT,    GL_RG,     uint16, double, 2, 4,             true,            0, 1, -1, -1,     0,  0,  0,  0,   0, 0, 0, 0,   FP16, FP16, NON_FP, NON_FP);
FMT_INFO(PXFMT_RG32_FLOAT,        GL_RG,     float,  double, 2, 8, true, false, false, false,   0, 1, -1, -1,     0,  0,  0,  0,   0, 0, 0, 0);

// GL_RGB
FMT_INFO(PXFMT_RGB8_UNORM,        GL_RGB,    uint8,  double, 3, 3,  true, true, false, false,   0, 1, 2, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB8_SNORM,        GL_RGB,    int8,   double, 3, 3,  true, true, true, false,    0, 1, 2, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB16_UNORM,       GL_RGB,    uint16, double, 3, 6,  true, true, false, false,   0, 1, 2, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB16_SNORM,       GL_RGB,    int16,  double, 3, 6,  true, true, true, false,    0, 1, 2, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB32_UNORM,       GL_RGB,    uint32, double, 3, 12, true, true, false, false,   0, 1, 2, -1,     32, 32, 32,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB32_SNORM,       GL_RGB,    int32,  double, 3, 12, true, true, true, false,    0, 1, 2, -1,     32, 32, 32,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_RGB16_FLOAT,   GL_RGB,    uint16, double, 3, 6,              true,           0, 1, 2, -1,      0,  0,  0,  0,   0, 0, 0, 0,   FP16, FP16, FP16, NON_FP);
FMT_INFO(PXFMT_RGB32_FLOAT,       GL_RGB,    float,  double, 3, 12, true, false, false, false,  0, 1, 2, -1,      0,  0,  0,  0,   0, 0, 0, 0);

FMT_INFO(PXFMT_RGB332_UNORM,      GL_RGB,    uint8,  double, 3, 1,  true, true, false, true,    0, 1, 2, -1,      3,  3,  2,  0,   5, 2, 0, 0);
FMT_INFO(PXFMT_RGB233_UNORM,      GL_RGB,    uint8,  double, 3, 1,  true, true, false, true,    0, 1, 2, -1,      3,  3,  2,  0,   0, 3, 6, 0);
FMT_INFO(PXFMT_RGB565_UNORM,      GL_RGB,    uint16, double, 3, 2,  true, true, false, true,    0, 1, 2, -1,      5,  6,  5,  0,   11, 5, 0, 0);
FMT_INFO(PXFMT_RGB565REV_UNORM,   GL_RGB,    uint16, double, 3, 2,  true, true, false, true,    0, 1, 2, -1,      5,  6,  5,  0,   0, 5, 11, 0);
FMT_INFO(PXFMT_RGB5999_REV,       GL_RGB,    uint32, double, 3, 4,  true, false, false, true,   0, 1, 2, -1,      9,  9,  9,  5,   0, 9, 18, 27);
FMT_INFO_SFP(PXFMT_RGB10F_11F_11F,GL_RGB,    uint32, double, 3, 4,              false,          0, 1, 2, -1,      0,  0,  0,  0,   0, 0, 0, 0,   FP10, FP11, FP11, NON_FP);

// GL_BGR
FMT_INFO(PXFMT_BGR8_UNORM,        GL_BGR,    uint8,  double, 3, 3,  true, true, false, false,   2, 1, 0, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR8_SNORM,        GL_BGR,    int8,   double, 3, 3,  true, true, true, false,    2, 1, 0, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR16_UNORM,       GL_BGR,    uint16, double, 3, 6,  true, true, false, false,   2, 1, 0, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR16_SNORM,       GL_BGR,    int16,  double, 3, 6,  true, true, true, false,    2, 1, 0, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR32_UNORM,       GL_BGR,    uint32, double, 3, 12, true, true, false, false,   2, 1, 0, -1,     32, 32, 32,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR32_SNORM,       GL_BGR,    int32,  double, 3, 12, true, true, true, false,    2, 1, 0, -1,     32, 32, 32,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_BGR16_FLOAT,   GL_BGR,    uint16, double, 3, 6,              true,           2, 1, 0, -1,      0,  0,  0,  0,   0, 0, 0, 0,   FP16, FP16, FP16, NON_FP);
FMT_INFO(PXFMT_BGR32_FLOAT,       GL_BGR,    float,  double, 3, 12, true, false, false, false,  2, 1, 0, -1,      0,  0,  0,  0,   0, 0, 0, 0);

// GL_RGBA
FMT_INFO(PXFMT_RGBA8_UNORM,       GL_RGBA,   uint32, double, 4, 4,  true, true, false, true,    0, 1, 2, 3,       8,  8,  8,  8,   0, 8, 16, 24);
FMT_INFO(PXFMT_RGBA8_SNORM,       GL_RGBA,   uint32, double, 4, 4,  true, true, true, true,     0, 1, 2, 3,       8,  8,  8,  8,   0, 8, 16, 24);
FMT_INFO(PXFMT_RGBA16_UNORM,      GL_RGBA,   uint16, double, 4, 8,  true, true, false, false,   0, 1, 2, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGBA16_SNORM,      GL_RGBA,   int16,  double, 4, 8,  true, true, true, false,    0, 1, 2, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGBA32_UNORM,      GL_RGBA,   uint32, double, 4, 16, true, true, false, false,   0, 1, 2, 3,      32, 32, 32, 32,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGBA32_SNORM,      GL_RGBA,   int32,  double, 4, 16, true, true, true, false,    0, 1, 2, 3,      32, 32, 32, 32,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_RGBA16_FLOAT,  GL_RGBA,   uint16, double, 4, 8,          true,               0, 1, 2, 3,       0,  0,  0,  0,   0, 0, 0, 0,   FP16, FP16, FP16, FP16);
FMT_INFO(PXFMT_RGBA32_FLOAT,      GL_RGBA,   float,  double, 4, 16, true, false, false, false,  0, 1, 2, 3,       0,  0,  0,  0,   0, 0, 0, 0);

FMT_INFO(PXFMT_RGBA4_UNORM,       GL_RGBA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       4,  4,  4,  4,   12, 8, 4, 0);
FMT_INFO(PXFMT_RGBA4REV_UNORM,    GL_RGBA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       4,  4,  4,  4,   0, 4, 8, 12);
FMT_INFO(PXFMT_RGB5A1_UNORM,      GL_RGBA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       5,  5,  5,  1,   11, 6, 1, 0);
FMT_INFO(PXFMT_A1RGB5_UNORM,      GL_RGBA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       5,  5,  5,  1,   0, 5, 10, 15);
FMT_INFO(PXFMT_RGBA8888_UNORM,    GL_RGBA,   uint32, double, 4, 4, true, true, false, true,     0, 1, 2, 3,       8,  8,  8,  8,   24, 16, 8, 0);
FMT_INFO(PXFMT_RGB10A2_UNORM,     GL_RGBA,   uint32, double, 4, 4, true, true, false, true,     0, 1, 2, 3,      10, 10, 10,  2,   22, 12, 2, 0);
FMT_INFO(PXFMT_A2RGB10_UNORM,     GL_RGBA,   uint32, double, 4, 4, true, true, false, true,     0, 1, 2, 3,      10, 10, 10,  2,   0, 10, 20, 30);

// GL_BGRA
FMT_INFO(PXFMT_BGRA8_UNORM,       GL_BGRA,   uint32, double, 4, 4,  true, true, false, true,    0, 1, 2, 3,       8,  8,  8,  8,   16, 8, 0, 24);
FMT_INFO(PXFMT_BGRA8_SNORM,       GL_BGRA,   uint32, double, 4, 4,  true, true, true, true,     0, 1, 2, 3,       8,  8,  8,  8,   16, 8, 0, 24);
FMT_INFO(PXFMT_BGRA16_UNORM,      GL_BGRA,   uint16, double, 4, 8,  true, true, false, false,   2, 1, 0, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGRA16_SNORM,      GL_BGRA,   int16,  double, 4, 8,  true, true, true, false,    2, 1, 0, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGRA32_UNORM,      GL_BGRA,   uint32, double, 4, 16, true, true, false, false,   2, 1, 0, 3,      32, 32, 32, 32,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGRA32_SNORM,      GL_BGRA,   int32,  double, 4, 16, true, true, true, false,    2, 1, 0, 3,      32, 32, 32, 32,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_BGRA16_FLOAT,  GL_BGRA,   uint16, double, 4, 8,              true,           2, 1, 0, 3,       0,  0,  0,  0,   0, 0, 0, 0,   FP16, FP16, FP16, FP16);
FMT_INFO(PXFMT_BGRA32_FLOAT,      GL_BGRA,   float,  double, 4, 16,  true, false, false, false, 2, 1, 0, 3,       0,  0,  0,  0,   0, 0, 0, 0);

FMT_INFO(PXFMT_BGRA4_UNORM,       GL_BGRA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       4,  4,  4,  4,   4, 8, 12, 0);
FMT_INFO(PXFMT_BGRA4REV_UNORM,    GL_BGRA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       4,  4,  4,  4,   0, 12, 8, 4);
FMT_INFO(PXFMT_BGR5A1_UNORM,      GL_BGRA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       5,  5,  5,  1,   1, 6, 11, 0);
FMT_INFO(PXFMT_A1BGR5_UNORM,      GL_BGRA,   uint16, double, 4, 2, true, true, false, true,     0, 1, 2, 3,       5,  5,  5,  1,   10, 5, 0, 15);
FMT_INFO(PXFMT_BGRA8888_UNORM,    GL_BGRA,   uint32, double, 4, 4, true, true, false, true,     0, 1, 2, 3,       8,  8,  8,  8,   24, 0, 8, 16);
FMT_INFO(PXFMT_BGR10A2_UNORM,     GL_BGRA,   uint32, double, 4, 4, true, true, false, true,     0, 1, 2, 3,      10, 10, 10,  2,   2, 12, 22, 0);
FMT_INFO(PXFMT_A2BGR10_UNORM,     GL_BGRA,   uint32, double, 4, 4, true, true, false, true,     0, 1, 2, 3,      10, 10, 10,  2,   20, 10, 0, 30);

// GL_LUMINANCE
FMT_INFO(PXFMT_L8_UNORM,      GL_LUMINANCE,  uint8,  double, 1, 1, true, true, false, false,    0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_L8_SNORM,      GL_LUMINANCE,  int8,   double, 1, 1, true, true, true, false,     0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_L16_UNORM,     GL_LUMINANCE,  uint16, double, 1, 2, true, true, false, false,    0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_L16_SNORM,     GL_LUMINANCE,  int16,  double, 1, 2, true, true, true, false,     0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_L32_UNORM,     GL_LUMINANCE,  uint32, double, 1, 4, true, true, false, false,    0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_L32_SNORM,     GL_LUMINANCE,  int32,  double, 1, 4, true, true, true, false,     0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_L16_FLOAT, GL_LUMINANCE,  uint16, double, 1, 2,             true,            0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   NON_FP, NON_FP, NON_FP, FP16);
FMT_INFO(PXFMT_L32_FLOAT,     GL_LUMINANCE,  float,  double, 1, 4, true, false, false, false,   0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_LUMINANCE_ALPHA
FMT_INFO(PXFMT_LA8_UNORM,     GL_LUMINANCE_ALPHA,  uint8,  double, 2, 2, true, true, false, false,    0,  3, -1, -1,    8,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_LA8_SNORM,     GL_LUMINANCE_ALPHA,  int8,   double, 2, 2, true, true, true, false,     0,  3, -1, -1,    8,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_LA16_UNORM,    GL_LUMINANCE_ALPHA,  uint16, double, 2, 4, true, true, false, false,    0,  3, -1, -1,   16, 16,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_LA16_SNORM,    GL_LUMINANCE_ALPHA,  int16,  double, 2, 4, true, true, true, false,     0,  3, -1, -1,   16, 16,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_LA32_UNORM,    GL_LUMINANCE_ALPHA,  uint32, double, 2, 8, true, true, false, false,    0,  3, -1, -1,   32, 32,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_LA32_SNORM,    GL_LUMINANCE_ALPHA,  int32,  double, 2, 8, true, true, true, false,     0,  3, -1, -1,   32, 32,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_LA16_FLOAT,GL_LUMINANCE_ALPHA,  uint16, double, 2, 4,             true,            0,  3, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   NON_FP, NON_FP, NON_FP, FP16);
FMT_INFO(PXFMT_LA32_FLOAT,    GL_LUMINANCE_ALPHA,  float,  double, 2, 8, true, false, false, false,   0,  3, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_RED_INTEGER
FMT_INFO(PXFMT_R8_UINT,     GL_RED_INTEGER,  uint8,  uint32, 1, 1, false, false, false, false,  0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R8_SINT,     GL_RED_INTEGER,  int8,   uint32, 1, 1, false, false, true, false,   0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R16_UINT,    GL_RED_INTEGER,  uint16, uint32, 1, 2, false, false, false, false,  0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R16_SINT,    GL_RED_INTEGER,  int16,  uint32, 1, 2, false, false, true, false,   0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R32_UINT,    GL_RED_INTEGER,  uint32, uint32, 1, 4, false, false, false, false,  0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_R32_SINT,    GL_RED_INTEGER,  int32,  uint32, 1, 4, false, false, true, false,   0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);

// GL_GREEN_INTEGER
FMT_INFO(PXFMT_G8_UINT,     GL_GREEN_INTEGER,uint8,  uint32, 1, 1, false, false, false, false,  1, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G8_SINT,     GL_GREEN_INTEGER,int8,   uint32, 1, 1, false, false, true, false,   1, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G16_UINT,    GL_GREEN_INTEGER,uint16, uint32, 1, 2, false, false, false, false,  1, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G16_SINT,    GL_GREEN_INTEGER,int16,  uint32, 1, 2, false, false, true, false,   1, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G32_UINT,    GL_GREEN_INTEGER,uint32, uint32, 1, 4, false, false, false, false,  1, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_G32_SINT,    GL_GREEN_INTEGER,int32,  uint32, 1, 4, false, false, true, false,   1, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);

// GL_BLUE_INTEGER
FMT_INFO(PXFMT_B8_UINT,     GL_BLUE_INTEGER, uint8,  uint32, 1, 1, false, false, false, false,  2, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B8_SINT,     GL_BLUE_INTEGER, int8,   uint32, 1, 1, false, false, true, false,   2, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B16_UINT,    GL_BLUE_INTEGER, uint16, uint32, 1, 2, false, false, false, false,  2, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B16_SINT,    GL_BLUE_INTEGER, int16,  uint32, 1, 2, false, false, true, false,   2, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B32_UINT,    GL_BLUE_INTEGER, uint32, uint32, 1, 4, false, false, false, false,  2, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_B32_SINT,    GL_BLUE_INTEGER, int32,  uint32, 1, 4, false, false, true, false,   2, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);

// GL_ALPHA_INTEGER
FMT_INFO(PXFMT_A8_UINT,     GL_ALPHA_INTEGER,uint8,  uint32, 1, 1, false, false, false, false,  3, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A8_SINT,     GL_ALPHA_INTEGER,int8,   uint32, 1, 1, false, false, true, false,   3, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A16_UINT,    GL_ALPHA_INTEGER,uint16, uint32, 1, 2, false, false, false, false,  3, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A16_SINT,    GL_ALPHA_INTEGER,int16,  uint32, 1, 2, false, false, true, false,   3, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A32_UINT,    GL_ALPHA_INTEGER,uint32, uint32, 1, 4, false, false, false, false,  3, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_A32_SINT,    GL_ALPHA_INTEGER,int32,  uint32, 1, 4, false, false, true, false,   3, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);

// GL_RG_INTEGER
FMT_INFO(PXFMT_RG8_UINT,    GL_RG_INTEGER,   uint8,  uint32, 2, 2, false, false, false, false,  0, 1, -1, -1,     8,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG8_SINT,    GL_RG_INTEGER,   int8,   uint32, 2, 2, false, false, true, false,   0, 1, -1, -1,     8,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG16_UINT,   GL_RG_INTEGER,   uint16, uint32, 2, 4, false, false, false, false,  0, 1, -1, -1,    16, 16,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG16_SINT,   GL_RG_INTEGER,   int16,  uint32, 2, 4, false, false, true, false,   0, 1, -1, -1,    16, 16,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG32_UINT,   GL_RG_INTEGER,   uint32, uint32, 2, 8, false, false, false, false,  0, 1, -1, -1,    32, 32,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RG32_SINT,   GL_RG_INTEGER,   int32,  uint32, 2, 8, false, false, true, false,   0, 1, -1, -1,    32, 32,  0,  0,   0, 0, 0, 0);

// GL_RGB_INTEGER
FMT_INFO(PXFMT_RGB8_UINT,   GL_RGB_INTEGER,  uint8,  uint32, 3, 3,  false, false, false, false, 0, 1, 2, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB8_SINT,   GL_RGB_INTEGER,  int8,   uint32, 3, 3,  false, false, true, false,  0, 1, 2, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB16_UINT,  GL_RGB_INTEGER,  uint16, uint32, 3, 6,  false, false, false, false, 0, 1, 2, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB16_SINT,  GL_RGB_INTEGER,  int16,  uint32, 3, 6,  false, false, true, false,  0, 1, 2, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB32_UINT,  GL_RGB_INTEGER,  uint32, uint32, 3, 12, false, false, false, false, 0, 1, 2, -1,     32, 32, 32,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGB32_SINT,  GL_RGB_INTEGER,  int32,  uint32, 3, 12, false, false, true, false,  0, 1, 2, -1,     32, 32, 32,  0,   0, 0, 0, 0);

FMT_INFO(PXFMT_RGB332_UINT, GL_RGB_INTEGER,  uint8,  uint32, 3, 1,  false, false, false, true,  0, 1, 2, -1,      3,  3,  2,  0,   5, 2, 0, 0);
FMT_INFO(PXFMT_RGB233_UINT, GL_RGB_INTEGER,  uint8,  uint32, 3, 1,  false, false, false, true,  0, 1, 2, -1,      3,  3,  2,  0,   0, 3, 6, 0);
FMT_INFO(PXFMT_RGB565_UINT, GL_RGB_INTEGER,  uint8,  uint32, 3, 2,  false, false, false, true,  0, 1, 2, -1,      5,  6,  5,  0,   11, 5, 0, 0);
FMT_INFO(PXFMT_RGB565REV_UINT,GL_RGB_INTEGER,uint8, uint32, 3, 2,  false, false, false, true,   0, 1, 2, -1,      5,  6,  5,  0,   0, 5, 11, 0);

// GL_BGR_INTEGER
FMT_INFO(PXFMT_BGR8_UINT,   GL_BGR_INTEGER,  uint8,  uint32, 3, 3,  false, false, false, false, 2, 1, 0, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR8_SINT,   GL_BGR_INTEGER,  int8,   uint32, 3, 3,  false, false, true, false,  2, 1, 0, -1,      8,  8,  8,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR16_UINT,  GL_BGR_INTEGER,  uint16, uint32, 3, 6,  false, false, false, false, 2, 1, 0, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR16_SINT,  GL_BGR_INTEGER,  int16,  uint32, 3, 6,  false, false, true, false,  2, 1, 0, -1,     16, 16, 16,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR32_UINT,  GL_BGR_INTEGER,  uint32, uint32, 3, 12, false, false, false, false, 2, 1, 0, -1,     32, 32, 32,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGR32_SINT,  GL_BGR_INTEGER,  int32,  uint32, 3, 12, false, false, true, false,  2, 1, 0, -1,     32, 32, 32,  0,   0, 0, 0, 0);

// GL_RGBA_INTEGER
FMT_INFO(PXFMT_RGBA8_UINT,  GL_RGBA_INTEGER, uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,       8,  8,  8,  8,   0, 8, 16, 24);
FMT_INFO(PXFMT_RGBA8_SINT,  GL_RGBA_INTEGER, uint32, uint32, 4, 4,  false, false, true, true,   0, 1, 2, 3,       8,  8,  8,  8,   0, 8, 16, 24);
FMT_INFO(PXFMT_RGBA16_UINT, GL_RGBA_INTEGER, uint16, uint32, 4, 8,  false, false, false, false, 0, 1, 2, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGBA16_SINT, GL_RGBA_INTEGER, int16,  uint32, 4, 8,  false, false, true, false,  0, 1, 2, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGBA32_UINT, GL_RGBA_INTEGER, uint32, uint32, 4, 16, false, false, false, false, 0, 1, 2, 3,      32, 32, 32, 32,   0, 0, 0, 0);
FMT_INFO(PXFMT_RGBA32_SINT, GL_RGBA_INTEGER, int32,  uint32, 4, 16, false, false, true, false,  0, 1, 2, 3,      32, 32, 32, 32,   0, 0, 0, 0);

FMT_INFO(PXFMT_RGBA4_UINT,  GL_RGBA_INTEGER, uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       4,  4,  4,  4,   12, 8, 4, 0);
FMT_INFO(PXFMT_RGBA4REV_UINT,GL_RGBA_INTEGER,uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       4,  4,  4,  4,   0, 4, 8, 12);
FMT_INFO(PXFMT_RGB5A1_UINT, GL_RGBA_INTEGER, uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       5,  5,  5,  1,   11, 6, 1, 0);
FMT_INFO(PXFMT_A1RGB5_UINT, GL_RGBA_INTEGER, uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       5,  5,  5,  1,   0, 5, 10, 15);
FMT_INFO(PXFMT_RGBA8REV_UINT,GL_RGBA_INTEGER,uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,       8,  8,  8,  8,   24, 16, 8, 0);
FMT_INFO(PXFMT_RGB10A2_UINT,GL_RGBA_INTEGER, uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,      10, 10, 10,  2,   22, 12, 2, 0);
FMT_INFO(PXFMT_A2RGB10_UINT,GL_RGBA_INTEGER, uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,      10, 10, 10,  2,   0, 10, 20, 30);

// GL_BGRA_INTEGER
FMT_INFO(PXFMT_BGRA8_UINT,  GL_BGRA_INTEGER, uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,       8,  8,  8,  8,   16, 8, 0, 24);
FMT_INFO(PXFMT_BGRA8_SINT,  GL_BGRA_INTEGER, uint32, uint32, 4, 4,  false, false, true, true,   0, 1, 2, 3,       8,  8,  8,  8,   16, 8, 0, 24);
FMT_INFO(PXFMT_BGRA16_UINT, GL_BGRA_INTEGER, uint16, uint32, 4, 8,  false, false, false, false, 2, 1, 0, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGRA16_SINT, GL_BGRA_INTEGER, int16,  uint32, 4, 8,  false, false, true, false,  2, 1, 0, 3,      16, 16, 16, 16,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGRA32_UINT, GL_BGRA_INTEGER, uint32, uint32, 4, 16, false, false, false, false, 2, 1, 0, 3,      32, 32, 32, 32,   0, 0, 0, 0);
FMT_INFO(PXFMT_BGRA32_SINT, GL_BGRA_INTEGER, int32,  uint32, 4, 16, false, false, true, false,  2, 1, 0, 3,      32, 32, 32, 32,   0, 0, 0, 0);

FMT_INFO(PXFMT_BGRA4_UINT,  GL_BGRA_INTEGER, uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       4,  4,  4,  4,   4, 8, 12, 0);
FMT_INFO(PXFMT_BGRA4REV_UINT,GL_BGRA_INTEGER,uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       4,  4,  4,  4,   0, 12, 8, 4);
FMT_INFO(PXFMT_BGR5A1_UINT, GL_BGRA_INTEGER, uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       5,  5,  5,  1,   1, 6, 11, 0);
FMT_INFO(PXFMT_A1BGR5_UINT, GL_BGRA_INTEGER, uint16, uint32, 4, 2,  false, false, false, true,  0, 1, 2, 3,       5,  5,  5,  1,   10, 5, 0, 15);
FMT_INFO(PXFMT_BGRA8REV_UINT,GL_BGRA_INTEGER,uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,       8,  8,  8,  8,   24, 0, 8, 16);
FMT_INFO(PXFMT_BGR10A2_UINT,GL_BGRA_INTEGER, uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,      10, 10, 10,  2,   2, 12, 22, 0);
FMT_INFO(PXFMT_A2BGR10_UINT,GL_BGRA_INTEGER, uint32, uint32, 4, 4,  false, false, false, true,  0, 1, 2, 3,      10, 10, 10,  2,   20, 10, 0, 30);

// GL_DEPTH_COMPONENT
FMT_INFO(PXFMT_D8_UNORM, GL_DEPTH_COMPONENT, uint8,  double, 1, 1, true, true, false, false,    0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_D8_SNORM, GL_DEPTH_COMPONENT, int8,   double, 1, 1, true, true, true, false,     0, -1, -1, -1,    8,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_D16_UNORM,GL_DEPTH_COMPONENT, uint16, double, 1, 2, true, true, false, false,    0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_D16_SNORM,GL_DEPTH_COMPONENT, int16,  double, 1, 2, true, true, true, false,     0, -1, -1, -1,   16,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_D32_UNORM,GL_DEPTH_COMPONENT, uint32, double, 1, 4, true, true, false, false,    0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_D32_SNORM,GL_DEPTH_COMPONENT, int32,  double, 1, 4, true, true, true, false,     0, -1, -1, -1,   32,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_D16_FLOAT,GL_DEPTH_COMPONENT,uint16,double,1,2,             true,            0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   FP16, NON_FP, NON_FP, NON_FP);
FMT_INFO(PXFMT_D32_FLOAT,GL_DEPTH_COMPONENT, float,  double, 1, 4, true, false, false, false,   0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_STENCIL_INDEX
FMT_INFO(PXFMT_S8_UINT,  GL_STENCIL_INDEX,   uint8,  uint32, 1, 1, false, false, false, false,  0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_S8_SINT,  GL_STENCIL_INDEX,   int8,   uint32, 1, 1, false, false, true, false,   0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_S16_UINT, GL_STENCIL_INDEX,   uint16, uint32, 1, 2, false, false, false, false,  0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_S16_SINT, GL_STENCIL_INDEX,   int16,  uint32, 1, 2, false, false, true, false,   0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_S32_UINT, GL_STENCIL_INDEX,   uint32, uint32, 1, 4, false, false, false, false,  0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_S32_SINT, GL_STENCIL_INDEX,   int32,  uint32, 1, 4, false, false, true, false,   0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);
FMT_INFO_SFP(PXFMT_S16_FLOAT,GL_STENCIL_INDEX,uint16,uint32, 1, 2,               true,          0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0,   FP16, NON_FP, NON_FP, NON_FP);
FMT_INFO(PXFMT_S32_FLOAT,GL_STENCIL_INDEX,   float,  uint32, 1, 4, false, false, false, false,  0, -1, -1, -1,    0,  0,  0,  0,   0, 0, 0, 0);

// GL_DEPTH_STENCIL - Note: These require special treatment; as a result not all of the values look correct:
FMT_INFO(PXFMT_D24_UNORM_S8_UINT,GL_DEPTH_STENCIL,uint32,double,2,4,true, false, false, false,  0, 1, -1, -1,    24,  8,  0,  0,   0, 0, 0, 0);
FMT_INFO(PXFMT_D32_FLOAT_S8_UINT,GL_DEPTH_STENCIL,float, double,2,8,true, false, false, false,  0, 1, -1, -1,     0,  8,  0,  0,   0, 0, 0, 0);

// FXT1 compressed texture internalformats
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGB_FXT1,    GL_RGB, 3, false, 8, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGBA_FXT1,  GL_RGBA, 4, false, 8, 4, 16);

// S3TC/DXT compressed texture internalformats
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGB_DXT1,    GL_RGB, 3, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGBA_DXT1,                      GL_RGBA, 4, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB_DXT1,                      GL_RGB, 3, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB_ALPHA_DXT1,                GL_RGBA, 4, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGBA_DXT3,                      GL_RGBA, 4, false, 4, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB_ALPHA_DXT3,                GL_RGBA, 4, false, 4, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGBA_DXT5,                      GL_RGBA, 4, false, 4, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB_ALPHA_DXT5,                GL_RGBA, 4, false, 4, 4, 16);

// ETC2 compressed texture internalformats
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGB8_ETC2,                      GL_RGB,  3, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB8_ETC2,                     GL_RGB,  3, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,  GL_RGBA, 4, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_RGBA, 4, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RGBA8_ETC2_EAC,                 GL_RGBA, 4, false, 4, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,          GL_RGBA, 4, false, 4, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_R11_EAC,                        GL_RGB,  1, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SIGNED_R11_EAC,                 GL_RGB,  1, false, 4, 4, 8);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_RG11_EAC,                       GL_RGB,  2, false, 4, 4, 16);
FMT_INFO_COMPRESSED(PXFMT_COMPRESSED_SIGNED_RG11_EAC,                GL_RGB,  2, false, 4, 4, 16);



/******************************************************************************
 *
 * NOTE: The same pattern repeats itself several times (starting here):
 *
 * 1) A templatized function
 *
 * 2) A function that converts from a run-time pxfmt_sized_format to a
 * compile-time templatized function for the specific pxfmt_sized_format.
 *
 ******************************************************************************/

// This templatized function determines the per-pixel and per-row stride for
// the given pxfmt_sized_format and width.
template <pxfmt_sized_format F>
inline
void get_pxfmt_info(const uint32 width, uint32 &pixel_stride,
                    uint32 &row_stride, bool &needs_fp_intermediate)
{
    pixel_stride = pxfmt_per_fmt_info<F>::m_bytes_per_pixel;
    row_stride = ((pixel_stride * width) + 3) & 0xFFFFFFFC;
    needs_fp_intermediate = pxfmt_per_fmt_info<F>::m_needs_fp_intermediate;
}

inline
void get_pxfmt_info(const uint32 width, uint32 &pixel_stride,
                    uint32 &row_stride, bool &needs_fp_intermediate,
                    const pxfmt_sized_format fmt)
{
#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt)                                             \
    case fmt:                                                           \
        get_pxfmt_info<fmt>(width, pixel_stride, row_stride,            \
                             needs_fp_intermediate);                    \
        break;

    switch (fmt)
    {
#include "pxfmt_case_statements.inl"
        case PXFMT_INVALID: break;
    }
}

inline
uint32 round_to_block_size(uint32 width, uint32 block_size)
{
    assert(block_size > 0);
    // The idea is that if width isn't a multiple of block_size, we should be
    // able to round-up to the next multiple.  The modulo operator (i.e. "%",
    // or the remainder of an integer division) will tell us if width is a
    // multiple (i.e. it will be zero), and if not, it will help us know how
    // much to add to width.
    uint32 remainder = width % block_size;
    if (remainder == 0)
    {
        return width;
    }
    uint32 scaled_width = (width + (block_size - remainder));
    return scaled_width;
}

// This templatized function determines the per-block and per-row stride for
// the given compressed-texture pxfmt_sized_format and width.
template <pxfmt_sized_format F>
inline
void get_compression_block_info(const uint32 width,
                                uint32 &block_width,
                                uint32 &block_height,
                                uint32 &block_perblock_stride,
                                uint32 &block_perrow_stride)
{
    assert(pxfmt_per_fmt_info<F>::m_is_compressed == true);

    block_width = pxfmt_per_fmt_info<F>::m_block_width;
    block_height = pxfmt_per_fmt_info<F>::m_block_height;

    block_perblock_stride = pxfmt_per_fmt_info<F>::m_block_size;
    // Just in case "width" isn't a multiple of "block_perblock_stride",
    // potentially scale it up to a multiple of "block_perblock_stride":
    uint32 scaled_width = round_to_block_size(width, block_width);
    // The per-row stride is a pixel stride
    block_perrow_stride = scaled_width;
}

inline
void get_compression_block_info(const uint32 width,
                                uint32 &block_width,
                                uint32 &block_height,
                                uint32 &block_perblock_stride,
                                uint32 &block_perrow_stride,
                                const pxfmt_sized_format fmt)
{
#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt)                                             \
    case fmt:                                                           \
        get_compression_block_info<fmt>(width,                          \
                                        block_width, block_height,      \
                                        block_perblock_stride,          \
                                        block_perrow_stride);           \
        break;

    switch (fmt)
    {
#include "pxfmt_case_statements.inl"
        case PXFMT_INVALID: break;
    }
}


/******************************************************************************
 *
 * The following are conversion functions for going from a pxfmt_sized_format
 * to an intermediate format (either double or uint32):
 *
 ******************************************************************************/

// This function sets the four components to default values of [0, 0, 0, 1]:
template <pxfmt_sized_format F>
inline
void set_intermediate_to_defaults(void *intermediate)
{
    // Point to the intermediate pixel values with the correct type:
    typename pxfmt_per_fmt_info<F>::m_intermediate_type *dst =
        (typename pxfmt_per_fmt_info<F>::m_intermediate_type *) intermediate;
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 0;
    dst[3] = 1;
}


// This function converts one component from a packed-integer value, (either
// integer or normalized-fixed-point) to an intermediate value (either a uint32
// or a double):
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_comp_packed(Tint *dst, const Tsrc *src, const uint32 c)
{
    // Note: local variables are used in these functions in order to improve
    // readability and debug-ability.  The compiler should optimize things
    // appropriately.
    uint32 raw = (uint32) *src;
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    uint32 mask = pxfmt_per_fmt_info<F>::m_mask[c];
    uint32 shift = pxfmt_per_fmt_info<F>::m_shift[c];
    if (pxfmt_per_fmt_info<F>::m_is_normalized)
    {
        uint32 max = pxfmt_per_fmt_info<F>::m_max[c];

        raw = ((raw & mask) >> shift);
        dst[index] = (Tint) ((double) raw / (double) max);
        if (pxfmt_per_fmt_info<F>::m_is_signed)
        {
            dst[index] = (Tint) std::max<double>(dst[index], -1.0);
        }
    }
    else
    {
        dst[index] = ((raw & mask) >> shift);
    }
}


// This function converts one component from a normalized-fixed-point value to
// an intermediate value, which is a double:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_comp_norm_unpacked(Tint *dst, const Tsrc *src, const uint32 c)
{
    uint32 raw = (uint32) src[c];
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    uint32 max = pxfmt_per_fmt_info<F>::m_max[c];
    dst[index] = (Tint) ((double) raw / (double) max);
    if (pxfmt_per_fmt_info<F>::m_is_signed)
    {
        dst[index] = (Tint) std::max<double>(dst[index], -1.0);
    }
}


// This function performs a "copy conversion" (e.g. from a floating-point value
// to a double, or from an integer to a uint32):
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_comp_copy(Tint *dst, const Tsrc *src, const uint32 c)
{
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    dst[index] = (Tint) src[c];
}


// The following macros are used for conversion to/from a pixel with a format
// of GL_RGB and a type of GL_UNSIGNED_INT_5_9_9_9_REV, where N is 9, B is 15,
// and E is 31:
#define RGB9E5_N 9
#define RGB9E5_B 15
#define RGB9E5_B1 (15 + 1)
#define RGB9E5_ADJUST (-(RGB9E5_B) -(RGB9E5_N))
// Use the following function (instead of macro) to reduce code size (below):
double POW2(double x) {return pow(2.0, (double) x); }


// This special function converts all components of a source pixel to an
// intermediate format for a format of GL_RGB and a type of
// GL_UNSIGNED_INT_5_9_9_9_REV:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_5999(Tint *dst, const Tsrc *src)
{
    uint32 raw = (uint32) *src;

    // Read and calculate the exponent:
    uint32 mask = pxfmt_per_fmt_info<F>::m_mask[3];
    uint32 shift = pxfmt_per_fmt_info<F>::m_shift[3];
// FIXME: USE MACROS FOR THE FOLLOWING CONSTANTS
    int32 exp = ((raw & mask) >> shift) + RGB9E5_ADJUST;
    double multiplier = POW2(exp);

    // Read and calculate the RGB values:
    for (uint32 c = 0 ; c < pxfmt_per_fmt_info<F>::m_num_components; c++)
    {
        mask = pxfmt_per_fmt_info<F>::m_mask[c];
        shift = pxfmt_per_fmt_info<F>::m_shift[c];
        uint32 component = (raw & mask) >> shift;
        dst[c] = (Tint) ((double) component * multiplier);
    }
}


// This special function converts a GL_LUMINANCE source pixel to an
// intermediate value, which is a double:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_L(Tint *dst, const Tsrc *src)
{
    if (pxfmt_per_fmt_info<F>::m_is_normalized)
    {
        uint32 raw = (uint32) *src;
        uint32 max = pxfmt_per_fmt_info<F>::m_max[0];
        Tint dst_value = (Tint) ((double) raw / (double) max);
        if (pxfmt_per_fmt_info<F>::m_is_signed)
        {
            dst[0] = dst[1] = dst[2] = (Tint) std::max<double>(dst_value, -1.0);
        }
        else
        {
            dst[0] = dst[1] = dst[2] = dst_value;
        }
    }
    else
    {
        dst[0] = dst[1] = dst[2] = (Tint) *src;
    }
}


// This special function converts a GL_LUMINANCE_ALPHA source pixel to an
// intermediate value, which is a double:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_LA(Tint *dst, const Tsrc *src)
{
    if (pxfmt_per_fmt_info<F>::m_is_normalized)
    {
        uint32 rawL = (uint32) src[0];
        uint32 rawA = (uint32) src[1];
        uint32 maxL = pxfmt_per_fmt_info<F>::m_max[0];
        uint32 maxA = pxfmt_per_fmt_info<F>::m_max[1];
        Tint dst_L_value = (Tint) ((double) rawL / (double) maxL);
        Tint dst_A_value;
        // The following compile-time "if" check is to avoid compile-time warnings
        // about "potential divide by 0" errors:
        if (pxfmt_per_fmt_info<F>::m_max[1] != 0)
        {
            dst_A_value = (Tint) ((double) rawA / (double) maxA);
        }
        else
        {
            dst_A_value = (Tint) 0;
        }
        if (pxfmt_per_fmt_info<F>::m_is_signed)
        {
            dst[0] = dst[1] = dst[2] = (Tint) std::max<double>(dst_L_value, -1.0);
            dst[3] = (Tint) std::max<double>(dst_A_value, -1.0);
        }
        else
        {
            dst[0] = dst[1] = dst[2] = dst_L_value;
            dst[3] = dst_A_value;
        }
    }
    else
    {
        dst[0] = dst[1] = dst[2] = (Tint) src[0];
        dst[3] = (Tint) src[1];
    }
}


// This special function converts all components of a source pixel to an
// intermediate format for a format of GL_DEPTH_STENCIL and a type of
// GL_UNSIGNED_INT_24_8:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_d24_unorm_s8_uint(Tint *dst, const Tsrc *src)
{
    // Convert the depth value:
    uint32 depth = (uint32) *src;
    double *pDepthDst = (double *) &dst[0];
    // The following compile-time "if" check is to avoid compile-time warnings
    // about "potential divide by 0" errors:
    if (pxfmt_per_fmt_info<F>::m_max[0] != 0)
    {
      *pDepthDst = (((double) ((depth & pxfmt_per_fmt_info<F>::m_mask[0]) >>
                               pxfmt_per_fmt_info<F>::m_shift[0])) /
                    ((double) pxfmt_per_fmt_info<F>::m_max[0]));
    }

    // Convert the stencil value:
    uint32 stencil = (uint32) *src;
    uint32 *pStencilDst = (uint32 *) &dst[1];
    *pStencilDst = ((stencil & pxfmt_per_fmt_info<F>::m_mask[1]) >>
                    pxfmt_per_fmt_info<F>::m_shift[1]);
}


// This special function converts all components of a source pixel to an
// intermediate format for a format of GL_DEPTH_STENCIL and a type of
// GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_d32_float_s8_uint(Tint *dst, const Tsrc *src)
{
    // Convert the depth value:
    to_int_comp_copy<F>(dst, src, 0);

    // Convert the stencil value:
    uint32 *pStencilSrc = (uint32 *) &src[1];
    uint32 *pStencilDst = (uint32 *) &dst[1];
    *pStencilDst = ((*pStencilSrc & pxfmt_per_fmt_info<F>::m_mask[1]) >>
                    pxfmt_per_fmt_info<F>::m_shift[1]);
}


// This function converts one component from a "small floating-point" value
// (i.e. 10, 11, or 16 bits) to an intermediate value, which is a double:
template <pxfmt_sized_format F, pxfmt_small_fp S, typename Tint, typename Tsrc>
inline
void to_int_comp_small_fp(Tint *dst, const Tsrc *src, const uint32 c)
{
    uint16 raw = (uint16) src[c];
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    // Only 16-bit floats have sign bits (i.e. 10/11-bit floats don't):
    uint32 sign_bit = ((pxfmt_per_fmt_info<F>::m_is_signed) ?
                       (raw >> small_fp<S>::m_sign_shift) : 0);
    // Get the exponent from the small-floating-point number, and format it for
    // the intermediate number:
    int32 exponent = (((raw & small_fp<S>::m_exp_mask) >>
                        small_fp<S>::m_exp_shift) - small_fp<S>::m_bias);
    if (exponent == 16)
    {
        // Adjust the exponent for zero:
        exponent = 1024;
    }
    if (exponent == -15)
    {
        // Adjust the exponent for zero:
        exponent = -1023;
    }
    exponent = (((exponent + 1023) << 20) & 0x7FF00000);
    // Get the mantissa from the small-floating-point number, and then shift it
    // into place for the intermediate number.  All 10/11/16-bit floats have
    // mantissa's that are smaller than that of the top-part of a double's
    // mantissa.  Hence, we need to shift the mantissa into place:
    uint32 mantissa = ((raw & small_fp<S>::m_man_mask) <<
                       small_fp<S>::m_man_shift);

    // Hand-craft a double:
    double_conversion d;
    d.d = 0;
    // Note: the bottom 32-bits of the mantissa will always be zero, because a
    // small-floating-point value is too small to ever populate it.
    d.i.u[1] = (sign_bit << 31) | exponent | mantissa;

    // Put the hand-crafted double into the destination:
    dst[index] = (Tint) d.d;
}


// This function converts a run-time call to a compile-time call to a function
// that converts one component from a "small floating-point" value
// (i.e. 10, 11, or 16 bits) to an intermediate value, which is a double:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_comp_small_fp(Tint *dst, const Tsrc *src, const uint32 c)
{
#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt, S)                                          \
    case S:                                                             \
        to_int_comp_small_fp<fmt,S>(dst, src, c);                       \
        break;

    switch (pxfmt_per_fmt_info<F>::m_small_fp[c])
    {
        CASE_STATEMENT(F, NON_FP);
        CASE_STATEMENT(F, FP10);
        CASE_STATEMENT(F, FP11);
        CASE_STATEMENT(F, FP16);
    }
}


// This special function converts all components of a 10F/11F/11F
// packed-integer source pixel to an intermediate format for a format of GL_RGB
// and a type of GL_UNSIGNED_INT_10F_11F_11F_REV:
template <pxfmt_sized_format F, typename Tint, typename Tsrc>
inline
void to_int_10f_11f_11f_packed(Tint *dst, const Tsrc *src)
{
    uint32 raw = (uint32) *src;
    uint16 srcBits[3];

    // Unpack the three components into an array of source values:
    srcBits[0] = (raw & 0x000003FF);
    srcBits[1] = ((raw & 0x001FFC00) >> 10);
    srcBits[2] = ((raw & 0xFFE00000) >> 21);

    // Process the unpacked source values:
    to_int_comp_small_fp<F>(dst, srcBits, 0);
    to_int_comp_small_fp<F>(dst, srcBits, 1);
    to_int_comp_small_fp<F>(dst, srcBits, 2);
}


// This function converts all components of a source pixel to an intermediate
// format:
template <pxfmt_sized_format F>
inline
void to_intermediate(void *intermediate, const void *pSrc)
{
    // Point to the source pixel with the correct type:
    typename pxfmt_per_fmt_info<F>::m_formatted_type *src =
        (typename pxfmt_per_fmt_info<F>::m_formatted_type *) pSrc;

    // Point to the intermediate pixel values with the correct type:
    typename pxfmt_per_fmt_info<F>::m_intermediate_type *dst =
        (typename pxfmt_per_fmt_info<F>::m_intermediate_type *) intermediate;

    // Initialize the intermediate pixel values to the correct default values,
    // of the correct type:
    set_intermediate_to_defaults<F>(intermediate);

    // Note: The compiler should remove most of the code below, during
    // optimization, because (at compile time) it can be determined that it is
    // not needed for the given pxfmt_sized_format:

    if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_RGB5999_REV)
    {
        to_int_5999<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_format == GL_LUMINANCE)
    {
        to_int_L<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_format == GL_LUMINANCE_ALPHA)
    {
        to_int_LA<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_D24_UNORM_S8_UINT)
    {
        to_int_d24_unorm_s8_uint<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_D32_FLOAT_S8_UINT)
    {
        to_int_d32_float_s8_uint<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_RGB10F_11F_11F)
    {
        to_int_10f_11f_11f_packed<F>(dst, src);
    }
    else
    {
        // Convert this pixel to the intermediate, one component at a time:
        for (uint32 c = 0 ; c < pxfmt_per_fmt_info<F>::m_num_components; c++)
        {
            if (pxfmt_per_fmt_info<F>::m_index[c] >= 0)
            {
                if (pxfmt_per_fmt_info<F>::m_small_fp[0] != NON_FP)
                {
                    to_int_comp_small_fp<F>(dst, src, c);
                }
                else if (pxfmt_per_fmt_info<F>::m_is_packed)
                {
                    to_int_comp_packed<F>(dst, src, c);
                }
                else
                {
                    if (pxfmt_per_fmt_info<F>::m_is_normalized)
                    {
                        to_int_comp_norm_unpacked<F>(dst, src, c);
                    }
                    else
                    {
                        to_int_comp_copy<F>(dst, src, c);
                    }
                }
            }
        }
    }
}


// This function converts a run-time call to a compile-time call to a function
// that converts all components of a source pixel to an intermediate format:
inline
void to_intermediate(void *intermediate, const void *pSrc,
                     const pxfmt_sized_format src_fmt)
{
#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt)                                             \
    case fmt:                                                           \
        to_intermediate<fmt>(intermediate, pSrc);                       \
        break;

    switch (src_fmt)
    {
#include "pxfmt_case_statements.inl"
        case PXFMT_INVALID: break;
    }
}



/******************************************************************************
 *
 * The following are conversion functions for going from an intermediate format
 * (either double or uint32) to a pxfmt_sized_format:
 *
 ******************************************************************************/

// This function converts all components from an intermediate format (either
// uint32 or double) to a packed-integer (either with integer or
// normalized-fixed-point):
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_comp_packed(Tdst *dst, const Tint *src)
{
    Tint red =   ((pxfmt_per_fmt_info<F>::m_is_normalized) ?
                  (src[0] * pxfmt_per_fmt_info<F>::m_max[0]) : src[0]);
    Tint green = ((pxfmt_per_fmt_info<F>::m_is_normalized) ?
                  (src[1] * pxfmt_per_fmt_info<F>::m_max[1]) : src[1]);
    Tint blue =  ((pxfmt_per_fmt_info<F>::m_is_normalized) ?
                  (src[2] * pxfmt_per_fmt_info<F>::m_max[2]) : src[2]);
    Tint alpha = ((pxfmt_per_fmt_info<F>::m_is_normalized) ?
                  (src[3] * pxfmt_per_fmt_info<F>::m_max[3]) : src[3]);

    *dst = (Tdst) ((((uint32) red << pxfmt_per_fmt_info<F>::m_shift[0]) &
                    pxfmt_per_fmt_info<F>::m_mask[0]) |
                   (((uint32) green << pxfmt_per_fmt_info<F>::m_shift[1]) &
                    pxfmt_per_fmt_info<F>::m_mask[1]) |
                   (((uint32) blue << pxfmt_per_fmt_info<F>::m_shift[2]) &
                    pxfmt_per_fmt_info<F>::m_mask[2]) |
                   (((uint32) alpha << pxfmt_per_fmt_info<F>::m_shift[3]) &
                    pxfmt_per_fmt_info<F>::m_mask[3]));
}


// This function converts one component from an intermediate format (double)
// to a normalized-fixed-point value:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_comp_norm_unpacked(Tdst *dst, const Tint *src, const uint32 c)
{
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    uint32 max = pxfmt_per_fmt_info<F>::m_max[c];

    dst[c] = (Tdst) ((double) src[index] * (double) max);
}


// This function performs a "copy conversion" (e.g. from a floating-point value
// to a double, or from an integer to a uint32):
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_comp_copy(Tdst *dst, const Tint *src, const uint32 c)
{
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    dst[c] = (Tdst) src[index];
}


// This special function converts all components of a source pixel from an
// intermediate format for a format of GL_RGB and a type of
// GL_UNSIGNED_INT_5_9_9_9_REV:
//
// Part of this function must calculate log2(x).  The log2(x) function is
// relatively new in C++, and is not available with older C++ implementations
// (e.g. it's not available with VisualStudio 2010).  However, this can be
// calculated using the exponent of a C++ double, using an algorithm described
// at: http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogFloat.
// The implementation below doesn't take into account subnormal numbers
// (i.e. numbers very close to zero).  That is okay for our purposes.  We will
// also take advantage of the fact that the number has been clamped such that
// it is a positive number (i.e. the sign bit is always zero, and thus we don't
// need to mask the value by 0x7FF00000).
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_5999(Tdst *dst, const Tint *src)
{
    // Note: the following is the result of calculating the following (per the
    // OpenGL specification), where N is 9, B is 15, and E is 31:
    //     (((POW2(N) - 1) / POW2(N)) * POW2(E - B))
    double max_val = 65408.0;
    // Per the OpenGL spec, clamp the starting red, green and blue values:
    double redc =
        (double) std::max<double>(0, std::min<double>(src[0], max_val));
    double greenc =
        (double) std::max<double>(0, std::min<double>(src[1], max_val));
    double bluec =
        (double) std::max<double>(0, std::min<double>(src[2], max_val));

    // Determine the largest value of the 3:
    double_conversion largest;
    largest.d =
        (double) std::max<double>(redc, std::max<double>(greenc, bluec));

    // Calculate a preliminary shared exponent:
    int32 log2_largest = (largest.i.u[1] >> 20) - 1023;
    int32 prelim_exp = std::max<int32>((-RGB9E5_B1), log2_largest) + RGB9E5_B1;

    // Calculate a refined shared exponent:
    double divisor = POW2(prelim_exp + RGB9E5_ADJUST) + 0.5;
    uint32 maxs = (uint32) floor(largest.d / divisor);
    int32 shared_exp = (maxs == (1 << RGB9E5_N)) ? prelim_exp + 1 : prelim_exp;

    // Calculate integer values for red, green, and blue, using the shared
    // exponent:
    divisor = POW2(shared_exp + RGB9E5_ADJUST) + 0.5;
    uint32 red   = (uint32) floor(redc   / divisor);
    uint32 green = (uint32) floor(greenc / divisor);
    uint32 blue  = (uint32) floor(bluec  / divisor);

    *dst = (Tdst) ((((uint32) red << pxfmt_per_fmt_info<F>::m_shift[0]) &
                    pxfmt_per_fmt_info<F>::m_mask[0]) |
                   (((uint32) green << pxfmt_per_fmt_info<F>::m_shift[1]) &
                    pxfmt_per_fmt_info<F>::m_mask[1]) |
                   (((uint32) blue << pxfmt_per_fmt_info<F>::m_shift[2]) &
                    pxfmt_per_fmt_info<F>::m_mask[2]) |
                   (((uint32) shared_exp << pxfmt_per_fmt_info<F>::m_shift[3]) &
                    pxfmt_per_fmt_info<F>::m_mask[3]));
}


// This special function converts a GL_LUMINANCE pixel from an intermediate
// format (double) to either a floating-point or normalized-fixed-point value:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_L(Tdst *dst, const Tint *src)
{
    // Note: We have a bit of a problem to address.  At this point, we've lost
    // knowledge of what the pxfmt_sized_format is for the original source.  If
    // it is also GL_LUMINANCE, the code below is correct (because the original
    // luminance value was copied to the red, green, and blue components of the
    // intermediate value).  However, if the source was another color format
    // (e.g. GL_RGBA), then we are simply taking the red intermediate value.
    // It is doubtful that anybody would want to do such a thing.  It's also
    // not clear what should be done.  The OpenGL spec for glReadPixels() (as
    // opposed to for glGetTexImage()) says to compute the luminance value by
    // summing the red, green, and blue values (which can saturate to/past
    // 1.0).  Is that desired?  We'll go with this approach for now, which
    // seems to handle the likely cases.
    if (pxfmt_per_fmt_info<F>::m_is_normalized)
    {
        uint32 max = pxfmt_per_fmt_info<F>::m_max[0];
        dst[0] = (Tdst) ((double) src[0] * (double) max);
    }
    else
    {
        dst[0] = (Tdst) src[0];
    }
}


// This special function converts a GL_LUMINANCE_ALPHA pixel from an
// intermediate format (double) to either a floating-point or
// normalized-fixed-point value:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_LA(Tdst *dst, const Tint *src)
{
    // Note: See the problem discussion in the from_int_L() function (above).
    if (pxfmt_per_fmt_info<F>::m_is_normalized)
    {
        uint32 maxL = pxfmt_per_fmt_info<F>::m_max[0];
        uint32 maxA = pxfmt_per_fmt_info<F>::m_max[1];
        dst[0] = (Tdst) ((double) src[0] * (double) maxL);
        dst[1] = (Tdst) ((double) src[3] * (double) maxA);
    }
    else
    {
        dst[0] = (Tdst) src[0];
        dst[1] = (Tdst) src[3];
    }
}


// This special function converts all components of a source pixel from an
// intermediate format for a format of GL_DEPTH_STENCIL and a type of
// GL_UNSIGNED_INT_24_8:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_d24_unorm_s8_uint(Tdst *dst, const Tint *src)
{
    // Convert the depth value:
    double depth_double = src[0] * (double) pxfmt_per_fmt_info<F>::m_max[0];
    uint32 depth_uint32 = (uint32) depth_double;
    depth_uint32 = ((depth_uint32 << pxfmt_per_fmt_info<F>::m_shift[0]) &
                    pxfmt_per_fmt_info<F>::m_mask[0]);

    // Convert the stencil value:
    uint32 *pStencilSrc = (uint32 *) &src[1];
    uint32 stencil = ((*pStencilSrc << pxfmt_per_fmt_info<F>::m_shift[1]) &
                      pxfmt_per_fmt_info<F>::m_mask[1]);

    // Combine the depth and stencil values into one, packed-integer value:
    *dst = (Tdst) (depth_uint32 | stencil);
}


// This special function converts all components of a source pixel from an
// intermediate format for a format of GL_DEPTH_STENCIL and a type of
// GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_d32_float_s8_uint(Tdst *dst, const Tint *src)
{
    // Convert the depth value:
    from_int_comp_copy<F>(dst, src, 0);

    // Convert the stencil value:
    uint32 *pStencilSrc = (uint32 *) &src[1];
    uint32 *pStencilDst = (uint32 *) &dst[1];
    *pStencilDst = ((*pStencilSrc << pxfmt_per_fmt_info<F>::m_shift[1]) &
                    pxfmt_per_fmt_info<F>::m_mask[1]);
}


// This function converts one component to a "small floating-point" value
// (i.e. 10, 11, or 16 bits) from an intermediate value, which is a double:
template <pxfmt_sized_format F, pxfmt_small_fp S, typename Tdst, typename Tint>
inline
void from_int_comp_small_fp(Tdst *dst, const Tint *src, const uint32 c)
{
    uint32 index = pxfmt_per_fmt_info<F>::m_index[c];
    // Get the intermediate value in a form that we can split it into its
    // components:
    double_conversion d;
    d.d = src[index];
    // Note: the bottom 32-bits of the mantissa is always ignored, because a
    // small-floating-point value is too small to ever need it.

    double min = (double) small_fp<S>::m_min;
    double max = (double) small_fp<S>::m_max;

    // Obtain the new exponent:
    int32 exponent;
    if ((d.i.u[1] & small_fp<S>::m_inf_exp_mask) == small_fp<S>::m_inf_exp_mask)
    {
        // Either have infinity or NaN (not a number).  There is special
        // handling for OpenGL's 10- and 11-bit floats, which convert
        // negative-infinity to positive-0.0 and negative-NaN to positive-NaN.
        // The sign-bit is handled below, but converting negative-infinity to
        // 0.0 must be handled here:
        if (!pxfmt_per_fmt_info<F>::m_is_signed &&
            ((d.i.u[1] & 0x000fffff) == 0))
        {
            // Need to convert negative-infinity to 0.0 by using the
            // encoded-exponent for zero (which is all zeroes):
            exponent = 0;
        }
        else
        {
            // The new exponent will is encoded as all ones:
            exponent = 0xFFFFFFFF & small_fp<S>::m_exp_mask;
        }
    }
    else
    {
        // Translate the exponent from src, after potentially clamping the
        // intermediate to the min/max values of the small-fp:
        if (d.d < min)
        {
            d.d = min;
        }
        else if (d.d > max)
        {
            d.d = max;
        }
        // Get the exponent from the intermediate number, and format it for the
        // small-floating-point number:
        exponent = (((d.i.u[1] & 0x7FF00000) >> 20) - 1023);
        if (exponent == -1023)
        {
            // Adjust the exponent for zero:
            exponent = -15;
        }
        exponent = (((exponent + small_fp<S>::m_bias) <<
                     small_fp<S>::m_exp_shift) & small_fp<S>::m_exp_mask);
    }

    // Only 16-bit floats have sign bits (i.e. 10/11-bit floats don't):
    uint32 sign_bit = ((pxfmt_per_fmt_info<F>::m_is_signed) ?
                       (d.i.u[1] >> 31) : 0);
    // Get the mantissa from the intermediate number, and then shift it into
    // place for the small-floating-point number.  All 10/11/16-bit floats have
    // mantissa's that are smaller than that of the top-part of a double's
    // mantissa.  Hence, we need to shift the mantissa into place:
    uint32 mantissa = ((d.i.u[1] & 0x000fffff) >> small_fp<S>::m_man_shift);

    // Hand-craft the small-floating-point number:
    dst[c] = (Tdst) ((sign_bit << small_fp<S>::m_sign_shift) |
                     exponent | mantissa);
}


// This function converts a run-time call to a compile-time call to a function
// that converts one component to a "small floating-point" value
// (i.e. 10, 11, or 16 bits) from an intermediate value, which is a double:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_comp_small_fp(Tdst *dst, const Tint *src, const uint32 c)
{
#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt, S)                                          \
    case S:                                                             \
        from_int_comp_small_fp<fmt,S>(dst, src, c);                       \
        break;

    switch (pxfmt_per_fmt_info<F>::m_small_fp[c])
    {
        CASE_STATEMENT(F, NON_FP);
        CASE_STATEMENT(F, FP10);
        CASE_STATEMENT(F, FP11);
        CASE_STATEMENT(F, FP16);
    }
}


// This special function converts all components of a source pixel from an
// intermediate format for a format of GL_RGB and a type of
// GL_UNSIGNED_INT_10F_11F_11F_REV:
template <pxfmt_sized_format F, typename Tdst, typename Tint>
inline
void from_int_10f_11f_11f_packed(Tdst *dst, const Tint *src)
{
    uint16 dstBits[3];

    // Process the source values:
    from_int_comp_small_fp<F>(dstBits, src, 0);
    from_int_comp_small_fp<F>(dstBits, src, 1);
    from_int_comp_small_fp<F>(dstBits, src, 2);

    // Pack the three components into one 32-bit integer:
    *dst = (Tdst) ((dstBits[0] & 0x000003FF) |
                   ((dstBits[1] << 10) & 0x001FFC00) |
                   ((dstBits[2] << 21) & 0xFFE00000));
}


// This function converts all components of a destination pixel from an
// intermediate format:
template <pxfmt_sized_format F>
inline
void from_intermediate(void *pDst, const void *intermediate)
{
    // Point to the destination pixel, with the correct type:
    typename pxfmt_per_fmt_info<F>::m_formatted_type *dst =
        (typename pxfmt_per_fmt_info<F>::m_formatted_type *) pDst;

    // Point to the intermediate pixel values with the correct type:
    typename pxfmt_per_fmt_info<F>::m_intermediate_type *src =
        (typename pxfmt_per_fmt_info<F>::m_intermediate_type *) intermediate;

    // Note: The compiler should remove most of the code below, during
    // optimization, because (at compile time) it can be determined that it is
    // not needed for the given pxfmt_sized_format:

    // Convert this pixel from the intermediate, to the destination:
    if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_RGB5999_REV)
    {
        from_int_5999<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_format == GL_LUMINANCE)
    {
        from_int_L<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_format == GL_LUMINANCE_ALPHA)
    {
        from_int_LA<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_D24_UNORM_S8_UINT)
    {
        from_int_d24_unorm_s8_uint<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_D32_FLOAT_S8_UINT)
    {
        from_int_d32_float_s8_uint<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_fmt == PXFMT_RGB10F_11F_11F)
    {
        from_int_10f_11f_11f_packed<F>(dst, src);
    }
    else if (pxfmt_per_fmt_info<F>::m_is_packed)
    {
        // Convert all components of this pixel at the same time:
        from_int_comp_packed<F>(dst, src);
    }
    else
    {
        // Convert one component of this pixel at a time:
        for (uint32 c = 0 ; c < pxfmt_per_fmt_info<F>::m_num_components; c++)
        {
            if (pxfmt_per_fmt_info<F>::m_small_fp[0] != NON_FP)
            {
                from_int_comp_small_fp<F>(dst, src, c);
            }
            else if (pxfmt_per_fmt_info<F>::m_index[c] >= 0)
            {
                if (pxfmt_per_fmt_info<F>::m_is_normalized)
                {
                    from_int_comp_norm_unpacked<F>(dst, src, c);
                }
                else
                {
                    from_int_comp_copy<F>(dst, src, c);
                }
            }
        }
    }
}


// This function converts a run-time call to a compile-time call to a function
// that converts all components of a destination pixel from an intermediate
// format:
inline
void from_intermediate(void *pDst, const void *intermediate,
                       const pxfmt_sized_format dst_fmt)
{
#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt)                                             \
    case fmt:                                                           \
        from_intermediate<fmt>(pDst, intermediate);                     \
        break;

    switch (dst_fmt)
    {
#include "pxfmt_case_statements.inl"
        case PXFMT_INVALID: break;
    }
}


// This function fills in information about a given pxfmt_sized_format.
template <pxfmt_sized_format F>
inline
void query_pxfmt_sized_format(bool *has_red,   bool *has_green,
                              bool *has_blue,  bool *has_alpha,
                              bool *has_depth, bool *has_stencil,
                              bool *has_large_components,
                              bool *is_floating_point,
                              bool *is_integer,
                              bool *is_compressed,
                              unsigned int *bytes_per_pixel,
                              unsigned int *bytes_per_compressed_block,
                              unsigned int *block_size)
{
    switch (pxfmt_per_fmt_info<F>::m_format)
    {
    case GL_RED:
        *has_red = true;
        *is_floating_point = true;
        break;
    case GL_GREEN:
        *has_green = true;
        *is_floating_point = true;
        break;
    case GL_BLUE:
        *has_blue = true;
        *is_floating_point = true;
        break;
    case GL_ALPHA:
        *has_alpha = true;
        *is_floating_point = true;
        break;
    case GL_RG:
        *has_red = true;
        *has_green = true;
        *is_floating_point = true;
        break;
    case GL_RGB:
    case GL_BGR:
    case GL_LUMINANCE:
        *has_red = true;
        *has_green = true;
        *has_blue = true;
        *is_floating_point = true;
        break;
    case GL_RGBA:
    case GL_BGRA:
    case GL_LUMINANCE_ALPHA:
        *has_red = true;
        *has_green = true;
        *has_blue = true;
        *has_alpha = true;
        *is_floating_point = true;
        break;
    case GL_RED_INTEGER:
        *has_red = true;
        *is_integer = true;
        break;
    case GL_GREEN_INTEGER:
        *has_green = true;
        *is_integer = true;
        break;
    case GL_BLUE_INTEGER:
        *has_blue = true;
        *is_integer = true;
        break;
    case GL_ALPHA_INTEGER:
        *has_alpha = true;
        *is_integer = true;
        break;
    case GL_RG_INTEGER:
        *has_red = true;
        *has_green = true;
        *is_integer = true;
        break;
    case GL_RGB_INTEGER:
    case GL_BGR_INTEGER:
        *has_red = true;
        *has_green = true;
        *has_blue = true;
        *is_integer = true;
        break;
    case GL_RGBA_INTEGER:
    case GL_BGRA_INTEGER:
        *has_red = true;
        *has_green = true;
        *has_blue = true;
        *has_alpha = true;
        *is_integer = true;
        break;
    case GL_DEPTH_COMPONENT:
        *has_depth = true;
        *is_floating_point = true;
        break;
    case GL_STENCIL_INDEX:
        *has_stencil = true;
        *is_integer = true;
        break;
    case GL_DEPTH_STENCIL:
        *has_depth = true;
        *has_stencil = true;
        *is_floating_point = true;
        *is_integer = true;
        break;
    default:
        break;
    }

    switch (pxfmt_per_fmt_info<F>::m_fmt)
    {
    case PXFMT_RGB10A2_UNORM:
    case PXFMT_A2RGB10_UNORM:
    case PXFMT_BGR10A2_UNORM:
    case PXFMT_A2BGR10_UNORM:
    case PXFMT_RGB10A2_UINT:
    case PXFMT_A2RGB10_UINT:
    case PXFMT_BGR10A2_UINT:
    case PXFMT_A2BGR10_UINT:
        *has_large_components = true;
        break;
    default:
        *has_large_components =
            ((pxfmt_per_fmt_info<F>::m_bytes_per_pixel >
              pxfmt_per_fmt_info<F>::m_num_components) ? true : false);
        break;
    }

    *bytes_per_pixel = pxfmt_per_fmt_info<F>::m_bytes_per_pixel;

    *is_compressed = pxfmt_per_fmt_info<F>::m_is_compressed;
    *bytes_per_compressed_block = pxfmt_per_fmt_info<F>::m_block_size;
    *block_size = pxfmt_per_fmt_info<F>::m_block_size * 8;
}


// This function calls the correct decompression function for a given PXFMT:
inline
void decompress(float *intermediate, const void *pSrc,
                uint32 row_stride, int x, int y,
                const pxfmt_sized_format fmt)
{
    switch (fmt)
    {
    case PXFMT_COMPRESSED_RGB_DXT1:
    case PXFMT_COMPRESSED_RGBA_DXT1:
    case PXFMT_COMPRESSED_SRGB_DXT1:
    case PXFMT_COMPRESSED_SRGB_ALPHA_DXT1:
    case PXFMT_COMPRESSED_RGBA_DXT3:
    case PXFMT_COMPRESSED_SRGB_ALPHA_DXT3:    
    case PXFMT_COMPRESSED_RGBA_DXT5:
    case PXFMT_COMPRESSED_SRGB_ALPHA_DXT5: 
        decompress_dxt(intermediate, pSrc, row_stride, x, y, fmt);
        break;
    case PXFMT_COMPRESSED_RGB_FXT1:
    case PXFMT_COMPRESSED_RGBA_FXT1:
        decompress_fxt1(intermediate, pSrc, row_stride, x, y, fmt);
        break;
    case PXFMT_COMPRESSED_RGB8_ETC2:
    case PXFMT_COMPRESSED_SRGB8_ETC2:
    case PXFMT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case PXFMT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case PXFMT_COMPRESSED_RGBA8_ETC2_EAC:
    case PXFMT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    case PXFMT_COMPRESSED_R11_EAC:
    case PXFMT_COMPRESSED_RG11_EAC:
    case PXFMT_COMPRESSED_SIGNED_R11_EAC:
    case PXFMT_COMPRESSED_SIGNED_RG11_EAC:
        decompress_etc(intermediate, pSrc, row_stride, x, y, fmt);
        break;
    default:
        break;
    }
}


} // unamed namespace



/******************************************************************************
 *
 * The following is an externally-visible function of this library:
 *
 ******************************************************************************/

// This function is used to get back a pxfmt_sized_format enum value for a
// given OpenGL "format" and "type".  If the format-type combination isn't
// supported, PXFMT_INVALID is returned.
pxfmt_sized_format validate_format_type_combo(const GLenum format,
                                              const GLenum type)
{
    switch (format)
    {
    case GL_RED:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_R8_UNORM;
        case GL_BYTE:
            return PXFMT_R8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_R16_UNORM;
        case GL_SHORT:
            return PXFMT_R16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_R32_UNORM;
        case GL_INT:
            return PXFMT_R32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_R16_FLOAT;
        case GL_FLOAT:
            return PXFMT_R32_FLOAT;
        default:
            break;
        }
        break;
    case GL_GREEN:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_G8_UNORM;
        case GL_BYTE:
            return PXFMT_G8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_G16_UNORM;
        case GL_SHORT:
            return PXFMT_G16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_G32_UNORM;
        case GL_INT:
            return PXFMT_G32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_G16_FLOAT;
        case GL_FLOAT:
            return PXFMT_G32_FLOAT;
        default:
            break;
        }
        break;
    case GL_BLUE:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_B8_UNORM;
        case GL_BYTE:
            return PXFMT_B8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_B16_UNORM;
        case GL_SHORT:
            return PXFMT_B16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_B32_UNORM;
        case GL_INT:
            return PXFMT_B32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_B16_FLOAT;
        case GL_FLOAT:
            return PXFMT_B32_FLOAT;
        default:
            break;
        }
        break;
    case GL_ALPHA:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_A8_UNORM;
        case GL_BYTE:
            return PXFMT_A8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_A16_UNORM;
        case GL_SHORT:
            return PXFMT_A16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_A32_UNORM;
        case GL_INT:
            return PXFMT_A32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_A16_FLOAT;
        case GL_FLOAT:
            return PXFMT_A32_FLOAT;
        default:
            break;
        }
        break;
    case GL_RG:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_RG8_UNORM;
        case GL_BYTE:
            return PXFMT_RG8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_RG16_UNORM;
        case GL_SHORT:
            return PXFMT_RG16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_RG32_UNORM;
        case GL_INT:
            return PXFMT_RG32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_RG16_FLOAT;
        case GL_FLOAT:
            return PXFMT_RG32_FLOAT;
        default:
            break;
        }
        break;
    case GL_RGB:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_RGB8_UNORM;
        case GL_BYTE:
            return PXFMT_RGB8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_RGB16_UNORM;
        case GL_SHORT:
            return PXFMT_RGB16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_RGB32_UNORM;
        case GL_INT:
            return PXFMT_RGB32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_RGB16_FLOAT;
        case GL_FLOAT:
            return PXFMT_RGB32_FLOAT;

        case GL_UNSIGNED_BYTE_3_3_2:
            return PXFMT_RGB332_UNORM;
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return PXFMT_RGB233_UNORM;
        case GL_UNSIGNED_SHORT_5_6_5:
            return PXFMT_RGB565_UNORM;
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            return PXFMT_RGB565REV_UNORM;
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return PXFMT_RGB5999_REV;
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            return PXFMT_RGB10F_11F_11F;
        default:
            break;
        }
        break;
    case GL_BGR:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_BGR8_UNORM;
        case GL_BYTE:
            return PXFMT_BGR8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_BGR16_UNORM;
        case GL_SHORT:
            return PXFMT_BGR16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_BGR32_UNORM;
        case GL_INT:
            return PXFMT_BGR32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_BGR16_FLOAT;
        case GL_FLOAT:
            return PXFMT_BGR32_FLOAT;
        default:
            break;
        }
        break;
    case GL_RGBA:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_RGBA8_UNORM;
        case GL_BYTE:
            return PXFMT_RGBA8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_RGBA16_UNORM;
        case GL_SHORT:
            return PXFMT_RGBA16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_RGBA32_UNORM;
        case GL_INT:
            return PXFMT_RGBA32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_RGBA16_FLOAT;
        case GL_FLOAT:
            return PXFMT_RGBA32_FLOAT;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            return PXFMT_RGBA4_UNORM;
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            return PXFMT_RGBA4REV_UNORM;
        case GL_UNSIGNED_SHORT_5_5_5_1:
            return PXFMT_RGB5A1_UNORM;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return PXFMT_A1RGB5_UNORM;
        case GL_UNSIGNED_INT_8_8_8_8:
            return PXFMT_RGBA8888_UNORM;
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return PXFMT_RGBA8_UNORM;
        case GL_UNSIGNED_INT_10_10_10_2:
            return PXFMT_RGB10A2_UNORM;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return PXFMT_A2RGB10_UNORM;
        default:
            break;
        }
        break;
    case GL_BGRA:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_BGRA8_UNORM;
        case GL_BYTE:
            return PXFMT_BGRA8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_BGRA16_UNORM;
        case GL_SHORT:
            return PXFMT_BGRA16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_BGRA32_UNORM;
        case GL_INT:
            return PXFMT_BGRA32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_BGRA16_FLOAT;
        case GL_FLOAT:
            return PXFMT_BGRA32_FLOAT;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            return PXFMT_BGRA4_UNORM;
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            return PXFMT_BGRA4REV_UNORM;
        case GL_UNSIGNED_SHORT_5_5_5_1:
            return PXFMT_BGR5A1_UNORM;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return PXFMT_A1BGR5_UNORM;
        case GL_UNSIGNED_INT_8_8_8_8:
            return PXFMT_BGRA8888_UNORM;
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return PXFMT_BGRA8_UNORM;
        case GL_UNSIGNED_INT_10_10_10_2:
            return PXFMT_BGR10A2_UNORM;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return PXFMT_A2BGR10_UNORM;
        default:
            break;
        }
        break;
    case GL_LUMINANCE:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_L8_UNORM;
        case GL_BYTE:
            return PXFMT_L8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_L16_UNORM;
        case GL_SHORT:
            return PXFMT_L16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_L32_UNORM;
        case GL_INT:
            return PXFMT_L32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_L16_FLOAT;
        case GL_FLOAT:
            return PXFMT_L32_FLOAT;
        default:
            break;
        }
        break;
    case GL_LUMINANCE_ALPHA:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_LA8_UNORM;
        case GL_BYTE:
            return PXFMT_LA8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_LA16_UNORM;
        case GL_SHORT:
            return PXFMT_LA16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_LA32_UNORM;
        case GL_INT:
            return PXFMT_LA32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_LA16_FLOAT;
        case GL_FLOAT:
            return PXFMT_LA32_FLOAT;
        default:
            break;
        }
        break;
    case GL_RED_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_R8_UINT;
        case GL_BYTE:
            return PXFMT_R8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_R16_UINT;
        case GL_SHORT:
            return PXFMT_R16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_R32_UINT;
        case GL_INT:
            return PXFMT_R32_SINT;
        default:
            break;
        }
        break;
    case GL_GREEN_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_G8_UINT;
        case GL_BYTE:
            return PXFMT_G8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_G16_UINT;
        case GL_SHORT:
            return PXFMT_G16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_G32_UINT;
        case GL_INT:
            return PXFMT_G32_SINT;
        default:
            break;
        }
        break;
    case GL_BLUE_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_B8_UINT;
        case GL_BYTE:
            return PXFMT_B8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_B16_UINT;
        case GL_SHORT:
            return PXFMT_B16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_B32_UINT;
        case GL_INT:
            return PXFMT_B32_SINT;
        default:
            break;
        }
        break;
    case GL_ALPHA_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_A8_UINT;
        case GL_BYTE:
            return PXFMT_A8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_A16_UINT;
        case GL_SHORT:
            return PXFMT_A16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_A32_UINT;
        case GL_INT:
            return PXFMT_A32_SINT;
        default:
            break;
        }
        break;
    case GL_RG_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_RG8_UINT;
        case GL_BYTE:
            return PXFMT_RG8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_RG16_UINT;
        case GL_SHORT:
            return PXFMT_RG16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_RG32_UINT;
        case GL_INT:
            return PXFMT_RG32_SINT;
        default:
            break;
        }
        break;
    case GL_RGB_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_RGB8_UINT;
        case GL_BYTE:
            return PXFMT_RGB8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_RGB16_UINT;
        case GL_SHORT:
            return PXFMT_RGB16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_RGB32_UINT;
        case GL_INT:
            return PXFMT_RGB32_SINT;

        case GL_UNSIGNED_BYTE_3_3_2:
            return PXFMT_RGB332_UINT;
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return PXFMT_RGB233_UINT;
        case GL_UNSIGNED_SHORT_5_6_5:
            return PXFMT_RGB565_UINT;
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            return PXFMT_RGB565REV_UINT;
        default:
            break;
        }
        break;
    case GL_BGR_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_BGR8_UINT;
        case GL_BYTE:
            return PXFMT_BGR8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_BGR16_UINT;
        case GL_SHORT:
            return PXFMT_BGR16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_BGR32_UINT;
        case GL_INT:
            return PXFMT_BGR32_SINT;
        default:
            break;
        }
        break;
    case GL_RGBA_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_RGBA8_UINT;
        case GL_BYTE:
            return PXFMT_RGBA8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_RGBA16_UINT;
        case GL_SHORT:
            return PXFMT_RGBA16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_RGBA32_UINT;
        case GL_INT:
            return PXFMT_RGBA32_SINT;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            return PXFMT_RGBA4_UINT;
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            return PXFMT_RGBA4REV_UINT;
        case GL_UNSIGNED_SHORT_5_5_5_1:
            return PXFMT_RGB5A1_UINT;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return PXFMT_A1RGB5_UINT;
        case GL_UNSIGNED_INT_8_8_8_8:
            return PXFMT_RGBA8_UINT;
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return PXFMT_RGBA8REV_UINT;
        case GL_UNSIGNED_INT_10_10_10_2:
            return PXFMT_RGB10A2_UINT;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return PXFMT_A2RGB10_UINT;
        default:
            break;
        }
        break;
    case GL_BGRA_INTEGER:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_BGRA8_UINT;
        case GL_BYTE:
            return PXFMT_BGRA8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_BGRA16_UINT;
        case GL_SHORT:
            return PXFMT_BGRA16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_BGRA32_UINT;
        case GL_INT:
            return PXFMT_BGRA32_SINT;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            return PXFMT_BGRA4_UINT;
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            return PXFMT_BGRA4REV_UINT;
        case GL_UNSIGNED_SHORT_5_5_5_1:
            return PXFMT_BGR5A1_UINT;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return PXFMT_A1BGR5_UINT;
        case GL_UNSIGNED_INT_8_8_8_8:
            return PXFMT_BGRA8_UINT;
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return PXFMT_BGRA8REV_UINT;
        case GL_UNSIGNED_INT_10_10_10_2:
            return PXFMT_BGR10A2_UINT;
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return PXFMT_A2BGR10_UINT;
        default:
            break;
        }
        break;
    case GL_DEPTH_COMPONENT:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_D8_UNORM;
        case GL_BYTE:
            return PXFMT_D8_SNORM;
        case GL_UNSIGNED_SHORT:
            return PXFMT_D16_UNORM;
        case GL_SHORT:
            return PXFMT_D16_SNORM;
        case GL_UNSIGNED_INT:
            return PXFMT_D32_UNORM;
        case GL_INT:
            return PXFMT_D32_SNORM;
        case GL_HALF_FLOAT:
            return PXFMT_D16_FLOAT;
        case GL_FLOAT:
            return PXFMT_D32_FLOAT;
        default:
            break;
        }
        break;
    case GL_STENCIL_INDEX:
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            return PXFMT_S8_UINT;
        case GL_BYTE:
            return PXFMT_S8_SINT;
        case GL_UNSIGNED_SHORT:
            return PXFMT_S16_UINT;
        case GL_SHORT:
            return PXFMT_S16_SINT;
        case GL_UNSIGNED_INT:
            return PXFMT_S32_UINT;
        case GL_INT:
            return PXFMT_S32_SINT;
        case GL_HALF_FLOAT:
            return PXFMT_S16_FLOAT;
        case GL_FLOAT:
            return PXFMT_S32_FLOAT;
        default:
            break;
        }
        break;
    case GL_DEPTH_STENCIL:
        switch (type)
        {
        case GL_UNSIGNED_INT_24_8:
            return PXFMT_D24_UNORM_S8_UINT;
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            return PXFMT_D32_FLOAT_S8_UINT;
        default:
            break;
        }
        break;
    }
    // If we get to here, this is an unsupported format-type combination:
    return PXFMT_INVALID;
}



/******************************************************************************
 *
 * The following is an externally-visible function of this library:
 *
 ******************************************************************************/

// This function is used to get back a pxfmt_sized_format enum value for a
// given OpenGL "internalformat" of a compressed texture.  If the format-type
// combination isn't supported, PXFMT_INVALID is returned.
pxfmt_sized_format validate_internal_format(const GLenum internalformat)
{
// FIXME: REMOVE THIS #if ONCE WE SUPPORT ANY NON-DXT PXFMT ON WINDOWS:
#if defined(_WIN32)
    return PXFMT_INVALID;
#else  // defined(_WIN32)
    switch (internalformat)
    {
#if defined(_WIN32)
#else  // defined(_WIN32)
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        return PXFMT_COMPRESSED_RGB_DXT1;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return PXFMT_COMPRESSED_RGBA_DXT1;
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        return PXFMT_COMPRESSED_SRGB_DXT1;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        return PXFMT_COMPRESSED_SRGB_ALPHA_DXT1;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return PXFMT_COMPRESSED_RGBA_DXT3;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        return PXFMT_COMPRESSED_SRGB_ALPHA_DXT3;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return PXFMT_COMPRESSED_RGBA_DXT5;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        return PXFMT_COMPRESSED_SRGB_ALPHA_DXT5;
#endif // defined(_WIN32)

#if !defined(__APPLE__)
    case GL_COMPRESSED_RGBA_FXT1_3DFX:
        return PXFMT_COMPRESSED_RGB_FXT1;
    case GL_COMPRESSED_RGB_FXT1_3DFX:
        return PXFMT_COMPRESSED_RGBA_FXT1;
    case GL_COMPRESSED_RGB8_ETC2:
        return PXFMT_COMPRESSED_RGB8_ETC2;
    case GL_COMPRESSED_SRGB8_ETC2:
        return PXFMT_COMPRESSED_SRGB8_ETC2;
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        return PXFMT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        return PXFMT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
        return PXFMT_COMPRESSED_RGBA8_ETC2_EAC;
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        return PXFMT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;
    case GL_COMPRESSED_R11_EAC:
        return PXFMT_COMPRESSED_R11_EAC;
    case GL_COMPRESSED_SIGNED_R11_EAC:
        return PXFMT_COMPRESSED_SIGNED_R11_EAC;
    case GL_COMPRESSED_RG11_EAC:
        return PXFMT_COMPRESSED_RG11_EAC;
    case GL_COMPRESSED_SIGNED_RG11_EAC:
        return PXFMT_COMPRESSED_SIGNED_RG11_EAC;
#endif

    default:
        // If we get to here, this is an unsupported compressed-texture
        // internalformat:
        return PXFMT_INVALID;
    }
#endif // defined(_WIN32)
}


/******************************************************************************
 *
 * The following is an externally-visible function of this library:
 *
 ******************************************************************************/

// This function is used to obtain information about a given
// pxfmt_sized_format.  The first parameter specify the pxfmt_sized_format to
// obtain info about, and the rest of the parameters are modified by this call.
//
// This call converts a run-time call to a compile-time call to a function
// that actually performs the functionality.
void query_pxfmt_sized_format(pxfmt_sized_format fmt,
                              bool *has_red,   bool *has_green,
                              bool *has_blue,  bool *has_alpha,
                              bool *has_depth, bool *has_stencil,
                              bool *has_large_components,
                              bool *is_floating_point,
                              bool *is_integer,
                              bool *is_compressed,
                              unsigned int *bytes_per_pixel,
                              unsigned int *bytes_per_compressed_block,
                              unsigned int *block_size)
{
    // Set all values to a default, and modify in a fmt-specific manner:
    *has_red = false;
    *has_green = false;
    *has_blue = false;
    *has_alpha = false;
    *has_depth = false;
    *has_stencil = false;
    *has_large_components = false;
    *is_floating_point = false;
    *is_integer = false;
    *is_compressed = false;
    *bytes_per_pixel = 0;
    *bytes_per_compressed_block = 0;
    *block_size = 0;

#ifdef CASE_STATEMENT
#undef CASE_STATEMENT
#endif
#define CASE_STATEMENT(fmt)                                             \
    case fmt:                                                           \
        query_pxfmt_sized_format<fmt>(has_red,   has_green,             \
                                      has_blue,  has_alpha,             \
                                      has_depth, has_stencil,           \
                                      has_large_components,             \
                                      is_floating_point,                \
                                      is_integer,                       \
                                      is_compressed,                    \
                                      bytes_per_pixel,                  \
                                      bytes_per_compressed_block,       \
                                      block_size);                      \
        break;

    switch (fmt)
    {
#include "pxfmt_case_statements.inl"
        case PXFMT_INVALID: break;
    }
}



/******************************************************************************
 *
 * The following is an externally-visible function of this library:
 *
 ******************************************************************************/

// This function is used to obtain information about a given OpenGL "format"
// and "type".  The first two parameters specify the OpenGL "format" and "type"
// to obtain info about, and the rest of the parameters are modified by this
// call.
pxfmt_sized_format query_pxfmt_sized_format(const GLenum format,
                                            const GLenum type,
                                            bool *has_red,   bool *has_green,
                                            bool *has_blue,  bool *has_alpha,
                                            bool *has_depth, bool *has_stencil,
                                            bool *has_large_components,
                                            bool *is_floating_point,
                                            bool *is_integer,
                                            bool *is_compressed,
                                            unsigned int *bytes_per_pixel,
                                            unsigned int *bytes_per_compressed_block,
                                            unsigned int *block_size)
{
    pxfmt_sized_format fmt = validate_format_type_combo(format, type);
    query_pxfmt_sized_format(fmt, has_red, has_green, has_blue, has_alpha,
                             has_depth, has_stencil, has_large_components,
                             is_floating_point, is_integer,
                             is_compressed, bytes_per_pixel,
                             bytes_per_compressed_block, block_size);
    return fmt;
}



/******************************************************************************
 *
 * The following is an externally-visible function of this library:
 *
 ******************************************************************************/

// This function is used to convert a rectangular set of pixel data from one
// pxfmt_sized_format to another.  Size-wise, there are two types of
// "intermediate data" values (double and uint32, or 64-bit floating-poitn and
// 32-bit integers).  Data is converted from the source to a set of appropriate
// intermediate values, and then converted from that to the destination.  As
// long as the intermediate values contain enough precision, etc, values can be
// converted in a loss-less fashion.
//
// TODO: This function will eventually be able to handle mipmap levels,
// etc.  For now, all of the conversions are where the interesting action is
// at.
//
// TODO: Create another, similar function that will convert just one pixel
// (e.g. for the GUI).
//
// TODO: Add the "scale and bias" capability needed for the VOGL GUI.
//
// TBD: Does the VOGL GUI need the "scale and bias" capability only for
// floating-point and fixed-point data, or does it also need it for pure
// integer data?
pxfmt_conversion_status pxfmt_convert_pixels(void *pDst,
                                             const void *pSrc,
                                             const int width,
                                             const int height,
                                             const pxfmt_sized_format dst_fmt,
                                             const pxfmt_sized_format src_fmt,
                                             size_t dst_size,
                                             size_t src_size,
                                             size_t alt_dst_row_stride,
                                             size_t alt_src_row_stride)
{
    // Before proceeding, ensure that we are dealing with supported formats:
    if (dst_fmt == PXFMT_INVALID)
    {
        return PXFMT_CONVERSION_UNSUPPORTED_DST;
    }
    else if (src_fmt == PXFMT_INVALID)
    {
        return PXFMT_CONVERSION_UNSUPPORTED_SRC;
    }

    // Get the per-pixel and per-row strides for both the src and dst:
    uint32 dst_pixel_stride;
    uint32 dst_row_stride;
    bool dst_needs_fp_intermediate;
    uint32 src_pixel_stride;
    uint32 src_row_stride;
    bool src_needs_fp_intermediate;
    get_pxfmt_info(width, dst_pixel_stride, dst_row_stride,
                    dst_needs_fp_intermediate, dst_fmt);
    get_pxfmt_info(width, src_pixel_stride, src_row_stride,
                    src_needs_fp_intermediate, src_fmt);

    // Ensure that the values we got make sense, including that the
    // row_stride*width match the given size:
    if (!((dst_pixel_stride > 0) ||
          (dst_row_stride > 0) ||
          (src_pixel_stride > 0) ||
          (src_row_stride > 0) ||
          (src_needs_fp_intermediate == dst_needs_fp_intermediate)))
    {
        return PXFMT_CONVERSION_UNKNOWN_ERROR;
    }
    // At this point, if the caller provided an alternative row_stride, use it:
    if (alt_dst_row_stride != 0)
    {
        dst_row_stride = (uint32) alt_dst_row_stride;
    }
    if (alt_src_row_stride != 0)
    {
        src_row_stride = (uint32) alt_src_row_stride;
    }
// FIXME - REMOVE THIS #ifdef
#ifdef TEMPORARILY_DISABLE_SIZE_CHECK
    // Now check that the row_stride*width match the given size:
    if ((height * dst_row_stride) != dst_size)
    {
        return PXFMT_CONVERSION_BAD_SIZE_DST;
    }
    else if ((height * src_row_stride) != src_size)
    {
        return PXFMT_CONVERSION_BAD_SIZE_SRC;
    }
#endif // TEMPORARILY_DISABLE_SIZE_CHECK

    // Use local pointers to the src and dst (both to increment within and
    // between rows) in order to properly deal with all strides:
    uint8 *src, *src_row;
    uint8 *dst, *dst_row;
    src = src_row = (uint8 *) pSrc;
    dst = dst_row = (uint8 *) pDst;

    if (src_needs_fp_intermediate)
    {
        // In order to handle 32-bit normalized values, we need to use
        // double-precision floating-point intermediate values:
        double intermediate[4];

        for (int y = 0 ; y < height ; y++)
        {
            for (int x = 0 ; x < width ; x++)
            {
                to_intermediate(intermediate, src, src_fmt);
                from_intermediate(dst, intermediate, dst_fmt);
                src += src_pixel_stride;
                dst += dst_pixel_stride;
            }
            src = src_row += src_row_stride;
            dst = dst_row += dst_row_stride;
        }
    }
    else
    {
        // The actual intermediate value can be uint32's, or int32's.  They are
        // the same size, and so at this level of the functionality, any will
        // do.  We'll use uint32's:
        uint32 intermediate[4];

        for (int y = 0 ; y < height ; y++)
        {
            for (int x = 0 ; x < width ; x++)
            {
                to_intermediate(intermediate, src, src_fmt);
                from_intermediate(dst, intermediate, dst_fmt);
                src += src_pixel_stride;
                dst += dst_pixel_stride;
            }
            src = src_row += src_row_stride;
            dst = dst_row += dst_row_stride;
        }
    }
    return PXFMT_CONVERSION_SUCCESS;
}



/******************************************************************************
 *
 * The following is an externally-visible function of this library:
 *
 ******************************************************************************/

// This function is used to convert a rectangular set of compressed-texture
// data to an uncompressed pxfmt_sized_format.  Size-wise, there is only one
// type of "intermediate data" values (double).  Data is uncompressed to double
// intermediate values, and then converted from that to the destination.  As
// long as the intermediate values contain enough precision, etc, values can be
// converted in a loss-less fashion.
pxfmt_conversion_status
pxfmt_decompress_pixels(void *pDst,
                        const void *pSrc,
                        const int width,
                        const int height,
                        const pxfmt_sized_format dst_fmt,
                        const pxfmt_sized_format src_fmt,
                        size_t dst_size,
                        size_t src_size)
{
    // Before proceeding, ensure that we are dealing with supported formats:
    if (dst_fmt == PXFMT_INVALID)
    {
        return PXFMT_CONVERSION_UNSUPPORTED_DST;
    }
    else if (src_fmt == PXFMT_INVALID)
    {
        return PXFMT_CONVERSION_UNSUPPORTED_SRC;
    }

    // Get the per-pixel and per-row strides for the dst:
    uint32 dst_pixel_stride;
    uint32 dst_row_stride;
    bool dst_needs_fp_intermediate;
    get_pxfmt_info(width, dst_pixel_stride, dst_row_stride,
                    dst_needs_fp_intermediate, dst_fmt);

    // Get the per-compression-block info for the src:
    uint32 src_block_width = 0;
    uint32 src_block_height = 0;
    uint32 src_block_perblock_stride = 0;
    uint32 src_block_perrow_stride = 0;
    get_compression_block_info(width, src_block_width, src_block_height,
                               src_block_perblock_stride,
                               src_block_perrow_stride,
                               src_fmt);

    // Now that we have info about the src's per-compression-block, also
    // determine the dst's stride within a row of blocks, and between rows of
    // blocks:
    uint32 dst_block_perblock_stride = dst_pixel_stride * src_block_width;
    uint32 dst_block_perrow_stride = dst_row_stride * src_block_height;

    // Ensure that the values we got make sense, including that the
    // row_stride*width match the given size:
    if (!((dst_pixel_stride > 0) ||
          (dst_row_stride > 0) ||
          (dst_block_perblock_stride > 0) ||
          (dst_block_perrow_stride > 0) ||
          (src_block_width > 0) ||
          (src_block_height > 0) ||
          (src_block_perblock_stride > 0) ||
          (src_block_perrow_stride > 0) ||
          (true == dst_needs_fp_intermediate)))
    {
        return PXFMT_CONVERSION_UNKNOWN_ERROR;
    }
// FIXME - REMOVE THIS #ifdef
#ifdef TEMPORARILY_DISABLE_SIZE_CHECK
    // Now check that the row_stride*width match the given size:
    if ((height * dst_row_stride) != dst_size)
    {
        return PXFMT_CONVERSION_BAD_SIZE_DST;
    }
    else if ((height * src_row_stride) != src_size)
    {
        return PXFMT_CONVERSION_BAD_SIZE_SRC;
    }
#endif // TEMPORARILY_DISABLE_SIZE_CHECK

    // In order to handle 32-bit normalized values, we need to use
    // double-precision floating-point intermediate values:
    double intermediate[4];

    // Use local pointers to the src and dst (both to increment within and
    // between rows) in order to properly deal with all strides:
    uint8 *dst, *dst_row;
    dst = dst_row = (uint8 *) pDst;

    for (int y = 0 ; y < height ; y++)
    {
        for (int x = 0 ; x < width ; x++)
        {
            float ifloat[4] = {0.0, 0.0, 0.0, 1.0};
            // Decompress a src pixel:
            decompress(ifloat, pSrc, src_block_perrow_stride, x, y, src_fmt);
            // Copy the intermediate value to a double:
            intermediate[0] = ifloat[0];
            intermediate[1] = ifloat[1];
            intermediate[2] = ifloat[2];
            intermediate[3] = ifloat[3];
            // Convert the intermediate value to the dst format-type
            // combination:
            from_intermediate(dst, intermediate, dst_fmt);
#ifdef DECOMPRESS_DEBUG
            uint32 *dst_as_uint32 = (uint32 *) dst;
            printf("  *dst = 0x%08x\n", *dst_as_uint32);
#endif // DECOMPRESS_DEBUG
            dst += dst_pixel_stride;
        }
        dst = dst_row += dst_row_stride;
    }

    return PXFMT_CONVERSION_SUCCESS;
}
