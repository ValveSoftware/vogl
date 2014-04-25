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

namespace vogl
{
    static const char *s_msgnames[cCMTTotal] =
        {
          "Debug",
          "Progress",
          "Info",
          "Default",
          "Message",
          "Warning",
          "Error",
          "Header1",
        };

    eConsoleMessageType console::m_default_category = cInfoConsoleMessage;
    uint console::m_num_output_funcs;
    console::console_func console::m_output_funcs[cMaxOutputFuncs];
    bool console::m_prefixes = true;
    bool console::m_output_disabled;
    data_stream *console::m_pLog_stream;
    mutex *console::m_pMutex;
    uint console::m_num_messages[cCMTTotal];
    bool console::m_at_beginning_of_line = true;
    char console::m_tool_prefix[256];

    const uint cConsoleBufSize = 256 * 1024;

    void console::init()
    {
        if (!m_pMutex)
        {
            m_pMutex = vogl_new(mutex);
        }
    }

    void console::deinit()
    {
        if (m_pMutex)
        {
            vogl_delete(m_pMutex);
            m_pMutex = NULL;
        }

        m_num_output_funcs = 0;
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

        for (uint type = 0; type < VOGL_ARRAY_SIZE(s_msgnames); type++)
        {
            if (msgname.compare(s_msgnames[type], false) == 0)
                return (eConsoleMessageType)type;
        }
        return (eConsoleMessageType)-1;
    }

    void console::vprintf(eConsoleMessageType type, const char *p, va_list args)
    {
        init();

        if (m_pMutex)
            m_pMutex->lock();

        m_num_messages[type]++;

        static char buf[cConsoleBufSize];

        char *pDst = buf;
        uint buf_left = sizeof(buf);

        if ((m_prefixes) && (m_at_beginning_of_line))
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
                case cDebugConsoleMessage:
                    pPrefix = "Debug: ";
                    break;
                case cWarningConsoleMessage:
                    pPrefix = "Warning: ";
                    break;
                case cErrorConsoleMessage:
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

        vogl::vogl_vsprintf_s(pDst, buf_left, p, args);

        bool handled = false;

        if (m_num_output_funcs)
        {
            for (uint i = 0; i < m_num_output_funcs; i++)
                if (m_output_funcs[i].m_func(type, buf, m_output_funcs[i].m_pData))
                    handled = true;
        }

        if ((!m_output_disabled) && (!handled))
        {
            FILE *pFile = (type == cErrorConsoleMessage) ? stderr : stdout;

            fputs(buf, pFile);
        }

        uint n = static_cast<uint>(strlen(buf));
        m_at_beginning_of_line = (n) && (buf[n - 1] == '\n');

        if ((type != cProgressConsoleMessage) && (m_pLog_stream))
        {
            // Yes this is bad.
            dynamic_string tmp_buf(buf);

            tmp_buf.translate_lf_to_crlf();

            m_pLog_stream->printf("%s", tmp_buf.get_ptr());
            m_pLog_stream->flush();
        }

        if (m_pMutex)
            m_pMutex->unlock();
    }

    void console::printf(eConsoleMessageType type, const char *p, ...)
    {
        va_list args;
        va_start(args, p);
        vprintf(type, p, args);
        va_end(args);
    }

    void console::printf(const char *p, ...)
    {
        va_list args;
        va_start(args, p);
        vprintf(m_default_category, p, args);
        va_end(args);
    }

    void console::set_default_category(eConsoleMessageType category)
    {
        init();

        m_default_category = category;
    }

    eConsoleMessageType console::get_default_category()
    {
        init();

        return m_default_category;
    }

    void console::add_console_output_func(console_output_func pFunc, void *pData)
    {
        init();

        if (m_pMutex)
            m_pMutex->lock();

        VOGL_ASSERT(m_num_output_funcs < cMaxOutputFuncs);

        if (m_num_output_funcs < cMaxOutputFuncs)
        {
            m_output_funcs[m_num_output_funcs++] = console_func(pFunc, pData);
        }

        if (m_pMutex)
            m_pMutex->unlock();
    }

    void console::remove_console_output_func(console_output_func pFunc)
    {
        init();

        if (m_pMutex)
            m_pMutex->lock();

        for (int i = m_num_output_funcs - 1; i >= 0; i--)
        {
            if (m_output_funcs[i].m_func == pFunc)
            {
                m_output_funcs[i] = m_output_funcs[m_num_output_funcs - 1];
                m_num_output_funcs--;
            }
        }

        if (m_pMutex)
            m_pMutex->unlock();
    }

#define MESSAGE_VPRINTF_IMPL(_msgtype) \
        va_list args; \
        va_start(args, p); \
        vprintf(_msgtype, p, args); \
        va_end(args)

    void console::progress(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cProgressConsoleMessage); }
    void console::info(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cInfoConsoleMessage); }
    void console::message(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cMessageConsoleMessage); }
    void console::cons(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cConsoleConsoleMessage); }
    void console::debug(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cDebugConsoleMessage); }
    void console::warning(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cWarningConsoleMessage); }
    void console::error(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cErrorConsoleMessage); }
    void console::header1(const char *p, ...) { MESSAGE_VPRINTF_IMPL(cHeader1ConsoleMessage); }

#undef MESSAGE_VPRINTF_IMPL
    
#if VOGL_USE_WIN32_API
    int vogl_getch()
    {
        return _getch();
    }

    int vogl_kbhit()
    {
        return _kbhit();
    }
#elif VOGL_USE_LINUX_API
    int vogl_getch()
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
    int vogl_kbhit()
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
    int vogl_getch()
    {
        VOGL_ASSERT_ALWAYS;
        printf("Unimplemented!\n");
        return 0;
    }

    int vogl_kbhit()
    {
        VOGL_ASSERT_ALWAYS;
        printf("Unimplemented!\n");
        return 0;
    }
#endif

} // namespace vogl
