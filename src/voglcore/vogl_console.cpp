/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
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

// File: vogl_console.cpp
#include "vogl_core.h"
#include "vogl_console.h"
#include "vogl_data_stream.h"
#include "vogl_cfile_stream.h"
#include "vogl_threading.h"
#include "vogl_command_line_params.h"
#include "vogl_strutils.h"
#include "vogl_file_utils.h"
#include "vogl_port.h"

#ifdef PLATFORM_WINDOWS
    #include <tchar.h>
    #include <conio.h>
#endif

#if VOGL_USE_LINUX_API
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#if defined _XOPEN_STREAMS && _XOPEN_STREAMS != -1
    #include <stropts.h>
#endif
    #include <sys/ioctl.h>
#endif

using namespace vogl;

data_stream *console::m_pLog_stream;
uint32_t console::m_num_output_funcs = 0;
uint32_t console::m_num_messages[cMsgTotal];
eConsoleMessageType console::m_output_level = cMsgWarning;
bool console::m_at_beginning_of_line = true;
bool console::m_prefixes = true;
bool console::m_caller_info_always = false;
char console::m_tool_prefix[256];
const uint32_t cConsoleBufSize = 256 * 1024;
const uint32_t cMaxOutputFuncs = 16;

struct console_func
{
    console_func(console_output_func func = NULL, void *pData = NULL)
        : m_func(func), m_pData(pData)
    {
    }

    console_output_func m_func;
    void *m_pData;
};

static inline mutex &get_mutex()
{
    static mutex s_mutex;
    return s_mutex;
}

void console::init()
{
}

void console::deinit()
{
    m_num_output_funcs = 0;
    m_at_beginning_of_line = false;
}

static inline console_func *get_output_funcs()
{
    static console_func s_output_funcs[cMaxOutputFuncs];
    return s_output_funcs;
}

void console::vprintf(const char *caller_info, uint32_t msgtype, const char *p, va_list args)
{
    get_mutex().lock();
    
    eConsoleMessageType type = (eConsoleMessageType)(msgtype & cMsgMask);

    m_num_messages[type]++;

    static char buf[cConsoleBufSize];

    char *pDst = buf;
    size_t buf_left = sizeof(buf);

    if (m_prefixes && m_at_beginning_of_line)
    {
        if (m_tool_prefix[0])
        {
            size_t len = strlen(m_tool_prefix);
            memcpy(pDst, m_tool_prefix, len);
            pDst += len;
            buf_left -= len;
        }

        const char *pPrefix = NULL;
        if (msgtype & cMsgFlagOpenGL)
        {
            static const char opengl_prefix[] = "OGL ";
            size_t len = strlen(opengl_prefix);
            memcpy(pDst, opengl_prefix, len);
            pDst += len;
            buf_left -= len;
        }

        switch (type)
        {
            case cMsgDebug:
                pPrefix = "Debug: ";
                break;
            case cMsgVerbose:
                pPrefix = "Verbose: ";
                break;
            case cMsgWarning:
                pPrefix = "Warning: ";
                break;
            case cMsgError:
                pPrefix = "Error: ";
                break;
            default:
                break;
        }

        if (pPrefix)
        {
            size_t len = strlen(pPrefix);
            memcpy(pDst, pPrefix, len);
            pDst += len;
            buf_left -= len;
        }
    }

    if (m_at_beginning_of_line && caller_info)
    {
        // If we've got caller info, display it if this is an error message or
        //  they've cranked the level up to debug.
        if (m_caller_info_always ||
            (m_output_level >= cMsgDebug) ||
            (type == cMsgError) ||
            ((m_output_level == cMsgVerbose) && (type == cMsgWarning)))
        {
            size_t len = strlen(caller_info);
            memcpy(pDst, caller_info, len);
            pDst += len;
            buf_left -= len;
        }
    }

    vogl::vogl_vsprintf_s(pDst, buf_left, p, args);

    bool handled = false;
    bool do_logfile = m_pLog_stream && !(msgtype & cMsgFlagNoLog) && (type <= m_output_level);

    if (do_logfile && (msgtype & cMsgFlagLogOnly))
    {
        // Don't spew to screen - logfile only for this message.
    }
    else
    {
        if (m_num_output_funcs)
        {
            console_func *funcs = get_output_funcs();

            for (uint32_t i = 0; i < m_num_output_funcs; i++)
            {
                if (funcs[i].m_func(type, (msgtype & ~cMsgMask), buf, funcs[i].m_pData))
                    handled = true;
            }
        }

        if (!handled && (type <= m_output_level))
        {
            FILE *pFile = (type == cMsgError) ? stderr : stdout;

            fputs(buf, pFile);
        }
    }

    uint32_t n = static_cast<uint32_t>(strlen(buf));
    m_at_beginning_of_line = n && (buf[n - 1] == '\n');

    if (do_logfile)
    {
        // Yes this is bad.
        dynamic_string tmp_buf(buf);

        tmp_buf.translate_lf_to_crlf();

        m_pLog_stream->printf("%s", tmp_buf.get_ptr());
        m_pLog_stream->flush();
    }

    get_mutex().unlock();
}

void console::printf(const char *caller_info, uint32_t type, const char *p, ...)
{
    va_list args;
    va_start(args, p);
    vprintf(caller_info, type, p, args);
    va_end(args);
}

bool console::set_log_stream_filename(const char *filename, bool logfile_per_pid)
{
    static cfile_stream *s_vogl_pLog_stream = NULL;

    vogl::dynamic_string filename_pid;
    if (logfile_per_pid)
    {
        pid_t pid = plat_getpid();
        dynamic_string drive, dir, fname, ext;

        file_utils::split_path(filename, &drive, &dir, &fname, &ext);
        dynamic_string new_fname(cVarArg, "%s_%" PRIu64, fname.get_ptr(), (uint64_t)pid);
        file_utils::combine_path_and_extension(filename_pid, &drive, &dir, &new_fname, &ext);

        filename = filename_pid.c_str();
    }

    if (s_vogl_pLog_stream)
    {
        vogl_delete(s_vogl_pLog_stream);
        s_vogl_pLog_stream = NULL;

        console::set_log_stream(NULL);
    }

    // This purposely leaks, don't care
    s_vogl_pLog_stream = vogl_new(cfile_stream);

    if (!s_vogl_pLog_stream->open(filename, cDataStreamWritable, true))
    {
        vogl_error_printf("Failed opening log file \"%s\"\n", filename);

        vogl_delete(s_vogl_pLog_stream);
        s_vogl_pLog_stream = NULL;
        return false;
    }

    console::set_log_stream(s_vogl_pLog_stream);
    
    //$ TODO: Add a time here, etc?
    vogl_message_printf("Opened log file \"%s\"\n", filename);

    return true;
}

void console::add_console_output_func(console_output_func pFunc, void *pData)
{
    get_mutex().lock();

    VOGL_ASSERT(m_num_output_funcs < cMaxOutputFuncs);

    if (m_num_output_funcs < cMaxOutputFuncs)
    {
        console_func *funcs = get_output_funcs();
        funcs[m_num_output_funcs++] = console_func(pFunc, pData);
    }

    get_mutex().unlock();
}

void console::remove_console_output_func(console_output_func pFunc)
{
    get_mutex().lock();

    console_func *funcs = get_output_funcs();
    for (int i = m_num_output_funcs - 1; i >= 0; i--)
    {
        if (funcs[i].m_func == pFunc)
        {
            funcs[i] = funcs[m_num_output_funcs - 1];
            m_num_output_funcs--;
        }
    }

    get_mutex().unlock();
}

#if defined(VOGL_USE_WIN32_API)

int vogl::vogl_getch()
{
    return _getch();
}

int vogl::vogl_kbhit()
{
    return _kbhit();
}

#elif defined(VOGL_USE_LINUX_API)

int vogl::vogl_getch()
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// See http://www.flipcode.com/archives/_kbhit_for_Linux.shtml
int vogl::vogl_kbhit()
{
    static const int STDIN = 0;
    static bool initialized = false;

    if (!initialized)
    {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

#else

int vogl::vogl_getch()
{
    VOGL_ASSERT_ALWAYS;
    vogl_error_printf("Unimplemented!\n");
    return 0;
}

int vogl::vogl_kbhit()
{
    VOGL_ASSERT_ALWAYS;
    vogl_error_printf("Unimplemented!\n");
    return 0;
}

#endif
