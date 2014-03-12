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
#warning VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT is enabled!
#endif

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

// --------------------------------- Misc debugging related helpers

void vogl_debug_break(void)
{
    VOGL_BREAKPOINT
}

#ifdef VOGL_USE_WIN32_API

void vogl_debug_break_if_debugging(void)
{
    if (vogl_is_debugger_present())
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

#else // !defined(VOGL_USE_WIN32_API)

// From http://stackoverflow.com/questions/3596781/detect-if-gdb-is-running
// For crying out loud, this is ridicuously complex compared to Windows and doesn't work if somebody attaches later.

#include <signal.h>

static int g_has_checked_for_debugger = -1;

bool vogl_is_debugger_present(bool force_check)
{
    if ((g_has_checked_for_debugger < 0) || (force_check))
    {
        g_has_checked_for_debugger = 0;

        dynamic_string_array status_strings;
        if (vogl::file_utils::read_text_file_crt("/proc/self/status", status_strings))
        {
            for (uint i = 0; i < status_strings.size(); i++)
            {
                if (status_strings[i].contains("TracerPid"))
                {
                    int val = 0;
                    sscanf(status_strings[i].get_ptr(), "TracerPid: %u", &val);
                    if (val)
                    {
                        g_has_checked_for_debugger = 1;
                        break;
                    }
                }
            }
        }

        if (!g_has_checked_for_debugger)
        {
            pid_t parent_process_id = getppid();
            dynamic_string exe_name(cVarArg, "/proc/%" PRIu64 "/exe", static_cast<uint64_t>(parent_process_id));

            char buf[4096];
            buf[sizeof(buf) - 1] = '\0';
            int64_t n = readlink(exe_name.get_ptr(), buf, sizeof(buf) - 1);

            if (n >= 0)
            {
                // total hackage to determine if the parent process is either gdb/lldb or qtcreator's process stub
                if ((strstr(buf, "gdb") != NULL) || (strstr(buf, "lldb") != NULL) || (strstr(buf, "qtcreator_process_stub") != NULL))
                    g_has_checked_for_debugger = 1;
            }
        }

        //printf("vogl_is_debugger_present: %i\n", g_has_checked_for_debugger);
    }

#if VOGL_ASSUME_DEBUGGER_IS_ALWAYS_PRESENT
    g_has_checked_for_debugger = 1;
    return true;
#else
    return g_has_checked_for_debugger == 1;
#endif
}

int g_debugger_present = -1;

static void _sigtrap_handler(int signum)
{
    VOGL_NOTE_UNUSED(signum);
    g_debugger_present = 0;
    signal(SIGTRAP, SIG_DFL);
}

void vogl_debug_break_if_debugging(void)
{
    if (-1 == g_debugger_present)
    {
        g_debugger_present = 1;
        signal(SIGTRAP, _sigtrap_handler);
    }

    if (g_debugger_present == 1)
    {
        __asm__("int3");
    }
}

void vogl_output_debug_string(const char *p)
{
    puts(p);
}

#endif // #ifdef VOGL_USE_WIN32_API

// --------------------------------- Process signal/exception handling

#ifdef VOGL_USE_WIN32_API

#else // !defined(VOGL_USE_WIN32_API)

static const int MAX_SIGNALS = 16;
struct sigaction g_prev_sigactions[MAX_SIGNALS];

static vogl_exception_callback_t g_exception_callback = NULL;

// Derived from apitrace, but this code appears to have been copied around and tweaked from multiple sources:
// - http://sourceware.org/git/?p=glibc.git;a=blob;f=debug/segfault.c
// - http://ggi.cvs.sourceforge.net/viewvc/ggi/ggi-core/libgg/gg/cleanup.c?view=markup

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

#endif // #ifdef VOGL_USE_WIN32_API

namespace vogl
{

#ifdef VOGL_USE_WIN32_API

    dynamic_string demangle(const char *pMangled)
    {
        // TODO
        console::debug("TODO: %s\n", VOGL_FUNCTION_NAME);

        return dynamic_string(pMangled);
    }

#else

#include <cxxabi.h>
    dynamic_string demangle(const char *pMangled)
    {
        int status = 0;

        char *p = abi::__cxa_demangle(pMangled, 0, 0, &status);

        dynamic_string demangled(p ? p : pMangled);

        std::free(p);

        return demangled;
    }

#endif

} // namespace vogl
