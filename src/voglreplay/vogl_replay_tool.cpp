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
#include "vogl_colorized_console.h"
#include "libtelemetry.h"

bool tool_dump_mode();
bool tool_parse_mode();
bool tool_info_mode();
bool tool_unpack_json_mode();
bool tool_pack_json_mode();
bool tool_find_mode();
bool tool_compare_hash_files_mode();
bool tool_replay_mode();

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
    { "benchmark", 0, false, "Replay mode: Disable glGetError()'s, divergence checks, state teardown/restore, during replaying" },
    { "allow_state_teardown", 0, false, "Benchmark: When in benchmark mode, enables state teardown/restore at frame loop boundaries" },
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
        vogl_error_printf("%s: Failed opening log file \"%s\"\n", VOGL_FUNCTION_INFO_CSTR, filename.get_ptr());
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
        vogl_error_printf("%s: Failed parsing command line parameters!\n", VOGL_FUNCTION_INFO_CSTR);
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

    g_actual_libgl_module_handle = plat_load_system_gl(PLAT_RTLD_LAZY);
    
    if (!g_actual_libgl_module_handle)
    {
        vogl_error_printf("%s: Failed loading %s!\n", VOGL_FUNCTION_INFO_CSTR, plat_get_system_gl_module_name());
        return false;
    }

    #if (VOGL_PLATFORM_HAS_GLX)
        GL_ENTRYPOINT(glXGetProcAddress) = reinterpret_cast<glXGetProcAddress_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, "glXGetProcAddress"));
        if (!GL_ENTRYPOINT(glXGetProcAddress))
        {
            vogl_error_printf("%s: Failed getting address of glXGetProcAddress() from %s!\n", VOGL_FUNCTION_INFO_CSTR, plat_get_system_gl_module_name());
            return false;
        }
    #elif (VOGL_PLATFORM_HAS_WGL)
        GL_ENTRYPOINT(wglGetProcAddress) = reinterpret_cast<wglGetProcAddress_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, "wglGetProcAddress"));
        if (!GL_ENTRYPOINT(wglGetProcAddress))
        {
            vogl_error_printf("%s: Failed getting address of wglGetProcAddress() from %s!\n", VOGL_FUNCTION_INFO_CSTR, plat_get_system_gl_module_name());
            return false;
        }
    #else
        #error "Implement load_gl for this platform."
    #endif

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_proc_address_helper
//----------------------------------------------------------------------------------------------------------------------
static vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName)
{
    VOGL_FUNC_TRACER

    vogl_void_func_ptr_t pFunc = g_actual_libgl_module_handle ? reinterpret_cast<vogl_void_func_ptr_t>(plat_dlsym(g_actual_libgl_module_handle, pName)) : NULL;

    #if (VOGL_PLATFORM_HAS_GLX)
        if ((!pFunc) && (GL_ENTRYPOINT(glXGetProcAddress)))
            pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(glXGetProcAddress)(reinterpret_cast<const GLubyte *>(pName)));
    #elif (VOGL_PLATFORM_HAS_WGL)
        if ((!pFunc) && (GL_ENTRYPOINT(wglGetProcAddress)))
            pFunc = reinterpret_cast<vogl_void_func_ptr_t>(GL_ENTRYPOINT(wglGetProcAddress)(pName));
    #else
        #error "Implement vogl_get_proc_address_helper this platform."
    #endif


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


#if (VOGL_PLATFORM_HAS_X11)

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

#endif

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

    #if (VOGL_PLATFORM_HAS_X11)
        XSetErrorHandler(xerror_handler);
    #endif

    if (!vogl_replay_init(argc, argv))
    {
        vogl_replay_deinit();
        return EXIT_FAILURE;
    }

#ifdef VOGL_REMOTING
    vogl_init_listener();
#endif // VOGL_REMOTING

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

    static const struct
    {
        const char *name;
        const char *desc;
        bool (*func)(void);
    } s_options[] =
    {
    #define XDEF(_x, _desc) { #_x, _desc, tool_ ## _x ## _mode }
        XDEF(dump, "Dump from binary to JSON mode"),
        XDEF(parse, "Parse from JSON to binary mode"),
        XDEF(info, "Dumping trace information"),
        XDEF(unpack_json, "Unpacking UBJ to text"),
        XDEF(pack_json, "Packing textual JSON to UBJ"),
        XDEF(find, "Find mode"),
        XDEF(compare_hash_files, "Comparing hash/sum files"),
        XDEF(replay, "Replay mode"),
    #undef XDEF
    };
    static const size_t s_options_count = sizeof(s_options) / sizeof(s_options[0]);

    bool success = false;
    for (size_t i = 0; i < s_options_count; i++)
    {
        // Check if the the command is specified. Execute last command by default if none found.
        if (g_command_line_params().get_value_as_bool(s_options[i].name) || (i == s_options_count - 1))
        {
            tmZone(TELEMETRY_LEVEL0, TMZF_NONE, s_options[i].name);
            vogl_message_printf("%s.\n", s_options[i].desc);

            success = s_options[i].func();
            break;
        }
    }

    console::printf("%u warning(s), %u error(s)\n", 
                    console::get_total_messages(cWarningConsoleMessage),
                    console::get_total_messages(cErrorConsoleMessage));

    vogl_replay_deinit();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
