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

// File: vogl_replay_tool.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"
#include "vogl_texture_format.h"
#include "vogl_trace_file_writer.h"

#include "vogl_colorized_console.h"
#include "vogl_command_line_params.h"
#include "vogl_cfile_stream.h"
#include "vogl_value.h"
#include "vogl_dynamic_stream.h"
#include "vogl_file_utils.h"
#include "vogl_mergesort.h"
#include "vogl_unique_ptr.h"
#include "vogl_find_files.h"
#include "vogl_bigint128.h"
#include "vogl_regex.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "vogl_json.h"
#include "vogl_blob_manager.h"

#if VOGL_REMOTING
#include "vogl_remote.h"
#endif // VOGL_REMOTING

#include "libtelemetry.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
static void *g_actual_libgl_module_handle;
static cfile_stream *g_vogl_pLog_stream;

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs[] =
    {
        { "replay", 0, false, "Replay mode (the default), must specify .BIN or .JSON trace file to replay" },
        { "dump", 0, false, "Dump mode: Dumps binary trace file to a JSON trace file, must specify input and output filenames" },
        { "parse", 0, false, "Parse mode: Parse JSON trace file to a binary trace file, must specify input and output filenames" },
        { "info", 0, false, "Info mode: Output statistics about a trace file" },
        { "unpack_json", 0, false, "Unpack UBJ to JSON mode: Unpack UBJ (Universal Binary JSON) to textual JSON, must specify input and output filenames" },
        { "pack_json", 0, false, "Pack JSON to UBJ mode: Pack textual JSON to UBJ, must specify input and output filenames" },
        { "find", 0, false, "Find all calls with parameters containing a specific value, combine with -find_param, -find_func, find_namespace, etc. params" },
        { "compare_hash_files", 0, false, "Compare two files containing CRC's or per-component sums (presumably written using dump_backbuffer_hashes)" },

        // replay specific
        { "width", 1, false, "Replay: Set replay window's initial width (default is 1024)" },
        { "height", 1, false, "Replay: Set replay window's initial height (default is 768)" },
        { "msaa", 1, false, "Replay: Set replay window's multisamples (default is 0)." },
        { "lock_window_dimensions", 0, false, "Replay: Don't automatically change window's dimensions during replay" },
        { "trim_file", 1, false, "Replay: Create a trimmed trace file during replay, must also specify -trim_frame" },
        { "trim_frame", 1, false, "Replay: Frame index to begin trim, 0=beginning of trace, 1=first API call after first swap, etc." },
        { "trim_len", 1, false, "Replay: Length of trim file, default=1 frame" },
        { "multitrim", 0, false, "Replay trimming: Trim each frame to a different file" },
        { "multitrim_interval", 1, false, "Replay trimming: Set the # of frames between each multitrimmed frame (default is 1)" },
        { "no_trim_optimization", 0, false, "Replay trimming: If specified, do not remove unused programs, shaders, etc. from trim file" },
        { "trim_call", 1, false, "Replay: Call counter index to begin trim" },
        { "write_snapshot_call", 1, false, "Replay: Write JSON snapshot at the specified call counter index" },
        { "write_snapshot_file", 1, false, "Replay: Write JSON snapshot to specified filename, must also specify --write_snapshot_call" },
        { "write_snapshot_blobs", 0, false, "Replay: Write JSON snapshot blob files, must also specify --write_snapshot_call" },
        { "endless", 0, false, "Replay: Loop replay endlessly instead of exiting" },
        { "hash_backbuffer", 0, false, "Replay: Hash and output backbuffer CRC before every swap" },
        { "dump_backbuffer_hashes", 1, false, "Replay: Dump backbuffer hashes to a text file" },
        { "sum_hashing", 0, false, "Replay: Use per-component sums, instead of CRC hashing (useful for multisampling)" },
        { "dump_screenshots", 0, false, "Replay: Dump backbuffer screenshot before every swap to numbered PNG files" },
        { "dump_screenshots_prefix", 1, false, "Replay: Set PNG screenshot file prefix" },
        { "swap_sleep", 1, false, "Replay: Sleep for X milliseconds after every swap" },
        { "dump_packets_on_error", 0, false, "Replay: Dump GL trace packets as JSON to stdout on replay errors" },
        { "dump_packet_blob_files_on_error", 0, false, "Replay: Used with -dump_packets_on_error, also dumps all binary blob files associated with each packet" },
        { "dump_all_packets", 0, false, "Replay: Dump all GL trace packets as JSON to stdout" },
        { "dump_shaders_on_draw", 0, false, "Replay: Dump shader source on draw calls" },
        { "dump_framebuffer_on_draw", 0, false, "Replay: Dump framebuffer to PNG files after each draw/glEnd/glCallList" },
        { "dump_framebuffer_on_draw_prefix", 1, false, "Replay: Base path/filename to use for --dump_framebuffer_on_draw" },
        { "dump_framebuffer_on_draw_frame", 1, false, "Replay: Limit dumping framebuffer PNG files" },
        { "dump_framebuffer_on_draw_first_gl_call", 1, false, "Replay: Limit dumping framebuffer PNG files" },
        { "dump_framebuffer_on_draw_last_gl_call", 1, false, "Replay: Limit dumping framebuffer PNG files" },
        { "clear_uninitialized_bufs", 0, false, "glBufferData(): Ensure buffers are unitialized to all-zeros when data param is NULL" },
        { "force_debug_context", 0, false, "Replay: Force GL debug contexts" },
        { "pause_on_exit", 0, false, "Replay: Wait for a keypress on exit" },
        { "debug_test_snapshot_serialization", 0, false, "Interactive Replay Mode: Immediately serialize/deserialize state snapshots after taking them" },
        { "pause_on_frame", 1, false, "Replay interactive mode: Pause on specified frame" },
        { "interactive", 0, false, "Replay mode: Enable keyboard keys" },
        { "disable_snapshot_caching", 0, false, "Replay mode: Disable caching of all state snapshot files, so they can be manually modified during replay" },
        { "benchmark", 0, false, "Replay mode: Disable glGetError()'s, divergence checks, during replaying" },
        { "keyframe_base_filename", 1, false, "Replay: Set base filename of trimmed replay keyframes, used for fast seeking" },
#ifdef USE_TELEMETRY
        { "telemetry_level", 1, false, "Set Telemetry level." },
#endif
        { "loop_frame", 1, false, "Replay: loop mode's start frame" },
        { "loop_len", 1, false, "Replay: loop mode's loop length" },
        { "loop_count", 1, false, "Replay: loop mode's loop count" },
        { "draw_kill_max_thresh", 1, false, "Replay: Enable draw kill mode during looping to visualize order of draws, sets the max # of draws before counter resets to 0" },
        { "disable_frontbuffer_restore", 0, false, "Replay: Do not restore the front buffer's contents when restoring a state snapshot" },

        // find specific
        { "find_func", 1, false, "Find: Limit the find to only the specified function name POSIX regex pattern" },
        { "find_param", 1, false, "Find: The parameter value to find, hex, decimal integers, or GL enum strings OK" },
        { "find_namespace", 1, false, "Find: Limits --find_param to only parameters using the specified handle namespace: invalid, GLhandleARB, GLframebuffer, GLtexture, GLrenderbuffer, GLquery, GLsampler, GLprogramARB, GLprogram, GLarray, GLlist, GLlocation, GLlocationARB, GLfence, GLsync, GLpipeline, GLshader, GLbuffer, GLfeedback, GLarrayAPPLE, GLfragmentShaderATI" },
        { "find_param_name", 1, false, "Find: Limits the find to only params with the specified name (specify \"return\" to limit search to only return values)" },
        { "find_frame_low", 1, false, "Find: Limit the find to frames beginning at the specified frame index" },
        { "find_frame_high", 1, false, "Find: Limit the find to frames up to and including the specified frame index" },
        { "find_call_low", 1, false, "Find: Limit the find to GL calls beginning at the specified call index" },
        { "find_call_high", 1, false, "Find: Limit the find to GL calls up to and including the specified call index" },

        // compare_hash_files specific
        { "sum_compare_threshold", 1, false, "compare_hash_files: Only report mismatches greater than the specified threshold, use with --sum_hashing" },
        { "compare_ignore_frames", 1, false, "compare_hash_files: Ignore first X frames" },
        { "compare_expected_frames", 1, false, "compare_hash_files: Fail if the # of frames is not X" },
        { "compare_first_frame", 1, false, "compare_hash_files: First frame to compare to in second hash file" },
        { "ignore_line_count_differences", 0, false, "compare_hash_files: Don't stop if the # of lines differs between the two files" },

        // dump specific
        { "verify", 0, false, "Dump: Fully round-trip verify all JSON objects vs. the original packet's" },
        { "no_blobs", 0, false, "Dump: Don't write binary blob files" },
        { "write_debug_info", 0, false, "Dump: Write extra debug info to output JSON trace files" },
        { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
        { "debug", 0, false, "Enable verbose debug information" },
        { "logfile", 1, false, "Create logfile" },
        { "logfile_append", 1, false, "Append output to logfile" },
        { "help", 0, false, "Display this help" },
        { "?", 0, false, "Display this help" },
        { "replay_debug", 0, false, "Enable various debug/verification code in the replayer" },
        { "pause", 0, false, "Wait for a key at startup (so a debugger can be attached)" },
        { "verbose", 0, false, "Verbose debug output" },
        { "quiet", 0, false, "Disable all console output" },
        { "gl_debug_log", 0, false, "Dump GL prolog/epilog messages to stdout (very slow - helpful to narrow down driver crashes)" },
        { "vogl_func_tracing", 0, false, NULL },
    };

static command_line_param_desc g_command_line_interactive_descs[] =
    {
        { "s", 0, false, "slow mode" },
        { "<space>", 0, false, "pause" },
        { "r", 0, false, "rewind to beginning" },
        { "e", 0, false, "seek to last frame" },
        { "t", 0, false, "trim frame" },
        { "j", 0, false, "trim and play json" },
        { "<left>", 0, false, "step left" },
        { "<right>", 0, false, "step right" },
    };

//----------------------------------------------------------------------------------------------------------------------
// init_logfile
//----------------------------------------------------------------------------------------------------------------------
static bool init_logfile()
{
    VOGL_FUNC_TRACER

    dynamic_string backbuffer_hash_file;
    if (g_command_line_params().get_value_as_string(backbuffer_hash_file, "dump_backbuffer_hashes"))
    {
        remove(backbuffer_hash_file.get_ptr());
        vogl_message_printf("Deleted backbuffer hash file \"%s\"\n", backbuffer_hash_file.get_ptr());
    }

    dynamic_string log_file(g_command_line_params().get_value_as_string_or_empty("logfile"));
    dynamic_string log_file_append(g_command_line_params().get_value_as_string_or_empty("logfile_append"));
    if (log_file.is_empty() && log_file_append.is_empty())
        return true;

    dynamic_string filename(log_file_append.is_empty() ? log_file : log_file_append);

    // This purposely leaks, don't care
    g_vogl_pLog_stream = vogl_new(cfile_stream);

    if (!g_vogl_pLog_stream->open(filename.get_ptr(), cDataStreamWritable, !log_file_append.is_empty()))
    {
        vogl_error_printf("%s: Failed opening log file \"%s\"\n", VOGL_FUNCTION_NAME, filename.get_ptr());
        return false;
    }
    else
    {
        vogl_message_printf("Opened log file \"%s\"\n", filename.get_ptr());

        console::set_log_stream(g_vogl_pLog_stream);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_title
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_title()
{
    VOGL_FUNC_TRACER

    printf("voglreplay ");
    if (sizeof(void *) > 4)
        console::printf("64-bit ");
    else
        console::printf("32-bit ");
#ifdef VOGL_BUILD_DEBUG
    console::printf("Debug ");
#else
    console::printf("Release ");
#endif
    console::printf("Built %s %s\n", __DATE__, __TIME__);
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_help
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_help()
{
    VOGL_FUNC_TRACER

    console::printf("Usage: voglreplay [ -option ... ] input_file optional_output_file [ -option ... ]\n");
    console::printf("Command line options may begin with single minus \"-\" or double minus \"--\"\n");

    console::printf("\nCommand line options:\n");

    dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, "--");

    console::printf("\nInteractive replay mode keys:\n");
    dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_interactive_descs), g_command_line_interactive_descs, " ");
}

//----------------------------------------------------------------------------------------------------------------------
// init_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static bool init_command_line_params(int argc, char *argv[])
{
    VOGL_FUNC_TRACER

    command_line_params::parse_config parse_cfg;
    parse_cfg.m_single_minus_params = true;
    parse_cfg.m_double_minus_params = true;

    if (!g_command_line_params().parse(get_command_line_params(argc, argv), VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, parse_cfg))
    {
        vogl_error_printf("%s: Failed parsing command line parameters!\n", VOGL_FUNCTION_NAME);
        return false;
    }

    if (!init_logfile())
        return false;

    if (g_command_line_params().get_value_as_bool("help") || g_command_line_params().get_value_as_bool("?"))
    {
        tool_print_help();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// load_gl
//----------------------------------------------------------------------------------------------------------------------
static bool load_gl()
{
    VOGL_FUNC_TRACER

    g_actual_libgl_module_handle = dlopen("libGL.so.1", RTLD_LAZY);
    if (!g_actual_libgl_module_handle)
    {
        vogl_error_printf("%s: Failed loading libGL.so.1!\n", VOGL_FUNCTION_NAME);
        return false;
    }

    GL_ENTRYPOINT(glXGetProcAddress) = reinterpret_cast<glXGetProcAddress_func_ptr_t>(dlsym(g_actual_libgl_module_handle, "glXGetProcAddress"));
    if (!GL_ENTRYPOINT(glXGetProcAddress))
    {
        vogl_error_printf("%s: Failed getting address of glXGetProcAddress() from libGL.so.1!\n", VOGL_FUNCTION_NAME);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_proc_address_helper
//----------------------------------------------------------------------------------------------------------------------
static vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName)
{
    VOGL_FUNC_TRACER

    vogl_void_func_ptr_t pFunc = g_actual_libgl_module_handle ? reinterpret_cast<vogl_void_func_ptr_t>(dlsym(g_actual_libgl_module_handle, pName)) : NULL;

    if ((!pFunc) && (GL_ENTRYPOINT(glXGetProcAddress)))
        pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(glXGetProcAddress)(reinterpret_cast<const GLubyte *>(pName)));

    return pFunc;
}

#if 0
// HACK HACK - for testing
static void vogl_direct_gl_func_prolog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **pStack_data)
{
	printf("*** PROLOG %u\n", entrypoint_id);
}

void vogl_direct_gl_func_epilog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **pStack_data)
{
	printf("*** EPILOG %u\n", entrypoint_id);
}
#endif

//----------------------------------------------------------------------------------------------------------------------
// vogl_direct_gl_func_prolog - This function is called before EVERY single GL/GLX function call we make.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_direct_gl_func_prolog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **ppStack_data)
{
    VOGL_NOTE_UNUSED(entrypoint_id);
    VOGL_NOTE_UNUSED(pUser_data);
    VOGL_NOTE_UNUSED(ppStack_data);

    printf("* GLPROLOG %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_direct_gl_func_epilog - This function is called immediately after EVERY single GL/GLX function call we make.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_direct_gl_func_epilog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **ppStack_data)
{
    VOGL_NOTE_UNUSED(entrypoint_id);
    VOGL_NOTE_UNUSED(pUser_data);
    VOGL_NOTE_UNUSED(ppStack_data);

    printf("* GLEPILOG %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_replay_init
//----------------------------------------------------------------------------------------------------------------------
static bool vogl_replay_init(int argc, char *argv[])
{
    VOGL_FUNC_TRACER

    get_thread_safe_random().seed_from_urandom();

    colorized_console::init();
    colorized_console::set_exception_callback();
    //console::set_tool_prefix("(voglreplay) ");

    tool_print_title();

    if (!init_command_line_params(argc, argv))
        return false;

#ifdef USE_TELEMETRY
    int telemetry_level = g_command_line_params().get_value_as_int("telemetry_level", 0,
                                                                 TELEMETRY_LEVEL_MIN + 1, TELEMETRY_LEVEL_MIN, TELEMETRY_LEVEL_MAX);
    telemetry_set_level(telemetry_level);
    telemetry_tick();
#endif

    vogl_common_lib_early_init();
    vogl_common_lib_global_init();

    if (g_command_line_params().get_value_as_bool("quiet"))
        console::disable_output();

    if (g_command_line_params().get_value_as_bool("gl_debug_log"))
    {
        vogl_set_direct_gl_func_prolog(vogl_direct_gl_func_prolog, NULL);
        vogl_set_direct_gl_func_epilog(vogl_direct_gl_func_epilog, NULL);
    }

    if (!load_gl())
        return false;

    bool wrap_all_gl_calls = true;

    if (g_command_line_params().get_value_as_bool("benchmark"))
        wrap_all_gl_calls = false;

    vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper, wrap_all_gl_calls);

#if 0
	// HACK HACK - for testing
	vogl_set_direct_gl_func_prolog(vogl_direct_gl_func_prolog, NULL);
	vogl_set_direct_gl_func_epilog(vogl_direct_gl_func_epilog, NULL);
#endif

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_replay_deinit
//----------------------------------------------------------------------------------------------------------------------
static void vogl_replay_deinit()
{
    VOGL_FUNC_TRACER

    colorized_console::deinit();
}

//----------------------------------------------------------------------------------------------------------------------
// X11_Pending - from SDL
//----------------------------------------------------------------------------------------------------------------------
static int X11_Pending(Display *display)
{
    VOGL_FUNC_TRACER

    /* Flush the display connection and look to see if events are queued */
    XFlush(display);
    if (XEventsQueued(display, QueuedAlready))
    {
        return (1);
    }

    /* More drastic measures are required -- see if X is ready to talk */
    {
        static struct timeval zero_time; /* static == 0 */
        int x11_fd;
        fd_set fdset;

        x11_fd = ConnectionNumber(display);
        FD_ZERO(&fdset);
        FD_SET(x11_fd, &fdset);
        if (select(x11_fd + 1, &fdset, NULL, NULL, &zero_time) == 1)
        {
            return (XPending(display));
        }
    }

    /* Oh well, nothing is ready .. */
    return (0);
}

//----------------------------------------------------------------------------------------------------------------------
// read_state_snapshot_from_trace
//----------------------------------------------------------------------------------------------------------------------
static vogl_gl_state_snapshot *read_state_snapshot_from_trace(dynamic_string filename)
{
    VOGL_FUNC_TRACER

    timed_scope ts(VOGL_FUNCTION_NAME);

    vogl_gl_state_snapshot *pSnapshot = NULL;

    vogl_loose_file_blob_manager file_blob_manager;
    dynamic_string keyframe_trace_path(file_utils::get_pathname(filename.get_ptr()));
    file_blob_manager.init(cBMFReadable, keyframe_trace_path.get_ptr());

    dynamic_string actual_keyframe_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(filename, actual_keyframe_filename, NULL));
    if (!pTrace_reader.get())
    {
        vogl_error_printf("%s: Failed reading keyframe file %s!\n", VOGL_FUNCTION_NAME, filename.get_ptr());
        return NULL;
    }

    vogl_ctypes trace_gl_ctypes(pTrace_reader->get_sof_packet().m_pointer_sizes);

    vogl_trace_packet keyframe_trace_packet(&trace_gl_ctypes);

    bool found_snapshot = false;
    do
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("%s: Failed reading from keyframe trace file!\n", VOGL_FUNCTION_NAME);
            return NULL;
        }

        if ((read_status == vogl_trace_file_reader::cEOF) || (pTrace_reader->get_packet_type() == cTSPTEOF))
        {
            vogl_error_printf("%s: Failed finding state snapshot in keyframe file!\n", VOGL_FUNCTION_NAME);
            return NULL;
        }

        if (pTrace_reader->get_packet_type() != cTSPTGLEntrypoint)
            continue;

        if (!keyframe_trace_packet.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
        {
            vogl_error_printf("%s: Failed parsing GL entrypoint packet in keyframe file\n", VOGL_FUNCTION_NAME);
            return NULL;
        }

        const vogl_trace_gl_entrypoint_packet *pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
        gl_entrypoint_id_t entrypoint_id = static_cast<gl_entrypoint_id_t>(pGL_packet->m_entrypoint_id);

        //const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];

        if (vogl_is_swap_buffers_entrypoint(entrypoint_id) || vogl_is_draw_entrypoint(entrypoint_id) || vogl_is_make_current_entrypoint(entrypoint_id))
        {
            vogl_error_printf("Failed finding state snapshot in keyframe file!\n");
            return NULL;
        }

        switch (entrypoint_id)
        {
            case VOGL_ENTRYPOINT_glInternalTraceCommandRAD:
            {
                GLuint cmd = keyframe_trace_packet.get_param_value<GLuint>(0);
                GLuint size = keyframe_trace_packet.get_param_value<GLuint>(1);
                VOGL_NOTE_UNUSED(size);

                if (cmd == cITCRKeyValueMap)
                {
                    key_value_map &kvm = keyframe_trace_packet.get_key_value_map();

                    dynamic_string cmd_type(kvm.get_string("command_type"));
                    if (cmd_type == "state_snapshot")
                    {
                        dynamic_string id(kvm.get_string("binary_id"));
                        if (id.is_empty())
                        {
                            vogl_error_printf("%s: Missing binary_id field in glInternalTraceCommandRAD key_valye_map command type: \"%s\"\n", VOGL_FUNCTION_NAME, cmd_type.get_ptr());
                            return NULL;
                        }

                        uint8_vec snapshot_data;
                        {
                            timed_scope ts2("get_multi_blob_manager().get");
                            if (!pTrace_reader->get_multi_blob_manager().get(id, snapshot_data) || (snapshot_data.is_empty()))
                            {
                                vogl_error_printf("%s: Failed reading snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                                return NULL;
                            }
                        }

                        vogl_message_printf("%s: Deserializing state snapshot \"%s\", %u bytes\n", VOGL_FUNCTION_NAME, id.get_ptr(), snapshot_data.size());

                        json_document doc;
                        {
                            timed_scope ts2("doc.binary_deserialize");
                            if (!doc.binary_deserialize(snapshot_data) || (!doc.get_root()))
                            {
                                vogl_error_printf("%s: Failed deserializing JSON snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                                return NULL;
                            }
                        }

                        pSnapshot = vogl_new(vogl_gl_state_snapshot);

                        timed_scope ts2("pSnapshot->deserialize");
                        if (!pSnapshot->deserialize(*doc.get_root(), pTrace_reader->get_multi_blob_manager(), &trace_gl_ctypes))
                        {
                            vogl_delete(pSnapshot);
                            pSnapshot = NULL;

                            vogl_error_printf("%s: Failed deserializing snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                            return NULL;
                        }

                        found_snapshot = true;
                    }
                }

                break;
            }
            default:
                break;
        }

    } while (!found_snapshot);

    return pSnapshot;
}

//----------------------------------------------------------------------------------------------------------------------
// get_replayer_flags_from_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static uint get_replayer_flags_from_command_line_params(bool interactive_mode)
{
    uint replayer_flags = 0;

    static struct
    {
        const char *m_pCommand;
        uint m_flag;
    } s_replayer_command_line_params[] =
          {
              { "benchmark", cGLReplayerBenchmarkMode },
              { "verbose", cGLReplayerVerboseMode },
              { "force_debug_context", cGLReplayerForceDebugContexts },
              { "dump_all_packets", cGLReplayerDumpAllPackets },
              { "debug", cGLReplayerDebugMode },
              { "lock_window_dimensions", cGLReplayerLockWindowDimensions },
              { "replay_debug", cGLReplayerLowLevelDebugMode },
              { "dump_packet_blob_files_on_error", cGLReplayerDumpPacketBlobFilesOnError },
              { "dump_shaders_on_draw", cGLReplayerDumpShadersOnDraw },
              { "dump_packets_on_error", cGLReplayerDumpPacketsOnError },
              { "dump_screenshots", cGLReplayerDumpScreenshots },
              { "hash_backbuffer", cGLReplayerHashBackbuffer },
              { "dump_backbuffer_hashes", cGLReplayerDumpBackbufferHashes },
              { "sum_hashing", cGLReplayerSumHashing },
              { "dump_framebuffer_on_draw", cGLReplayerDumpFramebufferOnDraws },
              { "clear_uninitialized_bufs", cGLReplayerClearUnintializedBuffers },
              { "disable_frontbuffer_restore", cGLReplayerDisableRestoreFrontBuffer },
          };

    for (uint i = 0; i < sizeof(s_replayer_command_line_params) / sizeof(s_replayer_command_line_params[0]); i++)
        if (g_command_line_params().get_value_as_bool(s_replayer_command_line_params[i].m_pCommand))
            replayer_flags |= s_replayer_command_line_params[i].m_flag;

    if (interactive_mode && !g_command_line_params().get_value_as_bool("disable_snapshot_caching"))
        replayer_flags |= cGLReplayerSnapshotCaching;

    return replayer_flags;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_replay_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_replay_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string trace_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (trace_filename.is_empty())
    {
        vogl_error_printf("%s: No trace file specified!\n", VOGL_FUNCTION_NAME);
        return false;
    }

    dynamic_string actual_trace_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(trace_filename, actual_trace_filename, g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
    {
        vogl_error_printf("%s: File not found, or unable to determine file type of trace file \"%s\"\n", VOGL_FUNCTION_NAME, trace_filename.get_ptr());
        return false;
    }

    vogl_printf("Reading trace file %s\n", actual_trace_filename.get_ptr());

    bool interactive_mode = g_command_line_params().get_value_as_bool("interactive");

    vogl_gl_replayer replayer;

    uint replayer_flags = get_replayer_flags_from_command_line_params(interactive_mode);

    vogl_replay_window window;

    // TODO: This will create a window with default attributes, which seems fine for the majority of traces.
    // Unfortunately, some GL call streams *don't* want an alpha channel, or depth, or stencil etc. in the default framebuffer so this may become a problem.
    // Also, this design only supports a single window, which is going to be a problem with multiple window traces.
    if (!window.open(g_command_line_params().get_value_as_int("width", 0, 1024, 1, 65535), g_command_line_params().get_value_as_int("height", 0, 768, 1, 65535), g_command_line_params().get_value_as_int("msaa", 0, 0, 0, 65535)))
    {
        vogl_error_printf("%s: Failed initializing replay window\n", VOGL_FUNCTION_NAME);
        return false;
    }

    if (!replayer.init(replayer_flags, &window, pTrace_reader->get_sof_packet(), pTrace_reader->get_multi_blob_manager()))
    {
        vogl_error_printf("%s: Failed initializing GL replayer\n", VOGL_FUNCTION_NAME);
        return false;
    }

    if (replayer_flags & cGLReplayerBenchmarkMode)
    {
        // Also disable all glGetError() calls in vogl_utils.cpp.
        vogl_disable_gl_get_error();
    }

    replayer.set_swap_sleep_time(g_command_line_params().get_value_as_uint("swap_sleep"));
    replayer.set_dump_framebuffer_on_draw_prefix(g_command_line_params().get_value_as_string("dump_framebuffer_on_draw_prefix", 0, "screenshot"));
    replayer.set_screenshot_prefix(g_command_line_params().get_value_as_string("dump_screenshots_prefix", 0, "screenshot"));
    replayer.set_backbuffer_hash_filename(g_command_line_params().get_value_as_string_or_empty("dump_backbuffer_hashes"));
    replayer.set_dump_framebuffer_on_draw_frame_index(g_command_line_params().get_value_as_int("dump_framebuffer_on_draw_frame", 0, -1, 0, INT_MAX));
    replayer.set_dump_framebuffer_on_draw_first_gl_call_index(g_command_line_params().get_value_as_int("dump_framebuffer_on_draw_first_gl_call", 0, -1, 0, INT_MAX));
    replayer.set_dump_framebuffer_on_draw_last_gl_call_index(g_command_line_params().get_value_as_int("dump_framebuffer_on_draw_last_gl_call", 0, -1, 0, INT_MAX));

    XSelectInput(window.get_display(), window.get_xwindow(),
                 EnterWindowMask | LeaveWindowMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask | PropertyChangeMask | StructureNotifyMask | KeymapStateMask);

    Atom wmDeleteMessage = XInternAtom(window.get_display(), "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window.get_display(), window.get_xwindow(), &wmDeleteMessage, 1);

    Bool win_mapped = false;

    vogl_gl_state_snapshot *pSnapshot = NULL;
    int64_t snapshot_loop_start_frame = -1;
    int64_t snapshot_loop_end_frame = -1;

    vogl::hash_map<uint64_t> keys_pressed, keys_down;
    dynamic_string keyframe_base_filename(g_command_line_params().get_value_as_string("keyframe_base_filename"));
    vogl::vector<uint64_t> keyframes;
    int64_t paused_mode_frame_index = -1;
    int64_t take_snapshot_at_frame_index = g_command_line_params().get_value_as_int64("pause_on_frame", 0, -1);
    bool paused_mode = false;
    bool slow_mode = false;

    if (!keyframe_base_filename.is_empty())
    {
        find_files finder;
        if (!finder.find((keyframe_base_filename + "*.bin").get_ptr()))
        {
            vogl_error_printf("Failed finding files: %s\n", keyframe_base_filename.get_ptr());
            return false;
        }

        for (uint i = 0; i < finder.get_files().size(); i++)
        {
            dynamic_string base_name(finder.get_files()[i].m_name);
            dynamic_string ext(base_name);
            file_utils::get_extension(ext);
            if (ext != "bin")
                continue;

            file_utils::remove_extension(base_name);
            int underscore_ofs = base_name.find_right('_');
            if (underscore_ofs < 0)
                continue;

            dynamic_string frame_index_str(base_name.right(underscore_ofs + 1));
            if (frame_index_str.is_empty())
                continue;

            const char *pFrame_index = frame_index_str.get_ptr();

            uint64_t frame_index = 0;
            if (!string_ptr_to_uint64(pFrame_index, frame_index))
                continue;

            keyframes.push_back(frame_index);
            printf("Found keyframe file %s index %" PRIu64 "\n", finder.get_files()[i].m_fullname.get_ptr(), frame_index);
        }

        keyframes.sort();
    }

    int loop_frame = g_command_line_params().get_value_as_int("loop_frame", 0, -1);
    int loop_len = math::maximum<int>(g_command_line_params().get_value_as_int("loop_len", 0, 1), 1);
    int loop_count = math::maximum<int>(g_command_line_params().get_value_as_int("loop_count", 0, cINT32_MAX), 1);
    int draw_kill_max_thresh = g_command_line_params().get_value_as_int("draw_kill_max_thresh", 0, -1);
    bool endless_mode = g_command_line_params().get_value_as_bool("endless");

    bool multitrim_mode = g_command_line_params().get_value_as_bool("multitrim");
    int multitrim_interval = g_command_line_params().get_value_as_int("multitrim_interval", 0, 1, 1);
    int multitrim_frames_remaining = 0;

    int64_t write_snapshot_index = g_command_line_params().get_value_as_int64("write_snapshot_call", 0, -1, 0);
    dynamic_string write_snapshot_filename = g_command_line_params().get_value_as_string("write_snapshot_file", 0, "state_snapshot.json");

    int64_t trim_call_index = g_command_line_params().get_value_as_int64("trim_call", 0, -1, 0);
    vogl::vector<uint> trim_frames(g_command_line_params().get_count("trim_frame"));
    for (uint i = 0; i < trim_frames.size(); i++)
    {
        bool parsed_successfully;
        trim_frames[i] = g_command_line_params().get_value_as_uint("trim_frame", i, 0, 0, cUINT32_MAX, 0, &parsed_successfully);
        if (!parsed_successfully)
        {
            vogl_error_printf("%s: Failed parsing -trim_frame at index %u\n", VOGL_FUNCTION_NAME, i);
            return false;
        }
    }

    vogl::vector<dynamic_string> trim_filenames(g_command_line_params().get_count("trim_file"));
    for (uint i = 0; i < trim_filenames.size(); i++)
    {
        dynamic_string filename(g_command_line_params().get_value_as_string("trim_file", i));

        if (filename.is_empty())
        {
            vogl_error_printf("%s: Invalid trim filename\n", VOGL_FUNCTION_NAME);
            return false;
        }

        trim_filenames[i] = filename;

        if (file_utils::add_default_extension(trim_filenames[i], ".bin"))
            vogl_message_printf("%s: Trim output filename \"%s\", didn't have an extension, appended \".bin\" to the filename: %s\n", VOGL_FUNCTION_NAME, filename.get_ptr(), trim_filenames[i].get_ptr());
    }

    vogl::vector<uint> trim_lens(g_command_line_params().get_count("trim_len"));
    for (uint i = 0; i < trim_lens.size(); i++)
    {
        bool parsed_successfully;
        trim_lens[i] = g_command_line_params().get_value_as_uint("trim_len", i, 1, 0, cUINT32_MAX, 0, &parsed_successfully);
        if (!parsed_successfully)
        {
            vogl_error_printf("%s: Failed parsing -trim_len at index %u\n", VOGL_FUNCTION_NAME, i);
            return false;
        }
    }

    uint num_trim_files_written = 0;
    uint highest_frame_to_trim = 0;

    vogl_loose_file_blob_manager trim_file_blob_manager;

    timer tm;

    if (trim_frames.size())
    {
        if (trim_filenames.is_empty())
        {
            console::error("%s: -trim_frame specified without specifying at least one -trim_file or -trim_call\n", VOGL_FUNCTION_NAME);
            return false;
        }
    }

    if (write_snapshot_index >= 0)
    {
        if ((multitrim_mode) || (trim_frames.size()) || (trim_lens.size()) || (trim_call_index >= 0))
        {
            console::warning("%s: Can't write snapshot and trim at the same time, disabling trimming\n", VOGL_FUNCTION_NAME);

            multitrim_mode = false;
            trim_frames.clear();
            trim_lens.clear();
            trim_call_index = -1;
        }

        if (draw_kill_max_thresh > 0)
        {
            console::warning("%s: Write snapshot mode is enabled, disabling -draw_kill_max_thresh\n", VOGL_FUNCTION_NAME);
            draw_kill_max_thresh = -1;
        }

        if (endless_mode)
        {
            console::warning("%s: Write snapshot mode is enabled, disabling -endless\n", VOGL_FUNCTION_NAME);
            endless_mode = false;
        }
    }
    else if ((trim_frames.size()) || (trim_call_index != -1))
    {
        if (trim_filenames.is_empty())
        {
            console::error("%s: Must also specify at least one -trim_file\n", VOGL_FUNCTION_NAME);
            return false;
        }

        if (trim_call_index != -1)
        {
            if (trim_frames.size())
            {
                console::error("%s: Can't specify both -trim_call and -trim_frame\n", VOGL_FUNCTION_NAME);
                return false;
            }

            if (multitrim_mode)
            {
                console::error("%s: Can't specify both -trim_call and -multitrim\n", VOGL_FUNCTION_NAME);
                return false;
            }

            if (trim_filenames.size() > 1)
            {
                console::warning("%s: -trim_call specified but more than 1 -trim_file specified - ignoring all but first -trim_file's\n", VOGL_FUNCTION_NAME);
                trim_filenames.resize(1);
            }
        }

        trim_file_blob_manager.init(cBMFWritable);

        if (trim_frames.size() > 1)
        {
            if ((trim_filenames.size() > 1) && (trim_filenames.size() != trim_frames.size()))
            {
                console::error("%s: More than 1 -trim_frame was specified, and more than 1 -trim_file was specified, but the number of -trim_file's must match the number of -trim_frame's (or only specify one -trim_file to use it as a base filename)\n", VOGL_FUNCTION_NAME);
                return false;
            }

            if ((trim_lens.size() > 1) && (trim_lens.size() != trim_frames.size()))
            {
                console::error("%s: More than 1 -trim_frame was specified, and more than 1 -trim_len's was specified, but the number of -trim_len's must match the number of -trim_frame's (or only specify one -trim_len to use it for all trims)\n", VOGL_FUNCTION_NAME);
                return false;
            }
        }

        if ((multitrim_mode) && (trim_filenames.size() > 1))
        {
            console::warning("%s: Only 1 filename needs to be specified in -multitrim mode\n", VOGL_FUNCTION_NAME);
        }

        if (loop_frame != -1)
        {
            console::warning("%s: Trim is enabled, disabling -loop_frame\n", VOGL_FUNCTION_NAME);
            loop_frame = -1;
            loop_len = 1;
            loop_count = 1;
        }

        if (draw_kill_max_thresh > 0)
        {
            console::warning("%s: Trim is enabled, disabling -draw_kill_max_thresh\n", VOGL_FUNCTION_NAME);
            draw_kill_max_thresh = -1;
        }

        if (endless_mode)
        {
            console::warning("%s: Trim is enabled, disabling -endless\n", VOGL_FUNCTION_NAME);
            endless_mode = false;
        }

        if (trim_call_index == -1)
        {
            for (uint tf = 0; tf < trim_frames.size(); tf++)
            {
                uint len = 1;
                if (trim_lens.size())
                {
                    if (trim_lens.size() < trim_frames.size())
                        len = trim_lens[0];
                    else
                        len = trim_lens[tf];
                }
                len = math::maximum(len, 1U);
                highest_frame_to_trim = math::maximum<uint>(highest_frame_to_trim, trim_frames[tf] + len - 1);
            }
        }
    }
    else
    {
        if (trim_filenames.size())
            console::warning("%s: -trim_file was specified, but -trim_frame was not specified so ignoring\n", VOGL_FUNCTION_NAME);
        if (trim_lens.size())
            console::warning("%s: -trim_len was specified, but -trim_frame was not specified so ignoring\n", VOGL_FUNCTION_NAME);

        trim_filenames.clear();
        trim_lens.clear();
    }

    tm.start();

    for (;;)
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "Main Loop");

        while (X11_Pending(window.get_display()))
        {
            XEvent newEvent;

            // Watch for new X eventsn
            XNextEvent(window.get_display(), &newEvent);

            switch (newEvent.type)
            {
                case KeyPress:
                {
                    KeySym xsym = XLookupKeysym(&newEvent.xkey, 0);

                    //printf("KeyPress 0%04" PRIX64 "%" PRIu64 "\n", (uint64_t)xsym, (uint64_t)xsym);

                    keys_down.insert(xsym);
                    keys_pressed.insert(xsym);

                    break;
                }
                case KeyRelease:
                {
                    KeySym xsym = XLookupKeysym(&newEvent.xkey, 0);

                    //printf("KeyRelease 0x%04" PRIX64 " %" PRIu64 "\n", (uint64_t)xsym, (uint64_t)xsym);

                    keys_down.erase(xsym);

                    break;
                }
                case FocusIn:
                case FocusOut:
                {
                    //printf("FocusIn/FocusOut\n");

                    keys_down.reset();

                    break;
                }
                case MappingNotify:
                {
                    //XRefreshKeyboardMapping(&newEvent);
                    break;
                }
                case UnmapNotify:
                {
                    //printf("UnmapNotify\n");

                    win_mapped = false;

                    keys_down.reset();

                    break;
                }
                case MapNotify:
                {
                    //printf("MapNotify\n");

                    win_mapped = true;

                    keys_down.reset();

                    if (!replayer.update_window_dimensions())
                        goto error_exit;

                    break;
                }
                case ConfigureNotify:
                {
                    if (!replayer.update_window_dimensions())
                        goto error_exit;

                    break;
                }
                case DestroyNotify:
                {
                    vogl_message_printf("Exiting\n");
                    goto normal_exit;
                }
                case ClientMessage:
                {
                    if (newEvent.xclient.data.l[0] == (int)wmDeleteMessage)
                    {
                        vogl_message_printf("Exiting\n");
                        goto normal_exit;
                    }

                    break;
                }
                default:
                    break;
            }
        }

        // TODO: Massively refactor this! Move the replayer into a class, etc.
        if (interactive_mode)
        {
            if (!win_mapped)
            {
                vogl_sleep(10);
                continue;
            }

            tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "Interactive");

            // Interactive mode is more of a test bad to validate a bunch of classes. It's kind of narly because the replayer's object can be in odd intermediate/pending states during window
            // resizes - hopefully this complexity will go away once we get pbuffer's or whatever in.
            vogl_debug_printf("%s: At frame boundary: %u, beginning of frame %u, pause frame %" PRIi64 ", taking snapshot at frame %" PRIi64 "\n", VOGL_FUNCTION_NAME, replayer.get_at_frame_boundary(), replayer.get_frame_index(), paused_mode_frame_index, take_snapshot_at_frame_index);

            if (keys_pressed.contains('c'))
            {
                keys_pressed.erase('c');
                if (replayer.is_valid())
                {
                    dynamic_string filename;
                    for (uint i = 0; i < 10000000; i++)
                    {
                        filename.format("screenshot_%06u.png", i);
                        if (!file_utils::does_file_exist(filename.get_ptr()))
                            break;
                    }

                    replayer.dump_frontbuffer_screenshot_before_next_swap(filename);
                }
            }

            if (keys_pressed.contains('s'))
            {
                keys_pressed.erase('s');
                slow_mode = !slow_mode;
            }

            // When paused, we'll NOT be at a frame boundary because the prev. loop applied a state snapshot (which will be pending)
            if (replayer.get_at_frame_boundary() && !replayer.get_pending_apply_snapshot())
            {
                bool take_new_snapshot = false;

                // See if we've scheduled a snapshot at this frame
                if ((int64_t)replayer.get_frame_index() == take_snapshot_at_frame_index)
                {
                    take_new_snapshot = true;

                    take_snapshot_at_frame_index = -1;
                }
                // Check for pausing
                else if (keys_pressed.contains(XK_space))
                {
                    keys_pressed.erase(XK_space);

                    if (paused_mode)
                    {
                        console::info("Unpausing\n");

                        keys_pressed.erase(XK_space);

                        vogl_delete(pSnapshot);
                        pSnapshot = NULL;

                        paused_mode_frame_index = -1;

                        paused_mode = false;
                    }
                    else
                    {
                        console::info("Pausing\n");

                        paused_mode = true;

                        take_new_snapshot = true;
                    }
                }

                // Snapshot the current state
                if (take_new_snapshot)
                {
                    vogl_delete(pSnapshot);
                    pSnapshot = NULL;

                    pSnapshot = replayer.snapshot_state();
                    if (!pSnapshot)
                    {
                        vogl_error_printf("%s: Snapshot failed!\n", VOGL_FUNCTION_NAME);
                        goto error_exit;
                    }

                    if (g_command_line_params().get_value_as_bool("debug_test_snapshot_serialization"))
                    {
                        // Obviously, this crap is only for debugging.
                        vogl_memory_blob_manager mem_blob_manager;
                        mem_blob_manager.init(cBMFReadWrite);

                        json_document temp_json_doc;
                        if (!pSnapshot->serialize(*temp_json_doc.get_root(), mem_blob_manager, &replayer.get_trace_gl_ctypes()))
                        {
                            console::error("%s: Failed serializing state snapshot!\n", VOGL_FUNCTION_NAME);
                        }
                        else
                        {
                            uint8_vec ubj_data;
                            temp_json_doc.binary_serialize(ubj_data);
                            temp_json_doc.binary_deserialize(ubj_data);
                            ubj_data.clear();

                            vogl_gl_state_snapshot *pNew_snapshot = vogl_new(vogl_gl_state_snapshot);

                            if (!pNew_snapshot->deserialize(*temp_json_doc.get_root(), mem_blob_manager, &replayer.get_trace_gl_ctypes()))
                            {
                                console::error("%s: Failed deserializing state snapshot!\n", VOGL_FUNCTION_NAME);
                            }
                            else
                            {
                                vogl_delete(pSnapshot);

                                pSnapshot = pNew_snapshot;
                            }
                        }
                    }

                    paused_mode_frame_index = replayer.get_frame_index();

                    paused_mode = true;
                }
            }

            // Begin processing the next frame
            bool applied_snapshot = false;
            vogl_gl_replayer::status_t status = replayer.process_pending_window_resize(&applied_snapshot);

            if (status == vogl_gl_replayer::cStatusOK)
            {
                if (replayer.get_at_frame_boundary())
                {
                    const char *pWindow_name = (sizeof(void *) == sizeof(uint32)) ? "voglreplay 32-bit" : "voglreplay 64-bit";
                    dynamic_string window_title(cVarArg, "%s: File: %s Frame %u %s %s", pWindow_name, trace_filename.get_ptr(), replayer.get_frame_index(), paused_mode ? "PAUSED" : "", keyframes.find_sorted(replayer.get_frame_index()) >= 0 ? "(At Keyframe)" : "");
                    window.set_title(window_title.get_ptr());
                }

                // At this point, if we're paused the frame snapshot as been applied, and we're just about going to replay the frame's commands.
                if (((applied_snapshot) || (replayer.get_at_frame_boundary())) &&
                    (keys_pressed.contains('t') || keys_pressed.contains('j')))
                {
                    uint64_t frame_to_trim;
                    if (paused_mode)
                        frame_to_trim = paused_mode_frame_index;
                    else
                        frame_to_trim = replayer.get_frame_index();

                    dynamic_string trim_name;
                    for (uint i = 0; i < 1000000; i++)
                    {
                        trim_name.format("trim_%06" PRIu64 "_%u", frame_to_trim, i);
                        if (!file_utils::does_dir_exist(trim_name.get_ptr()))
                            break;
                    }

                    if (!file_utils::create_directory(trim_name.get_ptr()))
                    {
                        vogl_error_printf("%s: Failed creating trim directory %s\n", VOGL_FUNCTION_NAME, trim_name.get_ptr());
                    }
                    else
                    {
                        //$ TODO WSHADOW: This declaration shadows declaration towards top of function.
                        vogl_loose_file_blob_manager trim_file_blob_manager;
                        trim_file_blob_manager.init(cBMFReadWrite, trim_name.get_ptr());

                        dynamic_string trim_filename(trim_name + "/" + trim_name + ".bin");
                        dynamic_string snapshot_id;
                        uint write_trim_file_flags = vogl_gl_replayer::cWriteTrimFileFromStartOfFrame | (g_command_line_params().get_value_as_bool("no_trim_optimization") ? 0 : vogl_gl_replayer::cWriteTrimFileOptimizeSnapshot);
                        if (replayer.write_trim_file(write_trim_file_flags, trim_filename, 1, *pTrace_reader, &snapshot_id))
                        {
                            dynamic_string json_trim_base_filename(trim_name + "/j" + trim_name);

                            char voglreplay_exec_filename[1024];
                            file_utils::get_exec_filename(voglreplay_exec_filename, sizeof(voglreplay_exec_filename));

                            dynamic_string convert_to_json_spawn_str(cVarArg, "%s --dump \"%s\" \"%s\"", voglreplay_exec_filename, trim_filename.get_ptr(), json_trim_base_filename.get_ptr());
                            if (system(convert_to_json_spawn_str.get_ptr()) != 0)
                            {
                                vogl_error_printf("%s: Failed running voglreplay: %s\n", VOGL_FUNCTION_NAME, convert_to_json_spawn_str.get_ptr());
                            }
                            else
                            {
                                dynamic_string json_trim_full_filename(trim_name + "/j" + trim_name + "_000000.json");

                                dynamic_string view_json_spawn_str(cVarArg, "np \"%s\" &", json_trim_full_filename.get_ptr());
                                system(view_json_spawn_str.get_ptr());
                            }

                            if (keys_pressed.contains('j'))
                            {
                                dynamic_string workdir(".");
                                file_utils::full_path(workdir);

                                dynamic_string replay_json_spawn_str(cVarArg, "konsole --workdir \"%s\" --hold -e \"%s\" --endless \"%s\" &", workdir.get_ptr(), voglreplay_exec_filename, json_trim_base_filename.get_ptr());
                                system(replay_json_spawn_str.get_ptr());
                            }
                        }
                    }

                    keys_pressed.erase('t');
                    keys_pressed.erase('j');
                }

                // Now replay the next frame's GL commands up to the swap
                status = replayer.process_frame(*pTrace_reader);
            }

            if (status == vogl_gl_replayer::cStatusHardFailure)
                break;

            if ((slow_mode) && (!paused_mode))
                vogl_sleep(100);

            int64_t seek_to_target_frame = -1;
            bool seek_to_closest_keyframe = false;
            int seek_to_closest_frame_dir_bias = 0;

            if (status == vogl_gl_replayer::cStatusAtEOF)
            {
                vogl_message_printf("%s: At trace EOF, frame index %u\n", VOGL_FUNCTION_NAME, replayer.get_frame_index());

                // Right after the last swap in the file, either rewind or pause back on the last frame
                if ((paused_mode) && (replayer.get_frame_index()))
                {
                    seek_to_target_frame = replayer.get_frame_index() - 1;
                    take_snapshot_at_frame_index = -1;
                }
                else
                {
                    vogl_printf("Resetting state and rewinding back to frame 0\n");

                    replayer.reset_state();

                    if (!pTrace_reader->seek_to_frame(0))
                    {
                        vogl_error_printf("%s: Failed rewinding trace reader!\n", VOGL_FUNCTION_NAME);
                        goto error_exit;
                    }

                    take_snapshot_at_frame_index = -1;
                    paused_mode_frame_index = -1;
                    paused_mode = false;
                }

                vogl_delete(pSnapshot);
                pSnapshot = NULL;
            }
            else if (replayer.get_at_frame_boundary() && (!replayer.get_pending_apply_snapshot()))
            {
                // Rewind to beginning
                if (keys_pressed.contains('r'))
                {
                    bool ctrl = (keys_down.contains(XK_Control_L) || keys_down.contains(XK_Control_R));
                    keys_pressed.erase('r');

                    vogl_delete(pSnapshot);
                    pSnapshot = NULL;

                    if ((paused_mode) && (ctrl))
                        seek_to_target_frame = paused_mode_frame_index;

                    take_snapshot_at_frame_index = -1;
                    paused_mode_frame_index = -1;
                    paused_mode = false;

                    vogl_printf("Resetting state and rewinding back to frame 0\n");

                    replayer.reset_state();

                    if (!pTrace_reader->seek_to_frame(0))
                    {
                        vogl_error_printf("%s: Failed rewinding trace reader!\n", VOGL_FUNCTION_NAME);
                        goto error_exit;
                    }
                }
                // Seek to last frame
                else if (keys_pressed.contains('e'))
                {
                    keys_pressed.erase('e');

                    if (paused_mode)
                    {
                        vogl_delete(pSnapshot);
                        pSnapshot = NULL;

                        paused_mode_frame_index = -1;
                        take_snapshot_at_frame_index = -1;

                        vogl_printf("Seeking to last frame\n");

                        int64_t max_frame_index = pTrace_reader->get_max_frame_index();
                        if (max_frame_index < 0)
                        {
                            vogl_error_printf("%s: Failed determining the total number of trace frames!\n", VOGL_FUNCTION_NAME);
                            goto error_exit;
                        }

                        // "frames" are actually "swaps" to the tracer/replayer, so don't seek to the "last" swap (because no rendering will follow that one), but the one before
                        seek_to_target_frame = max_frame_index - 1;
                    }
                }
                // Check if paused and process seek related keypresses
                else if ((paused_mode) && (pSnapshot))
                {
                    int num_key;
                    for (num_key = '0'; num_key <= '9'; num_key++)
                    {
                        if (keys_pressed.contains(num_key))
                        {
                            keys_pressed.erase(num_key);
                            break;
                        }
                    }

                    if (num_key <= '9')
                    {
                        int64_t max_frame_index = pTrace_reader->get_max_frame_index();
                        if (max_frame_index < 0)
                        {
                            vogl_error_printf("%s: Failed determining the total number of trace frames!\n", VOGL_FUNCTION_NAME);
                            goto error_exit;
                        }

                        float fraction = ((num_key - '0') + 1) / 11.0f;

                        seek_to_target_frame = math::clamp<int64_t>(static_cast<int64_t>(max_frame_index * fraction + .5f), 0, max_frame_index - 1);
                        seek_to_closest_keyframe = true;
                    }
                    else if (keys_pressed.contains(XK_Left) || keys_pressed.contains(XK_Right))
                    {
                        int dir = keys_pressed.contains(XK_Left) ? -1 : 1;

                        bool shift = (keys_down.contains(XK_Shift_L) || keys_down.contains(XK_Shift_R));
                        bool ctrl = (keys_down.contains(XK_Control_L) || keys_down.contains(XK_Control_R));

                        int mag = 1;
                        if ((shift) && (ctrl))
                            mag = 500;
                        else if (shift)
                            mag = 10;
                        else if (ctrl)
                            mag = 100;

                        int rel = dir * mag;

                        int64_t target_frame_index = math::maximum<int64_t>(0, paused_mode_frame_index + rel);

                        seek_to_target_frame = target_frame_index;

                        keys_pressed.erase(XK_Left);
                        keys_pressed.erase(XK_Right);

                        if ((keyframes.size()) && (keys_down.contains(XK_Alt_L) || keys_down.contains(XK_Alt_R)))
                        {
                            uint keyframe_array_index = 0;
                            for (keyframe_array_index = 1; keyframe_array_index < keyframes.size(); keyframe_array_index++)
                                if ((int64_t)keyframes[keyframe_array_index] > paused_mode_frame_index)
                                    break;

                            if (dir < 0)
                            {
                                if ((paused_mode_frame_index == static_cast<int64_t>(keyframes[keyframe_array_index - 1])) && (keyframe_array_index > 1))
                                    keyframe_array_index = keyframe_array_index - 2;
                                else
                                    keyframe_array_index = keyframe_array_index - 1;
                            }
                            else
                            {
                                if (keyframe_array_index < keyframes.size())
                                {
                                    if ((paused_mode_frame_index == static_cast<int64_t>(keyframes[keyframe_array_index])) && ((keyframe_array_index + 1) < keyframes.size()))
                                        keyframe_array_index = keyframe_array_index + 1;
                                    //else
                                    //   keyframe_array_index = keyframe_array_index;
                                }
                                else
                                    keyframe_array_index = keyframe_array_index - 1;
                            }

                            seek_to_target_frame = keyframes[keyframe_array_index];

                            if (mag > 1)
                            {
                                if (((dir < 0) && (target_frame_index < seek_to_target_frame)) || (target_frame_index > seek_to_target_frame))
                                    seek_to_target_frame = target_frame_index;

                                seek_to_closest_keyframe = true;
                                seek_to_closest_frame_dir_bias = dir;
                            }

                            console::info("Seeking to keyframe array index %u, target frame %" PRIu64 "\n", keyframe_array_index, seek_to_target_frame);
                        }
                    }
                    // Check for unpause
                    else if (keys_pressed.contains(XK_space))
                    {
                        console::info("Unpausing\n");

                        keys_pressed.erase(XK_space);

                        vogl_delete(pSnapshot);
                        pSnapshot = NULL;

                        paused_mode_frame_index = -1;

                        paused_mode = false;
                    }
                    else
                    {
                        // We're paused so seek back to the command right after the swap we're paused on, apply the paused frame's snapshot, then play the frame over
                        status = replayer.begin_applying_snapshot(pSnapshot, false);
                        if ((status != vogl_gl_replayer::cStatusOK) && (status != vogl_gl_replayer::cStatusResizeWindow))
                        {
                            vogl_error_printf("%s: Failed applying snapshot!\n", VOGL_FUNCTION_NAME);
                            goto error_exit;
                        }

                        pTrace_reader->seek_to_frame(static_cast<uint>(paused_mode_frame_index));
                    }
                }
            }

            // Seek to target frame
            if (seek_to_target_frame != -1)
            {
                vogl_delete(pSnapshot);
                pSnapshot = NULL;
                paused_mode_frame_index = -1;

                if ((int64_t)replayer.get_frame_index() == seek_to_target_frame)
                    take_snapshot_at_frame_index = seek_to_target_frame;
                else
                {
                    uint keyframe_array_index = 0;
                    for (keyframe_array_index = 1; keyframe_array_index < keyframes.size(); keyframe_array_index++)
                        if (static_cast<int64_t>(keyframes[keyframe_array_index]) > seek_to_target_frame)
                            break;

                    if ((!keyframes.is_empty()) && (static_cast<int64_t>(keyframes[keyframe_array_index - 1]) <= seek_to_target_frame))
                    {
                        int keyframe_array_index_to_use = keyframe_array_index - 1;

                        if (seek_to_closest_keyframe)
                        {
                            if (!seek_to_closest_frame_dir_bias)
                            {
                                if ((keyframe_array_index_to_use + 1U) < keyframes.size())
                                {
                                    if (llabs(static_cast<int64_t>(keyframes[keyframe_array_index_to_use + 1]) - seek_to_target_frame) < llabs(static_cast<int64_t>(keyframes[keyframe_array_index_to_use]) - seek_to_target_frame))
                                    {
                                        keyframe_array_index_to_use++;
                                    }
                                }
                            }
                            else if (seek_to_closest_frame_dir_bias > 0)
                            {
                                if ((keyframe_array_index_to_use + 1U) < keyframes.size())
                                {
                                    if (static_cast<int64_t>(keyframes[keyframe_array_index_to_use]) <= seek_to_target_frame)
                                    {
                                        keyframe_array_index_to_use++;
                                    }
                                }
                            }
                        }

                        int64_t keyframe_index = keyframes[keyframe_array_index_to_use];

                        if (seek_to_closest_keyframe)
                            seek_to_target_frame = keyframe_index;

                        vogl_debug_printf("Seeking to target frame %" PRIu64 "\n", seek_to_target_frame);

                        dynamic_string keyframe_filename(cVarArg, "%s_%06" PRIu64 ".bin", keyframe_base_filename.get_ptr(), keyframe_index);

                        vogl_gl_state_snapshot *pKeyframe_snapshot = read_state_snapshot_from_trace(keyframe_filename);
                        if (!pKeyframe_snapshot)
                            goto error_exit;

                        bool delete_snapshot_after_applying = true;
                        if (seek_to_target_frame == keyframe_index)
                            delete_snapshot_after_applying = false;

                        status = replayer.begin_applying_snapshot(pKeyframe_snapshot, delete_snapshot_after_applying);
                        if ((status != vogl_gl_replayer::cStatusOK) && (status != vogl_gl_replayer::cStatusResizeWindow))
                        {
                            vogl_error_printf("%s: Failed applying snapshot!\n", VOGL_FUNCTION_NAME);
                            goto error_exit;
                        }

                        pKeyframe_snapshot->set_frame_index(static_cast<uint>(keyframe_index));

                        if (!pTrace_reader->seek_to_frame(static_cast<uint>(keyframe_index)))
                        {
                            vogl_error_printf("%s: Failed seeking to keyframe!\n", VOGL_FUNCTION_NAME);
                            goto error_exit;
                        }

                        if (seek_to_target_frame == keyframe_index)
                        {
                            pSnapshot = pKeyframe_snapshot;
                            paused_mode_frame_index = seek_to_target_frame;
                        }
                        else
                            take_snapshot_at_frame_index = seek_to_target_frame;
                    }
                    else
                    {
                        if (seek_to_target_frame < static_cast<int64_t>(replayer.get_frame_index()))
                        {
                            replayer.reset_state();
                            pTrace_reader->seek_to_frame(0);
                        }

                        take_snapshot_at_frame_index = seek_to_target_frame;
                    }
                }
            }
        }
        else // !interactive_mode
        {
            tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "!Interactive");

            if (replayer.get_at_frame_boundary())
            {
                if (trim_frames.size())
                {
                    bool should_trim = false;
                    uint tf = 0;
                    uint len = 1;

                    for (tf = 0; tf < trim_frames.size(); tf++)
                    {
                        if (trim_lens.size())
                        {
                            if (trim_lens.size() < trim_frames.size())
                                len = trim_lens[0];
                            else
                                len = trim_lens[tf];
                        }
                        len = math::maximum(len, 1U);

                        if (multitrim_mode)
                        {
                            if ((replayer.get_frame_index() >= trim_frames[tf]) && (replayer.get_frame_index() < (trim_frames[tf] + math::maximum(len, 1U))))
                            {
                                should_trim = true;
                                break;
                            }
                        }
                        else
                        {
                            if (replayer.get_frame_index() == trim_frames[tf])
                            {
                                should_trim = true;
                                break;
                            }
                        }
                    }

                    if (multitrim_mode)
                    {
                        if (should_trim)
                        {
                            if (multitrim_frames_remaining)
                            {
                                should_trim = false;
                            }
                            else
                            {
                                multitrim_frames_remaining = multitrim_interval;
                            }

                            multitrim_frames_remaining--;
                        }
                        else
                        {
                            multitrim_frames_remaining = 0;
                        }
                    }
                    //printf("multitrim_interval: %u %u\n", multitrim_frames_remaining, multitrim_interval);

                    if (should_trim)
                    {
                        dynamic_string filename;

                        if ((multitrim_mode) || (trim_filenames.size() < trim_frames.size()))
                        {
                            filename = trim_filenames[0];

                            if ((multitrim_mode) || (trim_frames.size() > 1))
                            {
                                dynamic_string drive, dir, fname, ext;
                                file_utils::split_path(filename.get_ptr(), &drive, &dir, &fname, &ext);

                                dynamic_string new_fname(cVarArg, "%s_%06u", fname.get_ptr(), replayer.get_frame_index());

                                file_utils::combine_path_and_extension(filename, &drive, &dir, &new_fname, &ext);
                            }
                        }
                        else
                        {
                            filename = trim_filenames[tf];
                        }

                        dynamic_string trim_path(file_utils::get_pathname(filename.get_ptr()));
                        trim_file_blob_manager.set_path(trim_path);

                        file_utils::create_directories(trim_path, false);

                        uint write_trim_file_flags = vogl_gl_replayer::cWriteTrimFileFromStartOfFrame | (g_command_line_params().get_value_as_bool("no_trim_optimization") ? 0 : vogl_gl_replayer::cWriteTrimFileOptimizeSnapshot);
                        if (!replayer.write_trim_file(write_trim_file_flags, filename, multitrim_mode ? 1 : len, *pTrace_reader))
                            goto error_exit;

                        num_trim_files_written++;

                        if (!multitrim_mode)
                        {
                            if (num_trim_files_written == trim_frames.size())
                            {
                                vogl_message_printf("%s: All requested trim files written, stopping replay\n", VOGL_FUNCTION_NAME);
                                goto normal_exit;
                            }
                        }
                    }

                    if (multitrim_mode)
                    {
                        uint64_t next_frame_index = replayer.get_frame_index() + 1;

                        if (next_frame_index > highest_frame_to_trim)
                        {
                            vogl_message_printf("%s: No more frames to trim, stopping replay\n", VOGL_FUNCTION_NAME);
                            goto normal_exit;
                        }
                    }
                }

                if ((!pSnapshot) && (loop_frame != -1) && (static_cast<int64_t>(replayer.get_frame_index()) == loop_frame))
                {
                    vogl_debug_printf("%s: Capturing replayer state at start of frame %u\n", VOGL_FUNCTION_NAME, replayer.get_frame_index());

                    pSnapshot = replayer.snapshot_state();

                    if (pSnapshot)
                    {
                        vogl_printf("Snapshot succeeded\n");

                        snapshot_loop_start_frame = pTrace_reader->get_cur_frame();
                        snapshot_loop_end_frame = pTrace_reader->get_cur_frame() + loop_len;

                        if (draw_kill_max_thresh > 0)
                        {
                            replayer.set_frame_draw_counter_kill_threshold(0);
                        }

                        vogl_debug_printf("%s: Loop start: %" PRIi64 " Loop end: %" PRIi64 "\n", VOGL_FUNCTION_NAME, snapshot_loop_start_frame, snapshot_loop_end_frame);
                    }
                    else
                    {
                        vogl_error_printf("Snapshot failed!\n");

                        loop_frame = -1;
                    }
                }
            }

            vogl_gl_replayer::status_t status = replayer.process_pending_window_resize();
            if (status == vogl_gl_replayer::cStatusOK)
            {
                for (;;)
                {
                    status = replayer.process_next_packet(*pTrace_reader);

                    if ((status != vogl_gl_replayer::cStatusHardFailure) && (status != vogl_gl_replayer::cStatusAtEOF))
                    {
                        if ((write_snapshot_index >= 0) && (write_snapshot_index == replayer.get_last_processed_call_counter()))
                        {
                            dynamic_string filename(write_snapshot_filename);

                            dynamic_string write_snapshot_path(file_utils::get_pathname(filename.get_ptr()));
                            trim_file_blob_manager.init(cBMFReadWrite, write_snapshot_path.get_ptr());

                            file_utils::create_directories(write_snapshot_path, false);

                            pSnapshot = replayer.snapshot_state();

                            if (pSnapshot)
                            {
                                vogl_printf("Snapshot succeeded at call counter %" PRIu64 "\n", replayer.get_last_processed_call_counter());

                                vogl_null_blob_manager null_blob_manager;
                                null_blob_manager.init(cBMFReadWrite);

                                json_document doc;
                                vogl_blob_manager *pBlob_manager = g_command_line_params().get_value_as_bool("write_snapshot_blobs") ? static_cast<vogl_blob_manager *>(&trim_file_blob_manager) : static_cast<vogl_blob_manager *>(&null_blob_manager);
                                if (!pSnapshot->serialize(*doc.get_root(), *pBlob_manager, &replayer.get_trace_gl_ctypes()))
                                {
                                    vogl_error_printf("Failed serializing state snapshot document!\n");
                                }
                                else if (!doc.serialize_to_file(filename.get_ptr(), true))
                                {
                                    vogl_error_printf("Failed writing state snapshot to file \"%s\"!\n", filename.get_ptr());
                                }
                                else
                                {
                                    vogl_printf("Successfully wrote JSON snapshot to file \"%s\"\n", filename.get_ptr());
                                }

                                vogl_delete(pSnapshot);
                                pSnapshot = NULL;
                            }
                            else
                            {
                                vogl_error_printf("Snapshot failed!\n");
                            }

                            goto normal_exit;
                        }
                        else if ((trim_call_index >= 0) && (trim_call_index == replayer.get_last_processed_call_counter()))
                        {
                            dynamic_string filename(trim_filenames[0]);

                            dynamic_string trim_path(file_utils::get_pathname(filename.get_ptr()));
                            trim_file_blob_manager.set_path(trim_path);

                            file_utils::create_directories(trim_path, false);

                            if (!replayer.write_trim_file(0, filename, trim_lens.size() ? trim_lens[0] : 1, *pTrace_reader, NULL))
                                goto error_exit;

                            vogl_message_printf("%s: Trim file written, stopping replay\n", VOGL_FUNCTION_NAME);
                            goto normal_exit;
                        }
                    }

                    if ((status == vogl_gl_replayer::cStatusNextFrame) || (status == vogl_gl_replayer::cStatusResizeWindow) || (status == vogl_gl_replayer::cStatusAtEOF) || (status == vogl_gl_replayer::cStatusHardFailure))
                        break;
                }
            }

            if (status == vogl_gl_replayer::cStatusHardFailure)
                break;

            if (status == vogl_gl_replayer::cStatusAtEOF)
            {
                vogl_message_printf("%s: At trace EOF, frame index %u\n", VOGL_FUNCTION_NAME, replayer.get_frame_index());
            }

            if ((replayer.get_at_frame_boundary()) && (pSnapshot) && (loop_count > 0) && ((pTrace_reader->get_cur_frame() == snapshot_loop_end_frame) || (status == vogl_gl_replayer::cStatusAtEOF)))
            {
                status = replayer.begin_applying_snapshot(pSnapshot, false);
                if ((status != vogl_gl_replayer::cStatusOK) && (status != vogl_gl_replayer::cStatusResizeWindow))
                    goto error_exit;

                pTrace_reader->seek_to_frame(static_cast<uint>(snapshot_loop_start_frame));

                if (draw_kill_max_thresh > 0)
                {
                    int64_t thresh = replayer.get_frame_draw_counter_kill_threshold();
                    thresh += 1;
                    if (thresh >= draw_kill_max_thresh)
                        thresh = 0;
                    replayer.set_frame_draw_counter_kill_threshold(thresh);

                    vogl_debug_printf("%s: Applying snapshot and seeking back to frame %" PRIi64 " draw kill thresh %" PRIu64 "\n", VOGL_FUNCTION_NAME, snapshot_loop_start_frame, thresh);
                }
                else
                    vogl_debug_printf("%s: Applying snapshot and seeking back to frame %" PRIi64 "\n", VOGL_FUNCTION_NAME, snapshot_loop_start_frame);

                loop_count--;
            }
            else
            {
                bool print_progress = (status == vogl_gl_replayer::cStatusAtEOF) || ((replayer.get_at_frame_boundary()) && ((replayer.get_frame_index() % 100) == 0));
                if (print_progress)
                {
                    if (pTrace_reader->get_type() == cBINARY_TRACE_FILE_READER)
                    {
                        vogl_binary_trace_file_reader &binary_trace_reader = *static_cast<vogl_binary_trace_file_reader *>(pTrace_reader.get());

                        vogl_printf("Replay now at frame index %d, trace file offet %" PRIu64 ", GL call counter %" PRIu64 ", %3.2f%% percent complete\n",
                                   replayer.get_frame_index(),
                                   binary_trace_reader.get_cur_file_ofs(),
                                   replayer.get_last_parsed_call_counter(),
                                   binary_trace_reader.get_trace_file_size() ? (binary_trace_reader.get_cur_file_ofs() * 100.0f) / binary_trace_reader.get_trace_file_size() : 0);
                    }
                }

                if (status == vogl_gl_replayer::cStatusAtEOF)
                {
                    if (!endless_mode)
                    {
                        double time_since_start = tm.get_elapsed_secs();

                        vogl_printf("%u total swaps, %.3f secs, %3.3f avg fps\n", replayer.get_total_swaps(), time_since_start, replayer.get_frame_index() / time_since_start);

                        break;
                    }

                    vogl_printf("Resetting state and rewinding back to frame 0\n");

                    replayer.reset_state();

                    if (!pTrace_reader->seek_to_frame(0))
                    {
                        vogl_error_printf("%s: Failed rewinding trace reader!\n", VOGL_FUNCTION_NAME);
                        goto error_exit;
                    }
                }
            }
        } // interactive_mode

        telemetry_tick();
    }

    if (trim_frames.size())
    {
        console::message("Wrote %u trim file(s)\n", num_trim_files_written);

        if (((multitrim_mode) && (!num_trim_files_written)) ||
            ((!multitrim_mode) && (num_trim_files_written != trim_frames.size())))
            console::warning("Requested %u trim frames, but was only able to write %u trim frames (one or more -trim_frames must have been too large)\n", trim_frames.size(), num_trim_files_written);
    }

normal_exit:

    if (g_command_line_params().get_value_as_bool("pause_on_exit") && (window.is_opened()))
    {
        vogl_printf("Press a key to continue.\n");

        for (;;)
        {
            if (vogl_kbhit())
                break;

            bool exit_flag = false;
            while (!exit_flag && X11_Pending(window.get_display()))
            {
                XEvent newEvent;
                XNextEvent(window.get_display(), &newEvent);

                switch (newEvent.type)
                {
                    case KeyPress:
                    case DestroyNotify:
                    {
                        exit_flag = true;
                        break;
                    }
                    case ClientMessage:
                    {
                        if (newEvent.xclient.data.l[0] == (int)wmDeleteMessage)
                            exit_flag = true;
                        break;
                    }
                    default:
                        break;
                }
            }
            if (exit_flag)
                break;
            vogl_sleep(50);
        }
    }

    return true;

error_exit:
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_dump_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_dump_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string input_trace_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_trace_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input binary trace file!\n");
        return false;
    }

    dynamic_string output_base_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_base_filename.is_empty())
    {
        vogl_error_printf("Must specify base filename of output JSON/blob files!\n");
        return false;
    }

    vogl_loose_file_blob_manager output_file_blob_manager;

    dynamic_string output_trace_path(file_utils::get_pathname(output_base_filename.get_ptr()));
    vogl_debug_printf("%s: Output trace path: %s\n", VOGL_FUNCTION_NAME, output_trace_path.get_ptr());
    output_file_blob_manager.init(cBMFReadWrite, output_trace_path.get_ptr());

    file_utils::create_directories(output_trace_path, false);

    dynamic_string actual_input_trace_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(input_trace_filename, actual_input_trace_filename, g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
    {
        vogl_error_printf("%s: Failed opening input trace file \"%s\"\n", VOGL_FUNCTION_NAME, input_trace_filename.get_ptr());
        return false;
    }

    const bool full_verification = g_command_line_params().get_value_as_bool("verify");

    vogl_ctypes trace_gl_ctypes;
    trace_gl_ctypes.init(pTrace_reader->get_sof_packet().m_pointer_sizes);

    dynamic_string archive_name;
    if (pTrace_reader->get_archive_blob_manager().is_initialized())
    {
        dynamic_string archive_filename(output_base_filename.get_ptr());
        archive_filename += "_trace_archive.zip";

        archive_name = file_utils::get_filename(archive_filename.get_ptr());

        vogl_message_printf("Writing trace archive \"%s\", size %" PRIu64 " bytes\n", archive_filename.get_ptr(), pTrace_reader->get_archive_blob_manager().get_archive_size());

        cfile_stream archive_stream;
        if (!archive_stream.open(archive_filename.get_ptr(), cDataStreamWritable | cDataStreamSeekable))
        {
            vogl_error_printf("%s: Failed opening output trace archive \"%s\"!\n", VOGL_FUNCTION_NAME, archive_filename.get_ptr());
            return false;
        }

        if (!pTrace_reader->get_archive_blob_manager().write_archive_to_stream(archive_stream))
        {
            vogl_error_printf("%s: Failed writing to output trace archive \"%s\"!\n", VOGL_FUNCTION_NAME, archive_filename.get_ptr());
            return false;
        }

        if (!archive_stream.close())
        {
            vogl_error_printf("%s: Failed writing to output trace archive \"%s\"!\n", VOGL_FUNCTION_NAME, archive_filename.get_ptr());
            return false;
        }
    }

    vogl_trace_packet gl_packet_cracker(&trace_gl_ctypes);

    uint cur_file_index = 0;
    uint cur_frame_index = 0;
    uint64_t cur_packet_index = 0;
    VOGL_NOTE_UNUSED(cur_packet_index);
    json_document cur_doc;

    {
        json_node &meta_node = cur_doc.get_root()->add_object("meta");
        meta_node.add_key_value("cur_frame", cur_frame_index);

        json_node &sof_node = cur_doc.get_root()->add_object("sof");
        sof_node.add_key_value("pointer_sizes", pTrace_reader->get_sof_packet().m_pointer_sizes);
        sof_node.add_key_value("version", to_hex_string(pTrace_reader->get_sof_packet().m_version));
        if (!archive_name.is_empty())
            sof_node.add_key_value("archive_filename", archive_name);

        json_node &uuid_array = sof_node.add_array("uuid");
        for (uint i = 0; i < VOGL_ARRAY_SIZE(pTrace_reader->get_sof_packet().m_uuid); i++)
            uuid_array.add_value(pTrace_reader->get_sof_packet().m_uuid[i]);
    }

    // TODO: Automatically dump binary snapshot file to text?
    // Right now we can't afford to do it at trace time, it takes too much memory.

    json_node *pPacket_array = &cur_doc.get_root()->add_array("packets");

    bool flush_current_document = false;

    bool status = true;

    for (;;)
    {
        uint64_t cur_packet_ofs = (pTrace_reader->get_type() == cBINARY_TRACE_FILE_READER) ? static_cast<vogl_binary_trace_file_reader *>(pTrace_reader.get())->get_cur_file_ofs() : 0;

        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if (read_status == vogl_trace_file_reader::cEOF)
        {
            vogl_message_printf("At trace file EOF\n");
            break;
        }
        else if (read_status != vogl_trace_file_reader::cOK)
        {
            vogl_error_printf("Failed reading from trace file, or file size was too small\n");

            status = false;
            break;
        }

        if (pTrace_reader->get_packet_type() != cTSPTGLEntrypoint)
        {
            if (pTrace_reader->get_packet_type() == cTSPTSOF)
            {
                vogl_error_printf("Encountered redundant SOF packet!\n");
                status = false;
            }

            json_node *pMeta_node = cur_doc.get_root()->find_child_object("meta");
            if (pMeta_node)
                pMeta_node->add_key_value("eof", (pTrace_reader->get_packet_type() == cTSPTEOF) ? 1 : 2);

            break;
        }

        if (flush_current_document)
        {
            flush_current_document = false;

            dynamic_string output_filename(cVarArg, "%s_%06u.json", output_base_filename.get_ptr(), cur_file_index);
            vogl_message_printf("Writing file: \"%s\"\n", output_filename.get_ptr());

            if (!cur_doc.serialize_to_file(output_filename.get_ptr(), true))
            {
                vogl_error_printf("%s: Failed serializing JSON document to file %s\n", VOGL_FUNCTION_NAME, output_filename.get_ptr());

                status = false;
                break;
            }

            if (cur_doc.get_root()->check_for_duplicate_keys())
                vogl_warning_printf("%s: JSON document %s has nodes with duplicate keys, this document may not be readable by some JSON parsers\n", VOGL_FUNCTION_NAME, output_filename.get_ptr());

            cur_file_index++;

            pPacket_array = NULL;
            cur_doc.clear();

            json_node &note_node = cur_doc.get_root()->add_object("meta");
            note_node.add_key_value("cur_frame", cur_frame_index);

            json_node &uuid_array = note_node.add_array("uuid");
            for (uint i = 0; i < VOGL_ARRAY_SIZE(pTrace_reader->get_sof_packet().m_uuid); i++)
                uuid_array.add_value(pTrace_reader->get_sof_packet().m_uuid[i]);

            pPacket_array = &cur_doc.get_root()->add_array("packets");
        }

        const vogl_trace_gl_entrypoint_packet &gl_packet = pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
        const char *pFunc_name = g_vogl_entrypoint_descs[gl_packet.m_entrypoint_id].m_pName;
        VOGL_NOTE_UNUSED(pFunc_name);

        if (g_command_line_params().get_value_as_bool("debug"))
        {
            vogl_debug_printf("Trace packet: File offset: %" PRIu64 ", Total size %u, Param size: %u, Client mem size %u, Name value size %u, call %" PRIu64 ", ID: %s (%u), Thread ID: 0x%" PRIX64 ", Trace Context: 0x%" PRIX64 "\n",
                             cur_packet_ofs,
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

        if (!gl_packet_cracker.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), true))
        {
            vogl_error_printf("Failed deserializing GL entrypoint packet. Trying to continue parsing the file, this may die!\n");

            //status = false;
            //break;
            continue;
        }

        json_node &new_node = pPacket_array->add_object();

        vogl_trace_packet::json_serialize_params serialize_params;
        serialize_params.m_output_basename = file_utils::get_filename(output_base_filename.get_ptr());
        serialize_params.m_pBlob_manager = &output_file_blob_manager;
        serialize_params.m_cur_frame = cur_file_index;
        serialize_params.m_write_debug_info = g_command_line_params().get_value_as_bool("write_debug_info");
        if (!gl_packet_cracker.json_serialize(new_node, serialize_params))
        {
            vogl_error_printf("JSON serialization failed!\n");

            status = false;
            break;
        }

        if (full_verification)
        {
#if 0
            if (!strcmp(pFunc_name, "glClearColor"))
            {
                VOGL_BREAKPOINT
            }
#endif

            vogl::vector<char> new_node_as_text;
            new_node.serialize(new_node_as_text, true, 0);

#if 0
            if (new_node_as_text.size())
            {
                printf("%s\n", new_node_as_text.get_ptr());
            }
#endif

            json_document round_tripped_node;
            if (!round_tripped_node.deserialize(new_node_as_text.get_ptr()) || !round_tripped_node.get_root())
            {
                vogl_error_printf("Failed verifying serialized JSON data (step 1)!\n");

                status = false;
                break;
            }
            else
            {
                vogl_trace_packet temp_cracker(&trace_gl_ctypes);
                bool success = temp_cracker.json_deserialize(*round_tripped_node.get_root(), "<memory>", &output_file_blob_manager);
                if (!success)
                {
                    vogl_error_printf("Failed verifying serialized JSON data (step 2)!\n");

                    status = false;
                    break;
                }
                else
                {
                    success = gl_packet_cracker.compare(temp_cracker, false);
                    if (!success)
                    {
                        vogl_error_printf("Failed verifying serialized JSON data (step 3)!\n");

                        status = false;
                        break;
                    }
                    else
                    {
                        dynamic_stream dyn_stream;

                        success = temp_cracker.serialize(dyn_stream);
                        if (!success)
                        {
                            vogl_error_printf("Failed verifying serialized JSON data (step 4)!\n");

                            status = false;
                            break;
                        }
                        else
                        {
                            vogl_trace_packet temp_cracker2(&trace_gl_ctypes);
                            success = temp_cracker2.deserialize(static_cast<const uint8 *>(dyn_stream.get_ptr()), static_cast<uint32>(dyn_stream.get_size()), true);
                            if (!success)
                            {
                                vogl_error_printf("Failed verifying serialized JSON data (step 5)!\n");

                                status = false;
                                break;
                            }
                            success = gl_packet_cracker.compare(temp_cracker2, true);
                            if (!success)
                            {
                                vogl_error_printf("Failed verifying serialized JSON data (step 6)!\n");

                                status = false;
                                break;
                            }
                            else
                            {
                                uint64_t binary_serialized_size = dyn_stream.get_size();
                                if (binary_serialized_size != pTrace_reader->get_packet_buf().size())
                                {
                                    vogl_error_printf("Round-tripped binary serialized size differs from original packet's' size (step 7)!\n");

                                    status = false;
                                    break;
                                }
                                else
                                {
#if 0
									// This is excessive- the key value map fields may be binary serialized in different orders
									// TODO: maybe fix the key value map class so it serializes in a stable order (independent of hash table construction)?
									const uint8 *p = static_cast<const uint8 *>(dyn_stream.get_ptr());
									const uint8 *q = static_cast<const uint8 *>(pTrace_reader->get_packet_buf().get_ptr());
									if (memcmp(p, q, binary_serialized_size) != 0)
									{
										file_utils::write_buf_to_file("p.bin", p, binary_serialized_size);
										file_utils::write_buf_to_file("q.bin", q, binary_serialized_size);
										vogl_error_printf("Round-tripped binary serialized data differs from original packet's data (step 7)!\n");

										status = false;
										break;
									}
#endif
                                }
                            }
                        }
                    }
                }
            }
        }

        if (vogl_is_swap_buffers_entrypoint(static_cast<gl_entrypoint_id_t>(gl_packet.m_entrypoint_id)))
        {
            flush_current_document = true;
            cur_frame_index++;
        }

        if (cur_doc.get_root()->size() >= 1 * 1000 * 1000)
        {
            // TODO: Support replaying dumps like this, or fix the code to serialize the text as it goes.
            vogl_error_printf("Haven't encountered a SwapBuffers() call in over 1000000 GL calls, dumping current in-memory JSON document to disk to avoid running out of memory. This JSON dump may not be replayable, but writing it anyway.\n");
            flush_current_document = true;
        }
    }

    json_node *pMeta_node = cur_doc.get_root()->find_child_object("meta");

    if (pMeta_node)
    {
        if (!pMeta_node->has_key("eof"))
            pMeta_node->add_key_value("eof", 2);

        dynamic_string output_filename(cVarArg, "%s_%06u.json", output_base_filename.get_ptr(), cur_file_index);
        vogl_message_printf("Writing file: \"%s\"\n", output_filename.get_ptr());

        if (!cur_doc.serialize_to_file(output_filename.get_ptr(), true))
        {
            vogl_error_printf("%s: Failed serializing JSON document to file %s\n", VOGL_FUNCTION_NAME, output_filename.get_ptr());

            status = false;
        }

        if (cur_doc.get_root()->check_for_duplicate_keys())
            vogl_warning_printf("%s: JSON document %s has nodes with duplicate keys, this document may not be readable by some JSON parsers\n", VOGL_FUNCTION_NAME, output_filename.get_ptr());

        cur_file_index++;
    }

    if (!status)
        vogl_error_printf("Failed dumping binary trace to JSON files starting with filename prefix \"%s\" (but wrote as much as possible)\n", output_base_filename.get_ptr());
    else
        vogl_message_printf("Successfully dumped binary trace to %u JSON file(s) starting with filename prefix \"%s\"\n", cur_file_index, output_base_filename.get_ptr());

    return status;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_parse_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_parse_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string input_base_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_base_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input JSON/blob trace files!\n");
        return false;
    }

    dynamic_string actual_input_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(input_base_filename, actual_input_filename, g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
        return false;

    dynamic_string output_trace_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_trace_filename.is_empty())
    {
        vogl_error_printf("Must specify full filename of output binary trace file!\n");
        return false;
    }

    file_utils::create_directories(output_trace_filename, true);

    if (file_utils::add_default_extension(output_trace_filename, ".bin"))
        vogl_message_printf("Trim output filename doesn't have an extension, appending \".bin\" to the filename\n");

    // TODO: Refactor this, I think we're going to write the actual ctypes and entrpoint descs, etc. into the trace archive next.
    vogl_ctypes trace_ctypes;
    trace_ctypes.init(pTrace_reader->get_sof_packet().m_pointer_sizes);

    vogl_trace_file_writer trace_writer(&trace_ctypes);
    if (!trace_writer.open(output_trace_filename.get_ptr(), NULL, true, false, pTrace_reader->get_sof_packet().m_pointer_sizes))
    {
        vogl_error_printf("Unable to create file \"%s\"!\n", output_trace_filename.get_ptr());
        return false;
    }

    if (pTrace_reader->get_archive_blob_manager().is_initialized())
    {
        dynamic_string_array blob_files(pTrace_reader->get_archive_blob_manager().enumerate());
        for (uint i = 0; i < blob_files.size(); i++)
        {
            if (blob_files[i] == VOGL_TRACE_ARCHIVE_FRAME_FILE_OFFSETS_FILENAME)
                continue;

            vogl_message_printf("Adding blob file %s to output trace archive\n", blob_files[i].get_ptr());

            uint8_vec blob_data;
            if (pTrace_reader->get_archive_blob_manager().get(blob_files[i], blob_data))
            {
                if (trace_writer.get_trace_archive()->add_buf_using_id(blob_data.get_ptr(), blob_data.size(), blob_files[i]).is_empty())
                {
                    vogl_error_printf("%s: Failed writing blob data %s to output trace archive!\n", VOGL_FUNCTION_NAME, blob_files[i].get_ptr());
                    return false;
                }
            }
            else
            {
                vogl_error_printf("%s: Failed reading blob data %s from trace archive!\n", VOGL_FUNCTION_NAME, blob_files[i].get_ptr());
                return false;
            }
        }
    }

    for (;;)
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("Failed reading from trace file\n");
            goto failed;
        }

        if ((read_status == vogl_trace_file_reader::cEOF) || (pTrace_reader->is_eof_packet()))
        {
            vogl_message_printf("At trace file EOF\n");
            break;
        }

        const vogl::vector<uint8> &packet_buf = pTrace_reader->get_packet_buf();

        if (!trace_writer.write_packet(packet_buf.get_ptr(), packet_buf.size(), pTrace_reader->is_swap_buffers_packet()))
        {
            vogl_error_printf("Failed writing to output trace file \"%s\"\n", output_trace_filename.get_ptr());
            goto failed;
        }
    }

    if (!trace_writer.close())
    {
        vogl_error_printf("Failed closing output trace file \"%s\"\n", output_trace_filename.get_ptr());
        goto failed;
    }

    vogl_message_printf("Successfully wrote binary trace file \"%s\"\n", output_trace_filename.get_ptr());

    return true;

failed:
    trace_writer.close();

    vogl_warning_printf("Processing failed, output trace file \"%s\" may be invalid!\n", output_trace_filename.get_ptr());

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// struct histo_entry
//----------------------------------------------------------------------------------------------------------------------
typedef vogl::map<dynamic_string> dynamic_string_set;
typedef vogl::map<dynamic_string, uint64_t> dynamic_string_hash_map;
typedef vogl::map<uint> uint_map;

struct histo_entry
{
    histo_entry()
    {
    }
    histo_entry(dynamic_string_hash_map::const_iterator it, double val)
        : m_it(it), m_value(val)
    {
    }

    dynamic_string_hash_map::const_iterator m_it;
    double m_value;

    bool operator<(const histo_entry &rhs) const
    {
        return m_value > rhs.m_value;
    }
};

//----------------------------------------------------------------------------------------------------------------------
// dump_histogram
//----------------------------------------------------------------------------------------------------------------------
#define SAFE_FLOAT_DIV(n, d) (d ? ((double)(n) / (d)) : 0)

static void dump_histogram(const char *pMsg, const dynamic_string_hash_map &map, uint64_t total_gl_entrypoint_packets, uint64_t total_swaps)
{
    VOGL_FUNC_TRACER

    vogl::vector<histo_entry> histo;
    for (dynamic_string_hash_map::const_iterator it = map.begin(); it != map.end(); ++it)
        histo.push_back(histo_entry(it, static_cast<double>(it->second)));
    mergesort(histo);
    vogl_printf("\n----------------------\n%s: %u\n", pMsg, map.size());

    for (uint i = 0; i < histo.size(); i++)
    {
        dynamic_string_hash_map::const_iterator it = histo[i].m_it;
        vogl_printf("%s: Total calls: %" PRIu64 " %3.1f%%, Avg calls per frame: %f\n", it->first.get_ptr(), it->second, SAFE_FLOAT_DIV(it->second * 100.0f, total_gl_entrypoint_packets), SAFE_FLOAT_DIV(it->second, total_swaps));
    }
}

//----------------------------------------------------------------------------------------------------------------------
// tool_info_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_info_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string input_base_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_base_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input JSON/blob trace files!\n");
        return false;
    }

    dynamic_string actual_input_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(input_base_filename, actual_input_filename, g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
        return false;

    vogl_printf("Scanning trace file %s\n", actual_input_filename.get_ptr());

    const vogl_trace_stream_start_of_file_packet &sof_packet = pTrace_reader->get_sof_packet();

    if (pTrace_reader->get_type() == cBINARY_TRACE_FILE_READER)
    {
        uint64_t file_size = dynamic_cast<vogl_binary_trace_file_reader *>(pTrace_reader.get())->get_stream().get_size();
        vogl_printf("Total file size: %s\n", uint64_to_string_with_commas(file_size).get_ptr());
    }

    vogl_printf("SOF packet size: %" PRIu64 " bytes\n", sof_packet.m_size);
    vogl_printf("Version: 0x%04X\n", sof_packet.m_version);
    vogl_printf("UUID: 0x%08x 0x%08x 0x%08x 0x%08x\n", sof_packet.m_uuid[0], sof_packet.m_uuid[1], sof_packet.m_uuid[2], sof_packet.m_uuid[3]);
    vogl_printf("First packet offset: %" PRIu64 "\n", sof_packet.m_first_packet_offset);
    vogl_printf("Trace pointer size: %u\n", sof_packet.m_pointer_sizes);
    vogl_printf("Trace archive size: %" PRIu64 " offset: %" PRIu64 "\n", sof_packet.m_archive_size, sof_packet.m_archive_offset);
    vogl_printf("Can quickly seek forward: %u\nMax frame index: %" PRIu64 "\n", pTrace_reader->can_quickly_seek_forward(), pTrace_reader->get_max_frame_index());

    if (!pTrace_reader->get_archive_blob_manager().is_initialized())
    {
        vogl_warning_printf("This trace does not have a trace archive!\n");
    }
    else
    {
        vogl_printf("----------------------\n");
        vogl::vector<dynamic_string> archive_files(pTrace_reader->get_archive_blob_manager().enumerate());
        vogl_printf("Total trace archive files: %u\n", archive_files.size());
        for (uint i = 0; i < archive_files.size(); i++)
            vogl_printf("\"%s\"\n", archive_files[i].get_ptr());
        vogl_printf("----------------------\n");
    }

    uint min_packet_size = cUINT32_MAX;
    uint max_packet_size = 0;
    uint64_t total_packets = 1; // 1, not 0, to account for the SOF packet
    uint64_t total_packet_bytes = 0;
    uint64_t total_swaps = 0;
    uint64_t total_make_currents = 0;
    uint64_t total_internal_trace_commands = 0;
    uint64_t total_non_gl_entrypoint_packets = 1; // 1, not 0, to account for the SOF packet
    uint64_t total_gl_entrypoint_packets = 0;
    uint64_t total_gl_commands = 0;
    uint64_t total_glx_commands = 0;
    uint64_t total_wgl_commands = 0;
    uint64_t total_unknown_commands = 0;
    uint64_t total_draws = 0;
    bool found_eof_packet = false;

    uint64_t min_packets_per_frame = cUINT64_MAX;
    uint64_t max_packets_per_frame = 0;
    uint64_t max_frame_make_currents = 0;
    uint64_t min_frame_make_currents = cUINT64_MAX;

    uint64_t min_frame_draws = cUINT64_MAX;
    uint64_t max_frame_draws = 0;
    uint64_t cur_frame_draws = 0;

    uint64_t cur_frame_packet_count = 0;
    uint64_t total_frame_make_currents = 0;

    uint64_t num_non_whitelisted_funcs = 0;
    dynamic_string_set non_whitelisted_funcs_called;

    uint64_t total_programs_linked = 0;
    uint64_t total_program_binary_calls = 0;
    uint_map unique_programs_used;

    uint total_gl_state_snapshots = 0;

    uint total_display_list_calls = false;
    uint total_gl_get_errors = 0;

    uint total_context_creates = 0;
    uint total_context_destroys = 0;

    dynamic_string_hash_map all_apis_called, category_histogram, version_histogram, profile_histogram, deprecated_histogram;

    vogl_ctypes trace_gl_ctypes(pTrace_reader->get_sof_packet().m_pointer_sizes);

    vogl_trace_packet trace_packet(&trace_gl_ctypes);

    for (;;)
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("Failed reading from trace file!\n");

            goto done;
        }

        if (read_status == vogl_trace_file_reader::cEOF)
        {
            vogl_printf("At trace file EOF on swap %" PRIu64 "\n", total_swaps);
            break;
        }

        const vogl::vector<uint8> &packet_buf = pTrace_reader->get_packet_buf();
        VOGL_NOTE_UNUSED(packet_buf);

        uint packet_size = pTrace_reader->get_packet_size();

        min_packet_size = math::minimum<uint>(min_packet_size, packet_size);
        max_packet_size = math::maximum<uint>(max_packet_size, packet_size);
        total_packets++;
        total_packet_bytes += packet_size;

        cur_frame_packet_count++;

        const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet();
        VOGL_NOTE_UNUSED(base_packet);
        const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;

        if (pTrace_reader->get_packet_type() == cTSPTGLEntrypoint)
        {
            if (!trace_packet.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
            {
                console::error("%s: Failed parsing GL entrypoint packet\n", VOGL_FUNCTION_NAME);
                goto done;
            }

            pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
            gl_entrypoint_id_t entrypoint_id = static_cast<gl_entrypoint_id_t>(pGL_packet->m_entrypoint_id);

            const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[entrypoint_id];

            all_apis_called[entrypoint_desc.m_pName]++;
            category_histogram[entrypoint_desc.m_pCategory]++;
            version_histogram[entrypoint_desc.m_pVersion]++;
            profile_histogram[entrypoint_desc.m_pProfile]++;
            deprecated_histogram[entrypoint_desc.m_pDeprecated]++;

            if (!entrypoint_desc.m_is_whitelisted)
            {
                num_non_whitelisted_funcs++;
                non_whitelisted_funcs_called.insert(entrypoint_desc.m_pName);
            }

            if (!strcmp(entrypoint_desc.m_pAPI_prefix, "GLX"))
                total_glx_commands++;
            else if (!strcmp(entrypoint_desc.m_pAPI_prefix, "WGL"))
                total_wgl_commands++;
            else if (!strcmp(entrypoint_desc.m_pAPI_prefix, "GL"))
                total_gl_commands++;
            else
                total_unknown_commands++;

            total_gl_entrypoint_packets++;

            if (vogl_is_swap_buffers_entrypoint(entrypoint_id))
            {
                total_swaps++;
                if ((total_swaps & 255) == 255)
                    console::progress("Frame %" PRIu64 "\n", total_swaps);

                min_packets_per_frame = math::minimum(min_packets_per_frame, cur_frame_packet_count);
                max_packets_per_frame = math::maximum(max_packets_per_frame, cur_frame_packet_count);

                min_frame_draws = math::minimum(min_frame_draws, cur_frame_draws);
                max_frame_draws = math::maximum(max_frame_draws, cur_frame_draws);

                max_frame_make_currents = math::maximum(max_frame_make_currents, total_frame_make_currents);
                min_frame_make_currents = math::minimum(min_frame_make_currents, total_frame_make_currents);

                cur_frame_packet_count = 0;
                total_frame_make_currents = 0;
                cur_frame_draws = 0;
            }
            else if (vogl_is_draw_entrypoint(entrypoint_id))
            {
                total_draws++;
                cur_frame_draws++;
            }
            else if (vogl_is_make_current_entrypoint(entrypoint_id))
            {
                total_make_currents++;
                total_frame_make_currents++;
            }

            switch (entrypoint_id)
            {
                case VOGL_ENTRYPOINT_glInternalTraceCommandRAD:
                {
                    total_internal_trace_commands++;

                    GLuint cmd = trace_packet.get_param_value<GLuint>(0);
                    GLuint size = trace_packet.get_param_value<GLuint>(1);
                    VOGL_NOTE_UNUSED(size);

                    if (cmd == cITCRKeyValueMap)
                    {
                        key_value_map &kvm = trace_packet.get_key_value_map();

                        dynamic_string cmd_type(kvm.get_string("command_type"));
                        if (cmd_type == "state_snapshot")
                        {
                            total_gl_state_snapshots++;
                        }
                    }

                    break;
                }
                case VOGL_ENTRYPOINT_glProgramBinary:
                {
                    total_program_binary_calls++;
                    break;
                }
                case VOGL_ENTRYPOINT_glLinkProgram:
                case VOGL_ENTRYPOINT_glLinkProgramARB:
                {
                    total_programs_linked++;
                    break;
                }
                case VOGL_ENTRYPOINT_glUseProgram:
                case VOGL_ENTRYPOINT_glUseProgramObjectARB:
                {
                    GLuint trace_handle = trace_packet.get_param_value<GLuint>(0);
                    unique_programs_used.insert(trace_handle);

                    break;
                }
                case VOGL_ENTRYPOINT_glGenLists:
                case VOGL_ENTRYPOINT_glDeleteLists:
                case VOGL_ENTRYPOINT_glXUseXFont:
                case VOGL_ENTRYPOINT_glCallList:
                case VOGL_ENTRYPOINT_glCallLists:
                case VOGL_ENTRYPOINT_glListBase:
                {
                    total_display_list_calls++;
                    break;
                }
                case VOGL_ENTRYPOINT_glGetError:
                {
                    total_gl_get_errors++;
                    break;
                }
                case VOGL_ENTRYPOINT_glXCreateContext:
                case VOGL_ENTRYPOINT_glXCreateContextAttribsARB:
                {
                    total_context_creates++;
                    break;
                }
                case VOGL_ENTRYPOINT_glXDestroyContext:
                {
                    total_context_destroys++;
                    break;
                }
                default:
                    break;
            }
        }
        else
        {
            total_non_gl_entrypoint_packets++;
        }

        if (pTrace_reader->get_packet_type() == cTSPTEOF)
        {
            found_eof_packet = true;
            vogl_printf("Found trace file EOF packet on swap %" PRIu64 "\n", total_swaps);
            break;
        }
    }

done:
    vogl_printf("\n");

#define PRINT_UINT_VAR(x) vogl_printf("%s: %u\n", dynamic_string(#x).replace("_", " ").get_ptr(), x);
#define PRINT_UINT64_VAR(x) vogl_printf("%s: %" PRIu64 "\n", dynamic_string(#x).replace("_", " ").get_ptr(), x);
#define PRINT_FLOAT(x, f) vogl_printf("%s: %f\n", dynamic_string(#x).replace("_", " ").get_ptr(), f);

    PRINT_UINT64_VAR(num_non_whitelisted_funcs);
    PRINT_UINT_VAR(total_gl_state_snapshots);

    PRINT_UINT64_VAR(total_swaps);

    PRINT_UINT64_VAR(total_make_currents);
    PRINT_FLOAT(avg_make_currents_per_frame, SAFE_FLOAT_DIV(total_make_currents, total_swaps));
    PRINT_UINT64_VAR(max_frame_make_currents);
    PRINT_UINT64_VAR(min_frame_make_currents);

    PRINT_UINT64_VAR(total_draws);
    PRINT_FLOAT(avg_draws_per_frame, SAFE_FLOAT_DIV(total_draws, total_swaps));
    PRINT_UINT64_VAR(min_frame_draws);
    PRINT_UINT64_VAR(max_frame_draws);

    PRINT_UINT_VAR(min_packet_size);
    PRINT_UINT_VAR(max_packet_size);
    PRINT_UINT64_VAR(total_packets);
    PRINT_UINT64_VAR(total_packet_bytes);
    PRINT_FLOAT(avg_packet_bytes_per_frame, SAFE_FLOAT_DIV(total_packet_bytes, total_swaps));
    PRINT_FLOAT(avg_packet_size, SAFE_FLOAT_DIV(total_packet_bytes, total_packets));
    PRINT_UINT64_VAR(min_packets_per_frame);
    PRINT_UINT64_VAR(max_packets_per_frame);
    PRINT_FLOAT(avg_packets_per_frame, SAFE_FLOAT_DIV(total_packets, total_swaps));

    PRINT_UINT64_VAR(total_internal_trace_commands);
    PRINT_UINT64_VAR(total_non_gl_entrypoint_packets);
    PRINT_UINT64_VAR(total_gl_entrypoint_packets);
    PRINT_UINT64_VAR(total_gl_commands);
    PRINT_UINT64_VAR(total_glx_commands);
    PRINT_UINT64_VAR(total_wgl_commands);
    PRINT_UINT64_VAR(total_unknown_commands);

    PRINT_UINT_VAR(found_eof_packet);

    PRINT_UINT_VAR(total_display_list_calls);
    printf("Avg display lists calls per frame: %f\n", SAFE_FLOAT_DIV(total_display_list_calls, total_swaps));

    PRINT_UINT_VAR(total_gl_get_errors);
    printf("Avg glGetError calls per frame: %f\n", SAFE_FLOAT_DIV(total_gl_get_errors, total_swaps));

    PRINT_UINT_VAR(total_context_creates);
    PRINT_UINT_VAR(total_context_destroys);

#undef PRINT_UINT
#undef PRINT_UINT64_VAR
#undef PRINT_FLOAT

    vogl_printf("----------------------\n%s: %" PRIu64 "\n", "Total calls to glLinkProgram/glLinkProgramARB", total_programs_linked);
    vogl_printf("%s: %" PRIu64 "\n", "Total calls to glProgramBinary", total_program_binary_calls);
    vogl_printf("%s: %u\n", "Total unique program handles passed to glUseProgram/glUseProgramObjectARB", unique_programs_used.size());

    dump_histogram("API histogram", all_apis_called, total_gl_entrypoint_packets, total_swaps);
    dump_histogram("API Category histogram", category_histogram, total_gl_entrypoint_packets, total_swaps);
    dump_histogram("API Version histogram", version_histogram, total_gl_entrypoint_packets, total_swaps);
//dump_histogram("API Profile histogram", profile_histogram, total_gl_entrypoint_packets, total_swaps);
//dump_histogram("API Deprecated histogram", deprecated_histogram, total_gl_entrypoint_packets, total_swaps);

#undef DUMP_HISTOGRAM

    if (non_whitelisted_funcs_called.size())
    {
        vogl_warning_printf("\n----------------------\n");
        vogl_warning_printf("Number of non-whitelisted functions actually called: %u\n", non_whitelisted_funcs_called.size());
        for (dynamic_string_set::const_iterator it = non_whitelisted_funcs_called.begin(); it != non_whitelisted_funcs_called.end(); ++it)
            vogl_warning_printf("%s\n", it->first.get_ptr());
        vogl_warning_printf("\n----------------------\n");
    }

    return true;
}
#undef SAFE_FLOAT_DIV

//----------------------------------------------------------------------------------------------------------------------
// param_value_matches
//----------------------------------------------------------------------------------------------------------------------
static bool param_value_matches(const bigint128 &value_to_find, vogl_namespace_t find_namespace, uint64_t value_data, vogl_ctype_t value_ctype, vogl_namespace_t value_namespace)
{
    if (find_namespace != VOGL_NAMESPACE_UNKNOWN)
    {
        if (find_namespace != value_namespace)
            return false;
    }

    bigint128 value(0);

    switch (value_ctype)
    {
        case VOGL_BOOL:
        case VOGL_GLBOOLEAN:
        {
            value = value_data;
            break;
        }
        case VOGL_GLENUM:
        {
            value = value_data;
            break;
        }
        case VOGL_FLOAT:
        case VOGL_GLFLOAT:
        case VOGL_GLCLAMPF:
        {
            // Not supporting float/double finds for now
            return false;
        }
        case VOGL_GLDOUBLE:
        case VOGL_GLCLAMPD:
        {
            // Not supporting float/double finds for now
            return false;
        }
        case VOGL_GLINT:
        case VOGL_INT:
        case VOGL_INT32T:
        case VOGL_GLSIZEI:
        case VOGL_GLFIXED:
        {
            value = static_cast<int32>(value_data);
            break;
        }
        case VOGL_GLSHORT:
        {
            value = static_cast<int16>(value_data);
            break;
        }
        case VOGL_GLBYTE:
        {
            value = static_cast<int8>(value_data);
            break;
        }
        case VOGL_GLINT64:
        case VOGL_GLINT64EXT:
        {
            value = static_cast<int64_t>(value_data);
            break;
        }
        default:
        {
            value = value_data;
            break;
        }
    }

    if (value == value_to_find)
        return true;

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// print_match
//----------------------------------------------------------------------------------------------------------------------
static void print_match(const vogl_trace_packet &trace_packet, int param_index, int array_element_index, uint64_t total_swaps)
{
    json_document doc;
    vogl_trace_packet::json_serialize_params params;
    trace_packet.json_serialize(*doc.get_root(), params);

    dynamic_string packet_as_json;
    doc.serialize(packet_as_json);

    if (param_index == -2)
        vogl_printf("----- Function match, frame %" PRIu64 ":\n", total_swaps);
    else if (param_index < 0)
        vogl_printf("----- Return value match, frame %" PRIu64 ":\n", total_swaps);
    else if (array_element_index >= 0)
        vogl_printf("----- Parameter %i element %i match, frame %" PRIu64 ":\n", param_index, array_element_index, total_swaps);
    else
        vogl_printf("----- Parameter %i match, frame %" PRIu64 ":\n", param_index, total_swaps);

    vogl_printf("%s\n", packet_as_json.get_ptr());
}

//----------------------------------------------------------------------------------------------------------------------
// tool_find_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_find_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string input_base_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_base_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input JSON/blob trace files!\n");
        return false;
    }

    dynamic_string actual_input_filename;
    vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(vogl_open_trace_file(input_base_filename, actual_input_filename, g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()));
    if (!pTrace_reader.get())
        return false;

    bigint128 value_to_find(static_cast<uint64_t>(gl_enums::cUnknownEnum));
    bool has_find_param = g_command_line_params().has_key("find_param");
    if (has_find_param)
    {
        dynamic_string find_value_str(g_command_line_params().get_value_as_string("find_param"));
        if ((find_value_str.has_content()) && (vogl_isalpha(find_value_str[0])))
        {
            value_to_find = get_gl_enums().find_enum(find_value_str);
        }

        if (value_to_find == static_cast<uint64_t>(gl_enums::cUnknownEnum))
        {
            bool find_param_u64_valid = false;
            value_to_find = g_command_line_params().get_value_as_uint64("find_param", 0, 0, 0, cUINT64_MAX, 0, &find_param_u64_valid);

            if (!find_param_u64_valid)
            {
                bool find_param_i64_valid = false;
                value_to_find = g_command_line_params().get_value_as_int64("find_param", 0, 0, 0, cINT64_MAX, 0, &find_param_i64_valid);

                if (!find_param_i64_valid)
                {
                    vogl_error_printf("Failed parsing \"-find_param X\" command line option!\n");
                    return false;
                }
            }
        }
    }

    dynamic_string find_namespace_str(g_command_line_params().get_value_as_string_or_empty("find_namespace"));
    vogl_namespace_t find_namespace = VOGL_NAMESPACE_UNKNOWN;
    if (find_namespace_str.has_content())
    {
        find_namespace = vogl_find_namespace_from_gl_type(find_namespace_str.get_ptr());
        if ((find_namespace == VOGL_NAMESPACE_INVALID) && (find_namespace_str != "invalid"))
        {
            vogl_error_printf("Invalid namespace: \"%s\"\n", find_namespace_str.get_ptr());
            return false;
        }
    }

    dynamic_string find_param_name(g_command_line_params().get_value_as_string_or_empty("find_param_name"));

    dynamic_string find_func_pattern(g_command_line_params().get_value_as_string_or_empty("find_func"));
    regexp func_regex;
    if (find_func_pattern.has_content())
    {
        if (!func_regex.init(find_func_pattern.get_ptr(), REGEX_IGNORE_CASE))
        {
            vogl_error_printf("Invalid func regex: \"%s\"\n", find_func_pattern.get_ptr());
            return false;
        }
    }

    int64_t find_frame_low = g_command_line_params().get_value_as_int64("find_frame_low", 0, -1);
    int64_t find_frame_high = g_command_line_params().get_value_as_int64("find_frame_high", 0, -1);
    int64_t find_call_low = g_command_line_params().get_value_as_int64("find_call_low", 0, -1);
    int64_t find_call_high = g_command_line_params().get_value_as_int64("find_call_high", 0, -1);

    vogl_printf("Scanning trace file %s\n", actual_input_filename.get_ptr());

    vogl_ctypes trace_gl_ctypes(pTrace_reader->get_sof_packet().m_pointer_sizes);
    vogl_trace_packet trace_packet(&trace_gl_ctypes);

    uint64_t total_matches = 0;
    uint64_t total_swaps = 0;

    for (;;)
    {
        vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

        if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
        {
            vogl_error_printf("Failed reading from trace file!\n");
            goto done;
        }

        if (read_status == vogl_trace_file_reader::cEOF)
            break;

        const vogl::vector<uint8> &packet_buf = pTrace_reader->get_packet_buf();
        VOGL_NOTE_UNUSED(packet_buf);

        const vogl_trace_stream_packet_base &base_packet = pTrace_reader->get_base_packet();
        VOGL_NOTE_UNUSED(base_packet);

        const vogl_trace_gl_entrypoint_packet *pGL_packet = NULL;
        VOGL_NOTE_UNUSED(pGL_packet);

        if (pTrace_reader->get_packet_type() == cTSPTEOF)
            break;
        else if (pTrace_reader->get_packet_type() != cTSPTGLEntrypoint)
            continue;

        if (!trace_packet.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
        {
            console::error("%s: Failed parsing GL entrypoint packet\n", VOGL_FUNCTION_NAME);
            goto done;
        }

        pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();

        if (find_frame_low >= 0)
        {
            if (total_swaps < static_cast<uint64_t>(find_frame_low))
                goto skip;
        }
        if (find_frame_high >= 0)
        {
            if (total_swaps > static_cast<uint64_t>(find_frame_high))
                break;
        }

        if (find_call_low >= 0)
        {
            if (trace_packet.get_call_counter() < static_cast<uint64_t>(find_call_low))
                goto skip;
        }
        if (find_call_high >= 0)
        {
            if (trace_packet.get_call_counter() > static_cast<uint64_t>(find_call_high))
                break;
        }

        if (func_regex.is_initialized())
        {
            if (!func_regex.full_match(trace_packet.get_entrypoint_desc().m_pName))
                goto skip;
        }

        if (!has_find_param)
        {
            print_match(trace_packet, -2, -1, total_swaps);
            total_matches++;
        }
        else
        {
            if (trace_packet.has_return_value())
            {
                if ((find_param_name.is_empty()) || (find_param_name == "return"))
                {
                    if (param_value_matches(value_to_find, find_namespace, trace_packet.get_return_value_data(), trace_packet.get_return_value_ctype(), trace_packet.get_return_value_namespace()))
                    {
                        print_match(trace_packet, -1, -1, total_swaps);
                        total_matches++;
                    }
                }
            }

            for (uint i = 0; i < trace_packet.total_params(); i++)
            {
                if ((find_param_name.has_content()) && (find_param_name != trace_packet.get_param_desc(i).m_pName))
                    continue;

                const vogl_ctype_desc_t &param_ctype_desc = trace_packet.get_param_ctype_desc(i);

                if (param_ctype_desc.m_is_pointer)
                {
                    if ((!param_ctype_desc.m_is_opaque_pointer) && (param_ctype_desc.m_pointee_ctype != VOGL_VOID) && (trace_packet.has_param_client_memory(i)))
                    {
                        const vogl_client_memory_array array(trace_packet.get_param_client_memory_array(i));

                        for (uint j = 0; j < array.size(); j++)
                        {
                            if (param_value_matches(value_to_find, find_namespace, array.get_element<uint64_t>(j), array.get_element_ctype(), trace_packet.get_param_namespace(i)))
                            {
                                print_match(trace_packet, i, j, total_swaps);
                                total_matches++;
                            }
                        }
                    }
                }
                else if (param_value_matches(value_to_find, find_namespace, trace_packet.get_param_data(i), trace_packet.get_param_ctype(i), trace_packet.get_param_namespace(i)))
                {
                    print_match(trace_packet, i, -1, total_swaps);
                    total_matches++;
                }
            }
        }

    skip:
        if (vogl_is_swap_buffers_entrypoint(trace_packet.get_entrypoint_id()))
            total_swaps++;
    }

done:
    vogl_printf("Total matches found: %" PRIu64 "\n", total_matches);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_compare_hash_files
//----------------------------------------------------------------------------------------------------------------------
static bool tool_compare_hash_files()
{
    dynamic_string src1_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    dynamic_string src2_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if ((src1_filename.is_empty()) || (src2_filename.is_empty()))
    {
        vogl_error_printf("Must specify two source filenames!\n");
        return false;
    }

    dynamic_string_array src1_lines;
    if (!file_utils::read_text_file(src1_filename.get_ptr(), src1_lines, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
    {
        console::error("%s: Failed reading source file %s!\n", VOGL_FUNCTION_NAME, src1_filename.get_ptr());
        return false;
    }

    vogl_printf("Read 1st source file \"%s\", %u lines\n", src1_filename.get_ptr(), src1_lines.size());

    dynamic_string_array src2_lines;
    if (!file_utils::read_text_file(src2_filename.get_ptr(), src2_lines, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines | file_utils::cRTFIgnoreCommentedLines | file_utils::cRTFPrintErrorMessages))
    {
        console::error("%s: Failed reading source file %s!\n", VOGL_FUNCTION_NAME, src2_filename.get_ptr());
        return false;
    }

    vogl_printf("Read 2nd source file \"%s\", %u lines\n", src2_filename.get_ptr(), src2_lines.size());

    const uint64_t sum_comp_thresh = g_command_line_params().get_value_as_uint64("sum_compare_threshold");

    const uint compare_first_frame = g_command_line_params().get_value_as_uint("compare_first_frame");
    if (compare_first_frame > src2_lines.size())
    {
        vogl_error_printf("%s: -compare_first_frame is %u, but the second file only has %u frames!\n", VOGL_FUNCTION_NAME, compare_first_frame, src2_lines.size());
        return false;
    }

    const uint lines_to_comp = math::minimum(src1_lines.size(), src2_lines.size() - compare_first_frame);

    if (src1_lines.size() != src2_lines.size())
    {
        // FIXME: When we replay q2, we get 2 more frames vs. tracing. Not sure why, this needs to be investigated.
        if ( (!g_command_line_params().get_value_as_bool("ignore_line_count_differences")) && (labs(src1_lines.size() - src2_lines.size()) > 3) )
        {
            vogl_error_printf("%s: Input files have a different number of lines! (%u vs %u)\n", VOGL_FUNCTION_NAME, src1_lines.size(), src2_lines.size());
            return false;
        }
        else
        {
            vogl_warning_printf("%s: Input files have a different number of lines! (%u vs %u)\n", VOGL_FUNCTION_NAME, src1_lines.size(), src2_lines.size());
        }
    }

    const uint compare_ignore_frames = g_command_line_params().get_value_as_uint("compare_ignore_frames");
    if (compare_ignore_frames > lines_to_comp)
    {
        vogl_error_printf("%s: -compare_ignore_frames is too large!\n", VOGL_FUNCTION_NAME);
        return false;
    }

    const bool sum_hashing = g_command_line_params().get_value_as_bool("sum_hashing");

    if (g_command_line_params().has_key("compare_expected_frames"))
    {
        const uint compare_expected_frames = g_command_line_params().get_value_as_uint("compare_expected_frames");
        if ((src1_lines.size() != compare_expected_frames) || (src2_lines.size() != compare_expected_frames))
        {
            vogl_warning_printf("%s: Expected %u frames! First file has %u frames, second file has %u frames.\n", VOGL_FUNCTION_NAME, compare_expected_frames, src1_lines.size(), src2_lines.size());
            return false;
        }
    }

    uint total_mismatches = 0;
    uint64_t max_sum_delta = 0;

    for (uint i = compare_ignore_frames; i < lines_to_comp; i++)
    {
        const char *pStr1 = src1_lines[i].get_ptr();
        const char *pStr2 = src2_lines[i + compare_first_frame].get_ptr();

        uint64_t val1 = 0, val2 = 0;
        if (!string_ptr_to_uint64(pStr1, val1))
        {
            vogl_error_printf("%s: Failed parsing line at index %u of first source file!\n", VOGL_FUNCTION_NAME, i);
            return false;
        }

        if (!string_ptr_to_uint64(pStr2, val2))
        {
            vogl_error_printf("%s: Failed parsing line at index %u of second source file!\n", VOGL_FUNCTION_NAME, i);
            return false;
        }

        bool mismatch = false;

        if (sum_hashing)
        {
            uint64_t delta;
            if (val1 > val2)
                delta = val1 - val2;
            else
                delta = val2 - val1;
            max_sum_delta = math::maximum(max_sum_delta, delta);

            if (delta > sum_comp_thresh)
                mismatch = true;
        }
        else
        {
            mismatch = val1 != val2;
        }

        if (mismatch)
        {
            if (sum_hashing)
                vogl_error_printf("Mismatch at frame %u: %" PRIu64 ", %" PRIu64 "\n", i, val1, val2);
            else
                vogl_error_printf("Mismatch at frame %u: 0x%" PRIX64 ", 0x%" PRIX64 "\n", i, val1, val2);
            total_mismatches++;
        }
    }

    if (sum_hashing)
        vogl_printf("Max sum delta: %" PRIu64 "\n", max_sum_delta);

    if (!total_mismatches)
        vogl_printf("No mismatches\n");
    else
    {
        vogl_error_printf("%u total mismatches!\n", total_mismatches);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_unpack_json_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_unpack_json_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string input_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input UBJ file!\n");
        return false;
    }

    dynamic_string output_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of output text file!\n");
        return false;
    }

    json_document doc;
    vogl_message_printf("Reading UBJ file \"%s\"\n", input_filename.get_ptr());

    if (!doc.binary_deserialize_file(input_filename.get_ptr()))
    {
        vogl_error_printf("Unable to deserialize input file \"%s\"!\n", input_filename.get_ptr());
        if (doc.get_error_msg().has_content())
            vogl_error_printf("%s\n", doc.get_error_msg().get_ptr());
        return false;
    }

    if (!doc.serialize_to_file(output_filename.get_ptr()))
    {
        vogl_error_printf("Failed serializing to output file \"%s\"!\n", output_filename.get_ptr());
        return false;
    }

    vogl_message_printf("Wrote textual JSON file to \"%s\"\n", output_filename.get_ptr());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// tool_pack_json_mode
//----------------------------------------------------------------------------------------------------------------------
static bool tool_pack_json_mode()
{
    VOGL_FUNC_TRACER

    dynamic_string input_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
    if (input_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of input text file!\n");
        return false;
    }

    dynamic_string output_filename(g_command_line_params().get_value_as_string_or_empty("", 2));
    if (output_filename.is_empty())
    {
        vogl_error_printf("Must specify filename of output UBJ file!\n");
        return false;
    }

    json_document doc;
    vogl_message_printf("Reading JSON text file \"%s\"\n", input_filename.get_ptr());

    if (!doc.deserialize_file(input_filename.get_ptr()))
    {
        vogl_error_printf("Unable to deserialize input file \"%s\"!\n", input_filename.get_ptr());
        if (doc.get_error_msg().has_content())
            vogl_error_printf("%s (Line: %u)\n", doc.get_error_msg().get_ptr(), doc.get_error_line());
        return false;
    }

    if (!doc.binary_serialize_to_file(output_filename.get_ptr()))
    {
        vogl_error_printf("Failed serializing to output file \"%s\"!\n", output_filename.get_ptr());
        return false;
    }

    vogl_message_printf("Wrote binary UBJ file to \"%s\"\n", output_filename.get_ptr());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// xerror_handler
//----------------------------------------------------------------------------------------------------------------------
static int xerror_handler(Display *dsp, XErrorEvent *error)
{
    char error_string[256];
    XGetErrorText(dsp, error->error_code, error_string, sizeof(error_string));

    fprintf(stderr, "voglreplay: Fatal X Windows Error: %s\n", error_string);
    abort();
}

//----------------------------------------------------------------------------------------------------------------------
// main
//----------------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
#if VOGL_FUNCTION_TRACING
    fflush(stdout);
    fflush(stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    VOGL_FUNC_TRACER

    // Initialize vogl_core.
    vogl_core_init();

    XSetErrorHandler(xerror_handler);

    if (!vogl_replay_init(argc, argv))
    {
        vogl_replay_deinit();
        return EXIT_FAILURE;
    }

#if VOGL_REMOTING
    vogl_init_listener();
#endif // VOGL_REMOTING

#if 0
	test();
	return EXIT_SUCCESS;
#endif

    if (g_command_line_params().get_count("") < 2)
    {
        vogl_error_printf("Must specify at least one trace (or input) files!\n");

        tool_print_help();

        vogl_replay_deinit();
        return EXIT_FAILURE;
    }

    if (g_command_line_params().get_value_as_bool("pause"))
    {
        vogl_message_printf("Press key to continue\n");
        vogl_sleep(1000);
        getchar();
    }

    bool success = false;

    if (g_command_line_params().get_value_as_bool("dump"))
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "dump");
        vogl_message_printf("Dump from binary to JSON mode\n");

        success = tool_dump_mode();
    }
    else if (g_command_line_params().get_value_as_bool("parse"))
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "parse");
        vogl_message_printf("Parse from JSON to binary mode\n");

        success = tool_parse_mode();
    }
    else if (g_command_line_params().get_value_as_bool("info"))
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "info");
        vogl_message_printf("Dumping trace information\n");

        success = tool_info_mode();
    }
    else if (g_command_line_params().get_value_as_bool("unpack_json"))
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "unpack_json");
        vogl_message_printf("Unpacking UBJ to text\n");

        success = tool_unpack_json_mode();
    }
    else if (g_command_line_params().get_value_as_bool("pack_json"))
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "pack_json");
        vogl_message_printf("Packing textual JSON to UBJ\n");

        success = tool_pack_json_mode();
    }
    else if (g_command_line_params().get_value_as_bool("find"))
    {
        vogl_message_printf("Find mode\n");

        success = tool_find_mode();
    }
    else if (g_command_line_params().get_value_as_bool("compare_hash_files"))
    {
       vogl_message_printf("Comparing hash/sum files\n");

       success = tool_compare_hash_files();
    }
    else
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "replay_mode");
        vogl_message_printf("Replay mode\n");

        success = tool_replay_mode();
    }

    console::printf("%u warning(s), %u error(s)\n", console::get_total_messages(cWarningConsoleMessage), console::get_total_messages(cErrorConsoleMessage));

    vogl_replay_deinit();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
