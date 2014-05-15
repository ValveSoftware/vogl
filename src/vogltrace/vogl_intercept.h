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

// File: vogl_intercept.h
#ifndef VOGL_INTERCEPT_H
#define VOGL_INTERCEPT_H

#include "vogl_command_line_params.h"

// Globals
extern bool g_dump_gl_calls_flag;
extern bool g_dump_gl_buffers_flag;
extern bool g_dump_gl_shaders_flag;
extern bool g_disable_gl_program_binary_flag;
extern bool g_null_mode;
extern bool g_backtrace_all_calls;
extern bool g_vogl_has_been_initialized;

// Functions
void vogl_init();
void vogl_early_init();
void vogl_deinit();

class vogl_thread_local_data;
void vogl_destroy_thread_local_data(vogl_thread_local_data *pData);

// Capture status callback:
//   pFilename will be either NULL on failure, or a pointer to a valid filename on success
//   pOpaque is the user provided pointer
// Notes:
// It's *not* guaranteed that this func will be called at all. If it is, it'll be called asynchronously on whatever thread called glXSwapBuffers().
typedef void (*vogl_capture_status_callback_func_ptr)(const char *pFilename, void *pOpaque);

// vogl_capture_on_next_swap() will cause the tracer to snapshot the state of all GL contexts, then capture the next X frames to a trace file.
// total_frames: number of frames to capture, set to cUINT32_MAX to capture all frames until the app exits.
// pPath and pBase_filename are both optional:
//   If pPath is not NULL, it overrides whatever trace path was specified by the "--vogl_tracepath X" env command line.
//   If pBase_filename is not NULL, it overrides the default base filename, which is "capture_".
// If not NULL, pStatus_callback will be asynchronously called when the capture succeeds or fails.
// There is *no guarantee* that this status func will actually be called - it depends on the client's GL/GLX usage. But the tracer will try pretty hard to close the trace at exit, so there's a high probability it'll be called.
// Returns true if the capture was successfully queued up, false if it's not possible to queue up a capture.
bool vogl_capture_on_next_swap(uint32_t total_frames, const char *pPath, const char *pBase_filename, vogl_capture_status_callback_func_ptr pStatus_callback, void *pStatus_callback_opaque);

// Causes the trace to close as soon as the app calls SwapBuffers(), or false if tracing is not active.
// Note the status callbackwill version always overrides any status callback set via vogl_capture_on_next_swap().
bool vogl_stop_capturing();
bool vogl_stop_capturing(vogl_capture_status_callback_func_ptr pStatus_callback, void *pStatus_callback_opaque);

// Returns true if a full-stream or triggered capturing is currently active.
bool vogl_is_capturing();

#endif // VOGL_INTERCEPT_H
