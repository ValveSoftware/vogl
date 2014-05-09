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

// File: vogl_strutils.cpp
#include "vogl_core.h"
#include "vogl_strutils.h"

namespace vogl
{
    char *vogl_strlwr(char *p)
    {
        char *q = p;
        while (*q)
        {
            char c = *q;
            *q++ = vogl_tolower(c);
        }
        return p;
    }

    char *vogl_strupr(char *p)
    {
        char *q = p;
        while (*q)
        {
            char c = *q;
            *q++ = vogl_toupper(c);
        }
        return p;
    }

    char *vogl_strdup(const char *pStr)
    {
        if (!pStr)
            pStr = "";

        size_t l = strlen(pStr) + 1;
        char *p = (char *)vogl_malloc(l);
        if (p)
            memcpy(p, pStr, l);

        return p;
    }

    char *strcpy_safe(char *pDst, uint32_t dst_len, const char *pSrc)
    {
        VOGL_ASSERT(pDst && pSrc && dst_len);
        if (!dst_len)
            return pDst;

        char *pCur_dst = pDst;
        char c;

        do
        {
            if (dst_len == 1)
            {
                *pCur_dst++ = '\0';
                break;
            }

            c = *pSrc++;
            *pCur_dst++ = c;

            dst_len--;

        } while (c);

        VOGL_ASSERT((pCur_dst - pDst) <= (int)dst_len);

        return pDst;
    }

    int vogl_sprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...)
    {
        if (!sizeOfBuffer)
            return 0;

        va_list args;
        va_start(args, format);
        int c = vsnprintf(buffer, sizeOfBuffer, format, args);
        va_end(args);

        if (c < 0)
        {
            buffer[0] = '\0';
            return 0;
        }

        // Absolutely guarantee the buffer is null terminated.
        buffer[sizeOfBuffer - 1] = '\0';

        return VOGL_MIN(c, (int)sizeOfBuffer - 1);
    }

    int vogl_vsprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, va_list args)
    {
        if (!sizeOfBuffer)
            return 0;

        int c = vsnprintf(buffer, sizeOfBuffer, format, args);
        if (c < 0)
        {
            buffer[0] = '\0';
            return 0;
        }

        // Absolutely guarantee the buffer is null terminated.
        buffer[sizeOfBuffer - 1] = '\0';

        return VOGL_MIN(c, (int)sizeOfBuffer - 1);
    }

    int vogl_strcasecmp(const char *s1, const char *s2)
    {
        for (;;)
        {
            int c1 = vogl_tolower((unsigned char)*s1++);
            int c2 = vogl_tolower((unsigned char)*s2++);
            if ((c1 == 0) || (c1 != c2))
                return c1 - c2;
        }
    }

    int vogl_strncasecmp(const char *s1, const char *s2, size_t n)
    {
        if (!n)
            return 0;

        while ((n-- != 0) && (vogl_tolower(*s1) == vogl_tolower(*s2)))
        {
            if ((n == 0) || (*s1 == '\0') || (*s2 == '\0'))
                break;
            ++s1;
            ++s2;
        }

        return vogl_tolower(*(unsigned char *)s1) - vogl_tolower(*(unsigned char *)s2);
    }

    // http://leetcode.com/2010/10/implement-strstr-to-find-substring-in.html
    const char *vogl_strstr(const char *str, const char *target)
    {
        if (!*target)
            return str;

        const char *p1 = str;
        while (*p1)
        {
            const char *p1Begin = p1;

            const char *p2 = target;
            while ((*p1) && (*p2) && (*p1 == *p2))
            {
                ++p1;
                ++p2;
            }

            if (!*p2)
                return p1Begin;

            p1 = p1Begin + 1;
        }

        return NULL;
    }

    int vogl_strcmp(const char *s1, const char *s2)
    {
        for (;;)
        {
            int c1 = (unsigned char)*s1++;
            int c2 = (unsigned char)*s2++;
            if ((c1 == 0) || (c1 != c2))
                return c1 - c2;
        }
    }

    // Locale-independent integer<->string conversion.
    bool int_to_string(int value, char *pDst, uint32_t len)
    {
        VOGL_ASSERT(pDst);

        const uint32_t cBufSize = 16;
        char buf[cBufSize];

        // -value could overflow here, but we cast to uint32_t so it doesn't matter
        uint32_t j = static_cast<uint32_t>((value < 0) ? -value : value);

        char *p = buf + cBufSize - 1;

        *p-- = '\0';

        do
        {
            *p-- = static_cast<uint8_t>('0' + (j % 10U));
            j /= 10U;
        } while (j);

        if (value < 0)
            *p-- = '-';

        const size_t total_bytes = (buf + cBufSize - 1) - p;
        if (total_bytes > len)
        {
            if ((pDst) && (len))
                *pDst = '\0';
            return false;
        }

        for (size_t i = 0; i < total_bytes; i++)
            pDst[i] = p[1 + i];

        return true;
    }

    bool uint_to_string(uint32_t value, char *pDst, uint32_t len)
    {
        VOGL_ASSERT(pDst);

        const uint32_t cBufSize = 16;
        char buf[cBufSize];

        char *p = buf + cBufSize - 1;

        *p-- = '\0';

        do
        {
            *p-- = static_cast<uint8_t>('0' + (value % 10U));
            value /= 10U;
        } while (value);

        const size_t total_bytes = (buf + cBufSize - 1) - p;
        if (total_bytes > len)
        {
            if ((pDst) && (len))
                *pDst = '\0';
            return false;
        }

        for (size_t i = 0; i < total_bytes; i++)
            pDst[i] = p[1 + i];

        return true;
    }

    bool uint64_to_string(uint64_t value, char *pDst, uint32_t len)
    {
        VOGL_ASSERT(pDst);

        // really only 20 digits (plus \0 or 21), but whatever
        const uint32_t cBufSize = 32;
        char buf[cBufSize];

        char *p = buf + cBufSize - 1;

        *p-- = '\0';

        do
        {
            *p-- = static_cast<uint8_t>('0' + (value % 10U));
            value /= 10U;
        } while (value);

        const size_t total_bytes = (buf + cBufSize - 1) - p;
        if (total_bytes > len)
        {
            if ((pDst) && (len))
                *pDst = '\0';
            return false;
        }

        for (size_t i = 0; i < total_bytes; i++)
            pDst[i] = p[1 + i];

        return true;
    }

    dynamic_string int_to_string(int64_t value)
    {
        const uint32_t cBufSize = 32;
        char buf[cBufSize];
        int64_to_string(value, buf, cBufSize);
        return dynamic_string(buf);
    }

    dynamic_string uint_to_string(uint64_t value)
    {
        const uint32_t cBufSize = 32;
        char buf[cBufSize];
        uint64_to_string(value, buf, cBufSize);
        return dynamic_string(buf);
    }

    bool uint64_to_string_with_commas(uint64_t value, char *pDst, uint32_t len)
    {
        VOGL_ASSERT(pDst);

        const uint32_t cBufSize = 32;
        char buf[cBufSize];

        char *p = buf + cBufSize - 1;

        *p-- = '\0';
        uint32_t num_chars_until_comma = 3;

        do
        {
            if (!num_chars_until_comma)
            {
                *p-- = ',';
                num_chars_until_comma = 3;
            }

            *p-- = static_cast<uint8_t>('0' + (value % 10U));
            num_chars_until_comma--;

            value /= 10U;
        } while (value);

        const size_t total_bytes = (buf + cBufSize - 1) - p;
        if (total_bytes > len)
            return false;

        for (size_t i = 0; i < total_bytes; i++)
            pDst[i] = p[1 + i];

        return true;
    }

    dynamic_string uint64_to_string_with_commas(uint64_t value)
    {
        const uint32_t cBufSize = 32;
        char buf[cBufSize];
        uint64_to_string_with_commas(value, buf, sizeof(buf));
        return dynamic_string(buf);
    }

    bool int64_to_string(int64_t value, char *pDst, uint32_t len)
    {
        VOGL_ASSERT(pDst);

        const uint32_t cBufSize = 32;
        char buf[cBufSize];

        // -value could overflow here, but we cast to uint64_t so it doesn't matter
        uint64_t j = static_cast<uint64_t>((value < 0) ? -value : value);

        char *p = buf + cBufSize - 1;

        *p-- = '\0';

        do
        {
            *p-- = static_cast<uint8_t>('0' + (j % 10U));
            j /= 10U;
        } while (j);

        if (value < 0)
            *p-- = '-';

        const size_t total_bytes = (buf + cBufSize - 1) - p;
        if (total_bytes > len)
        {
            if ((pDst) && (len))
                *pDst = '\0';
            return false;
        }

        for (size_t i = 0; i < total_bytes; i++)
            pDst[i] = p[1 + i];

        return true;
    }

    bool string_ptr_to_int(const char *&pBuf, int &value)
    {
        value = 0;

        VOGL_ASSERT(pBuf);
        const char *p = pBuf;

        while (*p && vogl_isspace(*p))
            p++;

        uint32_t result = 0;
        bool negative = false;

        if ((p[0] == '0') && (p[1] == 'x'))
        {
            p += 2;

            if (!vogl_isxdigit(*p))
                return false;

            while (*p)
            {
                int v = utils::from_hex(*p);
                if (v < 0)
                    break;

                if (result & 0xF0000000UL)
                    return false;

                result = (result << 4) | static_cast<uint32_t>(v);

                p++;
            }
        }
        else
        {
            if (!vogl_isdigit(*p))
            {
                if (p[0] == '-')
                {
                    negative = true;
                    p++;
                }
                else
                    return false;
            }

            while (*p && vogl_isdigit(*p))
            {
                if (result & 0xE0000000U)
                    return false;

                const uint32_t result8 = result << 3U;
                const uint32_t result2 = result << 1U;

                if (result2 > (0xFFFFFFFFU - result8))
                    return false;

                result = result8 + result2;

                uint32_t c = p[0] - '0';
                if (c > (0xFFFFFFFFU - result))
                    return false;

                result += c;

                p++;
            }
        }

        if (negative)
        {
            if (result > 0x80000000U)
                return false;
            value = -static_cast<int>(result);
        }
        else
        {
            if (result > 0x7FFFFFFFU)
                return false;
            value = static_cast<int>(result);
        }

        pBuf = p;

        return true;
    }

    bool string_ptr_to_int64(const char *&pBuf, int64_t &value)
    {
        value = 0;

        VOGL_ASSERT(pBuf);
        const char *p = pBuf;

        while (*p && vogl_isspace(*p))
            p++;

        uint64_t result = 0;
        bool negative = false;

        if ((p[0] == '0') && (p[1] == 'x'))
        {
            p += 2;

            if (!vogl_isxdigit(*p))
                return false;

            while (*p)
            {
                int v = utils::from_hex(*p);
                if (v < 0)
                    break;

                if (result & 0xF000000000000000ULL)
                    return false;

                result = (result << 4) | static_cast<uint64_t>(v);

                p++;
            }
        }
        else
        {
            if (!vogl_isdigit(*p))
            {
                if (p[0] == '-')
                {
                    negative = true;
                    p++;
                }
                else
                    return false;
            }

            while (*p && vogl_isdigit(*p))
            {
                if (result & 0xE000000000000000ULL)
                    return false;

                const uint64_t result8 = result << 3U;
                const uint64_t result2 = result << 1U;

                if (result2 > (0xFFFFFFFFFFFFFFFFULL - result8))
                    return false;

                result = result8 + result2;

                uint32_t c = p[0] - '0';
                if (c > (0xFFFFFFFFFFFFFFFFULL - result))
                    return false;

                result += c;

                p++;
            }
        }

        if (negative)
        {
            if (result > 0x8000000000000000ULL)
                return false;
            value = -static_cast<int64_t>(result);
        }
        else
        {
            if (result > 0x7FFFFFFFFFFFFFFFULL)
                return false;
            value = static_cast<int64_t>(result);
        }

        pBuf = p;

        return true;
    }

    bool string_ptr_to_uint(const char *&pBuf, uint32_t &value)
    {
        value = 0;

        VOGL_ASSERT(pBuf);
        const char *p = pBuf;

        while (*p && vogl_isspace(*p))
            p++;

        uint32_t result = 0;
        if ((p[0] == '0') && (p[1] == 'x'))
        {
            p += 2;

            if (!vogl_isxdigit(*p))
                return false;

            while (*p)
            {
                int v = utils::from_hex(*p);
                if (v < 0)
                    break;

                if (result & 0xF0000000UL)
                    return false;

                result = (result << 4) | static_cast<uint32_t>(v);

                p++;
            }
        }
        else
        {
            if (!vogl_isdigit(*p))
                return false;

            while (*p && vogl_isdigit(*p))
            {
                if (result & 0xE0000000U)
                    return false;

                const uint32_t result8 = result << 3U;
                const uint32_t result2 = result << 1U;

                if (result2 > (0xFFFFFFFFU - result8))
                    return false;

                result = result8 + result2;

                uint32_t c = p[0] - '0';
                if (c > (0xFFFFFFFFU - result))
                    return false;

                result += c;

                p++;
            }
        }

        value = result;

        pBuf = p;

        return true;
    }

    bool string_ptr_to_uint64(const char *&pBuf, uint64_t &value)
    {
        value = 0;

        VOGL_ASSERT(pBuf);
        const char *p = pBuf;

        while (*p && vogl_isspace(*p))
            p++;

        uint64_t result = 0;

        if ((p[0] == '0') && (p[1] == 'x'))
        {
            p += 2;

            if (!vogl_isxdigit(*p))
                return false;

            while (*p)
            {
                int v = utils::from_hex(*p);
                if (v < 0)
                    break;

                if (result & 0xF000000000000000ULL)
                    return false;

                result = (result << 4) | static_cast<uint64_t>(v);

                p++;
            }
        }
        else
        {
            if (!vogl_isdigit(*p))
                return false;

            while (*p && vogl_isdigit(*p))
            {
                if (result & 0xE000000000000000ULL)
                    return false;

                const uint64_t result8 = result << 3U;
                const uint64_t result2 = result << 1U;

                if (result2 > (0xFFFFFFFFFFFFFFFFULL - result8))
                    return false;

                result = result8 + result2;

                uint32_t c = p[0] - '0';
                if (c > (0xFFFFFFFFFFFFFFFFULL - result))
                    return false;

                result += c;

                p++;
            }
        }

        value = result;

        pBuf = p;

        return true;
    }

    bool string_ptr_to_bool(const char *&p, bool &value)
    {
        VOGL_ASSERT(p);

        value = false;

        if (vogl_stricmp(p, "false") == 0)
        {
            p += 5;
            return true;
        }

        if (vogl_stricmp(p, "true") == 0)
        {
            p += 4;
            value = true;
            return true;
        }

        uint32_t v;
        if (string_ptr_to_uint(p, v))
        {
            if (!v)
                return true;
            else if (v == 1)
            {
                value = true;
                return true;
            }
        }

        return false;
    }

    bool string_ptr_to_float(const char *&p, float &value, uint32_t round_digit)
    {
        double d;
        if (!string_ptr_to_double(p, d, round_digit))
        {
            value = 0;
            return false;
        }
        value = static_cast<float>(d);
        return true;
    }

    bool string_ptr_to_double(const char *&p, double &value, uint32_t round_digit)
    {
        return string_ptr_to_double(p, p + 128, value, round_digit);
    }

    // I wrote this approx. 20 years ago in C/assembly using a limited FP emulator package, so it's a bit crude.
    bool string_ptr_to_double(const char *&p, const char *pEnd, double &value, uint32_t round_digit)
    {
        VOGL_ASSERT(p);

        value = 0;

        enum
        {
            AF_BLANK = 1,
            AF_SIGN = 2,
            AF_DPOINT = 3,
            AF_BADCHAR = 4,
            AF_OVRFLOW = 5,
            AF_EXPONENT = 6,
            AF_NODIGITS = 7
        };
        int status = 0;

        const char *buf = p;

        int got_sign_flag = 0, got_dp_flag = 0, got_num_flag = 0;
        int got_e_flag = 0, got_e_sign_flag = 0, e_sign = 0;
        uint32_t whole_count = 0, frac_count = 0;

        double whole = 0, frac = 0, scale = 1, exponent = 1;

        if (p >= pEnd)
        {
            status = AF_NODIGITS;
            goto af_exit;
        }

        while (*buf)
        {
            if (!vogl_isspace(*buf))
                break;
            if (++buf >= pEnd)
            {
                status = AF_NODIGITS;
                goto af_exit;
            }
        }

        p = buf;

        while (*buf)
        {
            p = buf;
            if (buf >= pEnd)
                break;

            int i = *buf++;

            switch (i)
            {
                case 'e':
                case 'E':
                {
                    got_e_flag = 1;
                    goto exit_while;
                }
                case '+':
                {
                    if ((got_num_flag) || (got_sign_flag))
                    {
                        status = AF_SIGN;
                        goto af_exit;
                    }

                    got_sign_flag = 1;

                    break;
                }
                case '-':
                {
                    if ((got_num_flag) || (got_sign_flag))
                    {
                        status = AF_SIGN;
                        goto af_exit;
                    }

                    got_sign_flag = -1;

                    break;
                }
                case '.':
                {
                    if (got_dp_flag)
                    {
                        status = AF_DPOINT;
                        goto af_exit;
                    }

                    got_dp_flag = 1;

                    break;
                }
                default:
                {
                    if ((i < '0') || (i > '9'))
                        goto exit_while;
                    else
                    {
                        i -= '0';

                        got_num_flag = 1;

                        if (got_dp_flag)
                        {
                            if (frac_count < round_digit)
                            {
                                frac = frac * 10.0f + i;

                                scale = scale * 10.0f;
                            }
                            else if (frac_count == round_digit)
                            {
                                if (i >= 5) /* check for round */
                                    frac = frac + 1.0f;
                            }

                            frac_count++;
                        }
                        else
                        {
                            whole = whole * 10.0f + i;

                            whole_count++;

                            if (whole > 1e+100)
                            {
                                status = AF_OVRFLOW;
                                goto af_exit;
                            }
                        }
                    }

                    break;
                }
            }
        }

    exit_while:

        if (got_e_flag)
        {
            if ((got_num_flag == 0) && (got_dp_flag))
            {
                status = AF_EXPONENT;
                goto af_exit;
            }

            int e = 0;
            e_sign = 1;
            got_num_flag = 0;
            got_e_sign_flag = 0;

            while (*buf)
            {
                p = buf;
                if (buf >= pEnd)
                    break;

                int i = *buf++;

                if (i == '+')
                {
                    if ((got_num_flag) || (got_e_sign_flag))
                    {
                        status = AF_EXPONENT;
                        goto af_exit;
                    }

                    e_sign = 1;
                    got_e_sign_flag = 1;
                }
                else if (i == '-')
                {
                    if ((got_num_flag) || (got_e_sign_flag))
                    {
                        status = AF_EXPONENT;
                        goto af_exit;
                    }

                    e_sign = -1;
                    got_e_sign_flag = 1;
                }
                else if ((i >= '0') && (i <= '9'))
                {
                    got_num_flag = 1;

                    if ((e = (e * 10) + (i - 48)) > 100)
                    {
                        status = AF_EXPONENT;
                        goto af_exit;
                    }
                }
                else
                    break;
            }

            for (int i = 1; i <= e; i++) /* compute 10^e */
                exponent = exponent * 10.0f;
        }

        if (((whole_count + frac_count) == 0) && (got_e_flag == 0))
        {
            status = AF_NODIGITS;
            goto af_exit;
        }

        if (frac)
            whole = whole + (frac / scale);

        if (got_e_flag)
        {
            if (e_sign > 0)
                whole = whole * exponent;
            else
                whole = whole / exponent;
        }

        if (got_sign_flag < 0)
            whole = -whole;

        value = whole;

    af_exit:
        return (status == 0);
    }

    dynamic_string to_hex_string(int value)
    {
        return dynamic_string(cVarArg, "0x%X", value);
    }

    dynamic_string to_hex_string(uint32_t value)
    {
        return dynamic_string(cVarArg, "0x%X", value);
    }

    dynamic_string to_hex_string(int64_t value)
    {
        return dynamic_string(cVarArg, "0x%" PRIX64, value);
    }

    dynamic_string to_hex_string(uint64_t value)
    {
        return dynamic_string(cVarArg, "0x%" PRIX64, value);
    }

    bool strutils_test()
    {
#define CHECK(x)                  \
    do                            \
    {                             \
        if (!(x))                 \
        {                         \
            VOGL_ASSERT_ALWAYS; \
            return false;         \
        }                         \
    } while (0)
        for (uint32_t i = 0; i < 256; i++)
        {
            CHECK((isspace(i) != 0) == vogl_isspace(i));
            CHECK((isdigit(i) != 0) == vogl_isdigit(i));
            CHECK((isxdigit(i) != 0) == vogl_isxdigit(i));
            CHECK(toupper(i) == vogl_toupper(i));
            CHECK(tolower(i) == vogl_tolower(i));
            CHECK((isupper(i) != 0) == vogl_isupper(i));
            CHECK((islower(i) != 0) == vogl_islower(i));
            CHECK((isalpha(i) != 0) == vogl_isalpha(i));
        }

        vogl::random r;
        for (uint32_t t = 0; t < 0x1FFFFFF; t++)
        {
            int64_t i = r.urand64();
            if (r.irand(0, 100) == 0)
                i = static_cast<int>(r.urand32());

            if (t == 0)
                i = cINT64_MIN;
            else if (t == 1)
                i = cINT32_MIN;
            else if (t == 2)
                i = cINT32_MAX;
            else if (t == 3)
                i = cINT64_MAX;
            else if (t == 4)
                i = cUINT64_MIN;
            else if (t == 5)
                i = cUINT64_MAX;
            else if (t == 6)
                i = cUINT32_MIN;
            else if (t == 7)
                i = cUINT32_MAX;
            else if (t == 8)
                i = -1;
            else if (t == 9)
                i = 0;
            else if (t == 10)
                i = 1;

            char buf[256];

            vogl::int64_to_string(i, buf, sizeof(buf));
            int64_t v64;
            const char *pBuf = buf;
            CHECK(vogl::string_ptr_to_int64(pBuf, v64));
            CHECK(v64 == i);

            int v32;
            pBuf = buf;
            bool success = vogl::string_ptr_to_int(pBuf, v32);
            if (!success)
            {
                CHECK((i < cINT32_MIN) || (i > cINT32_MAX));
            }
            else
            {
                CHECK(v32 == i);
            }

            vogl::uint64_to_string(static_cast<uint64_t>(i), buf, sizeof(buf));
            uint64_t uv64;
            pBuf = buf;
            CHECK(vogl::string_ptr_to_uint64(pBuf, uv64));
            CHECK(uv64 == static_cast<uint64_t>(i));

            if ((t & 32767) == 32767)
                printf("0x%08X\n", t);
        }

        return true;
#undef CHECK
    }

} // namespace vogl
