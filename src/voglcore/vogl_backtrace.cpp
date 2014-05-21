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

// File vogl_backtrace.cpp
#include "vogl_backtrace.h"
#include "vogl_console.h"

#if VOGL_USE_LINUX_API
#define USE_LIBBACKTRACE 0

#if USE_LIBBACKTRACE
#include "backtrace.h"
#else
#include <linux/unistd.h>
#include <execinfo.h>
#endif
#endif

namespace vogl
{
#if VOGL_USE_LINUX_API

#if !USE_LIBBACKTRACE
    bool get_printable_backtrace(dynamic_string_array &string_vec)
    {
        string_vec.resize(0);

        const uint32_t BUF_SIZE = 256;
        void *buffer[BUF_SIZE];

        int nptrs = backtrace(buffer, BUF_SIZE);
        if (!nptrs)
            return false;

        char **strings = backtrace_symbols(buffer, nptrs);
        if (!strings)
            return false;

        string_vec.resize(nptrs);

        for (int i = 0; i < nptrs; i++)
            string_vec[i].set(strings[i] ? strings[i] : "?");

        free(strings);

        return true;
    }
#else
    bool get_printable_backtrace(dynamic_string_array &string_vec)
    {
        string_vec.resize(0);

        const int N = 128;
        uintptr_t pcs[N];
        int n = backtrace_simple_get_pcs(pcs, N);
        if (!n)
            return false;

        VOGL_ASSERT(n <= N);
        if (n > N)
            return false;

        string_vec.reserve(n);

        for (int i = 0; i < n; i++)
        {
            stackframe_info info;
            utils::zero_object(info);
            if (backtrace_simple_resolve_pc(&info, pcs[i]))
            {
                string_vec.enlarge(1)->format("%u %s(%i), PC: 0x%llX, Ofs: 0x%llX, Mod: %s, Filename: \"%s\"", i,
                                              info.function ? demangle(info.function).get_ptr() : "?",
                                              info.linenumber,
                                              (uint64_t)info.pc, (uint64_t)info.offset,
                                              info.module ? info.module : "?",
                                              info.filename ? info.filename : "?");
            }
            else
            {
                string_vec.push_back("?");
            }
        }

        return true;
    }
#endif // !USE_LIBBACKTRACE

#else  // !VOGL_USE_LINUX_API

    bool get_printable_backtrace(dynamic_string_array &strings)
    {
        strings.resize(0);

        vogl_debug_printf("TODOs\n");

        return false;
    }
#endif // VOGL_USE_LINUX_API

} // namespace vogl
