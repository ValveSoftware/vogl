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

// File: vogl_texture_file_types.cpp
#include "vogl_core.h"
#include "vogl_texture_file_types.h"
#include "vogl_file_utils.h"

namespace vogl
{
    const char *texture_file_types::get_extension(format fmt)
    {
        VOGL_ASSERT(fmt < cNumFileFormats);
        if (fmt >= cNumFileFormats)
            return NULL;

        static const char *extensions[cNumFileFormats] =
            {
                "dds",
                "crn",
                "ktx",
                "tga",
                "png",
                "jpg",
                "jpeg",
                "bmp",
                "gif",
                "tif",
                "tiff",
                "ppm",
                "pgm",
                "psd",
                "jp2",
                "<clipboard>",
                "<dragdrop>"
            };
        return extensions[fmt];
    }

    texture_file_types::format texture_file_types::determine_file_format(const char *pFilename)
    {
        dynamic_string ext;
        if (!file_utils::split_path(pFilename, NULL, NULL, NULL, &ext))
            return cFormatInvalid;

        if (ext.is_empty())
            return cFormatInvalid;

        if (ext[0] == '.')
            ext.right(1);

        for (uint32_t i = 0; i < cNumFileFormats; i++)
            if (ext == get_extension(static_cast<format>(i)))
                return static_cast<format>(i);

        return cFormatInvalid;
    }

    bool texture_file_types::supports_mipmaps(format fmt)
    {
        switch (fmt)
        {
            case cFormatCRN:
            case cFormatDDS:
            case cFormatKTX:
                return true;
            default:
                break;
        }

        return false;
    }

    bool texture_file_types::supports_alpha(format fmt)
    {
        switch (fmt)
        {
            case cFormatJPG:
            case cFormatJPEG:
            case cFormatGIF:
            case cFormatJP2:
                return false;
            default:
                break;
        }

        return true;
    }

    const char *get_texture_type_desc(texture_type t)
    {
        switch (t)
        {
            case cTextureTypeUnknown:
                return "Unknown";
            case cTextureTypeRegularMap:
                return "2D map";
            case cTextureTypeNormalMap:
                return "Normal map";
            case cTextureTypeVerticalCrossCubemap:
                return "Vertical Cross Cubemap";
            case cTextureTypeCubemap:
                return "Cubemap";
            default:
                break;
        }

        VOGL_ASSERT(false);

        return "?";
    }

} // namespace vogl
