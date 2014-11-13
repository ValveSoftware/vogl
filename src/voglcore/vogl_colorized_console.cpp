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

// File: vogl_colorized_console.cpp
#include "vogl_core.h"
#include "vogl_colorized_console.h"
#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

namespace vogl
{
    static vogl_exception_callback_t g_pPrev_exception_callback;

    void colorized_console::atexit_func()
    {
        set_default_color();
    }

    void colorized_console::exception_callback()
    {
        set_default_color();

        if (g_pPrev_exception_callback)
            (*g_pPrev_exception_callback)();
    }

    void colorized_console::init()
    {
        console::init();
        console::add_console_output_func(console_output_func, NULL);

        atexit(atexit_func);
    }

    void colorized_console::set_exception_callback()
    {
        g_pPrev_exception_callback = vogl_set_exception_callback(exception_callback);
    }

    void colorized_console::deinit()
    {
        console::remove_console_output_func(console_output_func);
        console::deinit();
    }

#ifdef VOGL_USE_WIN32_API

    void colorized_console::set_default_color()
    {
		HANDLE cons = GetStdHandle(STD_OUTPUT_HANDLE);
		if (INVALID_HANDLE_VALUE == cons)
			return;

        DWORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        SetConsoleTextAttribute(cons, (WORD)attr);
    }

    bool colorized_console::console_output_func(eConsoleMessageType type, uint32_t flags, const char *pMsg, void *pData)
    {
        pData;

        if (type > console::get_output_level())
            return true;

        HANDLE cons = GetStdHandle(STD_OUTPUT_HANDLE);

        DWORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        switch (type)
        {
            case cMsgDebug:
                attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
                break;
            case cMsgMessage:
                attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
                break;
            case cMsgWarning:
                attr = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
                break;
            case cMsgError:
                attr = FOREGROUND_RED | FOREGROUND_INTENSITY;
                break;
            default:
                break;
        }

        if (INVALID_HANDLE_VALUE != cons)
            SetConsoleTextAttribute(cons, (WORD)attr);

        fputs(pMsg, stdout);

        if (INVALID_HANDLE_VALUE != cons)
            SetConsoleTextAttribute(cons, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        return true;
    }

#elif VOGL_USE_LINUX_API

#define ANSI_RESET "\033[0m"
#define ANSI_BLACK "\033[30m"              /* Black */
#define ANSI_RED "\033[31m"                /* Red */
#define ANSI_GREEN "\033[32m"              /* Green */
#define ANSI_YELLOW "\033[33m"             /* Yellow */
#define ANSI_BLUE "\033[34m"               /* Blue */
#define ANSI_MAGENTA "\033[35m"            /* Magenta */
#define ANSI_CYAN "\033[36m"               /* Cyan */
#define ANSI_WHITE "\033[37m"              /* White */
#define ANSI_BOLDBLACK "\033[1m\033[30m"   /* Bold Black */
#define ANSI_BOLDRED "\033[1m\033[31m"     /* Bold Red */
#define ANSI_BOLDGREEN "\033[1m\033[32m"   /* Bold Green */
#define ANSI_BOLDYELLOW "\033[1m\033[33m"  /* Bold Yellow */
#define ANSI_BOLDBLUE "\033[1m\033[34m"    /* Bold Blue */
#define ANSI_BOLDMAGENTA "\033[1m\033[35m" /* Bold Magenta */
#define ANSI_BOLDCYAN "\033[1m\033[36m"    /* Bold Cyan */
#define ANSI_BOLDWHITE "\033[1m\033[37m"   /* Bold White */

    void colorized_console::set_default_color()
    {
        if (isatty(fileno(stdout)))
        {
            fputs(ANSI_RESET, stdout);
        }

        if (isatty(fileno(stderr)))
        {
            fputs(ANSI_RESET, stderr);
        }
    }

    static uint32_t get_message_type_from_str(const char *str)
    {
        static const char *s_msgnames[cMsgTotal + 1] =
        {
            "Info",     // cMsgPrint
            "Message",  // cMsgMessage
            "Error",    // cMsgError
            "Warning",  // cMsgWarning
            "Verbose",  // cMsgVerbose
            "Debug",    // cMsgDebug
            "Header",   // cMsgTotal
        };
        dynamic_string msgname = str;

        for (uint32_t type = 0; type < VOGL_ARRAY_SIZE(s_msgnames); type++)
        {
            if (msgname.compare(s_msgnames[type], false) == 0)
                return type;
        }
        return (uint32_t)-1;
    }

    bool colorized_console::console_output_func(eConsoleMessageType type, uint32_t flags, const char *pMsg, void *pData)
    {
        (void)pData;

        static bool g_colors_inited = false;
        static dynamic_string g_colors[cMsgTotal + 1];

        if (type > console::get_output_level())
            return true;

        if (!g_colors_inited)
        {
            g_colors_inited = true;

            g_colors[cMsgPrint] = "";                  // ordinary printf messages
            g_colors[cMsgMessage] = ANSI_BOLDCYAN;     // ordinary printf messages w/ color
            g_colors[cMsgError] = ANSI_BOLDRED;        // errors
            g_colors[cMsgWarning] = ANSI_BOLDYELLOW;   // warnings
            g_colors[cMsgVerbose] = "";                // verbose messages
            g_colors[cMsgDebug] = ANSI_BOLDBLUE;       // debugging messages
            g_colors[cMsgTotal] = "\033[44m\033[1m";   // header (dark blue background)

            // Use just like LS_COLORS. Example: VOGL_COLORS='warning=1;34:message=31'
            // Tokens are info, message, error, etc. (see above).
            const char *vogl_colors = getenv("VOGL_COLORS");
            if (vogl_colors)
            {
                dynamic_string_array tokens;
                dynamic_string colors = (vogl_colors[0] == '\'') ? (vogl_colors + 1) : vogl_colors;

                colors.tokenize(":", tokens, true);
                for (uint32_t i = 0; i < tokens.size(); i++)
                {
                    dynamic_string &str = tokens[i];
                    dynamic_string_array color_tokens;

                    str.tokenize("=;", color_tokens, true);
                    if (color_tokens.size() >= 2)
                    {
                        uint32_t color_type = get_message_type_from_str(color_tokens[0].c_str());
                        if (color_type != (uint32_t)-1)
                        {
                            for (uint32_t j = 1; j < color_tokens.size(); j++)
                            {
                                if (j == 1)
                                    g_colors[color_type].format("\033[%sm", color_tokens[j].c_str());
                                else
                                    g_colors[color_type].format_append("\033[%sm", color_tokens[j].c_str());
                            }
                        }
                    }
                }
            }
        }

        const char *pANSIColor = (flags & cMsgFlagHeader) ?
                    g_colors[cMsgTotal].c_str() : g_colors[type].c_str();

        FILE *pFile = (type == cMsgError) ? stderr : stdout;
        bool is_redirected = !isatty(fileno(pFile)) && pANSIColor[0];

        if (!is_redirected)
            fputs(pANSIColor, pFile);

        fputs(pMsg, pFile);

        if (!is_redirected)
            fputs(ANSI_RESET, pFile);

        return true;
    }

#else

    void colorized_console::set_default_color()
    {
    }

    bool colorized_console::console_output_func(eConsoleMessageType type, uint32_t flags, const char *pMsg, void *pData)
    {
        (void) pData;

        if (type > console::get_output_level())
            return true;

        FILE *pFile = (type == cMsgError) ? stderr : stdout;

        fputs(pMsg, pFile);

        return true;
    }

#endif

} // namespace vogl
