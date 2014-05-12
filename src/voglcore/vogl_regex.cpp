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

// File: vogl_regex.cpp
#include "vogl_core.h"
#include "vogl_regex.h"
#include "vogl_strutils.h"

namespace vogl
{
    regexp::regexp()
        : m_is_initialized(false)
    {
    }

    regexp::regexp(const char *pPattern, uint32_t flags)
        : m_is_initialized(false)
    {
        init(pPattern, flags);
    }

    regexp::~regexp()
    {
        deinit();
    }

    void regexp::deinit()
    {
        if (m_is_initialized)
        {
            vogl_regfree(&m_regex);
            m_is_initialized = false;
        }

        m_error.clear();
        m_match_strings.clear();
        m_match_locs.clear();
    }

    bool regexp::init(const char *pPattern, uint32_t flags)
    {
        deinit();

        flags |= REG_EXTENDED;

        int errcode = vogl_regcomp(&m_regex, pPattern, flags);
        if (errcode != 0)
        {
            size_t errbuf_size = vogl_regerror(errcode, &m_regex, NULL, 0);
            if ((errbuf_size) && (errbuf_size < cUINT32_MAX))
            {
                vogl::vector<char> errbuf(static_cast<uint32_t>(errbuf_size));

                vogl_regerror(errcode, &m_regex, errbuf.get_ptr(), errbuf_size);

                m_error.set(errbuf.get_ptr());
            }
            else
            {
                m_error = "unknown regex error";
            }

            return false;
        }

        m_is_initialized = true;

        return true;
    }

    bool regexp::find_any(const char *pString)
    {
        if (!m_is_initialized)
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        return vogl_regexec(&m_regex, pString, 0, NULL, 0) == 0;
    }

    const char *regexp::find_first(const char *pString, int &begin, int &end)
    {
        begin = 0;
        end = 0;

        if (!m_is_initialized)
        {
            VOGL_ASSERT_ALWAYS;
            return NULL;
        }

        regmatch_t match;
        if ((vogl_regexec(&m_regex, pString, 1, &match, 0)) != 0)
            return NULL;

        if (match.rm_so < 0)
            return NULL;

        begin = static_cast<int>(match.rm_so);
        end = static_cast<int>(match.rm_eo);

        return pString + match.rm_so;
    }

    bool regexp::full_match(const char *pString)
    {
        int begin, end;
        const char *p = find_first(pString, begin, end);
        if (!p)
            return false;

        return (!begin) && (pString[end] == '\0');
    }

    uint32_t regexp::find(const char *pString)
    {
        if (!m_is_initialized)
            return false;

        m_match_strings.resize(0);
        m_match_locs.resize(0);

        regmatch_t match;
        const char *pCur = pString;

        for (;;)
        {
            if ((vogl_regexec(&m_regex, pCur, 1, &match, 0)) != 0)
                break;

            if (match.rm_so < 0)
                break;

            if ((match.rm_eo >= cMaxDynamicStringLen) || (match.rm_so >= cMaxDynamicStringLen))
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }

            int len = static_cast<int>(match.rm_eo) - static_cast<int>(match.rm_so);
            if ((len < 0) || (len >= cMaxDynamicStringLen))
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }

            dynamic_string str;
            str.set_len(len);
            str.set_from_buf(pCur + match.rm_so, static_cast<int>(len));

            m_match_strings.enlarge(1)->swap(str);
            m_match_locs.enlarge(1)->set(static_cast<int>(pCur - pString) + static_cast<int>(match.rm_so), len);

            pCur += match.rm_eo;
            if (match.rm_so == match.rm_eo)
                break;

            if (*pCur == '\0')
                break;
        }

        return m_match_strings.size();
    }

    uint32_t regexp::replace(dynamic_string &result, const char *pString, const char *pReplacement)
    {
        uint32_t n = find(pString);
        if (!n)
        {
            result = pString;
            return 0;
        }

        result.set_len(0);

        uint32_t str_len = vogl_strlen(pString);
        uint32_t cur_ofs = 0;

        for (uint32_t i = 0; i < n; i++)
        {
            const regexp_match_loc &cur_match = m_match_locs[i];

            VOGL_ASSERT(cur_match.m_start >= cur_ofs);

            if (cur_match.m_start > cur_ofs)
                result.append(pString + cur_ofs, cur_match.m_start - cur_ofs);

            if (pReplacement)
                result.append(pReplacement);

            cur_ofs = cur_match.m_start + cur_match.m_len;
        }

        if (cur_ofs < str_len)
        {
            result.append(pString + cur_ofs, str_len - cur_ofs);
        }

        return n;
    }

    static uint32_t regexp_test(const char *pPat, const char *pStr)
    {
        dynamic_string_array matches(regexp_find(pStr, pPat));
        printf("Pattern: \"%s\" String: \"%s\" Find: %u\n", pPat, pStr, matches.size());
        for (uint32_t i = 0; i < matches.size(); i++)
            printf("\"%s\"\n", matches[i].get_ptr());
        return matches.size();
    }

    static bool regexp_test2(const char *pPat, const char *pStr)
    {
        bool result = regexp_find_any(pStr, pPat);
        printf("Pattern: \"%s\" String: \"%s\" FindAny: %u\n", pPat, pStr, result);
        return result;
    }

    static bool regexp_test3(const char *pPat, const char *pStr)
    {
        bool result = regexp_full_match(pStr, pPat);
        printf("Pattern: \"%s\" String: \"%s\" Match: %u\n", pPat, pStr, result);
        return result;
    }

    static uint32_t g_regexp_test_fail_count;
    static void regexp_test_check(bool success)
    {
        if (!success)
        {
            g_regexp_test_fail_count++;
            VOGL_VERIFY(0);
        }
    }

    bool regexp_test()
    {
        g_regexp_test_fail_count = 0;

        // Some of these tests where borrowed from stb.h.

        regexp_test_check(regexp_test("ab+c", "ab") == 0);
        regexp_test_check(regexp_test("ab+c", "abc blah") == 1);
        regexp_test_check(regexp_test("ab+c$", "abc blah") == 0);
        regexp_test_check(regexp_test("ab+c$", "abc") == 1);
        regexp_test_check(regexp_test("ab+c", "abc") == 1);
        regexp_test_check(regexp_test("ab+c", "abbc") == 1);
        regexp_test_check(regexp_test("get|set", "getter") == 1);
        regexp_test_check(regexp_test("get|set", "setter") == 1);
        regexp_test_check(regexp_test("[a.c]", "a") == 1);
        regexp_test_check(regexp_test("[a.c]", "z") == 0);
        regexp_test_check(regexp_test("[a.c]", ".") == 1);
        regexp_test_check(regexp_test("[a.c]", "c") == 1);
        regexp_test_check(regexp_test("gr(a|e)y", "grey") == 1);
        regexp_test_check(regexp_test("gr(a|e)y", "gray") == 1);
        regexp_test_check(regexp_test("gr(a|e)y", "gry") == 0);
        regexp_test_check(regexp_test("g{1}", "g") == 1);
        regexp_test_check(regexp_test("g{3,5}", "gg") == 0);
        regexp_test_check(regexp_test("g{3,5}", "gggg") == 1);
        regexp_test_check(regexp_test("g{3,5}", "ggggg") == 1);
        regexp_test_check(regexp_test("g{3,5}", "gggggg") == 1);
        regexp_test_check(regexp_test("g{3,5}", "ggggggXggg") == 2);
        regexp_test_check(regexp_test("(g{3,5}|x{3,7})", "ggggggXxxx!gggg") == 3);
        regexp_test_check(regexp_test2("Uniform", "glUniform") == 1);
        regexp_test_check(regexp_test3("Uniform", "Uniform") == 1);
        regexp_test_check(regexp_test3("Uniform$", "Uniform") == 1);
        regexp_test_check(regexp_test3("Uniform$", "UniformBlah") == 0);
        regexp_test_check(regexp_test3("Uniform", "UniformBlah") == 0);
        regexp_test_check(regexp_test3("Uniform", "glUniformBlah") == 0);

        regexp_test_check(regexp_test("<h(.)>([^<]+)", "<h2>Egg prices</h2>") == 1);

        {
            dynamic_string x;
            regexp r;
            r.init("[az]");
            r.replace(x, "AaThis is a test zz BlAh", "!!!!!!");
            printf("%s\n", x.get_ptr());
            regexp_test_check(x.compare("A!!!!!!This is !!!!!! test !!!!!!!!!!!! BlAh", true) == 0);
        }

        {
            dynamic_string x;
            regexp r;
            r.init("[az]");
            r.replace(x, "aThis is a test zz BlAh", "!!!!!!");
            printf("%s\n", x.get_ptr());
            regexp_test_check(x.compare("!!!!!!This is !!!!!! test !!!!!!!!!!!! BlAh", true) == 0);
        }

        {
            dynamic_string x;
            regexp r;
            r.init("[az]");
            r.replace(x, "aThis is a test zz BlAha", "!!!!!!");
            printf("%s\n", x.get_ptr());
            regexp_test_check(x.compare("!!!!!!This is !!!!!! test !!!!!!!!!!!! BlAh!!!!!!", true) == 0);
        }

        regexp_test_check(regexp_replace("Blah", "a", "!").compare("Bl!h", true) == 0);
        regexp_test_check(regexp_replace("Blah", "Blah", "!").compare("!", true) == 0);
        regexp_test_check(regexp_replace("BlahX", "Blah", "!").compare("!X", true) == 0);
        regexp_test_check(regexp_replace("BlahX", "Blah", "X").compare("XX", true) == 0);
        regexp_test_check(regexp_replace("Blah", "Blah", "") == "");
        regexp_test_check(regexp_replace("XBlahX", "Blah", "").compare("XX", true) == 0);
        regexp_test_check(regexp_replace("XBlahX", "Blah", NULL).compare("XX", true) == 0);

        const char *z = "foo.*[bd]ak?r";
        regexp_test_check(regexp_test(z, "muggle man food is barfy") == 1);
        regexp_test_check(regexp_test("foo.*bar", "muggle man food is farfy") == 0);
        regexp_test_check(regexp_test("[^a-zA-Z]foo[^a-zA-Z]", "dfoobar xfood") == 0);
        regexp_test_check(regexp_test(z, "muman foob is bakrfy") == 1);
        z = "foo.*[bd]bk?r";
        regexp_test_check(regexp_test(z, "muman foob is bakrfy") == 0);
        regexp_test_check(regexp_test(z, "muman foob is bbkrfy") == 1);

        regexp_test_check(regexp_test("ab+c", "abc") == 1);
        regexp_test_check(regexp_test("ab+c", "abbc") == 1);
        regexp_test_check(regexp_test("ab+c", "abbbc") == 1);
        regexp_test_check(regexp_test("ab+c", "ac") == 0);

        regexp_test_check(regexp_test("ab?c", "ac") == 1);
        regexp_test_check(regexp_test("ab?c", "abc") == 1);
        regexp_test_check(regexp_test("ab?c", "abbc") == 0);
        regexp_test_check(regexp_test("ab?c", "abbbc") == 0);
        regexp_test_check(regexp_test("ab?c", "babbbc") == 0);

        regexp_test_check(regexp_test("gr(a|e)y", "grey") == 1);
        regexp_test_check(regexp_test("gr(a|e)y", "graey") == 0);
        regexp_test_check(regexp_test("gr(a|e)y", "gray") == 1);
        regexp_test_check(regexp_test("gr(a?|e)y", "gry") == 1);
        regexp_test_check(regexp_test("gr(a?|e)y", "grey") == 1);
        regexp_test_check(regexp_test("gr(a?|e)y", "gray") == 1);

        regexp_test_check(regexp_test("(a|b)*", "") == 1);
        regexp_test_check(regexp_test("(a|b)*", "a") == 1);
        regexp_test_check(regexp_test("(a|b)*", "ab") == 1);
        regexp_test_check(regexp_test("(a|b)*", "aa") == 1);
        regexp_test_check(regexp_test("(a|b)*", "ba") == 1);
        regexp_test_check(regexp_test("(a|b)*", "bb") == 1);
        regexp_test_check(regexp_test("(a|b)*", "aaa") == 1);
        regexp_test_check(regexp_test("(a|b)", "z") == 0);
        regexp_test_check(regexp_test("*", "z") == 0); // purposely bogus

        regexp_test_check(regexp_test("", "abc") == 0); // purposely bogus
        regexp_test_check(regexp_test("(a|b)*", "abc") == 2);
        regexp_test_check(regexp_test("(a|b)*", "c") == 1);
        regexp_test_check(regexp_test("(a|b)*c", "abcab") == 1);
        regexp_test_check(regexp_test("(a|b)*c", "abcbac") == 2);

        {
            regexp r;
            r.init(".*foo.*bar.*");
            regexp_test_check(r.find_any("foobarx") == 1);
            regexp_test_check(r.find_any("foobar") == 1);
            regexp_test_check(r.find_any("foo bar") == 1);
            regexp_test_check(r.find_any("fo foo ba ba bar ba") == 1);
            regexp_test_check(r.find_any("fo oo oo ba ba bar foo") == 0);
        }

        {
            regexp r;
            r.init(".*foo.?bar.*");
            regexp_test_check(r.find_any("abfoobarx") == 1);
            regexp_test_check(r.find_any("abfoobar") == 1);
            regexp_test_check(r.find_any("abfoo bar") == 1);
            regexp_test_check(r.find_any("abfoo  bar") == 0);
            regexp_test_check(r.find_any("abfo foo ba ba bar ba") == 0);
            regexp_test_check(r.find_any("abfo oo oo ba ba bar foo") == 0);
        }

        {
            regexp r;
            r.init(".*m((foo|bar)*baz)m.*");
            regexp_test_check(r.find_any("abfoobarx") == 0);
            regexp_test_check(r.find_any("a mfoofoofoobazm d") == 1);
            regexp_test_check(r.find_any("a mfoobarbazfoom d") == 0);
            regexp_test_check(r.find_any("a mbarbarfoobarbazm d") == 1);
            regexp_test_check(r.find_any("a mfoobarfoo bazm d") == 0);
            regexp_test_check(r.find_any("a mm foobarfoobarfoobar ") == 0);
        }

        {
            regexp r;
            r.init("f*|z");
            regexp_test_check(r.find_any("fz") == 1);
            regexp_test_check(r.find("fz") == 2);
            regexp_test_check(r.find_any("ff") == 1);
            regexp_test_check(r.find_any("z") == 1);
        }

        {
            regexp r;
            r.init("m(f|z*)n");
            regexp_test_check(r.find_any("mfzn") == 0);
            regexp_test_check(r.find_any("mffn") == 0);
            regexp_test_check(r.find_any("mzn") == 1);
            regexp_test_check(r.find_any("mn") == 1);
            regexp_test_check(r.find_any("mzfn") == 0);

            regexp_test_check(r.find_any("manmanmannnnnnnmmmmmmmmm       ") == 0);
            regexp_test_check(r.find_any("manmanmannnnnnnmmmmmmmmm       ") == 0);
            regexp_test_check(r.find_any("manmanmannnnnnnmmmmmmmmmffzzz  ") == 0);
            regexp_test_check(r.find_any("manmanmannnnnnnmmmmmmmmmnfzzz  ") == 1);
            regexp_test_check(r.find_any("mmmfn aanmannnnnnnmmmmmm fzzz  ") == 1);
            regexp_test_check(r.find_any("mmmzzn anmannnnnnnmmmmmm fzzz  ") == 1);
            regexp_test_check(r.find_any("mm anmannnnnnnmmmmmm fzmzznzz  ") == 1);
            regexp_test_check(r.find_any("mm anmannnnnnnmmmmmm fzmzzfnzz ") == 0);
            regexp_test_check(r.find_any("manmfnmannnnnnnmmmmmmmmmffzzz  ") == 1);
        }

        {
            regexp r;
            r.init(".*m((foo|bar)*|baz)m.*");
            regexp_test_check(r.find_any("abfoobarx") == 0);
            regexp_test_check(r.find_any("a mfoofoofoobazm d") == 0);
            regexp_test_check(r.find_any("a mfoobarbazfoom d") == 0);
            regexp_test_check(r.find_any("a mbazm d") == 1);
            regexp_test_check(r.find_any("a mfoobarfoom d") == 1);
            regexp_test_check(r.find_any("a mm foobarfoobarfoobar ") == 1);
        }

        {
            regexp r;
            r.init("[a-fA-F]..[^]a-zA-Z]");
            regexp_test_check(r.find_any("Axx1") == 1);
            regexp_test_check(r.find_any("Fxx1") == 1);
            regexp_test_check(r.find_any("Bxx]") == 0);
            regexp_test_check(r.find_any("Cxxz") == 0);
            regexp_test_check(r.find_any("gxx[") == 0);
            regexp_test_check(r.find_any("-xx0") == 0);
        }

        vogl_check_heap();

        return !g_regexp_test_fail_count;
    }

} // namespace vogl
