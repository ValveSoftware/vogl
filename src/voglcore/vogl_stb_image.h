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
#pragma once

#include "vogl_core.h"

////   begin header file  ////////////////////////////////////////////////////
//
// Limitations:
//    - no progressive/interlaced support (jpeg, png)
//    - 8-bit samples only (jpeg, png)
//    - not threadsafe
//    - channel subsampling of at most 2 in each dimension (jpeg)
//    - no delayed line count (jpeg) -- IJG doesn't support either
//
// Basic usage (see HDR discussion below):
//    int x,y,n;
//    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
//    // ... process data if not NULL ...
//    // ... x = width, y = height, n = # 8-bit components per pixel ...
//    // ... replace '0' with '1'..'4' to force that many components per pixel
//    stbi_image_free(data)
//
// Standard parameters:
//    int *x       -- outputs image width in pixels
//    int *y       -- outputs image height in pixels
//    int *comp    -- outputs # of image components in image file
//    int req_comp -- if non-zero, # of image components requested in result
//
// The return value from an image loader is an 'unsigned char *' which points
// to the pixel data. The pixel data consists of *y scanlines of *x pixels,
// with each pixel consisting of N interleaved 8-bit components; the first
// pixel pointed to is top-left-most in the image. There is no padding between
// image scanlines or between pixels, regardless of format. The number of
// components N is 'req_comp' if req_comp is non-zero, or *comp otherwise.
// If req_comp is non-zero, *comp has the number of components that _would_
// have been output otherwise. E.g. if you set req_comp to 4, you will always
// get RGBA output, but you can check *comp to easily see if it's opaque.
//
// An output image with N components has the following components interleaved
// in this order in each pixel:
//
//     N=#comp     components
//       1           grey
//       2           grey, alpha
//       3           red, green, blue
//       4           red, green, blue, alpha
//
// If image loading fails for any reason, the return value will be NULL,
// and *x, *y, *comp will be unchanged. The function stbi_failure_reason()
// can be queried for an extremely brief, end-user unfriendly explanation
// of why the load failed. Define STBI_NO_FAILURE_STRINGS to avoid
// compiling these strings at all, and STBI_FAILURE_USERMSG to get slightly
// more user-friendly ones.
//
// Paletted PNG and BMP images are automatically depalettized.
//
//
// ===========================================================================
//
// HDR image support   (disable by defining STBI_NO_HDR)
//
// stb_image now supports loading HDR images in general, and currently
// the Radiance .HDR file format, although the support is provided
// generically. You can still load any file through the existing interface;
// if you attempt to load an HDR file, it will be automatically remapped to
// LDR, assuming gamma 2.2 and an arbitrary scale factor defaulting to 1;
// both of these constants can be reconfigured through this interface:
//
//     stbi_hdr_to_ldr_gamma(2.2f);
//     stbi_hdr_to_ldr_scale(1.0f);
//
// (note, do not use _inverse_ constants; stbi_image will invert them
// appropriately).
//
// Additionally, there is a new, parallel interface for loading files as
// (linear) floats to preserve the full dynamic range:
//
//    float *data = stbi_loadf(filename, &x, &y, &n, 0);
//
// If you load LDR images through this interface, those images will
// be promoted to floating point values, run through the inverse of
// constants corresponding to the above:
//
//     stbi_ldr_to_hdr_scale(1.0f);
//     stbi_ldr_to_hdr_gamma(2.2f);
//
// Finally, given a filename (or an open file or memory block--see header
// file for details) containing image data, you can query for the "most
// appropriate" interface to use (that is, whether the image is HDR or
// not), using:
//
//     stbi_is_hdr(char *filename);

#ifndef STBI_NO_STDIO
#include <stdio.h>
#endif

#ifndef STBI_SIMD
#define STBI_SIMD 0
#endif

namespace vogl
{

#define STBI_VERSION 1

    enum
    {
        STBI_default = 0, // only used for req_comp
        STBI_grey = 1,
        STBI_grey_alpha = 2,
        STBI_rgb = 3,
        STBI_rgb_alpha = 4,
    };

    typedef unsigned char stbi_uc;

//#ifdef __cplusplus
//extern "C" {
//#endif

// WRITING API

#if !defined(STBI_NO_WRITE) && !defined(STBI_NO_STDIO)
    // write a BMP/TGA file given tightly packed 'comp' channels (no padding, nor bmp-stride-padding)
    // (you must include the appropriate extension in the filename).
    // returns TRUE on success, FALSE if couldn't open file, error writing file
    extern int stbi_write_bmp(char const *filename, int x, int y, int comp, const void *data);
#ifdef COMPILER_MSVC
    extern int stbi_write_bmp_w(wchar_t const *filename, int x, int y, int comp, const void *data);
#endif
    extern int stbi_write_tga(char const *filename, int x, int y, int comp, const void *data);
#ifdef COMPILER_MSVC
    extern int stbi_write_tga_w(wchar_t const *filename, int x, int y, int comp, const void *data);
#endif
#endif

// PRIMARY API - works on images of any type

// load image by filename, open file, or memory buffer
#ifndef STBI_NO_STDIO
    extern stbi_uc *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);
#ifdef COMPILER_MSVC
    extern stbi_uc *stbi_load_w(wchar_t const *filename, int *x, int *y, int *comp, int req_comp);
#endif
    extern stbi_uc *stbi_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
    extern int stbi_info_from_file(FILE *f, int *x, int *y, int *comp);
#endif
    extern stbi_uc *stbi_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
// for stbi_load_from_file, file pointer is left pointing immediately after image

#ifndef STBI_NO_HDR
#ifndef STBI_NO_STDIO
    extern float *stbi_loadf(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern float *stbi_loadf_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif
    extern float *stbi_loadf_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);

    extern void stbi_hdr_to_ldr_gamma(float gamma);
    extern void stbi_hdr_to_ldr_scale(float scale);

    extern void stbi_ldr_to_hdr_gamma(float gamma);
    extern void stbi_ldr_to_hdr_scale(float scale);

#endif // STBI_NO_HDR

    // get a VERY brief reason for failure
    // NOT THREADSAFE
    extern const char *stbi_failure_reason(void);

    // free the loaded image -- this is just stb_free()
    extern void stbi_image_free(void *retval_from_stbi_load);

    // get image dimensions & components without fully decoding
    extern int stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp);
    extern int stbi_is_hdr_from_memory(stbi_uc const *buffer, int len);
#ifndef STBI_NO_STDIO
    extern int stbi_info(char const *filename, int *x, int *y, int *comp);
    extern int stbi_is_hdr(char const *filename);
    extern int stbi_is_hdr_from_file(FILE *f);
#endif

    // ZLIB client - used by PNG, available for other purposes

    extern char *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len, int initial_size, int *outlen);
    extern char *stbi_zlib_decode_malloc(const char *buffer, int len, int *outlen);
    extern int stbi_zlib_decode_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);

    extern char *stbi_zlib_decode_noheader_malloc(const char *buffer, int len, int *outlen);
    extern int stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);

    // TYPE-SPECIFIC ACCESS

    // is it a jpeg?
    extern int stbi_jpeg_test_memory(stbi_uc const *buffer, int len);
    extern stbi_uc *stbi_jpeg_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
    extern int stbi_jpeg_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp);

#ifndef STBI_NO_STDIO
    extern stbi_uc *stbi_jpeg_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern int stbi_jpeg_test_file(FILE *f);
    extern stbi_uc *stbi_jpeg_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);

    extern int stbi_jpeg_info(char const *filename, int *x, int *y, int *comp);
    extern int stbi_jpeg_info_from_file(FILE *f, int *x, int *y, int *comp);
#endif

    // is it a png?
    extern int stbi_png_test_memory(stbi_uc const *buffer, int len);
    extern stbi_uc *stbi_png_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
    extern int stbi_png_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp);

#ifndef STBI_NO_STDIO
    extern stbi_uc *stbi_png_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern int stbi_png_info(char const *filename, int *x, int *y, int *comp);
    extern int stbi_png_test_file(FILE *f);
    extern stbi_uc *stbi_png_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
    extern int stbi_png_info_from_file(FILE *f, int *x, int *y, int *comp);
#endif

    // is it a bmp?
    extern int stbi_bmp_test_memory(stbi_uc const *buffer, int len);

    extern stbi_uc *stbi_bmp_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern stbi_uc *stbi_bmp_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
#ifndef STBI_NO_STDIO
    extern int stbi_bmp_test_file(FILE *f);
    extern stbi_uc *stbi_bmp_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif

    // is it a tga?
    extern int stbi_tga_test_memory(stbi_uc const *buffer, int len);

    extern stbi_uc *stbi_tga_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern stbi_uc *stbi_tga_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
#ifndef STBI_NO_STDIO
    extern int stbi_tga_test_file(FILE *f);
    extern stbi_uc *stbi_tga_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif

    // is it a psd?
    extern int stbi_psd_test_memory(stbi_uc const *buffer, int len);

    extern stbi_uc *stbi_psd_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern stbi_uc *stbi_psd_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
#ifndef STBI_NO_STDIO
    extern int stbi_psd_test_file(FILE *f);
    extern stbi_uc *stbi_psd_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif

    // is it an hdr?
    extern int stbi_hdr_test_memory(stbi_uc const *buffer, int len);

    extern float *stbi_hdr_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern float *stbi_hdr_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
#ifndef STBI_NO_STDIO
    extern int stbi_hdr_test_file(FILE *f);
    extern float *stbi_hdr_load_from_file(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif

    // define new loaders
    typedef struct
    {
        int (*test_memory)(stbi_uc const *buffer, int len);
        stbi_uc *(*load_from_memory)(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp);
#ifndef STBI_NO_STDIO
        int (*test_file)(FILE *f);
        stbi_uc *(*load_from_file)(FILE *f, int *x, int *y, int *comp, int req_comp);
#endif
    } stbi_loader;

    // register a loader by filling out the above structure (you must defined ALL functions)
    // returns 1 if added or already added, 0 if not added (too many loaders)
    // NOT THREADSAFE
    extern int stbi_register_loader(stbi_loader *loader);

// define faster low-level operations (typically SIMD support)
#if STBI_SIMD
    typedef void (*stbi_idct_8x8)(uint8_t *out, int out_stride, short data[64], unsigned short *dequantize);
    // compute an integer IDCT on "input"
    //     input[x] = data[x] * dequantize[x]
    //     write results to 'out': 64 samples, each run of 8 spaced by 'out_stride'
    //                             CLAMP results to 0..255
    typedef void (*stbi_YCbCr_to_RGB_run)(uint8_t *output, uint8_t const *y, uint8_t const *cb, uint8_t const *cr, int count, int step);
    // compute a conversion from YCbCr to RGB
    //     'count' pixels
    //     write pixels to 'output'; each pixel is 'step' bytes (either 3 or 4; if 4, write '255' as 4th), order R,G,B
    //     y: Y input channel
    //     cb: Cb input channel; scale/biased to be 0..255
    //     cr: Cr input channel; scale/biased to be 0..255

    extern void stbi_install_idct(stbi_idct_8x8 func);
    extern void stbi_install_YCbCr_to_RGB(stbi_YCbCr_to_RGB_run func);
#endif // STBI_SIMD

    //#ifdef __cplusplus
    //}
    //#endif
}

//
//
////   end header file   /////////////////////////////////////////////////////
