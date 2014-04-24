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

// File: vogl.h
//
// Important: If compiling with gcc, be sure strict aliasing is disabled: -fno-strict-aliasing
#pragma once

#if defined(COMPILER_MSVC)
#pragma warning (disable: 4127) //  conditional expression is constant
#endif

#define VOGL_VERSION 104

#define VOGL_SUPPORT_ATI_COMPRESS 0
#define VOGL_SUPPORT_SQUISH 0

typedef unsigned char   vogl_uint8;
typedef unsigned short  vogl_uint16;
typedef unsigned int    vogl_uint32;
typedef signed char     vogl_int8;
typedef signed short    vogl_int16;
typedef signed int      vogl_int32;
typedef unsigned int    vogl_bool;

enum vogl_file_type
{
   // .CRN
   cCRNFileTypeCRN = 0,

   // .DDS using regular DXT or clustered DXT
   cCRNFileTypeDDS,

   cCRNFileTypeForceDWORD = 0xFFFFFFFF
};

// Supported compressed pixel formats.
// Basically all the standard DX9 formats, with some swizzled DXT5 formats
// (most of them supported by ATI's Compressonator), along with some ATI/X360 GPU specific formats.
enum vogl_format
{
   cCRNFmtInvalid = -1,

   cCRNFmtDXT1 = 0,

   cCRNFmtFirstValid = cCRNFmtDXT1,

   // cCRNFmtDXT3 is not currently supported when writing to CRN - only DDS.
   cCRNFmtDXT3,

   cCRNFmtDXT5,

   // Various DXT5 derivatives
   cCRNFmtDXT5_CCxY,    // Luma-chroma
   cCRNFmtDXT5_xGxR,    // Swizzled 2-component
   cCRNFmtDXT5_xGBR,    // Swizzled 3-component
   cCRNFmtDXT5_AGBR,    // Swizzled 4-component

   // ATI 3DC and X360 DXN
   cCRNFmtDXN_XY,
   cCRNFmtDXN_YX,

   // DXT5 alpha blocks only
   cCRNFmtDXT5A,

   cCRNFmtETC1,

   cCRNFmtTotal,

   cCRNFmtForceDWORD = 0xFFFFFFFF
};

// Various library/file format limits.
enum vogl_limits
{
   // Max. mipmap level resolution on any axis.
   cCRNMaxLevelResolution     = 4096,

   cCRNMinPaletteSize         = 8,
   cCRNMaxPaletteSize         = 8192,

   cCRNMaxFaces               = 6,
   cCRNMaxLevels              = 16,

   cCRNMaxHelperThreads       = 16,

   cCRNMinQualityLevel        = 0,
   cCRNMaxQualityLevel        = 255
};

// CRN/DDS compression flags.
// See the m_flags member in the vogl_comp_params struct, below.
enum vogl_comp_flags
{
   // Enables perceptual colorspace distance metrics if set.
   // Important: Be sure to disable this when compressing non-sRGB colorspace images, like normal maps!
   // Default: Set
   cCRNCompFlagPerceptual = 1,

   // Enables (up to) 8x8 macroblock usage if set. If disabled, only 4x4 blocks are allowed.
   // Compression ratio will be lower when disabled, but may cut down on blocky artifacts because the process used to determine
   // where large macroblocks can be used without artifacts isn't perfect.
   // Default: Set.
   cCRNCompFlagHierarchical = 2,

   // cCRNCompFlagQuick disables several output file optimizations - intended for things like quicker previews.
   // Default: Not set.
   cCRNCompFlagQuick = 4,

   // DXT1: OK to use DXT1 alpha blocks for better quality or DXT1A transparency.
   // DXT5: OK to use both DXT5 block types.
   // Currently only used when writing to .DDS files, as .CRN uses only a subset of the possible DXTn block types.
   // Default: Set.
   cCRNCompFlagUseBothBlockTypes = 8,

   // OK to use DXT1A transparent indices to encode black (assumes pixel shader ignores fetched alpha).
   // Currently only used when writing to .DDS files, .CRN never uses alpha blocks.
   // Default: Not set.
   cCRNCompFlagUseTransparentIndicesForBlack = 16,

   // Disables endpoint caching, for more deterministic output.
   // Currently only used when writing to .DDS files.
   // Default: Not set.
   cCRNCompFlagDisableEndpointCaching = 32,

   // If enabled, use the cCRNColorEndpointPaletteSize, etc. params to control the CRN palette sizes. Only useful when writing to .CRN files.
   // Default: Not set.
   cCRNCompFlagManualPaletteSizes = 64,

   // If enabled, DXT1A alpha blocks are used to encode single bit transparency.
   // Default: Not set.
   cCRNCompFlagDXT1AForTransparency = 128,

   // If enabled, the DXT1 compressor's color distance metric assumes the pixel shader will be converting the fetched RGB results to luma (Y part of YCbCr).
   // This increases quality when compressing grayscale images, because the compressor can spread the luma error amoung all three channels (i.e. it can generate blocks
   // with some chroma present if doing so will ultimately lead to lower luma error).
   // Only enable on grayscale source images.
   // Default: Not set.
   cCRNCompFlagGrayscaleSampling = 256,

   // If enabled, debug information will be output during compression.
   // Default: Not set.
   cCRNCompFlagDebugging = 0x80000000,

   cCRNCompFlagForceDWORD = 0xFFFFFFFF
};

// Controls DXTn quality vs. speed control - only used when compressing to .DDS.
enum vogl_dxt_quality
{
   cCRNDXTQualitySuperFast,
   cCRNDXTQualityFast,
   cCRNDXTQualityNormal,
   cCRNDXTQualityBetter,
   cCRNDXTQualityUber,

   cCRNDXTQualityTotal,

   cCRNDXTQualityForceDWORD = 0xFFFFFFFF
};

// Which DXTn compressor to use when compressing to plain (non-clustered) .DDS.
enum vogl_dxt_compressor_type
{
   cCRNDXTCompressorCRN,      // Use voglcore's ETC1 or DXTc block compressor (default, highest quality, comparable or better than ati_compress or squish, and voglcore's ETC1 is a lot faster with similiar quality to Erricson's)
   cCRNDXTCompressorCRNF,     // Use voglcore's "fast" DXTc block compressor
   cCRNDXTCompressorRYG,      // Use RYG's DXTc block compressor (low quality, but very fast)

#if VOGL_SUPPORT_ATI_COMPRESS
   cCRNDXTCompressorATI,
#endif

#if VOGL_SUPPORT_SQUISH
   cCRNDXTCompressorSquish,
#endif

   cCRNTotalDXTCompressors,

   cCRNDXTCompressorForceDWORD = 0xFFFFFFFF
};

// Progress callback function.
// Processing will stop prematurely (and fail) if the callback returns false.
// phase_index, total_phases - high level progress
// subphase_index, total_subphases - progress within current phase
typedef vogl_bool (*vogl_progress_callback_func)(vogl_uint32 phase_index, vogl_uint32 total_phases, vogl_uint32 subphase_index, vogl_uint32 total_subphases, void* pUser_data_ptr);

// CRN/DDS compression parameters struct.
struct vogl_comp_params
{
   inline vogl_comp_params() { clear(); }

   // Clear struct to default parameters.
   inline void clear()
   {
      m_size_of_obj = sizeof(*this);
      m_file_type = cCRNFileTypeCRN;
      m_faces = 1;
      m_width = 0;
      m_height = 0;
      m_levels = 1;
      m_format = cCRNFmtDXT1;
      m_flags = cCRNCompFlagPerceptual | cCRNCompFlagHierarchical | cCRNCompFlagUseBothBlockTypes;

      for (vogl_uint32 f = 0; f < cCRNMaxFaces; f++)
         for (vogl_uint32 l = 0; l < cCRNMaxLevels; l++)
            m_pImages[f][l] = NULL;

      m_target_bitrate = 0.0f;
      m_quality_level = cCRNMaxQualityLevel;
      m_dxt1a_alpha_threshold = 128;
      m_dxt_quality = cCRNDXTQualityUber;
      m_dxt_compressor_type = cCRNDXTCompressorCRN;
      m_alpha_component = 3;

      m_vogl_adaptive_tile_color_psnr_derating = 2.0f;
      m_vogl_adaptive_tile_alpha_psnr_derating = 2.0f;
      m_vogl_color_endpoint_palette_size = 0;
      m_vogl_color_selector_palette_size = 0;
      m_vogl_alpha_endpoint_palette_size = 0;
      m_vogl_alpha_selector_palette_size = 0;

      m_num_helper_threads = 0;
      m_userdata0 = 0;
      m_userdata1 = 0;
      m_pProgress_func = NULL;
      m_pProgress_func_data = NULL;
   }

   inline bool operator== (const vogl_comp_params& rhs) const
   {
#define VOGL_COMP(x) do { if ((x) != (rhs.x)) return false; } while(0)
      VOGL_COMP(m_size_of_obj);
      VOGL_COMP(m_file_type);
      VOGL_COMP(m_faces);
      VOGL_COMP(m_width);
      VOGL_COMP(m_height);
      VOGL_COMP(m_levels);
      VOGL_COMP(m_format);
      VOGL_COMP(m_flags);
      VOGL_COMP(m_target_bitrate);
      VOGL_COMP(m_quality_level);
      VOGL_COMP(m_dxt1a_alpha_threshold);
      VOGL_COMP(m_dxt_quality);
      VOGL_COMP(m_dxt_compressor_type);
      VOGL_COMP(m_alpha_component);
      VOGL_COMP(m_vogl_adaptive_tile_color_psnr_derating);
      VOGL_COMP(m_vogl_adaptive_tile_alpha_psnr_derating);
      VOGL_COMP(m_vogl_color_endpoint_palette_size);
      VOGL_COMP(m_vogl_color_selector_palette_size);
      VOGL_COMP(m_vogl_alpha_endpoint_palette_size);
      VOGL_COMP(m_vogl_alpha_selector_palette_size);
      VOGL_COMP(m_num_helper_threads);
      VOGL_COMP(m_userdata0);
      VOGL_COMP(m_userdata1);
      VOGL_COMP(m_pProgress_func);
      VOGL_COMP(m_pProgress_func_data);

      for (vogl_uint32 f = 0; f < cCRNMaxFaces; f++)
         for (vogl_uint32 l = 0; l < cCRNMaxLevels; l++)
            VOGL_COMP(m_pImages[f][l]);

#undef VOGL_COMP
      return true;
   }

   // Returns true if the input parameters are reasonable.
   inline bool check() const
   {
      if ( (m_file_type > cCRNFileTypeDDS) ||
         (((int)m_quality_level < (int)cCRNMinQualityLevel) || ((int)m_quality_level > (int)cCRNMaxQualityLevel)) ||
         (m_dxt1a_alpha_threshold > 255) ||
         ((m_faces != 1) && (m_faces != 6)) ||
         ((m_width < 1) || (m_width > cCRNMaxLevelResolution)) ||
         ((m_height < 1) || (m_height > cCRNMaxLevelResolution)) ||
         ((m_levels < 1) || (m_levels > cCRNMaxLevels)) ||
         ((m_format < cCRNFmtDXT1) || (m_format >= cCRNFmtTotal)) ||
         ((m_vogl_color_endpoint_palette_size) && ((m_vogl_color_endpoint_palette_size < cCRNMinPaletteSize) || (m_vogl_color_endpoint_palette_size > cCRNMaxPaletteSize))) ||
         ((m_vogl_color_selector_palette_size) && ((m_vogl_color_selector_palette_size < cCRNMinPaletteSize) || (m_vogl_color_selector_palette_size > cCRNMaxPaletteSize))) ||
         ((m_vogl_alpha_endpoint_palette_size) && ((m_vogl_alpha_endpoint_palette_size < cCRNMinPaletteSize) || (m_vogl_alpha_endpoint_palette_size > cCRNMaxPaletteSize))) ||
         ((m_vogl_alpha_selector_palette_size) && ((m_vogl_alpha_selector_palette_size < cCRNMinPaletteSize) || (m_vogl_alpha_selector_palette_size > cCRNMaxPaletteSize))) ||
         (m_alpha_component > 3) ||
         (m_num_helper_threads > cCRNMaxHelperThreads) ||
         (m_dxt_quality > cCRNDXTQualityUber) ||
         (m_dxt_compressor_type >= cCRNTotalDXTCompressors) )
      {
         return false;
      }
      return true;
   }

   // Helper to set/get flags from m_flags member.
   inline bool get_flag(vogl_comp_flags flag) const { return (m_flags & flag) != 0; }
   inline void set_flag(vogl_comp_flags flag, bool val) { m_flags &= ~flag; if (val) m_flags |= flag; }

   vogl_uint32                 m_size_of_obj;

   vogl_file_type              m_file_type;               // Output file type: cCRNFileTypeCRN or cCRNFileTypeDDS.

   vogl_uint32                 m_faces;                   // 1 (2D map) or 6 (cubemap)
   vogl_uint32                 m_width;                   // [1,cCRNMaxLevelResolution], non-power of 2 OK, non-square OK
   vogl_uint32                 m_height;                  // [1,cCRNMaxLevelResolution], non-power of 2 OK, non-square OK
   vogl_uint32                 m_levels;                  // [1,cCRNMaxLevelResolution], non-power of 2 OK, non-square OK

   vogl_format                 m_format;                  // Output pixel format.

   vogl_uint32                 m_flags;                   // see vogl_comp_flags enum

   // Array of pointers to 32bpp input images.
   const vogl_uint32*          m_pImages[cCRNMaxFaces][cCRNMaxLevels];

   // Target bitrate - if non-zero, the compressor will use an interpolative search to find the
   // highest quality level that is <= the target bitrate. If it fails to find a bitrate high enough, it'll
   // try disabling adaptive block sizes (cCRNCompFlagHierarchical flag) and redo the search. This process can be pretty slow.
   float                      m_target_bitrate;

   // Desired quality level.
   // Currently, CRN and DDS quality levels are not compatible with eachother from an image quality standpoint.
   vogl_uint32                 m_quality_level;           // [cCRNMinQualityLevel, cCRNMaxQualityLevel]

   // DXTn compression parameters.
   vogl_uint32                 m_dxt1a_alpha_threshold;
   vogl_dxt_quality            m_dxt_quality;
   vogl_dxt_compressor_type    m_dxt_compressor_type;

   // Alpha channel's component. Defaults to 3.
   vogl_uint32                 m_alpha_component;

   // Various low-level CRN specific parameters.
   float                      m_vogl_adaptive_tile_color_psnr_derating;
   float                      m_vogl_adaptive_tile_alpha_psnr_derating;

   vogl_uint32                 m_vogl_color_endpoint_palette_size;  // [cCRNMinPaletteSize,cCRNMaxPaletteSize]
   vogl_uint32                 m_vogl_color_selector_palette_size;  // [cCRNMinPaletteSize,cCRNMaxPaletteSize]

   vogl_uint32                 m_vogl_alpha_endpoint_palette_size;  // [cCRNMinPaletteSize,cCRNMaxPaletteSize]
   vogl_uint32                 m_vogl_alpha_selector_palette_size;  // [cCRNMinPaletteSize,cCRNMaxPaletteSize]

   // Number of helper threads to create during compression. 0=no threading.
   vogl_uint32                 m_num_helper_threads;

   // CRN userdata0 and userdata1 members, which are written directly to the header of the output file.
   vogl_uint32                 m_userdata0;
   vogl_uint32                 m_userdata1;

   // User provided progress callback.
   vogl_progress_callback_func m_pProgress_func;
   void*                      m_pProgress_func_data;
};

// Mipmap generator's mode.
enum vogl_mip_mode
{
   cCRNMipModeUseSourceOrGenerateMips,       // Use source texture's mipmaps if it has any, otherwise generate new mipmaps
   cCRNMipModeUseSourceMips,                 // Use source texture's mipmaps if it has any, otherwise the output has no mipmaps
   cCRNMipModeGenerateMips,                  // Always generate new mipmaps
   cCRNMipModeNoMips,                        // Output texture has no mipmaps

   cCRNMipModeTotal,

   cCRNModeForceDWORD = 0xFFFFFFFF
};

const char* vogl_get_mip_mode_desc(vogl_mip_mode m);
const char* vogl_get_mip_mode_name(vogl_mip_mode m);

// Mipmap generator's filter kernel.
enum vogl_mip_filter
{
   cCRNMipFilterBox,
   cCRNMipFilterTent,
   cCRNMipFilterLanczos4,
   cCRNMipFilterMitchell,
   cCRNMipFilterKaiser,                      // Kaiser=default mipmap filter

   cCRNMipFilterTotal,

   cCRNMipFilterForceDWORD = 0xFFFFFFFF
};

const char* vogl_get_mip_filter_name(vogl_mip_filter f);

// Mipmap generator's scale mode.
enum vogl_scale_mode
{
   cCRNSMDisabled,
   cCRNSMAbsolute,
   cCRNSMRelative,
   cCRNSMLowerPow2,
   cCRNSMNearestPow2,
   cCRNSMNextPow2,

   cCRNSMTotal,

   cCRNSMForceDWORD = 0xFFFFFFFF
};

const char* vogl_get_scale_mode_desc(vogl_scale_mode sm);

// Mipmap generator parameters.
struct vogl_mipmap_params
{
   inline vogl_mipmap_params() { clear(); }

   inline void clear()
   {
      m_size_of_obj = sizeof(*this);
      m_mode = cCRNMipModeUseSourceOrGenerateMips;
      m_filter = cCRNMipFilterKaiser;
      m_gamma_filtering = true;
      m_gamma = 2.2f;
      // Default "blurriness" factor of .9 actually sharpens the output a little.
      m_blurriness = .9f;
      m_renormalize = false;
      m_tiled = false;
      m_max_levels = cCRNMaxLevels;
      m_min_mip_size = 1;

      m_scale_mode = cCRNSMDisabled;
      m_scale_x = 1.0f;
      m_scale_y = 1.0f;

      m_window_left = 0;
      m_window_top = 0;
      m_window_right = 0;
      m_window_bottom = 0;

      m_clamp_scale = false;
      m_clamp_width = 0;
      m_clamp_height = 0;
   }

   inline bool check() const { return true; }

   inline bool operator== (const vogl_mipmap_params& rhs) const
   {
#define VOGL_COMP(x) do { if ((x) != (rhs.x)) return false; } while(0)
      VOGL_COMP(m_size_of_obj);
      VOGL_COMP(m_mode);
      VOGL_COMP(m_filter);
      VOGL_COMP(m_gamma_filtering);
      VOGL_COMP(m_gamma);
      VOGL_COMP(m_blurriness);
      VOGL_COMP(m_renormalize);
      VOGL_COMP(m_tiled);
      VOGL_COMP(m_max_levels);
      VOGL_COMP(m_min_mip_size);
      VOGL_COMP(m_scale_mode);
      VOGL_COMP(m_scale_x);
      VOGL_COMP(m_scale_y);
      VOGL_COMP(m_window_left);
      VOGL_COMP(m_window_top);
      VOGL_COMP(m_window_right);
      VOGL_COMP(m_window_bottom);
      VOGL_COMP(m_clamp_scale);
      VOGL_COMP(m_clamp_width);
      VOGL_COMP(m_clamp_height);
      return true;
#undef VOGL_COMP
   }
   vogl_uint32     m_size_of_obj;

   vogl_mip_mode   m_mode;
   vogl_mip_filter m_filter;

   vogl_bool       m_gamma_filtering;
   float          m_gamma;

   float          m_blurriness;

   vogl_uint32     m_max_levels;
   vogl_uint32     m_min_mip_size;

   vogl_bool       m_renormalize;
   vogl_bool       m_tiled;

   vogl_scale_mode m_scale_mode;
   float          m_scale_x;
   float          m_scale_y;

   vogl_uint32     m_window_left;
   vogl_uint32     m_window_top;
   vogl_uint32     m_window_right;
   vogl_uint32     m_window_bottom;

   vogl_bool       m_clamp_scale;
   vogl_uint32     m_clamp_width;
   vogl_uint32     m_clamp_height;
};

// -------- High-level helper function definitions for CDN/DDS compression.

#ifndef VOGL_MIN_ALLOC_ALIGNMENT
#define VOGL_MIN_ALLOC_ALIGNMENT sizeof(size_t) * 2
#endif

// -------- String helpers.

// Converts a vogl_file_type to a string.
const char* vogl_get_file_type_ext(vogl_file_type file_type);

// Converts a vogl_format to a string.
const char* vogl_get_format_string(vogl_format fmt);

