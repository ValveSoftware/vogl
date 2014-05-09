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

// File: dds_defs.h
// DX9 .DDS file header definitions.
#pragma once

#include "vogl.h"

#define VOGL_PIXEL_FMT_FOURCC(a, b, c, d) ((a) | ((b) << 8U) | ((c) << 16U) | ((d) << 24U))

namespace vogl
{
   enum pixel_format
   {
      PIXEL_FMT_INVALID               = 0,

      PIXEL_FMT_DXT1                  = VOGL_PIXEL_FMT_FOURCC('D', 'X', 'T', '1'),
      PIXEL_FMT_DXT2                  = VOGL_PIXEL_FMT_FOURCC('D', 'X', 'T', '2'),
      PIXEL_FMT_DXT3                  = VOGL_PIXEL_FMT_FOURCC('D', 'X', 'T', '3'),
      PIXEL_FMT_DXT4                  = VOGL_PIXEL_FMT_FOURCC('D', 'X', 'T', '4'),
      PIXEL_FMT_DXT5                  = VOGL_PIXEL_FMT_FOURCC('D', 'X', 'T', '5'),
      PIXEL_FMT_3DC                   = VOGL_PIXEL_FMT_FOURCC('A', 'T', 'I', '2'), // DXN_YX
      PIXEL_FMT_DXN                   = VOGL_PIXEL_FMT_FOURCC('A', '2', 'X', 'Y'), // DXN_XY
      PIXEL_FMT_DXT5A                 = VOGL_PIXEL_FMT_FOURCC('A', 'T', 'I', '1'), // ATI1N, http://developer.amd.com/media/gpu_assets/Radeon_X1x00_Programming_Guide.pdf

      // Non-standard, voglcore-specific pixel formats (some of these are supported by ATI's Compressonator)
      PIXEL_FMT_DXT5_CCxY             = VOGL_PIXEL_FMT_FOURCC('C', 'C', 'x', 'Y'),
      PIXEL_FMT_DXT5_xGxR             = VOGL_PIXEL_FMT_FOURCC('x', 'G', 'x', 'R'),
      PIXEL_FMT_DXT5_xGBR             = VOGL_PIXEL_FMT_FOURCC('x', 'G', 'B', 'R'),
      PIXEL_FMT_DXT5_AGBR             = VOGL_PIXEL_FMT_FOURCC('A', 'G', 'B', 'R'),

      PIXEL_FMT_DXT1A                 = VOGL_PIXEL_FMT_FOURCC('D', 'X', '1', 'A'),
      PIXEL_FMT_ETC1                  = VOGL_PIXEL_FMT_FOURCC('E', 'T', 'C', '1'),

      PIXEL_FMT_R8G8B8                = VOGL_PIXEL_FMT_FOURCC('R', 'G', 'B', 'x'),
      PIXEL_FMT_L8                    = VOGL_PIXEL_FMT_FOURCC('L', 'x', 'x', 'x'),
      PIXEL_FMT_A8                    = VOGL_PIXEL_FMT_FOURCC('x', 'x', 'x', 'A'),
      PIXEL_FMT_A8L8                  = VOGL_PIXEL_FMT_FOURCC('L', 'x', 'x', 'A'),
      PIXEL_FMT_A8R8G8B8              = VOGL_PIXEL_FMT_FOURCC('R', 'G', 'B', 'A')
   };

   const vogl_uint32 cDDSMaxImageDimensions = 8192U;

   // Total size of header is sizeof(uint32_t)+cDDSSizeofDDSurfaceDesc2;
   const vogl_uint32 cDDSSizeofDDSurfaceDesc2 = 124;

   // "DDS "
   const vogl_uint32 cDDSFileSignature = 0x20534444;

   struct DDCOLORKEY
   {
      vogl_uint32 dwUnused0;
      vogl_uint32 dwUnused1;
   };

   struct DDPIXELFORMAT
   {
      vogl_uint32 dwSize;
      vogl_uint32 dwFlags;
      vogl_uint32 dwFourCC;
      vogl_uint32 dwRGBBitCount;     // ATI compressonator and voglcore will place a FOURCC code here for swizzled/cooked DXTn formats
      vogl_uint32 dwRBitMask;
      vogl_uint32 dwGBitMask;
      vogl_uint32 dwBBitMask;
      vogl_uint32 dwRGBAlphaBitMask;
   };

   struct DDSCAPS2
   {
      vogl_uint32 dwCaps;
      vogl_uint32 dwCaps2;
      vogl_uint32 dwCaps3;
      vogl_uint32 dwCaps4;
   };

   struct DDSURFACEDESC2
   {
      vogl_uint32 dwSize;
      vogl_uint32 dwFlags;
      vogl_uint32 dwHeight;
      vogl_uint32 dwWidth;
      union
      {
         vogl_int32 lPitch;
         vogl_uint32 dwLinearSize;
      };
      vogl_uint32 dwBackBufferCount;
      vogl_uint32 dwMipMapCount;
      vogl_uint32 dwAlphaBitDepth;
      vogl_uint32 dwUnused0;
      vogl_uint32 lpSurface;
      DDCOLORKEY unused0;
      DDCOLORKEY unused1;
      DDCOLORKEY unused2;
      DDCOLORKEY unused3;
      DDPIXELFORMAT ddpfPixelFormat;
      DDSCAPS2 ddsCaps;
      vogl_uint32 dwUnused1;
   };
   
   const vogl_uint32 DDSD_CAPS                   = 0x00000001;
   const vogl_uint32 DDSD_HEIGHT                 = 0x00000002;
   const vogl_uint32 DDSD_WIDTH                  = 0x00000004;
   const vogl_uint32 DDSD_PITCH                  = 0x00000008;

   const vogl_uint32 DDSD_BACKBUFFERCOUNT        = 0x00000020;
   const vogl_uint32 DDSD_ZBUFFERBITDEPTH        = 0x00000040;
   const vogl_uint32 DDSD_ALPHABITDEPTH          = 0x00000080;

   const vogl_uint32 DDSD_LPSURFACE              = 0x00000800;
                                                            
   const vogl_uint32 DDSD_PIXELFORMAT            = 0x00001000;
   const vogl_uint32 DDSD_CKDESTOVERLAY          = 0x00002000;
   const vogl_uint32 DDSD_CKDESTBLT              = 0x00004000;
   const vogl_uint32 DDSD_CKSRCOVERLAY           = 0x00008000;

   const vogl_uint32 DDSD_CKSRCBLT               = 0x00010000;
   const vogl_uint32 DDSD_MIPMAPCOUNT            = 0x00020000;
   const vogl_uint32 DDSD_REFRESHRATE            = 0x00040000;
   const vogl_uint32 DDSD_LINEARSIZE             = 0x00080000;

   const vogl_uint32 DDSD_TEXTURESTAGE           = 0x00100000;
   const vogl_uint32 DDSD_FVF                    = 0x00200000;
   const vogl_uint32 DDSD_SRCVBHANDLE            = 0x00400000;
   const vogl_uint32 DDSD_DEPTH                  = 0x00800000;
   
   const vogl_uint32 DDSD_ALL                    = 0x00fff9ee;

   const vogl_uint32 DDPF_ALPHAPIXELS            = 0x00000001;
   const vogl_uint32 DDPF_ALPHA                  = 0x00000002;
   const vogl_uint32 DDPF_FOURCC                 = 0x00000004;
   const vogl_uint32 DDPF_PALETTEINDEXED8        = 0x00000020;
   const vogl_uint32 DDPF_RGB                    = 0x00000040;
   const vogl_uint32 DDPF_LUMINANCE              = 0x00020000;

   const vogl_uint32 DDSCAPS_COMPLEX             = 0x00000008;
   const vogl_uint32 DDSCAPS_TEXTURE             = 0x00001000;
   const vogl_uint32 DDSCAPS_MIPMAP              = 0x00400000;

   const vogl_uint32 DDSCAPS2_CUBEMAP            = 0x00000200;
   const vogl_uint32 DDSCAPS2_CUBEMAP_POSITIVEX  = 0x00000400;
   const vogl_uint32 DDSCAPS2_CUBEMAP_NEGATIVEX  = 0x00000800;

   const vogl_uint32 DDSCAPS2_CUBEMAP_POSITIVEY  = 0x00001000;
   const vogl_uint32 DDSCAPS2_CUBEMAP_NEGATIVEY  = 0x00002000;
   const vogl_uint32 DDSCAPS2_CUBEMAP_POSITIVEZ  = 0x00004000;
   const vogl_uint32 DDSCAPS2_CUBEMAP_NEGATIVEZ  = 0x00008000;

   const vogl_uint32 DDSCAPS2_VOLUME             = 0x00200000;

} // namespace vogl
