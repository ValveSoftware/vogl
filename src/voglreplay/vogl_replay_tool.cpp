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

bool tool_dump_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_parse_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_info_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_unpack_json_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_pack_json_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_find_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_compare_hash_files_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_replay_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_play_mode(vogl::vector<command_line_param_desc> *desc);
bool tool_symbols_mode(vogl::vector<command_line_param_desc> *desc);

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
static void *g_actual_libgl_module_handle;
static cfile_stream *g_vogl_pLog_stream;

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
    bool (*func)(vogl::vector<command_line_param_desc> *desc);
    const char *filename_args;
};

static const command_t g_options[] =
{
#define XDEF(_x, _desc, _args) { #_x, _desc, tool_ ## _x ## _mode, _args }
    XDEF(play, "Play trace file", "<input JSON/blob trace filename>"),
    XDEF(replay, "Replay trace file with extended options for trimming, validation, interactive mode, and snapshots", "<input JSON/blob trace filename>"),
    XDEF(info, "Output statistics about a trace file", "<input JSON/blob trace filename>"),
    XDEF(find, "Find all calls with parameters containing a specific value", "<input JSON/blob trace filename>"),
    XDEF(compare_hash_files, "Comparing hash/sum files", "<input hash filename1> <input hash filename2>"),
    XDEF(symbols, "Output trace file symbol information", "<input JSON/blob trace filename>"),
    XDEF(dump, "Dumps binary trace file to a JSON trace file", "<input binary trace filename> <output JSON/blob base filename>"),
    XDEF(parse, "Parse JSON trace file to a binary trace file", "<input JSON/blob trace filename>"),
    XDEF(unpack_json, "Unpack UBJ (Universal Binary JSON) to textual JSON", "<input UBJ filename> <output text filename>"),
    XDEF(pack_json, "Pack textual JSON to UBJ", "<input text filename> <output UBJ filename>"),
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

    cmd->func(&cmdline_desc);

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


#if VOGL_PLATFORM_HAS_X11

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

#if VOGL_PLATFORM_HAS_X11
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
        success = cmd->func(NULL);
    }

    console::printf("%u warning(s), %u error(s)\n", 
                    console::get_total_messages(cWarningConsoleMessage),
                    console::get_total_messages(cErrorConsoleMessage));

    vogl_replay_deinit();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
