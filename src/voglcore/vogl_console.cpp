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
#include "vogl_threading.h"
#include "vogl_command_line_params.h"
#include "vogl_strutils.h"

#ifdef PLATFORM_WINDOWS
    #include <tchar.h>
    #include <conio.h>
#endif

#if VOGL_USE_LINUX_API
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
    #include <stropts.h>
    #include <sys/ioctl.h>
#endif

using namespace vogl;

data_stream *console::m_pLog_stream;
uint32_t console::m_num_output_funcs = 0;
uint32_t console::m_num_messages[cMsgTotal];
//$ TODO: Default to verbose for now. Theoretically, this should be cMsgWarning but
// tons of people are calling vogl_message_printf() and relying on that getting printed.
eConsoleMessageType console::m_output_level = cMsgVerbose;
bool console::m_at_beginning_of_line = true;
bool console::m_prefixes = true;
char console::m_tool_prefix[256];
const uint32_t cConsoleBufSize = 256 * 1024;
const uint32_t cMaxOutputFuncs = 16;

static const char *s_msgnames[cMsgTotal] =
{
    "Info",
    "Progress",
    "Header",
    "Error",
    "Warning",
    "Verbose",
    "Debug",
};

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

const char *console::get_message_type_str(eConsoleMessageType type)
{
    if (type >= 0 && (type < VOGL_ARRAY_SIZE(s_msgnames)))
        return s_msgnames[type];
    return "??";
}

eConsoleMessageType console::get_message_type_from_str(const char *str)
{
    dynamic_string msgname = str;

    for (uint32_t type = 0; type < VOGL_ARRAY_SIZE(s_msgnames); type++)
    {
        if (msgname.compare(s_msgnames[type], false) == 0)
            return (eConsoleMessageType)type;
    }
    return (eConsoleMessageType)-1;
}

void console::vprintf(const char *caller_info, eConsoleMessageType type, const char *p, va_list args)
{
    get_mutex().lock();

    m_num_messages[type]++;

    static char buf[cConsoleBufSize];

    char *pDst = buf;
    size_t buf_left = sizeof(buf);

    if (m_prefixes && m_at_beginning_of_line)
    {
        if (m_tool_prefix[0])
        {
            size_t l = strlen(m_tool_prefix);
            memcpy(pDst, m_tool_prefix, l);
            pDst += l;
            buf_left -= l;
        }

        const char *pPrefix = NULL;
        switch (type)
        {
            case cMsgDebug:
                pPrefix = "Debug: ";
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
            size_t l = strlen(pPrefix);
            memcpy(pDst, pPrefix, l);
            pDst += l;
            buf_left -= l;
        }
    }

    if (m_at_beginning_of_line && caller_info)
    {
        // If we've got caller info, display it if this is an error message or
        //  they've cranked the level up to debug.
        if ((m_output_level >= cMsgDebug) || (type == cMsgError) || (type == cMsgWarning))
        {
            size_t l = strlen(caller_info);
            memcpy(pDst, caller_info, l);
            pDst += l;
            buf_left -= l;
        }
    }

    vogl::vogl_vsprintf_s(pDst, buf_left, p, args);

    bool handled = false;

    if (m_num_output_funcs)
    {
        console_func *funcs = get_output_funcs();

        for (uint32_t i = 0; i < m_num_output_funcs; i++)
        {
            if (funcs[i].m_func(type, buf, funcs[i].m_pData))
                handled = true;
        }
    }

    if (!handled && (type <= m_output_level))
    {
        FILE *pFile = (type == cMsgError) ? stderr : stdout;

        fputs(buf, pFile);
    }

    uint32_t n = static_cast<uint32_t>(strlen(buf));
    m_at_beginning_of_line = n && (buf[n - 1] == '\n');

    if ((type != cMsgProgress) && m_pLog_stream)
    {
        // Yes this is bad.
        dynamic_string tmp_buf(buf);

        tmp_buf.translate_lf_to_crlf();

        m_pLog_stream->printf("%s", tmp_buf.get_ptr());
        m_pLog_stream->flush();
    }

    get_mutex().unlock();
}

void console::printf(const char *caller_info, eConsoleMessageType type, const char *p, ...)
{
    va_list args;
    va_start(args, p);
    vprintf(caller_info, type, p, args);
    va_end(args);
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
    printf("Unimplemented!\n");
    return 0;
}

int vogl::vogl_kbhit()
{
    VOGL_ASSERT_ALWAYS;
    printf("Unimplemented!\n");
    return 0;
}

#endif
