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

// File: vogl_console.h
#pragma once

#include "vogl_core.h"
#include "vogl_dynamic_string.h"

namespace vogl
{
    class dynamic_string;
    class data_stream;
    class mutex;

    enum eConsoleMessageType
    {
        cDebugConsoleMessage,    // debugging messages
        cProgressConsoleMessage, // progress messages
        cInfoConsoleMessage,     // ordinary messages
        cConsoleConsoleMessage,  // user console output
        cMessageConsoleMessage,  // high importance messages
        cWarningConsoleMessage,  // warnings
        cErrorConsoleMessage,    // errors
        cHeader1ConsoleMessage,  // header1
        cCMTTotal
    };

    typedef bool (*console_output_func)(eConsoleMessageType type, const char *pMsg, void *pData);

    class console
    {
    public:
        static void init();
        static void deinit();

        static bool is_initialized()
        {
            return m_pMutex != NULL;
        }

        static const char *get_message_type_str(eConsoleMessageType type);
        static eConsoleMessageType get_message_type_from_str(const char *str);

        static void set_default_category(eConsoleMessageType category);
        static eConsoleMessageType get_default_category();

        static void add_console_output_func(console_output_func pFunc, void *pData);
        static void remove_console_output_func(console_output_func pFunc);

        static void printf(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);

        static void vprintf(eConsoleMessageType type, const char *p, va_list args);
        static void printf(eConsoleMessageType type, const char *p, ...) VOGL_ATTRIBUTE_PRINTF(2, 3);

        static void cons(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void debug(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void progress(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void info(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void message(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void warning(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void error(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);
        static void header1(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);

        static void disable_prefixes()
        {
            m_prefixes = false;
        }
        static void enable_prefixes()
        {
            m_prefixes = true;
        }
        static bool get_prefixes()
        {
            return m_prefixes;
        }
        static bool get_at_beginning_of_line()
        {
            return m_at_beginning_of_line;
        }

        static void set_tool_prefix(const char *pPrefix)
        {
            strcpy(m_tool_prefix, pPrefix);
        }
        static const char *get_tool_prefix()
        {
            return m_tool_prefix;
        }

        static void disable_output()
        {
            m_output_disabled = true;
        }
        static void enable_output()
        {
            m_output_disabled = false;
        }
        static bool get_output_disabled()
        {
            return m_output_disabled;
        }

        static void set_log_stream(data_stream *pStream)
        {
            m_pLog_stream = pStream;
        }
        static data_stream *get_log_stream()
        {
            return m_pLog_stream;
        }

        static uint get_total_messages(eConsoleMessageType type)
        {
            return m_num_messages[type];
        }

    private:
        static eConsoleMessageType m_default_category;

        struct console_func
        {
            console_func(console_output_func func = NULL, void *pData = NULL)
                : m_func(func), m_pData(pData)
            {
            }

            console_output_func m_func;
            void *m_pData;
        };

        enum
        {
            cMaxOutputFuncs = 16
        };
        static uint m_num_output_funcs;
        static console_func m_output_funcs[cMaxOutputFuncs];

        static bool m_prefixes, m_output_disabled;
        static data_stream *m_pLog_stream;
        static mutex *m_pMutex;
        static uint m_num_messages[cCMTTotal];
        static bool m_at_beginning_of_line;
        static char m_tool_prefix[256];
    };

    int vogl_getch();
    int vogl_kbhit();

} // namespace vogl
