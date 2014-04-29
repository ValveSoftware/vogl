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

// File: vogl.cpp
#include "vogl_core.h"
#include "vogl.h"
#include "vogl_dynamic_stream.h"
#include "vogl_buffer_stream.h"
#include "vogl_ryg_dxt.hpp"
#include "vogl_pixel_format.h"

//----------------------------------------------------------------------------------------------------------------------
// vogl_core_initialize
//   Internal vogl_core initialization routine.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_core_initialize()
{
    vogl::vogl_init_heap();

    vogl::vogl_threading_init();

    vogl_enable_fail_exceptions(true);

    //$ TODO: Could probably move this and only initialize when dxt routines used (if ever?).
    ryg_dxt::sInitDXT();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_core_init
//----------------------------------------------------------------------------------------------------------------------
void vogl_core_init()
{
    static pthread_once_t s_vogl_core_initialize = PTHREAD_ONCE_INIT;
    pthread_once(&s_vogl_core_initialize, vogl_core_initialize);
}

// TODO: Move or delete these straggler funcs, they are artifacts from when voglcore was crnlib

const char *vogl_get_format_string(vogl_format fmt)
{
    return vogl::pixel_format_helpers::get_vogl_format_string(fmt);
}

const char *vogl_get_file_type_ext(vogl_file_type file_type)
{
    switch (file_type)
    {
        case cCRNFileTypeDDS:
            return "dds";
        case cCRNFileTypeCRN:
            return "crn";
        default:
            break;
    }
    return "?";
}

const char *vogl_get_mip_mode_desc(vogl_mip_mode m)
{
    switch (m)
    {
        case cCRNMipModeUseSourceOrGenerateMips:
            return "Use source/generate if none";
        case cCRNMipModeUseSourceMips:
            return "Only use source MIP maps (if any)";
        case cCRNMipModeGenerateMips:
            return "Always generate new MIP maps";
        case cCRNMipModeNoMips:
            return "No MIP maps";
        default:
            break;
    }
    return "?";
}

const char *vogl_get_mip_mode_name(vogl_mip_mode m)
{
    switch (m)
    {
        case cCRNMipModeUseSourceOrGenerateMips:
            return "UseSourceOrGenerate";
        case cCRNMipModeUseSourceMips:
            return "UseSource";
        case cCRNMipModeGenerateMips:
            return "Generate";
        case cCRNMipModeNoMips:
            return "None";
        default:
            break;
    }
    return "?";
}

const char *vogl_get_mip_filter_name(vogl_mip_filter f)
{
    switch (f)
    {
        case cCRNMipFilterBox:
            return "box";
        case cCRNMipFilterTent:
            return "tent";
        case cCRNMipFilterLanczos4:
            return "lanczos4";
        case cCRNMipFilterMitchell:
            return "mitchell";
        case cCRNMipFilterKaiser:
            return "kaiser";
        default:
            break;
    }
    return "?";
}

const char *vogl_get_scale_mode_desc(vogl_scale_mode sm)
{
    switch (sm)
    {
        case cCRNSMDisabled:
            return "disabled";
        case cCRNSMAbsolute:
            return "absolute";
        case cCRNSMRelative:
            return "relative";
        case cCRNSMLowerPow2:
            return "lowerpow2";
        case cCRNSMNearestPow2:
            return "nearestpow2";
        case cCRNSMNextPow2:
            return "nextpow2";
        default:
            break;
    }
    return "?";
}

