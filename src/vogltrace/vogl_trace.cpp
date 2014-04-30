/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
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

// File: vogl_trace.cpp
#include "vogl_trace.h"
#include "vogl_command_line_params.h"
#include "vogl_colorized_console.h"
#include <signal.h>

#include "vogl_port.h"

#if VOGL_PLATFORM_SUPPORTS_BTRACE
    #include "btrace.h"
#endif

#if defined(PLATFORM_LINUX)
    #include <execinfo.h>
    #include <linux/unistd.h>
#endif



//----------------------------------------------------------------------------------------------------------------------
// Has exception hooks, dlopen() interception, super low-level global init.  Be super careful what you
//  do in some of these funcs (particularly vogl_shared_object_constructor_func() or any of the funcs
//  it calls).  I've found it's possible to do shit here that will work fine in primitive apps (like
//  vogltest) but kill tracing in some real apps.
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
void *g_vogl_actual_libgl_module_handle = NULL;
bool g_vogl_initializing_flag = false;

//----------------------------------------------------------------------------------------------------------------------
// dlopen interceptor
//----------------------------------------------------------------------------------------------------------------------
#if defined(PLATFORM_LINUX)
    typedef void *(*dlopen_func_ptr_t)(const char *pFile, int mode);
    VOGL_API_EXPORT void *dlopen(const char *pFile, int mode)
    {
        static dlopen_func_ptr_t s_pActual_dlopen = (dlopen_func_ptr_t)dlsym(RTLD_NEXT, "dlopen");
        if (!s_pActual_dlopen)
            return NULL;

        printf("(vogltrace) dlopen: %s %i\n", pFile ? pFile : "(nullptr)", mode);

        // Only redirect libGL.so when it comes from the app, NOT the driver or one of its associated helpers.
        // This is definitely fragile as all fuck.
        if (!g_vogl_initializing_flag)
        {
            if (pFile && (strstr(pFile, "libGL.so") != NULL))
            {
                const char *calling_module = btrace_get_calling_module();
                bool should_redirect = !strstr(calling_module, "fglrx");

                if (should_redirect)
                {
                    pFile = btrace_get_current_module();
                    printf("(vogltrace) redirecting dlopen to %s\n", pFile);
                }
                else
                {
                    printf("(vogltrace) NOT redirecting dlopen to %s, this dlopen() call appears to be coming from the driver\n", pFile);
                }

                printf("------------\n");
            }
        }

        // Check if this module is already loaded.
        void *is_loaded = (*s_pActual_dlopen)(pFile, RTLD_NOLOAD);
        // Call the real dlopen().
        void *dlopen_ret = (*s_pActual_dlopen)(pFile, mode);

        // Only call btrace routines after vogl has been initialized. Otherwise we get
        //  called before memory routines have been set up, etc. and possibly crash.
        if (g_vogl_has_been_initialized)
        {
            // If this file hadn't been loaded before, notify btrace.
            if (!is_loaded && dlopen_ret)
                btrace_dlopen_notify(pFile);
        }

        return dlopen_ret;
    }

    //----------------------------------------------------------------------------------------------------------------------
    // Intercept _exit() so we get a final chance to flush our trace/log files
    //----------------------------------------------------------------------------------------------------------------------
    typedef void (*exit_func_ptr_t)(int status);
    VOGL_API_EXPORT VOGL_NORETURN void _exit(int status)
    {
        static exit_func_ptr_t s_pActual_exit = (exit_func_ptr_t)dlsym(RTLD_NEXT, "_exit");

        vogl_deinit();

        if (s_pActual_exit)
            (*s_pActual_exit)(status);

        raise(SIGKILL);

        // to shut up compiler about this func marked as noreturn
        for (;;)
            ;
    }

    //----------------------------------------------------------------------------------------------------------------------
    // Intercept _exit() so we get a final chance to flush our trace/log files
    //----------------------------------------------------------------------------------------------------------------------
    typedef void (*Exit_func_ptr_t)(int status);
    VOGL_API_EXPORT VOGL_NORETURN void _Exit(int status)
    {
        static Exit_func_ptr_t s_pActual_Exit = (Exit_func_ptr_t)dlsym(RTLD_NEXT, "_Exit");

        vogl_deinit();

        if (s_pActual_Exit)
            (*s_pActual_Exit)(status);

        raise(SIGKILL);

        // to shut up compiler about this func marked as noreturn
        for (;;)
            ;
    }
#endif
//----------------------------------------------------------------------------------------------------------------------
// vogl_glInternalTraceCommandRAD_dummy_func
//----------------------------------------------------------------------------------------------------------------------
static void vogl_glInternalTraceCommandRAD_dummy_func(GLuint cmd, GLuint size, const GLubyte *data)
{
    // Nothing to do, this is an internal command used for serialization purposes that only we understand.
    VOGL_NOTE_UNUSED(cmd);
    VOGL_NOTE_UNUSED(size);
    VOGL_NOTE_UNUSED(data);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_proc_address_helper_return_actual
//----------------------------------------------------------------------------------------------------------------------
vogl_void_func_ptr_t vogl_get_proc_address_helper_return_actual(const char *pName)
{
    if (!strcmp(pName, "glInternalTraceCommandRAD"))
        return reinterpret_cast<vogl_void_func_ptr_t>(vogl_glInternalTraceCommandRAD_dummy_func);

    vogl_void_func_ptr_t pFunc = reinterpret_cast<vogl_void_func_ptr_t>(plat_dlsym(g_vogl_actual_libgl_module_handle ? g_vogl_actual_libgl_module_handle : PLAT_RTLD_NEXT, pName));

    #if (VOGL_PLATFORM_HAS_GLX)
        if ((!pFunc) && (g_vogl_actual_gl_entrypoints.m_glXGetProcAddress))
            pFunc = reinterpret_cast<vogl_void_func_ptr_t>(g_vogl_actual_gl_entrypoints.m_glXGetProcAddress(reinterpret_cast<const GLubyte *>(pName)));
    #elif 0 && (VOGL_PLATFORM_HAS_WGL)
        if ((!pFunc) && (g_vogl_actual_gl_entrypoints.m_wglGetProcAddress))
            pFunc = reinterpret_cast<vogl_void_func_ptr_t>(g_vogl_actual_gl_entrypoints.m_wglGetProcAddress(reinterpret_cast<const GLubyte *>(pName)));
    #else
        VOGL_VERIFY(!"impl wgl functions in g_vogl_actual_gl_entrypoints");
    #endif

    return pFunc;
}

//----------------------------------------------------------------------------------------------------------------------
// global constructor init
// Note: Be VERY careful what you do in here! It's called very early during init (long before main, during c++ init)
//----------------------------------------------------------------------------------------------------------------------
#if defined(PLATFORM_LINUX)
    VOGL_CONSTRUCTOR_FUNCTION(vogl_shared_object_constructor_func);
    static void vogl_shared_object_constructor_func()
    {
        vogl_init();
    }

    __attribute__((destructor)) static void vogl_shared_object_destructor_func()
    {
        if (vogl_is_capturing())
        {
            // This shouldn't ever happen hopefully. Ie, our atexit() handler or exit() hooks should call vogl_deinit()
            //  well before this destructor is called.
            vogl_warning_printf_once("ERROR: %s called with open trace file. Somehow vogl_deinit() hasn't been called?", VOGL_FUNCTION_NAME);

            // Try to do vogl_deinit().
            vogl_deinit();
        }
    }
#endif
