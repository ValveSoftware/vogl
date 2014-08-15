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

// File: voglbench.cpp
#include "vogl_common.h"
#include "vogl_gl_replayer.h"
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

#include "libtelemetry.h"

#if (VOGL_PLATFORM_HAS_SDL)
    #define SDL_MAIN_HANDLED 1
    #include "SDL.h"
#endif

//$ TODO: investigate using SDL for windows and any keyboard controls.
//$ Run clang-format on everything.
//$ Use Telemetry to speed this bugger up.

//----------------------------------------------------------------------------------------------------------------------
// command line params
//----------------------------------------------------------------------------------------------------------------------
static command_line_param_desc g_command_line_param_descs[] =
{
    { "width", 1, false, "Replay: Set initial window width (default is 1024)" },
    { "height", 1, false, "Replay: Set initial window height (default is 768)" },
    { "msaa", 1, false, "Replay: Set initial window multisamples (default is 0)" },
    { "lock_window_dimensions", 0, false, "Replay: Don't automatically change window's dimensions during replay" },
    { "endless", 0, false, "Replay: Loop replay endlessly instead of exiting" },
    { "force_debug_context", 0, false, "Replay: Force GL debug contexts" },
#ifdef USE_TELEMETRY
    { "telemetry_level", 1, false, "Set Telemetry level." },
#endif
    { "loop_frame", 1, false, "Replay: loop mode's start frame" },
    { "loop_len", 1, false, "Replay: loop mode's loop length" },
    { "loop_count", 1, false, "Replay: loop mode's loop count" },
    { "logfile", 1, false, "Create logfile" },
    { "help", 0, false, "Display this help" },
    { "?", 0, false, "Display this help" },
    { "pause", 0, false, "Wait for a key at startup (so a debugger can be attached)" },
    { "quiet", 0, false, "Disable warning, verbose, and debug output" },
    { "verbose", 0, false, "Enable verbose output" },
    { "debug", 0, false, "Enable verbose debug information" },
    { "fs_preprocessor", 1, false, "Replay: Run all FS through specified pre-processor prior to glCreateShader() call and substitute the shader output by the pre-processor into the glCreateShader() call." },
    { "fs_preprocessor_options", 1, false, "Replay: Options to pass to the FS pre-processor.  Must also specify --fs_preprocessor" },
    { "fs_preprocessor_prefix", 1, false, "Replay: Dir/prefix to append to FS input to PP and output from PP.  Must also specify --fs_preprocessor" },
};

//----------------------------------------------------------------------------------------------------------------------
// init_logfile
//----------------------------------------------------------------------------------------------------------------------
static bool init_logfile()
{
    VOGL_FUNC_TRACER

    dynamic_string filename(g_command_line_params().get_value_as_string_or_empty("logfile"));
    if (filename.is_empty())
        return true;

    return console::set_log_stream_filename(filename.c_str(), false);
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_title
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_title()
{
    VOGL_FUNC_TRACER

    vogl_printf("voglbench ");
    if (sizeof(void *) > 4)
        vogl_printf("64-bit ");
    else
        vogl_printf("32-bit ");
#ifdef VOGL_BUILD_DEBUG
    vogl_printf("Debug ");
#else
    vogl_printf("Release ");
#endif
    vogl_printf("Built %s %s\n", __DATE__, __TIME__);
}

//----------------------------------------------------------------------------------------------------------------------
// tool_print_help
//----------------------------------------------------------------------------------------------------------------------
static void tool_print_help()
{
    VOGL_FUNC_TRACER

    vogl_printf("Usage: voglbench [ -option ... ] input_file optional_output_file [ -option ... ]\n");
    vogl_printf("Command line options may begin with single minus \"-\" or double minus \"--\"\n");

    vogl_printf("\nCommand line options:\n");

    dump_command_line_info(VOGL_ARRAY_SIZE(g_command_line_param_descs), g_command_line_param_descs, "--");
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
        vogl_error_printf("Failed parsing command line parameters!\n");
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
// vogl_direct_gl_func_prolog - This function is called before EVERY single GL/GLX function call we make.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_direct_gl_func_prolog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **ppStack_data)
{
    VOGL_NOTE_UNUSED(entrypoint_id);
    VOGL_NOTE_UNUSED(pUser_data);
    VOGL_NOTE_UNUSED(ppStack_data);

    vogl_printf("* GLPROLOG %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_direct_gl_func_epilog - This function is called immediately after EVERY single GL/GLX function call we make.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_direct_gl_func_epilog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **ppStack_data)
{
    VOGL_NOTE_UNUSED(entrypoint_id);
    VOGL_NOTE_UNUSED(pUser_data);
    VOGL_NOTE_UNUSED(ppStack_data);

    vogl_printf("* GLEPILOG %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);
}

//----------------------------------------------------------------------------------------------------------------------
// voglbench_init
//----------------------------------------------------------------------------------------------------------------------
static bool voglbench_init(int argc, char *argv[])
{
    VOGL_FUNC_TRACER

    colorized_console::init();
    colorized_console::set_exception_callback();
    //console::set_tool_prefix("(voglbench) ");

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
        console::set_output_level(cMsgError);
    else if (g_command_line_params().get_value_as_bool("debug"))
        console::set_output_level(cMsgDebug);
    else if (g_command_line_params().get_value_as_bool("verbose"))
        console::set_output_level(cMsgVerbose);

    if (g_command_line_params().get_value_as_bool("gl_debug_log"))
    {
        vogl_set_direct_gl_func_prolog(vogl_direct_gl_func_prolog, NULL);
        vogl_set_direct_gl_func_epilog(vogl_direct_gl_func_epilog, NULL);
    }

    #if VOGL_PLATFORM_HAS_SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) 
            return false;
    #endif

    if (!load_gl())
        return false;

    bool wrap_all_gl_calls = false;
    vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper, wrap_all_gl_calls);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// voglbench_deinit
//----------------------------------------------------------------------------------------------------------------------
static void voglbench_deinit()
{
    VOGL_FUNC_TRACER

    colorized_console::deinit();
}

//----------------------------------------------------------------------------------------------------------------------
// get_replayer_flags_from_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static uint get_replayer_flags_from_command_line_params()
{
    uint replayer_flags = cGLReplayerBenchmarkMode;

    static struct
    {
        const char *m_pCommand;
        uint m_flag;
    } s_replayer_command_line_params[] =
    {
        { "verbose", cGLReplayerVerboseMode },
        { "force_debug_context", cGLReplayerForceDebugContexts },
        { "debug", cGLReplayerDebugMode },
        { "lock_window_dimensions", cGLReplayerLockWindowDimensions },
        { "fs_preprocessor", cGLReplayerFSPreprocessor },
    };

    for (uint i = 0; i < sizeof(s_replayer_command_line_params) / sizeof(s_replayer_command_line_params[0]); i++)
    {
        if (g_command_line_params().get_value_as_bool(s_replayer_command_line_params[i].m_pCommand))
            replayer_flags |= s_replayer_command_line_params[i].m_flag;
    }

    return replayer_flags;
}

#if (VOGL_PLATFORM_HAS_SDL)
    //----------------------------------------------------------------------------------------------------------------------
    // tool_replay_mode
    //----------------------------------------------------------------------------------------------------------------------
    static bool tool_replay_mode()
    {
        VOGL_FUNC_TRACER

        dynamic_string trace_filename(g_command_line_params().get_value_as_string_or_empty("", 1));
        if (trace_filename.is_empty())
        {
            vogl_error_printf("No trace file specified!\n");
            return false;
        }

        dynamic_string actual_trace_filename;
        vogl_unique_ptr<vogl_trace_file_reader> pTrace_reader(
            vogl_open_trace_file(
                trace_filename,
                actual_trace_filename,
                g_command_line_params().get_value_as_string_or_empty("loose_file_path").get_ptr()
            )
        );

        if (!pTrace_reader.get())
        {
            vogl_error_printf("File not found, or unable to determine file type of trace file \"%s\"\n", trace_filename.get_ptr());
            return false;
        }

        vogl_printf("Reading trace file %s\n", actual_trace_filename.get_ptr());

        vogl_gl_replayer replayer;
        vogl_replay_window window;

        #if defined(PLATFORM_WINDOWS)
            // We need to get proc addresses for windows late.
            replayer.set_proc_address_helper(vogl_get_proc_address_helper, false);
        #endif

        uint replayer_flags = get_replayer_flags_from_command_line_params();

        // TODO: This will create a window with default attributes, which seems fine for the majority of traces.
        // Unfortunately, some GL call streams *don't* want an alpha channel, or depth, or stencil etc. in the default framebuffer so this may become a problem.
        // Also, this design only supports a single window, which is going to be a problem with multiple window traces.
        if (!window.open(g_command_line_params().get_value_as_int("width", 0, 1024, 1, 65535), g_command_line_params().get_value_as_int("height", 0, 768, 1, 65535), g_command_line_params().get_value_as_int("msaa", 0, 0, 0, 65535)))
        {
            vogl_error_printf("Failed initializing replay window\n");
            return false;
        }

        if (!replayer.init(replayer_flags, &window, pTrace_reader->get_sof_packet(), pTrace_reader->get_multi_blob_manager()))
        {
            vogl_error_printf("Failed initializing GL replayer\n");
            return false;
        }

        // Disable all glGetError() calls in vogl_utils.cpp.
        vogl_disable_gl_get_error();

        // Bool win_mapped = false;

        vogl_gl_state_snapshot *pSnapshot = NULL;
        int64_t snapshot_loop_start_frame = -1;
        int64_t snapshot_loop_end_frame = -1;

        vogl::hash_map<SDL_Keycode> keys_pressed, keys_down;

        int loop_frame = g_command_line_params().get_value_as_int("loop_frame", 0, -1);
        int loop_len = math::maximum<int>(g_command_line_params().get_value_as_int("loop_len", 0, 1), 1);
        int loop_count = math::maximum<int>(g_command_line_params().get_value_as_int("loop_count", 0, cINT32_MAX), 1);
        bool endless_mode = g_command_line_params().get_value_as_bool("endless");

        timer tm;
        tm.start();

        for (;;)
        {
            tmZone(TELEMETRY_LEVEL0, TMZF_NONE, "Main Loop");

            SDL_Event wnd_event;
            
            while (SDL_PollEvent(&wnd_event))
            {
                switch (wnd_event.type)
                {
                    case SDL_KEYDOWN:
                    {
                        keys_down.insert(wnd_event.key.keysym.sym);
                        keys_pressed.insert(wnd_event.key.keysym.sym);
                        break;
                    }

                    case SDL_KEYUP:
                    {
                        keys_down.erase(wnd_event.key.keysym.sym);

                        break;
                    }

                    case SDL_WINDOWEVENT:
                    {
                        switch(wnd_event.window.event)
                        {
                            case SDL_WINDOWEVENT_FOCUS_GAINED:
                            case SDL_WINDOWEVENT_FOCUS_LOST:
                                keys_down.reset();
                                break;

                            case SDL_WINDOWEVENT_CLOSE:
                                vogl_message_printf("Window told to close, exiting.\n");
                                goto normal_exit;
                                break;
                            default:
                                break;
                        };
                        break;
                    }

                    default:
                        // TODO: Handle these somehow?
                        break;
                };
            }

            if (replayer.get_at_frame_boundary())
            {
                if ((!pSnapshot) && (loop_frame != -1) && (static_cast<int64_t>(replayer.get_frame_index()) == loop_frame))
                {
                    vogl_debug_printf("Capturing replayer state at start of frame %u\n", replayer.get_frame_index());

                    pSnapshot = replayer.snapshot_state();

                    if (pSnapshot)
                    {
                        vogl_printf("Snapshot succeeded\n");

                        snapshot_loop_start_frame = pTrace_reader->get_cur_frame();
                        snapshot_loop_end_frame = pTrace_reader->get_cur_frame() + loop_len;

                        vogl_debug_printf("Loop start: %" PRIi64 " Loop end: %" PRIi64 "\n", snapshot_loop_start_frame, snapshot_loop_end_frame);
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
                    status = replayer.process_pending_packets();

                    if (status == vogl_gl_replayer::cStatusOK)
                    {
                        status = replayer.process_next_packet(*pTrace_reader);
                    }

                    if ((status == vogl_gl_replayer::cStatusNextFrame) ||
                        (status == vogl_gl_replayer::cStatusResizeWindow) ||
                        (status == vogl_gl_replayer::cStatusAtEOF) ||
                        (status == vogl_gl_replayer::cStatusHardFailure))
                    {
                        break;
                    }
                }
            }

            if (status == vogl_gl_replayer::cStatusHardFailure)
                break;

            if (status == vogl_gl_replayer::cStatusAtEOF)
            {
                vogl_message_printf("At trace EOF, frame index %u\n", replayer.get_frame_index());
            }

            if (replayer.get_at_frame_boundary() &&
                pSnapshot && 
                (loop_count > 0) &&
                ((pTrace_reader->get_cur_frame() == snapshot_loop_end_frame) || (status == vogl_gl_replayer::cStatusAtEOF)))
            {
                status = replayer.begin_applying_snapshot(pSnapshot, false);
                if ((status != vogl_gl_replayer::cStatusOK) && (status != vogl_gl_replayer::cStatusResizeWindow))
                    goto error_exit;

                pTrace_reader->seek_to_frame(static_cast<uint>(snapshot_loop_start_frame));

                vogl_debug_printf("Applying snapshot and seeking back to frame %" PRIi64 "\n", snapshot_loop_start_frame);
                loop_count--;
            }
            else
            {
                bool print_progress = (status == vogl_gl_replayer::cStatusAtEOF) ||
                    ((replayer.get_at_frame_boundary()) && ((replayer.get_frame_index() % 100) == 0));
                if (print_progress)
                {
                    if (pTrace_reader->get_type() == cBINARY_TRACE_FILE_READER)
                    {
                        vogl_binary_trace_file_reader &binary_trace_reader = *static_cast<vogl_binary_trace_file_reader *>(pTrace_reader.get());

                        vogl_printf("Replay now at frame index %u, trace file offet %" PRIu64 ", GL call counter %" PRIu64 ", %3.2f%% percent complete\n",
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
                        if (replayer.get_flags() & cGLReplayerFSPreprocessor)
                        {
                            double fs_pp_time = replayer.get_fs_pp_time();
                            vogl_printf("Ran with FS Preprocessor (FSPP) enabled:\n");
                            vogl_printf("FSPP Time: %.3f secs\n", fs_pp_time);
                            vogl_printf("Overall Stats:             %u total swaps, %.3f secs, %3.3f avg fps\n", replayer.get_total_swaps(), time_since_start, replayer.get_frame_index() / time_since_start);
                            vogl_printf("Stats Excluding FSPP time: %u total swaps, %.3f secs, %3.3f avg fps\n", replayer.get_total_swaps(), time_since_start-fs_pp_time, replayer.get_frame_index() / (time_since_start-fs_pp_time));
                            break;
                        }
                        else
                        {
                            vogl_printf("%u total swaps, %.3f secs, %3.3f avg fps\n", replayer.get_total_swaps(), time_since_start, replayer.get_frame_index() / time_since_start);
                            break;
                        }
                    }
                    vogl_printf("Resetting state and rewinding back to frame 0\n");

                    replayer.reset_state();

                    if (!pTrace_reader->seek_to_frame(0))
                    {
                        vogl_error_printf("Failed rewinding trace reader!\n");
                        goto error_exit;
                    }
                }
            }

            telemetry_tick();
        }

    normal_exit:
        return true;

    error_exit:
        return false;
    }
#else
    #error "Need to provide tool_replay_mode for this platform."
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

    if (!voglbench_init(argc, argv))
    {
        voglbench_deinit();
        return EXIT_FAILURE;
    }

    if (g_command_line_params().get_count("") < 2)
    {
        vogl_error_printf("No trace file specified!\n");

        tool_print_help();

        voglbench_deinit();
        return EXIT_FAILURE;
    }

    if (g_command_line_params().get_value_as_bool("pause"))
    {
        vogl_message_printf("Press key to continue\n");
        vogl_sleep(1000);
        getchar();
    }

    bool success = tool_replay_mode();

    vogl_printf("%u warning(s), %u error(s)\n",
                    console::get_total_messages(cMsgWarning),
                    console::get_total_messages(cMsgError));

    voglbench_deinit();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
