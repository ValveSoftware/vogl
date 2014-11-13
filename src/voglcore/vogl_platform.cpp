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

// File: vogl_platform.cpp
#include "vogl_core.h"
#include "vogl_file_utils.h"

using namespace vogl;

// HACK HACK - don't leave this enabled
#ifndef VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT
#define VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT 0
#endif

#if VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT
#pragma message("VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT is enabled.")
#endif

#pragma message(__FILE__ " Warning: Slow string checking enabled")

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

// --------------------------------- Misc debugging related helpers

#if !defined(VOGL_USE_WIN32_API)

static int IsDebuggerPresent(void)
{
    char buf[128];
    int tracerpid = -1;

    FILE *file = fopen("/proc/self/status", "r");
    if (file)
    {
        static const char tracerpid_str[] = "TracerPid";
        static const size_t tracerpid_str_len = strlen(tracerpid_str);

        while (fgets(buf, sizeof(buf), file))
        {
            if ((strncmp(buf, tracerpid_str, tracerpid_str_len) == 0) && (buf[tracerpid_str_len] == ':'))
            {
                tracerpid = atoi(&buf[tracerpid_str_len + 1]);
                break;
            }
        }

        fclose(file);
    }

    return (tracerpid > 0);
}

static void OutputDebugStringA(const char *p)
{
    puts(p);
}

#endif

void vogl_debug_break_if_debugging(void)
{
    static const char *vogl_break_on_assert = getenv("VOGL_BREAK_ON_ASSERT");

    if (vogl_break_on_assert || vogl_is_debugger_present())
    {
        VOGL_BREAKPOINT
    }
}

bool vogl_is_debugger_present(void)
{
#if VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT
    return true;
#else
    return IsDebuggerPresent() != 0;
#endif
}

void vogl_output_debug_string(const char *p)
{
    OutputDebugStringA(p);
}

// --------------------------------- Process signal/exception handling

#if defined(VOGL_USE_WIN32_API)

vogl_exception_callback_t vogl_set_exception_callback(vogl_exception_callback_t callback)
{
    #pragma message("Windows: TODO: Review set_exception_callback; currently does nothing.")
    return callback;
}

#else

#include <signal.h>

#if defined(PLATFORM_OSX)
#include <unistd.h>
#endif

static const int MAX_SIGNALS = 16;
struct sigaction g_prev_sigactions[MAX_SIGNALS];

static vogl_exception_callback_t g_exception_callback = NULL;

/**************************************************************************
 *
 * Signal handler code derived from apitrace. Apitrace license:
 *
 * Copyright 2011 Jose Fonseca
 * Copyright 2008-2010 VMware, Inc.
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

/*
 * See also:
 * - http://sourceware.org/git/?p=glibc.git;a=blob;f=debug/segfault.c
 * - http://ggi.cvs.sourceforge.net/viewvc/ggi/ggi-core/libgg/gg/cleanup.c?view=markup
 */
static void signal_handler(int sig, siginfo_t *info, void *context)
{
    fixed_string256 buf;

    if (sig == SIGPIPE)
        return;

    static int s_recursion_count = 0;

    buf.format("voglcore: ERROR: Caught signal %i\n", sig);
    write(STDERR_FILENO, buf.get_ptr(), buf.size());

    if (s_recursion_count)
    {
        buf.format("voglcore: Recursion detected handling signal %i\n", sig);
        write(STDERR_FILENO, buf.get_ptr(), buf.size());
    }
    else if (g_exception_callback)
    {
        buf.format("voglcore: Calling global exception callback\n");
        write(STDERR_FILENO, buf.get_ptr(), buf.size());

        ++s_recursion_count;
        g_exception_callback();
        --s_recursion_count;
    }

    if (sig >= MAX_SIGNALS)
    {
        buf.format("voglcore: ERROR: Unexpected signal %i\n", sig);
        write(STDERR_FILENO, buf.get_ptr(), buf.size());

        raise(SIGKILL);
    }

    // pass on the exception to the prev handler
    struct sigaction *pPrev_action = &g_prev_sigactions[sig];

    if (pPrev_action->sa_flags & SA_SIGINFO)
    {
        buf.format("voglcore: Dispatching to prev handler for signal %i\n", sig);
        write(STDERR_FILENO, buf.get_ptr(), buf.size());

        pPrev_action->sa_sigaction(sig, info, context);
    }
    else
    {
        if (pPrev_action->sa_handler == SIG_DFL)
        {
            buf.format("voglcore: Taking default action for signal %i\n", sig);
            write(STDERR_FILENO, buf.get_ptr(), buf.size());

            struct sigaction dfl_action;
            dfl_action.sa_handler = SIG_DFL;
            sigemptyset(&dfl_action.sa_mask);
            dfl_action.sa_flags = 0;
            sigaction(sig, &dfl_action, NULL);

            raise(sig);
        }
        else if (pPrev_action->sa_handler == SIG_IGN)
        {
            // ignore
            buf.format("voglcore: Ignoring signal %i\n", sig);
            write(STDERR_FILENO, buf.get_ptr(), buf.size());
        }
        else
        {
            // dispatch to handler
            buf.format("voglcore: Dispatching handler registered for signal %i\n", sig);
            write(STDERR_FILENO, buf.get_ptr(), buf.size());

            pPrev_action->sa_handler(sig);
        }
    }
}

vogl_exception_callback_t vogl_set_exception_callback(vogl_exception_callback_t callback)
{
    if (g_exception_callback)
    {
        vogl_exception_callback_t pPrev = g_exception_callback;
        g_exception_callback = callback;
        return pPrev;
    }

    g_exception_callback = callback;

    struct sigaction new_action;
    new_action.sa_sigaction = signal_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = SA_SIGINFO | SA_RESTART;

    for (int i = 1; i < MAX_SIGNALS; i++)
    {
        if ((i == SIGPIPE) || (i == SIGKILL) || (i == SIGSTOP))
            continue;

        if (sigaction(i, NULL, &g_prev_sigactions[i]) >= 0)
            sigaction(i, &new_action, NULL);
    }

    return NULL;
}

void vogl_reset_exception_callback()
{
    g_exception_callback = NULL;
}

#endif // !VOGL_USE_WIN32_API

