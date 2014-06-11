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

static const command_line_param_desc g_tracer_cmdline_opts[] =
{
    { "vogl_tracefile", 1, false, "Tracefile name." },
    { "vogl_logfile", 1, false, "Spew output to logfile." },

    { "vogl_pause", 0, false, "Pause until debugger attaches." },
    { "vogl_quiet", 0, false, "Don't print vogl warnings." },
    { "vogl_verbose", 0, false, "Print verbose messages." },
    { "vogl_debug", 0, false, "Print debug messages and run extra validation code." },

    { "vogl_force_debug_context", 0, false, "Use OpenGL debug context." },

    { "vogl_backtrace_all_calls", 0, false, "Get backtraces for all gl calls." },
    { "vogl_backtrace_no_calls", 0, false, "Don't backtrace any calls." },

    { "vogl_dump_gl_calls", 0, false, "Dump all gl calls." },
    { "vogl_dump_gl_buffers", 0, false, "Dump all gl buffers." },
    { "vogl_dump_gl_shaders", 0, false, "Dump all gl shaders." },
    { "vogl_dump_gl_full", 0, false, "Dump all gl calls, buffers, and shaders." },
    { "vogl_sleep_at_startup", 1, false, "Sleep for X seconds at startup." },
    { "vogl_flush_files_after_each_call", 0, false, "Flush trace files after each packet write." },
    { "vogl_flush_files_after_each_swap", 0, false, "Flush trace files after glXSwapBuffer." },
    { "vogl_disable_signal_interception", 0, false, "Don't set exception handler." },
    { "vogl_tracepath", 1, false, "Default tracefile path." },
    { "vogl_dump_png_screenshots", 0, false, "Save png screenshots." },
    { "vogl_dump_jpeg_screenshots", 0, false, "Save jpeg screenshots." },
    { "vogl_jpeg_quality", 1, false, "Screenshot jpeg quality." },
    { "vogl_screenshot_prefix", 1, false, "Screenshot prefix" },
    { "vogl_hash_backbuffer", 0, false, "Do crc64 for framebuffer." },
    { "vogl_sum_hashing", 0, false, "Do sum64 for framebuffer." },
    { "vogl_dump_backbuffer_hashes", 1, false, "Print backbuffer hash values." },
    { "vogl_null_mode", 0, false, "Simple null renderer." },
    { "vogl_disable_client_side_array_tracing", 0, false, "Disable client side arrays which can impact performance." },
    { "vogl_disable_gl_program_binary", 0, false, "Remove GL_ARB_get_program_binary." },
    { "vogl_exit_after_x_frames", 1, false, "Call exit() after X frames." },
#ifdef VOGL_REMOTING
    { "vogl_traceport", 1, false, "Vogl remote port." },
#endif
#if VOGL_FUNCTION_TRACING
    { "vogl_func_tracing", 0, false, "Print called vogl functions." },
#endif
};
