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

// File: vogl_dynamic_string.h
//
// Dynamic string class with the small string optimization.
//
// Important note: For historical reasons the comparisons operators in this class are
// case insensitive (using locale independent helper functions) by default, mostly because
// the last X engines I've worked on used this default and I got used to it. Its the day after
// and now I'm kinda regretting this idea, but there's too much code that depends on this behavior.
// If you need to do case sensitive comparisons: either call compare(other, false), or dynamic_string_less_than_case_insensitive.
#pragma once

#include "vogl_core.h"

#ifdef VOGL_BUILD_DEBUG
#ifndef VOGL_SLOW_STRING_LEN_CHECKS
#define VOGL_SLOW_STRING_LEN_CHECKS 1
#endif
#endif

namespace vogl
{
    class json_value;
    class dynamic_string;
    typedef vogl::vector<dynamic_string> dynamic_string_array;

    // These limits are in place so plain signed int's can be safely used as offsets into the buffer (use something else if you need strings this big).
    enum
    {
        cMaxDynamicStringBufSize = 0x70000000U,
        cMaxDynamicStringLen = cMaxDynamicStringBufSize - 1
    };

    class dynamic_string
    {
    public:
        inline dynamic_string()
        {
            set_to_empty_small_string();
        }
        dynamic_string(eVarArg dummy, const char *p, ...) VOGL_ATTRIBUTE_PRINTF(3, 4);
        dynamic_string(const char *p);
        dynamic_string(const char *p, uint32_t len);
        dynamic_string(const dynamic_string &other);

        inline ~dynamic_string()
        {
            if (is_dynamic())
                vogl_delete_array(m_dyn.m_pStr);
        }

        // Truncates the string to 0 chars and frees the buffer.
        void clear();
        void optimize();

        void reserve(uint32_t new_capacity);

        // Truncates the string to 0 chars, but does not free the buffer.
        void empty();

        // Returns the string buffer to the caller and clears the string.
        // The caller takes ownership of the returned buffer, free it using vogl_delete_array(pStr);
        inline const char *assume_ownership()
        {
            ensure_dynamic();
            const char *p = m_dyn.m_pStr;
            set_to_empty_small_string();
            return p;
        }

        inline uint32_t get_len() const
        {
            return m_len;
        }
        inline uint32_t size() const
        {
            return m_len;
        }
        inline bool is_empty() const
        {
            return !m_len;
        }
        inline bool has_content() const
        {
            return m_len != 0;
        }

        // Returns a pointer to the zero terminated C-style string (which may be a zero terminated empty string) - never NULL.
        inline const char *get_ptr() const
        {
            return get_ptr_priv();
        }
        inline const char *c_str() const
        {
            return get_ptr_priv();
        }

        // Returns a pointer to the zero terminated C-style string (which may be a zero terminated empty string) - never NULL.
        // Do not change the length of the string via this pointer! This method is intended for faster deserialization.
        inline char *get_ptr_raw()
        {
            return get_ptr_priv();
        }

        // Returns a duplicate of the string.
        dynamic_string get_clone() const
        {
            return dynamic_string(*this);
        }

        inline char front() const
        {
            return m_len ? get_ptr_priv()[0] : '\0';
        }
        inline char back() const
        {
            return m_len ? get_ptr_priv()[m_len - 1] : '\0';
        }

        inline char operator[](uint32_t i) const
        {
            VOGL_ASSERT(i <= m_len);
            return get_ptr()[i];
        }

        // Index string, beginning from back, 0 = last character, 1 = next to last char, etc.
        inline char get_char_at_end(uint32_t i) const
        {
            VOGL_ASSERT(i <= m_len);
            return (i < m_len) ? get_ptr()[m_len - 1U - i] : '\0';
        }

        inline string_hash get_hash() const
        {
            return string_hash(get_ptr(), m_len);
        }

        // IMPORTANT: These comparison methods are all CASE INSENSITIVE by default!
        // Yes this seems maybe sorta nutty to me now, but in the past I've worked on a bunch of projects where this was the default and it was rarely a problem.
        int compare(const char *p, bool case_sensitive = false) const;
        int compare(const dynamic_string &rhs, bool case_sensitive = false) const;

        // length of string is the first key, then followed by a lex compare using compare() if lengths are equal
        int compare_using_length(const char *p, bool case_sensitive = false) const;
        int compare_using_length(const dynamic_string &rhs, bool case_sensitive = false) const;

        // IMPORTANT: These comparison methods are all CASE INSENSITIVE by default!
        inline bool operator==(const dynamic_string &rhs) const
        {
            return compare(rhs) == 0;
        }
        inline bool operator==(const char *p) const
        {
            return compare(p) == 0;
        }

        inline bool operator!=(const dynamic_string &rhs) const
        {
            return compare(rhs) != 0;
        }
        inline bool operator!=(const char *p) const
        {
            return compare(p) != 0;
        }

        inline bool operator<(const dynamic_string &rhs) const
        {
            return compare(rhs) < 0;
        }
        inline bool operator<(const char *p) const
        {
            return compare(p) < 0;
        }

        inline bool operator>(const dynamic_string &rhs) const
        {
            return compare(rhs) > 0;
        }
        inline bool operator>(const char *p) const
        {
            return compare(p) > 0;
        }

        inline bool operator<=(const dynamic_string &rhs) const
        {
            return compare(rhs) <= 0;
        }
        inline bool operator<=(const char *p) const
        {
            return compare(p) <= 0;
        }

        inline bool operator>=(const dynamic_string &rhs) const
        {
            return compare(rhs) >= 0;
        }
        inline bool operator>=(const char *p) const
        {
            return compare(p) >= 0;
        }

        friend inline bool operator==(const char *p, const dynamic_string &rhs)
        {
            return rhs.compare(p) == 0;
        }

        dynamic_string &set(const char *p, uint32_t max_len = UINT_MAX);
        dynamic_string &set(const dynamic_string &other, uint32_t max_len = UINT_MAX);

        bool set_len(uint32_t new_len, char fill_char = ' ');

        // Set from a NON-zero terminated buffer (i.e. pBuf cannot contain any 0's, buf_len_in_chars is the size of the string).
        dynamic_string &set_from_buf(const void *pBuf, uint32_t buf_len_in_chars);

        dynamic_string &operator=(const dynamic_string &rhs)
        {
            return set(rhs);
        }
        dynamic_string &operator=(const char *p)
        {
            return set(p);
        }

        dynamic_string &set_char(uint32_t index, char c);
        dynamic_string &append_char(char c);
        dynamic_string &append_char(int c)
        {
            VOGL_ASSERT((c > 0) && (c <= 255));
            return append_char(static_cast<char>(c));
        }
        dynamic_string &truncate(uint32_t new_len);
        dynamic_string &shorten(uint32_t chars_to_remove);
        dynamic_string &tolower();
        dynamic_string &toupper();

        dynamic_string &append(const char *p);
        dynamic_string &append(const char *p, uint32_t len);
        dynamic_string &append(const dynamic_string &other);
        dynamic_string &operator+=(const char *p)
        {
            return append(p);
        }
        dynamic_string &operator+=(const dynamic_string &other)
        {
            return append(other);
        }

        friend dynamic_string operator+(const char *p, const dynamic_string &a);
        friend dynamic_string operator+(const dynamic_string &a, const char *p);
        friend dynamic_string operator+(const dynamic_string &a, const dynamic_string &b);

        dynamic_string &format_args(const char *p, va_list args);
        dynamic_string &format(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(2, 3);
        dynamic_string &format_append(const char *p, ...) VOGL_ATTRIBUTE_PRINTF(2, 3);

        // Note many of these ops are IN-PLACE string operations to avoid constructing new dynamic_string objects.
        // Yes this is awkward, one way to work around this is to close the string like this (which introduces a clone and then does the op in-place):
        // result = str.get_clone().left(x);
        dynamic_string &crop(uint32_t start, uint32_t len);
        dynamic_string &remove(uint32_t start, uint32_t len);

        // trims string to [start, end), i.e. up to but not including end
        dynamic_string &substring(uint32_t start, uint32_t end);

        dynamic_string &left(uint32_t len);
        dynamic_string &mid(uint32_t start, uint32_t len);
        dynamic_string &right(uint32_t start);
        dynamic_string &tail(uint32_t num);

        dynamic_string &replace(const char *pFind, const char *pReplacement, bool case_sensitive = true, uint32_t *pNum_found = NULL, uint32_t max_replacements = UINT_MAX);

        dynamic_string &unquote();

        uint32_t count_char(char c) const;

        int find_left(const char *p, bool case_sensitive = false, uint32_t start_ofs = 0) const;
        int find_left(char c, int start_ofs = 0) const;

        int find_right(char c) const;
        int find_right(char c, uint32_t start_ofs) const;
        int find_right(const char *p, bool case_sensitive = false) const;

        bool contains(const char *p, bool case_sensitive = false) const;
        bool contains(char c) const;

        bool begins_with(const char *p, bool case_sensitive = false) const;
        bool ends_with(const char *p, bool case_sensitive = false) const;

        dynamic_string &trim();
        dynamic_string &trim_end();
        dynamic_string &trim_crlf();

        dynamic_string &remap(int from_char, int to_char);

        void swap(dynamic_string &other);

        void tokenize(const char *pDelims, dynamic_string_array &tokens, bool trim = false) const;

        uint32_t get_serialize_size() const
        {
            return sizeof(uint32_t) + m_len;
        }

        // Returns -1 on failure, or the number of bytes written.
        int serialize(void *pBuf, uint32_t buf_size, bool little_endian) const;

        // Returns -1 on failure, or the number of bytes read.
        int deserialize(const void *pBuf, uint32_t buf_size, bool little_endian);

        void translate_lf_to_crlf();

        static inline char *create_raw_buffer(uint32_t &buf_size_in_chars);
        static inline void free_raw_buffer(char *p)
        {
            vogl_delete_array(p);
        }

        // Buf size must be a power of 2, or cUINT32_MAX. TODO: Relax this BS.
        dynamic_string &set_from_raw_buf_and_assume_ownership(char *pBuf, uint32_t buf_size_in_bytes, uint32_t len_in_chars);

        bool validate() const;

        // Forces the string to use a dynamically allocated buffer (the buffer is not optimized for enlarging).
        void ensure_dynamic();

        // True if the string is dynamic, false if it's static/small
        inline bool is_dynamic() const
        {
            return (m_small.m_flag & 1) == 0;
        }

        // True if the string is inside of the object, false if it's dynamic.
        inline bool is_small_string() const
        {
            return !is_dynamic();
        }

    private:
        enum
        {
            cSmallStringExtraBufSize = 4,
            cSmallStringBufSize = ((sizeof(char *) - 1) + sizeof(uint32_t)) + cSmallStringExtraBufSize,
            cSmallStringMaxLen = cSmallStringBufSize - 1,
        };

        // TODO: Store m_len at the end of the struct, and use 1 byte to hold X-len. When the string is size X, the last byte will be zero, so we can use the entire struct to hold the string.
        // Current string length in bytes.
        // In this design, the string can be either static (small) or dynamic, independent of its size.
        // I could gate whether or not the string is dynamic pased purely off the length, but this would be more restrictive.
        uint32_t m_len;

        union
        {
            struct
            {
                // m_pStr is always zero terminated, and never NULL. Obviously, don't access it directly, use get_ptr() or get_ptr_priv() instead.
                char *m_pStr;

                // Current buffer size in bytes.
                uint32_t m_buf_size;
            } m_dyn;

            struct
            {
                uint8_t m_flag;
                char m_buf[cSmallStringBufSize];
            } m_small;
        };

        inline void check() const
        {
            VOGL_ASSERT(validate());
        }

        // expands buffer to at least new_buf_size bytes
        bool expand_buf(uint32_t new_buf_size, bool preserve_contents);

        // ensures a buffer at least large enough to hold len chars + zero terminator
        bool ensure_buf(uint32_t len, bool preserve_contents = true);

        inline uint32_t get_buf_size() const
        {
            return is_dynamic() ? static_cast<uint32_t>(m_dyn.m_buf_size) : static_cast<uint32_t>(cSmallStringBufSize);
        }

        inline void set_small_string_flag()
        {
            m_small.m_flag = 1;
        }

        inline void set_to_empty_small_string()
        {
            m_len = 0;
            m_small.m_flag = 1;
            m_small.m_buf[0] = 0;
        }

        inline void set_dyn_string_ptr(char *p)
        {
            VOGL_ASSERT(p);
            m_dyn.m_pStr = p;
            VOGL_ASSERT(is_dynamic());
        }

        // Returns ptr to string, never NULL
        inline const char *get_ptr_priv() const
        {
            return is_dynamic() ? m_dyn.m_pStr : m_small.m_buf;
        }

        // Returns ptr to string, never NULL
        inline char *get_ptr_priv()
        {
            return is_dynamic() ? m_dyn.m_pStr : m_small.m_buf;
        }

        inline bool ptr_refers_to_self(const char *p) const
        {
            const char *pStr = get_ptr_priv();
            return (p >= pStr) && (p < (pStr + get_buf_size()));
        }
    };

    inline dynamic_string& get_empty_dynamic_string()
    {
        static dynamic_string s_empty_dynamic_string;
        return s_empty_dynamic_string;
    }

    VOGL_DEFINE_BITWISE_MOVABLE(dynamic_string);

    inline void swap(dynamic_string &a, dynamic_string &b)
    {
        a.swap(b);
    }

    inline char *dynamic_string::create_raw_buffer(uint32_t &buf_size_in_chars)
    {
        buf_size_in_chars = static_cast<uint32_t>(math::minimum<uint64_t>(cUINT32_MAX, math::next_pow2(static_cast<uint64_t>(buf_size_in_chars))));
        return vogl_new_array(char, buf_size_in_chars);
    }

    inline int scan_dynamic_string_array_for_string(const dynamic_string_array &strings, const char *pStr, bool case_sensitive)
    {
        for (uint32_t i = 0; i < strings.size(); i++)
            if (!strings[i].compare(pStr, case_sensitive))
                return i;
        return -1;
    }

    template <>
    struct hasher<dynamic_string>
    {
        inline size_t operator()(const dynamic_string &key) const
        {
            return key.get_hash();
        }
    };

    struct dynamic_string_less_than_case_insensitive
    {
        inline bool operator()(const dynamic_string &a, const dynamic_string &b) const
        {
            return a.compare(b, false) < 0;
        }
    };

    struct dynamic_string_equal_to_case_insensitive
    {
        inline bool operator()(const dynamic_string &a, const dynamic_string &b) const
        {
            return a.compare(b, false) == 0;
        }
    };

    struct dynamic_string_less_than_case_sensitive
    {
        inline bool operator()(const dynamic_string &a, const dynamic_string &b) const
        {
            return a.compare(b, true) < 0;
        }
    };

    struct dynamic_string_equal_to_case_sensitive
    {
        inline bool operator()(const dynamic_string &a, const dynamic_string &b) const
        {
            return a.compare(b, true) == 0;
        }
    };

    bool json_serialize(const dynamic_string &str, json_value &val);
    bool json_deserialize(dynamic_string &str, const json_value &val);

    bool dynamic_string_test();
} // namespace vogl

namespace std
{
    template <>
    inline void swap(vogl::dynamic_string &a, vogl::dynamic_string &b)
    {
        a.swap(b);
    }

} // namespace std

