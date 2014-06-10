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

struct tracer_cmdline_opts_t
{
    const char *name;
    unsigned int num_values;
    const char *desc;
    int group;
    const char *arg_name;
};

static const tracer_cmdline_opts_t g_tracer_cmdline_opts[] =
{
    { "vogl_tracefile", 1, "Tracefile name.", 10, "TRACEFILE" },
    { "vogl_logfile", 1, "Spew output to logfile.", 10, "LOGFILE" },

    { "vogl_pause", 0, "Pause until debugger attaches.", 20, NULL },
    { "vogl_quiet", 0, "Don't print vogl warnings.", 20, NULL },
    { "vogl_verbose", 0, "Print verbose messages.", 20, NULL },
    { "vogl_debug", 0, "Print debug messages and run extra validation code.", 20, NULL },

    { "vogl_force_debug_context", 0, "Use OpenGL debug context.", 30, NULL },

    { "vogl_backtrace_all_calls", 0, "Get backtraces for all gl calls.", 30, NULL },
    { "vogl_backtrace_no_calls", 0, "Don't backtrace any calls.", 30, NULL },

    { "vogl_dump_gl_calls", 0, "Dump all gl calls.", 40, NULL },
    { "vogl_dump_gl_buffers", 0, "Dump all gl buffers.", 40, NULL },
    { "vogl_dump_gl_shaders", 0, "Dump all gl shaders.", 40, NULL },
    { "vogl_dump_gl_full", 0, "Dump all gl calls, buffers, and shaders.", 40, NULL },
    { "vogl_sleep_at_startup", 1, "Sleep for X seconds at startup.", 40, "TIME" },
    { "vogl_flush_files_after_each_call", 0, "Flush trace files after each packet write.", 40, NULL },
    { "vogl_flush_files_after_each_swap", 0, "Flush trace files after glXSwapBuffer.", 40, NULL },
    { "vogl_disable_signal_interception", 0, "Don't set exception handler.", 40, NULL },
    { "vogl_tracepath", 1, "Default tracefile path.", 40, NULL },
    { "vogl_dump_png_screenshots", 0, "Save png screenshots.", 40, NULL },
    { "vogl_dump_jpeg_screenshots", 0, "Save jpeg screenshots.", 40, NULL },
    { "vogl_jpeg_quality", 0, "Screenshot jpeg quality.", 40, NULL },
    { "vogl_screenshot_prefix", 1, "Screenshot prefix", 40, "PREFIX" },
    { "vogl_hash_backbuffer", 0, "Do crc64 for framebuffer.", 40, NULL },
    { "vogl_sum_hashing", 0, "Do sum64 for framebuffer.", 40, NULL },
    { "vogl_dump_backbuffer_hashes", 1, "Print backbuffer hash values.", 40, "FILENAME" },
    { "vogl_null_mode", 0, "Simple null renderer.", 40, NULL },
    { "vogl_disable_client_side_array_tracing", 0, "Disable client side arrays which can impact performance.", 40, NULL },
    { "vogl_disable_gl_program_binary", 0, "Remove GL_ARB_get_program_binary.", 40, NULL },
    { "vogl_exit_after_x_frames", 1, "Call exit() after X frames.", 40, "COUNT" },
#ifdef VOGL_REMOTING
    { "vogl_traceport", 1, "Vogl remote port.", 50 },
#endif
#if VOGL_FUNCTION_TRACING
    { "vogl_func_tracing", 0, "Print called vogl functions.", 50 },
#endif
};
