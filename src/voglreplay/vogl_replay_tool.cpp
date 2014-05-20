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
bool tool_play_mode();

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
static void *g_actual_libgl_module_handle;
static cfile_stream *g_vogl_pLog_stream;

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs_play[] =
{
    // play specific
    { "width", 1, false, "Replay: Set replay window's initial width (default is 1024)" },
    { "height", 1, false, "Replay: Set replay window's initial height (default is 768)" },
    { "msaa", 1, false, "Replay: Set replay window's multisamples (default is 0)." },
    { "lock_window_dimensions", 0, false, "Replay: Don't automatically change window's dimensions during replay" },
    { "endless", 0, false, "Replay: Loop replay endlessly instead of exiting" },
    { "force_debug_context", 0, false, "Replay: Force GL debug contexts" },
    { "loop_frame", 1, false, "Replay: loop mode's start frame" },
    { "loop_len", 1, false, "Replay: loop mode's loop length" },
    { "loop_count", 1, false, "Replay: loop mode's loop count" },
    { "benchmark", 0, false, NULL }, // Always set hidden option.
};

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs_replay[] =
{
    // replay specific
    { "width", 1, false, "Replay: Set replay window's initial width (default is 1024)" },
    { "height", 1, false, "Replay: Set replay window's initial height (default is 768)" },
    { "msaa", 1, false, "Replay: Set replay window's multisamples (default is 0)." },
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
    { "swap_sleep", 1, false, "Replay: Sleep for X milliseconds after every swap" },
    { "pause_on_exit", 0, false, "Replay: Wait for a keypress on exit" },
    { "debug_test_snapshot_serialization", 0, false, "Interactive Replay Mode: Immediately serialize/deserialize state snapshots after taking them" },
    { "pause_on_frame", 1, false, "Replay interactive mode: Pause on specified frame" },
    { "interactive", 0, false, "Replay mode: Enable keyboard keys" },
    { "disable_snapshot_caching", 0, false, "Replay mode: Disable caching of all state snapshot files, so they can be manually modified during replay" },
    { "keyframe_base_filename", 1, false, "Replay: Set base filename of trimmed replay keyframes, used for fast seeking" },
    { "loop_frame", 1, false, "Replay: loop mode's start frame" },
    { "loop_len", 1, false, "Replay: loop mode's loop length" },
    { "loop_count", 1, false, "Replay: loop mode's loop count" },
    { "draw_kill_max_thresh", 1, false, "Replay: Enable draw kill mode during looping to visualize order of draws, sets the max # of draws before counter resets to 0" },

    // replay flags
    { "benchmark", 0, false, "Replay mode: Disable glGetError()'s, divergence checks, state teardown/restore, during replaying" },
    { "allow_state_teardown", 0, false, "Benchmark: When in benchmark mode, enables state teardown/restore at frame loop boundaries" },
    { "verbose", 0, false, "Verbose debug output" },
    { "force_debug_context", 0, false, "Replay: Force GL debug contexts" },
    { "dump_all_packets", 0, false, "Replay: Dump all GL trace packets as JSON to stdout" },
    { "debug", 0, false, "Enable verbose debug information" },
    { "lock_window_dimensions", 0, false, "Replay: Don't automatically change window's dimensions during replay" },
    { "replay_debug", 0, false, "Enable various debug/verification code in the replayer" },
    { "dump_packet_blob_files_on_error", 0, false, "Replay: Used with -dump_packets_on_error, also dumps all binary blob files associated with each packet" },
    { "dump_shaders_on_draw", 0, false, "Replay: Dump shader source on draw calls" },
    { "dump_packets_on_error", 0, false, "Replay: Dump GL trace packets as JSON to stdout on replay errors" },
    { "dump_screenshots", 0, false, "Replay: Dump backbuffer screenshot before every swap to numbered PNG files" },
    { "dump_screenshots_prefix", 1, false, "Replay: Set PNG screenshot file prefix" },
    { "hash_backbuffer", 0, false, "Replay: Hash and output backbuffer CRC before every swap" },
    { "dump_backbuffer_hashes", 1, false, "Replay: Dump backbuffer hashes to a text file" },
    { "sum_hashing", 0, false, "Replay: Use per-component sums, instead of CRC hashing (useful for multisampling)" },
    { "dump_framebuffer_on_draw", 0, false, "Replay: Dump framebuffer to PNG files after each draw/glEnd/glCallList" },
    { "dump_framebuffer_on_draw_prefix", 1, false, "Replay: Base path/filename to use for --dump_framebuffer_on_draw" },
    { "dump_framebuffer_on_draw_frame", 1, false, "Replay: Limit dumping framebuffer PNG files" },
    { "dump_framebuffer_on_draw_first_gl_call", 1, false, "Replay: Limit dumping framebuffer PNG files" },
    { "dump_framebuffer_on_draw_last_gl_call", 1, false, "Replay: Limit dumping framebuffer PNG files" },
    { "clear_uninitialized_bufs", 0, false, "glBufferData(): Ensure buffers are unitialized to all-zeros when data param is NULL" },
    { "disable_frontbuffer_restore", 0, false, "Replay: Do not restore the front buffer's contents when restoring a state snapshot" },
};

static command_line_param_desc g_command_line_param_descs_find[] =
{
    // find specific
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
    { "find_func", 1, false, "Find: Limit the find to only the specified function name POSIX regex pattern" },
    { "find_param", 1, false, "Find: The parameter value to find, hex, decimal integers, or GL enum strings OK" },
    { "find_namespace", 1, false, "Find: Limits --find_param to only parameters using the specified handle namespace: invalid, GLhandleARB, GLframebuffer, GLtexture, GLrenderbuffer, GLquery, GLsampler, GLprogramARB, GLprogram, GLarray, GLlist, GLlocation, GLlocationARB, GLfence, GLsync, GLpipeline, GLshader, GLbuffer, GLfeedback, GLarrayAPPLE, GLfragmentShaderATI" },
    { "find_param_name", 1, false, "Find: Limits the find to only params with the specified name (specify \"return\" to limit search to only return values)" },
    { "find_frame_low", 1, false, "Find: Limit the find to frames beginning at the specified frame index" },
    { "find_frame_high", 1, false, "Find: Limit the find to frames up to and including the specified frame index" },
    { "find_call_low", 1, false, "Find: Limit the find to GL calls beginning at the specified call index" },
    { "find_call_high", 1, false, "Find: Limit the find to GL calls up to and including the specified call index" },
};

static command_line_param_desc g_command_line_param_descs_compare_hash_files[] =
{
    // compare_hash_files specific
    { "sum_compare_threshold", 1, false, "compare_hash_files: Only report mismatches greater than the specified threshold, use with --sum_hashing" },
    { "sum_hashing", 0, false, "Replay: Use per-component sums, instead of CRC hashing (useful for multisampling)" },
    { "compare_ignore_frames", 1, false, "compare_hash_files: Ignore first X frames" },
    { "compare_expected_frames", 1, false, "compare_hash_files: Fail if the # of frames is not X" },
    { "compare_first_frame", 1, false, "compare_hash_files: First frame to compare to in second hash file" },
    { "ignore_line_count_differences", 0, false, "compare_hash_files: Don't stop if the # of lines differs between the two files" },
};

static command_line_param_desc g_command_line_param_descs_parse[] =
{
    // parse specific
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
};

static command_line_param_desc g_command_line_param_descs_info[] =
{
    // info specific
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
};

static command_line_param_desc g_command_line_param_descs_unpack_json[] =
{
    // unpack_json specific
};

static command_line_param_desc g_command_line_param_descs_pack_json[] =
{
    // pack_json specific
};

static command_line_param_desc g_command_line_param_descs_dump[] =
{
    // dump specific
    { "verify", 0, false, "Dump: Fully round-trip verify all JSON objects vs. the original packet's" },
    { "write_debug_info", 0, false, "Dump: Write extra debug info to output JSON trace files" },
    { "loose_file_path", 1, false, "Prefer reading trace blob files from this directory vs. the archive referred to or present in the trace file" },
    { "debug", 0, false, "Enable verbose debug information" },
};

static command_line_param_desc g_command_line_param_descs_common[] =
{
    // common command options
    { "logfile", 1, false, "Create logfile" },
    { "logfile_append", 1, false, "Append output to logfile" },
    { "pause", 0, false, "Wait for a key at startup (so a debugger can be attached)" },
    { "quiet", 0, false, "Disable all console output" },
    { "gl_debug_log", 0, false, "Dump GL prolog/epilog messages to stdout (very slow - helpful to narrow down driver crashes)" },
#if VOGL_FUNCTION_TRACING
    { "vogl_func_tracing", 0, false, "Enable vogl function tracing (must build with VOGL_FUNCTION_TRACING)" },
#endif
#ifdef USE_TELEMETRY
    { "telemetry_level", 1, false, "Set Telemetry level." },
#endif
};

static command_line_param_desc g_command_line_interactive_descs[] =
{
    // replay interactive specific commands
    { "s", 0, false, "slow mode" },
    { "<space>", 0, false, "pause" },
    { "r", 0, false, "rewind to beginning" },
    { "e", 0, false, "seek to last frame" },
    { "t", 0, false, "trim frame" },
    { "j", 0, false, "trim and play json" },
    { "<left>", 0, false, "step left" },
    { "<right>", 0, false, "step right" },
};

struct command_t
{
    const char *name;
    const char *desc;
    bool (*func)(void);
    command_line_param_desc *cmdline_desc;
    size_t cmdline_desc_count;
    const char *filename_args;
};

static const command_t g_options[] =
{
#define XDEF(_x, _desc, _args) { #_x, _desc, tool_ ## _x ## _mode, \
        g_command_line_param_descs_ ## _x, \
        sizeof(g_command_line_param_descs_ ## _x) / sizeof(g_command_line_param_descs_ ## _x[0]), \
        _args }

    // voglreplay subcommands
    XDEF(play, "Play mode, must specify .BIN or .JSON trace file to play", "<input JSON/blob trace filename>"),
    XDEF(replay, "Replay mode, must specify .BIN or .JSON trace file to replay", "<input JSON/blob trace filename>"),
    XDEF(dump, "Dump mode: Dumps binary trace file to a JSON trace file, must specify input and output filenames", "<input binary trace filename> <output JSON/blob base filename>"),
    XDEF(parse, "Parse mode: Parse JSON trace file to a binary trace file, must specify input and output filenames", "<input JSON/blob trace filename>"),
    XDEF(info, "Info mode: Output statistics about a trace file", "<input JSON/blob trace filename>"),
    XDEF(unpack_json, "Unpack UBJ to JSON mode: Unpack UBJ (Universal Binary JSON) to textual JSON, must specify input and output filenames", "<input UBJ filename> <output text filename>"),
    XDEF(pack_json, "Pack JSON to UBJ mode: Pack textual JSON to UBJ, must specify input and output filenames", "<input text filename> <output UBJ filename>"),
    XDEF(find, "Find all calls with parameters containing a specific value, combine with -find_param, -find_func, find_namespace, etc. params", "<input JSON/blob trace filename>"),
    XDEF(compare_hash_files, "Comparing hash/sum files", "<input hash filename1> <input hash filename2>"),

#undef XDEF
};
static const size_t g_options_count = sizeof(g_options) / sizeof(g_options[0]);

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
// vogl_replay_deinit
//----------------------------------------------------------------------------------------------------------------------
static void vogl_replay_deinit()
{
    VOGL_FUNC_TRACER

    colorized_console::deinit();
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_help
//----------------------------------------------------------------------------------------------------------------------
VOGL_NORETURN static void tool_print_help(const command_t *cmd, const command_line_param_desc *descs, size_t descs_count)
{
    VOGL_FUNC_TRACER

    if (cmd == NULL)
    {
        console::printf("\nUsage: voglreplay [--help] <command> [<args>]\n\n");

        printf("The voglreplay commands are:\n");
        for (size_t i = 0; i < g_options_count; i++)
        {
            console::message("  %s:", g_options[i].name);
            printf(" %s.\n", g_options[i].desc);
        }
        printf("\n");
    }
    else
    {
        console::printf("\nUsage: ");
        console::message("voglreplay %s %s", cmd->name, cmd->filename_args);
        console::printf(" [<args>]\n\n");

        if (descs_count)
        {
            dump_command_line_info(descs_count, descs, "--", true);
            printf("\n");
        }

        printf("Common options:\n");
        dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_param_descs_common), g_command_line_param_descs_common, "--", true);

        if (!vogl_strcmp(cmd->name, "replay"))
        {
            console::printf("\nInteractive replay mode keys:\n");
            dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_interactive_descs), g_command_line_interactive_descs, " ", true);
        }
    }

    vogl_replay_deinit();
    exit(EXIT_FAILURE);
}

//----------------------------------------------------------------------------------------------------------------------
// init_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static const command_t *init_command_line_params(int argc, char *argv[])
{
    VOGL_FUNC_TRACER

    dynamic_string_array args = get_command_line_params(argc, argv);
    if (args.size() <= 1)
        tool_print_help(NULL, NULL, 0);

    if ((args[1] == "help") || (args[1] == "--help") || (args[1] == "-h") || (args[1] == "-?"))
    {
        if (args.size() > 2)
        {
            // Transform "voglreplay --help command" to "voglreplay command --help".
            args[1] = args[2];
            args[2] = "--help";
        }
        else
        {
            // Spew command help and exit.
            tool_print_help(NULL, NULL, 0);
        }
    }

    // Search for this specific command.
    const command_t *cmd = NULL;
    for (size_t i = 0; i < g_options_count; i++)
    {
        if (args[1] == g_options[i].name)
        {
            cmd = &g_options[i];
            break;
        }
    }

    if (!cmd)
    {
        // Command not found: spew command help and exit.
        vogl_error_printf("Unknown voglreplay command '%s'.\n", args[1].c_str());
        tool_print_help(NULL, NULL, 0);
    }

    vogl::vector<command_line_param_desc> cmdline_desc;

    cmdline_desc.append(cmd->cmdline_desc, cmd->cmdline_desc_count);

    // Check if they want help for this command.
    for (size_t i = 0; i < args.size(); i++)
    {
        if ((args[i] == "help") || (args[i] == "--help") || (args[i] == "-h") || (args[i] == "-?"))
            tool_print_help(cmd, cmdline_desc.get_ptr(), cmdline_desc.size());
    }

    // If we're doing a playback, add the --benchmark flag by default.
    if (args[1] == "play")
        args.push_back("--benchmark");

    // Erase the subcommand so it doesn't show up as a filename.
    args.erase(1, 1);

    // Add the common options to the list and parse em all.
    cmdline_desc.append(g_command_line_param_descs_common, VOGL_ARRAY_SIZE(g_command_line_param_descs_common));

    // Parse the command line options for this command.
    command_line_params::parse_config parse_cfg;
    parse_cfg.m_single_minus_params = true;
    parse_cfg.m_double_minus_params = true;

    if (!g_command_line_params().parse(args, cmdline_desc.size(), &cmdline_desc[0], parse_cfg))
    {
        vogl_error_printf("%s: Failed parsing command line parameters!\n", VOGL_FUNCTION_INFO_CSTR);
        return NULL;
    }

    if (!init_logfile())
        return NULL;

    return cmd;
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
static const command_t *vogl_replay_init(int argc, char *argv[])
{
    VOGL_FUNC_TRACER

    colorized_console::init();
    colorized_console::set_exception_callback();
    //console::set_tool_prefix("(voglreplay) ");

    tool_print_title();

    const command_t *cmd = init_command_line_params(argc, argv);
    if (cmd)
    {
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
            return NULL;

        bool wrap_all_gl_calls = true;
        if (g_command_line_params().get_value_as_bool("benchmark"))
            wrap_all_gl_calls = false;

        vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper, wrap_all_gl_calls);
    }

    return cmd;
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

    const command_t *cmd = vogl_replay_init(argc, argv);
    if (!cmd)
    {
        vogl_replay_deinit();
        return EXIT_FAILURE;
    }

#ifdef VOGL_REMOTING
    vogl_init_listener();
#endif

    if (g_command_line_params().get_value_as_bool("pause"))
    {
        vogl_message_printf("Press key to continue\n");
        vogl_sleep(1000);
        getchar();
    }

    bool success = false;
    {
        tmZone(TELEMETRY_LEVEL0, TMZF_NONE, cmd->name);

        vogl_message_printf("%s.\n", cmd->desc);
        success = cmd->func();
    }

    console::printf("%u warning(s), %u error(s)\n", 
                    console::get_total_messages(cWarningConsoleMessage),
                    console::get_total_messages(cErrorConsoleMessage));

    vogl_replay_deinit();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
