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

// File: vogl_json.h
#include "vogl_core.h"
#include "vogl_json.h"
#include "vogl_console.h"
#include "vogl_growable_array.h"
#include "vogl_hash_map.h"
#include "vogl_map.h"

namespace vogl
{

    json_node_object_pool *g_pJSON_node_pool;

    static const char *g_to_hex = "0123456789ABCDEF";

    const uint32_t cMaxValidDepth = 511;

    const uint32_t cJSONTabSize = 3;

    void json_node_pool_init()
    {
        if (!g_pJSON_node_pool)
            g_pJSON_node_pool = vogl_new(json_node_object_pool, 256, cObjectPoolGrowExponential);
    }

    // class json_deserialize_buf_ptr

    class json_deserialize_buf_ptr
    {
    public:
        inline json_deserialize_buf_ptr(const char *p, size_t len)
        {
            init(p, len);
        }

        inline void init(const char *p, size_t len)
        {
            m_pPtr = p;
            m_pEnd = p + len;
            m_cur_line = 1;
        }

        inline const char *get_end() const
        {
            return m_pEnd;
        }
        inline bool at_end() const
        {
            return m_pPtr >= m_pEnd;
        }

        inline size_t get_num_remaining() const
        {
            return m_pEnd - m_pPtr;
        }

        inline char get_char() const
        {
            char c = (m_pPtr < m_pEnd) ? *m_pPtr : 0;
            if (c == '\r')
                c = '\n';
            return c;
        }

        inline uint32_t get_cur_line() const
        {
            return m_cur_line;
        }

        inline char operator*() const
        {
            return get_char();
        }
        inline operator const char *() const
        {
            return m_pPtr;
        }

        inline json_deserialize_buf_ptr &operator++()
        {
            get_and_advance();
            return *this;
        }

        inline json_deserialize_buf_ptr operator++(int)
        {
            json_deserialize_buf_ptr cur(*this);
            get_and_advance();
            return cur;
        }

        inline char get_and_advance()
        {
            char c = (m_pPtr < m_pEnd) ? *m_pPtr++ : 0;
            if (c <= '\r')
            {
                if ((c == '\r') && (m_pPtr < m_pEnd) && (*m_pPtr == '\n'))
                    c = *m_pPtr++;
                if ((c == '\n') || (c == '\r'))
                {
                    c = '\n';
                    m_cur_line++;
                }
            }
            return c;
        }

        inline void advance()
        {
            get_and_advance();
        }

        json_deserialize_buf_ptr &skip_whitespace();

        inline bool compare_string(const char *pStr, size_t n) const
        {
            return ((m_pEnd - m_pPtr) < (int)n) ? false : !strncmp(m_pPtr, pStr, n);
        }

        inline json_deserialize_buf_ptr &advance_in_line(uint32_t n)
        {
            m_pPtr = VOGL_MIN(m_pEnd, m_pPtr + n);
            return *this;
        }
        inline json_deserialize_buf_ptr &advance_in_line_no_end_check(uint32_t n)
        {
            m_pPtr += n;
            VOGL_ASSERT(m_pPtr <= m_pEnd);
            return *this;
        }

    private:
        const char *m_pPtr;
        const char *m_pEnd;
        uint32_t m_cur_line;
    };

    json_deserialize_buf_ptr &json_deserialize_buf_ptr::skip_whitespace()
    {
        while (m_pPtr < m_pEnd)
        {
            char c = *m_pPtr;
            if ((c == ' ') || (c == '\t'))
            {
                ++m_pPtr;
                continue;
            }

            if ((c == '/') && (((m_pPtr + 1) < m_pEnd) && (m_pPtr[1] == '/')))
            {
                m_pPtr += 2;
                while ((!at_end()) && (get_char() != '\n'))
                    advance();
                continue;
            }
            else if (c > ' ')
                break;
            else if (c == '\r')
            {
                if (((m_pPtr + 1) < m_pEnd) && (m_pPtr[1] == '\n'))
                    ++m_pPtr;
                m_cur_line++;
            }
            else if (c == '\n')
                m_cur_line++;

            ++m_pPtr;
        }
        return *this;
    }

    // class json_growable_char_buf

    class json_growable_char_buf
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(json_growable_char_buf);

    public:
        inline json_growable_char_buf(vogl::vector<char> &buf)
            : m_buf(buf)
        {
        }

        inline uint32_t size() const
        {
            return m_buf.size();
        }

        inline const vogl::vector<char> &get() const
        {
            return m_buf;
        }
        inline vogl::vector<char> &get()
        {
            return m_buf;
        }

        void puts(const char *pStr);
        void printf(const char *pFmt, ...);
        void print_escaped(const char *pStr);
        void print_tabs(uint32_t n);
        inline void print_char(char c)
        {
            m_buf.push_back(c);
        }

    private:
        vogl::vector<char> &m_buf;
    };

    void json_growable_char_buf::puts(const char *pStr)
    {
        size_t l = strlen(pStr);
        if (l > cUINT32_MAX)
            return;

        char *p = m_buf.try_enlarge(static_cast<uint32_t>(l));
        if (p)
            memcpy(p, pStr, l);
    }

    void json_growable_char_buf::printf(const char *pFmt, ...)
    {
        char str[512];

        va_list args;
        va_start(args, pFmt);
        int l = vogl_vsprintf_s(str, sizeof(str), pFmt, args);
        va_end(args);

        char *p = m_buf.try_enlarge(l);
        if (p)
            memcpy(p, str, l);
    }

    void json_growable_char_buf::print_tabs(uint32_t n)
    {
#if 0
	// Apparently tabs are not valid JSON.
	char *p = m_buf.enlarge(n);
	if (p)
		memset(p, '\t', n);
#else
        char *p = m_buf.enlarge(n * cJSONTabSize);
        if (p)
            memset(p, ' ', n * cJSONTabSize);
#endif
    }

    void json_growable_char_buf::print_escaped(const char *pStr)
    {
        m_buf.push_back('\"');

        while (*pStr)
        {
            char c = *pStr++;
            if ((static_cast<uint8_t>(c) >= 32U) && (c != '\"') && (c != '\\'))
                m_buf.push_back(c);
            else
            {
                m_buf.push_back('\\');
                switch (c)
                {
                    case '\b':
                        m_buf.push_back('b');
                        break;
                    case '\r':
                        m_buf.push_back('r');
                        break;
                    case '\t':
                        m_buf.push_back('t');
                        break;
                    case '\f':
                        m_buf.push_back('f');
                        break;
                    case '\n':
                        m_buf.push_back('n');
                        break;
                    case '\\':
                        m_buf.push_back('\\');
                        break;
                    case '\"':
                        m_buf.push_back('\"');
                        break;
                    default:
                        m_buf.push_back('u');
                        m_buf.push_back('0');
                        m_buf.push_back('0');
                        m_buf.push_back(g_to_hex[static_cast<uint8_t>(c) >> 4]);
                        m_buf.push_back(g_to_hex[c & 0xF]);
                        break;
                }
            }
        }

        m_buf.push_back('\"');
    }

    void json_error_info_t::set_error(uint32_t line, const char *pMsg, ...)
    {
        m_error_line = line;

        va_list args;
        va_start(args, pMsg);
        m_error_msg.format_args(pMsg, args);
        va_end(args);
    }

    // class json_value

    json_value::json_value(const json_value &other)
        : m_type(other.m_type),
          m_line(0)
    {
        if (other.m_type == cJSONValueTypeString)
            m_data.m_pStr = vogl_strdup(other.m_data.m_pStr);
        else if (other.m_type == cJSONValueTypeNode)
            m_data.m_pNode = get_json_node_pool()->alloc(*other.get_node_ptr());
        else
            memcpy(&m_data, &other.m_data, sizeof(m_data));
    }

    void json_value::set_value(uint64_t nVal)
    {
        free_data();

        if (nVal > static_cast<uint64_t>(cINT64_MAX))
        {
            // Encode as a hex string because otherwise some json parsers have a fit, and the UJB format doesn't directly support uint64_t.
            char buf[32];
            vogl_sprintf_s(buf, sizeof(buf), "0x%" PRIX64, nVal);
            set_value(buf);
        }
        else
        {
            set_value(static_cast<int64_t>(nVal));
        }
    }

    json_value &json_value::operator=(const json_value &rhs)
    {
        free_data();

        m_type = rhs.m_type;

        if (m_type == cJSONValueTypeString)
            m_data.m_pStr = vogl_strdup(rhs.m_data.m_pStr);
        else if (m_type == cJSONValueTypeNode)
            m_data.m_pNode = get_json_node_pool()->alloc(*rhs.get_node_ptr());
        else
            memcpy(&m_data, &rhs.m_data, sizeof(m_data));

        m_line = rhs.m_line;

        return *this;
    }

    bool json_value::convert_to_bool(bool &val, bool def) const
    {
        switch (m_type)
        {
            case cJSONValueTypeBool:
            case cJSONValueTypeInt:
            {
                val = (m_data.m_nVal != 0);
                return true;
            }
            case cJSONValueTypeDouble:
            {
                val = (m_data.m_flVal != 0);
                return true;
            }
            case cJSONValueTypeString:
            {
                if (!vogl_stricmp(m_data.m_pStr, "false"))
                {
                    val = false;
                    return true;
                }
                else if (!vogl_stricmp(m_data.m_pStr, "true"))
                {
                    val = true;
                    return true;
                }

                double flVal = 0.0f;
                const char *pStr = m_data.m_pStr;
                if (!string_ptr_to_double(pStr, flVal))
                {
                    val = def;
                    return false;
                }
                val = (flVal != 0);
                return true;
            }
            default:
                break;
        }
        val = def;
        return false;
    }

    bool json_value::convert_to_int32(int32_t &val, int32_t def) const
    {
        val = def;
        int64_t val64;
        if (!convert_to_int64(val64, def))
            return false;
        if ((val64 < cINT32_MIN) || (val64 > cINT32_MAX))
            return false;
        val = static_cast<int32_t>(val64);
        return true;
    }

    bool json_value::convert_to_int64(int64_t &val, int64_t def) const
    {
        switch (m_type)
        {
            case cJSONValueTypeBool:
            case cJSONValueTypeInt:
            {
                val = m_data.m_nVal;
                return true;
            }
            case cJSONValueTypeDouble:
            {
                val = static_cast<int64_t>(m_data.m_flVal);
                return true;
            }
            case cJSONValueTypeString:
            {
                if (!vogl_stricmp(m_data.m_pStr, "false"))
                {
                    val = 0;
                    return true;
                }
                else if (!vogl_stricmp(m_data.m_pStr, "true"))
                {
                    val = 1;
                    return true;
                }

                const char *pStr = m_data.m_pStr;

                // string_to_ptr_int64() will handle hex or decimal conversions
                if (!string_ptr_to_int64(pStr, val))
                {
                    val = def;
                    return false;
                }
                return true;
            }
            default:
                break;
        }
        val = def;
        return false;
    }

    bool json_value::convert_to_uint64(uint64_t &val, uint64_t def) const
    {
        switch (m_type)
        {
            case cJSONValueTypeBool:
            {
                val = m_data.m_nVal;
                break;
            }
            case cJSONValueTypeInt:
            {
                if (m_data.m_nVal < 0)
                    break;

                val = m_data.m_nVal;
                return true;
            }
            case cJSONValueTypeDouble:
            {
                if (m_data.m_flVal < 0)
                    break;

                val = static_cast<uint64_t>(m_data.m_flVal);
                return true;
            }
            case cJSONValueTypeString:
            {
                if (!vogl_stricmp(m_data.m_pStr, "false"))
                {
                    val = 0;
                    return true;
                }
                else if (!vogl_stricmp(m_data.m_pStr, "true"))
                {
                    val = 1;
                    return true;
                }

                const char *pStr = m_data.m_pStr;

                // string_to_uint64() will handle hex or decimal conversions
                if (!string_ptr_to_uint64(pStr, val))
                    break;

                return true;
            }
            default:
                break;
        }
        val = def;
        return false;
    }

    bool json_value::convert_to_float(float &val, float def) const
    {
        switch (m_type)
        {
            case cJSONValueTypeBool:
            case cJSONValueTypeInt:
            {
                val = static_cast<float>(m_data.m_nVal);
                return true;
            }
            case cJSONValueTypeDouble:
            {
                val = static_cast<float>(m_data.m_flVal);
                return true;
            }
            case cJSONValueTypeString:
            {
                if (!vogl_stricmp(m_data.m_pStr, "false"))
                {
                    val = 0;
                    return true;
                }
                else if (!vogl_stricmp(m_data.m_pStr, "true"))
                {
                    val = 1;
                    return true;
                }

                const char *pStr = m_data.m_pStr;
                if (!string_ptr_to_float(pStr, val))
                    break;

                return true;
            }
            default:
                break;
        }
        val = def;
        return false;
    }

    bool json_value::convert_to_double(double &val, double def) const
    {
        switch (m_type)
        {
            case cJSONValueTypeBool:
            case cJSONValueTypeInt:
            {
                val = static_cast<double>(m_data.m_nVal);
                return true;
            }
            case cJSONValueTypeDouble:
            {
                val = m_data.m_flVal;
                return true;
            }
            case cJSONValueTypeString:
            {
                if (!vogl_stricmp(m_data.m_pStr, "false"))
                {
                    val = 0;
                    return true;
                }
                else if (!vogl_stricmp(m_data.m_pStr, "true"))
                {
                    val = 1;
                    return true;
                }

                const char *pStr = m_data.m_pStr;
                if (!string_ptr_to_double(pStr, val))
                {
                    val = def;
                    return false;
                }
                return true;
            }
            default:
                break;
        }
        val = def;
        return false;
    }

    bool json_value::convert_to_string(dynamic_string &val, const char *pDef) const
    {
        switch (m_type)
        {
            case cJSONValueTypeNull:
            {
                val.set("null");
                return true;
            }
            case cJSONValueTypeBool:
            {
                val.set(!m_data.m_nVal ? "false" : "true");
                return true;
            }
            case cJSONValueTypeInt:
            {
                val.format("%" PRIi64, m_data.m_nVal);
                return true;
            }
            case cJSONValueTypeDouble:
            {
                val.format("%1.18f", m_data.m_flVal);

                if ((!val.contains('E')) && (!val.contains('e')))
                {
                    // Remove trailing 0's, except the first after the decimal point which is required for valid JSON.
                    int dot_ofs = val.find_right('.');
                    if (dot_ofs >= 0)
                    {
                        int len = val.get_len();

                        int i;
                        for (i = dot_ofs + 1; i < len; i++)
                        {
                            char c = val[i];
                            if (!vogl_isdigit(c))
                                break;
                        }
                        if (i == len)
                        {
                            int cur_ofs = len - 1;
                            while ((val[cur_ofs] == '0') && (cur_ofs > (dot_ofs + 1)))
                            {
                                val.set_len(cur_ofs);
                                cur_ofs--;
                            }
                        }
                    }
                }
                return true;
            }
            case cJSONValueTypeString:
            {
                val = m_data.m_pStr;
                return true;
            }
            default:
                break;
        }
        val.set(pDef);
        return false;
    }

    static int scan_string_list(const char *p, const char **pList, int def)
    {
        int index = 0;
        for (;;)
        {
            const char *s = pList[index];
            if (!s)
                break;
            if (vogl_stricmp(p, s) == 0)
                return index;
            index++;
        }
        return def;
    }

    bool json_value::get_enum(const char **pStringList, int &val, int def) const
    {
        val = def;

        dynamic_string tmp_str;
        const char *pStr;

        if (get_type() == cJSONValueTypeString)
            pStr = m_data.m_pStr;
        else
        {
            if (!convert_to_string(tmp_str, ""))
                return false;
            pStr = tmp_str.get_ptr();
        }

        int val_index = scan_string_list(pStr, pStringList, -1);
        if (val_index < 0)
            return false;
        val = val_index;
        return true;
    }

    bool json_value::get_numeric(int8_t &val, int8_t def) const
    {
        int64_t val64;
        if (!get_numeric(val64))
        {
            val = def;
            return false;
        }
        if ((val64 < static_cast<int64_t>(cINT8_MIN)) || (val64 > static_cast<int64_t>(cINT8_MAX)))
        {
            val = def;
            return false;
        }
        val = static_cast<int8_t>(val64);
        return true;
    }

    bool json_value::get_numeric(int16_t &val, int16_t def) const
    {
        int64_t val64;
        if (!get_numeric(val64))
        {
            val = def;
            return false;
        }
        if ((val64 < static_cast<int64_t>(cINT16_MIN)) || (val64 > static_cast<int64_t>(cINT16_MAX)))
        {
            val = def;
            return false;
        }
        val = static_cast<int16_t>(val64);
        return true;
    }

    bool json_value::get_numeric(uint8_t &val, uint8_t def) const
    {
        uint64_t val64;
        if (!get_numeric(val64))
        {
            val = def;
            return false;
        }
        if (val64 > static_cast<int64_t>(cUINT8_MAX))
        {
            val = def;
            return false;
        }
        val = static_cast<uint8_t>(val64);
        return true;
    }

    bool json_value::get_numeric(uint16_t &val, uint16_t def) const
    {
        uint64_t val64;
        if (!get_numeric(val64))
        {
            val = def;
            return false;
        }
        if (val64 > static_cast<int64_t>(cUINT16_MAX))
        {
            val = def;
            return false;
        }
        val = static_cast<uint16_t>(val64);
        return true;
    }

    bool json_value::get_numeric(uint32_t &val, uint32_t def) const
    {
        int64_t val64;
        if (!get_numeric(val64))
        {
            val = def;
            return false;
        }
        if ((val64 < 0) || (val64 > static_cast<int64_t>(cUINT32_MAX)))
        {
            val = def;
            return false;
        }
        val = static_cast<uint32_t>(val64);
        return true;
    }

    bool json_value::validate(const json_node *pParent) const
    {
        if (is_node())
        {
            const json_node *pNode = get_node_ptr();
            if (!pNode)
                return false;

            if (!pNode->basic_validation(pParent))
                return false;

            for (uint32_t i = 0; i < pNode->size(); i++)
                if ((pNode->is_child(i)) && (!pNode->get_value(i).validate(pNode)))
                    return false;
        }
        else if (is_string())
        {
            if (!m_data.m_pStr)
                return false;
        }

        return true;
    }

    void json_value::binary_serialize_value(vogl::vector<uint8_t> &buf) const
    {
        switch (m_type)
        {
            case cJSONValueTypeNull:
            {
                buf.push_back('Z');
                break;
            }
            case cJSONValueTypeBool:
            {
                buf.push_back(m_data.m_nVal ? 'T' : 'F');
                break;
            }
            case cJSONValueTypeInt:
            {
                int shift;
                uint8_t *pDst;
                if (static_cast<uint64_t>(m_data.m_nVal) <= 0xFFU)
                {
                    pDst = buf.enlarge(2);
                    *pDst++ = 'B';
                    shift = 8 - 8;
                }
                else if ((m_data.m_nVal >= cINT16_MIN) && (m_data.m_nVal <= cINT16_MAX))
                {
                    pDst = buf.enlarge(3);
                    *pDst++ = 'i';
                    shift = 16 - 8;
                }
                else if ((m_data.m_nVal >= cINT32_MIN) && (m_data.m_nVal <= cINT32_MAX))
                {
                    pDst = buf.enlarge(5);
                    *pDst++ = 'I';
                    shift = 32 - 8;
                }
                else
                {
                    pDst = buf.enlarge(9);
                    *pDst++ = 'L';
                    shift = 64 - 8;
                }

                uint64_t n = static_cast<uint64_t>(m_data.m_nVal);
                do
                {
                    *pDst++ = static_cast<uint8_t>(n >> shift);
                } while ((shift -= 8) >= 0);

                break;
            }
            case cJSONValueTypeDouble:
            {
                float flVal32 = static_cast<float>(m_data.m_flVal);
                if (math::equal_tol(m_data.m_flVal, static_cast<double>(flVal32), 1e-17))
                {
                    uint8_t *pDst = buf.enlarge(5);
                    pDst[0] = 'd';
                    utils::write_obj(flVal32, pDst + 1, false);
                }
                else
                {
                    uint8_t *pDst = buf.enlarge(9);
                    pDst[0] = 'D';
                    utils::write_obj(m_data.m_flVal, pDst + 1, false);
                }

                break;
            }
            case cJSONValueTypeString:
            {
                uint32_t len = static_cast<uint32_t>(strlen(m_data.m_pStr));
                if (len <= 254)
                {
                    uint8_t *pDst = buf.enlarge(2 + len);
                    pDst[0] = 's';
                    pDst[1] = static_cast<uint8_t>(len);
                    memcpy(pDst + 2, m_data.m_pStr, len);
                }
                else
                {
                    uint8_t *pDst = buf.enlarge(5 + len);
                    pDst[0] = 'S';
                    pDst[1] = static_cast<uint8_t>(len >> 24);
                    pDst[2] = static_cast<uint8_t>(len >> 16);
                    pDst[3] = static_cast<uint8_t>(len >> 8);
                    pDst[4] = static_cast<uint8_t>(len);
                    memcpy(pDst + 5, m_data.m_pStr, len);
                }

                break;
            }
            default:
                break;
        }
    }

    void json_value::binary_serialize(vogl::vector<uint8_t> &buf) const
    {
        const json_node *pNode = get_node_ptr();
        if (pNode)
            pNode->binary_serialize(buf);
        else
            binary_serialize_value(buf);
    }

    bool json_value::binary_serialize_to_file(const char *pFilename)
    {
        FILE *pFile = vogl_fopen(pFilename, "wb");
        if (!pFile)
            return false;
        bool success = binary_serialize(pFile);
        if (vogl_fclose(pFile) == EOF)
            success = false;
        return success;
    }

    bool json_value::binary_serialize(FILE *pFile)
    {
        vogl::vector<uint8_t> buf;
        binary_serialize(buf);
        return fwrite(buf.get_ptr(), 1, buf.size(), pFile) == buf.size();
    }

    void json_value::serialize(vogl::vector<char> &buf, bool formatted, uint32_t cur_indent, bool null_terminate, uint32_t max_line_len) const
    {
        const json_node *pNode = get_node_ptr();
        if (pNode)
        {
            pNode->serialize(buf, formatted, cur_indent, false, max_line_len);
            if (formatted)
                buf.push_back('\n');
        }
        else
        {
            if (formatted)
            {
                while (cur_indent--)
                {
#if 0
                    // Apparently tabs are not valid JSON.
                    buf.push_back('\t');
#else
                    for (uint32_t i = 0; i < cJSONTabSize; i++)
                        buf.push_back(' ');
#endif
                }
            }
            if (is_string())
            {
                json_growable_char_buf growable_buf(buf);
                growable_buf.print_escaped(m_data.m_pStr);
            }
            else
            {
                dynamic_string str;
                get_string(str);
                buf.append(str.get_ptr(), str.get_len());
            }
        }

        if (null_terminate)
            buf.push_back('\0');
    }

    bool json_value::binary_deserialize_file(const char *pFilename)
    {
        FILE *pFile = vogl_fopen(pFilename, "rb");
        if (!pFile)
            return false;
        bool success = binary_deserialize(pFile);
        if (vogl_fclose(pFile) == EOF)
            success = false;
        return success;
    }

    bool json_value::binary_deserialize(FILE *pFile)
    {
        vogl_fseek(pFile, 0, SEEK_END);
        const uint64_t filesize = vogl_ftell(pFile);
        vogl_fseek(pFile, 0, SEEK_SET);

        if (!filesize)
            return false;

        if ((filesize > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE) || (filesize != static_cast<size_t>(filesize)))
            return false;

        uint8_t *pBuf = (uint8_t *)vogl_malloc(static_cast<size_t>(filesize));
        if (!pBuf)
            return false;

        if (vogl_fread(pBuf, 1, static_cast<size_t>(filesize), pFile) != filesize)
        {
            vogl_free(pBuf);
            return false;
        }

        bool status = binary_deserialize(pBuf, static_cast<size_t>(filesize));

        vogl_free(pBuf);

        return status;
    }

    bool json_value::binary_deserialize(const vogl::vector<uint8_t> &buf)
    {
        return buf.size() ? binary_deserialize(buf.get_ptr(), buf.size()) : false;
    }

    bool json_value::binary_deserialize(const uint8_t *pBuf, size_t n)
    {
        return binary_deserialize(pBuf, pBuf + n, NULL, 0);
    }

    bool json_value::binary_deserialize(const uint8_t *&pBuf, const uint8_t *pBuf_end, json_node *pParent, uint32_t depth)
    {
#define JSON_BD_FAIL() \
    do                 \
    {                  \
        pBuf = pSrc;   \
        return false;  \
    } while (0)
        const uint8_t *pSrc = pBuf;
        set_value_to_null();

        if (depth > cMaxValidDepth)
            return false;

        uint8_t c;
        for (;;)
        {
            if (pSrc >= pBuf_end)
                JSON_BD_FAIL();
            c = *pSrc++;
            if (c != 'N')
                break;
        }

        size_t n = pBuf_end - pSrc;

        // Process huge nums as strings
        if (c == 'h')
            c = 's';
        else if (c == 'H')
            c = 'S';

        switch (c)
        {
            case 'E':
                JSON_BD_FAIL();
            case 'Z':
                break;
            case 'A':
            case 'a':
            case 'O':
            case 'o':
            {
                const bool is_object = ((c == 'O') || (c == 'o'));

                if (n < 1)
                    JSON_BD_FAIL();

                uint32_t l = *pSrc++;
                n--;

                bool unknown_len = false;

                if (c <= 'O')
                {
                    if (n < 3)
                        JSON_BD_FAIL();
                    l = (l << 24) | (pSrc[0] << 16) | (pSrc[1] << 8) | pSrc[2];

                    pSrc += 3;
                    n -= 3;
                }
                else if (l == 255)
                    unknown_len = true;

                json_node *pNode = set_value_to_node(is_object, pParent);
                if (!unknown_len)
                    pNode->resize(l);

                uint32_t idx = 0;
                while ((unknown_len) || (idx < l))
                {
                    uint8_t q;
                    for (;;)
                    {
                        if (n < 1)
                            JSON_BD_FAIL();
                        q = *pSrc;
                        if (q != 'N')
                            break;
                        pSrc++;
                        n--;
                    }

                    if (q == 'E')
                    {
                        pSrc++;
                        break;
                    }
                    if (unknown_len)
                        pNode->enlarge(1);

                    if (is_object)
                    {
                        if ((q != 'S') && (q != 's'))
                            JSON_BD_FAIL();

                        pSrc++;
                        n--;

                        if (!n)
                            JSON_BD_FAIL();

                        uint32_t l2 = *pSrc++;
                        n--;

                        if (c == 'S')
                        {
                            if (n < 3)
                                JSON_BD_FAIL();
                            l2 = (l2 << 24) | (pSrc[0] << 16) | (pSrc[1] << 8) | pSrc[2];
                            pSrc += 3;
                            n -= 3;
                        }

                        if (n < l2)
                            JSON_BD_FAIL();

                        pNode->get_key(idx).set_from_buf(pSrc, l2);

                        pSrc += l2;
                    }

                    if (!pNode->get_value(idx).binary_deserialize(pSrc, pBuf_end, pNode, depth + 1))
                        JSON_BD_FAIL();

                    ++idx;
                }

                break;
            }
            case 's':
            case 'S':
            {
                if (!n)
                    JSON_BD_FAIL();

                uint32_t l = *pSrc++;
                n--;

                if (c == 'S')
                {
                    if (n < 3)
                        JSON_BD_FAIL();
                    l = (l << 24) | (pSrc[0] << 16) | (pSrc[1] << 8) | pSrc[2];
                    pSrc += 3;
                    n -= 3;
                }

                if (n < l)
                    JSON_BD_FAIL();

                m_type = cJSONValueTypeString;
                m_data.m_pStr = (char *)vogl_malloc(l + 1);
                if (!m_data.m_pStr)
                {
                    set_value_to_null();
                    JSON_BD_FAIL();
                }
                memcpy(m_data.m_pStr, pSrc, l);
                m_data.m_pStr[l] = '\0';
                pSrc += l;
                break;
            }
            case 'T':
            {
                set_value(true);
                break;
            }
            case 'F':
            {
                set_value(false);
                break;
            }
            case 'B':
            {
                if (n < 1)
                    JSON_BD_FAIL();
                m_type = cJSONValueTypeInt;
                m_data.m_nVal = *pSrc++;
                break;
            }
            case 'i':
            {
                if (n < 2)
                    JSON_BD_FAIL();
                m_type = cJSONValueTypeInt;
                m_data.m_nVal = static_cast<int16_t>((pSrc[0] << 8) | pSrc[1]);
                pSrc += 2;
                break;
            }
            case 'I':
            {
                if (n < 4)
                    JSON_BD_FAIL();
                m_type = cJSONValueTypeInt;
                m_data.m_nVal = static_cast<int32_t>((pSrc[0] << 24) | (pSrc[1] << 16) | (pSrc[2] << 8) | pSrc[3]);
                pSrc += 4;
                break;
            }
            case 'L':
            {
                if (n < 8)
                    JSON_BD_FAIL();
                m_type = cJSONValueTypeInt;
                uint64_t v = 0;
                for (uint32_t i = 8; i; --i)
                    v = (v << 8) | *pSrc++;
                m_data.m_nVal = static_cast<int64_t>(v);
                break;
            }
            case 'd':
            {
                if (n < 4)
                    JSON_BD_FAIL();
                m_type = cJSONValueTypeDouble;
                union
                {
                    uint32_t m_u32;
                    float m_fl32;
                } bits;
                bits.m_u32 = static_cast<uint32_t>((pSrc[0] << 24) | (pSrc[1] << 16) | (pSrc[2] << 8) | pSrc[3]);
                pSrc += 4;
                m_data.m_flVal = static_cast<double>(bits.m_fl32);
                break;
            }
            case 'D':
            {
                if (n < 8)
                    JSON_BD_FAIL();
                m_type = cJSONValueTypeDouble;
                uint64_t v = 0;
                for (uint32_t i = 8; i; --i)
                    v = (v << 8) | *pSrc++;
                m_data.m_nVal = v;
                break;
            }
            default:
                return false;
        }

#undef JSON_BD_FAIL

        pBuf = pSrc;
        return true;
    }

    dynamic_string &json_value::serialize(dynamic_string &str, bool formatted, uint32_t cur_indent, uint32_t max_line_len) const
    {
        vogl::vector<char> buf;
        serialize(buf, formatted, cur_indent, false, max_line_len);
        return str.set_from_buf(buf.get_ptr(), buf.size_in_bytes());
    }

    void json_value::print(bool formatted, uint32_t cur_indent, uint32_t max_line_len) const
    {
        vogl::vector<char> buf;
        serialize(buf, formatted, cur_indent, true, max_line_len);
        puts(buf.get_ptr());
    }

    bool json_value::serialize_to_file(const char *pFilename, bool formatted, uint32_t max_line_len) const
    {
        FILE *pFile = vogl_fopen(pFilename, "wb");
        if (!pFile)
            return false;
        bool success = serialize(pFile, formatted, max_line_len);
        if (vogl_fclose(pFile) == EOF)
            success = false;
        return success;
    }

    bool json_value::serialize(FILE *pFile, bool formatted, uint32_t max_line_len) const
    {
        vogl::vector<char> buf;
        serialize(buf, formatted, 0, false, max_line_len);
        return fwrite(buf.get_ptr(), 1, buf.size(), pFile) == buf.size();
    }

    bool json_value::deserialize_node(json_deserialize_buf_ptr &pStr, json_node *pParent, uint32_t level, json_error_info_t &error_info)
    {
        const bool is_object = (*pStr == '{');
        const char end_char = is_object ? '}' : ']';

        json_node *pNode = set_value_to_node(is_object, pParent);
        pNode->m_line = pStr.get_cur_line();

        pStr.advance_in_line_no_end_check(1);
        pStr.skip_whitespace();

        for (;;)
        {
            char c = pStr.get_char();
            if (c == end_char)
                break;

            uint32_t value_index = pNode->enlarge(1);

            if (is_object)
            {
                if (c != '\"')
                {
                    error_info.set_error(pStr.get_cur_line(), "Expected quoted key string");
                    return false;
                }

                pStr.advance_in_line_no_end_check(1);

                uint32_t estimated_len;
                if (!estimate_deserialized_string_size(pStr, error_info, estimated_len))
                    return false;

                if (estimated_len >= cMaxDynamicStringLen)
                {
                    error_info.set_error(pStr.get_cur_line(), "Quoted name string is too long");
                    return false;
                }
                uint32_t buf_size = estimated_len + 1;
                char *pBuf = dynamic_string::create_raw_buffer(buf_size);

                char *pDst = pBuf;
                if (!deserialize_quoted_string_to_buf(pDst, buf_size, pStr, error_info))
                {
                    dynamic_string::free_raw_buffer(pBuf);
                    return false;
                }
                pNode->get_key(value_index).set_from_raw_buf_and_assume_ownership(pBuf, buf_size, static_cast<uint32_t>(pDst - pBuf));

                pStr.skip_whitespace();

                if (*pStr != ':')
                {
                    error_info.set_error(pStr.get_cur_line(), "Missing colon after key");
                    return false;
                }

                pStr.advance_in_line_no_end_check(1);

                pStr.skip_whitespace();
            }

            json_value &newVal = pNode->m_values[value_index];
            if (level > cMaxValidDepth)
            {
                error_info.set_error(pStr.get_cur_line(), "Document is too complex");
                return false;
            }

            if (!newVal.deserialize(pStr, pNode, level + 1, error_info))
                return false;

            pStr.skip_whitespace();

            if (*pStr != ',')
                break;

            pStr.advance_in_line_no_end_check(1);

            pStr.skip_whitespace();
        }

        if (pStr.get_and_advance() != end_char)
        {
            error_info.set_error(pStr.get_cur_line(), "Unexpected character within object or array");
            return false;
        }

        return true;
    }

    static const uint8_t g_utf8_first_byte[7] =
        {
            0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
        };

    bool json_value::estimate_deserialized_string_size(json_deserialize_buf_ptr &pStr, json_error_info_t &error_info, uint32_t &size)
    {
        json_deserialize_buf_ptr pTmpStr(pStr);
        uint32_t len = 0;
        for (;;)
        {
            char c = pTmpStr.get_and_advance();
            if ((!c) || (c == '\n'))
            {
                error_info.set_error(pStr.get_cur_line(), "Missing end quote in string");
                return false;
            }
            if (c == '\"')
                break;
            else if (c == '\\')
            {
                c = pTmpStr.get_and_advance();
                if ((!c) || (c == '\n'))
                {
                    error_info.set_error(pStr.get_cur_line(), "Missing escape character");
                    return false;
                }
                len++; // in case the escape char is a backslash
            }
            len++;
        }
        size = len;
        return true;
    }

    // Note: This method assumes estimate_deserialized_string_size() was called on the string first, so it skips some checks.
    bool json_value::deserialize_quoted_string_to_buf(char *&pBuf, uint32_t buf_size, json_deserialize_buf_ptr &pStr, json_error_info_t &error_info)
    {
        VOGL_NOTE_UNUSED(buf_size);

        char *pDst = pBuf;
        for (;;)
        {
            char c = pStr.get_and_advance();
            if (c == '\"')
                break;
            else if (c == '\\')
            {
                c = pStr.get_and_advance();
                if (c == 'u')
                {
                    uint32_t u = 0;
                    for (uint32_t i = 0; i < 4; i++)
                    {
                        u <<= 4;
                        int c2 = vogl_tolower(pStr.get_and_advance());
                        if ((c2 >= 'a') && (c2 <= 'f'))
                            u += c2 - 'a' + 10;
                        else if ((c2 >= '0') && (c2 <= '9'))
                            u += c2 - '0';
                        else
                        {
                            error_info.set_error(pStr.get_cur_line(), "Invalid Unicode escape");
                            return false;
                        }
                    }

                    uint32_t len;
                    if ((u) && (u < 0x80))
                        len = 1;
                    else if (u < 0x800)
                        len = 2;
                    else
                        len = 3;

                    pDst += len;
                    switch (len)
                    {
                        case 3:
                            *--pDst = static_cast<char>((u | 0x80) & 0xBF);
                            u >>= 6; // falls through
                        case 2:
                            *--pDst = static_cast<char>((u | 0x80) & 0xBF);
                            u >>= 6; // falls through
                        case 1:
                            *--pDst = static_cast<char>(u | g_utf8_first_byte[len]);
                    }
                    pDst += len;
                }
                else
                {
                    switch (c)
                    {
                        case 'b':
                            c = '\b';
                            break;
                        case 'f':
                            c = '\f';
                            break;
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        case '\\':
                        case '\"':
                        case '/':
                            break;
                        default:
                            *pDst++ = '\\';
                            break; // unrecognized escape, so forcefully escape the backslash
                    }

                    *pDst++ = c;
                }
            }
            else
            {
                *pDst++ = c;
            }
        }

        *pDst = '\0';

        VOGL_ASSERT((pDst - pBuf) < (int)buf_size);

        pBuf = pDst;

        return true;
    }

    bool json_value::deserialize_string(json_deserialize_buf_ptr &pStr, json_error_info_t &error_info)
    {
        pStr.advance_in_line_no_end_check(1);

        uint32_t estimated_len;
        if (!estimate_deserialized_string_size(pStr, error_info, estimated_len))
            return false;

        uint32_t buf_size = estimated_len + 1;
        char *pBuf = (char *)vogl_malloc(buf_size);
        if (!pBuf)
        {
            error_info.set_error(pStr.get_cur_line(), "Out of memory");
            return false;
        }

        char *pDst = pBuf;
        if (!deserialize_quoted_string_to_buf(pDst, buf_size, pStr, error_info))
            return false;

        set_value_assume_ownership(pBuf);

        return true;
    }

    bool json_value::is_equal(const json_value &other, double tol) const
    {
        if (this == &other)
            return true;

        if (get_type() != other.get_type())
        {
            if (is_numeric() && other.is_numeric())
                return math::equal_tol(as_double(), other.as_double(), tol);
            return false;
        }

        switch (get_type())
        {
            case cJSONValueTypeNull:
                return true;
            case cJSONValueTypeBool:
            case cJSONValueTypeInt:
                return m_data.m_nVal == other.m_data.m_nVal;
            case cJSONValueTypeDouble:
                return math::equal_tol(m_data.m_flVal, other.m_data.m_flVal, tol);
            case cJSONValueTypeString:
                return !strcmp(m_data.m_pStr, other.m_data.m_pStr);
            case cJSONValueTypeNode:
            {
                const json_node *pNode = get_node_ptr();
                const json_node *pOther_node = other.get_node_ptr();
                if (pNode->is_object() != pOther_node->is_object())
                    return false;

                if (pNode->size() != pOther_node->size())
                    return false;

                for (uint32_t i = 0; i < pNode->size(); i++)
                {
                    if ((pNode->is_object()) && (pNode->get_key(i).compare(pOther_node->get_key(i), true) != 0))
                        return false;
                    if (!pNode->get_value(i).is_equal(pOther_node->get_value(i), tol))
                        return false;
                }
            }
        }

        return true;
    }

    static const double g_pow10_table[63] =
        {
            1.e-031, 1.e-030, 1.e-029, 1.e-028, 1.e-027, 1.e-026, 1.e-025, 1.e-024, 1.e-023, 1.e-022, 1.e-021, 1.e-020, 1.e-019, 1.e-018, 1.e-017, 1.e-016,
            1.e-015, 1.e-014, 1.e-013, 1.e-012, 1.e-011, 1.e-010, 1.e-009, 1.e-008, 1.e-007, 1.e-006, 1.e-005, 1.e-004, 1.e-003, 1.e-002, 1.e-001, 1.e+000,
            1.e+001, 1.e+002, 1.e+003, 1.e+004, 1.e+005, 1.e+006, 1.e+007, 1.e+008, 1.e+009, 1.e+010, 1.e+011, 1.e+012, 1.e+013, 1.e+014, 1.e+015, 1.e+016,
            1.e+017, 1.e+018, 1.e+019, 1.e+020, 1.e+021, 1.e+022, 1.e+023, 1.e+024, 1.e+025, 1.e+026, 1.e+027, 1.e+028, 1.e+029, 1.e+030, 1.e+031
        };

    bool json_value::deserialize_number(json_deserialize_buf_ptr &pStr, json_error_info_t &error_info)
    {
        uint64_t n = 0, limit = cINT64_MAX;
        int sign = 1;
        bool bParseAsDouble = false;

        char c = pStr.get_char();
        if (c == '-')
        {
            sign = -1;
            limit++;
            pStr.advance_in_line_no_end_check(1);
            c = pStr.get_char();
        }

        while ((c >= '0') && (c <= '9'))
        {
            if (n > 0x1999999999999998ULL)
            {
                bParseAsDouble = true;
                break;
            }
            n = n * 10U + c - '0';
            pStr.advance_in_line_no_end_check(1);
            c = pStr.get_char();
        }

        if ((bParseAsDouble) || (n > limit) || (c == 'e') || (c == 'E') || (c == '.'))
        {
            double f = static_cast<double>(n);
            int scale = 0, escalesign = 1, escale = 0;

            while ((c >= '0') && (c <= '9'))
            {
                f = f * 10.0f + (c - '0');
                pStr.advance_in_line_no_end_check(1);
                c = pStr.get_char();
            }

            if (c == '.')
            {
                pStr.advance_in_line_no_end_check(1);
                c = pStr.get_char();
                while ((c >= '0') && (c <= '9'))
                {
                    scale--;
                    f = f * 10.0f + (c - '0');
                    pStr.advance_in_line_no_end_check(1);
                    c = pStr.get_char();
                }
            }

            if ((c == 'e') || (c == 'E'))
            {
                pStr.advance_in_line_no_end_check(1);

                c = pStr.get_char();
                if (c == '-')
                {
                    escalesign = -1;
                    pStr.advance_in_line_no_end_check(1);
                    c = pStr.get_char();
                }
                else if (c == '+')
                {
                    pStr.advance_in_line_no_end_check(1);
                    c = pStr.get_char();
                }
                while ((c >= '0') && (c <= '9'))
                {
                    if (escale > 0xCCCCCCB)
                    {
                        error_info.set_error(pStr.get_cur_line(), "Failed parsing numeric value");
                        return false;
                    }
                    escale = escale * 10 + (c - '0');
                    pStr.advance_in_line_no_end_check(1);
                    c = pStr.get_char();
                }
            }

            double v = sign * f;
            int64_t final_scale = scale + escale * escalesign;
            if ((final_scale < -31) || (final_scale > 31))
            {
                if ((final_scale < cINT32_MIN) || (final_scale > cINT32_MAX))
                {
                    error_info.set_error(pStr.get_cur_line(), "Failed parsing numeric value");
                    return false;
                }
                v *= pow(10.0, static_cast<int>(final_scale));
            }
            else
                v *= g_pow10_table[static_cast<int>(final_scale) + 31];

            set_value(v);
        }
        else
        {
            int64_t v = static_cast<int64_t>(n);
            if (sign < 0)
                v = -v;
            set_value(v);
        }

        return true;
    }

    bool json_value::deserialize_file(const char *pFilename, json_error_info_t *pError_info)
    {
        clear();

        FILE *pFile = vogl_fopen(pFilename, "rb");
        if (!pFile)
        {
            if (pError_info)
                pError_info->set_error(0, "Unable to open file");
            return false;
        }

        bool success = deserialize(pFile, pError_info);

        if (vogl_fclose(pFile) == EOF)
            success = false;

        return success;
    }

    bool json_value::deserialize(const vogl::vector<char> &buf, json_error_info_t *pError_info)
    {
        return deserialize(buf.get_ptr(), buf.size(), pError_info);
    }

    bool json_value::deserialize(FILE *pFile, json_error_info_t *pError_info)
    {
        vogl_fseek(pFile, 0, SEEK_END);
        const uint64_t filesize = vogl_ftell(pFile);
        vogl_fseek(pFile, 0, SEEK_SET);

        if ((filesize > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE) || (filesize != static_cast<size_t>(filesize)))
        {
            if (pError_info)
                pError_info->set_error(0, "File too large");
            return false;
        }

        char *pBuf = (char *)vogl_malloc(static_cast<size_t>(filesize) + 1);
        if (!pBuf)
        {
            if (pError_info)
                pError_info->set_error(0, "Out of memory");
            return false;
        }

        if (vogl_fread(pBuf, 1, static_cast<size_t>(filesize), pFile) != static_cast<size_t>(filesize))
        {
            vogl_free(pBuf);
            if (pError_info)
                pError_info->set_error(0, "Failed reading from file");
            return false;
        }

        // Not really necesssary
        pBuf[filesize] = '\0';

        bool status = deserialize(pBuf, static_cast<size_t>(filesize), pError_info);

        vogl_free(pBuf);

        return status;
    }

    bool json_value::deserialize(const char *pStr, json_error_info_t *pError_info)
    {
        return deserialize(pStr, strlen(pStr), pError_info);
    }

    bool json_value::deserialize(const dynamic_string &str, json_error_info_t *pError_info)
    {
        return deserialize(str.get_ptr(), str.get_len(), pError_info);
    }

    bool json_value::deserialize(const char *pBuf, size_t buf_size, json_error_info_t *pError_info)
    {
        set_value_to_null();

        if (!buf_size)
        {
            if (pError_info)
                pError_info->set_error(0, "Nothing to deserialize");
            return false;
        }

        json_deserialize_buf_ptr buf_ptr(pBuf, buf_size);
        buf_ptr.skip_whitespace();
        if (*buf_ptr == '\0')
        {
            if (pError_info)
                pError_info->set_error(0, "Nothing to deserialize");
            return false;
        }

        json_error_info_t dummy_error_info;
        if (!deserialize(buf_ptr, NULL, 0, pError_info ? *pError_info : dummy_error_info))
            return false;

        buf_ptr.skip_whitespace();
        if (!buf_ptr.at_end())
        {
            if (pError_info)
                pError_info->set_error(buf_ptr.get_cur_line(), "Syntax error on/near line");
            return false;
        }
        return true;
    }

    bool json_value::deserialize(json_deserialize_buf_ptr &pStr, json_node *pParent, uint32_t level, json_error_info_t &error_info)
    {
        m_line = pStr.get_cur_line();

        char c = *pStr;
        switch (c)
        {
            case '{':
            case '[':
                return deserialize_node(pStr, pParent, level, error_info);
            case '\"':
                return deserialize_string(pStr, error_info);
            case 'n':
            {
                if (pStr.compare_string("null", 4))
                {
                    set_value_to_null();
                    pStr.advance_in_line(4);
                    return true;
                }
                break;
            }
            case 't':
            {
                if (pStr.compare_string("true", 4))
                {
                    set_value(true);
                    pStr.advance_in_line(4);
                    return true;
                }
                break;
            }
            case 'f':
            {
                if (pStr.compare_string("false", 5))
                {
                    set_value(false);
                    pStr.advance_in_line(5);
                    return true;
                }
                break;
            }
            default:
            {
                if ((c == '-') || (c == '.') || ((c >= '0') && (c <= '9')))
                    return deserialize_number(pStr, error_info);
                break;
            }
        }
        error_info.set_error(pStr.get_cur_line(), "Unrecognized character: '%c'", c);
        return false;
    }

    // class json_node

    json_node::json_node()
        : m_pParent(NULL),
          m_line(0),
          m_is_object(false)
    {
    }

    json_node::json_node(const json_node &other)
        : m_pParent(NULL),
          m_line(0),
          m_is_object(false)
    {
        *this = other;
    }

    json_node::json_node(const json_node *pParent, bool is_object)
        : m_pParent(pParent),
          m_line(0),
          m_is_object(is_object)
    {
    }

    json_node::~json_node()
    {
        clear();
    }

    json_node &json_node::operator=(const json_node &rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        set_is_object(rhs.is_object());
        m_pParent = rhs.m_pParent;

        if (is_object())
            m_keys.resize(rhs.size());
        m_values.resize(rhs.size());

        for (uint32_t i = 0; i < rhs.size(); i++)
        {
            if (is_object())
                m_keys[i] = rhs.m_keys[i];

            m_values[i] = rhs.m_values[i];
            if (m_values[i].is_node())
                m_values[i].get_node_ptr()->m_pParent = this;

            //VOGL_ASSERT(m_values[i].validate(this));
        }

        m_line = rhs.m_line;

        return *this;
    }

    const json_value &json_node::find_value(const char *pKey) const
    {
        int index = find_key(pKey);
        return (index < 0) ? get_null_json_value() : m_values[index];
    }

    int json_node::find_key(const char *pKey) const
    {
        for (uint32_t i = 0; i < m_keys.size(); i++)
            if (m_keys[i] == pKey)
                return i;
        return cInvalidIndex;
    }

    int json_node::find_child(const json_node *pNode) const
    {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (m_values[i].get_node_ptr() == pNode)
                return i;
        return cInvalidIndex;
    }

    json_value_type_t json_node::find_value_type(const char *pKey) const
    {
        int index = find_key(pKey);
        return (index >= 0) ? get_value_type(index) : cJSONValueTypeNull;
    }

    const json_node *json_node::find_child(const char *pKey) const
    {
        int index = find_key(pKey);
        return (index >= 0) ? get_child(index) : NULL;
    }

    json_node *json_node::find_child(const char *pKey)
    {
        int index = find_key(pKey);
        return (index >= 0) ? get_child(index) : NULL;
    }

    const json_node *json_node::find_child_array(const char *pKey) const
    {
        const json_node *pNode = find_child(pKey);
        return (pNode && pNode->is_array()) ? pNode : NULL;
    }

    json_node *json_node::find_child_array(const char *pKey)
    {
        json_node *pNode = find_child(pKey);
        return (pNode && pNode->is_array()) ? pNode : NULL;
    }

    const json_node *json_node::find_child_object(const char *pKey) const
    {
        const json_node *pNode = find_child(pKey);
        return (pNode && pNode->is_object()) ? pNode : NULL;
    }

    bool json_node::are_all_children_values() const
    {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (m_values[i].is_object_or_array())
                return false;
        return true;
    }

    bool json_node::are_all_children_objects() const
    {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (!m_values[i].is_object())
                return false;
        return true;
    }

    bool json_node::are_all_children_arrays() const
    {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (!m_values[i].is_array())
                return false;
        return true;
    }

    bool json_node::are_all_children_objects_or_arrays() const
    {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (!m_values[i].is_object_or_array())
                return false;
        return true;
    }

    json_node *json_node::find_child_object(const char *pKey)
    {
        json_node *pNode = find_child(pKey);
        return (pNode && pNode->is_object()) ? pNode : NULL;
    }

    bool json_node::get_value_as_string(const char *pKey, dynamic_string &val, const char *pDef) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val.set(pDef);
            return false;
        }
        return m_values[index].get_string(val, pDef);
    }

    bool json_node::get_value_as_int64(const char *pKey, int64_t &val, int64_t def) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val = def;
            return false;
        }
        return m_values[index].get_numeric(val, def);
    }

    bool json_node::get_value_as_uint32(const char *pKey, uint32_t &val, uint32_t def) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val = def;
            return false;
        }
        return m_values[index].get_numeric(val, def);
    }

    bool json_node::get_value_as_uint64(const char *pKey, uint64_t &val, uint64_t def) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val = def;
            return false;
        }
        return m_values[index].get_numeric(val, def);
    }

    bool json_node::get_value_as_float(const char *pKey, float &val, float def) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val = def;
            return false;
        }
        return m_values[index].get_numeric(val, def);
    }

    bool json_node::get_value_as_double(const char *pKey, double &val, double def) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val = def;
            return false;
        }
        return m_values[index].get_numeric(val, def);
    }

    bool json_node::get_value_as_bool(const char *pKey, bool &val, bool def) const
    {
        int index = find_key(pKey);
        if (index < 0)
        {
            val = def;
            return false;
        }
        return m_values[index].get_bool(val, def);
    }

    dynamic_string json_node::value_as_string(const char *pKey, const char *pDef) const
    {
        dynamic_string result;
        get_value_as_string(pKey, result, pDef);
        return result;
    }

    const char *json_node::value_as_string_ptr(const char *pKey, const char *pDef) const
    {
        int index = find_key(pKey);
        if (index < 0)
            return pDef;
        return value_as_string_ptr(index, pDef);
    }

    int json_node::value_as_int(const char *pKey, int def) const
    {
        int result;
        get_value_as_int(pKey, result, def);
        return result;
    }

    int32_t json_node::value_as_int32(const char *pKey, int32_t def) const
    {
        int32_t result;
        get_value_as_int(pKey, result, def);
        return result;
    }

    uint32_t json_node::value_as_uint32(const char *pKey, uint32_t def) const
    {
        uint32_t result;
        get_value_as_uint32(pKey, result, def);
        return result;
    }

    uint64_t json_node::value_as_uint64(const char *pKey, uint64_t def) const
    {
        uint64_t result;
        get_value_as_uint64(pKey, result, def);
        return result;
    }

    int64_t json_node::value_as_int64(const char *pKey, int64_t def) const
    {
        int64_t result;
        get_value_as_int(pKey, result, def);
        return result;
    }

    float json_node::value_as_float(const char *pKey, float def) const
    {
        float result;
        get_value_as_float(pKey, result, def);
        return result;
    }

    double json_node::value_as_double(const char *pKey, double def) const
    {
        double result;
        get_value_as_double(pKey, result, def);
        return result;
    }

    bool json_node::value_as_bool(const char *pKey, bool def) const
    {
        bool result;
        get_value_as_bool(pKey, result, def);
        return result;
    }

    dynamic_string json_node::value_as_string(uint32_t index, const char *pDef) const
    {
        return m_values[index].as_string(pDef);
    }

    const char *json_node::value_as_string_ptr(uint32_t index, const char *pDef) const
    {
        return m_values[index].as_string_ptr(pDef);
    }

    int json_node::value_as_int(uint32_t index, int def) const
    {
        return m_values[index].as_int(def);
    }

    int32_t json_node::value_as_int32(uint32_t index, int32_t def) const
    {
        return m_values[index].as_int32(def);
    }

    int64_t json_node::value_as_int64(uint32_t index, int64_t def) const
    {
        return m_values[index].as_int64(def);
    }

    uint64_t json_node::value_as_uint64(uint32_t index, uint64_t def) const
    {
        return m_values[index].as_uint64(def);
    }

    uint32_t json_node::value_as_uint32(uint32_t index, uint32_t def) const
    {
        return m_values[index].as_uint32(def);
    }

    float json_node::value_as_float(uint32_t index, float def) const
    {
        return m_values[index].as_float(def);
    }

    double json_node::value_as_double(uint32_t index, double def) const
    {
        return m_values[index].as_double(def);
    }

    bool json_node::value_as_bool(uint32_t index, bool def) const
    {
        return m_values[index].as_bool(def);
    }

    void json_node::clear()
    {
        m_keys.clear();
        m_values.clear();
        m_is_object = false;
        m_line = 0;
    }

    void json_node::resize(uint32_t new_size)
    {
        if (m_is_object)
            m_keys.resize(new_size, true);
        m_values.resize(new_size, true);
    }

    void json_node::reserve(uint32_t new_capacity)
    {
        if (m_is_object)
            m_keys.reserve(new_capacity);
        m_values.reserve(new_capacity);
    }

    void json_node::set_is_object(bool is_object)
    {
        if (m_is_object == is_object)
            return;
        if (is_object)
            m_keys.resize(size());
        else
            m_keys.clear();
        m_is_object = is_object;
    }

    void json_node::ensure_is_object()
    {
        if (!m_is_object)
        {
            m_is_object = true;
            m_keys.resize(m_values.size());
        }
    }

    json_value &json_node::add_key_value(const char *pKey, const json_value &val)
    {
        ensure_is_object();

        uint32_t new_index = m_values.size();

        m_keys.push_back(pKey);

        m_values.push_back(val);
        if (val.is_node())
            m_values[new_index].get_node_ptr()->m_pParent = this;

        return m_values[new_index];
    }

    json_value &json_node::add_value()
    {
        if (m_is_object)
            m_keys.enlarge(1);
        return *m_values.enlarge(1);
    }

    json_value &json_node::add_value(const json_value &val)
    {
        uint32_t new_index = m_values.size();

        if (m_is_object)
            m_keys.enlarge(1);

        m_values.push_back(val);
        if (val.is_node())
            m_values[new_index].get_node_ptr()->m_pParent = this;

        return m_values[new_index];
    }

    bool json_node::add_key_and_parsed_value(const char *pKey, const char *pStr)
    {
        json_value val;
        if (!val.deserialize(pStr))
            return false;

        ensure_is_object();

        m_keys.push_back(pKey);

        json_value *p = m_values.enlarge(1);
        val.release_ownership(*p);
        if (p->is_node())
            p->get_node_ptr()->m_pParent = this;

        return true;
    }

    bool json_node::add_parsed_value(const char *pStr)
    {
        json_value val;
        if (!val.deserialize(pStr))
            return false;

        if (m_is_object)
            m_keys.enlarge(1);

        json_value *p = m_values.enlarge(1);
        val.release_ownership(*p);
        if (p->is_node())
            p->get_node_ptr()->m_pParent = this;

        return true;
    }

    dynamic_string &json_node::get_key(uint32_t index)
    {
        if (!m_is_object)
        {
            VOGL_ASSERT_ALWAYS;
            return get_empty_dynamic_string();
        }
        return m_keys[index];
    }

    void json_node::set_key_value(uint32_t index, const char *pKey, const json_value &val)
    {
        ensure_is_object();
        m_keys[index].set(pKey);

        m_values[index] = val;
        if (m_values[index].is_node())
            m_values[index].get_node_ptr()->m_pParent = this;
    }

    void json_node::set_key(uint32_t index, const char *pKey)
    {
        ensure_is_object();
        m_keys[index].set(pKey);
    }

    void json_node::set_value(uint32_t index, const json_value &val)
    {
        m_values[index] = val;
        if (m_values[index].is_node())
            m_values[index].get_node_ptr()->m_pParent = this;
    }

    void json_node::set_value_assume_ownership(uint32_t index, json_value &val)
    {
        json_value &val_to_set = m_values[index];
        val_to_set.set_value_to_null();
        val_to_set.swap(val);

        if (val_to_set.is_node())
            val_to_set.get_node_ptr()->m_pParent = this;
    }

    void json_node::add_value_assume_ownership(json_value &val)
    {
        if (m_is_object)
            m_keys.enlarge(1);
        uint32_t new_index = m_values.size();
        m_values.enlarge(1);
        set_value_assume_ownership(new_index, val);
    }

    void json_node::add_key_value_assume_ownership(const char *pKey, json_value &val)
    {
        ensure_is_object();
        m_keys.push_back(pKey);

        uint32_t new_index = m_values.size();
        m_values.enlarge(1);
        set_value_assume_ownership(new_index, val);
    }

    json_value &json_node::get_or_add(const char *pKey)
    {
        int index = find_key(pKey);
        if (index >= 0)
            return m_values[index];

        ensure_is_object();
        m_keys.push_back(pKey);
        return *m_values.enlarge(1);
    }

    json_value &json_node::add(const char *pKey)
    {
        ensure_is_object();
        m_keys.push_back(pKey);
        return *m_values.enlarge(1);
    }

    json_node &json_node::add_object(const char *pKey)
    {
        ensure_is_object();
        m_keys.push_back(pKey);

        json_node *pNode = get_json_node_pool()->alloc(this, true);
        m_values.enlarge(1);
        m_values.back().set_value_assume_ownership(pNode);
        return *pNode;
    }

    json_node &json_node::add_array(const char *pKey)
    {
        ensure_is_object();
        m_keys.push_back(pKey);

        json_node *pNode = get_json_node_pool()->alloc(this, false);
        m_values.enlarge(1)->set_value_assume_ownership(pNode);
        return *pNode;
    }

    json_node &json_node::add_object()
    {
        if (m_is_object)
            m_keys.enlarge(1);
        json_node *pNode = get_json_node_pool()->alloc(this, true);
        m_values.enlarge(1)->set_value_assume_ownership(pNode);
        return *pNode;
    }

    json_node &json_node::add_array()
    {
        if (m_is_object)
            m_keys.enlarge(1);
        json_node *pNode = get_json_node_pool()->alloc(this, false);
        m_values.enlarge(1)->set_value_assume_ownership(pNode);
        return *pNode;
    }

    bool json_node::erase(const char *pKey)
    {
        int index = find_key(pKey);
        if (index >= 0)
        {
            erase(index);
            return true;
        }
        return false;
    }

    void json_node::erase(uint32_t index)
    {
        if (m_is_object)
            m_keys.erase(index);
        m_values.erase(index);
    }

    bool json_node::basic_validation(const json_node *pParent) const
    {
        if (m_pParent != pParent)
            return false;

        if (!m_is_object)
        {
            if (m_keys.size())
                return false;
        }
        else if (m_keys.size() != m_values.size())
            return false;

        return true;
    }

    bool json_node::validate(const json_node *pParent) const
    {
        if (!basic_validation(pParent))
            return false;

        for (uint32_t i = 0; i < m_values.size(); i++)
            if (!m_values[i].validate(this))
                return false;

        return true;
    }

    dynamic_string json_node::get_path_to_node() const
    {
        dynamic_string_array node_names;

        const json_node *pCur_node = this;

        for (;;)
        {
            const json_node *pParent = pCur_node->m_pParent;

            if (!pParent)
            {
                node_names.push_back("[root]");
                break;
            }

            int parent_child_index = pParent->find_child(pCur_node);
            if (parent_child_index < 0)
            {
                VOGL_VERIFY(0);
                break;
            }

            if (pParent->is_array())
                node_names.push_back(dynamic_string(cVarArg, "[%u]", parent_child_index));
            else
                node_names.push_back(pParent->get_key(parent_child_index));

            pCur_node = pParent;
        }

        dynamic_string result;
        for (int i = node_names.size() - 1; i >= 0; i--)
        {
            result.append(node_names[i]);
            if (i)
                result.append("/");
        }

        return result;
    }

    dynamic_string json_node::get_path_to_key(const char *pKey) const
    {
        dynamic_string path(get_path_to_node());
        path.append("/");
        path.append(pKey);
        return path;
    }

    dynamic_string json_node::get_path_to_item(uint32_t index) const
    {
        dynamic_string path(get_path_to_node());
        if (is_array())
            path.append(dynamic_string(cVarArg, "/[%u]", index).get_ptr());
        else
            path.append(dynamic_string(cVarArg, "/%s", m_keys[index].get_ptr()).get_ptr());
        return path;
    }

    bool json_node::check_for_duplicate_keys() const
    {
        VOGL_VERIFY(validate(m_pParent));

        bool status = false;

        if (m_is_object)
        {
            dynamic_string_array sorted_keys(m_keys);

            sorted_keys.sort(dynamic_string_less_than_case_sensitive());

            for (uint32_t i = 0; i < sorted_keys.size(); i++)
            {
                uint32_t count = 1;

                while (((i + 1) < sorted_keys.size()) && (sorted_keys[i].compare(sorted_keys[i + 1], true) == 0))
                {
                    count++;
                    i++;
                }

                VOGL_ASSERT(((i + 1) == sorted_keys.size()) || (sorted_keys[i].compare(sorted_keys[i + 1], true) < 0));

                if (count > 1)
                {
                    status = true;

                    vogl_debug_printf("Key appears %u times in node: %s\n", count, get_path_to_item(i).get_ptr());
                }
            }
        }

        for (uint32_t i = 0; i < m_values.size(); i++)
        {
            const json_node *pChild = get_child(i);

            if ((pChild) && (pChild->check_for_duplicate_keys()))
                status = true;
        }

        return status;
    }

    void json_node::serialize(vogl::vector<char> &buf, bool formatted, uint32_t cur_indent, bool null_terminate, uint32_t max_line_len) const
    {
        json_growable_char_buf growable_buf(buf);
        if ((cur_indent) && (formatted))
            growable_buf.print_tabs(cur_indent);
        serialize(growable_buf, formatted, cur_indent, max_line_len);
        if (null_terminate)
            buf.push_back('\0');
    }

    void json_node::serialize(dynamic_string &str, bool formatted, uint32_t cur_index, uint32_t max_line_len) const
    {
        vogl::vector<char> buf;
        serialize(buf, formatted, cur_index, true, max_line_len);

        // FIXME: Pass along string ownership once dynamic_string supports non-pow2 sized buffers
        str.set_from_buf(buf.get_ptr(), vogl_strlen(buf.get_ptr()));
    }

    void json_node::serialize(json_growable_char_buf &buf, bool formatted, uint32_t cur_indent, uint32_t max_line_len) const
    {
        if (is_object())
        {
            VOGL_ASSERT(m_keys.size() == m_values.size());
        }
        else
        {
            VOGL_ASSERT(!m_keys.size());
        }

        if (!size())
        {
            if (formatted)
                buf.puts(is_object() ? "{ }" : "[ ]");
            else
                buf.puts(is_object() ? "{}" : "[]");
            return;
        }

        const uint32_t cMaxLineLen = max_line_len;

#if 0
	if (formatted && !has_children() && !is_object() && (size() <= 40))
	{
		uint32_t orig_buf_size = buf.size();
		buf.puts("[ ");

		for (uint32_t i = 0; i < size(); i++)
		{
			dynamic_string val;
			get_value(i).get_string(val);
			if (get_value_type(i) == cJSONValueTypeString)
				buf.print_escaped(val.get_ptr());
			else
				buf.puts(val.get_ptr());

			if (i != size() - 1)
			{
				buf.print_char(',');
				buf.print_char(' ');
			}

			if ((buf.size() - orig_buf_size) > cMaxLineLen)
				break;
		}

		buf.puts(" ]");

		if ((buf.size() - orig_buf_size) <= cMaxLineLen)
			return;

		buf.get().resize(orig_buf_size);
	}
#else
        if (formatted && !has_children() && !is_object())
        {
            uint32_t line_start_ofs = buf.size();

            buf.puts("[ ");
            if (formatted && !cMaxLineLen)
                buf.print_char('\n');

            for (uint32_t i = 0; i < size(); i++)
            {
                dynamic_string val;
                get_value(i).get_string(val);

                if (formatted && !cMaxLineLen)
                    buf.print_tabs(cur_indent);

                if (get_value_type(i) == cJSONValueTypeString)
                    buf.print_escaped(val.get_ptr());
                else
                    buf.puts(val.get_ptr());

                if (i != size() - 1)
                {
                    buf.print_char(',');
                    if (formatted && !cMaxLineLen)
                        buf.print_char('\n');
                }

                if (cMaxLineLen)
                {
                    if ((buf.size() - line_start_ofs) >= cMaxLineLen)
                    {
                        buf.print_char('\n');

                        buf.print_tabs(cur_indent);

                        line_start_ofs = buf.size();
                    }
                    else if (i != size() - 1)
                        buf.print_char(' ');
                }
            }

            buf.puts(" ]");

            return;
        }
#endif

        buf.print_char(is_object() ? '{' : '[');
        if (formatted)
            buf.print_char('\n');

        cur_indent++;

        dynamic_string val;

        for (uint32_t i = 0; i < size(); i++)
        {
            if (formatted)
                buf.print_tabs(cur_indent);
            if (is_object())
            {
                buf.print_escaped(get_key(i).get_ptr());
                buf.puts(formatted ? " : " : ":");
            }

            json_value_type_t val_type = get_value_type(i);
            if (val_type == cJSONValueTypeNode)
                get_child(i)->serialize(buf, formatted, cur_indent, max_line_len);
            else if (val_type == cJSONValueTypeString)
                buf.print_escaped(get_value(i).as_string_ptr());
            else
            {
                get_value(i).get_string(val);
                buf.puts(val.get_ptr());
            }

            if (i != size() - 1)
                buf.print_char(',');
            if (formatted)
                buf.print_char('\n');
        }

        cur_indent--;

        if (formatted)
            buf.print_tabs(cur_indent);
        buf.print_char(is_object() ? '}' : ']');
    }

    void json_node::binary_serialize(vogl::vector<uint8_t> &buf) const
    {
        uint8_t *pDst;
        uint32_t n = m_values.size();
        if (n <= 254)
        {
            pDst = buf.enlarge(2);
            pDst[0] = m_is_object ? 'o' : 'a';
            pDst[1] = static_cast<uint8_t>(n);
        }
        else
        {
            pDst = buf.enlarge(5);
            pDst[0] = m_is_object ? 'O' : 'A';
            pDst[1] = static_cast<uint8_t>(n >> 24);
            pDst[2] = static_cast<uint8_t>(n >> 16);
            pDst[3] = static_cast<uint8_t>(n >> 8);
            pDst[4] = static_cast<uint8_t>(n);
        }
        if (!n)
            return;

        const dynamic_string *pKey = m_keys.get_ptr();
        const json_value *pVal = m_values.get_ptr();
        for (uint32_t i = n; i; --i, ++pKey, ++pVal)
        {
            if (m_is_object)
            {
                uint32_t len = pKey->get_len();
                if (len <= 254)
                {
                    pDst = buf.enlarge(2 + len);
                    pDst[0] = 's';
                    pDst[1] = static_cast<uint8_t>(len);
                    memcpy(pDst + 2, pKey->get_ptr(), len);
                }
                else
                {
                    pDst = buf.enlarge(5 + len);
                    pDst[0] = 'S';
                    pDst[1] = static_cast<uint8_t>(len >> 24);
                    pDst[2] = static_cast<uint8_t>(len >> 16);
                    pDst[3] = static_cast<uint8_t>(len >> 8);
                    pDst[4] = static_cast<uint8_t>(len);
                    memcpy(pDst + 5, pKey->get_ptr(), len);
                }
            }

            if (pVal->is_node())
                pVal->get_node_ptr()->binary_serialize(buf);
            else
                pVal->binary_serialize_value(buf);
        }
    }

    bool json_node::has_children() const
    {
        for (uint32_t i = 0; i < m_values.size(); i++)
            if (m_values[i].get_type() == cJSONValueTypeNode)
                return true;
        return false;
    }

    bool json_node::get_value_as_enum(const char *pKey, const char **pStringList, int &val, int def) const
    {
        val = def;
        int index = find_key(pKey);
        if (index < 0)
            return false;
        return get_value(index).get_enum(pStringList, val, def);
    }

    // class json_document

    json_document::json_document()
        : m_error_line(0)
    {
        init_object();
    }

    json_document::json_document(const json_value &other)
        : m_error_line(0)
    {
        get_value() = other;
    }

    json_document &json_document::operator=(const json_value &rhs)
    {
        if (&get_value() == &rhs)
            return *this;

        clear();

        get_value() = rhs;

        return *this;
    }

    json_document::json_document(const json_document &other)
        : json_value(other),
          m_error_line(0)
    {
        *this = other;
    }

    json_document &json_document::operator=(const json_document &rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        m_error_msg = rhs.m_error_msg;
        m_error_line = rhs.m_error_line;
        m_filename = rhs.m_filename;

        get_value() = rhs.get_value();

        return *this;
    }

    json_document::json_document(const char *pStr, const char *pFilename)
        : m_error_line(0)
    {
        deserialize(pStr, pFilename);
    }

    json_document::json_document(const char *pBuf, uint32_t n, const char *pFilename)
        : m_error_line(0)
    {
        deserialize(pBuf, n, pFilename);
    }

    json_document::json_document(json_value_type_t value_type)
        : m_error_line(0)
    {
        init(value_type);
    }

    json_document::~json_document()
    {
        clear();
    }

    void json_document::swap(json_document &other)
    {
        std::swap(m_error_line, other.m_error_line);
        m_error_msg.swap(other.m_error_msg);
        m_filename.swap(other.m_filename);
        get_value().swap(other.get_value());
    }

    bool json_document::deserialize_file(const char *pFilename)
    {
        set_filename(pFilename);

        json_error_info_t err_info;
        if (!json_value::deserialize_file(pFilename, &err_info))
        {
            m_error_msg.swap(err_info.m_error_msg);
            m_error_line = err_info.m_error_line;

            return false;
        }

        return true;
    }

    bool json_document::deserialize(const vogl::vector<char> &buf, const char *pFilename)
    {
        return deserialize(buf.get_ptr(), buf.size(), pFilename);
    }

    bool json_document::deserialize(FILE *pFile, const char *pFilename)
    {
        set_filename(pFilename);

        json_error_info_t err_info;
        if (!json_value::deserialize(pFile, &err_info))
        {
            m_error_msg.swap(err_info.m_error_msg);
            m_error_line = err_info.m_error_line;

            return false;
        }

        return true;
    }

    bool json_document::deserialize(const char *pBuf, size_t n, const char *pFilename)
    {
        set_filename(pFilename);

        json_error_info_t err_info;
        if (!json_value::deserialize(pBuf, n, &err_info))
        {
            m_error_msg.swap(err_info.m_error_msg);
            m_error_line = err_info.m_error_line;

            return false;
        }

        return true;
    }

    bool json_document::deserialize(const char *pStr, const char *pFilename)
    {
        return deserialize(pStr, static_cast<uint32_t>(strlen(pStr)), pFilename);
    }

    bool json_document::deserialize(const dynamic_string &str, const char *pFilename)
    {
        return deserialize(str.get_ptr(), str.get_len(), pFilename);
    }

    class my_type
    {
    public:
        vogl::vector<dynamic_string> a1;
        vogl::map<dynamic_string, dynamic_string_array> a2;
        dynamic_string a;
        dynamic_string b;

        bool json_serialize(json_value &val) const
        {
            json_node *pObj = val.init_object();
            pObj->add_object("a1", a1);
            pObj->add_object("a2", a2);
            pObj->add_object("a", a);
            pObj->add_object("b", b);
            return true;
        }

        bool json_deserialize(const json_value &val)
        {
            const json_node *pObj = val.get_node_ptr();
            if (!pObj)
                return false;
            pObj->get_object("a1", a1);
            pObj->get_object("a2", a2);
            pObj->get_object("a", a);
            pObj->get_object("b", b);
            return true;
        }
    };

    // TODO: Actually test these classes.
    bool json_test()
    {
        // Create a JSON document
        json_document doc;
        json_node *pRoot = doc.get_root();

        typedef vogl::growable_array<uint32_t, 16> my_growable_array_type;
        my_growable_array_type growable_array;
        growable_array.push_back(99);
        growable_array.push_back(100);
        pRoot->add_object("growable_array", growable_array);
        pRoot->get_object("growable_array", growable_array);

        pRoot->add_vector("growable_array2", growable_array);
        pRoot->get_vector("growable_array2", growable_array);

        // Create a vector of my_type's, put it into a JSON document, then retrieve it into another vector.
        typedef vogl::vector<my_type> my_type_vec;

        my_type_vec mv;
        mv.resize(2);
        mv[0].a1.push_back("a");
        mv[0].a1.push_back("b");
        mv[0].a1.push_back("c");
        dynamic_string_array dsa;
        dsa.push_back("1");
        mv[0].a2.insert("zz", dsa);
        dsa.push_back("2");
        mv[0].a2.insert("xx", dsa);
        dsa.push_back("3");
        mv[0].a2.insert("yy", dsa);
        mv[0].a = "one";
        mv[0].b = "two";
        mv[1].a = "three";
        mv[1].b = "four";

        pRoot->add_vector("my_type_vec", mv);

        pRoot->add_object("hey", 2);
        int z = 0;
        pRoot->get_object("hey", z);

        pRoot->add_object("blah", mv);
        pRoot->get_object("blah", mv);

        my_type_vec mv2;
        pRoot->get_vector("my_type_vec", mv2);

        printf("my_type_vec:\n");
        for (uint32_t i = 0; i < mv2.size(); i++)
        {
            printf("a: %s\nb: %s\n", mv2[i].a.get_ptr(), mv2[i].b.get_ptr());
        }

        // Create a string set and add it to the JSON document under the "set" object
        typedef vogl::map<dynamic_string> string_set;
        string_set ss;
        ss.insert("blah");
        pRoot->add_map("set", ss);

        typedef vogl::vector<int16_t> cool_vec;
        cool_vec blah;
        blah.push_back(1);
        blah.push_back(3);
        blah.push_back(5);
        pRoot->add_vector("vec", blah);

        cool_vec blah2;
        pRoot->get_vector("vec", blah2);
        printf("vec:\n");
        for (uint32_t i = 0; i < blah2.size(); i++)
            printf("%i\n", blah2[i]);

        // Now create a string to string array map
        typedef vogl::map<dynamic_string, dynamic_string_array> string_map;
        string_map s;

        dynamic_string_array sa;
        sa.push_back("a");
        s.insert("1", sa);

        dynamic_string_array sb;
        sb.push_back("b");
        sb.push_back("c");
        sb.push_back("d");
        s.insert("2", sb);

        // Add the string to string map to the JSON document under the "hashmap" object
        pRoot->add_map("hashmap", s);

        // Now serialize the JSON document to text and print it to stdout
        dynamic_string str;
        doc.serialize(str);
        printf("JSON: %s\n", str.get_ptr());

        // Retrieve the string map under the "hashmap" object and put it into q
        string_map q;
        pRoot->get_map("hashmap", q);

        // Now print q
        printf("hashmap:\n");
        for (string_map::const_iterator it = q.begin(); it != q.end(); ++it)
        {
            const dynamic_string_array &b = it->second;

            printf("%s :\n", it->first.get_ptr());
            for (uint32_t i = 0; i < b.size(); i++)
                printf(" %s\n", b[i].get_ptr());
        }

        return true;
    }

} // namespace vogl
