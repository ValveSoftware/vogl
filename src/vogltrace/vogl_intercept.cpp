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

// File: vogl_intercept.cpp
#include "vogl_trace.h"
#include "vogl_trace_stream_types.h"
#include "vogl_trace_packet.h"
#include "vogl_texture_format.h"
#include "vogl_gl_state_snapshot.h"
#include "vogl_trace_file_writer.h"
#include "vogl_framebuffer_capturer.h"
#include "vogl_trace_file_reader.h"

// voglcore
#include "vogl_hash_map.h"
#include "vogl_console.h"
#include "vogl_colorized_console.h"
#include "vogl_command_line_params.h"
#include "vogl_cfile_stream.h"
#include "vogl_value.h"
#include "vogl_file_utils.h"
#include "vogl_uuid.h"
#include "vogl_unique_ptr.h"
#include "vogl_port.h"

#if defined(PLATFORM_POSIX)
    #include <unistd.h>
    #include <sys/syscall.h>
#endif

#if defined(VOGL_USE_LINUX_API)
    #include <X11/Xatom.h>
#endif

#ifdef VOGL_REMOTING
#include "vogl_remote.h"
#endif

#if VOGL_PLATFORM_SUPPORTS_BTRACE
    #include "btrace.h"
#endif

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "vogl_miniz.h"

// loki
#include "TypeTraits.h"

#if defined(PLATFORM_OSX)
	#include "gl_types.h"
#endif

#include <turbojpeg.h>

#include "vogl_cmdline_opts.h"

#define VOGL_INTERCEPT_TRACE_FILE_VERSION 0x0102

#define VOGL_STOP_CAPTURE_FILENAME "__stop_capture__"
#define VOGL_TRIGGER_CAPTURE_FILENAME "__trigger_capture__"

#define VOGL_BACKTRACE_HASHMAP_CAPACITY 50000

#define VOGL_LIBGL_SO_FILENAME "libGL.so.1"

#if VOGL_PLATFORM_HAS_GLX
    #define CONTEXT_TYPE GLXContext

#elif VOGL_PLATFORM_HAS_CGL
	#define CONTEXT_TYPE CGLContextObj

#elif VOGL_PLATFORM_HAS_WGL
    #define CONTEXT_TYPE HGLRC
#else
    #error "Need to define CONTEXT_TYPE for this platform"
#endif

#if defined(COMPILER_MSVC)
	// As long as we include windows.h, we will get different dll linkage for this. However, we will "win" in here
	// so we get the results we are looking for. 
	#pragma warning( disable : 4273 )
#endif

// Message to logfile only if logging is set, otherwise to screen.
#define vogl_log_printf(...) vogl::console::printf(VOGL_FUNCTION_INFO_CSTR, cMsgMessage | cMsgFlagLogOnly, __VA_ARGS__)

class vogl_context;

typedef vogl::hash_map<CONTEXT_TYPE, vogl_context *, bit_hasher<CONTEXT_TYPE> > context_map;
bool get_dimensions_from_dc(unsigned int* out_width, unsigned int* out_height, HDC hdc);
typedef struct _vogl_xlib_trap_state vogl_xlib_trap_state_t;

enum VoglBufferCreateMethod
{
    VBCM_BufferData,
    VBCM_BufferStorage
};

//----------------------------------------------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------------------------------------------
bool g_vogl_has_been_initialized = false;
static pthread_once_t g_vogl_init_once_control = PTHREAD_ONCE_INIT;
static pthread_key_t g_vogl_thread_local_data;
static vogl_exception_callback_t g_vogl_pPrev_exception_callback;
static GLuint g_dummy_program;
static vogl_xlib_trap_state_t *g_vogl_xlib_trap_state;

//----------------------------------------------------------------------------------------------------------------------
// Forward declaration
//----------------------------------------------------------------------------------------------------------------------
static void vogl_glInternalTraceCommandRAD(GLuint cmd, GLuint size, const GLubyte *data);
static void vogl_end_capture(bool inside_signal_handler = false);
static void vogl_atexit();
// TODO: Move this declaration into a header that we share with the location the code actually exists.
vogl_void_func_ptr_t vogl_get_proc_address_helper_return_actual(const char *pName);

//----------------------------------------------------------------------------------------------------------------------
// Context sharelist shadow locking
//----------------------------------------------------------------------------------------------------------------------
static bool g_app_uses_sharelists;
static void vogl_context_shadow_lock();
static void vogl_context_shadow_unlock();

class vogl_scoped_context_shadow_lock
{
    bool m_took_lock;

public:
    inline vogl_scoped_context_shadow_lock()
        : m_took_lock(g_app_uses_sharelists)
    {
        if (m_took_lock)
            vogl_context_shadow_lock();
    }

    inline ~vogl_scoped_context_shadow_lock()
    {
        if (m_took_lock)
            vogl_context_shadow_unlock();
    }
};

//----------------------------------------------------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------------------------------------------------
bool g_dump_gl_calls_flag;
bool g_dump_gl_buffers_flag;
bool g_dump_gl_shaders_flag;
bool g_disable_gl_program_binary_flag;
bool g_null_mode;
bool g_backtrace_all_calls;
bool g_backtrace_no_calls;
static bool g_disable_client_side_array_tracing;

static bool g_flush_files_after_each_call;
static bool g_flush_files_after_each_swap;

static uint32_t g_vogl_total_frames_to_capture;
static uint32_t g_vogl_frames_remaining_to_capture;
static bool g_vogl_stop_capturing;

static vogl_capture_status_callback_func_ptr g_vogl_pCapture_status_callback;
static void *g_vogl_pCapture_status_opaque;

static vogl_trace_file_writer& get_vogl_trace_writer()
{
    // If we wind up having issues with destructor ordering, we could changed these
    //  routines to do a "new CObject()" and return that instead of using static.
    //
    // See these links to C++ FAQ Lite for more information:
    //  [10.15] http://www.parashift.com/c++-faq-lite/static-init-order-on-first-use.html 
    //  [10.16] http://www.parashift.com/c++-faq-lite/construct-on-first-use-v2.html 
    static vogl_trace_file_writer s_vogl_trace_writer(&get_vogl_process_gl_ctypes());
    return s_vogl_trace_writer;
}

static mutex &get_vogl_trace_mutex()
{
    static mutex s_vogl_trace_mutex(0, true);
    return s_vogl_trace_mutex;
}

static mutex &get_backtrace_hashmap_mutex()
{
    static mutex s_backtrace_hashmap_mutex(0, false);
    return s_backtrace_hashmap_mutex;
}

struct vogl_intercept_data
{
    dynamic_string capture_path;
    dynamic_string capture_basename;
    #if VOGL_PLATFORM_SUPPORTS_BTRACE
        vogl_backtrace_hashmap backtrace_hashmap;
    #endif
};
static vogl_intercept_data &get_vogl_intercept_data()
{
    static vogl_intercept_data s_data;
    return s_data;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_current_kernel_thread_id
//----------------------------------------------------------------------------------------------------------------------
static inline uint64_t vogl_get_current_kernel_thread_id()
{
    return plat_gettid();
}

//----------------------------------------------------------------------------------------------------------------------
// class vogl_entrypoint_serializer
//----------------------------------------------------------------------------------------------------------------------
class vogl_entrypoint_serializer
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_entrypoint_serializer);

public:
    inline vogl_entrypoint_serializer()
        : m_packet(&get_vogl_process_gl_ctypes()),
          m_in_begin(false)
    {
    }

    inline vogl_entrypoint_serializer(gl_entrypoint_id_t id, vogl_context *pContext)
        : m_packet(&get_vogl_process_gl_ctypes()),
          m_in_begin(false)
    {
        begin(id, pContext);
    }

    const vogl_trace_packet &get_packet() const
    {
        return m_packet;
    }
    vogl_trace_packet &get_packet()
    {
        return m_packet;
    }

    // begin()/end() is NOT nestable
    // Returns false if nesting detected
    bool begin(gl_entrypoint_id_t id, vogl_context *pContext);

    void set_begin_rdtsc(uint64_t val)
    {
        m_packet.set_begin_rdtsc(val);
    }

    bool is_in_begin() const
    {
        return m_in_begin;
    }

    inline gl_entrypoint_id_t get_cur_entrypoint() const
    {
        return m_packet.get_entrypoint_id();
    }

    inline void add_return_param(vogl_ctype_t ctype, const void *pParam, uint32_t param_size)
    {
        VOGL_ASSERT(m_in_begin);
        m_packet.set_return_param(ctype, pParam, param_size);
    }

    inline void add_param(uint8_t param_id, vogl_ctype_t ctype, const void *pParam, uint32_t param_size)
    {
        VOGL_ASSERT(m_in_begin);
        m_packet.set_param(param_id, ctype, pParam, param_size);
    }

    inline void add_ref_client_memory(uint8_t param_id, vogl_ctype_t pointee_ctype, const void *pData, uint64_t data_size)
    {
        VOGL_ASSERT(m_in_begin);
        m_packet.set_ref_client_memory(param_id, pointee_ctype, pData, data_size);
    }

    inline void add_array_client_memory(uint8_t param_id, vogl_ctype_t pointee_ctype, uint64_t array_size, const void *pData, uint64_t data_size)
    {
        VOGL_ASSERT(m_in_begin);
        m_packet.set_array_client_memory(param_id, pointee_ctype, array_size, pData, data_size);
    }

    inline bool add_key_value(const value &key, const value &value)
    {
        VOGL_ASSERT(m_in_begin);
        return m_packet.set_key_value(key, value);
    }

    inline bool add_key_value_blob(const value &key, uint8_vec &blob)
    {
        VOGL_ASSERT(m_in_begin);
        return m_packet.set_key_value(key, blob);
    }

    inline bool add_key_value_blob(const value &key, const void *pData, uint32_t data_size)
    {
        VOGL_ASSERT(m_in_begin);
        return m_packet.set_key_value_blob(key, pData, data_size);
    }

    inline bool add_key_value_json_document(const value &key, const json_document &doc)
    {
        VOGL_ASSERT(m_in_begin);
        return m_packet.set_key_value_json_document(key, doc);
    }

    inline const key_value_map &get_key_value_map() const
    {
        VOGL_ASSERT(m_in_begin);
        return m_packet.get_key_value_map();
    }
    inline key_value_map &get_key_value_map()
    {
        VOGL_ASSERT(m_in_begin);
        return m_packet.get_key_value_map();
    }

    inline void set_gl_begin_rdtsc(uint64_t val)
    {
        m_packet.set_gl_begin_rdtsc(val);
    }
    inline void set_gl_end_rdtsc(uint64_t val)
    {
        m_packet.set_gl_end_rdtsc(val);
    }
    inline void set_gl_begin_end_rdtsc(uint64_t begin, uint64_t end)
    {
        m_packet.set_gl_begin_rdtsc(begin);
        m_packet.set_gl_end_rdtsc(end);
    }

    inline void end()
    {
        VOGL_ASSERT(m_in_begin);

        if (!m_in_begin)
        {
            vogl_error_printf("end() called without a matching call to begin()! Something is seriously wrong!\n");
            return;
        }

        m_packet.end_construction(utils::RDTSC());

        VOGL_ASSERT(m_packet.check());

        const gl_entrypoint_desc_t &entrypoint_desc = g_vogl_entrypoint_descs[get_cur_entrypoint()];

        if (m_packet.total_params() != entrypoint_desc.m_num_params)
        {
            vogl_error_printf("Unexpected number of params passed to serializer! (entrypoint id %u)\n", get_cur_entrypoint());
            m_in_begin = false;
            return;
        }

        if (m_packet.has_return_value() != (entrypoint_desc.m_return_ctype != VOGL_VOID))
        {
            vogl_error_printf("Return parameter serialization error (entrypoint id %u)!\n", get_cur_entrypoint());
            m_in_begin = false;
            return;
        }

        m_in_begin = false;
    }

private:
    vogl_trace_packet m_packet;
    bool m_in_begin;
};

//----------------------------------------------------------------------------------------------------------------------
// struct vogl_thread_local_data
//----------------------------------------------------------------------------------------------------------------------
class vogl_thread_local_data
{
public:
    vogl_thread_local_data()
        : m_pContext(NULL),
          m_calling_driver_entrypoint_id(VOGL_ENTRYPOINT_INVALID)
    {
    }

    ~vogl_thread_local_data()
    {
        m_pContext = NULL;
    }

    vogl_context *m_pContext;
    vogl_entrypoint_serializer m_serializer;

    // Set to a valid entrypoint ID if we're currently trying to call the driver on this thread. The "direct" GL function wrappers (in
    // vogl_entrypoints.cpp) call our vogl_direct_gl_func_prolog/epilog func callbacks below, which manipulate this member.
    gl_entrypoint_id_t m_calling_driver_entrypoint_id;
};

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_thread_local_data
//----------------------------------------------------------------------------------------------------------------------
static VOGL_FORCE_INLINE vogl_thread_local_data *vogl_get_thread_local_data()
{
    return static_cast<vogl_thread_local_data *>(pthread_getspecific(g_vogl_thread_local_data));
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_or_create_thread_local_data
//----------------------------------------------------------------------------------------------------------------------
static VOGL_FORCE_INLINE vogl_thread_local_data *vogl_get_or_create_thread_local_data()
{
    vogl_thread_local_data *pTLS_data = vogl_get_thread_local_data();
    if (!pTLS_data)
    {
        pTLS_data = vogl_new(vogl_thread_local_data);

        pthread_setspecific(g_vogl_thread_local_data, pTLS_data);
    }

    return pTLS_data;
}

//----------------------------------------------------------------------------------------------------------------------
// tls_thread_local_data_destructor
//----------------------------------------------------------------------------------------------------------------------
static void vogl_thread_local_data_destructor(void *pValue)
{
    vogl_delete(static_cast<vogl_thread_local_data *>(pValue));

    pthread_setspecific(g_vogl_thread_local_data, NULL);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_init_thread_local_data
//----------------------------------------------------------------------------------------------------------------------
static void vogl_init_thread_local_data()
{
    int rc = pthread_key_create(&g_vogl_thread_local_data, vogl_thread_local_data_destructor);
    if (rc)
    {
        vogl_error_printf("pthread_key_create failed!\n");
        exit(EXIT_FAILURE);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_capture_on_next_swap
//----------------------------------------------------------------------------------------------------------------------
bool vogl_capture_on_next_swap(uint32_t total_frames, const char *pPath, const char *pBase_filename, vogl_capture_status_callback_func_ptr pStatus_callback, void *pStatus_callback_opaque)
{
    if (!total_frames)
    {
        vogl_error_printf("total_frames cannot be 0\n");
        return false;
    }

    scoped_mutex lock(get_vogl_trace_mutex());

    if ((!g_vogl_frames_remaining_to_capture) && (!get_vogl_trace_writer().is_opened()))
    {
        g_vogl_total_frames_to_capture = total_frames;
        get_vogl_intercept_data().capture_path = pPath ? pPath : "";
        get_vogl_intercept_data().capture_basename = pBase_filename ? pBase_filename : "";
        g_vogl_pCapture_status_callback = pStatus_callback;
        g_vogl_pCapture_status_opaque = pStatus_callback_opaque;
        g_vogl_stop_capturing = false;

        vogl_debug_printf("Total frames: %u, path: \"%s\", base filename: \"%s\", status callback: %p, status callback opaque: %p\n",
                         total_frames, pPath, pBase_filename, pStatus_callback, pStatus_callback_opaque);
    }
    else
    {
        vogl_error_printf("Cannot trigger capturing while a trace is currently in progress\n");
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_stop_capturing
//----------------------------------------------------------------------------------------------------------------------
bool vogl_stop_capturing()
{
    scoped_mutex lock(get_vogl_trace_mutex());

    if (!get_vogl_trace_writer().is_opened())
    {
        vogl_error_printf("Tracing is not active!\n");
        return false;
    }

    g_vogl_stop_capturing = true;

    vogl_debug_printf("Closing trace immediately after next swap\n");

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_stop_capturing
//----------------------------------------------------------------------------------------------------------------------
bool vogl_stop_capturing(vogl_capture_status_callback_func_ptr pStatus_callback, void *pStatus_callback_opaque)
{
    scoped_mutex lock(get_vogl_trace_mutex());

    if (!get_vogl_trace_writer().is_opened())
    {
        vogl_error_printf("Tracing is not active!\n");
        return false;
    }

    g_vogl_stop_capturing = true;
    g_vogl_pCapture_status_callback = pStatus_callback;
    g_vogl_pCapture_status_opaque = pStatus_callback_opaque;

    vogl_debug_printf("Closing trace immediately after next swap, status callback: %p, status callback opaque: %p\n",
                     pStatus_callback, pStatus_callback_opaque);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_capturing
//----------------------------------------------------------------------------------------------------------------------
bool vogl_is_capturing()
{
    scoped_mutex lock(get_vogl_trace_mutex());

    return get_vogl_trace_writer().is_opened();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_dump_statistics
//----------------------------------------------------------------------------------------------------------------------
static void vogl_dump_statistics()
{
    for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; i++)
    {
        if ((!g_vogl_entrypoint_descs[i].m_is_whitelisted) && (g_vogl_entrypoint_descs[i].m_trace_call_counter))
        {
            vogl_error_printf("A non-whitelisted function was called during tracing: %s\n", g_vogl_entrypoint_descs[i].m_pName);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_console_init
//----------------------------------------------------------------------------------------------------------------------
bool vogl_console_init(bool doinit)
{
    VOGL_FUNC_TRACER

    static bool s_inited = false;
    if (s_inited)
        return true;
    if (!doinit)
        return false;
    s_inited = true;

    // Initialize vogl_core.
    vogl_core_init();

    colorized_console::init();
    console::set_tool_prefix("(vogltrace) ");

#if 0
    // Enable to see file and line information in console output.
    console::set_caller_info_always(true);
#endif

    dynamic_string_array cmd_line_params;
    char *pEnv_cmd_line = getenv("VOGL_CMD_LINE");
    if (pEnv_cmd_line)
    {
        dynamic_string cmd_line(pEnv_cmd_line);
        cmd_line.unquote();

        if (!split_command_line_params(cmd_line.get_ptr(), cmd_line_params))
        {
            vogl_error_printf("Failed splitting command line params from env var: %s\n", pEnv_cmd_line);
            VOGL_ASSERT(false);
        }
    }
    else
    {
        cmd_line_params = get_command_line_params();
    }

    command_line_params::parse_config parse_cfg;
    parse_cfg.m_single_minus_params = true;
    parse_cfg.m_double_minus_params = true;
    parse_cfg.m_ignore_non_params = pEnv_cmd_line ? false : true;
    parse_cfg.m_ignore_unrecognized_params = pEnv_cmd_line ? false : true;
    parse_cfg.m_pParam_accept_prefix = "vogl_";

    if (!g_command_line_params().parse(cmd_line_params, VOGL_ARRAY_SIZE(g_tracer_cmdline_opts), g_tracer_cmdline_opts, parse_cfg))
    {
        vogl_error_printf("Failed parsing command line parameters\n");
        VOGL_ASSERT(false);
    }

    dynamic_string filename(g_command_line_params().get_value_as_string_or_empty("vogl_logfile"));
    if (!filename.is_empty())
    {
        // Create a logfile with PID appended to the log filename.
        bool ret = console::set_log_stream_filename(filename.c_str(), true);
        if (!ret)
        {
            vogl_error_printf("Failed opening log file \"%s\"\n", filename.get_ptr());
            VOGL_ASSERT(false);
        }
    }

   
    char exec_fname[VOGL_MAX_PATH];
	file_utils::get_exec_filename(exec_fname, sizeof(exec_fname));
    exec_fname[sizeof(exec_fname) - 1] = 0;

    vogl_message_printf("exec_filename: '%s'\n", exec_fname);
    if (pEnv_cmd_line)
        vogl_message_printf("VOGL_CMD_LINE: %s\n", pEnv_cmd_line);
    vogl_message_printf("Command line params: \"%s\"\n", get_command_line().get_ptr());

    if (g_command_line_params().get_value_as_bool("vogl_quiet"))
        console::set_output_level(cMsgError);
    else if (g_command_line_params().get_value_as_bool("vogl_debug"))
        console::set_output_level(cMsgDebug);
    else if (g_command_line_params().get_value_as_bool("vogl_verbose"))
        console::set_output_level(cMsgVerbose);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// Initialization
//----------------------------------------------------------------------------------------------------------------------
void vogl_init()
{
    // Cannot telemetry this function, too early.
    g_vogl_initializing_flag = true;

    // Init vogl console.
    vogl_console_init(true);

#if VOGL_PLATFORM_SUPPORTS_BTRACE
    vogl_message_printf("%s built %s %s, begin initialization in %s\n", btrace_get_current_module(), __DATE__, __TIME__, getenv("_"));
#endif

    // We can't use the regular cmd line parser here because this func is called before global objects are constructed.
    bool pause = g_command_line_params().get_value_as_bool("vogl_pause");
    if (pause)
    {
        int count = 60000;
        bool debugger_connected = false;

        vogl_message_printf("\nPausing %d ms or until debugger is attached (pid %d).\n", count, plat_getpid());
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

    #if (VOGL_PLATFORM_HAS_GLX)
        g_vogl_actual_gl_entrypoints.m_glXGetProcAddress = reinterpret_cast<glXGetProcAddress_func_ptr_t>(dlsym(RTLD_NEXT, "glXGetProcAddress"));
        if (!g_vogl_actual_gl_entrypoints.m_glXGetProcAddress)
        {
            vogl_warning_printf("dlsym(RTLD_NEXT, \"glXGetProcAddress\") failed, trying to manually load %s\n", VOGL_LIBGL_SO_FILENAME);

            g_vogl_actual_libgl_module_handle = dlopen(VOGL_LIBGL_SO_FILENAME, RTLD_LAZY);
            if (!g_vogl_actual_libgl_module_handle)
            {
                vogl_error_printf("Failed loading %s!\n", VOGL_LIBGL_SO_FILENAME);
                exit(EXIT_FAILURE);
            }

            g_vogl_actual_gl_entrypoints.m_glXGetProcAddress = reinterpret_cast<glXGetProcAddress_func_ptr_t>(dlsym(g_vogl_actual_libgl_module_handle, "glXGetProcAddress"));
            if (!g_vogl_actual_gl_entrypoints.m_glXGetProcAddress)
            {
                vogl_error_printf("Failed getting address of glXGetProcAddress() from %s!\n", VOGL_LIBGL_SO_FILENAME);
                exit(EXIT_FAILURE);
            }

            vogl_message_printf("Manually loaded %s\n", VOGL_LIBGL_SO_FILENAME);
        }

	#elif (VOGL_PLATFORM_HAS_CGL)
		VOGL_ASSERT(!"UNIMPLEMENTED vogl_init");
	
    #elif (VOGL_PLATFORM_HAS_WGL)
        g_vogl_actual_gl_entrypoints.m_wglGetProcAddress = reinterpret_cast<wglGetProcAddress_func_ptr_t>(plat_dlsym(PLAT_RTLD_NEXT, "wglGetProcAddress"));
        if (!g_vogl_actual_gl_entrypoints.m_wglGetProcAddress)
        {
            // There's no fallback on windows, the underlying code already forcibly loaded the system dll--so if we couldn't find it we need to understand why.
            vogl_error_printf("Failed getting address of wglGetProcAddress from opengl32.dll\n");
            exit(EXIT_FAILURE);
        }

    #else
        #error "impl vogl_init on this platform."
    #endif

    vogl_common_lib_early_init();

    vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper_return_actual);

    vogl_early_init();

    vogl_verbose_printf("end initialization\n");

    g_vogl_initializing_flag = false;
}

//----------------------------------------------------------------------------------------------------------------------
// Deinitialization
//----------------------------------------------------------------------------------------------------------------------
static void vogl_deinit_callback()
{
    VOGL_FUNC_TRACER // I'm not sure this is a good idea here.

    vogl_end_capture();
    vogl_dump_statistics();
}

void vogl_deinit()
{
    VOGL_FUNC_TRACER // I'm not sure this is a good idea here.

    static pthread_once_t s_vogl_deinit_once_control = PTHREAD_ONCE_INIT;
    pthread_once(&s_vogl_deinit_once_control, vogl_deinit_callback);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_exception_callback
// Note this function can be called ASYNCHRONOUSLY at signal time. Don't call any API function which is not async safe!
// TODO: Fix this function!
//----------------------------------------------------------------------------------------------------------------------
static void vogl_exception_callback()
{
    VOGL_FUNC_TRACER

    fprintf(stderr, "(vogltrace) Flushing log and closing trace files. Note any outstanding async buffer readbacks (for screen capturing) cannot be safely flushed!\n");

    vogl_end_capture(true);

    //uint32_t num_contexts = get_context_manager().get_context_map().size();
    //if (num_contexts)
    //   vogl_error_printf("App is exiting with %u active GL context(s)! Any outstanding async buffer readbacks cannot be safely flushed!\n", num_contexts);

    if (console::get_log_stream())
        console::get_log_stream()->flush();

    if (g_vogl_pPrev_exception_callback)
    {
        fprintf(stderr, "Calling prev handler\n");
        (*g_vogl_pPrev_exception_callback)();
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_init_command_line_params
//----------------------------------------------------------------------------------------------------------------------
static void vogl_init_command_line_params()
{
    VOGL_FUNC_TRACER

    float sleep_time = g_command_line_params().get_value_as_float("vogl_sleep_at_startup");
    if (sleep_time > 0.0f)
    {
        vogl_sleep(static_cast<uint32_t>(ceil(1000.0f * sleep_time)));
    }

    g_dump_gl_calls_flag = g_command_line_params().get_value_as_bool("vogl_dump_gl_calls");
    g_dump_gl_buffers_flag = g_command_line_params().get_value_as_bool("vogl_dump_gl_buffers");
    g_dump_gl_shaders_flag = g_command_line_params().get_value_as_bool("vogl_dump_gl_shaders");
    g_disable_gl_program_binary_flag = g_command_line_params().get_value_as_bool("vogl_disable_gl_program_binary");
    g_flush_files_after_each_call = g_command_line_params().get_value_as_bool("vogl_flush_files_after_each_call");
    g_flush_files_after_each_swap = g_command_line_params().get_value_as_bool("vogl_flush_files_after_each_swap");

    g_null_mode = g_command_line_params().get_value_as_bool("vogl_null_mode");
    g_backtrace_all_calls = g_command_line_params().get_value_as_bool("vogl_backtrace_all_calls");
    g_backtrace_no_calls = g_command_line_params().get_value_as_bool("vogl_backtrace_no_calls");
    g_disable_client_side_array_tracing = g_command_line_params().get_value_as_bool("vogl_disable_client_side_array_tracing");

    if (g_command_line_params().get_value_as_bool("vogl_dump_gl_full"))
    {
        g_dump_gl_calls_flag = true;
        g_dump_gl_buffers_flag = true;
        g_dump_gl_shaders_flag = true;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_check_for_threaded_driver_optimizations
//----------------------------------------------------------------------------------------------------------------------
static bool vogl_check_for_threaded_driver_optimizations()
{
    VOGL_FUNC_TRACER

    const char *pThreaded_optimizations = getenv("__GL_THREADED_OPTIMIZATIONS");
    if (!pThreaded_optimizations)
        return false;

    if (string_to_int(pThreaded_optimizations) != 0)
    {
        vogl_error_printf("-----------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        vogl_error_printf("__GL_THREADED_OPTIMIZATIONS is defined and non-zero -- it is HIGHLY recommended you remove this environment variable or tracing will be very slow!\n");
        vogl_error_printf("-----------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_global_init - called once on the first intercepted GL/GLX function call
//
// Guaranteed to be called once only on the first GLX func call.  You can do more or less whatever you
// want here (this is after the low-level init, way past global constructor time, and probably during
// app init).  This is where cmd lines are processed, remoting is initialized, the log/trace files are
// opened, etc.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_global_init()
{
#if VOGL_FUNCTION_TRACING
    fflush(stdout);
    fflush(stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif

    VOGL_FUNC_TRACER

    bool reliable_rdtsc = vogl::utils::init_rdtsc();
    if (reliable_rdtsc)
        vogl_verbose_printf("Reliable tsc clocksource found. Using rdtsc.\n");
    else
        vogl_warning_printf("Unreliable tsc clocksource found. Not using rdtsc.\n");

    vogl_init_command_line_params();

    vogl_common_lib_global_init();

    dynamic_string backbuffer_hash_file;
    if (g_command_line_params().get_value_as_string(backbuffer_hash_file, "vogl_dump_backbuffer_hashes"))
    {
        remove(backbuffer_hash_file.get_ptr());
        vogl_message_printf("Deleted backbuffer hash file \"%s\"\n", backbuffer_hash_file.get_ptr());
    }

    if (g_command_line_params().has_key("vogl_tracefile"))
    {
        if (!get_vogl_trace_writer().open(g_command_line_params().get_value_as_string_or_empty("vogl_tracefile").get_ptr()))
        {
            // FIXME: What do we do? The caller WANTS a full-stream trace, and continuing execution is probably not desired.

            vogl_error_printf("Failed opening trace file, exiting application!\n");

            exit(EXIT_FAILURE);
        }
    }

    if (!g_command_line_params().get_value_as_bool("vogl_disable_signal_interception"))
    {
        vogl_verbose_printf("Installing exception/signal callbacks\n");

        colorized_console::set_exception_callback();

        g_vogl_pPrev_exception_callback = vogl_set_exception_callback(vogl_exception_callback);
    }

#ifdef VOGL_REMOTING
    vogl_message_printf("vogl_traceport = %d\n", g_command_line_params().get_value_as_int("vogl_traceport"));
    vogl_init_listener(g_command_line_params().get_value_as_int("vogl_traceport"));
#endif

    vogl_check_for_threaded_driver_optimizations();

    #if VOGL_PLATFORM_SUPPORTS_BTRACE
        get_vogl_intercept_data().backtrace_hashmap.reserve(VOGL_BACKTRACE_HASHMAP_CAPACITY);
    #endif

    // atexit routines are called in the reverse order in which they were registered. We would like
	//  our vogl_atexit() routine to be called before anything else (Ie C++ destructors, etc.) So we
	//  put atexit at the end of vogl_global_init() and another at the end of glXMakeCurrent.
    atexit(vogl_atexit);

    vogl_verbose_printf("vogl_global_init finished\n");

    g_vogl_has_been_initialized = true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_check_context
//----------------------------------------------------------------------------------------------------------------------
static inline void vogl_check_context(gl_entrypoint_id_t id, vogl_context *pContext)
{
    if (!pContext)
    {
        const char *pName = g_vogl_entrypoint_descs[id].m_pName;

        // FIXME: Check to see if the func is in a non-GL category, anything but this!
        // These are rare enough that this is not the biggest fire right now.
        if (   ((pName[0] != 'g') || (pName[1] != 'l') || (pName[2] != 'X'))
            && ((pName[0] != 'w') || (pName[1] != 'g') || (pName[2] != 'l')))
        {
            uint32_t n = vogl_strlen(pName);
            if ((n <= 3) || ((pName[n - 3] != 'R') || (pName[n - 2] != 'A') || (pName[n - 1] != 'D')))
            {
                vogl_error_printf("OpenGL function \"%s\" called without an active context!\n", pName);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_entrypoint_prolog
// This function gets called on every GL call - be careful what you do here!
//----------------------------------------------------------------------------------------------------------------------
static inline vogl_thread_local_data *vogl_entrypoint_prolog(gl_entrypoint_id_t entrypoint_id)
{
    pthread_once(&g_vogl_init_once_control, vogl_global_init);

    vogl_thread_local_data *pTLS_data = vogl_get_or_create_thread_local_data();

    // Something very odd is going on - the driver somehow called itself while WE where calling it, so let's immediately bail for safety.
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        return pTLS_data;

    // Atomically inc the 64-bit counter
    // TODO: Make a helper for this
    int64_t cur_ctr;
    do
    {
        cur_ctr = g_vogl_entrypoint_descs[entrypoint_id].m_trace_call_counter;
    } while (atomic_compare_exchange64(&g_vogl_entrypoint_descs[entrypoint_id].m_trace_call_counter, cur_ctr + 1, cur_ctr) != cur_ctr);

    if ((!cur_ctr) && (!g_vogl_entrypoint_descs[entrypoint_id].m_is_whitelisted))
        vogl_error_printf("Function \"%s\" not yet in function whitelist, this API will not be replayed and this trace will not be replayable!\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);

    vogl_check_context(entrypoint_id, pTLS_data->m_pContext);

    return pTLS_data;
}

//----------------------------------------------------------------------------------------------------------------------
// struct gl_vertex_attribute
//----------------------------------------------------------------------------------------------------------------------
struct gl_vertex_attribute
{
    GLint m_size;
    GLenum m_type;
    GLboolean m_normalized;
    GLsizei m_stride;
    const GLvoid *m_pointer;
    GLuint m_buffer;
    bool m_enabled;

    void clear()
    {
        m_size = 4;
        m_type = GL_FLOAT;
        m_normalized = false;
        m_stride = 0;
        m_pointer = NULL;
        m_buffer = 0;
        m_enabled = 0;
    }
};
const uint32_t VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES = 32;

typedef vogl::hash_map<GLenum, GLuint> gl_buffer_binding_map;
//----------------------------------------------------------------------------------------------------------------------
// struct gl_buffer_desc
//----------------------------------------------------------------------------------------------------------------------
struct gl_buffer_desc
{
    VoglBufferCreateMethod m_create_method;

    GLuint m_handle;
    int64_t m_size;
    GLenum m_usage;     // Only one of m_usage or m_flags is non-zero, it depends on how the buffer was created.
    GLbitfield m_flags;

    void *m_pMap;
    int64_t m_map_ofs;
    int64_t m_map_size;

    // FIXME: Don't alias this!
    // NOTE: This is a GLbitfield of m_map_range is true, otherwise it's a GL_ENUM!!
    GLbitfield m_map_access;

    bool m_map_range;

    struct flushed_range
    {
        inline flushed_range(int64_t ofs, int64_t size)
            : m_ofs(ofs), m_size(size)
        {
        }
        inline flushed_range()
        {
        }

        int64_t m_ofs;
        int64_t m_size;
    };
    vogl::vector<flushed_range> m_flushed_ranges;
    inline gl_buffer_desc()
    {
        clear();
    }
    inline gl_buffer_desc(GLuint handle)
    {
        clear();
        m_handle = handle;
    }

    inline void clear()
    {
        m_handle = 0;
        m_size = 0;
        m_usage = 0;
        m_pMap = NULL;
        m_map_ofs = 0;
        m_map_size = 0;
        m_map_access = 0;
        m_map_range = false;
        m_flushed_ranges.clear();
    }
};

typedef vogl::hash_map<GLuint, gl_buffer_desc> gl_buffer_desc_map;

//----------------------------------------------------------------------------------------------------------------------
// vogl_scoped_gl_error_absorber
//----------------------------------------------------------------------------------------------------------------------
class vogl_scoped_gl_error_absorber
{
    vogl_context *m_pContext;

public:
    vogl_scoped_gl_error_absorber(vogl_context *pContext);
    ~vogl_scoped_gl_error_absorber();
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_tracer_snapshot_handle_remapper
//----------------------------------------------------------------------------------------------------------------------
class vogl_context_handle_remapper : public vogl_handle_remapper
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_context_handle_remapper);

    vogl_context *m_pContext;

public:
    vogl_context_handle_remapper(vogl_context *pContext)
        : m_pContext(pContext)
    {
    }

    virtual bool is_default_remapper() const
    {
        return false;
    }

    virtual bool is_valid_handle(vogl_namespace_t handle_namespace, uint64_t from_handle);

    virtual bool determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t from_handle, GLenum &target);

    virtual bool determine_to_object_target(vogl_namespace_t handle_namespace, uint64_t to_handle, GLenum &target)
    {
        return determine_from_object_target(handle_namespace, to_handle, target);
    }
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_context
//----------------------------------------------------------------------------------------------------------------------
class vogl_context
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_context);

public:
    vogl_context(CONTEXT_TYPE handle)
        : m_ref_count(1),
          m_deleted_flag(false),
          m_pShared_state(this),
          m_context_handle(handle),
          m_sharelist_handle(NULL),
          m_pDpy(NULL),
          m_drawable(None),
          m_read_drawable(None),
          m_render_type(0),
          m_fb_config(NULL),
          m_hdc(NULL),
          m_direct(false),
          m_created_from_attribs(false),
          m_current_thread(0),
          m_max_vertex_attribs(0),
          m_has_been_made_current(false),
          m_window_width(-1),
          m_window_height(-1),
          m_frame_index(0),
          m_creation_func(VOGL_ENTRYPOINT_INVALID),
          m_latched_gl_error(GL_NO_ERROR),
          m_cur_program(0),
          m_in_gl_begin(false),
          m_uses_client_side_arrays(false),
          m_handle_remapper(this),
          m_current_display_list_handle(-1),
          m_current_display_list_mode(GL_NONE)
    {
        utils::zero_object(m_xvisual_info);
    }

    ~vogl_context()
    {
    }

    int get_ref_count() const
    {
        return m_ref_count;
    }
    int add_ref()
    {
        ++m_ref_count;
        return m_ref_count;
    }
    int del_ref()
    {
        --m_ref_count;
        return m_ref_count;
    }

    bool get_deleted_flag() const
    {
        return m_deleted_flag;
    }
    void set_deleted_flag(bool val)
    {
        m_deleted_flag = val;
    }

    vogl_context *get_shared_state() const
    {
        return m_pShared_state;
    }
    void set_shared_context(vogl_context *pContext)
    {
        m_pShared_state = pContext;
    }

    vogl_context *get_context_state()
    {
        return this;
    }

    bool is_root_context() const
    {
        return this == m_pShared_state;
    }
    bool is_share_context() const
    {
        return this != m_pShared_state;
    }

    CONTEXT_TYPE get_context_handle() const
    {
        return m_context_handle;
    }

    CONTEXT_TYPE get_sharelist_handle() const
    {
        return m_sharelist_handle;
    }
    void set_sharelist_handle(CONTEXT_TYPE context)
    {
        m_sharelist_handle = context;
    }

    const Display *get_display() const
    {
        return m_pDpy;
    }
    void set_display(const Display *pDpy)
    {
        m_pDpy = pDpy;
    }

    const HDC get_hdc() const
    {
        return m_hdc;
    }
    void set_hdc(HDC hdc)
    {
        m_hdc = hdc;
    }


    void set_drawable(GLXDrawable drawable)
    {
        m_drawable = drawable;
    }
    GLXDrawable get_drawable() const
    {
        return m_drawable;
    }

    void set_read_drawable(GLXDrawable read)
    {
        m_read_drawable = read;
    }
    GLXDrawable get_read_drawable() const
    {
        return m_read_drawable;
    }

    void set_render_type(int render_type)
    {
        m_render_type = render_type;
    }
    int get_render_type() const
    {
        return m_render_type;
    }

    bool get_direct() const
    {
        return m_direct;
    }
    void set_direct(bool direct)
    {
        m_direct = direct;
    }

    const XVisualInfo &get_xvisual_info() const
    {
        return m_xvisual_info;
    }
    void set_xvisual_info(const XVisualInfo *p)
    {
        m_xvisual_info = *p;
    }

    bool get_created_from_attribs() const
    {
        return m_created_from_attribs;
    }
    void set_created_from_attribs(bool created_from_attribs)
    {
        m_created_from_attribs = created_from_attribs;
    }

    uint64_t get_current_thread() const
    {
        return m_current_thread;
    }
    void set_current_thread(uint64_t tid)
    {
        m_current_thread = tid;
    }

    void set_creation_func(gl_entrypoint_id_t func)
    {
        m_creation_func = func;
    }
    gl_entrypoint_id_t get_creation_func() const
    {
        return m_creation_func;
    }

    void set_fb_config(GLXFBConfig config)
    {
        m_fb_config = config;
    }
    GLXFBConfig get_fb_config()
    {
        return m_fb_config;
    }

    inline bool get_has_been_made_current() const
    {
        return m_has_been_made_current;
    }

    void init()
    {
        VOGL_ASSERT(m_creation_func != VOGL_ENTRYPOINT_INVALID);
        if (m_creation_func == VOGL_ENTRYPOINT_INVALID)
            return;

        vogl_context_attribs attribs(m_attrib_list.get_ptr(), m_attrib_list.size());
        m_context_desc.init(m_creation_func, m_direct, (vogl_trace_ptr_value)m_context_handle, (vogl_trace_ptr_value)m_sharelist_handle, attribs);
    }

    void set_attrib_list(const int *attrib_list)
    {
        int n = vogl_determine_attrib_list_array_size(attrib_list);
        m_attrib_list.clear();
        m_attrib_list.append(attrib_list, n);
    }

    const vogl::vector<int> &get_attrib_list() const
    {
        return m_attrib_list;
    }

    // true if a core profile was explictly requested in the attrib list.
    inline bool requested_core_profile() const
    {
        return (m_context_desc.get_attribs().get_value_or_default(GLX_CONTEXT_PROFILE_MASK_ARB) & GLX_CONTEXT_CORE_PROFILE_BIT_ARB) != 0;
    }

    // true if a compat profile was explictly requested in the attrib list.
    inline bool requested_compatibility_profile() const
    {
        return (m_context_desc.get_attribs().get_value_or_default(GLX_CONTEXT_PROFILE_MASK_ARB) & GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB) != 0;
    }

    // true if a debug context was explictly requested in the attrib list.
    inline bool requested_debug_context() const
    {
        return (m_context_desc.get_attribs().get_value_or_default(GLX_CONTEXT_FLAGS_ARB) & GLX_CONTEXT_DEBUG_BIT_ARB) != 0;
    }

    // true if a forward compatible context was explictly requested in the attrib list.
    inline bool requested_forward_compatible() const
    {
        return (m_context_desc.get_attribs().get_value_or_default(GLX_CONTEXT_FLAGS_ARB) & GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB) != 0;
    }

    // true if this is a core profile context (the context must have been made current first)
    inline bool is_core_profile() const
    {
        return m_context_info.is_core_profile();
    }

    // only valid after the first MakeCurrent
    inline GLuint get_max_vertex_attribs() const
    {
        return m_max_vertex_attribs;
    }
    inline GLuint get_max_texture_coords() const
    {
        return m_context_info.get_max_texture_coords();
    }

    // compat only - will be 0 in core
    inline GLuint get_max_texture_units() const
    {
        return m_context_info.get_max_texture_units();
    }

    const vogl_context_desc &get_context_desc() const
    {
        return m_context_desc;
    }
    const vogl_context_info &get_context_info() const
    {
        return m_context_info;
    }

    const vogl_capture_context_params &get_capture_context_params() const
    {
        return m_capture_context_params;
    }
    vogl_capture_context_params &get_capture_context_params()
    {
        return m_capture_context_params;
    }

    const vogl_framebuffer_capturer &get_framebuffer_capturer() const
    {
        return m_framebuffer_capturer;
    }
    vogl_framebuffer_capturer &get_framebuffer_capturer()
    {
        return m_framebuffer_capturer;
    }

    inline void on_make_current()
    {
        set_current_thread(vogl_get_current_kernel_thread_id());

        if (!m_has_been_made_current)
        {
            vogl_scoped_gl_error_absorber gl_error_absorber(this);
            VOGL_NOTE_UNUSED(gl_error_absorber);

            m_max_vertex_attribs = vogl_get_gl_integer(GL_MAX_VERTEX_ATTRIBS);
            if (m_max_vertex_attribs > VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES)
            {
                vogl_error_printf("GL_MAX_VERTEX_ATTRIBS is larger than supported (%u, max supported is %u), trace may not contain all client side vertex attribs\n", m_max_vertex_attribs, VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES);
                m_max_vertex_attribs = VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES;
            }

            if (!m_context_info.init(m_context_desc))
            {
                vogl_error_printf("Failed initializing m_context_info!\n");
            }

            if (!m_has_been_made_current)
            {
                on_first_make_current();
            }

            m_has_been_made_current = true;
        }
    }

    bool is_context_current()
    {
        if (!GL_ENTRYPOINT(glXGetCurrentContext))
            return false;

        // Double check that the context that we think is current is actually current.
        CONTEXT_TYPE cur_context = GetCurrentContext(); 
        if (!cur_context)
        {
            VOGL_ASSERT(!m_current_thread);
            return false;
        }

        if (get_context_handle() != cur_context)
        {
            VOGL_ASSERT(!m_current_thread);
            return false;
        }

        VOGL_ASSERT(vogl_get_current_kernel_thread_id() == m_current_thread);

        return true;
    }

    // This will be called with the context made current, however the TLS struct won't indicate that this context is current, and m_current_thread will not reflect this either!
    // Note this will be called before glXMakeCurrent() when contexts are switching, and it's possible the call could fail (and this context could remain current). So don't do anything drastic here.
    void on_release_current_prolog()
    {
        if (!is_context_current())
            return;

        peek_and_record_gl_error();

        m_framebuffer_capturer.flush();

        peek_and_drop_gl_error();
    }

    void on_release_current_epilog()
    {
        set_current_thread(0);
    }

    void on_destroy_prolog()
    {
        if (!is_context_current())
        {
            m_framebuffer_capturer.deinit(false);
            return;
        }

        peek_and_record_gl_error();

        m_framebuffer_capturer.deinit(true);

        peek_and_drop_gl_error();
    }

    // buffer handle and target shadowing
    void gen_buffers(GLsizei n, GLuint *pIDs)
    {
        if (!pIDs)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint id = pIDs[i];
            if (id)
            {
                if (!get_shared_state()->m_capture_context_params.m_buffer_targets.insert(id, GL_NONE).second)
                {
                    vogl_error_printf("Can't insert buffer handle 0x%04X into buffer target map!\n", id);
                }
            }
        }
    }

    void bind_buffer(GLenum target, GLuint id)
    {
        if (!id)
            return;

        vogl_scoped_context_shadow_lock lock;

        GLenum *pTarget = get_shared_state()->m_capture_context_params.m_buffer_targets.find_value(id);
        if (!pTarget)
        {
            vogl_error_printf("Unable to find buffer handle 0x%04X in buffer target map!\n", id);
            return;
        }

        if (*pTarget == GL_NONE)
        {
            // This is the first bind, so record the target. Otherwise they are rebinding to a different target, which shouldn't matter to us for snapshotting purposes (right??).
            *pTarget = target;
        }
    }

    void delete_buffers(GLsizei n, const GLuint *buffers)
    {
        if ((!n) || (!buffers))
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint buffer = buffers[i];
            if (!buffer)
                continue;

            get_shared_state()->m_buffer_descs.erase(buffer);

            if (!get_shared_state()->m_capture_context_params.m_buffer_targets.erase(buffer))
            {
                vogl_error_printf("Can't erase buffer handle 0x%04X from buffer target map!\n", buffer);
            }
        }
    }

    inline gl_buffer_desc &get_or_create_buffer_desc(GLuint handle)
    {
        vogl_scoped_context_shadow_lock lock;

        gl_buffer_desc_map::iterator buf_it(get_shared_state()->m_buffer_descs.find(handle));
        if (buf_it != get_shared_state()->m_buffer_descs.end())
            return buf_it->second;

        gl_buffer_desc desc(handle);
        return (get_shared_state()->m_buffer_descs.insert(handle, desc).first)->second;
    }

    inline uint32_t get_total_mapped_buffers() const
    {
        vogl_scoped_context_shadow_lock lock;

        uint32_t total = 0;
        for (gl_buffer_desc_map::const_iterator it = get_shared_state()->m_buffer_descs.begin(); it != get_shared_state()->m_buffer_descs.end(); ++it)
        {
            if (it->second.m_pMap)
                total++;
        }
        return total;
    }

    inline void set_window_dimensions(int width, int height)
    {
        m_window_width = width;
        m_window_height = height;
    }

    inline int get_window_width() const
    {
        return m_window_width;
    }
    inline int get_window_height() const
    {
        return m_window_height;
    }

    uint64_t get_frame_index() const
    {
        return m_frame_index;
    }
    void set_frame_index(uint32_t f)
    {
        m_frame_index = f;
    }
    void inc_frame_index()
    {
        m_frame_index++;
    }

    GLuint get_cur_program() const
    {
        return m_cur_program;
    }
    void set_cur_program(GLuint program)
    {
        m_cur_program = program;
    }

    GLenum get_latched_gl_error() const
    {
        return m_latched_gl_error;
    }
    bool has_latched_gl_error() const
    {
        return m_latched_gl_error != GL_NO_ERROR;
    }
    void clear_latched_gl_error()
    {
        m_latched_gl_error = GL_NO_ERROR;
    }
    void set_latched_gl_error(GLenum err)
    {
        m_latched_gl_error = err;
    }

    // Returns the context's most recent GL error, NOT the currently latched error.
    GLenum peek_and_record_gl_error()
    {
        if (m_in_gl_begin)
            return GL_NO_ERROR;

        GLenum gl_err = GL_ENTRYPOINT(glGetError)();

        if (gl_err != GL_NO_ERROR)
        {
            if (!has_latched_gl_error())
            {
                set_latched_gl_error(gl_err);

                vogl_warning_printf("GL error %s occurred sometime before this function was called. "
                                    "This error has been latched by the tracer onto the tracer's context shadow struct. "
                                    "This error will be reported to the caller the next time glGetError() is called, "
                                    "and will mask any GL error returned by that future call.\n",
                                    get_gl_enums().find_gl_error_name(gl_err));
            }
            else
            {
                vogl_warning_printf("GL error %s occurred sometime before this function was called. "
                                    "The tracer already had a latched GL error of %s. "
                                    "This error will be suppressed by the tracer "
                                    "(because it would have been by GL itself if the tracer was not present).\n",
                                    get_gl_enums().find_gl_error_name(gl_err), get_gl_enums().find_gl_error_name(get_latched_gl_error()));
            }
        }

        return gl_err;
    }

    // Gets and drops the current GL error (this is done to "hide" any errors we've introduced by tracing from the client app).
    // Be sure to retrieve and latch any client errors by calling peek_and_record_gl_error() first!
    GLenum peek_and_drop_gl_error()
    {
        if (m_in_gl_begin)
            return GL_NO_ERROR;

        GLenum gl_err = GL_ENTRYPOINT(glGetError)();

        if (gl_err != GL_NO_ERROR)
        {
            vogl_error_printf("GL error %s occurred internally while libvogltrace was making GL calls. This GL error will not be seen by the client app (THIS SHOULD NOT HAPPEN)\n", get_gl_enums().find_gl_error_name(gl_err));
        }

        return gl_err;
    }

    // TODO: Modify each shadowing method to print detailed error messages
    // TODO: Move all these shadow methods right into vogl_capture_context_params

    // Texture handle shadowing
    void gen_textures(GLsizei n, GLuint *pTextures)
    {
        if (!pTextures)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pTextures[i];
            if (handle)
            {
                if (!get_shared_state()->m_capture_context_params.m_textures.insert(handle, handle, GL_NONE))
                {
                    vogl_warning_printf("Unable to add texture handle %u to texture handle shadow map!\n", handle);
                }
            }
        }
    }

    void del_textures(GLsizei n, const GLuint *pTextures)
    {
        if (!pTextures)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pTextures[i];
            if (handle)
            {
                if (!get_shared_state()->m_capture_context_params.m_textures.erase(handle))
                {
                    vogl_warning_printf("Failed erasing handle %u from texture handle shadow map!\n", handle);
                }
            }
        }
    }

    void bind_texture(GLenum target, GLuint texture)
    {
        if (texture)
        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_textures.update(texture, target);
        }
    }

    void bind_texture_conditionally(GLenum target, GLuint texture)
    {
        if (texture)
        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_textures.conditional_update(texture, GL_NONE, target);
        }
    }

    // Framebuffer handle shadowing
    void gen_framebuffers(GLsizei n, GLuint *pFramebuffers)
    {
        if (!pFramebuffers)
            return;
        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pFramebuffers[i];
            if (handle)
                get_context_state()->m_capture_context_params.m_framebuffers.insert(handle);
        }
    }

    void del_framebuffers(GLsizei n, const GLuint *pFramebuffers)
    {
        if (!pFramebuffers)
            return;
        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pFramebuffers[i];
            if (handle)
                get_context_state()->m_capture_context_params.m_framebuffers.erase(handle);
        }
    }

    // VAO handle shadowing
    void gen_vertexarrays(GLsizei n, GLuint *pArrays)
    {
        if (!pArrays)
            return;
        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pArrays[i];
            if (handle)
                get_context_state()->m_capture_context_params.m_vaos.insert(handle);
        }
    }

    void del_vertexarrays(GLsizei n, const GLuint *pArrays)
    {
        if (!pArrays)
            return;
        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pArrays[i];
            if (handle)
                get_context_state()->m_capture_context_params.m_vaos.erase(handle);
        }
    }

    void bind_vertexarray(GLuint array)
    {
        if (array)
            get_context_state()->m_capture_context_params.m_vaos.insert(array);
    }

    // sync handle shadowing
    void gen_sync(GLsync sync)
    {
        if (sync)
        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_syncs.insert((uint64_t)sync);
        }
    }

    void del_sync(GLsync sync)
    {
        if (sync)
        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_syncs.erase((uint64_t)sync);
        }
    }

    // sampler handle shadowing
    void gen_samplers(GLsizei n, const GLuint *pSamplers)
    {
        if (!pSamplers)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pSamplers[i];
            if (handle)
                get_shared_state()->m_capture_context_params.m_samplers.insert(handle);
        }
    }

    void del_samplers(GLsizei n, const GLuint *pSamplers)
    {
        if (!pSamplers)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pSamplers[i];
            if (handle)
                get_shared_state()->m_capture_context_params.m_samplers.erase(handle);
        }
    }

    // query handle and target shadowing
    void gen_queries(GLsizei n, GLuint *pIDs)
    {
        if (!pIDs)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint id = pIDs[i];
            if (id)
                get_shared_state()->m_capture_context_params.m_query_targets.insert(id, GL_NONE);
        }
    }

    void del_queries(GLsizei n, const GLuint *pIDs)
    {
        if (!pIDs)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint id = pIDs[i];
            if (id)
                get_shared_state()->m_capture_context_params.m_query_targets.erase(id);
        }
    }

    void begin_query(GLenum target, GLuint id)
    {
        if (id)
        {
            vogl_scoped_context_shadow_lock lock;

            if (get_shared_state()->m_capture_context_params.m_query_targets.contains(id))
            {
                get_shared_state()->m_capture_context_params.m_query_targets[id] = target;
            }
            else
            {
                vogl_error_printf("Unable to find query target handle %u in query target handle shadow\n", id);
            }
        }
    }

    // arb programs
    void gen_arb_programs(GLsizei n, const GLuint *pHandles)
    {
        if (!pHandles)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pHandles[i];
            if (handle)
                get_shared_state()->m_capture_context_params.m_arb_program_targets.insert(handle, GL_NONE);
        }
    }

    void bind_arb_program(GLenum target, GLuint handle)
    {
        if (handle)
        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_arb_program_targets[handle] = target;
        }
    }

    void del_arb_programs(GLsizei n, const GLuint *pHandles)
    {
        if (!pHandles)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pHandles[i];
            if (handle)
                get_shared_state()->m_capture_context_params.m_arb_program_targets.erase(handle);
        }
    }
    
    // program pipelines
    void gen_program_pipelines(GLsizei n, const GLuint *pHandles)
    {
        if (!pHandles)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pHandles[i];
            if (handle)
                if (!get_shared_state()->m_capture_context_params.m_program_pipelines.insert(handle, handle, GL_NONE))
                    vogl_error_printf("Can't insert program pipeline handle 0x%04X into program pipeline shadow!\n", handle);
        }
    }

    void bind_program_pipelines(GLenum target, GLuint handle)
    {
        if (handle)
        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_program_pipelines.update(handle, target);
        }
    }

    void del_program_pipelines(GLsizei n, const GLuint *pHandles)
    {
        if (!pHandles)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint handle = pHandles[i];
            if (handle)
                get_shared_state()->m_capture_context_params.m_program_pipelines.erase(handle);
        }
    }

    // renderbuffer handle shadowing
    void gen_render_buffers(GLsizei n, GLuint *pIDs)
    {
        if (!pIDs)
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint id = pIDs[i];
            if (id)
            {
                if (!get_shared_state()->m_capture_context_params.m_rbos.insert(id, id, GL_NONE))
                {
                    vogl_error_printf("Can't insert render buffer handle 0x%04X into render buffer shadow!\n", id);
                }
            }
        }
    }

    void del_render_buffers(GLsizei n, const GLuint *buffers)
    {
        if ((!n) || (!buffers))
            return;

        vogl_scoped_context_shadow_lock lock;

        for (GLsizei i = 0; i < n; i++)
        {
            GLuint buffer = buffers[i];
            if (!buffer)
                continue;

            if (!get_shared_state()->m_capture_context_params.m_rbos.erase(buffer))
            {
                vogl_error_printf("Can't erase render buffer handle 0x%04X from render buffer shadow!\n", buffer);
            }
        }
    }

    // program/shader shadowing
    GLuint handle_create_program(gl_entrypoint_id_t id)
    {
        GLuint handle;

        if (id == VOGL_ENTRYPOINT_glCreateProgram)
            handle = GL_ENTRYPOINT(glCreateProgram)();
        else
        {
            VOGL_ASSERT(id == VOGL_ENTRYPOINT_glCreateProgramObjectARB);
            handle = GL_ENTRYPOINT(glCreateProgramObjectARB)();
        }

        vogl_scoped_context_shadow_lock lock;

        if (handle)
        {
            if (!get_shared_state()->m_capture_context_params.m_objs.update(handle, handle, VOGL_PROGRAM_OBJECT))
                vogl_error_printf("Failed inserting program handle %u into object shadow!\n", handle);
        }
        else
        {
            vogl_error_printf("glCreateProgram/glCreateProgramObjectARB on handle %u failed!\n", handle);
        }

        return handle;
    }

    void add_program(GLuint handle)
    {
        vogl_scoped_context_shadow_lock lock;

        if (!get_shared_state()->m_capture_context_params.m_objs.update(handle, handle, VOGL_PROGRAM_OBJECT))
            vogl_error_printf("Failed inserting program handle %u into object shadow!\n", handle);
    }

    GLuint handle_create_shader(gl_entrypoint_id_t id, GLenum type)
    {
        GLuint handle;

        if (id == VOGL_ENTRYPOINT_glCreateShader)
            handle = GL_ENTRYPOINT(glCreateShader)(type);
        else
        {
            VOGL_ASSERT(id == VOGL_ENTRYPOINT_glCreateShaderObjectARB);
            handle = GL_ENTRYPOINT(glCreateShaderObjectARB)(type);
        }

        vogl_scoped_context_shadow_lock lock;

        if (handle)
        {
            if (!get_shared_state()->m_capture_context_params.m_objs.update(handle, handle, VOGL_SHADER_OBJECT))
                vogl_error_printf("Failed inserting shader handle %u into object shadow! (type=%s)\n", handle, get_gl_enums().find_gl_name(type));
        }
        else
        {
            vogl_error_printf("glCreateShader/glCreateShaderObjectARB on handle %u failed! (type=%s)\n", handle, get_gl_enums().find_gl_name(type));
        }

        return handle;
    }

    void add_shader(GLuint handle)
    {
        vogl_scoped_context_shadow_lock lock;

        if (!get_shared_state()->m_capture_context_params.m_objs.update(handle, handle, VOGL_SHADER_OBJECT))
            vogl_error_printf("Failed inserting shader handle %u into object shadow!\n", handle);
    }

    bool has_linked_program_snapshot(GLuint handle)
    {
        vogl_scoped_context_shadow_lock lock;

        return get_shared_state()->m_capture_context_params.m_linked_programs.find_snapshot(handle) != NULL;
    }

    bool add_linked_program_snapshot(gl_entrypoint_id_t link_entrypoint, GLuint handle, GLenum binary_format = GL_NONE, const void *pBinary = NULL, uint32_t binary_size = 0)
    {
        vogl_scoped_context_shadow_lock lock;

        return get_shared_state()->m_capture_context_params.m_linked_programs.add_snapshot(m_context_info, m_handle_remapper, link_entrypoint, handle, binary_format, pBinary, binary_size);
    }

    bool add_linked_program_snapshot(gl_entrypoint_id_t link_entrypoint, GLuint handle, GLenum type, GLsizei count, GLchar *const *strings)
    {
        vogl_scoped_context_shadow_lock lock;

        return get_shared_state()->m_capture_context_params.m_linked_programs.add_snapshot(m_context_info, m_handle_remapper, link_entrypoint, handle, type, count, strings);
    }

    void handle_use_program(gl_entrypoint_id_t id, GLuint program)
    {
        // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

        peek_and_record_gl_error();

        check_program_binding_shadow();

        vogl_scoped_context_shadow_lock lock;

        VOGL_ASSERT(!program || (get_shared_state()->m_capture_context_params.m_objs.get_target(program) == VOGL_PROGRAM_OBJECT));

        GLuint prev_program = m_cur_program;

        bool prev_is_program = false;
        GLint prev_is_marked_for_deletion = false;
        vogl::growable_array<GLuint, 8> prev_attached_replay_shaders;

        if ((prev_program) && (program != prev_program))
        {
            prev_is_program = GL_ENTRYPOINT(glIsProgram)(prev_program) != GL_FALSE;
            peek_and_drop_gl_error();

            if (prev_is_program)
            {
                GL_ENTRYPOINT(glGetProgramiv)(prev_program, GL_DELETE_STATUS, &prev_is_marked_for_deletion);
                peek_and_drop_gl_error();

                if (prev_is_marked_for_deletion)
                {
                    // The currently bound program is marked for deletion, so record which shaders are attached to it in case the program is actually deleted by the driver on the UseProgram() call.
                    GLint num_attached_shaders = 0;
                    GL_ENTRYPOINT(glGetProgramiv)(prev_program, GL_ATTACHED_SHADERS, &num_attached_shaders);
                    peek_and_drop_gl_error();

                    if (num_attached_shaders)
                    {
                        prev_attached_replay_shaders.resize(num_attached_shaders);

                        GLsizei actual_count = 0;
                        GL_ENTRYPOINT(glGetAttachedShaders)(prev_program, num_attached_shaders, &actual_count, prev_attached_replay_shaders.get_ptr());
                        peek_and_drop_gl_error();

                        VOGL_ASSERT(actual_count == num_attached_shaders);
                    }
                }
            }
        }

        if (id == VOGL_ENTRYPOINT_glUseProgram)
            GL_ENTRYPOINT(glUseProgram)(program);
        else
        {
            VOGL_ASSERT(id == VOGL_ENTRYPOINT_glUseProgramObjectARB);
            GL_ENTRYPOINT(glUseProgramObjectARB)(program);
        }

        GLenum gl_err = peek_and_record_gl_error();
        if (gl_err != GL_NO_ERROR)
        {
            vogl_error_printf("glUseProgram/glUseProgramObjectARB on handle %u returned GL error %s\n", program, get_gl_enums().find_gl_error_name(gl_err));
            return;
        }

        if ((prev_program) && (prev_program != program))
        {
            bool is_prev_still_program = GL_ENTRYPOINT(glIsProgram)(prev_program) != GL_FALSE;
            if (!is_prev_still_program)
            {
                VOGL_ASSERT(prev_is_program);
                VOGL_ASSERT(prev_is_marked_for_deletion);

                // The previously bound program is really dead now, kill it from our tables and also check up on any shaders it was attached to.
                bool was_deleted = get_shared_state()->m_capture_context_params.m_objs.erase(prev_program);
                VOGL_ASSERT(was_deleted);
                VOGL_NOTE_UNUSED(was_deleted);

                get_shared_state()->m_capture_context_params.m_linked_programs.remove_snapshot(prev_program);

                for (uint32_t i = 0; i < prev_attached_replay_shaders.size(); i++)
                {
                    GLuint shader_handle = prev_attached_replay_shaders[i];

                    bool is_still_shader = GL_ENTRYPOINT(glIsShader)(shader_handle) != GL_FALSE;
                    peek_and_drop_gl_error();

                    if (is_still_shader)
                        continue;

                    if (!get_shared_state()->m_capture_context_params.m_objs.contains(shader_handle))
                    {
                        // We didn't create this shader handle, the AMD driver did on a program binary link, so ignore it.
                        continue;
                    }

                    // The attached shader is now really dead.
                    VOGL_ASSERT(get_shared_state()->m_capture_context_params.m_objs.get_target(shader_handle) == VOGL_SHADER_OBJECT);
                    if (!get_shared_state()->m_capture_context_params.m_objs.erase(shader_handle))
                    {
                        vogl_error_printf("Failed finding attached shader %u in objects hash table, while handling the actual deletion of program %u\n", shader_handle, prev_program);
                    }
                }
            }
        }

        m_cur_program = program;

        check_program_binding_shadow();
    }

    void handle_del_shader(gl_entrypoint_id_t id, GLuint obj)
    {
        // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

        peek_and_record_gl_error();

        check_program_binding_shadow();

        vogl_scoped_context_shadow_lock lock;

        VOGL_ASSERT(!obj || (get_shared_state()->m_capture_context_params.m_objs.get_target(obj) == VOGL_SHADER_OBJECT));

        if (id == VOGL_ENTRYPOINT_glDeleteObjectARB)
            GL_ENTRYPOINT(glDeleteObjectARB)(obj);
        else
            GL_ENTRYPOINT(glDeleteShader)(obj);

        GLenum gl_err = peek_and_record_gl_error();
        if (gl_err != GL_NO_ERROR)
        {
            vogl_error_printf("glDeleteShader/glDeleteObjectARB on handle %u returned GL error %s\n", obj, get_gl_enums().find_gl_error_name(gl_err));
            return;
        }

        if (!obj)
            return;

        bool is_still_shader = GL_ENTRYPOINT(glIsShader)(obj) != GL_FALSE;
        peek_and_drop_gl_error();

        if (!is_still_shader)
        {
            // The shader is really gone.
            bool was_deleted = get_shared_state()->m_capture_context_params.m_objs.erase(obj);
            VOGL_ASSERT(was_deleted);
            VOGL_NOTE_UNUSED(was_deleted);
        }
        else
        {
            GLint marked_for_deletion = 0;
            GL_ENTRYPOINT(glGetShaderiv)(obj, GL_DELETE_STATUS, &marked_for_deletion);
            peek_and_drop_gl_error();

            VOGL_VERIFY(marked_for_deletion);

            // The shader is attached to a live program object (which may or may not be actually in the marked_as_deleted state)
            // we'll get around to it when the program object is deleted, or when they remove the program object from the current state.
        }
    }

    void handle_del_program(gl_entrypoint_id_t id, GLuint obj)
    {
        // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

        peek_and_record_gl_error();

        check_program_binding_shadow();

        vogl_scoped_context_shadow_lock lock;

        VOGL_ASSERT(!obj || (get_shared_state()->m_capture_context_params.m_objs.get_target(obj) == VOGL_PROGRAM_OBJECT));

        bool is_program = GL_ENTRYPOINT(glIsProgram)(obj) != 0;
        peek_and_drop_gl_error();

        vogl::growable_array<GLuint, 8> attached_replay_shaders;

        if ((is_program) && (obj))
        {
            GLint num_attached_shaders = 0;
            GL_ENTRYPOINT(glGetProgramiv)(obj, GL_ATTACHED_SHADERS, &num_attached_shaders);
            peek_and_drop_gl_error();

            if (num_attached_shaders)
            {
                attached_replay_shaders.resize(num_attached_shaders);

                GLsizei actual_count = 0;
                GL_ENTRYPOINT(glGetAttachedShaders)(obj, num_attached_shaders, &actual_count, attached_replay_shaders.get_ptr());
                peek_and_drop_gl_error();

                VOGL_ASSERT(actual_count == num_attached_shaders);
            }
        }

        if (id == VOGL_ENTRYPOINT_glDeleteObjectARB)
            GL_ENTRYPOINT(glDeleteObjectARB)(obj);
        else
            GL_ENTRYPOINT(glDeleteProgram)(obj);

        GLenum gl_err = peek_and_record_gl_error();
        if (gl_err != GL_NO_ERROR)
        {
            vogl_error_printf("glDeleteProgram/glDeleteObjectARB on handle %u returned GL error %s\n", obj, get_gl_enums().find_gl_error_name(gl_err));
            return;
        }

        if (!obj)
            return;

        bool is_still_program = GL_ENTRYPOINT(glIsProgram)(obj) != 0;
        peek_and_drop_gl_error();

        GLint marked_for_deletion = 0;
        if (is_still_program)
        {
            // It must still be bound to the context, or referred to in some other way that we don't know about.
            GL_ENTRYPOINT(glGetProgramiv)(obj, GL_DELETE_STATUS, &marked_for_deletion);
            bool delete_status_check_succeeded = (peek_and_drop_gl_error() == GL_NO_ERROR);

            VOGL_VERIFY(delete_status_check_succeeded);
            VOGL_VERIFY(marked_for_deletion);
        }
        else if (!is_still_program)
        {
            VOGL_ASSERT(is_program);

            // The program is really gone now.
            bool was_deleted = get_shared_state()->m_capture_context_params.m_objs.erase(obj);
            VOGL_ASSERT(was_deleted);
            VOGL_NOTE_UNUSED(was_deleted);

            get_shared_state()->m_capture_context_params.m_linked_programs.remove_snapshot(obj);

            if (m_cur_program == obj)
            {
                // This shouldn't happen - if the program is still bound to the context then it should still be a program.
                VOGL_ASSERT_ALWAYS;
                m_cur_program = 0;
            }

            for (uint32_t i = 0; i < attached_replay_shaders.size(); i++)
            {
                GLuint shader_handle = attached_replay_shaders[i];

                bool is_still_shader = GL_ENTRYPOINT(glIsShader)(shader_handle) != 0;
                peek_and_drop_gl_error();

                if (is_still_shader)
                    continue;

                if (!get_shared_state()->m_capture_context_params.m_objs.contains(shader_handle))
                {
                    // We didn't create this shader handle, the AMD driver did on a program binary link, so ignore it.
                    continue;
                }

                // The attached shader is now really dead.
                VOGL_ASSERT(get_shared_state()->m_capture_context_params.m_objs.get_target(shader_handle) == VOGL_SHADER_OBJECT);
                if (!get_shared_state()->m_capture_context_params.m_objs.erase(shader_handle))
                {
                    vogl_error_printf("Failed finding attached shader %u in objects shadow, while handling the deletion of program %u\n", shader_handle, obj);
                }
            }
        }

        check_program_binding_shadow();
    }

    void handle_del_object(gl_entrypoint_id_t id, GLuint obj)
    {
        vogl_scoped_context_shadow_lock lock;

        if (!obj)
            return;

        GLenum target = get_shared_state()->m_capture_context_params.m_objs.get_target(obj);

        if (target == VOGL_PROGRAM_OBJECT)
            handle_del_program(id, obj);
        else if (target == VOGL_SHADER_OBJECT)
            handle_del_shader(id, obj);
        else
        {
            vogl_error_printf("glDeleteObjectARB: Unable to find object handle %u in object shadow\n", obj);

            GL_ENTRYPOINT(glDeleteObjectARB)(obj);
        }
    }

    void handle_detach_shader(gl_entrypoint_id_t id, GLuint program, GLuint shader)
    {
        vogl_scoped_context_shadow_lock lock;

        peek_and_record_gl_error();

        // Note: This mixes ARB and non-ARB funcs. to probe around, which is evil.

        GLboolean was_shader = GL_ENTRYPOINT(glIsShader)(shader);
        peek_and_drop_gl_error();

        GLint marked_for_deletion = 0;
        GL_ENTRYPOINT(glGetShaderiv)(shader, GL_DELETE_STATUS, &marked_for_deletion);
        peek_and_drop_gl_error();

        if (id == VOGL_ENTRYPOINT_glDetachObjectARB)
            GL_ENTRYPOINT(glDetachObjectARB)(program, shader);
        else
        {
            VOGL_ASSERT(id == VOGL_ENTRYPOINT_glDetachShader);
            GL_ENTRYPOINT(glDetachShader)(program, shader);
        }

        GLenum gl_err = peek_and_record_gl_error();
        if (gl_err != GL_NO_ERROR)
        {
            vogl_error_printf("glDetachShader/glDetachObjectARB on program handle %u shader handle %u returned GL error %s\n", program, shader, get_gl_enums().find_gl_error_name(gl_err));
            return;
        }

        GLboolean is_shader = GL_ENTRYPOINT(glIsShader)(shader);
        peek_and_drop_gl_error();

        if ((marked_for_deletion) && (was_shader) && (!is_shader))
        {
            // The shader is really gone now.
            bool was_deleted = get_shared_state()->m_capture_context_params.m_objs.erase(shader);
            VOGL_ASSERT(was_deleted);
            VOGL_NOTE_UNUSED(was_deleted);
        }
    }

    // glBegin/glEnd shadowing

    // Note: This flag gets set even when the client is composing a display list!
    void set_in_gl_begin(bool value)
    {
        m_in_gl_begin = value;
    }
    bool get_in_gl_begin() const
    {
        return m_in_gl_begin;
    }

    void set_uses_client_side_arrays(bool value)
    {
        m_uses_client_side_arrays = value;
    }
    bool get_uses_client_side_arrays() const
    {
        return m_uses_client_side_arrays;
    }

    typedef vogl::map<dynamic_string, dynamic_string, dynamic_string_less_than_case_sensitive, dynamic_string_equal_to_case_sensitive> extension_id_to_string_map_t;
    extension_id_to_string_map_t &get_extension_map()
    {
        return m_extension_map;
    }

    const vogl_context_handle_remapper &get_handle_remapper() const
    {
        return m_handle_remapper;
    }
    vogl_context_handle_remapper &get_handle_remapper()
    {
        return m_handle_remapper;
    }

    // Display list shadowing

    GLenum get_current_display_list_mode() const
    {
        return m_current_display_list_mode;
    }
    bool is_composing_display_list() const
    {
        return m_current_display_list_handle >= 0;
    }
    GLint get_current_display_list_handle() const
    {
        return m_current_display_list_handle;
    }

    bool new_list(GLuint handle, GLenum mode)
    {
        VOGL_FUNC_TRACER

        VOGL_ASSERT((mode == GL_COMPILE) || (mode == GL_COMPILE_AND_EXECUTE));

        if (m_current_display_list_handle >= 0)
        {
            vogl_error_printf("Can't define new display list %u while display list %i is already being defined!\n", handle, m_current_display_list_handle);
            return false;
        }

        m_current_display_list_handle = handle;
        m_current_display_list_mode = mode;

        vogl_scoped_context_shadow_lock lock;

        get_shared_state()->m_capture_context_params.m_display_lists.new_list(handle, handle);

        return true;
    }

    bool end_list()
    {
        VOGL_FUNC_TRACER

        if (m_current_display_list_handle < 0)
        {
            vogl_error_printf("No display list is active!\n");
            return false;
        }

        {
            vogl_scoped_context_shadow_lock lock;

            get_shared_state()->m_capture_context_params.m_display_lists.end_list(m_current_display_list_handle);
        }

        m_current_display_list_handle = -1;
        m_current_display_list_mode = GL_NONE;

        return true;
    }

    void gen_lists(GLuint result, GLsizei range)
    {
        VOGL_FUNC_TRACER

        if (!range)
            return;

        vogl_scoped_context_shadow_lock lock;

        get_shared_state()->m_capture_context_params.m_display_lists.gen_lists(result, range);
    }

    void del_lists(GLuint list, GLsizei range)
    {
        VOGL_FUNC_TRACER

        if (!range)
            return;

        vogl_scoped_context_shadow_lock lock;

        get_shared_state()->m_capture_context_params.m_display_lists.del_lists(list, range);
    }

    void glx_font(const char *pFont, int first, int count, int list_base)
    {
        vogl_scoped_context_shadow_lock lock;

        get_shared_state()->m_capture_context_params.m_display_lists.glx_font(pFont, first, count, list_base);
    }

    bool parse_list_and_update_shadows(GLuint handle, vogl_display_list_state::pBind_callback_func_ptr pBind_callback, void *pBind_callback_opaque)
    {
        vogl_scoped_context_shadow_lock lock;

        return get_shared_state()->m_capture_context_params.m_display_lists.parse_list_and_update_shadows(handle, pBind_callback, pBind_callback_opaque);
    }

    bool parse_lists_and_update_shadows(GLsizei n, GLenum type, const GLvoid *lists, vogl_display_list_state::pBind_callback_func_ptr pBind_callback, void *pBind_callback_opaque)
    {
        vogl_scoped_context_shadow_lock lock;

        return get_shared_state()->m_capture_context_params.m_display_lists.parse_lists_and_update_shadows(n, type, lists, pBind_callback, pBind_callback_opaque);
    }

    bool add_packet_to_current_display_list(gl_entrypoint_id_t func, const vogl_trace_packet &packet)
    {
        VOGL_FUNC_TRACER

        if (m_current_display_list_handle < 0)
            return true;

        if (!vogl_display_list_state::is_call_listable(func, packet))
        {
            if (g_vogl_entrypoint_descs[func].m_is_listable)
            {
                if (!g_vogl_entrypoint_descs[func].m_whitelisted_for_displaylists)
                    vogl_error_printf("Failed serializing trace packet into display list shadow! Call is not listable.\n");
                else
                    vogl_error_printf("Failed serializing trace packet into display list shadow! Call with these parameters is not listable.\n");
            }
            return false;
        }

        vogl_scoped_context_shadow_lock lock;

        return get_shared_state()->m_capture_context_params.m_display_lists.add_packet_to_list(m_current_display_list_handle, func, packet);
    }

    bool MakeCurrent(CONTEXT_TYPE new_context)
    {
        #if (VOGL_PLATFORM_HAS_GLX)
            return GL_ENTRYPOINT(glXMakeCurrent)(get_display(), get_drawable(), new_context);

		#elif (VOGL_PLATFORM_HAS_CGL)
			VOGL_ASSERT(!"UNIMPLEMENTED MakeCurrent");
			return false;

        #elif (VOGL_PLATFORM_HAS_WGL)
            return GL_ENTRYPOINT(wglMakeCurrent)(get_hdc(), new_context) != GL_FALSE;

        #else
            #error "Need a MakeCurrent implementation for this platform."
        #endif
    }

    CONTEXT_TYPE GetCurrentContext() const
    {
        #if (VOGL_PLATFORM_HAS_GLX)
            return GL_ENTRYPOINT(glXGetCurrentContext)();

		#elif (VOGL_PLATFORM_HAS_CGL)
			VOGL_ASSERT(!"UNIMPLEMENTED GetCurrentContext");
			return false;

        #elif (VOGL_PLATFORM_HAS_WGL)
            return GL_ENTRYPOINT(wglGetCurrentContext)();

        #else
            #error "Need a GetCurrentContext implementation for this platform."
            return CONTEXT_TYPE(0);
        #endif
    }


private:
    int m_ref_count;
    bool m_deleted_flag;
    vogl_context *m_pShared_state;

    CONTEXT_TYPE m_context_handle;
    CONTEXT_TYPE m_sharelist_handle;

    const Display *m_pDpy;
    GLXDrawable m_drawable;
    GLXDrawable m_read_drawable;
    int m_render_type;
    XVisualInfo m_xvisual_info;
    GLXFBConfig m_fb_config;

    HDC m_hdc;

    bool m_direct;

    bool m_created_from_attribs;
    vogl::vector<int> m_attrib_list;

    uint64_t m_current_thread; // as returned by vogl_get_current_kernel_thread_id()

    GLuint m_max_vertex_attribs;

    bool m_has_been_made_current;

    int m_window_width, m_window_height;
    uint64_t m_frame_index;

    gl_entrypoint_id_t m_creation_func;
    vogl_context_desc m_context_desc;
    vogl_context_info m_context_info; // only valid after first MakeCurrent

    extension_id_to_string_map_t m_extension_map;

    GLenum m_latched_gl_error;

    gl_buffer_desc_map m_buffer_descs;

    vogl_capture_context_params m_capture_context_params;

    vogl_framebuffer_capturer m_framebuffer_capturer;

    GLuint m_cur_program;

    bool m_in_gl_begin;
    bool m_uses_client_side_arrays;

    vogl_context_handle_remapper m_handle_remapper;

    int m_current_display_list_handle;
    GLenum m_current_display_list_mode;

    static void GLAPIENTRY debug_callback_arb(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *pUser_param)
    {
        (void)length;

        char final_message[4096];

        vogl_context *pContext = (vogl_context *)(pUser_param);

        vogl_format_debug_output_arb(final_message, sizeof(final_message), source, type, id, severity, reinterpret_cast<const char *>(message));

        if (pContext)
        {
            vogl_warning_printf("Trace context: 0x%" PRIX64 ", Thread id: 0x%" PRIX64 "\n%s\n",
                               cast_val_to_uint64(pContext->m_context_handle), vogl_get_current_kernel_thread_id(), final_message);
        }
        else
        {
            vogl_warning_printf("%s\n", final_message);
        }
    }

    void on_first_make_current()
    {
        if ((g_command_line_params().get_value_as_bool("vogl_force_debug_context")) && (m_context_info.is_debug_context()))
        {
            if (GL_ENTRYPOINT(glDebugMessageCallbackARB) && m_context_info.supports_extension("GL_ARB_debug_output"))
            {
                VOGL_CHECK_GL_ERROR;

                GL_ENTRYPOINT(glDebugMessageCallbackARB)(debug_callback_arb, (GLvoid *)this);
                GL_ENTRYPOINT(glEnable)(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

                VOGL_CHECK_GL_ERROR;
            }
            else
            {
                vogl_error_printf("Can't enable debug context, either glDebugMessageCallbackARB func or the GL_ARB_debug_output extension is not available!\n");
            }
        }
    }

    // Note: Check for any GL errors before calling this method.
    bool check_program_binding_shadow()
    {
        GLint actual_cur_program = 0;
        GL_ENTRYPOINT(glGetIntegerv)(GL_CURRENT_PROGRAM, &actual_cur_program);

        if (peek_and_drop_gl_error() != GL_NO_ERROR)
            vogl_error_printf("GL error checking program binding shadow!\n");

        if (m_cur_program == static_cast<GLuint>(actual_cur_program))
            return true;

        // OK, on AMD it plays games with GL_CURRENT_PROGRAM when the currently bound program is deleted. Check for this scenario.
        bool is_still_program = GL_ENTRYPOINT(glIsProgram)(m_cur_program) != 0;
        if ((peek_and_drop_gl_error() == GL_NO_ERROR) && (is_still_program))
        {
            GLint marked_for_deletion = GL_FALSE;
            GL_ENTRYPOINT(glGetProgramiv)(m_cur_program, GL_DELETE_STATUS, &marked_for_deletion);

            if ((peek_and_drop_gl_error() == GL_NO_ERROR) && (marked_for_deletion))
                return true;
        }

        VOGL_VERIFY(!"m_cur_program appears out of sync with GL's GL_CURRENT_PROGRAM");
        return false;
    }
};

//----------------------------------------------------------------------------------------------------------------------
// vogl_context_handle_remapper::is_valid_handle
//----------------------------------------------------------------------------------------------------------------------
bool vogl_context_handle_remapper::is_valid_handle(vogl_namespace_t handle_namespace, uint64_t from_handle)
{
    if (!from_handle)
        return 0;

    uint32_t handle = static_cast<uint32_t>(from_handle);

    const vogl_capture_context_params &capture_context_params = m_pContext->get_shared_state()->get_capture_context_params();

    // TODO: Support all the object types
    if (handle_namespace == VOGL_NAMESPACE_SHADERS)
    {
        VOGL_ASSERT(handle == from_handle);
        return (capture_context_params.m_objs.get_target(handle) == VOGL_SHADER_OBJECT);
    }
    else if (handle_namespace == VOGL_NAMESPACE_PROGRAMS)
    {
        VOGL_ASSERT(handle == from_handle);
        return (capture_context_params.m_objs.get_target(handle) == VOGL_PROGRAM_OBJECT);
    }
    else if (handle_namespace == VOGL_NAMESPACE_TEXTURES)
    {
        VOGL_ASSERT(handle == from_handle);
        return capture_context_params.m_textures.contains(handle);
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_context_handle_remapper::determine_from_object_target
//----------------------------------------------------------------------------------------------------------------------
bool vogl_context_handle_remapper::determine_from_object_target(vogl_namespace_t handle_namespace, uint64_t from_handle, GLenum &target)
{
    target = GL_NONE;

    if (!from_handle)
        return false;

    uint32_t handle = static_cast<uint32_t>(from_handle);

    const vogl_capture_context_params &capture_context_params = m_pContext->get_shared_state()->get_capture_context_params();

    // TODO: Support all the object types
    if (handle_namespace == VOGL_NAMESPACE_TEXTURES)
    {
        if (capture_context_params.m_textures.contains(handle))
        {
            target = capture_context_params.m_textures.get_target(handle);
            return true;
        }
    }

    VOGL_VERIFY(0);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_scoped_gl_error_absorber::vogl_scoped_gl_error_absorber
//----------------------------------------------------------------------------------------------------------------------
vogl_scoped_gl_error_absorber::vogl_scoped_gl_error_absorber(vogl_context *pContext)
    : m_pContext(pContext)
{
    // Latch any errors that are present on the context, if any, so the client will see it later
    if (pContext)
        pContext->peek_and_record_gl_error();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_scoped_gl_error_absorber::~vogl_scoped_gl_error_absorber
//----------------------------------------------------------------------------------------------------------------------
vogl_scoped_gl_error_absorber::~vogl_scoped_gl_error_absorber()
{
    // Now get the current GL error and drop it, so the client app doesn't see it
    if (m_pContext)
        m_pContext->peek_and_drop_gl_error();
}

//----------------------------------------------------------------------------------------------------------------------
// class vogl_context_manager
//----------------------------------------------------------------------------------------------------------------------
class vogl_context_manager
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(vogl_context_manager);

public:
    vogl_context_manager()
        : m_glx_context_map_lock(0, true)
    {
    }

    vogl_context *create_context(CONTEXT_TYPE handle)
    {
        VOGL_ASSERT(handle);

        vogl_context *pVOGL_context = vogl_new(vogl_context, handle);

        {
            scoped_mutex lock(m_glx_context_map_lock);
            m_glx_context_map.insert(handle, pVOGL_context);
        }

        return pVOGL_context;
    }

    vogl_context *lookup_vogl_context(CONTEXT_TYPE handle)
    {
        VOGL_ASSERT(handle);

        vogl_context *pVOGL_context;
        VOGL_NOTE_UNUSED(pVOGL_context);
        context_map::iterator it;

        {
            scoped_mutex lock(m_glx_context_map_lock);
            it = m_glx_context_map.find(handle);
        }

        return (it != m_glx_context_map.end()) ? it->second : NULL;
    }

    bool destroy_context(CONTEXT_TYPE handle)
    {
        VOGL_ASSERT(handle);

        vogl_context *pVOGL_context = lookup_vogl_context(handle);
        if (!pVOGL_context)
            return false;

        {
            scoped_mutex lock(m_glx_context_map_lock);
            m_glx_context_map.erase(handle);
        }

        vogl_thread_local_data *pTLS_data = vogl_get_or_create_thread_local_data();
        if ((pTLS_data) && (pTLS_data->m_pContext == pVOGL_context))
            pTLS_data->m_pContext = NULL;

        vogl_delete(pVOGL_context);

        return true;
    }

    vogl_context *get_or_create_context(CONTEXT_TYPE handle)
    {
        VOGL_ASSERT(handle);

        vogl_context *pVOGL_context = lookup_vogl_context(handle);
        if (!pVOGL_context)
            pVOGL_context = vogl_new(vogl_context, handle);

        return pVOGL_context;
    }

    vogl_context *make_current(CONTEXT_TYPE handle)
    {
        VOGL_ASSERT(handle);

        vogl_context *pVOGL_context = get_or_create_context(handle);

        if (pVOGL_context->get_current_thread())
        {
            if (pVOGL_context->get_current_thread() != vogl_get_current_kernel_thread_id())
            {
                vogl_error_printf("context is being made current, but is already current on another thread\n");
            }
            else
            {
                vogl_warning_printf("context is already current (redundant call)\n");
            }
        }

        vogl_get_or_create_thread_local_data()->m_pContext = pVOGL_context;

        pVOGL_context->on_make_current();

        #if defined(PLATFORM_WINDOWS)
            // On linux, they can resolve functions early. We cannot until the context has been made current
            // because the driver hasn't necessarily been loaded yet.
            vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper_return_actual);
        #endif

        return pVOGL_context;
    }

    vogl_context *get_current(bool create_if_not_set)
    {
        vogl_thread_local_data *pTLS_data = create_if_not_set ?
                    vogl_get_or_create_thread_local_data() :
                    vogl_get_thread_local_data();
        return pTLS_data ? pTLS_data->m_pContext : NULL;
    }

    void release_current()
    {
        // We're releasing the current context - don't need to create if it doesn't exist.
        vogl_context *pVOGL_context = get_current(false);
        if (pVOGL_context)
        {
            if (!pVOGL_context->get_current_thread())
            {
                vogl_error_printf("Context manager's current context is not marked as current on any thread\n");
            }
            pVOGL_context->on_release_current_epilog();

            vogl_get_or_create_thread_local_data()->m_pContext = NULL;
        }
    }

    void lock()
    {
        m_glx_context_map_lock.lock();
    }

    void unlock()
    {
        m_glx_context_map_lock.unlock();
    }

    const context_map &get_context_map() const
    {
        return m_glx_context_map;
    }

    vogl_context *find_context(const Display *dpy, GLXDrawable drawable)
    {
        lock();

        const context_map &contexts = m_glx_context_map;
        for (context_map::const_iterator it = contexts.begin(); it != contexts.end(); ++it)
        {
            vogl_context *p = it->second;
            if ((p->get_display() == dpy) && (p->get_drawable() == drawable))
            {
                unlock();
                return p;
            }
        }

        unlock();
        return NULL;
    }

private:
    mutex m_glx_context_map_lock;
    context_map m_glx_context_map;
};

static vogl_context_manager& get_context_manager()
{
    static vogl_context_manager s_context_manager;
    return s_context_manager;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_context_shadow_lock
//----------------------------------------------------------------------------------------------------------------------
static inline void vogl_context_shadow_lock()
{
    get_context_manager().lock();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_context_shadow_unlock
//----------------------------------------------------------------------------------------------------------------------
static inline void vogl_context_shadow_unlock()
{
    get_context_manager().unlock();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_atexit
//----------------------------------------------------------------------------------------------------------------------
static void vogl_atexit()
{
    #if !defined(PLATFORM_WINDOWS)
        vogl_debug_printf("vogl_atexit()\n");

        scoped_mutex lock(get_vogl_trace_mutex());

        vogl_deinit();
    #else
        vogl_debug_printf("vogl_atexit crashes on windows because of dll unloading issues. Need to investigate further.\n");
    #endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_backtrace
//----------------------------------------------------------------------------------------------------------------------
#if VOGL_PLATFORM_SUPPORTS_BTRACE
    static uint32_t vogl_backtrace(uint32_t addrs_to_skip)
    {
        vogl_backtrace_addrs addrs;
        addrs.m_num_addrs = btrace_get(addrs.m_addrs, addrs.cMaxAddrs, addrs_to_skip);

        vogl_backtrace_hashmap::insert_result ins_res;

        uint32_t index = 0;

        get_backtrace_hashmap_mutex().lock();

        if (!get_vogl_intercept_data().backtrace_hashmap.insert_no_grow(ins_res, addrs))
            vogl_error_printf("Backtrace hashmap exhausted! Please increase VOGL_BACKTRACE_HASHMAP_CAPACITY. Some backtraces in this trace will not have symbols.\n");
        else
        {
            index = ins_res.first.get_index();
            (ins_res.first)->second++;
        }

        get_backtrace_hashmap_mutex().unlock();

        return index;
    }
#endif
//----------------------------------------------------------------------------------------------------------------------
// vogl_flush_backtrace_to_trace_file
//----------------------------------------------------------------------------------------------------------------------
#if VOGL_PLATFORM_SUPPORTS_BTRACE
    static bool vogl_flush_backtrace_to_trace_file()
    {
        scoped_mutex lock(get_vogl_trace_mutex());

        if (!get_vogl_trace_writer().is_opened() || (get_vogl_trace_writer().get_trace_archive() == NULL))
            return false;

        json_document doc;

        get_backtrace_hashmap_mutex().lock();

        vogl_backtrace_hashmap &backtrace_hashmap = get_vogl_intercept_data().backtrace_hashmap;
        uint32_t backtrace_hashmap_size = backtrace_hashmap.size();
        if (backtrace_hashmap_size)
        {
            vogl_verbose_printf("Writing backtrace %u addrs\n", backtrace_hashmap_size);

            json_node *pRoot = doc.get_root();
            pRoot->init_array();

            for (vogl_backtrace_hashmap::const_iterator it = backtrace_hashmap.begin(); it != backtrace_hashmap.end(); ++it)
            {
                json_node &node = pRoot->add_array();

                node.add_key_value("index", it.get_index());
                node.add_key_value("count", it->second);

                json_node &addrs_arr = node.add_array("addrs");
                const vogl_backtrace_addrs &addrs = it->first;

                for (uint32_t i = 0; i < addrs.m_num_addrs; i++)
                {
                    addrs_arr.add_value(to_hex_string(static_cast<uint64_t>(addrs.m_addrs[i])));
                }
            }
        }

        backtrace_hashmap.reset();
        get_backtrace_hashmap_mutex().unlock();

        if (backtrace_hashmap_size)
        {
            char_vec data;
            doc.serialize(data, true, 0, false);

            if (get_vogl_trace_writer().get_trace_archive()->add_buf_using_id(data.get_ptr(), data.size(), VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME).is_empty())
                vogl_error_printf("Failed adding serialized backtrace addrs to trace archive\n");
            else
                vogl_verbose_printf("Done writing backtrace addrs\n");
        }

        return true;
    }
#endif

//----------------------------------------------------------------------------------------------------------------------
// vogl_flush_compilerinfo_to_trace_file
//----------------------------------------------------------------------------------------------------------------------
static bool vogl_flush_compilerinfo_to_trace_file()
{
    scoped_mutex lock(get_vogl_trace_mutex());

    if (!get_vogl_trace_writer().is_opened() || (get_vogl_trace_writer().get_trace_archive() == NULL))
        return false;

    json_document doc;

    vogl_verbose_printf("Begin resolving compilerinfo to symbols\n");

    json_node *pRoot = doc.get_root();

    vogl::json_node &compiler_info_node = pRoot->add_array("compiler_info");
    compiler_info_node.add_key_value("time", __TIME__);
    compiler_info_node.add_key_value("date", __DATE__);
#ifdef __VERSION__
    compiler_info_node.add_key_value("version", __VERSION__);
#endif

    compiler_info_node.add_key_value("arch", (sizeof(void *) > 4) ? "64bit" : "32bit");

    char_vec data;
    doc.serialize(data, true, 0, false);

    if (get_vogl_trace_writer().get_trace_archive()->add_buf_using_id(data.get_ptr(), data.size(), VOGL_TRACE_ARCHIVE_COMPILER_INFO_FILENAME).is_empty())
        vogl_error_printf("Failed adding serialized compilerinfo to trace archive\n");
    else
        vogl_verbose_printf("Done resolving compilerinfo to symbols\n");

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_flush_machineinfo_to_trace_file
//----------------------------------------------------------------------------------------------------------------------
static bool vogl_flush_machineinfo_to_trace_file()
{
    scoped_mutex lock(get_vogl_trace_mutex());

    if (!get_vogl_trace_writer().is_opened() || (get_vogl_trace_writer().get_trace_archive() == NULL))
        return false;

    json_document doc;

    vogl_verbose_printf("Begin resolving machineinfo to symbols\n");

    json_node *pRoot = doc.get_root();

    #if VOGL_PLATFORM_SUPPORTS_BTRACE
        btrace_get_machine_info(pRoot);
    #endif

    char_vec data;
    doc.serialize(data, true, 0, false);

    if (get_vogl_trace_writer().get_trace_archive()->add_buf_using_id(data.get_ptr(), data.size(), VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME).is_empty())
        vogl_error_printf("Failed adding serialized machineinfo to trace archive\n");
    else
        vogl_verbose_printf("Done resolving machineinfo to symbols\n");

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_entrypoint_serializer::begin
//----------------------------------------------------------------------------------------------------------------------
inline bool vogl_entrypoint_serializer::begin(gl_entrypoint_id_t id, vogl_context *pContext)
{
    if (m_in_begin)
    {
        // Nothing good will come of this - the output will be fucked.
        vogl_error_printf("begin() call not matched with end() - something is very wrong!)\n");
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    m_in_begin = true;

    uint64_t thread_id = vogl_get_current_kernel_thread_id();
    uint64_t context_id = pContext ? reinterpret_cast<uint64_t>(pContext->get_context_handle()) : 0;

    m_packet.begin_construction(id, context_id, get_vogl_trace_writer().get_next_gl_call_counter(), thread_id, utils::RDTSC());

    #if VOGL_PLATFORM_SUPPORTS_BTRACE
        if (!g_backtrace_no_calls)
        {
            bool do_backtrace = g_backtrace_all_calls ||
                    vogl_is_make_current_entrypoint(id) ||
                    vogl_is_swap_buffers_entrypoint(id) ||
                    vogl_is_clear_entrypoint(id) ||
                    vogl_is_draw_entrypoint(id);
            if (do_backtrace && get_vogl_trace_writer().is_opened())
            {
                // Take a backtrace and store its hashtable index into the packet.
                m_packet.set_backtrace_hash_index(vogl_backtrace(1));
            }
        }
    #endif

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_in_null_mode
//----------------------------------------------------------------------------------------------------------------------
static inline bool vogl_is_in_null_mode()
{
    return g_null_mode;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_is_in_null_mode
//----------------------------------------------------------------------------------------------------------------------
static inline bool vogl_func_is_nulled(gl_entrypoint_id_t id)
{
    return vogl_is_in_null_mode() && g_vogl_entrypoint_descs[id].m_is_nullable;
}

//----------------------------------------------------------------------------------------------------------------------
// Custom array parameter size helper macros
// IMPORTANT: These helpers need to not crash or return weird shit if there is no context current, in case the client
// messes up and makes a GL call without an active context.
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_GL_ENUM(pname) get_gl_enums().get_pname_count(pname)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXChooseFBConfig_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXChooseVisual_attribList(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attribList)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXCreateWindow_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXCreatePixmap_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXCreatePbuffer_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXCreateContextAttribsARB_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXBindVideoDeviceNV_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXBindTexImageEXT_attrib_list(e, c, rt, r, nu, ne, a, p) vogl_determine_attrib_list_array_size(attrib_list)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetUniformLocation_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)name) + 1) : -1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetUniformLocationARB_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen(name) + 1) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBindAttribLocation_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)name) + 1) : -1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBindAttribLocationARB_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen(name) + 1) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetAttribLocationARB_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)name) + 1) : -1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetAttribLocation_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)name) + 1) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetShaderInfoLog_infoLog(e, c, rt, r, nu, ne, a, p) (length ? (*length + 1) : bufSize)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetInfoLogARB_infoLog(e, c, rt, r, nu, ne, a, p) (length ? (*length + 1) : maxLength)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glClearBufferData_data(e, c, rt, r, nu, ne, a, p) vogl_get_image_format_size_in_bytes(format, type)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glVertexPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glVertexPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glColorPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glColorPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glSecondaryColorPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexCoordPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexCoordPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glFogCoordPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glFogCoordPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glIndexPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glIndexPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glNormalPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glNormalPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glEdgeFlagPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glEdgeFlagPointerEXT_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glInterleavedArrays_pointer(e, c, rt, r, nu, ne, a, p) 0

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glVertexAttribPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glVertexAttribIPointer_pointer(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glVertexAttribPointerARB_pointer(e, c, rt, r, nu, ne, a, p) 0

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawRangeElements_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawRangeElementsBaseVertex_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawRangeElementsEXT_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawElements_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawElementsInstanced_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawElementsInstancedARB_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawElementsInstancedEXT_indices(e, c, rt, r, nu, ne, a, p) 0
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawElementsBaseVertex_indices(e, c, rt, r, nu, ne, a, p) 0

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glNamedBufferDataEXT_data(e, c, rt, r, nu, ne, a, p) size
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glNamedBufferSubDataEXT_data(e, c, rt, r, nu, ne, a, p) size

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMap1f_points(e, c, rt, r, nu, ne, a, p) vogl_determine_glMap1_size(target, stride, order)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMap1d_points(e, c, rt, r, nu, ne, a, p) vogl_determine_glMap1_size(target, stride, order)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMap2f_points(e, c, rt, r, nu, ne, a, p) vogl_determine_glMap2_size(target, ustride, uorder, vstride, vorder)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMap2d_points(e, c, rt, r, nu, ne, a, p) vogl_determine_glMap2_size(target, ustride, uorder, vstride, vorder)

// KHR_debug
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDebugMessageInsert_buf(e, c, rt, r, nu, ne, a, p) ((length < 0) ? (buf ? strlen((const char *)buf) : 0 ) : length)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDebugMessageInsertAMD_buf(e, c, rt, r, nu, ne, a, p) ((length < 0) ? (buf ? strlen((const char *)buf) : 0 ) : length)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDebugMessageInsertARB_buf(e, c, rt, r, nu, ne, a, p) ((length < 0) ? (buf ? strlen((const char *)buf) : 0 ) : length)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glPushDebugGroup_message(e, c, rt, r, nu, ne, a, p) ((length < 0) ? (message ? strlen((const char *)message) : 0 ) : length)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glObjectLabel_label(e, c, rt, r, nu, ne, a, p) ((length < 0) ? (label ? strlen((const char *)label) : 0 ) : length)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glObjectPtrLabel_label(e, c, rt, r, nu, ne, a, p) ((length < 0) ? (label ? strlen((const char *)label) : 0 ) : length)

//----------------------------------------------------------------------------------------------------------------------
// Texture/image API's array size helper macros
// TODO: For glTexImage3DEXT, glTexSubImage1DEXT, etc. - should these check the currently bound GL_PIXEL_UNPACK_BUFFER?
//----------------------------------------------------------------------------------------------------------------------
size_t vogl_calc_set_tex_image_serialize_size(vogl_context *pContext, GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth)
{
    if (pContext)
    {
        if (vogl_get_bound_gl_buffer(GL_PIXEL_UNPACK_BUFFER))
            return 0;
    }

    return vogl_get_image_size(format, type, width, height, depth);
}

size_t vogl_calc_get_tex_image_serialize_size(vogl_context *pContext, GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth)
{
    if (pContext)
    {
        if (vogl_get_bound_gl_buffer(GL_PIXEL_PACK_BUFFER))
            return 0;
    }

    return vogl_get_image_size(format, type, width, height, depth);
}

size_t vogl_calc_get_tex_target_serialize_size(vogl_context *pContext, GLenum target, GLint level, GLenum format, GLenum type)
{
    if (pContext)
    {
        if (vogl_get_bound_gl_buffer(GL_PIXEL_PACK_BUFFER))
            return 0;
    }

    return vogl_get_tex_target_image_size(target, level, format, type);
}

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexImage1D_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexImage2D_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexImage3D_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexImage3DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexSubImage1D_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexSubImage1DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexSubImage2D_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexSubImage2DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexSubImage3D_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTexSubImage3DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTextureImage3DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTextureImage2DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTextureImage1DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTextureSubImage3DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTextureSubImage2DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glTextureSubImage1DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMultiTexImage3DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMultiTexImage2DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMultiTexImage1DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMultiTexSubImage3DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, depth)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMultiTexSubImage2DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glMultiTexSubImage1DEXT_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDrawPixels_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glConvolutionFilter1D_image(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glConvolutionFilter2D_image(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, height, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glColorTable_table(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glColorSubTable_data(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, count, 1, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBitmap_size(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, GL_COLOR_INDEX, GL_BITMAP, width, height, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glPolygonStipple_mask(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, GL_COLOR_INDEX, GL_BITMAP, 32, 32, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetTexImage_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_get_tex_target_serialize_size(pContext, target, level, format, type)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glStringMarkerGREMEDY_string(e, c, rt, r, nu, ne, a, p) ((string) ? (!(len) ? (strlen(static_cast<const char *>(string)) + 1) : (len)) : 0)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glViewportArrayv_v(e, c, rt, r, nu, ne, a, p) (4 * (count))
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glScissorArrayv_v(e, c, rt, r, nu, ne, a, p) (4 * (count))
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glDepthRangeArrayv_v(e, c, rt, r, nu, ne, a, p) (2 * (count))

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetActiveAttrib_name(e, c, rt, r, nu, ne, a, p) ((name) ? (strlen((const char *)(name)) + 1) : -1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetActiveAttribARB_name(e, c, rt, r, nu, ne, a, p) ((name) ? (strlen((const char *)(name)) + 1) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glReadPixels_pixels(e, c, rt, r, nu, ne, a, p) vogl_calc_get_tex_image_serialize_size(pContext, format, type, width, height, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBindFragDataLocationIndexed_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)(name)) + 1) : -1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBindFragDataLocation_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)(name)) + 1) : -1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBindFragDataLocationEXT_name(e, c, rt, r, nu, ne, a, p) (name ? (strlen((const char *)(name)) + 1) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glBitmap_bitmap(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, GL_COLOR_INDEX, GL_BITMAP, width, height, 1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetActiveUniform_name(e, c, rt, r, nu, ne, a, p) ((name) ? ((length) ? (*(length) + 1) : (bufSize)) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetAttachedShaders_obj(e, c, rt, r, nu, ne, a, p) ((obj) ? ((count) ? *(count) : (maxCount)) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetProgramInfoLog_infoLog(e, c, rt, r, nu, ne, a, p) ((infoLog) ? ((length) ? (*length + 1) : (bufSize)) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetShaderSource_source(e, c, rt, r, nu, ne, a, p) ((source) ? ((length) ? (*(length) + 1) : (bufSize)) : -1)

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glCallLists_lists(e, c, rt, r, nu, ne, a, p) (vogl_get_gl_type_size(type) * (n))

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetDebugMessageLogARB_messageLog(e, c, rt, r, nu, ne, a, p) vogl_compute_message_log_size_in_bytes(count, lengths)
static GLsizei vogl_compute_message_log_size_in_bytes(GLuint count, const GLsizei *lengths)
{
    if (!lengths)
        return 0;
    GLsizei total_length = 0;
    for (uint32_t i = 0; i < count; i++)
        total_length += lengths[i];
    return total_length;
}

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glSeparableFilter2D_row(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, width, 1, 1)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glSeparableFilter2D_column(e, c, rt, r, nu, ne, a, p) vogl_calc_set_tex_image_serialize_size(pContext, format, type, height, 1, 1)

// TODO - These are all gets, so they aren't completely necessary for proper replaying because they have no side effects (except for possibly glError's).
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetUniformfv_params(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetUniformiv_params(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetPolygonStipple_mask(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetMapdv_v(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetMapfv_v(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetMapiv_v(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetColorTable_table(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetConvolutionFilter_image(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetSeparableFilter_row(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetSeparableFilter_column(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetSeparableFilter_span(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetHistogram_values(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetMinmax_values(e, c, rt, r, nu, ne, a, p) -1
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetCompressedTexImage_img(e, c, rt, r, nu, ne, a, p) -1

#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glClearBufferfv_value(e, c, rt, r, nu, ne, a, p) vogl_get_clearbuffer_array_size(buffer)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glClearBufferuiv_value(e, c, rt, r, nu, ne, a, p) vogl_get_clearbuffer_array_size(buffer)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glClearBufferiv_value(e, c, rt, r, nu, ne, a, p) vogl_get_clearbuffer_array_size(buffer)
static int vogl_get_clearbuffer_array_size(GLenum buffer)
{
    if ((buffer == GL_DEPTH) || (buffer == GL_STENCIL))
        return 1;
    else if (utils::is_in_set<GLenum, GLenum>(buffer, GL_COLOR, GL_FRONT, GL_BACK, GL_LEFT, GL_RIGHT, GL_FRONT_AND_BACK))
        return 4;

    vogl_warning_printf("Invalid value for buffer parameter passed to glClearBufferfv: 0x%04X\n", buffer);
    return -1;
}

#if 0
// TODO/investigate
(vogltrace) Debug:
glTexImage3DEXT
(vogltrace) Debug:
glColorTableSGI
(vogltrace) Debug:
glColorSubTableEXT
(vogltrace) Debug:
glColorTableEXT
(vogltrace) Debug:
glXChooseFBConfig
#endif

//----------------------------------------------------------------------------------------------------------------------
// Custom return parameter array size helper macros
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXGetFBConfigs(dpy, screen, nelements) (nelements ? *nelements : 1)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXChooseFBConfig(dpy, screen, attrib_list, nelements) (nelements ? *nelements : 1)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXChooseVisual(dpy, screen, nelements) 1
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXGetVisualFromFBConfig(dpy, config) 1
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXGetCurrentDisplay() 1
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXGetCurrentDisplayEXT() 1
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXEnumerateVideoCaptureDevicesNV(dpy, screen, nelements) (nelements ? *nelements : 1)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXEnumerateVideoDevicesNV(dpy, screen, nelements) (nelements ? *nelements : 1)

#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetString(name) (result ? (strlen(reinterpret_cast<const char *>(result)) + 1) : 0)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glGetStringi(name, idx) (result ? (strlen(reinterpret_cast<const char *>(result)) + 1) : 0)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXGetClientString(dpy, name) (result ? (strlen(reinterpret_cast<const char *>(result)) + 1) : 0)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXQueryExtensionsString(dpy, name) (result ? (strlen(reinterpret_cast<const char *>(result)) + 1) : 0)
#define DEF_FUNCTION_RETURN_PARAM_COMPUTE_ARRAY_SIZE_FUNC_glXQueryServerString(dpy, screen, name) (result ? (strlen(reinterpret_cast<const char *>(result)) + 1) : 0)

//----------------------------------------------------------------------------------------------------------------------
static void vogl_print_hex(const void *p, uint64_t size, uint64_t type_size)
{
    if (!p)
    {
        vogl_log_printf("<null>\n");
        return;
    }

    // If we're not doing full on --vogl_debug then limit the size of data we spew.
    bool limited = ((size > 15) && (console::get_output_level() < cMsgDebug));
    if (limited)
        size = 15;

    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(p);
    if ((type_size == 2) && (!(size & 1)))
    {
        int c = 0;
        vogl_log_printf("[ ");
        for (uint64_t i = 0; i < size; i += 2)
        {
            if (i)
                vogl_log_printf(", ");
            vogl_log_printf("0x%04X", *reinterpret_cast<const uint16_t *>(ptr + i));

            if ((++c & 7) == 7)
                vogl_log_printf("\n");
        }
    }
    else if ((type_size == 4) && (!(size & 3)))
    {
        int c = 0;
        vogl_log_printf("[ ");
        for (uint64_t i = 0; i < size; i += 4)
        {
            if (i)
                vogl_log_printf(", ");
            vogl_log_printf("0x%08X", *reinterpret_cast<const uint32_t *>(ptr + i));

            if ((++c & 7) == 7)
                vogl_log_printf("\n");
        }
    }
    else if ((type_size == 8) && (!(size & 7)))
    {
        int c = 0;
        vogl_log_printf("[ ");
        for (uint64_t i = 0; i < size; i += 8)
        {
            if (i)
                vogl_log_printf(", ");
            vogl_log_printf("0x%" PRIX64, *reinterpret_cast<const uint64_t *>(ptr + i));

            if ((++c & 7) == 7)
                vogl_log_printf("\n");
        }
    }
    else
    {
        int c = 0;
        if (size > 15)
            vogl_log_printf("\n");

        vogl_log_printf("[ ");
        for (uint64_t i = 0; i < size; i++)
        {
            if (i)
                vogl_log_printf(", ");
            vogl_log_printf("%02X", ptr[i]);

            if ((++c & 63) == 63)
                vogl_log_printf("\n");
        }
    }
    
    if (limited)
        vogl_log_printf(" ...");
    vogl_log_printf(" ]");
}

//----------------------------------------------------------------------------------------------------------------------
static void vogl_print_string(const char *pStr, uint64_t total_size)
{
    for (uint64_t i = 0; i < total_size; i++)
    {
        uint8_t c = reinterpret_cast<const uint8_t *>(pStr)[i];
        if (c == '\n')
            vogl_log_printf("\n");
        else
        {
            if ((c < 32) || (c > 127))
                c = '.';
            vogl_log_printf("%c", c);
        }

        if ((i & 511) == 511)
            vogl_log_printf(" \\\n");
    }
}

//----------------------------------------------------------------------------------------------------------------------
template <typename T>
static inline void vogl_dump_value_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pDesc, uint32_t param_index, const char *pParam_name, const char *pType, vogl_ctype_t type, const T &val)
{
    VOGL_NOTE_UNUSED(pContext);

    int size = sizeof(T);

    if (get_vogl_process_gl_ctypes()[type].m_size != size)
        vogl_error_printf("size mismatch on ctype %u\n", type);

    if (serializer.is_in_begin())
    {
        serializer.add_param(param_index, type, &val, sizeof(val));
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("%s: %s %s, ctype: %s, size: %i: ", pDesc, pType, pParam_name, get_vogl_process_gl_ctypes()[type].m_pName, size);

        if (Loki::TypeTraits<T>::isPointer)
        {
            vogl_log_printf("OPAQUE POINTER TYPE");
        }
        else if (size <= 0)
        {
            vogl_log_printf("OPAQUE TYPE");
        }
        else
        {
            vogl_print_hex(&val, size, size);

            if (((type == VOGL_GLFLOAT) || (type == VOGL_GLCLAMPF)) && (size == sizeof(float)))
            {
                float flt_val = *reinterpret_cast<const float *>(&val);
                vogl_log_printf(" %f", flt_val);
            }
            else if (((type == VOGL_GLDOUBLE) || (type == VOGL_GLCLAMPD)) && (size == sizeof(double)))
            {
                double dbl_val = *reinterpret_cast<const double *>(&val);
                vogl_log_printf(" %f", dbl_val);
            }
            else if ((type == VOGL_GLENUM) && (size == sizeof(int)))
            {
                GLenum enum_val = *reinterpret_cast<const GLenum *>(&val);
                const char *pName = get_gl_enums().find_name(enum_val);
                if (pName)
                    vogl_log_printf(" %s", pName);
            }
        }

        vogl_log_printf("\n");
    }
}

//----------------------------------------------------------------------------------------------------------------------
template <typename T>
static inline void vogl_dump_ptr_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pDesc, uint32_t param_index, const char *pParam_name, const char *pType, vogl_ctype_t type, const T &val)
{
    VOGL_NOTE_UNUSED(pContext);

    VOGL_ASSUME(Loki::TypeTraits<T>::isPointer);
    int size = sizeof(T);

    if (get_vogl_process_gl_ctypes()[type].m_size != size)
        vogl_error_printf("size mismatch on ctype %u\n", type);

    if (serializer.is_in_begin())
    {
        serializer.add_param(param_index, type, &val, sizeof(val));
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("%s: %s %s, ctype: %s, size: %i, ptr: 0x%" PRIX64 "\n", pDesc, pType, pParam_name, get_vogl_process_gl_ctypes()[type].m_pName, size, cast_val_to_uint64(val));
    }
}

//----------------------------------------------------------------------------------------------------------------------
template <class T>
static inline void vogl_dump_ref_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pDesc, uint32_t param_index, const char *pParam_name, const char *pType, vogl_ctype_t type, const T *pObj)
{
    VOGL_NOTE_UNUSED(pContext);

    if (get_vogl_process_gl_ctypes()[type].m_size != sizeof(const T *))
        vogl_error_printf("size mismatch on ctype %u\n", type);

    int obj_size = gl_ctype_sizeof<T>::size;

    vogl_ctype_t pointee_type = get_vogl_process_gl_ctypes()[type].m_pointee_ctype;
    if (pointee_type == VOGL_INVALID_CTYPE)
    {
        vogl_error_printf("Type %u doesn't have a pointee ctype\n", type);
        return;
    }

    if (get_vogl_process_gl_ctypes()[pointee_type].m_size != obj_size)
        vogl_error_printf("size mismatch on pointee ctype %u\n", type);

    if (serializer.is_in_begin())
    {
        serializer.add_param(param_index, type, &pObj, sizeof(pObj));

        if ((pObj) && (obj_size > 0))
        {
            serializer.add_ref_client_memory(param_index, pointee_type, pObj, obj_size);
        }
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("%s: %s %s, ptr: 0x%" PRIX64 ", ctype: %s, pointee_ctype: %s, pointee_size: %i: ", pDesc, pType, pParam_name, cast_val_to_uint64(pObj), get_vogl_process_gl_ctypes()[type].m_pName, get_vogl_process_gl_ctypes()[pointee_type].m_pName, obj_size);
        if (!pObj)
        {
            vogl_log_printf("NULL");
        }
        else if (obj_size <= 0)
        {
            vogl_log_printf("OPAQUE TYPE");
        }
        else
        {
            vogl_print_hex(pObj, obj_size, obj_size);
        }

        vogl_log_printf("\n");
    }
}

//----------------------------------------------------------------------------------------------------------------------
template <class T>
static inline void vogl_dump_array_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pDesc, uint32_t param_index, const char *pParam_name, const char *pType, vogl_ctype_t type, const T *pArray, int64_t size)
{
    VOGL_NOTE_UNUSED(pContext);

    int64_t obj_size = gl_ctype_sizeof<T>::size;
    int64_t total_size = obj_size * math::maximum<int64_t>(size, 0);

    vogl_ctype_t pointee_type = get_vogl_process_gl_ctypes()[type].m_pointee_ctype;
    if (((type == VOGL_CONST_VOID_PTR) || (type == VOGL_CONST_GLVOID_PTR) || (type == VOGL_GLVOID_PTR)) && (size > 0))
    {
        obj_size = 1;
        total_size = size;
    }
    else
    {
        if (pointee_type == VOGL_INVALID_CTYPE)
        {
            vogl_error_printf("Type %u doesn't have a pointee ctype\n", type);
            return;
        }

        if (get_vogl_process_gl_ctypes()[pointee_type].m_size != obj_size)
            vogl_error_printf("Size mismatch on ctype %u\n", type);
    }

    bool pointee_is_ptr = get_vogl_process_gl_ctypes()[pointee_type].m_is_pointer;

    if (serializer.is_in_begin())
    {
        serializer.add_param(param_index, type, &pArray, sizeof(pArray));

        if ((pArray) && (size > 0) && (obj_size > 0))
        {
            serializer.add_array_client_memory(param_index, pointee_type, size, pArray, total_size);
        }
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("%s: %s %s, ptr: 0x%" PRIX64 ", ctype: %s, pointee_ctype: %s, size: %" PRIi64 ", pointee_size: %" PRIi64 ", total size: %" PRIi64 ": ", pDesc, pType, pParam_name, cast_val_to_uint64(pArray), get_vogl_process_gl_ctypes()[type].m_pName, get_vogl_process_gl_ctypes()[pointee_type].m_pName,
                       size, obj_size, total_size);
        if (!pArray)
        {
            vogl_log_printf("NULL");
        }
        else if (size <= 0)
        {
            vogl_log_printf("UNKNOWN SIZE");
        }
        else if (obj_size <= 0)
        {
            vogl_log_printf("OPAQUE TYPE");
        }
        else
        {
            if (pointee_is_ptr)
            {
                vogl_log_printf("POINTEE IS POINTER: \n");
            }

            vogl_print_hex(pArray, total_size, obj_size);

            switch (pointee_type)
            {
                case VOGL_GLUBYTE:
                case VOGL_GLBYTE:
                case VOGL_GLCHAR:
                case VOGL_GLCHARARB:
                {
                    vogl_log_printf("\nAs string: \"");
                    vogl_print_string(*(const char **)&pArray, total_size);
                    vogl_log_printf("\"");
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        vogl_log_printf("\n");
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Return parameters
//----------------------------------------------------------------------------------------------------------------------
template <class T>
static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, const T &val)
{
    VOGL_NOTE_UNUSED(size);

    VOGL_ASSUME(!Loki::TypeTraits<T>::isPointer);

    vogl_dump_value_param(pContext, serializer, "RETURN_VALUE", VOGL_RETURN_PARAM_INDEX, "result", pType, type, val);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, char *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    size_t array_size = pPtr ? (strlen(reinterpret_cast<const char *>(pPtr)) + 1) : 0;
    vogl_dump_array_param(pContext, serializer, "RETURN_UCHAR_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr, array_size);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, const GLubyte *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    size_t array_size = pPtr ? (strlen(reinterpret_cast<const char *>(pPtr)) + 1) : 0;
    vogl_dump_array_param(pContext, serializer, "RETURN_GLUBYTE_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr, array_size);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, const char *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    size_t array_size = pPtr ? (strlen(pPtr) + 1) : 0;
    vogl_dump_array_param(pContext, serializer, "RETURN_CONST_CHAR_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr, array_size);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, void *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_VOID_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, unsigned int *pPtr)
{
    vogl_dump_array_param(pContext, serializer, "RETURN_UINT_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr, size);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, long unsigned int *pPtr)
{
    vogl_dump_array_param(pContext, serializer, "RETUURN_LUINT_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr, size);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, __GLsync *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_GLSYNC_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, XVisualInfo *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    vogl_dump_ref_param(pContext, serializer, "RETURN_XVISUALINFO_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, GLXFBConfig *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    vogl_dump_ref_param(pContext, serializer, "RETURN_GLXFBCONFIG_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, GLXContext pPtr)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_GLXCONTEXT", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, _XDisplay *pPtr)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_XDISPLAY_PTR", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pPtr);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, CGLContextObj context)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_CGLCONTEXTOBJ", VOGL_RETURN_PARAM_INDEX, "result", pType, type, context);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, CGLPBufferObj pbuffer)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_CGLPBUFFEROBJ", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pbuffer);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, CGLPixelFormatObj pixelFormat)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_CGLPIXELFORMATOBJ", VOGL_RETURN_PARAM_INDEX, "result", pType, type, pixelFormat);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, CGLShareGroupObj shareGroup)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_CGLSHAREGROUPOBJ", VOGL_RETURN_PARAM_INDEX, "result", pType, type, shareGroup);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, HDC hdc)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_HDC", VOGL_RETURN_PARAM_INDEX, "result", pType, type, hdc);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, HGLRC hglrc)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_HGLRC", VOGL_RETURN_PARAM_INDEX, "result", pType, type, hglrc);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, PROC proc)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_PROC", VOGL_RETURN_PARAM_INDEX, "result", pType, type, proc);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, HPBUFFERARB hpbufferarb)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_HPBUFFERARB", VOGL_RETURN_PARAM_INDEX, "result", pType, type, hpbufferarb);
}

static inline void vogl_dump_return_param(vogl_context *pContext, vogl_entrypoint_serializer &serializer, const char *pType, vogl_ctype_t type, int64_t size, HPBUFFEREXT hpbufferext)
{
    VOGL_NOTE_UNUSED(size);

    // opaque data
    vogl_dump_ptr_param(pContext, serializer, "RETURN_HPBUFFEREXT", VOGL_RETURN_PARAM_INDEX, "result", pType, type, hpbufferext);
}



//----------------------------------------------------------------------------------------------------------------------
// vogl_should_serialize_call
//----------------------------------------------------------------------------------------------------------------------
static inline bool vogl_should_serialize_call(gl_entrypoint_id_t func, vogl_context *pContext)
{
    bool is_in_display_list = pContext && pContext->is_composing_display_list();
    bool is_listable = g_vogl_entrypoint_descs[func].m_is_listable;
    bool is_whitelisted = g_vogl_entrypoint_descs[func].m_whitelisted_for_displaylists;

    if ((is_in_display_list) && (is_listable) && (!is_whitelisted))
    {
        vogl_error_printf("Called GL func %s is not currently supported in display lists! The replay will diverge.\n", g_vogl_entrypoint_descs[func].m_pName);
    }

    // When we're writing a trace we ALWAYS want to serialize, even if the func is not listable (so we can at least process the trace, etc.)
    if (get_vogl_trace_writer().is_opened())
        return true;

    return is_in_display_list && is_whitelisted;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_write_packet_to_trace
//----------------------------------------------------------------------------------------------------------------------
static inline void vogl_write_packet_to_trace(vogl_trace_packet &packet)
{
    if (!get_vogl_trace_writer().is_opened())
        return;

    scoped_mutex lock(get_vogl_trace_mutex());

    // The trace got closed on another thread while we where serializing - this is OK I guess.
    // This can happen when control+c is pressed.
    if (get_vogl_trace_writer().is_opened())
    {
        bool success = get_vogl_trace_writer().write_packet(packet);

        if (success)
        {
            if ((g_flush_files_after_each_call) ||
                ((g_flush_files_after_each_swap) && (packet.get_entrypoint_id() == VOGL_ENTRYPOINT_glXSwapBuffers)))
            {
                get_vogl_trace_writer().flush();

                if (console::get_log_stream())
                    console::get_log_stream()->flush();
            }
        }
        else
        {
            vogl_error_printf("Failed writing to trace file! Exiting app.\n");

            // TODO: Add common termination handler func, or just stop writing to the trace? or call abort()
            exit(EXIT_FAILURE);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Declare gl/glx internal wrapper functions, all start with "vogl_" (so we can safely get the address of our internal
// wrappers, avoiding global symbol naming conflicts that I started to see on test apps when I started building with -fPIC)
//----------------------------------------------------------------------------------------------------------------------
// func begin
#define DEF_PROTO_EXPORTED(ret, name, args, params)    \
    static ret VOGL_API_CALLCONV VOGL_GLUER(vogl_, name) args           \
    {                                                  \
        if (vogl_func_is_nulled(VOGL_ENTRYPOINT_##name)) \
        {                                              \
            return (ret)0;                             \
        }                                              \
        ret result;
#define DEF_PROTO_EXPORTED_VOID(ret, name, args, params) \
    static ret VOGL_API_CALLCONV VOGL_GLUER(vogl_, name) args             \
    {                                                    \
        if (vogl_func_is_nulled(VOGL_ENTRYPOINT_##name))   \
            return;
#define DEF_PROTO_INTERNAL(ret, name, args, params)    \
    static ret VOGL_API_CALLCONV VOGL_GLUER(vogl_, name) args           \
    {                                                  \
        if (vogl_func_is_nulled(VOGL_ENTRYPOINT_##name)) \
        {                                              \
            return (ret)0;                             \
        }                                              \
        ret result;
#define DEF_PROTO_INTERNAL_VOID(ret, name, args, params) \
    static ret VOGL_API_CALLCONV VOGL_GLUER(vogl_, name) args             \
    {                                                    \
        if (vogl_func_is_nulled(VOGL_ENTRYPOINT_##name))   \
            return;

#define DEF_FUNCTION_BEGIN(exported, category, ret, ret_type_enum, num_params, name, args, params) exported(ret, name, args, params)
#define DEF_FUNCTION_BEGIN_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params) exported(ret, name, args, params)

// TODO: When serialization is off, the array size determination code is still active (which could involve useless GL calls and in the worst case could trigger errors when/if this code is buggy).
// A completely different code path through the wrapper should be used.

// func init (after the optional custom function prolog)
#define VOGL_MASTER_FUNCTION_PROLOG(name, params)                                                                                                                                                                                                  \
    if (g_dump_gl_calls_flag)                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                             \
        vogl_log_printf("** BEGIN %s 0x%" PRIX64 "" PRIX64 "\n", #name, vogl_get_current_kernel_thread_id());                                                                                                                                                           \
    }                                                                                                                                                                                                                                             \
    vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_##name);                                                                                                                                                              \
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)                                                                                                                                                                      \
    {                                                                                                                                                                                                                                             \
        vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName); \
        return g_vogl_actual_gl_entrypoints.m_##name params;                                                                                                                                                                                       \
    }                                                                                                                                                                                                                                             \
    vogl_context *pContext = pTLS_data->m_pContext;                                                                                                                                                                                                \
    vogl_entrypoint_serializer &trace_serializer = pTLS_data->m_serializer;                                                                                                                                                                        \
    if (vogl_should_serialize_call(VOGL_ENTRYPOINT_##name, pContext))                                                                                                                                                                               \
    {                                                                                                                                                                                                                                             \
        if (!trace_serializer.begin(VOGL_ENTRYPOINT_##name, pContext))                                                                                                                                                                             \
        {                                                                                                                                                                                                                                         \
            vogl_warning_printf("Reentrant wrapper call detected!\n");                                                                                                                                                   \
            return g_vogl_actual_gl_entrypoints.m_##name params;                                                                                                                                                                                   \
        }                                                                                                                                                                                                                                         \
    }

#define VOGL_MASTER_FUNCTION_PROLOG_VOID(name, params)                                                                                                                                                                                             \
    if (g_dump_gl_calls_flag)                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                             \
        vogl_log_printf("** BEGIN %s 0x%" PRIX64 "\n", #name, vogl_get_current_kernel_thread_id());                                                                                                                                                           \
    }                                                                                                                                                                                                                                             \
    vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_##name);                                                                                                                                                              \
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)                                                                                                                                                                      \
    {                                                                                                                                                                                                                                             \
        vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName); \
        g_vogl_actual_gl_entrypoints.m_##name params;                                                                                                                                                                                              \
        return;                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                             \
    vogl_context *pContext = pTLS_data->m_pContext;                                                                                                                                                                                                \
    vogl_entrypoint_serializer &trace_serializer = pTLS_data->m_serializer;                                                                                                                                                                        \
    if (vogl_should_serialize_call(VOGL_ENTRYPOINT_##name, pContext))                                                                                                                                                                               \
    {                                                                                                                                                                                                                                             \
        if (!trace_serializer.begin(VOGL_ENTRYPOINT_##name, pContext))                                                                                                                                                                             \
        {                                                                                                                                                                                                                                         \
            vogl_warning_printf("Reentrant wrapper call detected!\n");                                                                                                                                                   \
            g_vogl_actual_gl_entrypoints.m_##name params;                                                                                                                                                                                          \
            return;                                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                         \
    }

#define DEF_FUNCTION_INIT(exported, category, ret, ret_type_enum, num_params, name, args, params) VOGL_MASTER_FUNCTION_PROLOG(name, params)
#define DEF_FUNCTION_INIT_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params) VOGL_MASTER_FUNCTION_PROLOG_VOID(name, params)

// func params
#define DEF_FUNCTION_INPUT_VALUE_PARAM(idx, spectype, type, type_enum, param) vogl_dump_value_param(pContext, trace_serializer, "INPUT_VALUE", idx, #param, #type, type_enum, param);
#define DEF_FUNCTION_INPUT_REFERENCE_PARAM(idx, spectype, type, type_enum, param) vogl_dump_ref_param(pContext, trace_serializer, "INPUT_REF", idx, #param, #type, type_enum, param);
#define DEF_FUNCTION_INPUT_ARRAY_PARAM(idx, spectype, type, type_enum, param, size) vogl_dump_array_param(pContext, trace_serializer, "INPUT_ARRAY", idx, #param, #type, type_enum, param, size);

#define DEF_FUNCTION_OUTPUT_REFERENCE_PARAM(idx, spectype, type, type_enum, param) vogl_dump_ref_param(pContext, trace_serializer, "OUTPUT_REF", idx, #param, #type, type_enum, param);
#define DEF_FUNCTION_OUTPUT_ARRAY_PARAM(idx, spectype, type, type_enum, param, size) vogl_dump_array_param(pContext, trace_serializer, "OUTPUT_ARRAY", idx, #param, #type, type_enum, param, size);

#define DEF_FUNCTION_RETURN_PARAM(spectype, type, type_enum, size) vogl_dump_return_param(pContext, trace_serializer, #type, type_enum, size, result);

#define DEF_FUNCTION_CALL_GL(exported, category, ret, ret_type_enum, num_params, name, args, params) \
    if (trace_serializer.is_in_begin())                                                              \
        trace_serializer.set_gl_begin_rdtsc(utils::RDTSC());                                         \
    result = g_vogl_actual_gl_entrypoints.m_##name params;                                            \
    if (trace_serializer.is_in_begin())                                                              \
        trace_serializer.set_gl_end_rdtsc(utils::RDTSC());

#define DEF_FUNCTION_CALL_GL_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params) \
    if (trace_serializer.is_in_begin())                                                                   \
        trace_serializer.set_gl_begin_rdtsc(utils::RDTSC());                                              \
    g_vogl_actual_gl_entrypoints.m_##name params;                                                          \
    if (trace_serializer.is_in_begin())                                                                   \
        trace_serializer.set_gl_end_rdtsc(utils::RDTSC());

// Be careful modifying these END macros. They must be compatible with the logic in the glXSwapBuffers GL end prolog!
// func end (after the optional custom function epilog)
#define DEF_FUNCTION_END(exported, category, ret, ret_type_enum, num_params, name, args, params)                \
    if (g_dump_gl_calls_flag)                                                                                   \
    {                                                                                                           \
        vogl_log_printf("** END %s res=%s 0x%" PRIX64 "\n", #name, #ret, cast_val_to_uint64(result));                  \
    }                                                                                                           \
    if (trace_serializer.is_in_begin())                                                                         \
    {                                                                                                           \
        trace_serializer.end();                                                                                 \
        vogl_write_packet_to_trace(trace_serializer.get_packet());                                               \
        if (pContext)                                                                                           \
            pContext->add_packet_to_current_display_list(VOGL_ENTRYPOINT_##name, trace_serializer.get_packet()); \
    }                                                                                                           \
    return result;                                                                                              \
    }

#define DEF_FUNCTION_END_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params)           \
    if (g_dump_gl_calls_flag)                                                                                   \
    {                                                                                                           \
        vogl_log_printf("** END %s\n", #name);                                                               \
    }                                                                                                           \
    if (trace_serializer.is_in_begin())                                                                         \
    {                                                                                                           \
        trace_serializer.end();                                                                                 \
        vogl_write_packet_to_trace(trace_serializer.get_packet());                                               \
        if (pContext)                                                                                           \
            pContext->add_packet_to_current_display_list(VOGL_ENTRYPOINT_##name, trace_serializer.get_packet()); \
    }                                                                                                           \
    }

//----------------------------------------------------------------------------------------------------------------------
// gl/glx override functions
//----------------------------------------------------------------------------------------------------------------------
template <typename GLFuncType, typename ProcNameType>
static GLFuncType vogl_get_proc_address_helper_return_wrapper(GLFuncType (GLAPIENTRY *realGetProcAddressFunc)(ProcNameType), ProcNameType procName)
{
    if (!procName)
        return NULL;

    if (g_dump_gl_calls_flag)
    {
        vogl_printf("GetProcAddress: \"%s\"\n", procName);
    }

    if (!realGetProcAddressFunc)
        return NULL;

    // FIXME: We need to make this per-context on OSX (under Linux it doesn't matter, because all GL funcs are statically exported anyway as symbols by the SO)
    GLFuncType pActual_entrypoint = (realGetProcAddressFunc)(procName);
    if (!pActual_entrypoint)
        return NULL;

    for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; ++i)
    {
        if (vogl_strcmp(reinterpret_cast<const char *>(procName), g_vogl_entrypoint_descs[i].m_pName) == 0)
        {
            if (g_vogl_entrypoint_descs[i].m_pWrapper_func)
            {
                if (!g_vogl_entrypoint_descs[i].m_is_whitelisted)
                {
                    // TODO: Only print this message once
                    vogl_warning_printf("App has queried the address of non-whitelisted GL func %s (this will only be a problem if this func. is actually called, and will reported during tracing and at exit)\n", g_vogl_entrypoint_descs[i].m_pName);
                }

                return reinterpret_cast<GLFuncType>(g_vogl_entrypoint_descs[i].m_pWrapper_func);
            }
            else
                break;
        }
    }

    return pActual_entrypoint;
}

//----------------------------------------------------------------------------------------------------------------------
// Custom function handler implementations
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_HANDLER_glInternalTraceCommandRAD(exported, category, ret, ret_type_enum, num_params, name, args, params)
static void vogl_glInternalTraceCommandRAD(GLuint cmd, GLuint size, const GLubyte *data)
{
    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    if (get_vogl_trace_writer().is_opened())
    {
        vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glInternalTraceCommandRAD, NULL);

        uint64_t cur_rdtsc = utils::RDTSC();
        serializer.set_gl_begin_end_rdtsc(cur_rdtsc, cur_rdtsc + 1);

        serializer.add_param(0, VOGL_GLUINT, &cmd, sizeof(cmd));
        serializer.add_param(1, VOGL_GLUINT, &size, sizeof(size));
        serializer.add_param(2, VOGL_CONST_GLUBYTE_PTR, &data, sizeof(data));

        switch (cmd)
        {
            case cITCRDemarcation:
            {
                break;
            }
            case cITCRKeyValueMap:
            {
                if ((size == sizeof(key_value_map)) && (data))
                {
                    // Directly jam in the key value map, so it properly serializes as readable JSON.
                    serializer.get_key_value_map() = *reinterpret_cast<const key_value_map *>(data);
                }
                else
                {
                    vogl_error_printf("data pointer is NULL, or invalid key_value_map size %u\n", size);
                    VOGL_ASSERT_ALWAYS;
                }
                break;
            }
            default:
            {
                vogl_error_printf("Unknown trace command type %u\n", cmd);
                VOGL_ASSERT_ALWAYS;
                break;
            }
        }

        serializer.end();
        vogl_write_packet_to_trace(serializer.get_packet());
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }
}

//----------------------------------------------------------------------------------------------------------------------
// TODO: Merge this with vogl_glXGetProcAddressARB to avoid duplicated code
#define DEF_FUNCTION_CUSTOM_HANDLER_glXGetProcAddress(exported, category, ret, ret_type_enum, num_params, name, args, params)
static __GLXextFuncPtr vogl_glXGetProcAddress(const GLubyte *procName)
{
    uint64_t begin_rdtsc = utils::RDTSC();

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXGetProcAddress);
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
    {
        vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
        return GL_ENTRYPOINT(glXGetProcAddress)(procName);
    }

    uint64_t gl_begin_rdtsc = utils::RDTSC();
    __GLXextFuncPtr ptr = vogl_get_proc_address_helper_return_wrapper(GL_ENTRYPOINT(glXGetProcAddress), procName);
    uint64_t gl_end_rdtsc = utils::RDTSC();

    if (get_vogl_trace_writer().is_opened())
    {
        vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXGetProcAddress, get_context_manager().get_current(true));
        serializer.set_begin_rdtsc(begin_rdtsc);
        serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
        serializer.add_param(0, VOGL_CONST_GLUBYTE_PTR, &procName, sizeof(procName));
        if (procName)
        {
            size_t len = strlen(reinterpret_cast<const char *>(procName)) + 1;
            serializer.add_array_client_memory(0, VOGL_GLUBYTE, len, procName, len);
        }
        serializer.add_return_param(VOGL_GLXEXTFUNCPTR, &ptr, sizeof(ptr));
        serializer.end();
        vogl_write_packet_to_trace(serializer.get_packet());
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    return ptr;
}

//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_HANDLER_glXGetProcAddressARB(exported, category, ret, ret_type_enum, num_params, name, args, params)
static __GLXextFuncPtr vogl_glXGetProcAddressARB(const GLubyte *procName)
{
    uint64_t begin_rdtsc = utils::RDTSC();

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXGetProcAddressARB);
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
    {
        vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
        return GL_ENTRYPOINT(glXGetProcAddressARB)(procName);
    }

    uint64_t gl_begin_rdtsc = utils::RDTSC();
    __GLXextFuncPtr ptr = vogl_get_proc_address_helper_return_wrapper(GL_ENTRYPOINT(glXGetProcAddress), procName);
    uint64_t gl_end_rdtsc = utils::RDTSC();

    if (get_vogl_trace_writer().is_opened())
    {
        vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXGetProcAddressARB, get_context_manager().get_current(true));
        serializer.set_begin_rdtsc(begin_rdtsc);
        serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
        serializer.add_param(0, VOGL_CONST_GLUBYTE_PTR, &procName, sizeof(procName));
        if (procName)
        {
            size_t len = strlen(reinterpret_cast<const char *>(procName)) + 1;
            serializer.add_array_client_memory(0, VOGL_GLUBYTE, len, procName, len);
        }
        serializer.add_return_param(VOGL_GLXEXTFUNCPTR, &ptr, sizeof(ptr));
        serializer.end();
        vogl_write_packet_to_trace(serializer.get_packet());
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    return ptr;
}

//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_HANDLER_wglGetProcAddress(exported, category, ret, ret_type_enum, num_params, name, args, params)
static PROC vogl_wglGetProcAddress(LPCSTR lpszProc)
{
    uint64_t begin_rdtsc = utils::RDTSC();

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_wglGetProcAddress);
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
    {
        vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
        return GL_ENTRYPOINT(wglGetProcAddress)(lpszProc);
    }

    uint64_t gl_begin_rdtsc = utils::RDTSC();
    PROC ptr = vogl_get_proc_address_helper_return_wrapper(GL_ENTRYPOINT(wglGetProcAddress), lpszProc);
    uint64_t gl_end_rdtsc = utils::RDTSC();

    if (get_vogl_trace_writer().is_opened())
    {
        vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_wglGetProcAddress, get_context_manager().get_current(true));
        serializer.set_begin_rdtsc(begin_rdtsc);
        serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
        serializer.add_param(0, VOGL_LPCSTR, &lpszProc, sizeof(lpszProc));
        if (lpszProc)
        {
            size_t len = strlen(reinterpret_cast<const char *>(lpszProc)) + 1;
            serializer.add_array_client_memory(0, VOGL_CHAR, len, lpszProc, len);
        }
        serializer.add_return_param(VOGL_PROC, &ptr, sizeof(ptr));
        serializer.end();
        vogl_write_packet_to_trace(serializer.get_packet());
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    return ptr;
}


//----------------------------------------------------------------------------------------------------------------------
#if (VOGL_PLATFORM_HAS_GLX)

    struct _vogl_xlib_trap_state
    {
	int (* old_error_handler) (Display *, XErrorEvent *);
	int trapped_error_code;
	vogl_xlib_trap_state_t *old_state;
    };

    static int vogl_xlib_error_handler (Display *xdpy, XErrorEvent *error)
    {
	VOGL_ASSERT(g_vogl_xlib_trap_state);

	g_vogl_xlib_trap_state->trapped_error_code = error->error_code;

	return 0;
    }

    static void vogl_xlib_trap_errors (vogl_xlib_trap_state_t *state)
    {
	state->trapped_error_code = 0;
	state->old_error_handler = XSetErrorHandler(vogl_xlib_error_handler);

	state->old_state = g_vogl_xlib_trap_state;
	g_vogl_xlib_trap_state = state;
    }

    static int vogl_xlib_untrap_errors (vogl_xlib_trap_state_t *state)
    {
	VOGL_ASSERT(g_vogl_xlib_trap_state == state);

	XSetErrorHandler(state->old_error_handler);

	g_vogl_xlib_trap_state = state->old_state;

	return state->trapped_error_code;
    }

    static bool vogl_query_glx_drawable_size(const Display *dpy, GLXDrawable drawable, unsigned int *width, unsigned int *height)
    {
	Window root = 0;
	int x = 0, y = 0;
	unsigned int border_width = 0, depth = 0;
	vogl_xlib_trap_state_t state;

	// It turns out that there is some inconsistency between drivers over
	// whether it's ok to pass a vanilla X drawable to glXQueryDrawable or pass
	// a GLXDrawable from glXCreateWindow to XGetGeometry. glXQueryDrawable has
	// been also been seen to be inconsistent with mesa and an indirect context
	// where it may return a size of 0 with no error.
	//
	// See: https://bugs.freedesktop.org/show_bug.cgi?id=54080

	vogl_xlib_trap_errors (&state);
	XGetGeometry(const_cast<Display *>(dpy), drawable, &root, &x, &y, width, height, &border_width, &depth);
	if (vogl_xlib_untrap_errors (&state) == 0)
	    return true;

	vogl_xlib_trap_errors (&state);
	GL_ENTRYPOINT(glXQueryDrawable(const_cast<Display *>(dpy), drawable, GLX_WIDTH, width));
	GL_ENTRYPOINT(glXQueryDrawable(const_cast<Display *>(dpy), drawable, GLX_HEIGHT, height));
	return vogl_xlib_untrap_errors (&state) == 0;
    }

    static void vogl_add_make_current_key_value_fields(const Display *dpy, GLXDrawable drawable, Bool result, vogl_context *pVOGL_context, vogl_entrypoint_serializer &serializer)
    {
        if ((result) && (pVOGL_context))
        {
            vogl_scoped_gl_error_absorber gl_error_absorber(pVOGL_context);
            VOGL_NOTE_UNUSED(gl_error_absorber);

            GLint cur_viewport[4];
            GL_ENTRYPOINT(glGetIntegerv)(GL_VIEWPORT, cur_viewport);

            serializer.add_key_value(string_hash("viewport_x"), cur_viewport[0]);
            serializer.add_key_value(string_hash("viewport_y"), cur_viewport[1]);
            serializer.add_key_value(string_hash("viewport_width"), cur_viewport[2]);
            serializer.add_key_value(string_hash("viewport_height"), cur_viewport[3]);
        }

        if ((pVOGL_context) && (pVOGL_context->get_window_width() < 0))
        {
            bool valid_dims = false;
            unsigned int width = 1, 
                         height = 1;

            if ((dpy) && (drawable) && (result))
            {
		valid_dims = vogl_query_glx_drawable_size (dpy, drawable, &width, &height);
            }

            if (valid_dims)
            {
                pVOGL_context->set_window_dimensions(width, height);

                serializer.add_key_value(string_hash("win_width"), width);
                serializer.add_key_value(string_hash("win_height"), height);

                if (g_dump_gl_calls_flag)
                {
                    vogl_log_printf("** Current window dimensions: %ix%i\n", width, height);
                }                
            }
        }
    }

#elif (VOGL_PLATFORM_HAS_CGL)
    static void vogl_add_make_current_key_value_fields(CGLContextObj cglContext, vogl_context *pVOGL_context, vogl_entrypoint_serializer &serializer)
    {
    	VOGL_ASSERT(!"UNIMPLEMENTED vogl_add_make_current_key_value_fields");
    }


#elif (VOGL_PLATFORM_HAS_WGL)

    bool get_dimensions_from_dc(unsigned int* out_width, unsigned int* out_height, HDC hdc)
    {
        VOGL_ASSERT(out_width);
        VOGL_ASSERT(out_height);

        if (!hdc)
            return false;

        HWND wnd = WindowFromDC(hdc);
        if (!wnd)
            return false;

        RECT dims = {};
        if (GetClientRect(wnd, &dims))
        {
            (*out_width) = dims.right - dims.left;
            (*out_height) = dims.bottom - dims.top;

            return (*out_width) > 0 
                && (*out_height) > 0;
        }

        return false;
    }

    static void vogl_add_make_current_key_value_fields(HDC hdc, HGLRC hglrc, Bool result, vogl_context *pVOGL_context, vogl_entrypoint_serializer &serializer)
    {
        if ((result) && (pVOGL_context))
        {
            vogl_scoped_gl_error_absorber gl_error_absorber(pVOGL_context);
            VOGL_NOTE_UNUSED(gl_error_absorber);

            GLint cur_viewport[4];
            GL_ENTRYPOINT(glGetIntegerv)(GL_VIEWPORT, cur_viewport);

            serializer.add_key_value(string_hash("viewport_x"), cur_viewport[0]);
            serializer.add_key_value(string_hash("viewport_y"), cur_viewport[1]);
            serializer.add_key_value(string_hash("viewport_width"), cur_viewport[2]);
            serializer.add_key_value(string_hash("viewport_height"), cur_viewport[3]);
        }

        if ((pVOGL_context) && (pVOGL_context->get_window_width() < 0))
        {
            unsigned int width = 1, 
                         height = 1;

            if (hglrc && result && get_dimensions_from_dc(&width, &height, hdc))
            {
                pVOGL_context->set_window_dimensions(width, height);

                serializer.add_key_value(string_hash("win_width"), width);
                serializer.add_key_value(string_hash("win_height"), height);

                if (g_dump_gl_calls_flag)
                {
                    vogl_log_printf("** Current window dimensions: %ix%i\n", width, height);
                }
            }
        }
    }

#else
    #error "impl vogl_add_make_current_key_value_fields on this platform"
#endif


#if 0
//----------------------------------------------------------------------------------------------------------------------
// HACK HACK - for NS2 experiment in AMD
#define DEF_FUNCTION_CUSTOM_HANDLER_glTexStorage2D(exported, category, ret, ret_type_enum, num_params, name, args, params)
static void vogl_glTexStorage2D(	GLenum target,
                                GLsizei levels,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height)
{
	GL_ENTRYPOINT(glTexParameteri)(target, GL_TEXTURE_BASE_LEVEL, 0);
	GL_ENTRYPOINT(glTexParameteri)(target, GL_TEXTURE_MAX_LEVEL, levels - 1);

	if (target == GL_TEXTURE_2D)
	{
		for (uint32_t i = 0; i < levels; i++)
		{
			uint32_t w = math::maximum<uint32_t>(width >> i, 1);
			uint32_t h = math::maximum<uint32_t>(height >> i, 1);
			vogl::vector<uint8_t> pixels(w * h * 4);
			GL_ENTRYPOINT(glTexImage2D)(GL_TEXTURE_2D, i, internalformat, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels.get_ptr());
		}
	}
}
#endif

#if VOGL_PLATFORM_HAS_GLX
    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXMakeCurrent(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static Bool vogl_glXMakeCurrent(const Display *dpy, GLXDrawable drawable, GLXContext context)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXMakeCurrent);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXMakeCurrent)(dpy, drawable, context);
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXMakeCurrent TID: 0x%" PRIX64 " dpy: 0x%" PRIX64 " drawable: 0x%" PRIX64 " context: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(drawable), cast_val_to_uint64(context));
        }

        vogl_context_manager &context_manager = get_context_manager();
        vogl_context *pCur_context = context_manager.get_current(true);

        vogl_context *pNew_context = context ? context_manager.lookup_vogl_context(context) : NULL;
        if ((context) && (!pNew_context))
        {
            vogl_error_printf("Unknown context handle 0x%" PRIX64 "\n", cast_val_to_uint64(context));
        }

        if (pCur_context)
        {
            if (pCur_context != pNew_context)
                pCur_context->on_release_current_prolog();
            else
                vogl_warning_printf("Context 0x%" PRIX64 " is already current (redundant call)\n", cast_val_to_uint64(context));
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        Bool result = GL_ENTRYPOINT(glXMakeCurrent)(dpy, drawable, context);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if ((result) && (pCur_context != pNew_context))
        {
            if (pCur_context)
                context_manager.release_current();

            if (context)
            {
                vogl_context *p = context_manager.make_current(context);
                VOGL_ASSERT(p == pNew_context);
                VOGL_NOTE_UNUSED(p);

                pNew_context->set_display(dpy);
                pNew_context->set_drawable(drawable);
                pNew_context->set_read_drawable(drawable);
            }
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXMakeCurrent result: %i\n", result);
        }

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXMakeCurrent, pCur_context);
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_GLXDRAWABLE, &drawable, sizeof(drawable));
            serializer.add_param(2, VOGL_GLXCONTEXT, &context, sizeof(context));
            serializer.add_return_param(VOGL_BOOL, &result, sizeof(result));

            vogl_add_make_current_key_value_fields(dpy, drawable, result, pNew_context, serializer);

            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        static bool s_added_atexit = false;
        if (!s_added_atexit)
        {
	        // atexit routines are called in the reverse order in which they were registered. We would like
		    //  our vogl_atexit() routine to be called before anything else (Ie C++ destructors, etc.) So we
		    //  put atexit at the end of vogl_global_init() and another at the end of glXMakeCurrent.
            atexit(vogl_atexit);
            s_added_atexit = true;
        }
    
        return result;
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXMakeContextCurrent(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static Bool vogl_glXMakeContextCurrent(const Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext context)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXMakeContextCurrent);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXMakeContextCurrent)(dpy, draw, read, context);
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXMakeContextCurrent TID: 0x%" PRIX64 " dpy: 0x%" PRIX64 " draw: 0x%" PRIX64 " read: 0x%" PRIX64 " context: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(draw), cast_val_to_uint64(read), cast_val_to_uint64(context));
        }

        vogl_context_manager &context_manager = get_context_manager();
        vogl_context *pCur_context = context_manager.get_current(true);

        vogl_context *pNew_context = context ? context_manager.lookup_vogl_context(context) : NULL;
        if ((context) && (!pNew_context))
        {
            vogl_error_printf("Unknown coontext handle 0x%" PRIX64 "\n", cast_val_to_uint64(context));
        }

        if (pCur_context)
        {
            if (pCur_context != pNew_context)
                pCur_context->on_release_current_prolog();
            else
                vogl_warning_printf("Context 0x%" PRIX64 " is already current (redundant call)\n", cast_val_to_uint64(context));
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        Bool result = GL_ENTRYPOINT(glXMakeContextCurrent)(dpy, draw, read, context);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (result)
        {
            if ((pCur_context != pNew_context) && pCur_context)
                context_manager.release_current();

            if (context)
            {
                vogl_context *p = context_manager.make_current(context);
                VOGL_ASSERT(p == pNew_context);
                VOGL_NOTE_UNUSED(p);

                pNew_context->set_display(dpy);
                pNew_context->set_drawable(draw);
                pNew_context->set_read_drawable(read);
            }
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXMakeCurrent result: %i\n", result);
        }

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXMakeContextCurrent, pCur_context);
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_GLXDRAWABLE, &draw, sizeof(draw));
            serializer.add_param(2, VOGL_GLXDRAWABLE, &read, sizeof(read));
            serializer.add_param(3, VOGL_GLXCONTEXT, &context, sizeof(context));
            serializer.add_return_param(VOGL_BOOL, &result, sizeof(result));

            vogl_add_make_current_key_value_fields(dpy, draw, result, pNew_context, serializer);

            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        return result;
    }
#endif

#if VOGL_PLATFORM_HAS_WGL
    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_wglMakeCurrent(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static Bool vogl_wglMakeCurrent(HDC hdc, HGLRC newContext)
    {
        
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_wglMakeCurrent);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(wglMakeCurrent)(hdc, newContext);
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** wglMakeCurrent TID: 0x%" PRIX64 " hdc: 0x%" PRIX64 " newContext: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(hdc), cast_val_to_uint64(newContext));
        }

        vogl_context_manager &context_manager = get_context_manager();
        vogl_context *pCur_context = context_manager.get_current(true);

        vogl_context *pNew_context = newContext ? context_manager.lookup_vogl_context(newContext) : NULL;
        if ((newContext) && (!pNew_context))
        {
            vogl_error_printf("Unknown context handle 0x%" PRIX64 "\n", cast_val_to_uint64(newContext));
        }

        if (pCur_context)
        {
            if (pCur_context != pNew_context)
                pCur_context->on_release_current_prolog();
            else
                vogl_warning_printf("Context (newContext) 0x%" PRIX64 " is already current (redundant call)\n", cast_val_to_uint64(newContext));
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        Bool result = GL_ENTRYPOINT(wglMakeCurrent)(hdc, newContext);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if ((result) && (pCur_context != pNew_context))
        {
            if (pCur_context)
                context_manager.release_current();

            if (newContext)
            {
                vogl_context *p = context_manager.make_current(newContext);
                VOGL_ASSERT(p == pNew_context);
                VOGL_NOTE_UNUSED(p);

                pNew_context->set_hdc(hdc);
            }
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** wglMakeCurrent result: %i\n", result);
        }

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_wglMakeCurrent, pCur_context);
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_HDC, &hdc, sizeof(hdc));
            serializer.add_param(1, VOGL_HGLRC, &newContext, sizeof(newContext));
            serializer.add_return_param(VOGL_BOOL, &result, sizeof(result));

            vogl_add_make_current_key_value_fields(hdc, newContext, result, pNew_context, serializer);

            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        static bool s_added_atexit = false;
        if (!s_added_atexit)
        {
            // atexit routines are called in the reverse order in which they were registered. We would like
            //  our vogl_atexit() routine to be called before anything else (Ie C++ destructors, etc.) So we
            //  put atexit at the end of vogl_global_init() and another at the end of glXMakeCurrent.
            atexit(vogl_atexit);
            s_added_atexit = true;
        }

        return result;
    }
#endif

//----------------------------------------------------------------------------------------------------------------------
static bool vogl_screen_capture_callback(uint32_t width, uint32_t height, uint32_t pitch, size_t size, GLenum pixel_format, GLenum pixel_type, const void *pImage, void *pOpaque, uint64_t frame_index)
{
    vogl_context *pContext = static_cast<vogl_context *>(pOpaque);

    if ((!pContext) || (!width) || (!height))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if ((pixel_format != GL_RGB) || (pixel_type != GL_UNSIGNED_BYTE) || (pitch != width * 3))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if (g_command_line_params().get_value_as_bool("vogl_dump_png_screenshots"))
    {
        size_t png_size = 0;
        void *pPNG_data = tdefl_write_image_to_png_file_in_memory_ex(pImage, width, height, 3, &png_size, 1, true);

        dynamic_string screenshot_filename(cVarArg, "%s_%08" PRIx64 "_%08" PRIu64 ".png", g_command_line_params().get_value_as_string("vogl_screenshot_prefix", 0, "screenshot").get_ptr(), cast_val_to_uint64(pContext->get_context_handle()), cast_val_to_uint64(frame_index));
        if (!file_utils::write_buf_to_file(screenshot_filename.get_ptr(), pPNG_data, png_size))
        {
            vogl_error_printf("Failed writing PNG screenshot to file %s\n", screenshot_filename.get_ptr());
        }

        mz_free(pPNG_data);
    }
    else if (g_command_line_params().get_value_as_bool("vogl_dump_jpeg_screenshots"))
    {
        int jpeg_quality = g_command_line_params().get_value_as_int("vogl_jpeg_quality", 0, 80, 1, 100);

        long unsigned int jpeg_size = 0;
        unsigned char *pJPEG_data = NULL;

        tjhandle _jpegCompressor = tjInitCompress();

        int status = tjCompress2(_jpegCompressor, (unsigned char *)pImage, width, pitch, height, TJPF_RGB,
                                 &pJPEG_data, &jpeg_size, TJSAMP_422, jpeg_quality,
                                 TJFLAG_FASTDCT | TJFLAG_BOTTOMUP);

        tjDestroy(_jpegCompressor);

        if (status == 0)
        {
            dynamic_string screenshot_filename(cVarArg, "%s_%08" PRIx64 "_%08" PRIu64 ".jpg",
                g_command_line_params().get_value_as_string("vogl_screenshot_prefix", 0, "screenshot").get_ptr(),
                cast_val_to_uint64(pContext->get_context_handle()),
                cast_val_to_uint64(frame_index));
            if (!file_utils::write_buf_to_file(screenshot_filename.get_ptr(), pJPEG_data, jpeg_size))
            {
                vogl_error_printf("Failed writing JPEG screenshot to file %s\n", screenshot_filename.get_ptr());
            }
        }

        tjFree(pJPEG_data);
    }

    if (g_command_line_params().get_value_as_bool("vogl_dump_backbuffer_hashes") ||
            g_command_line_params().get_value_as_bool("vogl_hash_backbuffer"))
    {
        uint64_t backbuffer_crc64;

        if (g_command_line_params().get_value_as_bool("vogl_sum_hashing"))
        {
            backbuffer_crc64 = calc_sum64(static_cast<const uint8_t *>(pImage), size);
        }
        else
        {
            backbuffer_crc64 = calc_crc64(CRC64_INIT, static_cast<const uint8_t *>(pImage), size);
        }

        vogl_verbose_printf("Frame %" PRIu64 " hash: 0x%016" PRIX64 "\n", cast_val_to_uint64(frame_index), backbuffer_crc64);

        dynamic_string backbuffer_hash_file;
        if (g_command_line_params().get_value_as_string(backbuffer_hash_file, "vogl_dump_backbuffer_hashes"))
        {
            FILE *pFile = vogl_fopen(backbuffer_hash_file.get_ptr(), "a");
            if (!pFile)
                vogl_error_printf("Failed writing to backbuffer hash file %s\n", backbuffer_hash_file.get_ptr());
            else
            {
                vogl_fprintf(pFile, "0x%016" PRIX64 "\n", backbuffer_crc64);
                vogl_fclose(pFile);
            }
        }
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
static void vogl_tick_screen_capture(vogl_context *pVOGL_context)
{
    if (!pVOGL_context)
        return;

    uint32_t width = pVOGL_context->get_window_width();
    uint32_t height = pVOGL_context->get_window_height();
    if ((!width) || (!height))
        return;

    bool grab_backbuffer = g_command_line_params().get_value_as_bool("vogl_dump_backbuffer_hashes") ||
            g_command_line_params().get_value_as_bool("vogl_hash_backbuffer") ||
            g_command_line_params().get_value_as_bool("vogl_dump_jpeg_screenshots") ||
            g_command_line_params().get_value_as_bool("vogl_dump_png_screenshots");
    if (!grab_backbuffer)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pVOGL_context);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    if (!pVOGL_context->get_framebuffer_capturer().is_initialized())
    {
        if (!pVOGL_context->get_framebuffer_capturer().init(2, vogl_screen_capture_callback, pVOGL_context, GL_RGB, GL_UNSIGNED_BYTE))
        {
            vogl_error_printf("Failed initializing context's vogl_framebuffer_capturer!\n");
            return;
        }
    }

    if (!pVOGL_context->get_framebuffer_capturer().capture(width, height, 0, GL_BACK, pVOGL_context->get_frame_index()))
    {
        vogl_error_printf("vogl_framebuffer_capturer::capture() failed!\n");
        return;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_end_capture
// Important: This could be called at signal time!
//----------------------------------------------------------------------------------------------------------------------
static void vogl_end_capture(bool inside_signal_handler)
{
    VOGL_FUNC_TRACER

    vogl_debug_printf("vogl_end_capture(inside_signal_handler: %d)\n", inside_signal_handler);

    vogl_context_manager &context_manager = get_context_manager();
    context_manager.lock();

    vogl_context *pVOGL_context = context_manager.get_current(false);

    if (pVOGL_context)
    {
        pVOGL_context->get_framebuffer_capturer().flush();

        if (inside_signal_handler)
        {
            vogl_thread_local_data *pTLS_data = vogl_get_thread_local_data();
            if ((pTLS_data) && (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID))
            {
                vogl_error_printf("Signal handler called while the tracer was calling OpenGL in func %s!\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);

                if (get_vogl_trace_writer().is_opened())
                {
                    vogl_entrypoint_serializer &trace_serializer = pTLS_data->m_serializer;
                    if (trace_serializer.is_in_begin())
                    {
                        vogl_error_printf("Attempting to flush final trace packet\n");

                        trace_serializer.end();

                        vogl_write_packet_to_trace(trace_serializer.get_packet());
                    }
                }
            }
        }
    }

    context_manager.unlock();

    scoped_mutex lock(get_vogl_trace_mutex());

    if (get_vogl_trace_writer().is_opened())
    {
        dynamic_string filename(get_vogl_trace_writer().get_filename());
        
        vogl_flush_compilerinfo_to_trace_file();
        vogl_flush_machineinfo_to_trace_file();
        #if VOGL_PLATFORM_SUPPORTS_BTRACE
            vogl_flush_backtrace_to_trace_file();
        #endif

        if (!get_vogl_trace_writer().close())
        {
            vogl_error_printf("Failed closing trace file!\n");

            if (g_vogl_pCapture_status_callback)
                (*g_vogl_pCapture_status_callback)(NULL, g_vogl_pCapture_status_opaque);
        }
        else
        {
            if (g_vogl_pCapture_status_callback)
                (*g_vogl_pCapture_status_callback)(filename.get_ptr(), g_vogl_pCapture_status_opaque);
        }

        if (g_pJSON_node_pool)
        {
            uint64_t total_bytes_freed = static_cast<uint64_t>(g_pJSON_node_pool->free_unused_blocks());
            vogl_debug_printf("Freed %" PRIu64 " bytes from the JSON object pool (%" PRIu64 " bytes remaining)\n", total_bytes_freed, static_cast<uint64_t>(g_pJSON_node_pool->get_total_heap_bytes()));
        }
    }

    g_vogl_frames_remaining_to_capture = 0;

    g_vogl_pCapture_status_callback = NULL;
    g_vogl_pCapture_status_opaque = NULL;
}

//----------------------------------------------------------------------------------------------------------------------
static void vogl_capture_status_callback_func(const char *pFilename, void *pOpaque)
{
    // only for testing
    VOGL_ASSERT(pOpaque == (void *)1);
    vogl_message_printf("Filename \"%s\", opaque: %p\n", pFilename ? pFilename : "<null>", pOpaque);
}

//----------------------------------------------------------------------------------------------------------------------
static bool vogl_check_for_trigger_file(const char *pBase_name, dynamic_string &filename)
{
    filename = pBase_name;
    if (!file_utils::does_file_exist(filename.get_ptr()))
    {
        dynamic_string path_to_check(g_command_line_params().get_value_as_string_or_empty("vogl_tracepath"));
        if (path_to_check.is_empty())
            path_to_check = file_utils::get_pathname(g_command_line_params().get_value_as_string_or_empty("vogl_tracefile").get_ptr());

        if (path_to_check.is_empty())
            return false;

        file_utils::combine_path(filename, path_to_check.get_ptr(), pBase_name);
        if (!file_utils::does_file_exist(filename.get_ptr()))
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
static void vogl_check_for_capture_stop_file()
{
    if (!vogl_is_capturing())
        return;

    dynamic_string stop_filename;
    if (!vogl_check_for_trigger_file(VOGL_STOP_CAPTURE_FILENAME, stop_filename))
        return;

    file_utils::delete_file(stop_filename.get_ptr());

    vogl_stop_capturing();
}

//----------------------------------------------------------------------------------------------------------------------
static void vogl_check_for_capture_trigger_file()
{
    {
        scoped_mutex lock(get_vogl_trace_mutex());
        if ((g_vogl_frames_remaining_to_capture) || (get_vogl_trace_writer().is_opened()))
            return;
    }

    dynamic_string trigger_filename;
    if (!vogl_check_for_trigger_file(VOGL_TRIGGER_CAPTURE_FILENAME, trigger_filename))
        return;

    vogl_sleep(100);

    // Lamely try to protected against racing - a better method is probably inotify: http://www.thegeekstuff.com/2010/04/inotify-c-program-example/
    for (;;)
    {
        uint64_t size;
        file_utils::get_file_size(trigger_filename.get_ptr(), size);

        vogl_sleep(100);

        uint64_t size1;
        file_utils::get_file_size(trigger_filename.get_ptr(), size1);

        if (size == size1)
            break;

        vogl_sleep(250);
    }

    uint32_t total_frames = 1;
    dynamic_string path;
    dynamic_string base_name;

    dynamic_string_array lines;
    if (!file_utils::read_text_file(trigger_filename.get_ptr(), lines, file_utils::cRTFTrim | file_utils::cRTFIgnoreEmptyLines))
    {
        vogl_error_printf("Failed reading from trigger file \"%s\", using default parameters to trigger a capture.\n", trigger_filename.get_ptr());
        lines.clear();
    }

    if (lines.size() >= 1)
    {
        if (lines[0] == "all")
            total_frames = cUINT32_MAX;
        else
            total_frames = string_to_uint(lines[0].get_ptr(), 1);

        if (lines.size() >= 2)
        {
            path = lines[1];

            if (lines.size() >= 3)
                base_name = lines[2];
        }
    }

    file_utils::delete_file(trigger_filename.get_ptr());

    bool success = vogl_capture_on_next_swap(total_frames, path.get_ptr(), base_name.get_ptr(), vogl_capture_status_callback_func, (void *)1);

    if (!success)
        vogl_error_printf("Failed enabling capture mode\n");
    else
        vogl_message_printf("Successfully enabled capture mode, will capture up to %u frame(s), override path \"%s\", override base_name \"%s\"\n",
                            total_frames, path.get_ptr(), base_name.get_ptr());
}

#if (VOGL_PLATFORM_HAS_GLX)
    //----------------------------------------------------------------------------------------------------------------------
    static vogl_gl_state_snapshot *vogl_snapshot_state(const Display *dpy, GLXDrawable drawable, vogl_context *pCur_context)
    {
        VOGL_NOTE_UNUSED(drawable);

        // pCur_context may be NULL!

        if (!GL_ENTRYPOINT(glXGetCurrentContext) || !GL_ENTRYPOINT(glXGetCurrentDisplay) ||
            !GL_ENTRYPOINT(glXGetCurrentDrawable) || !GL_ENTRYPOINT(glXGetCurrentReadDrawable))
            return NULL;

        timed_scope ts(VOGL_FUNCTION_INFO_CSTR);

        vogl_context_manager &context_manager = get_context_manager();
        context_manager.lock();

        if (vogl_check_gl_error())
            vogl_error_printf("A GL error occured sometime before this function was called\n");

        const Display *orig_dpy = GL_ENTRYPOINT(glXGetCurrentDisplay)();
        GLXDrawable orig_drawable = GL_ENTRYPOINT(glXGetCurrentDrawable)();
        GLXDrawable orig_read_drawable = GL_ENTRYPOINT(glXGetCurrentReadDrawable)();
        GLXContext orig_context = GL_ENTRYPOINT(glXGetCurrentContext)();

        if (!orig_dpy)
            orig_dpy = dpy;

        vogl_check_gl_error();

        vogl_gl_state_snapshot *pSnapshot = vogl_new(vogl_gl_state_snapshot);

        const context_map &contexts = context_manager.get_context_map();

        // TODO: Find a better way of determining which window dimensions to use.
        // No context is current, let's just find the biggest window.
        uint32_t win_width = 0;
        uint32_t win_height = 0;
        if (pCur_context)
        {
            win_width = pCur_context->get_window_width();
            win_height = pCur_context->get_window_height();
        }
        else
        {
            context_map::const_iterator it;
            for (it = contexts.begin(); it != contexts.end(); ++it)
            {
                vogl_context *pVOGL_context = it->second;
                if ((pVOGL_context->get_window_width() > (int)win_width) || (pVOGL_context->get_window_height() > (int)win_height))
                {
                    win_width = pVOGL_context->get_window_width();
                    win_height = pVOGL_context->get_window_height();
                }
            }
        }

        vogl_message_printf("Beginning capture: width %u, height %u\n", win_width, win_height);

        if (!pSnapshot->begin_capture(win_width, win_height, pCur_context ? (uint64_t)pCur_context->get_context_handle() : 0, 0, get_vogl_trace_writer().get_cur_gl_call_counter(), true))
        {
            vogl_error_printf("Failed beginning capture\n");

            vogl_delete(pSnapshot);
            pSnapshot = NULL;

            get_context_manager().unlock();

            return NULL;
        }

        vogl_printf("Capturing %u context(s)\n", contexts.size());

        context_map::const_iterator it;
        for (it = contexts.begin(); it != contexts.end(); ++it)
        {
            CONTEXT_TYPE gl_context = it->first;
            vogl_context *pVOGL_context = it->second;

            if (pVOGL_context->get_deleted_flag())
            {
                vogl_error_printf("Context 0x%" PRIX64 " has been deleted but is the root of a sharelist group - this scenario is not yet supported for state snapshotting.\n", cast_val_to_uint64(gl_context));
                break;
            }

            if (pVOGL_context->get_has_been_made_current())
            {
                if (!pVOGL_context->MakeCurrent(gl_context))
                {
                    vogl_error_printf("Failed switching to trace context 0x%" PRIX64 ". (This context may be current on another thread.) Capture failed!\n", cast_val_to_uint64(gl_context));
                    break;
                }

                if (pVOGL_context->get_total_mapped_buffers())
                {
                    vogl_error_printf("Trace context 0x%" PRIX64 " has %u buffer(s) mapped across a call to glXSwapBuffers(), which is not currently supported!\n", cast_val_to_uint64(gl_context), pVOGL_context->get_total_mapped_buffers());
                    break;
                }

                if (pVOGL_context->is_composing_display_list())
                {
                    vogl_error_printf("Trace context 0x%" PRIX64 " is currently composing a display list across a call to glXSwapBuffers()!\n", cast_val_to_uint64(gl_context));
                    break;
                }

                if (pVOGL_context->get_in_gl_begin())
                {
                    vogl_error_printf("Trace context 0x%" PRIX64 " is currently in a glBegin() across a call to glXSwapBuffers(), which is not supported!\n", cast_val_to_uint64(gl_context));
                    break;
                }
            }

            vogl_capture_context_params &capture_context_params = pVOGL_context->get_capture_context_params();

            if (!pSnapshot->capture_context(pVOGL_context->get_context_desc(), pVOGL_context->get_context_info(), pVOGL_context->get_handle_remapper(), capture_context_params))
            {
                vogl_error_printf("Failed capturing trace context 0x%" PRIX64 ", capture failed\n", cast_val_to_uint64(gl_context));
                break;
            }
        }

        if ((it == contexts.end()) && (pSnapshot->end_capture()))
        {
            vogl_printf("Capture succeeded\n");
        }
        else
        {
            vogl_printf("Capture failed\n");

            vogl_delete(pSnapshot);
            pSnapshot = NULL;
        }

        if (orig_dpy)
        {
            GL_ENTRYPOINT(glXMakeContextCurrent)(orig_dpy, orig_drawable, orig_read_drawable, orig_context);
        }

        vogl_check_gl_error();

        get_context_manager().unlock();

        return pSnapshot;
    }

    //----------------------------------------------------------------------------------------------------------------------
    static bool vogl_write_snapshot_to_trace(const char *pTrace_filename, const Display *dpy, GLXDrawable drawable, vogl_context *pVOGL_context)
    {
        VOGL_FUNC_TRACER
        vogl_message_printf("Snapshot begin %s\n", pTrace_filename);

        // pVOGL_context may be NULL here!

        vogl_unique_ptr<vogl_gl_state_snapshot> pSnapshot(vogl_snapshot_state(dpy, drawable, pVOGL_context));
        if (!pSnapshot.get())
        {
            vogl_error_printf("Failed snapshotting GL/GLX state!\n");

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        pSnapshot->set_frame_index(0);

        if (!get_vogl_trace_writer().open(pTrace_filename, NULL, true, false))
        {
            vogl_error_printf("Failed creating trace file \"%s\"!\n", pTrace_filename);

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        vogl_archive_blob_manager &trace_archive = *get_vogl_trace_writer().get_trace_archive();

        vogl_message_printf("Serializing snapshot data to JSON document\n");

        // TODO: This can take a lot of memory, probably better off to split the snapshot into separate smaller binary json or whatever files stored directly in the archive.
        json_document doc;
        if (!pSnapshot->serialize(*doc.get_root(), trace_archive, &get_vogl_process_gl_ctypes()))
        {
            vogl_error_printf("Failed serializing GL state snapshot!\n");

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        pSnapshot.reset();

        vogl_message_printf("Serializing JSON document to UBJ\n");

        uint8_vec binary_snapshot_data;
        vogl::vector<char> snapshot_data;

        // TODO: This can take a lot of memory
        doc.binary_serialize(binary_snapshot_data);

        vogl_message_printf("Compressing UBJ data and adding to trace archive\n");

        dynamic_string binary_snapshot_id(trace_archive.add_buf_compute_unique_id(binary_snapshot_data.get_ptr(), binary_snapshot_data.size(), "binary_state_snapshot", VOGL_BINARY_JSON_EXTENSION));
        if (binary_snapshot_id.is_empty())
        {
            vogl_error_printf("Failed adding binary GL snapshot file to output blob manager!\n");

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        binary_snapshot_data.clear();

        snapshot_data.clear();

    #if 0
        // TODO: This requires too much temp memory!
        doc.serialize(snapshot_data, true, 0, false);

        dynamic_string snapshot_id(trace_archive.add_buf_compute_unique_id(snapshot_data.get_ptr(), snapshot_data.size(), "state_snapshot", VOGL_TEXT_JSON_EXTENSION));
        if (snapshot_id.is_empty())
        {
            vogl_error_printf("Failed adding binary GL snapshot file to output blob manager!\n");
            get_vogl_trace_writer().deinit();
            return false;
        }
    #endif

        key_value_map snapshot_key_value_map;
        snapshot_key_value_map.insert("command_type", "state_snapshot");
        snapshot_key_value_map.insert("binary_id", binary_snapshot_id);

    #if 0
        // TODO: This requires too much temp memory!
        snapshot_key_value_map.insert("id", snapshot_id);
    #endif

        vogl_ctypes &trace_gl_ctypes = get_vogl_process_gl_ctypes();
        if (!vogl_write_glInternalTraceCommandRAD(get_vogl_trace_writer().get_stream(), &trace_gl_ctypes, cITCRKeyValueMap, sizeof(snapshot_key_value_map), reinterpret_cast<const GLubyte *>(&snapshot_key_value_map)))
        {
            VOGL_FUNC_TRACER
                vogl_end_capture();
            vogl_error_printf("Failed writing to trace file!\n");
            return false;
        }

        if (!vogl_write_glInternalTraceCommandRAD(get_vogl_trace_writer().get_stream(), &trace_gl_ctypes, cITCRDemarcation, 0, NULL))
        {
            VOGL_FUNC_TRACER
                vogl_end_capture();
            vogl_error_printf("Failed writing to trace file!\n");
            return false;
        }

        doc.clear(false);

        if (g_pJSON_node_pool)
        {
            uint64_t total_bytes_freed = static_cast<uint64_t>(g_pJSON_node_pool->free_unused_blocks());
            vogl_debug_printf("Freed %" PRIu64 " bytes from the JSON object pool (%" PRIu64 " bytes remaining)\n", total_bytes_freed, static_cast<uint64_t>(g_pJSON_node_pool->get_total_heap_bytes()));
        }

        vogl_message_printf("Snapshot complete\n");

        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------
    //  vogl_tick_capture
    //----------------------------------------------------------------------------------------------------------------------
    static void vogl_tick_capture(const Display *dpy, GLXDrawable drawable, vogl_context *pVOGL_context)
    {
        VOGL_FUNC_TRACER

        // pVOGL_context may be NULL here!

        vogl_check_for_capture_stop_file();
        vogl_check_for_capture_trigger_file();

        scoped_mutex lock(get_vogl_trace_mutex());

        if ((g_vogl_total_frames_to_capture) && (!g_vogl_frames_remaining_to_capture))
        {
            g_vogl_frames_remaining_to_capture = g_vogl_total_frames_to_capture;
            g_vogl_total_frames_to_capture = 0;
        }

        if (g_vogl_stop_capturing)
        {
            g_vogl_stop_capturing = false;

            if (get_vogl_trace_writer().is_opened())
            {
                vogl_end_capture();
                return;
            }
        }

        if (!g_vogl_frames_remaining_to_capture)
            return;

        if (!get_vogl_trace_writer().is_opened())
        {
            dynamic_string trace_path(g_command_line_params().get_value_as_string_or_empty("vogl_tracepath"));
            if (trace_path.is_empty())
                trace_path = "/tmp";
            if (!get_vogl_intercept_data().capture_path.is_empty())
                trace_path = get_vogl_intercept_data().capture_path;

            time_t t = time(NULL);
            struct tm ltm = *localtime(&t);

            dynamic_string trace_basename("capture");
            if (!get_vogl_intercept_data().capture_basename.is_empty())
                trace_basename = get_vogl_intercept_data().capture_basename;

            dynamic_string filename(cVarArg, "%s_%04d_%02d_%02d_%02d_%02d_%02d.bin", trace_basename.get_ptr(),
                                    ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

            dynamic_string full_trace_filename;
            file_utils::combine_path(full_trace_filename, trace_path.get_ptr(), filename.get_ptr());

            if (g_vogl_frames_remaining_to_capture == cUINT32_MAX)
            {
                vogl_message_printf("Initiating capture of all remaining frames to file \"%s\"\n", full_trace_filename.get_ptr());
            }
            else
            {
                vogl_message_printf("Initiating capture of up to %u frame(s) to file \"%s\"\n",
                                    g_vogl_frames_remaining_to_capture, full_trace_filename.get_ptr());
            }

            if (!vogl_write_snapshot_to_trace(full_trace_filename.get_ptr(), dpy, drawable, pVOGL_context))
            {
                file_utils::delete_file(full_trace_filename.get_ptr());
                vogl_error_printf("Failed creating GL state snapshot, closing and deleting trace file\n");
            }
        }
        else
        {
            if (g_vogl_frames_remaining_to_capture != cUINT32_MAX)
                g_vogl_frames_remaining_to_capture--;

            // See if we should stop capturing.
            if (!g_vogl_frames_remaining_to_capture)
                vogl_end_capture();
        }
    }

    //----------------------------------------------------------------------------------------------------------------------
    //  vogl_glXSwapBuffersGLFuncProlog
    //----------------------------------------------------------------------------------------------------------------------
    static inline void vogl_glXSwapBuffersGLFuncProlog(const Display *dpy, GLXDrawable drawable, vogl_context *pVOGL_context, vogl_entrypoint_serializer &trace_serializer)
    {
        // pVOGL_context may be NULL here!

        if (!GL_ENTRYPOINT(glXGetCurrentContext) || !GL_ENTRYPOINT(glXGetCurrentDisplay) ||
            !GL_ENTRYPOINT(glXGetCurrentDrawable) || !GL_ENTRYPOINT(glXGetCurrentReadDrawable))
        {
            return;
        }

        bool override_current_context = false;

        if ((!pVOGL_context) || ((pVOGL_context->get_display() != dpy) || (pVOGL_context->get_drawable() != drawable)))
        {
            // NVidia/AMD closed source don't seem to care if a context is current when glXSwapBuffer()'s is called. But Mesa doesn't like it.
            vogl_warning_printf_once("No context is current, or the current context's display/drawable don't match the provided display/drawable. Will try to find the first context which matches the provided params, but this may not work reliably.\n");

            pVOGL_context = get_context_manager().find_context(dpy, drawable);

            if (!pVOGL_context)
            {
                vogl_error_printf("Unable to determine which GL context is associated with the indicated drawable/display! Will not be able to take a screen capture, or record context related information to the trace!\n");
                return;
            }
            else if (pVOGL_context->get_current_thread() != 0)
            {
                vogl_error_printf("The GL context which matches the provided display/drawable is already current on another thread!\n");
                return;
            }

            override_current_context = true;
        }

        GLXContext orig_context = 0;
        const Display *orig_dpy = NULL;
        GLXDrawable orig_drawable = 0;
        GLXDrawable orig_read_drawable = 0;
        if (override_current_context)
        {
            orig_context = GL_ENTRYPOINT(glXGetCurrentContext)();
            orig_dpy = GL_ENTRYPOINT(glXGetCurrentDisplay)();
            orig_drawable = GL_ENTRYPOINT(glXGetCurrentDrawable)();
            orig_read_drawable = GL_ENTRYPOINT(glXGetCurrentReadDrawable)();

            if (!orig_dpy)
                orig_dpy = dpy;

            GL_ENTRYPOINT(glXMakeCurrent)(pVOGL_context->get_display(), pVOGL_context->get_drawable(), pVOGL_context->get_context_handle());
        }

        unsigned int width = 0, height = 0;
        bool dims_valid = vogl_query_glx_drawable_size(dpy, drawable, &width, &height);

        if (dims_valid)
        {
            pVOGL_context->set_window_dimensions(width, height);

            vogl_tick_screen_capture(pVOGL_context);
        }
        else
        {
            vogl_warning_printf("XGetGeometry() call failed!\n");
        }

        if (trace_serializer.is_in_begin())
        {
            trace_serializer.add_key_value(string_hash("win_width"), pVOGL_context->get_window_width());
            trace_serializer.add_key_value(string_hash("win_height"), pVOGL_context->get_window_height());
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** Current window dimensions: %ix%i\n", pVOGL_context->get_window_width(), pVOGL_context->get_window_height());
        }

        pVOGL_context->inc_frame_index();

        if ((override_current_context) && (orig_dpy))
        {
            GL_ENTRYPOINT(glXMakeContextCurrent)(orig_dpy, orig_drawable, orig_read_drawable, orig_context);
        }
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXSwapBuffers(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static void vogl_glXSwapBuffers(const Display *dpy, GLXDrawable drawable)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXSwapBuffers);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXSwapBuffers)(dpy, drawable);
        }

        // Use a local serializer because the call to glXSwapBuffer()'s will make GL calls if something like the Steam Overlay is active.
        vogl_entrypoint_serializer serializer;

        if (get_vogl_trace_writer().is_opened())
        {
            serializer.begin(VOGL_ENTRYPOINT_glXSwapBuffers, get_context_manager().get_current(true));
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_GLXDRAWABLE, &drawable, sizeof(drawable));
        }

        vogl_glXSwapBuffersGLFuncProlog(dpy, drawable, pTLS_data->m_pContext, serializer);

        uint64_t gl_begin_rdtsc = utils::RDTSC();

        // Call the driver directly, bypassing our GL/GLX wrappers which set m_calling_driver_entrypoint_id. The Steam Overlay may call us back!
        DIRECT_GL_ENTRYPOINT(glXSwapBuffers)(dpy, drawable);

        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (get_vogl_trace_writer().is_opened())
        {
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        vogl_tick_capture(dpy, drawable, pTLS_data->m_pContext);

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXSwapBuffers TID: 0x%" PRIX64 " Display: 0x%" PRIX64 " drawable: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(drawable));
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        if (g_command_line_params().has_key("vogl_exit_after_x_frames") && (pTLS_data->m_pContext))
        {
            uint64_t max_num_frames = g_command_line_params().get_value_as_uint64("vogl_exit_after_x_frames");
            uint64_t cur_num_frames = pTLS_data->m_pContext->get_frame_index();

            if (cur_num_frames >= max_num_frames)
            {
                vogl_message_printf("Number of frames specified by --vogl_exit_after_x_frames param reached, forcing trace to close and calling exit()\n");

                vogl_end_capture();

                // Exit the app
                exit(0);
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXCreateContextAttribsARB(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static GLXContext vogl_glXCreateContextAttribsARB(const Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXCreateContextAttribsARB);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXCreateContextAttribsARB)(dpy, config, share_context, direct, attrib_list);
        }

        vogl_context_attribs context_attribs;

        if (g_command_line_params().get_value_as_bool("vogl_force_debug_context"))
        {
            vogl_warning_printf("Forcing debug context\n");

            context_attribs.init(attrib_list);
            if (!context_attribs.has_key(GLX_CONTEXT_FLAGS_ARB))
                context_attribs.add_key(GLX_CONTEXT_FLAGS_ARB, 0);

            int context_flags_value_ofs = context_attribs.find_value_ofs(GLX_CONTEXT_FLAGS_ARB);
            VOGL_ASSERT(context_flags_value_ofs >= 0);
            if (context_flags_value_ofs >= 0)
                context_attribs[context_flags_value_ofs] |= GLX_CONTEXT_DEBUG_BIT_ARB;

            int context_major_version_ofs = context_attribs.find_value_ofs(GLX_CONTEXT_MAJOR_VERSION_ARB);
            int context_minor_version_ofs = context_attribs.find_value_ofs(GLX_CONTEXT_MINOR_VERSION_ARB);

            if (context_major_version_ofs < 0)
            {
                // Don't slam up if they haven't requested a specific GL version, the driver will give us the most recent version that is backwards compatible with 1.0 (i.e. 4.3 for NVidia's current dirver).
            }
            else if (context_attribs[context_major_version_ofs] < 3)
            {
                context_attribs[context_major_version_ofs] = 3;

                if (context_minor_version_ofs < 0)
                    context_attribs.add_key(GLX_CONTEXT_MINOR_VERSION_ARB, 0);
                else
                    context_attribs[context_minor_version_ofs] = 0;

                vogl_warning_printf("Forcing GL context version up to v3.0 due to debug context usage\n");
            }

            attrib_list = context_attribs.get_vec().get_ptr();
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        GLXContext result = GL_ENTRYPOINT(glXCreateContextAttribsARB)(dpy, config, share_context, direct, attrib_list);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXCreateContextAttribsARB TID: 0x%" PRIX64 " Display: 0x%" PRIX64 " config: 0x%" PRIX64 " share_context: 0x%" PRIX64 " direct %i attrib_list: 0x%" PRIX64 ", result: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(config), cast_val_to_uint64(share_context), (int)direct, cast_val_to_uint64(attrib_list), cast_val_to_uint64(result));
        }

        vogl_context_manager &context_manager = get_context_manager();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXCreateContextAttribsARB, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_GLXFBCONFIG, &config, sizeof(config));
            serializer.add_param(2, VOGL_GLXCONTEXT, &share_context, sizeof(share_context));
            serializer.add_param(3, VOGL_BOOL, &direct, sizeof(direct));
            serializer.add_param(4, VOGL_CONST_INT_PTR, &attrib_list, sizeof(attrib_list));
            if (attrib_list)
            {
                uint32_t n = vogl_determine_attrib_list_array_size(attrib_list);
                serializer.add_array_client_memory(4, VOGL_INT, n, attrib_list, sizeof(int) * n);
            }
            serializer.add_return_param(VOGL_GLXCONTEXT, &result, sizeof(result));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (result)
        {
            if (share_context)
            {
                if (!g_app_uses_sharelists)
                    vogl_message_printf("sharelist usage detected\n");

                g_app_uses_sharelists = true;
            }

            context_manager.lock();

            vogl_context *pVOGL_context = context_manager.create_context(result);
            pVOGL_context->set_display(dpy);
            pVOGL_context->set_fb_config(config);
            pVOGL_context->set_sharelist_handle(share_context);
            pVOGL_context->set_direct(direct);
            pVOGL_context->set_attrib_list(attrib_list);
            pVOGL_context->set_created_from_attribs(true);
            pVOGL_context->set_creation_func(VOGL_ENTRYPOINT_glXCreateContextAttribsARB);

            if (share_context)
            {
                vogl_context *pShare_context = context_manager.lookup_vogl_context(share_context);

                if (!pShare_context)
                    vogl_error_printf("Failed finding share context 0x%" PRIx64 " in context manager's hashmap! This handle is probably invalid.\n", cast_val_to_uint64(share_context));
                else
                {
                    while (!pShare_context->is_root_context())
                        pShare_context = pShare_context->get_shared_state();

                    pVOGL_context->set_shared_context(pShare_context);

                    pShare_context->add_ref();
                }
            }

            pVOGL_context->init();

            context_manager.unlock();
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        return result;
    }

    //----------------------------------------------------------------------------------------------------------------------
    // vogl_get_fb_config_from_xvisual_info
    // Attempts to find the GLXFBConfig corresponding to a particular visual.
    // TODO: Test this more!
    //----------------------------------------------------------------------------------------------------------------------
    static GLXFBConfig *vogl_get_fb_config_from_xvisual_info(const Display *dpy, const XVisualInfo *vis)
    {
        vogl_context_attribs attribs;

    #define GET_CONFIG(attrib)                                   \
        do                                                       \
        {                                                        \
            int val = 0;                                         \
            GL_ENTRYPOINT(glXGetConfig)(dpy, vis, attrib, &val); \
            if (val)                                             \
                attribs.add_key(attrib, val);                    \
        } while (0)

        int is_rgba = 0;
        GL_ENTRYPOINT(glXGetConfig)(dpy, vis, GLX_RGBA, &is_rgba);

        attribs.add_key(GLX_RENDER_TYPE, is_rgba ? GLX_RGBA_BIT : GLX_COLOR_INDEX_BIT);
        attribs.add_key(GLX_X_RENDERABLE, True);
        attribs.add_key(GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT);

        if (!is_rgba)
            GET_CONFIG(GLX_BUFFER_SIZE);

        GET_CONFIG(GLX_LEVEL);

        GET_CONFIG(GLX_DOUBLEBUFFER);
        GET_CONFIG(GLX_STEREO);
        GET_CONFIG(GLX_AUX_BUFFERS);
        if (is_rgba)
        {
            GET_CONFIG(GLX_RED_SIZE);
            GET_CONFIG(GLX_GREEN_SIZE);
            GET_CONFIG(GLX_BLUE_SIZE);
            GET_CONFIG(GLX_ALPHA_SIZE);
        }
        GET_CONFIG(GLX_DEPTH_SIZE);
        GET_CONFIG(GLX_STENCIL_SIZE);

        GET_CONFIG(GLX_TRANSPARENT_INDEX_VALUE);
        GET_CONFIG(GLX_TRANSPARENT_RED_VALUE);
        GET_CONFIG(GLX_TRANSPARENT_GREEN_VALUE);
        GET_CONFIG(GLX_TRANSPARENT_BLUE_VALUE);
        GET_CONFIG(GLX_TRANSPARENT_ALPHA_VALUE);

        if (attribs.get_value_or_default(GLX_TRANSPARENT_INDEX_VALUE) || attribs.get_value_or_default(GLX_TRANSPARENT_RED_VALUE) ||
            attribs.get_value_or_default(GLX_TRANSPARENT_GREEN_VALUE) || attribs.get_value_or_default(GLX_TRANSPARENT_BLUE_VALUE) ||
            attribs.get_value_or_default(GLX_TRANSPARENT_ALPHA_VALUE))
        {
            GET_CONFIG(GLX_TRANSPARENT_TYPE);
        }
    #undef GET_CONFIG

    #if 0
	    for (uint32_t i = 0; i < attribs.size(); i += 2)
	    {
		    if (!attribs[i])
			    break;
		    printf("%s 0x%x\n", get_gl_enums().find_glx_name(attribs[i]), attribs[i + 1]);
	    }
    #endif

        int num_configs = 0;
        GLXFBConfig *pConfigs = GL_ENTRYPOINT(glXChooseFBConfig)(dpy, vis->screen, attribs.get_ptr(), &num_configs);
        return num_configs ? pConfigs : NULL;
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXCreateContext(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static GLXContext vogl_glXCreateContext(const Display *dpy, const XVisualInfo *vis, GLXContext shareList, Bool direct)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXCreateContext);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXCreateContext)(dpy, vis, shareList, direct);
        }

        if (g_command_line_params().get_value_as_bool("vogl_force_debug_context"))
        {
            vogl_warning_printf("Can't enable debug contexts via glXCreateContext(), forcing call to use glXCreateContextsAttribsARB() instead\n");

            GLXFBConfig *pConfig = vogl_get_fb_config_from_xvisual_info(dpy, vis);
            if (!pConfig)
            {
                vogl_error_printf("Can't enable debug contexts: Unable to find the FB config that matches the passed in XVisualInfo!\n");
            }
            else
            {
                int empty_attrib_list[1] = { 0 };
                return vogl_glXCreateContextAttribsARB(dpy, pConfig[0], shareList, direct, empty_attrib_list);
            }
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        GLXContext result = GL_ENTRYPOINT(glXCreateContext)(dpy, vis, shareList, direct);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXCreateContext TID: 0x%" PRIX64 " dpy: 0x%" PRIX64 " vis: 0x%" PRIX64 " shareList: 0x%" PRIX64 " direct %i, result: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(vis), cast_val_to_uint64(shareList), (int)direct, cast_val_to_uint64(result));
        }

        vogl_context_manager &context_manager = get_context_manager();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXCreateContext, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_CONST_XVISUALINFO_PTR, &vis, sizeof(vis));
            serializer.add_param(2, VOGL_GLXCONTEXT, &shareList, sizeof(shareList));
            serializer.add_param(3, VOGL_BOOL, &direct, sizeof(direct));
            serializer.add_return_param(VOGL_GLXCONTEXT, &result, sizeof(result));
            if (vis)
                serializer.add_ref_client_memory(1, VOGL_XVISUALINFO, vis, sizeof(XVisualInfo));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (result)
        {
            if (shareList)
            {
                if (!g_app_uses_sharelists)
                    vogl_message_printf("sharelist usage detected\n");

                g_app_uses_sharelists = true;
            }

            context_manager.lock();

            vogl_context *pVOGL_context = context_manager.create_context(result);
            pVOGL_context->set_display(dpy);
            pVOGL_context->set_xvisual_info(vis);
            pVOGL_context->set_sharelist_handle(shareList);
            pVOGL_context->set_direct(direct);
            pVOGL_context->set_creation_func(VOGL_ENTRYPOINT_glXCreateContext);

            if (shareList)
            {
                vogl_context *pShare_context = context_manager.lookup_vogl_context(shareList);

                if (!pShare_context)
                    vogl_error_printf("Failed finding share context 0x%" PRIx64 " in context manager's hashmap! This handle is probably invalid.\n", cast_val_to_uint64(shareList));
                else
                {
                    while (!pShare_context->is_root_context())
                        pShare_context = pShare_context->get_shared_state();

                    pVOGL_context->set_shared_context(pShare_context);

                    pShare_context->add_ref();
                }
            }

            pVOGL_context->init();

            context_manager.unlock();
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        return result;
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXCreateNewContext(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static GLXContext vogl_glXCreateNewContext(const Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct)
    {
        if (render_type != GLX_RGBA_TYPE)
        {
            vogl_error_printf("Unsupported render type (%s)!\n", get_gl_enums().find_glx_name(render_type));
        }

        if (g_command_line_params().get_value_as_bool("vogl_force_debug_context"))
        {
            vogl_warning_printf("Redirecting call from glxCreateNewContext() to glxCreateContextAttribsARB because --vogl_force_debug_context was specified. Note this may fail if glXCreateWindow() was called.\n");

            return vogl_glXCreateContextAttribsARB(dpy, config, share_list, direct, NULL);
        }

        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXCreateNewContext);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXCreateNewContext)(dpy, config, render_type, share_list, direct);
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        GLXContext result = GL_ENTRYPOINT(glXCreateNewContext)(dpy, config, render_type, share_list, direct);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXCreateNewContext TID: 0x%" PRIX64 " dpy: 0x%" PRIX64 " config: 0x%" PRIX64 " render_type: %i shareList: 0x%" PRIX64 " direct %i, result: 0x%" PRIX64 "\n",
                           vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(config), render_type, cast_val_to_uint64(share_list), static_cast<int>(direct), cast_val_to_uint64(result));
        }

        vogl_context_manager &context_manager = get_context_manager();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXCreateNewContext, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_GLXFBCONFIG, &config, sizeof(config));
            serializer.add_param(2, VOGL_INT, &render_type, sizeof(render_type));
            serializer.add_param(3, VOGL_GLXCONTEXT, &share_list, sizeof(share_list));
            serializer.add_param(4, VOGL_BOOL, &direct, sizeof(direct));
            serializer.add_return_param(VOGL_GLXCONTEXT, &result, sizeof(result));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (result)
        {
            if (share_list)
            {
                if (!g_app_uses_sharelists)
                    vogl_message_printf("sharelist usage detected\n");

                g_app_uses_sharelists = true;
            }

            context_manager.lock();

            vogl_context *pVOGL_context = context_manager.create_context(result);
            pVOGL_context->set_display(dpy);
            pVOGL_context->set_fb_config(config);
            pVOGL_context->set_sharelist_handle(share_list);
            pVOGL_context->set_direct(direct);
            pVOGL_context->set_creation_func(VOGL_ENTRYPOINT_glXCreateNewContext);

            if (share_list)
            {
                vogl_context *pShare_context = context_manager.lookup_vogl_context(share_list);

                if (!pShare_context)
                    vogl_error_printf("Failed finding share context 0x%" PRIx64 " in context manager's hashmap! This handle is probably invalid.\n", cast_val_to_uint64(share_list));
                else
                {
                    while (!pShare_context->is_root_context())
                        pShare_context = pShare_context->get_shared_state();

                    pVOGL_context->set_shared_context(pShare_context);

                    pShare_context->add_ref();
                }
            }

            pVOGL_context->init();

            context_manager.unlock();
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        return result;
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_glXDestroyContext(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static void vogl_glXDestroyContext(const Display *dpy, GLXContext context)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glXDestroyContext);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(glXDestroyContext)(dpy, context);
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** glXDestroyContext TID: 0x%" PRIX64 " Display: 0x%" PRIX64 " context: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(dpy), cast_val_to_uint64(context));
        }

        vogl_context_manager &context_manager = get_context_manager();
        vogl_context *pContext = context ? context_manager.lookup_vogl_context(context) : NULL;
        if (!pContext)
            vogl_error_printf("glXDestroyContext() called on an unknown context handle 0x%" PRIX64 "!\n", cast_val_to_uint64(context));
        else if (pContext->get_current_thread())
            vogl_error_printf("glXDestroyContext() called on a handle 0x%" PRIX64 " that is still current on thread 0x%" PRIX64 "! This may cause the asynchronous framebuffer capturing system to miss frames!\n", cast_val_to_uint64(context), pContext->get_current_thread());

        if (pContext)
        {
            pContext->on_destroy_prolog();
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        GL_ENTRYPOINT(glXDestroyContext)(dpy, context);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glXDestroyContext, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_CONST_DISPLAY_PTR, &dpy, sizeof(dpy));
            serializer.add_param(1, VOGL_GLXCONTEXT, &context, sizeof(context));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
            get_vogl_trace_writer().flush();
        }

        if (pContext)
        {
            context_manager.lock();

            VOGL_ASSERT(!pContext->get_deleted_flag());
            if (!pContext->get_deleted_flag())
            {
                pContext->set_deleted_flag(true);

                if (pContext->is_share_context())
                {
                    VOGL_ASSERT(pContext->get_ref_count() == 1);
                }

                int new_ref_count = pContext->del_ref();
                if (new_ref_count <= 0)
                {
                    if (pContext->is_share_context())
                    {
                        vogl_context *pRoot_context = pContext->get_shared_state();

                        pContext->set_shared_context(NULL);

                        if (pRoot_context->del_ref() == 0)
                        {
                            VOGL_ASSERT(pRoot_context->get_deleted_flag());

                            bool status = context_manager.destroy_context(pRoot_context->get_context_handle());
                            VOGL_ASSERT(status);
                            VOGL_NOTE_UNUSED(status);
                        }
                    }

                    bool status = context_manager.destroy_context(context);
                    VOGL_ASSERT(status);
                    VOGL_NOTE_UNUSED(status);
                }
            }

            context_manager.unlock();
        }

        // TODO: Ensure this is actually thread safe (does the gcc C runtime mutex this?)
        if (console::get_log_stream())
            console::get_log_stream()->flush();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }
    }

#elif VOGL_PLATFORM_HAS_WGL
    //----------------------------------------------------------------------------------------------------------------------
    static vogl_gl_state_snapshot *vogl_snapshot_state(HDC hdc, vogl_context *pCur_context)
    {
        VOGL_NOTE_UNUSED(hdc);

        // pCur_context may be NULL!

        if (!GL_ENTRYPOINT(wglGetCurrentContext) || !GL_ENTRYPOINT(wglGetCurrentDC))
            return NULL;

        timed_scope ts(VOGL_FUNCTION_INFO_CSTR);

        vogl_context_manager &context_manager = get_context_manager();
        context_manager.lock();

        if (vogl_check_gl_error())
            vogl_error_printf("A GL error occured sometime before this function was called\n");

        HDC orig_hdc = GL_ENTRYPOINT(wglGetCurrentDC)();
        HGLRC orig_context = GL_ENTRYPOINT(wglGetCurrentContext)();

        vogl_check_gl_error();

        vogl_gl_state_snapshot *pSnapshot = vogl_new(vogl_gl_state_snapshot);

        const context_map &contexts = context_manager.get_context_map();

        // TODO: Find a better way of determining which window dimensions to use.
        // No context is current, let's just find the biggest window.
        uint32_t win_width = 0;
        uint32_t win_height = 0;
        if (pCur_context)
        {
            win_width = pCur_context->get_window_width();
            win_height = pCur_context->get_window_height();
        }
        else
        {
            context_map::const_iterator it;
            for (it = contexts.begin(); it != contexts.end(); ++it)
            {
                vogl_context *pVOGL_context = it->second;
                if ((pVOGL_context->get_window_width() > (int)win_width) || (pVOGL_context->get_window_height() > (int)win_height))
                {
                    win_width = pVOGL_context->get_window_width();
                    win_height = pVOGL_context->get_window_height();
                }
            }
        }

        vogl_message_printf("Beginning capture: width %u, height %u\n", win_width, win_height);

        if (!pSnapshot->begin_capture(win_width, win_height, pCur_context ? (uint64_t)pCur_context->get_context_handle() : 0, 0, get_vogl_trace_writer().get_cur_gl_call_counter(), true))
        {
            vogl_error_printf("Failed beginning capture\n");

            vogl_delete(pSnapshot);
            pSnapshot = NULL;

            get_context_manager().unlock();

            return NULL;
        }

        vogl_printf("Capturing %u context(s)\n", contexts.size());

        context_map::const_iterator it;
        for (it = contexts.begin(); it != contexts.end(); ++it)
        {
            CONTEXT_TYPE gl_context = it->first;
            vogl_context *pVOGL_context = it->second;

            if (pVOGL_context->get_deleted_flag())
            {
                vogl_error_printf("Context 0x%" PRIX64 " has been deleted but is the root of a sharelist group - this scenario is not yet supported for state snapshotting.\n", cast_val_to_uint64(gl_context));
                break;
            }

            if (pVOGL_context->get_has_been_made_current())
            {
                if (!pVOGL_context->MakeCurrent(gl_context))
                {
                    vogl_error_printf("Failed switching to trace context 0x%" PRIX64 ". (This context may be current on another thread.) Capture failed!\n", cast_val_to_uint64(gl_context));
                    break;
                }

                if (pVOGL_context->get_total_mapped_buffers())
                {
                    vogl_error_printf("Trace context 0x%" PRIX64 " has %u buffer(s) mapped across a call to glXSwapBuffers(), which is not currently supported!\n", cast_val_to_uint64(gl_context), pVOGL_context->get_total_mapped_buffers());
                    break;
                }

                if (pVOGL_context->is_composing_display_list())
                {
                    vogl_error_printf("Trace context 0x%" PRIX64 " is currently composing a display list across a call to glXSwapBuffers()!\n", cast_val_to_uint64(gl_context));
                    break;
                }

                if (pVOGL_context->get_in_gl_begin())
                {
                    vogl_error_printf("Trace context 0x%" PRIX64 " is currently in a glBegin() across a call to glXSwapBuffers(), which is not supported!\n", cast_val_to_uint64(gl_context));
                    break;
                }
            }

            vogl_capture_context_params &capture_context_params = pVOGL_context->get_capture_context_params();

            if (!pSnapshot->capture_context(pVOGL_context->get_context_desc(), pVOGL_context->get_context_info(), pVOGL_context->get_handle_remapper(), capture_context_params))
            {
                vogl_error_printf("Failed capturing trace context 0x%" PRIX64 ", capture failed\n", cast_val_to_uint64(gl_context));
                break;
            }
        }

        if ((it == contexts.end()) && (pSnapshot->end_capture()))
        {
            vogl_printf("Capture succeeded\n");
        }
        else
        {
            vogl_printf("Capture failed\n");

            vogl_delete(pSnapshot);
            pSnapshot = NULL;
        }

        if (orig_hdc && orig_context)
        {
            GL_ENTRYPOINT(wglMakeCurrent)(orig_hdc, orig_context);
        }

        vogl_check_gl_error();

        get_context_manager().unlock();

        return pSnapshot;
    }


    //----------------------------------------------------------------------------------------------------------------------
    static bool vogl_write_snapshot_to_trace(const char *pTrace_filename, HDC hdc, vogl_context *pVOGL_context)
    {
        VOGL_FUNC_TRACER
        vogl_message_printf("Snapshot begin %s\n", pTrace_filename);

        // pVOGL_context may be NULL here!

        vogl_unique_ptr<vogl_gl_state_snapshot> pSnapshot(vogl_snapshot_state(hdc, pVOGL_context));
        if (!pSnapshot.get())
        {
            vogl_error_printf("Failed snapshotting GL/GLX state!\n");

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        pSnapshot->set_frame_index(0);

        if (!get_vogl_trace_writer().open(pTrace_filename, NULL, true, false))
        {
            vogl_error_printf("Failed creating trace file \"%s\"!\n", pTrace_filename);

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        vogl_archive_blob_manager &trace_archive = *get_vogl_trace_writer().get_trace_archive();

        vogl_message_printf("Serializing snapshot data to JSON document\n");

        // TODO: This can take a lot of memory, probably better off to split the snapshot into separate smaller binary json or whatever files stored directly in the archive.
        json_document doc;
        if (!pSnapshot->serialize(*doc.get_root(), trace_archive, &get_vogl_process_gl_ctypes()))
        {
            vogl_error_printf("Failed serializing GL state snapshot!\n");

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        pSnapshot.reset();

        vogl_message_printf("Serializing JSON document to UBJ\n");

        uint8_vec binary_snapshot_data;
        vogl::vector<char> snapshot_data;

        // TODO: This can take a lot of memory
        doc.binary_serialize(binary_snapshot_data);

        vogl_message_printf("Compressing UBJ data and adding to trace archive\n");

        dynamic_string binary_snapshot_id(trace_archive.add_buf_compute_unique_id(binary_snapshot_data.get_ptr(), binary_snapshot_data.size(), "binary_state_snapshot", VOGL_BINARY_JSON_EXTENSION));
        if (binary_snapshot_id.is_empty())
        {
            vogl_error_printf("Failed adding binary GL snapshot file to output blob manager!\n");

            VOGL_FUNC_TRACER
                vogl_end_capture();
            return false;
        }

        binary_snapshot_data.clear();

        snapshot_data.clear();

    #if 0
        // TODO: This requires too much temp memory!
        doc.serialize(snapshot_data, true, 0, false);

        dynamic_string snapshot_id(trace_archive.add_buf_compute_unique_id(snapshot_data.get_ptr(), snapshot_data.size(), "state_snapshot", VOGL_TEXT_JSON_EXTENSION));
        if (snapshot_id.is_empty())
        {
            vogl_error_printf("Failed adding binary GL snapshot file to output blob manager!\n");
            get_vogl_trace_writer().deinit();
            return false;
        }
    #endif

        key_value_map snapshot_key_value_map;
        snapshot_key_value_map.insert("command_type", "state_snapshot");
        snapshot_key_value_map.insert("binary_id", binary_snapshot_id);

    #if 0
        // TODO: This requires too much temp memory!
        snapshot_key_value_map.insert("id", snapshot_id);
    #endif

        vogl_ctypes &trace_gl_ctypes = get_vogl_process_gl_ctypes();
        if (!vogl_write_glInternalTraceCommandRAD(get_vogl_trace_writer().get_stream(), &trace_gl_ctypes, cITCRKeyValueMap, sizeof(snapshot_key_value_map), reinterpret_cast<const GLubyte *>(&snapshot_key_value_map)))
        {
            VOGL_FUNC_TRACER
                vogl_end_capture();
            vogl_error_printf("Failed writing to trace file!\n");
            return false;
        }

        if (!vogl_write_glInternalTraceCommandRAD(get_vogl_trace_writer().get_stream(), &trace_gl_ctypes, cITCRDemarcation, 0, NULL))
        {
            VOGL_FUNC_TRACER
                vogl_end_capture();
            vogl_error_printf("Failed writing to trace file!\n");
            return false;
        }

        doc.clear(false);

        if (g_pJSON_node_pool)
        {
            uint64_t total_bytes_freed = static_cast<uint64_t>(g_pJSON_node_pool->free_unused_blocks());
            vogl_debug_printf("Freed %" PRIu64 " bytes from the JSON object pool (%" PRIu64 " bytes remaining)\n", total_bytes_freed, static_cast<uint64_t>(g_pJSON_node_pool->get_total_heap_bytes()));
        }

        vogl_message_printf("Snapshot complete\n");

        return true;
    }

    //----------------------------------------------------------------------------------------------------------------------
    //  vogl_tick_capture
    //----------------------------------------------------------------------------------------------------------------------
    static void vogl_tick_capture(HDC hdc, vogl_context *pVOGL_context)
    {
        VOGL_FUNC_TRACER

        // pVOGL_context may be NULL here!

        vogl_check_for_capture_stop_file();
        vogl_check_for_capture_trigger_file();

        scoped_mutex lock(get_vogl_trace_mutex());

        if ((g_vogl_total_frames_to_capture) && (!g_vogl_frames_remaining_to_capture))
        {
            g_vogl_frames_remaining_to_capture = g_vogl_total_frames_to_capture;
            g_vogl_total_frames_to_capture = 0;
        }

        if (g_vogl_stop_capturing)
        {
            g_vogl_stop_capturing = false;

            if (get_vogl_trace_writer().is_opened())
            {
                vogl_end_capture();
                return;
            }
        }

        if (!g_vogl_frames_remaining_to_capture)
            return;

        if (!get_vogl_trace_writer().is_opened())
        {
            dynamic_string trace_path(g_command_line_params().get_value_as_string_or_empty("vogl_tracepath"));
            if (trace_path.is_empty())
                trace_path = "/tmp";
            if (!get_vogl_intercept_data().capture_path.is_empty())
                trace_path = get_vogl_intercept_data().capture_path;

            time_t t = time(NULL);
            struct tm ltm = *localtime(&t);

            dynamic_string trace_basename("capture");
            if (!get_vogl_intercept_data().capture_basename.is_empty())
                trace_basename = get_vogl_intercept_data().capture_basename;

            dynamic_string filename(cVarArg, "%s_%04d_%02d_%02d_%02d_%02d_%02d.bin", trace_basename.get_ptr(), ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

            dynamic_string full_trace_filename;
            file_utils::combine_path(full_trace_filename, trace_path.get_ptr(), filename.get_ptr());

            if (g_vogl_frames_remaining_to_capture == cUINT32_MAX)
                vogl_message_printf("Initiating capture of all remaining frames to file \"%s\"\n", full_trace_filename.get_ptr());
            else
                vogl_message_printf("Initiating capture of up to %u frame(s) to file \"%s\"\n", g_vogl_frames_remaining_to_capture, full_trace_filename.get_ptr());

            if (!vogl_write_snapshot_to_trace(full_trace_filename.get_ptr(), hdc, pVOGL_context))
            {
                file_utils::delete_file(full_trace_filename.get_ptr());
                vogl_error_printf("Failed creating GL state snapshot, closing and deleting trace file\n");
            }
        }
        else
        {
            if (g_vogl_frames_remaining_to_capture != cUINT32_MAX)
                g_vogl_frames_remaining_to_capture--;

            // See if we should stop capturing.
            if (!g_vogl_frames_remaining_to_capture)
                vogl_end_capture();
        }
    }

    //----------------------------------------------------------------------------------------------------------------------
    //  vogl_glXSwapBuffersGLFuncProlog
    //----------------------------------------------------------------------------------------------------------------------
    static inline void vogl_wglSwapBuffersGLFuncProlog(HDC hdc, vogl_context *pVOGL_context, vogl_entrypoint_serializer &trace_serializer)
    {
        // pVOGL_context may be NULL here!

        if (!GL_ENTRYPOINT(glXGetCurrentContext) || !GL_ENTRYPOINT(glXGetCurrentDisplay) ||
            !GL_ENTRYPOINT(glXGetCurrentDrawable) || !GL_ENTRYPOINT(glXGetCurrentReadDrawable))
        {
            return;
        }

        unsigned int width = 0,
                     height = 0;

        if (get_dimensions_from_dc(&width, &height, hdc))
        {
            pVOGL_context->set_window_dimensions(width, height);

            vogl_tick_screen_capture(pVOGL_context);
        }
        else
        {
            vogl_warning_printf("Couldn't determine dimensions from hdc!\n");
        }

        if (trace_serializer.is_in_begin())
        {
            trace_serializer.add_key_value(string_hash("win_width"), pVOGL_context->get_window_width());
            trace_serializer.add_key_value(string_hash("win_height"), pVOGL_context->get_window_height());
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** Current window dimensions: %ix%i\n", pVOGL_context->get_window_width(), pVOGL_context->get_window_height());
        }

        pVOGL_context->inc_frame_index();
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_wglSwapBuffers(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static BOOL WINAPI vogl_wglSwapBuffers(HDC hdc)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_wglSwapBuffers);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(wglSwapBuffers)(hdc);
        }

        // Use a local serializer because the call to wglSwapBuffer()'s will make GL calls if something like the Steam Overlay is active.
        vogl_entrypoint_serializer serializer;

        if (get_vogl_trace_writer().is_opened())
        {
            serializer.begin(VOGL_ENTRYPOINT_wglSwapBuffers, get_context_manager().get_current(true));
            serializer.add_param(0, VOGL_HDC, &hdc, sizeof(hdc));
        }

        vogl_wglSwapBuffersGLFuncProlog(hdc, pTLS_data->m_pContext, serializer);

        uint64_t gl_begin_rdtsc = utils::RDTSC();

        // Call the driver directly, bypassing our GL/GLX wrappers which set m_calling_driver_entrypoint_id. The Steam Overlay may call us back!
        BOOL result = DIRECT_GL_ENTRYPOINT(wglSwapBuffers)(hdc);

        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (get_vogl_trace_writer().is_opened())
        {
            // Add the return parameter here.
            serializer.add_return_param(VOGL_BOOL, &result, sizeof(result));

            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        vogl_tick_capture(hdc, pTLS_data->m_pContext);

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** wglSwapBuffers TID: 0x%" PRIX64 " HDC: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(hdc));
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        if (g_command_line_params().has_key("vogl_exit_after_x_frames") && (pTLS_data->m_pContext))
        {
            uint64_t max_num_frames = g_command_line_params().get_value_as_uint64("vogl_exit_after_x_frames");
            uint64_t cur_num_frames = pTLS_data->m_pContext->get_frame_index();

            if (cur_num_frames >= max_num_frames)
            {
                vogl_message_printf("Number of frames specified by --vogl_exit_after_x_frames param reached, forcing trace to close and calling exit()\n");

                vogl_end_capture();

                // Exit the app
                exit(0);
            }
        }

        return result;
    }

    #define DEF_FUNCTION_CUSTOM_HANDLER_wglCreateContextAttribsARB(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static HGLRC WINAPI vogl_wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN %s 0x%" PRIX64 "\n", VOGL_FUNCTION_INFO_CSTR, vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_wglCreateContextAttribsARB);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("%s: GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", VOGL_FUNCTION_INFO_CSTR, g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(wglCreateContextAttribsARB)(hDC, hShareContext, attribList);
        }

        vogl_context_attribs context_attribs;

        if (g_command_line_params().get_value_as_bool("vogl_force_debug_context"))
        {
            vogl_warning_printf("%s: Forcing debug context\n", VOGL_FUNCTION_INFO_CSTR);

            context_attribs.init(attribList);
            if (!context_attribs.has_key(WGL_CONTEXT_FLAGS_ARB))
                context_attribs.add_key(WGL_CONTEXT_FLAGS_ARB, 0);

            int context_flags_value_ofs = context_attribs.find_value_ofs(WGL_CONTEXT_FLAGS_ARB);
            VOGL_ASSERT(context_flags_value_ofs >= 0);
            if (context_flags_value_ofs >= 0)
                context_attribs[context_flags_value_ofs] |= WGL_CONTEXT_DEBUG_BIT_ARB;

            int context_major_version_ofs = context_attribs.find_value_ofs(WGL_CONTEXT_MAJOR_VERSION_ARB);
            int context_minor_version_ofs = context_attribs.find_value_ofs(WGL_CONTEXT_MINOR_VERSION_ARB);

            if (context_major_version_ofs < 0)
            {
                // Don't slam up if they haven't requested a specific GL version, the driver will give us the most recent version that is backwards compatible with 1.0 (i.e. 4.3 for NVidia's current dirver).
            }
            else if (context_attribs[context_major_version_ofs] < 3)
            {
                context_attribs[context_major_version_ofs] = 3;

                if (context_minor_version_ofs < 0)
                    context_attribs.add_key(WGL_CONTEXT_MINOR_VERSION_ARB, 0);
                else
                    context_attribs[context_minor_version_ofs] = 0;

                vogl_warning_printf("%s: Forcing GL context version up to v3.0 due to debug context usage\n", VOGL_FUNCTION_INFO_CSTR);
            }

            attribList = context_attribs.get_vec().get_ptr();
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        HGLRC result = GL_ENTRYPOINT(wglCreateContextAttribsARB)(hDC, hShareContext, attribList);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** wglCreateContextAttribsARB TID: 0x%" PRIX64 " hDC: 0x%" PRIX64 " hShareContext: 0x%" PRIX64 " attribList: 0x%" PRIX64 ", result: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(hDC), cast_val_to_uint64(hShareContext), cast_val_to_uint64(attribList), cast_val_to_uint64(result));
        }

        vogl_context_manager &context_manager = get_context_manager();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_wglCreateContextAttribsARB, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_HDC, &hDC, sizeof(hDC));
            serializer.add_param(1, VOGL_HGLRC, &hShareContext, sizeof(hShareContext));
            serializer.add_param(2, VOGL_CONST_INT_PTR, &attribList, sizeof(attribList));
            if (attribList)
            {
                uint32_t n = vogl_determine_attrib_list_array_size(attribList);
                serializer.add_array_client_memory(2, VOGL_INT, n, attribList, sizeof(int) * n);
            }
            serializer.add_return_param(VOGL_HGLRC, &result, sizeof(result));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (result)
        {
            if (hShareContext)
            {
                if (!g_app_uses_sharelists)
                    vogl_message_printf("%s: sharelist usage detected\n", VOGL_FUNCTION_INFO_CSTR);

                g_app_uses_sharelists = true;
            }

            context_manager.lock();

            vogl_context *pVOGL_context = context_manager.create_context(result);
            pVOGL_context->set_hdc(hDC);
            pVOGL_context->set_sharelist_handle(hShareContext);
            pVOGL_context->set_attrib_list(attribList);
            pVOGL_context->set_created_from_attribs(true);
            pVOGL_context->set_creation_func(VOGL_ENTRYPOINT_wglCreateContextAttribsARB);

            if (hShareContext)
            {
                vogl_context *pShare_context = context_manager.lookup_vogl_context(hShareContext);

                if (!pShare_context)
                    vogl_error_printf("%s: Failed finding share context 0x%" PRIx64 " in context manager's hashmap! This handle is probably invalid.\n", VOGL_FUNCTION_INFO_CSTR, cast_val_to_uint64(hShareContext));
                else
                {
                    while (!pShare_context->is_root_context())
                        pShare_context = pShare_context->get_shared_state();

                    pVOGL_context->set_shared_context(pShare_context);

                    pShare_context->add_ref();
                }
            }

            pVOGL_context->init();

            context_manager.unlock();
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END %s 0x%" PRIX64 "\n", VOGL_FUNCTION_INFO_CSTR, vogl_get_current_kernel_thread_id());
        }

        return result;
    }

    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_wglCreateContext(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static HGLRC WINAPI vogl_wglCreateContext(HDC hdc)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_wglCreateContext);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(wglCreateContext)(hdc);
        }

        if (g_command_line_params().get_value_as_bool("vogl_force_debug_context"))
        {
            vogl_warning_printf("Can't enable debug contexts via wglCreateContext(), forcing call to use wglCreateContextsAttribsARB() instead\n");

            #if (VOGL_PLATFORM_HAS_WGL)
                VOGL_VERIFY(!"impl vogl_wglCreateContext vogl_force_debug_context on Windows.");
            #endif
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        HGLRC result = GL_ENTRYPOINT(wglCreateContext)(hdc);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** wglCreateContext TID: 0x%" PRIX64 " hdc: 0x%" PRIX64 ", result: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(hdc), cast_val_to_uint64(result));
        }

        vogl_context_manager &context_manager = get_context_manager();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_wglCreateContext, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_HDC, &hdc, sizeof(hdc));
            serializer.add_return_param(VOGL_HGLRC, &result, sizeof(result));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (result)
        {
            context_manager.lock();

            vogl_context *pVOGL_context = context_manager.create_context(result);
            pVOGL_context->set_hdc(hdc);
            pVOGL_context->set_creation_func(VOGL_ENTRYPOINT_wglCreateContext);

            pVOGL_context->init();

            context_manager.unlock();
        }

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        return result;
    }


    //----------------------------------------------------------------------------------------------------------------------
    #define DEF_FUNCTION_CUSTOM_HANDLER_wglShareLists(exported, category, ret, ret_type_enum, num_params, name, args, params)
    static BOOL WINAPI vogl_wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
    {
        uint64_t begin_rdtsc = utils::RDTSC();

        if (!g_app_uses_sharelists)
            vogl_message_printf("sharelist usage detected\n");
        g_app_uses_sharelists = true;

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_wglShareLists);
        if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
        {
            vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
            return GL_ENTRYPOINT(wglShareLists)(hglrc1, hglrc2);
        }

        uint64_t gl_begin_rdtsc = utils::RDTSC();
        BOOL result = GL_ENTRYPOINT(wglShareLists)(hglrc1, hglrc2);
        uint64_t gl_end_rdtsc = utils::RDTSC();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** wglShareLists TID: 0x%" PRIX64 " hglrc1: 0x%" PRIX64 ", hglrc2: 0x%" PRIX64 ", result: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id(), cast_val_to_uint64(hglrc1), cast_val_to_uint64(hglrc2), cast_val_to_uint64(result));
        }

        vogl_context_manager &context_manager = get_context_manager();

        if (get_vogl_trace_writer().is_opened())
        {
            vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_wglShareLists, context_manager.get_current(true));
            serializer.set_begin_rdtsc(begin_rdtsc);
            serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
            serializer.add_param(0, VOGL_HGLRC, &hglrc1, sizeof(hglrc1));
            serializer.add_param(1, VOGL_HGLRC, &hglrc2, sizeof(hglrc2));
            serializer.add_return_param(VOGL_BOOL, &result, sizeof(result));
            serializer.end();
            vogl_write_packet_to_trace(serializer.get_packet());
        }

        if (result)
        {
            context_manager.lock();

            vogl_context *pVOGL_context = context_manager.lookup_vogl_context(hglrc2);
            pVOGL_context->set_sharelist_handle(hglrc1);
            pVOGL_context->set_creation_func(VOGL_ENTRYPOINT_wglCreateContext);

            vogl_context *pShare_context = context_manager.lookup_vogl_context(hglrc1);

            if (!pShare_context)
                vogl_error_printf("Failed finding share context 0x%" PRIx64 " in context manager's hashmap! This handle is probably invalid.\n", cast_val_to_uint64(hglrc1));
            else
            {
                while (!pShare_context->is_root_context())
                    pShare_context = pShare_context->get_shared_state();

                pVOGL_context->set_shared_context(pShare_context);

                pShare_context->add_ref();
            }

            pVOGL_context->init();
            context_manager.unlock();
        }

        // TODO: Ensure this is actually thread safe (does the gcc C runtime mutex this?)
        if (console::get_log_stream())
            console::get_log_stream()->flush();

        if (g_dump_gl_calls_flag)
        {
            vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
        }

        return result;
    }
#endif


//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_HANDLER_glGetError(exported, category, ret, ret_type_enum, num_params, name, args, params)
static GLenum vogl_glGetError()
{
    uint64_t begin_rdtsc = utils::RDTSC();

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** BEGIN 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    vogl_thread_local_data *pTLS_data = vogl_entrypoint_prolog(VOGL_ENTRYPOINT_glGetError);
    if (pTLS_data->m_calling_driver_entrypoint_id != VOGL_ENTRYPOINT_INVALID)
    {
        vogl_warning_printf("GL call detected while libvogltrace was itself making a GL call to func %s! This call will not be traced.\n", g_vogl_entrypoint_descs[pTLS_data->m_calling_driver_entrypoint_id].m_pName);
        return GL_ENTRYPOINT(glGetError)();
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** glGetError TID: 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    vogl_context *pContext = pTLS_data->m_pContext;
    // pContext may be NULL here if the user has called glGetError() incorrectly, we've already printed the error message, but we're still going to call the driver (which will probably just immediately return).
    // Also, even if we have a latched error for this context, we MUST call glGetError() to guarantee there are no current errors on the context (to preserve the original behavior if the tracer wasn't present).

    uint64_t gl_begin_rdtsc = utils::RDTSC();
    GLenum gl_err = GL_ENTRYPOINT(glGetError)();
    uint64_t gl_end_rdtsc = utils::RDTSC();

    if (get_vogl_trace_writer().is_opened())
    {
        vogl_entrypoint_serializer serializer(VOGL_ENTRYPOINT_glGetError, get_context_manager().get_current(true));
        serializer.set_begin_rdtsc(begin_rdtsc);
        serializer.set_gl_begin_end_rdtsc(gl_begin_rdtsc, gl_end_rdtsc);
        serializer.add_return_param(VOGL_GLENUM, &gl_err, sizeof(gl_err));

        if ((pContext) && (pContext->has_latched_gl_error()))
        {
            // Record the latched error too, so the replayer knows what's going on.
            serializer.add_key_value("latched_gl_error", pContext->get_latched_gl_error());
        }

        serializer.end();
        vogl_write_packet_to_trace(serializer.get_packet());
        get_vogl_trace_writer().flush();
    }

    // See if our context's shadow has a latched GL error recorded by the tracer in an earlier call. If so, it must override this GL error.
    if (pContext && pContext->has_latched_gl_error())
    {
        if (gl_err != GL_NO_ERROR)
        {
            vogl_warning_printf("glGetError() called with a GL error latched on the context by the tracer. Current error is 0x%04X, latched error is 0x%04X. Note the queued error will suppress the current error.\n", gl_err, pContext->get_latched_gl_error());
        }

        // We're going to replace the current GL error with our previously latched error, which *would have* masked this error if the tracer wasn't present.
        gl_err = pContext->get_latched_gl_error();

        pContext->clear_latched_gl_error();
    }

    if (g_dump_gl_calls_flag)
    {
        vogl_log_printf("** END 0x%" PRIX64 "\n", vogl_get_current_kernel_thread_id());
    }

    return gl_err;
}

//----------------------------------------------------------------------------------------------------------------------
// shader source code
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glShaderSourceARB(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_serialize_shader_source(trace_serializer, count, string, length);
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glShaderSource(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_serialize_shader_source(trace_serializer, count, (const GLcharARB *const *)string, length);
static void vogl_serialize_shader_source(vogl_entrypoint_serializer &trace_serializer, GLsizei count, const GLcharARB *const *string, const GLint *length)
{
    if (g_dump_gl_shaders_flag)
    {
        vogl_log_printf("Source source code, %i string(s):\n", count);
        for (GLsizei i = 0; i < count; i++)
        {
            const char *pStr = (const char *)string[i];
            int str_len = 0;
            if (length)
                str_len = length[i];
            else
                str_len = pStr ? (vogl_strlen(pStr) + 1) : 0;

            vogl_log_printf("\"");
            vogl_print_string(pStr, str_len);
            vogl_log_printf("\"\n");
        }
    }

    if (trace_serializer.is_in_begin())
    {
        for (GLsizei i = 0; i < count; i++)
        {
            const char *pStr = (const char *)string[i];
            int str_len = 0;
            if (length && length[i] >= 0)
                str_len = length[i];
            else
                str_len = pStr ? (vogl_strlen(pStr) + 1) : 0;

            if ((str_len) && (pStr))
                trace_serializer.add_key_value_blob(i, pStr, str_len);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_uses_client_side_arrays
//----------------------------------------------------------------------------------------------------------------------
static bool vogl_uses_client_side_arrays(vogl_context *pContext, bool indexed)
{
    if ((!pContext) || (!pContext->get_uses_client_side_arrays()) || (pContext->is_core_profile()))
        return false;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    if (indexed)
    {
        GLuint element_array_buffer = vogl_get_bound_gl_buffer(GL_ELEMENT_ARRAY_BUFFER);
        if (!element_array_buffer)
            return true;
    }

    bool used_old_style_gl_client_side_arrays = false;

    GLint prev_client_active_texture = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &prev_client_active_texture);

    const uint32_t tex_coords = pContext->get_max_texture_coords();

    for (uint32_t i = 0; i < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS; i++)
    {
        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[i];
        if (i == vogl_texcoord_pointer_array_id)
        {
            for (uint32_t tex_index = 0; tex_index < tex_coords; tex_index++)
            {
                GL_ENTRYPOINT(glClientActiveTexture)(GL_TEXTURE0 + tex_index);

                GLboolean is_enabled = GL_ENTRYPOINT(glIsEnabled)(desc.m_is_enabled);
                if (!is_enabled)
                    continue;

                GLint binding = 0;
                GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_binding, &binding);
                if (binding)
                    continue;

                used_old_style_gl_client_side_arrays = true;
                break;
            }

            if (used_old_style_gl_client_side_arrays)
                break;
        }
        else
        {
            GLboolean is_enabled = GL_ENTRYPOINT(glIsEnabled)(desc.m_is_enabled);
            if (!is_enabled)
                continue;

            GLint binding = 0;
            GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_binding, &binding);
            if (binding)
                continue;

            used_old_style_gl_client_side_arrays = true;
            break;
        }
    }

    GL_ENTRYPOINT(glClientActiveTexture)(prev_client_active_texture);

    if (used_old_style_gl_client_side_arrays)
        return true;

    uint64_t vertex_attrib_client_side_arrays = 0;
    VOGL_ASSUME(VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES <= sizeof(vertex_attrib_client_side_arrays) * 8);

    for (uint32_t i = 0; i < pContext->get_max_vertex_attribs(); i++)
    {
        GLint is_enabled = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &is_enabled);
        if (!is_enabled)
            continue;

        GLint cur_buf = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &cur_buf);
        if (cur_buf)
            continue;

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_serialize_client_side_arrays_helper
//----------------------------------------------------------------------------------------------------------------------
static void vogl_serialize_client_side_arrays_helper(
    vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, const char *pFunc,
    GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex, bool start_end_valid, bool indexed)
{
    VOGL_NOTE_UNUSED(mode);

    VOGL_NOTE_UNUSED(pFunc);

    if ((!pContext) || (pContext->is_core_profile()))
        return;

    if (end < start)
    {
        vogl_error_printf("end (%i) must be >= start (%i)\n", end, start);
        return;
    }

    uint32_t index_size = vogl_get_gl_type_size(type);
    if (!index_size)
    {
        vogl_error_printf("Invalid type parameter 0x%08X\n", type);
        return;
    }

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint element_array_buffer = 0;
    if (indexed)
    {
        element_array_buffer = vogl_get_bound_gl_buffer(GL_ELEMENT_ARRAY_BUFFER);
        if (!element_array_buffer)
        {
            if (!indices)
            {
                vogl_error_printf("No bound element array buffer, and indices parameter is NULL\n");
                return;
            }
            else
            {
                if (g_dump_gl_buffers_flag)
                {
                    vogl_log_printf("Client side index data: ");
                    vogl_print_hex(indices, count * index_size, index_size);
                    vogl_log_printf("\n");
                }

                if (trace_serializer.is_in_begin())
                {
                    trace_serializer.add_key_value_blob(string_hash("indices"), indices, count * index_size);
                }
            }
        }
    }

    const gl_entrypoint_id_t cur_entrypoint = trace_serializer.get_cur_entrypoint();
    VOGL_NOTE_UNUSED(cur_entrypoint);

    // TODO: Have the glSet*Pointer()'s funcs set a flag when the client uses this old shit, so we can avoid checking for it on apps that don't.

    bool used_old_style_gl_client_side_arrays = false;

    GLint prev_client_active_texture = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_CLIENT_ACTIVE_TEXTURE, &prev_client_active_texture);

    const uint32_t tex_coords = pContext->get_max_texture_coords();

    for (uint32_t i = 0; i < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS; i++)
    {
        const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[i];
        if (i == vogl_texcoord_pointer_array_id)
        {
            for (uint32_t tex_index = 0; tex_index < tex_coords; tex_index++)
            {
                GL_ENTRYPOINT(glClientActiveTexture)(GL_TEXTURE0 + tex_index);

                GLboolean is_enabled = GL_ENTRYPOINT(glIsEnabled)(desc.m_is_enabled);
                if (!is_enabled)
                    continue;

                GLint binding = 0;
                GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_binding, &binding);
                if (binding)
                    continue;

                used_old_style_gl_client_side_arrays = true;
                break;
            }

            if (used_old_style_gl_client_side_arrays)
                break;
        }
        else
        {
            GLboolean is_enabled = GL_ENTRYPOINT(glIsEnabled)(desc.m_is_enabled);
            if (!is_enabled)
                continue;

            GLint binding = 0;
            GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_binding, &binding);
            if (binding)
                continue;

            used_old_style_gl_client_side_arrays = true;
            break;
        }
    }

    GL_ENTRYPOINT(glClientActiveTexture)(prev_client_active_texture);

    uint64_t vertex_attrib_client_side_arrays = 0;
    VOGL_ASSUME(VOGL_MAX_SUPPORTED_GL_VERTEX_ATTRIBUTES <= sizeof(vertex_attrib_client_side_arrays) * 8);

    for (uint32_t i = 0; i < pContext->get_max_vertex_attribs(); i++)
    {
        GLint is_enabled = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &is_enabled);
        if (!is_enabled)
            continue;

        GLint cur_buf = 0;
        GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &cur_buf);
        if (cur_buf)
            continue;

        vertex_attrib_client_side_arrays |= (1ULL << i);
    }

    if ((!used_old_style_gl_client_side_arrays) && (!vertex_attrib_client_side_arrays))
        return;

    if (indexed)
    {
        if (!start_end_valid)
        {
            uint32_t total_index_data_size = count * index_size;

            // FIXME: Move index_data array to context state
            vogl::vector<uint8_t> index_data;
            const uint8_t *pIndices_to_scan = static_cast<const uint8_t *>(indices);

            if (element_array_buffer)
            {
                index_data.resize(total_index_data_size);
                pIndices_to_scan = index_data.get_ptr();

                GL_ENTRYPOINT(glGetBufferSubData)(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)indices, total_index_data_size, index_data.get_ptr());
            }

            start = cUINT32_MAX;
            end = 0;

            for (int i = 0; i < count; i++)
            {
                uint32_t v = 0;

                if (type == GL_UNSIGNED_BYTE)
                    v = pIndices_to_scan[i];
                else if (type == GL_UNSIGNED_SHORT)
                    v = reinterpret_cast<const uint16_t *>(pIndices_to_scan)[i];
                else if (type == GL_UNSIGNED_INT)
                    v = reinterpret_cast<const uint32_t *>(pIndices_to_scan)[i];
                else
                {
                    VOGL_ASSERT_ALWAYS;
                }

                start = math::minimum(start, v);
                end = math::maximum(end, v);
            }
        }

        if (trace_serializer.is_in_begin())
        {
            trace_serializer.add_key_value(string_hash("start"), start);
            trace_serializer.add_key_value(string_hash("end"), end);
        }
    }

    if (used_old_style_gl_client_side_arrays)
    {
        for (uint32_t client_array_iter = 0; client_array_iter < VOGL_NUM_CLIENT_SIDE_ARRAY_DESCS; client_array_iter++)
        {
            const vogl_client_side_array_desc_t &desc = g_vogl_client_side_array_descs[client_array_iter];

            uint32_t n = 1;
            uint32_t base_key_index = 0x1000 + client_array_iter;

            // Special case texcoord pointers, which are accessed via the client active texture.
            if (client_array_iter == vogl_texcoord_pointer_array_id)
            {
                n = tex_coords;
                base_key_index = 0x2000;
            }

            for (uint32_t inner_iter = 0; inner_iter < n; inner_iter++)
            {
                if (client_array_iter == vogl_texcoord_pointer_array_id)
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

                GLint inner_type = GL_BOOL;
                if (desc.m_get_type)
                {
                    GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_type, &inner_type);
                }

                GLint stride = 0;
                GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_stride, &stride);

                GLint size = 1;
                if (desc.m_get_size)
                {
                    GL_ENTRYPOINT(glGetIntegerv)(desc.m_get_size, &size);
                }

                uint32_t type_size = vogl_get_gl_type_size(inner_type);
                if (!type_size)
                {
                    vogl_error_printf("Can't determine type size of enabled client side array set by func %s\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName);
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
                else if ((size < 1) || (size > 4))
                {
                    vogl_error_printf("Size of client side array set by func %s must be between 1 and 4\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName);
                    continue;
                }

                if (!stride)
                    stride = type_size * size;

                uint32_t first_vertex_ofs = (start + basevertex) * stride;
                uint32_t last_vertex_ofs = (end + basevertex) * stride;
                uint32_t vertex_data_size = (last_vertex_ofs + stride) - first_vertex_ofs;

                if (g_dump_gl_buffers_flag)
                {
                    vogl_log_printf("Client side vertex data from %s index %u (comps: %i type_size: %i stride: %i):\n", g_vogl_entrypoint_descs[desc.m_entrypoint].m_pName, inner_iter, size, type_size, stride);
                    vogl_print_hex(static_cast<const uint8_t *>(ptr) + first_vertex_ofs, vertex_data_size, type_size);
                    vogl_log_printf("\n");
                }

                if (trace_serializer.is_in_begin())
                {
                    uint32_t key_index = base_key_index + inner_iter;
                    trace_serializer.add_key_value_blob(static_cast<uint16_t>(key_index), static_cast<const uint8_t *>(ptr) + first_vertex_ofs, vertex_data_size);
                }
            } // inner_iter

        } // client_array_iter

        GL_ENTRYPOINT(glClientActiveTexture)(prev_client_active_texture);
    } // used_old_style_gl_client_side_arrays

    if (vertex_attrib_client_side_arrays)
    {
        for (uint32_t i = 0; i < pContext->get_max_vertex_attribs(); i++)
        {
            if ((vertex_attrib_client_side_arrays & (1ULL << i)) == 0)
                continue;

            GLvoid *attrib_ptr = NULL;
            GL_ENTRYPOINT(glGetVertexAttribPointerv)(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &attrib_ptr);

            if (!attrib_ptr)
            {
                vogl_error_printf("Enabled vertex attribute index %i has no vertex array buffer, and attribute pointer is NULL\n", i);
                continue;
            }

            GLint attrib_size = 0;
            GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib_size);

            GLint attrib_type = 0;
            GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &attrib_type);

            GLint attrib_stride = 0;
            GL_ENTRYPOINT(glGetVertexAttribiv)(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attrib_stride);

            uint32_t num_comps = 4;
            if ((attrib_size != GL_BGRA) && (attrib_size < 1) && (attrib_size > 4))
            {
                vogl_error_printf("Enabled vertex attribute index %i has invalid size 0x%0X\n", i, attrib_size);
                continue;
            }
            if ((attrib_size >= 1) && (attrib_size <= 4))
                num_comps = attrib_size;

            uint32_t type_size = vogl_get_gl_type_size(attrib_type);
            if (!type_size)
            {
                vogl_error_printf("Vertex attribute index %i has unsupported type 0x%0X\n", i, attrib_type);
                continue;
            }

            uint32_t stride = attrib_stride ? attrib_stride : (type_size * num_comps);

            uint32_t first_vertex_ofs = (start + basevertex) * stride;
            uint32_t last_vertex_ofs = (end + basevertex) * stride;
            uint32_t vertex_data_size = (last_vertex_ofs + stride) - first_vertex_ofs;

            if (g_dump_gl_buffers_flag)
            {
                vogl_log_printf("Client side vertex data for attrib %i (comps: %i type_size: %i stride: %i):\n", i, num_comps, type_size, stride);

                vogl_print_hex(static_cast<const uint8_t *>(attrib_ptr) + first_vertex_ofs, vertex_data_size, type_size);

                vogl_log_printf("\n");
            }

            if (trace_serializer.is_in_begin())
            {
                // TODO: Also send down start/end/first_vertex_ofs for debugging/verification purposes
                trace_serializer.add_key_value_blob(static_cast<uint16_t>(i), static_cast<const uint8_t *>(attrib_ptr) + first_vertex_ofs, vertex_data_size);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glDrawRangeElements, glDrawRangeElementsBaseVertex, glDrawRangeElementsEXT
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawRangeElementsBaseVertex(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_range_elements_base_vertex_helper(pContext, trace_serializer, #name, mode, start, end, count, type, indices, 0, true);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawRangeElements(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_range_elements_base_vertex_helper(pContext, trace_serializer, #name, mode, start, end, count, type, indices, 0, true);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawRangeElementsEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_range_elements_base_vertex_helper(pContext, trace_serializer, #name, mode, start, end, count, type, indices, 0, true);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawElements(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_range_elements_base_vertex_helper(pContext, trace_serializer, #name, mode, 0, 0, count, type, indices, 0, false);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawElementsInstanced(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_range_elements_base_vertex_helper(pContext, trace_serializer, #name, mode, 0, 0, count, type, indices, 0, false);

static inline void vogl_draw_range_elements_base_vertex_helper(
    vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, const char *pFunc,
    GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex, bool start_end_valid)
{
    if (trace_serializer.is_in_begin())
    {
        vogl_serialize_client_side_arrays_helper(pContext, trace_serializer, pFunc,
                                                mode, start, end, count, type, indices, basevertex, start_end_valid, true);
    }
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawArrays(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_arrays_helper(pContext, trace_serializer, #name, mode, first, count);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawArraysEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_arrays_helper(pContext, trace_serializer, #name, mode, first, count);
static void vogl_draw_arrays_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, const char *pFunc,
                                   GLenum mode, GLint first, GLsizei count)
{
    if (trace_serializer.is_in_begin())
    {
        vogl_serialize_client_side_arrays_helper(pContext, trace_serializer, pFunc,
                                                mode, first, first + count - 1, count, GL_UNSIGNED_BYTE, NULL, 0, true, false);
    }
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawArraysInstanced(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_arrays_instanced_helper(pContext, trace_serializer, #name, mode, first, count, instancecount);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDrawArraysInstancedEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_draw_arrays_instanced_helper(pContext, trace_serializer, #name, mode, start, count, primcount);
static void vogl_draw_arrays_instanced_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, const char *pFunc,
                                             GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
    VOGL_NOTE_UNUSED(primcount);
    if (trace_serializer.is_in_begin())
    {
        vogl_serialize_client_side_arrays_helper(pContext, trace_serializer, pFunc,
                                                mode, first, first + count - 1, count, GL_UNSIGNED_BYTE, NULL, 0, true, false);
    }
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glMultiDrawArrays(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_multi_draw_arrays_helper(pContext, trace_serializer, #name);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glMultiDrawArraysEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_multi_draw_arrays_helper(pContext, trace_serializer, #name);
static inline void vogl_multi_draw_arrays_helper(
    vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, const char *pFunc)
{
    if (trace_serializer.is_in_begin())
    {
        if (vogl_uses_client_side_arrays(pContext, false))
        {
            vogl_warning_printf("Function \"%s\" uses client side arrays, which is not currently supported. This call will not replay properly.\n", pFunc);
        }
    }
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glMultiDrawElements(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_multi_draw_elements_helper(pContext, trace_serializer, #name);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glMultiDrawElementsEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_multi_draw_elements_helper(pContext, trace_serializer, #name);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glMultiDrawElementsBaseVertex(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_multi_draw_elements_helper(pContext, trace_serializer, #name);
static void vogl_multi_draw_elements_helper(
    vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, const char *pFunc)
{
    if (trace_serializer.is_in_begin())
    {
        if (vogl_uses_client_side_arrays(pContext, true))
        {
            vogl_warning_printf("Function \"%s\" uses client side arrays, which is not currently supported. This call will not replay properly.\n", pFunc);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// String (extension manipulation)
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGetString(exported, category, ret, ret_type_enum, num_params, func_name, args, params) vogl_get_string_helper(#func_name, pContext, result, name, 0);
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGetStringi(exported, category, ret, ret_type_enum, num_params, func_name, args, params) vogl_get_string_helper(#func_name, pContext, result, name, index);
static inline void vogl_get_string_helper(const char *pFunc_name, vogl_context *pVOGL_context, const GLubyte *&pResult, GLenum name, GLuint index)
{
    if (!g_disable_gl_program_binary_flag)
        return;

    if ((pVOGL_context) && (pResult) && (name == GL_EXTENSIONS))
    {
        // Can't modify the driver's string directly, results in random crashes on my system. So we need to make a copy.
        char *pLoc = strstr((char *)pResult, "GL_ARB_get_program_binary");
        if (pLoc)
        {
            dynamic_string id(cVarArg, "%s_%x_%x", pFunc_name, name, index);

            dynamic_string &ext_str = pVOGL_context->get_extension_map()[id];
            ext_str.set((char *)pResult);

            int ofs = ext_str.find_left("GL_ARB_get_program_binary", true);
            if (ofs >= 0)
            {
                ext_str.set_char(ofs + 3, 'X');
                ext_str.set_char(ofs + 4, 'X');
                ext_str.set_char(ofs + 5, 'X');
                vogl_warning_printf("Disabled GL_ARB_get_program_binary by changing its name to GL_XXX_get_program_binary.\n");

                pResult = (const GLubyte *)ext_str.get_ptr();
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Buffer mapping, updating
//----------------------------------------------------------------------------------------------------------------------

// TODO:
// glMapNamedBufferEXT
// glMapNamedBufferRangeEXT
// glFlushMappedNamedBufferRangeEXT
// glUnmapNamedBufferEXT
// glGetNamedBufferSubDataEXT

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glNamedBufferDataEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_named_buffer_create_helper(pContext, trace_serializer, VBCM_BufferData, buffer, size, data, usage, 0);
static inline void vogl_named_buffer_create_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, VoglBufferCreateMethod create_method, GLuint buffer, GLsizeiptr size, const GLvoid *data, GLenum usage, GLbitfield flags)
{
    VOGL_NOTE_UNUSED(trace_serializer);

    if (!pContext)
        return;

    if (g_dump_gl_buffers_flag)
    {
        vogl_log_printf("Buffer data (size: %" PRIu64 "):\n", static_cast<uint64_t>(size));
        vogl_print_hex(data, size, 1);
        vogl_log_printf("\n");
    }

    gl_buffer_desc &buf_desc = pContext->get_or_create_buffer_desc(buffer);
    buf_desc.m_create_method = create_method;
    buf_desc.m_size = size;
    buf_desc.m_usage = usage;
    buf_desc.m_flags = flags;

    if (buf_desc.m_pMap)
    {
        vogl_warning_printf("Setting buffer's data on an already mapped buffer, buffer will get unmapped by GL\n");
    }

    buf_desc.m_pMap = NULL;
    buf_desc.m_map_ofs = 0;
    buf_desc.m_map_size = 0;
    buf_desc.m_map_access = 0;
    buf_desc.m_map_range = 0;
    buf_desc.m_flushed_ranges.resize(0);
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glBufferData(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_buffer_create_helper(pContext, trace_serializer, VBCM_BufferData, target, size, data, usage, 0);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glBufferDataARB(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_buffer_create_helper(pContext, trace_serializer, VBCM_BufferData, target, size, data, usage, 0);
static inline void vogl_buffer_create_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, VoglBufferCreateMethod create_method, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage, GLbitfield flags)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint buffer = vogl_get_bound_gl_buffer(target);
    if (!buffer)
    {
        vogl_error_printf("No mapped buffer at target 0x%08X\n", target);
        return;
    }

    vogl_named_buffer_create_helper(pContext, trace_serializer, create_method, buffer, size, data, usage, flags);
}

static inline void vogl_named_buffer_subdata_ext_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLuint buffer, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
    VOGL_NOTE_UNUSED(buffer);
    VOGL_NOTE_UNUSED(trace_serializer);
    VOGL_NOTE_UNUSED(pContext);

    if (g_dump_gl_buffers_flag)
    {
        vogl_log_printf("Buffer sub data (offset: %" PRIu64 " size: %" PRIu64 "):\n", static_cast<uint64_t>(offset), static_cast<uint64_t>(size));
        vogl_print_hex(data, size, 1);
        vogl_log_printf("\n");
    }
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glBufferSubData(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_buffer_subdata_helper(pContext, trace_serializer, target, offset, size, data);
static inline void vogl_buffer_subdata_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint buffer = vogl_get_bound_gl_buffer(target);
    if (!buffer)
    {
        vogl_error_printf("No mapped buffer at target 0x%08X\n", target);
        return;
    }

    vogl_named_buffer_subdata_ext_helper(pContext, trace_serializer, buffer, offset, size, data);
}

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glMapBuffer(exported, category, ret, ret_type_enum, num_params, name, args, params) \
    GLenum orig_access = access;                                                                                          \
    vogl_map_buffer_gl_prolog_helper(pContext, trace_serializer, target, access);
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glMapBufferARB(exported, category, ret, ret_type_enum, num_params, name, args, params) \
    GLenum orig_access = access;                                                                                             \
    vogl_map_buffer_gl_prolog_helper(pContext, trace_serializer, target, access);
static inline void vogl_map_buffer_gl_prolog_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLenum target, GLenum &access)
{
    VOGL_NOTE_UNUSED(target);
    VOGL_NOTE_UNUSED(pContext);

    if (trace_serializer.is_in_begin() || g_dump_gl_buffers_flag)
    {
        if (access == GL_WRITE_ONLY)
        {
            access = GL_READ_WRITE;
        }
    }
}

#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glMapBuffer(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_map_buffer_gl_epilog_helper(pContext, target, orig_access, result);
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glMapBufferARB(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_map_buffer_gl_epilog_helper(pContext, target, orig_access, result);
static inline void vogl_map_buffer_gl_epilog_helper(vogl_context *pContext, GLenum target, GLenum access, GLvoid *pPtr)
{
    if (!pContext)
        return;

    if (!pPtr)
    {
        vogl_warning_printf("Map failed!\n");
        return;
    }

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint buffer = vogl_get_bound_gl_buffer(target);
    if (!buffer)
    {
        vogl_error_printf("No mapped buffer at target 0x%08X\n", target);
        return;
    }

    gl_buffer_desc &buf_desc = pContext->get_or_create_buffer_desc(buffer);
    if (buf_desc.m_pMap)
    {
        vogl_warning_printf("Buffer 0x%08X is already mapped!\n", buffer);
        return;
    }

    // We need to sync with the underlying GL here to retreive the actual, true size of the mapped buffer in case an error occurred earlier and we didn't actually record the true size of the buffer.
    GLint64 actual_buf_size = buf_desc.m_size;
    GL_ENTRYPOINT(glGetBufferParameteri64v)(target, GL_BUFFER_SIZE, &actual_buf_size);

    buf_desc.m_size = actual_buf_size;
    buf_desc.m_pMap = pPtr;
    buf_desc.m_map_ofs = 0;
    buf_desc.m_map_size = actual_buf_size;
    buf_desc.m_map_access = access;
    buf_desc.m_map_range = false;
}

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glMapBufferRange(exported, category, ret, ret_type_enum, num_params, name, args, params) \
    GLbitfield orig_access = access;                                                                                           \
    vogl_map_buffer_range_gl_prolog_helper(pContext, trace_serializer, target, offset, length, access);
static inline void vogl_map_buffer_range_gl_prolog_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield &access)
{
    VOGL_NOTE_UNUSED(length);
    VOGL_NOTE_UNUSED(offset);
    VOGL_NOTE_UNUSED(target);
    VOGL_NOTE_UNUSED(pContext);

    if (trace_serializer.is_in_begin() || g_dump_gl_buffers_flag)
    {
        if (access & GL_MAP_WRITE_BIT)
        {
            // They are going to write, so we need to be able to read the data.
            // TODO: Warn user that we're going to not invalidate, which is definitely going to slow the GL driver down with CPU/GPU syncs.
            access &= ~GL_MAP_INVALIDATE_RANGE_BIT;
            access &= ~GL_MAP_INVALIDATE_BUFFER_BIT;
            access &= ~GL_MAP_UNSYNCHRONIZED_BIT;
            access |= GL_MAP_READ_BIT;
        }
    }
}

#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glMapBufferRange(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_map_buffer_range_gl_epilog_helper(pContext, target, offset, length, orig_access, result);
static inline void vogl_map_buffer_range_gl_epilog_helper(vogl_context *pContext, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield &access, GLvoid *pPtr)
{
    if (!pContext)
        return;

    if (!pPtr)
    {
        vogl_error_printf("vogl_map_buffer_range_gl_epilog_helper() Map failed!\n");
        return;
    }

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint buffer = vogl_get_bound_gl_buffer(target);
    if (!buffer)
    {
        vogl_error_printf("No mapped buffer at target 0x%08X\n", target);
        return;
    }

    gl_buffer_desc &buf_desc = pContext->get_or_create_buffer_desc(buffer);
    if (buf_desc.m_pMap)
    {
        vogl_error_printf("Buffer 0x%08X is already mapped!\n", buffer);
        return;
    }

    if (length > buf_desc.m_size)
    {
        vogl_warning_printf("passed in length parameter (%" PRIi64 ") is larger the buffer 0x%08X's recorded size (%" PRIi64 ")!\n",
                            static_cast<int64_t>(length), buffer, buf_desc.m_size);

        GLint64 actual_buf_size = buf_desc.m_size;
        GL_ENTRYPOINT(glGetBufferParameteri64v)(target, GL_BUFFER_SIZE, &actual_buf_size);
        buf_desc.m_size = actual_buf_size;
    }

    buf_desc.m_pMap = pPtr;
    buf_desc.m_map_ofs = offset;
    buf_desc.m_map_size = length;
    buf_desc.m_map_access = access;
    buf_desc.m_map_range = true;
}

#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glFlushMappedBufferRange(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_flush_mapped_buffer_range(pContext, target, offset, length);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glFlushMappedBufferRangeAPPLE(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_flush_mapped_buffer_range(pContext, target, offset, size);
static inline void vogl_flush_mapped_buffer_range(vogl_context *pContext, GLenum target, GLintptr offset, GLsizeiptr length)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint buffer = vogl_get_bound_gl_buffer(target);
    if (!buffer)
    {
        vogl_error_printf("No mapped buffer at target 0x%08X\n", target);
        return;
    }

    gl_buffer_desc &buf_desc = pContext->get_or_create_buffer_desc(buffer);
    if (!buf_desc.m_pMap)
    {
        vogl_error_printf("Buffer 0x%08X is not currently mapped!\n", buffer);
        return;
    }

    if ((offset + length) > buf_desc.m_map_size)
    {
        vogl_warning_printf("passed in offset (%" PRIi64 ") and/or length (%" PRIi64 ") parameters are out of range vs. buffer 0x%08X's recorded map size (%" PRIi64 ")!\n",
                            static_cast<int64_t>(offset), static_cast<int64_t>(length), buffer, buf_desc.m_map_size);
    }

    buf_desc.m_flushed_ranges.push_back(gl_buffer_desc::flushed_range(offset, length));
}

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glUnmapBuffer(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_unmap_buffer_helper(pContext, trace_serializer, target);
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glUnmapBufferARB(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_unmap_buffer_helper(pContext, trace_serializer, target);
static inline void vogl_unmap_buffer_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLenum target)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLuint buffer = vogl_get_bound_gl_buffer(target);
    if (!buffer)
    {
        vogl_error_printf("No mapped buffer at target 0x%08X\n", target);
        return;
    }

    gl_buffer_desc &buf_desc = pContext->get_or_create_buffer_desc(buffer);
    if (!buf_desc.m_pMap)
    {
        vogl_error_printf("Buffer 0x%08X is not currently mapped!\n", buffer);
        return;
    }

    bool writable_map = false;
    bool explicit_flush = false;
    if (buf_desc.m_map_range)
    {
        writable_map = (buf_desc.m_map_access & GL_MAP_WRITE_BIT) != 0;
        explicit_flush = (buf_desc.m_map_access & GL_MAP_FLUSH_EXPLICIT_BIT) != 0;
    }
    else
    {
        writable_map = (buf_desc.m_map_access != GL_READ_ONLY);
    }

    if (trace_serializer.is_in_begin())
    {
        trace_serializer.add_key_value(string_hash("map_access"), buf_desc.m_map_access);
        trace_serializer.add_key_value(string_hash("map_range"), buf_desc.m_map_range);
        trace_serializer.add_key_value(string_hash("explicit_flush"), explicit_flush);
        trace_serializer.add_key_value(string_hash("writable_map"), writable_map);
    }

    if (writable_map)
    {
        if (explicit_flush)
        {
            if (!buf_desc.m_flushed_ranges.size())
            {
                vogl_warning_printf("Mapped buffer at target 0x%08X was mapped with GL_MAP_FLUSH_EXPLICIT_BIT, but no ranges where actually flushed\n", target);
            }

            if (g_dump_gl_buffers_flag)
            {
                for (uint32_t i = 0; i < buf_desc.m_flushed_ranges.size(); i++)
                {
                    vogl_log_printf("Flushed buffer data (ofs: %" PRIu64 " size: %" PRIu64 "):\n", buf_desc.m_flushed_ranges[i].m_ofs, buf_desc.m_flushed_ranges[i].m_size);
                    vogl_print_hex(static_cast<const uint8_t *>(buf_desc.m_pMap) + buf_desc.m_flushed_ranges[i].m_ofs, buf_desc.m_flushed_ranges[i].m_size, 1);
                    vogl_log_printf("\n");
                }
            }

            if (trace_serializer.is_in_begin())
            {
                trace_serializer.add_key_value(string_hash("flushed_ranges"), buf_desc.m_flushed_ranges.size());
                for (uint32_t i = 0; i < buf_desc.m_flushed_ranges.size(); i++)
                {
                    int key_index = i * 4;
                    trace_serializer.add_key_value(key_index, buf_desc.m_flushed_ranges[i].m_ofs);
                    trace_serializer.add_key_value(key_index + 1, buf_desc.m_flushed_ranges[i].m_size);
                    // TODO
                    VOGL_ASSERT(buf_desc.m_flushed_ranges[i].m_size <= cUINT32_MAX);
                    trace_serializer.add_key_value_blob(key_index + 2, static_cast<const uint8_t *>(buf_desc.m_pMap) + buf_desc.m_flushed_ranges[i].m_ofs, static_cast<uint32_t>(buf_desc.m_flushed_ranges[i].m_size));
                }
            }
        }
        else
        {
            if (g_dump_gl_buffers_flag)
            {
                vogl_log_printf("Flushed buffer data (ofs: %" PRIu64 " size: %" PRIu64 "):\n", cast_val_to_uint64(buf_desc.m_map_ofs), cast_val_to_uint64(buf_desc.m_map_size));
                vogl_print_hex(static_cast<const uint8_t *>(buf_desc.m_pMap), buf_desc.m_map_size, 1);
                vogl_log_printf("\n");
            }

            if (trace_serializer.is_in_begin())
            {
                trace_serializer.add_key_value(0, buf_desc.m_map_ofs);
                trace_serializer.add_key_value(1, buf_desc.m_map_size);
                // TODO
                VOGL_ASSERT(buf_desc.m_map_size <= cUINT32_MAX);
                trace_serializer.add_key_value_blob(2, static_cast<const uint8_t *>(buf_desc.m_pMap), static_cast<uint32_t>(buf_desc.m_map_size));
            }
        }
    }

    buf_desc.m_pMap = NULL;
    buf_desc.m_map_ofs = 0;
    buf_desc.m_map_size = 0;
    buf_desc.m_map_access = 0;
    buf_desc.m_flushed_ranges.resize(0);
}

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBufferStorage(exported, category, ret, ret_type_enum, num_params, name, args, params) \
    GLbitfield orig_flags = flags;                                                                                            \
    VOGL_NOTE_UNUSED(orig_flags);                                                                                             \
    vogl_buffer_storage_gl_prolog_helper(pContext, trace_serializer, target, size, data, flags);
static inline void vogl_buffer_storage_gl_prolog_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLenum target, GLsizeiptr size, const GLvoid * data, GLbitfield& flags)
{
    VOGL_NOTE_UNUSED(pContext);
    VOGL_NOTE_UNUSED(target);
    VOGL_NOTE_UNUSED(size);
    VOGL_NOTE_UNUSED(target);

    if (trace_serializer.is_in_begin() || g_dump_gl_buffers_flag)
    {
        if (flags & GL_MAP_WRITE_BIT)
        {
            // They are going to write, so we need to be able to read the data.
            flags |= GL_MAP_READ_BIT;
        }
    }
}

// Deal with ARB_buffer_storage creation. 
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glNamedBufferStorageEXT(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_named_buffer_create_helper(pContext, trace_serializer, VBCM_BufferStorage, buffer, size, data, 0, flags);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glBufferStorage(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_buffer_create_helper(pContext, trace_serializer, VBCM_BufferStorage, target, size, data, 0, flags);

//----------------------------------------------------------------------------------------------------------------------
// glCreateProgram/glCreateProgramARB function epilog
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glCreateProgram(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        vogl_create_program_helper(pContext, result);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glCreateProgramObjectARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                           \
        vogl_create_program_helper(pContext, result);
static inline void vogl_create_program_helper(vogl_context *pContext, GLuint handle)
{
    VOGL_NOTE_UNUSED(pContext);
    VOGL_NOTE_UNUSED(handle);

#if 0
	if (handle)
	{
		vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
		VOGL_NOTE_UNUSED(gl_error_absorber);

		// Ensure program bins are always retrievable
		GL_ENTRYPOINT(glProgramParameteri)(handle, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	}
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// glProgramParameteri GL prolog
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glProgramParameteri(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                    \
        vogl_program_parameteri_prolog(pContext, program, pname, value);
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glProgramParameteriARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                       \
        vogl_program_parameteri_prolog(pContext, program, pname, value);
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glProgramParameteriEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                       \
        vogl_program_parameteri_prolog(pContext, program, pname, value);
static void vogl_program_parameteri_prolog(vogl_context *pContext, GLuint program, GLenum pname, GLint &value)
{
    VOGL_NOTE_UNUSED(pContext);
    VOGL_NOTE_UNUSED(program);
    VOGL_NOTE_UNUSED(pname);
    VOGL_NOTE_UNUSED(value);

#if 0
	if ((pname == GL_PROGRAM_BINARY_RETRIEVABLE_HINT) && (value == GL_FALSE))
	{
		vogl_warning_printf("Client is trying to set program %u's GL_PROGRAM_BINARY_RETRIEVABLE_HINT to GL_FALSE, the tracer is overriding this to GL_TRUE\n", program);

		value = GL_TRUE;
	}
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// glBindAttribLocationARB/glBindAttribLocation function epilog
//----------------------------------------------------------------------------------------------------------------------
//#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glBindAttribLocationARB(e, c, rt, r, nu, ne, a, p) if (pContext) pContext->bind_attrib_location(programObj, index, name);
//#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glBindAttribLocation(e, c, rt, r, nu, ne, a, p) if (pContext) pContext->bind_attrib_location(program, index, reinterpret_cast<const char *>(name));

//----------------------------------------------------------------------------------------------------------------------
// glDeleteProgramsARB/glDeleteProgram function epilog
//----------------------------------------------------------------------------------------------------------------------
//#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDeleteProgramsARB(e, c, rt, r, nu, ne, a, p) if (pContext) pContext->delete_program_attrib_locations(n, programs);
//#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glDeleteProgram(e, c, rt, r, nu, ne, a, p) if (pContext) pContext->delete_program_attrib_locations(1, &program);

//----------------------------------------------------------------------------------------------------------------------
// vogl_dump_program_outputs
//----------------------------------------------------------------------------------------------------------------------
static void vogl_dump_program_outputs(json_node &doc_root, vogl_context *pContext, GLuint program)
{
    if (pContext->get_context_info().supports_extension("GL_ARB_program_interface_query") &&
        GL_ENTRYPOINT(glGetProgramInterfaceiv) && GL_ENTRYPOINT(glGetProgramResourceName) && GL_ENTRYPOINT(glGetProgramResourceiv))
    {
        GLint num_active_outputs = 0;
        GL_ENTRYPOINT(glGetProgramInterfaceiv)(program, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &num_active_outputs);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("total_active_outputs", num_active_outputs);

        json_node &outputs_object = doc_root.add_array("active_outputs");
        for (int i = 0; i < num_active_outputs; i++)
        {
            GLchar name[256];
            GLsizei name_len = 0;
            GL_ENTRYPOINT(glGetProgramResourceName)(program, GL_PROGRAM_OUTPUT, i, sizeof(name), &name_len, name);
            pContext->peek_and_drop_gl_error();

            const GLenum props_to_query[5] = { GL_LOCATION, GL_LOCATION_INDEX, GL_TYPE, GL_ARRAY_SIZE, GL_IS_PER_PATCH }; // TODO: GL_LOCATION_COMPONENT
            GLint props[5] = { 0, 0, 0, 0, 0 };
            GL_ENTRYPOINT(glGetProgramResourceiv)(program, GL_PROGRAM_OUTPUT, i, VOGL_ARRAY_SIZE(props_to_query), props_to_query, VOGL_ARRAY_SIZE(props), NULL, props);
            pContext->peek_and_drop_gl_error();

            json_node &output_node = outputs_object.add_object();
            output_node.add_key_value("index", i);
            output_node.add_key_value("name", reinterpret_cast<const char *>(name));
            output_node.add_key_value("location", props[0]);
            output_node.add_key_value("location_index", props[1]);
            output_node.add_key_value("type", props[2]);
            output_node.add_key_value("array_size", props[3]);
            output_node.add_key_value("is_per_patch", props[4]);
            //output_node.add_key_value("location_component", props[5]);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glLinkProgramARB function epilog
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glLinkProgramARB(e, c, rt, r, nu, ne, a, p) vogl_link_program_arb(pContext, trace_serializer, programObj);
static void vogl_link_program_arb(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLhandleARB programObj)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLint link_status = 0;
    GL_ENTRYPOINT(glGetObjectParameterivARB)(programObj, GL_OBJECT_LINK_STATUS_ARB, &link_status);
    pContext->peek_and_drop_gl_error();

    if (trace_serializer.is_in_begin())
    {
        json_document doc;
        json_node &doc_root = *doc.get_root();
        doc_root.add_key_value("program", programObj);
        doc_root.add_key_value("link_status", link_status);
        doc_root.add_key_value("func_id", VOGL_ENTRYPOINT_glLinkProgramARB);

        GLint active_attributes = 0;
        GL_ENTRYPOINT(glGetObjectParameterivARB)(programObj, GL_OBJECT_ACTIVE_ATTRIBUTES_ARB, &active_attributes);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("total_active_attributes", active_attributes);

        if (active_attributes)
        {
            json_node &attribs_object = doc_root.add_array("active_attribs");

            for (int i = 0; i < active_attributes; i++)
            {
                GLint size = 0;
                GLenum type = 0;
                GLcharARB name[256];

                GL_ENTRYPOINT(glGetActiveAttribARB)(programObj, i, sizeof(name), NULL, &size, &type, name);
                pContext->peek_and_drop_gl_error();

                if ((!name[0]) || ((name[0] == 'g') && (name[1] == 'l') && (name[2] == '_')))
                    continue;

                GLint location = GL_ENTRYPOINT(glGetAttribLocationARB)(programObj, name);
                pContext->peek_and_drop_gl_error();

                if (location < 0)
                    continue;

                json_node &attrib_node = attribs_object.add_object();
                attrib_node.add_key_value("index", i);
                attrib_node.add_key_value("name", reinterpret_cast<const char *>(name));
                attrib_node.add_key_value("location", location);
            }
        }

        GLint active_uniforms = 0;
        GL_ENTRYPOINT(glGetObjectParameterivARB)(programObj, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &active_uniforms);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("total_active_uniforms", active_uniforms);

        if (active_uniforms)
        {
            json_node &uniforms_object = doc_root.add_array("active_uniforms");

            for (int i = 0; i < active_uniforms; i++)
            {
                GLsizei length = 0;
                GLint size = 0;
                GLenum type = 0;
                GLcharARB name[256];

                GL_ENTRYPOINT(glGetActiveUniformARB)(programObj, i, sizeof(name), &length, &size, &type, name);
                pContext->peek_and_drop_gl_error();

                if ((!name[0]) || (!length) || ((name[0] == 'g') && (name[1] == 'l') && (name[2] == '_')))
                    continue;

                GLint location = GL_ENTRYPOINT(glGetUniformLocationARB)(programObj, name);
                pContext->peek_and_drop_gl_error();

                if (location < 0)
                    continue;

                json_node &uniform_node = uniforms_object.add_object();
                uniform_node.add_key_value("index", i);
                uniform_node.add_key_value("name", reinterpret_cast<const char *>(name));
                uniform_node.add_key_value("location", location);
                uniform_node.add_key_value("size", size);
                uniform_node.add_key_value("type", type);
            }
        }

        vogl_dump_program_outputs(doc_root, pContext, programObj);

        trace_serializer.add_key_value_json_document("metadata", doc);
    }

    if (programObj)
    {
        if ((link_status) || (!pContext->has_linked_program_snapshot(programObj)))
        {
            if (!pContext->add_linked_program_snapshot(VOGL_ENTRYPOINT_glLinkProgramARB, programObj))
                vogl_error_printf("Failed snapshotting program into link-time program shadow table, program 0x%X\n", programObj);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glLinkProgram/glProgramBinary function epilog
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glLinkProgram(e, c, rt, r, nu, ne, a, p) vogl_link_program_epilog_helper(pContext, trace_serializer, program, VOGL_ENTRYPOINT_##ne, GL_NONE, NULL, 0, GL_NONE, 0, NULL);
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glProgramBinary(e, c, rt, r, nu, ne, a, p) vogl_link_program_epilog_helper(pContext, trace_serializer, program, VOGL_ENTRYPOINT_##ne, binaryFormat, binary, length, GL_NONE, 0, NULL);
static void vogl_link_program_epilog_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLuint program, gl_entrypoint_id_t id, GLenum binary_format, const GLvoid *pBinary, GLsizei binary_length, GLenum create_shader_program_type, GLsizei count, GLchar *const *strings)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    GLint link_status = 0;
    GL_ENTRYPOINT(glGetProgramiv)(program, GL_LINK_STATUS, &link_status);
    pContext->peek_and_drop_gl_error();

    if (trace_serializer.is_in_begin())
    {
        json_document doc;
        json_node &doc_root = *doc.get_root();
        doc_root.add_key_value("program", program);
        doc_root.add_key_value("link_status", link_status);
        doc_root.add_key_value("func_id", static_cast<uint32_t>(id));

        // Active uniform blocks
        GLint active_uniform_blocks = 0;
        GL_ENTRYPOINT(glGetProgramiv)(program, GL_ACTIVE_UNIFORM_BLOCKS, &active_uniform_blocks);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("active_uniform_blocks", active_uniform_blocks);

        // Active attributes
        GLint active_attributes = 0;
        GL_ENTRYPOINT(glGetProgramiv)(program, GL_ACTIVE_ATTRIBUTES, &active_attributes);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("total_active_attributes", active_attributes);

        if (active_attributes)
        {
            json_node &attribs_object = doc_root.add_array("active_attribs");

            for (int i = 0; i < active_attributes; i++)
            {
                GLint size = 0;
                GLenum type = 0;
                GLchar name[256];

                GL_ENTRYPOINT(glGetActiveAttrib)(program, i, sizeof(name), NULL, &size, &type, name);
                pContext->peek_and_drop_gl_error();

                if ((!name[0]) || ((name[0] == 'g') && (name[1] == 'l') && (name[2] == '_')))
                    continue;

                GLint location = GL_ENTRYPOINT(glGetAttribLocation)(program, name);
                pContext->peek_and_drop_gl_error();

                if (location < 0)
                    continue;

                json_node &attrib_node = attribs_object.add_object();
                attrib_node.add_key_value("index", i);
                attrib_node.add_key_value("name", reinterpret_cast<const char *>(name));
                attrib_node.add_key_value("location", location);
            }
        }

        // Active uniforms
        GLint active_uniforms = 0;
        GL_ENTRYPOINT(glGetProgramiv)(program, GL_ACTIVE_UNIFORMS, &active_uniforms);
        doc_root.add_key_value("total_active_uniforms", active_uniforms);

        if (active_uniforms)
        {
            json_node &uniforms_object = doc_root.add_array("active_uniforms");

            for (int i = 0; i < active_uniforms; i++)
            {
                GLsizei length = 0;
                GLint size = 0;
                GLenum type = 0;
                GLchar name[256];

                GL_ENTRYPOINT(glGetActiveUniform)(program, i, sizeof(name), &length, &size, &type, name);
                pContext->peek_and_drop_gl_error();

                if ((!name[0]) || (!length) || ((name[0] == 'g') && (name[1] == 'l') && (name[2] == '_')))
                    continue;

                GLint location = GL_ENTRYPOINT(glGetUniformLocation)(program, name);
                pContext->peek_and_drop_gl_error();

                if (location < 0)
                    continue;

                json_node &uniform_node = uniforms_object.add_object();
                uniform_node.add_key_value("index", i);
                uniform_node.add_key_value("name", reinterpret_cast<const char *>(name));
                uniform_node.add_key_value("location", location);
                uniform_node.add_key_value("size", size);
                uniform_node.add_key_value("type", type);
            }
        }

        // Program outputs
        vogl_dump_program_outputs(doc_root, pContext, program);

        // Transform feedback
        GLint mode = GL_NONE;
        GL_ENTRYPOINT(glGetProgramiv)(program, GL_TRANSFORM_FEEDBACK_BUFFER_MODE, &mode);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("transform_feedback_mode", get_gl_enums().find_gl_name(mode));

        GLint num_varyings = 0;
        GL_ENTRYPOINT(glGetProgramiv)(program, GL_TRANSFORM_FEEDBACK_VARYINGS, &num_varyings);
        pContext->peek_and_drop_gl_error();

        doc_root.add_key_value("transform_feedback_num_varyings", num_varyings);

        if (num_varyings)
        {
            json_node &transform_feedback_varyings = doc_root.add_array("transform_feedback_varyings");

            for (GLint i = 0; i < num_varyings; i++)
            {
                GLchar name[512];
                GLsizei length = 0, size = 0;
                GLenum type = GL_NONE;

                GL_ENTRYPOINT(glGetTransformFeedbackVarying)(program, i, sizeof(name), &length, &size, &type, name);
                pContext->peek_and_drop_gl_error();

                json_node &uniform_node = transform_feedback_varyings.add_object();
                uniform_node.add_key_value("index", i);
                uniform_node.add_key_value("name", reinterpret_cast<const char *>(name));
                uniform_node.add_key_value("size", size);
                uniform_node.add_key_value("type", type);
            }
        }

        // Add JSON document to packet
        trace_serializer.add_key_value_json_document("metadata", doc);
    }

    if (program)
    {
        if ((link_status) || (!pContext->has_linked_program_snapshot(program)))
        {
            if (id == VOGL_ENTRYPOINT_glProgramBinary)
            {
                if (!pContext->add_linked_program_snapshot(id, program, binary_format, pBinary, binary_length))
                {
                    vogl_error_printf("Failed snapshotting binary program into link-time program shadow table, program 0x%X\n", program);
                }
            }
            else if (id == VOGL_ENTRYPOINT_glCreateShaderProgramv)
            {
                if (!pContext->add_linked_program_snapshot(id, program, create_shader_program_type, count, strings))
                {
                    vogl_error_printf("Failed snapshotting program into link-time program shadow table, program 0x%X\n", program);
                }
            }
            else
            {
                if (!pContext->add_linked_program_snapshot(id, program))
                {
                    vogl_error_printf("Failed snapshotting program into link-time program shadow table, program 0x%X\n", program);
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glCreateShaderProgramv
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glCreateShaderProgramv(exported, category, ret, ret_type_enum, num_params, name, args, params) vogl_create_shader_programv_epilog_helper(pContext, trace_serializer, type, count, strings, result);
static void vogl_create_shader_programv_epilog_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLenum type, GLsizei count, GLchar *const *strings, GLuint program)
{
    vogl_serialize_shader_source(trace_serializer, count, (const GLcharARB *const *)strings, NULL);

    if (program)
    {
        pContext->add_program(program);

        vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
        VOGL_NOTE_UNUSED(gl_error_absorber);

        vogl_link_program_epilog_helper(pContext, trace_serializer, program, VOGL_ENTRYPOINT_glCreateShaderProgramv, GL_NONE, NULL, 0, type, count, strings);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glProgramBinary
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_PROLOG_glProgramBinary(e, c, rt, r, nu, ne, a, p) vogl_glProgramBinary_prolog(program, binaryFormat, binary, length);
static inline void vogl_glProgramBinary_prolog(GLuint program, GLenum binaryFormat, const void *&pBinary, GLsizei &length)
{
    VOGL_NOTE_UNUSED(binaryFormat);

    if (g_disable_gl_program_binary_flag)
    {
        vogl_debug_printf("Tracer is forcing a bogus program binary for program %d\n", program);

        // These will intentionally cause the program binary to not link, forcing the application to use shader strings instead of binaries.
        pBinary = &g_dummy_program;
        length = 1;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glTransformFeedbackVaryings func epilog
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_FUNC_EPILOG_glTransformFeedbackVaryings(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                              \
        vogl_transform_feedback_varyings(pContext, trace_serializer, count, varyings);
static inline void vogl_transform_feedback_varyings(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, GLsizei count, GLchar *const *pVaryings)
{
    VOGL_NOTE_UNUSED(pContext);

    if ((!count) || (!pVaryings) || (!trace_serializer.is_in_begin()))
        return;

    dynamic_string varying;
    for (int i = 0; i < count; i++)
    {
        varying.empty();
        if (pVaryings[i])
            varying = reinterpret_cast<const char *>(pVaryings[i]);

        trace_serializer.add_key_value(i, varying);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// ARB program shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenProgramsARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenProgramsARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->gen_arb_programs(n, programs);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteProgramsARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                    \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteProgramsARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                           \
        pContext->del_arb_programs(n, programs);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindProgramARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindProgramARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->bind_arb_program(target, program);

//----------------------------------------------------------------------------------------------------------------------
// Program pipeline shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenProgramPipelines(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenProgramPipelines(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->gen_program_pipelines(n, pipelines);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteProgramPipelines(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                    \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteProgramPipelines(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                           \
        pContext->del_program_pipelines(n, pipelines);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindProgramPipelines(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindProgramPipelines(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->bind_program_pipeline(pipeline);

//----------------------------------------------------------------------------------------------------------------------
// renderbuffer shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenRenderbuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                   \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenRenderbuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                          \
        pContext->gen_render_buffers(n, renderbuffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenRenderbuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                      \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenRenderbuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                             \
        pContext->gen_render_buffers(n, renderbuffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteRenderbuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                      \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteRenderbuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                             \
        pContext->del_render_buffers(n, renderbuffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteRenderbuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                         \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteRenderbuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                                \
        pContext->del_render_buffers(n, renderbuffers);
//----------------------------------------------------------------------------------------------------------------------
// Buffer shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenBuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                             \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenBuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                    \
        pContext->gen_buffers(n, buffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenBuffersARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenBuffersARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                       \
        pContext->gen_buffers(n, buffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBuffer(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                             \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBuffer(e, c, rt, r, nu, ne, a, p)                                           \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferBase(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferBase(e, c, rt, r, nu, ne, a, p)                                       \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferBaseEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                    \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferBaseEXT(e, c, rt, r, nu, ne, a, p)                                    \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferBaseNV(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                   \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferBaseNV(e, c, rt, r, nu, ne, a, p)                                     \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferRange(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferRange(e, c, rt, r, nu, ne, a, p)                                      \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferRangeEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                     \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferRangeEXT(e, c, rt, r, nu, ne, a, p)                                   \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferRangeNV(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                    \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferRangeNV(e, c, rt, r, nu, ne, a, p)                                    \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindBufferARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindBufferARB(e, c, rt, r, nu, ne, a, p)                                        \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_buffer(target, buffer);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteBuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteBuffers(e, c, rt, r, nu, ne, a, p)                                        \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->delete_buffers(n, buffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteBuffersARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                   \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteBuffersARB(e, c, rt, r, nu, ne, a, p)                                     \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->delete_buffers(n, buffers);

//----------------------------------------------------------------------------------------------------------------------
// Texture handle shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenTextures(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                              \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenTextures(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                     \
        pContext->gen_textures(n, textures);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenTexturesEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenTexturesEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->gen_textures(n, textures);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteTextures(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteTextures(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->del_textures(n, textures);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteTexturesEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                    \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteTexturesEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                           \
        pContext->del_textures(n, textures);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindTexture(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                              \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindTexture(e, c, rt, r, nu, ne, a, p)                                          \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_texture(target, texture);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindTextureEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindTextureEXT(e, c, rt, r, nu, ne, a, p)                                       \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_texture(target, texture);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindMultiTextureEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                      \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindMultiTextureEXT(e, c, rt, r, nu, ne, a, p)                                  \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_texture(target, texture);

//----------------------------------------------------------------------------------------------------------------------
// Framebuffer handle shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenFramebuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenFramebuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                         \
        pContext->gen_framebuffers(n, framebuffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenFramebuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                     \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenFramebuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                            \
        pContext->gen_framebuffers(n, framebuffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteFramebuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                     \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteFramebuffers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                            \
        pContext->del_framebuffers(n, framebuffers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteFramebuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                        \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteFramebuffersEXT(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                               \
        pContext->del_framebuffers(n, framebuffers);

//----------------------------------------------------------------------------------------------------------------------
// VAO handle shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenVertexArrays(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenVertexArrays(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                         \
        pContext->gen_vertexarrays(n, arrays);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteVertexArrays(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                     \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteVertexArrays(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                            \
        pContext->del_vertexarrays(n, arrays);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBindVertexArray(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBindVertexArray(e, c, rt, r, nu, ne, a, p)                                      \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->bind_vertexarray(array);

//----------------------------------------------------------------------------------------------------------------------
// Sync handle shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glFenceSync(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                            \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glFenceSync(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                   \
        pContext->gen_sync(result);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteSync(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                             \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteSync(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                    \
        pContext->del_sync(sync);

//----------------------------------------------------------------------------------------------------------------------
// Sampler object handle shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenSamplers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                              \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenSamplers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                     \
        pContext->gen_samplers(count, samplers);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteSamplers(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                 \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteSamplers(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                        \
        pContext->del_samplers(count, samplers);

//----------------------------------------------------------------------------------------------------------------------
// Query handle shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenQueries(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                             \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenQueries(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                    \
        pContext->gen_queries(n, ids);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenQueriesARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenQueriesARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                       \
        pContext->gen_queries(n, ids);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteQueries(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteQueries(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                       \
        pContext->del_queries(n, ids);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteQueriesARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                   \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteQueriesARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                          \
        pContext->del_queries(n, ids);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBeginQuery(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                             \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBeginQuery(e, c, rt, r, nu, ne, a, p)                                           \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->begin_query(target, id);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBeginQueryARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBeginQueryARB(e, c, rt, r, nu, ne, a, p)                                        \
    if (pContext && !pContext->peek_and_record_gl_error() && (pContext->get_current_display_list_mode() != GL_COMPILE)) \
        pContext->begin_query(target, id);

//----------------------------------------------------------------------------------------------------------------------
// Display list shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glGenLists(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                           \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glGenLists(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                  \
        pContext->gen_lists(result, range);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glDeleteLists(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                              \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glDeleteLists(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                     \
        pContext->del_lists(list, range);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glNewList(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                          \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glNewList(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                 \
        pContext->new_list(list, mode);

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glEndList(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                          \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glEndList(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                 \
        pContext->end_list();

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glXUseXFont(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                            \
        pContext->peek_and_record_gl_error();

//----------------------------------------------------------------------------------------------------------------------
// glBegin/glEnd shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glBegin(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                        \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glBegin(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                        \
        pContext->set_in_gl_begin(true);

//#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glEnd(e, c, rt, r, nu, ne, a, p)
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glEnd(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                      \
        pContext->set_in_gl_begin(false);

//----------------------------------------------------------------------------------------------------------------------
// Program/shader shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glCreateProgram(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        result = pContext->handle_create_program(VOGL_ENTRYPOINT_##ne);           \
    else                                                                         \
        result = 0;
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glCreateProgramObjectARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                         \
        result = pContext->handle_create_program(VOGL_ENTRYPOINT_##ne);                    \
    else                                                                                  \
        result = 0;

#define DEF_FUNCTION_CUSTOM_GL_CALLER_glCreateShader(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                               \
        result = pContext->handle_create_shader(VOGL_ENTRYPOINT_##ne, type);     \
    else                                                                        \
        result = 0;
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glCreateShaderObjectARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                        \
        result = pContext->handle_create_shader(VOGL_ENTRYPOINT_##ne, shaderType);        \
    else                                                                                 \
        result = 0;

#define DEF_FUNCTION_CUSTOM_GL_CALLER_glUseProgram(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                             \
        pContext->handle_use_program(VOGL_ENTRYPOINT_##ne, program);
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glUseProgramObjectARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                      \
        pContext->handle_use_program(VOGL_ENTRYPOINT_##ne, programObj);

#define DEF_FUNCTION_CUSTOM_GL_CALLER_glDeleteShader(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                               \
        pContext->handle_del_shader(VOGL_ENTRYPOINT_##ne, shader);
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glDeleteProgram(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                \
        pContext->handle_del_program(VOGL_ENTRYPOINT_##ne, program);
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glDeleteObjectARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        pContext->handle_del_object(VOGL_ENTRYPOINT_##ne, obj);

#define DEF_FUNCTION_CUSTOM_GL_CALLER_glDetachShader(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                               \
        pContext->handle_detach_shader(VOGL_ENTRYPOINT_##ne, program, shader);
#define DEF_FUNCTION_CUSTOM_GL_CALLER_glDetachObjectARB(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                                  \
        pContext->handle_detach_shader(VOGL_ENTRYPOINT_##ne, containerObj, attachedObj);

//----------------------------------------------------------------------------------------------------------------------
// Client side array usage detection
//----------------------------------------------------------------------------------------------------------------------
#define VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE                              \
    if ((!g_disable_client_side_array_tracing) && (pContext) && (pointer)) \
        vogl_check_for_client_side_array_usage(pContext, pointer);
static inline void vogl_check_for_client_side_array_usage(vogl_context *pContext, const void *pPointer)
{
    VOGL_NOTE_UNUSED(pPointer);

    if (pContext->get_uses_client_side_arrays() || pContext->is_core_profile())
        return;

    pContext->peek_and_record_gl_error();

    GLint cur_array_buf_binding = 0;
    GL_ENTRYPOINT(glGetIntegerv)(GL_ARRAY_BUFFER_BINDING, &cur_array_buf_binding);

    if (pContext->peek_and_drop_gl_error() == GL_NO_ERROR)
    {
        if (!cur_array_buf_binding)
        {
            pContext->set_uses_client_side_arrays(true);
            vogl_warning_printf("Client side array usage has been detected, this will negatively impact tracing performance, "
                                "use --vogl_disable_client_side_array_tracing to disable\n");
        }
    }
}

#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glNormalPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glColorPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glIndexPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glTexCoordPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glEdgeFlagPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glFogCoordPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glSecondaryColorPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glInterleavedArrays(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glNormalPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glColorPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glIndexPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glTexCoordPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glEdgeFlagPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glFogCoordPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glSecondaryColorPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribPointerARB(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribPointerNV(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribIPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribIPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribLPointer(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glVertexAttribLPointerEXT(e, c, rt, r, nu, ne, a, p) VOGL_CHECK_FOR_CLIENT_SIDE_ARRAY_USAGE

//----------------------------------------------------------------------------------------------------------------------
// glXUseXFont shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glXUseXFont(e, c, rt, r, nu, ne, a, p) vogl_glx_use_xfont_epilog_helper(pContext, trace_serializer, font, first, count, list_base);
static void vogl_glx_use_xfont_epilog_helper(vogl_context *pContext, vogl_entrypoint_serializer &trace_serializer, Font font, int first, int count, int list_base)
{
    #if (VOGL_PLATFORM_HAS_GLX)
        if (!pContext)
            return;

        if (pContext->peek_and_record_gl_error())
            return;

        char *pFont = NULL;

        if (pContext->get_display())
        {
            XFontStruct *pFont_struct = XQueryFont((Display *)pContext->get_display(), font);

            if (pFont_struct)
            {
                unsigned long value = 0;
                Bool result = XGetFontProperty(pFont_struct, XA_FONT, &value);
                if (result)
                {
                    pFont = (char *)XGetAtomName((Display *)pContext->get_display(), (Atom)value);
                    if ((pFont) && (trace_serializer.is_in_begin()))
                    {
                        trace_serializer.add_key_value("font_name", pFont);
                    }
                }
            }

            XFreeFontInfo(NULL, pFont_struct, 1);
        }

        pContext->glx_font(pFont, first, count, list_base);
    #else
        VOGL_VERIFY(!"Somehow got into vogl_glx_use_xfont_epilog_helper on a platform that doesn't have GLX, which is very surprising.");
    #endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_display_list_bind_callback
//----------------------------------------------------------------------------------------------------------------------
static void vogl_display_list_bind_callback(vogl_namespace_t handle_namespace, GLenum target, GLuint handle, void *pOpaque)
{
    vogl_context *pContext = static_cast<vogl_context *>(pOpaque);

    // TODO: We don't whitelist anything but texture binds in display lists currently.
    switch (handle_namespace)
    {
        case VOGL_NAMESPACE_TEXTURES:
        {
            if ((handle) && (target != GL_NONE))
                pContext->bind_texture_conditionally(target, handle);
            break;
        }
        default:
        {
            vogl_warning_printf("TODO: Unsupported bind in display list, namespace %s target %s handle %u\n", vogl_get_namespace_name(handle_namespace), get_gl_enums().find_gl_name(target), handle);
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// glCallList shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glCallList(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                           \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glCallList(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                  \
        vogl_gl_call_list_helper(pContext, list);
static void vogl_gl_call_list_helper(vogl_context *pContext, GLuint list)
{
    if (!pContext)
        return;

    pContext->parse_list_and_update_shadows(list, vogl_display_list_bind_callback, pContext);
}

//----------------------------------------------------------------------------------------------------------------------
// glCallLists shadowing
//----------------------------------------------------------------------------------------------------------------------
#define DEF_FUNCTION_CUSTOM_GL_PROLOG_glCallLists(e, c, rt, r, nu, ne, a, p) \
    if (pContext)                                                            \
        pContext->peek_and_record_gl_error();
#define DEF_FUNCTION_CUSTOM_GL_EPILOG_glCallLists(e, c, rt, r, nu, ne, a, p) \
    if (pContext && !pContext->peek_and_record_gl_error())                   \
        vogl_gl_call_lists_helper(pContext, n, type, lists);
static void vogl_gl_call_lists_helper(vogl_context *pContext, GLsizei n, GLenum type, const GLvoid *lists)
{
    if (!pContext)
        return;

    vogl_scoped_gl_error_absorber gl_error_absorber(pContext);
    VOGL_NOTE_UNUSED(gl_error_absorber);

    pContext->parse_lists_and_update_shadows(n, type, lists, vogl_display_list_bind_callback, pContext);
}

//----------------------------------------------------------------------------------------------------------------------
// Ensure all entrypoints are fully serializable
// This function MUST appear before #include "gl_glx_array_size_macros.inc", below
//----------------------------------------------------------------------------------------------------------------------
static void vogl_check_entrypoints()
{
    vogl_debug_printf("vogl_check_entrypoints: begin (specify --vogl_debug for more validation)\n");

    typedef vogl::hash_map<uint32_t, dynamic_string> array_size_macro_hashmap;
    array_size_macro_hashmap defined_array_size_macros;
    array_size_macro_hashmap undefined_array_size_macros;

#define CUSTOM_FUNC_HANDLER_DEFINED(id) g_vogl_entrypoint_descs[id].m_has_custom_func_handler = true; //printf("%u %s\n", id, g_vogl_entrypoint_descs[id].m_pName);
#define CUSTOM_FUNC_HANDLER_NOT_DEFINED(id)
#include "gl_glx_cgl_wgl_custom_func_handler_validator.inc"
#undef CUSTOM_FUNC_HANDLER_DEFINED
#undef CUSTOM_FUNC_HANDLER_NOT_DEFINED

#define VALIDATE_ARRAY_SIZE_MACRO_DEFINED(name, index) defined_array_size_macros.insert(index, #name);
#define VALIDATE_ARRAY_SIZE_MACRO_NOT_DEFINED(name, index) undefined_array_size_macros.insert(index, #name);
#include "gl_glx_cgl_wgl_array_size_macros_validator.inc"
#undef VALIDATE_ARRAY_SIZE_MACRO_DEFINED
#undef VALIDATE_ARRAY_SIZE_MACRO_NOT_DEFINED

    vogl::vector<uint32_t> undefined_func_return_array_size_macros;

#define CUSTOM_FUNC_RETURN_PARAM_ARRAY_SIZE_HANDLER_DEFINED(macro_name, func_name)
#define CUSTOM_FUNC_RETURN_PARAM_ARRAY_SIZE_HANDLER_NOT_DEFINED(macro_name, func_name) g_vogl_entrypoint_descs[VOGL_ENTRYPOINT_##func_name].m_custom_return_param_array_size_macro_is_missing = true;
#include "gl_glx_cgl_wgl_custom_return_param_array_size_macro_validator.inc"
#undef CUSTOM_FUNC_RETURN_PARAM_ARRAY_SIZE_HANDLER_DEFINED
#undef CUSTOM_FUNC_RETURN_PARAM_ARRAY_SIZE_HANDLER_NOT_DEFINED

    if (vogl::check_for_command_line_param("--vogl_debug"))
    {
        for (array_size_macro_hashmap::const_iterator it = undefined_array_size_macros.begin(); it != undefined_array_size_macros.end(); ++it)
        {
            uint32_t idx = it->first;
            const dynamic_string &name = it->second;
            VOGL_NOTE_UNUSED(name);

            gl_entrypoint_id_t func = static_cast<gl_entrypoint_id_t>(idx >> 16);
            uint32_t param = idx & 0xFFFF;

            vogl_warning_printf("Custom array size macro for func %u \"%s\" param %u has not been defined, this function cannot be traced\n",
                               func, g_vogl_entrypoint_descs[func].m_pName, param);

            g_vogl_entrypoint_param_descs[func][param].m_custom_array_size_macro_is_missing = true;
            g_vogl_entrypoint_descs[func].m_custom_array_size_macro_is_missing = true;
        }

        vogl_debug_printf("Undefined array size macros:\n");
        vogl_debug_printf("---\n");
        for (array_size_macro_hashmap::const_iterator it = undefined_array_size_macros.begin(); it != undefined_array_size_macros.end(); ++it)
        {
            const dynamic_string &name = it->second;
            vogl_debug_printf("%s\n", name.get_ptr());
        }
        vogl_debug_printf("---\n");

        vogl_debug_printf("Undefined return param array size macros:\n");
        vogl_debug_printf("---\n");
        for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; i++)
        {
            vogl_ctype_t return_ctype = g_vogl_entrypoint_descs[i].m_return_ctype;

            if (return_ctype == VOGL_VOID)
                continue;
            if (!get_vogl_process_gl_ctypes()[return_ctype].m_is_pointer)
                continue;
            //if (get_vogl_process_gl_ctypes()[return_ctype].m_is_opaque_pointer)
            //	continue;

            if (g_vogl_entrypoint_descs[i].m_custom_return_param_array_size_macro_is_missing)
            {
                vogl_debug_printf("%s, opaque_ptr: %u\n", g_vogl_entrypoint_descs[i].m_pName, get_vogl_process_gl_ctypes()[return_ctype].m_is_opaque_pointer);
            }

            if (g_vogl_entrypoint_descs[i].m_whitelisted_for_displaylists)
            {
                if (!g_vogl_entrypoint_descs[i].m_is_listable)
                {
                    vogl_debug_printf("Function %s is marked as whitelistable in display lists, but is not a listable function!\n", g_vogl_entrypoint_descs[i].m_pName);
                }
            }
        }
        vogl_debug_printf("---\n");

        vogl_debug_printf("Whitelisted funcs with undefined array size macros:\n");
        vogl_debug_printf("---\n");
        for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; i++)
        {
            const gl_entrypoint_desc_t &desc = g_vogl_entrypoint_descs[i];
            if ((!desc.m_is_whitelisted) || (desc.m_has_custom_func_handler))
                continue;

            if (desc.m_custom_array_size_macro_is_missing)
                vogl_debug_printf("%s\n", desc.m_pName);
        }
        vogl_debug_printf("---\n");

        vogl_debug_printf("Whitelisted funcs with undefined return param array size macros:\n");
        vogl_debug_printf("---\n");
        for (uint32_t i = 0; i < VOGL_NUM_ENTRYPOINTS; i++)
        {
            const gl_entrypoint_desc_t &desc = g_vogl_entrypoint_descs[i];
            if (desc.m_is_whitelisted)
                continue;

            vogl_ctype_t return_ctype = g_vogl_entrypoint_descs[i].m_return_ctype;

            if (return_ctype == VOGL_VOID)
                continue;
            if (!get_vogl_process_gl_ctypes()[return_ctype].m_is_pointer)
                continue;
            if (get_vogl_process_gl_ctypes()[return_ctype].m_is_opaque_pointer)
                continue;

            if (g_vogl_entrypoint_descs[i].m_custom_return_param_array_size_macro_is_missing)
            {
                vogl_debug_printf("%s\n", g_vogl_entrypoint_descs[i].m_pName);
            }
        }

        vogl_debug_printf("---\n");
    }

    vogl_debug_printf("vogl_check_entrypoints: done\n");
}

//----------------------------------------------------------------------------------------------------------------------
// Include generated macros to define the internal entrypoint funcs
//----------------------------------------------------------------------------------------------------------------------
#include "gl_glx_cgl_wgl_array_size_macros.inc"
#include "gl_glx_cgl_wgl_func_return_param_array_size_macros.inc"
#include "gl_glx_cgl_wgl_func_defs.inc"

#ifndef NO_PUBLIC_EXPORTS
//----------------------------------------------------------------------------------------------------------------------
// Declare exported gl/glx functions (each exported func immediately calls one of the internal vogl_* functions)
//----------------------------------------------------------------------------------------------------------------------
#define DEF_PROTO_EXPORTED(ret, name, args, params) \
    VOGL_API_EXPORT ret VOGL_API_CALLCONV name args \
    {                                               \
        return VOGL_GLUER(vogl_, name) params;      \
    }
#define DEF_PROTO_EXPORTED_VOID(ret, name, args, params) \
    VOGL_API_EXPORT ret VOGL_API_CALLCONV name args      \
    {                                                    \
        VOGL_GLUER(vogl_, name) params;                  \
    }
#define DEF_PROTO_INTERNAL(ret, name, args, params)
#define DEF_PROTO_INTERNAL_VOID(ret, name, args, params)
#define DEF_FUNCTION_BEGIN(exported, category, ret, ret_type_enum, num_params, name, args, params) exported(ret, name, args, params)
#define DEF_FUNCTION_BEGIN_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params) exported(ret, name, args, params)
#define DEF_FUNCTION_INIT(exported, category, ret, ret_type_enum, num_params, name, args, params)
#define DEF_FUNCTION_INIT_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params)
#define DEF_FUNCTION_INPUT_VALUE_PARAM(idx, spectype, type, type_enum, param)
#define DEF_FUNCTION_INPUT_REFERENCE_PARAM(idx, spectype, type, type_enum, param)
#define DEF_FUNCTION_INPUT_ARRAY_PARAM(idx, spectype, type, type_enum, param, size)
#define DEF_FUNCTION_OUTPUT_REFERENCE_PARAM(idx, spectype, type, type_enum, param)
#define DEF_FUNCTION_OUTPUT_ARRAY_PARAM(idx, spectype, type, type_enum, param, size)
#define DEF_FUNCTION_RETURN_PARAM(spectype, type, type_enum, size)
#define DEF_FUNCTION_CALL_GL(exported, category, ret, ret_type_enum, num_params, name, args, params)
#define DEF_FUNCTION_CALL_GL_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params)
#define DEF_FUNCTION_END(exported, category, ret, ret_type_enum, num_params, name, args, params)
#define DEF_FUNCTION_END_VOID(exported, category, ret, ret_type_enum, num_params, name, args, params)
#define DEF_FUNCTION_PARAM_COMPUTE_ARRAY_SIZE_GL_ENUM(pname) -1

#include "gl_glx_cgl_wgl_array_size_macros.inc"
#include "gl_glx_cgl_wgl_func_defs.inc"
#endif

//----------------------------------------------------------------------------------------------------------------------
// Define our exported gliGetProcAddressRAD function
//----------------------------------------------------------------------------------------------------------------------
VOGL_API_EXPORT __GLXextFuncPtr gliGetProcAddressRAD(const GLubyte *procName)
{
    if ((procName) && (!vogl_strcmp(reinterpret_cast<const char *>(procName), "gliGetProcAddressRAD")))
        return (__GLXextFuncPtr)gliGetProcAddressRAD;

    return vogl_glXGetProcAddressARB(procName);
}

//----------------------------------------------------------------------------------------------------------------------
// Determine addresses of our gl/glx wrapper functions
//----------------------------------------------------------------------------------------------------------------------
static void vogl_init_wrapper_func_ptrs()
{
    gl_entrypoint_desc_t *pDst = g_vogl_entrypoint_descs;

#define DEF_PROTO(exported, category, ret, ret_type, num_params, name, args, params) \
    pDst->m_pWrapper_func = reinterpret_cast<vogl_void_func_ptr_t>(vogl_##name);       \
    ++pDst;
#define DEF_PROTO_VOID(exported, category, ret, ret_type, num_params, name, args, params) \
    pDst->m_pWrapper_func = reinterpret_cast<vogl_void_func_ptr_t>(vogl_##name);            \
    ++pDst;
#include "gl_glx_cgl_wgl_protos.inc"
#undef DEF_PROTO
#undef DEF_PROTO_VOID
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_direct_gl_func_prolog - This function is called before EVERY single GL/GLX function call we make.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_direct_gl_func_prolog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **ppStack_data)
{
    VOGL_NOTE_UNUSED(pUser_data);

    if (g_dump_gl_calls_flag)
        vogl_log_printf("** GLPROLOG %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);

    gl_entrypoint_id_t *pPrev_state = reinterpret_cast<gl_entrypoint_id_t *>(ppStack_data);
    *pPrev_state = VOGL_ENTRYPOINT_INVALID;

    vogl_thread_local_data *pTLS_data = vogl_get_or_create_thread_local_data();
    if (pTLS_data)
    {
        *pPrev_state = pTLS_data->m_calling_driver_entrypoint_id;
        pTLS_data->m_calling_driver_entrypoint_id = entrypoint_id;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_direct_gl_func_epilog - This function is called immediately after EVERY single GL/GLX function call we make.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_direct_gl_func_epilog(gl_entrypoint_id_t entrypoint_id, void *pUser_data, void **ppStack_data)
{
    VOGL_NOTE_UNUSED(entrypoint_id);
    VOGL_NOTE_UNUSED(pUser_data);

#if 0
    if (entrypoint_id == VOGL_ENTRYPOINT_glXMakeCurrent)
	{
		// HACK HACK - crude test of the "calling driver entrypoint code"
		glXGetCurrentContext();
	}
#endif

    if (g_dump_gl_calls_flag)
        vogl_log_printf("** GLEPILOG %s\n", g_vogl_entrypoint_descs[entrypoint_id].m_pName);

    gl_entrypoint_id_t *pPrev_state = reinterpret_cast<gl_entrypoint_id_t *>(ppStack_data);

    vogl_thread_local_data *pTLS_data = vogl_get_or_create_thread_local_data();
    if (pTLS_data)
    {
        pTLS_data->m_calling_driver_entrypoint_id = *pPrev_state;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Initialization
// Note: Be VERY careful what you do in here! It's called very early during init (long before main, during c++ init)
//----------------------------------------------------------------------------------------------------------------------
void vogl_early_init()
{
    vogl_init_thread_local_data();

    vogl_set_direct_gl_func_prolog(vogl_direct_gl_func_prolog, NULL);
    vogl_set_direct_gl_func_epilog(vogl_direct_gl_func_epilog, NULL);

    vogl_init_wrapper_func_ptrs();

    vogl_check_entrypoints();
}

