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

// File: vogl_assert.cpp
#include "vogl_core.h"
#include "vogl_strutils.h"
#include "vogl_backtrace.h"

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

#if VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS && (defined(NDEBUG) || defined(VOGL_BUILD_RELEASE))
#pragma message("Assertions enabled in release build.")
#endif

static bool g_fail_exceptions = false;
static bool g_exit_on_failure = true;

void vogl_enable_fail_exceptions(bool enabled)
{
    g_fail_exceptions = enabled;
}

void vogl_assert(const char *pExp, const char *pFile, unsigned line)
{
    char buf[512];

    vogl::vogl_sprintf_s(buf, sizeof(buf), "%s(%u): Assertion failed: \"%s\"\n", pFile, line, pExp);

    vogl_output_debug_string(buf);
    fputs(buf, stderr);

#if 0
	vogl::dynamic_string_array backtrace;
	if (get_printable_backtrace(backtrace))
	{
		vogl_output_debug_string("Backtrace:");
		for (uint32_t i = 0; i < backtrace.size(); i++)
			vogl_output_debug_string(backtrace[i].get_ptr());
	}
#endif

    vogl_debug_break_if_debugging();
}

void vogl_fail(const char *pExp, const char *pFile, unsigned line)
{
    char buf[512];

    vogl::vogl_sprintf_s(buf, sizeof(buf), "%s(%u): Failure: \"%s\"\n", pFile, line, pExp);

    vogl_output_debug_string(buf);
    fputs(buf, stderr);

    vogl::dynamic_string_array backtrace;
    if (get_printable_backtrace(backtrace))
    {
        vogl_output_debug_string("Backtrace:");
        for (uint32_t i = 0; i < backtrace.size(); i++)
            vogl_output_debug_string(backtrace[i].get_ptr());
    }

    vogl_debug_break_if_debugging();

#ifdef VOGL_USE_WIN32_API
    if (g_fail_exceptions)
        RaiseException(VOGL_FAIL_EXCEPTION_CODE, 0, 0, NULL);
    else
#endif
        if (g_exit_on_failure)
        exit(EXIT_FAILURE);
}

void trace(const char *pFmt, va_list args)
{
    if (vogl_is_debugger_present())
    {
        char buf[512];
        vogl::vogl_vsprintf_s(buf, sizeof(buf), pFmt, args);

        vogl_output_debug_string(buf);
    }
};

void trace(const char *pFmt, ...)
{
    va_list args;
    va_start(args, pFmt);
    trace(pFmt, args);
    va_end(args);
};
