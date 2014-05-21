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

// File: vogl_value.cpp
#include "vogl_core.h"
#include "vogl_value.h"
#include <float.h>

namespace vogl
{
    const char *g_value_data_type_strings[cDTTotal + 1] =
        {
            "invalid",
            "bool",
            "int8_t",
            "uint8_t",
            "int16_t",
            "uint16_t",
            "int",
            "uint32_t",
            "int64_t",
            "uint64_t",
            "float",
            "double",
            "void*",
            "string_hash",
            "string",
            "vec3f",
            "vec3i",
            "blob",
            "json",
            NULL,
        };

    const char *g_value_data_classic_type_strings[cDTTotal + 1] =
        {
            "invalid",
            "bool",
            "int8",
            "uint8",
            "int16",
            "uint16",
            "int",
            "uint32",
            "int64",
            "uint64",
            "float",
            "double",
            "void*",
            "string_hash",
            "string",
            "vec3f",
            "vec3i",
            "blob",
            "json",
            NULL,
        };


    value_data_type string_to_value_data_type(const char *pStr)
    {
        const char ** value_type_strings[] = 
        { 
            g_value_data_type_strings, 
            g_value_data_classic_type_strings, 
            NULL 
        };

        for (int vts = 0; value_type_strings[vts] != NULL; ++vts )
            for (uint32_t i = 0; i < cDTTotal; i++)
                if (!vogl_stricmp(pStr, value_type_strings[vts][i]))
                    return static_cast<value_data_type>(i);

        // McJohn: I made a mistake when I was excising uint everywhere in favor of uint32_t, 
        // but it was checked in for "awhile" so in case we get all the way here, let's go ahead
        // and check and make sure that we don't have a trace with just 'uint'.
        if (!vogl_stricmp(pStr, "uint"))
            return cDTUInt;
        return cDTInvalid;
    }

    value &value::operator=(const value &other)
    {
        if (this == &other)
            return *this;

        change_type(other.m_type);
        m_user_data = other.m_user_data;
        m_flags = other.m_flags;

        switch (other.m_type)
        {
            case cDTString:
                m_pStr->set(*other.m_pStr);
                break;
            case cDTVec3F:
                m_pVec3F->set(*other.m_pVec3F);
                break;
            case cDTVec3I:
                m_pVec3I->set(*other.m_pVec3I);
                break;
            case cDTBlob:
                (*m_pBlob) = (*other.m_pBlob);
                break;
            case cDTJSONDoc:
                (*m_pJSONDoc) = (*other.m_pJSONDoc);
                break;
            default:
                m_uint64 = other.m_uint64;
                break;
        }
        return *this;
    }

    bool value::operator==(const value &other) const
    {
        if (this == &other)
            return true;

        if (m_flags != other.m_flags)
            return false;
        if (m_flags & cFlagsHasUserData)
        {
            if (m_user_data != other.m_user_data)
                return false;
        }

        if (m_type != other.m_type)
            return false;

        if (m_type == cDTInvalid)
            return true;
        else if (is_json_document())
            return *m_pJSONDoc == *other.m_pJSONDoc;

        if (get_data_size_in_bytes() != other.get_data_size_in_bytes())
            return false;

        return memcmp(get_data_ptr(), other.get_data_ptr(), other.get_data_size_in_bytes()) == 0;
    }

    bool value::operator<(const value &other) const
    {
        if (this == &other)
            return true;

        if (m_flags < other.m_flags)
            return true;
        else if (m_flags != other.m_flags)
            return false;

        if (m_flags & cFlagsHasUserData)
        {
            if (m_user_data < other.m_user_data)
                return true;
            else if (m_user_data != other.m_user_data)
                return false;
        }

        if (m_type < other.m_type)
            return true;
        else if (m_type == other.m_type)
        {
            switch (m_type)
            {
                case cDTInvalid:
                    break;
                case cDTString:
                    return m_pStr->compare(*other.m_pStr, true) < 0;
                case cDTBool:
                    return m_bool < other.m_bool;
                case cDTInt8:
                    return m_int8 < other.m_int8;
                case cDTUInt8:
                    return m_uint8 < other.m_uint8;
                case cDTInt16:
                    return m_int16 < other.m_int16;
                case cDTUInt16:
                    return m_uint16 < other.m_uint16;
                case cDTInt:
                    return m_int < other.m_int;
                case cDTStringHash:
                case cDTUInt:
                    return m_uint < other.m_uint;
                case cDTFloat:
                    return m_float < other.m_float;
                case cDTDouble:
                    return m_double < other.m_double;
                case cDTInt64:
                    return m_int64 < other.m_int64;
                case cDTUInt64:
                    return m_uint64 < other.m_uint64;
                case cDTVoidPtr:
                    return m_pPtr < other.m_pPtr;
                case cDTVec3F:
                    return (*m_pVec3F) < (*other.m_pVec3F);
                case cDTVec3I:
                    return (*m_pVec3I) < (*other.m_pVec3I);
                case cDTBlob:
                    return (*m_pBlob) < (*other.m_pBlob);
                case cDTJSONDoc:
                {
                    // This is brain dead slow, but I doubt we'll be using this path much if at all.
                    uint8_vec doc;
                    m_pJSONDoc->binary_serialize(doc);

                    uint8_vec other_doc;
                    other.m_pJSONDoc->binary_serialize(other_doc);

                    return doc < other_doc;
                }
                default:
                    VOGL_ASSERT_ALWAYS;
                    break;
            }
        }

        return false;
    }

    bool value::parse(const char *p)
    {
        if ((!p) || (!p[0]))
        {
            clear();
            return false;
        }

        size_t str_len = strlen(p);

        if (vogl_stricmp(p, "false") == 0)
        {
            set_bool(false);
            return true;
        }
        else if (vogl_stricmp(p, "true") == 0)
        {
            set_bool(true);
            return true;
        }

        if (p[0] == '\"')
        {
            dynamic_string str;
            str = p + 1;
            if (!str.is_empty())
            {
                if (str[str.get_len() - 1] == '\"')
                {
                    str.left(str.get_len() - 1);
                    set_str(str);

                    return true;
                }
            }
        }

        if (p[0] == '[')
        {
            if (p[str_len - 1] == ']')
            {
                size_t i;
                for (i = 1; i < (str_len - 1); i++)
                {
                    if (((static_cast<int>(i) - 1) % 3) == 2)
                    {
                        if (p[i] != ',')
                            break;
                    }
                    else if (utils::from_hex(p[i]) < 0)
                        break;
                }

                if (i == (str_len - 1))
                {
                    size_t size = str_len - 2;
                    if (size)
                    {
                        if (size == 2)
                            size = 1;
                        else
                            size = (size / 3) + 1;
                    }

                    if (size > cUINT32_MAX)
                    {
                        clear();
                        return false;
                    }

                    change_type(cDTBlob);
                    if (!m_pBlob->try_resize(static_cast<uint32_t>(size)))
                    {
                        clear();
                        return false;
                    }

                    if (size)
                    {
                        uint8_t *pDst = m_pBlob->get_ptr();

                        for (i = 1; i < (str_len - 1); i += 3)
                        {
                            int h = utils::from_hex(p[i]);
                            int l = utils::from_hex(p[i + 1]);

                            *pDst++ = ((h << 4) | l);
                        }
                    }

                    return true;
                }
            }
        }

        if (strchr(p, ',') != NULL)
        {
            float fx = 0, fy = 0, fz = 0;
#ifdef COMPILER_MSVC
            if (sscanf_s(p, "%f,%f,%f", &fx, &fy, &fz) == 3)
#else
            if (sscanf(p, "%f,%f,%f", &fx, &fy, &fz) == 3)
#endif
            {
                bool as_float = true;
                int ix = 0, iy = 0, iz = 0;
#ifdef COMPILER_MSVC
                if (sscanf_s(p, "%i,%i,%i", &ix, &iy, &iz) == 3)
#else
                if (sscanf(p, "%i,%i,%i", &ix, &iy, &iz) == 3)
#endif
                {
                    if ((ix == fx) && (iy == fy) && (iz == fz))
                        as_float = false;
                }

                if (as_float)
                    set_vec(vec3F(fx, fy, fz));
                else
                    set_vec(vec3I(ix, iy, iz));

                return true;
            }
        }

        if (str_len >= 2)
        {
            if ((p[0] == '{') && (p[str_len - 1] == '}'))
            {
                json_document doc;
                if (doc.deserialize(p))
                {
                    set_json_document_take_ownership(doc);
                    return true;
                }
            }
        }

        const char *q = p;
        bool success = string_ptr_to_uint64(q, m_uint64);
        if ((success) && (*q == 0))
        {
            if (m_uint64 <= cUINT8_MAX)
                set_uint8(static_cast<uint8_t>(m_uint64));
            else if (m_uint64 <= cUINT16_MAX)
                set_uint16(static_cast<uint16_t>(m_uint64));
            else if (m_uint64 <= cUINT32_MAX)
                set_uint(static_cast<uint32_t>(m_uint64));
            else
                set_uint64(m_uint64);
            return true;
        }

        q = p;
        success = string_ptr_to_int64(q, m_int64);
        if ((success) && (*q == 0))
        {
            if ((m_int64 >= cINT16_MIN) && (m_int64 <= cINT16_MAX))
                set_int16(static_cast<int16_t>(m_int64));
            else if ((m_int64 >= cINT32_MIN) && (m_int64 <= cINT32_MAX))
                set_int(static_cast<int>(m_int64));
            else
                set_int64(m_int64);
            return true;
        }

        q = p;
        success = string_ptr_to_double(q, m_double);
        if ((success) && (*q == 0))
        {
            float value_as_float = static_cast<float>(m_double);
            if (value_as_float == m_double)
                set_float(value_as_float);
            else
                set_double(m_double);
            return true;
        }

        set_string(p);

        return true;
    }

    // rename quote_strings to parsable? or add another method?
    // TODO: Get rid of quote_strings and add flags field
    dynamic_string &value::get_string(dynamic_string &dst, bool quote_strings) const
    {
        switch (m_type)
        {
            case cDTInvalid:
                break;
            case cDTString:
            {
                if (quote_strings)
                {
                    dst = "\"";
                    dst += *m_pStr;
                    dst += "\"";
                }
                else
                {
                    dst = *m_pStr;
                }
                break;
            }
            case cDTBool:
                dst = m_bool ? "TRUE" : "FALSE";
                break;
            case cDTInt8:
                dst.format("%i", m_int8);
                break;
            case cDTUInt8:
                dst.format("%u", m_uint8);
                break;
            case cDTInt16:
                dst.format("%i", m_int16);
                break;
            case cDTUInt16:
                dst.format("%u", m_uint16);
                break;
            case cDTInt:
                dst.format("%i", m_int);
                break;
            case cDTStringHash:
            case cDTUInt:
                dst.format("%u", m_uint);
                break;
            case cDTInt64:
                dst.format("%" PRIi64, m_int64);
                break;
            case cDTUInt64:
                dst.format("%" PRIu64, m_uint64);
                break;
            case cDTVoidPtr:
                dst.format("0x%" PRIxPTR, reinterpret_cast<uintptr_t>(m_pPtr));
                break;
            case cDTFloat:
                dst.format("%1.8f", m_float);
                break;
            case cDTDouble:
                dst.format("%1.17f", m_double);
                break;
            case cDTVec3F:
                dst.format("%1.8f,%1.8f,%1.8f", (*m_pVec3F)[0], (*m_pVec3F)[1], (*m_pVec3F)[2]);
                break;
            case cDTVec3I:
                dst.format("%i,%i,%i", (*m_pVec3I)[0], (*m_pVec3I)[1], (*m_pVec3I)[2]);
                break;
            case cDTBlob:
            {
                uint32_t blob_size = m_pBlob->size();
                const uint8_t *pSrc = m_pBlob->get_ptr();

                if (!blob_size)
                    dst = "[]";
                else
                {
                    dst.set_len(blob_size * 3 + 2 - 1);
                    dst.set_char(0, '[');
                    dst.set_char(dst.get_len() - 1, ']');

                    for (uint32_t i = 0; i < blob_size; i++)
                    {
                        uint8_t v = pSrc[i];
                        dst.set_char(1 + i * 3 + 0, utils::to_hex(v >> 4));
                        dst.set_char(1 + i * 3 + 1, utils::to_hex(v & 0xF));
                        if (i < (blob_size - 1))
                            dst.set_char(1 + i * 3 + 2, ',');
                    }
                }

                break;
            }
            case cDTJSONDoc:
            {
                m_pJSONDoc->serialize(dst);
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                break;
        }

        return dst;
    }

    bool value::get_int8_or_fail(int8_t &val, uint32_t component) const
    {
        int64_t i64_val;
        bool success = get_int64_or_fail(i64_val, component);
        if ((!success) || (i64_val < cINT8_MIN) || (i64_val > cINT8_MAX))
            return false;

        val = static_cast<int8_t>(i64_val);
        return true;
    }

    bool value::get_uint8_or_fail(uint8_t &val, uint32_t component) const
    {
        uint64_t u64_val;
        bool success = get_uint64_or_fail(u64_val, component);
        if ((!success) || (u64_val > cUINT8_MAX))
            return false;

        val = static_cast<uint8_t>(u64_val);
        return true;
    }

    bool value::get_int16_or_fail(int16_t &val, uint32_t component) const
    {
        int64_t i64_val;
        bool success = get_int64_or_fail(i64_val, component);
        if ((!success) || (i64_val < cINT16_MIN) || (i64_val > cINT16_MAX))
            return false;

        val = static_cast<int16_t>(i64_val);
        return true;
    }

    bool value::get_uint16_or_fail(uint16_t &val, uint32_t component) const
    {
        uint64_t u64_val;
        bool success = get_uint64_or_fail(u64_val, component);
        if ((!success) || (u64_val > cUINT16_MAX))
            return false;

        val = static_cast<uint16_t>(u64_val);
        return true;
    }

    bool value::get_int_or_fail(int &val, uint32_t component) const
    {
        int64_t i64_val;
        bool success = get_int64_or_fail(i64_val, component);
        if ((!success) || (i64_val < cINT32_MIN) || (i64_val > cINT32_MAX))
            return false;

        val = static_cast<int>(i64_val);
        return true;
    }

    bool value::get_int64_or_fail(int64_t &val, uint32_t component) const
    {
        switch (m_type)
        {
            case cDTInvalid:
            {
                return false;
            }
            case cDTString:
            {
                const char *p = m_pStr->get_ptr();
                return string_ptr_to_int64(p, val);
            }
            case cDTBool:
            {
                val = m_bool;
                break;
            }
            case cDTInt8:
            {
                val = m_int8;
                break;
            }
            case cDTUInt8:
            {
                val = m_uint8;
                break;
            }
            case cDTInt16:
            {
                val = m_int16;
                break;
            }
            case cDTUInt16:
            {
                val = m_uint16;
                break;
            }
            case cDTInt:
            {
                val = m_int;
                break;
            }
            case cDTStringHash:
            case cDTUInt:
            {
                val = m_uint;
                break;
            }
            case cDTInt64:
            {
                val = m_int64;
                break;
            }
            case cDTUInt64:
            {
                if (m_uint64 > static_cast<uint64_t>(cINT64_MAX))
                    return false;
                val = static_cast<int64_t>(m_uint64);
                break;
            }
            case cDTFloat:
            {
                if ((m_float < cINT64_MIN) || (m_float > cINT64_MAX))
                    return false;
                val = static_cast<int64_t>(m_float);
                break;
            }
            case cDTDouble:
            {
                if ((m_double < cINT64_MIN) || (m_double > cINT64_MAX))
                    return false;
                val = static_cast<int64_t>(m_double);
                break;
            }
            case cDTVec3F:
            {
                if (component > 2)
                    return false;
                if (((*m_pVec3F)[component] < cINT64_MIN) || ((*m_pVec3F)[component] > cINT64_MAX))
                    return false;
                val = static_cast<int64_t>((*m_pVec3F)[component]);
                break;
            }
            case cDTVec3I:
            {
                if (component > 2)
                    return false;
                val = (*m_pVec3I)[component];
                break;
            }
            case cDTBlob:
            {
                if (component >= m_pBlob->size())
                    return false;
                val = m_pBlob->at(component);
                break;
            }
            case cDTVoidPtr:
            {
                val = reinterpret_cast<int64_t>(m_pPtr);
                break;
            }
            case cDTJSONDoc:
            {
                if (m_pJSONDoc->is_node())
                    return false;
                val = m_pJSONDoc->as_int64();
                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                return false;
            }
        }
        return true;
    }

    bool value::get_uint_or_fail(uint32_t &val, uint32_t component) const
    {
        uint64_t u64_val;
        bool success = get_uint64_or_fail(u64_val, component);
        if (u64_val > cUINT32_MAX)
            return false;
        val = static_cast<uint32_t>(u64_val);
        return success;
    }

    bool value::get_uint64_or_fail(uint64_t &val, uint32_t component) const
    {
        switch (m_type)
        {
            case cDTInvalid:
            {
                return false;
            }
            case cDTString:
            {
                const char *p = m_pStr->get_ptr();
                return string_ptr_to_uint64(p, val);
            }
            case cDTBool:
            {
                val = m_bool;
                break;
            }
            case cDTInt8:
            {
                if (m_int8 < 0)
                    return false;
                val = m_int8;
                break;
            }
            case cDTUInt8:
            {
                val = m_uint8;
                break;
            }
            case cDTInt16:
            {
                if (m_int16 < 0)
                    return false;
                val = static_cast<uint64_t>(m_int16);
                break;
            }
            case cDTUInt16:
            {
                val = m_uint16;
                break;
            }
            case cDTInt:
            {
                if (m_int < 0)
                    return false;
                val = static_cast<uint64_t>(m_int);
                break;
            }
            case cDTStringHash:
            case cDTUInt:
            {
                val = m_uint;
                break;
            }
            case cDTInt64:
            {
                if (m_int64 < 0)
                    return false;
                val = static_cast<uint64_t>(m_int64);
                break;
            }
            case cDTUInt64:
            {
                val = static_cast<uint64_t>(m_uint64);
                break;
            }
            case cDTFloat:
            {
                if ((m_float < 0) || (m_float > cUINT64_MAX))
                    return false;
                val = static_cast<uint64_t>(m_float);
                break;
            }
            case cDTDouble:
            {
                if ((m_double < 0) || (m_double > cUINT64_MAX))
                    return false;
                val = static_cast<uint64_t>(m_double);
                break;
            }
            case cDTVec3F:
            {
                if (component > 2)
                    return false;
                if (((*m_pVec3F)[component] < 0) || ((*m_pVec3F)[component] > cUINT64_MAX))
                {
                    return false;
                }
                val = static_cast<uint64_t>((*m_pVec3F)[component]);
                break;
            }
            case cDTVec3I:
            {
                if (component > 2)
                    return false;
                if ((*m_pVec3I)[component] < 0)
                {
                    return false;
                }
                val = static_cast<uint64_t>((*m_pVec3I)[component]);
                break;
            }
            case cDTBlob:
            {
                if (component >= m_pBlob->size())
                    return false;
                val = m_pBlob->at(component);
                break;
            }
            case cDTVoidPtr:
            {
                val = reinterpret_cast<uint64_t>(m_pPtr);
                return false;
            }
            case cDTJSONDoc:
            {
                if (m_pJSONDoc->is_node())
                    return false;
                val = m_pJSONDoc->as_uint64();
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                break;
        }
        return true;
    }

    bool value::get_bool_or_fail(bool &val, uint32_t component) const
    {
        switch (m_type)
        {
            case cDTInvalid:
            {
                return false;
            }
            case cDTString:
            {
                const char *p = m_pStr->get_ptr();
                return string_ptr_to_bool(p, val);
            }
            case cDTBool:
            {
                val = m_bool;
                break;
            }
            case cDTInt8:
            case cDTUInt8:
            {
                val = (m_uint8 != 0);
                break;
            }
            case cDTInt16:
            case cDTUInt16:
            {
                val = (m_uint16 != 0);
                break;
            }
            case cDTStringHash:
            case cDTInt:
            case cDTUInt:
            {
                val = (m_uint != 0);
                break;
            }
            case cDTInt64:
            case cDTUInt64:
            {
                val = (m_uint64 != 0);
                break;
            }
            case cDTFloat:
            {
                val = (m_float != 0);
                break;
            }
            case cDTDouble:
            {
                val = (m_double != 0);
                break;
            }
            case cDTVec3F:
            {
                if (component > 2)
                    return false;
                val = ((*m_pVec3F)[component] != 0);
                break;
            }
            case cDTVec3I:
            {
                if (component > 2)
                    return false;
                val = ((*m_pVec3I)[component] != 0);
                break;
            }
            case cDTBlob:
            {
                if (component >= m_pBlob->size())
                    return false;
                val = m_pBlob->at(component) != 0;
                break;
            }
            case cDTVoidPtr:
            {
                val = (m_pPtr != NULL);
                break;
            }
            case cDTJSONDoc:
            {
                if (m_pJSONDoc->is_node())
                    return false;
                val = m_pJSONDoc->as_bool();
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                val = false;
                break;
        }
        return true;
    }

    bool value::get_double_or_fail(double &val, uint32_t component) const
    {
        switch (m_type)
        {
            case cDTInvalid:
            {
                return false;
            }
            case cDTString:
            {
                const char *p = m_pStr->get_ptr();
                return string_ptr_to_double(p, val);
            }
            case cDTBool:
            {
                val = m_bool;
                break;
            }
            case cDTInt8:
            {
                val = static_cast<double>(m_int8);
                break;
            }
            case cDTUInt8:
            {
                val = static_cast<double>(m_uint8);
                break;
            }
            case cDTInt16:
            {
                val = static_cast<double>(m_int16);
                break;
            }
            case cDTUInt16:
            {
                val = static_cast<double>(m_uint16);
                break;
            }
            case cDTInt:
            {
                val = static_cast<double>(m_int);
                break;
            }
            case cDTStringHash:
            case cDTUInt:
            {
                val = static_cast<double>(m_uint);
                break;
            }
            case cDTInt64:
            {
                val = static_cast<double>(m_int64);
                break;
            }
            case cDTUInt64:
            {
                val = static_cast<double>(m_uint64);
                break;
            }
            case cDTFloat:
            {
                val = m_float;
                break;
            }
            case cDTDouble:
            {
                val = m_double;
                break;
            }
            case cDTVec3F:
            {
                if (component > 2)
                    return false;
                val = (*m_pVec3F)[component];
                break;
            }
            case cDTVec3I:
            {
                if (component > 2)
                    return false;
                val = static_cast<double>((*m_pVec3I)[component]);
                break;
            }
            case cDTBlob:
            {
                if (component >= m_pBlob->size())
                    return false;
                val = static_cast<double>(m_pBlob->at(component));
                break;
            }
            case cDTVoidPtr:
            {
                val = 0;
                break;
            }
            case cDTJSONDoc:
            {
                if (m_pJSONDoc->is_node())
                    return false;
                val = m_pJSONDoc->as_double();
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                return false;
        }
        return true;
    }

    bool value::get_float_or_fail(float &val, uint32_t component) const
    {
        double dbl_val;
        bool success = get_double_or_fail(dbl_val, component);

        if ((dbl_val < -FLT_MAX) || (dbl_val > FLT_MAX))
        {
            dbl_val = 0;
            success = false;
        }

        val = static_cast<float>(dbl_val);
        return success;
    }

    bool value::get_vec3F_or_fail(vec3F &val) const
    {
        switch (m_type)
        {
            case cDTInvalid:
            {
                return false;
            }
            case cDTString:
            {
                const char *p = m_pStr->get_ptr();
                float x = 0, y = 0, z = 0;
#ifdef COMPILER_MSVC
                if (sscanf_s(p, "%f,%f,%f", &x, &y, &z) == 3)
#else
                if (sscanf(p, "%f,%f,%f", &x, &y, &z) == 3)
#endif
                {
                    val.set(x, y, z);
                    return true;
                }
                else
                {
                    return false;
                }
            }
            case cDTBool:
            {
                val.set(m_bool);
                break;
            }
            case cDTUInt8:
            {
                val.set(static_cast<float>(m_uint8));
                break;
            }
            case cDTInt16:
            {
                val.set(static_cast<float>(m_int16));
                break;
            }
            case cDTUInt16:
            {
                val.set(static_cast<float>(m_uint16));
                break;
            }
            case cDTInt:
            {
                val.set(static_cast<float>(m_int));
                break;
            }
            case cDTStringHash:
            case cDTUInt:
            {
                val.set(static_cast<float>(m_uint));
                break;
            }
            case cDTInt64:
            {
                val.set(static_cast<float>(m_int64));
                break;
            }
            case cDTUInt64:
            {
                val.set(static_cast<float>(m_uint64));
                break;
            }
            case cDTFloat:
            {
                val.set(m_float);
                break;
            }
            case cDTDouble:
            {
                val.set(static_cast<float>(m_double));
                break;
            }
            case cDTVec3F:
            {
                val = *m_pVec3F;
                break;
            }
            case cDTVec3I:
            {
                val.set(static_cast<float>((*m_pVec3I)[0]), static_cast<float>((*m_pVec3I)[1]), static_cast<float>((*m_pVec3I)[2]));
                break;
            }
            case cDTBlob:
            {
                if (!m_pBlob->size())
                    return false;
                val.set(static_cast<float>(m_pBlob->at(0)));
                break;
            }
            case cDTVoidPtr:
            {
                val.clear();
                break;
            }
            case cDTJSONDoc:
            {
                if (m_pJSONDoc->is_node())
                    return false;
				val.set(m_pJSONDoc->as_float());
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                return false;
        }
        return true;
    }

    bool value::get_vec3I_or_fail(vec3I &val) const
    {
        switch (m_type)
        {
            case cDTInvalid:
            {
                return false;
            }
            case cDTString:
            {
                const char *p = m_pStr->get_ptr();
                float x = 0, y = 0, z = 0;
#ifdef COMPILER_MSVC
                if (sscanf_s(p, "%f,%f,%f", &x, &y, &z) == 3)
#else
                if (sscanf(p, "%f,%f,%f", &x, &y, &z) == 3)
#endif
                {
                    if ((x < INT_MIN) || (x > INT_MAX) || (y < INT_MIN) || (y > INT_MAX) || (z < INT_MIN) || (z > INT_MAX))
                        return false;
                    val.set((int)x, (int)y, (int)z);
                    return true;
                }
                else
                    return false;

                break;
            }
            case cDTBool:
            {
                val.set(m_bool);
                break;
            }
            case cDTUInt8:
            {
                val.set(m_uint8);
                break;
            }
            case cDTInt16:
            {
                val.set(m_int16);
                break;
            }
            case cDTUInt16:
            {
                val.set(m_uint16);
                break;
            }
            case cDTInt:
            {
                val.set(m_int);
                break;
            }
            case cDTStringHash:
            case cDTUInt:
            {
                if (m_uint > static_cast<uint32_t>(cINT32_MAX))
                    return false;
                val.set(m_uint);
                break;
            }
            case cDTInt64:
            {
                if ((m_int64 < cINT32_MIN) || (m_int64 > cINT32_MAX))
                    return false;
                val.set(static_cast<int>(m_int64));
                break;
            }
            case cDTUInt64:
            {
                if (m_uint64 > static_cast<uint64_t>(cINT32_MAX))
                    return false;
                val.set(static_cast<int>(m_uint64));
                break;
            }
            case cDTFloat:
            {
                val.set(static_cast<int>(m_float));
                break;
            }
            case cDTDouble:
            {
                val.set(static_cast<int>(m_double));
                break;
            }
            case cDTVec3F:
            {
                val.set((int)(*m_pVec3F)[0], (int)(*m_pVec3F)[1], (int)(*m_pVec3F)[2]);
                break;
            }
            case cDTVec3I:
            {
                val = *m_pVec3I;
                break;
            }
            case cDTBlob:
            {
                if (!m_pBlob->size())
                    return false;
                val.set(m_pBlob->at(0));
                break;
            }
            case cDTVoidPtr:
            {
                val.clear();
                break;
            }
            case cDTJSONDoc:
            {
                if (m_pJSONDoc->is_node())
                    return false;
                val.set(m_pJSONDoc->as_int());
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                return false;
        }
        return true;
    }

    bool value::get_string_hash_or_fail(string_hash &hash) const
    {
        if (m_type == cDTString)
        {
            // Hash the string
            hash.set(m_pStr->get_ptr());
            return true;
        }
        else if (m_type == cDTStringHash)
        {
            hash = get_string_hash_ref();
            return true;
        }
        else if (m_type == cDTBlob)
        {
            // Just hash the blob and hope for the best
            hash.set(reinterpret_cast<const char *>(m_pBlob->get_ptr()), m_pBlob->size());
            return true;
        }
        else if (m_type == cDTJSONDoc)
        {
            // This is slow but it's better than nothing.
            uint8_vec data;
            m_pJSONDoc->binary_serialize(data);
            hash.set(reinterpret_cast<const char *>(data.get_ptr()), data.size());
            return true;
        }

        // Convert to uint32_t and hope for the best
        uint32_t val;
        bool success = get_uint_or_fail(val, 0);
        if (!success)
            return false;

        hash.set_hash(val);
        return true;
    }

    uint32_t value::get_serialize_size(bool serialize_user_data) const
    {
        uint32_t size = sizeof(uint8_t);

        if (serialize_user_data)
            size += sizeof(m_user_data);

        switch (m_type)
        {
            case cDTString:
            {
                size += m_pStr->get_serialize_size();
                break;
            }
            case cDTBool:
            {
                size += sizeof(m_bool);
                break;
            }
            case cDTUInt8:
            {
                size += sizeof(m_uint8);
                break;
            }
            case cDTInt16:
            case cDTUInt16:
            {
                size += sizeof(m_uint16);
                break;
            }
            case cDTStringHash:
            case cDTInt:
            case cDTUInt:
            case cDTFloat:
            {
                size += sizeof(m_uint);
                break;
            }
            case cDTInt64:
            case cDTUInt64:
            case cDTDouble:
            case cDTVoidPtr:
            {
                size += sizeof(m_uint64);
                break;
            }
            case cDTVec3F:
            {
                size += sizeof(vec3F);
                break;
            }
            case cDTVec3I:
            {
                size += sizeof(vec3I);
                break;
            }
            case cDTBlob:
            {
                size += sizeof(uint32_t) + m_pBlob->size();
                break;
            }
            case cDTJSONDoc:
            {
                // TODO: This binary serializes the sucker and tosses the data to get the serialization size.
                uint8_vec data;
                m_pJSONDoc->binary_serialize(data);
                size += sizeof(uint32_t) + data.size();
                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                break;
        }

        return size;
    }

    int value::serialize(void *pBuf, uint32_t buf_size, bool little_endian, bool serialize_user_data) const
    {
        uint32_t buf_left = buf_size;

        uint8_t t = (uint8_t)m_type;
        if (!utils::write_obj(t, pBuf, buf_left, little_endian))
            return -1;

        if (serialize_user_data)
        {
            if (!utils::write_obj(m_user_data, pBuf, buf_left, little_endian))
                return -1;
        }

        switch (m_type)
        {
            case cDTString:
            {
                int bytes_written = m_pStr->serialize(pBuf, buf_left, little_endian);
                if (bytes_written < 0)
                    return -1;

                pBuf = static_cast<uint8_t *>(pBuf) + bytes_written;
                buf_left -= bytes_written;

                break;
            }
            case cDTBool:
            {
                if (!utils::write_obj(m_bool, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTUInt8:
            {
                if (!utils::write_obj(m_uint8, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTInt16:
            case cDTUInt16:
            {
                if (!utils::write_obj(m_uint16, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTStringHash:
            case cDTInt:
            case cDTUInt:
            case cDTFloat:
            {
                if (!utils::write_obj(m_uint, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTInt64:
            case cDTUInt64:
            case cDTDouble:
            case cDTVoidPtr:
            {
                if (!utils::write_obj(m_uint64, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTVec3F:
            {
                for (uint32_t i = 0; i < 3; i++)
                    if (!utils::write_obj((*m_pVec3F)[i], pBuf, buf_left, little_endian))
                        return -1;
                break;
            }
            case cDTVec3I:
            {
                for (uint32_t i = 0; i < 3; i++)
                    if (!utils::write_obj((*m_pVec3I)[i], pBuf, buf_left, little_endian))
                        return -1;
                break;
            }
            case cDTBlob:
            {
                uint32_t size = m_pBlob->size();

                if (buf_left < (size + sizeof(uint32_t)))
                    return -1;

                if (!utils::write_obj(size, pBuf, buf_left, little_endian))
                    return -1;

                if (size)
                {
                    memcpy(pBuf, m_pBlob->get_ptr(), size);
                    pBuf = static_cast<uint8_t *>(pBuf) + size;
                    buf_left -= size;
                }

                break;
            }
            case cDTJSONDoc:
            {
                uint8_vec data;
                m_pJSONDoc->binary_serialize(data);

                uint32_t size = data.size();

                if (buf_left < (size + sizeof(uint32_t)))
                    return -1;

                if (!utils::write_obj(size, pBuf, buf_left, little_endian))
                    return -1;

                if (size)
                {
                    memcpy(pBuf, data.get_ptr(), size);
                    pBuf = static_cast<uint8_t *>(pBuf) + size;
                    buf_left -= size;
                }

                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                break;
        }

        return buf_size - buf_left;
    }

    int value::deserialize(const void *pBuf, uint32_t buf_size, bool little_endian, bool serialize_user_data)
    {
        uint32_t buf_left = buf_size;

        uint8_t t;
        if (!utils::read_obj(t, pBuf, buf_left, little_endian))
            return -1;

        if (t >= cDTTotal)
            return -1;

        if (serialize_user_data)
        {
            if (!utils::read_obj(m_user_data, pBuf, buf_left, little_endian))
                return -1;
            m_flags |= cFlagsHasUserData;
        }
        else
        {
            m_flags &= ~cFlagsHasUserData;
        }

        switch (t)
        {
            case cDTString:
            {
                change_type(cDTString);

                int bytes_read = m_pStr->deserialize(pBuf, buf_left, little_endian);
                if (bytes_read < 0)
                    return -1;

                pBuf = static_cast<const uint8_t *>(pBuf) + bytes_read;
                buf_left -= bytes_read;

                break;
            }
            case cDTBool:
            {
                change_type(static_cast<value_data_type>(t));

                if (!utils::read_obj(m_bool, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTUInt8:
            {
                change_type(static_cast<value_data_type>(t));

                if (!utils::read_obj(m_uint8, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTInt16:
            case cDTUInt16:
            {
                change_type(static_cast<value_data_type>(t));

                if (!utils::read_obj(m_uint16, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTStringHash:
            case cDTInt:
            case cDTUInt:
            case cDTFloat:
            {
                change_type(static_cast<value_data_type>(t));

                if (!utils::read_obj(m_uint, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTInt64:
            case cDTUInt64:
            case cDTDouble:
            case cDTVoidPtr:
            {
                change_type(static_cast<value_data_type>(t));

                if (!utils::read_obj(m_uint64, pBuf, buf_left, little_endian))
                    return -1;
                break;
            }
            case cDTVec3F:
            {
                change_type(cDTVec3F);

                for (uint32_t i = 0; i < 3; i++)
                    if (!utils::read_obj((*m_pVec3F)[i], pBuf, buf_left, little_endian))
                        return -1;
                break;
            }
            case cDTVec3I:
            {
                change_type(cDTVec3I);

                for (uint32_t i = 0; i < 3; i++)
                    if (!utils::read_obj((*m_pVec3I)[i], pBuf, buf_left, little_endian))
                        return -1;
                break;
            }
            case cDTBlob:
            {
                change_type(cDTBlob);

                uint32_t size = 0;
                if (!utils::read_obj(size, pBuf, buf_left, little_endian))
                    return -1;

                if (buf_left < size)
                    return -1;

                if (!m_pBlob->try_resize(size))
                    return -1;

                if (size)
                {
                    memcpy(m_pBlob->get_ptr(), pBuf, size);

                    pBuf = static_cast<const uint8_t *>(pBuf) + size;
                    buf_left -= size;
                }

                break;
            }
            case cDTJSONDoc:
            {
                change_type(cDTJSONDoc);

                uint32_t size = 0;
                if (!utils::read_obj(size, pBuf, buf_left, little_endian))
                    return -1;

                if (buf_left < size)
                    return -1;

                if (!m_pJSONDoc->binary_deserialize(static_cast<const uint8_t *>(pBuf), size))
                    return -1;

                pBuf = static_cast<const uint8_t *>(pBuf) + size;
                buf_left -= size;

                break;
            }
            default:
                VOGL_ASSERT_ALWAYS;
                clear();
                return -1;
        }

        return buf_size - buf_left;
    }

} // namespace vogl
