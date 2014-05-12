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

// File: vogl_dynamic_string.cpp
#include "vogl_core.h"
#include "vogl_strutils.h"
#include "vogl_json.h"

#if VOGL_SLOW_STRING_LEN_CHECKS
#pragma message("Warning: Slow string checking enabled")
#endif

namespace vogl
{
    dynamic_string::dynamic_string(eVarArg dummy, const char *p, ...)
    {
        VOGL_NOTE_UNUSED(dummy);

        set_to_empty_small_string();

        VOGL_ASSERT(p);

        va_list args;
        va_start(args, p);
        format_args(p, args);
        va_end(args);
    }

    dynamic_string::dynamic_string(const char *p)
    {
        VOGL_ASSUME(cMaxDynamicStringBufSize <= cINT32_MAX);
        // This class assumes little endian byte order, because the low bits of m_dyn.m_pStr must alias m_small.m_flag
        VOGL_ASSUME(VOGL_LITTLE_ENDIAN_CPU);

        VOGL_ASSERT(p);

        set_to_empty_small_string();

        set(p);
    }

    dynamic_string::dynamic_string(const char *p, uint32_t len)
    {
        VOGL_ASSERT(p);

        set_to_empty_small_string();

        set_from_buf(p, len);
    }

    dynamic_string::dynamic_string(const dynamic_string &other)
    {
        set_to_empty_small_string();

        set(other);
    }

    void dynamic_string::clear()
    {
        check();

        if (is_dynamic())
        {
            vogl_delete_array(m_dyn.m_pStr);
        }

        set_to_empty_small_string();
    }

    void dynamic_string::empty()
    {
        truncate(0);
    }

    void dynamic_string::optimize()
    {
        if (!m_len)
            clear();
        else
        {
            if ((m_len + 1U) <= cSmallStringBufSize)
            {
                if (is_dynamic())
                {
                    char *pStr = m_dyn.m_pStr;

                    memcpy(m_small.m_buf, pStr, m_len + 1);

                    vogl_delete_array(pStr);

                    set_small_string_flag();
                }
            }
            else
            {
                if (!is_dynamic())
                {
                    VOGL_ASSERT_ALWAYS;
                    clear();
                    return;
                }

                uint32_t min_buf_size = m_len + 1;

                // TODO: In some cases it'll make no difference to try to shrink the block due to allocation alignment, etc. issues
                if (m_dyn.m_buf_size > min_buf_size)
                {
                    char *p = vogl_new_array(char, min_buf_size);
                    memcpy(p, m_dyn.m_pStr, m_len + 1);

                    vogl_delete_array(m_dyn.m_pStr);

                    set_dyn_string_ptr(p);

                    m_dyn.m_buf_size = min_buf_size;
                }
            }
        }

        check();
    }

    void dynamic_string::reserve(uint32_t new_capacity)
    {
        ensure_buf(new_capacity, true);
    }

    int dynamic_string::compare(const char *p, bool case_sensitive) const
    {
        VOGL_ASSERT(p);

        const int result = (case_sensitive ? strcmp : vogl_stricmp)(get_ptr_priv(), p);

        if (result < 0)
            return -1;
        else if (result > 0)
            return 1;

        return 0;
    }

    int dynamic_string::compare(const dynamic_string &rhs, bool case_sensitive) const
    {
        return compare(rhs.get_ptr_priv(), case_sensitive);
    }

    int dynamic_string::compare_using_length(const char *p, bool case_sensitive) const
    {
        uint32_t l_len = get_len();
        uint32_t r_len = vogl_strlen(p);

        if (l_len < r_len)
            return -1;
        else if (l_len == r_len)
            return compare(p, case_sensitive);
        else
            return 1;
    }

    int dynamic_string::compare_using_length(const dynamic_string &rhs, bool case_sensitive) const
    {
        return compare_using_length(rhs.get_ptr(), case_sensitive);
    }

    dynamic_string &dynamic_string::set(const char *p, uint32_t max_len)
    {
        VOGL_ASSERT(p);

        const uint32_t len = math::minimum<uint32_t>(max_len, vogl_strlen(p));
        VOGL_ASSERT(len <= cMaxDynamicStringLen);

        if ((!len) || (len > cMaxDynamicStringLen))
            clear();
        else
        {
            char *pStr = get_ptr_priv();
            uint32_t buf_size = get_buf_size();
            if ((p >= pStr) && (p < (pStr + buf_size)))
            {
                if (pStr != p)
                    memmove(pStr, p, len);
                pStr[len] = '\0';

                m_len = len;
            }
            else if (ensure_buf(len, false))
            {
                m_len = len;
                memcpy(get_ptr_priv(), p, m_len + 1);
            }
        }

        check();

        return *this;
    }

    dynamic_string &dynamic_string::set(const dynamic_string &other, uint32_t max_len)
    {
        if (this == &other)
        {
            if (max_len < m_len)
            {
                get_ptr_priv()[max_len] = '\0';
                m_len = max_len;
            }
        }
        else
        {
            const uint32_t len = math::minimum<uint32_t>(max_len, other.m_len);

            if (!len)
                clear();
            else if (ensure_buf(len, false))
            {
                char *pStr = get_ptr_priv();
                m_len = len;
                memcpy(pStr, other.get_ptr_priv(), m_len);
                pStr[len] = '\0';
            }
        }

        check();

        return *this;
    }

    bool dynamic_string::set_len(uint32_t new_len, char fill_char)
    {
        if ((new_len > cMaxDynamicStringLen) || (!fill_char))
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        uint32_t cur_len = m_len;

        if (ensure_buf(new_len, true))
        {
            char *pStr = get_ptr_priv();

            if (new_len > cur_len)
                memset(pStr + cur_len, fill_char, new_len - cur_len);

            pStr[new_len] = 0;

            m_len = new_len;

            check();
        }

        return true;
    }

    dynamic_string &dynamic_string::set_from_raw_buf_and_assume_ownership(char *pBuf, uint32_t buf_size_in_bytes, uint32_t len_in_chars)
    {
        VOGL_ASSERT(buf_size_in_bytes <= cMaxDynamicStringBufSize);
        VOGL_ASSERT((len_in_chars + 1) <= buf_size_in_bytes);
        VOGL_ASSERT(len_in_chars <= cMaxDynamicStringLen);
        VOGL_ASSERT(!pBuf || !pBuf[len_in_chars]);
        VOGL_ASSERT(pBuf || ((!buf_size_in_bytes) && (!len_in_chars)));

        clear();

        if (!pBuf)
            return *this;

        set_dyn_string_ptr(pBuf);
        m_dyn.m_buf_size = buf_size_in_bytes;
        m_len = len_in_chars;

        check();

        return *this;
    }

    dynamic_string &dynamic_string::set_from_buf(const void *pBuf, uint32_t buf_len_in_chars)
    {
        if (!pBuf)
        {
            VOGL_ASSERT(!buf_len_in_chars);
            truncate(0);
            return *this;
        }

        if (buf_len_in_chars > cMaxDynamicStringLen)
        {
            VOGL_ASSERT_ALWAYS;
            clear();
            return *this;
        }

#ifdef VOGL_BUILD_DEBUG
        if ((buf_len_in_chars) && (memchr(pBuf, 0, buf_len_in_chars) != NULL))
        {
            VOGL_ASSERT_ALWAYS;
            clear();
            return *this;
        }
#endif

        if (ensure_buf(buf_len_in_chars, false))
        {
            char *pStr = get_ptr_priv();

            if (buf_len_in_chars)
                memcpy(pStr, pBuf, buf_len_in_chars);

            pStr[buf_len_in_chars] = 0;

            m_len = buf_len_in_chars;

            check();
        }

        return *this;
    }

    dynamic_string &dynamic_string::set_char(uint32_t index, char c)
    {
        VOGL_ASSERT(index <= m_len);

        if (!c)
            truncate(index);
        else if (index < m_len)
        {
            get_ptr_priv()[index] = c;

            check();
        }
        else if (index == m_len)
            append_char(c);

        return *this;
    }

    dynamic_string &dynamic_string::append_char(char c)
    {
        if (!c)
        {
            // Can't append a zero terminator - there's already one there.
            VOGL_ASSERT_ALWAYS;
            return *this;
        }

        if (m_len == cMaxDynamicStringLen)
        {
            VOGL_ASSERT_ALWAYS;
            return *this;
        }

        if (ensure_buf(m_len + 1))
        {
            char *pStr = get_ptr_priv();

            pStr[m_len] = c;
            pStr[m_len + 1] = '\0';
            m_len++;
            check();
        }

        return *this;
    }

    dynamic_string &dynamic_string::truncate(uint32_t new_len)
    {
        if (new_len < m_len)
        {
            get_ptr_priv()[new_len] = '\0';
            m_len = new_len;
            check();
        }
        return *this;
    }

    dynamic_string &dynamic_string::shorten(uint32_t chars_to_remove)
    {
        VOGL_ASSERT(m_len >= chars_to_remove);
        if (m_len < chars_to_remove)
            return *this;
        return truncate(m_len - chars_to_remove);
    }

    dynamic_string &dynamic_string::tolower()
    {
        if (m_len)
        {
            vogl_strlwr(get_ptr_priv());
        }
        return *this;
    }

    dynamic_string &dynamic_string::toupper()
    {
        if (m_len)
        {
            vogl_strupr(get_ptr_priv());
        }
        return *this;
    }

    dynamic_string &dynamic_string::append(const char *p, uint32_t len)
    {
        VOGL_ASSERT(p);
        VOGL_ASSERT(len <= vogl_strlen(p));

        if (!len)
            return *this;

        if (ptr_refers_to_self(p))
        {
            dynamic_string temp(*this);
            temp.append(p, len);
            swap(temp);
            return *this;
        }

        if (len > (cMaxDynamicStringLen - m_len))
        {
            VOGL_ASSERT_ALWAYS;
            return *this;
        }

        uint32_t new_total_len = m_len + len;
        VOGL_ASSERT(new_total_len <= cMaxDynamicStringLen);
        if ((new_total_len) && (ensure_buf(new_total_len)))
        {
            char *pStr = get_ptr_priv();

            memcpy(pStr + m_len, p, len);
            pStr[m_len + len] = '\0';
            m_len += len;
            check();
        }

        return *this;
    }

    dynamic_string &dynamic_string::append(const char *p)
    {
        VOGL_ASSERT(p);

        if (ptr_refers_to_self(p))
        {
            dynamic_string temp(*this);
            temp.append(p);
            swap(temp);
            return *this;
        }

        uint32_t len = vogl_strlen(p);
        if (len > (cMaxDynamicStringLen - m_len))
        {
            VOGL_ASSERT_ALWAYS;
            return *this;
        }

        uint32_t new_total_len = m_len + len;
        VOGL_ASSERT(new_total_len <= cMaxDynamicStringLen);
        if ((new_total_len) && (ensure_buf(new_total_len)))
        {
            memcpy(get_ptr_priv() + m_len, p, len + 1);
            m_len += len;
            check();
        }

        return *this;
    }

    dynamic_string &dynamic_string::append(const dynamic_string &other)
    {
        if (this == &other)
        {
            dynamic_string temp(*this);
            temp.append(other);
            swap(temp);
            return *this;
        }

        uint32_t len = other.m_len;

        if (len > (cMaxDynamicStringLen - m_len))
        {
            VOGL_ASSERT_ALWAYS;
            return *this;
        }

        uint32_t new_total_len = m_len + len;
        VOGL_ASSERT(new_total_len <= cMaxDynamicStringLen);
        if ((new_total_len) && ensure_buf(new_total_len))
        {
            memcpy(get_ptr_priv() + m_len, other.get_ptr_priv(), len + 1);
            m_len = m_len + len;
            check();
        }

        return *this;
    }

    dynamic_string operator+(const char *p, const dynamic_string &a)
    {
        return dynamic_string(p).append(a);
    }

    dynamic_string operator+(const dynamic_string &a, const char *p)
    {
        return dynamic_string(a).append(p);
    }

    dynamic_string operator+(const dynamic_string &a, const dynamic_string &b)
    {
        return dynamic_string(a).append(b);
    }

    dynamic_string &dynamic_string::format_args(const char *p, va_list args)
    {
        VOGL_ASSERT(p);

        const uint32_t cBufSize = 4096;
        int buf_size = cBufSize;
        char buf[cBufSize];
        char *pBuf = buf;

#ifdef COMPILER_MSVC
        int l = vsnprintf_s(pBuf, buf_size, _TRUNCATE, p, args);
#else
        int l = vsnprintf(pBuf, buf_size, p, args);
#endif

        if (l >= buf_size)
        {
            buf_size = l + 1;
            pBuf = static_cast<char *>(vogl_malloc(buf_size));

#ifdef COMPILER_MSVC
            l = vsnprintf_s(pBuf, buf_size, _TRUNCATE, p, args);
#else
            l = vsnprintf(pBuf, buf_size, p, args);
#endif
        }

        if (l > cMaxDynamicStringLen)
        {
            VOGL_ASSERT_ALWAYS;
            clear();
        }
        else if (l <= 0)
            clear();
        else if (ensure_buf(l, false))
        {
            memcpy(get_ptr_priv(), pBuf, l + 1);

            m_len = l;

            check();
        }
        else
        {
            clear();
        }

        if (pBuf != buf)
            vogl_free(pBuf);

        return *this;
    }

    dynamic_string &dynamic_string::format(const char *p, ...)
    {
        VOGL_ASSERT(p);

        va_list args;
        va_start(args, p);
        format_args(p, args);
        va_end(args);
        return *this;
    }

    dynamic_string &dynamic_string::format_append(const char *p, ...)
    {
        VOGL_ASSERT(p);

        dynamic_string temp;

        va_list args;
        va_start(args, p);
        temp.format_args(p, args);
        va_end(args);

        append(temp);

        return *this;
    }

    dynamic_string &dynamic_string::crop(uint32_t start, uint32_t len)
    {
        if (start >= m_len)
        {
            clear();
            return *this;
        }

        len = math::minimum<uint32_t>(len, m_len - start);

        char *pStr = get_ptr_priv();

        if (start)
            memmove(pStr, pStr + start, len);

        pStr[len] = '\0';

        m_len = len;

        check();

        return *this;
    }

    dynamic_string &dynamic_string::remove(uint32_t start, uint32_t len)
    {
        VOGL_ASSERT(start < m_len);

        if ((start >= m_len) || (!len))
            return *this;

        uint32_t max_len = m_len - start;
        VOGL_ASSERT(len <= max_len);
        if (len > max_len)
            return *this;

        uint32_t num_chars_remaining = m_len - (start + len);

        // + 1 to move terminator
        memmove(get_ptr_priv() + start, get_ptr_priv() + start + len, num_chars_remaining + 1);

        m_len = start + num_chars_remaining;

        check();

        return *this;
    }

    dynamic_string &dynamic_string::substring(uint32_t start, uint32_t end)
    {
        VOGL_ASSERT(start <= end);
        if (start > end)
            return *this;
        return crop(start, end - start);
    }

    dynamic_string &dynamic_string::left(uint32_t len)
    {
        return substring(0, len);
    }

    dynamic_string &dynamic_string::mid(uint32_t start, uint32_t len)
    {
        return crop(start, len);
    }

    dynamic_string &dynamic_string::right(uint32_t start)
    {
        return substring(start, get_len());
    }

    dynamic_string &dynamic_string::tail(uint32_t num)
    {
        return substring(math::maximum<int>(static_cast<int>(get_len()) - static_cast<int>(num), 0), get_len());
    }

    // This is not particularly efficient. Just here as a one-off utility method.
    dynamic_string &dynamic_string::replace(const char *pFind, const char *pReplacement, bool case_sensitive, uint32_t *pNum_found, uint32_t max_replacements)
    {
        VOGL_ASSERT(pFind);

        if (pNum_found)
            *pNum_found = 0;

        VOGL_ASSERT(max_replacements);
        if (!max_replacements)
            return *this;

        uint32_t num_found = 0;

        uint32_t find_len = vogl_strlen(pFind);
        uint32_t replacement_len = pReplacement ? vogl_strlen(pReplacement) : 0;

        dynamic_string temp;

        int cur_ofs = 0;
        for (;;)
        {
            int find_ofs = find_left(pFind, case_sensitive, cur_ofs);
            if (find_ofs < 0)
                break;

            temp = *this;
            temp.truncate(find_ofs);
            if (pReplacement)
                temp += pReplacement;
            temp += right(find_ofs + find_len);
            temp.swap(*this);

            cur_ofs = find_ofs + replacement_len;

            num_found++;
            if (num_found >= max_replacements)
                break;
        }

        if (pNum_found)
            *pNum_found = num_found;

        return *this;
    }

    dynamic_string &dynamic_string::unquote()
    {
        if (m_len >= 2)
        {
            if (((*this)[0] == '\"') && ((*this)[m_len - 1] == '\"'))
            {
                return mid(1, m_len - 2);
            }
        }

        return *this;
    }

    int dynamic_string::find_left(const char *p, bool case_sensitive, uint32_t start_ofs) const
    {
        VOGL_ASSERT(p);
        VOGL_ASSERT(start_ofs <= m_len);

        const uint32_t p_len = vogl_strlen(p);

        if (p_len > m_len)
            return -1;

        const char *pStr = get_ptr_priv();

        for (uint32_t i = start_ofs; i <= m_len - p_len; i++)
            if ((case_sensitive ? strncmp : vogl_strnicmp)(p, pStr + i, p_len) == 0)
                return i;

        return -1;
    }

    bool dynamic_string::contains(const char *p, bool case_sensitive) const
    {
        return find_left(p, case_sensitive) >= 0;
    }

    bool dynamic_string::contains(char c) const
    {
        return find_left(c) >= 0;
    }

    bool dynamic_string::begins_with(const char *p, bool case_sensitive) const
    {
        VOGL_ASSERT(p);

        const uint32_t p_len = vogl_strlen(p);
        if ((!p_len) || (m_len < p_len))
            return false;

        return (case_sensitive ? strncmp : vogl_strnicmp)(p, get_ptr_priv(), p_len) == 0;
    }

    bool dynamic_string::ends_with(const char *p, bool case_sensitive) const
    {
        VOGL_ASSERT(p);

        const uint32_t p_len = vogl_strlen(p);
        if ((!p_len) || (m_len < p_len))
            return false;

        return (case_sensitive ? strcmp : vogl_stricmp)(get_ptr_priv() + m_len - p_len, p) == 0;
    }

    uint32_t dynamic_string::count_char(char c) const
    {
        const char *pStr = get_ptr_priv();

        uint32_t count = 0;
        for (uint32_t i = 0; i < m_len; i++)
            if (pStr[i] == c)
                count++;
        return count;
    }

    int dynamic_string::find_left(char c, int start_ofs) const
    {
        const char *pStr = get_ptr_priv();

        for (uint32_t i = start_ofs; i < m_len; i++)
            if (pStr[i] == c)
                return i;
        return -1;
    }

    int dynamic_string::find_right(char c) const
    {
        if (m_len)
        {
            const char *pStr = get_ptr_priv();

            uint32_t i = m_len - 1;
            for (;;)
            {
                if (pStr[i] == c)
                    return i;
                if (!i)
                    break;
                --i;
            }
        }

        return -1;
    }

    int dynamic_string::find_right(char c, uint32_t start_ofs) const
    {
        if (start_ofs >= m_len)
        {
            VOGL_ASSERT_ALWAYS;
            return -1;
        }

        if (m_len)
        {
            const char *pStr = get_ptr_priv();

            uint32_t i = start_ofs;
            for (;;)
            {
                if (pStr[i] == c)
                    return i;
                if (!i)
                    break;
                --i;
            }
        }

        return -1;
    }

    int dynamic_string::find_right(const char *p, bool case_sensitive) const
    {
        VOGL_ASSERT(p);
        const uint32_t p_len = vogl_strlen(p);

        if (p_len > m_len)
            return -1;

        if (m_len)
        {
            const char *pStr = get_ptr_priv();

            uint32_t i = m_len - p_len;
            for (;;)
            {
                if ((case_sensitive ? strncmp : vogl_strnicmp)(p, &pStr[i], p_len) == 0)
                    return i;
                if (!i)
                    break;
                --i;
            }
        }

        return -1;
    }

    dynamic_string &dynamic_string::trim()
    {
        VOGL_ASSERT(m_len <= cMaxDynamicStringLen);

        const char *pStr = get_ptr_priv();

        int s, e;
        for (s = 0; s < static_cast<int>(m_len); s++)
            if (!vogl_isspace(pStr[s]))
                break;

        for (e = m_len - 1; e > s; e--)
            if (!vogl_isspace(pStr[e]))
                break;

        return crop(s, e - s + 1);
    }

    dynamic_string &dynamic_string::trim_end()
    {
        VOGL_ASSERT(m_len <= cMaxDynamicStringLen);

        const char *pStr = get_ptr_priv();

        int e;
        for (e = static_cast<int>(m_len) - 1; e >= 0; e--)
            if (!vogl_isspace(pStr[e]))
                break;

        return crop(0, e + 1);
    }

    dynamic_string &dynamic_string::trim_crlf()
    {
        VOGL_ASSERT(m_len <= cMaxDynamicStringLen);

        const char *pStr = get_ptr_priv();

        int s = 0, e;

        for (e = static_cast<int>(m_len) - 1; e > s; e--)
            if ((pStr[e] != 13) && (pStr[e] != 10))
                break;

        return crop(s, e - s + 1);
    }

    dynamic_string &dynamic_string::remap(int from_char, int to_char)
    {
        char *pStr = get_ptr_priv();

        for (uint32_t i = 0; i < m_len; i++)
            if (pStr[i] == from_char)
                pStr[i] = static_cast<char>(to_char);

        return *this;
    }

    bool dynamic_string::validate() const
    {
#define CHECK(x)                         \
    if (!(x))                            \
    {                                    \
        vogl_debug_break_if_debugging(); \
        return false;                    \
    }
        CHECK(m_len < get_buf_size());
        CHECK(m_len <= cMaxDynamicStringLen);

        if (is_dynamic())
        {
            CHECK(m_dyn.m_pStr != NULL);
            CHECK(m_dyn.m_buf_size);
            CHECK(m_dyn.m_buf_size <= cMaxDynamicStringBufSize);

            CHECK(((m_dyn.m_pStr + m_dyn.m_buf_size) <= (const char *)this) || (m_dyn.m_pStr >= (const char *)(this + 1)));
            CHECK(((uint64_t)m_dyn.m_pStr & (VOGL_MIN_ALLOC_ALIGNMENT - 1)) == 0);
            CHECK(vogl_msize_array(m_dyn.m_pStr) >= m_dyn.m_buf_size);
        }

        const char *pStr = get_ptr_priv();

        CHECK(!pStr[m_len]);

#if VOGL_SLOW_STRING_LEN_CHECKS
        CHECK(vogl_strlen(pStr) == m_len);
#endif

#undef CHECK
        return true;
    }

    void dynamic_string::ensure_dynamic()
    {
        if (is_dynamic())
            return;

        uint32_t new_buf_size = math::maximum(m_len + 1, 16U);

        new_buf_size = math::minimum<uint32_t>(new_buf_size, cMaxDynamicStringBufSize);

        char *p = vogl_new_array(char, new_buf_size);

        VOGL_ASSERT(new_buf_size >= (m_len + 1));
        memcpy(p, m_small.m_buf, m_len + 1);

        set_dyn_string_ptr(p);

        m_dyn.m_buf_size = static_cast<uint32_t>(math::minimum<uint64_t>(cMaxDynamicStringBufSize, vogl_msize_array(m_dyn.m_pStr)));
        VOGL_ASSERT(m_dyn.m_buf_size >= new_buf_size);

        check();
    }

    bool dynamic_string::ensure_buf(uint32_t len, bool preserve_contents)
    {
        uint32_t buf_size_needed = len + 1;
        if (buf_size_needed > cMaxDynamicStringBufSize)
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        if (buf_size_needed > get_buf_size())
            return expand_buf(buf_size_needed, preserve_contents);

        return true;
    }

    bool dynamic_string::expand_buf(uint32_t new_buf_size, bool preserve_contents)
    {
        VOGL_ASSERT(new_buf_size <= cMaxDynamicStringBufSize);

        new_buf_size = static_cast<uint32_t>(math::minimum<uint64_t>(cMaxDynamicStringBufSize, math::next_pow2(new_buf_size)));

        char *p = vogl_new_array(char, new_buf_size);

        if (preserve_contents)
        {
            VOGL_ASSERT(new_buf_size >= (m_len + 1));
            memcpy(p, get_ptr_priv(), m_len + 1);
        }

        if (is_dynamic())
        {
            vogl_delete_array(m_dyn.m_pStr);
        }

        set_dyn_string_ptr(p);
        m_dyn.m_buf_size = static_cast<uint32_t>(math::minimum<uint64_t>(cMaxDynamicStringBufSize, vogl_msize_array(m_dyn.m_pStr)));
        VOGL_ASSERT(m_dyn.m_buf_size >= new_buf_size);

        if (preserve_contents)
            check();

        return get_buf_size() >= new_buf_size;
    }

    void dynamic_string::swap(dynamic_string &other)
    {
        VOGL_ASSUME((sizeof(*this) & (sizeof(uint32_t) - 1)) == 0);
        uint32_t *pA = reinterpret_cast<uint32_t *>(this);
        uint32_t *pB = reinterpret_cast<uint32_t *>(&other);

        uint32_t num_uint32s = sizeof(*this) / sizeof(uint32_t);
        while (num_uint32s >= 4)
        {
            std::swap(reinterpret_cast<uint64_t *>(pA)[0], reinterpret_cast<uint64_t *>(pB)[0]);
            std::swap(reinterpret_cast<uint64_t *>(pA)[1], reinterpret_cast<uint64_t *>(pB)[1]);
            pA += 4;
            pB += 4;
            num_uint32s -= 4;
        }

        while (num_uint32s >= 2)
        {
            std::swap(reinterpret_cast<uint64_t *>(pA)[0], reinterpret_cast<uint64_t *>(pB)[0]);
            pA += 2;
            pB += 2;
            num_uint32s -= 2;
        }

        if (num_uint32s)
            std::swap(pA[0], pB[0]);
    }

    int dynamic_string::serialize(void *pBuf, uint32_t buf_size, bool little_endian) const
    {
        uint32_t buf_left = buf_size;

        VOGL_ASSUME(sizeof(m_len) == sizeof(uint32_t));

        if (!utils::write_val(static_cast<uint32_t>(m_len), pBuf, buf_left, little_endian))
            return -1;

        if (buf_left < m_len)
            return -1;

        memcpy(pBuf, get_ptr_priv(), m_len);

        buf_left -= m_len;

        return buf_size - buf_left;
    }

    int dynamic_string::deserialize(const void *pBuf, uint32_t buf_size, bool little_endian)
    {
        uint32_t buf_left = buf_size;

        if (buf_left < sizeof(uint32_t))
            return -1;

        uint32_t len;
        if (!utils::read_obj(len, pBuf, buf_left, little_endian))
            return -1;

        if (buf_left < len)
            return -1;

        if (len > cMaxDynamicStringLen)
            return -1;

        set_from_buf(pBuf, len);

        buf_left -= len;

        return buf_size - buf_left;
    }

    void dynamic_string::translate_lf_to_crlf()
    {
        if (find_left(0x0A) < 0)
            return;

        dynamic_string tmp;
        if (!tmp.ensure_buf(m_len + 2))
        {
            VOGL_ASSERT_ALWAYS;
            return;
        }

        // normal sequence is 0x0D 0x0A (CR LF, \r\n)

        int prev_char = -1;
        for (uint32_t i = 0; i < get_len(); i++)
        {
            const int cur_char = (*this)[i];

            if ((cur_char == 0x0A) && (prev_char != 0x0D))
                tmp.append_char(0x0D);

            tmp.append_char(cur_char);

            prev_char = cur_char;
        }

        swap(tmp);
    }

    // simple strtok wrapper
    void dynamic_string::tokenize(const char *pDelims, dynamic_string_array &tokens, bool trim) const
    {
        if (!pDelims || !pDelims[0])
            return;

        vogl::vector<char> tok_buf;
        tok_buf.append(get_ptr(), get_len());

        tok_buf.push_back('\0');

        char *pTok = strtok(tok_buf.get_ptr(), pDelims);

        while (pTok)
        {
            dynamic_string tok(pTok);
            tokens.enlarge(1)->swap(tok);

            pTok = strtok(NULL, pDelims);
        }

        if (trim)
        {
            for (uint32_t i = 0; i < tokens.size(); i++)
                tokens[i].trim();
        }
    }

    bool json_serialize(const dynamic_string &str, json_value &val)
    {
        val.set_value(str);
        return true;
    }

    bool json_deserialize(dynamic_string &str, const json_value &val)
    {
        str = val.as_string();
        return true;
    }

    bool dynamic_string_test()
    {
        random rnd;

        rnd.seed();

#define CHECK(x) \
    if (!(x))    \
        return false;

        {
            dynamic_string x;
            x = "This is a test";
            dynamic_string y(x);
            CHECK(x.compare(y, true) == 0);
            x.ensure_dynamic();
            CHECK(x.compare(y, true) == 0);
            x.optimize();
            CHECK(x.compare(y, true) == 0);

            x.clear();
            x = y;
            CHECK(y.compare(x, true) == 0);
        }

        {
            dynamic_string x("BlahCool");
            CHECK(x.contains("Cool"));
            CHECK(x.find_left("Cool") == 4);
            CHECK(x.find_left("hC") == 3);
            CHECK(x.find_right("o") == 6);
            CHECK(x.find_right("hC") == 3);
            CHECK(x.replace("Cool", "XXXX") == "BlahXXXX");

            x = "BlahCool";
            CHECK(x.replace("Blah", "Z") == "ZCool");

            x = "BlahCool";
            CHECK(x.replace("Blah", "BlahBlah") == "BlahBlahCool");

            x = "ABCDEF";

            x.tolower();
            CHECK(x.compare("abcdef") == 0);
            CHECK(x.compare("ABCDEF") == 0);
            CHECK(x.compare("abcdef", true) == 0);
            CHECK(x.compare("ABCDEF", true) > 0);

            x.toupper();
            CHECK(x.compare("abcdef") == 0);
            CHECK(x.compare("ABCDEF") == 0);
            CHECK(x.compare("abcdef", true) < 0);
            CHECK(x.compare("ABCDEF", true) == 0);
        }

        {
            dynamic_string x("Blah");
            x.append(x);
            CHECK(x == "BlahBlah");
            x.append(x.get_ptr());
            CHECK(x == "BlahBlahBlahBlah");
            x.append(x.get_ptr(), x.get_len());
            CHECK(x == "BlahBlahBlahBlahBlahBlahBlahBlah");
            x.append(x.get_ptr(), x.get_len());
            CHECK(x == "BlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlah");
            x.append(x.get_ptr(), x.get_len());
            CHECK(x == "BlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlah");
            x.append(x.get_ptr(), x.get_len());
            CHECK(x == "BlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlahBlah");
        }

        {
            dynamic_string_array tokens;
            dynamic_string x("   This is,a, test   ");
            x.tokenize(" ,", tokens, true);
            CHECK(tokens.size() == 4);
            CHECK(tokens[0] == "This");
            CHECK(tokens[1] == "is");
            CHECK(tokens[2] == "a");
            CHECK(tokens[3] == "test");
        }

        {
            const uint32_t N = 10000;
            dynamic_string_array x(N);
            vogl::vector<uint64_t> y(N);
            for (uint32_t i = 0; i < N; i++)
            {
                uint64_t r = rnd.urand64();
                x[i].format("%" PRIu64, r);
                CHECK(string_to_uint64(x[i].get_ptr()) == r);
                y[i] = r;
            }

            for (uint32_t i = 0; i < 100000; i++)
            {
                uint32_t k = rnd.irand(0, N);
                uint32_t l = rnd.irand(0, N);
                CHECK(string_to_uint64(x[k].get_ptr()) == y[k]);
                CHECK(string_to_uint64(x[l].get_ptr()) == y[l]);

                std::swap(x[k], x[l]);
                std::swap(y[k], y[l]);

                CHECK(string_to_uint64(x[k].get_ptr()) == y[k]);
                CHECK(string_to_uint64(x[l].get_ptr()) == y[l]);
            }

            for (uint32_t i = 0; i < N; i++)
                x[i].ensure_dynamic();
            for (uint32_t i = 0; i < N; i++)
                CHECK(string_to_uint64(x[i].get_ptr()) == y[i]);

            for (uint32_t i = 0; i < N; i++)
                x[i].optimize();
            for (uint32_t i = 0; i < N; i++)
                CHECK(string_to_uint64(x[i].get_ptr()) == y[i]);

            for (uint32_t i = 0; i < N; i++)
            {
                dynamic_string temp(x[i]);
                x[i] = temp;
            }
            for (uint32_t i = 0; i < N; i++)
                CHECK(string_to_uint64(x[i].get_ptr()) == y[i]);

            for (uint32_t i = 0; i < N; i++)
            {
                dynamic_string temp(x[i]);
                x[i] = temp.get_ptr();
            }
            for (uint32_t i = 0; i < N; i++)
                CHECK(string_to_uint64(x[i].get_ptr()) == y[i]);
        }

        {
            dynamic_string x;
            vogl::vector<char> y;
            for (uint32_t t = 0; t < 10000000; t++)
            {
                if ((rnd.irand(0, 1000) == 0) || (x.size() > 8000000))
                {
                    x.clear();
                    y.clear();
                }

                if (rnd.irand(0, 100) == 0)
                    x.optimize();
                else if (rnd.irand(0, 100) == 0)
                    x.ensure_dynamic();
                else if (rnd.irand(0, 10000) == 0)
                    x = dynamic_string(x);
                else if (rnd.irand(0, 4000) == 0)
                {
                    dynamic_string xx;
                    xx.set(x.get_ptr());
                    xx.swap(x);
                }
                else if (rnd.irand(0, 4000) == 0)
                {
                    dynamic_string xx;
                    xx.set(x.get_ptr());
                    x = xx;
                }
                else if (rnd.irand(0, 4000) == 0)
                {
                    x.truncate(0);
                    uint32_t pos = 0;
                    uint32_t left = y.size();
                    while (left)
                    {
                        uint32_t n = rnd.irand_inclusive(1, left);

                        if ((n == 1) && (rnd.get_bit()))
                            x.append_char(y[pos]);
                        else
                        {
                            vogl::vector<char> z;
                            z.append(&y[pos], n);
                            z.push_back(0);
                            x.append(z.get_ptr());
                        }

                        pos += n;
                        left -= n;
                    }
                }

                if (rnd.irand(0, 1000) == 0)
                {
                    x.append(x);
                    y.append(vogl::vector<char>(y));
                }

                if (x.get_len() > 1000000)
                {
                    x.truncate(x.get_len() / 2);
                    y.resize(y.size() / 2);
                }

                switch (rnd.irand(0, 6))
                {
                    case 0:
                    {
                        uint32_t n = (uint32_t)fabs(rnd.gaussian(10, 10));
                        for (uint32_t i = 0; i < n; i++)
                        {
                            int c = rnd.irand_inclusive(1, 255);
                            x.append_char(c);
                            y.push_back(c);
                        }
                        break;
                    }
                    case 1:
                    {
                        if (x.get_len())
                        {
                            uint32_t p = rnd.irand(0, x.get_len());
                            uint32_t n = (uint32_t)fabs(rnd.gaussian(10, 10));
                            n = math::minimum(n, x.get_len() - p);

                            if (rnd.get_bit())
                                x = dynamic_string(x).left(p) + dynamic_string(x).right(p + n);
                            else
                                x.remove(p, n);

                            y.erase(p, n);
                        }

                        break;
                    }
                    case 2:
                    {
                        int c = rnd.irand_inclusive(1, 255);
                        uint32_t p = rnd.irand_inclusive(0, x.get_len());

                        x = dynamic_string(x).left(p) + dynamic_string(cVarArg, "%c", c) + dynamic_string(x).right(p);
                        y.insert(p, c);

                        break;
                    }
                    case 3:
                    {
                        if (x.size())
                        {
                            if (rnd.get_bit())
                                x.shorten(1);
                            else
                                x.truncate(x.get_len() - 1);
                            y.pop_back();
                        }
                        break;
                    }
                    case 4:
                    {
                        char buf[3] = {(char)rnd.irand_inclusive(1, 255), rnd.get_bit() ? (char)rnd.irand(1, 255) : (char)0, 0 };
                        uint32_t l = vogl_strlen(buf);

                        int p0 = x.find_left(buf, true);

                        int p1 = -1;
                        for (int i = 0; i <= (int)y.size() - (int)l; i++)
                        {
                            if (strncmp(&y[i], buf, l) == 0)
                            {
                                p1 = i;
                                break;
                            }
                        }

                        CHECK(p0 == p1);

                        break;
                    }
                    case 5:
                    {
                        char buf[3] = {(char)rnd.irand_inclusive(1, 255), rnd.get_bit() ? (char)rnd.irand(1, 255) : (char)0, 0 };
                        uint32_t l = vogl_strlen(buf);

                        int p0 = x.find_right(buf, true);

                        int p1 = -1;
                        for (int i = (int)y.size() - (int)l; i >= 0; --i)
                        {
                            if (strncmp(&y[i], buf, l) == 0)
                            {
                                p1 = i;
                                break;
                            }
                        }

                        CHECK(p0 == p1);

                        break;
                    }
                    default:
                        VOGL_ASSERT_ALWAYS;
                        break;
                }

                CHECK(x.get_len() == y.size());
                for (uint32_t i = 0; i < x.get_len(); i++)
                {
                    CHECK(x[i] == y[i]);
                }

                printf("%u: %u\n", t, x.size());
            }
        }

        {
            const uint32_t N = 5;
            for (uint32_t t = 0; t < N; t++)
            {
                uint32_t i = rnd.irand_inclusive(0, cMaxDynamicStringLen);
                printf("Size: %u\n", i);

                dynamic_string k;

                int fill_char = rnd.irand_inclusive(1, 255);
                CHECK(k.set_len(i, (char)fill_char));

                CHECK(k.validate());

                k.ensure_dynamic();

                CHECK(k.validate());

                k.optimize();

                CHECK(k.validate());

                CHECK(k.get_len() == i);

                for (uint32_t i2 = 0; i2 < k.get_len(); i2++)
                    CHECK(k[i2] == (char)fill_char);

                //if ((t & 4095) == 4095)
                printf("%3.3f%%\n", t * 100.0f / N);
            }
        }

        for (uint32_t i = 0; i < 1000000; i++)
        {
            uint32_t k0 = rnd.irand(0, 20000);
            uint32_t k1 = rnd.irand(0, 20000);

            dynamic_string a0(cVarArg, "%u", k0);
            dynamic_string a1(cVarArg, "%u", k1);

            vogl::vector<char> b0;
            b0.append(a0.get_ptr(), a0.get_len());

            vogl::vector<char> b1;
            b1.append(a1.get_ptr(), a1.get_len());

            bool c0 = b0 < b1;
            bool c1 = b0 <= b1;
            bool c2 = b0 > b1;
            bool c3 = b0 >= b1;
            bool c4 = b0 == b1;
            bool c5 = b0 != b1;

            if ((a0 < a1) != c0)
                return false;
            if ((a0 <= a1) != c1)
                return false;
            if ((a0 > a1) != c2)
                return false;
            if ((a0 >= a1) != c3)
                return false;
            if ((a0 == a1) != c4)
                return false;
            if ((a0 != a1) != c5)
                return false;
        }

        return true;
    }

} // namespace vogl
