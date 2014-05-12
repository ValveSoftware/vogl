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

// File: fixed_string.h
// Copyright (C) 2008-2009 Tenacious Software LLC, All rights reserved.
#pragma once

#include "vogl_core.h"
#include "vogl_fixed_array.h"
#include "vogl_hash.h"
#include "vogl_strutils.h"

namespace vogl
{
    // Simple hybrid C/Pascal style fixed string class, limited to a max buffer size of 65535 chars.
    // Contains 16-bit length at beginning, followed by the zero terminated C-style string.
    //
    // TODO: Probably should delete this guy. This class is not nearly used as much as dynamic_string,
    // which has the small string optimization so the value add here isn't high. This class
    // never touches the heap, though, which could be useful.
    // TODO: Create unit tests, this class has been barely used.
    //
    // Important note: For historical reasons the comparisons operators in this class are
    // case insensitive (using locale independent helper functions) by default, mostly because
    // the last X engines I've worked on used this default and I got used to it. Its the day after
    // and now I'm regretting this idea.
    template <uint32_t BufSize>
    class fixed_string
    {
    public:
        enum
        {
            cBufSize = BufSize,
            cMaxLen = cBufSize - 1,
        };

        inline fixed_string()
            : m_len(0)
        {
            VOGL_ASSUME(BufSize < static_cast<uint32_t>(cINT32_MAX));
            m_buf[0] = 0;
        }

        inline fixed_string(eVarArg dummy, const char *p, ...) VOGL_ATTRIBUTE_PRINTF(3, 4)
        {
            VOGL_NOTE_UNUSED(dummy);
            VOGL_ASSUME(BufSize < static_cast<uint32_t>(cINT32_MAX));

            va_list args;
            va_start(args, p);
            format_args(p, args);
            va_end(args);
        }

        inline fixed_string(const char *p)
        {
            VOGL_ASSUME(BufSize < static_cast<uint32_t>(cINT32_MAX));

            set(p);
        }

        template <uint32_t OtherBufSize>
        explicit inline fixed_string(const fixed_string<OtherBufSize> &other)
            : m_len(other.get_len())
        {
            memcpy(m_buf.get_ptr(), other.get_ptr(), other.get_len() + 1);
        }

        inline fixed_string &clear(bool full = false)
        {
            if (full)
                m_buf.bitwise_zero();
            else
                m_buf[0] = 0;

            m_len = 0;

            return *this;
        }

        inline uint32_t get_buf_size() const
        {
            return cBufSize;
        }

        inline uint32_t get_max_len() const
        {
            return cMaxLen;
        }

        inline uint32_t get_len() const
        {
            return m_len;
        }

        inline uint32_t size() const
        {
            return m_len;
        }

        // Truncates the string to 0 chars, but does not free the buffer.
        inline void empty()
        {
            return clear(false);
        }

        inline bool is_empty() const
        {
            return !m_len;
        }

        inline bool has_content() const
        {
            return m_len != 0;
        }

        inline const char *get_ptr() const
        {
            return m_buf.get_ptr();
        }

        inline char front() const
        {
            return m_len ? get_ptr()[0] : '\0';
        }
        inline char back() const
        {
            return m_len ? get_ptr()[m_len - 1] : '\0';
        }

        inline char operator[](uint32_t i) const
        {
            return m_buf[i];
        }

        // Index string, beginning from back, 0 = last character, 1 = next to last char, etc.
        inline char get_char_at_end(uint32_t i) const
        {
            VOGL_ASSERT(i <= m_len);
            return (i < m_len) ? get_ptr()[static_cast<int>(m_len) - 1 - static_cast<int>(i)] : '\0';
        }

        inline string_hash get_hash() const
        {
            return string_hash(get_ptr(), m_len);
        }

        inline fixed_string get_clone() const
        {
            return fixed_string(*this);
        }

        // IMPORTANT: These comparison methods are all CASE INSENSITIVE by default!
        inline int compare(const fixed_string &rhs, bool case_sensitive = false) const
        {
            return compare(rhs.get_ptr(), case_sensitive);
        }

        inline int compare(const char *p, bool case_sensitive = false) const
        {
            VOGL_ASSERT(p);

            uint32_t l_len = m_len;

            int result = 0;
            if (case_sensitive)
                result = strncmp(get_ptr(), p, l_len);
            else
                result = vogl_strnicmp(get_ptr(), p, l_len);

            return (result < 0) ? -1 : ((result > 0) ? 1 : 0);
        }

        // IMPORTANT: These comparison methods are all CASE INSENSITIVE by default!
        inline bool operator==(const fixed_string &rhs) const
        {
            return compare(rhs) == 0;
        }
        inline bool operator==(const char *p) const
        {
            return compare(p) == 0;
        }

        inline bool operator<(const fixed_string &rhs) const
        {
            return compare(rhs) < 0;
        }
        inline bool operator<(const char *p) const
        {
            return compare(p) < 0;
        }

        inline bool operator>(const fixed_string &rhs) const
        {
            return compare(rhs) > 0;
        }
        inline bool operator>(const char *p) const
        {
            return compare(p) > 0;
        }

        inline bool operator<=(const fixed_string &rhs) const
        {
            return compare(rhs) <= 0;
        }
        inline bool operator<=(const char *p) const
        {
            return compare(p) <= 0;
        }

        inline bool operator>=(const fixed_string &rhs) const
        {
            return compare(rhs) >= 0;
        }
        inline bool operator>=(const char *p) const
        {
            return compare(p) >= 0;
        }

        friend inline bool operator==(const char *p, const fixed_string &rhs)
        {
            return rhs.compare(p) == 0;
        }

        inline bool set(const char *p, uint32_t max_len = UINT_MAX)
        {
            VOGL_ASSERT(p);

            uint32_t len = vogl_strlen(p);

            const uint32_t copy_len = math::minimum<uint32_t>(max_len, len, cMaxLen);

            memmove(m_buf.get_ptr(), p, copy_len * sizeof(char));

            m_buf[len] = 0;

            m_len = len;

            check();

            return copy_len == len;
        }

        template <uint32_t OtherBufSize>
        inline fixed_string &operator=(const fixed_string<OtherBufSize> &other)
        {
            if ((void *)this != (void *)&other)
            {
                memcpy(m_buf.get_ptr(), other.get_ptr(), other.get_len() + 1);
                m_len = other.get_len();

                check();
            }

            return *this;
        }

        inline fixed_string &operator=(const char *p)
        {
            set(p);
            return *this;
        }

        inline fixed_string &set_char(uint32_t index, char c)
        {
            VOGL_ASSERT(index <= m_len);
            VOGL_ASSERT(c != 0);
            if (!c)
                return *this;

            if (index <= m_len)
            {
                m_buf[index] = c;
                if (index == m_len)
                {
                    m_buf[index + 1] = 0;
                    m_len = math::minimum<uint32_t>(cMaxLen, m_len + 1);
                }

                check();
            }

            return *this;
        }

        inline fixed_string &append_char(char c)
        {
            return set_char(m_len, c);
        }

        inline fixed_string &truncate(uint32_t new_len)
        {
            VOGL_ASSERT(new_len <= m_len);
            if (new_len < m_len)
            {
                m_buf[new_len] = 0;
                m_len = new_len;
            }

            return *this;
        }

        inline fixed_string &tolower()
        {
            vogl_strlwr(m_buf);
            return *this;
        }

        inline fixed_string &toupper()
        {
            vogl_strupr(m_buf);
            return *this;
        }

        inline fixed_string &append(const char *p)
        {
            uint32_t p_len = math::minimum<uint32_t>(cMaxLen - m_len, vogl_strlen(p));

            if (p_len)
            {
                memmove(m_buf.get_ptr() + m_len, p, p_len);

                m_len = m_len + p_len;

                m_buf[m_len] = 0;

                check();
            }

            return *this;
        }

        template <uint32_t OtherBufSize>
        inline fixed_string &append(const fixed_string<OtherBufSize> &b)
        {
            uint32_t b_len = math::minimum<uint32_t>(cMaxLen - m_len, b.get_len());

            if (b_len)
            {
                memmove(m_buf.get_ptr() + m_len, b.get_ptr(), b_len);

                m_len = m_len + b_len;

                m_buf[m_len] = 0;

                check();
            }

            return *this;
        }

        inline fixed_string &operator+=(const char *p)
        {
            return append(p);
        }

        template <uint32_t OtherBufSize>
        inline fixed_string &operator+=(const fixed_string<OtherBufSize> &b)
        {
            return append(b);
        }

        friend inline fixed_string operator+(const fixed_string &a, const fixed_string &b)
        {
            return fixed_string(a).append(b);
        }

        inline fixed_string &format_args(const char *p, va_list args)
        {
            int l = vogl_vsprintf_s(m_buf.get_ptr(), BufSize, p, args);

            if (l < 0)
                truncate(0);
            else
                m_len = l;

            check();

            return *this;
        }

        inline fixed_string &format(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(2, 3)
        {
            va_list args;
            va_start(args, p);
            format_args(p, args);
            va_end(args);
            return *this;
        }

        // Note many of these ops are IN-PLACE string operations to avoid constructing new objects, which can make them inconvienent.
        inline fixed_string &crop(uint32_t start, uint32_t len)
        {
            if (start >= m_len)
            {
                clear();
                return *this;
            }

            len = math::minimum<uint32_t>(len, m_len - start);

            if (start)
                memmove(m_buf.get_ptr(), m_buf.get_ptr() + start, len);

            m_buf[len] = 0;

            m_len = len;

            check();

            return *this;
        }

        // half-open interval
        inline fixed_string &substring(uint32_t start, uint32_t end)
        {
            VOGL_ASSERT(start <= end);
            if (start > end)
                return *this;
            return crop(start, end - start);
        }

        inline fixed_string &left(uint32_t len)
        {
            return substring(0, len);
        }

        inline fixed_string &mid(uint32_t start, uint32_t len)
        {
            return crop(start, len);
        }

        inline fixed_string &right(uint32_t start)
        {
            return substring(start, get_len());
        }

        inline int find_left(const char *p, bool case_sensitive = false) const
        {
            VOGL_ASSERT(p);

            const int p_len = vogl_strlen(p);

            for (int i = 0; i <= (static_cast<int>(m_len) - p_len); i++)
                if ((case_sensitive ? strncmp : vogl_strnicmp)(p, &m_buf[i], p_len) == 0)
                    return i;

            return -1;
        }

        inline int find_left(char c) const
        {
            for (uint32_t i = 0; i < m_len; i++)
                if (m_buf[i] == c)
                    return i;
            return -1;
        }

        inline int find_right(char c) const
        {
            for (int i = (int)m_len - 1; i >= 0; i--)
                if (m_buf[i] == c)
                    return i;
            return -1;
        }

        inline int find_right(const char *p, bool case_sensitive = false) const
        {
            VOGL_ASSERT(p);
            const int p_len = vogl_strlen(p);

            for (int i = m_len - p_len; i >= 0; i--)
                if ((case_sensitive ? strncmp : vogl_strnicmp)(p, &m_buf[i], p_len) == 0)
                    return i;

            return -1;
        }

        inline fixed_string &trim()
        {
            int s, e;
            for (s = 0; s < (int)m_len; s++)
                if (!vogl_isspace(m_buf[s]))
                    break;

            for (e = m_len - 1; e > s; e--)
                if (!vogl_isspace(m_buf[e]))
                    break;

            crop(s, e - s + 1);
            return *this;
        }

    private:
        uint32_t m_len;
        fixed_array<char, BufSize> m_buf;

        inline void check() const
        {
            VOGL_ASSERT(m_len <= cMaxLen);
            VOGL_ASSERT(vogl_strlen(m_buf.get_ptr()) == m_len);
        }
    };

    typedef fixed_string<32> fixed_string32;
    typedef fixed_string<64> fixed_string64;
    typedef fixed_string<128> fixed_string128;
    typedef fixed_string<256> fixed_string256;

    template <uint32_t N>
    struct bitwise_movable<fixed_string<N> >
    {
        enum
        {
            cFlag = true
        };
    };

    template <uint32_t N>
    struct hasher<fixed_string<N> >
    {
        inline size_t operator()(const fixed_string<N> &key) const
        {
            return key.get_hash();
        }
    };

} // namespace vogl
