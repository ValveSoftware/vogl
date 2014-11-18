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

// File: vogl_gl_replayer.cpp
#include "vogl_gl_replayer.h"
#include "vogl_general_context_state.h"
#include "vogl_sync_object.h"
#include "vogl_trace_file_writer.h"
#include "vogl_texture_format.h"
#include "gl_glx_cgl_wgl_replay_helper_macros.inc"
#include "vogl_backtrace.h"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vogl_miniz.h"

#include "vogl_timer.h"
#include "vogl_file_utils.h"
#include "vogl_map.h"
#include "vogl_vector.h"

#define VOGL_GL_REPLAYER_ARRAY_OVERRUN_BYTE_MAGIC 129
#define VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC 0x12345678
#define VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC -999999.0f

static void *g_actual_libgl_module_handle;

// TODO: Move this declaration into a header that we share with the location the code actually exists.
vogl_void_func_ptr_t vogl_get_proc_address_helper_return_actual(const char *pName);

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_proc_address_helper
//----------------------------------------------------------------------------------------------------------------------
vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName)
{
    VOGL_FUNC_TRACER

        vogl_void_func_ptr_t pFunc = g_actual_libgl_module_handle ? reinterpret_cast<vogl_void_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, pName)) : NULL;
#if (VOGL_PLATFORM_HAS_GLX)
    if ((!pFunc) && (GL_ENTRYPOINT(glXGetProcAddress)))
        pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(glXGetProcAddress)(reinterpret_cast<const GLubyte *>(pName)));

#elif (VOGL_PLATFORM_HAS_CGL)
	VOGL_ASSERT(!"UNIMPLEMENTED vogl_get_proc_address_helper");

#elif (VOGL_PLATFORM_HAS_WGL)
    if ((!pFunc) && (GL_ENTRYPOINT(wglGetProcAddress)))
        pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(wglGetProcAddress)(pName));
#else
#error "Implement vogl_get_proc_address_helper this platform."
#endif

    return pFunc;
}

//----------------------------------------------------------------------------------------------------------------------
// load gl entrypoints
//----------------------------------------------------------------------------------------------------------------------
bool load_gl()
{
    VOGL_FUNC_TRACER

        g_actual_libgl_module_handle = plat_load_system_gl(PLAT_RTLD_LAZY);

    if (!g_actual_libgl_module_handle)
    {
        vogl_error_printf("Failed loading %s!\n", plat_get_system_gl_module_name());
        return false;
    }

#if (VOGL_PLATFORM_HAS_GLX)
    GL_ENTRYPOINT(glXGetProcAddress) = reinterpret_cast<glXGetProcAddress_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, "glXGetProcAddress"));
    if (!GL_ENTRYPOINT(glXGetProcAddress))
    {
        vogl_error_printf("Failed getting address of glXGetProcAddress() from %s!\n", plat_get_system_gl_module_name());
        return false;
    }

#elif (VOGL_PLATFORM_HAS_CGL)
	VOGL_ASSERT(!"UNIMPLEMENTED load_gl");
	return(false);

#elif (VOGL_PLATFORM_HAS_WGL)
    GL_ENTRYPOINT(wglGetProcAddress) = reinterpret_cast<wglGetProcAddress_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, "wglGetProcAddress"));
    if (!GL_ENTRYPOINT(wglGetProcAddress))
    {
        vogl_error_printf("Failed getting address of wglGetProcAddress() from %s!\n", plat_get_system_gl_module_name());
        return false;
    }
#else
#error "Implement load_gl for this platform."
#endif

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// glInterleavedArrays helper table
//----------------------------------------------------------------------------------------------------------------------
struct interleaved_array_desc_entry_t
{
    uint32_t fmt;
    bool et;
    bool ec;
    bool en;
    uint32_t st;
    uint32_t sc;
    uint32_t sv;
    uint32_t tc;
    uint32_t pc;
    uint32_t pn;
    uint32_t pv;
    uint32_t s;
};

#define _2f (sizeof(GLfloat) * 2)
#define _3f (sizeof(GLfloat) * 3)
#define _4f (sizeof(GLfloat) * 4)
#define _5f (sizeof(GLfloat) * 5)
#define _6f (sizeof(GLfloat) * 6)
#define _7f (sizeof(GLfloat) * 7)
#define _8f (sizeof(GLfloat) * 8)
#define _9f (sizeof(GLfloat) * 9)
#define _10f (sizeof(GLfloat) * 10)
#define _11f (sizeof(GLfloat) * 11)
#define _12f (sizeof(GLfloat) * 12)
#define _15f (sizeof(GLfloat) * 15)
#define _c (sizeof(GL_UNSIGNED_BYTE) * 4)

static const interleaved_array_desc_entry_t vogl_g_interleaved_array_descs[] =
    {
        // format				et 		ec			en       st sc sv tc							pc  pn  pv 		 s
        { GL_V2F, false, false, false, 0, 0, 2, 0, 0, 0, 0, _2f },
        { GL_V3F, false, false, false, 0, 0, 3, 0, 0, 0, 0, _3f },
        { GL_C4UB_V2F, false, true, false, 0, 4, 2, GL_UNSIGNED_BYTE, 0, 0, _c, _c + _2f },
        { GL_C4UB_V3F, false, true, false, 0, 4, 3, GL_UNSIGNED_BYTE, 0, 0, _c, _c + _3f },
        { GL_C3F_V3F, false, true, false, 0, 3, 3, GL_FLOAT, 0, 0, _3f, _6f },
        { GL_N3F_V3F, false, false, true, 0, 0, 3, 0, 0, 0, _3f, _6f },
        { GL_C4F_N3F_V3F, false, true, true, 0, 4, 3, GL_FLOAT, 0, _4f, _7f, _10f },
        { GL_T2F_V3F, true, false, false, 2, 0, 3, 0, 0, 0, _2f, _5f },
        { GL_T4F_V4F, true, false, false, 4, 0, 4, 0, 0, 0, _4f, _8f },
        { GL_T2F_C4UB_V3F, true, true, false, 2, 4, 3, GL_UNSIGNED_BYTE, _2f, 0, _c + _2f, _c + _5f },
        { GL_T2F_C3F_V3F, true, true, false, 2, 3, 3, GL_FLOAT, 0, _2f, _5f, _8f },
        { GL_T2F_N3F_V3F, true, false, true, 2, 0, 3, 0, 0, _2f, _5f, _8f },
        { GL_T2F_C4F_N3F_V3F, true, true, true, 2, 4, 3, GL_FLOAT, _2f, _6f, _9f, _12f },
        { GL_T4F_C4F_N3F_V4F, true, true, true, 4, 4, 4, GL_FLOAT, _4f, _8f, _11f, _15f }
    };

#undef _2f
#undef _3f
#undef _4f
#undef _5f
#undef _6f
#undef _7f
#undef _8f
#undef _9f
#undef _10f
#undef _11f
#undef _12f
#undef _15f
#undef _c

#define VOGL_INTERLEAVED_ARRAY_SIZE (sizeof(vogl_g_interleaved_array_descs) / sizeof(vogl_g_interleaved_array_descs[0]))

//----------------------------------------------------------------------------------------------------------------------
static vogl_trace_ptr_value get_trace_context_from_packet(const vogl_trace_packet& pkt)
{
    const gl_entrypoint_id_t entrypoint_id = pkt.get_entrypoint_id();

    int param_index = 0;
    switch (entrypoint_id)
    {
    case VOGL_ENTRYPOINT_glXMakeCurrent:            param_index = 2; break;
    case VOGL_ENTRYPOINT_glXMakeContextCurrent:     param_index = 3; break;
    case VOGL_ENTRYPOINT_wglMakeCurrent:            param_index = 1; break;
    default:
        vogl_error_printf("Unexpected packet type (%s) in get_trace_context_from_packet, review for possible bug.\n", 
                          pkt.get_entrypoint_desc().m_pName);
        return (vogl_trace_ptr_value)0;
        break;
    }

    return pkt.get_param_ptr_value(param_index);
}


//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::vogl_gl_replayer
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::vogl_gl_replayer()
    : m_flags(0),
      m_swap_sleep_time(0),
      m_dump_framebuffer_on_draw_prefix("screenshot"),
      m_screenshot_prefix("screenshot"),
      m_dump_framebuffer_on_draw_frame_index(-1),
      m_dump_framebuffer_on_draw_first_gl_call_index(-1),
      m_dump_framebuffer_on_draw_last_gl_call_index(-1),
      m_allow_snapshot_restoring(true),
      m_ctypes_packet(&m_trace_gl_ctypes),
      m_trace_pointer_size_in_bytes(0),
      m_trace_pointer_size_in_uints(0),
      m_temp_gl_packet(&m_trace_gl_ctypes),
      m_temp2_gl_packet(&m_trace_gl_ctypes),
      m_pCur_gl_packet(NULL),
      m_pWindow(NULL),
      m_pending_make_current_packet(&m_trace_gl_ctypes),
      m_pending_window_resize_width(0),
      m_pending_window_resize_height(0),
      m_pending_window_resize_attempt_counter(false),
      m_frame_index(0),
      m_total_swaps(0),
      m_last_parsed_call_counter(-1),
      m_last_processed_call_counter(-1),
      m_cur_trace_context(0),
      m_cur_replay_context(NULL),
      m_pCur_context_state(NULL),
      m_frame_draw_counter(0),
      m_frame_draw_counter_kill_threshold(cUINT64_MAX),
      m_is_valid(false),
      m_pBlob_manager(NULL),
      m_pPending_snapshot(NULL),
      m_delete_pending_snapshot_after_applying(false),
      m_proc_address_helper_func(NULL),
      m_wrap_all_gl_calls(false),
      m_replay_to_trace_remapper(*this)

{
    VOGL_FUNC_TRACER

    m_trace_gl_ctypes.init();
    m_fs_pp = vogl_fs_preprocessor::get_instance();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::~vogl_gl_replayer
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::~vogl_gl_replayer()
{
    VOGL_FUNC_TRACER

    deinit();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::init
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::init(uint32_t flags, vogl_replay_window *pWindow, const vogl_trace_stream_start_of_file_packet &sof_packet, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    if (m_is_valid)
        deinit();

    if ((!pWindow) || (!pWindow->is_opened()))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((sof_packet.m_pointer_sizes != sizeof(uint32_t)) && (sof_packet.m_pointer_sizes != sizeof(uint64_t)))
    {
        vogl_error_printf("Invalid trace pointer size (%u)\n", m_sof_packet.m_pointer_sizes);
        return false;
    }

    m_pBlob_manager = &blob_manager;
    m_flags = flags;
    m_pWindow = pWindow;

    m_sof_packet = sof_packet;
    m_trace_pointer_size_in_bytes = m_sof_packet.m_pointer_sizes;
    m_trace_pointer_size_in_uints = m_sof_packet.m_pointer_sizes / sizeof(uint32_t);

    m_trace_gl_ctypes.init();
    m_trace_gl_ctypes.change_pointer_sizes(m_trace_pointer_size_in_bytes);

    if (!m_pWindow->is_opened())
    {
        const uint32_t initial_window_width = 1024;
        const uint32_t initial_window_height = 768;
        if (!m_pWindow->open(initial_window_width, initial_window_height))
        {
            vogl_error_printf("Failed opening window!\n");
            return false;
        }
    }

    m_pCur_gl_packet = NULL;

    m_frame_index = 0;
    m_total_swaps = 0;
    m_last_parsed_call_counter = 0;
    m_last_processed_call_counter = 0;

    m_pending_make_current_packet.clear();
    m_pending_window_resize_width = 0;
    m_pending_window_resize_height = 0;
    m_pending_window_resize_attempt_counter = 0;

    m_at_frame_boundary = true;

    m_cur_trace_context = 0;
    m_cur_replay_context = 0;
    m_pCur_context_state = NULL;

    m_contexts.clear();

    m_frame_draw_counter = 0;
    m_frame_draw_counter_kill_threshold = cUINT64_MAX;

    m_is_valid = true;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::deinit
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::deinit()
{
    VOGL_FUNC_TRACER

    destroy_pending_snapshot();
    destroy_contexts();

    // TODO: Make a 1st class snapshot cache class
    for (uint32_t i = 0; i < m_snapshots.size(); i++)
        vogl_delete(m_snapshots[i].m_pSnapshot);
    m_snapshots.clear();

    m_ctypes_packet.reset();

    m_pCur_gl_packet = NULL;

    m_frame_index = 0;
    m_total_swaps = 0;
    m_last_parsed_call_counter = 0;
    m_last_processed_call_counter = 0;

    m_pending_make_current_packet.clear();
    m_pending_window_resize_width = 0;
    m_pending_window_resize_height = 0;
    m_pending_window_resize_attempt_counter = 0;

    m_at_frame_boundary = true;

    m_cur_trace_context = 0;
    m_cur_replay_context = 0;
    m_pCur_context_state = NULL;

    m_frame_draw_counter = 0;
    m_frame_draw_counter_kill_threshold = cUINT32_MAX;

    m_pBlob_manager = NULL;

    m_flags = 0;
    m_swap_sleep_time = 0;
    m_dump_framebuffer_on_draw_prefix = "screenshot";
    m_screenshot_prefix = "screenshot";
    m_backbuffer_hash_filename.clear();
    m_dump_framebuffer_on_draw_frame_index = -1;
    m_dump_framebuffer_on_draw_first_gl_call_index = -1;
    m_dump_framebuffer_on_draw_last_gl_call_index = -1;
    m_fs_pp->reset();

    m_dump_frontbuffer_filename.clear();

    m_is_valid = false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_trace_gl_packet_debug_info
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::dump_trace_gl_packet_debug_info(const vogl_trace_gl_entrypoint_packet &gl_packet)
{
    VOGL_FUNC_TRACER

    vogl_debug_printf("Trace packet: Total size %u, Param size: %u, Client mem size %u, Name value size %u, call %" PRIu64 ", ID: %s (%u), Thread ID: 0x%" PRIX64 ", Trace Context: 0x%" PRIX64 "\n",
                     gl_packet.m_size,
                     gl_packet.m_param_size,
                     gl_packet.m_client_memory_size,
                     gl_packet.m_name_value_map_size,
                     gl_packet.m_call_counter,
                     g_vogl_entrypoint_descs[gl_packet.m_entrypoint_id].m_pName,
                     gl_packet.m_entrypoint_id,
                     gl_packet.m_thread_id,
                     gl_packet.m_context_handle);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_packet_as_func_call
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::dump_packet_as_func_call(const vogl_trace_packet& trace_packet)
{
    VOGL_FUNC_TRACER

    dynamic_string str;
    str.reserve(128);
    if (!trace_packet.pretty_print(str, true))
        vogl_error_printf("packet pretty print failed!\n");
    else
        vogl_debug_printf("%s\n", str.get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::has_pending_packets
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::has_pending_packets()
{
    return (m_pPending_snapshot ||
            m_pending_make_current_packet.is_valid());
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_next_packet
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_pending_packets()
{
    return process_pending_gl_entrypoint_packets();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_next_packet
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_next_packet(const vogl_trace_packet &gl_packet)
{
    // TODO: Fix const correctness
    return process_gl_entrypoint_packet((vogl_trace_packet &)gl_packet);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_next_packet
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_next_packet(vogl_trace_file_reader &trace_reader)
{
    VOGL_FUNC_TRACER

    vogl_trace_file_reader::trace_file_reader_status_t read_status = trace_reader.read_next_packet();
    if (read_status == vogl_trace_file_reader::cEOF)
    {
        vogl_message_printf("At trace file EOF\n");
        return cStatusAtEOF;
    }
    else if (read_status != vogl_trace_file_reader::cOK)
    {
        vogl_error_printf("Failed reading from trace file\n");
        return cStatusHardFailure;
    }

    status_t status = cStatusOK;

    switch (trace_reader.get_packet_type())
    {
        case cTSPTSOF:
        {
            // just ignore it
            break;
        }
        case cTSPTGLEntrypoint:
        {
            if (!m_temp_gl_packet.deserialize(trace_reader.get_packet_buf().get_ptr(), trace_reader.get_packet_buf().size(), false))
            {
                vogl_error_printf("Failed deserializing GL entrypoint packet\n");
                status = cStatusHardFailure;
                break;
            }

            status = process_pending_packets();

            if (status == cStatusOK)
            {
                status = process_next_packet(m_temp_gl_packet);
            }

            break;
        }
        case cTSPTEOF:
        {
            vogl_verbose_printf("Encountered EOF packet in trace file\n");
            status = cStatusAtEOF;
            break;
        }
        default:
        {
            vogl_error_printf("Encountered unknown packet type in trace file\n");
            status = cStatusSoftFailure;
            break;
        }
    }

    if (status < 0)
    {
        vogl_error_printf("%s failure processing GL entrypoint packet\n",
                          (status == cStatusHardFailure) ? "Hard" : "Soft");
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_pending_window_resize
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_pending_window_resize(bool *pApplied_snapshot)
{
    VOGL_FUNC_TRACER

    if (pApplied_snapshot)
        *pApplied_snapshot = false;

    status_t status = cStatusOK;

    if (get_has_pending_window_resize())
    {
        status = process_frame_check_for_pending_window_resize();
        if (status != cStatusOK)
            return status;
    }

    if (m_pPending_snapshot)
    {
        if (pApplied_snapshot)
            *pApplied_snapshot = true;

        status = process_applying_pending_snapshot();
        if (status != cStatusOK)
            return status;
    }

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_frame
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_frame(vogl_trace_file_reader &trace_reader)
{
    VOGL_FUNC_TRACER

    status_t status = cStatusOK;

    for (;;)
    {
        status = process_next_packet(trace_reader);
        if ((status == cStatusNextFrame) || (status == cStatusResizeWindow) || (status == cStatusAtEOF) || (status == cStatusHardFailure))
            break;
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_event
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::update_window_dimensions()
{
    VOGL_FUNC_TRACER

    m_pWindow->update_dimensions();

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_frontbuffer_screenshot_before_next_swap
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::dump_frontbuffer_screenshot_before_next_swap(const dynamic_string &filename)
{
    m_dump_frontbuffer_filename = filename;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_frontbuffer_to_file
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::dump_frontbuffer_to_file(const dynamic_string &filename)
{
    if ((!m_is_valid) || (!m_pWindow))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    uint32_t width = 0, height = 0;
    m_pWindow->get_actual_dimensions(width, height);

    m_screenshot_buffer.resize(width * height * 3);

    bool success = vogl_copy_buffer_to_image(m_screenshot_buffer.get_ptr(), m_screenshot_buffer.size(), width, height, GL_RGB, GL_UNSIGNED_BYTE, false, 0, GL_FRONT);
    if (!success)
    {
        vogl_error_printf("Failed calling glReadPixels() to take frontbuffer screenshot!\n");
        return false;
    }

    size_t png_size = 0;
    void *pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(m_screenshot_buffer.get_ptr(), width, height, 3, &png_size, 1, true);

    success = file_utils::write_buf_to_file(filename.get_ptr(), pPNG_data, png_size);
    if (!success)
    {
        vogl_error_printf("Failed writing PNG screenshot to file \"%s\"\n", filename.get_ptr());
    }
    else
    {
        vogl_message_printf("Wrote PNG screenshot to file \"%s\"\n", filename.get_ptr());
    }

    mz_free(pPNG_data);

    return success;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::trigger_pending_window_resize
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::trigger_pending_window_resize(uint32_t win_width, uint32_t win_height)
{
    VOGL_FUNC_TRACER

    m_pending_window_resize_width = win_width;
    m_pending_window_resize_height = win_height;
    m_pending_window_resize_attempt_counter = 0;
    m_time_since_pending_window_resize.start();

    m_pWindow->resize(win_width, win_height);

    vogl_verbose_printf("Waiting for window to resize to %ux%u\n", win_width, win_height);

    return cStatusResizeWindow;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::clear_pending_window_resize
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::clear_pending_window_resize()
{
    VOGL_FUNC_TRACER

    m_pending_window_resize_width = 0;
    m_pending_window_resize_height = 0;
    m_pending_window_resize_attempt_counter = 0;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_frame_check_for_pending_window_resize
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_frame_check_for_pending_window_resize()
{
    VOGL_FUNC_TRACER

    const uint32_t cMaxSecsToWait = 5;

    if (!get_has_pending_window_resize())
        return cStatusOK;

    if (m_pending_window_resize_attempt_counter >= cMaxSecsToWait)
    {
        vogl_warning_printf("Waited too long for window to resize to %ux%u, giving up and continuing replay\n", get_pending_window_resize_width(), get_pending_winow_resize_height());

        clear_pending_window_resize();

        return cStatusOK;
    }

    uint32_t win_width = 0, win_height = 0;
    m_pWindow->get_actual_dimensions(win_width, win_height);

    if ((win_width != get_pending_window_resize_width()) ||
        (win_height != get_pending_winow_resize_height()))
    {
        if (m_time_since_pending_window_resize.get_elapsed_secs() < 1.0f)
        {
            // sleep 1ms, then retry
            vogl_sleep(1);
            return cStatusResizeWindow;
        }

        // What could possibly go wrong?
        m_pending_window_resize_attempt_counter++;
        if (m_pending_window_resize_attempt_counter < cMaxSecsToWait)
        {
            m_pWindow->resize(get_pending_window_resize_width(), get_pending_winow_resize_height());

            m_time_since_pending_window_resize.start();

            vogl_warning_printf("Waiting up to 5 secs for window to resize to %ux%u\n", get_pending_window_resize_width(), get_pending_winow_resize_height());
            return cStatusResizeWindow;
        }
    }

    clear_pending_window_resize();
    m_pWindow->update_dimensions();

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::destroy_pending_snapshot
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::destroy_pending_snapshot()
{
    VOGL_FUNC_TRACER

    if (m_pPending_snapshot)
    {
        if (m_delete_pending_snapshot_after_applying)
        {
            // Ensure the snapshot cache can't be pointing to the snapshot, just to be safe.
            // TODO: Make this a real class damn it.
            for (uint32_t i = 0; i < m_snapshots.size(); i++)
            {
                if (m_snapshots[i].m_pSnapshot == m_pPending_snapshot)
                {
                    m_snapshots.erase(i);
                    break;
                }
            }

            vogl_delete(const_cast<vogl_gl_state_snapshot *>(m_pPending_snapshot));
        }

        m_pPending_snapshot = NULL;
    }

    m_delete_pending_snapshot_after_applying = false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::check_gl_error
// Returns *true* on error, just like vogl_check_gl_error()
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::check_gl_error_internal(bool quietly, const char *pFile, uint32_t line, const char *pFunc)
{
    VOGL_FUNC_TRACER

    bool status = false;
    for (;;)
    {
        // http://www.opengl.org/sdk/docs/man/xhtml/glGetError.xml
        // "Thus, glGetError should always be called in a loop, until it returns GL_NO_ERROR, if all error flags are to be reset."
        GLenum gl_err = GL_ENTRYPOINT(glGetError)();
        if (gl_err == GL_NO_ERROR)
            break;

        if (!quietly)
        {
            process_entrypoint_warning("GL error: 0x%08X (%u): %s (Called from File: %s Line: %u Func: %s)\n", gl_err, gl_err, get_gl_enums().find_name("ErrorCode", gl_err), pFile ? pFile : "?", line, pFunc ? pFunc : "?");
        }

        status = true;
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::clear_contexts
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::clear_contexts()
{
    VOGL_FUNC_TRACER

    for (context_hash_map::iterator it = m_contexts.begin(); it != m_contexts.end(); ++it)
    {
        context_state *pContext_state = it->second;
        it->second = NULL;
        vogl_delete(pContext_state);
    }

    m_contexts.clear();

    m_cur_trace_context = 0;
    m_cur_replay_context = NULL;
    m_pCur_context_state = NULL;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::destroy_contexts
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::destroy_contexts()
{
    VOGL_FUNC_TRACER

    #if (VOGL_PLATFORM_HAS_SDL)
        if ((m_contexts.size()) && (m_pWindow->is_opened()))
        {
            m_pWindow->make_current(NULL);

            vogl::vector<context_state *> contexts_to_destroy;
            for (context_hash_map::const_iterator it = m_contexts.begin(); it != m_contexts.end(); ++it)
                contexts_to_destroy.push_back(it->second);

            // Delete "tail" contexts (ones that are not referenced by any other context) in sharegroups first.
            // TODO: I think we could just lazily O(n^2) delete the contexts by picking one that doesn't have a 
            // child (or deleting all of the ones each time that doesn't have at least one child) and then 
            // repeat the process until there are zero contexts. The perf should be fine, we're typically dealing
            // with a small number of contexts.
            while (contexts_to_destroy.size())
            {
                for (int i = 0; i < static_cast<int>(contexts_to_destroy.size()); i++)
                {
                    context_state *pContext_state = contexts_to_destroy[i];

                    vogl_trace_ptr_value trace_context = pContext_state->m_context_desc.get_trace_context();

                    bool skip_context = false;
                    for (int j = 0; j < static_cast<int>(contexts_to_destroy.size()); j++)
                    {
                        if (i == j)
                            continue;

                        if (contexts_to_destroy[j]->m_context_desc.get_trace_share_context() == trace_context)
                        {
                            skip_context = true;
                            break;
                        }
                    }

                    if (skip_context)
                        continue;

                    // This context may have been the sharegroup's root and could have been already deleted.
                    if (!pContext_state->m_deleted)
                    {
                        m_pWindow->destroy_context(pContext_state->m_replay_context);
                    }

                    contexts_to_destroy.erase(i);
                    i--;
                }
            }
        }

    #else
        #error "Need vogl_gl_replayer::destroy_contexts this platform"
    #endif

    clear_contexts();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::define_new_context
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::context_state *vogl_gl_replayer::define_new_context(
    vogl_trace_context_ptr_value trace_context, GLReplayContextType replay_context, vogl_trace_context_ptr_value trace_share_context, GLboolean direct, gl_entrypoint_id_t creation_func, const int *pAttrib_list, uint32_t attrib_list_size)
{
    VOGL_FUNC_TRACER

    if ((!trace_context) || (!replay_context))
    {
        VOGL_ASSERT_ALWAYS;
        return NULL;
    }

    context_state *pContext_state = vogl_new(context_state, *this);

    pContext_state->m_trace_context = trace_context;
    pContext_state->m_replay_context = replay_context;

    pContext_state->m_context_desc.init(creation_func, direct, trace_context, trace_share_context, vogl_context_attribs(pAttrib_list, attrib_list_size));

    if (trace_share_context)
    {
        for (context_hash_map::const_iterator it = m_contexts.begin(); it != m_contexts.end(); ++it)
        {
            if (trace_share_context == it->first)
            {
                context_state *pShare_context = it->second;
                while (!pShare_context->is_root_context())
                    pShare_context = pShare_context->m_pShared_state;

                pContext_state->m_pShared_state = pShare_context;
                pContext_state->m_pShared_state->m_ref_count++;

                break;
            }
        }

        if (!pContext_state->m_pShared_state)
        {
            process_entrypoint_error("Unable to find trace share context handle 0x%" PRIX64 "!\n", trace_share_context);
        }
    }

    m_contexts.insert(trace_context, pContext_state);

    return pContext_state;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::remap_context
//----------------------------------------------------------------------------------------------------------------------
GLReplayContextType vogl_gl_replayer::remap_context(vogl_trace_context_ptr_value trace_context)
{
    VOGL_FUNC_TRACER

    if (!trace_context)
        return NULL;

    context_hash_map::iterator it = m_contexts.find(trace_context);
    return (it == m_contexts.end()) ? NULL : it->second->m_replay_context;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::destroy_context
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::destroy_context(vogl_trace_context_ptr_value trace_context)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(trace_context);

    context_hash_map::iterator it = m_contexts.find(trace_context);
    if (it == m_contexts.end())
        return false;

    context_state *pContext_state = it->second;
    VOGL_ASSERT(pContext_state->m_ref_count >= 1);
    VOGL_ASSERT(!pContext_state->m_deleted);

    if (pContext_state->is_share_context())
    {
        VOGL_ASSERT(pContext_state->m_ref_count == 1);

        context_state *pShare_context = pContext_state->m_pShared_state;
        VOGL_ASSERT(pShare_context->m_ref_count >= 1);

        pShare_context->m_ref_count--;
        if (!pShare_context->m_ref_count)
        {
            VOGL_ASSERT(pShare_context->m_deleted);

            vogl_trace_context_ptr_value trace_share_context = pShare_context->m_context_desc.get_trace_context();

            vogl_delete(pShare_context);

            bool removed = m_contexts.erase(trace_share_context);
            VOGL_NOTE_UNUSED(removed);
            VOGL_ASSERT(removed);
        }

        vogl_delete(pContext_state);

        bool removed = m_contexts.erase(trace_context);
        VOGL_NOTE_UNUSED(removed);
        VOGL_ASSERT(removed);
    }
    else
    {
        pContext_state->m_deleted = true;
        pContext_state->m_ref_count--;

        if (!pContext_state->m_ref_count)
        {
            it->second = NULL;

            vogl_delete(pContext_state);

            bool removed = m_contexts.erase(trace_context);
            VOGL_NOTE_UNUSED(removed);
            VOGL_ASSERT(removed);
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::set_client_side_array_data
// glVertexPointer, glNormalPointer, etc. client side data
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::set_client_side_array_data(const key_value_map &map, GLuint start, GLuint end, GLuint basevertex)
{
    VOGL_FUNC_TRACER

    // TODO: Add early out
    GLint prev_client_active_texture = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &prev_client_active_texture);

    const uint32_t tex_coords = math::minimum<uint32_t>(m_pCur_context_state->m_context_info.is_core_profile() ? m_pCur_context_state->m_context_info.get_max_texture_units() : m_pCur_context_state->m_context_info.get_max_texture_coords(), VOGL_MAX_SUPPORTED_GL_TEXCOORD_ARRAYS);

    for (uint32_t client_array_iter = 0; client_array_iter < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS; client_array_iter++)
    {
        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[client_array_iter];

        const bool is_texcoord_array = (client_array_iter == vogl_texcoord_pointer_array_id);

        uint32_t n = 1;
        uint32_t base_key_index = 0x1000 + client_array_iter;

        // Special case texcoord pointers, which are accessed via the client active texture.
        if (is_texcoord_array)
        {
            n = tex_coords;
            base_key_index = 0x2000;
        }

        for (uint32_t inner_iter = 0; inner_iter < n; inner_iter++)
        {
            uint32_t key_index = base_key_index + inner_iter;

            const uint8_vec *pVertex_blob = map.get_blob(static_cast<uint16_t>(key_index));
            // TODO: Check for case where blob (or map) is not present, but they still access client side data, this is a bad error
            if (!pVertex_blob)
                continue;

            if (is_texcoord_array)
            {
                GL_ENTRYPOINT(glClientActiveTexture)(GL_TEXTURE0 + inner_iter);
            }

            GLboolean is_enabled = GL_ENTRYPOINT(glIsEnabled)(desc.m_is_enabled);
            if (!is_enabled)
                continue;

            GLint binding = 0;
            GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_binding, &binding);
            if (binding)
                continue;

            GLvoid *ptr = NULL;
            GL_ENTRYPOINT(glGetPointerv)(desc.m_get_pointer, &ptr);
            if (!ptr)
                continue;

            uint8_vec &array_data = is_texcoord_array ? m_client_side_texcoord_data[inner_iter] : m_client_side_array_data[client_array_iter];
            if (ptr != array_data.get_ptr())
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }

            GLint type = GL_BOOL;
            if (desc.m_get_type)
            {
                GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_type, &type);
            }

            GLint stride = 0;
            GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_stride, &stride);

            GLint size = 1;
            if (desc.m_get_size)
            {
                GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_size, &size);
            }

            uint32_t type_size = vogl_get_gl_type_size(type);
            if (!type_size)
            {
                process_entrypoint_error("Can't determine type size of enabled client side array set by func %s\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName);
                continue;
            }

            if ((desc.m_entrypoint == VOGL_ENTRYPOINT_glIndexPointer) || (desc.m_entrypoint == VOGL_ENTRYPOINT_glIndexPointerEXT) ||
                (desc.m_entrypoint == VOGL_ENTRYPOINT_glFogCoordPointer) || (desc.m_entrypoint == VOGL_ENTRYPOINT_glFogCoordPointerEXT) ||
                (desc.m_entrypoint == VOGL_ENTRYPOINT_glEdgeFlagPointer) || (desc.m_entrypoint == VOGL_ENTRYPOINT_glEdgeFlagPointerEXT))
            {
                size = 1;
            }
            else if ((desc.m_entrypoint == VOGL_ENTRYPOINT_glNormalPointer) || (desc.m_entrypoint == VOGL_ENTRYPOINT_glNormalPointerEXT))
            {
                size = 3;
            }
            else if ((size < 1) && (size > 4))
            {
                process_entrypoint_error("Size of client side array set by func %s must be between 1 and 4\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName);
                continue;
            }

            if (!stride)
                stride = type_size * size;

            uint32_t first_vertex_ofs = (start + basevertex) * stride;
            uint32_t last_vertex_ofs = (end + basevertex) * stride;
            uint32_t vertex_data_size = (last_vertex_ofs + stride) - first_vertex_ofs;
            uint32_t total_data_size = last_vertex_ofs + stride;

#if 0
			if (!pVertex_blob)
			{
				process_entrypoint_error("Failed finding client side vertex data blob set by func %s \n",  g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName);
				continue;
			}
#endif

            uint8_vec temp_blob;
            if (vertex_data_size != pVertex_blob->size())
            {
                process_entrypoint_error("%s will access more client side data (%u bytes) than stored in the trace (%u bytes), using what is in the trace and using zeros for the rest\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName, vertex_data_size, pVertex_blob->size());
                temp_blob = *pVertex_blob;
                temp_blob.resize(vertex_data_size);
                pVertex_blob = &temp_blob;
            }

            uint32_t bytes_remaining_at_end = math::maximum<int>(0, (int)VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE - (int)first_vertex_ofs);
            uint32_t bytes_to_copy = math::minimum<uint32_t>(pVertex_blob->size(), bytes_remaining_at_end);
            if (bytes_to_copy != pVertex_blob->size())
            {
                // Can't resize buffer, it could move and that would invalidate any VAO pointer bindings.
                process_entrypoint_error("%s accesses too much client side data (%u bytes), increase VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName, first_vertex_ofs + total_data_size);
            }

            VOGL_ASSERT((first_vertex_ofs + bytes_to_copy) <= array_data.size());

            memcpy(array_data.get_ptr() + first_vertex_ofs, pVertex_blob->get_ptr(), bytes_to_copy);
        }
    }

    GL_ENTRYPOINT(glClientActiveTexture)(prev_client_active_texture);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::set_client_side_vertex_attrib_array_data
// glVertexAttrib client side data
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::set_client_side_vertex_attrib_array_data(const key_value_map &map, GLuint start, GLuint end, GLuint basevertex)
{
    VOGL_FUNC_TRACER

    // TODO: Add early out

    for (int vertex_attrib_index = 0; vertex_attrib_index < static_cast<int>(m_pCur_context_state->m_context_info.get_max_vertex_attribs()); vertex_attrib_index++)
    {
        const uint8_vec *pVertex_blob = map.get_blob(static_cast<uint16_t>(vertex_attrib_index));

        // TODO: Check for case where blob (or map) is not present, but they still access client side data, this is a bad error
        if (!pVertex_blob)
            continue;

        GLint enabled = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(vertex_attrib_index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        if (!enabled)
            continue;

        GLint cur_buf = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(vertex_attrib_index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &cur_buf);
        if (cur_buf)
            continue;

        GLvoid *attrib_ptr = NULL;
        GL_ENTRYPOINT(glGetVertexAttribPointerv)(vertex_attrib_index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &attrib_ptr);
        if (!attrib_ptr)
            continue;

        if (attrib_ptr != m_client_side_vertex_attrib_data[vertex_attrib_index].get_ptr())
        {
            VOGL_ASSERT_ALWAYS;
            continue;
        }

        GLint attrib_size = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(vertex_attrib_index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib_size);

        GLint attrib_type = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(vertex_attrib_index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &attrib_type);

        GLint attrib_stride = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(vertex_attrib_index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attrib_stride);

#if 0
		if (!pVertex_blob)
		{
			process_entrypoint_error("Failed finding client side vertex data blob for generic vertex attrib %u\n", vertex_attrib_index);
			continue;
		}
#endif

        uint32_t num_comps = 4;
        if ((attrib_size != GL_BGRA) && (attrib_size < 1) && (attrib_size > 4))
        {
            process_entrypoint_error("Enabled vertex attribute index %i has invalid size 0x%0X\n", vertex_attrib_index, attrib_size);
            continue;
        }
        if ((attrib_size >= 1) && (attrib_size <= 4))
            num_comps = attrib_size;

        uint32_t type_size = vogl_get_gl_type_size(attrib_type);
        if (!type_size)
        {
            process_entrypoint_error("Vertex attribute index %i has unsupported type 0x%0X\n", vertex_attrib_index, attrib_type);
            continue;
        }

        uint32_t stride = attrib_stride ? attrib_stride : (type_size * num_comps);

        uint32_t first_vertex_ofs = (start + basevertex) * stride;
        uint32_t last_vertex_ofs = (end + basevertex) * stride;
        uint32_t vertex_data_size = (last_vertex_ofs + stride) - first_vertex_ofs;
        uint32_t total_data_size = last_vertex_ofs + stride;

        uint8_vec temp_blob;
        if (vertex_data_size != pVertex_blob->size())
        {
            process_entrypoint_error("Vertex attribute index %i will access more client side data (%u bytes) than stored in the trace (%u bytes), using what is in the trace and using zeros for the rest\n", vertex_attrib_index, vertex_data_size, pVertex_blob->size());
            temp_blob = *pVertex_blob;
            temp_blob.resize(vertex_data_size);
            pVertex_blob = &temp_blob;
        }

        uint32_t bytes_remaining_at_end = math::maximum<int>(0, (int)VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE - (int)first_vertex_ofs);
        uint32_t bytes_to_copy = math::minimum<uint32_t>(pVertex_blob->size(), bytes_remaining_at_end);
        if (bytes_to_copy != pVertex_blob->size())
        {
            // Can't resize buffer, it could move and that would invalidate any VAO pointer bindings.
            process_entrypoint_error("Vertex attribute index %i accesses too much client side data (%u bytes), increase VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE\n", vertex_attrib_index, first_vertex_ofs + total_data_size);
        }

        VOGL_ASSERT((first_vertex_ofs + bytes_to_copy) <= m_client_side_vertex_attrib_data[vertex_attrib_index].size());

        memcpy(m_client_side_vertex_attrib_data[vertex_attrib_index].get_ptr() + first_vertex_ofs, pVertex_blob->get_ptr(), bytes_to_copy);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::draw_elements_client_side_array_setup
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::draw_elements_client_side_array_setup(
    GLenum mode,
    GLuint start,
    GLuint end,
    GLsizei count,
    GLenum type,
    vogl_trace_ptr_value trace_indices_ptr_value, const GLvoid *&pIndices,
    GLint basevertex,
    bool has_valid_start_end,
    bool indexed)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(mode);

    pIndices = NULL;

    GLuint element_array_buffer = 0;
    uint32_t index_size = 0;
    if (indexed)
    {
        index_size = vogl_get_gl_type_size(type);
        if (!index_size)
        {
            process_entrypoint_error("Invalid type parameter 0x%08X\n", type);
            return false;
        }

        element_array_buffer = vogl_get_bound_gl_buffer(GL_ELEMENT_ARRAY_BUFFER);
        if (element_array_buffer)
        {
            // trace_indices_ptr_value is actually an offset into the buffer, not a real trace ptr
            pIndices = reinterpret_cast<GLvoid *>(trace_indices_ptr_value);
        }
    }

    if (count <= 0)
    {
        process_entrypoint_warning("count parameter is <= 0 (%i)\n", count);
        return true;
    }

    const key_value_map &map = m_pCur_gl_packet->get_key_value_map();
    if (!map.size())
    {
        if ((indexed) && (!element_array_buffer) && (trace_indices_ptr_value))
        {
            process_entrypoint_error("No element array buffer is bound, but key value map doesn't have an indices blob. Can't remap indices pointer!\n");
            if (trace_indices_ptr_value)
            {
                // We can't remap the pointer to valid memory, so give up.
                return false;
            }
        }

        // TODO: Check for client side array usage but no data in blob (which would be a desync or error)
        return true;
    }

    // TODO: If a VAO is bound, client side data isn't supported according to this:
    // http://www.opengl.org/registry/specs/ARB/vertex_array_object.txt

    //GLint current_vertex_array_binding = 0;
    //ACTUAL_GL_ENTRYPOINT(glGetIntegerv)(GL_VERTEX_ARRAY_BINDING, &current_vertex_array_binding);

    if ((indexed) && (!element_array_buffer))
    {
        const uint8_vec *pIndices_vec = map.get_blob(string_hash("indices"));
        if (!pIndices_vec)
        {
            process_entrypoint_error("No element array buffer is bound, but key value map doesn't have an indices blob\n");
            return false;
        }

        pIndices = pIndices_vec->get_ptr();
        if (!pIndices)
        {
            process_entrypoint_error("No element array buffer is bound, but key value map has an empty indices blob\n");
            return false;
        }

        if ((pIndices_vec->size() / index_size) != static_cast<uint32_t>(count))
        {
            process_entrypoint_error("Client side index data blob stored in packet is too small (wanted %u indices, got %u indices)\n", count, pIndices_vec->size() / index_size);
            return false;
        }
    }

    if ((indexed) && (!has_valid_start_end))
    {
        uint32_t total_index_data_size = count * index_size;

        const uint8_t *pIndices_to_scan = static_cast<const uint8_t *>(pIndices);

        if (element_array_buffer)
        {
            if (m_index_data.size() < total_index_data_size)
                m_index_data.resize(total_index_data_size);

            pIndices_to_scan = m_index_data.get_ptr();

            check_gl_error();

            GL_ENTRYPOINT(glGetBufferSubData)(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)pIndices, total_index_data_size, m_index_data.get_ptr());

            if (check_gl_error())
            {
                process_entrypoint_warning("GL error while trying to retrieve index buffer data\n");

                memset(m_index_data.get_ptr(), 0, total_index_data_size);
            }
        }

        start = cUINT32_MAX;
        end = 0;

        for (int i = 0; i < count; i++)
        {
            uint32_t v;

            if (type == GL_UNSIGNED_BYTE)
                v = pIndices_to_scan[i];
            else if (type == GL_UNSIGNED_SHORT)
                v = reinterpret_cast<const uint16_t *>(pIndices_to_scan)[i];
            else if (type == GL_UNSIGNED_INT)
                v = reinterpret_cast<const uint32_t *>(pIndices_to_scan)[i];
            else
            {
                process_entrypoint_error("Invalid index type\n");
                return false;
            }

            start = math::minimum(start, v);
            end = math::maximum(end, v);
        }

        has_valid_start_end = true;
    }

    if (!set_client_side_array_data(map, start, end, basevertex))
        return false;

    if (!set_client_side_vertex_attrib_array_data(map, start, end, basevertex))
        return false;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::determine_uniform_replay_location
//----------------------------------------------------------------------------------------------------------------------
GLint vogl_gl_replayer::determine_uniform_replay_location(GLuint trace_program, GLint trace_location)
{
    VOGL_FUNC_TRACER

    // Seems better to return -1 when we can't find the uniform (which can happen if the driver optimizes the program differently vs. tracing).
    // Otherwise, we can pass an invalid handle down to the driver and this will crash AMD's fglrx.
    //GLint replay_location = trace_location;
    GLint replay_location = -1;

    glsl_program_hash_map::iterator it = get_shared_state()->m_glsl_program_hash_map.find(trace_program);
    if (it == get_shared_state()->m_glsl_program_hash_map.end())
    {
        process_entrypoint_warning("Failed looking up current trace program in GLSL program hash map\n");
    }
    else
    {
        glsl_program_state &state = it->second;

        uniform_location_hash_map::const_iterator loc_it = state.m_uniform_locations.find(trace_location);
        if (loc_it == state.m_uniform_locations.end())
        {
            process_entrypoint_warning("Failed looking up uniform location index\n");
        }
        else
        {
            replay_location = loc_it->second;
        }
    }
    return replay_location;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_entrypoint_print_summary_context
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::process_entrypoint_print_summary_context(const char *caller_info, eConsoleMessageType msg_type)
{
    VOGL_FUNC_TRACER

    if (!m_pCur_gl_packet)
        return;

    console::printf(caller_info, msg_type | cMsgFlagOpenGL,
                    "While processing GL entrypoint packet func %s, frame %u, swaps %u, GL call counter %" PRIu64
                        ", packet start trace context 0x%" PRIX64 ", cur trace context 0x%" PRIX64 ", trace thread 0x%" PRIX64 ":\n",
                    g_vogl_entrypoint_descs[m_pCur_gl_packet->get_entrypoint_id()].m_pName,
                    m_frame_index, m_total_swaps,
                    m_pCur_gl_packet->get_entrypoint_packet().m_call_counter,
                    m_pCur_gl_packet->get_entrypoint_packet().m_context_handle,
                    m_cur_trace_context,
                    m_pCur_gl_packet->get_entrypoint_packet().m_thread_id);

#if 0
	dynamic_string_array backtrace;
	if (get_printable_backtrace(backtrace))
	{
		vogl_prinf("Backtrace:\n");
		for (uint32_t i = 0; i < backtrace.size(); i++)
			vogl_prinf("%s\n", backtrace[i].get_ptr());
	}
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::print_detailed_context
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::print_detailed_context(const char *caller_info, eConsoleMessageType msg_type)
{
    VOGL_FUNC_TRACER

    json_node node;

    vogl_loose_file_blob_manager blob_file_manager;
    blob_file_manager.init(cBMFWritable);

    vogl_trace_packet::json_serialize_params serialize_params;
    serialize_params.m_output_basename = "replay_error";
    serialize_params.m_cur_frame = m_frame_index;
    serialize_params.m_blob_file_size_threshold = 1024;
    serialize_params.m_pBlob_manager = (m_flags & cGLReplayerDumpPacketBlobFilesOnError) ? &blob_file_manager : NULL;
    m_pCur_gl_packet->json_serialize(node, serialize_params);

    dynamic_string node_str;
    node.serialize(node_str, true, 0);
    console::printf(caller_info, msg_type | cMsgFlagOpenGL, "Packet at call counter %" PRIu64 " as JSON:\n%s\n",
                    m_pCur_gl_packet->get_entrypoint_packet().m_call_counter, node_str.get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_entrypoint_msg_print_detailed_context
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::process_entrypoint_msg_print_detailed_context(const char *caller_info, eConsoleMessageType msg_type)
{
    VOGL_FUNC_TRACER

    if (!m_pCur_gl_packet)
        return;

    dump_packet_as_func_call(*m_pCur_gl_packet);

    if (!(m_flags & cGLReplayerDumpPacketsOnError))
        return;

    print_detailed_context(caller_info, msg_type);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_entrypoint_
//  Don't call directly - use these macros:
//    process_entrypoint_message
//    process_entrypoint_warning
//    process_entrypoint_error
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::process_entrypoint_(const char *caller_info, eConsoleMessageType msgtype, const char *pFmt, ...)
{
    VOGL_FUNC_TRACER

    process_entrypoint_print_summary_context(caller_info, msgtype);

    va_list args;
    va_start(args, pFmt);
    console::vprintf(caller_info, msgtype | cMsgFlagOpenGL, pFmt, args);
    va_end(args);

    process_entrypoint_msg_print_detailed_context(caller_info, cMsgPrint);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::switch_contexts
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::switch_contexts(vogl_trace_context_ptr_value trace_context)
{
    VOGL_FUNC_TRACER

    //vogl_trace_context_ptr_value trace_context = gl_packet.m_context_handle;
    if (trace_context == m_cur_trace_context)
        return cStatusOK;

    if (m_flags & cGLReplayerDebugMode)
    {
        process_entrypoint_message("Forcing context switch via glXMakeCurrent, from current trace context 0x%" PRIX64 " to new context 0x%" PRIX64 "\n", cast_val_to_uint64(m_cur_trace_context), cast_val_to_uint64(trace_context));
    }

    // pContext_state will be NULL if they are unmapping!
    context_state *pContext_state = get_trace_context_state(trace_context);
    GLReplayContextType replay_context = pContext_state ? pContext_state->m_replay_context : 0;

    if (!m_pWindow->make_current(replay_context))
    {
        process_entrypoint_error("Failed switching current trace context to 0x%" PRIX64 "\n", trace_context);
        return cStatusHardFailure;
    }

    // This needs to be done after every (or at least the first) make current because windows can't get proper 
    // extensions until MakeCurrent time.
    if (m_proc_address_helper_func)
        vogl_init_actual_gl_entrypoints(m_proc_address_helper_func, m_wrap_all_gl_calls);

    m_cur_trace_context = trace_context;
    m_cur_replay_context = replay_context;
    m_pCur_context_state = pContext_state;

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::debug_callback_arb
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::debug_callback_arb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(length);

    char final_message[4096];

    context_state *pContext_state = (context_state *)(pUser_param);

    vogl_format_debug_output_arb(final_message, sizeof(final_message), source, type, id, severity, reinterpret_cast<const char *>(message));

    if (pContext_state)
    {
        vogl_warning_printf("Trace context: 0x%" PRIX64 ", Replay context 0x%" PRIX64 ", Last trace call counter: %" PRIu64 "\n%s\n",
                           cast_val_to_uint64(pContext_state->m_trace_context), cast_val_to_uint64(pContext_state->m_replay_context), cast_val_to_uint64(pContext_state->m_last_call_counter), final_message);
    }
    else
    {
        vogl_warning_printf("%s\n", final_message);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::debug_callback
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(length);

    char final_message[4096];

    context_state *pContext_state = (context_state *)(pUser_param);

    vogl_format_debug_output_arb(final_message, sizeof(final_message), source, type, id, severity, reinterpret_cast<const char *>(message));

    if (pContext_state)
    {
        vogl_warning_printf("Trace context: 0x%" PRIX64 ", Replay context 0x%" PRIX64 ", Last trace call counter: %" PRIu64 "\n%s\n",
                           cast_val_to_uint64(pContext_state->m_trace_context), cast_val_to_uint64(pContext_state->m_replay_context), cast_val_to_uint64(pContext_state->m_last_call_counter), final_message);
    }
    else
    {
        vogl_warning_printf("%s\n", final_message);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::is_extension_supported
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::is_extension_supported(const char *pExt)
{
    VOGL_FUNC_TRACER

    if ((m_pCur_context_state) && (m_pCur_context_state->m_context_info.is_valid()))
    {
        return m_pCur_context_state->m_context_info.supports_extension(pExt);
    }

    VOGL_ASSERT_ALWAYS;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::context_state::handle_context_made_current
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::context_state::handle_context_made_current()
{
    VOGL_FUNC_TRACER

    if (m_has_been_made_current)
        return true;

    VOGL_CHECK_GL_ERROR;

    m_has_been_made_current = true;

    if (!m_context_info.init(m_context_desc))
    {
        vogl_error_printf("vogl_context_info::init() failed!\n");
        return false;
    }

    if (!m_context_info.get_max_vertex_attribs())
    {
        vogl_warning_printf("GL_MAX_VERTEX_ATTRIBS is 0\n");
    }
    else if (m_context_info.get_max_vertex_attribs() >= VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES)
    {
        vogl_error_printf("GL_MAX_VERTEX_ATTRIBS is too large, max supported is %u. Please increase VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES.\n", VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES);
        return false;
    }

    if (m_replayer.m_flags & cGLReplayerLowLevelDebugMode)
    {
        vogl_debug_printf("Creating dummy handles\n");

        // HACK HACK
        // Generate a bunch of replay handles, so the trace and replay namespaces are totally different to shake out handle or target remapping bugs
        // TODO: All more object types
        vogl::vector<GLuint> dummy_handles(65536);

        GL_ENTRYPOINT(glGenTextures)(4000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGenBuffers)(6000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGenLists)(8000);
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGenQueries)(10000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGenVertexArrays)(12000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGenProgramsARB)(14000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        GL_ENTRYPOINT(glGenFramebuffers)(16000, dummy_handles.get_ptr());
        GL_ENTRYPOINT(glGenSamplers)(18000, dummy_handles.get_ptr());
        GL_ENTRYPOINT(glGenRenderbuffers)(20000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        for (uint32_t i = 0; i < 22000; i++)
            GL_ENTRYPOINT(glCreateProgram)();

        GL_ENTRYPOINT(glGenProgramPipelines)(24000, dummy_handles.get_ptr());
        VOGL_CHECK_GL_ERROR;

        vogl_debug_printf("Finished creating dummy handles\n");
    }

    VOGL_CHECK_GL_ERROR;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_context_made_current
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::handle_context_made_current()
{
    VOGL_FUNC_TRACER

    if (!m_pCur_context_state->handle_context_made_current())
        return false;

    if ((m_pCur_context_state->m_context_info.is_debug_context()) && (GL_ENTRYPOINT(glDebugMessageCallbackARB)) && (m_pCur_context_state->m_context_info.supports_extension("GL_ARB_debug_output")))
    {
        GL_ENTRYPOINT(glDebugMessageCallbackARB)(debug_callback_arb, (GLvoid *)m_pCur_context_state);
        GL_ENTRYPOINT(glEnable)(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
        check_gl_error();
    }

    vogl_verbose_printf("Trace context: 0x%" PRIX64 ", replay context 0x%" PRIX64 ", GL_VERSION: %s\n",
                        (uint64_t)m_cur_trace_context,
                        (uint64_t)m_cur_replay_context,
                        m_pCur_context_state->m_context_info.get_version_str().get_ptr());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_context_attrib_list
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::dump_context_attrib_list(const int *pAttrib_list, uint32_t size)
{
    VOGL_FUNC_TRACER

    if (!pAttrib_list)
    {
        vogl_debug_printf("Attrib list is NULL\n");
        return;
    }
    vogl_debug_printf("Context attribs:\n");

    uint32_t ofs = 0;
    for (;;)
    {
        if (ofs >= size)
        {
            vogl_error_printf("Attrib list ended prematurely (must end in a 0 key)\n");
            break;
        }

        uint32_t key = pAttrib_list[ofs];
        if (!key)
            break;
        ofs++;

        if (ofs >= size)
        {
            vogl_error_printf("Attrib list ended prematurely (must end in a 0 key)\n");
            break;
        }

        uint32_t value = pAttrib_list[ofs];
        ofs++;

        vogl_debug_printf("Key: %s (0x%08X), Value: 0x%08X\n", get_gl_enums().find_name(key, "GLX"), key, value);
    }
    vogl_debug_printf("End of context attribs\n");
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::find_attrib_key
//----------------------------------------------------------------------------------------------------------------------
int vogl_gl_replayer::find_attrib_key(const vogl::vector<int> &attrib_list, int key_to_find)
{
    VOGL_FUNC_TRACER

    uint32_t ofs = 0;
    while (ofs < attrib_list.size())
    {
        int key = attrib_list[ofs];
        if (!key)
            break;

        if (++ofs >= attrib_list.size())
        {
            process_entrypoint_warning("Failed parsing attrib list, this call is probably going to fail\n");
            break;
        }

        if (key == key_to_find)
            return ofs;
        ofs++;
    }

    return -1;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::create_context_attribs
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::create_context_attribs(
    vogl_trace_context_ptr_value trace_context, vogl_trace_context_ptr_value trace_share_context, GLReplayContextType replay_share_context, Bool direct,
    const int *pTrace_attrib_list, int trace_attrib_list_size, bool expecting_attribs)
{
    VOGL_FUNC_TRACER

    vogl::vector<int> temp_attrib_list;

    if ((pTrace_attrib_list) && (trace_attrib_list_size))
    {
        temp_attrib_list.append(pTrace_attrib_list, trace_attrib_list_size);
        if (temp_attrib_list.back() != 0)
        {
            process_entrypoint_warning("attrib list does not end with 0\n");
        }
    }
    else
    {
        if (expecting_attribs)
        {
            if (m_flags & cGLReplayerVerboseMode)
                process_entrypoint_message("No attrib list found in trace, assuming an attrib list ending with 0\n");
        }

        temp_attrib_list.push_back(0);
    }

    if (m_flags & cGLReplayerForceDebugContexts)
    {
        // See http://www.opengl.org/registry/specs/ARB/glx_create_context.txt
        int context_flags_ofs = find_attrib_key(temp_attrib_list, GLX_CONTEXT_FLAGS_ARB);
        if (context_flags_ofs < 0)
        {
            temp_attrib_list.back() = GLX_CONTEXT_FLAGS_ARB;
            temp_attrib_list.push_back(GLX_CONTEXT_DEBUG_BIT_ARB);
            temp_attrib_list.push_back(0);

            if (m_flags & cGLReplayerVerboseMode)
                process_entrypoint_warning("Appending GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB to attrib list to enable debug context\n");
        }
        else
        {
            temp_attrib_list[context_flags_ofs] |= GLX_CONTEXT_DEBUG_BIT_ARB;

            if (m_flags & cGLReplayerVerboseMode)
                process_entrypoint_warning("Slamming on GLX_CONTEXT_DEBUG_BIT_ARB bit to enable debug context\n");
        }

        int context_major_version_ofs = find_attrib_key(temp_attrib_list, GLX_CONTEXT_MAJOR_VERSION_ARB);
        int context_minor_version_ofs = find_attrib_key(temp_attrib_list, GLX_CONTEXT_MINOR_VERSION_ARB);

        bool slammed_up_to_3_0 = false;
        if (context_major_version_ofs < 0)
        {
// Don't slam up if they haven't requested a specific GL version, the driver will give us the most recent version that is backwards compatible with 1.0 (i.e. 4.3 for NVidia's current dirver).
#if 0
			temp_attrib_list.back() = GLX_CONTEXT_MAJOR_VERSION_ARB;
			temp_attrib_list.push_back(3);
			temp_attrib_list.push_back(0);

			slammed_up_to_3_0 = true;
#endif
        }
        else if (temp_attrib_list[context_major_version_ofs] < 3)
        {
            temp_attrib_list[context_major_version_ofs] = 3;

            slammed_up_to_3_0 = true;
        }

        if (slammed_up_to_3_0)
        {
            if (context_minor_version_ofs < 0)
            {
                temp_attrib_list.back() = GLX_CONTEXT_MINOR_VERSION_ARB;
                temp_attrib_list.push_back(0);
                temp_attrib_list.push_back(0);
            }
            else
            {
                temp_attrib_list[context_minor_version_ofs] = 0;
            }

            process_entrypoint_warning("Forcing GL context version up to 3.0 due to debug context usage\n");
        }
    }

    const int *pAttrib_list = temp_attrib_list.get_ptr();
    const uint32_t attrib_list_size = temp_attrib_list.size();

    if (m_flags & cGLReplayerVerboseMode)
        dump_context_attrib_list(pAttrib_list, attrib_list_size);

    GLReplayContextType replay_context = m_pWindow->create_context_attrib(replay_share_context, direct, pAttrib_list);

    if (!replay_context)
    {
        if (trace_context)
        {
            process_entrypoint_error("Failed creating new GL context!\n");
            return cStatusHardFailure;
        }
        else
        {
            process_entrypoint_warning("Successfully created a new GL context where the traced app failed!\n");
        }
    }

    if (replay_context)
    {
        if (trace_context)
        {
            context_state *pContext_state = define_new_context(trace_context, replay_context, trace_share_context, direct, VOGL_ENTRYPOINT_glXCreateContextAttribsARB, pAttrib_list, attrib_list_size);
            VOGL_NOTE_UNUSED(pContext_state);
        }
        else
        {
            m_pWindow->destroy_context(replay_context);
            // TODO: Not sure why we don't return an error code here. Figure out when this happens?
        }
    }

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_pending_make_current
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_pending_make_current()
{
    VOGL_FUNC_TRACER

    if (!m_pending_make_current_packet.is_valid())
        return cStatusOK;

    vogl_trace_packet &gl_packet = m_pending_make_current_packet;

    Bool trace_result = gl_packet.get_return_value<Bool>();

    // pContext_state will be NULL if they are unmapping!
    vogl_trace_ptr_value trace_context = get_trace_context_from_packet(gl_packet);
    context_state *pContext_state = get_trace_context_state(trace_context);
    GLReplayContextType replay_context = pContext_state ? pContext_state->m_replay_context : 0;

    if ((trace_context) && (!replay_context))
    {
        process_entrypoint_error("Failed remapping GL context\n");
        m_pending_make_current_packet.clear();
        return cStatusHardFailure;
    }

    bool result = m_pWindow->make_current(replay_context);

    // This needs to be done after every (or at least the first) make current because windows can't get proper 
    // extensions until MakeCurrent time.
    if (m_proc_address_helper_func)
        vogl_init_actual_gl_entrypoints(m_proc_address_helper_func, m_wrap_all_gl_calls);

    if (!result)
    {
        if (trace_result)
        {
            process_entrypoint_error("Failed making context current, but in the trace this call succeeded!\n");
            m_pending_make_current_packet.clear();
            return cStatusHardFailure;
        }
        else
        {
            process_entrypoint_warning("Failed making context current, in the trace this call also failed\n");
        }
    }
    else
    {
        m_cur_trace_context = trace_context;
        m_cur_replay_context = replay_context;
        m_pCur_context_state = pContext_state;

        if (!trace_result)
        {
            process_entrypoint_warning("Context was successfuly made current, but this operation failed in the trace\n");
        }

        if (m_cur_replay_context)
        {
            int viewport_x = gl_packet.get_key_value_map().get_int(string_hash("viewport_x"));
            int viewport_y = gl_packet.get_key_value_map().get_int(string_hash("viewport_y"));
            int viewport_width = gl_packet.get_key_value_map().get_int(string_hash("viewport_width"));
            int viewport_height = gl_packet.get_key_value_map().get_int(string_hash("viewport_height"));
            int win_width = gl_packet.get_key_value_map().get_int(string_hash("win_width"));
            int win_height = gl_packet.get_key_value_map().get_int(string_hash("win_height"));

            if (!m_pCur_context_state->m_has_been_made_current)
            {
                vogl_verbose_printf("MakeCurrent(): Trace Viewport: [%u,%u,%u,%u], Window: [%u %u]\n",
                                    viewport_x, viewport_y,
                                    viewport_width, viewport_height,
                                    win_width, win_height);
            }

            GLint cur_viewport[4];
            GL_ENTRYPOINT(glGetIntegerv)(GL_VIEWPORT, cur_viewport);

            uint32_t cur_win_width = 0, cur_win_height = 0;
            m_pWindow->get_actual_dimensions(cur_win_width, cur_win_height);

            if (!m_pCur_context_state->m_has_been_made_current)
            {
                vogl_verbose_printf("MakeCurrent(): Replay Viewport: [%u,%u,%u,%u], Window: [%u %u]\n",
                                    cur_viewport[0], cur_viewport[1],
                                    cur_viewport[2], cur_viewport[3],
                                    cur_win_width, cur_win_height);
            }

            if ((cur_viewport[0] != viewport_x) || (cur_viewport[1] != viewport_y) || (cur_viewport[2] != viewport_width) || (cur_viewport[3] != viewport_height))
            {
                process_entrypoint_warning("Replay viewport differs from traces!\n");
            }

            if (!handle_context_made_current())
                return cStatusHardFailure;
        }
    }

    m_last_processed_call_counter = gl_packet.get_call_counter();

    m_pending_make_current_packet.clear();

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_process_internal_trace_command_ctypes_packet
//----------------------------------------------------------------------------------------------------------------------
bool vogl_process_internal_trace_command_ctypes_packet(const key_value_map &kvm, const vogl_ctypes &ctypes)
{
    VOGL_FUNC_TRACER

    // TODO: Implement the code to map trace ctypes to replay ctypes. That's going to be fun.
    int num_trace_ctypes = kvm.get_int("num_ctypes");
    VOGL_VERIFY(num_trace_ctypes == VOGL_NUM_CTYPES);

    // TODO: This just verifies that the replayer's idea of each ctype matches the tracer's.
    // This will need to be revisited once we port to other OS's.
    // TODO: Move the ctypes crap into the SOF packet or something, it's not easy to deal with this packet on the fly.
    for (int ctype_iter = 0; ctype_iter < num_trace_ctypes; ctype_iter++)
    {
        const vogl_ctype_desc_t &desc = ctypes[static_cast<vogl_ctype_t>(ctype_iter)];

        uint32_t base_index = ctype_iter << 8;
        dynamic_string name(kvm.get_string(base_index++));
        dynamic_string ctype(kvm.get_string(base_index++));
        int size = kvm.get_int(base_index++);
        uint32_t loki_type_flags = kvm.get_uint(base_index++);

        bool is_pointer = kvm.get_bool(base_index++);
        bool is_opaque_pointer = kvm.get_bool(base_index++);
        bool is_pointer_diff = kvm.get_bool(base_index++);
        bool is_opaque_type = kvm.get_bool(base_index++);

        VOGL_VERIFY(name.compare(desc.m_pName, true) == 0);
        VOGL_VERIFY(ctype.compare(desc.m_pCType, true) == 0);
        if (!desc.m_is_opaque_type)
        {
            //$ TODO: Made this assert a printf so regression tests won't exit. John is re-working all this
            // so it should get fixed in the next couple weeks. 5/27/2014.
            //$ VOGL_VERIFY(size == desc.m_size);
            if (size != desc.m_size)
                vogl_error_printf("ctypes packet error. size != desc.m_size (%d != %d).\n", size, desc.m_size);
        }
        
        // Loki is a bit too specific, and so we don't care about certain 
        const uint32_t loki_ignored_types = 
            LOKI_TYPE_BITMASK(LOKI_IS_SIGNED_LONG) 
          | LOKI_TYPE_BITMASK(LOKI_IS_UNSIGNED_LONG)
          | LOKI_TYPE_BITMASK(LOKI_IS_FUNCTION_POINTER)
        ;

        VOGL_VERIFY((loki_type_flags & ~loki_ignored_types) == (desc.m_loki_type_flags & ~loki_ignored_types));

        VOGL_VERIFY(is_pointer == desc.m_is_pointer);
        VOGL_VERIFY(is_opaque_pointer == desc.m_is_opaque_pointer);
        VOGL_VERIFY(is_pointer_diff == desc.m_is_pointer_diff);

        // We need to massively rethink the way this is going to work. 
        // I think what we need to do is always shmoo types into cross-platform friendly types
        // where they just become "name + bits we care about". In the meantime, relax the restriction
        // that trace opacity matches replayer opacity. If a type is captured as transparent, it 
        // can be marked as opaque during replay time.
        VOGL_VERIFY(is_opaque_type == false || desc.m_is_opaque_type == true);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_internal_trace_command
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_internal_trace_command(const vogl_trace_gl_entrypoint_packet &gl_packet)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(gl_packet);

    GLuint cmd = m_pCur_gl_packet->get_param_value<GLuint>(0);
    GLuint size = m_pCur_gl_packet->get_param_value<GLuint>(1);
    VOGL_NOTE_UNUSED(size);
    vogl_trace_ptr_value trace_data_ptr_value = m_pCur_gl_packet->get_param_ptr_value(2);
    VOGL_NOTE_UNUSED(trace_data_ptr_value);

    vogl_gl_replayer::status_t status = cStatusOK;

    switch (cmd)
    {
        case cITCRDemarcation:
        {
            break;
        }
        case cITCRKeyValueMap:
        {
            const key_value_map &kvm = m_pCur_gl_packet->get_key_value_map();

            dynamic_string cmd_type(kvm.get_string("command_type"));
            if (cmd_type == "state_snapshot")
            {
                if (!m_allow_snapshot_restoring)
                {
                    break;
                }

                dynamic_string text_id(kvm.get_string("id"));
                dynamic_string binary_id(kvm.get_string("binary_id"));
                if (text_id.is_empty() && binary_id.is_empty())
                {
                    process_entrypoint_error("Missing id and binary_id fields in glInternalTraceCommandRAD key_value_map command type: \"%s\"\n", cmd_type.get_ptr());
                    return cStatusHardFailure;
                }

                dynamic_string id_to_use(text_id.is_empty() ? binary_id : text_id);

                // TODO: Make a 1st class snapshot cache class
                // TODO: This could fail if the user hand modifies the snapshot in some way - add an option to disable caching.
                vogl_gl_state_snapshot *pSnapshot = NULL;
                if (m_flags & cGLReplayerSnapshotCaching)
                {
                    for (uint32_t snapshot_index = 0; snapshot_index < m_snapshots.size(); snapshot_index++)
                    {
                        if (!m_snapshots[snapshot_index].m_name.compare(id_to_use, false))
                        {
                            pSnapshot = m_snapshots[snapshot_index].m_pSnapshot;
                            if (snapshot_index)
                            {
                                snapshot_cache_entry cache_entry(m_snapshots[snapshot_index]);
                                m_snapshots.erase(snapshot_index);
                                m_snapshots.insert(0, cache_entry);
                            }
                            break;
                        }
                    }
                }

                if (!pSnapshot)
                {
                    timed_scope ts("Deserialize snapshot time");

                    if (!m_pBlob_manager)
                    {
                        process_entrypoint_error("Failed reading snapshot blob data \"%s\"!\n", id_to_use.get_ptr());
                        return cStatusHardFailure;
                    }

                    uint8_vec snapshot_data;

                    if (!m_pBlob_manager->get(id_to_use, snapshot_data) || (snapshot_data.is_empty()))
                    {
                        process_entrypoint_error("Failed reading snapshot blob data \"%s\"!\n", id_to_use.get_ptr());
                        return cStatusHardFailure;
                    }

                    vogl_message_printf("Deserializing state snapshot \"%s\", %u bytes\n", id_to_use.get_ptr(), snapshot_data.size());

                    json_document doc;

                    bool success;
                    if (id_to_use == text_id)
                        success = doc.deserialize(reinterpret_cast<const char *>(snapshot_data.get_ptr()), snapshot_data.size());
                    else
                        success = doc.binary_deserialize(snapshot_data);
                    if (!success || (!doc.get_root()))
                    {
                        process_entrypoint_error("Failed deserializing JSON snapshot blob data \"%s\"!\n", id_to_use.get_ptr());
                        return cStatusHardFailure;
                    }

                    pSnapshot = vogl_new(vogl_gl_state_snapshot);
                    if (!pSnapshot->deserialize(*doc.get_root(), *m_pBlob_manager, &m_trace_gl_ctypes))
                    {
                        vogl_delete(pSnapshot);
                        pSnapshot = NULL;

                        process_entrypoint_error("Failed deserializing snapshot blob data \"%s\"!\n", id_to_use.get_ptr());
                        return cStatusHardFailure;
                    }

                    if (m_flags & cGLReplayerSnapshotCaching)
                    {
                        snapshot_cache_entry new_cache_entry;
                        new_cache_entry.m_name = id_to_use;
                        new_cache_entry.m_pSnapshot = pSnapshot;
                        m_snapshots.insert(0, new_cache_entry);

                        // FIXME: Even 3-4 snapshots in memory may be too much in 32-bit mode for some large apps.
                        while (m_snapshots.size() > 3)
                        {
                            vogl_delete(m_snapshots.back().m_pSnapshot);
                            m_snapshots.resize(m_snapshots.size() - 1);
                        }
                    }
                }

                status = begin_applying_snapshot(pSnapshot, (m_flags & cGLReplayerSnapshotCaching) ? false : true);

                if ((status != cStatusOK) && (status != cStatusResizeWindow))
                {
                    if (m_flags & cGLReplayerSnapshotCaching)
                    {
                        VOGL_ASSERT(m_snapshots[0].m_pSnapshot == pSnapshot);

                        vogl_delete(m_snapshots[0].m_pSnapshot);
                        m_snapshots.erase(0U);
                    }

                    if (m_flags & cGLReplayerSnapshotCaching)
                    {
                        vogl_delete(pSnapshot);
                        pSnapshot = NULL;
                    }

                    process_entrypoint_error("Failed applying GL snapshot from blob data \"%s\"!\n", id_to_use.get_ptr());
                    return status;
                }

                vogl_message_printf("Successfully applied GL state snapshot from blob \"%s\"\n", id_to_use.get_ptr());
            }
            else if (cmd_type == "ctypes")
            {
                m_ctypes_packet = *m_pCur_gl_packet;

                if (!vogl_process_internal_trace_command_ctypes_packet(kvm, m_trace_gl_ctypes))
                    return cStatusHardFailure;
            }
            else if (cmd_type == "entrypoints")
            {
                // TODO
            }
            else
            {
                process_entrypoint_warning("Unknown glInternalTraceCommandRAD key_value_map command type: \"%s\"\n", cmd_type.get_ptr());
            }
            break;
        }
        default:
        {
            process_entrypoint_warning("Unknown glInternalTraceCommandRAD command type: %u\n", cmd);
            break;
        }
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::check_program_binding_shadow
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::check_program_binding_shadow()
{
    VOGL_FUNC_TRACER

    if (!m_pCur_context_state)
        return true;

    // Make sure shadow is good
    GLint actual_current_replay_program = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_CURRENT_PROGRAM, &actual_current_replay_program);
    check_gl_error();

    if (m_pCur_context_state->m_cur_replay_program == static_cast<GLuint>(actual_current_replay_program))
        return true;

    // OK, on AMD it plays games with GL_CURRENT_PROGRAM when the currently bound program is deleted. Check for this scenario.
    bool is_still_program = GL_ENTRYPOINT(glIsProgram)(m_pCur_context_state->m_cur_replay_program) != 0;
    if ((!check_gl_error()) && (is_still_program))
    {
        GLint marked_for_deletion = GL_FALSE;
        GL_ENTRYPOINT(glGetProgramiv)(m_pCur_context_state->m_cur_replay_program, GL_DELETE_STATUS, &marked_for_deletion);

        if ((!check_gl_error()) && (marked_for_deletion))
            return true;
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_use_program
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_use_program(GLuint trace_handle, gl_entrypoint_id_t entrypoint_id)
{
    VOGL_FUNC_TRACER

    // TODO: This code assumes the non-ARB entrypoints are being used, which works fine on NV but who knows what'll happen on other drivers.
    check_program_binding_shadow();

    GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
    VOGL_ASSERT(!trace_handle || get_shared_state()->m_shadow_state.m_objs.get_target(trace_handle) == VOGL_PROGRAM_OBJECT);
    VOGL_ASSERT(!trace_handle || get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle) == VOGL_PROGRAM_OBJECT);

    // For safety, absorb any previous error.
    check_gl_error();

    GLuint prev_replay_program = m_pCur_context_state->m_cur_replay_program;
    GLuint prev_trace_program = m_pCur_context_state->m_cur_trace_program;

    bool prev_is_program = false;
    GLint prev_link_status = false;
    GLint prev_is_marked_for_deletion = false;
    vogl::growable_array<GLuint, 8> prev_attached_replay_shaders;

    if ((prev_replay_program) && (replay_handle != prev_replay_program))
    {
        prev_is_program = GL_ENTRYPOINT(glIsProgram)(prev_replay_program) != GL_FALSE;
        check_gl_error_quietly();

        if (prev_is_program)
        {
            GL_ENTRYPOINT(glGetProgramiv)(prev_replay_program, GL_DELETE_STATUS, &prev_is_marked_for_deletion);
            check_gl_error_quietly();

            GL_ENTRYPOINT(glGetProgramiv)(prev_replay_program, GL_LINK_STATUS, &prev_link_status);
            check_gl_error_quietly();

            if (prev_is_marked_for_deletion)
            {
                // The currently bound program is marked for deletion, so record which shaders are attached to it in case the program is actually deleted by the driver on the UseProgram() call.
                GLint num_attached_shaders = 0;
                GL_ENTRYPOINT(glGetProgramiv)(prev_replay_program, GL_ATTACHED_SHADERS, &num_attached_shaders);
                check_gl_error_quietly();

                if (num_attached_shaders)
                {
                    prev_attached_replay_shaders.resize(num_attached_shaders);

                    GLsizei actual_count = 0;
                    GL_ENTRYPOINT(glGetAttachedShaders)(prev_replay_program, num_attached_shaders, &actual_count, prev_attached_replay_shaders.get_ptr());
                    check_gl_error_quietly();

                    VOGL_ASSERT(actual_count == num_attached_shaders);
                }
            }
        }
    }

    if (entrypoint_id == VOGL_ENTRYPOINT_glUseProgram)
        GL_ENTRYPOINT(glUseProgram)(replay_handle);
    else
        GL_ENTRYPOINT(glUseProgramObjectARB)(replay_handle);

    // Can't shadow if glUseProgram failed.
    if (check_gl_error())
        return;

    if ((prev_replay_program) && (prev_replay_program != replay_handle))
    {
        bool is_prev_still_program = GL_ENTRYPOINT(glIsProgram)(prev_replay_program) != GL_FALSE;
        if (!is_prev_still_program)
        {
            VOGL_ASSERT(prev_is_program);
            VOGL_ASSERT(prev_is_marked_for_deletion);

            // The previously bound program is really dead now, kill it from our tables and also check up on any shaders it was attached to.
            bool was_deleted = get_shared_state()->m_shadow_state.m_objs.erase(prev_trace_program);
            VOGL_ASSERT(was_deleted);
            VOGL_NOTE_UNUSED(was_deleted);

            get_shared_state()->m_glsl_program_hash_map.erase(prev_trace_program);

            was_deleted = get_shared_state()->m_shadow_state.m_linked_programs.remove_snapshot(prev_replay_program);
            if ((prev_link_status) && (!was_deleted))
            {
                VOGL_ASSERT_ALWAYS;
            }

            for (uint32_t i = 0; i < prev_attached_replay_shaders.size(); i++)
            {
                GLuint replay_shader_handle = prev_attached_replay_shaders[i];

                bool is_still_shader = GL_ENTRYPOINT(glIsShader)(replay_shader_handle) != GL_FALSE;
                check_gl_error_quietly();

                if (is_still_shader)
                    continue;

                if (!get_shared_state()->m_shadow_state.m_objs.contains_inv(replay_shader_handle))
                {
                    // We didn't create this shader handle, the AMD driver did on a program binary link, so ignore it.
                    continue;
                }

                // The attached shader is now really dead.
                VOGL_ASSERT(get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_shader_handle) == VOGL_SHADER_OBJECT);
                if (!get_shared_state()->m_shadow_state.m_objs.erase_inv(replay_shader_handle))
                {
                    process_entrypoint_error("Failed finding attached GL shader %u in objects hash table, while handling the actual deletion of trace program %u replay program %u\n", replay_shader_handle, prev_trace_program, prev_replay_program);
                }
            }
        }
    }

    m_pCur_context_state->m_cur_replay_program = replay_handle;
    m_pCur_context_state->m_cur_trace_program = trace_handle;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_delete_program
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_delete_program(GLuint trace_handle)
{
    VOGL_FUNC_TRACER

    check_program_binding_shadow();

    // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

    GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
    VOGL_ASSERT(get_shared_state()->m_shadow_state.m_objs.get_target(trace_handle) == VOGL_PROGRAM_OBJECT);
    VOGL_ASSERT(get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle) == VOGL_PROGRAM_OBJECT);

    context_state *pContext_shareroot = m_pCur_context_state->m_pShared_state;
    for (context_hash_map::iterator it = m_contexts.begin(); it != m_contexts.end(); ++it)
    {
        if (it->first == m_pCur_context_state->m_context_desc.get_trace_context())
            continue;

        context_state *pContext = it->second;
        if (pContext->m_pShared_state == pContext_shareroot)
        {
            if (pContext->m_cur_trace_program == trace_handle)
            {
                process_entrypoint_error("Trace program %u replay program %u is being deleted on context 0x%" PRIx64 ", but it's currently bound to trace context 0x%" PRIx64 "! This scenario is not currently supported with sharelists.\n",
                                         trace_handle, replay_handle,
                                         cast_val_to_uint64(m_pCur_context_state->m_context_desc.get_trace_context()),
                                         cast_val_to_uint64(it->first));
            }
        }
    }

    bool is_program = GL_ENTRYPOINT(glIsProgram)(replay_handle) != 0;
    check_gl_error_quietly();

    vogl::growable_array<GLuint, 8> attached_replay_shaders;

    if ((is_program) && (replay_handle))
    {
        GLint num_attached_shaders = 0;
        GL_ENTRYPOINT(glGetProgramiv)(replay_handle, GL_ATTACHED_SHADERS, &num_attached_shaders);
        check_gl_error_quietly();

        if (num_attached_shaders)
        {
            attached_replay_shaders.resize(num_attached_shaders);

            GLsizei actual_count = 0;
            GL_ENTRYPOINT(glGetAttachedShaders)(replay_handle, num_attached_shaders, &actual_count, attached_replay_shaders.get_ptr());
            check_gl_error_quietly();

            VOGL_ASSERT(actual_count == num_attached_shaders);
        }
    }

    GL_ENTRYPOINT(glDeleteProgram)(replay_handle);

    bool deletion_succeeded = !check_gl_error();
    if (!deletion_succeeded)
    {
        VOGL_ASSERT(!is_program);

        process_entrypoint_warning("Failed deleting program, trace handle %u GL handle %u\n", trace_handle, replay_handle);
        return;
    }

    if (!replay_handle)
        return;

    bool is_still_program = GL_ENTRYPOINT(glIsProgram)(replay_handle) != 0;
    check_gl_error_quietly();

    GLint marked_for_deletion = 0;
    if (is_still_program)
    {
        // It must still be bound to the context, or referred to in some other way that we don't know about.
        GL_ENTRYPOINT(glGetProgramiv)(replay_handle, GL_DELETE_STATUS, &marked_for_deletion);
        bool delete_status_check_succeeded = !check_gl_error_quietly();

        VOGL_VERIFY(delete_status_check_succeeded);
        VOGL_VERIFY(marked_for_deletion);
    }
    else if (!is_still_program)
    {
        VOGL_ASSERT(is_program);

        // The program is really gone now.
        bool was_deleted = get_shared_state()->m_shadow_state.m_objs.erase(trace_handle);
        VOGL_ASSERT(was_deleted);
        VOGL_NOTE_UNUSED(was_deleted);

        was_deleted = get_shared_state()->m_glsl_program_hash_map.erase(trace_handle);

        get_shared_state()->m_shadow_state.m_linked_programs.remove_snapshot(replay_handle);

        if (m_pCur_context_state->m_cur_replay_program == replay_handle)
        {
            // This shouldn't happen - if the program is still bound to the context then it should still be a program.
            VOGL_ASSERT_ALWAYS;
            m_pCur_context_state->m_cur_replay_program = 0;
            m_pCur_context_state->m_cur_trace_program = 0;
        }

        for (uint32_t i = 0; i < attached_replay_shaders.size(); i++)
        {
            GLuint replay_shader_handle = attached_replay_shaders[i];

            bool is_still_shader = GL_ENTRYPOINT(glIsShader)(replay_shader_handle) != 0;
            check_gl_error_quietly();

            if (is_still_shader)
                continue;

            if (!get_shared_state()->m_shadow_state.m_objs.contains_inv(replay_shader_handle))
            {
                // We didn't create this shader handle, the AMD driver did on a program binary link, so ignore it.
                continue;
            }

            // The attached shader is now really dead.
            VOGL_ASSERT(get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_shader_handle) == VOGL_SHADER_OBJECT);
            if (!get_shared_state()->m_shadow_state.m_objs.erase_inv(replay_shader_handle))
            {
                process_entrypoint_error("Failed finding attached GL shader %u in objects hash table, while handling the actual deletion of trace program %u replay program %u\n", replay_shader_handle, trace_handle, replay_handle);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_delete_shader
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_delete_shader(GLuint trace_handle)
{
    VOGL_FUNC_TRACER

    check_program_binding_shadow();

    check_gl_error();

    // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

    GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
    VOGL_ASSERT(get_shared_state()->m_shadow_state.m_objs.get_target(trace_handle) == VOGL_SHADER_OBJECT);
    VOGL_ASSERT(get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle) == VOGL_SHADER_OBJECT);

    GL_ENTRYPOINT(glDeleteShader)(replay_handle);

    bool deletion_succeeded = !check_gl_error();
    if (!deletion_succeeded)
    {
        process_entrypoint_warning("Failed deleting shader, trace handle %u GL handle %u\n", trace_handle, replay_handle);
        return;
    }

    if (!replay_handle)
        return;

    bool is_still_shader = GL_ENTRYPOINT(glIsShader)(replay_handle) != GL_FALSE;
    check_gl_error_quietly();

    if (!is_still_shader)
    {
        // The shader is really gone.
        bool was_deleted = get_shared_state()->m_shadow_state.m_objs.erase(trace_handle);
        VOGL_ASSERT(was_deleted);
        VOGL_NOTE_UNUSED(was_deleted);
    }
    else
    {
        GLint marked_for_deletion = 0;
        GL_ENTRYPOINT(glGetShaderiv)(replay_handle, GL_DELETE_STATUS, &marked_for_deletion);
        check_gl_error_quietly();

        VOGL_VERIFY(marked_for_deletion);

        // The shader is attached to a live program object (which may or may not be actually in the marked_as_deleted state)
        // we'll get around to it when the program object is deleted, or when they remove the program object from the current state.
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_detach_shader
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_detach_shader(gl_entrypoint_id_t entrypoint_id)
{
    GLuint trace_program = m_pCur_gl_packet->get_param_value<GLuint>(0);
    GLuint replay_program = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_program);

    GLuint trace_shader = m_pCur_gl_packet->get_param_value<GLuint>(1);
    GLuint replay_shader = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_shader);

    check_gl_error();

    // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

    GLboolean was_shader = GL_ENTRYPOINT(glIsShader)(replay_shader);
    check_gl_error_quietly();

    GLint marked_for_deletion = 0;
    GL_ENTRYPOINT(glGetShaderiv)(replay_shader, GL_DELETE_STATUS, &marked_for_deletion);
    check_gl_error_quietly();

    if (entrypoint_id == VOGL_ENTRYPOINT_glDetachObjectARB)
        GL_ENTRYPOINT(glDetachObjectARB)(replay_program, replay_shader);
    else
    {
        VOGL_ASSERT(entrypoint_id == VOGL_ENTRYPOINT_glDetachShader);
        GL_ENTRYPOINT(glDetachShader)(replay_program, replay_shader);
    }

    bool detach_failed = check_gl_error();

    GLboolean is_shader = GL_ENTRYPOINT(glIsShader)(replay_shader);
    check_gl_error_quietly();

    if (!detach_failed)
    {
        if ((marked_for_deletion) && (was_shader) && (!is_shader))
        {
            // The shader is really gone now.
            bool was_deleted = get_shared_state()->m_shadow_state.m_objs.erase(trace_shader);
            VOGL_ASSERT(was_deleted);
            VOGL_NOTE_UNUSED(was_deleted);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_post_link_program
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_post_link_program(gl_entrypoint_id_t entrypoint_id, GLuint trace_handle, GLuint replay_handle, GLenum type, GLsizei count, GLchar *const *strings)
{
    const json_document *pDoc = m_pCur_gl_packet->get_key_value_map().get_json_document("metadata");

    GLint replay_link_status = 0;
    GLint replay_active_attributes = 0;
    if (entrypoint_id == VOGL_ENTRYPOINT_glLinkProgramARB)
    {
        GL_ENTRYPOINT(glGetObjectParameterivARB)(replay_handle, GL_OBJECT_LINK_STATUS_ARB, &replay_link_status);
        check_gl_error();

        GL_ENTRYPOINT(glGetObjectParameterivARB)(replay_handle, GL_OBJECT_ACTIVE_ATTRIBUTES_ARB, &replay_active_attributes);
        check_gl_error();
    }
    else
    {
        GL_ENTRYPOINT(glGetProgramiv)(replay_handle, GL_LINK_STATUS, &replay_link_status);
        check_gl_error();

        GL_ENTRYPOINT(glGetProgramiv)(replay_handle, GL_ACTIVE_ATTRIBUTES, &replay_active_attributes);
        check_gl_error();
    }

    if ((replay_link_status) || (!get_shared_state()->m_shadow_state.m_linked_programs.find_snapshot(replay_handle)))
    {
        bool success;
        if (entrypoint_id == VOGL_ENTRYPOINT_glProgramBinary)
            success = get_shared_state()->m_shadow_state.m_linked_programs.add_snapshot(m_pCur_context_state->m_context_info, m_replay_to_trace_remapper, entrypoint_id, replay_handle, m_pCur_gl_packet->get_param_value<GLenum>(1), m_pCur_gl_packet->get_param_client_memory<GLvoid>(2), m_pCur_gl_packet->get_param_value<GLsizei>(3));
        else if (entrypoint_id == VOGL_ENTRYPOINT_glCreateShaderProgramv)
            success = get_shared_state()->m_shadow_state.m_linked_programs.add_snapshot(m_pCur_context_state->m_context_info, m_replay_to_trace_remapper, entrypoint_id, replay_handle, type, count, strings);
        else
            success = get_shared_state()->m_shadow_state.m_linked_programs.add_snapshot(m_pCur_context_state->m_context_info, m_replay_to_trace_remapper, entrypoint_id, replay_handle);

        if (!success)
            process_entrypoint_warning("Failed inserting into link time program shadow, trace program 0x%X replay program 0x%X\n", trace_handle, replay_handle);
    }

    int trace_active_attributes = 0;
    int trace_active_uniforms = 0;
    int trace_active_uniform_blocks = 0;
    int trace_link_status = 1;

    VOGL_NOTE_UNUSED(trace_active_uniforms);
    VOGL_NOTE_UNUSED(trace_active_uniform_blocks);

    if (pDoc)
    {
        const json_node &doc_root = *pDoc->get_root();

        trace_link_status = doc_root.value_as_int("link_status");
        trace_active_attributes = doc_root.value_as_int("total_active_attributes");
        trace_active_uniforms = doc_root.value_as_int("total_active_uniforms");
        trace_active_uniform_blocks = doc_root.value_as_int("active_uniform_blocks");

        if (replay_link_status != trace_link_status)
        {
            process_entrypoint_warning("Trace link status (%i) differs from replay link status (%i), trace program 0x%X replay program 0x%X\n", trace_link_status, replay_link_status, trace_handle, replay_handle);
        }
    }

    if (!replay_link_status)
    {
        vogl::vector<GLchar> log;

        if (entrypoint_id == VOGL_ENTRYPOINT_glLinkProgramARB)
        {
            GLsizei length = 0;
            GL_ENTRYPOINT(glGetObjectParameterivARB)(replay_handle, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
            check_gl_error();

            if (length)
            {
                log.resize(length);

                GLint actual_length = 0;
                GL_ENTRYPOINT(glGetInfoLogARB)(replay_handle, log.size(), &actual_length, reinterpret_cast<GLcharARB *>(log.get_ptr()));
                check_gl_error();
            }
        }
        else
        {
            GLint length = 0;
            GL_ENTRYPOINT(glGetProgramiv)(replay_handle, GL_INFO_LOG_LENGTH, &length);
            check_gl_error();

            if (length)
            {
                log.resize(length);

                GL_ENTRYPOINT(glGetProgramInfoLog)(replay_handle, log.size(), &length, log.get_ptr());
                check_gl_error();
            }
        }

        if ((log.size()) && (log[0]))
        {
            process_entrypoint_message("Info log for trace object %u, replay object %u:\n%s\n", trace_handle, replay_handle, log.get_ptr());
        }
    }

    if ((pDoc) && (replay_active_attributes != trace_active_attributes))
    {
        process_entrypoint_warning("Number of trace active attributes (%i) differs from number of replay active attributes (%i) after linking program, trace program 0x%X replay program 0x%X\n", trace_active_attributes, replay_active_attributes, trace_handle, replay_handle);
    }

    const json_node *pUniforms_node = pDoc ? pDoc->get_root()->find_child_array("active_uniforms") : NULL;

    if (pUniforms_node)
    {
        glsl_program_hash_map::iterator it = get_shared_state()->m_glsl_program_hash_map.find(trace_handle);
        if (it == get_shared_state()->m_glsl_program_hash_map.end())
            it = get_shared_state()->m_glsl_program_hash_map.insert(trace_handle).first;
        glsl_program_state &prog_state = it->second;

        for (uint32_t i = 0; i < pUniforms_node->size(); i++)
        {
            const json_node *pUniform = pUniforms_node->get_child(i);
            if (!pUniform)
            {
                VOGL_ASSERT_ALWAYS;
                continue;
            }

            const char *pName = pUniform->value_as_string_ptr("name");
            if (!pName)
            {
                VOGL_ASSERT_ALWAYS;
                continue;
            }
            int trace_loc = pUniform->value_as_int("location");
            int trace_array_size = pUniform->value_as_int("size");
            //int trace_type = pUniform->value_as_int("type");

            VOGL_ASSERT(trace_array_size >= 1);

            if ((trace_loc < 0) || (trace_array_size <= 0))
                continue;

            if (trace_array_size > 1)
            {
                dynamic_string element_name;
                for (int j = 0; j < trace_array_size; j++)
                {
                    element_name = pName;
                    int start_bracket_ofs = element_name.find_right('[');
                    if (start_bracket_ofs >= 0)
                        element_name.left(start_bracket_ofs);
                    element_name.format_append("[%u]", j);

                    GLint element_trace_loc = trace_loc + j;
                    GLint element_replay_loc;
                    if (entrypoint_id == VOGL_ENTRYPOINT_glLinkProgramARB)
                        element_replay_loc = GL_ENTRYPOINT(glGetUniformLocationARB)(replay_handle, reinterpret_cast<const GLcharARB *>(element_name.get_ptr()));
                    else
                        element_replay_loc = GL_ENTRYPOINT(glGetUniformLocation)(replay_handle, reinterpret_cast<const GLchar *>(element_name.get_ptr()));
                    check_gl_error();

                    if (element_replay_loc < 0)
                    {
                        process_entrypoint_warning("glGetUniformLocation: Trace active array uniform %s trace location %i trace array size %i is not active during replay, trace program 0x%X replay program 0x%X\n", element_name.get_ptr(), trace_loc, trace_array_size, trace_handle, replay_handle);
                    }
                    else
                    {
                        prog_state.m_uniform_locations.erase(element_trace_loc);
                        prog_state.m_uniform_locations.insert(element_trace_loc, element_replay_loc);
                    }
                }
            }
            else if (trace_array_size == 1)
            {
                GLint replay_loc;
                if (entrypoint_id == VOGL_ENTRYPOINT_glLinkProgramARB)
                    replay_loc = GL_ENTRYPOINT(glGetUniformLocationARB)(replay_handle, reinterpret_cast<const GLcharARB *>(pName));
                else
                    replay_loc = GL_ENTRYPOINT(glGetUniformLocation)(replay_handle, reinterpret_cast<const GLchar *>(pName));
                check_gl_error();

                if (replay_loc < 0)
                {
                    process_entrypoint_warning("glGetUniformLocation: Trace active uniform %s trace location %i is not active during replay, trace program 0x%X replay program 0x%X\n", pName, trace_loc, trace_handle, replay_handle);
                }
                else
                {
                    prog_state.m_uniform_locations.erase(trace_loc);
                    prog_state.m_uniform_locations.insert(trace_loc, replay_loc);
                }
            }
        } // i
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_link_program
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_link_program(gl_entrypoint_id_t entrypoint_id)
{
    check_gl_error();

    GLuint trace_handle = m_pCur_gl_packet->get_param_value<GLuint>(0);
    GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

    GLboolean is_program = GL_ENTRYPOINT(glIsProgram)(replay_handle);
    check_gl_error();
    if (!is_program)
    {
        process_entrypoint_warning("Handle is not a program, trace program 0x%X replay program 0x%X\n", trace_handle, replay_handle);
    }

    const json_document *pDoc = m_pCur_gl_packet->get_key_value_map().get_json_document("metadata");

    if (!pDoc)
    {
        process_entrypoint_warning("JSON metadata document is missing, program will be linked without setting its attributes or initializing the uniform location shadow, trace program 0x%X replay program 0x%X\n", trace_handle, replay_handle);
    }
    else if ((pDoc) && (!pDoc->is_object()))
    {
        process_entrypoint_warning("JSON metadata document must be an object, trace program 0x%X replay program 0x%X\n", trace_handle, replay_handle);
        pDoc = NULL;
    }

    if (pDoc)
    {
        const json_node &doc_root = *pDoc->get_root();

        const json_node *pAttrib_node = doc_root.find_child_array("active_attribs");
        if (pAttrib_node)
        {
            for (uint32_t i = 0; i < pAttrib_node->size(); i++)
            {
                const json_node *pAttrib = pAttrib_node->get_child(i);
                if (!pAttrib)
                {
                    VOGL_ASSERT_ALWAYS;
                    continue;
                }

                const char *pName = pAttrib->value_as_string_ptr("name");
                int attrib_loc = pAttrib->value_as_int("location", -1);

                if ((pName) && (pName[0]) && (attrib_loc >= 0))
                {
                    if (entrypoint_id == VOGL_ENTRYPOINT_glLinkProgramARB)
                        GL_ENTRYPOINT(glBindAttribLocationARB)(replay_handle, attrib_loc, pName);
                    else
                        GL_ENTRYPOINT(glBindAttribLocation)(replay_handle, attrib_loc, reinterpret_cast<const GLchar *>(pName));

                    check_gl_error();
                }
            }
        }

        const json_node *pOutputs_object = doc_root.find_child_array("active_outputs");
        if (pOutputs_object)
        {
            for (uint32_t i = 0; i < pOutputs_object->size(); i++)
            {
                const json_node *pOutput_node = pOutputs_object->get_child(i);
                if (!pOutput_node)
                    continue;

                dynamic_string name(pOutput_node->value_as_string("name"));
                if ((name.is_empty()) || (name.begins_with("gl_", true)))
                    continue;

                int location = pOutput_node->value_as_int("location");
                int location_index = pOutput_node->value_as_int("location_index");

                if (m_pCur_context_state->m_context_info.supports_extension("GL_ARB_blend_func_extended") && GL_ENTRYPOINT(glBindFragDataLocationIndexed))
                {
                    GL_ENTRYPOINT(glBindFragDataLocationIndexed)(replay_handle, location, location_index, reinterpret_cast<const GLchar *>(name.get_ptr()));
                }
                else
                {
                    if (location_index)
                        process_entrypoint_error("GL_ARB_blend_func_extended is not supported, but trace program %u GL program %u uses a non-zero location index\n", trace_handle, replay_handle);

                    GL_ENTRYPOINT(glBindFragDataLocation)(replay_handle, location, reinterpret_cast<const GLchar *>(name.get_ptr()));
                }

                check_gl_error();
            }
        }

        GLenum transform_feedback_mode = vogl_get_json_value_as_enum(doc_root, "transform_feedback_mode");
        GLint num_varyings = doc_root.value_as_int("transform_feedback_num_varyings");
        if (num_varyings)
        {
            const json_node *pTransform_feedback_varyings = doc_root.find_child_array("transform_feedback_varyings");
            if (pTransform_feedback_varyings)
            {
                dynamic_string_array names;

                for (uint32_t i = 0; i < pTransform_feedback_varyings->size(); i++)
                {
                    const json_node *pVarying_node = pTransform_feedback_varyings->get_child(i);
                    if (!pVarying_node)
                        continue;

                    GLint index = pVarying_node->value_as_int("index");
                    if (index < 0)
                        continue;

                    dynamic_string name(pVarying_node->value_as_string("name"));

                    //GLsizei size(pVarying_node->value_as_int("size"));
                    //GLenum type(vogl_get_json_value_as_enum(*pVarying_node, "type"));

                    names.ensure_element_is_valid(index);
                    names[index] = name;
                }

                vogl::vector<GLchar *> varyings(names.size());
                for (uint32_t i = 0; i < names.size(); i++)
                    varyings[i] = (GLchar *)(names[i].get_ptr());

                GL_ENTRYPOINT(glTransformFeedbackVaryings)(replay_handle, varyings.size(), varyings.get_ptr(), transform_feedback_mode);
                check_gl_error();
            }
        }
    }

    switch (entrypoint_id)
    {
        case VOGL_ENTRYPOINT_glLinkProgram:
        {
            GL_ENTRYPOINT(glLinkProgram)(replay_handle);
            break;
        }
        case VOGL_ENTRYPOINT_glLinkProgramARB:
        {
            GL_ENTRYPOINT(glLinkProgramARB)(replay_handle);
            break;
        }
        case VOGL_ENTRYPOINT_glProgramBinary:
        {
            GL_ENTRYPOINT(glProgramBinary)(replay_handle, m_pCur_gl_packet->get_param_value<GLenum>(1), m_pCur_gl_packet->get_param_client_memory<GLvoid>(2), m_pCur_gl_packet->get_param_value<GLsizei>(3));
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            return;
        }
    }

    check_gl_error();

    handle_post_link_program(entrypoint_id, trace_handle, replay_handle, GL_NONE, 0, NULL);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::post_draw_call
// Called after each draw call or blit.
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::post_draw_call()
{
    VOGL_FUNC_TRACER

    if (m_pCur_context_state->m_inside_gl_begin)
        return cStatusOK;

    if (check_gl_error())
        return cStatusGLError;

    bool is_draw = vogl_is_draw_entrypoint(m_pCur_gl_packet->get_entrypoint_id());

    if ((m_flags & cGLReplayerDumpShadersOnDraw) && (is_draw))
    {
        dump_current_shaders();
    }

    if (m_flags & cGLReplayerDumpFramebufferOnDraws)
    {
        bool should_dump = false;

        if (m_dump_framebuffer_on_draw_frame_index != -1)
        {
            if (m_frame_index == m_dump_framebuffer_on_draw_frame_index)
                should_dump = true;
        }
        else if ((m_dump_framebuffer_on_draw_first_gl_call_index >= 0) && (m_dump_framebuffer_on_draw_last_gl_call_index >= 0))
        {
            should_dump = math::is_within_closed_range<uint64_t>(m_last_parsed_call_counter, m_dump_framebuffer_on_draw_first_gl_call_index, m_dump_framebuffer_on_draw_last_gl_call_index);
        }
        else
        {
            should_dump = true;
        }

        if (should_dump)
        {
            dump_current_framebuffer();
        }
    }

    m_frame_draw_counter += is_draw;

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_framebuffer
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::dump_framebuffer(
        uint32_t width, uint32_t height, GLuint read_framebuffer, GLenum read_buffer, GLenum internal_format, uint32_t orig_samples, GLuint replay_texture, GLuint replay_rbo,
        int channel_to_write, bool force_rgb, GLuint fmt, GLuint type, const char *pFilename_suffix)
{
    VOGL_FUNC_TRACER

    uint32_t trace_read_framebuffer = 0;
    if (read_framebuffer)
    {
        gl_handle_hash_map::const_iterator it = get_context_state()->m_framebuffers.search_table_for_value(read_framebuffer);
        if (it != get_context_state()->m_framebuffers.end())
            trace_read_framebuffer = it->second;
    }

    uint32_t trace_texture = replay_texture;
    if (replay_texture)
    {
        if (!get_shared_state()->m_shadow_state.m_textures.map_inv_handle_to_handle(replay_texture, trace_texture))
            vogl_warning_printf("Failed finding GL handle %u in texture handle shadow!\n", replay_texture);
    }

    uint32_t trace_rbo = 0;
    if (replay_rbo)
    {
        if (!get_shared_state()->m_shadow_state.m_rbos.map_inv_handle_to_handle(replay_rbo, trace_rbo))
            vogl_error_printf("Failed finding GL handle %u in RBO handle shadow!\n", replay_rbo);
    }

    uint32_t image_fmt_size = vogl_get_image_format_size_in_bytes(fmt, type);
    const uint32_t total_pixels = width * height;

    m_screenshot_buffer.resize(width * height * image_fmt_size);

    //bool success = vogl_copy_buffer_to_image(m_screenshot_buffer.get_ptr(), m_screenshot_buffer.size(), width, height, GL_RGB, GL_UNSIGNED_BYTE, false, read_framebuffer, read_buffer, 0);
    bool success = vogl_copy_buffer_to_image(m_screenshot_buffer.get_ptr(), m_screenshot_buffer.size(), width, height, fmt, type, false, read_framebuffer, read_buffer, 0);

    if (!success)
    {
        process_entrypoint_warning("vogl_copy_buffer_to_image() failed!\n");
        return false;
    }

    uint8_t *pImage_data = m_screenshot_buffer.get_ptr();
    uint32_t image_data_bpp = image_fmt_size;

    if (channel_to_write >= 0)
    {
        VOGL_ASSERT(channel_to_write < static_cast<int>(image_fmt_size));

        m_screenshot_buffer2.resize(width * height);

        for (uint32_t i = 0; i < total_pixels; i++)
            m_screenshot_buffer2[i] = m_screenshot_buffer[image_fmt_size * i + channel_to_write];

        pImage_data = m_screenshot_buffer2.get_ptr();
        image_data_bpp = 1;

        m_screenshot_buffer2.swap(m_screenshot_buffer);
    }

    if ((force_rgb) && (image_data_bpp != 3))
    {
        m_screenshot_buffer2.resize(width * height * 3);
        m_screenshot_buffer2.set_all(0);

        for (uint32_t i = 0; i < total_pixels; i++)
        {
            if (image_data_bpp == 1)
            {
                uint8_t c = pImage_data[i * image_data_bpp];
                memset(&m_screenshot_buffer2[i * 3], c, 3);
            }
            else
            {
                memcpy(&m_screenshot_buffer2[i * 3], &pImage_data[i * image_data_bpp], math::minimum(3U, image_data_bpp));
            }
        }

        pImage_data = m_screenshot_buffer2.get_ptr();
        image_data_bpp = 3;

        m_screenshot_buffer2.swap(m_screenshot_buffer);
    }

    if ((image_data_bpp < 1) || (image_data_bpp > 4))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    size_t png_size = 0;
    void *pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(pImage_data, width, height, image_data_bpp, &png_size, 1, true);

    // FIXME:
    // Removing frame # for now so diffing two directories full of screenshots from different traces is easier. Once we add the original frame # in trims this can be fixed.
    //dynamic_string screenshot_filename(cVarArg, "%s_GLCTR%08llu_%s_FR%06u_DCTR%05llu_W%04i_H%04i_FBO%04u_%s",
    dynamic_string screenshot_filename(cVarArg, "%s_GLCTR%08llu_%s_DCTR%05llu_W%04i_H%04i_FBO%04u_%s",
                                       m_dump_framebuffer_on_draw_prefix.get_ptr(),
                                       (unsigned long long)m_pCur_gl_packet->get_call_counter(),
                                       g_vogl_entrypoint_descs[m_pCur_gl_packet->get_entrypoint_id()].m_pName,
                                       //m_frame_index,
                                       (unsigned long long)m_frame_draw_counter,
                                       width, height,
                                       trace_read_framebuffer,
                                       get_gl_enums().find_gl_name(read_buffer));

    if (internal_format != GL_NONE)
    {
        screenshot_filename += "_";
        screenshot_filename += get_gl_enums().find_gl_image_format_name(internal_format);
    }

    if (orig_samples != 0)
        screenshot_filename += dynamic_string(cVarArg, "_MSAA%u", orig_samples);

    if (replay_texture)
    {
        GLuint trace_texture2 = 0;
        get_shared_state()->m_shadow_state.m_textures.map_inv_handle_to_handle(replay_texture, trace_texture2);
        screenshot_filename += dynamic_string(cVarArg, "_TEX%04u", trace_texture2);
    }

    if (replay_rbo)
    {
        GLuint trace_rbo2 = 0;
        get_shared_state()->m_shadow_state.m_rbos.map_inv_handle_to_handle(replay_rbo, trace_rbo2);
        screenshot_filename += dynamic_string(cVarArg, "_RBO%04u", trace_rbo2);
    }

    if (pFilename_suffix)
        screenshot_filename += pFilename_suffix;

    screenshot_filename += ".png";

    file_utils::create_directories(file_utils::get_pathname(screenshot_filename.get_ptr()), false);

    if (!file_utils::write_buf_to_file(screenshot_filename.get_ptr(), pPNG_data, png_size))
    {
        process_entrypoint_error("Failed writing framebuffer screenshot to file \"%s\"\n", screenshot_filename.get_ptr());
        success = false;
    }
    else
    {
        vogl_message_printf("Wrote framebuffer screenshot to file \"%s\"\n", screenshot_filename.get_ptr());
    }

    mz_free(pPNG_data);

    return success;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_current_framebuffer
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::dump_current_framebuffer()
{
    VOGL_FUNC_TRACER

    uint32_t draw_framebuffer_binding = vogl_get_gl_integer(GL_DRAW_FRAMEBUFFER_BINDING);

    uint32_t max_draw_buffers = vogl_get_gl_integer(GL_MAX_DRAW_BUFFERS);
    if (!max_draw_buffers)
    {
        process_entrypoint_warning("GL_MAX_DRAW_BUFFERS is 0\n");
        return;
    }

    //GL_COLOR_ATTACHMENT0-GL_COLOR_ATTACHMENT15, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT

    vogl::vector<GLenum> draw_buffers(max_draw_buffers);
    for (uint32_t i = 0; i < max_draw_buffers; i++)
        draw_buffers[i] = vogl_get_gl_integer(GL_DRAW_BUFFER0 + i);

    if (!draw_framebuffer_binding)
    {
        for (uint32_t i = 0; i < max_draw_buffers; i++)
        {
            if (draw_buffers[i] != GL_NONE)
            {
                dump_framebuffer(m_pWindow->get_width(), m_pWindow->get_height(), 0, draw_buffers[i], GL_NONE, 0, 0, 0, -1, false, GL_RGB, GL_UNSIGNED_BYTE, "");
                dump_framebuffer(m_pWindow->get_width(), m_pWindow->get_height(), 0, draw_buffers[i], GL_NONE, 0, 0, 0, -1, true, GL_ALPHA, GL_UNSIGNED_BYTE, "_A");
            }
        }

        return;
    }

    // TODO: We should probably keep around a persistent set of per-context (or sharelist) remappers
    vogl_framebuffer_state fbo_state;
    if (!fbo_state.snapshot(m_pCur_context_state->m_context_info, m_replay_to_trace_remapper, draw_framebuffer_binding, GL_NONE))
    {
        process_entrypoint_warning("Unable to snapshot current FBO %u\n", draw_framebuffer_binding);
        return;
    }

    for (uint32_t i = 0; i < draw_buffers.size(); i++)
    {
        if (draw_buffers[i] == GL_NONE)
            continue;

        const vogl_framebuffer_attachment *pAttachment = fbo_state.get_attachments().find_value(draw_buffers[i]);
        if (!pAttachment)
        {
            process_entrypoint_warning("Can't find draw buffer %s in currently bound FBO %u\n", get_gl_enums().find_gl_name(draw_buffers[i]), draw_framebuffer_binding);
            continue;
        }

        if (pAttachment->get_type() == GL_FRAMEBUFFER_DEFAULT)
            continue;
        else if (pAttachment->get_type() == GL_RENDERBUFFER)
        {
            GLuint rbo_handle = pAttachment->get_param(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
            if (!rbo_handle)
                continue;

            vogl_renderbuffer_state rbo_state;
            if (!rbo_state.snapshot(m_pCur_context_state->m_context_info, m_replay_to_trace_remapper, rbo_handle, GL_NONE))
            {
                process_entrypoint_warning("Failed getting RBO %u's' state!\n", rbo_handle);
                continue;
            }

            if (rbo_state.get_desc().get_int_or_default(GL_RENDERBUFFER_DEPTH_SIZE) || rbo_state.get_desc().get_int_or_default(GL_RENDERBUFFER_STENCIL_SIZE))
                continue;

            uint32_t width = rbo_state.get_desc().get_int_or_default(GL_RENDERBUFFER_WIDTH);
            uint32_t height = rbo_state.get_desc().get_int_or_default(GL_RENDERBUFFER_HEIGHT);
            uint32_t samples = rbo_state.get_desc().get_int_or_default(GL_RENDERBUFFER_SAMPLES);
            GLenum internal_format = rbo_state.get_desc().get_int_or_default(GL_RENDERBUFFER_INTERNAL_FORMAT);
            const vogl_internal_tex_format *pFmt = vogl_find_internal_texture_format(internal_format);

            if ((!width) || (!height) || (!internal_format))
            {
                process_entrypoint_warning("Unable to determine FBO %u color attachment %u's RBO %u's dimensions\n", draw_framebuffer_binding, i, rbo_handle);
                continue;
            }

            if (samples > 1)
            {
                vogl_scoped_binding_state orig_framebuffers(GL_DRAW_FRAMEBUFFER, GL_READ_FRAMEBUFFER, GL_RENDERBUFFER);

                GLuint temp_rbo = 0;
                GL_ENTRYPOINT(glGenRenderbuffers)(1, &temp_rbo);
                check_gl_error();

                if (!temp_rbo)
                    continue;

                GL_ENTRYPOINT(glBindRenderbuffer)(GL_RENDERBUFFER, temp_rbo);
                check_gl_error();

                GL_ENTRYPOINT(glRenderbufferStorage)(GL_RENDERBUFFER, internal_format, width, height);
                check_gl_error();

                GLuint temp_fbo = 0;
                GL_ENTRYPOINT(glGenFramebuffers)(1, &temp_fbo);
                check_gl_error();

                GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, temp_fbo);
                check_gl_error();

                GL_ENTRYPOINT(glFramebufferRenderbuffer)(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, temp_rbo);
                check_gl_error();

                GLenum draw_buf = GL_COLOR_ATTACHMENT0;
                GL_ENTRYPOINT(glDrawBuffers)(1, &draw_buf);
                check_gl_error();

                GL_ENTRYPOINT(glReadBuffer)(GL_NONE);
                check_gl_error();

                GLenum cur_status = GL_ENTRYPOINT(glCheckFramebufferStatus)(GL_DRAW_FRAMEBUFFER);
                check_gl_error();

                if (cur_status == GL_FRAMEBUFFER_COMPLETE)
                {
                    GL_ENTRYPOINT(glBindFramebuffer)(GL_READ_FRAMEBUFFER, draw_framebuffer_binding);
                    check_gl_error();

                    // Save the framebuffer's readbuffer (it's per-framebuffer state, not context state).
                    vogl_scoped_state_saver state_saver(cGSTReadBuffer);

                    GL_ENTRYPOINT(glReadBuffer)(draw_buffers[i]);
                    check_gl_error();

                    GL_ENTRYPOINT(glBlitFramebuffer)(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

                    if (!check_gl_error())
                    {
                        dump_framebuffer(width, height, temp_fbo, GL_COLOR_ATTACHMENT0, internal_format, samples, 0, rbo_handle, -1, false, GL_RGB, GL_UNSIGNED_BYTE, "");

                        if ((pFmt) && (pFmt->m_comp_sizes[3]))
                        {
                            dump_framebuffer(width, height, temp_fbo, GL_COLOR_ATTACHMENT0, internal_format, samples, 0, rbo_handle, -1, true, GL_ALPHA, GL_UNSIGNED_BYTE, "_A");
                        }
                    }
                    else
                    {
                        process_entrypoint_warning("Failed downsampling FBO %u color attachment %u's RBO %u to temporary RBO\n", draw_framebuffer_binding, i, rbo_handle);
                    }
                }

                GL_ENTRYPOINT(glBindRenderbuffer)(GL_RENDERBUFFER, 0);
                check_gl_error();

                GL_ENTRYPOINT(glBindFramebuffer)(GL_DRAW_FRAMEBUFFER, 0);
                check_gl_error();

                GL_ENTRYPOINT(glDeleteFramebuffers)(1, &temp_fbo);
                check_gl_error();

                GL_ENTRYPOINT(glDeleteRenderbuffers)(1, &temp_rbo);
                check_gl_error();
            }
            else
            {
                dump_framebuffer(width, height, draw_framebuffer_binding, draw_buffers[i], internal_format, 0, 0, rbo_handle, -1, false, GL_RGB, GL_UNSIGNED_BYTE, "");

                if ((pFmt) && (pFmt->m_comp_sizes[3]))
                {
                    dump_framebuffer(width, height, draw_framebuffer_binding, draw_buffers[i], internal_format, 0, 0, rbo_handle, -1, true, GL_ALPHA, GL_UNSIGNED_BYTE, "_A");
                }
            }
        }
        else if (pAttachment->get_type() == GL_TEXTURE)
        {
            GLuint tex_handle = pAttachment->get_param(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
            if (!tex_handle)
            {
                process_entrypoint_warning("Current FBO %u has a invalid object name\n", draw_framebuffer_binding);
                continue;
            }

            GLenum target = get_shared_state()->m_shadow_state.m_textures.get_target_inv(tex_handle);
            if (target == GL_NONE)
            {
                process_entrypoint_warning("Current FBO %u first color attachment's type is GL_TEXTURE, but unable to determine the texture's target type, GL texture handle %u\n", draw_framebuffer_binding, tex_handle);
                continue;
            }

            if ((target == GL_TEXTURE_CUBE_MAP) || (target == GL_TEXTURE_CUBE_MAP_ARRAY))
                target = pAttachment->get_param(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE);

            if (!utils::is_in_set<GLenum, GLenum>(target, GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_RECTANGLE, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP_ARRAY))
            {
                process_entrypoint_warning("Unsupported FBO attachment texture target type (%s), GL texture handle %u\n", get_gl_enums().find_gl_name(target), tex_handle);
                continue;
            }

            uint32_t level = pAttachment->get_param(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
            uint32_t layer = pAttachment->get_param(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
            VOGL_NOTE_UNUSED(layer);

            GLint width = 0, height = 0, samples = 0;
            GLenum internal_format = GL_NONE;

            {
                vogl_scoped_binding_state binding_saver;
                binding_saver.save_textures(&(m_pCur_context_state->m_context_info));

                GL_ENTRYPOINT(glBindTexture)(target, tex_handle);
                check_gl_error();

                GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_WIDTH, &width);
                check_gl_error();

                GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_HEIGHT, &height);
                check_gl_error();

                GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_format));
                check_gl_error();

                if (m_pCur_context_state->m_context_info.supports_extension("GL_ARB_texture_multisample"))
                {
                    GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, GL_TEXTURE_SAMPLES, &samples);
                    check_gl_error();
                }
            }

            const vogl_internal_tex_format *pFmt = vogl_find_internal_texture_format(internal_format);

            if ((!width) || (!height))
            {
                process_entrypoint_warning("Unable to determine FBO %u color attachment %u's texture %u's dimensions\n", draw_framebuffer_binding, i, tex_handle);
                continue;
            }

            if (samples > 1)
            {
                // TODO: We have the code to do this now, just need to hook it up.
                process_entrypoint_warning("Can't dump multisample texture FBO attachments yet\n");
                continue;
            }

            dump_framebuffer(width, height, draw_framebuffer_binding, draw_buffers[i], internal_format, 0, tex_handle, 0, -1, false, GL_RGB, GL_UNSIGNED_BYTE, "");

            if ((pFmt) && (pFmt->m_comp_sizes[3]))
            {
                dump_framebuffer(width, height, draw_framebuffer_binding, draw_buffers[i], internal_format, 0, tex_handle, 0, -1, true, GL_ALPHA, GL_UNSIGNED_BYTE, "_A");
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::dump_current_shaders
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::dump_current_shaders()
{
    VOGL_FUNC_TRACER

    if (!m_pCur_context_state)
        return;

    check_gl_error();

    const GLuint replay_program = m_pCur_context_state->m_cur_replay_program;

    // Get the current program.
    GLuint current_program = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_CURRENT_PROGRAM, (GLint *)(&current_program));
    check_gl_error();

    VOGL_ASSERT(replay_program == current_program);

    if (!current_program)
        return;

    // Get the attached shaders.
    GLsizei attached_shader_count = -1;
    GL_ENTRYPOINT(glGetProgramiv)(replay_program, GL_ATTACHED_SHADERS, &attached_shader_count);
    check_gl_error();

    if (!attached_shader_count)
        return;

    vogl::vector<GLuint> shaders(attached_shader_count);
    GLsizei actual_shader_count = 0;
    GL_ENTRYPOINT(glGetAttachedShaders)(replay_program,
                                        attached_shader_count,
                                        &actual_shader_count,
                                        shaders.get_ptr());
    check_gl_error();

    VOGL_ASSERT(attached_shader_count == actual_shader_count); // Sanity check.

    vogl_printf("Trace context 0x%" PRIx64 ", GL draw counter %" PRIu64 ", frame %u, replay program %u trace program %u has %d attached shaders:\n",
               cast_val_to_uint64(m_cur_trace_context), m_last_parsed_call_counter, m_frame_index,
               replay_program, m_pCur_context_state->m_cur_trace_program,
               attached_shader_count);

    // Get source from shaders.
    vogl::vector<GLchar> source; // Shared buffer for each iteration.
    for (GLsizei i = 0; i < attached_shader_count; ++i)
    {
        const GLuint shader = shaders[i];
        GLint shader_type = 0;
        GL_ENTRYPOINT(glGetShaderiv)(shader, GL_SHADER_TYPE, &shader_type);
        check_gl_error();

        vogl_printf("\n%s: %u\n", get_gl_enums().find_gl_name(shader_type), shader);

        GLint source_length = -1; // Includes NUL terminator.
        GL_ENTRYPOINT(glGetShaderiv)(shader, GL_SHADER_SOURCE_LENGTH, &source_length);
        check_gl_error();

        VOGL_ASSERT(source_length > 0);

        source.resize(source_length);
        GLint actual_length = 0; // Excludes NUL terminator!
        GL_ENTRYPOINT(glGetShaderSource)(shader, source_length, &actual_length, source.get_ptr());
        check_gl_error();

        VOGL_ASSERT(source_length == actual_length + 1); // Sanity check.
        vogl_printf("%.*s\n", source_length, source.get_const_ptr());
    }
    vogl_printf("========\n");
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_ShaderSource
// Handle ShaderSource and ShaderSourceARB.
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::handle_ShaderSource(GLhandleARB trace_object,
                                                               GLsizei count,
                                                               const vogl_client_memory_array trace_strings_glchar_ptr_array,
                                                               const GLint *pTrace_lengths)
{
    VOGL_FUNC_TRACER

    GLhandleARB replay_object = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_object);

    // m_pCur_gl_packet->get_param_client_memory_data_size(2) / sizeof(const GLchar *);
    const uint32_t trace_strings_count = trace_strings_glchar_ptr_array.size();
    const uint32_t trace_lengths_count = m_pCur_gl_packet->get_param_client_memory_data_size(3) / sizeof(const GLint);

    if ((trace_strings_glchar_ptr_array.get_ptr()) &&
        (trace_strings_count != static_cast<uint32_t>(count)))
    {
        process_entrypoint_error("Trace strings array has an invalid count (expected %u, got %u)\n",
                                 count, trace_strings_count);
        return cStatusHardFailure;
    }

    if ((pTrace_lengths) && (trace_lengths_count != static_cast<uint32_t>(count)))
    {
        process_entrypoint_error("Trace lengths array has an invalid count (expected %u, got %u)\n",
                                 count, trace_lengths_count);
        return cStatusHardFailure;
    }

    vogl::vector<const GLcharARB *> strings(count);
    vogl::vector<GLint> lengths(count);

    const key_value_map &map = m_pCur_gl_packet->get_key_value_map();

    vogl::vector<uint8_vec> blobs(count);

    for (GLsizei i = 0; i < count; i++)
    {
        strings[i] = NULL;
        if ((trace_strings_glchar_ptr_array.get_ptr()) &&
            (trace_strings_glchar_ptr_array.get_element<vogl_trace_ptr_value>(i) != 0))
        {
            strings[i] = "";
        }

        lengths[i] = pTrace_lengths ? pTrace_lengths[i] : 0;

        key_value_map::const_iterator it = map.find(i);
        if (it == map.end())
        {
            if (lengths[i] > 0)
            {
                process_entrypoint_error("Failed finding blob for non-empty string %i in packet's key value map\n", i);
                return cStatusHardFailure;
            }
            continue;
        }

        const uint8_vec *pBlob = it->second.get_blob();
        if (!pBlob)
        {
            process_entrypoint_error("Can't convert string %i to a blob\n", i);
            return cStatusHardFailure;
        }

        blobs[i] = *pBlob;
        uint8_vec &blob = blobs[i];

        if ((pTrace_lengths) && (pTrace_lengths[i] >= 0))
        {
            if (static_cast<uint32_t>(pTrace_lengths[i]) != blob.size())
            {
                process_entrypoint_warning("Length value (%u) stored in length array at index %u doesn't match string %u's length - changing to match\n", pTrace_lengths[i], i, blob.size());
                lengths[i] = blob.size();
            }
        }
        else
        {
            if ((blob.size()) && (blob.back() != '\0'))
            {
                process_entrypoint_warning("String %u doesn't end in 0 terminator - appending terminator\n", i);

                blob.push_back('\0');
            }

            VOGL_ASSERT(blob.size() &&
                          (blob.back() == '\0') &&
                          (blob.size() == (1 + vogl_strlen(reinterpret_cast<const char *>(blob.get_ptr())))));
        }

        strings[i] = reinterpret_cast<const GLcharARB *>(blob.get_ptr());
    }
    if (m_flags & cGLReplayerFSPreprocessor)
    {
        // Query the shader type to verify if it's an FS
        GLint type = -1;
        GL_ENTRYPOINT(glGetShaderiv)(replay_object, GL_SHADER_TYPE, &type);
        if (GL_FRAGMENT_SHADER == type)
        {
            // Assuming that m_fs_pp class was instantiated when this class was and that it already has pp cmd & option info
            // TODO : Need to handle the case where count != 1
            m_fs_pp->set_shader(static_cast<GLuint>(trace_object), strings, lengths);
            if (m_fs_pp->run())
            {
                count = m_fs_pp->get_count();
                strings[0] = m_fs_pp->get_output_shader();
                lengths[0] = m_fs_pp->get_length();
            }
            else
                return cStatusHardFailure;
        }
    }
    // no pre-processor so just use passed in values
    if (m_pCur_gl_packet->get_entrypoint_id() == VOGL_ENTRYPOINT_glShaderSource)
    {
        GL_ENTRYPOINT(glShaderSource)(replay_object,
                                      count,
                                      trace_strings_glchar_ptr_array.get_ptr() ? (GLchar * const *)strings.get_ptr() : NULL,
                                      pTrace_lengths ? lengths.get_ptr() : NULL);
    }
    else
    {
        GL_ENTRYPOINT(glShaderSourceARB)(replay_object,
                                         count,
                                         trace_strings_glchar_ptr_array.get_ptr() ? strings.get_ptr() : NULL,
                                         pTrace_lengths ? lengths.get_ptr() : NULL);
    }
    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::display_list_bind_callback
// handle is in the trace namespace
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::display_list_bind_callback(vogl_namespace_t handle_namespace, GLenum target, GLuint handle, void *pOpaque)
{
    VOGL_FUNC_TRACER

    vogl_gl_replayer *pReplayer = static_cast<vogl_gl_replayer *>(pOpaque);

    if (handle_namespace == VOGL_NAMESPACE_TEXTURES)
    {
        if ((handle) && (target != GL_NONE))
        {
            // A conditional update because we can't really test to see if the bind inside the display list really succeeded.
            pReplayer->get_shared_state()->m_shadow_state.m_textures.conditional_update(handle, GL_NONE, target);
        }
    }
    else
    {
        // TODO - right now the display list whitelist doesn't let anything else get bound.
        pReplayer->process_entrypoint_warning("Unsupported bind in display lists, namespace %s target %s trace handle %u\n", vogl_get_namespace_name(handle_namespace), get_gl_enums().find_gl_name(target), handle);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Helper macros - slightly simplifies hand-generating entrypoints with EXT/ARB/etc. variants
//----------------------------------------------------------------------------------------------------------------------
#define SWITCH_GL_ENTRYPOINT2(e0, e1, ...)                 \
    if (entrypoint_id == VOGL_JOIN(VOGL_ENTRYPOINT_, e0)) \
        result = GL_ENTRYPOINT(e0)(__VA_ARGS__);           \
    else                                                   \
        result = GL_ENTRYPOINT(e1)(__VA_ARGS__);

#define SWITCH_GL_ENTRYPOINT3(e0, e1, e2, ...)                  \
    if (entrypoint_id == VOGL_JOIN(VOGL_ENTRYPOINT_, e0))      \
        result = GL_ENTRYPOINT(e0)(__VA_ARGS__);                \
    else if (entrypoint_id == VOGL_JOIN(VOGL_ENTRYPOINT_, e1)) \
        result = GL_ENTRYPOINT(e1)(__VA_ARGS__);                \
    else                                                        \
        result = GL_ENTRYPOINT(e2)(__VA_ARGS__);

#define SWITCH_GL_ENTRYPOINT2_VOID(e0, e1, ...)            \
    if (entrypoint_id == VOGL_JOIN(VOGL_ENTRYPOINT_, e0)) \
        GL_ENTRYPOINT(e0)(__VA_ARGS__);                    \
    else                                                   \
        GL_ENTRYPOINT(e1)(__VA_ARGS__);

#define SWITCH_GL_ENTRYPOINT3_VOID(e0, e1, e2, ...)             \
    if (entrypoint_id == VOGL_JOIN(VOGL_ENTRYPOINT_, e0))      \
        GL_ENTRYPOINT(e0)(__VA_ARGS__);                         \
    else if (entrypoint_id == VOGL_JOIN(VOGL_ENTRYPOINT_, e1)) \
        GL_ENTRYPOINT(e1)(__VA_ARGS__);                         \
    else                                                        \
        GL_ENTRYPOINT(e2)(__VA_ARGS__);

//----------------------------------------------------------------------------------------------------------------------
// should be called just prior to process_gl_entrypoint_packet(...)
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_pending_gl_entrypoint_packets()
{
    status_t status = cStatusOK;

    if (m_pPending_snapshot)
    {
        status = process_applying_pending_snapshot();
        if (status != cStatusOK)
            return status;
    }

    if (m_pending_make_current_packet.is_valid())
    {
        status = process_pending_make_current();
        if (status != cStatusOK)
            return status;
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_gl_entrypoint_packet
// This will be called during replaying, or when building display lists during state restoring.
// Call process_pending_gl_entrypoint_packets() before this to ensure pending snapshots and makeCurrents have completed.
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_gl_entrypoint_packet(vogl_trace_packet& trace_packet)
{
    m_pCur_gl_packet = &trace_packet;

    status_t status = cStatusOK;

    const vogl_trace_gl_entrypoint_packet &entrypoint_packet = trace_packet.get_entrypoint_packet();

    m_last_parsed_call_counter = entrypoint_packet.m_call_counter;

    status = process_gl_entrypoint_packet_internal(trace_packet);

    if (status != cStatusResizeWindow)
        m_last_processed_call_counter = entrypoint_packet.m_call_counter;

    m_pCur_gl_packet = NULL;

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_gl_entrypoint_packet_internal
// This will be called during replaying, or when building display lists during state restoring.
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_gl_entrypoint_packet_internal(vogl_trace_packet& trace_packet)
{
    VOGL_FUNC_TRACER

    m_at_frame_boundary = false;

    const vogl_trace_gl_entrypoint_packet &gl_entrypoint_packet = trace_packet.get_entrypoint_packet();
    const gl_entrypoint_id_t entrypoint_id = trace_packet.get_entrypoint_id();

    if (m_flags & cGLReplayerDebugMode)
        dump_trace_gl_packet_debug_info(gl_entrypoint_packet);

    if (m_flags & cGLReplayerDebugMode)
        dump_packet_as_func_call(trace_packet);

    if (m_flags & cGLReplayerDumpAllPackets)
        print_detailed_context(VOGL_FUNCTION_INFO_CSTR, cMsgDebug);

    if (entrypoint_id == VOGL_ENTRYPOINT_glInternalTraceCommandRAD)
        return process_internal_trace_command(gl_entrypoint_packet);

    status_t status = cStatusOK;

    if (gl_entrypoint_packet.m_context_handle != m_cur_trace_context)
    {
        status = switch_contexts(gl_entrypoint_packet.m_context_handle);
        if (status != cStatusOK)
            return status;
    }

    bool processed_glx_packet = true;
    switch (entrypoint_id)
    {
        case VOGL_ENTRYPOINT_wglDeleteContext:
        case VOGL_ENTRYPOINT_glXDestroyContext:
        {
            vogl_trace_context_ptr_value trace_context = trace_packet.get_param_ptr_value(entrypoint_id == VOGL_ENTRYPOINT_wglDeleteContext ? 0 : 1);
            GLReplayContextType replay_context = remap_context(trace_context);

            if ((trace_context) && (!replay_context))
            {
                process_entrypoint_error("Failed remapping GL trace context 0x%" PRIx64 "\n", (uint64_t)trace_context);
                return cStatusHardFailure;
            }

            if (trace_context == m_cur_trace_context)
            {
                process_entrypoint_warning("glXDestroyContext() called while trace context 0x%" PRIx64 " is still current, forcing it to not be current\n",
                                           (uint64_t)trace_context);

                m_cur_trace_context = 0;
                m_cur_replay_context = 0;
                m_pCur_context_state = NULL;
            }

            m_pWindow->destroy_context(replay_context);

            destroy_context(trace_context);

            break;
        }

        case VOGL_ENTRYPOINT_wglMakeCurrent:
        {
            Bool trace_result = trace_packet.get_return_value<Bool>();

            vogl_trace_context_ptr_value trace_context = trace_packet.get_param_ptr_value(1);

            // pContext_state can be NULL!
            context_state *pContext_state = get_trace_context_state(trace_context);
            GLReplayContextType replay_context = pContext_state ? pContext_state->m_replay_context : 0;

            if ((trace_context) && (!replay_context))
            {
                process_entrypoint_error("Failed remapping GL context\n");
                return cStatusHardFailure;
            }

            int viewport_x = trace_packet.get_key_value_map().get_int(string_hash("viewport_x"));
            VOGL_NOTE_UNUSED(viewport_x);
            int viewport_y = trace_packet.get_key_value_map().get_int(string_hash("viewport_y"));
            VOGL_NOTE_UNUSED(viewport_y);
            int viewport_width = trace_packet.get_key_value_map().get_int(string_hash("viewport_width"));
            VOGL_NOTE_UNUSED(viewport_width);
            int viewport_height = trace_packet.get_key_value_map().get_int(string_hash("viewport_height"));
            VOGL_NOTE_UNUSED(viewport_height);
            int win_width = trace_packet.get_key_value_map().get_int(string_hash("win_width"));
            int win_height = trace_packet.get_key_value_map().get_int(string_hash("win_height"));

            // We may need to defer the make current until the window is the proper size, because the initial GL viewport's state depends on the Window size. Ugh.
            if ((trace_context) && (trace_result))
            {
                if ((win_width) && (win_height))
                {
                    if (!(m_flags & cGLReplayerLockWindowDimensions))
                    {
                        if ((m_pWindow->get_width() != win_width) || (m_pWindow->get_height() != win_height))
                        {
                            m_pending_make_current_packet = *m_pCur_gl_packet;

                            status = trigger_pending_window_resize(win_width, win_height);

                            vogl_printf("Deferring MakeCurrent() until window resizes to %ux%u\n", win_width, win_height);
                        }
                    }
                }
            }

            if (status != cStatusResizeWindow)
            {
                bool result = m_pWindow->make_current(replay_context);

                // This needs to be done after every (or at least the first) make current because windows can't get proper 
                // extensions until MakeCurrent time.
                if (m_proc_address_helper_func)
                    vogl_init_actual_gl_entrypoints(m_proc_address_helper_func, m_wrap_all_gl_calls);

                if (!result)
                {
                    if (trace_result)
                    {
                        process_entrypoint_error("Failed making context current, but in the trace this call succeeded!\n");
                        return cStatusHardFailure;
                    }
                    else
                    {
                        process_entrypoint_warning("Failed making context current, in the trace this call also failed\n");
                    }
                }
                else
                {
                    m_cur_trace_context = trace_context;
                    m_cur_replay_context = replay_context;
                    m_pCur_context_state = pContext_state;

                    if (!trace_result)
                    {
                        process_entrypoint_warning("Context was successfuly made current, but this operation failed in the trace\n");
                    }

                    #if 0
                        vogl_printf("glXMakeCurrent(): Trace Viewport: [%u,%u,%u,%u], Window: [%u %u]\n",
                            viewport_x, viewport_y,
                            viewport_width, viewport_height,
                            win_width, win_height);
                    #endif

                    if (m_cur_replay_context)
                    {
                        if (!handle_context_made_current())
                            return cStatusHardFailure;
                    }
                }
            }
            
            break;
        }
        case VOGL_ENTRYPOINT_glXMakeCurrent:
        case VOGL_ENTRYPOINT_glXMakeContextCurrent:
        {
            Bool trace_result = trace_packet.get_return_value<Bool>();

            vogl_trace_context_ptr_value trace_context = trace_packet.get_param_ptr_value((entrypoint_id == VOGL_ENTRYPOINT_glXMakeCurrent) ? 2 : 3);

            // pContext_state can be NULL!
            context_state *pContext_state = get_trace_context_state(trace_context);
            GLReplayContextType replay_context = pContext_state ? pContext_state->m_replay_context : 0;

            if ((trace_context) && (!replay_context))
            {
                process_entrypoint_error("Failed remapping GL context\n");
                return cStatusHardFailure;
            }

            int viewport_x = trace_packet.get_key_value_map().get_int(string_hash("viewport_x"));
            VOGL_NOTE_UNUSED(viewport_x);
            int viewport_y = trace_packet.get_key_value_map().get_int(string_hash("viewport_y"));
            VOGL_NOTE_UNUSED(viewport_y);
            int viewport_width = trace_packet.get_key_value_map().get_int(string_hash("viewport_width"));
            VOGL_NOTE_UNUSED(viewport_width);
            int viewport_height = trace_packet.get_key_value_map().get_int(string_hash("viewport_height"));
            VOGL_NOTE_UNUSED(viewport_height);
            int win_width = trace_packet.get_key_value_map().get_int(string_hash("win_width"));
            int win_height = trace_packet.get_key_value_map().get_int(string_hash("win_height"));

            // We may need to defer the make current until the window is the proper size, because the initial GL viewport's state depends on the Window size. Ugh.
            if ((trace_context) && (trace_result))
            {
                if ((win_width) && (win_height))
                {
                    if (!(m_flags & cGLReplayerLockWindowDimensions))
                    {
                        if ((m_pWindow->get_width() != win_width) || (m_pWindow->get_height() != win_height))
                        {
                            m_pending_make_current_packet = *m_pCur_gl_packet;

                            status = trigger_pending_window_resize(win_width, win_height);

                            vogl_verbose_printf("Deferring glXMakeCurrent() until window resizes to %ux%u\n", win_width, win_height);
                        }
                    }
                }
            }

            if (status != cStatusResizeWindow)
            {
                bool result = m_pWindow->make_current(replay_context);

                // This needs to be done after every (or at least the first) make current because windows can't get proper 
                // extensions until MakeCurrent time.
                if (m_proc_address_helper_func)
                    vogl_init_actual_gl_entrypoints(m_proc_address_helper_func, m_wrap_all_gl_calls);

                if (!result)
                {
                    if (trace_result)
                    {
                        process_entrypoint_error("Failed making context current, but in the trace this call succeeded!\n");
                        return cStatusHardFailure;
                    }
                    else
                    {
                        process_entrypoint_warning("Failed making context current, in the trace this call also failed\n");
                    }
                }
                else
                {
                    m_cur_trace_context = trace_context;
                    m_cur_replay_context = replay_context;
                    m_pCur_context_state = pContext_state;

                    if (!trace_result)
                    {
                        process_entrypoint_warning("Context was successfuly made current, but this operation failed in the trace\n");
                    }

#if 0
				vogl_printf("glXMakeCurrent(): Trace Viewport: [%u,%u,%u,%u], Window: [%u %u]\n",
				           viewport_x, viewport_y,
				           viewport_width, viewport_height,
				           win_width, win_height);
#endif

                    if (m_cur_replay_context)
                    {
                        if (!handle_context_made_current())
                            return cStatusHardFailure;
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glXQueryVersion:
        {
            int major = 0, minor = 0;
            Bool status2 = m_pWindow->query_version(&major, &minor);

            process_entrypoint_message("glXQueryVersion returned major %u minor %u status %u, trace recorded major %u minor %u status %u\n", major, minor, status2,
                                       *trace_packet.get_param_client_memory<int>(1),
                                       *trace_packet.get_param_client_memory<int>(2),
                                       trace_packet.get_return_value<Bool>());

            break;
        }
        case VOGL_ENTRYPOINT_glXChooseFBConfig:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_glXGetFBConfigAttrib:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_glXGetVisualFromFBConfig:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_wglDescribePixelFormat:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_wglSetPixelFormat:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_wglGetPixelFormat:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_wglGetPixelFormatAttribivARB:
        {
            // TODO
            break;
        }
        case VOGL_ENTRYPOINT_wglGetExtensionsStringARB:
        case VOGL_ENTRYPOINT_wglGetExtensionsStringEXT:
        {
            // For cross-platform replay, there's not much we can do.
            // For same-platform replay, we could ensure that the extensions returned are equivalent or a superset
            // of those on the capture platform.
            break;
        }
        case VOGL_ENTRYPOINT_wglGetProcAddress:
        case VOGL_ENTRYPOINT_glXGetProcAddress:
        case VOGL_ENTRYPOINT_glXGetProcAddressARB:
        {
            const GLubyte *procName = trace_packet.get_param_client_memory<GLubyte>(0);
            vogl_trace_ptr_value trace_func_ptr_value = trace_packet.get_return_ptr_value();

            #if (VOGL_PLATFORM_HAS_SDL)
                void *pFunc = (void *)SDL_GL_GetProcAddress(reinterpret_cast<const char*>(procName));
            #else
                #error "Need to implement GetProcAddress for this platform."
                void *pFunc = NULL;
            #endif

            if ((pFunc != NULL) != (trace_func_ptr_value != 0))
            {
                process_entrypoint_warning("GetProcAddress of function %s %s in the replay, but %s in the trace\n",
                                           (const char *)procName,
                                           (pFunc != NULL) ? "succeeded" : "failed",
                                           (trace_func_ptr_value != 0) ? "succeeded" : "failed");
            }
            
            break;
        }
        
        case VOGL_ENTRYPOINT_glXCreateNewContext:
        {
            int render_type = trace_packet.get_param_value<GLint>(2);

            vogl_trace_context_ptr_value trace_share_context = trace_packet.get_param_ptr_value(3);
            GLReplayContextType replay_share_context = remap_context(trace_share_context);

            if ((trace_share_context) && (!replay_share_context))
            {
                process_entrypoint_warning("Failed remapping trace sharelist context 0x%" PRIx64 "!\n", cast_val_to_uint64(trace_share_context));
            }

            Bool direct = trace_packet.get_param_value<Bool>(4);
            vogl_trace_context_ptr_value trace_context = trace_packet.get_return_ptr_value();

            if (m_flags & cGLReplayerForceDebugContexts)
            {
                process_entrypoint_warning("glxCreateNewContext() called but we're trying to force debug contexts, which requires us to call glXCreateContextAttribsARB(). This may fail if the user has called glXCreateWindow().\n");

                status = create_context_attribs(trace_context, trace_share_context, replay_share_context, direct, NULL, 0, false);
                if (status != cStatusOK)
                    return status;
            }
            else
            {
                GLReplayContextType replay_context = m_pWindow->create_new_context(replay_share_context, render_type, direct); 

                if (!replay_context)
                {
                    if (trace_context)
                    {
                        process_entrypoint_error("Failed creating new GL context!\n");
                        return cStatusHardFailure;
                    }
                    else
                    {
                        // TODO: This logic is wrong, this case is where both the traced app and the replay failed to create a context.
                        process_entrypoint_warning("Successfully created a new GL context where the traced app failed!\n");
                    }
                }

                if (replay_context)
                {
                    if (trace_context)
                    {
                        context_state *pContext_state = define_new_context(trace_context, replay_context, trace_share_context, direct, VOGL_ENTRYPOINT_glXCreateNewContext, NULL, 0);
                        VOGL_NOTE_UNUSED(pContext_state);
                    }
                    else
                    {
                        m_pWindow->destroy_context(replay_context);
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_wglCreateContext:
        {
            // We just force this to true for windows; it's bogus here.
            const Bool cDirect = 1;

            vogl_trace_context_ptr_value trace_context = trace_packet.get_return_ptr_value();
            if (m_flags & cGLReplayerForceDebugContexts)
            {
                status = create_context_attribs(trace_context, (vogl_trace_context_ptr_value)0, NULL, cDirect, NULL, 0, false);
                if (status != cStatusOK)
                    return status;
            }
            else
            {
                GLReplayContextType replay_context = m_pWindow->create_context(NULL, cDirect);

                if (!replay_context)
                {
                    if (trace_context)
                    {
                        process_entrypoint_error("Failed creating new GL context!\n");
                        return cStatusHardFailure;
                    }
                    else
                    {
                        process_entrypoint_warning("Successfully created a new GL context where the traced app failed!\n");
                    }
                }

                if (replay_context)
                {
                    if (trace_context)
                    {
                        context_state *pContext_state = define_new_context(trace_context, replay_context, (vogl_trace_context_ptr_value)0,
                                                                           cDirect, VOGL_ENTRYPOINT_wglCreateContext, NULL, 0);
                        VOGL_NOTE_UNUSED(pContext_state);
                        // On windows load entrypoints here after context is created
                        static bool update_entries = true;
                        if (update_entries)
                        {
                            update_entries = false;
                            bool wrap_all_gl_calls = true;
                            if (benchmark_mode())
                                wrap_all_gl_calls = false;
                            vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper, wrap_all_gl_calls);
                        }
                    }
                    else
                    {
                        m_pWindow->destroy_context(replay_context);
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glXCreateContext:
        {
            vogl_trace_context_ptr_value trace_share_context = trace_packet.get_param_ptr_value(2);
            GLReplayContextType replay_share_context = remap_context(trace_share_context);

            if ((trace_share_context) && (!replay_share_context))
            {
                process_entrypoint_warning("Failed remapping trace sharelist context 0x%" PRIx64 "!\n", cast_val_to_uint64(trace_share_context));
            }

            Bool direct = trace_packet.get_param_value<Bool>(3);
            vogl_trace_context_ptr_value trace_context = trace_packet.get_return_ptr_value();

            if (m_flags & cGLReplayerForceDebugContexts)
            {
                status = create_context_attribs(trace_context, trace_share_context, replay_share_context, direct, NULL, 0, false);
                if (status != cStatusOK)
                    return status;
            }
            else
            {
                GLReplayContextType replay_context = m_pWindow->create_context(replay_share_context, direct);

                if (!replay_context)
                {
                    if (trace_context)
                    {
                        process_entrypoint_error("Failed creating new GL context!\n");
                        return cStatusHardFailure;
                    }
                    else
                    {
                        process_entrypoint_warning("Successfully created a new GL context where the traced app failed!\n");
                    }
                }

                if (replay_context)
                {
                    if (trace_context)
                    {
                        context_state *pContext_state = define_new_context(trace_context, replay_context, trace_share_context, direct, VOGL_ENTRYPOINT_glXCreateContext, NULL, 0);
                        VOGL_NOTE_UNUSED(pContext_state);
                    }
                    else
                    {
                        m_pWindow->destroy_context(replay_context);
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_wglCreateContextAttribsARB:
        {
            vogl_trace_ptr_value trace_share_context = trace_packet.get_param_ptr_value(1);
            GLReplayContextType replay_share_context = remap_context(trace_share_context);

            if ((trace_share_context) && (!replay_share_context))
            {
                process_entrypoint_warning("Failed remapping trace sharelist context 0x%" PRIx64 "!\n", cast_val_to_uint64(trace_share_context));
            }

            const int *pTrace_attrib_list = static_cast<const int *>(trace_packet.get_param_client_memory_ptr(2));
            const uint32_t trace_attrib_list_size = trace_packet.get_param_client_memory_data_size(2) / sizeof(int);

            vogl_trace_ptr_value trace_context = trace_packet.get_return_ptr_value();

            status = create_context_attribs(trace_context, trace_share_context, replay_share_context, true, pTrace_attrib_list, trace_attrib_list_size, true);
            if (status != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_glXCreateContextAttribsARB:
        {
            vogl_trace_ptr_value trace_share_context = trace_packet.get_param_ptr_value(2);
            GLReplayContextType replay_share_context = remap_context(trace_share_context);

            if ((trace_share_context) && (!replay_share_context))
            {
                process_entrypoint_warning("Failed remapping trace sharelist context 0x%" PRIx64 "!\n", cast_val_to_uint64(trace_share_context));
            }

            Bool direct = trace_packet.get_param_value<Bool>(3);
            const int *pTrace_attrib_list = static_cast<const int *>(trace_packet.get_param_client_memory_ptr(4));
            const uint32_t trace_attrib_list_size = trace_packet.get_param_client_memory_data_size(4) / sizeof(int);

            vogl_trace_ptr_value trace_context = trace_packet.get_return_ptr_value();

            status = create_context_attribs(trace_context, trace_share_context, replay_share_context, direct, pTrace_attrib_list, trace_attrib_list_size, true);
            if (status != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_wglSwapBuffers:
        case VOGL_ENTRYPOINT_glXSwapBuffers:
        {
            check_program_binding_shadow();

            if (m_flags & cGLReplayerLowLevelDebugMode)
            {
                if (!validate_program_and_shader_handle_tables())
                    vogl_warning_printf("Failed validating program/shaders against handle mapping tables\n");
                if (!validate_textures())
                    vogl_warning_printf("Failed validating texture handles against handle mapping tables\n");
            }

            if ((m_flags & cGLReplayerHashBackbuffer) || (m_flags & cGLReplayerDumpScreenshots) || (m_flags & cGLReplayerDumpBackbufferHashes))
            {
                snapshot_backbuffer();
            }

            if (m_dump_frontbuffer_filename.has_content())
            {
                dump_frontbuffer_to_file(m_dump_frontbuffer_filename);
                m_dump_frontbuffer_filename.clear();
            }

            m_pWindow->swap_buffers();

            if (m_swap_sleep_time)
                vogl_sleep(m_swap_sleep_time);

            status = cStatusNextFrame;

            m_at_frame_boundary = true;

            if (m_flags & cGLReplayerDebugMode)
            {
                vogl_debug_printf("glXSwapBuffers() processed at end of frame %u, swap %u, last GL call counter %" PRIu64 "\n", m_frame_index, m_total_swaps, m_last_parsed_call_counter);
            }

            m_frame_index++;
            m_total_swaps++;

            m_frame_draw_counter = 0;

            int win_width = trace_packet.get_key_value_map().get_int(string_hash("win_width"));
            int win_height = trace_packet.get_key_value_map().get_int(string_hash("win_height"));
            if ((win_width) && (win_height))
            {
                if (!(m_flags & cGLReplayerLockWindowDimensions))
                {
                    if ((win_width != m_pWindow->get_width()) || (win_height != m_pWindow->get_height()))
                    {
                        // TODO: This resize might need to be deferred until the window system actually resizes the window.
                        //m_pWindow->resize(win_width, win_height);
                        trigger_pending_window_resize(win_width, win_height);

                        vogl_verbose_printf("Resizing window after swap to %ux%u\n", win_width, win_height);
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glXWaitX:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glXWaitX;

            VOGL_REPLAY_CALL_GL_HELPER_glXWaitX;

            break;
        }
        case VOGL_ENTRYPOINT_glXWaitGL:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glXWaitGL;

            VOGL_REPLAY_CALL_GL_HELPER_glXWaitGL;

            break;
        }
        case VOGL_ENTRYPOINT_glXIsDirect:
        {
            vogl_trace_ptr_value trace_context = trace_packet.get_param_ptr_value(1);
            GLReplayContextType replay_context = remap_context(trace_context);

            Bool replay_is_direct = m_pWindow->is_direct(replay_context); 
            Bool trace_is_direct = trace_packet.get_return_value<Bool>();

            if (replay_is_direct != trace_is_direct)
            {
                process_entrypoint_warning("glXIsDirect() returned different results while replaying (%u) vs tracing (%u)!\n", replay_is_direct, trace_is_direct);
            }

            break;
        }

#if VOGL_PLATFORM_HAS_SDL
        case VOGL_ENTRYPOINT_glXGetCurrentContext:
        case VOGL_ENTRYPOINT_wglGetCurrentContext:
        {
            GLReplayContextType replay_context = SDL_GL_GetCurrentContext();
            vogl_trace_ptr_value trace_context = trace_packet.get_return_ptr_value();

            break;

            if ((replay_context != 0) != (trace_context != 0))
            {
                process_entrypoint_warning("glXGetCurrentContext() returned different results while replaying (0x%" PRIX64 ") vs tracing (0x%" PRIX64 ")!\n", (uint64_t)replay_context, (uint64_t)trace_context);
            }

            break;
        }
#else
#   error "Need to handle *GetCurrentContext for this platform."
#endif
        case VOGL_ENTRYPOINT_glXCreateWindow:
        case VOGL_ENTRYPOINT_glXDestroyWindow:
        case VOGL_ENTRYPOINT_glXChooseVisual:
        case VOGL_ENTRYPOINT_glXGetCurrentDisplay:
        case VOGL_ENTRYPOINT_glXQueryDrawable:
        case VOGL_ENTRYPOINT_glXQueryExtension:
        case VOGL_ENTRYPOINT_glXQueryExtensionsString:
        case VOGL_ENTRYPOINT_glXSwapIntervalSGI:
        case VOGL_ENTRYPOINT_glXGetCurrentDrawable:
        case VOGL_ENTRYPOINT_glXGetCurrentReadDrawable:
        case VOGL_ENTRYPOINT_glXQueryContext:
        case VOGL_ENTRYPOINT_glXGetClientString:
        case VOGL_ENTRYPOINT_glXGetConfig:
        case VOGL_ENTRYPOINT_glXGetFBConfigs:
        {
            break;

        }
        #if VOGL_PLATFORM_HAS_SDL
            case VOGL_ENTRYPOINT_wglSwapIntervalEXT:
            case VOGL_ENTRYPOINT_glXSwapIntervalEXT:
            {
                SDL_GL_SetSwapInterval(trace_packet.get_param_value<int>(0));
                break;
            }
        #elif VOGL_PLATFORM_HAS_X11
            case VOGL_ENTRYPOINT_wglSwapIntervalEXT:
            case VOGL_ENTRYPOINT_glXSwapIntervalEXT:
            {
                break;
            }
        #endif
        
        case VOGL_ENTRYPOINT_wglChoosePixelFormat:
        case VOGL_ENTRYPOINT_wglChoosePixelFormatARB:
        {
            // These may be okay to ignore, or we may need to do something with them.
            break;
        }

        default:
        {
            processed_glx_packet = false;
            break;
        }
    }

    if (processed_glx_packet)
    {
        // TODO: Check for GLX errors?
        return status;
    }

    if (!m_cur_replay_context)
    {
        process_entrypoint_error("Trace contains a GL call with no current context! Skipping call.\n");
        return cStatusSoftFailure;
    }

    VOGL_ASSERT(m_pCur_context_state);
    m_pCur_context_state->m_last_call_counter = m_last_parsed_call_counter;

#ifdef VOGL_BUILD_DEBUG
    VOGL_ASSERT(get_trace_context_state(m_cur_trace_context) == m_pCur_context_state);
#endif

    // Add call to current display list
    if ((get_context_state()->is_composing_display_list()) && (g_vogl_entrypoint_descs[entrypoint_id].m_is_listable))
    {
        if (!vogl_display_list_state::is_call_listable(entrypoint_id, trace_packet))
        {
            if (!g_vogl_entrypoint_descs[entrypoint_id].m_whitelisted_for_displaylists)
                process_entrypoint_error("Failed serializing trace packet into display list shadow! Call is not whitelisted for display list usage by vogl.\n");
            else
                process_entrypoint_warning("Failed serializing trace packet into display list shadow! Call with these parameters is not listable.\n");
        }
        else
        {
            if (!get_shared_state()->m_shadow_state.m_display_lists.add_packet_to_list(get_context_state()->m_current_display_list_handle, entrypoint_id, trace_packet))
            {
                process_entrypoint_warning("Failed adding current packet to display list shadow!\n");
            }
        }
    }

    switch (entrypoint_id)
    {
// ----- Create simple auto-generated replay funcs - voglgen creates this inc file from the funcs in gl_glx_simple_replay_funcs.txt
// These simple GL entrypoints only take value params that don't require handle remapping, or simple pointers to client memory
// (typically pointers to fixed size buffers, or params directly controlling the size of buffers).
#define VOGL_SIMPLE_REPLAY_FUNC_BEGIN(name, num_params) \
    case VOGL_ENTRYPOINT_##name:                        \
    { if (!GL_ENTRYPOINT(name)) { process_entrypoint_error("vogl_gl_replayer::process_gl_entrypoint_packet_internal: Can't call NULL GL entrypoint %s (maybe a missing extension?)\n", #name); } else \
    GL_ENTRYPOINT(name)(
#define VOGL_SIMPLE_REPLAY_FUNC_PARAM_VALUE(type, index) trace_packet.get_param_value<type>(index)
#define VOGL_SIMPLE_REPLAY_FUNC_PARAM_SEPERATOR ,
#define VOGL_SIMPLE_REPLAY_FUNC_PARAM_CLIENT_MEMORY(type, index) trace_packet.get_param_client_memory<type>(index)
#define VOGL_SIMPLE_REPLAY_FUNC_END(name) ); \
    break;                                  \
    }
#include "gl_glx_cgl_wgl_simple_replay_funcs.inc"
#undef VOGL_SIMPLE_REPLAY_FUNC_BEGIN
#undef VOGL_SIMPLE_REPLAY_FUNC_PARAM_VALUE
#undef VOGL_SIMPLE_REPLAY_FUNC_PARAM_SEPERATOR
#undef VOGL_SIMPLE_REPLAY_FUNC_PARAM_CLIENT_MEMORY
#undef VOGL_SIMPLE_REPLAY_FUNC_PARAM_END
        // -----
        case VOGL_ENTRYPOINT_glXUseXFont:
        {
            #if VOGL_PLATFORM_HAS_SDL
                // TODO: Rely on GLX for now. Support SDL if possible.
            #elif VOGL_PLATFORM_HAS_GLX
                const key_value_map &key_value_map = trace_packet.get_key_value_map();

                const dynamic_string *pFont_name = key_value_map.get_string_ptr("font_name");
                if ((!pFont_name) || (pFont_name->is_empty()))
                {
                    process_entrypoint_warning("Couldn't find font_name key, or key was empty - unable to call glXUseXFont()!\n");
                }
                else
                {
                    XFontStruct *pFont = XLoadQueryFont(m_pWindow->get_display(), pFont_name->get_ptr());
                    if (!pFont)
                    {
                        process_entrypoint_warning("Couldn't load X font %s  - unable to call glXUseXFont()!\n", pFont_name->get_ptr());
                    }
                    else
                    {
                        GLint first = trace_packet.get_param_value<int>(1);
                        GLint count = trace_packet.get_param_value<int>(2);
                        int trace_list_base = trace_packet.get_param_value<int>(3);
                        GLuint replay_list_base = map_handle(get_shared_state()->m_lists, trace_list_base);

                        GL_ENTRYPOINT(glXUseXFont)(pFont->fid, first, count, replay_list_base);

                        XFreeFont(m_pWindow->get_display(), pFont);

                        if (get_context_state()->is_composing_display_list())
                        {
                            process_entrypoint_warning("glXUseXFont() called while composing a display list!\n");
                        }
                        else
                        {
                            if (!get_shared_state()->m_shadow_state.m_display_lists.glx_font(pFont_name->get_ptr(), first, count, trace_list_base))
                            {
                                process_entrypoint_warning("Failed updating display list shadow\n");
                            }
                        }
                    }
                }
            #else
                vogl_warning_printf("impl - VOGL_ENTRYPOINT_glXUseXFont\n");
            #endif

            break;
        }
        case VOGL_ENTRYPOINT_glBlitFramebufferEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBlitFramebufferEXT;

            VOGL_REPLAY_CALL_GL_HELPER_glBlitFramebufferEXT;

            if ((status = post_draw_call()) != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_glBlitFramebuffer:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBlitFramebuffer;

            VOGL_REPLAY_CALL_GL_HELPER_glBlitFramebuffer;

            if ((status = post_draw_call()) != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_glBegin:
        {
            if (m_pCur_context_state->m_inside_gl_begin)
            {
                process_entrypoint_warning("Got a glBegin while already inside a glBegin\n");
            }
            m_pCur_context_state->m_inside_gl_begin = true;

            g_vogl_actual_gl_entrypoints.m_glBegin(trace_packet.get_param_value<GLenum>(0));

            break;
        }
        case VOGL_ENTRYPOINT_glEnd:
        {
            if (!m_pCur_context_state->m_inside_gl_begin)
            {
                process_entrypoint_warning("Got glEnd without a matching glBegin\n");
            }
            m_pCur_context_state->m_inside_gl_begin = false;

            g_vogl_actual_gl_entrypoints.m_glEnd();

            if ((status = post_draw_call()) != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_glGetError:
        {
            // TODO: Compare trace error vs. replay error

            break;
        }
        case VOGL_ENTRYPOINT_glGetStringi:
        {
            if (!benchmark_mode())
            {
                const GLubyte *pStr = GL_ENTRYPOINT(glGetStringi)(
                    trace_packet.get_param_value<GLenum>(0),
                    trace_packet.get_param_value<GLuint>(1));
                VOGL_NOTE_UNUSED(pStr);

                // TODO: Compare vs. trace's?
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetString:
        {
            if (!benchmark_mode())
            {
                const GLubyte *pStr = GL_ENTRYPOINT(glGetString)(
                    trace_packet.get_param_value<GLenum>(0));
                VOGL_NOTE_UNUSED(pStr);

                // TODO: Compare vs. trace's?
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGenFramebuffers:
        case VOGL_ENTRYPOINT_glGenFramebuffersEXT:
        {
            if (!gen_handles(get_context_state()->m_framebuffers,
                             trace_packet.get_param_value<GLsizei>(0),
                             static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)),
                             (entrypoint_id == VOGL_ENTRYPOINT_glGenFramebuffers) ? GL_ENTRYPOINT(glGenFramebuffers) : GL_ENTRYPOINT(glGenFramebuffersEXT), NULL))
                return cStatusHardFailure;

            break;
        }
        case VOGL_ENTRYPOINT_glBindFramebuffer:
        case VOGL_ENTRYPOINT_glBindFramebufferEXT:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(1);

            GLuint replay_handle = map_handle(get_context_state()->m_framebuffers, trace_handle);

            SWITCH_GL_ENTRYPOINT2_VOID(glBindFramebuffer, glBindFramebufferEXT, target, replay_handle);

            break;
        }
        case VOGL_ENTRYPOINT_glGetRenderbufferParameterivEXT:
        case VOGL_ENTRYPOINT_glGetRenderbufferParameteriv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                GLint *pTrace_params = trace_packet.get_param_client_memory<GLint>(2);
                uint32_t trace_params_size = trace_packet.get_param_client_memory_data_size(2);
                uint32_t trace_params_count = trace_params_size / sizeof(GLint);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }
                else if (n < static_cast<int>(trace_params_count))
                {
                    process_entrypoint_error("Expected %i GLint's for GL pname 0x%08X, but trace only contains %i GLint's\n", n, pname, trace_params_count);
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLint, 16> params(n + 1);
                    params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                    SWITCH_GL_ENTRYPOINT2_VOID(glGetRenderbufferParameteriv, glGetRenderbufferParameterivEXT, target, pname, params.get_ptr());

                    VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                    if (memcmp(pTrace_params, params.get_ptr(), n * sizeof(GLint)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLint data differed from trace's, pname %s target %s\n", get_gl_enums().find_gl_name(pname), get_gl_enums().find_gl_name(target));
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glCheckFramebufferStatus:
        case VOGL_ENTRYPOINT_glCheckFramebufferStatusEXT:
        {
            GLenum result;
            SWITCH_GL_ENTRYPOINT2(glCheckFramebufferStatus, glCheckFramebufferStatusEXT, trace_packet.get_param_value<GLenum>(0));

            GLenum trace_status = trace_packet.get_return_value<GLenum>();
            if (result != trace_status)
            {
                process_entrypoint_warning("glCheckFramebufferStatus returned status 0x%08X during trace, but status 0x%08X during replay\n", trace_status, result);
            }
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteFramebuffers:
        {
            delete_handles(get_context_state()->m_framebuffers, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteFramebuffers));
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteFramebuffersEXT:
        {
            delete_handles(get_context_state()->m_framebuffers, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteFramebuffersEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glFramebufferTexture:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLenum attachment = trace_packet.get_param_value<GLenum>(1);
            GLuint trace_texture = trace_packet.get_param_value<GLuint>(2);
            GLuint replay_texture = trace_texture;
            GLint level = trace_packet.get_param_value<GLint>(3);

            if (!get_shared_state()->m_shadow_state.m_textures.map_handle_to_inv_handle(trace_texture, replay_texture))
            {
                if (trace_texture)
                    process_entrypoint_warning("Couldn't map trace texture ID %u to GL texture ID, using trace texture ID instead\n", trace_texture);
            }

            GL_ENTRYPOINT(glFramebufferTexture)(target, attachment, replay_texture, level);

            break;
        }
        case VOGL_ENTRYPOINT_glFramebufferTextureLayer:
        case VOGL_ENTRYPOINT_glFramebufferTextureLayerEXT:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLenum attachment = trace_packet.get_param_value<GLenum>(1);
            GLuint trace_texture = trace_packet.get_param_value<GLuint>(2);
            GLuint replay_texture = trace_texture;
            GLint level = trace_packet.get_param_value<GLint>(3);
            GLint layer = trace_packet.get_param_value<GLint>(4);

            if (!get_shared_state()->m_shadow_state.m_textures.map_handle_to_inv_handle(trace_texture, replay_texture))
            {
                if (trace_texture)
                    process_entrypoint_warning("Couldn't map trace texture ID %u to GL texture ID, using trace texture ID instead\n", trace_texture);
            }

            SWITCH_GL_ENTRYPOINT2_VOID(glFramebufferTextureLayer, glFramebufferTextureLayerEXT, target, attachment, replay_texture, level, layer);

            break;
        }
        case VOGL_ENTRYPOINT_glFramebufferTexture1DEXT:
        case VOGL_ENTRYPOINT_glFramebufferTexture1D:
        case VOGL_ENTRYPOINT_glFramebufferTexture2DEXT:
        case VOGL_ENTRYPOINT_glFramebufferTexture2D:
        case VOGL_ENTRYPOINT_glFramebufferTexture3DEXT:
        case VOGL_ENTRYPOINT_glFramebufferTexture3D:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLenum attachment = trace_packet.get_param_value<GLenum>(1);
            GLenum textarget = trace_packet.get_param_value<GLenum>(2);
            GLuint trace_texture = trace_packet.get_param_value<GLuint>(3);
            GLuint replay_texture = trace_texture;
            GLint level = trace_packet.get_param_value<GLint>(4);

            if (!get_shared_state()->m_shadow_state.m_textures.map_handle_to_inv_handle(trace_texture, replay_texture))
            {
                if (trace_texture)
                    process_entrypoint_warning("Couldn't map trace texture ID %u to GL texture ID, using trace texture ID instead\n", trace_texture);
            }

            switch (entrypoint_id)
            {
                case VOGL_ENTRYPOINT_glFramebufferTexture1DEXT:
                    GL_ENTRYPOINT(glFramebufferTexture1DEXT)(target, attachment, textarget, replay_texture, level);
                    break;
                case VOGL_ENTRYPOINT_glFramebufferTexture1D:
                    GL_ENTRYPOINT(glFramebufferTexture1D)(target, attachment, textarget, replay_texture, level);
                    break;
                case VOGL_ENTRYPOINT_glFramebufferTexture2DEXT:
                    GL_ENTRYPOINT(glFramebufferTexture2DEXT)(target, attachment, textarget, replay_texture, level);
                    break;
                case VOGL_ENTRYPOINT_glFramebufferTexture2D:
                    GL_ENTRYPOINT(glFramebufferTexture2D)(target, attachment, textarget, replay_texture, level);
                    break;
                case VOGL_ENTRYPOINT_glFramebufferTexture3DEXT:
                    GL_ENTRYPOINT(glFramebufferTexture3DEXT)(target, attachment, textarget, replay_texture, level, trace_packet.get_param_value<GLint>(5));
                    break;
                case VOGL_ENTRYPOINT_glFramebufferTexture3D:
                    GL_ENTRYPOINT(glFramebufferTexture3D)(target, attachment, textarget, replay_texture, level, trace_packet.get_param_value<GLint>(5));
                    break;
                default:
                    break;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGenTextures:
        {
            if (!gen_handles(get_shared_state()->m_shadow_state.m_textures, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenTextures), NULL, GL_NONE))
                return cStatusHardFailure;
            break;
        }
        case VOGL_ENTRYPOINT_glGenTexturesEXT:
        {
            if (!gen_handles(get_shared_state()->m_shadow_state.m_textures, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenTexturesEXT), NULL, GL_NONE))
                return cStatusHardFailure;
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteTextures:
        {
            delete_handles(get_shared_state()->m_shadow_state.m_textures, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteTextures));
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteTexturesEXT:
        {
            delete_handles(get_shared_state()->m_shadow_state.m_textures, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteTexturesEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glBindMultiTextureEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindMultiTextureEXT;

            GLuint trace_texture = texture;
            map_handle(get_shared_state()->m_shadow_state.m_textures, trace_texture, texture);

            if ((texture) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
                check_gl_error();

            VOGL_REPLAY_CALL_GL_HELPER_glBindMultiTextureEXT;

            if ((texture) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                if (!check_gl_error())
                    get_shared_state()->m_shadow_state.m_textures.update(trace_texture, texture, target);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBindTexture:
        case VOGL_ENTRYPOINT_glBindTextureEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindTexture;

            GLuint trace_texture = texture;
            map_handle(get_shared_state()->m_shadow_state.m_textures, trace_texture, texture);

            if ((texture) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
                check_gl_error();

            SWITCH_GL_ENTRYPOINT2_VOID(glBindTexture, glBindTextureEXT, target, texture);

            if ((texture) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                if (!check_gl_error())
                    get_shared_state()->m_shadow_state.m_textures.update(trace_texture, texture, target);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBindSampler:
        {
            GLuint replay_handle = map_handle(get_shared_state()->m_sampler_objects, trace_packet.get_param_value<GLuint>(1));
            GL_ENTRYPOINT(glBindSampler)(trace_packet.get_param_value<GLuint>(0), replay_handle);
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteSamplers:
        {
            delete_handles(get_shared_state()->m_sampler_objects, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteSamplers));
            break;
        }
        case VOGL_ENTRYPOINT_glGenSamplers:
        {
            if (!gen_handles(get_shared_state()->m_sampler_objects, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenSamplers), NULL))
                return cStatusHardFailure;
            break;
        }

        case VOGL_ENTRYPOINT_glSamplerParameterf:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glSamplerParameterf;
            sampler = map_handle(get_shared_state()->m_sampler_objects, sampler);
            VOGL_REPLAY_CALL_GL_HELPER_glSamplerParameterf;
            break;
        }
        case VOGL_ENTRYPOINT_glSamplerParameteri:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glSamplerParameteri;
            sampler = map_handle(get_shared_state()->m_sampler_objects, sampler);
            VOGL_REPLAY_CALL_GL_HELPER_glSamplerParameteri;
            break;
        }
        case VOGL_ENTRYPOINT_glSamplerParameterfv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glSamplerParameterfv;
            sampler = map_handle(get_shared_state()->m_sampler_objects, sampler);
            VOGL_REPLAY_CALL_GL_HELPER_glSamplerParameterfv;
            break;
        }
        case VOGL_ENTRYPOINT_glSamplerParameteriv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glSamplerParameteriv;
            sampler = map_handle(get_shared_state()->m_sampler_objects, sampler);
            VOGL_REPLAY_CALL_GL_HELPER_glSamplerParameteriv;
            break;
        }
        case VOGL_ENTRYPOINT_glSamplerParameterIiv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glSamplerParameterIiv;
            sampler = map_handle(get_shared_state()->m_sampler_objects, sampler);
            VOGL_REPLAY_CALL_GL_HELPER_glSamplerParameterIiv;
            break;
        }
        case VOGL_ENTRYPOINT_glSamplerParameterIuiv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glSamplerParameterIuiv;
            sampler = map_handle(get_shared_state()->m_sampler_objects, sampler);
            VOGL_REPLAY_CALL_GL_HELPER_glSamplerParameterIuiv;
            break;
        }
        case VOGL_ENTRYPOINT_glGenBuffers:
        case VOGL_ENTRYPOINT_glGenBuffersARB:
        {
            uint32_t n = trace_packet.get_param_value<GLsizei>(0);
            const GLuint *pTrace_handles = static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1));

            if (entrypoint_id == VOGL_ENTRYPOINT_glGenBuffers)
            {
                if (!gen_handles(get_shared_state()->m_buffers, n, pTrace_handles, GL_ENTRYPOINT(glGenBuffers), NULL))
                    return cStatusHardFailure;
            }
            else
            {
                if (!gen_handles(get_shared_state()->m_buffers, n, pTrace_handles, GL_ENTRYPOINT(glGenBuffersARB), NULL))
                    return cStatusHardFailure;
            }

            if (pTrace_handles)
            {
                for (uint32_t i = 0; i < n; i++)
                {
                    if (pTrace_handles[i])
                        get_shared_state()->m_buffer_targets.insert(pTrace_handles[i], GL_NONE);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteBuffers:
        case VOGL_ENTRYPOINT_glDeleteBuffersARB:
        {
            GLsizei trace_n = trace_packet.get_param_value<GLsizei>(0);
            const GLuint *pTrace_ids = static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1));
            uint32_t trace_ids_count = trace_packet.get_param_client_memory_data_size(1);

            if ((pTrace_ids) && (static_cast<GLsizei>(trace_ids_count) < trace_n))
            {
                process_entrypoint_warning("trace_ids trace array is too small\n");
                return cStatusHardFailure;
            }

            for (GLsizei iter = 0; iter < trace_n; iter++)
            {
                GLuint trace_id = pTrace_ids[iter];
                if (!trace_id)
                    continue;

                if (!get_shared_state()->m_buffer_targets.erase(trace_id))
                {
                    process_entrypoint_warning("Couldn't find trace buffer id %u in buffer target map!\n", trace_id);
                }

                gl_handle_hash_map::const_iterator it = get_shared_state()->m_buffers.find(trace_id);
                if (it == get_shared_state()->m_buffers.end())
                {
                    process_entrypoint_warning("Couldn't map trace buffer id %u to GL buffer id\n", trace_id);
                    continue;
                }

                GLuint replay_id = it->second;

                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                for (uint32_t i = 0; i < mapped_bufs.size(); i++)
                {
                    if (mapped_bufs[i].m_buffer == replay_id)
                    {
                        process_entrypoint_warning("glDeleteBuffers() called on mapped trace buffer %u GL buffer %u\n", trace_id, replay_id);

                        mapped_bufs.erase_unordered(i);
                        break;
                    }
                }
            }

            if (entrypoint_id == VOGL_ENTRYPOINT_glDeleteBuffers)
                delete_handles(get_shared_state()->m_buffers, trace_n, pTrace_ids, GL_ENTRYPOINT(glDeleteBuffers));
            else
                delete_handles(get_shared_state()->m_buffers, trace_n, pTrace_ids, GL_ENTRYPOINT(glDeleteBuffersARB));

            break;
        }
        case VOGL_ENTRYPOINT_glGenProgramsARB:
        {
            // arb program objects
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glGenProgramsARB;

            if (!gen_handles(get_shared_state()->m_arb_programs, n, pTrace_programs, GL_ENTRYPOINT(glGenProgramsARB), NULL))
                return cStatusHardFailure;

            for (GLsizei i = 0; (pTrace_programs) && (i < n); i++)
                if (pTrace_programs[i])
                    get_shared_state()->m_arb_program_targets.insert(pTrace_programs[i], GL_NONE);

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteProgramsARB:
        {
            // arb program objects
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glDeleteProgramsARB;

            for (GLsizei i = 0; (pTrace_programs) && (i < n); i++)
                get_shared_state()->m_arb_program_targets.erase(pTrace_programs[i]);

            delete_handles(get_shared_state()->m_arb_programs, n, pTrace_programs, GL_ENTRYPOINT(glDeleteProgramsARB));
            break;
        }
        case VOGL_ENTRYPOINT_glBindProgramARB:
        {
            // arb program objects
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindProgramARB;

            GLuint trace_program = program;
            gl_handle_hash_map::const_iterator it;
            if (program)
            {
                it = get_shared_state()->m_arb_programs.find(program);
                if (it != get_shared_state()->m_arb_programs.end())
                    program = it->second;
                else
                    process_entrypoint_warning("Couldn't map trace handle %u to GL handle, using trace handle instead (handle may not have been genned)\n", program);
            }

            check_gl_error();

            VOGL_REPLAY_CALL_GL_HELPER_glBindProgramARB;

            if (!check_gl_error() && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                if (trace_program)
                {
                    if (it == get_shared_state()->m_arb_programs.end())
                        get_shared_state()->m_arb_programs.insert(trace_program, program);

                    get_shared_state()->m_arb_program_targets[trace_program] = target;
                }
            }
            else
            {
                process_entrypoint_warning("GL error while binding ARB program, trace handle %u GL handle %u target %s\n", trace_program, program, get_gl_enums().find_gl_name(target));
                return cStatusGLError;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glIsProgramARB:
        {
            if (!benchmark_mode())
            {
                VOGL_REPLAY_LOAD_PARAMS_HELPER_glIsProgramARB;

                GLuint trace_program = program;
                program = map_handle(get_shared_state()->m_arb_programs, program);

                GLboolean replay_result = VOGL_REPLAY_CALL_GL_HELPER_glIsProgramARB;
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u, trace ARB program: %u replay ARB program %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result), trace_program, program);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGenProgramPipelines:
        {
            if (!gen_handles(get_shared_state()->m_shadow_state.m_program_pipelines, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenProgramPipelines), NULL, GL_NONE))
                return cStatusHardFailure;
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteProgramPipelines:
        {
            delete_handles(get_shared_state()->m_shadow_state.m_program_pipelines, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteProgramPipelines));
            break;
        }
        case VOGL_ENTRYPOINT_glIsProgramPipeline:
        {
            if (!benchmark_mode())
            {
                GLboolean replay_result = GL_ENTRYPOINT(glIsProgramPipeline)(map_handle(get_shared_state()->m_shadow_state.m_program_pipelines, trace_packet.get_param_value<GLuint>(0)));
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();
                if (replay_result != trace_result)
                    process_entrypoint_warning("Replay's Program Pipeline returned GLboolean data differed from trace's (got %i, expected %i)\n", replay_result, trace_result);
            }
            break;
        }
        case VOGL_ENTRYPOINT_glBindProgramPipeline:
        {
            GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_program_pipelines, trace_packet.get_param_value<GLuint>(0));
            GL_ENTRYPOINT(glBindProgramPipeline)(replay_handle);
            break;
        }
        case VOGL_ENTRYPOINT_glUseProgramStages:
        {
            // TODO: Verify this is correct. Pipeline obj should come from pipeline state mapping and Prog obj from obj mappings
            GL_ENTRYPOINT(glUseProgramStages)(
                map_handle(get_shared_state()->m_shadow_state.m_program_pipelines, trace_packet.get_param_value<GLuint>(0)),
                trace_packet.get_param_value<GLenum>(1),
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLuint>(2)));
            break;
        }
        case VOGL_ENTRYPOINT_glGenQueries:
        case VOGL_ENTRYPOINT_glGenQueriesARB:
        {
            GLsizei n = trace_packet.get_param_value<GLsizei>(0);
            vogl::growable_array<GLuint, 16> replay_handles(n);

            if (!gen_handles(get_shared_state()->m_queries, n, static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), (entrypoint_id == VOGL_ENTRYPOINT_glGenQueries) ? GL_ENTRYPOINT(glGenQueries) : GL_ENTRYPOINT(glGenQueriesARB), replay_handles.get_ptr()))
                return cStatusHardFailure;

            for (GLsizei i = 0; i < n; i++)
                get_shared_state()->m_query_targets[replay_handles[i]] = GL_NONE;

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteQueries:
        case VOGL_ENTRYPOINT_glDeleteQueriesARB:
        {
            GLsizei n = trace_packet.get_param_value<GLsizei>(0);
            const GLuint *pTrace_ids = static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1));

            if (pTrace_ids)
            {
                for (GLsizei i = 0; i < n; i++)
                {
                    GLuint trace_id = pTrace_ids[i];
                    if (!trace_id)
                        continue;
                    gl_handle_hash_map::const_iterator it(get_shared_state()->m_queries.find(trace_id));
                    if (it != get_shared_state()->m_queries.end())
                        get_shared_state()->m_query_targets.erase(it->second);
                }
            }

            if (entrypoint_id == VOGL_ENTRYPOINT_glDeleteQueries)
                delete_handles(get_shared_state()->m_queries, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteQueries));
            else
                delete_handles(get_shared_state()->m_queries, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteQueriesARB));

            break;
        }
        case VOGL_ENTRYPOINT_glGenRenderbuffersEXT:
        {
            if (!gen_handles(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenRenderbuffersEXT), NULL, GL_NONE))
                return cStatusHardFailure;
            break;
        }
        case VOGL_ENTRYPOINT_glGenRenderbuffers:
        {
            if (!gen_handles(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenRenderbuffers), NULL, GL_NONE))
                return cStatusHardFailure;
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteRenderbuffersEXT:
        {
            delete_handles(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteRenderbuffersEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteRenderbuffers:
        {
            delete_handles(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteRenderbuffers));
            break;
        }
        case VOGL_ENTRYPOINT_glIsRenderbuffer:
        {
            if (!benchmark_mode())
            {
                GLboolean replay_result = GL_ENTRYPOINT(glIsRenderbuffer)(map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLuint>(0)));
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();
                if (replay_result != trace_result)
                    process_entrypoint_warning("Replay's Renderbuffer returned GLboolean data differed from trace's (got %i, expected %i)\n", replay_result, trace_result);
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsRenderbufferEXT:
        {
            if (!benchmark_mode())
            {
                GLboolean replay_result = GL_ENTRYPOINT(glIsRenderbufferEXT)(map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLuint>(0)));
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();
                if (replay_result != trace_result)
                {
                    process_entrypoint_warning("Replay's Renderbuffer returned GLboolean data differed from trace's (got %i, expected %i)\n", replay_result, replay_result);
                }
            }
            break;
        }
        case VOGL_ENTRYPOINT_glBindRenderbufferEXT:
        {
            GL_ENTRYPOINT(glBindRenderbufferEXT)(trace_packet.get_param_value<GLenum>(0), map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLuint>(1)));
            break;
        }
        case VOGL_ENTRYPOINT_glBindRenderbuffer:
        {
            GL_ENTRYPOINT(glBindRenderbuffer)(trace_packet.get_param_value<GLenum>(0), map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLuint>(1)));
            break;
        }
        case VOGL_ENTRYPOINT_glFramebufferRenderbufferEXT:
        {
            GL_ENTRYPOINT(glFramebufferRenderbufferEXT)(
                trace_packet.get_param_value<GLenum>(0),
                trace_packet.get_param_value<GLenum>(1),
                trace_packet.get_param_value<GLenum>(2),
                map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLuint>(3)));
            break;
        }
        case VOGL_ENTRYPOINT_glFramebufferRenderbuffer:
        {
            GL_ENTRYPOINT(glFramebufferRenderbuffer)(
                trace_packet.get_param_value<GLenum>(0),
                trace_packet.get_param_value<GLenum>(1),
                trace_packet.get_param_value<GLenum>(2),
                map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_packet.get_param_value<GLuint>(3)));
            break;
        }
        case VOGL_ENTRYPOINT_glUseProgramObjectARB:
        case VOGL_ENTRYPOINT_glUseProgram:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            handle_use_program(trace_handle, entrypoint_id);
            break;
        }
        case VOGL_ENTRYPOINT_glProgramParameteri:
        case VOGL_ENTRYPOINT_glProgramParameteriARB:
        case VOGL_ENTRYPOINT_glProgramParameteriEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glProgramParameteri;

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);

            if (entrypoint_id == VOGL_ENTRYPOINT_glProgramParameteriARB)
                VOGL_REPLAY_CALL_GL_HELPER_glProgramParameteriARB;
            else if (entrypoint_id == VOGL_ENTRYPOINT_glProgramParameteriEXT)
                VOGL_REPLAY_CALL_GL_HELPER_glProgramParameteriEXT;
            else
                VOGL_REPLAY_CALL_GL_HELPER_glProgramParameteri;

            break;
        }
        case VOGL_ENTRYPOINT_glBindFragDataLocation:
        case VOGL_ENTRYPOINT_glBindFragDataLocationEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindFragDataLocation;

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);

            if (entrypoint_id == VOGL_ENTRYPOINT_glBindFragDataLocation)
                VOGL_REPLAY_CALL_GL_HELPER_glBindFragDataLocation;
            else
                VOGL_REPLAY_CALL_GL_HELPER_glBindFragDataLocationEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glBindFragDataLocationIndexed:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindFragDataLocationIndexed;

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);

            VOGL_REPLAY_CALL_GL_HELPER_glBindFragDataLocationIndexed;

            break;
        }
        case VOGL_ENTRYPOINT_glValidateProgramARB:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glValidateProgramARB;

            programObj = map_handle(get_shared_state()->m_shadow_state.m_objs, programObj);

            VOGL_REPLAY_CALL_GL_HELPER_glValidateProgramARB;

            break;
        }
        case VOGL_ENTRYPOINT_glValidateProgram:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glValidateProgram;

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);

            VOGL_REPLAY_CALL_GL_HELPER_glValidateProgram;

            break;
        }
        case VOGL_ENTRYPOINT_glCreateProgram:
        case VOGL_ENTRYPOINT_glCreateProgramObjectARB:
        {
            GLuint trace_handle = trace_packet.get_return_value<GLuint>();
            if (trace_handle)
            {
                GLuint replay_handle;

                if (entrypoint_id == VOGL_ENTRYPOINT_glCreateProgram)
                    replay_handle = GL_ENTRYPOINT(glCreateProgram)();
                else
                    replay_handle = GL_ENTRYPOINT(glCreateProgramObjectARB)();

                VOGL_ASSERT(!replay_handle || (GL_ENTRYPOINT(glIsProgram)(replay_handle) != 0));

                if (!gen_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle, replay_handle, VOGL_PROGRAM_OBJECT))
                    return cStatusHardFailure;
            }
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteProgram:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            handle_delete_program(trace_handle);

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteObjectARB:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLenum>(0);
            GLenum target = get_shared_state()->m_shadow_state.m_objs.get_target(trace_handle);

            if (target == VOGL_SHADER_OBJECT)
                handle_delete_shader(trace_handle);
            else if (target == VOGL_PROGRAM_OBJECT)
                handle_delete_program(trace_handle);
            else
            {
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

                process_entrypoint_error("Failed determining if trace handle %u relay handle %u is a shader or program -- unable to delete object!\n", trace_handle, replay_handle);
                return cStatusSoftFailure;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteShader:
        {
            GLuint trace_shader = trace_packet.get_param_value<GLuint>(0);
            handle_delete_shader(trace_shader);

            break;
        }
        case VOGL_ENTRYPOINT_glCreateShader:
        case VOGL_ENTRYPOINT_glCreateShaderObjectARB:
        {
            GLuint trace_handle = trace_packet.get_return_value<GLuint>();
            if (trace_handle)
            {
                GLuint replay_handle;

                if (entrypoint_id == VOGL_ENTRYPOINT_glCreateShader)
                    replay_handle = GL_ENTRYPOINT(glCreateShader)(trace_packet.get_param_value<GLenum>(0));
                else
                    replay_handle = GL_ENTRYPOINT(glCreateShaderObjectARB)(trace_packet.get_param_value<GLenum>(0));

                VOGL_ASSERT(!replay_handle || (GL_ENTRYPOINT(glIsShader)(replay_handle) != 0));

                if (!gen_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle, replay_handle, VOGL_SHADER_OBJECT))
                    return cStatusHardFailure;
            }
            break;
        }
        case VOGL_ENTRYPOINT_glAttachShader:
        {
            GL_ENTRYPOINT(glAttachShader)(
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLuint>(0)),
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLuint>(1)));
            break;
        }
        case VOGL_ENTRYPOINT_glAttachObjectARB:
        {
            GL_ENTRYPOINT(glAttachObjectARB)(
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLhandleARB>(0)),
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLhandleARB>(1)));
            break;
        }
        case VOGL_ENTRYPOINT_glDetachShader:
        {
            handle_detach_shader(entrypoint_id);

            break;
        }
        case VOGL_ENTRYPOINT_glDetachObjectARB:
        {
            GLhandleARB trace_object_handle = trace_packet.get_param_value<GLhandleARB>(1);

            GLenum target = get_shared_state()->m_shadow_state.m_objs.get_target(trace_object_handle);

            if (target == VOGL_SHADER_OBJECT)
                handle_detach_shader(entrypoint_id);
            else
            {
                GLuint replay_object_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_object_handle);
                GL_ENTRYPOINT(glDetachObjectARB)(map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLhandleARB>(0)), replay_object_handle);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBindAttribLocation:
        {
            GL_ENTRYPOINT(glBindAttribLocation)(
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLuint>(0)),
                trace_packet.get_param_value<GLuint>(1),
                trace_packet.get_param_client_memory<GLchar>(2));
            break;
        }
        case VOGL_ENTRYPOINT_glBindAttribLocationARB:
        {
            GL_ENTRYPOINT(glBindAttribLocationARB)(
                map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLhandleARB>(0)),
                trace_packet.get_param_value<GLuint>(1),
                trace_packet.get_param_client_memory<GLcharARB>(2));
            break;
        }
        case VOGL_ENTRYPOINT_glGetObjectParameterivARB:
        {
            if (!benchmark_mode())
            {
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLint *pParams = trace_packet.get_param_client_memory<GLint>(2);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLint, 16> params(n + 1);
                    params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                    GL_ENTRYPOINT(glGetObjectParameterivARB)(
                        map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLhandleARB>(0)),
                        pname,
                        params.get_ptr());

                    VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                    if (memcmp(pParams, params.get_ptr(), n * sizeof(GLint)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLint data differed from trace's, pname %s\n", get_gl_enums().find_gl_name(pname));
                    }
                }
            }
            break;
        }
        case VOGL_ENTRYPOINT_glGetBufferParameteriv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum value = trace_packet.get_param_value<GLenum>(1);
                const GLint *pTrace_data = trace_packet.get_param_client_memory<GLint>(2);

                int n = get_gl_enums().get_pname_count(value);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL value 0x%08X\n", value);
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLint, 16> data(n + 1);
                    data[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                    GL_ENTRYPOINT(glGetBufferParameteriv)(target, value, data.get_ptr());

                    VOGL_VERIFY(data[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                    GLint trace_data = pTrace_data ? pTrace_data[0] : -1;
                    if (data[0] != trace_data)
                    {
                        process_entrypoint_warning("Replay's returned GLint differed from trace's!\n");
                        vogl_warning_printf("Trace data: %i, Replay data: %i\n", trace_data, data[0]);
                    }
                }
            }

            break;
        }

        case VOGL_ENTRYPOINT_glGetBufferPointerv:
        {
            if (!benchmark_mode())
            {
                GLvoid *pReplay_ptr = NULL;
                GL_ENTRYPOINT(glGetBufferPointerv)(trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLenum>(1), &pReplay_ptr);

                vogl_client_memory_array trace_void_ptr_array = trace_packet.get_param_client_memory_array(2);
                vogl_trace_ptr_value first_trace_ptr = trace_void_ptr_array.get_ptr() ? trace_void_ptr_array.get_element<vogl_trace_ptr_value>(0) : 0;

                if ((pReplay_ptr != NULL) != (first_trace_ptr != 0))
                {
                    process_entrypoint_warning("First replay's returned GLvoid* differed from trace's!\n");
                    vogl_warning_printf("Trace: 0x%" PRIx64 ", Replay: 0x%" PRIx64 "\n", first_trace_ptr, reinterpret_cast<uint64_t>(pReplay_ptr));
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glShaderSource:
        case VOGL_ENTRYPOINT_glShaderSourceARB:
        {
            status = handle_ShaderSource(trace_packet.get_param_value<GLhandleARB>(0),
                                         trace_packet.get_param_value<GLsizei>(1),
                                         trace_packet.get_param_client_memory_array(2),
                                         trace_packet.get_param_client_memory<const GLint>(3));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetProgramInfoLog:
        {
            GLuint trace_object = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_object = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_object);

            GLint length = -1;
            GL_ENTRYPOINT(glGetProgramiv)(replay_object, GL_INFO_LOG_LENGTH, &length);
            if (length < 0)
            {
                process_entrypoint_error("Failed retrieving info log length for trace object %u, reply object %u\n", trace_object, replay_object);
                return cStatusSoftFailure;
            }
            else
            {
                vogl::vector<GLchar> log(length);

                GLsizei actual_length = 0;
                GL_ENTRYPOINT(glGetProgramInfoLog)(replay_object, length, &actual_length, log.get_ptr());

                if (actual_length)
                {
                    process_entrypoint_message("Info log for trace object %u, replay object %u:\n%s\n", trace_object, replay_object, log.get_ptr());
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetPointerv:
        {
            if (!benchmark_mode())
            {
                GLvoid *ptr = NULL;
                GL_ENTRYPOINT(glGetPointerv)(trace_packet.get_param_value<GLenum>(0), &ptr);

                // TODO: Differ vs. trace's in some way?
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetInfoLogARB:
        {
            GLhandleARB trace_object = trace_packet.get_param_value<GLhandleARB>(0);
            GLhandleARB replay_object = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_object);

            GLsizei length = -1;
            GL_ENTRYPOINT(glGetObjectParameterivARB)(replay_object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
            if (length < 0)
            {
                process_entrypoint_error("Failed retrieving info log length for trace object %u, reply object %u\n", trace_object, replay_object);
                return cStatusSoftFailure;
            }
            else
            {
                vogl::vector<GLcharARB> log(length);

                GLsizei actual_length = 0;
                GL_ENTRYPOINT(glGetInfoLogARB)(replay_object, length, &actual_length, log.get_ptr());

                if (actual_length)
                {
                    process_entrypoint_message("Info log for trace object %u, replay object %u:\n%s\n", trace_object, replay_object, log.get_ptr());
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetUniformLocation:
        {
            GLhandleARB trace_handle = trace_packet.get_param_value<GLhandleARB>(0);
            GLhandleARB replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
            GLint trace_loc = trace_packet.get_return_value<GLint>();

            if (replay_handle)
            {
                const GLchar *pName = trace_packet.get_param_client_memory<GLchar>(1);

                GLint replay_loc = GL_ENTRYPOINT(glGetUniformLocation)(replay_handle, pName);
                if (replay_loc < 0)
                {
                    if (trace_loc >= 0)
                        process_entrypoint_warning("glGetUniformLocation: Function succeeded during trace, but failed during replay! (name: %s trace_handle: %u, trace_loc: %i)\n", (const char *)pName, trace_handle, trace_loc);
                }
                else
                {
                    if (trace_loc < 0)
                        process_entrypoint_warning("glGetUniformLocation: Function failed during trace, but succeeded during replay! (name: %s trace_handle: %u, trace_loc: %i)\n", (const char *)pName, trace_handle, trace_loc);
                    else
                    {
                        glsl_program_hash_map::iterator it = get_shared_state()->m_glsl_program_hash_map.find(trace_handle);
                        if (it == get_shared_state()->m_glsl_program_hash_map.end())
                            it = get_shared_state()->m_glsl_program_hash_map.insert(trace_handle).first;

                        glsl_program_state &state = it->second;
                        state.m_uniform_locations.erase(trace_loc);
                        state.m_uniform_locations.insert(trace_loc, replay_loc);
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetUniformLocationARB:
        {
            GLhandleARB trace_handle = trace_packet.get_param_value<GLhandleARB>(0);
            GLhandleARB replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
            GLint trace_loc = trace_packet.get_return_value<GLint>();

            if (replay_handle)
            {
                const GLcharARB *pName = trace_packet.get_param_client_memory<GLcharARB>(1);

                GLint replay_loc = GL_ENTRYPOINT(glGetUniformLocationARB)(replay_handle, pName);
                if (replay_loc < 0)
                {
                    if (trace_loc >= 0)
                        process_entrypoint_warning("glGetUniformLocationARB: Function succeeded during trace, but failed during replay! (name: %s trace_handle: %u, trace_loc: %i)\n", (const char *)pName, trace_handle, trace_loc);
                }
                else
                {
                    if (trace_loc < 0)
                        process_entrypoint_warning("glGetUniformLocationARB: Function failed during trace, but succeeded during replay! (name: %s trace_handle: %u, trace_loc: %i)\n", (const char *)pName, trace_handle, trace_loc);
                    else
                    {
                        glsl_program_hash_map::iterator it = get_shared_state()->m_glsl_program_hash_map.find(trace_handle);
                        if (it == get_shared_state()->m_glsl_program_hash_map.end())
                            it = get_shared_state()->m_glsl_program_hash_map.insert(trace_handle).first;

                        glsl_program_state &state = it->second;
                        state.m_uniform_locations.erase(trace_loc);
                        state.m_uniform_locations.insert(trace_loc, replay_loc);
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetActiveAttrib:
        case VOGL_ENTRYPOINT_glGetActiveUniform:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

                GLuint index = trace_packet.get_param_value<GLuint>(1);
                GLsizei bufSize = trace_packet.get_param_value<GLsizei>(2);

                GLsizei *pTrace_length = trace_packet.get_param_client_memory<GLsizei>(3);
                GLint *pTrace_size = trace_packet.get_param_client_memory<GLint>(4);
                GLenum *pTrace_type = trace_packet.get_param_client_memory<GLenum>(5);
                GLchar *pTrace_name = trace_packet.get_param_client_memory<GLchar>(6);

                vogl::growable_array<GLchar, 1024> name_buf(bufSize + 1); // + 1 guarantees non-empty and null terminated

                GLsizei len = 0;
                GLint size = 0;
                GLenum type = 0;

                if (entrypoint_id == VOGL_ENTRYPOINT_glGetActiveAttrib)
                    GL_ENTRYPOINT(glGetActiveAttrib)(replay_handle, index, bufSize, &len, &size, &type, name_buf.get_ptr());
                else
                    GL_ENTRYPOINT(glGetActiveUniform)(replay_handle, index, bufSize, &len, &size, &type, name_buf.get_ptr());

                bool mismatch = false;

                GLsizei trace_len = 0;
                if (pTrace_length)
                {
                    trace_len = pTrace_length[0];
                    if (trace_len != len)
                        mismatch = true;
                }

                GLint trace_size = 0;
                if (pTrace_size)
                {
                    trace_size = pTrace_size[0];
                    if (trace_size != size)
                        mismatch = true;
                }

                GLenum trace_type = 0;
                if (pTrace_type)
                {
                    trace_type = pTrace_type[0];
                    if (trace_type != type)
                        mismatch = true;
                }

                if ((bufSize) && (pTrace_name))
                {
                    uint32_t n = vogl_strlen((const char *)pTrace_name) + 1;
                    if (bufSize < (GLsizei)n)
                        mismatch = true;
                    else if (memcmp(name_buf.get_ptr(), pTrace_name, n) != 0)
                        mismatch = true;
                }

                if (mismatch)
                {
                    process_entrypoint_warning("Replay of %s returned data differed from trace's\n", trace_packet.get_entrypoint_desc().m_pName);
                    vogl_warning_printf("Trace handle: %u, index: %u, bufSize: %u, trace_len: %u, trace_type: %u, name: %s\n",
                                       (uint32_t)trace_handle, (uint32_t)index, (uint32_t)bufSize, (uint32_t)trace_len, (uint32_t)trace_type, (pTrace_name != NULL) ? (const char *)pTrace_name : "");
                    vogl_warning_printf("GL handle: %u, index: %u, bufSize: %u, trace_len: %u, trace_type: %u, name: %s\n",
                                       (uint32_t)replay_handle, (uint32_t)index, (uint32_t)bufSize, (uint32_t)len, (uint32_t)type, name_buf.get_ptr());
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetAttachedShaders:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

                GLsizei max_count = trace_packet.get_param_value<GLsizei>(1);
                GLsizei count = 0;
                vogl::growable_array<GLuint, 16> shaders(max_count);

                GL_ENTRYPOINT(glGetAttachedShaders)(replay_handle, trace_packet.get_param_value<GLsizei>(1), &count, shaders.get_ptr());

                // TODO: Diff results
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetAttribLocation:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

                const GLchar *pName = trace_packet.get_param_client_memory<GLchar>(1);

                GLint replay_result = GL_ENTRYPOINT(glGetAttribLocation)(replay_handle, pName);
                GLint trace_result = trace_packet.get_return_value<GLint>();

                if (replay_result != trace_result)
                {
                    process_entrypoint_warning("Replay of %s returned data differed from trace's\n", trace_packet.get_entrypoint_desc().m_pName);
                    vogl_warning_printf("Trace value: %i, replay: %i\n", trace_result, replay_result);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetProgramivARB:
        {
            if (!benchmark_mode())
            {
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLint *pParams = trace_packet.get_param_client_memory<GLint>(2);
                uint32_t params_size = trace_packet.get_param_client_memory_data_size(2);
                uint32_t params_count = params_size / sizeof(GLint);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLint, 16> params(n + 1);
                    params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                    GL_ENTRYPOINT(glGetProgramivARB)(
                        trace_packet.get_param_value<GLenum>(0),
                        pname,
                        params.get_ptr());

                    VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                    if (params_count != static_cast<uint32_t>(n))
                    {
                        process_entrypoint_warning("Size of replay's params array differs from trace's\n");
                    }
                    else if (pParams && memcmp(pParams, params.get_ptr(), n * sizeof(GLint)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLint data differed from trace's, pname %s\n", get_gl_enums().find_gl_name(pname));
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetProgramiv:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

                GLenum pname = trace_packet.get_param_value<GLenum>(1);

                const GLint *pParams = trace_packet.get_param_client_memory<GLint>(2);
                uint32_t params_size = trace_packet.get_param_client_memory_data_size(2);
                uint32_t params_count = params_size / sizeof(GLint);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLint, 16> params(n + 1);
                    params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                    GL_ENTRYPOINT(glGetProgramiv)(
                        replay_handle,
                        pname,
                        params.get_ptr());

                    VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                    if (params_count != static_cast<uint32_t>(n))
                    {
                        process_entrypoint_warning("Size of replay's params array differs from trace's\n");
                    }
                    else if (pParams && memcmp(pParams, params.get_ptr(), n * sizeof(GLint)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLint data differed from trace's, pname %s\n", get_gl_enums().find_gl_name(pname));
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glLinkProgram:
        case VOGL_ENTRYPOINT_glLinkProgramARB:
        case VOGL_ENTRYPOINT_glProgramBinary:
        {
            handle_link_program(entrypoint_id);

            break;
        }
        case VOGL_ENTRYPOINT_glCompileShader:
        {
            GL_ENTRYPOINT(glCompileShader)(map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLuint>(0)));
            break;
        }
        case VOGL_ENTRYPOINT_glCompileShaderARB:
        {
            GL_ENTRYPOINT(glCompileShaderARB)(map_handle(get_shared_state()->m_shadow_state.m_objs, trace_packet.get_param_value<GLhandleARB>(0)));
            break;
        }
        case VOGL_ENTRYPOINT_glGetShaderiv:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                GLint params = 0;

                const GLint *pClient_params = trace_packet.get_param_client_memory<GLint>(2);

                GL_ENTRYPOINT(glGetShaderiv)(replay_handle, pname, &params);

                if ((pClient_params) && (*pClient_params != params))
                {
                    process_entrypoint_warning("Replay's returned data differed from trace's\n");
                    vogl_warning_printf("Trace data: %i, Replay data: %i\n", pClient_params ? *pClient_params : 0, params);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetShaderInfoLog:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);

            GLsizei trace_max_length = trace_packet.get_param_value<GLsizei>(1);
            const GLsizei *pTrace_length = trace_packet.get_param_client_memory<GLsizei>(2);
            VOGL_NOTE_UNUSED(pTrace_length);
            const GLchar *pTrace_info_log = trace_packet.get_param_client_memory<GLchar>(3);
            VOGL_NOTE_UNUSED(pTrace_info_log);

            vogl::growable_array<GLchar, 512> log(trace_max_length);
            GLsizei length = 0;
            GL_ENTRYPOINT(glGetShaderInfoLog)(replay_handle, trace_max_length, &length, log.get_ptr());

            if (length)
            {
                process_entrypoint_message("Info log for trace object %u, replay object %u:\n%s\n", trace_handle, replay_handle, log.get_ptr());
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetBooleanv:
        {
            if (!benchmark_mode())
            {
                GLenum pname = trace_packet.get_param_value<GLenum>(0);
                const GLboolean *pParams = trace_packet.get_param_client_memory<GLboolean>(1);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X (%s)\n", pname, get_gl_enums().find_gl_name(pname));
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLboolean, 16> params(n + 1);
                    params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_BYTE_MAGIC;

                    GL_ENTRYPOINT(glGetBooleanv)(pname, params.get_ptr());

                    VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_BYTE_MAGIC);

                    if (memcmp(pParams, params.get_ptr(), n * sizeof(GLboolean)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLboolean data for pname 0x%08X (%s) differed from trace's\n", pname, get_gl_enums().find_gl_name(pname));
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetDoublev:
        {
            if (!benchmark_mode())
            {
                GLenum pname = trace_packet.get_param_value<GLenum>(0);
                const GLdouble *pParams = trace_packet.get_param_client_memory<GLdouble>(1);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }
                else
                {
                    vogl::growable_array<GLdouble, 17> params(n + 1);
                    params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC;

                    GL_ENTRYPOINT(glGetDoublev)(pname, params.get_ptr());

                    VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC);

                    if (memcmp(pParams, params.get_ptr(), n * sizeof(GLdouble)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLdouble data differed from trace's\n");
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetFloatv:
        {
            if (!benchmark_mode())
            {
                GLenum pname = trace_packet.get_param_value<GLenum>(0);
                const GLfloat *pTrace_params = trace_packet.get_param_client_memory<GLfloat>(1);
                uint32_t trace_params_count = trace_packet.get_param_client_memory_data_size(1) / sizeof(GLfloat);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLfloat, 17> params(n + 1);
                params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC;

                GL_ENTRYPOINT(glGetFloatv)(pname, params.get_ptr());

                VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC);

                if (static_cast<int>(trace_params_count) < n)
                    process_entrypoint_warning("Replay param array size (%u) does not match the expected size (%u)\n", trace_params_count, n);
                else if (memcmp(pTrace_params, params.get_ptr(), n * sizeof(GLfloat)) != 0)
                {
                    process_entrypoint_warning("Replay's returned GLfloat data differed from trace's\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetIntegerv:
        {
            if (!benchmark_mode())
            {
                GLenum pname = trace_packet.get_param_value<GLenum>(0);
                const GLint *pTrace_params = trace_packet.get_param_client_memory<GLint>(1);
                uint32_t trace_params_count = trace_packet.get_param_client_memory_data_size(1) / sizeof(GLint);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLint, 16> params(n + 1);
                params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetIntegerv)(pname, params.get_ptr());

                VOGL_VERIFY(params[n] == (GLint)VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                bool is_binding = false;
                switch (pname)
                {
                    case GL_ARRAY_BUFFER_BINDING:
                    case GL_COLOR_ARRAY_BUFFER_BINDING:
                    case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
                    case GL_DRAW_FRAMEBUFFER_BINDING:
                    case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING:
                    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
                    case GL_FOG_COORD_ARRAY_BUFFER_BINDING:
                    case GL_INDEX_ARRAY_BUFFER_BINDING:
                    case GL_NORMAL_ARRAY_BUFFER_BINDING:
                    case GL_PIXEL_PACK_BUFFER_BINDING:
                    case GL_PIXEL_UNPACK_BUFFER_BINDING:
                    case GL_PROGRAM_PIPELINE_BINDING:
                    case GL_READ_FRAMEBUFFER_BINDING:
                    case GL_RENDERBUFFER_BINDING:
                    case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING:
                    case GL_SHADER_STORAGE_BUFFER_BINDING:
                    case GL_TEXTURE_BINDING_1D:
                    case GL_TEXTURE_BINDING_1D_ARRAY:
                    case GL_TEXTURE_BINDING_2D:
                    case GL_TEXTURE_BINDING_2D_ARRAY:
                    case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
                    case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
                    case GL_TEXTURE_BINDING_3D:
                    case GL_TEXTURE_BINDING_BUFFER:
                    case GL_TEXTURE_BINDING_CUBE_MAP:
                    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
                    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
                    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
                    case GL_UNIFORM_BUFFER_BINDING:
                    case GL_VERTEX_ARRAY_BINDING:
                    case GL_VERTEX_ARRAY_BUFFER_BINDING:
                    case GL_CURRENT_PROGRAM:
                    // Should be GL_TEXTURE_BUFFER_BINDING, see http://www.khronos.org/bugzilla/show_bug.cgi?id=844
                    case GL_TEXTURE_BUFFER:
                    {
                        is_binding = true;
                        break;
                    }
                    default:
                        break;
                }

                // Don't bother diffing bindings, the trace's are in the trace domain while the glGet's results are in the replay domain.
                if (!is_binding)
                {
                    if (static_cast<int>(trace_params_count) < n)
                    {
                        process_entrypoint_warning("Replay param array size (%u) does not match the expected size (%u)\n", trace_params_count, n);
                    }
                    else if (memcmp(pTrace_params, params.get_ptr(), n * sizeof(GLint)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned GLint data differed from trace's, pname %s\n", get_gl_enums().find_gl_name(pname));
                        for (int i = 0; i < n; i++)
                            vogl_verbose_printf("GLint %u: Trace: %i, Replay: %i\n", i, pTrace_params[i], params[i]);
                    }
                }
            }

            break;
        }
        // glProgramUniform's
        case VOGL_ENTRYPOINT_glProgramUniform1f:
        {
            set_program_uniform_helper1<GLfloat>(GL_ENTRYPOINT(glProgramUniform1f));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform1i:
        {
            set_program_uniform_helper1<GLint>(GL_ENTRYPOINT(glProgramUniform1i));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform1ui:
        {
            set_program_uniform_helper1<GLuint>(GL_ENTRYPOINT(glProgramUniform1ui));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform2f:
        {
            set_program_uniform_helper2<GLfloat>(GL_ENTRYPOINT(glProgramUniform2f));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform2i:
        {
            set_program_uniform_helper2<GLint>(GL_ENTRYPOINT(glProgramUniform2i));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform2ui:
        {
            set_program_uniform_helper2<GLuint>(GL_ENTRYPOINT(glProgramUniform2ui));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform3f:
        {
            set_program_uniform_helper3<GLfloat>(GL_ENTRYPOINT(glProgramUniform3f));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform3i:
        {
            set_program_uniform_helper3<GLint>(GL_ENTRYPOINT(glProgramUniform3i));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform3ui:
        {
            set_program_uniform_helper3<GLuint>(GL_ENTRYPOINT(glProgramUniform3ui));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform4f:
        {
            set_program_uniform_helper4<GLfloat>(GL_ENTRYPOINT(glProgramUniform4f));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform4i:
        {
            set_program_uniform_helper4<GLint>(GL_ENTRYPOINT(glProgramUniform4i));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform4ui:
        {
            set_program_uniform_helper4<GLuint>(GL_ENTRYPOINT(glProgramUniform4ui));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform1fv:
        {
            set_program_uniformv_helper<1, float>(GL_ENTRYPOINT(glProgramUniform1fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform2fv:
        {
            set_program_uniformv_helper<2, float>(GL_ENTRYPOINT(glProgramUniform2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform3fv:
        {
            set_program_uniformv_helper<3, float>(GL_ENTRYPOINT(glProgramUniform3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform4fv:
        {
            set_program_uniformv_helper<4, float>(GL_ENTRYPOINT(glProgramUniform4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform1iv:
        {
            set_program_uniformv_helper<1, GLint>(GL_ENTRYPOINT(glProgramUniform1iv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform2iv:
        {
            set_program_uniformv_helper<2, GLint>(GL_ENTRYPOINT(glProgramUniform2iv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform3iv:
        {
            set_program_uniformv_helper<3, GLint>(GL_ENTRYPOINT(glProgramUniform3iv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform4iv:
        {
            set_program_uniformv_helper<4, GLint>(GL_ENTRYPOINT(glProgramUniform4iv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform1uiv:
        {
            set_program_uniformv_helper<1, GLuint>(GL_ENTRYPOINT(glProgramUniform1uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform2uiv:
        {
            set_program_uniformv_helper<2, GLuint>(GL_ENTRYPOINT(glProgramUniform2uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform3uiv:
        {
            set_program_uniformv_helper<3, GLuint>(GL_ENTRYPOINT(glProgramUniform3uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniform4uiv:
        {
            set_program_uniformv_helper<4, GLuint>(GL_ENTRYPOINT(glProgramUniform4uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix2fv:
        {
            set_program_uniform_matrixv_helper<2, 2, float>(GL_ENTRYPOINT(glProgramUniformMatrix2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix3fv:
        {
            set_program_uniform_matrixv_helper<3, 3, float>(GL_ENTRYPOINT(glProgramUniformMatrix3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix4fv:
        {
            set_program_uniform_matrixv_helper<4, 4, float>(GL_ENTRYPOINT(glProgramUniformMatrix4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix2x3fv:
        {
            set_program_uniform_matrixv_helper<2, 3, float>(GL_ENTRYPOINT(glProgramUniformMatrix2x3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix3x2fv:
        {
            set_program_uniform_matrixv_helper<3, 2, float>(GL_ENTRYPOINT(glProgramUniformMatrix3x2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix2x4fv:
        {
            set_program_uniform_matrixv_helper<2, 4, float>(GL_ENTRYPOINT(glProgramUniformMatrix2x4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix4x2fv:
        {
            set_program_uniform_matrixv_helper<4, 2, float>(GL_ENTRYPOINT(glProgramUniformMatrix4x2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix3x4fv:
        {
            set_program_uniform_matrixv_helper<3, 4, float>(GL_ENTRYPOINT(glProgramUniformMatrix3x4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glProgramUniformMatrix4x3fv:
        {
            set_program_uniform_matrixv_helper<4, 3, float>(GL_ENTRYPOINT(glProgramUniformMatrix4x3fv));
            break;
        }
        // glUniform's
        case VOGL_ENTRYPOINT_glUniform1f:
        {
            set_uniform_helper1<GLfloat>(GL_ENTRYPOINT(glUniform1f));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1fARB:
        {
            set_uniform_helper1<GLfloat>(GL_ENTRYPOINT(glUniform1fARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2f:
        {
            set_uniform_helper2<GLfloat>(GL_ENTRYPOINT(glUniform2f));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2fARB:
        {
            set_uniform_helper2<GLfloat>(GL_ENTRYPOINT(glUniform2fARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3f:
        {
            set_uniform_helper3<GLfloat>(GL_ENTRYPOINT(glUniform3f));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3fARB:
        {
            set_uniform_helper3<GLfloat>(GL_ENTRYPOINT(glUniform3fARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4f:
        {
            set_uniform_helper4<GLfloat>(GL_ENTRYPOINT(glUniform4f));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4fARB:
        {
            set_uniform_helper4<GLfloat>(GL_ENTRYPOINT(glUniform4fARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1i:
        {
            set_uniform_helper1<GLint>(GL_ENTRYPOINT(glUniform1i));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1iARB:
        {
            set_uniform_helper1<GLint>(GL_ENTRYPOINT(glUniform1iARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2i:
        {
            set_uniform_helper2<GLint>(GL_ENTRYPOINT(glUniform2i));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2iARB:
        {
            set_uniform_helper2<GLint>(GL_ENTRYPOINT(glUniform2iARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3i:
        {
            set_uniform_helper3<GLint>(GL_ENTRYPOINT(glUniform3i));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3iARB:
        {
            set_uniform_helper3<GLint>(GL_ENTRYPOINT(glUniform3iARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4i:
        {
            set_uniform_helper4<GLint>(GL_ENTRYPOINT(glUniform4i));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4iARB:
        {
            set_uniform_helper4<GLint>(GL_ENTRYPOINT(glUniform4iARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1ui:
        {
            set_uniform_helper1<GLuint>(GL_ENTRYPOINT(glUniform1ui));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1uiEXT:
        {
            set_uniform_helper1<GLuint>(GL_ENTRYPOINT(glUniform1uiEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2ui:
        {
            set_uniform_helper2<GLuint>(GL_ENTRYPOINT(glUniform2ui));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2uiEXT:
        {
            set_uniform_helper2<GLuint>(GL_ENTRYPOINT(glUniform2uiEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3ui:
        {
            set_uniform_helper3<GLuint>(GL_ENTRYPOINT(glUniform3ui));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3uiEXT:
        {
            set_uniform_helper3<GLuint>(GL_ENTRYPOINT(glUniform3uiEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4ui:
        {
            set_uniform_helper4<GLuint>(GL_ENTRYPOINT(glUniform4ui));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4uiEXT:
        {
            set_uniform_helper4<GLuint>(GL_ENTRYPOINT(glUniform4uiEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1uiv:
        {
            set_uniformv_helper<1, GLuint>(GL_ENTRYPOINT(glUniform1uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1uivEXT:
        {
            set_uniformv_helper<1, GLuint>(GL_ENTRYPOINT(glUniform1uivEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2uiv:
        {
            set_uniformv_helper<2, GLuint>(GL_ENTRYPOINT(glUniform2uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2uivEXT:
        {
            set_uniformv_helper<2, GLuint>(GL_ENTRYPOINT(glUniform2uivEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3uiv:
        {
            set_uniformv_helper<3, GLuint>(GL_ENTRYPOINT(glUniform3uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3uivEXT:
        {
            set_uniformv_helper<3, GLuint>(GL_ENTRYPOINT(glUniform3uivEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4uiv:
        {
            set_uniformv_helper<4, GLuint>(GL_ENTRYPOINT(glUniform4uiv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4uivEXT:
        {
            set_uniformv_helper<4, GLuint>(GL_ENTRYPOINT(glUniform4uivEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1iv:
        {
            set_uniformv_helper<1, GLint>(GL_ENTRYPOINT(glUniform1iv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1ivARB:
        {
            set_uniformv_helper<1, GLint>(GL_ENTRYPOINT(glUniform1ivARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2iv:
        {
            set_uniformv_helper<2, GLint>(GL_ENTRYPOINT(glUniform2iv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2ivARB:
        {
            set_uniformv_helper<2, GLint>(GL_ENTRYPOINT(glUniform2ivARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3iv:
        {
            set_uniformv_helper<3, GLint>(GL_ENTRYPOINT(glUniform3iv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3ivARB:
        {
            set_uniformv_helper<3, GLint>(GL_ENTRYPOINT(glUniform3ivARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4iv:
        {
            set_uniformv_helper<4, GLint>(GL_ENTRYPOINT(glUniform4iv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4ivARB:
        {
            set_uniformv_helper<4, GLint>(GL_ENTRYPOINT(glUniform4ivARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1fv:
        {
            set_uniformv_helper<1, GLfloat>(GL_ENTRYPOINT(glUniform1fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform1fvARB:
        {
            set_uniformv_helper<1, GLfloat>(GL_ENTRYPOINT(glUniform1fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2fv:
        {
            set_uniformv_helper<2, GLfloat>(GL_ENTRYPOINT(glUniform2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform2fvARB:
        {
            set_uniformv_helper<2, GLfloat>(GL_ENTRYPOINT(glUniform2fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3fv:
        {
            set_uniformv_helper<3, GLfloat>(GL_ENTRYPOINT(glUniform3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform3fvARB:
        {
            set_uniformv_helper<3, GLfloat>(GL_ENTRYPOINT(glUniform3fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4fv:
        {
            set_uniformv_helper<4, GLfloat>(GL_ENTRYPOINT(glUniform4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniform4fvARB:
        {
            set_uniformv_helper<4, GLfloat>(GL_ENTRYPOINT(glUniform4fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix2fvARB:
        {
            set_uniform_matrixv_helper<2, 2, GLfloat>(GL_ENTRYPOINT(glUniformMatrix2fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix2fv:
        {
            set_uniform_matrixv_helper<2, 2, GLfloat>(GL_ENTRYPOINT(glUniformMatrix2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix3fvARB:
        {
            set_uniform_matrixv_helper<3, 3, GLfloat>(GL_ENTRYPOINT(glUniformMatrix3fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix3fv:
        {
            set_uniform_matrixv_helper<3, 3, GLfloat>(GL_ENTRYPOINT(glUniformMatrix3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix4fvARB:
        {
            set_uniform_matrixv_helper<4, 4, GLfloat>(GL_ENTRYPOINT(glUniformMatrix4fvARB));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix4fv:
        {
            set_uniform_matrixv_helper<4, 4, GLfloat>(GL_ENTRYPOINT(glUniformMatrix4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix2x3fv:
        {
            set_uniform_matrixv_helper<2, 3, GLfloat>(GL_ENTRYPOINT(glUniformMatrix2x3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix3x2fv:
        {
            set_uniform_matrixv_helper<3, 2, GLfloat>(GL_ENTRYPOINT(glUniformMatrix3x2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix2x4fv:
        {
            set_uniform_matrixv_helper<2, 4, GLfloat>(GL_ENTRYPOINT(glUniformMatrix2x4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix4x2fv:
        {
            set_uniform_matrixv_helper<4, 2, GLfloat>(GL_ENTRYPOINT(glUniformMatrix4x2fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix3x4fv:
        {
            set_uniform_matrixv_helper<3, 4, GLfloat>(GL_ENTRYPOINT(glUniformMatrix3x4fv));
            break;
        }
        case VOGL_ENTRYPOINT_glUniformMatrix4x3fv:
        {
            set_uniform_matrixv_helper<4, 3, GLfloat>(GL_ENTRYPOINT(glUniformMatrix4x3fv));
            break;
        }
        case VOGL_ENTRYPOINT_glBeginQuery:
        case VOGL_ENTRYPOINT_glBeginQueryARB:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(1);
            GLuint replay_handle = map_handle(get_shared_state()->m_queries, trace_handle);

            if (!m_pCur_context_state->m_inside_gl_begin)
                check_gl_error();

            if (entrypoint_id == VOGL_ENTRYPOINT_glBeginQuery)
                GL_ENTRYPOINT(glBeginQuery)(target, replay_handle);
            else
                GL_ENTRYPOINT(glBeginQueryARB)(target, replay_handle);

            if ((replay_handle) && (!m_pCur_context_state->m_inside_gl_begin) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                if (check_gl_error())
                    return cStatusGLError;

                get_shared_state()->m_query_targets[replay_handle] = target;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glEndQuery:
        {
            GL_ENTRYPOINT(glEndQuery)(trace_packet.get_param_value<GLenum>(0));
            break;
        }
        case VOGL_ENTRYPOINT_glEndQueryARB:
        {
            GL_ENTRYPOINT(glEndQueryARB)(trace_packet.get_param_value<GLenum>(0));
            break;
        }
        case VOGL_ENTRYPOINT_glGetQueryObjectiv:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_queries, trace_handle);

            GLenum pname = trace_packet.get_param_value<GLenum>(1);

            int n = get_gl_enums().get_pname_count(pname);
            if (n <= 0)
            {
                process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                return cStatusSoftFailure;
            }
            else
            {
                vogl::growable_array<GLint, 16> params(n + 1);
                params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetQueryObjectiv)(replay_handle, pname, params.get_ptr());

                VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetQueryObjectivARB:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_queries, trace_handle);

            GLenum pname = trace_packet.get_param_value<GLenum>(1);

            int n = get_gl_enums().get_pname_count(pname);
            if (n <= 0)
            {
                process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                return cStatusSoftFailure;
            }
            else
            {
                vogl::growable_array<GLint, 16> params(n + 1);
                params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetQueryObjectivARB)(replay_handle, pname, params.get_ptr());

                VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetQueryObjectuiv:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_queries, trace_handle);

            GLenum pname = trace_packet.get_param_value<GLenum>(1);

            int n = get_gl_enums().get_pname_count(pname);
            if (n <= 0)
            {
                process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                return cStatusSoftFailure;
            }
            else
            {
                vogl::growable_array<GLuint, 16> params(n + 1);
                params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetQueryObjectuiv)(replay_handle, pname, params.get_ptr());

                VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetQueryObjectuivARB:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_queries, trace_handle);

            GLenum pname = trace_packet.get_param_value<GLenum>(1);

            int n = get_gl_enums().get_pname_count(pname);
            if (n <= 0)
            {
                process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                return cStatusSoftFailure;
            }
            else
            {
                vogl::growable_array<GLuint, 16> params(n + 1);
                params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetQueryObjectuivARB)(replay_handle, pname, params.get_ptr());

                VOGL_VERIFY(params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glQueryCounter:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glQueryCounter;

            id = map_handle(get_shared_state()->m_queries, id);

            VOGL_REPLAY_CALL_GL_HELPER_glQueryCounter;

            break;
        }
        case VOGL_ENTRYPOINT_glGetQueryObjecti64v:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glGetQueryObjecti64v;
            VOGL_NOTE_UNUSED(pTrace_params);

            id = map_handle(get_shared_state()->m_queries, id);

            int n = get_gl_enums().get_pname_count(pname);
            if (n <= 0)
            {
                process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                return cStatusSoftFailure;
            }

            vogl::growable_array<GLint64, 16> temp_params(n + 1);
            temp_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

            GLint64 *pReplay_params = temp_params.get_ptr();

            VOGL_REPLAY_CALL_GL_HELPER_glGetQueryObjecti64v;

            VOGL_VERIFY(temp_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

            break;
        }
        case VOGL_ENTRYPOINT_glGetQueryObjectui64v:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glGetQueryObjectui64v;
            VOGL_NOTE_UNUSED(pTrace_params);

            id = map_handle(get_shared_state()->m_queries, id);

            int n = get_gl_enums().get_pname_count(pname);
            if (n <= 0)
            {
                process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                return cStatusSoftFailure;
            }

            vogl::growable_array<GLuint64, 16> temp_params(n + 1);
            temp_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

            GLuint64 *pReplay_params = temp_params.get_ptr();

            VOGL_REPLAY_CALL_GL_HELPER_glGetQueryObjectui64v;

            VOGL_VERIFY(temp_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

            break;
        }
        case VOGL_ENTRYPOINT_glBindBuffer:
        case VOGL_ENTRYPOINT_glBindBufferARB:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(1);
            GLuint replay_handle = map_handle(get_shared_state()->m_buffers, trace_handle);

            check_gl_error();

            SWITCH_GL_ENTRYPOINT2_VOID(glBindBuffer, glBindBufferARB, target, replay_handle);

            if (check_gl_error())
                return cStatusGLError;

            if ((trace_handle) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                GLuint *pBinding = get_shared_state()->m_buffer_targets.find_value(trace_handle);
                if (!pBinding)
                    process_entrypoint_error("Couldn't find trace buffer handle 0x%X in buffer target map!\n", trace_handle);
                else if (*pBinding == GL_NONE)
                    *pBinding = target;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBindBufferBase:
        case VOGL_ENTRYPOINT_glBindBufferBaseEXT:
        case VOGL_ENTRYPOINT_glBindBufferBaseNV:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindBufferBase;

            GLuint trace_buffer = buffer;
            buffer = map_handle(get_shared_state()->m_buffers, buffer);

            check_gl_error();

            if (entrypoint_id == VOGL_ENTRYPOINT_glBindBufferBaseNV)
                VOGL_REPLAY_CALL_GL_HELPER_glBindBufferBaseNV;
            else if (entrypoint_id == VOGL_ENTRYPOINT_glBindBufferBaseEXT)
                VOGL_REPLAY_CALL_GL_HELPER_glBindBufferBaseEXT;
            else
                VOGL_REPLAY_CALL_GL_HELPER_glBindBufferBase;

            if (check_gl_error())
                return cStatusGLError;

            if ((trace_buffer) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                GLuint *pBinding = get_shared_state()->m_buffer_targets.find_value(trace_buffer);
                if (!pBinding)
                    process_entrypoint_error("Couldn't find trace buffer handle 0x%X in buffer target map!\n", trace_buffer);
                else if (*pBinding == GL_NONE)
                    *pBinding = target;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBindBufferRange:
        case VOGL_ENTRYPOINT_glBindBufferRangeEXT:
        case VOGL_ENTRYPOINT_glBindBufferRangeNV:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindBufferRange;

            GLuint trace_buffer = buffer;
            buffer = map_handle(get_shared_state()->m_buffers, buffer);

            check_gl_error();

            if (entrypoint_id == VOGL_ENTRYPOINT_glBindBufferRangeNV)
                VOGL_REPLAY_CALL_GL_HELPER_glBindBufferRangeNV;
            else if (entrypoint_id == VOGL_ENTRYPOINT_glBindBufferRangeEXT)
                VOGL_REPLAY_CALL_GL_HELPER_glBindBufferRangeEXT;
            else
                VOGL_REPLAY_CALL_GL_HELPER_glBindBufferRange;

            if (check_gl_error())
                return cStatusGLError;

            if ((trace_buffer) && (get_context_state()->m_current_display_list_mode != GL_COMPILE))
            {
                GLuint *pBinding = get_shared_state()->m_buffer_targets.find_value(trace_buffer);
                if (!pBinding)
                    process_entrypoint_error("Couldn't find trace buffer handle 0x%X in buffer target map!\n", trace_buffer);
                else if (*pBinding == GL_NONE)
                    *pBinding = target;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glFenceSync:
        {
            vogl_sync_ptr_value trace_handle = trace_packet.get_return_ptr_value();
            if (trace_handle)
            {
                GLsync replay_handle = GL_ENTRYPOINT(glFenceSync)(trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLbitfield>(1));
                if (!replay_handle)
                {
                    process_entrypoint_error("glFenceSync on trace handle 0x%" PRIX64 " succeeded in the trace, but failed during replay!\n", trace_handle);
                    return cStatusHardFailure;
                }
                else
                {
                    get_shared_state()->m_syncs.insert(trace_handle, replay_handle);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glWaitSync:
        case VOGL_ENTRYPOINT_glClientWaitSync:
        {
            vogl_sync_ptr_value trace_sync = trace_packet.get_param_ptr_value(0);
            GLsync replay_sync = NULL;

            gl_sync_hash_map::const_iterator it = get_shared_state()->m_syncs.find(trace_sync);
            if (it == get_shared_state()->m_syncs.end())
            {
                if (trace_sync)
                {
                    process_entrypoint_error("Unable to map trace sync handle 0x%" PRIX64 " to GL handle\n", trace_sync);
                    return cStatusHardFailure;
                }
            }
            else
            {
                replay_sync = it->second;
            }

            if (entrypoint_id == VOGL_ENTRYPOINT_glWaitSync)
                GL_ENTRYPOINT(glWaitSync)(replay_sync, trace_packet.get_param_value<GLbitfield>(1), trace_packet.get_param_value<GLuint64>(2));
            else
                GL_ENTRYPOINT(glClientWaitSync)(replay_sync, trace_packet.get_param_value<GLbitfield>(1), trace_packet.get_param_value<GLuint64>(2));

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteSync:
        {
            vogl_sync_ptr_value trace_sync = trace_packet.get_param_ptr_value(0);
            GLsync replay_sync = NULL;

            gl_sync_hash_map::const_iterator it = get_shared_state()->m_syncs.find(trace_sync);
            if (it == get_shared_state()->m_syncs.end())
            {
                if (trace_sync)
                {
                    process_entrypoint_error("Unable to map trace sync handle 0x%" PRIX64 " to GL handle\n", trace_sync);
                    return cStatusHardFailure;
                }
            }
            else
            {
                replay_sync = it->second;
            }

            GL_ENTRYPOINT(glDeleteSync)(replay_sync);

            if (trace_sync)
            {
                get_shared_state()->m_syncs.erase(trace_sync);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glVertexPointer:
        {
            vertex_array_helper(trace_packet.get_param_value<GLint>(0), trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLsizei>(2), trace_packet.get_param_ptr_value(3),
                                vogl_vertex_pointer_array_id, GL_ENTRYPOINT(glVertexPointer), m_client_side_array_data[vogl_vertex_pointer_array_id]);
            break;
        }
        case VOGL_ENTRYPOINT_glVertexPointerEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glVertexPointerEXT;
            VOGL_NOTE_UNUSED(pTrace_pointer);

            vertex_array_helper_count(size, type, stride, count, trace_packet.get_param_ptr_value(4), vogl_vertex_pointer_array_id, GL_ENTRYPOINT(glVertexPointerEXT), m_client_side_array_data[vogl_vertex_pointer_array_id]);
            break;
        }
        case VOGL_ENTRYPOINT_glColorPointer:
        {
            vertex_array_helper(trace_packet.get_param_value<GLint>(0), trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLsizei>(2), trace_packet.get_param_ptr_value(3),
                                vogl_color_pointer_array_id, GL_ENTRYPOINT(glColorPointer), m_client_side_array_data[vogl_color_pointer_array_id]);
            break;
        }
        case VOGL_ENTRYPOINT_glColorPointerEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glColorPointerEXT;
            VOGL_NOTE_UNUSED(pTrace_pointer);

            vertex_array_helper_count(size, type, stride, count, trace_packet.get_param_ptr_value(4), vogl_color_pointer_array_id, GL_ENTRYPOINT(glColorPointerEXT), m_client_side_array_data[vogl_color_pointer_array_id]);
            break;
        }
        case VOGL_ENTRYPOINT_glSecondaryColorPointer:
        {
            vertex_array_helper(trace_packet.get_param_value<GLint>(0), trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLsizei>(2), trace_packet.get_param_ptr_value(3),
                                vogl_secondary_color_pointer_array_id, GL_ENTRYPOINT(glSecondaryColorPointer), m_client_side_array_data[vogl_secondary_color_pointer_array_id]);
            break;
        }
        case VOGL_ENTRYPOINT_glSecondaryColorPointerEXT:
        {
            vertex_array_helper(trace_packet.get_param_value<GLint>(0), trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLsizei>(2), trace_packet.get_param_ptr_value(3),
                                vogl_secondary_color_pointer_array_id, GL_ENTRYPOINT(glSecondaryColorPointerEXT), m_client_side_array_data[vogl_secondary_color_pointer_array_id]);
            break;
        }
        case VOGL_ENTRYPOINT_glTexCoordPointer:
        {
            GLint cur_client_active_texture = 0;
            GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &cur_client_active_texture);

            int tex_index = cur_client_active_texture - GL_TEXTURE0;
            if ((tex_index < 0) || (tex_index >= VOGL_MAX_SUPPORTED_GL_TEXCOORD_ARRAYS))
            {
                process_entrypoint_error("glTexCoordPointer/glTexCoordPointerEXT called with an invalid or unsupported client active texture (0x%08X)\n", cur_client_active_texture);
                return cStatusSoftFailure;
            }

            vertex_array_helper(trace_packet.get_param_value<GLint>(0), trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLsizei>(2), trace_packet.get_param_ptr_value(3),
                                vogl_texcoord_pointer_array_id, GL_ENTRYPOINT(glTexCoordPointer), m_client_side_texcoord_data[tex_index]);
            break;
        }
        case VOGL_ENTRYPOINT_glTexCoordPointerEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexCoordPointerEXT;
            VOGL_NOTE_UNUSED(pTrace_pointer);

            GLint cur_client_active_texture = 0;
            GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &cur_client_active_texture);

            int tex_index = cur_client_active_texture - GL_TEXTURE0;
            if ((tex_index < 0) || (tex_index >= VOGL_MAX_SUPPORTED_GL_TEXCOORD_ARRAYS))
            {
                process_entrypoint_error("glTexCoordPointer/glTexCoordPointerEXT called with an invalid or unsupported client active texture (0x%08X)\n", cur_client_active_texture);
                return cStatusSoftFailure;
            }

            vertex_array_helper_count(size, type, stride, count, trace_packet.get_param_ptr_value(4), vogl_texcoord_pointer_array_id, GL_ENTRYPOINT(glTexCoordPointerEXT), m_client_side_texcoord_data[tex_index]);
            break;
        }
        case VOGL_ENTRYPOINT_glFogCoordPointer:
        {
            vertex_array_helper_no_size(trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLsizei>(1), trace_packet.get_param_ptr_value(2),
                                        vogl_fog_coord_pointer_array_id, GL_ENTRYPOINT(glFogCoordPointer));
            break;
        }
        case VOGL_ENTRYPOINT_glFogCoordPointerEXT:
        {
            vertex_array_helper_no_size(trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLsizei>(1), trace_packet.get_param_ptr_value(2),
                                        vogl_fog_coord_pointer_array_id, GL_ENTRYPOINT(glFogCoordPointerEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glIndexPointer:
        {
            vertex_array_helper_no_size(trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLsizei>(1), trace_packet.get_param_ptr_value(2),
                                        vogl_index_pointer_array_id, GL_ENTRYPOINT(glIndexPointer));
            break;
        }
        case VOGL_ENTRYPOINT_glIndexPointerEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glIndexPointerEXT;
            VOGL_NOTE_UNUSED(pTrace_pointer);

            vertex_array_helper_no_size_count(type, stride, count, trace_packet.get_param_ptr_value(3), vogl_index_pointer_array_id, GL_ENTRYPOINT(glIndexPointerEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glNormalPointer:
        {
            vertex_array_helper_no_size(trace_packet.get_param_value<GLenum>(0), trace_packet.get_param_value<GLsizei>(1), trace_packet.get_param_ptr_value(2),
                                        vogl_normal_pointer_array_id, GL_ENTRYPOINT(glNormalPointer));
            break;
        }
        case VOGL_ENTRYPOINT_glNormalPointerEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glNormalPointerEXT;
            VOGL_NOTE_UNUSED(pTrace_pointer);

            vertex_array_helper_no_size_count(type, stride, count, trace_packet.get_param_ptr_value(3), vogl_normal_pointer_array_id, GL_ENTRYPOINT(glNormalPointerEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glEdgeFlagPointer:
        {
            vertex_array_helper_no_type_no_size(trace_packet.get_param_value<GLsizei>(0), trace_packet.get_param_ptr_value(1),
                                                vogl_edge_flag_pointer_array_id, GL_ENTRYPOINT(glEdgeFlagPointer));
            break;
        }
        case VOGL_ENTRYPOINT_glEdgeFlagPointerEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glEdgeFlagPointerEXT;
            VOGL_NOTE_UNUSED(pTrace_pointer);

            vertex_array_helper_no_type_no_size_count(stride, count, trace_packet.get_param_ptr_value(2), vogl_edge_flag_pointer_array_id, GL_ENTRYPOINT(glEdgeFlagPointerEXT));
            break;
        }
        case VOGL_ENTRYPOINT_glInterleavedArrays:
        {
            // TODO: Test this more!
            GLenum format = trace_packet.get_param_value<GLenum>(0);
            GLsizei stride = trace_packet.get_param_value<GLsizei>(1);
            const vogl_trace_ptr_value trace_pointer_value = trace_packet.get_param_ptr_value(2);

            uint32_t fmt_index;
            for (fmt_index = 0; fmt_index < VOGL_INTERLEAVED_ARRAY_SIZE; fmt_index++)
                if (format == vogl_g_interleaved_array_descs[fmt_index].fmt)
                    break;
            if (fmt_index == VOGL_INTERLEAVED_ARRAY_SIZE)
            {
                process_entrypoint_error("Invalid interleaved vertex format: 0x%X \n", format);
                return cStatusSoftFailure;
            }

            if (stride < 0)
            {
                process_entrypoint_error("Invalid interleaved vertex stride: %i\n", static_cast<int>(stride));
                return cStatusSoftFailure;
            }

            const interleaved_array_desc_entry_t &fmt = vogl_g_interleaved_array_descs[fmt_index];

            if (!stride)
            {
                stride = fmt.s;
                VOGL_ASSERT(stride > 0);
            }

            GL_ENTRYPOINT(glDisableClientState)(GL_EDGE_FLAG_ARRAY);
            GL_ENTRYPOINT(glDisableClientState)(GL_INDEX_ARRAY);
            GL_ENTRYPOINT(glDisableClientState)(GL_SECONDARY_COLOR_ARRAY);
            GL_ENTRYPOINT(glDisableClientState)(GL_FOG_COORD_ARRAY);

            check_gl_error();

            if (fmt.et)
            {
                GLint cur_client_active_texture = 0;
                GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &cur_client_active_texture);

                int tex_index = cur_client_active_texture - GL_TEXTURE0;
                if ((tex_index < 0) || (tex_index >= VOGL_MAX_SUPPORTED_GL_TEXCOORD_ARRAYS))
                {
                    process_entrypoint_error("glInterleavedArrays called with an invalid or unsupported client active texture (0x%08X)\n", cur_client_active_texture);
                    return cStatusSoftFailure;
                }

                GL_ENTRYPOINT(glEnableClientState)(GL_TEXTURE_COORD_ARRAY);
                vertex_array_helper(fmt.st, GL_FLOAT, stride, trace_pointer_value,
                                    vogl_texcoord_pointer_array_id, GL_ENTRYPOINT(glTexCoordPointer), m_client_side_texcoord_data[tex_index]);
            }
            else
            {
                GL_ENTRYPOINT(glDisableClientState)(GL_TEXTURE_COORD_ARRAY);
            }

            check_gl_error();

            if (fmt.ec)
            {
                GL_ENTRYPOINT(glEnableClientState)(GL_COLOR_ARRAY);
                vertex_array_helper(fmt.sc, fmt.tc, stride, trace_pointer_value + fmt.pc,
                                    vogl_color_pointer_array_id, GL_ENTRYPOINT(glColorPointer), m_client_side_array_data[vogl_color_pointer_array_id]);
            }
            else
            {
                GL_ENTRYPOINT(glDisableClientState)(GL_COLOR_ARRAY);
            }

            check_gl_error();

            if (fmt.en)
            {
                GL_ENTRYPOINT(glEnableClientState)(GL_NORMAL_ARRAY);
                vertex_array_helper_no_size(GL_FLOAT, stride, trace_pointer_value + fmt.pn,
                                            vogl_normal_pointer_array_id, GL_ENTRYPOINT(glNormalPointer));
            }
            else
            {
                GL_ENTRYPOINT(glDisableClientState)(GL_NORMAL_ARRAY);
            }

            check_gl_error();

            GL_ENTRYPOINT(glEnableClientState)(GL_VERTEX_ARRAY);
            vertex_array_helper(fmt.sv, GL_FLOAT, stride, trace_pointer_value + fmt.pv,
                                vogl_vertex_pointer_array_id, GL_ENTRYPOINT(glVertexPointer), m_client_side_array_data[vogl_vertex_pointer_array_id]);

            break;
        }
        case VOGL_ENTRYPOINT_glVertexAttribIPointer:
        case VOGL_ENTRYPOINT_glVertexAttribIPointerEXT:
        {
            GLuint index = trace_packet.get_param_value<GLuint>(0);
            GLint size = trace_packet.get_param_value<GLint>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            GLsizei stride = trace_packet.get_param_value<GLsizei>(3);
            vogl_trace_ptr_value trace_pointer = trace_packet.get_param_ptr_value(4);

            if (index >= m_pCur_context_state->m_context_info.get_max_vertex_attribs())
            {
                process_entrypoint_error("Generic vertex attribute index is too large\n");
                return cStatusSoftFailure;
            }

            GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);
            void *pPtr = reinterpret_cast<void *>(trace_pointer);
            if ((!buffer) && (trace_pointer))
            {
                // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
                // So point this guy into one of our client size memory buffers that's hopefully large enough.
                if (!m_client_side_vertex_attrib_data[index].size())
                {
                    m_client_side_vertex_attrib_data[index].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
                }
                pPtr = m_client_side_vertex_attrib_data[index].get_ptr();
            }

            if (entrypoint_id == VOGL_ENTRYPOINT_glVertexAttribIPointer)
                GL_ENTRYPOINT(glVertexAttribIPointer)(index, size, type, stride, pPtr);
            else
                GL_ENTRYPOINT(glVertexAttribIPointerEXT)(index, size, type, stride, pPtr);

            break;
        }
        case VOGL_ENTRYPOINT_glVertexAttribPointerARB:
        case VOGL_ENTRYPOINT_glVertexAttribPointer:
        {
            GLuint index = trace_packet.get_param_value<GLuint>(0);
            GLint size = trace_packet.get_param_value<GLint>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            GLboolean normalized = trace_packet.get_param_value<GLboolean>(3);
            GLsizei stride = trace_packet.get_param_value<GLsizei>(4);
            vogl_trace_ptr_value trace_pointer = trace_packet.get_param_ptr_value(5);

            if (index >= m_pCur_context_state->m_context_info.get_max_vertex_attribs())
            {
                process_entrypoint_error("Generic vertex attribute index is too large\n");
                return cStatusSoftFailure;
            }

            GLuint buffer = vogl_get_bound_gl_buffer(GL_ARRAY_BUFFER);
            void *pPtr = reinterpret_cast<void *>(trace_pointer);
            if ((!buffer) && (trace_pointer))
            {
                // We've got a trace pointer to client side memory, but we don't have it until the actual draw.
                // So point this guy into one of our client size memory buffers that's hopefully large enough.
                if (!m_client_side_vertex_attrib_data[index].size())
                {
                    m_client_side_vertex_attrib_data[index].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
                }
                pPtr = m_client_side_vertex_attrib_data[index].get_ptr();
            }

            if (entrypoint_id == VOGL_ENTRYPOINT_glVertexAttribPointer)
                GL_ENTRYPOINT(glVertexAttribPointer)(index, size, type, normalized, stride, pPtr);
            else
                GL_ENTRYPOINT(glVertexAttribPointerARB)(index, size, type, normalized, stride, pPtr);

            break;
        }
        case VOGL_ENTRYPOINT_glDrawRangeElements:
        case VOGL_ENTRYPOINT_glDrawRangeElementsEXT:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLuint start = trace_packet.get_param_value<GLuint>(1);
            GLuint end = trace_packet.get_param_value<GLuint>(2);
            GLsizei count = trace_packet.get_param_value<GLsizei>(3);
            GLenum type = trace_packet.get_param_value<GLenum>(4);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(5);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, start, end, count, type, trace_indices_ptr_value, pIndices, 0, true, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (entrypoint_id == VOGL_ENTRYPOINT_glDrawRangeElements)
                    GL_ENTRYPOINT(glDrawRangeElements)(mode, start, end, count, type, pIndices);
                else
                    GL_ENTRYPOINT(glDrawRangeElementsEXT)(mode, start, end, count, type, pIndices);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawRangeElementsBaseVertex:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLuint start = trace_packet.get_param_value<GLuint>(1);
            GLuint end = trace_packet.get_param_value<GLuint>(2);
            GLsizei count = trace_packet.get_param_value<GLsizei>(3);
            GLenum type = trace_packet.get_param_value<GLenum>(4);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(5);
            GLint basevertex = trace_packet.get_param_value<GLint>(6);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, start, end, count, type, trace_indices_ptr_value, pIndices, basevertex, true, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                GL_ENTRYPOINT(glDrawRangeElementsBaseVertex)(mode, start, end, count, type, pIndices, basevertex);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElements:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLsizei count = trace_packet.get_param_value<GLsizei>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(3);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, 0, 0, count, type, trace_indices_ptr_value, pIndices, 0, false, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                GL_ENTRYPOINT(glDrawElements)(mode, count, type, pIndices);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawArraysInstanced:
        case VOGL_ENTRYPOINT_glDrawArraysInstancedEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glDrawArraysInstanced;

            const GLvoid *pIndices = NULL;
            if (!draw_elements_client_side_array_setup(mode, first, first + count - 1, count, GL_UNSIGNED_BYTE, 0, pIndices, 0, true, false))
                return cStatusSoftFailure;

            if (entrypoint_id == VOGL_ENTRYPOINT_glDrawArraysInstancedEXT)
            {
                GLsizei start = first, primcount = instancecount;
                VOGL_REPLAY_CALL_GL_HELPER_glDrawArraysInstancedEXT;
            }
            else
                VOGL_REPLAY_CALL_GL_HELPER_glDrawArraysInstanced;

            break;
        }
        case VOGL_ENTRYPOINT_glDrawArraysInstancedBaseInstance:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glDrawArraysInstancedBaseInstance;

            const GLvoid *pIndices = NULL;
            if (!draw_elements_client_side_array_setup(mode, first, first + count - 1, count, GL_UNSIGNED_BYTE, 0, pIndices, 0, true, false))
                return cStatusSoftFailure;

            VOGL_REPLAY_CALL_GL_HELPER_glDrawArraysInstancedBaseInstance;

            break;
        }
        case VOGL_ENTRYPOINT_glDrawArrays:
        case VOGL_ENTRYPOINT_glDrawArraysEXT:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLint first = trace_packet.get_param_value<GLint>(1);
            GLsizei count = trace_packet.get_param_value<GLsizei>(2);

            const GLvoid *pIndices = NULL;
            if (!draw_elements_client_side_array_setup(mode, first, first + count - 1, count, GL_UNSIGNED_BYTE, 0, pIndices, 0, true, false))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (entrypoint_id == VOGL_ENTRYPOINT_glDrawArraysEXT)
                    GL_ENTRYPOINT(glDrawArraysEXT)(mode, first, count);
                else
                    GL_ENTRYPOINT(glDrawArrays)(mode, first, count);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElementsInstanced:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedARB:
        case VOGL_ENTRYPOINT_glDrawElementsInstancedEXT:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLsizei count = trace_packet.get_param_value<GLsizei>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(3);
            GLsizei primcount = trace_packet.get_param_value<GLsizei>(4);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, 0, 0, count, type, trace_indices_ptr_value, pIndices, 0, false, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (entrypoint_id == VOGL_ENTRYPOINT_glDrawElementsInstanced)
                    GL_ENTRYPOINT(glDrawElementsInstanced)(mode, count, type, pIndices, primcount);
                else if (entrypoint_id == VOGL_ENTRYPOINT_glDrawElementsInstancedEXT)
                    GL_ENTRYPOINT(glDrawElementsInstancedEXT)(mode, count, type, pIndices, primcount);
                else
                    GL_ENTRYPOINT(glDrawElementsInstancedARB)(mode, count, type, pIndices, primcount);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElementsInstancedBaseInstance:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLsizei count = trace_packet.get_param_value<GLsizei>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(3);
            GLsizei primcount = trace_packet.get_param_value<GLsizei>(4);
            GLuint baseinstance = trace_packet.get_param_value<GLuint>(5);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, 0, 0, count, type, trace_indices_ptr_value, pIndices, 0, false, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                GL_ENTRYPOINT(glDrawElementsInstancedBaseInstance)(mode, count, type, pIndices, primcount, baseinstance);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElementsInstancedBaseVertex:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLsizei count = trace_packet.get_param_value<GLsizei>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(3);
            GLsizei primcount = trace_packet.get_param_value<GLsizei>(4);
            GLint basevertex = trace_packet.get_param_value<GLint>(5);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, 0, 0, count, type, trace_indices_ptr_value, pIndices, basevertex, false, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                GL_ENTRYPOINT(glDrawElementsInstancedBaseVertex)(mode, count, type, pIndices, primcount, basevertex);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElementsInstancedBaseVertexBaseInstance:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLsizei count = trace_packet.get_param_value<GLsizei>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(3);
            GLsizei primcount = trace_packet.get_param_value<GLsizei>(4);
            GLint basevertex = trace_packet.get_param_value<GLint>(5);
            GLuint baseinstance = trace_packet.get_param_value<GLuint>(6);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, 0, 0, count, type, trace_indices_ptr_value, pIndices, basevertex, false, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                GL_ENTRYPOINT(glDrawElementsInstancedBaseVertexBaseInstance)(mode, count, type, pIndices, primcount, basevertex, baseinstance);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMultiDrawArrays:
        case VOGL_ENTRYPOINT_glMultiDrawArraysEXT:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);

            const GLint *pFirst = trace_packet.get_param_client_memory<const GLint>(1);
            uint32_t first_size = trace_packet.get_param_client_memory_data_size(1);

            const GLsizei *pCount = trace_packet.get_param_client_memory<const GLsizei>(2);
            uint32_t count_size = trace_packet.get_param_client_memory_data_size(2);

            GLsizei primcount = trace_packet.get_param_value<GLsizei>(3);

            if ((first_size != primcount * sizeof(GLint)) || (count_size != primcount * sizeof(GLsizei)))
            {
                process_entrypoint_error("first and/or count params do not point to arrays of the expected size\n");
                return cStatusSoftFailure;
            }

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                //  Multi-draws with client side arrays are not supported for replay.
                if (entrypoint_id == VOGL_ENTRYPOINT_glMultiDrawElements)
                    GL_ENTRYPOINT(glMultiDrawArrays)(mode, pFirst, pCount, primcount);
                else
                    GL_ENTRYPOINT(glMultiDrawArraysEXT)(mode, pFirst, pCount, primcount);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMultiDrawElements:
        case VOGL_ENTRYPOINT_glMultiDrawElementsEXT:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);

            const GLsizei *pCount = trace_packet.get_param_client_memory<const GLsizei>(1);
            uint32_t count_size = trace_packet.get_param_client_memory_data_size(1);

            GLenum type = trace_packet.get_param_value<GLenum>(2);

            const vogl_client_memory_array trace_indices_void_ptr_array = trace_packet.get_param_client_memory_array(3); // const GLvoid *

            GLsizei primcount = trace_packet.get_param_value<GLsizei>(4);

            if ((count_size != static_cast<uint32_t>(primcount * sizeof(GLsizei))) || (trace_indices_void_ptr_array.size() != static_cast<uint32_t>(primcount)))
            {
                process_entrypoint_error("count and/or indices params do not point to arrays of the expected size\n");
                return cStatusSoftFailure;
            }

            vogl::growable_array<GLvoid *, 256> replay_indices(trace_indices_void_ptr_array.size());
            for (uint32_t i = 0; i < trace_indices_void_ptr_array.size(); i++)
                replay_indices[i] = reinterpret_cast<GLvoid *>(trace_indices_void_ptr_array.get_element<vogl_trace_ptr_value>(i));

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                //  Multi-draws with client side arrays are not supported for replay.
                if (entrypoint_id == VOGL_ENTRYPOINT_glMultiDrawElements)
                    GL_ENTRYPOINT(glMultiDrawElements)(mode, pCount, type, replay_indices.get_ptr(), primcount);
                else
                    GL_ENTRYPOINT(glMultiDrawElementsEXT)(mode, pCount, type, (const GLvoid **)replay_indices.get_ptr(), primcount);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMultiDrawElementsBaseVertex:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);

            const GLsizei *pCount = trace_packet.get_param_client_memory<const GLsizei>(1);
            uint32_t count_size = trace_packet.get_param_client_memory_data_size(1);

            GLenum type = trace_packet.get_param_value<GLenum>(2);

            const vogl_client_memory_array trace_indices_void_ptr_array = trace_packet.get_param_client_memory_array(3); // const GLvoid *
            //GLvoid * const *ppIndices = trace_packet.get_param_client_memory<GLvoid *>(3);
            //uint32_t index_size = trace_packet.get_param_client_memory_data_size(3);

            GLsizei primcount = trace_packet.get_param_value<GLsizei>(4);

            const GLint *pBase_vertex = trace_packet.get_param_client_memory<const GLint>(5);
            uint32_t base_vertex_size = trace_packet.get_param_client_memory_data_size(5);

            if ((count_size != primcount * sizeof(GLsizei)) ||
                (trace_indices_void_ptr_array.size() != static_cast<uint32_t>(primcount)) ||
                (base_vertex_size != primcount * sizeof(GLint)))
            {
                process_entrypoint_error("count, indices, and/or base_vertex_size params do not point to arrays of the expected size\n");
                return cStatusSoftFailure;
            }

            vogl::growable_array<GLvoid *, 256> replay_indices(trace_indices_void_ptr_array.size());
            for (uint32_t i = 0; i < trace_indices_void_ptr_array.size(); i++)
                replay_indices[i] = reinterpret_cast<GLvoid *>(trace_indices_void_ptr_array.get_element<vogl_trace_ptr_value>(i));

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                //  Multi-draws with client side arrays are not supported for replay.
                GL_ENTRYPOINT(glMultiDrawElementsBaseVertex)(mode, pCount, type, replay_indices.get_ptr(), primcount, pBase_vertex);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElementsBaseVertex:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLsizei count = trace_packet.get_param_value<GLsizei>(1);
            GLenum type = trace_packet.get_param_value<GLenum>(2);
            vogl_trace_ptr_value trace_indices_ptr_value = trace_packet.get_param_ptr_value(3);
            GLint base_vertex = trace_packet.get_param_value<GLint>(4);

            const GLvoid *pIndices;
            if (!draw_elements_client_side_array_setup(mode, 0, 0, count, type, trace_indices_ptr_value, pIndices, base_vertex, false, true))
                return cStatusSoftFailure;

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                GL_ENTRYPOINT(glDrawElementsBaseVertex)(mode, count, type, pIndices, base_vertex);
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawElementsIndirect:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLenum type = trace_packet.get_param_value<GLenum>(1);
            vogl_trace_ptr_value trace_indirect_ptr_value = trace_packet.get_param_ptr_value(2);

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (trace_packet.get_param_client_memory_data_size(2) != 0)
                {
                    // TODO: Handle glDrawElementsIndirect with client-side indirect data
                    process_entrypoint_error("client-side indirect commands not implemented\n");
                    return cStatusSoftFailure;
                }
                else
                {
                    GL_ENTRYPOINT(glDrawElementsIndirect)(mode, type, (const GLvoid *)trace_indirect_ptr_value);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMultiDrawElementsIndirect:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            GLenum type = trace_packet.get_param_value<GLenum>(1);
            vogl_trace_ptr_value trace_indirect_ptr_value = trace_packet.get_param_ptr_value(2);
            GLsizei drawcount = trace_packet.get_param_value<GLenum>(3);
            GLsizei stride = trace_packet.get_param_value<GLenum>(4);

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (trace_packet.get_param_client_memory_data_size(2) != 0)
                {
                    // TODO: Handle glMultiDrawElementsIndirect with client-side indirect data
                    process_entrypoint_error("client-side indirect commands not implemented\n");
                    return cStatusSoftFailure;
                }
                else
                {
                    GL_ENTRYPOINT(glMultiDrawElementsIndirect)(mode, type, (const GLvoid *)trace_indirect_ptr_value, drawcount, stride);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glDrawArraysIndirect:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            vogl_trace_ptr_value trace_indirect_ptr_value = trace_packet.get_param_ptr_value(1);

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (trace_packet.get_param_client_memory_data_size(1) != 0)
                {
                    // TODO: Handle glDrawArraysIndirect with client-side indirect data
                    process_entrypoint_error("client-side indirect commands not implemented\n");
                    return cStatusSoftFailure;
                }
                else
                {
                    GL_ENTRYPOINT(glDrawArraysIndirect)(mode, (const GLvoid *)trace_indirect_ptr_value);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMultiDrawArraysIndirect:
        {
            GLenum mode = trace_packet.get_param_value<GLenum>(0);
            vogl_trace_ptr_value trace_indirect_ptr_value = trace_packet.get_param_ptr_value(1);
            GLsizei drawcount = trace_packet.get_param_value<GLsizei>(2);
            GLsizei stride = trace_packet.get_param_value<GLsizei>(3);

            if (m_frame_draw_counter < m_frame_draw_counter_kill_threshold)
            {
                if (trace_packet.get_param_client_memory_data_size(1) != 0)
                {
                    // TODO: Handle glMultiDrawArraysIndirect with client-side indirect data
                    process_entrypoint_error("client-side indirect commands not implemented\n");
                    return cStatusSoftFailure;
                }
                else
                {
                    GL_ENTRYPOINT(glMultiDrawArraysIndirect)(mode, (const GLvoid *)trace_indirect_ptr_value, drawcount, stride);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetBufferSubData:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                vogl_trace_ptr_value offset = trace_packet.get_param_ptr_value(1);
                vogl_trace_ptr_value size = trace_packet.get_param_ptr_value(2);
                GLvoid *pTrace_ptr = trace_packet.get_param_client_memory<GLvoid>(3);

                if (offset != static_cast<uintptr_t>(offset))
                {
                    process_entrypoint_error("offset parameter is too large (%" PRIu64 ")\n", (uint64_t)offset);
                    return cStatusHardFailure;
                }

                if ((size > cUINT32_MAX) || (size != static_cast<uintptr_t>(size)))
                {
                    process_entrypoint_error("size parameter is too large (%" PRIu64 ")\n", (uint64_t)size);
                    return cStatusHardFailure;
                }

                vogl::growable_array<uint8_t, 1024> buf(pTrace_ptr ? static_cast<uint32_t>(size) : 0);

                GL_ENTRYPOINT(glGetBufferSubData)(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), pTrace_ptr ? buf.get_ptr() : NULL);

                if ((buf.size()) && (pTrace_ptr))
                {
                    if (memcmp(buf.get_ptr(), pTrace_ptr, static_cast<size_t>(size)) != 0)
                    {
                        process_entrypoint_warning("Replay's returned data differed from trace's\n");
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetClipPlane:
        {
            if (!benchmark_mode())
            {
                GLenum plane = trace_packet.get_param_value<GLenum>(0);
                const GLdouble *pTrace_equation = trace_packet.get_param_client_memory<GLdouble>(1);

                GLdouble equation[4];
                GL_ENTRYPOINT(glGetClipPlane)(plane, pTrace_equation ? equation : NULL);

                if (pTrace_equation)
                {
                    if (memcmp(equation, pTrace_equation, sizeof(GLdouble) * 4) != 0)
                    {
                        process_entrypoint_warning("Replay's returned data differed from trace's\n");
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBufferData:
        case VOGL_ENTRYPOINT_glBufferDataARB:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            vogl_trace_ptr_value size = trace_packet.get_param_value<vogl_trace_ptr_value>(1); // GLsizeiptrARB
            const GLvoid *data = trace_packet.get_param_client_memory_ptr(2);
            uint32_t data_size = trace_packet.get_param_client_memory_data_size(2);
            GLenum usage = trace_packet.get_param_value<GLenum>(3);

            if ((data) && (static_cast<vogl_trace_ptr_value>(data_size) < size))
            {
                process_entrypoint_error("trace's data array is too small\n");
                return cStatusHardFailure;
            }

            if (size != static_cast<uintptr_t>(size))
            {
                process_entrypoint_error("size parameter is too large (%" PRIu64 ")\n", static_cast<uint64_t>(size));
                return cStatusHardFailure;
            }

            uint8_vec temp_vec;
            if (m_flags & cGLReplayerClearUnintializedBuffers)
            {
                if ((!data) && (size))
                {
                    temp_vec.resize(static_cast<uint32_t>(size));
                    data = temp_vec.get_ptr();
                }
            }

            if (entrypoint_id == VOGL_ENTRYPOINT_glBufferData)
                g_vogl_actual_gl_entrypoints.m_glBufferData(target, static_cast<GLsizeiptr>(size), data, usage);
            else
                g_vogl_actual_gl_entrypoints.m_glBufferDataARB(target, static_cast<GLsizeiptrARB>(size), data, usage);

            if (check_gl_error())
            {
                process_entrypoint_error("glBufferData() failed\n");
                return cStatusGLError;
            }

            GLuint buffer = vogl_get_bound_gl_buffer(target);
            if (buffer)
            {
                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                uint32_t i;
                for (i = 0; i < mapped_bufs.size(); i++)
                {
                    if (mapped_bufs[i].m_buffer == buffer)
                    {
                        process_entrypoint_warning("glBufferData() called on already mapped GL buffer %u, assuming GL will be unmapping it\n", buffer);

                        mapped_bufs.erase_unordered(i);
                        break;
                    }
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMapBufferARB:
        case VOGL_ENTRYPOINT_glMapBuffer:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLenum access = trace_packet.get_param_value<GLenum>(1);
            vogl_trace_ptr_value trace_result_ptr_value = trace_packet.get_return_ptr_value();

            // FIXME - must call GL even if !pTrace_result
            if (trace_result_ptr_value)
            {
                void *pMap;
                if (entrypoint_id == VOGL_ENTRYPOINT_glMapBuffer)
                    pMap = GL_ENTRYPOINT(glMapBuffer)(target, access);
                else
                    pMap = GL_ENTRYPOINT(glMapBufferARB)(target, access);

                if (!pMap)
                {
                    process_entrypoint_error("glMapBuffer succeeded during trace, but failed during replay!\n");
                    return cStatusHardFailure;
                }

                GLuint buffer = vogl_get_bound_gl_buffer(target);

                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                uint32_t i;
                for (i = 0; i < mapped_bufs.size(); i++)
                {
                    if (mapped_bufs[i].m_buffer == buffer)
                    {
                        process_entrypoint_error("Buffer %u is already mapped\n", buffer);
                        return cStatusHardFailure;
                    }
                }

                if (i == mapped_bufs.size())
                {
                    GLint length = 0;
                    GL_ENTRYPOINT(glGetBufferParameteriv)(target, GL_BUFFER_SIZE, &length);

                    vogl_mapped_buffer_desc m;
                    m.m_buffer = buffer;
                    m.m_target = target;
                    m.m_offset = 0;
                    m.m_length = length;
                    m.m_access = access;
                    m.m_range = false;
                    m.m_pPtr = pMap;
                    mapped_bufs.push_back(m);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glMapBufferRange:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            vogl_trace_ptr_value offset = trace_packet.get_param_value<vogl_trace_ptr_value>(1); // GLintptr
            vogl_trace_ptr_value length = trace_packet.get_param_value<vogl_trace_ptr_value>(2); // GLsizeiptr
            GLbitfield access = trace_packet.get_param_value<GLbitfield>(3);
            vogl_trace_ptr_value trace_result_ptr_value = trace_packet.get_return_ptr_value();

            if (offset != static_cast<uintptr_t>(offset))
            {
                process_entrypoint_error("offset parameter is too large (%" PRIu64 ")\n", static_cast<uint64_t>(offset));
                return cStatusHardFailure;
            }
            if (length != static_cast<uintptr_t>(length))
            {
                process_entrypoint_error("length parameter is too large (%" PRIu64 ")\n", static_cast<uint64_t>(length));
                return cStatusHardFailure;
            }

            // FIXME - must call GL even if !pTrace_result
            if (trace_result_ptr_value)
            {
                void *pMap = GL_ENTRYPOINT(glMapBufferRange)(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(length), access);
                if (!pMap)
                {
                    process_entrypoint_error("glMapBufferRange succeeded during trace, but failed during replay!\n");
                    return cStatusHardFailure;
                }

                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                GLuint buffer = vogl_get_bound_gl_buffer(target);
                uint32_t i;
                for (i = 0; i < mapped_bufs.size(); i++)
                {
                    if (mapped_bufs[i].m_buffer == buffer)
                    {
                        process_entrypoint_error("Buffer %u is already mapped\n", buffer);
                        return cStatusHardFailure;
                    }
                }

                if (i == mapped_bufs.size())
                {
                    vogl_mapped_buffer_desc m;
                    m.m_buffer = buffer;
                    m.m_target = target;
                    m.m_offset = offset;
                    m.m_length = length;
                    m.m_access = access;
                    m.m_range = true;
                    m.m_pPtr = pMap;
                    mapped_bufs.push_back(m);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glFlushMappedBufferRange:
        {
            // vogltrace queues up the flushes, will process them while handling the glUnmapBuffer() call
            break;
        }
        case VOGL_ENTRYPOINT_glUnmapBufferARB:
        case VOGL_ENTRYPOINT_glUnmapBuffer:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

            GLuint buffer = vogl_get_bound_gl_buffer(target);

            // FIXME - must call GL even if !buffer
            if (buffer)
            {
                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                uint32_t mapped_buffers_index;
                for (mapped_buffers_index = 0; mapped_buffers_index < mapped_bufs.size(); mapped_buffers_index++)
                    if (mapped_bufs[mapped_buffers_index].m_buffer == buffer)
                        break;
                if (mapped_buffers_index == mapped_bufs.size())
                {
                    process_entrypoint_error("Unable to find mapped buffer during unmap\n");
                    return cStatusHardFailure;
                }

                vogl_mapped_buffer_desc &map_desc = mapped_bufs[mapped_buffers_index];

                bool writable_map = false;
                bool explicit_bit = false;
                if (map_desc.m_range)
                {
                    writable_map = ((map_desc.m_access & GL_MAP_WRITE_BIT) != 0);
                    explicit_bit = (map_desc.m_access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0;
                }
                else
                {
                    writable_map = (map_desc.m_access != GL_READ_ONLY);
                }

                if (writable_map)
                {
                    const key_value_map &unmap_data = trace_packet.get_key_value_map();

                    if (explicit_bit)
                    {
                        int num_flushed_ranges = unmap_data.get_int(string_hash("flushed_ranges"));

                        for (int i = 0; i < num_flushed_ranges; i++)
                        {
                            int64_t ofs = unmap_data.get_int64(i * 4 + 0);
                            int64_t size = unmap_data.get_int64(i * 4 + 1);
                            VOGL_NOTE_UNUSED(size);
                            const uint8_vec *pData = unmap_data.get_blob(i * 4 + 2);
                            if (!pData)
                            {
                                process_entrypoint_error("Failed finding flushed range data in key value map\n");
                                return cStatusHardFailure;
                            }

                            if (ofs != static_cast<GLintptr>(ofs))
                            {
                                process_entrypoint_error("Flush offset is too large (%" PRIu64 ")\n", static_cast<uint64_t>(ofs));
                                return cStatusHardFailure;
                            }

                            VOGL_ASSERT(size == pData->size());

                            memcpy(static_cast<uint8_t *>(map_desc.m_pPtr) + ofs, pData->get_ptr(), pData->size());

                            GL_ENTRYPOINT(glFlushMappedBufferRange)(target, static_cast<GLintptr>(ofs), pData->size());
                        }
                    }
                    else
                    {
                        int64_t ofs = unmap_data.get_int64(0);
                        VOGL_NOTE_UNUSED(ofs);
                        int64_t size = unmap_data.get_int64(1);
                        VOGL_NOTE_UNUSED(size);
                        const uint8_vec *pData = unmap_data.get_blob(2);
                        if (!pData)
                        {
                            process_entrypoint_error("Failed finding mapped data in key value map\n");
                            return cStatusHardFailure;
                        }
                        else
                        {
                            memcpy(map_desc.m_pPtr, pData->get_ptr(), pData->size());
                        }
                    }
                }

                get_shared_state()->m_shadow_state.m_mapped_buffers.erase_unordered(mapped_buffers_index);
            }

            GLboolean replay_result;
            if (entrypoint_id == VOGL_ENTRYPOINT_glUnmapBuffer)
                replay_result = GL_ENTRYPOINT(glUnmapBuffer)(target);
            else
                replay_result = GL_ENTRYPOINT(glUnmapBufferARB)(target);

            if (trace_result != replay_result)
                process_entrypoint_warning("Replay glUnmapBuffer's return value differs from trace's (replay: %u vs. trace: %u)\n", replay_result, trace_result);

            break;
        }
        case VOGL_ENTRYPOINT_glBufferStorage:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            vogl_trace_ptr_value size = trace_packet.get_param_value<vogl_trace_ptr_value>(1); // GLsizeiptrARB
            const GLvoid *data = trace_packet.get_param_client_memory_ptr(2);
            uint32_t data_size = trace_packet.get_param_client_memory_data_size(2);
            GLbitfield flags = trace_packet.get_param_value<GLbitfield>(3);

            if ((data) && (static_cast<vogl_trace_ptr_value>(data_size) < size))
            {
                process_entrypoint_error("trace's data array is too small\n");
                return cStatusHardFailure;
            }

            if (size != static_cast<uintptr_t>(size))
            {
                process_entrypoint_error("size parameter is too large (%" PRIu64 ")\n", static_cast<uint64_t>(size));
                return cStatusHardFailure;
            }

            uint8_vec temp_vec;
            if (m_flags & cGLReplayerClearUnintializedBuffers)
            {
                if ((!data) && (size))
                {
                    temp_vec.resize(static_cast<uint32_t>(size));
                    data = temp_vec.get_ptr();
                }
            }

            g_vogl_actual_gl_entrypoints.m_glBufferStorage(target, static_cast<GLsizeiptr>(size), data, flags);

            if (check_gl_error())
            {
                process_entrypoint_error("glBufferStorage() failed\n");
                return cStatusGLError;
            }

            GLuint buffer = vogl_get_bound_gl_buffer(target);
            if (buffer)
            {
                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                uint32_t i;
                for (i = 0; i < mapped_bufs.size(); i++)
                {
                    if (mapped_bufs[i].m_buffer == buffer)
                    {
                        process_entrypoint_warning("glBufferStorage() called on already mapped GL buffer %u, assuming GL will be unmapping it\n", buffer);

                        mapped_bufs.erase_unordered(i);
                        break;
                    }
                }
            }

            break;
        }

        case VOGL_ENTRYPOINT_glGenVertexArrays:
        {
            if (!gen_handles(get_context_state()->m_vertex_array_objects, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glGenVertexArrays), NULL))
                return cStatusHardFailure;
            break;
        }
        case VOGL_ENTRYPOINT_glBindVertexArray:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_context_state()->m_vertex_array_objects, trace_handle);

            GL_ENTRYPOINT(glBindVertexArray)(replay_handle);
            break;
        }
        case VOGL_ENTRYPOINT_glDeleteVertexArrays:
        {
            delete_handles(get_context_state()->m_vertex_array_objects, trace_packet.get_param_value<GLsizei>(0), static_cast<const GLuint *>(trace_packet.get_param_client_memory_ptr(1)), GL_ENTRYPOINT(glDeleteVertexArrays));
            break;
        }
        case VOGL_ENTRYPOINT_glIsFramebuffer:
        case VOGL_ENTRYPOINT_glIsFramebufferEXT:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_context_state()->m_framebuffers, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result;
                if (entrypoint_id == VOGL_ENTRYPOINT_glIsFramebuffer)
                    replay_result = GL_ENTRYPOINT(glIsFramebuffer)(replay_handle);
                else
                    replay_result = GL_ENTRYPOINT(glIsFramebufferEXT)(replay_handle);

                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }

            break;
        }
        case VOGL_ENTRYPOINT_glIsBuffer:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_buffers, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsBuffer)(replay_handle);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsEnabledi:
        {
            if (!benchmark_mode())
            {
                GLenum cap = trace_packet.get_param_value<GLenum>(0);
                GLuint index = trace_packet.get_param_value<GLuint>(1);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsEnabledi)(cap, index);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsEnabled:
        {
            if (!benchmark_mode())
            {
                GLenum cap = trace_packet.get_param_value<GLenum>(0);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsEnabled)(cap);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsProgram:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsProgram)(replay_handle);

                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsQuery:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_queries, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsQuery)(replay_handle);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsShader:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_shadow_state.m_objs, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsShader)(replay_handle);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }
            break;
        }
        case VOGL_ENTRYPOINT_glIsTexture:
        case VOGL_ENTRYPOINT_glIsTextureEXT:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = trace_handle;
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                map_handle(get_shared_state()->m_shadow_state.m_textures, trace_handle, replay_handle);

                GLboolean replay_result;
                if (entrypoint_id == VOGL_ENTRYPOINT_glIsTexture)
                    replay_result = GL_ENTRYPOINT(glIsTexture)(replay_handle);
                else
                    replay_result = GL_ENTRYPOINT(glIsTextureEXT)(replay_handle);

                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }

            break;
        }
        case VOGL_ENTRYPOINT_glIsVertexArray:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_context_state()->m_vertex_array_objects, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsVertexArray)(replay_handle);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexLevelParameterfv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLint level = trace_packet.get_param_value<GLint>(1);
                GLenum pname = trace_packet.get_param_value<GLenum>(2);
                const GLfloat *pTrace_params = trace_packet.get_param_client_memory<const GLfloat>(3);
                uint32_t trace_params_size = trace_packet.get_param_client_memory_data_size(3);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLfloat, 17> replay_params(n + 1);
                replay_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC;

                GL_ENTRYPOINT(glGetTexLevelParameterfv)(target, level, pname, replay_params.get_ptr());

                VOGL_VERIFY(replay_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC);

                if (!pTrace_params)
                    process_entrypoint_warning("Trace param data is missing from packet\n");
                else if (trace_params_size != sizeof(GLfloat) * n)
                    process_entrypoint_warning("Invalid trace param data size\n");
                else if (memcmp(pTrace_params, replay_params.get_ptr(), sizeof(GLfloat) * n) != 0)
                    process_entrypoint_error("Trace's param data differs from replay's\n");
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexLevelParameteriv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLint level = trace_packet.get_param_value<GLint>(1);
                GLenum pname = trace_packet.get_param_value<GLenum>(2);
                const GLint *pTrace_params = trace_packet.get_param_client_memory<const GLint>(3);
                uint32_t trace_params_size = trace_packet.get_param_client_memory_data_size(3);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLint, 16> replay_params(n + 1);
                replay_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetTexLevelParameteriv)(target, level, pname, replay_params.get_ptr());

                VOGL_VERIFY(replay_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                if (!pTrace_params)
                    process_entrypoint_warning("Trace param data is missing from packet\n");
                else if (trace_params_size != sizeof(GLint) * n)
                    process_entrypoint_warning("Invalid trace param data size\n");
                else if (memcmp(pTrace_params, replay_params.get_ptr(), sizeof(GLint) * n) != 0)
                    process_entrypoint_error("Trace's param data differs from replay's\n");
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexParameterIiv:
        case VOGL_ENTRYPOINT_glGetTexParameteriv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLint *pTrace_params = trace_packet.get_param_client_memory<const GLint>(2);
                uint32_t trace_params_size = trace_packet.get_param_client_memory_data_size(2);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLint, 16> replay_params(n + 1);
                replay_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                if (entrypoint_id == VOGL_ENTRYPOINT_glGetTexParameterIiv)
                    GL_ENTRYPOINT(glGetTexParameterIiv)(target, pname, replay_params.get_ptr());
                else
                    GL_ENTRYPOINT(glGetTexParameteriv)(target, pname, replay_params.get_ptr());

                VOGL_VERIFY(replay_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                if (!pTrace_params)
                    process_entrypoint_warning("Trace param data is missing from packet\n");
                else if (trace_params_size != sizeof(GLint) * n)
                    process_entrypoint_warning("Invalid trace param data size\n");
                else if (memcmp(pTrace_params, replay_params.get_ptr(), sizeof(GLint) * n) != 0)
                    process_entrypoint_error("Trace's param data differs from replay's\n");
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexParameterIuiv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLuint *pTrace_params = trace_packet.get_param_client_memory<const GLuint>(2);
                uint32_t trace_params_size = trace_packet.get_param_client_memory_data_size(2);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLuint, 16> replay_params(n + 1);
                replay_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC;

                GL_ENTRYPOINT(glGetTexParameterIuiv)(target, pname, replay_params.get_ptr());

                VOGL_VERIFY(replay_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_INT_MAGIC);

                if (!pTrace_params)
                    process_entrypoint_warning("Trace param data is missing from packet\n");
                else if (trace_params_size != sizeof(GLuint) * n)
                    process_entrypoint_warning("Invalid trace param data size\n");
                else if (memcmp(pTrace_params, replay_params.get_ptr(), sizeof(GLuint) * n) != 0)
                    process_entrypoint_error("Trace's param data differs from replay's\n");
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexParameterfv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLfloat *pTrace_params = trace_packet.get_param_client_memory<const GLfloat>(2);
                uint32_t trace_params_size = trace_packet.get_param_client_memory_data_size(2);

                int n = get_gl_enums().get_pname_count(pname);
                if (n <= 0)
                {
                    process_entrypoint_error("Can't determine count of GL pname 0x%08X\n", pname);
                    return cStatusSoftFailure;
                }

                vogl::growable_array<GLfloat, 17> replay_params(n + 1);
                replay_params[n] = VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC;

                GL_ENTRYPOINT(glGetTexParameterfv)(target, pname, replay_params.get_ptr());

                VOGL_VERIFY(replay_params[n] == VOGL_GL_REPLAYER_ARRAY_OVERRUN_FLOAT_MAGIC);

                if (!pTrace_params)
                    process_entrypoint_warning("Trace param data is missing from packet\n");
                else if (trace_params_size != sizeof(GLfloat) * n)
                    process_entrypoint_warning("Invalid trace param data size\n");
                else if (memcmp(pTrace_params, replay_params.get_ptr(), sizeof(GLfloat) * n) != 0)
                    process_entrypoint_error("Trace's param data differs from replay's\n");
            }

            break;
        }

        case VOGL_ENTRYPOINT_glGetVertexAttribdv:
        {
            status = get_vertex_attrib_helper<GLdouble>(GL_ENTRYPOINT(glGetVertexAttribdv));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetVertexAttribfv:
        {
            status = get_vertex_attrib_helper<GLfloat>(GL_ENTRYPOINT(glGetVertexAttribfv));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetVertexAttribiv:
        {
            status = get_vertex_attrib_helper<GLint>(GL_ENTRYPOINT(glGetVertexAttribiv));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetVertexAttribIiv:
        {
            status = get_vertex_attrib_helper<GLint>(GL_ENTRYPOINT(glGetVertexAttribIiv));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetVertexAttribIivEXT:
        {
            status = get_vertex_attrib_helper<GLint>(GL_ENTRYPOINT(glGetVertexAttribIivEXT));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetVertexAttribIuiv:
        {
            status = get_vertex_attrib_helper<GLuint>(GL_ENTRYPOINT(glGetVertexAttribIuiv));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGetVertexAttribIuivEXT:
        {
            status = get_vertex_attrib_helper<GLuint>(GL_ENTRYPOINT(glGetVertexAttribIuivEXT));
            if (status != cStatusOK)
                return status;
            break;
        }
        case VOGL_ENTRYPOINT_glGenLists:
        {
            GLsizei range = trace_packet.get_param_value<GLsizei>(0);
            GLuint trace_base_handle = trace_packet.get_return_value<GLuint>();

            if (trace_base_handle)
            {
                check_gl_error();

                GLuint replay_base_handle = GL_ENTRYPOINT(glGenLists)(range);

                if ((check_gl_error()) || (!replay_base_handle))
                {
                    process_entrypoint_error("glGenLists() succeeded in the trace, but failed during replay!\n");
                    return cStatusHardFailure;
                }

                for (GLsizei i = 0; i < range; i++)
                {
                    GLuint trace_handle = trace_base_handle + i;
                    GLuint replay_handle = replay_base_handle + i;

                    if (!gen_handle(get_shared_state()->m_lists, trace_handle, replay_handle))
                        return cStatusHardFailure;

                    if (!get_shared_state()->m_shadow_state.m_display_lists.gen_lists(trace_handle, 1, &replay_handle))
                    {
                        process_entrypoint_warning("Failed genning list into display list shadow, trace handle %u GL handle %u\n", trace_handle, replay_handle);
                    }
                }
            }
            else
            {
                GLuint replay_base_handle = GL_ENTRYPOINT(glGenLists)(range);
                if (replay_base_handle)
                {
                    process_entrypoint_warning("glGenLists() failed in the trace, but succeeded during replay!\n");

                    GL_ENTRYPOINT(glDeleteLists)(replay_base_handle, range);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glCallList:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_lists, trace_handle);

            GL_ENTRYPOINT(glCallList)(replay_handle);

            if (!get_shared_state()->m_shadow_state.m_display_lists.parse_list_and_update_shadows(trace_handle, display_list_bind_callback, this))
            {
                process_entrypoint_warning("Failed processing display list shadow for trace handle %u GL handle %u\n", trace_handle, replay_handle);
            }

            if ((status = post_draw_call()) != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_glCallLists:
        {
            GLsizei n = trace_packet.get_param_value<GLsizei>(0);
            GLenum type = trace_packet.get_param_value<GLenum>(1);
            const GLvoid *pTrace_lists = trace_packet.get_param_client_memory<const GLvoid>(2);
            uint32_t trace_lists_size = trace_packet.get_param_client_memory_data_size(2);

            uint32_t type_size = vogl_get_gl_type_size(type);
            if (!type_size)
            {
                process_entrypoint_error("Unable to execute glCallLists, type is invalid\n");
                return cStatusSoftFailure;
            }

            if ((n) && (!pTrace_lists))
            {
                process_entrypoint_error("Unable to execute glCallLists, lists param is NULL\n");
                return cStatusSoftFailure;
            }

            if (trace_lists_size < (type_size * n))
            {
                process_entrypoint_error("Unable to execute glCallLists, lists param data size is too small in trace\n");
                return cStatusSoftFailure;
            }

            GLuint list_base = 0;
            GL_ENTRYPOINT(glGetIntegerv)(GL_LIST_BASE, reinterpret_cast<GLint *>(&list_base));

            const uint8_t *pTrace_lists_ptr = static_cast<const uint8_t *>(pTrace_lists);
            for (GLsizei i = 0; i < n; i++)
            {
                GLint trace_handle = list_base;
                switch (type)
                {
                    case GL_BYTE:
                    {
                        trace_handle += *reinterpret_cast<const signed char *>(pTrace_lists_ptr);
                        pTrace_lists_ptr++;
                        break;
                    }
                    case GL_UNSIGNED_BYTE:
                    {
                        trace_handle += *pTrace_lists_ptr;
                        pTrace_lists_ptr++;
                        break;
                    }
                    case GL_SHORT:
                    {
                        trace_handle += *reinterpret_cast<const int16_t *>(pTrace_lists_ptr);
                        pTrace_lists_ptr += sizeof(int16_t);
                        break;
                    }
                    case GL_UNSIGNED_SHORT:
                    {
                        trace_handle += *reinterpret_cast<const uint16_t *>(pTrace_lists_ptr);
                        pTrace_lists_ptr += sizeof(uint16_t);
                        break;
                    }
                    case GL_INT:
                    {
                        trace_handle += *reinterpret_cast<const int32_t *>(pTrace_lists_ptr);
                        pTrace_lists_ptr += sizeof(int32_t);
                        break;
                    }
                    case GL_UNSIGNED_INT:
                    {
                        trace_handle += *reinterpret_cast<const uint32_t *>(pTrace_lists_ptr);
                        pTrace_lists_ptr += sizeof(uint32_t);
                        break;
                    }
                    case GL_FLOAT:
                    {
                        trace_handle += static_cast<GLint>(*reinterpret_cast<const float *>(pTrace_lists_ptr));
                        pTrace_lists_ptr += sizeof(float);
                        break;
                    }
                    case GL_2_BYTES:
                    {
                        trace_handle += ((pTrace_lists_ptr[0] << 8U) + pTrace_lists_ptr[1]);
                        pTrace_lists_ptr += 2;
                        break;
                    }
                    case GL_3_BYTES:
                    {
                        trace_handle += ((pTrace_lists_ptr[0] << 16U) + (pTrace_lists_ptr[1] << 8U) + pTrace_lists_ptr[2]);
                        pTrace_lists_ptr += 3;
                        break;
                    }
                    case GL_4_BYTES:
                    {
                        trace_handle += ((pTrace_lists_ptr[0] << 24U) + (pTrace_lists_ptr[1] << 16U) + (pTrace_lists_ptr[2] << 8U) + pTrace_lists_ptr[3]);
                        pTrace_lists_ptr += 4;
                        break;
                    }
                    default:
                    {
                        process_entrypoint_error("Invalid type parameter (0x%08X)\n", type);
                        return cStatusSoftFailure;
                    }
                }

                if (trace_handle <= 0)
                {
                    process_entrypoint_error("Trace handle after adding list base is negative (%i), skipping this list index\n", trace_handle);
                }
                else
                {
                    GLuint replay_handle = map_handle(get_shared_state()->m_lists, trace_handle);
                    GL_ENTRYPOINT(glCallList)(replay_handle);

                    if (!get_shared_state()->m_shadow_state.m_display_lists.parse_list_and_update_shadows(trace_handle, display_list_bind_callback, this))
                    {
                        process_entrypoint_warning("Failed processing display list shadow for trace handle %u GL handle %u\n", trace_handle, replay_handle);
                    }
                }
            }

            if ((status = post_draw_call()) != cStatusOK)
                return status;

            break;
        }
        case VOGL_ENTRYPOINT_glDeleteLists:
        {
            GLuint trace_list = trace_packet.get_param_value<GLuint>(0);
            GLsizei range = trace_packet.get_param_value<GLsizei>(1);

            for (GLsizei i = 0; i < range; i++)
            {
                GLuint trace_handle = trace_list + i;
                delete_handles(get_shared_state()->m_lists, 1, &trace_handle, delete_list_helper);

                if (!get_shared_state()->m_shadow_state.m_display_lists.del_lists(trace_handle, 1))
                {
                    process_entrypoint_warning("Unable to delete list in display list shadow, trace handle %u\n", trace_handle);
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glIsList:
        {
            if (!benchmark_mode())
            {
                GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                GLuint replay_handle = map_handle(get_shared_state()->m_lists, trace_handle);
                GLboolean trace_result = trace_packet.get_return_value<GLboolean>();

                GLboolean replay_result = GL_ENTRYPOINT(glIsList)(replay_handle);
                if (trace_result != replay_result)
                    process_entrypoint_error("Replay's returned GLboolean differed from trace's! Replay: %u Trace: %u\n", static_cast<uint32_t>(replay_result), static_cast<uint32_t>(trace_result));
            }

            break;
        }
        case VOGL_ENTRYPOINT_glNewList:
        {
            GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_handle = map_handle(get_shared_state()->m_lists, trace_handle);
            GLenum mode = trace_packet.get_param_value<GLenum>(1);

            check_gl_error();

            GL_ENTRYPOINT(glNewList)(replay_handle, mode);

            if (!check_gl_error())
            {
                get_shared_state()->m_shadow_state.m_display_lists.new_list(trace_handle, replay_handle);

                get_context_state()->m_current_display_list_mode = mode;
                get_context_state()->m_current_display_list_handle = trace_handle;
            }

            // TODO: Check if glNewList() failed vs the replay.
            // This is important, because if the new failed during tracing but succeeded during replay then we've seriously diverged.
            // This is a bigger topic, we really should have the option of calling glGetError() during tracing after each GL call and recording the results to check for divergence.
            break;
        }
        case VOGL_ENTRYPOINT_glListBase:
        {
            GLuint base = trace_packet.get_param_value<GLuint>(0);
            GL_ENTRYPOINT(glListBase)(base);
            break;
        }
        case VOGL_ENTRYPOINT_glEndList:
        {
            GL_ENTRYPOINT(glEndList)();

            if (!get_context_state()->is_composing_display_list())
            {
                process_entrypoint_warning("glEndList() called without calling glNewList()!\n");
            }
            else
            {
                if (!get_shared_state()->m_shadow_state.m_display_lists.end_list(get_context_state()->m_current_display_list_handle))
                    process_entrypoint_warning("Failed ending display list, trace handle %u\n", get_context_state()->m_current_display_list_handle);

                get_context_state()->m_current_display_list_mode = GL_NONE;
                get_context_state()->m_current_display_list_handle = -1;
            }

            break;
        }
        case VOGL_ENTRYPOINT_glFeedbackBuffer:
        {
            GLsizei size = trace_packet.get_param_value<GLsizei>(0);
            GLenum type = trace_packet.get_param_value<GLenum>(1);

            if (static_cast<GLsizei>(m_pCur_context_state->m_feedback_buffer.size()) < size)
                m_pCur_context_state->m_feedback_buffer.resize(size);

            GL_ENTRYPOINT(glFeedbackBuffer)(size, type, m_pCur_context_state->m_feedback_buffer.get_ptr());

            break;
        }
        case VOGL_ENTRYPOINT_glSeparableFilter2D:
        {
            GLenum target = trace_packet.get_param_value<GLenum>(0);
            GLenum internalformat = trace_packet.get_param_value<GLenum>(1);
            GLsizei width = trace_packet.get_param_value<GLsizei>(2);
            GLsizei height = trace_packet.get_param_value<GLsizei>(3);
            GLenum format = trace_packet.get_param_value<GLenum>(4);
            GLenum type = trace_packet.get_param_value<GLenum>(5);

            const GLvoid *row = trace_packet.get_param_client_memory<const GLvoid>(6);
            uint32_t row_size = trace_packet.get_param_client_memory_data_size(6);
            if (row_size < vogl_get_image_size(format, type, width, 1, 1))
            {
                process_entrypoint_error("row trace array is too small\n");
                return cStatusSoftFailure;
            }

            const GLvoid *column = trace_packet.get_param_client_memory<const GLvoid>(7);
            uint32_t col_size = trace_packet.get_param_client_memory_data_size(7);
            if (col_size < vogl_get_image_size(format, type, width, 1, 1))
            {
                process_entrypoint_error("column trace array is too small\n");
                return cStatusSoftFailure;
            }

            GL_ENTRYPOINT(glSeparableFilter2D)(target, internalformat, width, height, format, type, row, column);
            break;
        }

        case VOGL_ENTRYPOINT_glNamedProgramLocalParameters4fvEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParameters4fvEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_value<GLsizei>(3), trace_packet.get_param_client_memory<const GLfloat>(4));
            break;
        }

        case VOGL_ENTRYPOINT_glNamedProgramLocalParameterI4iEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParameterI4iEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_value<GLint>(3), trace_packet.get_param_value<GLint>(4), trace_packet.get_param_value<GLint>(5), trace_packet.get_param_value<GLint>(6));
            break;
        }
        case VOGL_ENTRYPOINT_glNamedProgramLocalParameterI4ivEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParameterI4ivEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_client_memory<GLint>(3));
            break;
        }

        case VOGL_ENTRYPOINT_glNamedProgramLocalParametersI4ivEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParametersI4ivEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_value<GLsizei>(3), trace_packet.get_param_client_memory<GLint>(4));
            break;
        }
        case VOGL_ENTRYPOINT_glNamedProgramLocalParameterI4uiEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParameterI4uiEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2),
                                                               trace_packet.get_param_value<GLuint>(3), trace_packet.get_param_value<GLuint>(4),
                                                               trace_packet.get_param_value<GLuint>(5), trace_packet.get_param_value<GLuint>(6));
            break;
        }
        case VOGL_ENTRYPOINT_glNamedProgramLocalParameterI4uivEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParameterI4uivEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_client_memory<GLuint>(3));
            break;
        }
        case VOGL_ENTRYPOINT_glNamedProgramLocalParametersI4uivEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParametersI4uivEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_value<GLsizei>(3), trace_packet.get_param_client_memory<GLuint>(4));
            break;
        }
        case VOGL_ENTRYPOINT_glNamedProgramLocalParameter4fvEXT:
        {
            GLuint trace_program = trace_packet.get_param_value<GLuint>(0);
            GLuint replay_program = map_handle(get_shared_state()->m_arb_programs, trace_program);

            GL_ENTRYPOINT(glNamedProgramLocalParameter4fvEXT)(replay_program, trace_packet.get_param_value<GLenum>(1), trace_packet.get_param_value<GLuint>(2), trace_packet.get_param_client_memory<const GLfloat>(3));
            break;
        }
        case VOGL_ENTRYPOINT_glGetTexEnvfv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLfloat *pParams = trace_packet.get_param_client_memory<GLfloat>(2);

                GLfloat vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetTexEnvfv)(target, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLfloat)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexEnviv:
        {
            if (!benchmark_mode())
            {
                GLenum target = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLint *pParams = trace_packet.get_param_client_memory<GLint>(2);

                GLint vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetTexEnviv)(target, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLint)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexGendv:
        {
            if (!benchmark_mode())
            {
                GLenum coord = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLdouble *pParams = trace_packet.get_param_client_memory<GLdouble>(2);

                GLdouble vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetTexGendv)(coord, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLdouble)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexGenfv:
        {
            if (!benchmark_mode())
            {
                GLenum coord = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLfloat *pParams = trace_packet.get_param_client_memory<GLfloat>(2);

                GLfloat vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetTexGenfv)(coord, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLfloat)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexGeniv:
        {
            if (!benchmark_mode())
            {
                GLenum coord = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLint *pParams = trace_packet.get_param_client_memory<GLint>(2);

                GLint vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetTexGeniv)(coord, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLint)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetLightfv:
        {
            if (!benchmark_mode())
            {
                GLenum light = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLfloat *pParams = trace_packet.get_param_client_memory<GLfloat>(2);

                GLfloat vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetLightfv)(light, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLfloat)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetLightiv:
        {
            if (!benchmark_mode())
            {
                GLenum light = trace_packet.get_param_value<GLenum>(0);
                GLenum pname = trace_packet.get_param_value<GLenum>(1);
                const GLint *pParams = trace_packet.get_param_client_memory<GLint>(2);

                GLint vals[4] = { 0, 0, 0, 0 };

                int n = get_gl_enums().get_pname_count(pname);
                VOGL_ASSERT(n <= (int)VOGL_ARRAY_SIZE(vals));

                GL_ENTRYPOINT(glGetLightiv)(light, pname, vals);

                if (n < 0)
                {
                    process_entrypoint_error("Unable to compute element count for pname 0x%08X\n", pname);
                }
                else if (memcmp(pParams, vals, n * sizeof(GLint)) != 0)
                {
                    process_entrypoint_error("Replay divergence detected\n");
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glSelectBuffer:
        {
            GLsizei size = trace_packet.get_param_value<GLsizei>(0);

            if (m_pCur_context_state->m_select_buffer.try_resize(size))
            {
                GL_ENTRYPOINT(glSelectBuffer)(size, m_pCur_context_state->m_select_buffer.get_ptr());
            }
            else
            {
                process_entrypoint_error("Failed resizing context's select buffer\n");
            }

            break;
        }
        case VOGL_ENTRYPOINT_glClearBufferfv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glClearBufferfv;

            // TODO: Check params

            VOGL_REPLAY_CALL_GL_HELPER_glClearBufferfv;

            break;
        }
        case VOGL_ENTRYPOINT_glClearBufferiv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glClearBufferiv;

            // TODO: Check params

            VOGL_REPLAY_CALL_GL_HELPER_glClearBufferiv;

            break;
        }
        case VOGL_ENTRYPOINT_glClearBufferuiv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glClearBufferuiv;

            // TODO: Check params

            VOGL_REPLAY_CALL_GL_HELPER_glClearBufferuiv;

            break;
        }
        case VOGL_ENTRYPOINT_glTexBuffer:
        case VOGL_ENTRYPOINT_glTexBufferARB:
        case VOGL_ENTRYPOINT_glTexBufferEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexBuffer;

            buffer = map_handle(get_shared_state()->m_buffers, buffer);

            SWITCH_GL_ENTRYPOINT3_VOID(glTexBuffer, glTexBufferARB, glTexBufferEXT, target, internalformat, buffer);
            break;
        }
        case VOGL_ENTRYPOINT_glBeginConditionalRender:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBeginConditionalRender;

            id = map_handle(get_shared_state()->m_queries, id);

            VOGL_REPLAY_CALL_GL_HELPER_glBeginConditionalRender;
            break;
        }
        case VOGL_ENTRYPOINT_glEndConditionalRender:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glEndConditionalRender;

            VOGL_REPLAY_CALL_GL_HELPER_glEndConditionalRender;
            break;
        }
        case VOGL_ENTRYPOINT_glBeginTransformFeedback:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBeginTransformFeedback;

            VOGL_REPLAY_CALL_GL_HELPER_glBeginTransformFeedback;
            break;
        }
        case VOGL_ENTRYPOINT_glEndTransformFeedback:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glEndTransformFeedback;

            VOGL_REPLAY_CALL_GL_HELPER_glEndTransformFeedback;

            break;
        }
        case VOGL_ENTRYPOINT_glTransformFeedbackVaryings:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTransformFeedbackVaryings;
            VOGL_NOTE_UNUSED(pTrace_varyings);

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);

            dynamic_string_array replay_varyings(count);

            const key_value_map &key_value_map = trace_packet.get_key_value_map();
            const value_to_value_hash_map &hash_map = key_value_map.get_map();

            for (value_to_value_hash_map::const_iterator it = hash_map.begin(); it != hash_map.end(); ++it)
            {
                int key_index = it->first.get_int();

                if ((key_index >= 0) && (key_index < count))
                {
                    const dynamic_string *pName = it->second.get_string_ptr();
                    VOGL_ASSERT(pName);

                    replay_varyings[key_index] = pName ? *pName : "";
                }
                else
                {
                    VOGL_ASSERT_ALWAYS;
                }
            }

            vogl::vector<const GLchar *> str_ptrs(count);
            for (int i = 0; i < count; i++)
                str_ptrs[i] = reinterpret_cast<const GLchar *>(replay_varyings[i].get_ptr());

            GLchar *const *pReplay_varyings = (GLchar *const *)(str_ptrs.get_ptr());

            VOGL_REPLAY_CALL_GL_HELPER_glTransformFeedbackVaryings;

            break;
        }
        case VOGL_ENTRYPOINT_glUniformBufferEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glUniformBufferEXT;

            GLuint trace_program = program;

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);
            location = determine_uniform_replay_location(trace_program, location);
            buffer = map_handle(get_shared_state()->m_buffers, buffer);

            VOGL_REPLAY_CALL_GL_HELPER_glUniformBufferEXT;
            break;
        }
        case VOGL_ENTRYPOINT_glUniformBlockBinding:
        {
            // TODO: Does any of this other stuff need to be remapped?
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glUniformBlockBinding;

            program = map_handle(get_shared_state()->m_shadow_state.m_objs, program);

            VOGL_REPLAY_CALL_GL_HELPER_glUniformBlockBinding;
            break;
        }
        case VOGL_ENTRYPOINT_glFrameTerminatorGREMEDY:
        {
            // TODO - we need to hook up this extension to the tracer
            break;
        }
        case VOGL_ENTRYPOINT_glStringMarkerGREMEDY:
        {
            // TODO - we need to hook up this extension to the tracer
            break;
        }
        case VOGL_ENTRYPOINT_glBitmap:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBitmap;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(6);
                pTrace_bitmap = (const GLubyte *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glBitmap;

            break;
        }
        case VOGL_ENTRYPOINT_glColorSubTable:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glColorSubTable;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(5);
                pTrace_data = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glColorSubTable;

            break;
        }
        case VOGL_ENTRYPOINT_glColorSubTableEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glColorSubTableEXT;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(5);
                pTrace_data = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glColorSubTableEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glColorTable:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glColorTable;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(5);
                pTrace_table = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glColorTable;

            break;
        }
        case VOGL_ENTRYPOINT_glColorTableEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glColorTableEXT;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(5);
                pTrace_table = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glColorTableEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glConvolutionFilter1D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glConvolutionFilter1D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(5);
                pTrace_image = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glConvolutionFilter1D;

            break;
        }
        case VOGL_ENTRYPOINT_glConvolutionFilter2D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glConvolutionFilter2D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(6);
                pTrace_image = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glConvolutionFilter2D;

            break;
        }
        case VOGL_ENTRYPOINT_glDrawPixels:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glDrawPixels;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(4);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glDrawPixels;

            break;
        }
        case VOGL_ENTRYPOINT_glPolygonStipple:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glPolygonStipple;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(0);
                pTrace_mask = (const GLubyte *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glPolygonStipple;

            break;
        }
        case VOGL_ENTRYPOINT_glTexImage1D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexImage1D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(7);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexImage1D;

            break;
        }
        case VOGL_ENTRYPOINT_glTexImage2D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexImage2D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(8);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexImage2D;

            break;
        }
        case VOGL_ENTRYPOINT_glTexImage3D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexImage3D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(9);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexImage3D;

            break;
        }
        case VOGL_ENTRYPOINT_glTexImage3DEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexImage3DEXT;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(9);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexImage3DEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glTexSubImage1D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexSubImage1D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(6);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexSubImage1D;

            break;
        }
        case VOGL_ENTRYPOINT_glTexSubImage1DEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexSubImage1DEXT;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(6);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexSubImage1DEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glTexSubImage2D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexSubImage2D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(8);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexSubImage2D;

            break;
        }
        case VOGL_ENTRYPOINT_glTexSubImage2DEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexSubImage2DEXT;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(8);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexSubImage2DEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glTexSubImage3D:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexSubImage3D;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(10);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexSubImage3D;

            break;
        }
        case VOGL_ENTRYPOINT_glTexSubImage3DEXT:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTexSubImage3DEXT;

            if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            {
                vogl_trace_ptr_value ptr_val = trace_packet.get_param_ptr_value(10);
                pTrace_pixels = (const GLvoid *)ptr_val;
            }

            VOGL_REPLAY_CALL_GL_HELPER_glTexSubImage3DEXT;

            break;
        }
        case VOGL_ENTRYPOINT_glDebugMessageInsert:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glDebugMessageInsert;

            VOGL_REPLAY_CALL_GL_HELPER_glDebugMessageInsert;
            break;
        }
        case VOGL_ENTRYPOINT_glDebugMessageInsertARB:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glDebugMessageInsertARB;

            VOGL_REPLAY_CALL_GL_HELPER_glDebugMessageInsertARB;
            break;
        }
        case VOGL_ENTRYPOINT_glDebugMessageCallbackARB:
        {
            GL_ENTRYPOINT(glDebugMessageCallbackARB)(debug_callback_arb, (GLvoid *)m_pCur_context_state);

            break;
        }
        case VOGL_ENTRYPOINT_glDebugMessageCallback:
        {
            GL_ENTRYPOINT(glDebugMessageCallback)(debug_callback, (GLvoid *)m_pCur_context_state);

            break;
        }
        case VOGL_ENTRYPOINT_glObjectLabel:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glObjectLabel;

            switch (identifier)
            {
                case GL_BUFFER:
                {
                    name = map_handle(get_shared_state()->m_buffers, name);
                    break;
                }
                case GL_SHADER:
                case GL_PROGRAM:
                {
                    name = map_handle(get_shared_state()->m_shadow_state.m_objs, name);
                    break;
                }
                case GL_VERTEX_ARRAY:
                {
                    name = map_handle(get_shared_state()->m_vertex_array_objects, name);
                    break;
                }
                case GL_QUERY:
                {
                    name = map_handle(get_shared_state()->m_queries, name);
                    break;
                }
                case GL_SAMPLER:
                {
                    name = map_handle(get_shared_state()->m_sampler_objects, name);
                    break;
                }
                case GL_TEXTURE:
                {
                    name = map_handle(get_shared_state()->m_shadow_state.m_textures, name);
                    break;
                }
                case GL_RENDERBUFFER:
                {
                    name = map_handle(get_shared_state()->m_shadow_state.m_rbos, name);
                    break;
                }
                case GL_FRAMEBUFFER:
                {
                    name = map_handle(get_shared_state()->m_framebuffers, name);
                    break;
                }
                case GL_DISPLAY_LIST:
                {
                    name = map_handle(get_shared_state()->m_lists, name);
                    break;
                }
                case GL_PROGRAM_PIPELINE:
                {
                    name = map_handle(get_shared_state()->m_shadow_state.m_program_pipelines, name);
                    break;
                }
                case GL_TRANSFORM_FEEDBACK: // TODO: Investigate this more
                default:
                {
                    process_entrypoint_error("Unsupported object identifier 0x%X\n", identifier);
                    return cStatusSoftFailure;
                }
            }

            VOGL_REPLAY_CALL_GL_HELPER_glObjectLabel;

            break;
        }
        case VOGL_ENTRYPOINT_glObjectPtrLabel:
        {
            vogl_sync_ptr_value trace_sync = trace_packet.get_param_ptr_value(0);
            GLsizei length = trace_packet.get_param_value<GLsizei>(1);
            const GLchar *pTrace_label = reinterpret_cast<const GLchar *>(trace_packet.get_param_client_memory_ptr(2));
            GLsync replay_sync = NULL;

            if (trace_sync)
            {
                gl_sync_hash_map::const_iterator it = get_shared_state()->m_syncs.find(trace_sync);
                if (it == get_shared_state()->m_syncs.end())
                {
                    process_entrypoint_error("Failed remapping trace sync value 0x%" PRIx64 "\n", static_cast<uint64_t>(trace_sync));
                    return cStatusSoftFailure;
                }
                else
                {
                    replay_sync = it->second;
                }
            }

            GL_ENTRYPOINT(glObjectPtrLabel)(replay_sync, length, pTrace_label);

            break;
        }
        case VOGL_ENTRYPOINT_glReadPixels:
        {
            GLint x = trace_packet.get_param_value<GLint>(0);
            GLint y = trace_packet.get_param_value<GLint>(1);
            GLsizei width = trace_packet.get_param_value<GLsizei>(2);
            GLsizei height = trace_packet.get_param_value<GLsizei>(3);
            GLenum format = trace_packet.get_param_value<GLenum>(4);
            GLenum type = trace_packet.get_param_value<GLenum>(5);

            GLuint pixel_pack_buf = vogl_get_bound_gl_buffer(GL_PIXEL_PACK_BUFFER);
            if (pixel_pack_buf)
            {
                GL_ENTRYPOINT(glReadPixels)(x, y, width, height, format, type, reinterpret_cast<GLvoid *>(trace_packet.get_param_ptr_value(6)));
            }
            else
            {
                const GLvoid *trace_data = trace_packet.get_param_client_memory<const GLvoid>(6);
                uint32_t trace_data_size = trace_packet.get_param_client_memory_data_size(6);

                size_t replay_data_size = vogl_get_image_size(format, type, width, height, 1);
                if (replay_data_size != trace_data_size)
                {
                    process_entrypoint_warning("Unexpected trace data size, got %u expected %" PRIu64 "\n", trace_data_size, (uint64_t)replay_data_size);
                }
                else if (!trace_data)
                {
                    process_entrypoint_warning("Trace data is missing from packet\n");
                }

                if (replay_data_size > cUINT32_MAX)
                {
                    process_entrypoint_error("Replay data size is too large (%" PRIu64 ")!\n", (uint64_t)replay_data_size);
                    return cStatusHardFailure;
                }

                vogl::vector<uint8_t> data(static_cast<uint32_t>(replay_data_size));
                GL_ENTRYPOINT(glReadPixels)(x, y, width, height, format, type, data.get_ptr());

                if ((trace_data_size == replay_data_size) && (trace_data_size) && (trace_data))
                {
                    if (memcmp(data.get_ptr(), trace_data, trace_data_size) != 0)
                    {
                        process_entrypoint_error("Replay's returned pixel data differed from trace's!\n");
                    }
                }
                else
                {
                    process_entrypoint_warning("Replay's computed glReadPixels image size differs from traces (%u vs %u)!\n", static_cast<uint32_t>(trace_data_size), static_cast<uint32_t>(replay_data_size));
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetTexImage:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glGetTexImage;

            GLuint pixel_pack_buf = vogl_get_bound_gl_buffer(GL_PIXEL_PACK_BUFFER);
            if (pixel_pack_buf)
            {
                GLvoid *pReplay_pixels = reinterpret_cast<GLvoid *>(trace_packet.get_param_ptr_value(4));

                VOGL_REPLAY_CALL_GL_HELPER_glGetTexImage;
            }
            else
            {
                uint32_t trace_data_size = trace_packet.get_param_client_memory_data_size(4);

                size_t replay_data_size = vogl_get_tex_target_image_size(target, level, format, type);

                if (replay_data_size > cUINT32_MAX)
                {
                    process_entrypoint_error("Replay data size is too large (%" PRIu64 ")!\n", (uint64_t)replay_data_size);
                    return cStatusHardFailure;
                }

                uint8_vec data(static_cast<uint32_t>(replay_data_size));

                GLvoid *pReplay_pixels = data.get_ptr();

                VOGL_REPLAY_CALL_GL_HELPER_glGetTexImage;

                if ((trace_data_size == replay_data_size) && (trace_data_size) && (pTrace_pixels))
                {
                    if (memcmp(data.get_ptr(), pTrace_pixels, trace_data_size) != 0)
                    {
                        process_entrypoint_error("Replay's returned pixel data differed from trace's!\n");
                    }
                }
                else
                {
                    process_entrypoint_warning("Replay's computed glGetTexImage() image size differs from traces (%u vs %u)!\n", static_cast<uint32_t>(trace_data_size), static_cast<uint32_t>(replay_data_size));
                }
            }

            break;
        }
        case VOGL_ENTRYPOINT_glGetCompressedTexImage:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glGetCompressedTexImage

            VOGL_NOTE_UNUSED(pTrace_img);

            GLuint pixel_pack_buf = vogl_get_bound_gl_buffer(GL_PIXEL_PACK_BUFFER);
            if (pixel_pack_buf)
            {
                GLvoid *pReplay_img = reinterpret_cast<GLvoid *>(trace_packet.get_param_ptr_value(2));

                VOGL_REPLAY_CALL_GL_HELPER_glGetCompressedTexImage;
            }
            else
            {
                // TODO: Implement non-pixel pack buffer path, compare for divergence
            }

            break;
        }
        case VOGL_ENTRYPOINT_glBindImageTexture:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glBindImageTexture;

            GLuint trace_texture = texture;
            map_handle(get_shared_state()->m_shadow_state.m_textures, trace_texture, texture);

            VOGL_REPLAY_CALL_GL_HELPER_glBindImageTexture;

            break;
        }
        case VOGL_ENTRYPOINT_glCopyImageSubData:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glCopyImageSubData;

            GLuint trace_src_name = srcName;
            GLuint trace_dst_name = dstName;

            if (srcTarget == GL_RENDERBUFFER)
                map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_src_name, srcName);
            else
                map_handle(get_shared_state()->m_shadow_state.m_textures, trace_src_name, srcName);

            if (dstTarget == GL_RENDERBUFFER)
                map_handle(get_shared_state()->m_shadow_state.m_rbos, trace_dst_name, dstName);
            else
                map_handle(get_shared_state()->m_shadow_state.m_textures, trace_dst_name, dstName);

            VOGL_REPLAY_CALL_GL_HELPER_glCopyImageSubData;

            break;
        }
        case VOGL_ENTRYPOINT_glTextureView:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glTextureView;

            GLuint trace_texture = texture;
            GLuint trace_origtexture = origtexture;
            map_handle(get_shared_state()->m_shadow_state.m_textures, trace_texture, texture);
            map_handle(get_shared_state()->m_shadow_state.m_textures, trace_origtexture, origtexture);

            VOGL_REPLAY_CALL_GL_HELPER_glTextureView;

            break;
        }
        case VOGL_ENTRYPOINT_glCreateShaderProgramv:
        {
            VOGL_REPLAY_LOAD_PARAMS_HELPER_glCreateShaderProgramv;

            VOGL_NOTE_UNUSED(pTrace_strings);

            const key_value_map &map = m_pCur_gl_packet->get_key_value_map();

            dynamic_string_array strings;

            for (GLsizei i = 0; i < count; i++)
            {
                key_value_map::const_iterator it = map.find(i);
                if (it == map.end())
                {
                    process_entrypoint_error("Failed finding all strings in packet's key value map\n");
                    return cStatusHardFailure;
                }

                const uint8_vec *pBlob = it->second.get_blob();
                if (!pBlob)
                {
                    process_entrypoint_error("Can't convert string %i to a blob\n", i);
                    return cStatusHardFailure;
                }

                if (pBlob->find('\0') < 0)
                {
                    process_entrypoint_error("String %i is not null terminated\n", i);
                    return cStatusHardFailure;
                }

                dynamic_string str;
                str.set(reinterpret_cast<const char *>(pBlob->get_ptr()));

                strings.enlarge(1)->swap(str);
            }

            vogl::vector<const GLchar *> string_ptrs(count);
            for (int i = 0; i < count; i++)
                string_ptrs[i] = reinterpret_cast<const GLchar *>(strings[i].get_ptr());

            GLchar* const * pReplay_strings = (GLchar * const *)(string_ptrs.get_ptr());

            GLuint replay_program = VOGL_REPLAY_CALL_GL_HELPER_glCreateShaderProgramv;
            GLuint trace_program = m_pCur_gl_packet->get_return_value<GLuint>();

            if (replay_program)
            {
                if (!gen_handle(get_shared_state()->m_shadow_state.m_objs, trace_program, replay_program, VOGL_PROGRAM_OBJECT))
                    return cStatusHardFailure;

                handle_post_link_program(VOGL_ENTRYPOINT_glCreateShaderProgramv, trace_program, replay_program, type, count, pReplay_strings);
            }

            break;
        }

        case VOGL_ENTRYPOINT_glGetDebugMessageLogARB:
        case VOGL_ENTRYPOINT_glGetObjectLabel:
        case VOGL_ENTRYPOINT_glGetObjectPtrLabel:
        case VOGL_ENTRYPOINT_glGetDebugMessageLog:
        case VOGL_ENTRYPOINT_glAreTexturesResident:
        case VOGL_ENTRYPOINT_glAreTexturesResidentEXT:
        case VOGL_ENTRYPOINT_glGetActiveAtomicCounterBufferiv:
        case VOGL_ENTRYPOINT_glGetActiveAttribARB:
        case VOGL_ENTRYPOINT_glGetActiveSubroutineName:
        case VOGL_ENTRYPOINT_glGetActiveSubroutineUniformName:
        case VOGL_ENTRYPOINT_glGetActiveSubroutineUniformiv:
        case VOGL_ENTRYPOINT_glGetActiveUniformARB:
        case VOGL_ENTRYPOINT_glGetActiveUniformBlockName:
        case VOGL_ENTRYPOINT_glGetActiveUniformBlockiv:
        case VOGL_ENTRYPOINT_glGetActiveUniformName:
        case VOGL_ENTRYPOINT_glGetActiveUniformsiv:
        case VOGL_ENTRYPOINT_glGetActiveVaryingNV:
        case VOGL_ENTRYPOINT_glGetArrayObjectfvATI:
        case VOGL_ENTRYPOINT_glGetArrayObjectivATI:
        case VOGL_ENTRYPOINT_glGetAttachedObjectsARB:
        case VOGL_ENTRYPOINT_glGetAttribLocationARB:
        case VOGL_ENTRYPOINT_glGetBooleanIndexedvEXT:
        case VOGL_ENTRYPOINT_glGetBooleani_v:
        case VOGL_ENTRYPOINT_glGetBufferParameteri64v:
        case VOGL_ENTRYPOINT_glGetBufferParameterivARB:
        case VOGL_ENTRYPOINT_glGetBufferParameterui64vNV:
        case VOGL_ENTRYPOINT_glGetBufferPointervARB:
        case VOGL_ENTRYPOINT_glGetBufferSubDataARB:
        case VOGL_ENTRYPOINT_glGetClipPlanefOES:
        case VOGL_ENTRYPOINT_glGetClipPlanexOES:
        case VOGL_ENTRYPOINT_glGetColorTable:
        case VOGL_ENTRYPOINT_glGetColorTableEXT:
        case VOGL_ENTRYPOINT_glGetColorTableParameterfv:
        case VOGL_ENTRYPOINT_glGetColorTableParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetColorTableParameterfvSGI:
        case VOGL_ENTRYPOINT_glGetColorTableParameteriv:
        case VOGL_ENTRYPOINT_glGetColorTableParameterivEXT:
        case VOGL_ENTRYPOINT_glGetColorTableParameterivSGI:
        case VOGL_ENTRYPOINT_glGetColorTableSGI:
        case VOGL_ENTRYPOINT_glGetCombinerInputParameterfvNV:
        case VOGL_ENTRYPOINT_glGetCombinerInputParameterivNV:
        case VOGL_ENTRYPOINT_glGetCombinerOutputParameterfvNV:
        case VOGL_ENTRYPOINT_glGetCombinerOutputParameterivNV:
        case VOGL_ENTRYPOINT_glGetCombinerStageParameterfvNV:
        case VOGL_ENTRYPOINT_glGetCompressedMultiTexImageEXT:
        case VOGL_ENTRYPOINT_glGetCompressedTexImageARB:
        case VOGL_ENTRYPOINT_glGetCompressedTextureImageEXT:
        case VOGL_ENTRYPOINT_glGetConvolutionFilter:
        case VOGL_ENTRYPOINT_glGetConvolutionFilterEXT:
        case VOGL_ENTRYPOINT_glGetConvolutionParameterfv:
        case VOGL_ENTRYPOINT_glGetConvolutionParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetConvolutionParameteriv:
        case VOGL_ENTRYPOINT_glGetConvolutionParameterivEXT:
        case VOGL_ENTRYPOINT_glGetConvolutionParameterxvOES:
        case VOGL_ENTRYPOINT_glGetDebugMessageLogAMD:
        case VOGL_ENTRYPOINT_glGetDetailTexFuncSGIS:
        case VOGL_ENTRYPOINT_glGetDoubleIndexedvEXT:
        case VOGL_ENTRYPOINT_glGetDoublei_v:
        case VOGL_ENTRYPOINT_glGetFenceivNV:
        case VOGL_ENTRYPOINT_glGetFinalCombinerInputParameterfvNV:
        case VOGL_ENTRYPOINT_glGetFinalCombinerInputParameterivNV:
        case VOGL_ENTRYPOINT_glGetFixedvOES:
        case VOGL_ENTRYPOINT_glGetFloatIndexedvEXT:
        case VOGL_ENTRYPOINT_glGetFloati_v:
        case VOGL_ENTRYPOINT_glGetFogFuncSGIS:
        case VOGL_ENTRYPOINT_glGetFragDataIndex:
        case VOGL_ENTRYPOINT_glGetFragDataLocation:
        case VOGL_ENTRYPOINT_glGetFragDataLocationEXT:
        case VOGL_ENTRYPOINT_glGetFragmentLightfvSGIX:
        case VOGL_ENTRYPOINT_glGetFragmentLightivSGIX:
        case VOGL_ENTRYPOINT_glGetFragmentMaterialfvSGIX:
        case VOGL_ENTRYPOINT_glGetFragmentMaterialivSGIX:
        case VOGL_ENTRYPOINT_glGetFramebufferAttachmentParameteriv:
        case VOGL_ENTRYPOINT_glGetFramebufferAttachmentParameterivEXT:
        case VOGL_ENTRYPOINT_glGetFramebufferParameteriv:
        case VOGL_ENTRYPOINT_glGetFramebufferParameterivEXT:
        case VOGL_ENTRYPOINT_glGetGraphicsResetStatusARB:
        case VOGL_ENTRYPOINT_glGetHandleARB:
        case VOGL_ENTRYPOINT_glGetHistogram:
        case VOGL_ENTRYPOINT_glGetHistogramEXT:
        case VOGL_ENTRYPOINT_glGetHistogramParameterfv:
        case VOGL_ENTRYPOINT_glGetHistogramParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetHistogramParameteriv:
        case VOGL_ENTRYPOINT_glGetHistogramParameterivEXT:
        case VOGL_ENTRYPOINT_glGetHistogramParameterxvOES:
        case VOGL_ENTRYPOINT_glGetImageHandleNV:
        case VOGL_ENTRYPOINT_glGetImageTransformParameterfvHP:
        case VOGL_ENTRYPOINT_glGetImageTransformParameterivHP:
        case VOGL_ENTRYPOINT_glGetInstrumentsSGIX:
        case VOGL_ENTRYPOINT_glGetInteger64i_v:
        case VOGL_ENTRYPOINT_glGetInteger64v:
        case VOGL_ENTRYPOINT_glGetIntegerIndexedvEXT:
        case VOGL_ENTRYPOINT_glGetIntegeri_v:
        case VOGL_ENTRYPOINT_glGetIntegerui64i_vNV:
        case VOGL_ENTRYPOINT_glGetIntegerui64vNV:
        case VOGL_ENTRYPOINT_glGetInternalformati64v:
        case VOGL_ENTRYPOINT_glGetInternalformativ:
        case VOGL_ENTRYPOINT_glGetInvariantBooleanvEXT:
        case VOGL_ENTRYPOINT_glGetInvariantFloatvEXT:
        case VOGL_ENTRYPOINT_glGetInvariantIntegervEXT:
        case VOGL_ENTRYPOINT_glGetLightxOES:
        case VOGL_ENTRYPOINT_glGetListParameterfvSGIX:
        case VOGL_ENTRYPOINT_glGetListParameterivSGIX:
        case VOGL_ENTRYPOINT_glGetLocalConstantBooleanvEXT:
        case VOGL_ENTRYPOINT_glGetLocalConstantFloatvEXT:
        case VOGL_ENTRYPOINT_glGetLocalConstantIntegervEXT:
        case VOGL_ENTRYPOINT_glGetMapAttribParameterfvNV:
        case VOGL_ENTRYPOINT_glGetMapAttribParameterivNV:
        case VOGL_ENTRYPOINT_glGetMapControlPointsNV:
        case VOGL_ENTRYPOINT_glGetMapParameterfvNV:
        case VOGL_ENTRYPOINT_glGetMapParameterivNV:
        case VOGL_ENTRYPOINT_glGetMapdv:
        case VOGL_ENTRYPOINT_glGetMapfv:
        case VOGL_ENTRYPOINT_glGetMapiv:
        case VOGL_ENTRYPOINT_glGetMapxvOES:
        case VOGL_ENTRYPOINT_glGetMaterialfv:
        case VOGL_ENTRYPOINT_glGetMaterialiv:
        case VOGL_ENTRYPOINT_glGetMaterialxOES:
        case VOGL_ENTRYPOINT_glGetMinmax:
        case VOGL_ENTRYPOINT_glGetMinmaxEXT:
        case VOGL_ENTRYPOINT_glGetMinmaxParameterfv:
        case VOGL_ENTRYPOINT_glGetMinmaxParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetMinmaxParameteriv:
        case VOGL_ENTRYPOINT_glGetMinmaxParameterivEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexEnvfvEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexEnvivEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexGendvEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexGenfvEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexGenivEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexImageEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexLevelParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexLevelParameterivEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexParameterIivEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexParameterIuivEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetMultiTexParameterivEXT:
        case VOGL_ENTRYPOINT_glGetMultisamplefv:
        case VOGL_ENTRYPOINT_glGetMultisamplefvNV:
        case VOGL_ENTRYPOINT_glGetNamedBufferParameterivEXT:
        case VOGL_ENTRYPOINT_glGetNamedBufferParameterui64vNV:
        case VOGL_ENTRYPOINT_glGetNamedBufferPointervEXT:
        case VOGL_ENTRYPOINT_glGetNamedBufferSubDataEXT:
        case VOGL_ENTRYPOINT_glGetNamedFramebufferAttachmentParameterivEXT:
        case VOGL_ENTRYPOINT_glGetNamedFramebufferParameterivEXT:
        case VOGL_ENTRYPOINT_glGetNamedProgramLocalParameterIivEXT:
        case VOGL_ENTRYPOINT_glGetNamedProgramLocalParameterIuivEXT:
        case VOGL_ENTRYPOINT_glGetNamedProgramLocalParameterdvEXT:
        case VOGL_ENTRYPOINT_glGetNamedProgramLocalParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetNamedProgramStringEXT:
        case VOGL_ENTRYPOINT_glGetNamedProgramivEXT:
        case VOGL_ENTRYPOINT_glGetNamedRenderbufferParameterivEXT:
        case VOGL_ENTRYPOINT_glGetNamedStringARB:
        case VOGL_ENTRYPOINT_glGetNamedStringivARB:
        case VOGL_ENTRYPOINT_glGetObjectBufferfvATI:
        case VOGL_ENTRYPOINT_glGetObjectBufferivATI:
        case VOGL_ENTRYPOINT_glGetObjectParameterfvARB:
        case VOGL_ENTRYPOINT_glGetObjectParameterivAPPLE:
        case VOGL_ENTRYPOINT_glGetOcclusionQueryivNV:
        case VOGL_ENTRYPOINT_glGetOcclusionQueryuivNV:
        case VOGL_ENTRYPOINT_glGetPathColorGenfvNV:
        case VOGL_ENTRYPOINT_glGetPathColorGenivNV:
        case VOGL_ENTRYPOINT_glGetPathCommandsNV:
        case VOGL_ENTRYPOINT_glGetPathCoordsNV:
        case VOGL_ENTRYPOINT_glGetPathDashArrayNV:
        case VOGL_ENTRYPOINT_glGetPathLengthNV:
        case VOGL_ENTRYPOINT_glGetPathMetricRangeNV:
        case VOGL_ENTRYPOINT_glGetPathMetricsNV:
        case VOGL_ENTRYPOINT_glGetPathParameterfvNV:
        case VOGL_ENTRYPOINT_glGetPathParameterivNV:
        case VOGL_ENTRYPOINT_glGetPathSpacingNV:
        case VOGL_ENTRYPOINT_glGetPathTexGenfvNV:
        case VOGL_ENTRYPOINT_glGetPathTexGenivNV:
        case VOGL_ENTRYPOINT_glGetPerfMonitorCounterDataAMD:
        case VOGL_ENTRYPOINT_glGetPerfMonitorCounterInfoAMD:
        case VOGL_ENTRYPOINT_glGetPerfMonitorCounterStringAMD:
        case VOGL_ENTRYPOINT_glGetPerfMonitorCountersAMD:
        case VOGL_ENTRYPOINT_glGetPerfMonitorGroupStringAMD:
        case VOGL_ENTRYPOINT_glGetPerfMonitorGroupsAMD:
        case VOGL_ENTRYPOINT_glGetPixelMapfv:
        case VOGL_ENTRYPOINT_glGetPixelMapuiv:
        case VOGL_ENTRYPOINT_glGetPixelMapusv:
        case VOGL_ENTRYPOINT_glGetPixelMapxv:
        case VOGL_ENTRYPOINT_glGetPixelTexGenParameterfvSGIS:
        case VOGL_ENTRYPOINT_glGetPixelTexGenParameterivSGIS:
        case VOGL_ENTRYPOINT_glGetPixelTransformParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetPixelTransformParameterivEXT:
        case VOGL_ENTRYPOINT_glGetPointerIndexedvEXT:
        case VOGL_ENTRYPOINT_glGetPointervEXT:
        case VOGL_ENTRYPOINT_glGetPolygonStipple:
        case VOGL_ENTRYPOINT_glGetProgramBinary:
        case VOGL_ENTRYPOINT_glGetProgramEnvParameterIivNV:
        case VOGL_ENTRYPOINT_glGetProgramEnvParameterIuivNV:
        case VOGL_ENTRYPOINT_glGetProgramEnvParameterdvARB:
        case VOGL_ENTRYPOINT_glGetProgramEnvParameterfvARB:
        case VOGL_ENTRYPOINT_glGetProgramInterfaceiv:
        case VOGL_ENTRYPOINT_glGetProgramLocalParameterIivNV:
        case VOGL_ENTRYPOINT_glGetProgramLocalParameterIuivNV:
        case VOGL_ENTRYPOINT_glGetProgramLocalParameterdvARB:
        case VOGL_ENTRYPOINT_glGetProgramLocalParameterfvARB:
        case VOGL_ENTRYPOINT_glGetProgramNamedParameterdvNV:
        case VOGL_ENTRYPOINT_glGetProgramNamedParameterfvNV:
        case VOGL_ENTRYPOINT_glGetProgramParameterdvNV:
        case VOGL_ENTRYPOINT_glGetProgramParameterfvNV:
        case VOGL_ENTRYPOINT_glGetProgramPipelineInfoLog:
        case VOGL_ENTRYPOINT_glGetProgramPipelineiv:
        case VOGL_ENTRYPOINT_glGetProgramResourceIndex:
        case VOGL_ENTRYPOINT_glGetProgramResourceLocation:
        case VOGL_ENTRYPOINT_glGetProgramResourceLocationIndex:
        case VOGL_ENTRYPOINT_glGetProgramResourceName:
        case VOGL_ENTRYPOINT_glGetProgramResourceiv:
        case VOGL_ENTRYPOINT_glGetProgramStageiv:
        case VOGL_ENTRYPOINT_glGetProgramStringARB:
        case VOGL_ENTRYPOINT_glGetProgramStringNV:
        case VOGL_ENTRYPOINT_glGetProgramSubroutineParameteruivNV:
        case VOGL_ENTRYPOINT_glGetProgramivNV:
        case VOGL_ENTRYPOINT_glGetQueryIndexediv:
        case VOGL_ENTRYPOINT_glGetQueryObjecti64vEXT:
        case VOGL_ENTRYPOINT_glGetQueryObjectui64vEXT:
        case VOGL_ENTRYPOINT_glGetQueryiv:
        case VOGL_ENTRYPOINT_glGetQueryivARB:
        case VOGL_ENTRYPOINT_glGetSamplerParameterIiv:
        case VOGL_ENTRYPOINT_glGetSamplerParameterIuiv:
        case VOGL_ENTRYPOINT_glGetSamplerParameterfv:
        case VOGL_ENTRYPOINT_glGetSamplerParameteriv:
        case VOGL_ENTRYPOINT_glGetSeparableFilter:
        case VOGL_ENTRYPOINT_glGetSeparableFilterEXT:
        case VOGL_ENTRYPOINT_glGetShaderPrecisionFormat:
        case VOGL_ENTRYPOINT_glGetShaderSource:
        case VOGL_ENTRYPOINT_glGetShaderSourceARB:
        case VOGL_ENTRYPOINT_glGetSharpenTexFuncSGIS:
        case VOGL_ENTRYPOINT_glGetSubroutineIndex:
        case VOGL_ENTRYPOINT_glGetSubroutineUniformLocation:
        case VOGL_ENTRYPOINT_glGetSynciv:
        case VOGL_ENTRYPOINT_glGetTexBumpParameterfvATI:
        case VOGL_ENTRYPOINT_glGetTexBumpParameterivATI:
        case VOGL_ENTRYPOINT_glGetTexEnvxvOES:
        case VOGL_ENTRYPOINT_glGetTexFilterFuncSGIS:
        case VOGL_ENTRYPOINT_glGetTexGenxvOES:
        case VOGL_ENTRYPOINT_glGetTexLevelParameterxvOES:
        case VOGL_ENTRYPOINT_glGetTexParameterIivEXT:
        case VOGL_ENTRYPOINT_glGetTexParameterIuivEXT:
        case VOGL_ENTRYPOINT_glGetTexParameterPointervAPPLE:
        case VOGL_ENTRYPOINT_glGetTexParameterxvOES:
        case VOGL_ENTRYPOINT_glGetTextureHandleNV:
        case VOGL_ENTRYPOINT_glGetTextureImageEXT:
        case VOGL_ENTRYPOINT_glGetTextureLevelParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetTextureLevelParameterivEXT:
        case VOGL_ENTRYPOINT_glGetTextureParameterIivEXT:
        case VOGL_ENTRYPOINT_glGetTextureParameterIuivEXT:
        case VOGL_ENTRYPOINT_glGetTextureParameterfvEXT:
        case VOGL_ENTRYPOINT_glGetTextureParameterivEXT:
        case VOGL_ENTRYPOINT_glGetTextureSamplerHandleNV:
        case VOGL_ENTRYPOINT_glGetTrackMatrixivNV:
        case VOGL_ENTRYPOINT_glGetTransformFeedbackVarying:
        case VOGL_ENTRYPOINT_glGetTransformFeedbackVaryingEXT:
        case VOGL_ENTRYPOINT_glGetTransformFeedbackVaryingNV:
        case VOGL_ENTRYPOINT_glGetUniformBlockIndex:
        case VOGL_ENTRYPOINT_glGetUniformBufferSizeEXT:
        case VOGL_ENTRYPOINT_glGetUniformIndices:
        case VOGL_ENTRYPOINT_glGetUniformOffsetEXT:
        case VOGL_ENTRYPOINT_glGetUniformSubroutineuiv:
        case VOGL_ENTRYPOINT_glGetUniformdv:
        case VOGL_ENTRYPOINT_glGetUniformfv:
        case VOGL_ENTRYPOINT_glGetUniformfvARB:
        case VOGL_ENTRYPOINT_glGetUniformi64vNV:
        case VOGL_ENTRYPOINT_glGetUniformiv:
        case VOGL_ENTRYPOINT_glGetUniformivARB:
        case VOGL_ENTRYPOINT_glGetUniformui64vNV:
        case VOGL_ENTRYPOINT_glGetUniformuiv:
        case VOGL_ENTRYPOINT_glGetUniformuivEXT:
        case VOGL_ENTRYPOINT_glGetVariantArrayObjectfvATI:
        case VOGL_ENTRYPOINT_glGetVariantArrayObjectivATI:
        case VOGL_ENTRYPOINT_glGetVariantBooleanvEXT:
        case VOGL_ENTRYPOINT_glGetVariantFloatvEXT:
        case VOGL_ENTRYPOINT_glGetVariantIntegervEXT:
        case VOGL_ENTRYPOINT_glGetVariantPointervEXT:
        case VOGL_ENTRYPOINT_glGetVaryingLocationNV:
        case VOGL_ENTRYPOINT_glGetVertexAttribArrayObjectfvATI:
        case VOGL_ENTRYPOINT_glGetVertexAttribArrayObjectivATI:
        case VOGL_ENTRYPOINT_glGetVertexAttribLdv:
        case VOGL_ENTRYPOINT_glGetVertexAttribLdvEXT:
        case VOGL_ENTRYPOINT_glGetVertexAttribLi64vNV:
        case VOGL_ENTRYPOINT_glGetVertexAttribLui64vNV:
        case VOGL_ENTRYPOINT_glGetVertexAttribPointerv:
        case VOGL_ENTRYPOINT_glGetVertexAttribPointervARB:
        case VOGL_ENTRYPOINT_glGetVertexAttribPointervNV:
        case VOGL_ENTRYPOINT_glGetVertexAttribdvARB:
        case VOGL_ENTRYPOINT_glGetVertexAttribdvNV:
        case VOGL_ENTRYPOINT_glGetVertexAttribfvARB:
        case VOGL_ENTRYPOINT_glGetVertexAttribfvNV:
        case VOGL_ENTRYPOINT_glGetVertexAttribivARB:
        case VOGL_ENTRYPOINT_glGetVertexAttribivNV:
        case VOGL_ENTRYPOINT_glGetVideoCaptureStreamdvNV:
        case VOGL_ENTRYPOINT_glGetVideoCaptureStreamfvNV:
        case VOGL_ENTRYPOINT_glGetVideoCaptureStreamivNV:
        case VOGL_ENTRYPOINT_glGetVideoCaptureivNV:
        case VOGL_ENTRYPOINT_glGetVideoi64vNV:
        case VOGL_ENTRYPOINT_glGetVideoivNV:
        case VOGL_ENTRYPOINT_glGetVideoui64vNV:
        case VOGL_ENTRYPOINT_glGetVideouivNV:
        case VOGL_ENTRYPOINT_glGetnColorTableARB:
        case VOGL_ENTRYPOINT_glGetnCompressedTexImageARB:
        case VOGL_ENTRYPOINT_glGetnConvolutionFilterARB:
        case VOGL_ENTRYPOINT_glGetnHistogramARB:
        case VOGL_ENTRYPOINT_glGetnMapdvARB:
        case VOGL_ENTRYPOINT_glGetnMapfvARB:
        case VOGL_ENTRYPOINT_glGetnMapivARB:
        case VOGL_ENTRYPOINT_glGetnMinmaxARB:
        case VOGL_ENTRYPOINT_glGetnPixelMapfvARB:
        case VOGL_ENTRYPOINT_glGetnPixelMapuivARB:
        case VOGL_ENTRYPOINT_glGetnPixelMapusvARB:
        case VOGL_ENTRYPOINT_glGetnPolygonStippleARB:
        case VOGL_ENTRYPOINT_glGetnSeparableFilterARB:
        case VOGL_ENTRYPOINT_glGetnTexImageARB:
        case VOGL_ENTRYPOINT_glGetnUniformdvARB:
        case VOGL_ENTRYPOINT_glGetnUniformfvARB:
        case VOGL_ENTRYPOINT_glGetnUniformivARB:
        case VOGL_ENTRYPOINT_glGetnUniformuivARB:
        case VOGL_ENTRYPOINT_glIsBufferARB:
        case VOGL_ENTRYPOINT_glIsEnabledIndexedEXT:
        case VOGL_ENTRYPOINT_glIsQueryARB:
        case VOGL_ENTRYPOINT_glIsSync:
        case VOGL_ENTRYPOINT_glPrioritizeTextures:
        case VOGL_ENTRYPOINT_glPrioritizeTexturesEXT:
        {
            if (!(g_vogl_entrypoint_descs[entrypoint_id].m_flags & cGLEFPrintedUnimplementedWarning))
            {
                process_entrypoint_warning("TODO: Implement glGet() function %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);

                g_vogl_entrypoint_descs[entrypoint_id].m_flags |= cGLEFPrintedUnimplementedWarning;
            }
            break;
        }
        default:
        {
            if (g_vogl_entrypoint_descs[entrypoint_id].m_is_whitelisted)
                process_entrypoint_error("Unhandled GL function %s. This function is marked as whitelisted but was not handled!\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);
            else
                process_entrypoint_error("Unhandled GL function %s. This function needs to be added to the whitelist!\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);
            return cStatusSoftFailure;
        }
    }

    m_last_processed_call_counter = trace_packet.get_call_counter();

    if (!m_pCur_context_state->m_inside_gl_begin)
    {
        if (check_gl_error())
            return cStatusGLError;
    }

    if (vogl_is_draw_entrypoint(entrypoint_id) || vogl_is_clear_entrypoint(entrypoint_id) || (entrypoint_id == VOGL_ENTRYPOINT_glBitmap))
    {
        if ((status = post_draw_call()) != cStatusOK)
            return status;
    }

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::snapshot_backbuffer
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::snapshot_backbuffer()
{
    VOGL_FUNC_TRACER

    if (!m_pCur_context_state)
    {
        vogl_warning_printf("Can't take snapshot without an active context\n");
        return;
    }

    uint32_t recorded_width = m_pWindow->get_width();
    uint32_t recorded_height = m_pWindow->get_height();

    uint32_t width = 0, height = 0;
    m_pWindow->get_actual_dimensions(width, height);

    VOGL_ASSERT((recorded_width == width) && (recorded_height == height));
    VOGL_NOTE_UNUSED(recorded_width);
    VOGL_NOTE_UNUSED(recorded_height);

    m_screenshot_buffer.resize(width * height * 3);

    bool success = vogl_copy_buffer_to_image(m_screenshot_buffer.get_ptr(), m_screenshot_buffer.size(), width, height, GL_RGB, GL_UNSIGNED_BYTE, false, 0, GL_BACK);
    if (!success)
    {
        process_entrypoint_error("Failed calling glReadPixels() to take screenshot\n");
    }

    if (m_flags & cGLReplayerDumpScreenshots)
    {
        size_t png_size = 0;
        void *pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(m_screenshot_buffer.get_ptr(), width, height, 3, &png_size, 1, true);

        dynamic_string screenshot_filename(cVarArg, "%s_%07u.png", m_screenshot_prefix.get_ptr(), m_total_swaps);
        if (!file_utils::write_buf_to_file(screenshot_filename.get_ptr(), pPNG_data, png_size))
        {
            process_entrypoint_error("Failed writing PNG screenshot to file %s\n", screenshot_filename.get_ptr());
        }
        else
        {
            vogl_message_printf("Wrote screenshot to file %s\n", screenshot_filename.get_ptr());
        }

        mz_free(pPNG_data);
    }

    if ((m_flags & cGLReplayerDumpBackbufferHashes) || (m_flags & cGLReplayerHashBackbuffer))
    {
        uint64_t backbuffer_crc64;

        if (m_flags & cGLReplayerSumHashing)
        {
            backbuffer_crc64 = calc_sum64(m_screenshot_buffer.get_ptr(), m_screenshot_buffer.size());
        }
        else
        {
            backbuffer_crc64 = calc_crc64(CRC64_INIT, m_screenshot_buffer.get_ptr(), m_screenshot_buffer.size());
        }

        vogl_verbose_printf("Frame %u hash: 0x%016" PRIX64 "\n", m_frame_index, backbuffer_crc64);

        if (m_backbuffer_hash_filename.has_content())
        {
            FILE *pFile = vogl_fopen(m_backbuffer_hash_filename.get_ptr(), "a");
            if (!pFile)
                vogl_error_printf("Failed writing to backbuffer hash file %s\n", m_backbuffer_hash_filename.get_ptr());
            else
            {
                vogl_fprintf(pFile, "0x%016" PRIX64 "\n", cast_val_to_uint64(backbuffer_crc64));
                vogl_fclose(pFile);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::is_valid_handle
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::replay_to_trace_handle_remapper::is_valid_handle(vogl_namespace_t handle_namespace, uint64_t replay_handle)
{
    VOGL_FUNC_TRACER

    if (!replay_handle)
        return 0;

    uint32_t replay_handle32 = static_cast<uint32_t>(replay_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_context_state()->m_vertex_array_objects.search_table_for_value_get_count(replay_handle32) != 0);
        }
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_context_state()->m_framebuffers.search_table_for_value_get_count(replay_handle32) != 0);
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_shared_state()->m_shadow_state.m_textures.contains_inv(replay_handle32) != 0);
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_shared_state()->m_shadow_state.m_rbos.contains_inv(replay_handle32) != 0);
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_shared_state()->m_queries.search_table_for_value_get_count(replay_handle32) != 0);
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_shared_state()->m_sampler_objects.search_table_for_value_get_count(replay_handle32) != 0);
        }
        case VOGL_NAMESPACE_PROGRAMS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return m_replayer.get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle32) == VOGL_PROGRAM_OBJECT;
        }
        case VOGL_NAMESPACE_SHADERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return m_replayer.get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle32) == VOGL_SHADER_OBJECT;
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return m_replayer.get_shared_state()->m_buffers.search_table_for_value_get_count(replay_handle32) != 0;
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            GLsync replay_sync = vogl_handle_to_sync(replay_handle);
            return m_replayer.get_shared_state()->m_syncs.search_table_for_value_get_count(replay_sync) != 0;
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return m_replayer.get_shared_state()->m_arb_programs.search_table_for_value_get_count(replay_handle32) != 0;
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            return (m_replayer.get_shared_state()->m_shadow_state.m_program_pipelines.contains_inv(replay_handle32) != 0);
        }
        default:
            break;
    }

    VOGL_VERIFY(0);

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::remap_handle
//----------------------------------------------------------------------------------------------------------------------
uint64_t vogl_gl_replayer::replay_to_trace_handle_remapper::remap_handle(vogl_namespace_t handle_namespace, uint64_t replay_handle)
{
    VOGL_FUNC_TRACER

    if (!replay_handle)
        return 0;

    uint32_t replay_handle32 = static_cast<uint32_t>(replay_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            if (remap_replay_to_trace_handle(m_replayer.get_context_state()->m_vertex_array_objects, replay_handle32))
                return replay_handle32;
            break;
        }
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            if (remap_replay_to_trace_handle(m_replayer.get_context_state()->m_framebuffers, replay_handle32))
                return replay_handle32;
            break;
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);

            uint32_t trace_handle = replay_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_textures.map_inv_handle_to_handle(replay_handle32, trace_handle))
                return trace_handle;

            break;
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            GLuint trace_handle = replay_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_rbos.map_inv_handle_to_handle(replay_handle32, trace_handle))
                return trace_handle;

            break;
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            if (remap_replay_to_trace_handle(m_replayer.get_shared_state()->m_queries, replay_handle32))
                return replay_handle32;
            break;
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            if (remap_replay_to_trace_handle(m_replayer.get_shared_state()->m_sampler_objects, replay_handle32))
                return replay_handle32;
            break;
        }
        case VOGL_NAMESPACE_PROGRAMS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            VOGL_ASSERT(m_replayer.get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle32) == VOGL_PROGRAM_OBJECT);
            GLuint trace_handle = replay_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_objs.map_inv_handle_to_handle(replay_handle32, trace_handle))
                return trace_handle;
            break;
        }
        case VOGL_NAMESPACE_SHADERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            VOGL_ASSERT(m_replayer.get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle32) == VOGL_SHADER_OBJECT);
            GLuint trace_handle = replay_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_objs.map_inv_handle_to_handle(replay_handle32, trace_handle))
                return trace_handle;
            break;
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            if (remap_replay_to_trace_handle(m_replayer.get_shared_state()->m_buffers, replay_handle32))
                return replay_handle32;
            break;
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            GLsync replay_sync = vogl_handle_to_sync(replay_handle);

            gl_sync_hash_map::const_iterator it(m_replayer.get_shared_state()->m_syncs.search_table_for_value(replay_sync));
            if (it != m_replayer.get_shared_state()->m_syncs.end())
            {
                VOGL_ASSERT(it->second == replay_sync);
                return it->first;
            }

            break;
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            if (remap_replay_to_trace_handle(m_replayer.get_shared_state()->m_arb_programs, replay_handle32))
                return replay_handle32;
            break;
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            VOGL_ASSERT(replay_handle32 == replay_handle);
            GLuint trace_handle = replay_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_program_pipelines.map_inv_handle_to_handle(replay_handle32, trace_handle))
                return trace_handle;
            break;
        }
        default:
        {
            break;
        }
    }

    // This is BAD.
    vogl_error_printf("Failed remapping handle %" PRIu64 " in namespace %s. This is either a handle shadowing bug, or this object was deleted while it was still bound on another context or attached to an object.\n", replay_handle, vogl_get_namespace_name(handle_namespace));

    VOGL_ASSERT_ALWAYS;

    return replay_handle;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::replay_to_trace_handle_remapper::remap_location
//----------------------------------------------------------------------------------------------------------------------
int32_t vogl_gl_replayer::replay_to_trace_handle_remapper::remap_location(uint32_t replay_program, int32_t replay_location)
{
    VOGL_FUNC_TRACER

    if ((!replay_program) || (replay_location < 0))
        return replay_location;

    GLuint trace_program = static_cast<GLuint>(remap_handle(VOGL_NAMESPACE_PROGRAMS, replay_program));

    glsl_program_hash_map::const_iterator it(m_replayer.get_shared_state()->m_glsl_program_hash_map.find(trace_program));
    if (it != m_replayer.get_shared_state()->m_glsl_program_hash_map.end())
    {
        const glsl_program_state &state = it->second;

        uniform_location_hash_map::const_iterator loc_it(state.m_uniform_locations.search_table_for_value(replay_location));
        if (loc_it != state.m_uniform_locations.end())
            return loc_it->first;
    }

    vogl_warning_printf("Failed remapping location %i of program %u\n", replay_location, replay_program);

    return replay_location;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::replay_to_trace_handle_remapper::determine_from_object_target
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::replay_to_trace_handle_remapper::determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t replay_handle, GLenum &target)
{
    VOGL_FUNC_TRACER

    target = GL_NONE;

    uint32_t handle32 = static_cast<uint32_t>(replay_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(handle32 == replay_handle);
            if (!m_replayer.get_shared_state()->m_shadow_state.m_textures.contains_inv(handle32))
                return false;

            target = m_replayer.get_shared_state()->m_shadow_state.m_textures.get_target_inv(handle32);
            return true;
        }
        default:
            break;
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::replay_to_trace_handle_remapper::determine_to_object_target
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::replay_to_trace_handle_remapper::determine_to_object_target(vogl_namespace_t handle_namespace, uint64_t trace_handle, GLenum &target)
{
    VOGL_FUNC_TRACER

    target = GL_NONE;

    uint32_t handle32 = static_cast<uint32_t>(trace_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(handle32 == trace_handle);
            if (!m_replayer.get_shared_state()->m_shadow_state.m_textures.contains(handle32))
                return false;

            target = m_replayer.get_shared_state()->m_shadow_state.m_textures.get_target(handle32);
            return true;
        }
        default:
            break;
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::determine_used_program_handles
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::determine_used_program_handles(const vogl_trace_packet_array &trim_packets, vogl_handle_hash_set &replay_program_handles)
{
    VOGL_FUNC_TRACER

    trace_to_replay_handle_remapper trace_to_replay_remapper(*this);

#if 0
	GLint cur_program_handle = 0;
	GL_ENTRYPOINT(glGetIntegerv)(GL_CURRENT_PROGRAM, &cur_program_handle);
	check_gl_error();

	if (cur_program_handle)
		replay_program_handles.insert(cur_program_handle);
#endif

    // Scan for bound programs on all contexts in this sharegroup
    context_state *pContext_shareroot = m_pCur_context_state->m_pShared_state;
    for (context_hash_map::iterator it = m_contexts.begin(); it != m_contexts.end(); ++it)
    {
        context_state *pContext = it->second;
        if (pContext->m_pShared_state == pContext_shareroot)
        {
            if (pContext->m_cur_replay_program)
                replay_program_handles.insert(pContext->m_cur_replay_program);
        }
    }

    for (uint32_t packet_index = 0; packet_index < trim_packets.size(); packet_index++)
    {
        if (trim_packets.get_packet_type(packet_index) != cTSPTGLEntrypoint)
            continue;

        const uint8_vec &packet_buf = trim_packets.get_packet_buf(packet_index);

        // Important note: This purposesly doesn't process ctype packets, because they don't really do anything and I'm going to be redesigning the ctype/entrypoint stuff anyway so they are always processed after SOF.
        if (!m_temp2_gl_packet.deserialize(packet_buf.get_ptr(), packet_buf.size(), true))
            return false;

        GLuint trace_handle = 0;
        bool refers_to_program = vogl_does_packet_refer_to_program(m_temp2_gl_packet, trace_handle);
        if (!refers_to_program)
            continue;
        if (!trace_handle)
            continue;

        // trace_handle is conservative, and it's fine if it can't be actually mapped into the replay space because as we process packets it's possible for the trace to create/delete program handles
        // All that matters is that we're conservative (i.e. we can't filter out any programs that are actually referenced in this trace packet array).
        if (!trace_to_replay_remapper.is_valid_handle(VOGL_NAMESPACE_PROGRAMS, trace_handle))
            continue;

        uint64_t replay_handle = trace_to_replay_remapper.remap_handle(VOGL_NAMESPACE_PROGRAMS, trace_handle);
        if (!replay_handle)
            continue;

        VOGL_ASSERT(utils::is_32bit(replay_handle));

        replay_program_handles.insert(static_cast<uint32_t>(replay_handle));
    }

    vogl_message_printf("Found %u actually referenced program handles\n", replay_program_handles.size());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::fill_replay_handle_hash_set
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::fill_replay_handle_hash_set(vogl_handle_hash_set &replay_handle_hash, const gl_handle_hash_map &trace_to_replay_hash)
{
    VOGL_FUNC_TRACER

    replay_handle_hash.reset();
    replay_handle_hash.reserve(trace_to_replay_hash.size());
    for (gl_handle_hash_map::const_iterator it = trace_to_replay_hash.begin(); it != trace_to_replay_hash.end(); ++it)
    {
        // Insert replay handles into destination hash table
        bool success = replay_handle_hash.insert(it->second).second;
        VOGL_ASSERT(success);
        VOGL_NOTE_UNUSED(success);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::snapshot_state
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_state_snapshot *vogl_gl_replayer::snapshot_state(const vogl_trace_packet_array *pTrim_packets, bool optimize_snapshot)
{
    VOGL_FUNC_TRACER

    timed_scope ts(VOGL_FUNCTION_INFO_CSTR);

    vogl_gl_state_snapshot *pSnapshot = vogl_new(vogl_gl_state_snapshot);

    vogl_message_printf("Beginning capture: width %u, height %u, trace context 0x%" PRIx64 ", frame index %u, last call counter %" PRIu64 ", at frame boundary: %u\n",
                       m_pWindow->get_width(), m_pWindow->get_height(), m_cur_trace_context, m_frame_index, m_last_parsed_call_counter, m_at_frame_boundary);

    if (!pSnapshot->begin_capture(m_pWindow->get_width(), m_pWindow->get_height(), m_cur_trace_context, m_frame_index, m_last_parsed_call_counter, m_at_frame_boundary))
    {
        vogl_error_printf("Failed beginning capture\n");

        vogl_delete(pSnapshot);
        pSnapshot = NULL;

        return NULL;
    }

    vogl_client_side_array_desc_vec client_side_vertex_attrib_ptrs;
    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(m_client_side_vertex_attrib_data); i++)
        client_side_vertex_attrib_ptrs.push_back(vogl_client_side_array_desc(reinterpret_cast<vogl_trace_ptr_value>(m_client_side_vertex_attrib_data[i].get_ptr()), m_client_side_vertex_attrib_data[i].size()));

    vogl_client_side_array_desc_vec client_side_array_ptrs;
    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(m_client_side_array_data); i++)
        client_side_array_ptrs.push_back(vogl_client_side_array_desc(reinterpret_cast<vogl_trace_ptr_value>(m_client_side_array_data[i].get_ptr()), m_client_side_array_data[i].size()));

    vogl_client_side_array_desc_vec client_side_texcoord_ptrs;
    for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(m_client_side_texcoord_data); i++)
        client_side_texcoord_ptrs.push_back(vogl_client_side_array_desc(reinterpret_cast<vogl_trace_ptr_value>(m_client_side_texcoord_data[i].get_ptr()), m_client_side_texcoord_data[i].size()));

    pSnapshot->add_client_side_array_ptrs(client_side_vertex_attrib_ptrs, client_side_array_ptrs, client_side_texcoord_ptrs);

    vogl_verbose_printf("Capturing %u context(s)\n", m_contexts.size());

    context_hash_map::iterator it;
    for (it = m_contexts.begin(); it != m_contexts.end(); ++it)
    {
        context_state *pContext_state = it->second;

        if (pContext_state->m_deleted)
        {
            vogl_error_printf("Sharelist root context 0x%" PRIx64 " was destroyed - this scenario is not yet supported for state snapshotting.\n", cast_val_to_uint64(it->first));
            break;
        }

        vogl_capture_context_params temp_shadow_state;
        vogl_capture_context_params *pShadow_state = &temp_shadow_state;

        if (pContext_state->m_has_been_made_current)
        {
            status_t status = switch_contexts(it->first);
            if (status != cStatusOK)
            {
                vogl_error_printf("Failed switching to trace context 0x%" PRIX64 ", capture failed\n", cast_val_to_uint64(it->first));
                break;
            }

            VOGL_ASSERT(m_pCur_context_state == pContext_state);

            if (m_pCur_context_state->m_inside_gl_begin)
            {
                vogl_warning_printf("Trace context 0x%" PRIX64 " is inside a glBegin, which is not fully supported for state capturing. Capture will continue but will not be replayable.\n", cast_val_to_uint64(it->first));
                pSnapshot->set_is_restorable(false);
            }


            // Init the shadow state needed by the snapshot code.
            if (!m_pCur_context_state->is_root_context())
            {
                // Only fill in non-shared state.
                fill_replay_handle_hash_set(pShadow_state->m_framebuffers, get_context_state()->m_framebuffers);
                fill_replay_handle_hash_set(pShadow_state->m_vaos, get_context_state()->m_vertex_array_objects);
            }
            else
            {
                pShadow_state = &m_pCur_context_state->m_shadow_state;

                pShadow_state->m_query_targets = get_shared_state()->m_query_targets;

                fill_replay_handle_hash_set(pShadow_state->m_samplers, get_shared_state()->m_sampler_objects);
                fill_replay_handle_hash_set(pShadow_state->m_framebuffers, get_context_state()->m_framebuffers);
                fill_replay_handle_hash_set(pShadow_state->m_vaos, get_context_state()->m_vertex_array_objects);

                // Buffer targets
                pShadow_state->m_buffer_targets.reset();
                for (gl_handle_hash_map::const_iterator buf_it = get_shared_state()->m_buffers.begin(); buf_it != get_shared_state()->m_buffers.end(); ++buf_it)
                {
                    GLuint trace_handle = buf_it->first;
                    GLuint replay_handle = buf_it->second;

                    gl_handle_hash_map::const_iterator target_it = get_shared_state()->m_buffer_targets.find(trace_handle);
                    if (target_it == get_shared_state()->m_buffer_targets.end())
                    {
                        vogl_error_printf("Unable to find buffer trace handle 0x%X GL handle 0x%X in buffer target map! This should not happen!\n", trace_handle, replay_handle);
                        continue;
                    }
                    GLenum target = target_it->second;

                    pShadow_state->m_buffer_targets.insert(replay_handle, target);
                }

                // Syncs
                pShadow_state->m_syncs.reset();
                pShadow_state->m_syncs.reserve(get_shared_state()->m_syncs.size());
                for (gl_sync_hash_map::const_iterator sync_it = get_shared_state()->m_syncs.begin(); sync_it != get_shared_state()->m_syncs.end(); ++sync_it)
                {
                    bool success = pShadow_state->m_syncs.insert(vogl_sync_to_handle(sync_it->second)).second;
                    VOGL_ASSERT(success);
                    VOGL_NOTE_UNUSED(success);
                }

                // Program handles filter
                pShadow_state->m_filter_program_handles = false;
                pShadow_state->m_program_handles_filter.reset();

#if 0
				// TODO: The program optimization code works, but we also need to delete any unused shaders (or shaders that have been marked as deleted but are referred to by optimized out programs).
				// This is an optimization issue, and we're concentrating on correctness right now so let's figure this out later.
				if ((pTrim_packets) && (optimize_snapshot))
				{
                    if (!determine_used_program_handles(*pTrim_packets, pShadow_state->m_program_handles_filter))
					{
						vogl_warning_printf("Failed determining used program handles\n");
						pShadow_state->m_program_handles_filter.clear();
					}
					else
					{
						pShadow_state->m_filter_program_handles = true;
					}
				}
#else
                VOGL_NOTE_UNUSED(optimize_snapshot);
                VOGL_NOTE_UNUSED(pTrim_packets);
#endif

                // ARB program targets
                pShadow_state->m_arb_program_targets.reset();
                for (gl_handle_hash_map::const_iterator arb_prog_it = get_shared_state()->m_arb_program_targets.begin(); arb_prog_it != get_shared_state()->m_arb_program_targets.end(); ++arb_prog_it)
                {
                    GLuint trace_handle = arb_prog_it->first;
                    GLuint replay_handle = get_shared_state()->m_arb_programs.value(trace_handle);
                    if ((!trace_handle) || (!replay_handle))
                    {
                        VOGL_ASSERT_ALWAYS;
                        continue;
                    }

                    GLenum target = arb_prog_it->second;
                    pShadow_state->m_arb_program_targets.insert(replay_handle, target);
                }
                // TODO: How to deal with Program pipeline objects

                // Deal with any currently mapped buffers.
                vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

                pShadow_state->m_mapped_buffers = mapped_bufs;

                if (mapped_bufs.size())
                {
                    vogl_warning_printf("%u buffer(s) are currently mapped, these will be temporarily unmapped in order to snapshot them and then remapped\n", m_pCur_context_state->m_shadow_state.m_mapped_buffers.size());

                    for (uint32_t i = 0; i < mapped_bufs.size(); i++)
                    {
                        vogl_mapped_buffer_desc &desc = mapped_bufs[i];

                        GLuint prev_handle = vogl_get_bound_gl_buffer(desc.m_target);

                        GL_ENTRYPOINT(glBindBuffer)(desc.m_target, desc.m_buffer);
                        VOGL_CHECK_GL_ERROR;

                        GL_ENTRYPOINT(glUnmapBuffer)(desc.m_target);
                        VOGL_CHECK_GL_ERROR;

                        desc.m_pPtr = NULL;

                        GL_ENTRYPOINT(glBindBuffer)(desc.m_target, prev_handle);
                        VOGL_CHECK_GL_ERROR;
                    }
                }

            } // if (!m_pCur_context_state->is_root_context())

        } // if (pContext_state->m_has_been_made_current)

        if (!pSnapshot->capture_context(pContext_state->m_context_desc, pContext_state->m_context_info, m_replay_to_trace_remapper, *pShadow_state))
        {
            vogl_error_printf("Failed capturing trace context 0x%" PRIX64 ", capture failed\n", static_cast<uint64_t>(it->first));
            break;
        }

        if ((pContext_state->m_has_been_made_current) && (m_pCur_context_state->is_root_context()))
        {
            vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;

            // Now remap any mapped buffers
            for (uint32_t i = 0; i < mapped_bufs.size(); i++)
            {
                vogl_mapped_buffer_desc &desc = mapped_bufs[i];

                GLuint prev_handle = vogl_get_bound_gl_buffer(desc.m_target);

                GL_ENTRYPOINT(glBindBuffer)(desc.m_target, desc.m_buffer);
                VOGL_CHECK_GL_ERROR;

                if (desc.m_range)
                {
                    desc.m_pPtr = GL_ENTRYPOINT(glMapBufferRange)(desc.m_target, static_cast<GLintptr>(desc.m_offset), static_cast<GLsizeiptr>(desc.m_length), desc.m_access);
                    VOGL_CHECK_GL_ERROR;
                }
                else
                {
                    desc.m_pPtr = GL_ENTRYPOINT(glMapBuffer)(desc.m_target, desc.m_access);
                    VOGL_CHECK_GL_ERROR;
                }

                GL_ENTRYPOINT(glBindBuffer)(desc.m_target, prev_handle);
                VOGL_CHECK_GL_ERROR;

            }
        }
    }

    if ((it == m_contexts.end()) && (pSnapshot->end_capture()))
    {
        vogl_message_printf("Capture succeeded\n");
    }
    else
    {
        vogl_error_printf("Capture failed\n");

        vogl_delete(pSnapshot);
        pSnapshot = NULL;
    }

    return pSnapshot;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::reset_state
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::reset_state()
{
    VOGL_FUNC_TRACER

    // Purposely does NOT destroy the cached snapshots

    destroy_pending_snapshot();
    destroy_contexts();

    m_pending_make_current_packet.clear();
    m_pending_window_resize_width = 0;
    m_pending_window_resize_height = 0;
    m_pending_window_resize_attempt_counter = 0;

    m_frame_index = 0;
    m_last_parsed_call_counter = -1;
    m_last_processed_call_counter = -1;
    m_at_frame_boundary = true;

    m_cur_trace_context = 0;
    m_cur_replay_context = 0;
    m_pCur_context_state = NULL;
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::is_valid_handle
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::trace_to_replay_handle_remapper::is_valid_handle(vogl_namespace_t handle_namespace, uint64_t from_handle)
{
    VOGL_FUNC_TRACER

    if (!from_handle)
        return false;

    uint32_t from_handle32 = static_cast<uint32_t>(from_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_context_state()->m_vertex_array_objects.contains(from_handle32);
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_shadow_state.m_textures.contains(from_handle32);
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_sampler_objects.contains(from_handle32);
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_buffers.contains(from_handle32);
        }
        case VOGL_NAMESPACE_SHADERS:
        case VOGL_NAMESPACE_PROGRAMS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_shadow_state.m_objs.contains(from_handle32);
        }
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_context_state()->m_framebuffers.contains(from_handle32);
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_shadow_state.m_rbos.contains(from_handle32);
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_queries.contains(from_handle32);
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            return m_replayer.get_shared_state()->m_syncs.contains(from_handle);
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            return m_replayer.get_shared_state()->m_arb_programs.contains(from_handle32);
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            return m_replayer.get_shared_state()->m_shadow_state.m_program_pipelines.contains(from_handle32);
        }
        default:
            break;
    }

    VOGL_VERIFY(0);

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::remap_handle
//----------------------------------------------------------------------------------------------------------------------
uint64_t vogl_gl_replayer::trace_to_replay_handle_remapper::remap_handle(vogl_namespace_t handle_namespace, uint64_t from_handle)
{
    VOGL_FUNC_TRACER

    if (!from_handle)
        return from_handle;

    uint32_t from_handle32 = static_cast<uint32_t>(from_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_context_state()->m_vertex_array_objects.value(from_handle32, from_handle32);
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(from_handle32 == from_handle);

            uint32_t replay_handle = from_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_textures.map_handle_to_inv_handle(from_handle32, replay_handle))
                return replay_handle;
            break;
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_sampler_objects.value(from_handle32, from_handle32);
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_buffers.value(from_handle32, from_handle32);
        }
        case VOGL_NAMESPACE_SHADERS:
        case VOGL_NAMESPACE_PROGRAMS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);

            GLuint replay_handle = from_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_objs.map_handle_to_inv_handle(from_handle32, replay_handle))
                return replay_handle;
            break;
        }
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_context_state()->m_framebuffers.value(from_handle32, from_handle32);
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT(from_handle32 == from_handle);

            GLuint replay_handle = from_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_rbos.map_handle_to_inv_handle(from_handle32, replay_handle))
                return replay_handle;
            // TODO : return 0 instead of break & assert here as temporary workaround for an apparent snapshot capture bug
            //   On Dota2 trace trying to restore RBs bound to FBs that don't appear to have been saved into the trace.
            return 0;
            //break;
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            return m_replayer.get_shared_state()->m_queries.value(from_handle32, from_handle32);
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            return vogl_sync_to_handle(m_replayer.get_shared_state()->m_syncs.value(from_handle, vogl_handle_to_sync(from_handle)));
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            return m_replayer.get_shared_state()->m_arb_programs.value(from_handle32, from_handle32);
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            VOGL_ASSERT(from_handle32 == from_handle);
            GLuint replay_handle = from_handle32;
            if (m_replayer.get_shared_state()->m_shadow_state.m_program_pipelines.map_handle_to_inv_handle(from_handle32, replay_handle))
                    return replay_handle;
            break;
        }
        default:
        {
            break;
        }
    }

    VOGL_ASSERT_ALWAYS;

    vogl_error_printf("Failed remapping handle %" PRIu64 " in namespace %s.\n", from_handle, vogl_get_namespace_name(handle_namespace));

    return from_handle;
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::remap_location
//----------------------------------------------------------------------------------------------------------------------
int32_t vogl_gl_replayer::trace_to_replay_handle_remapper::remap_location(uint32_t trace_program, int32_t from_location)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(trace_program);

    // restoring declares, but doesn't need to remap
    VOGL_ASSERT_ALWAYS;

    return from_location;
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::remap_vertex_attrib_ptr
//----------------------------------------------------------------------------------------------------------------------
vogl_trace_ptr_value vogl_gl_replayer::trace_to_replay_handle_remapper::remap_vertex_attrib_ptr(uint32_t index, vogl_trace_ptr_value ptr_val)
{
    VOGL_FUNC_TRACER

    if (!ptr_val)
        return ptr_val;

    VOGL_ASSERT(index < VOGL_ARRAY_SIZE(m_replayer.m_client_side_vertex_attrib_data));
    if (!m_replayer.m_client_side_vertex_attrib_data[index].size())
    {
        m_replayer.m_client_side_vertex_attrib_data[index].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
    }

    return reinterpret_cast<vogl_trace_ptr_value>(m_replayer.m_client_side_vertex_attrib_data[index].get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::remap_vertex_array_ptr
//----------------------------------------------------------------------------------------------------------------------
vogl_trace_ptr_value vogl_gl_replayer::trace_to_replay_handle_remapper::remap_vertex_array_ptr(vogl_client_side_array_desc_id_t id, uint32_t index, vogl_trace_ptr_value ptr_val)
{
    VOGL_FUNC_TRACER

    VOGL_ASSERT(id < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS);

    if (!ptr_val)
        return ptr_val;

    if (id == vogl_texcoord_pointer_array_id)
    {
        VOGL_ASSERT(index < VOGL_ARRAY_SIZE(m_replayer.m_client_side_texcoord_data));

        if (!m_replayer.m_client_side_texcoord_data[index].size())
        {
            m_replayer.m_client_side_texcoord_data[index].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
        }

        return reinterpret_cast<vogl_trace_ptr_value>(m_replayer.m_client_side_texcoord_data[index].get_ptr());
    }
    else
    {
        VOGL_ASSERT(index < VOGL_ARRAY_SIZE(m_replayer.m_client_side_array_data));

        if (!m_replayer.m_client_side_array_data[id].size())
        {
            m_replayer.m_client_side_array_data[id].resize(VOGL_MAX_CLIENT_SIDE_VERTEX_ARRAY_SIZE);
        }

        return reinterpret_cast<vogl_trace_ptr_value>(m_replayer.m_client_side_array_data[id].get_ptr());
    }
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::declare_handle
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::trace_to_replay_handle_remapper::declare_handle(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle, GLenum target)
{
    VOGL_FUNC_TRACER

    if ((!from_handle) || (!to_handle))
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    uint32_t from_handle32 = static_cast<uint32_t>(from_handle);
    uint32_t to_handle32 = static_cast<uint32_t>(to_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_context_state()->m_vertex_array_objects.insert(from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_textures.update(from_handle32, to_handle32, target))
                vogl_warning_printf("Failed inserting trace texture %u GL texture %u handle into texture handle tracker!\n", from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_sampler_objects.insert(from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_buffers.insert(from_handle32, to_handle32);
            m_replayer.get_shared_state()->m_buffer_targets.insert(from_handle32, target);
            break;
        }
        case VOGL_NAMESPACE_SHADERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_objs.insert(from_handle32, to_handle32, VOGL_SHADER_OBJECT))
                vogl_warning_printf("Failed inserting trace shader handle %u GL handle %u into object handle tracker!\n", from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_PROGRAMS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_objs.insert(from_handle32, to_handle32, VOGL_PROGRAM_OBJECT))
                vogl_warning_printf("Failed inserting trace program handle %u GL handle %u into object handle tracker!\n", from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_context_state()->m_framebuffers.insert(from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_rbos.insert(from_handle32, to_handle32, GL_NONE))
                vogl_warning_printf("Failed inserting trace RBO handle %u GL handle %u into RBO handle tracker!\n", from_handle32, to_handle32);
            break;
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_queries.insert(from_handle32, to_handle32);
            m_replayer.get_shared_state()->m_query_targets[to_handle32] = target;
            break;
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            m_replayer.get_shared_state()->m_syncs.insert(from_handle, vogl_handle_to_sync(to_handle));
            break;
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            m_replayer.get_shared_state()->m_arb_programs.insert(from_handle32, to_handle32);
            m_replayer.get_shared_state()->m_arb_program_targets.insert(from_handle32, target);
            break;
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_program_pipelines.insert(from_handle32, to_handle32, GL_NONE))
                vogl_warning_printf("Failed inserting trace SSO handle %u GL handle %u into SSO handle tracker!\n", from_handle32, to_handle32);
            break;
        }
        default:
        {
            VOGL_VERIFY(0);
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// trace_to_replay_handle_remapper::delete_handle_and_object
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::trace_to_replay_handle_remapper::delete_handle_and_object(vogl_namespace_t handle_namespace, uint64_t from_handle, uint64_t to_handle)
{
    VOGL_FUNC_TRACER

    if ((!from_handle) || (!to_handle))
    {
        VOGL_ASSERT_ALWAYS;
        return;
    }

    uint32_t from_handle32 = static_cast<uint32_t>(from_handle);
    uint32_t to_handle32 = static_cast<uint32_t>(to_handle);
    VOGL_NOTE_UNUSED(to_handle32);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_VERTEX_ARRAYS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_context_state()->m_vertex_array_objects.erase(from_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_textures.erase(from_handle32))
                vogl_warning_printf("Failed deleting trace texture handle %u GL handle %u from texture handle tracker!\n", from_handle32, to_handle32);

            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_SAMPLERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_sampler_objects.erase(from_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_BUFFERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_buffers.erase(from_handle32);
            m_replayer.get_shared_state()->m_buffer_targets.erase(from_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_SHADERS:
        {
            m_replayer.handle_delete_shader(from_handle32);
            break;
        }
        case VOGL_NAMESPACE_PROGRAMS:
        {
            m_replayer.handle_delete_program(from_handle32);
            break;
        }
        case VOGL_NAMESPACE_FRAMEBUFFERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_context_state()->m_framebuffers.erase(from_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_RENDER_BUFFERS:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));

            if (!m_replayer.get_shared_state()->m_shadow_state.m_rbos.erase(from_handle32))
                vogl_warning_printf("Failed deleting trace RBO handle %u GL handle %u from RBO handle tracker!\n", from_handle32, to_handle32);

            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_QUERIES:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_queries.erase(from_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_SYNCS:
        {
            m_replayer.get_shared_state()->m_syncs.erase(from_handle);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_PROGRAM_ARB:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            m_replayer.get_shared_state()->m_arb_programs.erase(from_handle32);
            m_replayer.get_shared_state()->m_arb_program_targets.erase(from_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        case VOGL_NAMESPACE_PIPELINES:
        {
            VOGL_ASSERT((from_handle32 == from_handle) && (to_handle32 == to_handle));
            if (!m_replayer.get_shared_state()->m_shadow_state.m_program_pipelines.erase(from_handle32))
                vogl_warning_printf("Failed deleting trace SSO handle %u GL handle %u from SSO handle tracker!\n", from_handle32, to_handle32);
            vogl_destroy_gl_object(handle_namespace, to_handle);
            break;
        }
        default:
        {
            VOGL_VERIFY(0);
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::trace_to_replay_handle_remapper::declare_location
// from_location - trace location
// to_location - replay/GL location
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::trace_to_replay_handle_remapper::declare_location(uint32_t from_program_handle, uint32_t to_program_handle, int32_t from_location, int32_t to_location)
{
    VOGL_FUNC_TRACER

    GLuint check_replay_handle = 0;
    VOGL_ASSERT(m_replayer.get_shared_state()->m_shadow_state.m_objs.map_handle_to_inv_handle(from_program_handle, check_replay_handle));
    VOGL_ASSERT(check_replay_handle == to_program_handle);
    VOGL_NOTE_UNUSED(check_replay_handle);

    VOGL_NOTE_UNUSED(to_program_handle);

    glsl_program_hash_map::iterator it(m_replayer.get_shared_state()->m_glsl_program_hash_map.insert(from_program_handle).first);

    glsl_program_state &prog_state = it->second;

    VOGL_ASSERT(!prog_state.m_uniform_locations.contains(from_location));

    prog_state.m_uniform_locations.insert(from_location, to_location);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::trace_to_replay_handle_remapper::determine_from_object_target
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::trace_to_replay_handle_remapper::determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t trace_handle, GLenum &target)
{
    VOGL_FUNC_TRACER

    target = GL_NONE;

    uint32_t handle32 = static_cast<uint32_t>(trace_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(handle32 == trace_handle);
            if (!m_replayer.get_shared_state()->m_shadow_state.m_textures.contains(handle32))
                return false;

            target = m_replayer.get_shared_state()->m_shadow_state.m_textures.get_target(handle32);
            return true;
        }
        default:
            break;
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::trace_to_replay_handle_remapper::determine_to_object_target
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::trace_to_replay_handle_remapper::determine_to_object_target(vogl_namespace_t handle_namespace, uint64_t replay_handle, GLenum &target)
{
    VOGL_FUNC_TRACER

    target = GL_NONE;

    uint32_t handle32 = static_cast<uint32_t>(replay_handle);

    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_TEXTURES:
        {
            VOGL_ASSERT(handle32 == replay_handle);
            if (!m_replayer.get_shared_state()->m_shadow_state.m_textures.contains_inv(handle32))
                return false;

            target = m_replayer.get_shared_state()->m_shadow_state.m_textures.get_target_inv(handle32);
            return true;
        }
        default:
            break;
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::restore_objects
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::restore_objects(
    vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_state, vogl_gl_object_state_type state_type,
    vogl_const_gl_object_state_ptr_vec &objects_to_delete)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(snapshot);

    vogl_verbose_printf("Restoring %s objects\n", get_gl_object_state_type_str(state_type));

    vogl::timer tm;
    if (m_flags & cGLReplayerVerboseMode)
        tm.start();

    const vogl_gl_object_state_ptr_vec &object_ptrs = context_state.get_objects();

    uint32_t n = 0;

    for (uint32_t i = 0; i < object_ptrs.size(); i++)
    {
        const vogl_gl_object_state *pState_obj = object_ptrs[i];

        if (pState_obj->get_type() != state_type)
            continue;

        GLuint64 restore_handle = 0;
        if (!pState_obj->restore(m_pCur_context_state->m_context_info, trace_to_replay_remapper, restore_handle))
        {
            vogl_error_printf("Failed restoring object type %s object index %u trace handle 0x%" PRIX64 " restore handle 0x%" PRIX64 "\n", get_gl_object_state_type_str(state_type), i, (uint64_t)pState_obj->get_snapshot_handle(), (uint64_t)restore_handle);
            return cStatusHardFailure;
        }
        n++;

        if (pState_obj->get_marked_for_deletion())
        {
            objects_to_delete.push_back(pState_obj);
        }

        VOGL_ASSERT(trace_to_replay_remapper.remap_handle(pState_obj->get_handle_namespace(), pState_obj->get_snapshot_handle()) == restore_handle);

        switch (pState_obj->get_type())
        {
            case cGLSTQuery:
            {
                const vogl_query_state *pQuery = static_cast<const vogl_query_state *>(pState_obj);

                VOGL_ASSERT(restore_handle <= cUINT32_MAX);
                get_shared_state()->m_query_targets[static_cast<GLuint>(restore_handle)] = pQuery->get_target();

                break;
            }
            case cGLSTProgram:
            {
                const vogl_program_state *pProg = static_cast<const vogl_program_state *>(pState_obj);

                if (pProg->has_link_time_snapshot())
                {
                    vogl_program_state link_snapshot(*pProg->get_link_time_snapshot());
                    if (!link_snapshot.remap_handles(trace_to_replay_remapper))
                    {
                        vogl_error_printf("Failed remapping handles in program link time snapshot, object index %u trace handle 0x%" PRIX64 " restore handle 0x%" PRIX64 "\n", i, (uint64_t)pState_obj->get_snapshot_handle(), (uint64_t)restore_handle);
                    }
                    else
                    {
                        get_shared_state()->m_shadow_state.m_linked_programs.add_snapshot(static_cast<uint32_t>(restore_handle), link_snapshot);
                    }
                }

                if ((n & 255) == 255)
                    vogl_verbose_printf("Restored %u programs\n", n);
                break;
            }
            case cGLSTBuffer:
            {
                const vogl_buffer_state *pBuf = static_cast<const vogl_buffer_state *>(pState_obj);

                // Check if the buffer was mapped during the snapshot, if so remap it and record the ptr in the replayer's context shadow.
                if (pBuf->get_is_mapped())
                {
                    vogl_mapped_buffer_desc map_desc;
                    map_desc.m_buffer = static_cast<GLuint>(restore_handle);
                    map_desc.m_target = pBuf->get_target();
                    map_desc.m_offset = pBuf->get_map_ofs();
                    map_desc.m_length = pBuf->get_map_size();
                    map_desc.m_access = pBuf->get_map_access();
                    map_desc.m_range = pBuf->get_is_map_range();

                    GLuint prev_handle = vogl_get_bound_gl_buffer(map_desc.m_target);

                    GL_ENTRYPOINT(glBindBuffer)(map_desc.m_target, map_desc.m_buffer);
                    VOGL_CHECK_GL_ERROR;

                    uint32_t access = map_desc.m_access & ~(GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
                    if (map_desc.m_range)
                    {
                        map_desc.m_pPtr = GL_ENTRYPOINT(glMapBufferRange)(map_desc.m_target, static_cast<GLintptr>(map_desc.m_offset), static_cast<GLintptr>(map_desc.m_length), access);
                        VOGL_CHECK_GL_ERROR;
                    }
                    else
                    {
                        map_desc.m_pPtr = GL_ENTRYPOINT(glMapBuffer)(map_desc.m_target, access);
                        VOGL_CHECK_GL_ERROR;
                    }

                    GL_ENTRYPOINT(glBindBuffer)(map_desc.m_target, prev_handle);
                    VOGL_CHECK_GL_ERROR;

                    vogl_mapped_buffer_desc_vec &mapped_bufs = get_shared_state()->m_shadow_state.m_mapped_buffers;
                    mapped_bufs.push_back(map_desc);
                }

                break;
            }
            default:
                break;
        }
    }

    if (m_flags & cGLReplayerVerboseMode)
    {
        tm.stop();
        vogl_verbose_printf("Restore took %f secs\n", tm.get_elapsed_secs());
        vogl_verbose_printf("Finished restoring %u %s objects\n", n, get_gl_object_state_type_str(state_type));
    }

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_xfont_cache
//----------------------------------------------------------------------------------------------------------------------
class vogl_xfont_cache
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_xfont_cache);

public:
    vogl_xfont_cache(Display *dpy)
        : m_dpy(dpy)
    {
        VOGL_FUNC_TRACER
    }

    ~vogl_xfont_cache()
    {
        VOGL_FUNC_TRACER

        clear();
    }

    void clear()
    {
        VOGL_FUNC_TRACER
        #if (VOGL_PLATFORM_HAS_GLX)
            for (xfont_map::iterator it = m_xfonts.begin(); it != m_xfonts.end(); ++it)
                XFreeFont(m_dpy, it->second);
            m_xfonts.clear();
        #else
            VOGL_ASSERT(!"impl vogl_xfont_cache::clear");
        #endif
    }

    XFontStruct *get_or_create(const char *pName)
    {
        VOGL_FUNC_TRACER
        #if (VOGL_PLATFORM_HAS_GLX)
            XFontStruct **ppXFont = m_xfonts.find_value(pName);
            if (ppXFont)
                return *ppXFont;

            XFontStruct *pXFont = XLoadQueryFont(m_dpy, pName);
            if (pXFont)
                m_xfonts.insert(pName, pXFont);

            return pXFont;

        #else
            VOGL_ASSERT(!"impl vogl_xfont_cache::clear");
            return NULL;
        #endif

    }

private:
    Display *m_dpy;

    typedef vogl::map<dynamic_string, XFontStruct *> xfont_map;
    xfont_map m_xfonts;
};

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::restore_display_lists
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::restore_display_lists(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(trace_to_replay_remapper);
    VOGL_NOTE_UNUSED(snapshot);

    VOGL_ASSERT(m_cur_trace_context);

    check_gl_error();

    const vogl_display_list_state &disp_lists = context_snapshot.get_display_list_state();

    if (!disp_lists.size())
        return cStatusOK;

    vogl_verbose_printf("Recreating %u display lists\n", disp_lists.get_display_list_map().size());

    #if VOGL_PLATFORM_HAS_SDL
        // TODO: Implement this with SDL. Remove the X11 path.
    #elif VOGL_PLATFORM_HAS_X11
        vogl_xfont_cache xfont_cache(m_pWindow->get_display());
    #endif

    const vogl_display_list_map &disp_list_map = disp_lists.get_display_list_map();

    for (vogl_display_list_map::const_iterator it = disp_list_map.begin(); it != disp_list_map.end(); ++it)
    {
        GLuint trace_handle = it->first;
        const vogl_display_list &disp_list = it->second;

        if (!trace_handle)
        {
            VOGL_ASSERT_ALWAYS;
            continue;
        }

        GLuint replay_handle = GL_ENTRYPOINT(glGenLists)(1);
        if (check_gl_error() || !replay_handle)
            goto handle_failure;

        if (disp_list.is_valid())
        {
            if (disp_list.is_xfont())
            {
                #if VOGL_PLATFORM_HAS_SDL
                    // TODO: Implement this with SDL. Remove the X11 path.
                #elif (VOGL_PLATFORM_HAS_X11)
                    XFontStruct *pXFont = xfont_cache.get_or_create(disp_list.get_xfont_name().get_ptr());
                    if (!pXFont)
                    {
                        vogl_error_printf("Unable to load XFont \"%s\", can't recreate trace display list %u!\n", disp_list.get_xfont_name().get_ptr(), trace_handle);
                    }
                    else
                    {
                        GL_ENTRYPOINT(glXUseXFont)(pXFont->fid, disp_list.get_xfont_glyph(), 1, replay_handle);
                    }
                #else
                    vogl_error_printf("Cannot create display lists that use X11 Fonts on non-X11 platforms.\n");
                #endif
            }
            else
            {
                GL_ENTRYPOINT(glNewList)(replay_handle, GL_COMPILE);

                if (check_gl_error() || !replay_handle)
                {
                    GL_ENTRYPOINT(glDeleteLists)(replay_handle, 1);
                    check_gl_error();

                    goto handle_failure;
                }

                const vogl_trace_packet_array &packets = disp_list.get_packets();

                for (uint32_t packet_index = 0; packet_index < packets.size(); packet_index++)
                {
                    if (packets.get_packet_type(packet_index) != cTSPTGLEntrypoint)
                    {
                        vogl_error_printf("Unexpected display list packet type %u, packet index %u, can't fully recreate trace display list %u!\n", packets.get_packet_type(packet_index), packet_index, trace_handle);
                        continue;
                    }

                    const uint8_vec &packet_buf = packets.get_packet_buf(packet_index);

                    if (!m_temp2_gl_packet.deserialize(packet_buf, true))
                    {
                        vogl_error_printf("Failed deserializing display list at packet index %u, can't fully recreate trace display list %u!\n", packet_index, trace_handle);
                        continue;
                    }

                    vogl_trace_gl_entrypoint_packet &gl_entrypoint_packet = m_temp2_gl_packet.get_entrypoint_packet();

                    gl_entrypoint_packet.m_context_handle = m_cur_trace_context;

                    if (m_flags & cGLReplayerDebugMode)
                        dump_trace_gl_packet_debug_info(gl_entrypoint_packet);

                    int64_t prev_parsed_call_counter = m_last_parsed_call_counter;
                    int64_t prev_processed_call_counter = m_last_processed_call_counter;
                    m_last_parsed_call_counter = gl_entrypoint_packet.m_call_counter;
                    m_last_processed_call_counter = gl_entrypoint_packet.m_call_counter;
                    bool prev_at_frame_boundary = m_at_frame_boundary;

                    const vogl_trace_packet *pPrev_gl_packet = m_pCur_gl_packet;

                    m_pCur_gl_packet = &m_temp2_gl_packet;

                    vogl_gl_replayer::status_t status = process_gl_entrypoint_packet_internal(m_temp2_gl_packet);

                    m_pCur_gl_packet = pPrev_gl_packet;

                    m_last_parsed_call_counter = prev_parsed_call_counter;
                    m_last_processed_call_counter = prev_processed_call_counter;
                    m_at_frame_boundary = prev_at_frame_boundary;

                    if (status != cStatusOK)
                    {
                        vogl_error_printf("Failed recreating display list at packet index %u, can't fully recreate trace display list %u!\n", packet_index, trace_handle);
                        continue;
                    }
                }

                // TODO: Set context state because we're currently generating a display list!
                if (disp_list.is_generating())
                {
                    VOGL_ASSERT_ALWAYS;
                }

                GL_ENTRYPOINT(glEndList)();
                check_gl_error();
            }
        }

        get_shared_state()->m_lists.insert(trace_handle, replay_handle);

        if (!get_shared_state()->m_shadow_state.m_display_lists.define_list(trace_handle, replay_handle, disp_list))
        {
            vogl_error_printf("Failed adding display list trace handle %u GL handle %u into display list shadow!\n", trace_handle, replay_handle);
        }
    }

    check_gl_error();

    vogl_verbose_printf("Done recreating display lists\n");

    return cStatusOK;

handle_failure:
    return cStatusHardFailure;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::restore_general_state
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::restore_general_state(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(snapshot);

    vogl_general_context_state::vogl_persistent_restore_state persistent_restore_state;
    persistent_restore_state.m_pSelect_buffer = &m_pCur_context_state->m_select_buffer;

    if (!context_snapshot.get_general_state().restore(m_pCur_context_state->m_context_info, trace_to_replay_remapper, persistent_restore_state))
        return cStatusHardFailure;

    if (!m_pCur_context_state->m_context_info.is_core_profile())
    {
        if (context_snapshot.get_texenv_state().is_valid())
        {
            if (!context_snapshot.get_texenv_state().restore(m_pCur_context_state->m_context_info))
                return cStatusHardFailure;
        }

        if (context_snapshot.get_material_state().is_valid())
        {
            if (!context_snapshot.get_material_state().restore(m_pCur_context_state->m_context_info))
                return cStatusHardFailure;
        }

        if (context_snapshot.get_light_state().is_valid())
        {
            if (!context_snapshot.get_light_state().restore(m_pCur_context_state->m_context_info))
                return cStatusHardFailure;
        }

        if (context_snapshot.get_matrix_state().is_valid())
        {
            if (!context_snapshot.get_matrix_state().restore(m_pCur_context_state->m_context_info))
                return cStatusHardFailure;
        }

        if (context_snapshot.get_polygon_stipple_state().is_valid())
        {
            if (!context_snapshot.get_polygon_stipple_state().restore(m_pCur_context_state->m_context_info))
                return cStatusHardFailure;
        }

        if (context_snapshot.get_arb_program_environment_state().is_valid())
        {
            if (m_pCur_context_state->m_context_info.supports_extension("GL_ARB_vertex_program"))
            {
                if (!context_snapshot.get_arb_program_environment_state().restore(m_pCur_context_state->m_context_info, trace_to_replay_remapper))
                    return cStatusHardFailure;
            }
            else
            {
                vogl_error_printf("Unable to restore ARB program environment state to context because it does not support the \"GL_ARB_vertex_program\" extension\n");
            }
        }
    }

    if (context_snapshot.get_current_vertex_attrib_state().is_valid())
    {
        if (!context_snapshot.get_current_vertex_attrib_state().restore(m_pCur_context_state->m_context_info))
            return cStatusHardFailure;
    }

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::validate_program_and_shader_handle_tables
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::validate_program_and_shader_handle_tables()
{
    VOGL_FUNC_TRACER

    if (!m_pCur_context_state)
        return true;

    if (!get_shared_state()->m_shadow_state.m_objs.check())
        vogl_error_printf("Object handle tracker failed validation!\n");

    uint_vec replay_handles;
    get_shared_state()->m_shadow_state.m_objs.get_inv_handles(replay_handles);

    for (uint32_t i = 0; i < replay_handles.size(); i++)
    {
        GLuint replay_handle = replay_handles[i];
        GLuint trace_handle = replay_handle;
        bool map_succeeded = get_shared_state()->m_shadow_state.m_objs.map_inv_handle_to_handle(replay_handle, trace_handle);
        VOGL_ASSERT(map_succeeded);
        VOGL_NOTE_UNUSED(map_succeeded);

        GLenum target = get_shared_state()->m_shadow_state.m_objs.get_target_inv(replay_handle);
        VOGL_ASSERT(target == get_shared_state()->m_shadow_state.m_objs.get_target(trace_handle));

        if (target == VOGL_PROGRAM_OBJECT)
        {
            if (!GL_ENTRYPOINT(glIsProgram)(replay_handle))
            {
                vogl_error_printf("GL handle %u is being tracked, but glIsProgram() reports the handle is not a program\n", replay_handle);
            }
        }
        else if (target == VOGL_SHADER_OBJECT)
        {
            if (!GL_ENTRYPOINT(glIsShader)(replay_handle))
            {
                vogl_error_printf("GL handle %u is being tracked, but glIsShader() reports the handle is not a program\n", replay_handle);
            }
        }
        else
        {
            VOGL_ASSERT_ALWAYS;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::validate_textures
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::validate_textures()
{
    VOGL_FUNC_TRACER

    if (!m_pCur_context_state)
        return true;

    if (!get_shared_state()->m_shadow_state.m_textures.check())
        vogl_error_printf("Texture handle tracker failed validation!\n");

    for (uint32_t replay_handle = 1; replay_handle <= 0xFFFFU; replay_handle++)
    {
        bool is_tex = GL_ENTRYPOINT(glIsTexture)(replay_handle) != 0;

        bool found_in_shadow = get_shared_state()->m_shadow_state.m_textures.contains_inv(replay_handle);

        if (!is_tex)
        {
            if (found_in_shadow)
            {
                GLuint trace_handle = 0;
                get_shared_state()->m_shadow_state.m_textures.map_inv_handle_to_handle(replay_handle, trace_handle);
                vogl_debug_printf("Texture %u is not a name, but it has a valid mapping to trace handle %u\n", replay_handle, trace_handle);
            }
        }
        else
        {
            if (!found_in_shadow)
            {
                vogl_debug_printf("Texture %u is a valid name, but it does have a mapping to a trace handle!\n", replay_handle);
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::update_context_shadows
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::update_context_shadows(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(snapshot);
    VOGL_NOTE_UNUSED(context_snapshot);
    VOGL_NOTE_UNUSED(trace_to_replay_remapper);

    check_gl_error();

    // Make sure shadow is good
    GLint actual_current_replay_program = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_CURRENT_PROGRAM, &actual_current_replay_program);
    check_gl_error();

    m_pCur_context_state->m_cur_replay_program = actual_current_replay_program;
    if (!actual_current_replay_program)
        m_pCur_context_state->m_cur_trace_program = 0;
    else
    {
        GLuint trace_handle = actual_current_replay_program;
        if (!get_shared_state()->m_shadow_state.m_objs.map_inv_handle_to_handle(actual_current_replay_program, trace_handle))
        {
            process_entrypoint_error("Failed finding restored GL shader %u in program/shader object handle hashmap\n", actual_current_replay_program);

            m_pCur_context_state->m_cur_replay_program = 0;
            m_pCur_context_state->m_cur_trace_program = 0;
        }
        else
        {
            m_pCur_context_state->m_cur_trace_program = trace_handle;
        }
    }

    check_program_binding_shadow();

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::handle_marked_for_deleted_objects
//----------------------------------------------------------------------------------------------------------------------
void vogl_gl_replayer::handle_marked_for_deleted_objects(vogl_const_gl_object_state_ptr_vec &objects_to_delete, trace_to_replay_handle_remapper &trace_to_replay_remapper)
{
    VOGL_FUNC_TRACER

    vogl_verbose_printf("%u program/shader objects where marked as deleted\n", objects_to_delete.size());

    for (uint32_t i = 0; i < objects_to_delete.size(); i++)
    {
        const vogl_gl_object_state *pState_obj = objects_to_delete[i];

        GLuint64 trace_handle = pState_obj->get_snapshot_handle();
        GLuint64 restore_handle = trace_to_replay_remapper.remap_handle(pState_obj->get_handle_namespace(), pState_obj->get_snapshot_handle());

            // This should be a rare/exception case so let's try to be a little paranoid.
        vogl_verbose_printf("Snapshot object type %s trace handle 0x%" PRIX64 " restore handle 0x%" PRIX64 ", was marked as deleted, deleting object after restoring (object should still be referenced by state in the GL context)\n",
                            get_gl_object_state_type_str(pState_obj->get_type()), (uint64_t)trace_handle, (uint64_t)restore_handle);

        GLboolean object_is_still_a_name = true;

        switch (pState_obj->get_type())
        {
            case cGLSTProgram:
            {
                handle_delete_program(static_cast<GLuint>(trace_handle));

                object_is_still_a_name = GL_ENTRYPOINT(glIsProgram)(static_cast<GLuint>(restore_handle));

                break;
            }
            case cGLSTShader:
            {
                handle_delete_shader(static_cast<GLuint>(trace_handle));

                object_is_still_a_name = GL_ENTRYPOINT(glIsShader)(static_cast<GLuint>(restore_handle));

                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }
        }

        // "A program object marked for deletion with glDeleteProgram but still in use as part of current rendering state is still considered a program object and glIsProgram will return GL_TRUE."
        // Same for shaders.
        if (!object_is_still_a_name)
        {
            vogl_debug_printf("Snapshot object type %s trace handle 0x%" PRIX64 " restore handle 0x%" PRIX64 ", was marked as deleted, then deleted after a full state restore, but the object which should still be referenced by state in the GL context fails the glIsProgram()/glIsShader()/etc. test\n",
                             get_gl_object_state_type_str(pState_obj->get_type()), (uint64_t)trace_handle, (uint64_t)restore_handle);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::begin_applying_snapshot
// Takes ownership (even on errors) when delete_snapshot_after_applying is true.
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::begin_applying_snapshot(const vogl_gl_state_snapshot *pSnapshot, bool delete_snapshot_after_applying)
{
    VOGL_FUNC_TRACER

    if (!pSnapshot->is_valid())
    {
        if (delete_snapshot_after_applying)
            vogl_delete(const_cast<vogl_gl_state_snapshot *>(pSnapshot));

        return cStatusHardFailure;
    }

    reset_state();

    m_pPending_snapshot = pSnapshot;
    m_delete_pending_snapshot_after_applying = delete_snapshot_after_applying;

    m_frame_index = pSnapshot->get_frame_index();
    m_last_parsed_call_counter = pSnapshot->get_gl_call_counter();
    m_last_processed_call_counter = pSnapshot->get_gl_call_counter();
    m_at_frame_boundary = false;

    if (!(m_flags & cGLReplayerLockWindowDimensions))
    {
        return trigger_pending_window_resize(pSnapshot->get_window_width(), pSnapshot->get_window_height());
    }

    return process_applying_pending_snapshot();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::restore_context
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::restore_context(vogl_handle_remapper &trace_to_replay_remapper, const vogl_gl_state_snapshot &snapshot, const vogl_context_snapshot &context_snapshot)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(trace_to_replay_remapper);
    VOGL_NOTE_UNUSED(snapshot);

    // TODO: This always creates with attribs, also need to support plain glXCreateContext()

    vogl_trace_context_ptr_value trace_share_context = context_snapshot.get_context_desc().get_trace_share_context();

    GLReplayContextType replay_share_context = remap_context(trace_share_context);
    if ((trace_share_context) && (!replay_share_context))
    {
        vogl_error_printf("Failed remapping trace share context handle 0x%" PRIx64 " to replay context!\n", cast_val_to_uint64(trace_share_context));
        return cStatusHardFailure;
    }

    GLboolean direct = context_snapshot.get_context_desc().get_direct();

    vogl_trace_context_ptr_value trace_context = context_snapshot.get_context_desc().get_trace_context();

    status_t status = create_context_attribs(trace_context, trace_share_context, replay_share_context, direct,
                                             context_snapshot.get_context_desc().get_attribs().get_vec().get_ptr(),
                                             context_snapshot.get_context_desc().get_attribs().get_vec().size(), true);
    if (status != cStatusOK)
    {
        vogl_error_printf("Failed creating new context\n");
        return status;
    }

    // Has this context ever been made current?
    if (context_snapshot.get_context_info().is_valid())
    {
        context_state *pContext_state = get_trace_context_state(trace_context);
        if (!pContext_state)
        {
            vogl_error_printf("Failed finding replay context current\n");
            return cStatusHardFailure;
        }

        GLReplayContextType replay_context = pContext_state->m_replay_context;
        if (!replay_context)
        {
            vogl_error_printf("Failed finding replay context current\n");
            return cStatusHardFailure;
        }

        if (!m_pWindow->make_current(replay_context))
        {
            vogl_error_printf("Failed making context current\n");
            return cStatusHardFailure;
        }

#if defined (_WIN32)
        // Since a new context has been made current, the GL entrypoints need to be re-queried
        if (load_gl())
            vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper);
#endif

        m_cur_trace_context = trace_context;
        m_cur_replay_context = replay_context;
        m_pCur_context_state = pContext_state;

        if (!handle_context_made_current())
            return cStatusHardFailure;
    }

    return cStatusOK;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::process_applying_pending_snapshot
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_replayer::status_t vogl_gl_replayer::process_applying_pending_snapshot()
{
    VOGL_FUNC_TRACER

    if (!m_pPending_snapshot)
        return cStatusOK;

    timed_scope ts(VOGL_FUNCTION_INFO_CSTR);

    const vogl_gl_state_snapshot &snapshot = *m_pPending_snapshot;

    trace_to_replay_handle_remapper trace_to_replay_remapper(*this);

    m_frame_index = snapshot.get_frame_index();
    m_last_parsed_call_counter = snapshot.get_gl_call_counter();
    m_last_processed_call_counter = snapshot.get_gl_call_counter();
    m_at_frame_boundary = snapshot.get_at_frame_boundary();

    // Ensure the client side array bufs are large enough (we don't care about the actual ptr values).
    for (uint32_t i = 0; i < snapshot.get_client_side_vertex_attrib_ptrs().size(); i++)
        m_client_side_vertex_attrib_data[i].resize(snapshot.get_client_side_vertex_attrib_ptrs()[i].m_size);

    for (uint32_t i = 0; i < snapshot.get_client_side_array_ptrs().size(); i++)
        m_client_side_array_data[i].resize(snapshot.get_client_side_array_ptrs()[i].m_size);

    for (uint32_t i = 0; i < snapshot.get_client_side_texcoord_ptrs().size(); i++)
        m_client_side_texcoord_data[i].resize(snapshot.get_client_side_texcoord_ptrs()[i].m_size);

    const vogl_context_snapshot_ptr_vec &context_ptrs = snapshot.get_contexts();

    vogl_context_snapshot_ptr_vec restore_context_ptrs(snapshot.get_contexts());
    vogl::vector<vogl_const_gl_object_state_ptr_vec> objects_to_delete_vec(context_ptrs.size());

    status_t status = cStatusOK;
    uint32_t total_contexts_restored = 0;
    bool restored_default_framebuffer = false;

    for (;;)
    {
        uint32_t num_contexts_restored_in_this_pass = 0;

        for (uint32_t context_index = 0; context_index < restore_context_ptrs.size(); context_index++)
        {
            if (!restore_context_ptrs[context_index])
                continue;

            const vogl_context_snapshot &context_state = *restore_context_ptrs[context_index];

            if (context_state.get_context_desc().get_trace_share_context())
            {
                // Don't restore this context if its sharelist context hasn't been restored yet
                if (!remap_context(context_state.get_context_desc().get_trace_share_context()))
                    continue;
            }

            status = restore_context(trace_to_replay_remapper, snapshot, context_state);
            if (status != cStatusOK)
                goto handle_error;

            // Has this context ever been made current?
            if (context_state.get_context_info().is_valid())
            {
                // Keep this in sync with vogl_gl_object_state_type (the order doesn't need to match the enum, but be sure to restore leaf GL objects first!)
                const vogl_gl_object_state_type s_object_type_restore_order[] = { cGLSTBuffer, cGLSTSampler, cGLSTQuery, cGLSTRenderbuffer, cGLSTTexture, cGLSTFramebuffer, cGLSTVertexArray, cGLSTShader, cGLSTProgram, cGLSTSync, cGLSTARBProgram, cGLSTProgramPipeline };
                VOGL_ASSUME(VOGL_ARRAY_SIZE(s_object_type_restore_order) == (cGLSTTotalTypes - 1));

                if (m_flags & cGLReplayerLowLevelDebugMode)
                {
                    if (!validate_textures())
                        vogl_warning_printf("Failed validating texture handles against handle mapping tables\n");
                }

                vogl_const_gl_object_state_ptr_vec &objects_to_delete = objects_to_delete_vec[context_index];

                for (uint32_t i = 0; i < VOGL_ARRAY_SIZE(s_object_type_restore_order); i++)
                {
                    status = restore_objects(trace_to_replay_remapper, snapshot, context_state, s_object_type_restore_order[i], objects_to_delete);
                    if (status != cStatusOK)
                        goto handle_error;

                    if (m_flags & cGLReplayerLowLevelDebugMode)
                    {
                        if (!validate_program_and_shader_handle_tables())
                            vogl_error_printf("Program/shader handle table validation failed!\n");

                        if (!validate_textures())
                            vogl_warning_printf("Failed validating texture handles against handle mapping tables\n");
                    }
                }

                if (m_flags & cGLReplayerLowLevelDebugMode)
                {
                    if (!validate_textures())
                        vogl_warning_printf("Failed validating texture handles against handle mapping tables\n");
                }

                status = restore_display_lists(trace_to_replay_remapper, snapshot, context_state);
                if (status != cStatusOK)
                    goto handle_error;

                // Restore default framebuffer
                if ((!restored_default_framebuffer) && (snapshot.get_default_framebuffer().is_valid()))
                {
                    restored_default_framebuffer = true;

                    if (!snapshot.get_default_framebuffer().restore(m_pCur_context_state->m_context_info, (m_flags & cGLReplayerDisableRestoreFrontBuffer) == 0))
                    {
                        vogl_warning_printf("Failed restoring default framebuffer!\n");
                    }
                }

                // Beware: restore_general_state() will bind a bunch of stuff from the trace!
                status = restore_general_state(trace_to_replay_remapper, snapshot, context_state);
                if (status != cStatusOK)
                    goto handle_error;

                status = update_context_shadows(trace_to_replay_remapper, snapshot, context_state);
                if (status != cStatusOK)
                    goto handle_error;

                if (m_flags & cGLReplayerLowLevelDebugMode)
                {
                    if (!validate_program_and_shader_handle_tables())
                        vogl_error_printf("Program/shader handle table validation failed!\n");

                    if (!validate_textures())
                        vogl_warning_printf("Failed validating texture handles against handle mapping tables\n");
                }
            }

            num_contexts_restored_in_this_pass++;

            total_contexts_restored++;

            restore_context_ptrs[context_index] = NULL;
        }

        if (!num_contexts_restored_in_this_pass)
            break;
    }

    if (total_contexts_restored != snapshot.get_contexts().size())
    {
        vogl_error_printf("Failed satisfying sharelist dependency during context restoration\n");
        goto handle_error;
    }

    for (uint32_t context_index = 0; context_index < context_ptrs.size(); context_index++)
    {
        const vogl_context_snapshot &context_state = *context_ptrs[context_index];

        if (!context_state.get_context_info().is_valid())
            continue;

        status = switch_contexts(context_state.get_context_desc().get_trace_context());
        if (status != cStatusOK)
        {
            vogl_error_printf("Failed switching to trace context 0x%" PRIX64 ", capture failed\n", cast_val_to_uint64(context_state.get_context_desc().get_trace_context()));
            goto handle_error;
        }

        vogl_const_gl_object_state_ptr_vec &objects_to_delete = objects_to_delete_vec[context_index];

        handle_marked_for_deleted_objects(objects_to_delete, trace_to_replay_remapper);
    }

    destroy_pending_snapshot();

    return cStatusOK;

handle_error:

    reset_state();

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::write_trim_file_internal
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::write_trim_file_internal(vogl_trace_packet_array &trim_packets, const dynamic_string &trim_filename, vogl_trace_file_reader &trace_reader, bool optimize_snapshot, dynamic_string *pSnapshot_id)
{
    // Open the output trace
    // TODO: This pretty much ignores the ctypes packet, and uses the one based off the ptr size in the header. The ctypes stuff needs to be refactored, storing it in an explicit packet is bad.
    const vogl_ctypes &trace_gl_ctypes = get_trace_gl_ctypes();

    vogl_trace_packet trace_packet(&trace_gl_ctypes);

    // TODO: This seems like WAY too much work! Move the snapshot to the beginning of the trace, in the header!
    bool found_state_snapshot = false;
    // Not currently used...
    // dynamic_string binary_snapshot_id, text_snapshot_id;

    bool is_at_start_of_trace = false;
    VOGL_NOTE_UNUSED(is_at_start_of_trace);

    int demarcation_packet_index = -1;
    for (uint32_t packet_index = 0; packet_index < trim_packets.size(); packet_index++)
    {
        const vogl_trace_stream_packet_types_t packet_type = trim_packets.get_packet_type(packet_index);
        if (packet_type != cTSPTGLEntrypoint)
            continue;

        const uint8_vec &packet_buf = trim_packets.get_packet_buf(packet_index);

        const vogl_trace_gl_entrypoint_packet *pGL_packet = &trim_packets.get_packet<vogl_trace_gl_entrypoint_packet>(packet_index);

        if (pGL_packet->m_entrypoint_id != VOGL_ENTRYPOINT_glInternalTraceCommandRAD)
            continue;

        if (!trace_packet.deserialize(packet_buf.get_ptr(), packet_buf.size(), true))
        {
            vogl_error_printf("Failed parsing glInternalTraceCommandRAD packet\n");
            return false;
        }

        GLuint cmd = trace_packet.get_param_value<GLuint>(0);
        if (cmd == cITCRDemarcation)
        {
            is_at_start_of_trace = true;
            demarcation_packet_index = packet_index;
        }
        else if (cmd == cITCRKeyValueMap)
        {
            key_value_map &kvm = trace_packet.get_key_value_map();

            dynamic_string cmd_type(kvm.get_string("command_type"));

            if (cmd_type == "state_snapshot")
            {
                found_state_snapshot = true;

                // Not currently used...
                // text_snapshot_id = kvm.get_string("id");
                // binary_snapshot_id = kvm.get_string("binary_id");
            }
        }
    }

    vogl_trace_file_writer trace_writer(&trace_gl_ctypes);
    if (!trace_writer.open(trim_filename.get_ptr(), NULL, true, false, m_trace_pointer_size_in_bytes))
    {
        vogl_error_printf("Failed creating trimmed trace file \"%s\"!\n", trim_filename.get_ptr());
        return false;
    }

    if (found_state_snapshot)
    {
        // Copy over the source trace's archive (it contains the snapshot, along with any files it refers to).
        if (trace_reader.get_archive_blob_manager().is_initialized())
        {
            dynamic_string_array blob_files(trace_reader.get_archive_blob_manager().enumerate());
            for (uint32_t i = 0; i < blob_files.size(); i++)
            {
                if ((blob_files[i].is_empty()) || (blob_files[i] == VOGL_TRACE_ARCHIVE_FRAME_FILE_OFFSETS_FILENAME))
                    continue;

                vogl_message_printf("Adding blob file %s to output trace archive\n", blob_files[i].get_ptr());

                if (!trace_writer.get_trace_archive()->copy_file(trace_reader.get_archive_blob_manager(), blob_files[i], blob_files[i]).has_content())
                {
                    vogl_error_printf("Failed copying blob data for file \"%s\" to output trace archive!\n", blob_files[i].get_ptr());
                    return false;
                }
            }
        }
    }
    else
    {
        // Copy over the source trace's backtrace map, machine info, etc. files.
        if (trace_reader.get_archive_blob_manager().is_initialized())
        {
            // compiler_info.json
            trace_writer.get_trace_archive()->copy_file(trace_reader.get_archive_blob_manager(), VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME, VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME);
            // machine_info.json
            trace_writer.get_trace_archive()->copy_file(trace_reader.get_archive_blob_manager(), VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME, VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME);
            // backtrace_map_syms.json
            trace_writer.get_trace_archive()->copy_file(trace_reader.get_archive_blob_manager(), VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME, VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_SYMS_FILENAME);
            // backtrace_map_addrs.json
            trace_writer.get_trace_archive()->copy_file(trace_reader.get_archive_blob_manager(), VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME, VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME);
        }

        vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper, true);

        vogl_unique_ptr<vogl_gl_state_snapshot> pTrim_snapshot(snapshot_state(&trim_packets, optimize_snapshot));

        if (!pTrim_snapshot.get())
        {
            vogl_error_printf("Failed creating replayer GL snapshot!\n");
            return false;
        }

        pTrim_snapshot->set_frame_index(0);

        json_document doc;
        if (!pTrim_snapshot->serialize(*doc.get_root(), *trace_writer.get_trace_archive(), &trace_gl_ctypes))
        {
            vogl_error_printf("Failed serializing GL state snapshot!\n");
            trace_writer.close();
            file_utils::delete_file(trim_filename.get_ptr());
            return false;
        }

        vogl::vector<char> snapshot_data;
        doc.serialize(snapshot_data, true, 0, false);

        uint8_vec binary_snapshot_data;
        doc.binary_serialize(binary_snapshot_data);

        pTrim_snapshot.reset();

        // Write the state_snapshot file to the trace archive
        dynamic_string snapshot_id(trace_writer.get_trace_archive()->add_buf_compute_unique_id(snapshot_data.get_ptr(), snapshot_data.size(), "state_snapshot", VOGL_TEXT_JSON_EXTENSION));
        if (snapshot_id.is_empty())
        {
            vogl_error_printf("Failed adding GL snapshot file to output blob manager!\n");
            trace_writer.close();
            file_utils::delete_file(trim_filename.get_ptr());
            return false;
        }

        if (pSnapshot_id)
            *pSnapshot_id = snapshot_id;

        snapshot_data.clear();

        // Write the binary_state_snapshot file to the trace archive
        dynamic_string binary_id(trace_writer.get_trace_archive()->add_buf_compute_unique_id(binary_snapshot_data.get_ptr(), binary_snapshot_data.size(), "binary_state_snapshot", VOGL_BINARY_JSON_EXTENSION));
        if (binary_id.is_empty())
        {
            vogl_error_printf("Failed adding binary GL snapshot file to output blob manager!\n");
            trace_writer.close();
            file_utils::delete_file(trim_filename.get_ptr());
            return false;
        }

        binary_snapshot_data.clear();

        key_value_map snapshot_key_value_map;
        snapshot_key_value_map.insert("command_type", "state_snapshot");
        snapshot_key_value_map.insert("id", snapshot_id);
        snapshot_key_value_map.insert("binary_id", binary_id);

        dynamic_stream snapshot_stream(0);
        if (!vogl_write_glInternalTraceCommandRAD(snapshot_stream, &trace_gl_ctypes, cITCRKeyValueMap, sizeof(snapshot_key_value_map), reinterpret_cast<const GLubyte *>(&snapshot_key_value_map)))
        {
            vogl_error_printf("Failed serializing snapshot packet!\n");
            trace_writer.close();
            file_utils::delete_file(trim_filename.get_ptr());
            return false;
        }

        if (demarcation_packet_index >= 0)
        {
            trim_packets.insert(demarcation_packet_index, snapshot_stream.get_buf());
            demarcation_packet_index++;
        }
        else
        {
            dynamic_stream demarcation_stream(0);
            vogl_write_glInternalTraceCommandRAD(demarcation_stream, &trace_gl_ctypes, cITCRDemarcation, 0, NULL);

            // Screw the ctypes packet, it's only used for debugging right now anyway.
            trim_packets.insert(0, snapshot_stream.get_buf());
            trim_packets.insert(1, demarcation_stream.get_buf());
        }
    }

    for (uint32_t packet_index = 0; packet_index < trim_packets.size(); packet_index++)
    {
        const uint8_vec &packet_buf = trim_packets.get_packet_buf(packet_index);

        const bool is_swap = trim_packets.is_swap_buffers_packet(packet_index);

        const vogl_trace_stream_packet_types_t packet_type = trim_packets.get_packet_type(packet_index);
        if (packet_type == cTSPTEOF)
            break;
        else if (packet_type != cTSPTGLEntrypoint)
        {
            VOGL_ASSERT_ALWAYS;
        }

        if (!trace_writer.write_packet(packet_buf.get_ptr(), packet_buf.size(), is_swap))
        {
            vogl_error_printf("Failed writing trace packet to output trace file \"%s\"!\n", trim_filename.get_ptr());
            trace_writer.close();
            file_utils::delete_file(trim_filename.get_ptr());
            return false;
        }
    }

    bool success = trace_writer.close();
    if (!success)
        vogl_error_printf("Failed closing wrote trim trace file \"%s\"\n", trim_filename.get_ptr());
    else
        vogl_message_printf("Successfully wrote trim trace file \"%s\"\n", trim_filename.get_ptr());

    return success;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_gl_replayer::write_trim_file
//----------------------------------------------------------------------------------------------------------------------
bool vogl_gl_replayer::write_trim_file(uint32_t flags, const dynamic_string &trim_filename, uint32_t trim_len, vogl_trace_file_reader &trace_reader, dynamic_string *pSnapshot_id)
{
    VOGL_FUNC_TRACER

    if (!m_is_valid)
    {
        vogl_error_printf("Trace is not open\n");
        return false;
    }

    bool from_start_of_frame = (flags & cWriteTrimFileFromStartOfFrame) != 0;

    if ((!from_start_of_frame) || (!trim_len))
        flags &= ~cWriteTrimFileOptimizeSnapshot;

    const uint32_t trim_frame = static_cast<uint32_t>(get_frame_index());
    const int64_t trim_call_counter = get_last_parsed_call_counter();

    // Read the desired packets from the source trace file
    vogl_trace_packet_array trim_packets;

    if ((trim_len) || (!trim_frame))
    {
        vogl_message_printf("Reading trim packets from source trace file\n");

        uint32_t frames_to_read = trim_len;
        if ((from_start_of_frame) && (!trim_frame) && (!trim_len))
            frames_to_read = 1;

        uint32_t actual_trim_len = 0;
        vogl_trace_file_reader::trace_file_reader_status_t read_packets_status = trace_reader.read_frame_packets(trim_frame, frames_to_read, trim_packets, actual_trim_len);
        if (read_packets_status == vogl_trace_file_reader::cFailed)
        {
            vogl_error_printf("Failed reading source trace file packets beginning at frame %u!\n", trim_frame);
            return false;
        }

        if (actual_trim_len != frames_to_read)
        {
            vogl_warning_printf("Only able to read %u frames from trim file beginning at frame %u, not the requested %u\n", actual_trim_len, trim_frame, frames_to_read);
        }

        if (from_start_of_frame)
        {
            vogl_message_printf("Read %u trim packets beginning at frame %u actual len %u from source trace file\n", trim_packets.size(), trim_frame, actual_trim_len);

            if ((!trim_frame) && (!trim_len))
            {
                // Special case: They want frame 0 with no packets, so be sure to copy any internal trace commands at the very beginning of the trace.
                // TODO: Most of this will go away once we move the state snapshot into the trace archive.

                vogl_trace_packet_array new_trim_packets;

                vogl_trace_packet trace_packet(&get_trace_gl_ctypes());

                for (uint32_t packet_index = 0; packet_index < trim_packets.size(); packet_index++)
                {
                    const vogl_trace_stream_packet_types_t packet_type = trim_packets.get_packet_type(packet_index);
                    if (packet_type != cTSPTGLEntrypoint)
                        break;

                    const uint8_vec &packet_buf = trim_packets.get_packet_buf(packet_index);

                    const vogl_trace_gl_entrypoint_packet *pGL_packet = &trim_packets.get_packet<vogl_trace_gl_entrypoint_packet>(packet_index);
                    if (pGL_packet->m_entrypoint_id != VOGL_ENTRYPOINT_glInternalTraceCommandRAD)
                        break;

                    if (!trace_packet.deserialize(packet_buf.get_ptr(), packet_buf.size(), true))
                    {
                        vogl_error_printf("Failed parsing glInternalTraceCommandRAD packet\n");
                        return false;
                    }

                    GLuint cmd = trace_packet.get_param_value<GLuint>(0);

                    new_trim_packets.push_back(packet_buf);

                    if (cmd == cITCRDemarcation)
                        break;
                }

                trim_packets.swap(new_trim_packets);
            }
        }
        else if (trim_call_counter >= 0)
        {
            uint32_t orig_num_packets = trim_packets.size();
            uint32_t total_erased_packets = 0;

            // Remove any calls before the current one.
            for (int64_t packet_index = 0; packet_index < trim_packets.size(); packet_index++)
            {
                if (trim_packets.get_packet_type(static_cast<uint32_t>(packet_index)) != cTSPTGLEntrypoint)
                    continue;

                const vogl_trace_gl_entrypoint_packet *pGL_packet = &trim_packets.get_packet<vogl_trace_gl_entrypoint_packet>(static_cast<uint32_t>(packet_index));

                if (static_cast<int64_t>(pGL_packet->m_call_counter) <= trim_call_counter)
                {
                    trim_packets.erase(static_cast<uint32_t>(packet_index));
                    packet_index--;

                    total_erased_packets++;
                }
            }

            vogl_message_printf("Read %u packets from frame %u, erased %u packets before call counter %" PRIu64 ", storing %u trim packets from source trace file\n", orig_num_packets, trim_frame, total_erased_packets, trim_call_counter, trim_packets.size());
        }
    }

    if (!write_trim_file_internal(trim_packets, trim_filename, trace_reader, (flags & cWriteTrimFileOptimizeSnapshot) != 0, pSnapshot_id))
    {
        vogl_warning_printf("Trim file write failed, deleting invalid trim trace file %s\n", trim_filename.get_ptr());

        file_utils::delete_file(trim_filename.get_ptr());
        return false;
    }

    return true;
}
