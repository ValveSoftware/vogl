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

// File: vogl_core.h
#pragma once

#include <inttypes.h>

#include "vogl_build_options.h"

// Will return a string with the function, filename, line number. Like this:
//   main():voglbench.cpp:675
// Usually used in error printf functions, etc.
#define VOGL_FUNCTION_INFO_CSTR vogl::vogl_function_info(__FILE__, __LINE__, __FUNCTION__)

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>

#include <locale>
#include <algorithm>
#include <limits>
#include <typeinfo>
#include <functional>
#include <iterator>

#ifdef min
    #undef min
#endif

#ifdef max
    #undef max
#endif

#define VOGL_FALSE (0)
#define VOGL_TRUE (1)
#define VOGL_MAX_PATH (260)

#if defined(_DEBUG) || defined(DEBUG)
    #define VOGL_BUILD_DEBUG

    #if defined(NDEBUG)
        #error NDEBUG cannot be defined in VOGL_BUILD_DEBUG
    #endif
#else
    #define VOGL_BUILD_RELEASE

    #ifndef NDEBUG
        #define NDEBUG
    #endif

    #if defined(DEBUG) || defined(_DEBUG)
        #error DEBUG and _DEBUG cannot be defined in VOGL_BUILD_RELEASE
    #endif
#endif

namespace vogl
{

inline const char *vogl_function_info(const char *file, int line, const char *function)
{
    // Don't trim on windows; we want the full path to show up so that if/when the message appears in debugger output 
    // you can double-click the line to jump to that source line in the editor.
#if (defined(COMPILER_MSVC))
    static __declspec(thread) char s_buf[512];
    const char *fname = file;
    _snprintf(s_buf, sizeof(s_buf), "%s(%d): %s():", fname, line, function);
#else
    static __thread char s_buf[512];
    const char *fname = strrchr(file, '/');
    if (!fname)
        fname = strrchr(file, '\\');
    fname = fname ? (fname + 1) : file;
    snprintf(s_buf, sizeof(s_buf), "%s(%d): %s():", fname, line, function);
#endif

    s_buf[sizeof(s_buf) - 1] = 0;
    return s_buf;
}

}

#include "vogl_warnings.h"
#include "vogl_types.h"
#include "vogl_assert.h"
#include "vogl_platform.h"
#include "vogl_helpers.h"
#include "vogl_traits.h"
#include "vogl_mem.h"
#include "vogl_math.h"
#include "vogl_utils.h"
#include "vogl_hash.h"
#include "vogl_rand.h"
#include "vogl_introsort.h"
#include "vogl_vector.h"
#include "vogl_fixed_string.h"
#include "vogl_dynamic_string.h"
#include "vogl_timer.h"

// vogl_core initialize function.
void vogl_core_init();
