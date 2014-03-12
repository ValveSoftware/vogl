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

// File: vogl_texture_file_types.h
#pragma once

#include "vogl_core.h"
#include "vogl.h"
#include "vogl_vec.h"
#include "vogl_pixel_format.h"

namespace vogl
{
    struct texture_file_types
    {
        enum format
        {
            cFormatInvalid = -1,
            cFormatDDS,
            cFormatCRN,
            cFormatKTX,
            cNumMipmappedFileFormats,
            cFormatTGA = cNumMipmappedFileFormats,
            cFormatPNG,
            cFormatJPG,
            cFormatJPEG,
            cFormatBMP,
            cFormatGIF,
            cFormatTIF,
            cFormatTIFF,
            cFormatPPM,
            cFormatPGM,
            cFormatPSD,
            cFormatJP2,
            cNumRegularFileFormats,
            cNumImageFileFormats = cNumRegularFileFormats - cNumMipmappedFileFormats,

            // Not really a file format
            cFormatClipboard = cNumRegularFileFormats,
            cFormatDragDrop,
            cNumFileFormats
        };

        static const char *get_extension(format fmt);

        static format determine_file_format(const char *pFilename);

        static bool supports_mipmaps(format fmt);
        static bool supports_alpha(format fmt);
    };

    enum texture_type
    {
        cTextureTypeUnknown = 0,
        cTextureTypeRegularMap,
        cTextureTypeNormalMap,
        cTextureTypeVerticalCrossCubemap,
        cTextureTypeCubemap,
        cNumTextureTypes
    };

    const char *get_texture_type_desc(texture_type t);

} // namespace vogl
