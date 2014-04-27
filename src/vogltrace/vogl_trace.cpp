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
#include <linux/unistd.h>
#include <execinfo.h>
#include "btrace.h"

//----------------------------------------------------------------------------------------------------------------------
// Has exception hooks, dlopen() interception, super low-level global init.  Be super careful what you
//  do in some of these funcs (particularly vogl_shared_object_constructor_func() or any of the funcs
//  it calls).  I've found it's possible to do shit here that will work fine in primitive apps (like
//  vogltest) but kill tracing in some real apps.
//----------------------------------------------------------------------------------------------------------------------

#define VOGL_LIBGL_SO_FILENAME "libGL.so.1"

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
void *g_vogl_actual_libgl_module_handle;
bool g_vogl_initializing_flag;

//----------------------------------------------------------------------------------------------------------------------
// dlopen interceptor
//----------------------------------------------------------------------------------------------------------------------
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
VOGL_API_EXPORT void _exit(int status)
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
VOGL_API_EXPORT void _Exit(int status)
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
// vogl_get_proc_address_helper
//----------------------------------------------------------------------------------------------------------------------
static vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName)
{
    if (!strcmp(pName, "glInternalTraceCommandRAD"))
        return reinterpret_cast<vogl_void_func_ptr_t>(vogl_glInternalTraceCommandRAD_dummy_func);

    vogl_void_func_ptr_t pFunc = reinterpret_cast<vogl_void_func_ptr_t>(dlsym(g_vogl_actual_libgl_module_handle ? g_vogl_actual_libgl_module_handle : RTLD_NEXT, pName));

    if ((!pFunc) && (g_vogl_actual_gl_entrypoints.m_glXGetProcAddress))
        pFunc = reinterpret_cast<vogl_void_func_ptr_t>(g_vogl_actual_gl_entrypoints.m_glXGetProcAddress(reinterpret_cast<const GLubyte *>(pName)));

    return pFunc;
}

//----------------------------------------------------------------------------------------------------------------------
// global constructor init
// Note: Be VERY careful what you do in here! It's called very early during init (long before main, during c++ init)
//----------------------------------------------------------------------------------------------------------------------
__attribute__((constructor)) static void vogl_shared_object_constructor_func()
{
    //printf("vogl_shared_object_constructor_func\n");

    // Doesn't seem necessary to do this - our global constructor gets called earlier, just being safe.
    vogl_init_heap();

    g_vogl_initializing_flag = true;

    // can't call vogl::colorized_console::init() because its global arrays will be cleared after this func returns
    vogl::console::set_tool_prefix("(vogltrace) ");

    vogl_message_printf("%s built %s %s, begin initialization in %s\n", btrace_get_current_module(), __DATE__, __TIME__, getenv("_"));

    // We can't use the regular cmd line parser here because this func is called before global objects are constructed.
    char *pEnv_cmd_line = getenv("VOGL_CMD_LINE");
    bool pause = vogl::check_for_command_line_param("-vogl_pause") || ((pEnv_cmd_line) && (strstr(pEnv_cmd_line, "-vogl_pause") != NULL));
    bool long_pause = vogl::check_for_command_line_param("-vogl_long_pause") || ((pEnv_cmd_line) && (strstr(pEnv_cmd_line, "-vogl_long_pause") != NULL));
    if (pause || long_pause)
    {
        int count = 60000;
        bool debugger_connected = false;
        dynamic_string cmdline = vogl::get_command_line();

        vogl_message_printf("cmdline: %s\n", cmdline.c_str());
        vogl_message_printf("Pausing %d ms or until debugger is attached (pid %d).\n", count, getpid());
        vogl_message_printf("  Or press any key to continue.\n");

        while (count >= 0)
        {
            vogl_sleep(200);
            count -= 200;
            debugger_connected = vogl_is_debugger_present();
            if (debugger_connected || vogl_kbhit())
                break;
        }

        if (debugger_connected)
            vogl_message_printf("  Debugger connected...\n");
    }

    g_vogl_actual_gl_entrypoints.m_glXGetProcAddress = reinterpret_cast<glXGetProcAddress_func_ptr_t>(dlsym(RTLD_NEXT, "glXGetProcAddress"));
    if (!g_vogl_actual_gl_entrypoints.m_glXGetProcAddress)
    {
        vogl_warning_printf("%s: dlsym(RTLD_NEXT, \"glXGetProcAddress\") failed, trying to manually load %s\n", VOGL_FUNCTION_NAME, VOGL_LIBGL_SO_FILENAME);

        g_vogl_actual_libgl_module_handle = dlopen(VOGL_LIBGL_SO_FILENAME, RTLD_LAZY);
        if (!g_vogl_actual_libgl_module_handle)
        {
            vogl_error_printf("%s: Failed loading %s!\n", VOGL_FUNCTION_NAME, VOGL_LIBGL_SO_FILENAME);
            exit(EXIT_FAILURE);
        }

        g_vogl_actual_gl_entrypoints.m_glXGetProcAddress = reinterpret_cast<glXGetProcAddress_func_ptr_t>(dlsym(g_vogl_actual_libgl_module_handle, "glXGetProcAddress"));
        if (!g_vogl_actual_gl_entrypoints.m_glXGetProcAddress)
        {
            vogl_error_printf("%s: Failed getting address of glXGetProcAddress() from %s!\n", VOGL_FUNCTION_NAME, VOGL_LIBGL_SO_FILENAME);
            exit(EXIT_FAILURE);
        }

        vogl_message_printf("%s: Manually loaded %s\n", VOGL_FUNCTION_NAME, VOGL_LIBGL_SO_FILENAME);
    }

    vogl_common_lib_early_init();

    vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper);

    vogl_early_init();

    vogl_message_printf("end initialization\n");

    g_vogl_initializing_flag = false;
}

__attribute__((destructor)) static void vogl_shared_object_destructor_func()
{
    vogl_deinit();
}
