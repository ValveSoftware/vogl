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

// File: vogl_regex.h
//
//
// Uses Henry Spencer's regex library, see regex_man.txt and regex2_man.txt
// regexp class enables REG_EXTENDED syntax.
//
#pragma once

#include "vogl_core.h"
#include "regex/regex.h"

namespace vogl
{
    enum regexp_flags
    {
        REGEX_IGNORE_CASE = REG_ICASE,
        REGEX_NOSUB = REG_NOSUB,
        REGEX_NEWLINE = REG_NEWLINE
    };

    struct regexp_match_loc
    {
        uint32_t m_start;
        uint32_t m_len;

        inline void set(uint32_t start, uint32_t len)
        {
            m_start = start;
            m_len = len;
        }
    };

    typedef vogl::vector<regexp_match_loc> regexp_match_loc_array;

    class regexp
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(regexp);

    public:
        regexp();
        regexp(const char *pPattern, uint32_t flags = 0);
        ~regexp();

        bool init(const char *pPattern, uint32_t flags = 0);
        void deinit();

        inline bool is_initialized() const
        {
            return m_is_initialized;
        }

        inline const dynamic_string &get_error() const
        {
            return m_error;
        }

        inline const dynamic_string_array &get_match_strings() const
        {
            return m_match_strings;
        }
        inline dynamic_string_array &get_match_strings()
        {
            return m_match_strings;
        }

        inline const regexp_match_loc_array &get_match_locs() const
        {
            return m_match_locs;
        }
        inline regexp_match_loc_array &get_match_locs()
        {
            return m_match_locs;
        }

        inline uint32_t get_num_matches() const
        {
            return m_match_strings.size();
        }
        inline const dynamic_string &get_match_string(uint32_t i) const
        {
            return m_match_strings[i];
        }
        inline const regexp_match_loc &get_match_loc(uint32_t i) const
        {
            return m_match_locs[i];
        }

        // True if there are any matches within string.
        bool find_any(const char *pString);

        // Returns the first match, or NULL if there are no matches.
        // begin will be the offset of the first char matched, end will be the offset 1 beyond the last char matched. end-begin=len of match.
        const char *find_first(const char *pString, int &begin, int &end);

        // Returns true if the regexp matches the entire string (i.e. find_first() successfully returns begin=0 end=length);
        bool full_match(const char *pString);

        // Finds all matches in string. To retrieve the matches, call get_num_matches(), get_match_string(), get_match_loc(), etc.
        uint32_t find(const char *pString);

        uint32_t replace(dynamic_string &result, const char *pString, const char *pReplacement);

    private:
        regex_t m_regex;
        bool m_is_initialized;
        dynamic_string_array m_match_strings;
        regexp_match_loc_array m_match_locs;
        dynamic_string m_error;
    };

    // Returns true if there are any matches.
    inline bool regexp_find_any(const char *pString, const char *pPattern, uint32_t flags = 0)
    {
        // TODO: Compile with REG_NOSUB?
        regexp r;
        if (!r.init(pPattern, flags))
            return false;
        return r.find_any(pString);
    }

    // Returns pointer to first match, or NULL if no matches.
    inline const char *regexp_find_first(const char *pString, const char *pPattern, int &begin, int &end, uint32_t flags = 0)
    {
        regexp r;
        if (!r.init(pPattern, flags))
            return NULL;
        return r.find_first(pString, begin, end);
    }

    // true if pattern matches the entire string.
    inline bool regexp_full_match(const char *pString, const char *pPattern, uint32_t flags = 0)
    {
        regexp r;
        if (!r.init(pPattern, flags))
            return false;
        return r.full_match(pString);
    }

    // Returns a string array containing all matches of pattern in string.
    inline dynamic_string_array regexp_find(const char *pString, const char *pPattern, uint32_t flags = 0)
    {
        regexp r;

        dynamic_string_array result;
        if (r.init(pPattern, flags))
        {
            if (r.find(pString))
                result.swap(r.get_match_strings());
        }

        return result;
    }

    // Replaces every instance of pattern in string, returns the new string. pReplacement may be NULL.
    inline dynamic_string regexp_replace(const char *pString, const char *pPattern, const char *pReplacement, uint32_t flags = 0)
    {
        dynamic_string result;

        regexp r;
        if (r.init(pPattern, flags))
            r.replace(result, pString, pReplacement);

        return result;
    }

    bool regexp_test();

} // namespace vogl

