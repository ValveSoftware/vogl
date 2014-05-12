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

// File: vogl_strutils.h
#pragma once

#include "vogl_core.h"

#ifdef PLATFORM_WINDOWS
#define VOGL_PATH_SEPERATOR_CHAR '\\'
#else
#define VOGL_PATH_SEPERATOR_CHAR '/'
#endif

namespace vogl
{
    class dynamic_string;

    // These functions exist mostly to work around cross platform runtime library issues, some also work around locale issues.
    inline bool vogl_isdigit(int c)
    {
        return (c >= '0') && (c <= '9');
    }
    inline bool vogl_isxdigit(int c)
    {
        return vogl_isdigit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
    }
    inline bool vogl_isspace(int c)
    {
        return (c == 0x20) || (c == 0x09) || (c == 0x0a) || (c == 0x0b) || (c == 0x0c) || (c == 0x0d);
    }
    inline bool vogl_islower(int c)
    {
        return (c >= 'a') && (c <= 'z');
    }
    inline bool vogl_isupper(int c)
    {
        return (c >= 'A') && (c <= 'Z');
    }
    inline bool vogl_isalpha(int c)
    {
        return vogl_islower(c) || vogl_isupper(c);
    }

    inline int vogl_toupper(int c)
    {
        return ((c >= 'a') && (c <= 'z')) ? (c - 'a' + 'A') : c;
    }
    inline int vogl_tolower(int c)
    {
        return ((c >= 'A') && (c <= 'Z')) ? (c - 'A' + 'a') : c;
    }

    char *vogl_strlwr(char *p);
    char *vogl_strupr(char *p);

    int vogl_sprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...) VOGL_ATTRIBUTE_PRINTF(3, 4);
    int vogl_vsprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, va_list args);

    int vogl_strcasecmp(const char *s1, const char *s2);
    inline int vogl_stricmp(const char *s1, const char *s2)
    {
        return vogl_strcasecmp(s1, s2);
    }

    const char *vogl_strstr(const char *s1, const char *s2);

    int vogl_strncasecmp(const char *s1, const char *s2, size_t n);
    inline int vogl_strnicmp(const char *s1, const char *s2, size_t n)
    {
        return vogl_strncasecmp(s1, s2, n);
    }

    inline uint32_t vogl_strlen(const char *pStr)
    {
        uint32_t len = static_cast<uint32_t>(strlen(pStr));
        VOGL_ASSERT(len < cUINT32_MAX);
        return static_cast<uint32_t>(len);
    }

    // Must free returned string pointer using vogl_free(), not free().
    char *vogl_strdup(const char *pStr);

    int vogl_strcmp(const char *p, const char *q);

    char *strcpy_safe(char *pDst, uint32_t dst_len, const char *pSrc);

    bool int_to_string(int value, char *pDst, uint32_t len);
    bool uint_to_string(uint32_t value, char *pDst, uint32_t len);
    bool uint64_to_string(uint64_t value, char *pDst, uint32_t len);
    bool int64_to_string(int64_t value, char *pDst, uint32_t len);

    dynamic_string int_to_string(int64_t value);
    dynamic_string uint_to_string(uint64_t value);

    bool uint64_to_string_with_commas(uint64_t value, char *pDst, uint32_t len);
    dynamic_string uint64_to_string_with_commas(uint64_t value);

    // string_to_int/uint32_t/etc.
    // Supports hex values beginning with "0x", skips leading whitespace, returns false on overflow or conversion errors.
    // pBuf will be moved forward the # of characters actually consumed.
    bool string_ptr_to_int(const char *&pBuf, int &value);
    bool string_ptr_to_uint(const char *&pBuf, uint32_t &value);
    bool string_ptr_to_int64(const char *&pBuf, int64_t &value);
    bool string_ptr_to_uint64(const char *&pBuf, uint64_t &value);

    // recognizes "true" or "false" (case insensitive), or 0 (false) 1 (true), anything else is an error
    bool string_ptr_to_bool(const char *&p, bool &value);

    bool string_ptr_to_float(const char *&p, float &value, uint32_t round_digit = 512U);
    bool string_ptr_to_double(const char *&p, double &value, uint32_t round_digit = 512U);
    bool string_ptr_to_double(const char *&p, const char *pEnd, double &value, uint32_t round_digit = 512U);

    // These wrappers return 0 on error.
    inline int string_to_int(const char *pBuf, int def = 0)
    {
        int val = 0;
        if (!string_ptr_to_int(pBuf, val))
            val = def;
        return val;
    }
    inline uint32_t string_to_uint(const char *pBuf, uint32_t def = 0)
    {
        uint32_t val = 0;
        if (!string_ptr_to_uint(pBuf, val))
            val = def;
        return val;
    }
    inline int64_t string_to_int64(const char *pBuf, int64_t def = 0)
    {
        int64_t val = 0;
        if (!string_ptr_to_int64(pBuf, val))
            val = def;
        return val;
    }
    inline uint64_t string_to_uint64(const char *pBuf, uint64_t def = 0)
    {
        uint64_t val = 0;
        if (!string_ptr_to_uint64(pBuf, val))
            val = def;
        return val;
    }
    inline bool string_to_bool(const char *p, bool def = 0)
    {
        bool val = false;
        if (!string_ptr_to_bool(p, val))
            val = def;
        return val;
    }
    inline float string_to_float(const char *p, float def = 0)
    {
        float val = 0;
        if (!string_ptr_to_float(p, val))
            val = def;
        return val;
    }
    inline double string_to_double(const char *p, double def = 0)
    {
        double val = 0;
        if (!string_ptr_to_double(p, val))
            val = def;
        return val;
    }

    dynamic_string to_hex_string(int value);
    dynamic_string to_hex_string(uint32_t value);
    dynamic_string to_hex_string(int64_t value);
    dynamic_string to_hex_string(uint64_t value);

    bool strutils_test();

} // namespace vogl
